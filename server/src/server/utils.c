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
