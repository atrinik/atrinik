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
 * Account system.
 *
 * @author Alex Tokar */

#include <global.h>

typedef struct account_struct
{
	char *password;

	char *last_host;

	time_t last_time;

	struct
	{
		archetype *at;

		char *name;

		uint8 level;
	} *characters;

	size_t characters_num;
} account_struct;

void account_init(void)
{
}

void account_deinit(void)
{
}

static void account_free(account_struct *account)
{
	size_t i;

	free(account->password);
	free(account->last_host);

	for (i = 0; i < account->characters_num; i++)
	{
		free(account->characters[i].name);
	}
}

static int account_save(account_struct *account, const char *path)
{
	FILE *fp;

	fp = fopen(path, "w");

	if (!fp)
	{
		logger_print(LOG(BUG), "Could not open %s for writing.", path);
		return 0;
	}

	return 1;
}

static int account_load(account_struct *account, const char *path)
{
	FILE *fp;
	char buf[MAX_BUF], *end;

	fp = fopen(path, "rb");

	if (!fp)
	{
		logger_print(LOG(BUG), "Could not open %s for reading.", path);
		return 0;
	}

	while (fgets(buf, sizeof(buf), fp))
	{
		end = strchr(buf, '\n');

		if (end)
		{
			*end = '\0';
		}

		if (strncmp(buf, "pswd ", 5) == 0)
		{
			account->password = strdup(buf + 5);
		}
		else if (strncmp(buf, "host ", 5) == 0)
		{
			account->last_host = strdup(buf + 5);
		}
		else if (strncmp(buf, "time ", 5) == 0)
		{
			account->last_time = atoll(buf);
		}
		else if (strncmp(buf, "char ", 5) == 0)
		{
			size_t pos, word_num;
			char word[MAX_BUF];

			account->characters = realloc(account->characters, sizeof(*account->characters) * (account->characters_num + 1));
			pos = 5;
			word_num = 0;

			while (string_get_word(buf, &pos, ':', word, sizeof(word)))
			{
				switch (word_num)
				{
					case 0:
						account->characters[account->characters_num].at = find_archetype(word);
						break;

					case 1:
						account->characters[account->characters_num].name = strdup(word);
						break;

					case 2:
						account->characters[account->characters_num].level = atoi(word);
						break;
				}

				word_num++;
			}

			account->characters_num++;
		}
	}

	fclose(fp);

	return 1;
}

static void account_send_characters(socket_struct *ns, account_struct *account)
{
}

char *account_make_path(const char *name)
{
	StringBuffer *sb;
	size_t i;
	char *cp;

	sb = stringbuffer_new();
	stringbuffer_append_printf(sb, "%s/accounts/", settings.datapath);

	for (i = 0; i < settings.limits[ALLOWED_CHARS_ACCOUNT][0]; i++)
	{
		stringbuffer_append_string_len(sb, name, i + 1);
		stringbuffer_append_string(sb, "/");
	}

	stringbuffer_append_printf(sb, "%s.dat", name);
	cp = stringbuffer_finish(sb);

	return cp;
}

void account_login(socket_struct *ns, char *name, char *password)
{
	account_struct account;
	char *path;

	if (ns->account)
	{
		ns->status = Ns_Dead;
		return;
	}

	if (*name == '\0' || *password == '\0' || string_contains_other(name, settings.allowed_chars[ALLOWED_CHARS_ACCOUNT]) || string_contains_other(password, settings.allowed_chars[ALLOWED_CHARS_PASSWORD]))
	{
		draw_info_send(0, COLOR_RED, ns, "Invalid name and/or password.");
		return;
	}

	string_tolower(name);
	path = account_make_path(name);

	if (path_exists(path))
	{
		draw_info_send(0, COLOR_RED, ns, "No such account.");
		return;
	}

	account_load(&account, path);

	if (strcmp(string_crypt(password, account.password), account.password) != 0)
	{
		draw_info_send(0, COLOR_RED, ns, "Invalid password.");
		account_free(&account);
		return;
	}

	account.last_host = ns->host;
	account.last_time = datetime_getutc();
	account_save(&account, path);
	account_send_characters(ns, &account);
	account_free(&account);

	ns->account = strdup(name);
}

void account_register(socket_struct *ns, char *name, char *password, char *password2)
{
	size_t name_len, password_len;
	char *path;
	account_struct account;

	if (ns->account)
	{
		ns->status = Ns_Dead;
		return;
	}

	if (*name == '\0' || *password == '\0' || *password2 == '\0' || string_contains_other(name, settings.allowed_chars[ALLOWED_CHARS_ACCOUNT]) || string_contains_other(password, settings.allowed_chars[ALLOWED_CHARS_PASSWORD]) || string_contains_other(password2, settings.allowed_chars[ALLOWED_CHARS_PASSWORD]))
	{
		draw_info_send(0, COLOR_RED, ns, "Invalid name and/or password.");
		return;
	}

	name_len = strlen(name);
	password_len = strlen(name);

	/* Ensure the name/password lengths are within the allowed range.
	 * No need to compare 'password2' length, as it needs to be the same
	 * as 'password' anyway. */
	if (name_len < settings.limits[ALLOWED_CHARS_ACCOUNT][0] || name_len > settings.limits[ALLOWED_CHARS_ACCOUNT][1] || password_len < settings.limits[ALLOWED_CHARS_PASSWORD][0] || password_len > settings.limits[ALLOWED_CHARS_PASSWORD][1])
	{
		draw_info_send(0, COLOR_RED, ns, "Invalid length for name and/or password.");
		return;
	}

	if (strcmp(password, password2) != 0)
	{
		draw_info_send(0, COLOR_RED, ns, "The passwords did not match.");
		return;
	}

	string_tolower(name);
	path = account_make_path(name);

	if (path_exists(path))
	{
		draw_info_send(0, COLOR_RED, ns, "That account name is already registered.");
		return;
	}

	account.password = string_crypt(password, NULL);
	account.last_host = ns->host;
	account.last_time = datetime_getutc();
	account.characters = NULL;
	account.characters_num = 0;

	if (!account_save(&account, path))
	{
		draw_info_send(0, COLOR_RED, ns, "Save error occurred, please contact server administrator.");
		return;
	}

	ns->account = strdup(name);
}

void account_new_char(socket_struct *ns, char *name, char *archname)
{
}

void account_password_change(socket_struct *ns, char *password, char *password2)
{
}
