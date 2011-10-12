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
 * Handles help files.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Hashtable that contains the help files. */
static hfile_struct *hfiles = NULL;
/**
 * Array of command matches in console text input. */
static UT_array *command_matches = NULL;
/**
 * Index in ::command_matches to add to text input on the next tabulator
 * key press. */
static size_t command_index = 0;
/**
 * ::text_input_string cache. */
static char command_buf[MAX_INPUT_STRING];

/**
 * Frees the ::hfiles hashtable. */
void hfiles_deinit(void)
{
	hfile_struct *hfile, *tmp;

	HASH_ITER(hh, hfiles, hfile, tmp)
	{
		HASH_DEL(hfiles, hfile);
		free(hfile->key);
		free(hfile->msg);
		free(hfile);
	}

	if (command_matches)
	{
		utarray_free(command_matches);
		command_matches = NULL;
	}
}

/**
 * Read help files from file. */
void hfiles_init(void)
{
	FILE *fp;
	char buf[HUGE_BUF], message[HUGE_BUF * 12], *end;
	uint8 in_msg = 0;
	hfile_struct *hfile = NULL;

	fp = server_file_open(SERVER_FILE_HFILES);

	if (!fp)
	{
		return;
	}

	hfiles_deinit();

	while (fgets(buf, sizeof(buf) - 1, fp))
	{
		/* Ignore comments and blank lines. */
		if (*buf == '\0' || *buf == '#' || (*buf == '\n' && !in_msg))
		{
			continue;
		}

		end = strchr(buf, '\n');

		if (hfile)
		{
			if (!strcmp(buf, "endmsg\n"))
			{
				in_msg = 0;
				hfile->msg = strdup(message);
				hfile->msg_len = strlen(hfile->msg);
			}
			else if (in_msg)
			{
				strncat(message, buf, sizeof(message) - strlen(message) - 1);
			}
			else if (!strcmp(buf, "end\n"))
			{
				HASH_ADD_KEYPTR(hh, hfiles, hfile->key, strlen(hfile->key), hfile);
				hfile = NULL;
			}
			else if (!strcmp(buf, "msg\n"))
			{
				in_msg = 1;
			}
			else if (!strncmp(buf, "autocomplete ", 13))
			{
				hfile->autocomplete = atoi(buf + 13);
			}
			else if (!strncmp(buf, "autocomplete_wiz ", 17))
			{
				hfile->autocomplete_wiz = atoi(buf + 17);
			}
			else if (!strncmp(buf, "title ", 6))
			{
				*end = '\0';
				snprintf(message, sizeof(message), "<book>%s</book>", buf + 6);
			}
		}
		else if (!strncmp(buf, "help ", 5))
		{
			*end = '\0';
			hfile = calloc(1, sizeof(*hfile));
			hfile->key = strdup(buf + 5);
			message[0] = '\0';
		}
	}

	fclose(fp);

	command_buf[0] = '\0';
	utarray_new(command_matches, &ut_str_icd);
}

/**
 * Show a help GUI.
 * @param name Name of the help file entry to show. */
void help_show(const char *name)
{
	hfile_struct *hfile;

	HASH_FIND_STR(hfiles, name, hfile);
	book_add_help_history(name);

	if (!hfile)
	{
		char buf[HUGE_BUF];

		snprintf(buf, sizeof(buf), "<book=Help not found><title>\n<center>The specified help file could not be found.</center></title>");
		book_load(buf, strlen(buf));
	}
	else
	{
		book_load(hfile->msg, hfile->msg_len);
	}
}

/**
 * Comparison function used in help_handle_tabulator(). */
static int command_match_cmp(const void *a, const void *b)
{
	return strcmp(*(char * const *) a, *(char * const *) b);
}

/**
 * Handle tabulator key in console text input. */
void help_handle_tabulator(void)
{
	size_t len;
	char buf[MAX_INPUT_STRING], *space;

	/* Only handle the key if we have any help files, the text input
	 * starts with '/', and there is either no space in the text input
	 * or there is nothing after the space. */
	if (!command_matches || *text_input_string != '/' || ((space = strrchr(text_input_string, ' ')) && *(space + 1) != '\0'))
	{
		return;
	}

	/* Does not match the previous command buffer, so rebuild the array. */
	if (strcmp(command_buf, text_input_string))
	{
		hfile_struct *hfile, *tmp;

		utarray_clear(command_matches);

		HASH_ITER(hh, hfiles, hfile, tmp)
		{
			if ((hfile->autocomplete || (cpl.dm && hfile->autocomplete_wiz)) && !strncasecmp(hfile->key, text_input_string + 1, text_input_count - 1))
			{
				utarray_push_back(command_matches, &hfile->key);
			}
		}

		utarray_sort(command_matches, command_match_cmp);
		command_index = 0;

		/* Cannot overflow, same size as ::text_input_string. */
		strcpy(command_buf, text_input_string);
	}

	len = utarray_len(command_matches);

	if (!len)
	{
		return;
	}

	snprintf(buf, sizeof(buf), "/%s ", *((char **) utarray_eltptr(command_matches, command_index)));
	text_input_set_string(buf);
	strcpy(command_buf, buf);

	command_index++;

	if (command_index >= len)
	{
		command_index = 0;
	}
}
