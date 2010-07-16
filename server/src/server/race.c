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
 * Handles code related to races. */

#include <global.h>

/**
 * This list is used for the item prefixes ('dwarven bolt', 'elven arrow',
 * etc). */
const char *item_races[NROF_ITEM_RACES] =
{
	"", "dwarven ", "elven ", "gnomish ", "drow ", "orcish ", "goblin ",
	"kobold ", "giant ", "tiny ", "demonish ", "draconish ", "ogre "
};

/**
 * Array of all the monster races. */
static ob_race *races = NULL;
/**
 * Number of all the monster races. */
static size_t num_races = 0;

/**
 * Link corpse to a race.
 * @param race_name Race name.
 * @param at Archetype of the corpse. */
static void race_add_corpse(shstr *race_name, archetype *at)
{
	ob_race *race;

	if (!at || !race_name)
	{
		return;
	}

	race = race_find(race_name);

	if (!race)
	{
		return;
	}

	race->corpse = at;
}

/**
 * Add an object to the racelist.
 * @param race_name Race name.
 * @param ob What object to add to the race. */
static void race_add(shstr *race_name, object *ob)
{
	ob_race *race;
	objectlink *ol;

	if (!ob || !race_name)
	{
		return;
	}

	race = race_find(race_name);

	/* No such race yet? Initialize it then. */
	if (!race)
	{
		size_t i, ii;

		races = realloc(races, sizeof(ob_race) * (num_races + 1));

		/* Now, insert the race into the correct spot in the array. */
		for (i = 0; i < num_races; i ++)
		{
			if (races[i].name > race_name)
			{
				break;
			}
		}

		/* If this is not the special case of insertion at the last point, then shift everything. */
		for (ii = num_races; ii > i; ii--)
		{
			races[ii] = races[ii - 1];
		}

		memset(&races[i], 0, sizeof(ob_race));
		FREE_AND_COPY_HASH(races[i].name, race_name);
		race = &races[i];
		num_races++;
	}

	ol = get_objectlink();
	ol->objlink.ob = ob;
	ol->id = ob->count;
	/* Link the new object link to the race's list. */
	objectlink_link(&race->members, NULL, NULL, race->members, ol);
	/* Increase the number of race's members. */
	race->num_members++;
}

/**
 * Comparison function for binary search in race_find(). */
static int race_compare(const void *one, const void *two)
{
	ob_race *one_race = (ob_race *) one;
	ob_race *two_race = (ob_race *) two;

	if (one == NULL)
	{
		return -1;
	}
	else if (two == NULL)
	{
		return 1;
	}

	if (one_race->name < two_race->name)
	{
		return -1;
	}
	else if (one_race->name > two_race->name)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

/**
 * Find a race.
 * @param name The name of the race to look for.
 * @return The race if found, NULL otherwise. */
ob_race *race_find(shstr *name)
{
	ob_race key;

	if (!races || !name)
	{
		return NULL;
	}

	key.name = name;

	return bsearch(&key, races, num_races, sizeof(ob_race), race_compare);
}

/**
 * Randomly select a race.
 * @return Race, NULL if there are no races available. */
ob_race *race_get_random()
{
	if (!races || !num_races)
	{
		return NULL;
	}

	return &races[rndm(1, num_races) - 1];
}

/**
 * Initialize races by looking through all the archetypes and checking if
 * the archetype is a @ref MONSTER or @ref PLAYER. */
void race_init()
{
	archetype *at, *tmp;
	size_t i;

	LOG(llevDebug, "Initializing races...\n");

	for (at = first_archetype; at; at = at->next)
	{
		if (at->clone.type == MONSTER || at->clone.type == PLAYER)
		{
			race_add(at->clone.race, &at->clone);
		}
	}

	/* Now search for corpses. */
	for (at = first_archetype; at; at = at->next)
	{
		if (at->clone.type == CONTAINER && at->clone.sub_type == ST1_CONTAINER_CORPSE)
		{
			race_add_corpse(at->clone.race, at);
		}
	}

	/* Try to find the default corpse archetype. */
	tmp = find_archetype(RACE_CORPSE_DEFAULT);

	if (!tmp)
	{
		LOG(llevError, " ERROR: race_init(): Can't find required archetype: '%s'.\n", RACE_CORPSE_DEFAULT);
		return;
	}

	/* Look through the races, and assign the default corpse archetype
	 * to those which do not have any yet. */
	for (i = 0; i < num_races; i++)
	{
		if (!races[i].corpse)
		{
			races[i].corpse = tmp;
		}
	}

	LOG(llevDebug, " Done.\n");
}

/**
 * Dumps all race information. */
void race_dump()
{
	size_t i;
	objectlink *ol;

	for (i = 0; i < num_races; i++)
	{
		LOG(llevInfo, "\nRACE '%s', corpse: '%s', %d members: ", races[i].name, races[i].corpse->name, races[i].num_members);

		for (ol = races[i].members; ol; ol = ol->next)
		{
			LOG(llevInfo, "%s, ", ol->objlink.ob->arch->name);
		}
	}

	LOG(llevInfo, "\n");
}

/**
 * Frees all race related information. */
void race_free()
{
	size_t i;
	objectlink *ol, *ol_next;

	for (i = 0; i < num_races; i++)
	{
		FREE_ONLY_HASH(races[i].name);

		for (ol = races[i].members; ol; ol = ol_next)
		{
			ol_next = ol->next;
			free_objectlink_simple(ol);
		}
	}

	free(races);
	LOG(-1, "Freed %"FMT64U" races.\n", (uint64) num_races);
}
