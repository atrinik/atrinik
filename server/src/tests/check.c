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

/* This is the main file for unit tests. From here, we call all unit
 * test functions. */

#include <global.h>
#include <check.h>
#include <checkstd.h>
#include <check_proto.h>
#include <toolkit_string.h>

static int saved_argc; ///< Stored argc.
static char **saved_argv; ///< Stored argv.
static enum fork_status fork_st; ///< Current fork status.

/*
 * Setup function.
 */
void check_setup(void)
{
    init(saved_argc, saved_argv);
}

/*
 * Cleanup function.
 */
void check_teardown(void)
{
    cleanup();

    if (fork_st != CK_FORK) {
        size_t num = memory_check_leak(false);

        if (num != 0) {
            fprintf(stderr, "%" PRIu64 " memory leaks detected!\n",
                    (uint64_t) num);
            abort();
        }
    } else {
        size_t num = memory_check_leak(true);

        if (num != 0) {
            ck_abort_msg("%" PRIu64 " memory leaks detected!", (uint64_t) num);
        }
    }
}

/*
 * Test setup function.
 */
void check_test_setup(void)
{
    if (fork_st != CK_FORK) {
        return;
    }

}

/*
 * Test cleanup function.
 */
void check_test_teardown(void)
{
    if (fork_st != CK_FORK) {
        return;
    }
}

/*
 * Sets up environment for doing tests related to players.
 */
void check_setup_env_pl(mapstruct **map, object **pl)
{
    HARD_ASSERT(map != NULL);
    HARD_ASSERT(pl != NULL);

    *map = get_empty_map(24, 24);
    ck_assert(*map != NULL);

    *pl = player_get_dummy();
    ck_assert(*pl != NULL);

    *pl = insert_ob_in_map(*pl, *map, NULL, 0);
    ck_assert(*pl != NULL);
}

/*
 * Runs the specified test suite.
 */
void check_run_suite(Suite *suite, const char *file)
{
    SRunner *srunner;
    char *sub, buf[HUGE_BUF], buf2[HUGE_BUF];

    toolkit_import(string);
    toolkit_import(path);

    srunner = srunner_create(suite);
    fork_st = srunner_fork_status(srunner);

    sub = string_last(file, "tests/");

    if (sub == NULL) {
        log_error("Bad filename for suite: %s", file);
        abort();
    }

    sub = string_sub(sub, 0, -2);
    path_ensure_directories(sub);
    snprintf(VS(buf2), "%s.xml", sub);
    srunner_set_xml(srunner, buf2);
    snprintf(VS(buf), "%s.out", sub);
    srunner_set_log(srunner, buf);
    efree(sub);

    srunner_run_all(srunner, CK_ENV);
    srunner_ntests_failed(srunner);
    srunner_free(srunner);
}

/* The main unit test function. Calls other functions to do the unit
 * tests. */
void check_main(int argc, char **argv)
{
    int i;

    toolkit_import(string);

    saved_argc = argc;
    saved_argv = malloc(sizeof(*argv) * argc);

    if (saved_argv == NULL) {
        log_error("OOM.");
        abort();
    }

    for (i = 0; i < argc; i++) {
        saved_argv[i] = strdup(argv[i]);

        if (saved_argv[i] == NULL) {
            log_error("OOM.");
            abort();
        }
    }

    /* bugs */
    check_bug_85();

    /* unit/commands */
    check_commands_object();

    /* unit/server */
    check_server_ban();
    check_server_arch();
    check_server_attack();
    check_server_object();
    check_server_packet();
    check_server_pbkdf2();
    check_server_re_cmp();
    check_server_cache();
    check_server_memory();
    check_server_shstr();
    check_server_string();
    check_server_stringbuffer();
    check_server_utils();

    /* unit/types */
    check_types_light_apply();
    check_types_sound_ambient();

    for (i = 0; i < argc; i++) {
        free(saved_argv[i]);
    }

    free(saved_argv);
}
