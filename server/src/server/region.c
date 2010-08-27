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
 * Region management.
 *
 * A region is a group of maps. It includes a "parent" region. */

#include <global.h>

/** First region. */
region *first_region = NULL;

static region *get_region_struct();
static void assign_region_parents();

/**
 * Gets a region by name.
 *
 * Used by the map parsing code.
 * @param region_name Name of region.
 * @return
 * - Matching region
 * - If no match, returns the first region with the 'fallback' property set and LOG()s to debug.
 * - If it can't find a matching name and a fallback region it LOG()s an info message and returns NULL. */
region *get_region_by_name(const char *region_name)
{
	region *reg;

	for (reg = first_region; reg; reg = reg->next)
	{
		if (!strcmp(reg->name, region_name))
		{
			return reg;
		}
	}

	LOG(llevInfo, "BUG: Got no region for region %s.\n", region_name);
	return NULL;
}

/**
 * Gets the longname of a region.
 *
 * The longname of a region is not a required field, any given region
 * may want to not set it and use the parent's one instead.
 * @param r Region we're searching the longname.
 * @return Long name of a region if found. Will also search recursively
 * in parents. NULL is never returned, instead a fake region name is returned. */
char *get_region_longname(const region *r)
{
	if (r->longname)
	{
		return r->longname;
	}
	else if (r->parent)
	{
		return get_region_longname(r->parent);
	}

	LOG(llevDebug, "BUG: Region %s has no parent and no longname.\n", r->name);
	return "no region name";
}

/**
 * Gets a message for a region.
 * @param r Region. Can't be NULL.
 * @return Message of a region if found. Will also search recursively in
 * parents. NULL is never returned, instead a fake region message is returned. */
char *get_region_msg(const region *r)
{
	if (r->msg)
	{
		return r->msg;
	}
	else if (r->parent)
	{
		return get_region_msg(r->parent);
	}

	LOG(llevDebug, "BUG: Region %s has no parent and no msg.\n", r->name);
	return "no region message";
}

/**
 * Returns an object which is an exit through which the player represented by
 * op should be sent in order to be imprisoned. If there is no suitable place
 * to which an exit can be constructed, then NULL will be returned.
 * @param op Object we want to jail. Must be a player.
 * @return Exit to jail, or NULL, in which case a message is LOG()ged . */
object *get_jail_exit(object *op)
{
	region *reg;
	object *exit;

	if (op->type != PLAYER)
	{
		LOG(llevBug, "BUG: get_jail_exit() called for non-player object.\n");
		return NULL;
	}

	if (!op->map->region)
	{
		return NULL;
	}

	reg = op->map->region;

	while (reg)
	{
		if (reg->jailmap)
		{
			exit = get_object();
			FREE_AND_COPY_HASH(EXIT_PATH(exit), reg->jailmap);
			/* Damned exits reset savebed and remove teleports, so the prisoner can't escape */
			SET_FLAG(exit, FLAG_DAMNED);
			EXIT_X(exit) = reg->jailx;
			EXIT_Y(exit) = reg->jaily;
			return exit;
		}
		else
		{
			reg = reg->parent;
		}
	}

	LOG(llevDebug, "No suitable jailmap for region %s was found.\n", reg->name);
	return NULL;
}

/**
 * Initializes regions from the regions file. */
void init_regions()
{
	FILE *fp;
	char filename[MAX_BUF];
	int comp;
	region *new = NULL, *reg;
	char buf[HUGE_BUF], msgbuf[HUGE_BUF], *key = NULL, *value, *end;
	int msgpos = 0;

	/* Only do this once */
	if (first_region)
	{
		return;
	}

	snprintf(filename, sizeof(filename), "%s/regions.reg", settings.mapdir);
	LOG(llevDebug, "Reading regions from %s...\n", filename);

	if ((fp = open_and_uncompress(filename, 0, &comp)) == NULL)
	{
		LOG(llevError, "init_regions(): Can't open regions file: %s.\n", filename);
		return;
	}

	while (fgets(buf, sizeof(buf) - 1, fp))
	{
		buf[sizeof(buf) - 1] = '\0';
		key = buf;

		while (isspace(*key))
		{
			key++;
		}

		/* Empty line or a comment */
		if (*key == '\0' || *key == '#')
		{
			continue;
		}

		value = strchr(key, ' ');

		if (!value)
		{
			end = strchr(key, '\n');
			*end = '\0';
		}
		else
		{
			*value = '\0';
			value++;

			while (isspace(*value))
			{
				value++;
			}

			end = strchr(value, '\n');
		}

		if (!strcmp(key, "region"))
		{
			*end = '\0';
			new = get_region_struct();
			new->name = strdup_local(value);
		}
		else if (!strcmp(key, "parent"))
		{
			*end = '\0';
			new->parent_name = strdup_local(value);
		}
		else if (!strcmp(key, "longname"))
		{
			*end = '\0';
			new->longname = strdup_local(value);
		}
		/* Jail entries are of the form: /path/to/map x y */
		else if (!strcmp(key, "jail"))
		{
			char path[MAX_BUF];
			int x, y;

			if (sscanf(value, "%[^ ] %d %d\n", path, &x, &y) != 3)
			{
				LOG(llevError, "init_regions(): Malformated regions entry: jail %s\n", value);
				continue;
			}

			new->jailmap = strdup_local(path);
			new->jailx = x;
			new->jaily = y;
		}
		else if (!strcmp(key, "msg"))
		{
			while (fgets(buf, sizeof(buf) - 1, fp))
			{
				if (!strcmp(buf, "endmsg\n"))
				{
					break;
				}

				strcpy(msgbuf + msgpos, buf);
				msgpos += strlen(buf);
			}

			if (msgpos != 0)
			{
				new->msg = strdup_local(msgbuf);
			}

			/* we have to reset msgpos, or the next region will store both msg blocks. */
			msgpos = 0;
		}
		else if (!strcmp(key, "end"))
		{
			/* Place this new region last on the list, if the list is empty put it first */
			for (reg = first_region; reg != NULL && reg->next != NULL; reg = reg->next)
			{
			}

			if (reg == NULL)
			{
				first_region = new;
			}
			else
			{
				reg->next = new;
			}

			new = NULL;
		}
		else
		{
			/* We should never get here, if we have, then something is wrong */
			LOG(llevError, "Got unknown value in region file: %s %s\n", key, value);
		}
	}

	assign_region_parents();
	LOG(llevDebug, " done\n");

	close_and_delete(fp, comp);
}

/**
 * Allocates and zeros a region struct.
 * @return Initialized region structure. */
static region *get_region_struct()
{
	region *new = (region *) CALLOC(1, sizeof(region));

	if (new == NULL)
	{
		LOG(llevError, "ERROR: get_region_struct(): Out of memory.");
	}

	memset(new, 0, sizeof(region));
	return new;
}

/**
 * Links child with their parent from the parent_name field. */
static void assign_region_parents()
{
	region *reg;
	uint32 parent_count = 0, region_count = 0;

	for (reg = first_region; reg; reg = reg->next)
	{
		if (reg->parent_name)
		{
			reg->parent = get_region_by_name(reg->parent_name);
			parent_count++;
		}

		region_count++;
	}

	LOG(llevDebug, "Assigned %u regions with %u parents.\n", region_count, parent_count);
}

/**
 * Deinitializes all regions. */
void free_regions()
{
	region *reg, *next;

	LOG(llevDebug, "Freeing regions.\n");

	for (reg = first_region; reg; reg = next)
	{
		next = reg->next;

		FREE_AND_NULL_PTR(reg->name);
		FREE_AND_NULL_PTR(reg->parent_name);
		FREE_AND_NULL_PTR(reg->longname);
		FREE_AND_NULL_PTR(reg->msg);
		FREE_AND_NULL_PTR(reg->jailmap);
		CFREE(reg);
	}
}
