/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
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
 * Player related common functions. */

#include <global.h>

/**
 * Looks for the skill and returns a pointer to it if found.
 * @param op The object to look for the skill in.
 * @param skillnr Skill ID.
 * @return The skill if found, NULL otherwise. */
object *find_skill(object *op, int skillnr)
{
	object *tmp;

	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		if (tmp->type == SKILL && tmp->stats.sp == skillnr)
		{
			return tmp;
		}
	}

	return NULL;
}

/**
 * Determine if the attacktype represented by the specified attack number
 * is enabled for dragon players.
 * @param attacknr Attacktype to check
 * @return 1 if the player can gain resistances in that, 0 otherwise. */
int atnr_is_dragon_enabled(int attacknr)
{
	if (attacknr == ATNR_MAGIC || attacknr == ATNR_FIRE || attacknr == ATNR_ELECTRICITY || attacknr == ATNR_COLD || attacknr == ATNR_ACID || attacknr == ATNR_POISON)
	{
		return 1;
	}

	return 0;
}

/**
 * Checks if player is a dragon.
 * @param op Player to check.
 * @return 1 if the player is of the dragon race, 0 otherwise. */
int is_dragon_pl(object *op)
{
	if (op != NULL && op->type == PLAYER && op->arch != NULL && op->arch->clone.race != NULL && strcmp(op->arch->clone.race, "dragon") == 0)
	{
		return 1;
	}

	return 0;
}
