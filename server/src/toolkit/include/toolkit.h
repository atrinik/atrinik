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
 * @author Alex Tokar
 */

#ifndef TOOLKIT_H
#define TOOLKIT_H

#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L

/* Porting API header file has extra priority. */
#include <porting.h>

/* Now all the other header files that are part of the toolkit. */
#include <binreloc.h>
#include <common.h>
#include <console.h>
#include <logger.h>
#include <memory.h>
#include <mempool.h>
#include <shstr.h>
#include <stringbuffer.h>
#include <utarray.h>
#include <uthash.h>
#include <utlist.h>

/**
 * Toolkit (de)initialization function.
 */
typedef void (*toolkit_func)(void);

typedef struct toolkit_dependency {
    toolkit_func func;
    bool depends;
} toolkit_dependency_t;

/**
 * Check if the specified API has been imported yet.
 */
#define toolkit_imported(__api_name) toolkit_check_imported(toolkit_ ## __api_name ## _deinit)
/**
 * Import the specified API (if it has not been imported yet).
 */
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
 * Defines an initialization function.
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
 * Finishes a previously defined initialization function.
 */
#define TOOLKIT_INIT_FUNC_FINISH \
        for (size_t _i_ = 1; _dependencies_[_i_].func != NULL; _i_++) { \
            if (!_dependencies_[_i_].depends) { \
                _dependencies_[_i_].func(); \
            } \
        } \
    }

/**
 * Defines a deinitialization function.
 */
#define TOOLKIT_DEINIT_FUNC(__api_name) \
    void TOOLKIT_FUNC(__api_name, _deinit)(void) \
    {

/**
 * Finishes a previously defined deinitialization function.
 */
#define TOOLKIT_DEINIT_FUNC_FINISH \
        _did_init_ = false; \
    }

/**
 * Declares API functions.
 */
#define TOOLKIT_FUNCS_DECLARE(__api_name)                       \
    void TOOLKIT_FUNC(__api_name, _init)(void);                 \
    void TOOLKIT_FUNC(__api_name, _deinit)(void);

#ifndef NDEBUG
#define TOOLKIT_PROTECT() \
    do { \
        if (!_did_init_) { \
            static bool did_warn = false; \
            if (!did_warn) { \
                toolkit_import(logger); \
                did_warn = true; \
                log_error("Toolkit API function used, but the API was not " \
                          "initialized - this could result in undefined " \
                          "behavior."); \
            } \
        } \
    } while (0)
#else
#define TOOLKIT_PROTECT()
#endif

/**
 * Takes a variable and returns the variable and its size.
 */
#define VS(var) (var), sizeof((var))

/**
 * @defgroup buffer_sizes Buffer sizes
 *@{*/
/** Used for all kinds of things. */
#define MAX_BUF             256
/** Used for messages - some can be quite long. */
#define HUGE_BUF            4096
/**
 * Maximum size of player name.
 * @todo Get rid of this.
 */
#define MAX_NAME            16
/**
 * Used for other named things.
 * @todo Get rid of this.
 */
#define BIG_NAME            32
/*@}*/

/**
 * Returns the element size of an array.
 *
 * @param arrayname
 * The array's name.
 * @return
 * The number of elements.
 */
#define arraysize(arrayname) (sizeof(arrayname) / sizeof(*(arrayname)))

/** Check if the keyword represents a true value. */
#define KEYWORD_IS_TRUE(_keyword) (!strcasecmp((_keyword), "yes") || !strcasecmp((_keyword), "on") || !strcasecmp((_keyword), "true"))
/** Check if the keyword represents a false value. */
#define KEYWORD_IS_FALSE(_keyword) (!strcasecmp((_keyword), "no") || !strcasecmp((_keyword), "off") || !strcasecmp((_keyword), "false"))
#define KEYWORD_TO_BOOLEAN(_keyword, _bool)     \
do {                                            \
    if (KEYWORD_IS_TRUE((_keyword))) {          \
        (_bool) = true;                         \
    } else if (KEYWORD_IS_FALSE((_keyword))) {  \
        (_bool) = false;                        \
    }                                           \
} while (0)

/**
 * @defgroup TIMER_xxx Timer macros
 *
 * Helper macros to perform high-precision timing in a platform-independent way.
 * Ideally we want a precision of 1 ms, if possible.
 *
 * The typical use case is using TIMER_START(1) in order to declare and start
 * the timer, then TIMER_UPDATE(1) whenever you want to update/stop the timer.
 * Afterwards, you can use TIMER_GET(1) to get the number of seconds the
 * execution took as a floating point number.
 *
 * You can have multiple timers by changing the 1 to 2 or anything else (note
 * that this is a compile-time feature).
 *@{*/

#define TIMER_VAR(__var, __id) (__var##_##__id)
#define TIMER_START(__id)                                                      \
    TIMER_DECLARE(__id);                                                       \
    TIMER_RESTART(__id);

#ifdef WIN32

#define TIMER_DECLARE(__id)                                                    \
    LARGE_INTEGER TIMER_VAR(__pt_frequency, __id);                             \
    LARGE_INTEGER TIMER_VAR(__pt_t1, __id), TIMER_VAR(__pt_t2, __id)
#define TIMER_RESTART(__id)                                                    \
    do {                                                                       \
        QueryPerformanceFrequency(&TIMER_VAR(__pt_frequency, __id));           \
        QueryPerformanceCounter(&TIMER_VAR(__pt_t1, __id));                    \
    } while (0)
#define TIMER_UPDATE(__id)                                                     \
    do {                                                                       \
        QueryPerformanceCounter(&TIMER_VAR(__pt_t2, __id));                    \
    } while (0)
#define TIMER_GET(__id)                                                        \
    (((TIMER_VAR(__pt_t2, __id).QuadPart -                                     \
    TIMER_VAR(__pt_t1, __id).QuadPart)) /                                      \
    (double) TIMER_VAR(__pt_frequency, __id).QuadPart)

#else

#define TIMER_DECLARE(__id)                                                    \
    struct timeval TIMER_VAR(__pt_t1, __id), TIMER_VAR(__pt_t2, __id)
#define TIMER_RESTART(__id)                                                    \
    do {                                                                       \
        GETTIMEOFDAY(&TIMER_VAR(__pt_t1, __id));                               \
    } while (0)
#define TIMER_UPDATE(__id)                                                     \
    do {                                                                       \
        GETTIMEOFDAY(&TIMER_VAR(__pt_t2, __id));                               \
    } while (0)
#define TIMER_GET(__id)                                                        \
    (((TIMER_VAR(__pt_t2, __id).tv_sec -                                       \
    TIMER_VAR(__pt_t1, __id).tv_sec)) +                                        \
    ((TIMER_VAR(__pt_t2, __id).tv_usec -                                       \
    TIMER_VAR(__pt_t1, __id).tv_usec) / 1000000.0))

#endif
/*@}*/

#define _STRINGIFY(_X_) #_X_
#define STRINGIFY(_X_) _STRINGIFY(_X_)

#define _CONCAT(_X_, _Y_) _X_ ## _Y_
#define CONCAT(_X_, _Y_) _CONCAT(_X_, _Y_)
#define UNIQUE_VAR(_X_) CONCAT(_X_, __LINE__)

#define SOFT_ASSERT_MSG(msg, ...) log_error((msg), ## __VA_ARGS__)

#define _CASSERT(_test, _var)                                       \
    typedef char _var[2*!!(_test) - 1] __attribute__((unused))
#define CASSERT(_test)                                              \
    _CASSERT(_test, UNIQUE_VAR(assertion_failed));
#define CASSERT_ARRAY(_array, _size)                                \
    _CASSERT(arraysize(_array) == (_size),                          \
             array_ ## _array ## _missing_or_extra_elements);

#ifndef NDEBUG

#define SOFT_ASSERT(cond, msg, ...) \
    do { \
        if (unlikely(!(cond))) { \
            SOFT_ASSERT_MSG(msg, ##__VA_ARGS__); \
            assert(cond); \
            return; \
        } \
    } while (0)

#define SOFT_ASSERT_RC(cond, rc, msg, ...) \
    do { \
        if (unlikely(!(cond))) { \
            SOFT_ASSERT_MSG(msg, ##__VA_ARGS__); \
            assert(cond); \
            return (rc); \
        } \
    } while (0)

#define SOFT_ASSERT_LABEL(cond, label, msg, ...) \
    do { \
        if (unlikely(!(cond))) { \
            SOFT_ASSERT_MSG(msg, ##__VA_ARGS__); \
            assert(cond); \
            goto label; \
        } \
    } while (0)

#else

#define SOFT_ASSERT(cond, msg, ...) \
    do { \
        if (unlikely(!(cond))) { \
            SOFT_ASSERT_MSG(msg, ##__VA_ARGS__); \
            return; \
        } \
    } while (0)

#define SOFT_ASSERT_RC(cond, rc, msg, ...) \
    do { \
        if (unlikely(!(cond))) { \
            SOFT_ASSERT_MSG(msg, ##__VA_ARGS__); \
            return (rc); \
        } \
    } while (0)

#define SOFT_ASSERT_LABEL(cond, label, msg, ...) \
    do { \
        if (unlikely(!(cond))) { \
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
#define BIT_CHANGE(_val, _bit, _x) BITMASK_CHANGE(_val, BIT_MASK(_bit), _x)

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

/**@cond*/
/* Helper functions for FOR_EACH(). */
#define _FOR_EACH_1(what, x, ...) what(x)
#define _FOR_EACH_2(what, x, ...) what(x) _FOR_EACH_1(what, __VA_ARGS__)
#define _FOR_EACH_3(what, x, ...) what(x) _FOR_EACH_2(what, __VA_ARGS__)
#define _FOR_EACH_4(what, x, ...) what(x) _FOR_EACH_3(what, __VA_ARGS__)
#define _FOR_EACH_5(what, x, ...) what(x) _FOR_EACH_4(what, __VA_ARGS__)
#define _FOR_EACH_6(what, x, ...) what(x) _FOR_EACH_5(what, __VA_ARGS__)
#define _FOR_EACH_7(what, x, ...) what(x) _FOR_EACH_6(what, __VA_ARGS__)
#define _FOR_EACH_8(what, x, ...) what(x) _FOR_EACH_7(what, __VA_ARGS__)

#define _FOR_EACH_NARG(...) _FOR_EACH_NARG_(__VA_ARGS__, _FOR_EACH_RSEQ_N())
#define _FOR_EACH_NARG_(...) _FOR_EACH_ARG_N(__VA_ARGS__)
#define _FOR_EACH_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, N, ...) N
#define _FOR_EACH_RSEQ_N() 8, 7, 6, 5, 4, 3, 2, 1, 0

#define _FOR_EACH(N, what, x, ...) \
    CONCAT(_FOR_EACH_, N)(what, x, __VA_ARGS__)
/**@endcond*/

/**
 * Applies a callable expression specified by 'what' to each element
 * following it, including x.
 *
 * @param what
 * The callable expression (eg, a macro or a function).
 * @param ...
 * Elements to apply the expression to.
 * @example
 * @code
 * #define print_obj(obj) printf("%s\n", obj->name);
 * // Assumes obj1 and obj2 are defined as object*
 * FOR_EACH(print_obj, obj1, obj2)
 * // The above would be translated into:
 * // print_obj(obj1);
 * // print_obj(obj2);
 * @endcode
 */
#define FOR_EACH(what, ...) \
    _FOR_EACH(_FOR_EACH_NARG(__VA_ARGS__), what, __VA_ARGS__)

#include <socket.h>
#include <toolkit_math.h> /* TODO: remove */

/* Prototypes */

void
toolkit_import_register(const char *name, toolkit_func func);
bool
toolkit_check_imported(toolkit_func func);
void
toolkit_deinit(void);

#endif
