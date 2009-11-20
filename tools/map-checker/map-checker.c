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
 * Small utility to check a map file for common mistakes.
 *
 * Compile as:
 *  gcc map-checker.c -O3 -Wall -W -pedantic -Werror -o map-checker
 *
 * Use as:
 *  ./map-checker "your map" */

#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>

/** Maximum size of any character buffer. */
#define MAX_BUF 4096

/** Path to archetypes file. */
#define ARCHETYPES_FILE "../../arch/archetypes"

/** Object structure. */
typedef struct object {
	/** Name of this object. */
	char archname[MAX_BUF];

	/** Object type */
	int type;

	/** Object layer. */
	int layer;

	/** Is this object blocking the tile? */
	int no_pass;

	/** Inventory of this object. */
	struct object *inv;

	/** Next object in this list. */
	struct object *next;
} object;

/** Map tile structure */
typedef struct map_tile {
	/** X position of this tile. */
	int x;

	/** Y position of this tile. */
	int y;

	/** Linked list of objects on this tile. */
	struct object *objects;

	/** Next map tile. */
	struct map_tile *next;
} map_tile;

/** Arch structure */
typedef struct arch {
	/** Name of this arch. */
	char archname[MAX_BUF];

	/** Arch type. */
	int type;

	/** Arch layer. */
	int layer;

	/** Is this object blocking the tile? */
	int no_pass;

	/** Next arch. */
	struct arch *next;
} arch;

/** Maximum number of layers. */
#define MAX_LAYERS 7

/** The object is a spawn point. */
#define SPAWN_POINT 81

/** Map tile list. */
map_tile *map_tile_list = NULL;
/** The arch list. */
arch *arch_list = NULL;

/** Name of the map file we're checking. */
char filename[MAX_BUF];

static void map_error(map_tile *tile, const char *format, ...);

/**
 * Signal handler for SIGSEGV -- make core with abort.
 * @param i Unused. */
static void signal_sigsegv(int i)
{
	(void) i;

	abort();
}

/**
 * Parse archetypes file. */
static void parse_arches()
{
	char line[MAX_BUF], archname[MAX_BUF];
	FILE *fh;

	/* Attempt to open the archetypes file in read-only mode */
	if ((fh = fopen(ARCHETYPES_FILE, "r")) == NULL)
	{
		printf("ERROR: Failed to open specified file in read-only mode: '%s'\n", ARCHETYPES_FILE);
		exit(0);
	}

	/* Loop through the lines */
	while (fgets(line, MAX_BUF, fh))
	{
		/* Parse all the objects in the archetype file */
		if (strncmp(line, "Object ", 7) == 0 && sscanf(line, "Object %s\n", archname))
		{
			int type = -1, layer = -1, no_pass = 0;

			/* As we've got the object name now, loop through the lines until "end" */
			while (fgets(line, MAX_BUF, fh) && strcmp(line, "end\n"))
			{
				/* Type of this object */
				if (strncmp(line, "type ", 5) == 0)
				{
					if (!sscanf(line, "type %d\n", &type))
						return;
				}
				/* Layer of this object */
				else if (strncmp(line, "layer ", 6) == 0)
				{
					if (!sscanf(line, "layer %d\n", &layer))
						return;
				}
				/* no_pass object? */
				else if (strncmp(line, "no_pass ", 8) == 0)
				{
					if (!sscanf(line, "no_pass %d\n", &no_pass))
						return;
				}
			}

			/* If we found the type and layer */
			if (type != -1 && layer != -1)
			{
				arch *arch_tmp = (arch *) malloc(sizeof(arch));

				/* Store the arch name */
				snprintf(arch_tmp->archname, sizeof(arch_tmp->archname), "%s", archname);

				/* Arch type */
				arch_tmp->type = type;

				/* Arch layer */
				arch_tmp->layer = layer;

				/* no_pass object? */
				arch_tmp->no_pass = no_pass;

				/* Append the old list structure to it */
				arch_tmp->next = arch_list;

				/* Switch the old structure with this new one */
				arch_list = arch_tmp;
			}
		}
	}

	/* Close the file handle */
	fclose(fh);
}

/**
 * Recursively parse object from a map file.
 * @param archname Arch name of the object.
 * @param fh The map file handled.
 * @param return_value Whether to returnthe object or not.
 * @return If return_value is 1, the newly allocated object is returned,
 * NULL otherwise. */
static object *parse_arch_recursive(char *archname, FILE *fh, int return_value)
{
	int x = -1, y = -1, type = -1, no_pass = -1, layer = -1, found = 0;
	object *ob, *inv = NULL;
	map_tile *map_tile_tmp;
	arch *arch_tmp;
	char inv_archname[MAX_BUF], line[MAX_BUF];

	/* Loop through the lines after the "arch " until the "end" */
	while (fgets(line, MAX_BUF, fh))
	{
		if (!strcmp(line, "end\n"))
		{
			break;
		}

		if (sscanf(line, "arch %s\n", inv_archname) == 1)
		{
			inv = (object *) malloc(sizeof(object));
			inv = parse_arch_recursive(inv_archname, fh, 1);
		}

		sscanf(line, "x %d\n", &x);
		sscanf(line, "y %d\n", &y);
		sscanf(line, "type %d\n", &type);
		sscanf(line, "layer %d\n", &layer);
		sscanf(line, "no_pass %d\n", &no_pass);
	}

	ob = (object *) malloc(sizeof(object));

	/* Loop through the archetypes list */
	for (arch_tmp = arch_list; arch_tmp; arch_tmp = arch_tmp->next)
	{
		/* If arch name matches the one in map, get the default values */
		if (strcmp(arch_tmp->archname, archname) == 0)
		{
			/* Object type */
			if (type == -1)
			{
				type = arch_tmp->type;
			}

			/* Object layer */
			if (layer == -1)
			{
				layer = arch_tmp->layer;
			}

			/* no_pass 1 object? */
			if (no_pass == -1)
			{
				no_pass = arch_tmp->no_pass;
			}

			/* Break out */
			break;
		}
	}

	/* Object type */
	ob->type = type;

	/* Object layer */
	ob->layer = layer;

	/* no_pass 1 object? */
	ob->no_pass = no_pass == -1 ? 0 : no_pass;

	/* Store the arch name */
	snprintf(ob->archname, sizeof(ob->archname), "%s", archname);

	if (return_value)
	{
		if (x != -1)
		{
			map_error(NULL, "%s has X position set, but is in inventory of another object.", archname);
		}

		if (y != -1)
		{
			map_error(NULL, "%s has Y position set, but is in inventory of another object.", archname);
		}

		return ob;
	}

	ob->inv = inv;

	if (x == -1)
	{
		x = 0;
	}

	if (y == -1)
	{
		y = 0;
	}

	/* Loop through the map tile list */
	for (map_tile_tmp = map_tile_list; map_tile_tmp; map_tile_tmp = map_tile_tmp->next)
	{
		/* If both X and Y positions match */
		if (x == map_tile_tmp->x && y == map_tile_tmp->y)
		{
			/* So the code that makes new map tile doesn't get executed */
			found = 1;

			/* Append the old list structure to it */
			ob->next = map_tile_tmp->objects;

			/* Switch the old structure with this new one */
			map_tile_tmp->objects = ob;
		}
	}

	/* If no X and Y positions matched, this must be a new tile */
	if (!found)
	{
		map_tile_tmp = (map_tile *) malloc(sizeof(map_tile));

		/* Store the X, Y, and the first object */
		map_tile_tmp->x = x;
		map_tile_tmp->y = y;
		map_tile_tmp->objects = ob;

		/* Append the old list structure to it */
		map_tile_tmp->next = map_tile_list;

		/* Switch the old structure with this new one */
		map_tile_list = map_tile_tmp;
	}

	return NULL;
}

/**
 * Scan the map. */
static void scan_map()
{
	char line[MAX_BUF], archname[MAX_BUF];
	int got_map = 0, got_map_end = 0;
	FILE *fh;

	/* Open the map file in read-only mode */
	if ((fh = fopen(filename, "r")) == NULL)
	{
		printf("ERROR: Failed to open specified file in read-only mode: '%s'\n", filename);
		exit(0);
	}

	/* Loop through the lines */
	while (fgets(line, MAX_BUF, fh))
	{
		/* Check if this is map arch, which should be the first line. */
		if (!strcmp(line, "arch map\n"))
		{
			got_map = 1;
		}
		/* If we are inside the map, and we haven't hit the end yet until
		 * now, set it. */
		else if (got_map && !got_map_end && !strcmp(line, "end\n"))
		{
			got_map_end = 1;
			continue;
		}

		/* We haven't got "arch map" as the first line, so break out. */
		if (!got_map)
		{
			break;
		}

		/* We still haven't got a map end, so continue to the next
		 * line. */
		if (!got_map_end)
		{
			continue;
		}

		/* If this is a new arch, call the parsing function. */
		if (sscanf(line, "arch %s\n", archname) == 1)
		{
			parse_arch_recursive(archname, fh, 0);
		}
	}

	/* Close the file handle */
	fclose(fh);
}

/**
 * Print out a map error.
 * @param tile Tile where to get X/Y positions from, can be NULL.
 * @param format Format string.
 * @param ... Format arguments. */
static void map_error(map_tile *tile, const char *format, ...)
{
	char buf[MAX_BUF];
	va_list ap;

	va_start(ap, format);
	vsprintf(buf, format, ap);
	va_end(ap);

	if (tile)
	{
		printf("MAP ERROR: %s (x: %d, y: %d, map: %s)\n", buf, tile->x, tile->y, filename);
	}
	else
	{
		printf("MAP ERROR: %s (map: %s)\n", buf, filename);
	}
}

/**
 * Actually check the whole map for errors. */
static void check_map()
{
	object *ob;
	map_tile *map_tile_tmp;

	/* Loop through the tiles. */
	for (map_tile_tmp = map_tile_list; map_tile_tmp; map_tile_tmp = map_tile_tmp->next)
	{
		int objects_count = 0, layers[MAX_LAYERS] = {0, 0, 0, 0, 0, 0, 0}, i;

		/* Loop through the objects. */
		for (ob = map_tile_tmp->objects; ob; ob = ob->next)
		{
			/* Store number of objects on this layer. */
			layers[ob->layer - 1]++;

			if (ob->type == SPAWN_POINT && ob->inv == NULL)
			{
				map_error(map_tile_tmp, "Empty spawn point object.");
			}

			/* Increase overall number of objects. */
			objects_count++;
		}

		/* Layer 1 is reserved for floor and doesn't have to be on every
		 * single tile, but should be on tiles with other objects. */
		if (layers[0] < 1 && objects_count)
		{
			map_error(map_tile_tmp, "Missing layer 1 object on tile with some objects -- missing floor?");
		}

		/* Check all layers; more than one object with same layer on
		 * a single tile is usually a bug. */
		for (i = 0; i < MAX_LAYERS; i++)
		{
			if (layers[i] > 1)
			{
				map_error(map_tile_tmp, "More than 1 object (%d) with layer %d on same tile.", layers[i], i + 1);
			}
		}
	}
}

/**
 * Main function of the checker. */
int main(int argc, char *argv[])
{
	(void) argc;

	/* Handle SIGSEGV signal, so we can dump core. */
	signal(SIGSEGV, signal_sigsegv);

	/* If map path was not specified, show usage. */
	if (argv[1] == NULL)
	{
		printf("Usage: %s <map file to check>\n", argv[0]);
		exit(0);
	}

	/* Otherwise store the filename. */
	snprintf(filename, sizeof(filename), "%s", argv[1]);

	/* First, parse the archetypes file. */
	parse_arches();

	/* Then scan the map for tiles and objects */
	scan_map();

	/* And check for any errors with the objects */
	check_map();

	return 0;
}
