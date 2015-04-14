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
#include <check_proto.h>

/*
 * Setup function. */
void check_setup(void)
{
    init(0, NULL);
}

/*
 * Cleanup function. */
void check_teardown(void)
{
    cleanup();
}

/*
 * Sets up environment for doing tests related to players.
 */
void check_setup_env_pl(mapstruct **map, object **pl)
{
    HARD_ASSERT(map != NULL);
    HARD_ASSERT(pl != NULL);

    *map = get_empty_map(24, 24);
    fail_if(*map == NULL, "Could not allocate a map.");

    *pl = player_get_dummy();
    fail_if(*pl == NULL, "Could not create player object.");

    *pl = insert_ob_in_map(*pl, *map, NULL, 0);
    fail_if(*pl == NULL, "Could not insert player.");
}

/* The main unit test function. Calls other functions to do the unit
 * tests. */
void check_main(void)
{
    toolkit_import(path);

    path_ensure_directories("unit/bugs/");
    path_ensure_directories("unit/commands/");
    path_ensure_directories("unit/server/");
    path_ensure_directories("unit/types/");

    /* bugs */
    check_bug_85();

    /* unit/commands */
    check_commands_object();

    /* unit/server */
    check_server_ban();
    check_server_arch();
    check_server_object();
    check_server_pbkdf2();
    check_server_re_cmp();
    check_server_cache();
    check_server_shstr();
    check_server_string();
    check_server_utils();

    /* unit/types */
    check_types_light_apply();
    check_types_sound_ambient();
}
