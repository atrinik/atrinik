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
 * Logger API header file.
 *
 * @author Alex Tokar
 */

#ifndef LOGGER_H
#define LOGGER_H

typedef void (*logger_print_func)(const char *str);

/**
 * Possible log levels.
 */
typedef enum logger_level {
    LOG_CHAT,
    LOG_INFO,
    LOG_SYSTEM,
    LOG_ERROR,
    LOG_BUG,
    LOG_DEBUG,
    LOG_DEVEL,
    LOG_PACKET,
    LOG_DUMPRX,
    LOG_DUMPTX,

    LOG_MAX
} logger_level;

#define log_error(...) \
    do { \
        logger_traceback(); \
        LOG(ERROR, ##__VA_ARGS__); \
    } while (0)
#define LOG(_level, ...) \
    do { \
        logger_print(LOG_ ## _level, __FUNCTION__, __LINE__, ## __VA_ARGS__); \
    } while(0)

/* Prototypes */

extern void toolkit_logger_init(void);
extern void toolkit_logger_deinit(void);
void logger_open_log(const char *path);
FILE *logger_get_logfile(void);
logger_level logger_get_level(const char *name);
void logger_set_filter_stdout(const char *str);
void logger_set_filter_logfile(const char *str);
void logger_set_print_func(logger_print_func func);
void logger_do_print(const char *str);
void logger_print(logger_level level, const char *function, uint64_t line,
        const char *format, ...) __attribute__((format(printf, 4, 5)));
void logger_traceback(void);

#endif
