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
 * General convenience functions for Atrinik. */

#include <global.h>

/**
 * Returns a random direction (1..8).
 * @return The random direction. */
int get_random_dir(void)
{
	return rndm(1, 8);
}

/**
 * Returns a random direction (1..8) similar to a given direction.
 * @param dir The exact direction.
 * @return The randomized direction. */
int get_randomized_dir(int dir)
{
	return absdir(dir + RANDOM() % 3 + RANDOM() % 3 - 2);
}

/**
 * We don't want to exceed the buffer size of buf1 by adding on buf2!
 * @param buf1
 * @param buf2
 * Buffers we plan on concatenating. Can be NULL.
 * @param bufsize Size of buf1. Can be 0.
 * @return 1 if overflow will occur, 0 otherwise. */
int buf_overflow(const char *buf1, const char *buf2, size_t bufsize)
{
	size_t len1 = 0, len2 = 0;

	if (buf1)
	{
		len1 = strlen(buf1);
	}

	if (buf2)
	{
		len2 = strlen(buf2);
	}

	if ((len1 + len2) >= bufsize)
	{
		return 1;
	}

	return 0;
}

/**
 * Copy a file.
 * @param filename Source file.
 * @param fpout Where to copy to. */
void copy_file(const char *filename, FILE *fpout)
{
	FILE *fp;
	char buf[HUGE_BUF];

	fp = fopen(filename, "r");

	if (!fp)
	{
		logger_print(LOG(BUG), "Failed to open '%s'.", filename);
		return;
	}

	while (fgets(buf, sizeof(buf), fp))
	{
		fputs(buf, fpout);
	}

	fclose(fp);
}
