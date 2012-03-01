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
 * Region management.
 *
 * A region is a group of maps. It includes a "parent" region. */

#include <global.h>

/** First region. */
region *first_region = NULL;

static region *get_region_struct(void);
static void assign_region_parents(void);

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

	logger_print(LOG(DEBUG), "Got no region for region %s.", region_name);
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

	logger_print(LOG(DEBUG), "Region %s has no parent and no longname.", r->name);
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

	logger_print(LOG(DEBUG), "Region %s has no parent and no msg.", r->name);
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
	object *exit_ob;

	if (op->type != PLAYER)
	{
		logger_print(LOG(BUG), "called for non-player object.");
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
			exit_ob = get_object();
			FREE_AND_COPY_HASH(EXIT_PATH(exit_ob), reg->jailmap);
			/* Damned exits reset savebed and remove teleports, so the prisoner can't escape */
			SET_FLAG(exit_ob, FLAG_DAMNED);
			EXIT_X(exit_ob) = reg->jailx;
			EXIT_Y(exit_ob) = reg->jaily;
			return exit_ob;
		}
		else
		{
			reg = reg->parent;
		}
	}

	logger_print(LOG(DEBUG), "No suitable jailmap for region %s was found.", reg->name);
	return NULL;
}

/**
 * Initializes regions from the regions file. */
void init_regions(void)
{
	FILE *fp;
	char filename[MAX_BUF];
	region *new = NULL, *reg;
	char buf[HUGE_BUF * 4], msgbuf[HUGE_BUF], *key = NULL, *value, *end;
	int msgpos = 0;

	/* Only do this once */
	if (first_region)
	{
		return;
	}

	snprintf(filename, sizeof(filename), "%s/regions.reg", settings.mapspath);
	fp = fopen(filename, "r");

	if (!fp)
	{
		logger_print(LOG(ERROR), "Can't open regions file: %s.", filename);
		exit(1);
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
			new->name = strdup(value);
		}
		else if (!strcmp(key, "parent"))
		{
			*end = '\0';
			new->parent_name = strdup(value);
		}
		else if (!strcmp(key, "longname"))
		{
			*end = '\0';
			new->longname = strdup(value);
		}
		else if (!strcmp(key, "map_first"))
		{
			*end = '\0';
			new->map_first = strdup(value);
		}
		else if (!strcmp(key, "map_bg"))
		{
			*end = '\0';
			new->map_bg = strdup(value);
		}
		else if (!strcmp(key, "map_quest"))
		{
			*end = '\0';
			new->map_quest = KEYWORD_IS_TRUE(value);
		}
		/* Jail entries are of the form: /path/to/map x y */
		else if (!strcmp(key, "jail"))
		{
			char path[MAX_BUF];
			int x, y;

			if (sscanf(value, "%[^ ] %d %d\n", path, &x, &y) != 3)
			{
				logger_print(LOG(ERROR), "Malformed regions entry: jail %s", value);
				exit(1);
			}

			new->jailmap = strdup(path);
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
				new->msg = strdup(msgbuf);
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
			logger_print(LOG(ERROR), "Got unknown value in region file: %s %s", key, value);
			exit(1);
		}
	}

	assign_region_parents();

	fclose(fp);
}

/**
 * Allocates and zeros a region struct.
 * @return Initialized region structure. */
static region *get_region_struct(void)
{
	region *new = (region *) calloc(1, sizeof(region));

	if (new == NULL)
	{
		logger_print(LOG(ERROR), "OOM.");
		exit(1);
	}

	return new;
}

/**
 * Links child with their parent from the parent_name field. */
static void assign_region_parents(void)
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
}

/**
 * Deinitializes all regions. */
void free_regions(void)
{
	region *reg, *next;

	for (reg = first_region; reg; reg = next)
	{
		next = reg->next;

		FREE_AND_NULL_PTR(reg->name);
		FREE_AND_NULL_PTR(reg->parent_name);
		FREE_AND_NULL_PTR(reg->longname);
		FREE_AND_NULL_PTR(reg->map_first);
		FREE_AND_NULL_PTR(reg->map_bg);
		FREE_AND_NULL_PTR(reg->msg);
		FREE_AND_NULL_PTR(reg->jailmap);
		free(reg);
	}
}
