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
 * Toolkit system header file.
 *
 * @author Alex Tokar */

#ifndef TOOLKIT_H
#define TOOLKIT_H

#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L

/* Porting API header file has extra priority. */
#include <porting.h>

/* Now all the other header files that are part of the toolkit. */
#include <binreloc.h>
#include <clioptions.h>
#include <common.h>
#include <console.h>
#include <logger.h>
#include <memory.h>
#include <mempool.h>
#include <sha1.h>
#include <shstr.h>
#include <socket.h>
#include <stringbuffer.h>
#include <utarray.h>
#include <uthash.h>
#include <utlist.h>

/**
 * Toolkit (de)initialization function. */
typedef void (*toolkit_func)(void);

/**
 * Check if the specified API has been imported yet. */
#define toolkit_imported(__api_name) toolkit_check_imported(toolkit_ ## __api_name ## _deinit)
/**
 * Import the specified API (if it has not been imported yet). */
#define toolkit_import(__api_name) toolkit_ ## __api_name ## _init()

/**
 * Start toolkit API initialization function. */
#define TOOLKIT_INIT_FUNC_START(__api_name) \
    { \
        toolkit_func __deinit_func = toolkit_ ## __api_name ## _deinit; \
        if (toolkit_imported(__api_name)) \
        { \
            return; \
        } \
        did_init = 1;

/**
 * End toolkit API initialization function. */
#define TOOLKIT_INIT_FUNC_END() \
    toolkit_import_register(STRINGIFY(API_NAME), __deinit_func); \
    }

/**
 * Start toolkit API deinitialization function. */
#define TOOLKIT_DEINIT_FUNC_START(__api_name) \
    {

/**
 * End toolkit API deinitialization function. */
#define TOOLKIT_DEINIT_FUNC_END() \
    did_init = 0; \
    }


#ifndef NDEBUG
#define TOOLKIT_FUNC_PROTECTOR(__api_name) \
    { \
        if (!did_init) \
        { \
            static uint8_t did_warn = 0; \
            if (!did_warn) \
            { \
                toolkit_import(logger); \
                logger_print(LOG(DEBUG), "Toolkit API function used, but the API was not initialized - this could result in undefined behavior."); \
                did_warn = 1; \
            } \
        } \
    }
#else
#define TOOLKIT_FUNC_PROTECTOR(__api_name)
#endif

/* Map the error-checking string duplicating functions into the toolkit
 * variants. This is done for convenience, and because the functions can't be
 * defined as they could conflict with functions from other libraries. */
#define estrdup string_estrdup
#define estrndup string_estrndup

/**
 * Takes a variable and returns the variable and its size. */
#define VS(var) (var), sizeof((var))

/**
 * @defgroup PERF_TIMER_xxx Performance timer macros
 *
 * Helper macros to perform high-precision performance profiling in a
 * platform-independent way. Ideally we want a precision of 1 ms, if possible.
 *
 * The typical use case is using, for example, PERF_TIMER_DECLARE(1) at the
 * beginning of the function to declare the variables used by the timer. Then
 * before the code you want to profile, use PERF_TIMER_START(1), and after
 * the code, use PERF_TIMER_STOP(1). Afterwards, you can use PERF_TIMER_GET(1)
 * to get the number of seconds the execution took as a floating point number.
 *
 * You can have multiple timers by changing the 1 to 2 or anything else (note
 * that this is a compile-time feature).
 *@{*/

#define PERF_TIMER_VAR(__var, __id) (__var##_##__id)

#ifdef WIN32

#define PERF_TIMER_DECLARE(__id) \
    LARGE_INTEGER PERF_TIMER_VAR(__pt_frequency, __id); \
    LARGE_INTEGER PERF_TIMER_VAR(__pt_t1, __id), PERF_TIMER_VAR(__pt_t2, __id);
#define PERF_TIMER_START(__id) \
    QueryPerformanceFrequency(&PERF_TIMER_VAR(__pt_frequency, __id)); \
    QueryPerformanceCounter(&PERF_TIMER_VAR(__pt_t1, __id));
#define PERF_TIMER_STOP(__id) \
    QueryPerformanceCounter(&PERF_TIMER_VAR(__pt_t2, __id));
#define PERF_TIMER_GET(__id) \
    ((PERF_TIMER_VAR(__pt_t2, __id).QuadPart - \
    PERF_TIMER_VAR(__pt_t1, __id).QuadPart) * 1000.0 / \
    PERF_TIMER_VAR(__pt_frequency, __id).QuadPart)

#else

#define PERF_TIMER_DECLARE(__id) \
    struct timeval PERF_TIMER_VAR(__pt_t1, __id), PERF_TIMER_VAR(__pt_t2, __id);
#define PERF_TIMER_START(__id) \
    GETTIMEOFDAY(&PERF_TIMER_VAR(__pt_t1, __id));
#define PERF_TIMER_STOP(__id) \
    GETTIMEOFDAY(&PERF_TIMER_VAR(__pt_t2, __id));
#define PERF_TIMER_GET(__id) \
    (((PERF_TIMER_VAR(__pt_t2, __id).tv_sec - \
    PERF_TIMER_VAR(__pt_t1, __id).tv_sec) * 1000.0) + \
    ((PERF_TIMER_VAR(__pt_t2, __id).tv_usec - \
    PERF_TIMER_VAR(__pt_t1, __id).tv_usec) / 1000.0))

#endif
/*@}*/

#define _STRINGIFY(_X_) #_X_
#define STRINGIFY(_X_) _STRINGIFY(_X_)

#define SOFT_ASSERT_MSG(msg, ...) log_error((msg), ## __VA_ARGS__)

#ifndef NDEBUG

#define SOFT_ASSERT(cond, msg, ...) \
    do { \
        if (!(cond)) { \
            SOFT_ASSERT_MSG(msg, ##__VA_ARGS__); \
            assert(cond); \
            return; \
        } \
    } while (0)

#define SOFT_ASSERT_RC(cond, rc, msg, ...) \
    do { \
        if (!(cond)) { \
            SOFT_ASSERT_MSG(msg, ##__VA_ARGS__); \
            assert(cond); \
            return (rc); \
        } \
    } while (0)

#define SOFT_ASSERT_LABEL(cond, label, msg, ...) \
    do { \
        if (!(cond)) { \
            SOFT_ASSERT_MSG(msg, ##__VA_ARGS__); \
            assert(cond); \
            goto label; \
        } \
    } while (0)

#else

#define SOFT_ASSERT(cond, msg, ...) \
    do { \
        if (!(cond)) { \
            SOFT_ASSERT_MSG(msg, ##__VA_ARGS__); \
            return; \
        } \
    } while (0)

#define SOFT_ASSERT_RC(cond, rc, msg, ...) \
    do { \
        if (!(cond)) { \
            SOFT_ASSERT_MSG(msg, ##__VA_ARGS__); \
            return (rc); \
        } \
    } while (0)

#define SOFT_ASSERT_LABEL(cond, label, msg, ...) \
    do { \
        if (!(cond)) { \
            SOFT_ASSERT_MSG(msg, ##__VA_ARGS__); \
            goto label; \
        } \
    } while (0)

#endif

#define HARD_ASSERT(cond) assert(cond)

#endif
