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
 * Handles god related code. */

#include <global.h>

static int lookup_god_by_name(const char *name);

/**
 * Returns the ID of specified god.
 * @param name God to search for.
 * @return Identifier of god, -1 if not found. */
static int lookup_god_by_name(const char *name)
{
	int godnr = -1;
	size_t nmlen = strlen(name);

	if (name && strcmp(name, "none"))
	{
		godlink *gl;

		for (gl = first_god; gl; gl = gl->next)
		{
			if (!strncmp(name, gl->name, MIN(strlen(gl->name), nmlen)))
			{
				break;
			}
		}

		if (gl)
		{
			godnr = gl->id;
		}
	}

	return godnr;
}

/**
 * Returns pointer to specified god's object through pntr_to_god_obj().
 * @param name God's name.
 * @return Pointer to god's object, NULL if doesn't match any god. */
object *find_god(const char *name)
{
	object *god = NULL;

	if (name)
	{
		godlink *gl;

		for (gl = first_god; gl; gl = gl->next)
		{
			if (!strcmp(name, gl->name))
			{
				break;
			}
		}

		if (gl)
		{
			god = pntr_to_god_obj(gl);
		}
	}

	return god;
}

/**
 * This function is called whenever a player has switched to a new god.
 * It basically handles all the stat changes that happen to the player,
 * including the removal of god-given items (from the former cult).
 * @param op Player switching cults.
 * @param new_god New god to worship. */
void become_follower(object *op, object *new_god)
{
	/* obj. containing god data */
	object *exp_obj = op->chosen_skill->exp_obj;
	int i;

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

	/* get rid of old god */
	if (exp_obj->title)
	{
		draw_info_format(COLOR_WHITE, op, "%s's blessing is withdrawn from you.", exp_obj->title);
		CLEAR_FLAG(exp_obj, FLAG_APPLIED);
		(void) change_abil(op, exp_obj);
		FREE_AND_CLEAR_HASH2(exp_obj->title);
	}

	/* now change to the new gods attributes to exp_obj */
	FREE_AND_COPY_HASH(exp_obj->title, new_god->name);
	exp_obj->path_attuned = new_god->path_attuned;
	exp_obj->path_repelled = new_god->path_repelled;
	exp_obj->path_denied = new_god->path_denied;
	/* copy god's protections */
	memcpy(exp_obj->protection, new_god->protection, sizeof(new_god->protection));

	/* make sure that certain immunities do NOT get passed to the
	 * follower! */
	for (i = 0; i < NROFATTACKS; i++)
	{
		if (exp_obj->protection[i] > 30 && (i == ATNR_FIRE || i == ATNR_COLD || i == ATNR_ELECTRICITY || i == ATNR_POISON))
		{
			exp_obj->protection[i] = 30;
		}
	}

	draw_info_format(COLOR_WHITE, op, "You are bathed in %s's aura.", new_god->name);

	SET_FLAG(exp_obj, FLAG_APPLIED);
	change_abil(op, exp_obj);
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
		if (lookup_god_by_name(op->title) >= 0)
		{
			return op->title;
		}
	}

	if (op->type == PLAYER)
	{
		object *tmp;

		for (tmp = op->inv; tmp; tmp = tmp->below)
		{
			if (tmp->type == EXPERIENCE && tmp->stats.Wis)
			{
				if (tmp->title)
				{
					return tmp->title;
				}
				else
				{
					return shstr_cons.none;
				}
			}
		}
	}

	return shstr_cons.none;
}
