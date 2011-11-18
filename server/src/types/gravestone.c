/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
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

/**
 * @file
 * Handles code related to @ref GRAVESTONE "gravestones". */

#include <global.h>

/**
 * Create text for a gravestone object.
 * @param op Object that died.
 * @return Pointer to a static string containing the gravestone text. */
const char *gravestone_text(object *op)
{
	static char buf2[MAX_BUF];
	char buf[MAX_BUF], race[MAX_BUF];
	time_t now = time(NULL);

	strcpy(buf2, "R.I.P.\n\n");

	if (op->type == PLAYER)
	{
		snprintf(buf, sizeof(buf), "Here rests the hero %s the %s\n", op->name, player_get_race_class(op, race, sizeof(race)));
	}
	else
	{
		snprintf(buf, sizeof(buf), "%s\n", op->name);
	}

	strncat(buf2, buf, sizeof(buf2) - strlen(buf2) - 1);

	if (op->type == PLAYER)
	{
		snprintf(buf, sizeof(buf), "who was killed at level %d\nby %s.", op->level, strcmp(CONTR(op)->killer, "") ? CONTR(op)->killer : "something nasty");
	}
	else
	{
		snprintf(buf, sizeof(buf), "who died at level %d.", op->level);
	}

	strncat(buf2, buf, sizeof(buf2) - strlen(buf2) - 1);

	strftime(buf, sizeof(buf), "\n\n%b %d %Y", localtime(&now));
	strncat(buf2, buf, sizeof(buf2) - strlen(buf2) - 1);

	return buf2;
}

/**
 * Initialize the gravestone type object methods. */
void object_type_init_gravestone(void)
{
}
