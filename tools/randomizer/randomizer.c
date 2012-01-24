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

/* Small utility to randomize map arches, with a custom list of things to
 * randomize, like floor, flowers, trees, etc.
 *
 * Syntax for the "random" file is as follows:
 *  Name: Arch name to match, can be incomplete
 *  Randoms: Random variants to add to the arch name above, if match found.
 * Comments in the file are allowed and will be ignored by the parser.
 *
 * Compile as:
 *  gcc randomizer.c -O3 -Wall -W -pedantic -Werror -o randomizer
 *
 * Use as:
 *  ./randomizer "your map" > "map to write to"
 *
 * Please note that the randomizer NEVER saves the randomized content to
 * the original file. It will output the randomized map content to the
 * terminal screen, unless told otherwise, like by ">" to write the output
 * to a file. */

#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define MAX_BUF 4096

#define RANDOM() random()
#define SRANDOM(xyz) srandom(xyz)

/* Structure for the random variations of arches */
typedef struct random_variations_struct {
	/* The variation ("1", "2", 3", "small", "large", etc) */
	char random_var[MAX_BUF];

	/* Next variation in this linked list */
	struct random_variations_struct *next;
} random_variations;

/* Structure for the arches to randomize from file */
typedef struct random_struct
{
	/* Name of arch to randomize */
	char archname[MAX_BUF];

	/* Number of possible variations */
	int variations;

	/* Start of the random variations list */
	random_variations *randoms_start;

	/* Next arch to randomize in this linked list */
	struct random_struct *next;
} random_struct;

/* Initialize random list to NULL */
random_struct *random_list = NULL;

/* Signal handler for SIGSEGV -- make core with abort. */
static void signal_sigsegv(int i)
{
	(void) i;

	abort();
}

/* Random number function, with min and max */
int rndm(int min, int max)
{
  	int diff;

  	diff = max - min + 1;

  	if (max < 1 || diff < 1)
    	return min;

  	return (RANDOM() % diff + min);
}

/* Parse the "random" file */
static void parse_randoms()
{
	FILE *fh;
	char line[MAX_BUF], name[MAX_BUF], randoms[MAX_BUF], *p;
	random_struct *random_tmp;
	random_variations *random_variations_tmp;

	/* If failed to open the random file, just return. */
	if ((fh = fopen("random", "r")) == NULL)
		return;

	/* Loop through all the lines in the file */
	while (fgets(line, MAX_BUF - 1, fh))
	{
		/* Ignore comments */
		if (line[0] == '#')
			continue;

		/* Scan the line for Name:, and store it */
		if (sscanf(line, "Name: %s\n", name))
		{
			/* Loop through the next lines, and break out on Randoms: match. */
			while (fgets(line, MAX_BUF - 1, fh))
			{
				/* Ignore comments */
				if (line[0] == '#')
					continue;

				/* Scan the next line for Randoms: and store it */
				if (sscanf(line, "Randoms: %s\n", randoms))
				{
					/* Allocate a new random list structure */
					random_tmp = (random_struct *) malloc(sizeof(random_struct));

					/* Append the old list structure to it */
					random_tmp->next = random_list;

					/* Switch the old structure with this new one */
					random_list = random_tmp;

					/* Number of variations starts at 0 */
					random_tmp->variations = 0;

					/* Store the arch name to look for in the map file */
					snprintf(random_tmp->archname, sizeof(random_tmp->archname), "%s", name);

					/* Now loop through the random vriations by "," */
					p = strtok(randoms, ",");

					while (p)
					{
						/* One more variation... */
						random_tmp->variations++;

						/* Allocate a new list of random variations */
						random_variations_tmp = (random_variations *) malloc(sizeof(random_variations));

						/* Append the old list structure to it */
						random_variations_tmp->next = random_tmp->randoms_start;

						/* Switch the old structure with this new one */
						random_tmp->randoms_start = random_variations_tmp;

						/* Store the random variation */
						snprintf(random_variations_tmp->random_var, sizeof(random_variations_tmp->random_var), "%s", p);

						p = strtok(NULL, ",");
					}

					break;
				}
			}
		}
	}

	/* Close the file handle */
	fclose(fh);
}

/* Main function of the randomizer */
int main(int argc, char *argv[])
{
	random_struct *random_tmp;
	random_variations *random_variations_tmp;
	char archname[MAX_BUF], filename[MAX_BUF], line[MAX_BUF];
	FILE *fh;

	(void) argc;

	/* Handle SIGSEGV (Segmentation fault) event, so we can dump core */
	signal(SIGSEGV, signal_sigsegv);

	/* If map file was not specified, show usage */
	if (argv[1] == NULL)
	{
		fprintf(stdout, "Usage: %s <map file to randomize>\n", argv[0]);
		exit(0);
	}
	/* Otherwise store the map file */
	else
		snprintf(filename, sizeof(filename), "%s", argv[1]);

	/* Now open the map file in read mode */
	if ((fh = fopen(filename, "r")) == NULL)
	{
		fprintf(stdout, "ERROR: Failed to open specified file in read-only mode: '%s'\n", filename);
		exit(0);
	}

	/* Seed the random number */
	SRANDOM(time(NULL));

	/* Parse the "random" file */
	parse_randoms();

	/* Loop through all the lines of this map */
	while (fgets(line, MAX_BUF, fh))
	{
		/* Check if this line has arch name, if so, store it */
		if (sscanf(line, "arch %s\n", archname))
		{
			/* Loop through the linked list of arches to randomize */
			for (random_tmp = random_list; random_tmp; random_tmp = random_tmp->next)
			{
				/* The arch name on map must match with the one from the list, at least partially */
				if (strncmp(random_tmp->archname, archname, strlen(random_tmp->archname)) == 0)
				{
					int found = 0, i = 1, random_int = rndm(1, random_tmp->variations);
					char tmparchname[MAX_BUF];

					/* First loop through the random variations list. This is necessary so
					 * that we know if we should randomize this arch. For example, do NOT
					 * randomize grassd_m2 if grassd_ is on the list, and m2 is not on the
					 * random variations list. */
					for (random_variations_tmp = random_tmp->randoms_start; random_variations_tmp; random_variations_tmp = random_variations_tmp->next)
					{
						/* Store the arch name from the config file in a temporary buffer,
						 * with both the (partial) arch name and the random variation. */
						snprintf(tmparchname, sizeof(tmparchname), "%s%s", random_tmp->archname, random_variations_tmp->random_var);

						/* Compare it, for secure also compare the lengths */
						if (strcmp(archname, tmparchname) == 0 && strlen(archname) == strlen(tmparchname))
						{
							/* Found it, break out and move on the next loop */
							found = 1;

							break;
						}
					}

					/* Only if the above loop returned 1 */
					if (found)
					{
						/* Second loop through the random variations list. Here we just
						 * check if i is equal to the random number calculated above. */
						for (random_variations_tmp = random_tmp->randoms_start; random_variations_tmp; random_variations_tmp = random_variations_tmp->next)
						{
							/* If i (variation ID) is equal to the random number,
							 * overwrite the old arch name and break out. */
							if (i == random_int)
							{
								snprintf(archname, sizeof(archname), "%s%s", random_tmp->archname, random_variations_tmp->random_var);

								break;
							}

							i++;
						}
					}
				}
			}

			/* Print out the arch name, even if we did no randomization */
			fprintf(stdout, "arch %s\n", archname);
		}
		/* Normal line -- just print it out */
		else
			fprintf(stdout, "%s", line);
	}

	/* Close the file handle */
	fclose(fh);

	/* Exit cleanly */
	return 0;
}

