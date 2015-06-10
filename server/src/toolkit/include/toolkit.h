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

typedef struct toolkit_dependency {
    toolkit_func func;
    bool depends;
} toolkit_dependency_t;

/**
 * Check if the specified API has been imported yet. */
#define toolkit_imported(__api_name) toolkit_check_imported(toolkit_ ## __api_name ## _deinit)
/**
 * Import the specified API (if it has not been imported yet). */
#define toolkit_import(__api_name) toolkit_ ## __api_name ## _init()

#define DEPENDS(__api_name) { toolkit_ ## __api_name ## _init, true}
#define IMPORTS(__api_name) { toolkit_ ## __api_name ## _init, false}

#define TOOLKIT_API(...) \
    static bool _did_init_ = false; \
    static toolkit_dependency_t _dependencies_[] = { \
        {NULL, true}, ## __VA_ARGS__, {NULL, true} \
    }

/**
 * Constructs a toolkit function name, for example, toolkit_string_init if the
 * two arguments passed to it were string and init.
 */
#define TOOLKIT_FUNC(__api_name, _x) CONCAT(toolkit_, CONCAT(__api_name, _x))

/**
 * Declares an initialization function.
 */
#define TOOLKIT_INIT_FUNC(__api_name) \
    void TOOLKIT_FUNC(__api_name, _init)(void) \
    { \
        toolkit_func _deinit_func_ = TOOLKIT_FUNC(__api_name, _deinit); \
        const char *const _api_name_ = STRINGIFY(__api_name); \
        if (toolkit_check_imported(_deinit_func_)) { \
            return; \
        } \
        _did_init_ = true; \
        for (size_t _i_ = 1; _dependencies_[_i_].func != NULL; _i_++) { \
            if (_dependencies_[_i_].depends) { \
                _dependencies_[_i_].func(); \
            } \
        } \
        toolkit_import_register(_api_name_, _deinit_func_);

/**
 * Finishes a previously declared initialization function.
 */
#define TOOLKIT_INIT_FUNC_FINISH \
        for (size_t _i_ = 1; _dependencies_[_i_].func != NULL; _i_++) { \
            if (!_dependencies_[_i_].depends) { \
                _dependencies_[_i_].func(); \
            } \
        } \
    }

/**
 * Declares a deinitialization function.
 */
#define TOOLKIT_DEINIT_FUNC(__api_name) \
    void TOOLKIT_FUNC(__api_name, _deinit)(void) \
    {

/**
 * Finishes a previously declared deinitialization function.
 */
#define TOOLKIT_DEINIT_FUNC_FINISH \
        _did_init_ = false; \
    }

#ifndef NDEBUG
#define TOOLKIT_PROTECT() \
    do { \
        if (!_did_init_) { \
            static bool did_warn = false; \
            if (!did_warn) { \
                toolkit_import(logger); \
                log_error("Toolkit API function used, but the API was not " \
                          "initialized - this could result in undefined " \
                          "behavior."); \
                did_warn = true; \
            } \
        } \
    } while (0)
#else
#define TOOLKIT_PROTECT()
#endif

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
    (((PERF_TIMER_VAR(__pt_t2, __id).QuadPart - \
    PERF_TIMER_VAR(__pt_t1, __id).QuadPart)) / \
    (double) PERF_TIMER_VAR(__pt_frequency, __id).QuadPart)

#else

#define PERF_TIMER_DECLARE(__id) \
    struct timeval PERF_TIMER_VAR(__pt_t1, __id), PERF_TIMER_VAR(__pt_t2, __id);
#define PERF_TIMER_START(__id) \
    GETTIMEOFDAY(&PERF_TIMER_VAR(__pt_t1, __id));
#define PERF_TIMER_STOP(__id) \
    GETTIMEOFDAY(&PERF_TIMER_VAR(__pt_t2, __id));
#define PERF_TIMER_GET(__id) \
    (((PERF_TIMER_VAR(__pt_t2, __id).tv_sec - \
    PERF_TIMER_VAR(__pt_t1, __id).tv_sec)) + \
    ((PERF_TIMER_VAR(__pt_t2, __id).tv_usec - \
    PERF_TIMER_VAR(__pt_t1, __id).tv_usec) / 1000000.0))

#endif
/*@}*/

#define _STRINGIFY(_X_) #_X_
#define STRINGIFY(_X_) _STRINGIFY(_X_)

#define _CONCAT(_X_, _Y_) _X_ ## _Y_
#define CONCAT(_X_, _Y_) _CONCAT(_X_, _Y_)

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

#define _FLOATING_EQUAL(_a, _b, _epsilon) (fabs((_a) - (_b)) < (_epsilon))
#define FLT_EQUAL(_a, _b) _FLOATING_EQUAL(_a, _b, FLT_EPSILON)
#define DBL_EQUAL(_a, _b) _FLOATING_EQUAL(_a, _b, DBL_EPSILON)
#define LDBL_EQUAL(_a, _b) _FLOATING_EQUAL(_a, _b, LDBL_EPSILON)

#define BIT_MASK(_bit) ((uintmax_t) 1 << (_bit))

#define BIT_QUERY(_val, _bit) (((_val) & BIT_MASK(_bit)) != 0)
#define BIT_SET(_val, _bit) BITMASK_SET(_val, BIT_MASK(_bit))
#define BIT_CLEAR(_val, _bit) BITMASK_CLEAR(_val, BIT_MASK(_bit))
#define BIT_FLIP(_val, _bit) BITMASK_FLIP(_val, BIT_MASK(_bit))
#define BIT_CHANGE(_val, _bit) BITMASK_CHANGE(_val, BIT_MASK(_bit))

#define BITMASK_QUERY(_val, _mask) (((_val) & (_mask)) == (_mask))
#define BITMASK_SET(_val, _mask)                          \
    do {                                                  \
        (_val) |= (_mask);                                \
    } while (0)
#define BITMASK_CLEAR(_val, _mask)                        \
    do {                                                  \
        (_val) &= ~(_mask);                               \
    } while (0)
#define BITMASK_FLIP(_val, _mask)                         \
    do {                                                  \
        (_val) ^= (_mask);                                \
    } while (0)
#define BITMASK_CHANGE(_val, _mask, _x)                   \
    do {                                                  \
        (_val) ^= ((-(_x)) ^ (_val)) & (_mask);           \
    } while (0)

#endif
