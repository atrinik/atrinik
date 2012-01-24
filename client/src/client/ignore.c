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
 * This file holds ignore list related code. */

#include <global.h>

/** The ignore list. */
static ignore_list_struct *ignore_list = NULL;

/**
 * Add an entry to the ignore list.
 * @param name Name to ignore.
 * @param type Type of the ignore. */
static void ignore_entry_add(const char *name, const char *type)
{
	ignore_list_struct *tmp = (ignore_list_struct *) malloc(sizeof(ignore_list_struct));

	strncpy(tmp->name, name, sizeof(tmp->name) - 1);
	tmp->name[sizeof(tmp->name) - 1] = '\0';
	strncpy(tmp->type, type, sizeof(tmp->type) - 1);
	tmp->type[sizeof(tmp->type) - 1] = '\0';

	tmp->next = ignore_list;
	ignore_list = tmp;
}

/**
 * Remove an entry from the ignore list.
 * @param name Name to remove.
 * @param type Type of the ignore.
 * @return 1 if the entry was removed, 0 otherwise. */
static int ignore_entry_remove(const char *name, const char *type)
{
	ignore_list_struct *tmp, *prev = NULL;

	for (tmp = ignore_list; tmp; prev = tmp, tmp = tmp->next)
	{
		if (!strcasecmp(name, tmp->name) && !strcasecmp(type, tmp->type))
		{
			if (prev)
			{
				prev->next = tmp->next;
			}
			else
			{
				ignore_list = tmp->next;
			}

			free(tmp);
			return 1;
		}
	}

	return 0;
}

/**
 * Clear the ignore list. */
void ignore_list_clear(void)
{
	ignore_list_struct *tmp, *tmp2;

	for (tmp = ignore_list; tmp; tmp = tmp2)
	{
		tmp2 = tmp->next;
		free(tmp);
	}

	ignore_list = NULL;
}

/**
 * Clear the ignore list and load it from a file. */
void ignore_list_load(void)
{
	char buf[MAX_BUF], name[MAX_BUF], type[MAX_BUF], filename[MAX_BUF];
	FILE *fp;

	snprintf(filename, sizeof(filename), "settings/%s.ignorelist", cpl.name);
	ignore_list_clear();

	if (!(fp = fopen_wrapper(filename, "r")))
	{
		return;
	}

	while (fgets(buf, sizeof(buf), fp))
	{
		if (sscanf(buf, "%s %s\n", name, type) == 2)
		{
			ignore_entry_add(name, type);
		}
	}

	fclose(fp);
}

/**
 * Save the ignore list to a file. */
static void ignore_list_save(void)
{
	ignore_list_struct *tmp;
	char filename[MAX_BUF];
	FILE *fp;

	snprintf(filename, sizeof(filename), "settings/%s.ignorelist", cpl.name);

	if (!(fp = fopen_wrapper(filename, "w")))
	{
		return;
	}

	for (tmp = ignore_list; tmp; tmp = tmp->next)
	{
		fprintf(fp, "%s %s\n", tmp->name, tmp->type);
	}

	fclose(fp);
}

/**
 * Check whether the specified player name is ignored.
 * @param name Name to check for.
 * @param type Type of the ignore to check for.
 * @return 1 if the player name is ignored, 0 otherwise. */
int ignore_check(const char *name, const char *type)
{
	ignore_list_struct *tmp;

	for (tmp = ignore_list; tmp; tmp = tmp->next)
	{
		if ((tmp->name[0] == '*' || !strcasecmp(name, tmp->name)) && (tmp->type[0] == '*' || !strcasecmp(type, tmp->type)))
		{
			return 1;
		}
	}

	return 0;
}

/**
 * Parse /ignore command.
 * @param cmd The command to parse, without the "/ignore" part. */
void ignore_command(const char *cmd)
{
	char name[64], type[64];

	if (cmd[0] == '\0')
	{
		ignore_list_struct *tmp;

		draw_info(COLOR_WHITE, "\nIGNORE LIST");
		draw_info(COLOR_WHITE, "--------------------------");

		for (tmp = ignore_list; tmp; tmp = tmp->next)
		{
			draw_info_format(COLOR_WHITE, "Name: %s Type: %s", tmp->name, tmp->type);
		}
	}
	else
	{
		cmd++;

		if (sscanf(cmd, "%s %s", name, type) != 2)
		{
			draw_info_format(COLOR_WHITE, "Syntax: /ignore <name> <type>");
		}
		else
		{
			if (ignore_entry_remove(name, type))
			{
				draw_info_format(COLOR_WHITE, "Removed %s (%s) from ignore list.", name, type);
			}
			else
			{
				ignore_entry_add(name, type);
				draw_info_format(COLOR_WHITE, "Added %s (%s) to ignore list.", name, type);
			}

			ignore_list_save();
		}
	}
}
