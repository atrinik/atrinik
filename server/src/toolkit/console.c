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
 * Interactive console API.
 *
 * In order to add a new command to the array of console's available
 * commands, use console_command_add().
 *
 * The API automatically creates the 'help' command at initialization
 * time.
 *
 * In order to actually allow usage of the console through terminal's
 * standard input, you will need to call console_command_handle() in your
 * program's main loop every iteration.
 *
 * @author Alex Tokar */

#include <global.h>
#include <toolkit_string.h>

#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

/**
 * Dynamic array of all the possible console commands. */
static console_command_struct *console_commands;

/**
 * Number of ::console_commands. */
static size_t console_commands_num;

/**
 * Command process queue. */
static UT_array *command_process_queue;

/**
 * Mutex protecting command command process queue. */
static pthread_mutex_t command_process_queue_mutex;

/**
 * The thread's ID. */
static pthread_t thread_id;

#ifdef HAVE_READLINE
/**
 * Prompt for readline. */
static const char *current_prompt;

static pthread_mutex_t rl_mutex; ///< Mutex for readline in general.
#endif

static void console_command_help(const char *params);

TOOLKIT_API(DEPENDS(logger), DEPENDS(memory), DEPENDS(string));

TOOLKIT_INIT_FUNC(console)
{
    console_commands = NULL;
    console_commands_num = 0;

    utarray_new(command_process_queue, &ut_str_icd);

    /* Add the 'help' command. */
    console_command_add(
            "help",
            console_command_help,
            "Displays this help.",
            "Displays the help, listing available console commands, etc.\n\n"
            "'help <command>' can be used to get more detailed help about the specified command."
            );
}
TOOLKIT_INIT_FUNC_FINISH

TOOLKIT_DEINIT_FUNC(console)
{
    size_t i;

    pthread_cancel(thread_id);

    for (i = 0; i < console_commands_num; i++) {
        efree(console_commands[i].command);
        efree(console_commands[i].desc_brief);
        efree(console_commands[i].desc);
    }

    if (console_commands) {
        efree(console_commands);
        console_commands = NULL;
    }

    console_commands_num = 0;
    utarray_free(command_process_queue);

    logger_set_print_func(logger_do_print);

#ifdef HAVE_READLINE
    pthread_mutex_lock(&rl_mutex);
    rl_unbind_key(RETURN);
    rl_callback_handler_remove();

    rl_set_prompt("");
    rl_redisplay();
    pthread_mutex_unlock(&rl_mutex);
#endif
}
TOOLKIT_DEINIT_FUNC_FINISH

/**
 * The help command of the console.
 * @param params Command parameters. */
static void console_command_help(const char *params)
{
    size_t i;

    /* Parameters provided, so give out info about the command. */
    if (params) {
        for (i = 0; i < console_commands_num; i++) {
            if (strcmp(console_commands[i].command, params) == 0) {
                char *curr, *next, *cp;

                logger_print(LOG(INFO), "##### Command: %s #####", console_commands[i].command);
                logger_print(LOG(INFO), " ");

                for (curr = console_commands[i].desc; (curr && (next = strchr(curr, '\n'))) || curr; curr = next ? next + 1 : NULL) {
                    cp = estrndup(curr, next ? (size_t) (next - curr) : strlen(curr));
                    logger_print(LOG(INFO), "%s", cp);
                    efree(cp);
                }

                return;
            }
        }

        logger_print(LOG(INFO), "No such command '%s'.", params);
    } else {
        /* Otherwise brief information about all available commands. */
        logger_print(LOG(INFO), "List of available commands:");
        logger_print(LOG(INFO), " ");

        for (i = 0; i < console_commands_num; i++) {
            logger_print(LOG(INFO), "    - %s: %s", console_commands[i].command, console_commands[i].desc_brief);
        }

        logger_print(LOG(INFO), " ");
        logger_print(LOG(INFO), "Use 'help <command>' to learn more about the specified command.");
    }
}

#ifdef HAVE_READLINE

/**
 * Implements callback handler for readline.
 * @param line Line. */
static void handle_line_fake(char *line)
{
    if (line) {
        rl_set_prompt(current_prompt);
        rl_already_prompted = 1;
    }
}

/**
 * Handle enter key. */
static int handle_enter(int cnt, int key)
{
    char *line;

    line = rl_copy_text(0, rl_end);

    rl_point = rl_end;
    rl_redisplay();
    rl_crlf();
    rl_on_new_line();
    rl_replace_line("", 1);

    if (*line != '\0') {
        add_history(line);
    }

    pthread_mutex_lock(&command_process_queue_mutex);
    utarray_push_back(command_process_queue, &line);
    pthread_mutex_unlock(&command_process_queue_mutex);

    free(line);
    rl_redisplay();
    rl_done = 1;

    return 0;
}

/**
 * Command generator for readline's completion. */
static char *command_generator(const char *text, int state)
{
    static size_t i, len;
    char *command;

    if (!state) {
        i = 0;
        len = strlen(text);
    }

    while (i < console_commands_num) {
        command = console_commands[i].command;
        i++;

        if (strncmp(command, text, len) == 0) {
            return strdup(command);
        }
    }

    return NULL;
}

/**
 * Readline completion. */
static char **readline_completion(const char *text, int start, int end)
{
    char **matches;

    matches = NULL;

    if (start == 0) {
        matches = rl_completion_matches(text, command_generator);
    }

    return matches;
}

/**
 * Overrides the logger's standard printing function. */
static void console_print(const char *str)
{
    char *saved_line;
    int saved_point;

    pthread_mutex_lock(&rl_mutex);

    saved_line = rl_copy_text(0, rl_end);
    saved_point = rl_point;

    rl_set_prompt("");
    rl_replace_line("", 0);
    rl_redisplay();

    logger_do_print(str);

    rl_set_prompt(current_prompt);
    rl_replace_line(saved_line, 0);
    rl_point = saved_point;
    rl_redisplay();

    free(saved_line);

    pthread_mutex_unlock(&rl_mutex);
}

#endif

/**
 * Thread for acquiring stdin data.
 * @param dummy Unused.
 * @return NULL. */
static void *do_thread(void *dummy)
{
#ifndef HAVE_READLINE
    char *line;
    ssize_t numread;
    size_t len;
#else
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
#endif

    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    for ( ; ; ) {
#ifdef HAVE_READLINE
        if (select(STDIN_FILENO + 1, &fds, NULL, NULL, NULL) != -1 &&
                FD_ISSET(STDIN_FILENO, &fds)) {
            pthread_mutex_lock(&rl_mutex);
            rl_callback_read_char();
            pthread_mutex_unlock(&rl_mutex);
        }
#else
        line = NULL;
        numread = getline(&line, &len, stdin);

        if (numread <= 0) {
            continue;
        }

        pthread_mutex_lock(&command_process_queue_mutex);
        utarray_push_back(command_process_queue, &line);
        pthread_mutex_unlock(&command_process_queue_mutex);
#endif

        usleep(10000);
    }

    return NULL;
}

/**
 * Start the console stdin-reading thread.
 * @return 1 on success, 0 on failure. */
int console_start_thread(void)
{
    int ret;

#ifdef HAVE_READLINE
    rl_readline_name = "atrinik-server";
    rl_initialize();
    rl_attempted_completion_function = readline_completion;

    current_prompt = "> ";
    rl_set_prompt(current_prompt);

    if (rl_bind_key(RETURN, handle_enter)) {
        logger_print(LOG(ERROR), "Could not bind enter.");
        exit(1);
    }

    rl_callback_handler_install(current_prompt, handle_line_fake);
    logger_set_print_func(console_print);
    pthread_mutex_init(&rl_mutex, NULL);
#endif

    pthread_mutex_init(&command_process_queue_mutex, NULL);
    ret = pthread_create(&thread_id, NULL, do_thread, NULL);

    if (ret != 0) {
        return 0;
    }

    return 1;
}

/**
 * Add a possible command to the console.
 * @param command Command name, must be unique.
 * @param handle_func Function that will handle the command.
 * @param desc_brief Brief, one-line description of the command.
 * @param desc More detailed description of the command. */
void console_command_add(const char *command, console_command_func handle_func, const char *desc_brief, const char *desc)
{
    size_t i;

    TOOLKIT_PROTECT();

    /* Make sure the command doesn't exist yet. */
    for (i = 0; i < console_commands_num; i++) {
        if (strcmp(console_commands[i].command, command) == 0) {
            logger_print(LOG(BUG), "Tried to add duplicate entry for command '%s'.", command);
            return;
        }
    }

    /* Add it to the commands array. */
    console_commands = ereallocz(console_commands, sizeof(*console_commands) * console_commands_num, sizeof(*console_commands) * (console_commands_num + 1));
    console_commands[console_commands_num].command = estrdup(command);
    console_commands[console_commands_num].handle_func = handle_func;
    console_commands[console_commands_num].desc_brief = estrdup(desc_brief);
    console_commands[console_commands_num].desc = estrdup(desc);
    console_commands_num++;
}

/**
 * Process the console API. This should usually be part of the program's
 * main loop. */
void console_command_handle(void)
{
    char **line, *cp;
    size_t i;

    TOOLKIT_PROTECT();

    pthread_mutex_lock(&command_process_queue_mutex);
    line = (char **) utarray_front(command_process_queue);
    pthread_mutex_unlock(&command_process_queue_mutex);

    if (!line) {
        return;
    }

    /* Remove the newline. */
    cp = strchr(*line, '\n');

    if (cp) {
        *cp = '\0';
    }

    /* Remove the command from the parameters. */
    cp = strchr(*line, ' ');

    if (cp) {
        *(cp++) = '\0';

        if (cp && *cp == '\0') {
            cp = NULL;
        }
    }

    for (i = 0; i < console_commands_num; i++) {
        if (strcmp(console_commands[i].command, *line) == 0) {
            console_commands[i].handle_func(cp);
            break;
        }
    }

    pthread_mutex_lock(&command_process_queue_mutex);
    utarray_erase(command_process_queue, 0, 1);
    pthread_mutex_unlock(&command_process_queue_mutex);
}
