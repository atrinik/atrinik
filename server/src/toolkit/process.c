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

#ifndef WIN32
#   include <sys/wait.h>
#   include <poll.h>
#   include <spawn.h>
#endif

#include "toolkit/process.h"

TOOLKIT_API(IMPORTS(logger));

enum {
    PROCESS_PIPE_IN, ///< Input pipe.
    PROCESS_PIPE_OUT, ///< Output pipe.
    PROCESS_PIPE_ERR, ///< Error pipe.

    PROCESS_PIPE_NUM ///< Total number of pipes.
};

/**
 * Platform-independent pipe representation.
 */
#ifndef WIN32
    typedef int process_pipe_t;
#else
    typedef HANDLE process_pipe_t;
#endif

/**
 * Value used for an invalid process_pipe_t.
 */
#ifndef WIN32
#   define PROCESS_PIPE_INVALID -1
#else
#   define PROCESS_PIPE_INVALID NULL
#endif

/**
 * Value used for an invalid process_pipe_t.
 */
#ifndef WIN32
#   define PROCESS_PIPE_CLOSE(pipe) do { close(pipe); } while (0)
#else
#   define PROCESS_PIPE_CLOSE(pipe) do { CloseHandle(pipe); } while (0)
#endif

/**
 * Structure representing a single process.
 */
struct process {
    process_t *next; ///< Next process in a linked list.
    process_t *prev; ///< Previous process in a linked list.

    uint32_t uid; ///< Unique process ID.

#ifndef WIN32
    int pid; ///< Process identifier.
#else
    PROCESS_INFORMATION pi; ///< Process information.
#endif
    process_pipe_t pipes[PROCESS_PIPE_NUM]; ///< Communication pipes.

    packet_struct *packets; ///< Outgoing packets.

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

    static uint32_t uid = 0;

    process_t *process = ecalloc(1, sizeof(*process));
    process->uid = uid++;
    process_add_arg(process, executable);

#ifndef WIN32
    process->pid = -1;
#endif

    for (int i = 0; i < PROCESS_PIPE_NUM; i++) {
        process->pipes[i] = PROCESS_PIPE_INVALID;
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
#ifndef WIN32
    snprintf(VS(buf), "PID %d", process->pid);
#else
    snprintf(VS(buf), "PID %" PRIu64 " (handle: %p)",
             (uint64_t) process->pi.dwProcessId,
             process->pi.hProcess);
#endif

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

#ifndef WIN32
    ssize_t ret = write(process->pipes[PROCESS_PIPE_IN],
                        packet->data,
                        packet->len);
#else
    DWORD ret = 0;
    WriteFile(process->pipes[PROCESS_PIPE_IN],
              packet->data + packet->pos,
              packet->len - packet->pos,
              &ret,
              0);
#endif
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

    enum {
        PROCESS_COMM_PIPE_RX,
        PROCESS_COMM_PIPE_TX,
        PROCESS_COMM_PIPE_NUM
    };

    /* Initialize the default pipe values. */
    process_pipe_t pipes[PROCESS_PIPE_NUM][PROCESS_COMM_PIPE_NUM];
    for (int i = 0; i < PROCESS_PIPE_NUM; i++) {
        for (int j = 0; j < PROCESS_COMM_PIPE_NUM; j++) {
            pipes[i][j] = PROCESS_PIPE_INVALID;
        }
    }

    /* Create the pipes. */
    for (int i = 0; i < PROCESS_PIPE_NUM; i++) {
#ifndef WIN32
        if (pipe(pipes[i]) != 0) {
            LOG(ERROR,
                "Failed to create a pipe: %s (%d)",
                strerror(errno),
                errno);
            goto error;
        }
#else
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;

        char pipe_name[MAX_BUF];
        snprintf(VS(pipe_name), "\\\\.\\pipe\\%u\\%d", process->uid, i);

        process_pipe_t *pipe_rx_tmp =
            CreateNamedPipe(pipe_name,
                            PIPE_ACCESS_INBOUND,
                            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE |
                                PIPE_NOWAIT,
                            1,
                            4096,
                            4096,
                            0,
                            &sa);
        if (pipe_rx_tmp == NULL) {
            LOG(ERROR, "Failed to create a named pipe.");
            goto error;
        }

        pipes[i][PROCESS_COMM_PIPE_TX] =
            CreateFile(pipe_name,
                       FILE_WRITE_DATA | SYNCHRONIZE,
                       0,
                       &sa,
                       OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL,
                       0);
        if (pipes[i][PROCESS_COMM_PIPE_TX] == NULL) {
            LOG(ERROR, "Failed to create a file handle.");
            CloseHandle(pipe_rx_tmp);
            goto error;
        }

        if (!DuplicateHandle(GetCurrentProcess(),
                             pipe_rx_tmp,
                             GetCurrentProcess(),
                             &pipes[i][PROCESS_COMM_PIPE_RX],
                             0,
                             FALSE,
                             DUPLICATE_SAME_ACCESS)) {
            LOG(ERROR, "Failed to duplicate handle.");
            CloseHandle(pipe_rx_tmp);
            goto error;
        }

        CloseHandle(pipe_rx_tmp);
#endif
    }

    /* Parent side of the pipes */
    process->pipes[PROCESS_PIPE_IN] =
        pipes[PROCESS_PIPE_IN][PROCESS_COMM_PIPE_TX];
    process->pipes[PROCESS_PIPE_OUT] =
        pipes[PROCESS_PIPE_OUT][PROCESS_COMM_PIPE_RX];
    process->pipes[PROCESS_PIPE_ERR] =
        pipes[PROCESS_PIPE_ERR][PROCESS_COMM_PIPE_RX];

    /* Configure the pipes to be non-blocking. */
    for (int i = 0; i < PROCESS_PIPE_NUM; i++) {
#ifndef WIN32
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
            goto error;
        }
#else
        COMMTIMEOUTS timeouts = {10, 0, 10, 0, 10};
        SetCommTimeouts(process->pipes[i], &timeouts);
#endif
    }

#ifndef WIN32
    int pin_rx = pipes[PROCESS_PIPE_IN][PROCESS_COMM_PIPE_RX];
    int pin_tx = pipes[PROCESS_PIPE_IN][PROCESS_COMM_PIPE_TX];
    int pout_rx = pipes[PROCESS_PIPE_OUT][PROCESS_COMM_PIPE_RX];
    int pout_tx = pipes[PROCESS_PIPE_OUT][PROCESS_COMM_PIPE_TX];
    int perr_rx = pipes[PROCESS_PIPE_ERR][PROCESS_COMM_PIPE_RX];
    int perr_tx = pipes[PROCESS_PIPE_ERR][PROCESS_COMM_PIPE_TX];

    /* Set up the child side of the pipes. */
    posix_spawn_file_actions_t actions;

    posix_spawn_file_actions_init(&actions);

    posix_spawn_file_actions_addclose(&actions, pin_tx);
    posix_spawn_file_actions_addclose(&actions, pout_rx);
    posix_spawn_file_actions_addclose(&actions, perr_rx);

    posix_spawn_file_actions_adddup2(&actions, pin_rx, STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, pout_tx, STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, perr_tx, STDERR_FILENO);

    posix_spawn_file_actions_addclose(&actions, pin_rx);
    posix_spawn_file_actions_addclose(&actions, pout_tx);
    posix_spawn_file_actions_addclose(&actions, perr_tx);

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
#else
    StringBuffer *sb = stringbuffer_new();
    for (size_t i = 0; i < process->num_args; i++) {
        stringbuffer_append_printf(sb,
                                   "%s%s",
                                   i != 0 ? " " : "",
                                   process->args[i]);
    }

    char *cmdline = stringbuffer_finish(sb);

    STARTUPINFO si;
    memset(&si, 0, sizeof(si));

    si.cb = sizeof(STARTUPINFO);
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = pipes[PROCESS_PIPE_IN][PROCESS_COMM_PIPE_RX];
    si.hStdOutput = pipes[PROCESS_PIPE_OUT][PROCESS_COMM_PIPE_TX];
    si.hStdError = pipes[PROCESS_PIPE_ERR][PROCESS_COMM_PIPE_TX];
    si.wShowWindow = SW_HIDE;

    if (!CreateProcess(NULL,
                       cmdline,
                       NULL,
                       NULL,
                       true,
                       0,
                       NULL,
                       NULL,
                       &si,
                       &process->pi)) {
        LOG(ERROR, "CreateProcess() failed");
        goto error;
    }

    efree(cmdline);
#endif

    PROCESS_PIPE_CLOSE(pipes[PROCESS_PIPE_IN][PROCESS_COMM_PIPE_RX]);
    PROCESS_PIPE_CLOSE(pipes[PROCESS_PIPE_OUT][PROCESS_COMM_PIPE_TX]);
    PROCESS_PIPE_CLOSE(pipes[PROCESS_PIPE_ERR][PROCESS_COMM_PIPE_TX]);

    process->running = true;
    LOG(INFO, "Started new process: %s", process_get_str(process));

    return true;

error:
    for (int i = 0; i < PROCESS_PIPE_NUM; i++) {
        for (int j = 0 ; j < PROCESS_COMM_PIPE_NUM; j++) {
            if (pipes[i][j] != PROCESS_PIPE_INVALID) {
                PROCESS_PIPE_CLOSE(pipes[i][j]);
            }
        }
    }

#ifndef WIN32
    process->pid = -1;
#else
    efree(cmdline);
    memset(&process->pi, 0, sizeof(process->pi));
#endif

    for (int i = 0; i < PROCESS_PIPE_NUM; i++) {
        process->pipes[i] = PROCESS_PIPE_INVALID;
    }

    return false;
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
        PROCESS_PIPE_CLOSE(process->pipes[i]);
        process->pipes[i] = PROCESS_PIPE_INVALID;
    }

#ifndef WIN32
    process->pid = -1;
#else
    CloseHandle(process->pi.hProcess);
    CloseHandle(process->pi.hThread);

    memset(&process->pi, 0, sizeof(process->pi));
#endif

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

#ifndef WIN32
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
#else
    TerminateProcess(process->pi.hProcess, 0);
    process_cleanup(process);
#endif
}

static void
process_check_internal_pipe (process_t     *process,
                             int            pipe_type,
                             process_pipe_t fd)
{
    HARD_ASSERT(process != NULL);

    switch (pipe_type) {
    case PROCESS_PIPE_OUT:
    case PROCESS_PIPE_ERR: {
        uint8_t buf[HUGE_BUF];
#ifndef WIN32
        ssize_t ret = read(fd, VS(buf));
#else
        DWORD ret = 0;
        ReadFile(fd, VS(buf), &ret, 0);
#endif
        if (ret > 0) {
            switch (pipe_type) {
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
#ifndef WIN32
            ssize_t ret = write(fd,
                                packet->data + packet->pos,
                                packet->len - packet->pos);
#else
            DWORD ret = 0;
            WriteFile(fd,
                      packet->data + packet->pos,
                      packet->len - packet->pos,
                      &ret,
                      0);
#endif
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

#ifndef WIN32

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
            process_check_internal_pipe(process, i, fds[*idx].fd);
        }

        (*idx)++;
    }
}

#endif

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

#ifndef WIN32
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
#else
    DWORD exit_code;
    if (!GetExitCodeProcess(process->pi.hProcess, &exit_code)) {
        LOG(ERROR,
            "GetExitCodeProcess() failed for %s",
            process_get_str(process));
        return;
    }

    /* If the exit code is not STILL_ACTIVE, the process has exited (unless
     * it exited with the STILL_ACTIVE exit code -- thanks Microsoft...) */
    if (exit_code != STILL_ACTIVE) {
        process_cleanup(process);
    }
#endif
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

#ifndef WIN32
    nfds_t nfds = 0;
    struct pollfd *fds = NULL;

    process_check_internal_begin(process, &fds, &nfds);

    if (process_check_internal_poll(fds, nfds)) {
        size_t idx = 0;
        process_check_internal_end(process, fds, nfds, &idx);
    }
#endif

    process_check_internal_exit(process);

#ifdef WIN32
    for (int i = 0; i < PROCESS_PIPE_NUM; i++) {
        process_check_internal_pipe(process, i, process->pipes[i]);
    }
#endif
}

/**
 * Check all processes for errors, read data from them and send any outgoing
 * packets if necessary.
 */
void
process_check_all (void)
{
    TOOLKIT_PROTECT();

#ifndef WIN32
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
#else
    process_t *process;
    DL_FOREACH(processes, process) {
        process_check(process);
    }
#endif
}
