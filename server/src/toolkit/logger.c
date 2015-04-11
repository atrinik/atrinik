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

#ifndef WIN32
#include <execinfo.h>
#endif

/**
 * Name of the API. */
#define API_NAME logger

/**
 * If 1, the API has been initialized.
 */
static uint8 did_init = 0;

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
    "PACKET"
};

/**
 * Which messages to print out.
 */
static uint64 logger_filter_stdout;

/**
 * Which messages to log to the log file.
 */
static uint64 logger_filter_logfile;

/**
 * Initialize the logger API.
 * @internal
 */
void toolkit_logger_init(void)
{

    TOOLKIT_INIT_FUNC_START(logger)
    {
        toolkit_import(string);

        log_fp = NULL;
        logger_set_print_func(logger_do_print);

        logger_filter_stdout = UINT64_MAX;
        logger_filter_logfile = UINT64_MAX;
    }
    TOOLKIT_INIT_FUNC_END()
}

/**
 * Deinitialize the logger API.
 * @internal
 */
void toolkit_logger_deinit(void)
{

    TOOLKIT_DEINIT_FUNC_START(logger)
    {
        if (log_fp != NULL) {
            fclose(log_fp);
        }
    }
    TOOLKIT_DEINIT_FUNC_END()
}

/**
 * Open the specified path as a log file.
 * @param path File to open.
 */
void logger_open_log(const char *path)
{
    TOOLKIT_FUNC_PROTECTOR(API_NAME);

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
    TOOLKIT_FUNC_PROTECTOR(API_NAME);
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
static void logger_set_filter(uint64 *filter, const char *str)
{
    char word[MAX_BUF];
    const char *cp;
    size_t pos;
    sint8 oper;
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
                *filter = UINT64_MAX;
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
    TOOLKIT_FUNC_PROTECTOR(API_NAME);
    print_func = func;
}

/**
 * Default logger printing function, uses fputs to write to stdout.
 * @param str String to print.
 */
void logger_do_print(const char *str)
{
    TOOLKIT_FUNC_PROTECTOR(API_NAME);
    fputs(str, stdout);
}

/**
 * Print a message to the console/stdout/log file/etc.
 * @param level Log level to use.
 * @param function Name of the function that is calling this.
 * @param line Line in the code that is calling this.
 * @param format Format string.
 * @param ... Format arguments.
 */
void logger_print(logger_level level, const char *function, uint64 line,
        const char *format, ...)
{
    char formatted[HUGE_BUF], timebuf[HUGE_BUF], buf[sizeof(formatted) * 2];
    va_list ap;
    struct timeval tv;
    struct tm *tm;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

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
        snprintf(VS(timebuf), "[%s.%06"FMT64U "] ", timebuf2,
                (uint64) tv.tv_usec);
    } else {
        timebuf[0] = '\0';
    }

    if ((1U << level) & logger_filter_stdout) {
        snprintf(
                VS(buf),
                LOGGER_ESC_SEQ_BOLD "%s" LOGGER_ESC_SEQ_END
                LOGGER_ESC_SEQ_RED "%-6s" LOGGER_ESC_SEQ_END " "
                LOGGER_ESC_SEQ_CYAN "[%s:%"FMT64U "]" LOGGER_ESC_SEQ_END " "
                LOGGER_ESC_SEQ_YELLOW "%s" LOGGER_ESC_SEQ_END "\n",
                timebuf, logger_names[level], function, line, formatted
                );
        print_func(buf);
    }

    if (log_fp != NULL && (1U << level) & logger_filter_logfile) {
        fprintf(log_fp, "%s%-6s [%s:%"FMT64U "] %s\n", timebuf,
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
    efree(bt_syms);
#endif
}

#endif
