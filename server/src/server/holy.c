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

/**
 * @file
 * God related common functions. */

#include <global.h>

static godlink *init_godslist();
static void add_god_to_list(archetype *god_arch);

/**
 * Initializes a god structure. */
static godlink *init_godslist()
{
	godlink *gl = (godlink *) malloc(sizeof(godlink));

	if (gl == NULL)
	{
		LOG(llevError, "ERROR: init_godslist(): Out of memory.\n");
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
void init_gods()
{
	archetype *at = NULL;

	LOG(llevDebug, "Initializing gods...");

	for (at = first_archetype; at != NULL; at = at->next)
	{
		if (at->clone.type == GOD)
		{
			add_god_to_list(at);
		}
	}

	LOG(llevDebug, " done.\n");
}

/**
 * Adds specified god to linked list, giving it an ID.
 * @param god_arch God to add. If NULL, will log an error. */
static void add_god_to_list(archetype *god_arch)
{
	godlink *god;

	if (!god_arch)
	{
		LOG(llevBug, "BUG: Tried to add null god to list!\n");
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

#ifdef DEBUG_GODS
	LOG(llevDebug, "Adding god %s (%d) to list\n", god->name, god->id);
#endif
}

/**
 * Returns a random god.
 * @return A random god, or NULL if no god was found. */
godlink *get_rand_god()
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

	if (!god)
	{
		LOG(llevBug, "BUG: get_rand_god(): Can't find a random god!\n");
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
void free_all_god()
{
	godlink *god, *godnext;

	LOG(llevDebug, "Freeing god information.\n");

	for (god = first_god; god; god = godnext)
	{
		godnext = god->next;
		FREE_AND_CLEAR_HASH(god->name);
		free(god);
	}
}

/**
 * Prints all gods using LOG(). */
void dump_gods()
{
	godlink *glist;

	LOG(llevInfo, "\n");

	for (glist = first_god; glist; glist = glist->next)
	{
		object *god = pntr_to_god_obj(glist);
		char tmpbuf[HUGE_BUF];
		int tmpvar, gifts = 0;

		tmpbuf[0] = '\0';

		LOG(llevInfo, "GOD: %s\n", god->name);
		LOG(llevInfo, " avatar stats:\n");
		LOG(llevInfo, "  S:%d C:%d D:%d I:%d W:%d P:%d\n", god->stats.Str, god->stats.Con, god->stats.Dex, god->stats.Int, god->stats.Wis, god->stats.Pow);
		LOG(llevInfo, "  lvl:%d speed:%4.2f\n", god->level, god->speed);
		LOG(llevInfo, "  wc:%d ac:%d hp:%d dam:%d \n", god->stats.wc, god->stats.ac, god->stats.hp, god->stats.dam);
		LOG(llevInfo, " enemy: %s\n", god->title ? god->title : "NONE");

		if (god->other_arch)
		{
			object *serv = &god->other_arch->clone;
			LOG(llevInfo, " servant stats: (%s)\n", god->other_arch->name);
			LOG(llevInfo, "  S:%d C:%d D:%d I:%d W:%d P:%d\n", serv->stats.Str, serv->stats.Con, serv->stats.Dex, serv->stats.Int, serv->stats.Wis, serv->stats.Pow);
			LOG(llevInfo, "  lvl:%d speed:%4.2f\n", serv->level, serv->speed);
			LOG(llevInfo, "  wc:%d ac:%d hp:%d dam:%d \n", serv->stats.wc, serv->stats.ac, serv->stats.hp, serv->stats.dam);
		}
		else
		{
			LOG(llevInfo, " servant: NONE\n");
		}

		LOG(llevInfo, " aligned_race(s): %s\n", god->race);
		LOG(llevInfo, " enemy_race(s): %s\n", (god->slaying ? god->slaying : "none"));
		LOG(llevInfo, "%s", describe_protections(god, 1));

		strcat(tmpbuf, "\n aura:");

		strcat(tmpbuf, "\n paths:");

		if ((tmpvar = god->path_attuned))
		{
			strcat(tmpbuf, "\n  ");
			DESCRIBE_PATH(tmpbuf, tmpvar, "Attuned");
		}

		if ((tmpvar = god->path_repelled))
		{
			strcat(tmpbuf, "\n  ");
			DESCRIBE_PATH(tmpbuf, tmpvar, "Repelled");
		}

		if ((tmpvar = god->path_denied))
		{
			strcat(tmpbuf, "\n  ");
			DESCRIBE_PATH(tmpbuf, tmpvar, "Denied");
		}

		LOG(llevInfo, "%s\n", tmpbuf);
		LOG(llevInfo, " Desc: %s\n", god->msg ? god->msg : "---");
		LOG(llevInfo, " Priest gifts/limitations: ");

		if (!QUERY_FLAG(god, FLAG_USE_WEAPON))
		{
			gifts = 1;
			LOG(llevInfo, "\n  weapon use is forbidden");
		}

		if (!QUERY_FLAG(god, FLAG_USE_ARMOUR))
		{
			gifts = 1;
			LOG(llevInfo, "\n  no armour may be worn");
		}

		if (QUERY_FLAG(god, FLAG_UNDEAD))
		{
			gifts = 1;
			LOG(llevInfo, "\n  is undead");
		}

		if (QUERY_FLAG(god, FLAG_SEE_IN_DARK))
		{
			gifts = 1;
			LOG(llevInfo, "\n  has infravision ");
		}

		if (QUERY_FLAG(god, FLAG_XRAYS))
		{
			gifts = 1;
			LOG(llevInfo, "\n  has X-ray vision");
		}

		if (QUERY_FLAG(god, FLAG_REFL_MISSILE))
		{
			gifts = 1;
			LOG(llevInfo, "\n  reflect missiles");
		}

		if (QUERY_FLAG(god, FLAG_REFL_SPELL))
		{
			gifts = 1;
			LOG(llevInfo, "\n  reflect spells");
		}

		if (QUERY_FLAG(god, FLAG_STEALTH))
		{
			gifts = 1;
			LOG(llevInfo, "\n  is stealthy");
		}

		if (QUERY_FLAG(god, FLAG_SEE_INVISIBLE))
		{
			gifts = 1;
			LOG(llevInfo, "\n  is (permanently) invisible");
		}

		if (QUERY_FLAG(god, FLAG_BLIND))
		{
			gifts = 1;
			LOG(llevInfo, "\n  is blind");
		}

		if (god->last_heal)
		{
			gifts = 1;
			LOG(llevInfo, "\n  hp regenerate at %d",god->last_heal);
		}

		if (god->last_sp)
		{
			gifts = 1;
			LOG(llevInfo, "\n  sp regenerate at %d",god->last_sp);
		}

		if (god->last_eat)
		{
			gifts = 1;
			LOG(llevInfo, "\n  digestion is %s (%d)",god->last_eat<0?"slowed":"faster",god->last_eat);
		}

		if (god->last_grace)
		{
			gifts = 1;
			LOG(llevInfo, "\n  grace regenerates at %d",god->last_grace);
		}

		if (god->stats.luck)
		{
			gifts = 1;
			LOG(llevInfo, "\n  luck is %d",god->stats.luck);
		}

		if (!gifts)
		{
			LOG(llevInfo, "NONE");
		}

		LOG(llevInfo, "\n\n");
	}
}
