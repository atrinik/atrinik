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
 * @ref BEACON "Beacon" related code. */

#include <global.h>

/** The beacons list. */
static objectlink *beacons_list;

/**
 * Add a beacon to ::beacons_list.
 * @param ob Beacon to add. */
void beacon_add(object *ob)
{
	objectlink *ol = get_objectlink();

	ol->objlink.ob = ob;
	ol->id = ob->count;
	objectlink_link(&beacons_list, NULL, NULL, beacons_list, ol);
}

/**
 * Remove a beacon from ::beacons_list.
 * @param ob Beacon to remove. */
void beacon_remove(object *ob)
{
	objectlink *ol;

	for (ol = beacons_list; ol; ol = ol->next)
	{
		if (ol->objlink.ob == ob)
		{
			objectlink_unlink(&beacons_list, NULL, ol);
			return_poolchunk(ol, pool_objectlink);
			break;
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
		if (ol->objlink.ob->name == name)
		{
			return ol->objlink.ob;
		}
	}

	return NULL;
}
