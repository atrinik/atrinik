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

/**
 * @file
 * Handles god related code. */

#include <global.h>

/**
 * Returns pointer to specified god's object through pntr_to_god_obj().
 * @param name God's name.
 * @return Pointer to god's object, NULL if doesn't match any god. */
object *find_god(const char *name)
{
	archetype *at;

	at = find_archetype(name);

	if (!at)
	{
		return NULL;
	}

	return &at->clone;
}

/**
 * This function is called whenever a player has switched to a new god.
 * It basically handles all the stat changes that happen to the player,
 * including the removal of god-given items (from the former cult).
 * @param op Player switching cults.
 * @param new_god New god to worship. */
void become_follower(object *op, object *new_god)
{
	if (!op || !new_god)
	{
		return;
	}

	CONTR(op)->socket.ext_title_flag = 1;

	if (op->race && new_god->slaying && strstr(op->race, new_god->slaying))
	{
		draw_info_format(COLOR_NAVY, op, "Fool! %s detests your kind!", new_god->name);

		if (rndm(0, op->level - 1) - 5 > 0)
		{
			cast_magic_storm(op, get_archetype("loose_magic"), new_god->level + 10);
		}

		return;
	}

	draw_info_format(COLOR_NAVY, op, "You become a follower of %s!", new_god->name);

	if (op->chosen_skill->title)
	{
		draw_info_format(COLOR_WHITE, op, "%s's blessing is withdrawn from you.", op->chosen_skill->title);
	}

	FREE_AND_COPY_HASH(op->chosen_skill->title, new_god->name);
	draw_info_format(COLOR_WHITE, op, "You are bathed in %s's aura.", new_god->name);
}

/**
 * Determines if op worships a god. Returns the godname if they do or
 * "none" if they have no god. In the case of an NPC, if they have no
 * god, we give them a random one.
 * @param op Object to get name of.
 * @return God name, "none" if nothing suitable. */
const char *determine_god(object *op)
{
	/* spells */
	if ((op->type == CONE || op->type == SWARM_SPELL) && op->title)
	{
		if (find_god(op->title))
		{
			return op->title;
		}
	}

	return shstr_cons.none;
}
