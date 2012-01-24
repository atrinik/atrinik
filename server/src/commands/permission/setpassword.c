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
 * Implements the /setpassword command.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc command_func */
void command_setpassword(object *op, const char *command, char *params)
{
	size_t pos;
	char playername[MAX_BUF], *password;
	player *pl;

	pos = 0;

	if (!string_get_word(params, &pos, playername, sizeof(playername)))
	{
		return;
	}

	password = params + pos;

	if (string_isempty(password))
	{
		return;
	}

	pl = find_player(playername);

	if (pl)
	{
		char filename[MAX_BUF], filename_out[MAX_BUF], buf[HUGE_BUF];
		FILE *fp, *fp_out;

		player_cleanup_name(playername);
		snprintf(filename, sizeof(filename), "%s/players/%s/%s.pl", settings.datapath, playername, playername);
		snprintf(filename_out, sizeof(filename_out), "%s.tmp", filename);

		fp = fopen(filename, "r");

		if (!fp)
		{
			draw_info_format(COLOR_WHITE, op, "Could not open %s.", filename);
			return;
		}

		fp_out = fopen(filename_out, "w");

		if (!fp_out)
		{
			draw_info_format(COLOR_WHITE, op, "Could not open %s.", filename_out);
			return;
		}

		while (fgets(buf, sizeof(buf) - 1, fp))
		{
			if (strncmp(buf, "password ", 9) == 0)
			{
				fprintf(fp_out, "password %s\n", crypt_string(password, NULL));
			}
			else
			{
				fputs(buf, fp_out);
			}
		}

		fclose(fp);
		fclose(fp_out);
		unlink(filename);
		rename(filename_out, filename);
	}
	else
	{
		strcpy(pl->password, crypt_string(password, NULL));
	}

	draw_info_format(COLOR_WHITE, op, "Changed password of %s.", playername);
}
