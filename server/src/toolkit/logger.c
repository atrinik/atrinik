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
 * Logger API.
 *
 * @author Alex Tokar
 */

#ifndef __CPROTO__

#include <global.h>
#include <stdarg.h>
#include <toolkit_string.h>

#ifndef WIN32
#include <execinfo.h>
#endif

/**
 * Pointer to open log file, if any.
 */
static FILE *log_fp;

/**
 * The print function.
 */
static logger_print_func print_func;

/**
 * String representations of the log levels.
 */
static const char *const logger_names[LOG_MAX] = {
    "CHAT",
    "INFO",
    "SYSTEM",
    "ERROR",
    "BUG",
    "DEBUG",
    "DEVEL",
    "PACKET",
    "DUMPRX",
    "DUMPTX"
};

/**
 * Escape sequence IDs.
 */
enum {
    LOGGER_ESC_SEQ_BOLD,
    LOGGER_ESC_SEQ_BLACK,
    LOGGER_ESC_SEQ_RED,
    LOGGER_ESC_SEQ_GREEN,
    LOGGER_ESC_SEQ_YELLOW,
    LOGGER_ESC_SEQ_BLUE,
    LOGGER_ESC_SEQ_MAGENTA,
    LOGGER_ESC_SEQ_CYAN,
    LOGGER_ESC_SEQ_WHITE,
    LOGGER_ESC_SEQ_END,

    LOGGER_ESC_SEQ_MAX
};

/**
 * Acquire the specified escape sequence from ::logger_escape_seqs.
 */
#define LOGGER_ESC_SEQ(_x) (logger_escape_seqs[LOGGER_ESC_SEQ_ ## _x])

/**
 * Escape sequence strings.
 */
static char logger_escape_seqs[LOGGER_ESC_SEQ_MAX][10] = {};

/**
 * Which messages to print out.
 */
static uint64_t logger_filter_stdout;

/**
 * Which messages to log to the log file.
 */
static uint64_t logger_filter_logfile;

/* Prototypes */
static bool logger_term_has_ansi_colors(void);

TOOLKIT_API(DEPENDS(string));

TOOLKIT_INIT_FUNC(logger)
{
    log_fp = NULL;
    logger_set_print_func(logger_do_print);

    logger_filter_stdout = logger_filter_logfile = 0;

    logger_set_filter_stdout("all,-dumptx,-dumprx");
    logger_set_filter_logfile("all,-dumptx,-dumprx");

    if (logger_term_has_ansi_colors()) {
        snprintf(VS(LOGGER_ESC_SEQ(BOLD)), "%s", "\033[1m");
        snprintf(VS(LOGGER_ESC_SEQ(BLACK)), "%s", "\033[30m");
        snprintf(VS(LOGGER_ESC_SEQ(RED)), "%s", "\033[31m");
        snprintf(VS(LOGGER_ESC_SEQ(GREEN)), "%s", "\033[32m");
        snprintf(VS(LOGGER_ESC_SEQ(YELLOW)), "%s", "\033[33m");
        snprintf(VS(LOGGER_ESC_SEQ(BLUE)), "%s", "\033[34m");
        snprintf(VS(LOGGER_ESC_SEQ(MAGENTA)), "%s", "\033[35m");
        snprintf(VS(LOGGER_ESC_SEQ(CYAN)), "%s", "\033[36m");
        snprintf(VS(LOGGER_ESC_SEQ(WHITE)), "%s", "\033[37m");
        snprintf(VS(LOGGER_ESC_SEQ(END)), "%s", "\033[0m");
    }
}
TOOLKIT_INIT_FUNC_FINISH

TOOLKIT_DEINIT_FUNC(logger)
{
    if (log_fp != NULL) {
        fclose(log_fp);
    }
}
TOOLKIT_DEINIT_FUNC_FINISH

/**
 * Open the specified path as a log file.
 * @param path File to open.
 */
void logger_open_log(const char *path)
{
    TOOLKIT_PROTECT();

    if (log_fp != NULL) {
        fclose(log_fp);
    }

    log_fp = fopen(path, "w");
}

/**
 * Acquire the log file pointer.
 * @return Log file pointer. In the case that no log file is currently open,
 * stdout is returned.
 */
FILE *logger_get_logfile(void)
{
    TOOLKIT_PROTECT();
    return log_fp != NULL ? log_fp : stdout;
}

/**
 * Get log level ID from its name.
 * @param name Name of the log level.
 * @return Log level. Can be LOG_MAX, in which case no log level has been
 * found.
 */
logger_level logger_get_level(const char *name)
{
    logger_level level;

    for (level = 0; level < LOG_MAX; level++) {
        if (strcasecmp(logger_names[level], name) == 0) {
            break;
        }
    }

    return level;
}

/**
 * Parses a comma-delimited string and sets the appropriate log filter levels.
 *
 * The individual levels CAN start with - or + to indicate whether to remove
 * or add the level.
 * @param[out] filter Which filter to update.
 * @param str String with the log filter levels.
 */
static void logger_set_filter(uint64_t *filter, const char *str)
{
    char word[MAX_BUF];
    const char *cp;
    size_t pos;
    int8_t oper;
    logger_level level;

    pos = 0;

    while ((cp = string_get_word(str, &pos, ',', word,
            sizeof(word), 0)) != NULL) {
        oper = -1;

        if (*cp == '-' || *cp == '+') {
            oper = *cp == '+';
            cp += 1;
        }

        if (strcasecmp(cp, "all") == 0) {
            if (oper == -1) {
                oper = *filter == 0;
            }

            if (oper == 1) {
                *filter = ~0;
            } else {
                *filter = 0;
            }

            continue;
        }

        level = logger_get_level(cp);

        if (level >= LOG_MAX) {
            continue;
        }

        if (oper == -1) {
            *filter ^= 1U << level;
        } else if (oper == 0) {
            *filter &= ~(1U << level);
        } else if (oper == 1) {
            *filter |= 1U << level;
        }
    }
}

/**
 * Sets the stdout filter to the specified log filter levels.
 */
void logger_set_filter_stdout(const char *str)
{
    logger_set_filter(&logger_filter_stdout, str);
}

/**
 * Sets the logfile filter to the specified log filter levels.
 */
void logger_set_filter_logfile(const char *str)
{
    logger_set_filter(&logger_filter_logfile, str);
}

/**
 * Set the logger's printing function.
 * @param func Function to use.
 */
void logger_set_print_func(logger_print_func func)
{
    TOOLKIT_PROTECT();
    print_func = func;
}

/**
 * Default logger printing function, uses fputs to write to stdout.
 * @param str String to print.
 */
void logger_do_print(const char *str)
{
    TOOLKIT_PROTECT();

    fputs(str, stdout);

#ifdef WIN32
    fflush(stdout);
#endif
}

/**
 * Print a message to the console/stdout/log file/etc.
 * @param level Log level to use.
 * @param function Name of the function that is calling this.
 * @param line Line in the code that is calling this.
 * @param format Format string.
 * @param ... Format arguments.
 */
void logger_print(logger_level level, const char *function, uint64_t line,
        const char *format, ...)
{
    char formatted[HUGE_BUF], timebuf[HUGE_BUF], buf[sizeof(formatted) * 2];
    va_list ap;
    struct timeval tv;
    struct tm *tm;

    TOOLKIT_PROTECT();

    /* Safety... */
    if (level >= LOG_MAX) {
        return;
    }

    /* If the log level is unwanted, bail out. */
    if (!((1U << level) & logger_filter_stdout) &&
            !((1U << level) & logger_filter_logfile)) {
        return;
    }

    va_start(ap, format);
    vsnprintf(formatted, sizeof(formatted), format, ap);
    va_end(ap);

    gettimeofday(&tv, NULL);
    tm = localtime(&tv.tv_sec);

    if (tm != NULL) {
        char timebuf2[MAX_BUF];

        strftime(VS(timebuf2), "%H:%M:%S", tm);
        snprintf(VS(timebuf), "[%s.%06"PRIu64 "] ", timebuf2,
                (uint64_t) tv.tv_usec);
    } else {
        timebuf[0] = '\0';
    }

    if ((1U << level) & logger_filter_stdout) {
        snprintf(VS(buf), "%s%s%s""%s%-6s%s ""%s[%s:%" PRIu64 "]%s ""%s%s%s\n",
                LOGGER_ESC_SEQ(BOLD), timebuf, LOGGER_ESC_SEQ(END),
                LOGGER_ESC_SEQ(RED), logger_names[level], LOGGER_ESC_SEQ(END),
                LOGGER_ESC_SEQ(CYAN), function, line, LOGGER_ESC_SEQ(END),
                LOGGER_ESC_SEQ(YELLOW), formatted, LOGGER_ESC_SEQ(END));
        print_func(buf);
    }

    if (log_fp != NULL && (1U << level) & logger_filter_logfile) {
        fprintf(log_fp, "%s%-6s [%s:%"PRIu64 "] %s\n", timebuf,
                logger_names[level], function, line, formatted);
        fflush(log_fp);
    }
}

/**
 * Print a traceback.
 */
void logger_traceback(void)
{
#ifndef WIN32
    void *bt[1024];
    int bt_size;
    char **bt_syms;
    int i;

    bt_size = backtrace(bt, 1024);
    bt_syms = backtrace_symbols(bt, bt_size);
    log(LOG(ERROR), "------------ TRACEBACK ------------");

    for (i = 1; i < bt_size; i++) {
        log(LOG(ERROR), "%s", bt_syms[i]);
    }

    log(LOG(ERROR), "-----------------------------------");
    free(bt_syms);
#endif
}

/**
 * Determine whether stdout is a TTY.
 * @return Whether stdout is a TTY.
 */
static bool logger_isatty(void)
{
#ifdef WIN32
    CONSOLE_SCREEN_BUFFER_INFO sbi;

    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),
            &sbi)) {
#else
    if (!isatty(fileno(stdout))) {
#endif
        return false;
    }

    return true;
}

/**
 * Determine whether the currently used terminal supports ANSI colors.
 * @return Whether the terminal supports ANSI colors.
 */
static bool logger_term_has_ansi_colors(void)
{
    /* If stdout is not a TTY, it cannot support ANSI colors. */
    if (!logger_isatty()) {
        return false;
    }

#ifdef WIN32
    /* Windows CLI does not support ANSI color sequences unless ANSICON is set.
     * It also does not specify the TERM environment variable, so we can
     * identify it by that. */
    if (getenv("TERM") == NULL && getenv("ANSICON") == NULL) {
        return false;
    }
#endif

    return true;
}

#endif
