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
 * Handles command aliases system.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * One command alias. */
typedef struct cmd_alias_struct
{
	/**
	 * Name of the command alias. */
	char *name;

	/**
	 * What to execute when there is an argument for the command. */
	char *arg;

	/**
	 * What to execute when there isn't an argument for the command, or
	 * if there is but cmd_alias_struct::arg is not set. */
	char *noarg;

	/**
	 * Hash handle. */
	UT_hash_handle hh;
} cmd_alias_struct;

/**
 * All the possible command aliases. */
static cmd_alias_struct *cmd_aliases = NULL;

/**
 * Load command aliases file.
 * @param path Where to load the file from. */
static void cmd_aliases_load(const char *path)
{
	FILE *fp;
	char buf[HUGE_BUF], *end;
	cmd_alias_struct *cmd_alias;

	fp = fopen_wrapper(path, "r");

	if (!fp)
	{
		return;
	}

	cmd_alias = NULL;

	while (fgets(buf, sizeof(buf) - 1, fp))
	{
		if (*buf == '#' || *buf == '\n')
		{
			continue;
		}

		end = strchr(buf, '\n');

		if (end)
		{
			*end = '\0';
		}

		if (string_startswith(buf, "[") && string_endswith(buf, "]"))
		{
			if (cmd_alias)
			{
				HASH_ADD_KEYPTR(hh, cmd_aliases, cmd_alias->name, strlen(cmd_alias->name), cmd_alias);
			}

			cmd_alias = calloc(1, sizeof(*cmd_alias));
			cmd_alias->name = string_sub(buf, 1, -1);
		}
		else if (string_startswith(buf, "arg = "))
		{
			cmd_alias->arg = string_sub(buf, 6, strlen(buf));
		}
		else if (string_startswith(buf, "noarg = "))
		{
			cmd_alias->noarg = string_sub(buf, 8, strlen(buf));
		}
	}

	if (cmd_alias)
	{
		HASH_ADD_KEYPTR(hh, cmd_aliases, cmd_alias->name, strlen(cmd_alias->name), cmd_alias);
	}

	fclose(fp);
}

/**
 * Initialize the command aliases system. */
void cmd_aliases_init(void)
{
	cmd_aliases_load("data/cmd_aliases.cfg");
	cmd_aliases_load("settings/cmd_aliases.cfg");
}

/**
 * Deinitialize the command aliases system. */
void cmd_aliases_deinit(void)
{
	cmd_alias_struct *curr, *tmp;

	HASH_ITER(hh, cmd_aliases, curr, tmp)
	{
		HASH_DEL(cmd_aliases, curr);

		free(curr->name);

		if (curr->arg)
		{
			free(curr->arg);
		}

		if (curr->noarg)
		{
			free(curr->noarg);
		}

		free(curr);
	}
}

/**
 * Execute the specified command alias.
 * @param cmd What to execute.
 * @param params Parameters passed by the player. NULL if none. */
static void cmd_aliases_execute(const char *cmd, const char *params)
{
	char word[MAX_BUF], *cp, *func_end;
	StringBuffer *sb;
	size_t pos;

	pos = 0;
	sb = stringbuffer_new();

	while (string_get_word(cmd, &pos, word, sizeof(word)))
	{
		if (stringbuffer_length(sb))
		{
			stringbuffer_append_string(sb, " ");
		}

		func_end = strchr(word, '>');

		if (string_startswith(word, "<") && func_end)
		{
			char *func, *cps[2];

			func = string_sub(word, 1, func_end - word);

			if (string_split(func, cps, arraysize(cps), ':') == 2)
			{
				if (strcmp(cps[0], "get") == 0)
				{
					if (strcmp(cps[1], "arg") == 0)
					{
						stringbuffer_append_string(sb, params ? params : "");
					}
					else if (strcmp(cps[1], "mplayer") == 0)
					{
						if (sound_map_background(-1) && sound_playing_music())
						{
							stringbuffer_append_string(sb, sound_get_bg_music_basename());
						}
						else
						{
							stringbuffer_append_string(sb, "nothing");
						}
					}
				}
				else if (strcmp(cps[0], "gender") == 0)
				{
					if (strcmp(cps[1], "possessive") == 0)
					{
						stringbuffer_append_string(sb, gender_possessive[cpl.gender]);
					}
					else if (strcmp(cps[1], "reflexive") == 0)
					{
						stringbuffer_append_string(sb, gender_reflexive[cpl.gender]);
					}
					else if (strcmp(cps[1], "subjective") == 0)
					{
						stringbuffer_append_string(sb, gender_subjective[cpl.gender]);
					}
				}
				else if (strcmp(cps[0], "choice") == 0)
				{
					UT_array *strs;
					char *s, **p;
					size_t idx;

					utarray_new(strs, &ut_str_icd);

					s = strtok(cps[1], ",");

					while (s)
					{
						utarray_push_back(strs, &s);
						s = strtok(NULL, ",");
					}

					idx = rndm(1, utarray_len(strs)) - 1;
					p = (char **) utarray_eltptr(strs, idx);

					if (p)
					{
						stringbuffer_append_string(sb, *p);
					}

					utarray_free(strs);
				}
				else if (strcmp(cps[0], "rndm") == 0)
				{
					int min, max;

					if (sscanf(cps[1], "%d-%d", &min, &max) == 2)
					{
						stringbuffer_append_printf(sb, "%d", rndm(min, max));
					}
				}
			}

			free(func);

			stringbuffer_append_string(sb, func_end + 1);
		}
		else
		{
			stringbuffer_append_string(sb, word);
		}
	}

	cp = stringbuffer_finish(sb);
	send_command(cp);
	free(cp);
}

/**
 * Try to handle player's command.
 * @param cmd Command to handle.
 * @return 1 if it was handled, 0 otherwise. */
int cmd_aliases_handle(const char *cmd)
{
	if (cmd[0] == '/' && cmd[1] != '\0')
	{
		char *cp;
		size_t cmd_len;
		const char *params;
		cmd_alias_struct *cmd_alias;

		cmd++;
		cp = strchr(cmd, ' ');

		if (cp)
		{
			cmd_len = cp - cmd;
			params = cp + 1;

			if (*params == '\0')
			{
				params = NULL;
			}
		}
		else
		{
			cmd_len = strlen(cmd);
			params = NULL;
		}

		HASH_FIND(hh, cmd_aliases, cmd, cmd_len, cmd_alias);

		if (cmd_alias)
		{
			if (params && cmd_alias->arg)
			{
				cmd_aliases_execute(cmd_alias->arg, params);
			}
			else if (cmd_alias->noarg)
			{
				cmd_aliases_execute(cmd_alias->noarg, params);
			}

			return 1;
		}
	}

	return 0;
}
