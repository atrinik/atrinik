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

/* Small utility to check a map file for any possible errors.
 *
 * Compile as:
 *  gcc map-checker.c -O3 -Wall -W -pedantic -Werror -o map-checker
 *
 * Use as:
 *  ./map-checker "your map"
 *
 * As of now, this map checker checks for the following things:
 *  - Two or more objects with no_pass 1 on same tile
 *  - Two or more objects with layer 1 (floor) on same tile
 *  - Tile with objects but no object with layer 1 (floor)
 *  - Two or more objects with layer 2 (floor mask) on same tile
 *
 * @todo Instead of checking for layer 1, check perhaps for type FLOOR? */

#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define MAX_BUF 4096

/* Path to archetypes file */
#define ARCHETYPES_FILE "../../arch/archetypes"

/* Object structure */
typedef struct object {
	/* Name of this object */
	char archname[MAX_BUF];

	/* Object type */
	int type;

	/* Object layer */
	int layer;

	/* Is this object blocking the tile? */
	int no_pass;

	/* Next object in this list */
	struct object *next;
} object;

/* Map tile structure */
typedef struct map_tile {
	/* X position of this tile */
	int x;

	/* Y position of this tile */
	int y;

	/* Count of layer 1 objects on this tile */
	int layer1_count;

	/* Count of layer 2 objects on this tile */
	int layer2_count;

	/* Count of layer 3 objects on this tile */
	int layer3_count;

	/* Count of layer 4 objects on this tile */
	int layer4_count;

	/* Count of layer 5 objects on this tile */
	int layer5_count;

	/* Count of layer 6 objects on this tile */
	int layer6_count;

	/* Count of layer 7 objects on this tile */
	int layer7_count;

	/* Count of blocked objects (no_pass 1) on this tile */
	int blocked_count;

	/* Linked list of objects on this tile */
	struct object *objects;

	/* Next map tile */
	struct map_tile *next;
} map_tile;

/* Arch structure */
typedef struct arch {
	/* Name of this arch */
	char archname[MAX_BUF];

	/* Arch type */
	int type;

	/* Arch layer */
	int layer;

	/* Is this object blocking the tile? */
	int no_pass;

	/* Next arch */
	struct arch *next;
} arch;

map_tile *map_tile_list = NULL;
arch *arch_list = NULL;

char filename[MAX_BUF];

/* Signal handler for SIGSEGV -- make core with abort. */
static void signal_sigsegv(int i)
{
	(void) i;

	abort();
}

/* Parse arche types file */
void parse_arches()
{
	char line[MAX_BUF], archname[MAX_BUF];
	FILE *fh;

	/* Attempt to open the archetypes file in read-only mode */
	if ((fh = fopen(ARCHETYPES_FILE, "r")) == NULL)
	{
		fprintf(stdout, "ERROR: Failed to open specified file in read-only mode: '%s'\n", ARCHETYPES_FILE);
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

/* Scan map */
void scan_map()
{
	char line[MAX_BUF], archname[MAX_BUF];
	FILE *fh;

	/* Open the map file in read-only mode */
	if ((fh = fopen(filename, "r")) == NULL)
	{
		fprintf(stdout, "ERROR: Failed to open specified file in read-only mode: '%s'\n", filename);
		exit(0);
	}

	/* Loop through the lines */
	while (fgets(line, MAX_BUF, fh))
	{
		/* If the line starts with "arch " */
		if (strncmp(line, "arch ", 5) == 0)
		{
			/* Must successfully store the arch name and it must not be map */
			if (sscanf(line, "arch %s\n", archname) && strcmp(archname, "map"))
			{
				int x = -1, y = -1, type = -1, no_pass = -1, layer = -1;

				/* Loop through the lines after the "arch " until the "end" */
				while (fgets(line, MAX_BUF, fh) && strcmp(line, "end\n"))
				{
					/* X position of the object */
					if (strncmp(line, "x ", 2) == 0)
					{
						if (!sscanf(line, "x %d\n", &x))
							return;
					}
					/* Y position of the object */
					else if (strncmp(line, "y ", 2) == 0)
					{
						if (!sscanf(line, "y %d\n", &y))
							return;
					}
					else if (strncmp(line, "type ", 5) == 0)
					{
						if (!sscanf(line, "type %d\n", &type))
							return;
					}
					else if (strncmp(line, "layer ", 6) == 0)
					{
						if (!sscanf(line, "layer %d\n", &layer))
							return;
					}
					else if (strncmp(line, "no_pass ", 8) == 0)
					{
						if (!sscanf(line, "no_pass %d\n", &no_pass))
							return;
					}
				}

				/* If both x and y are set */
				if (x != -1 && y != -1)
				{
					int found = 0;
					object *ob = (object *) malloc(sizeof(object));
					map_tile *map_tile_tmp;
					arch *arch_tmp;

					/* Loop through the archetypes list */
					for (arch_tmp = arch_list; arch_tmp; arch_tmp = arch_tmp->next)
					{
						/* If arch name matches the one in map, get the default values */
						if (strcmp(arch_tmp->archname, archname) == 0)
						{
							/* Object type */
							if (type == -1)
								type = arch_tmp->type;

							/* Object layer */
							if (layer == -1)
								layer = arch_tmp->layer;

							/* no_pass 1 object? */
							if (no_pass == -1)
								no_pass = arch_tmp->no_pass;

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

						/* Set these to 0 */
						map_tile_tmp->layer1_count = 0;
						map_tile_tmp->layer2_count = 0;
						map_tile_tmp->layer3_count = 0;
						map_tile_tmp->layer4_count = 0;
						map_tile_tmp->layer5_count = 0;
						map_tile_tmp->layer6_count = 0;
						map_tile_tmp->layer7_count = 0;
						map_tile_tmp->blocked_count = 0;

						/* Append the old list structure to it */
						map_tile_tmp->next = map_tile_list;

						/* Switch the old structure with this new one */
						map_tile_list = map_tile_tmp;
					}
				}
			}
		}
	}

	/* Close the file handle */
	fclose(fh);
}

/* Print out a map error, tile being the map tile of the error and message the error explanation */
void map_error(map_tile *tile, const char *message)
{
	fprintf(stdout, "MAP ERROR: %s (x: %d, y: %d, map: %s)\n", message, tile->x, tile->y, filename);
}

/* Actually check the whole map for errors */
void check_map()
{
	object *ob;
	map_tile *map_tile_tmp;

	/* Loop through the tiles */
	for (map_tile_tmp = map_tile_list; map_tile_tmp; map_tile_tmp = map_tile_tmp->next)
	{
		int objects_count = 0;

		/* Loop through the objects */
		for (ob = map_tile_tmp->objects; ob; ob = ob->next)
		{
			/* Increase number of layer1 objects */
			if (ob->layer == 1)
				map_tile_tmp->layer1_count++;

			/* Increase number of layer2 objects */
			if (ob->layer == 2)
				map_tile_tmp->layer2_count++;

			/* Increase number of layer3 objects */
			if (ob->layer == 3)
				map_tile_tmp->layer3_count++;

			/* Increase number of layer4 objects */
			if (ob->layer == 4)
				map_tile_tmp->layer4_count++;

			/* Increase number of layer5 objects */
			if (ob->layer == 5)
				map_tile_tmp->layer5_count++;

			/* Increase number of layer6 objects */
			if (ob->layer == 6)
				map_tile_tmp->layer6_count++;

			/* Increase number of layer7 objects */
			if (ob->layer == 7)
				map_tile_tmp->layer7_count++;

			/* Increase number of no_pass objects */
			if (ob->no_pass)
				map_tile_tmp->blocked_count++;

			/* Increase overall number of objects */
			objects_count++;
		}

		/* Multiple blocked objects shouldn't be necessary, and often are glitches */
		if (map_tile_tmp->blocked_count > 1)
			map_error(map_tile_tmp, "More than 1 object blocking same tile!");

		/* Layer 1 is reserved for floor, and should NOT have multiple instances of it on one tile */
		if (map_tile_tmp->layer1_count > 1)
			map_error(map_tile_tmp, "More than 1 object with layer 1 on same tile -- multiple floors?");

		/* Layer 1 is reserved for floor and doesn't have to be on every single tile, but should be on tiles with other objects. */
		if (map_tile_tmp->layer1_count < 1 && objects_count)
			map_error(map_tile_tmp, "Missing layer 1 object on tile with some objects -- missing floor?");

		/* Layer 2 is reserved for floor masks, and double floor masks on same tile will not show correctly for client. */
		if (map_tile_tmp->layer2_count > 1)
			map_error(map_tile_tmp, "More than 1 object with layer 2 on same tile -- multiple floor masks?");
	}
}

/* Main function of the checker */
int main(int argc, char *argv[])
{
	(void) argc;

	/* Handle SIGSEGV (Segmentation fault) event, so we can dump core */
	signal(SIGSEGV, signal_sigsegv);

	/* If map path was not specified, show usage */
	if (argv[1] == NULL)
	{
		fprintf(stdout, "Usage: %s <map file to check>\n", argv[0]);
		exit(0);
	}
	/* Otherwise store the filename */
	else
		snprintf(filename, sizeof(filename), "%s", argv[1]);

	/* First, parse the archetypes file. */
	parse_arches();

	/* Then scan the map for tiles and objects */
	scan_map();

	/* And check for any errors with the objects */
	check_map();

	return 0;
}

