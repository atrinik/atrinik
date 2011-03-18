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
 *  */

#include <include.h>

/**
 * Frees the ::help_files structure. */
void free_help_files()
{
	help_files_struct *help_file_tmp, *help_file = help_files;

	while (help_file)
	{
		help_file_tmp = help_file->next;
		free(help_file);
		help_file = help_file_tmp;
	}

	help_files = NULL;
}

/**
 * Read help files from file.
 *
 * This will also initialize and fill the linked list of help
 * files. The {@link #show_help} function then only needs to
 * loop through that list. */
void read_help_files()
{
	FILE *fp = server_file_open(SERVER_FILE_HFILES);
	char buf[HUGE_BUF], helpname[MAX_BUF], title[MAX_BUF], message[HUGE_BUF * 12];
	int end_marker = 0, dm_only = 0, autocomplete = 1;

	if (!fp)
	{
		return;
	}

	if (help_files)
	{
		free_help_files();
	}

	helpname[0] = title[0] = message[0] = '\0';

	/* Loop through the lines */
	while (fgets(buf, sizeof(buf), fp))
	{
		char *end = strchr(buf, '\n');

		if (!strncmp(buf, "Name: ", 6))
		{
			*end = '\0';
			strncpy(helpname, buf + 6, sizeof(helpname) - 1);
		}
		else if (!strncmp(buf, "Title: ", 7))
		{
			*end = '\0';
			strncpy(title, buf + 7, sizeof(title) - 1);
		}
		else if (!strcmp(buf, "DM: 1\n"))
		{
			dm_only = 1;
		}
		else if (!strcmp(buf, "Autocomplete: 0\n"))
		{
			autocomplete = 0;
		}
		else if (!strcmp(buf, "==========\n"))
		{
			end_marker = 1;
		}
		else
		{
			strncat(message, buf, sizeof(message) - strlen(message) - 1);
		}

		if (end_marker)
		{
			help_files_struct *help_files_tmp = (help_files_struct *) malloc(sizeof(help_files_struct));

			help_files_tmp->next = help_files;
			help_files = help_files_tmp;

			strncpy(help_files_tmp->helpname, helpname, sizeof(help_files_tmp->helpname) - 1);
			strncpy(help_files_tmp->title, title, sizeof(help_files_tmp->title) - 1);
			strncpy(help_files_tmp->message, message, sizeof(help_files_tmp->message) - 1);
			help_files_tmp->dm_only = dm_only;
			help_files_tmp->autocomplete = autocomplete;

			helpname[0] = title[0] = message[0] = '\0';
			dm_only = 0;
			autocomplete = 1;

			end_marker = 0;
		}
	}

	fclose(fp);
}

/**
 * Show a help GUI.
 * Uses book GUI to show the help.
 * @param helpname Help to be shown. */
void show_help(char *helpname)
{
	int len;
	unsigned char *data;
	char message[HUGE_BUF * 16];
	help_files_struct *help_files_tmp;

	/* This will be the default message if we don't find the help we want */
	snprintf(message, sizeof(message), "<book=Help not found><t t=\"The specified help file could not be found.\">\n");

	/* Loop through the linked list of help files */
	for (help_files_tmp = help_files; help_files_tmp; help_files_tmp = help_files_tmp->next)
	{
		/* If title or message are empty or helpname doesn't match, just continue to the next item */
		if (help_files_tmp->title[0] == '\0' || help_files_tmp->message[0] == '\0' || strcmp(help_files_tmp->helpname, helpname))
		{
			continue;
		}

		/* Got what we wanted, replace it with the default message */
		snprintf(message, sizeof(message), "<book>%s</book><title>%s</title>\n%s", help_files_tmp->helpname, help_files_tmp->title, help_files_tmp->message);
		break;
	}

	data = (uint8 *) message;

	len = (int) strlen((char *) data);

	book_load((char *) data, len);
}
