/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
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

/* The main unit test function. Calls other functions to do the unit
 * tests. */
void check_main()
{
	/* bugs */
	check_bug_85();

	/* unit/commands */
	check_commands_object();

	/* unit/server */
	check_server_ban();
	check_server_arch();
	check_server_object();
	check_server_re_cmp();
	check_server_cache();
	/* Anything that needs the shared string interface should go above
	 * this line (arches, artifacts, players, etc). */
	check_server_shstr();
	check_server_utils();
}
