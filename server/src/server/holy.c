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
 * God related common functions. */

#include <global.h>

static godlink *init_godslist();
static void add_god_to_list(archetype *god_arch);

/**
 * Initializes a god structure. */
static godlink *init_godslist(void)
{
	godlink *gl = (godlink *) malloc(sizeof(godlink));

	if (gl == NULL)
	{
		logger_print(LOG(ERROR), "OOM.");
		exit(1);
	}

	/* How to describe the god to the player */
	gl->name = NULL;
	/* Pointer to the archetype of this god */
	gl->arch = NULL;
	/* ID of the god */
	gl->id = 0;
	/* Next god in this linked list */
	gl->next = NULL;

	return gl;
}

/**
 * This takes a look at all of the archetypes to find the objects which
 * correspond to the @ref GOD "gods". */
void init_gods(void)
{
	archetype *at = NULL;

	for (at = first_archetype; at != NULL; at = at->next)
	{
		if (at->clone.type == GOD)
		{
			add_god_to_list(at);
		}
	}
}

/**
 * Adds specified god to linked list, giving it an ID.
 * @param god_arch God to add. If NULL, will log an error. */
static void add_god_to_list(archetype *god_arch)
{
	godlink *god;

	if (!god_arch)
	{
		return;
	}

	god = init_godslist();

	god->arch = god_arch;
	FREE_AND_COPY_HASH(god->name, god_arch->clone.name);

	if (!first_god)
	{
		god->id = 1;
	}
	else
	{
		god->id = first_god->id + 1;
		god->next = first_god;
	}

	first_god = god;
}

/**
 * Returns a random god.
 * @return A random god, or NULL if no god was found. */
godlink *get_rand_god(void)
{
	godlink *god = first_god;
	int i;

	if (god)
	{
		for (i = RANDOM() % (god->id) + 1; god; god = god->next)
		{
			if (god->id == i)
			{
				break;
			}
		}
	}

	return god;
}

/**
 * Returns a pointer to the object.
 *
 * We need to be VERY careful about using this, as we are returning a
 * pointer to the archetype::clone object.
 * @param godlnk God to get object. */
object *pntr_to_god_obj(godlink *godlnk)
{
	object *god = NULL;

	if (godlnk && godlnk->arch)
		god = &godlnk->arch->clone;

	return god;
}

/** Frees all god information. */
void free_all_god(void)
{
	godlink *god, *godnext;

	for (god = first_god; god; god = godnext)
	{
		godnext = god->next;
		FREE_AND_CLEAR_HASH(god->name);
		free(god);
	}
}
