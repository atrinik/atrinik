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

#include <global.h>
#include <check.h>
#include <check_proto.h>
#include <malloc.h>

#if defined(HAVE_MCHECK_H) && !defined(NDEBUG)
#include <mcheck.h>
static void no_op(enum mcheck_status status) {}
#endif

START_TEST(test_memory_emalloc)
{
    char *ptr;

#if defined(HAVE_MCHECK_H) && !defined(NDEBUG)
    mcheck(&no_op);
#endif

    ptr = emalloc(8);
    ck_assert_ptr_ne(ptr, NULL);
    snprintf(ptr, 8, "%s", "testing");
    ck_assert_str_eq(ptr, "testing");

#if defined(HAVE_MCHECK_H) && !defined(NDEBUG)
    ck_assert_int_eq(mprobe(ptr), MCHECK_OK);
    mcheck(NULL);
#endif

    efree(ptr);
}
END_TEST

START_TEST(test_memory_efree)
{
    void *ptr;

#if defined(HAVE_MCHECK_H) && !defined(NDEBUG)
    mcheck(&no_op);
#endif

    ptr = emalloc(42);
    ck_assert_ptr_ne(ptr, NULL);

#if defined(HAVE_MCHECK_H) && !defined(NDEBUG)
    ck_assert_int_eq(mprobe(ptr), MCHECK_OK);
#endif

    efree(ptr);

#if defined(HAVE_MCHECK_H) && !defined(NDEBUG)
    ck_assert_int_eq(mprobe(ptr), MCHECK_FREE);
    mcheck(NULL);
#endif
}
END_TEST

START_TEST(test_memory_ecalloc)
{
    int32_t *ptr;
    size_t i;

#if defined(HAVE_MCHECK_H) && !defined(NDEBUG)
    mcheck(&no_op);
#endif

    ptr = ecalloc(42, sizeof(*ptr));
    ck_assert_ptr_ne(ptr, NULL);

#if defined(HAVE_MCHECK_H) && !defined(NDEBUG)
    ck_assert_int_eq(mprobe(ptr), MCHECK_OK);
#endif

    for (i = 0; i < 42; i++) {
        ck_assert_uint_eq(ptr[i], 0);
    }

    efree(ptr);

#if defined(HAVE_MCHECK_H) && !defined(NDEBUG)
    ck_assert_int_eq(mprobe(ptr), MCHECK_FREE);
    mcheck(NULL);
#endif
}
END_TEST

START_TEST(test_memory_erealloc)
{
    char *ptr;

#if defined(HAVE_MCHECK_H) && !defined(NDEBUG)
    mcheck(&no_op);
#endif

    ptr = emalloc(4);
    ck_assert_ptr_ne(ptr, NULL);

#if defined(HAVE_MCHECK_H) && !defined(NDEBUG)
    ck_assert_int_eq(mprobe(ptr), MCHECK_OK);
#endif

    snprintf(ptr, 4, "%s", "testing");
    ck_assert_str_eq(ptr, "tes");

    ptr = erealloc(ptr, 8);

#if defined(HAVE_MCHECK_H) && !defined(NDEBUG)
    ck_assert_int_eq(mprobe(ptr), MCHECK_OK);
#endif

    ck_assert_str_eq(ptr, "tes");
    snprintf(ptr, 8, "%s", "testing");
    ck_assert_str_eq(ptr, "testing");

    ptr = erealloc(ptr, 20);

#if defined(HAVE_MCHECK_H) && !defined(NDEBUG)
    ck_assert_int_eq(mprobe(ptr), MCHECK_OK);
#endif

    ck_assert_str_eq(ptr, "testing");

    ptr = erealloc(ptr, 8);

#if defined(HAVE_MCHECK_H) && !defined(NDEBUG)
    ck_assert_int_eq(mprobe(ptr), MCHECK_OK);
#endif

    ck_assert_str_eq(ptr, "testing");

    efree(ptr);

#if defined(HAVE_MCHECK_H) && !defined(NDEBUG)
    ck_assert_int_eq(mprobe(ptr), MCHECK_FREE);
    mcheck(NULL);
#endif
}
END_TEST

START_TEST(test_memory_reallocz)
{
    int32_t *ptr;
    size_t i;

#if defined(HAVE_MCHECK_H) && !defined(NDEBUG)
    mcheck(&no_op);
#endif

    ptr = emalloc(sizeof(*ptr) * 42);
    ck_assert_ptr_ne(ptr, NULL);

    for (i = 0; i < 42; i++) {
        ptr[i] = 1337;
    }

#if defined(HAVE_MCHECK_H) && !defined(NDEBUG)
    ck_assert_int_eq(mprobe(ptr), MCHECK_OK);
#endif

    for (i = 0; i < 42; i++) {
        ck_assert_uint_eq(ptr[i], 1337);
    }

    ptr = ereallocz(ptr, sizeof(*ptr) * 42, sizeof(*ptr) * 42 * 2);

#if defined(HAVE_MCHECK_H) && !defined(NDEBUG)
    ck_assert_int_eq(mprobe(ptr), MCHECK_OK);
#endif

    for (i = 0; i < 42; i++) {
        ck_assert_uint_eq(ptr[i], 1337);
    }

    for (i = 42; i < 42 * 2; i++) {
        ck_assert_uint_eq(ptr[i], 0);
    }

    ptr = ereallocz(ptr, sizeof(*ptr) * 42 * 2, sizeof(*ptr) * 42);

#if defined(HAVE_MCHECK_H) && !defined(NDEBUG)
    ck_assert_int_eq(mprobe(ptr), MCHECK_OK);
#endif

    for (i = 0; i < 42; i++) {
        ck_assert_uint_eq(ptr[i], 1337);
    }

    efree(ptr);

#if defined(HAVE_MCHECK_H) && !defined(NDEBUG)
    ck_assert_int_eq(mprobe(ptr), MCHECK_FREE);
    mcheck(NULL);
#endif
}
END_TEST

static Suite *suite(void)
{
    Suite *s = suite_create("memory");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_memory_emalloc);
    tcase_add_test(tc_core, test_memory_efree);
    tcase_add_test(tc_core, test_memory_ecalloc);
    tcase_add_test(tc_core, test_memory_erealloc);
    tcase_add_test(tc_core, test_memory_reallocz);

    return s;
}

void check_server_memory(void)
{
    check_run_suite(suite(), __FILE__);
}
