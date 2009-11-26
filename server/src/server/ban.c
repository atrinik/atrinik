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
 * Banning related functions.
 *
 * It is possible to use the /ban DM command or interactive server mode
 * ban command to ban specified player or IP from the game.
 *
 * Syntax for banning:
 * Player:IP
 *
 * Where Player is the player name and IP is the IP address. It is
 * possible to use * for both IP and player, which means any match. */

#include <global.h>

/**
 * Check if this login or host is banned.
 * @param login Login name to check.
 * @param host Host name to check.
 * @return 1 if banned, 0 if not. */
int checkbanned(char *login, char *host)
{
	char filename[MAX_BUF], buf[MAX_BUF], log_buf[64], host_buf[64], *indexpos;
	FILE *fp;
	int Hits = 0, i;

	snprintf(filename, sizeof(filename), "%s/%s", settings.localdir, BANFILE);

	if (!(fp = fopen(filename, "r")))
	{
		return 0;
	}

	while (fgets(buf, sizeof(buf), fp))
	{
		/* Skip comments and blank lines. */
		if (buf[0] == '#' || buf[0] == '\n')
		{
			continue;
		}

		if ((indexpos = (char *) strrchr(buf, ':')) == 0)
		{
			LOG(llevDebug, "BUG: Bogus line in bans file: %s\n", buf);
			continue;
		}

		i = indexpos - buf;
		/* Copy login name into log_buf */
		strncpy(log_buf, buf, i);
		log_buf[i] = '\0';
		/* Copy host name into host_buf */
		strncpy(host_buf, indexpos + 1, 64);
		/* Cut off any extra spaces on the host buffer */
		indexpos = host_buf;

		while (!isspace(*indexpos))
		{
			indexpos++;
		}

		*indexpos = '\0';

		if (*log_buf == '*')
		{
			Hits = 1;
		}
		else if (!strcmp(login, log_buf))
		{
			Hits = 1;
		}

		if (Hits == 1)
		{
			/* Lock out any host */
			if (*host_buf == '*')
			{
				Hits++;
				/* break out now. otherwise Hits will get reset to one */
				break;
			}
			/* Lock out subdomains (eg, "*@usc.edu") */
			else if (strstr(host, host_buf) != NULL)
			{
				Hits++;
				/* break out now. otherwise Hits will get reset to one */
				break;
			}
			/* Lock out specific host */
			else if (!strcmp(host, host_buf))
			{
				Hits++;
				/* break out now. otherwise Hits will get reset to one */
				break;
			}
		}
	}

	fclose(fp);

	if (Hits >= 2)
	{
		return 1;
	}

	return 0;
}

/**
 * Add ban to the bans file. Will take care of getting the right values
 * from input string.
 * @param input The input string with both name and IP.
 * @return 1 on success, 0 on failure. */
int add_ban(const char *input)
{
	char filename[MAX_BUF], *host, *name, buf[MAX_BUF];
	FILE *fp;

	snprintf(filename, sizeof(filename), "%s/%s", settings.localdir, BANFILE);
	snprintf(buf, sizeof(buf), "%s", input);

	name = strtok(buf, ":");
	host = strtok(NULL, ":");

	if (!host || !name)
	{
		return 0;
	}

	if (!(fp = fopen(filename, "a")))
	{
		return 0;
	}

	fprintf(fp, "%s:%s\n", name, host);
	fclose(fp);

	return 1;
}

/**
 * Remove a ban from the bans file. Will take care of getting the right
 * values from input string.
 * @param input The input string with both name and IP.
 * @return 1 on success, 0 on failure. */
int remove_ban(const char *input)
{
	char filename[MAX_BUF], filename_tmp[MAX_BUF], *host, *name, buf[MAX_BUF], compare_buf[MAX_BUF];
	FILE *fp, *fp2;
	int ret = 0;

	snprintf(filename, sizeof(filename), "%s/%s", settings.localdir, BANFILE);
	snprintf(filename_tmp, sizeof(filename_tmp), "%s/%s.tmp", settings.localdir, BANFILE);
	snprintf(buf, sizeof(buf), "%s", input);

	name = strtok(buf, ":");
	host = strtok(NULL, ":");

	if (!host || !name)
	{
		return 0;
	}

	rename(filename, filename_tmp);

	if (!(fp = fopen(filename_tmp, "r")) || !(fp2 = fopen(filename, "w")))
	{
		return 0;
	}

	snprintf(compare_buf, sizeof(compare_buf), "%s:%s\n", name, host);

	while (fgets(buf, sizeof(buf), fp))
	{
		if (strcmp(buf, compare_buf) == 0)
		{
			ret = 1;
			continue;
		}

		fprintf(fp2, "%s", buf);
	}

	fclose(fp);
	fclose(fp2);

	unlink(filename_tmp);

	return ret;
}

/**
 * List all bans.
 * @param op Player object to print this information to, NULL to output
 * it to the log. */
void list_bans(object *op)
{
	char filename[MAX_BUF], buf[MAX_BUF];
	FILE *fp;

	snprintf(filename, sizeof(filename), "%s/%s", settings.localdir, BANFILE);

	if (!(fp = fopen(filename, "r")))
	{
		return;
	}

	if (op)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "List of bans:");
	}
	else
	{
		LOG(llevInfo, "\nList of bans:\n");
	}

	while (fgets(buf, sizeof(buf), fp))
	{
		/* Skip comments and blank lines. */
		if (buf[0] == '#' || buf[0] == '\n')
		{
			continue;
		}

		if (op)
		{
			buf[strlen(buf) - 1] = '\0';
			new_draw_info_format(NDI_UNIQUE, 0, op, buf);
		}
		else
		{
			LOG(llevInfo, buf);
		}
	}

	fclose(fp);
}
