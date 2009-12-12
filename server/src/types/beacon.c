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
 * @ref BEACON "Beacon" related code. */

#include <global.h>

/** The beacons list. */
static objectlink *beacons_list;

/**
 * Add a beacon to ::beacons_list.
 * @param ob Beacon to add. */
void beacon_add(object *ob)
{
	objectlink *ol = beacons_list;

	beacons_list = get_objectlink();
	beacons_list->ob = ob;
	beacons_list->id = ob->count;
	beacons_list->next = ol;
}

/**
 * Remove a beacon from ::beacons_list.
 * @param ob Beacon to remove. */
void beacon_remove(object *ob)
{
	objectlink *this;

	if (!beacons_list)
	{
		return;
	}

	/* Easier if the first object in the list matches */
	if (beacons_list->ob == ob)
	{
		this = beacons_list;
		beacons_list = this->next;
		free(this);
	}
	else
	{
		objectlink *prev = beacons_list;

		for (this = beacons_list->next; this; this = this->next)
		{
			if (this->ob == ob)
			{
				break;
			}

			prev = this;
		}

		if (this)
		{
			prev->next = this->next;
			free(this);
		}
	}
}

/**
 * Locate a beacon object in ::beacons_list.
 * @param name Name of the beacon to locate. Must be a shared string.
 * @return The beacon object if found, NULL otherwise. */
object *beacon_locate(const char *name)
{
	objectlink *ol;

	for (ol = beacons_list; ol; ol = ol->next)
	{
		if (ol->ob->name == name)
		{
			return ol->ob;
		}
	}

	return NULL;
}
