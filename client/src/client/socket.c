/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
 *                                                                       *
 * Fork from Crossfire (Multiplayer game for X-windows).                 *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the Free Software           *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
 *                                                                       *
 * The author can be reached at admin@atrinik.org                        *
 ************************************************************************/

/**
 * @file
 * Client sockets related code.
 */

#include <global.h>
#include <packet.h>
#include <network_graph.h>

static SDL_Thread *input_thread;
static SDL_mutex *input_buffer_mutex;
static SDL_cond *input_buffer_cond;

static SDL_Thread *output_thread;
static SDL_mutex *output_buffer_mutex;
static SDL_cond *output_buffer_cond;

/**
 * Mutex to protect socket deinitialization.
 */
static SDL_mutex *socket_mutex;

/**
 * All socket threads will exit if they see this flag set.
 */
static int abort_thread = 0;

/* start is the first waiting item in queue, end is the most recent enqueued */
static command_buffer *input_queue_start = NULL, *input_queue_end = NULL;
static command_buffer *output_queue_start = NULL, *output_queue_end = NULL;

/**
 * Create a new command buffer of the given size, copying the data buffer
 * if not NULL. The buffer will always be null-terminated for safety (and
 * one byte larger than requested).
 * @param len
 * Requested buffer size in bytes.
 * @param data
 * Buffer data to copy (len bytes), or NULL.
 * @return
 * A new command buffer or NULL in case of an error.
 */
command_buffer *command_buffer_new(size_t len, uint8_t *data)
{
    command_buffer *buf = emalloc(sizeof(command_buffer) + len + 1);

    buf->next = buf->prev = NULL;
    buf->len = len;

    if (data) {
        memcpy(buf->data, data, len);
    }

    buf->data[len] = '\0';
    return buf;
}

/**
 * Free all memory related to a single command buffer.
 * @param buf
 * Buffer to free.
 */
void command_buffer_free(command_buffer *buf)
{
    efree(buf);
}

/**
 * Enqueue a command buffer last in a queue.
 */
static void command_buffer_enqueue(command_buffer *buf, command_buffer **queue_start, command_buffer **queue_end)
{
    buf->next = NULL;
    buf->prev = *queue_end;

    if (*queue_start == NULL) {
        *queue_start = buf;
    }

    if (buf->prev) {
        buf->prev->next = buf;
    }

    *queue_end = buf;
}

/**
 * Enqueue a command buffer first in a queue.
 */
static void command_buffer_enqueue_first(command_buffer *buf, command_buffer **queue_start, command_buffer **queue_end)
{
    buf->next = *queue_start;
    buf->prev = NULL;

    if (*queue_end == NULL) {
        *queue_end = buf;
    }

    if (buf->next) {
        buf->next->prev = buf;
    }

    *queue_start = buf;
}

/**
 * Remove the first command buffer from a queue.
 */
static command_buffer *command_buffer_dequeue(command_buffer **queue_start, command_buffer **queue_end)
{
    command_buffer *buf = *queue_start;

    if (buf) {
        *queue_start = buf->next;

        if (buf->next) {
            buf->next->prev = NULL;
        } else {
            *queue_end = NULL;
        }
    }

    return buf;
}

void socket_send_packet(struct packet_struct *packet)
{
    HARD_ASSERT(packet != NULL);

    packet_struct *packet_meta = packet_new(0, 4, 0);
    if (socket_is_secure(csocket.sc)) {
        // TODO: figure out checksum_only flag
        packet = socket_crypto_encrypt(csocket.sc, packet, packet_meta, false);
        if (packet == NULL) {
            /* Logging already done. */
            cpl.state = ST_START;
            return;
        }
    } else {
        packet_append_uint16(packet_meta, packet->len + 1);
        packet_append_uint8(packet_meta, packet->type);
    }

    command_buffer *buf1 = command_buffer_new(packet_meta->len,
                                              packet_meta->data);
    packet_free(packet_meta);
    command_buffer *buf2 = command_buffer_new(packet->len,
                                              packet->data);
    packet_free(packet);

    SDL_LockMutex(output_buffer_mutex);
    command_buffer_enqueue(buf1, &output_queue_start, &output_queue_end);
    command_buffer_enqueue(buf2, &output_queue_start, &output_queue_end);
    SDL_CondSignal(output_buffer_cond);
    SDL_UnlockMutex(output_buffer_mutex);
}

/**
 * Get a command from the queue.
 * @return
 * The command (being removed from queue), NULL if there is no
 * command.
 */
command_buffer *get_next_input_command(void)
{
    command_buffer *buf;

    SDL_LockMutex(input_buffer_mutex);
    buf = command_buffer_dequeue(&input_queue_start, &input_queue_end);
    SDL_UnlockMutex(input_buffer_mutex);
    return buf;
}

void add_input_command(command_buffer *buf)
{
    SDL_LockMutex(input_buffer_mutex);
    command_buffer_enqueue_first(buf, &input_queue_start, &input_queue_end);
    SDL_CondSignal(input_buffer_cond);
    SDL_UnlockMutex(input_buffer_mutex);
}

static int reader_thread_loop(void *dummy)
{
    static uint8_t *readbuf = NULL;
    static int readbuf_size = 256;
    int readbuf_len = 0;
    int header_len = 0;
    int cmd_len = -1;

    if (!readbuf) {
        readbuf = emalloc(readbuf_size);
    }

    while (!abort_thread) {
        int toread;

        /* First, try to read a command length sequence */
        if (readbuf_len < 2) {
            /* Three-byte length? */
            if (readbuf_len > 0 && (readbuf[0] & 0x80)) {
                toread = 3 - readbuf_len;
            } else {
                toread = 2 - readbuf_len;
            }
        } else if (readbuf_len == 2 && (readbuf[0] & 0x80)) {
            toread = 1;
        } else {
            /* If we have a finished header, get the packet size from it. */
            if (readbuf_len <= 3) {
                uint8_t *p = readbuf;

                header_len = (*p & 0x80) ? 3 : 2;
                cmd_len = 0;

                if (header_len == 3) {
                    cmd_len += ((int) (*p++) & 0x7f) << 16;
                }

                cmd_len += ((int) (*p++)) << 8;
                cmd_len += ((int) (*p++));
            }

            toread = cmd_len + header_len - readbuf_len;

            if (readbuf_len + toread > readbuf_size) {
                uint8_t *tmp = readbuf;

                readbuf_size = readbuf_len + toread;
                readbuf = emalloc(readbuf_size);
                memcpy(readbuf, tmp, readbuf_len);
                efree(tmp);
            }
        }

        size_t amt;
        bool success = socket_read(csocket.sc, (void *) (readbuf + readbuf_len),
                toread, &amt);
        if (!success) {
            break;
        }

        readbuf_len += amt;
        network_graph_update(NETWORK_GRAPH_TYPE_GAME, NETWORK_GRAPH_TRAFFIC_RX,
                amt);

        /* Finished with a command? */
        if (readbuf_len == cmd_len + header_len && !abort_thread) {
            command_buffer *buf = command_buffer_new(readbuf_len - header_len,
                    readbuf + header_len);

            SDL_LockMutex(input_buffer_mutex);
            command_buffer_enqueue(buf, &input_queue_start, &input_queue_end);
            SDL_CondSignal(input_buffer_cond);
            SDL_UnlockMutex(input_buffer_mutex);

            cmd_len = -1;
            header_len = 0;
            readbuf_len = 0;
        }
    }

    client_socket_close(&csocket);

    if (readbuf != NULL) {
        efree(readbuf);
        readbuf = NULL;
    }

    return -1;
}

/**
 * Worker for the writer thread. It waits for enqueued outgoing packets
 * and sends them to the server as fast as it can.
 *
 * If any error is detected, the socket is closed and the thread exits. It is
 * up to them main thread to detect this and join() the worker threads.
 */
static int writer_thread_loop(void *dummy)
{
    command_buffer *buf = NULL;

    while (!abort_thread) {
        SDL_LockMutex(output_buffer_mutex);

        while (output_queue_start == NULL && !abort_thread) {
            SDL_CondWait(output_buffer_cond, output_buffer_mutex);
        }

        buf = command_buffer_dequeue(&output_queue_start, &output_queue_end);
        SDL_UnlockMutex(output_buffer_mutex);
        size_t written = 0;

        while (buf != NULL && written < buf->len && !abort_thread) {
            size_t amt;
            bool success = socket_write(csocket.sc, (const void *) (buf->data +
                    written), buf->len - written, &amt);
            if (!success) {
                break;
            }

            written += amt;
            network_graph_update(NETWORK_GRAPH_TYPE_GAME,
                    NETWORK_GRAPH_TRAFFIC_TX, amt);
        }

        if (buf != NULL) {
            command_buffer_free(buf);
            buf = NULL;
        }
    }

    client_socket_close(&csocket);
    return 0;
}

/**
 * Initialize and start up the worker threads.
 */
void socket_thread_start(void)
{
    if (input_buffer_cond == NULL) {
        input_buffer_cond = SDL_CreateCond();
        input_buffer_mutex = SDL_CreateMutex();
        output_buffer_cond = SDL_CreateCond();
        output_buffer_mutex = SDL_CreateMutex();
        socket_mutex = SDL_CreateMutex();
    }

    abort_thread = 0;

    input_thread = SDL_CreateThread(reader_thread_loop, NULL);

    if (input_thread == NULL) {
        LOG(ERROR, "Unable to start socket thread: %s", SDL_GetError());
        exit(1);
    }

    output_thread = SDL_CreateThread(writer_thread_loop, NULL);

    if (output_thread == NULL) {
        LOG(ERROR, "Unable to start socket thread: %s", SDL_GetError());
        exit(1);
    }
}

/**
 * Wait for the socket threads to finish.
 *
 * Closes the socket first, if it hasn't already been done.
 */
void socket_thread_stop(void)
{
    client_socket_close(&csocket);

    SDL_WaitThread(output_thread, NULL);
    SDL_WaitThread(input_thread, NULL);

    input_thread = output_thread = NULL;
}

/**
 * Detect and handle socket system shutdowns. Also reset the socket system
 * for a restart.
 *
 * The main thread should poll this function which detects connection
 * shutdowns and removes the threads if it happens.
 */
int handle_socket_shutdown(void)
{
    if (abort_thread) {
        socket_thread_stop();
        abort_thread = 0;

        /* Empty all queues */
        while (input_queue_start) {
            command_buffer_free(command_buffer_dequeue(&input_queue_start, &input_queue_end));
        }

        while (output_queue_start) {
            command_buffer_free(command_buffer_dequeue(&output_queue_start, &output_queue_end));
        }

        LOG(INFO, "Connection lost.");
        return 1;
    }

    return 0;
}

/**
 * Close a client socket.
 * @param csock
 * Socket to close.
 */
void client_socket_close(client_socket_t *csock)
{
    HARD_ASSERT(csock != NULL);

    SDL_LockMutex(socket_mutex);

    if (csock->sc != NULL) {
        socket_destroy(csock->sc);
        csock->sc = NULL;
    }

    abort_thread = 1;

    /* Poke anyone waiting at a cond */
    SDL_CondSignal(input_buffer_cond);
    SDL_CondSignal(output_buffer_cond);

    SDL_UnlockMutex(socket_mutex);
}

/**
 * Deinitialize the client sockets.
 */
void client_socket_deinitialize(void)
{
    if (csocket.sc != NULL) {
        client_socket_close(&csocket);
    }

#ifdef WIN32
    WSACleanup();
#endif
}

/**
 * Open a new socket.
 * @param csock
 * Socket to open.
 * @param host
 * Host to connect to.
 * @param port
 * Port to connect to.
 * @param secure
 * Whether the port is the secure port.
 * @return
 * True on success, false on failure.
 */
bool
client_socket_open (client_socket_t *csock,
                    const char      *host,
                    int              port,
                    bool             secure)
{
    HARD_ASSERT(csock != NULL);
    HARD_ASSERT(host != NULL);

    csock->sc = socket_create(host, port, secure);
    if (csock->sc == NULL) {
        return false;
    }

    if (!socket_connect(csock->sc)) {
        goto error;
    }

    if (!socket_opt_linger(csock->sc, true, 5)) {
        goto error;
    }

    if (setting_get_int(OPT_CAT_CLIENT, OPT_MINIMIZE_LATENCY)) {
        if (!socket_opt_ndelay(csock->sc, true)) {
            goto error;
        }
    }

    if (!socket_opt_recv_buffer(csock->sc, 65535)) {
        goto error;
    }

    return true;

error:
    socket_destroy(csock->sc);
    csock->sc = NULL;
    return false;
}
