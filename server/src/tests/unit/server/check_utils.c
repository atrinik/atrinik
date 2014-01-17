/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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
#include <stdarg.h>

static Suite *shstr_suite(void)
{
    Suite *s = suite_create("utils");
    TCase *tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, NULL, NULL);

    suite_add_tcase(s, tc_core);

    return s;
}

void check_server_utils(void)
{
    Suite *s = shstr_suite();
    SRunner *sr = srunner_create(s);

    srunner_set_xml(sr, "unit/server/utils.xml");
    srunner_set_log(sr, "unit/server/utils.out");
    srunner_run_all(sr, CK_ENV);
    srunner_ntests_failed(sr);
    srunner_free(sr);
}
