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
 * Process API.
 *
 * @author
 * Alex Tokar
 */

#include <global.h>
#include <toolkit_string.h>

#include <sys/wait.h>
#include <poll.h>
#include <spawn.h>

#include "toolkit/process.h"

TOOLKIT_API(IMPORTS(logger));

enum {
    PROCESS_PIPE_IN, ///< Input pipe.
    PROCESS_PIPE_OUT, ///< Output pipe.
    PROCESS_PIPE_ERR, ///< Error pipe.

    PROCESS_PIPE_NUM ///< Total number of pipes.
};

/**
 * Structure representing a single process.
 */
struct process {
    int pipes[PROCESS_PIPE_NUM]; ///< Communication pipes.

    packet_struct *packets; ///< Outgoing packets.

    process_t *next; ///< Next process in a linked list.
    process_t *prev; ///< Previous process in a linked list.

    int pid; ///< Process identifier.
    char **args; ///< Arguments for starting the process.
    size_t num_args; ///< Number of arguments.

    bool restart:1; ///< Whether the process should restart automatically.
    bool running:1; ///< Whether the process is currently running.

    /**
     * Callback for data read from the out pipe.
     */
    process_data_callback_t data_out_cb;

    /**
     * Callback for data read from the error pipe.
     */
    process_data_callback_t data_err_cb;
};

/**
 * Linked list of created processes.
 */
static process_t *processes = NULL;

/**
 * Initialize the process API.
 */
TOOLKIT_INIT_FUNC(process)
{
    processes = NULL;
}
TOOLKIT_INIT_FUNC_FINISH

/**
 * Deinitialize the process API.
 */
TOOLKIT_DEINIT_FUNC(process)
{
    process_t *process, *tmp;
    DL_FOREACH_SAFE(processes, process, tmp) {
        DL_DELETE(processes, process);
        process_free(process);
    }
}
TOOLKIT_DEINIT_FUNC_FINISH

/**
 * Creates a new process. It should be started with process_start().
 *
 * @param executable
 * Executable for the process.
 * @return
 * Pointer to the created process, never NULL.
 */
process_t *
process_create (const char *executable)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(executable != NULL);

    process_t *process = ecalloc(1, sizeof(*process));
    process_add_arg(process, executable);

    process->pid = -1;

    for (int i = 0; i < PROCESS_PIPE_NUM; i++) {
        process->pipes[i] = -1;
    }

    DL_APPEND(processes, process);

    return process;
}

/**
 * Frees the specified process and all data associated with it.
 *
 * If the process is still running, it will be forcefully stopped using
 * process_stop().
 *
 * @param process
 * Process to free.
 */
void
process_free (process_t *process)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(process != NULL);

    if (process->running) {
        /* We don't want it to restart. */
        process->restart = false;
        process_stop(process);
    }

    for (size_t i = 0; i < process->num_args; i++) {
        efree(process->args[i]);
    }

    efree(process->args);
    efree(process);
}

/**
 * Add an argument for the process to start with.
 *
 * @param process
 * Process.
 * @param arg
 * Argument to add.
 */
void
process_add_arg (process_t *process, const char *arg)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(process != NULL);
    HARD_ASSERT(arg != NULL);

    /* +2 = 1 for the new argument, 1 for the NULL */
    process->args = erealloc(process->args,
                             sizeof(*process->args) * (process->num_args + 2));
    process->args[process->num_args] = estrdup(arg);
    process->args[process->num_args + 1] = NULL;
    process->num_args++;
}

/**
 * Acquire a string representation of the specified process.
 *
 * @param process
 * Process.
 * @return
 * String representation of the process. Uses a static buffer.
 */
const char *
process_get_str (process_t *process)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(process != NULL);

    static char buf[HUGE_BUF];
    snprintf(VS(buf), "PID %d", process->pid);

    for (size_t i = 0; i < process->num_args; i++) {
        snprintfcat(VS(buf), " %s", process->args[i]);
    }

    return buf;
}

/**
 * Enable/disable automatic restart of the specified process.
 *
 * @param process
 * Process.
 * @param val
 * True to enable automatic restart, false to disable.
 */
void
process_set_restart (process_t *process, bool val)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(process != NULL);

    process->restart = !!val;
}

/**
 * Set a callback function to use for data received from the out pipe.
 *
 * @param process
 * Process.
 * @param cb
 * Callback to use.
 */
void
process_set_data_out_cb (process_t *process, process_data_callback_t cb)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(process != NULL);

    process->data_out_cb = cb;
}

/**
 * Set a callback function to use for data received from the error pipe.
 *
 * @param process
 * Process.
 * @param cb
 * Callback to use.
 */
void
process_set_data_err_cb (process_t *process, process_data_callback_t cb)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(process != NULL);

    process->data_err_cb = cb;
}

/**
 * Find out whether the specified process is running.
 *
 * @param process
 * Process.
 * @return
 * True if the process is running, false otherwise.
 */
bool
process_is_running (process_t *process)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(process != NULL);

    return process->running;
}

/**
 * Sends a packet to the specified process. It is an error if the process
 * is not running.
 *
 * @param process
 * Process.
 * @param packet
 * Packet to send.
 */
void
process_send (process_t *process, packet_struct *packet)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(process != NULL);
    HARD_ASSERT(packet != NULL);

    SOFT_ASSERT(process->running,
                "The process %s has not been started.",
                process_get_str(process));

    ssize_t ret = write(process->pipes[PROCESS_PIPE_IN],
                        packet->data,
                        packet->len);
    if (ret <= 0) {
        /* Failed to write to the pipe; add it to the outgoing packets queue.
         * For now, we don't care about errors here; they will be checked
         * by poll() in process_check_all(). */
        DL_APPEND(process->packets, packet);
        return;
    }

    /* Can never be signed here. */
    size_t written = ret;
    if (written == packet->len) {
        /* Everything was sent out immediately. */
        packet_free(packet);
        return;
    }

    /* Mark the position to write from the next time, and add it to the
     * outgoing packets queue. */
    packet->pos = written;
    DL_APPEND(process->packets, packet);
}

/**
 * Start the specified process. It is an error if the process is already
 * running.
 *
 * @param process
 * Process to start.
 * @return
 * True on success, false on failure.
 */
bool
process_start (process_t *process)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(process != NULL);

    SOFT_ASSERT_RC(!process->running,
                   false,
                   "The process %s is already started.",
                   process_get_str(process));

#ifndef WIN32
    int pin[2] = {-1, -1};
    int pout[2] = {-1, -1};
    int perr[2] = {-1, -1};

    if (pipe(pin) != 0) {
        LOG(ERROR,
            "Failed to create pipe for input: %s (%d)",
            strerror(errno),
            errno);
        goto error;
    }

    if (pipe(pout) != 0) {
        LOG(ERROR,
            "Failed to create pipe for output: %s (%d)",
            strerror(errno),
            errno);
        goto error;
    }

    if (pipe(perr) != 0) {
        LOG(ERROR,
            "Failed to create pipe for errors: %s (%d)",
            strerror(errno),
            errno);
        goto error;
    }

    /* Parent side of the pipes */
    process->pipes[PROCESS_PIPE_IN] = pin[1];
    process->pipes[PROCESS_PIPE_OUT] = pout[0];
    process->pipes[PROCESS_PIPE_ERR] = perr[0];

    /* Configure the pipes to be non-blocking. */
    for (int i = 0; i < PROCESS_PIPE_NUM; i++) {
#ifdef WIN32
        u_long flag = 1;
        if (ioctlsocket(process->pipes[i], FIONBIO, &flag) == -1) {
            LOG(ERROR,
                "Cannot ioctlsocket(): %s (%d)",
                s_strerror(s_errno),
                s_errno);
#else
        int flags = fcntl(process->pipes[i], F_GETFL);
        if (flags == -1) {
            LOG(ERROR, "Cannot fcntl(F_GETFL): %s (%d)",
                s_strerror(s_errno),
                s_errno);
            goto error;
        }

        flags |= O_NDELAY | O_NONBLOCK;

        if (fcntl(process->pipes[i], F_SETFL, flags) == -1) {
            LOG(ERROR, "Cannot fcntl(F_SETFL), flags %d: %s (%d)",
                flags,
                s_strerror(s_errno),
                s_errno);
#endif
            goto error;
        }
    }

    /* Set up the child side of the pipes. */
    posix_spawn_file_actions_t actions;

    posix_spawn_file_actions_init(&actions);

    posix_spawn_file_actions_addclose(&actions, pin[1]);
    posix_spawn_file_actions_addclose(&actions, pout[0]);
    posix_spawn_file_actions_addclose(&actions, perr[0]);

    posix_spawn_file_actions_adddup2(&actions, pin[0], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, pout[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, perr[1], STDERR_FILENO);

    posix_spawn_file_actions_addclose(&actions, pin[0]);
    posix_spawn_file_actions_addclose(&actions, pout[1]);
    posix_spawn_file_actions_addclose(&actions, perr[1]);

    /* Spawn the process. */
    if (posix_spawnp(&process->pid,
                     process->args[0],
                     &actions,
                     NULL,
                     process->args,
                     NULL) != 0) {
        LOG(ERROR,
            "Failed to start %s: %s (%d)",
            process_get_str(process),
            strerror(errno),
            errno);
        goto error;
    }

    close(pin[0]);
    close(pout[1]);
    close(perr[1]);

    process->running = true;
    LOG(INFO, "Started new process: %s", process_get_str(process));

    return true;

error:
    for (size_t i = 0; i < arraysize(pin); i++) {
        if (pin[i] != -1) {
            close(pin[i]);
        }

        if (pout[i] != -1) {
            close(pin[i]);
        }

        if (perr[i] != -1) {
            close(pin[i]);
        }
    }

    process->pid = -1;

    for (int i = 0; i < PROCESS_PIPE_NUM; i++) {
        process->pipes[i] = -1;
    }

    return false;
#else

    return false;
#endif
}

/**
 * Cleanup the specified process due to exiting and attempt to restart it
 * if appropriate.
 *
 * @param process
 * Process.
 */
static void
process_cleanup (process_t *process)
{
    HARD_ASSERT(process != NULL);

    SOFT_ASSERT(process->running,
                "The process %s has not been started.",
                process_get_str(process));

    LOG(INFO, "Process has exited: %s", process_get_str(process));

    for (int i = 0; i < PROCESS_PIPE_NUM; i++) {
        close(process->pipes[i]);
        process->pipes[i] = -1;
    }

    process->pid = -1;
    process->running = false;

    if (process->restart) {
        if (!process_start(process)) {
            LOG(ERROR, "Failed to re-start %s.", process_get_str(process));
        }
    }
}

/**
 * Forcefully stop the specified running process.
 *
 * @param process
 * Process to stop. Must be running.
 */
void
process_stop (process_t *process)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(process != NULL);

    SOFT_ASSERT(process->running,
                "The process %s has not been started.",
                process_get_str(process));

    LOG(INFO, "Stopping process: %s", process_get_str(process));

    /* Check the process; it may have exited in the meantime. */
    process_check(process);

    if (!process->running) {
        /* The checking function has stopped it, nothing else to do. */
        return;
    }

    if (kill(process->pid, SIGTERM) != 0) {
        LOG(ERROR,
            "kill() failed for process %s: %s (%d)",
            process_get_str(process),
            strerror(errno),
            errno);
        goto out;
    }

    usleep(100000);

    /* Check if the process has exited. */
    int status = -1;
    pid_t result = waitpid(process->pid, &status, WNOHANG);
    if (result == -1) {
        LOG(ERROR,
            "waitpid() failed for %s: %s (%d)",
            process_get_str(process),
            strerror(errno),
            errno);
        goto out;
    }

    /* Non-zero value means the child has exited. */
    if (result != 0) {
        process_cleanup(process);
        return;
    }

out:
    /* Last attempt */
    (void) kill(process->pid, SIGKILL);
    process_cleanup(process);
}

/**
 * Begin the setup of the file descriptors set for polling purposes. Used by
 * the process checking functions.
 *
 * @param process
 * Process.
 * @param[out] fds
 * Set of file descriptors that will be polled.
 * @param[out] nfds
 * Number of file descriptors in the set.
 */
static void
process_check_internal_begin (process_t      *process,
                              struct pollfd **fds,
                              nfds_t         *nfds)
{
    HARD_ASSERT(process != NULL);
    HARD_ASSERT(fds != NULL);
    HARD_ASSERT(nfds != NULL);

    /* Set up events for all the pipes of the process that will be
     * polled. */
    *fds = realloc(*fds, sizeof(**fds) * ((*nfds) + PROCESS_PIPE_NUM));
    (*fds)[*nfds].fd = process->pipes[PROCESS_PIPE_IN];
    (*fds)[*nfds].events = process->packets != NULL ? POLLOUT : 0;
    (*nfds)++;
    (*fds)[*nfds].fd = process->pipes[PROCESS_PIPE_OUT];
    (*fds)[*nfds].events = POLLIN;
    (*nfds)++;
    (*fds)[*nfds].fd = process->pipes[PROCESS_PIPE_ERR];
    (*fds)[*nfds].events = POLLIN;
    (*nfds)++;
}

/**
 * Polls the specified file descriptors. Used by the process checking
 * functions.
 *
 * @param fds
 * Set of file descriptors to poll.
 * @param nfds
 * Number of file descriptors in the set.
 * @return
 * True if any FDs have events waiting to be processed, false otherwise.
 */
static bool
process_check_internal_poll (struct pollfd *fds,
                             nfds_t         nfds)
{
    if (fds == NULL || nfds == 0) {
        return false;
    }

    int ready = poll(fds, nfds, 0);
    if (unlikely(ready == -1)) {
        LOG(ERROR,
            "poll() returned an error: %s (%d)",
            strerror(errno),
            errno);
        return false;
    }

    /* No FDs that need processing. */
    if (ready == 0) {
        return false;
    }

    return true;
}

/**
 * Perform processing after polling the specified set of file descriptors.
 * Used by the process checking functions.
 *
 * @param process
 * Process.
 * @param fds
 * Set of file descriptors.
 * @param nfds
 * Number of file descriptors in the set.
 * @param[out] idx
 * Index in the file descriptors set. Will be incremented for each processed
 * file descriptor.
 */
static void
process_check_internal_end (process_t           *process,
                            const struct pollfd *fds,
                            nfds_t               nfds,
                            size_t              *idx)
{
    HARD_ASSERT(process != NULL);
    HARD_ASSERT(fds != NULL);
    HARD_ASSERT(idx != NULL);

    for (int i = 0; i < PROCESS_PIPE_NUM; i++) {
        if (*idx == nfds) {
            LOG(ERROR,
                "Inconsistency detected, reached more than %u FDs",
                (unsigned) *idx);
            break;
        }

        if (fds[*idx].revents & (POLLERR | POLLHUP)) {
            LOG(ERROR,
                "Pipe error on FD %u (pipe: %d): %s",
                (unsigned) *idx,
                i,
                process_get_str(process));
        } else {

            switch (i) {
            case PROCESS_PIPE_OUT:
            case PROCESS_PIPE_ERR: {
                uint8_t buf[HUGE_BUF];
                ssize_t ret = read(fds[*idx].fd, VS(buf));
                if (ret > 0) {
                    switch (i) {
                    case PROCESS_PIPE_OUT:
                        if (process->data_out_cb != NULL) {
                            process->data_out_cb(process, buf, ret);
                        }

                        break;

                    case PROCESS_PIPE_ERR:
                        if (process->data_err_cb != NULL) {
                            process->data_err_cb(process, buf, ret);
                        }

                        break;

                    default:
                        IMPOSSIBLE();
                        break;
                    }
                }

                break;
            }

            case PROCESS_PIPE_IN:
                while (process->packets != NULL) {
                    packet_struct *packet = process->packets;
                    ssize_t ret = write(fds[*idx].fd,
                                        packet->data + packet->pos,
                                        packet->len - packet->pos);
                    if (ret <= 0) {
                        break;
                    }

                    /* Cannot be signed here. */
                    size_t written = ret;
                    packet->pos += written;

                    if (packet->pos == packet->len) {
                        /* The entire packet was sent to the process, delete
                         * it from the queue and free it. */
                        DL_DELETE(process->packets, packet);
                        packet_free(packet);
                    } else {
                        /* Otherwise only a part of it was sent, so no reason
                         * to keep going. */
                        break;
                    }
                }

                break;

            default:
                IMPOSSIBLE();
                break;
            }
        }

        (*idx)++;
    }
}

/**
 * Check if the specified process has exited. Used by the process checking
 * functions.
 *
 * @param process
 * Process.
 */
static void
process_check_internal_exit (process_t *process)
{
    HARD_ASSERT(process != NULL);

    if (!process->running) {
        return;
    }

    /* Check if the process has exited. */
    int status = -1;
    pid_t result = waitpid(process->pid, &status, WNOHANG);
    if (result == -1) {
        LOG(ERROR,
            "waitpid() failed for %s: %s (%d)",
            process_get_str(process),
            strerror(errno),
            errno);
        return;
    }

    /* Non-zero value means the child has exited. */
    if (result != 0) {
        process_cleanup(process);
    }
}

/**
 * Check the specified process for errors, read data from it and send any
 * outgoing packets if necessary.
 *
 * The process must have been started.
 *
 * @param process
 * Process to check.
 */
void
process_check (process_t *process)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(process != NULL);

    SOFT_ASSERT(process->running,
                "The process %s has not been started.",
                process_get_str(process));

    nfds_t nfds = 0;
    struct pollfd *fds = NULL;

    process_check_internal_begin(process, &fds, &nfds);

    if (process_check_internal_poll(fds, nfds)) {
        size_t idx = 0;
        process_check_internal_end(process, fds, nfds, &idx);
    }

    process_check_internal_exit(process);
}

/**
 * Check all processes for errors, read data from them and send any outgoing
 * packets if necessary.
 */
void
process_check_all (void)
{
    TOOLKIT_PROTECT();

    struct pollfd *fds = NULL;
    nfds_t nfds = 0;

    process_t *process;
    DL_FOREACH(processes, process) {
        if (!process->running) {
            continue;
        }

        process_check_internal_begin(process, &fds, &nfds);
    }

    if (process_check_internal_poll(fds, nfds)) {
        /* It is important that this loop keeps the same logic/checks as the
         * one above, otherwise inconsistencies are bound to happen. */
        nfds_t idx = 0;
        DL_FOREACH(processes, process) {
            if (!process->running) {
                continue;
            }

            process_check_internal_end(process, fds, nfds, &idx);
        }
    }

    DL_FOREACH(processes, process) {
        process_check_internal_exit(process);
    }
}
