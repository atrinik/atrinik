/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
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
#include <toolkit/packet.h>
#include <network_graph.h>

static SDL_Thread *io_thread;
static SDL_mutex *input_buffer_mutex;
static SDL_mutex *output_buffer_mutex;

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
command_buffer *command_buffer_new(size_t len, uint8_t *data) {
    command_buffer *buf = xmalloc(sizeof(command_buffer) + len + 1);

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
void command_buffer_free(command_buffer *buf) {
    free(buf);
}

/**
 * Enqueue a command buffer last in a queue.
 */
static void command_buffer_enqueue(command_buffer *buf,
                                   command_buffer **queue_start,
                                   command_buffer **queue_end) {
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
static void command_buffer_enqueue_first(command_buffer *buf,
                                         command_buffer **queue_start,
                                         command_buffer **queue_end) {
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
static command_buffer *command_buffer_dequeue(command_buffer **queue_start,
                                              command_buffer **queue_end) {
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

void socket_send_packet(struct packet_struct *packet) {
    HARD_ASSERT(packet != NULL);

    if (socket_mutex == NULL) {
        packet_free(packet);
        return;
    }

    SDL_LockMutex(socket_mutex);
    if (csocket.sc == NULL || abort_thread) {
        SDL_UnlockMutex(socket_mutex);
        packet_free(packet);
        return;
    }

    packet_struct *packet_meta = packet_new(0, 4, 0);
    packet_append_uint16(packet_meta, packet->len + 1);
    packet_append_uint8(packet_meta, packet->type);

    command_buffer *buf1 = command_buffer_new(packet_meta->len, packet_meta->data);
    packet_free(packet_meta);
    command_buffer *buf2 = command_buffer_new(packet->len, packet->data);
    packet_free(packet);
    SDL_UnlockMutex(socket_mutex);

    SDL_LockMutex(output_buffer_mutex);
    command_buffer_enqueue(buf1, &output_queue_start, &output_queue_end);
    command_buffer_enqueue(buf2, &output_queue_start, &output_queue_end);
    SDL_UnlockMutex(output_buffer_mutex);
}

/**
 * Get a command from the queue.
 * @return
 * The command (being removed from queue), NULL if there is no
 * command.
 */
command_buffer *get_next_input_command(void) {
    command_buffer *buf;

    SDL_LockMutex(input_buffer_mutex);
    buf = command_buffer_dequeue(&input_queue_start, &input_queue_end);
    SDL_UnlockMutex(input_buffer_mutex);
    return buf;
}

void add_input_command(command_buffer *buf) {
    SDL_LockMutex(input_buffer_mutex);
    command_buffer_enqueue_first(buf, &input_queue_start, &input_queue_end);
    SDL_UnlockMutex(input_buffer_mutex);
}

static bool socket_thread_aborted(void) {
    SDL_LockMutex(socket_mutex);
    bool aborted = abort_thread != 0;
    SDL_UnlockMutex(socket_mutex);
    return aborted;
}

/**
 * Single owner for all transport I/O and OpenSSL QUIC event handling.
 */
static int socket_io_thread_loop(void *dummy) {
    (void)dummy;

    int readbuf_size = 256;
    uint8_t *readbuf = xmalloc(readbuf_size);
    int readbuf_len = 0;
    int header_len = 0;
    int cmd_len = -1;
    command_buffer *output = NULL;
    size_t output_pos = 0;
    socket_t *sc = csocket.sc;

    while (!socket_thread_aborted()) {
        if (output == NULL) {
            SDL_LockMutex(output_buffer_mutex);
            output = command_buffer_dequeue(&output_queue_start, &output_queue_end);
            SDL_UnlockMutex(output_buffer_mutex);
            output_pos = 0;
            if (output != NULL && output->len == 0) {
                command_buffer_free(output);
                output = NULL;
            }
        }

        bool progressed = false;
        if (output != NULL) {
            size_t amt;
            if (!socket_write(sc, output->data + output_pos, output->len - output_pos, &amt)) {
                break;
            }
            if (amt != 0) {
                output_pos += amt;
                progressed = true;
                network_graph_update(NETWORK_GRAPH_TYPE_GAME, NETWORK_GRAPH_TRAFFIC_TX, amt);
                if (output_pos == output->len) {
                    command_buffer_free(output);
                    output = NULL;
                    output_pos = 0;
                }
            }
        }

        int toread;
        if (readbuf_len < 2) {
            if (readbuf_len > 0 && (readbuf[0] & 0x80)) {
                toread = 3 - readbuf_len;
            } else {
                toread = 2 - readbuf_len;
            }
        } else if (readbuf_len == 2 && (readbuf[0] & 0x80)) {
            toread = 1;
        } else {
            if (readbuf_len <= 3) {
                uint8_t *p = readbuf;
                header_len = (*p & 0x80) ? 3 : 2;
                cmd_len = 0;
                if (header_len == 3) {
                    cmd_len += ((int)(*p++) & 0x7f) << 16;
                }
                cmd_len += ((int)(*p++)) << 8;
                cmd_len += ((int)(*p++));
            }
            toread = cmd_len + header_len - readbuf_len;
            if (readbuf_len + toread > readbuf_size) {
                uint8_t *tmp = readbuf;
                readbuf_size = readbuf_len + toread;
                readbuf = xmalloc(readbuf_size);
                memcpy(readbuf, tmp, readbuf_len);
                free(tmp);
            }
        }

        size_t amt;
        if (!socket_read(sc, readbuf + readbuf_len, (size_t)toread, &amt)) {
            break;
        }
        if (amt != 0) {
            progressed = true;
            readbuf_len += (int)amt;
            network_graph_update(NETWORK_GRAPH_TYPE_GAME, NETWORK_GRAPH_TRAFFIC_RX, amt);

            if (readbuf_len == cmd_len + header_len && !socket_thread_aborted()) {
                command_buffer *input =
                    command_buffer_new(readbuf_len - header_len, readbuf + header_len);
                SDL_LockMutex(input_buffer_mutex);
                command_buffer_enqueue(input, &input_queue_start, &input_queue_end);
                SDL_UnlockMutex(input_buffer_mutex);
                cmd_len = -1;
                header_len = 0;
                readbuf_len = 0;
            }
        }

        if (!progressed) {
            bool write_pending = output != NULL;
            unsigned int timeout = socket_quic_timeout(sc, 20);
            bool ready = socket_wait(sc, true, write_pending, timeout);
            socket_quic_service(sc, ready, write_pending);
        }
    }

    if (output != NULL) {
        command_buffer_free(output);
    }
    free(readbuf);

    SDL_LockMutex(socket_mutex);
    if (csocket.sc == sc) {
        socket_destroy(csocket.sc);
        csocket.sc = NULL;
    }
    abort_thread = 1;
    SDL_UnlockMutex(socket_mutex);
    return 0;
}

/**
 * Initialize and start the transport I/O thread.
 */
void socket_thread_start(void) {
    if (socket_mutex == NULL) {
        input_buffer_mutex = SDL_CreateMutex();
        output_buffer_mutex = SDL_CreateMutex();
        socket_mutex = SDL_CreateMutex();
    }

    abort_thread = 0;
    io_thread = SDL_CreateThread(socket_io_thread_loop, NULL);
    if (io_thread == NULL) {
        LOG(ERROR, "Unable to start socket thread: %s", SDL_GetError());
        exit(1);
    }
}

/**
 * Wait for the socket thread to finish.
 *
 * Closes the socket first, if it hasn't already been done.
 */
void socket_thread_stop(void) {
    client_socket_close(&csocket);
    if (io_thread != NULL) {
        SDL_WaitThread(io_thread, NULL);
        io_thread = NULL;
    }
}

/**
 * Detect and handle socket system shutdowns. Also reset the socket system
 * for a restart.
 *
 * The main thread should poll this function which detects connection
 * shutdowns and removes the threads if it happens.
 */
int handle_socket_shutdown(void) {
    if (socket_mutex != NULL && socket_thread_aborted()) {
        socket_thread_stop();
        SDL_LockMutex(socket_mutex);
        abort_thread = 0;
        SDL_UnlockMutex(socket_mutex);

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
void client_socket_close(client_socket_t *csock) {
    HARD_ASSERT(csock != NULL);

    if (socket_mutex == NULL) {
        if (csock->sc != NULL) {
            socket_destroy(csock->sc);
            csock->sc = NULL;
        }
        abort_thread = 1;
        return;
    }

    SDL_LockMutex(socket_mutex);
    abort_thread = 1;
    if (io_thread == NULL && csock->sc != NULL) {
        socket_destroy(csock->sc);
        csock->sc = NULL;
    }
    SDL_UnlockMutex(socket_mutex);
}

/**
 * Deinitialize the client sockets.
 */
void client_socket_deinitialize(void) {
    if (io_thread != NULL) {
        socket_thread_stop();
    } else if (csocket.sc != NULL) {
        client_socket_close(&csocket);
    }
    if (input_buffer_mutex != NULL) {
        SDL_DestroyMutex(input_buffer_mutex);
        input_buffer_mutex = NULL;
    }
    if (output_buffer_mutex != NULL) {
        SDL_DestroyMutex(output_buffer_mutex);
        output_buffer_mutex = NULL;
    }
    if (socket_mutex != NULL) {
        SDL_DestroyMutex(socket_mutex);
        socket_mutex = NULL;
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
 * @param quic_certificate_sha256
 * Expected SHA-256 certificate fingerprint.
 * @return
 * True on success, false on failure.
 */
bool client_socket_open(client_socket_t *csock,
                        const char *host,
                        int port,
                        const char *quic_certificate_sha256,
                        socket_connection_preference_t preference) {
    HARD_ASSERT(csock != NULL);
    HARD_ASSERT(host != NULL);

    SOFT_ASSERT_RC(quic_certificate_sha256 != NULL, false, "Missing QUIC certificate fingerprint");

    char rendezvous_url[HUGE_BUF];
    const char *rendezvous =
        metaserver_rendezvous_url(selected_server, VS(rendezvous_url)) ? rendezvous_url : NULL;
    csock->sc = socket_quic_client_create(host,
                                          port,
                                          quic_certificate_sha256,
                                          rendezvous,
                                          clioption_settings.stun_server,
                                          preference);
    if (csock->sc == NULL) {
        return false;
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
