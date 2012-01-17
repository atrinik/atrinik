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
 * Migrates the settings from an older installation.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Client versions we know about. The process how these are checked is
 * explained in upgrader_init(). */
static const char *const client_versions[] =
{
	"2.0", "2.5", "3.0"
};

/** ::client_versions entry we are currently migrating. */
static sint64 version_id_migrating = -1;

/**
 * Upgrade 2.0 settings to 2.5.
 *
 * This handles upgrading settings/keybindings from the old (2.0 and
 * earlier) format to the new (2.5 and later) format.
 * @param from The old settings directory.
 * @param to The new setting directory. */
static void upgrade_20_to_25(const char *from, const char *to)
{
	char src[MAX_BUF], buf[HUGE_BUF];
	FILE *fp;

	/* Try to upgrade keybindings, if they exist. */
	snprintf(src, sizeof(src), "%s/keys.dat", from);
	fp = fopen(src, "r");

	if (fp)
	{
		int keycode, repeat;
		char keyname[MAX_BUF], command[HUGE_BUF];

		keybind_load();

		/* Read the old keys.dat file. */
		while (fgets(buf, sizeof(buf) - 1, fp))
		{
			/* Try to parse the macro definition lines. */
			if (sscanf(buf, "%d %d \"%200[^\"]\" \"%2000[^\"]\"", &keycode, &repeat, keyname, command) == 4)
			{
				keybind_struct *keybind;

				/* Is it a command? */
				if (*command == '/')
				{
					keybind = keybind_find_by_command(command);

					/* Does not exist yet, add it. */
					if (!keybind)
					{
						keybind = keybind_add(keycode, 0, command);
					}
					else
					{
						keybind->key = keycode;
					}

					keybind->repeat = repeat;
				}
				else if (!strncmp(command, "?M_MCON", 7))
				{
					char mcon_buf[HUGE_BUF];

					snprintf(mcon_buf, sizeof(mcon_buf), "?MCON %s", command + 7);

					if (!keybind_find_by_command(mcon_buf))
					{
						keybind = keybind_add(keycode, 0, mcon_buf);
						keybind->repeat = repeat;
					}
				}
				else if (*command == '?')
				{
					const char *new_cmd = command;

					if (!strcmp(command, "?M_NORTH"))
					{
						new_cmd = "?MOVE_N";
					}
					else if (!strcmp(command, "?M_NORTHEAST"))
					{
						new_cmd = "?MOVE_NE";
					}
					else if (!strcmp(command, "?M_EAST"))
					{
						new_cmd = "?MOVE_E";
					}
					else if (!strcmp(command, "?M_SOUTHEAST"))
					{
						new_cmd = "?MOVE_SE";
					}
					else if (!strcmp(command, "?M_SOUTH"))
					{
						new_cmd = "?MOVE_S";
					}
					else if (!strcmp(command, "?M_SOUTHWEST"))
					{
						new_cmd = "?MOVE_SW";
					}
					else if (!strcmp(command, "?M_WEST"))
					{
						new_cmd = "?MOVE_W";
					}
					else if (!strcmp(command, "?M_NORTHWEST"))
					{
						new_cmd = "?MOVE_NW";
					}
					else if (!strcmp(command, "?M_STAY"))
					{
						new_cmd = "?MOVE_STAY";
					}
					else if (!strcmp(command, "?M_UP"))
					{
						new_cmd = "?UP";
					}
					else if (!strcmp(command, "?M_DOWN"))
					{
						new_cmd = "?DOWN";
					}
					else if (!strcmp(command, "?M_LEFT"))
					{
						new_cmd = "?LEFT";
					}
					else if (!strcmp(command, "?M_RIGHT"))
					{
						new_cmd = "?RIGHT";
					}
					else if (!strcmp(command, "?M_SPELL_LIST"))
					{
						new_cmd = "?SPELL_LIST";
					}
					else if (!strcmp(command, "?M_SKILL_LIST"))
					{
						new_cmd = "?SKILL_LIST";
					}
					else if (!strcmp(command, "?M_HELP"))
					{
						new_cmd = "?HELP";
					}
					else if (!strcmp(command, "?M_KEYBIND"))
					{
						new_cmd = "?PARTY_LIST";
					}
					else if (!strcmp(command, "?M_QLIST"))
					{
						new_cmd = "?QLIST";
					}
					else if (!strcmp(command, "?M_RANGE"))
					{
						new_cmd = "?RANGE";
					}
					else if (!strcmp(command, "?M_FIRE_READY"))
					{
						new_cmd = "?FIRE_READY";
					}
					else if (!strcmp(command, "?COMBAT_TOGGLE"))
					{
						new_cmd = "?COMBAT";
					}
					else if (!strcmp(command, "?M_TARGET_ENEMY"))
					{
						new_cmd = "?TARGET_ENEMY";
					}
					else if (!strcmp(command, "?M_TARGET_FRIEND"))
					{
						new_cmd = "?TARGET_FRIEND";
					}
					else
					{
						continue;
					}

					keybind = keybind_find_by_command(new_cmd);

					if (keybind)
					{
						keybind->key = keycode;
					}
				}
			}
		}

		keybind_deinit();
		fclose(fp);
	}

	/* Try to upgrade options. */
	snprintf(src, sizeof(src), "%s/options.dat", from);
	fp = fopen(src, "r");

	if (fp)
	{
		char option_name[MAX_BUF];
		int option_value;

		settings_init();

		/* Read the old options.dat file. */
		while (fgets(buf, sizeof(buf) - 1, fp))
		{
			/* Handle the x/y options. */
			if (!strncmp(buf, "%3x ", 4))
			{
				setting_set_int(OPT_CAT_CLIENT, OPT_RESOLUTION_X, atoi(buf + 4));
				continue;
			}
			else if (!strncmp(buf, "%3y ", 4))
			{
				setting_set_int(OPT_CAT_CLIENT, OPT_RESOLUTION_Y, atoi(buf + 4));
				continue;
			}

			/* Parse the option lines. */
			if (sscanf(buf, "%200[^:]: %d", option_name, &option_value) == 2)
			{
				int cat = -1, setting = -1;

				if (!strcmp(option_name, "Playerdoll"))
				{
					cat = OPT_CAT_GENERAL;
					setting = OPT_PLAYERDOLL;
				}
				else if (!strcmp(option_name, "Show yourself targeted"))
				{
					cat = OPT_CAT_GENERAL;
					setting = OPT_TARGET_SELF;
				}
				else if (!strcmp(option_name, "Collect mode"))
				{
					cat = OPT_CAT_GENERAL;
					setting = OPT_COLLECT_MODE;
				}
				else if (!strcmp(option_name, "Exp display"))
				{
					cat = OPT_CAT_GENERAL;
					setting = OPT_EXP_DISPLAY;
				}
				else if (!strcmp(option_name, "Chat Timestamps"))
				{
					cat = OPT_CAT_GENERAL;
					setting = OPT_CHAT_TIMESTAMPS;
				}
				else if (!strcmp(option_name, "Maximum chat lines"))
				{
					cat = OPT_CAT_GENERAL;
					setting = OPT_MAX_CHAT_LINES;
				}
				else if (!strcmp(option_name, "Fullscreen"))
				{
					cat = OPT_CAT_CLIENT;
					setting = OPT_FULLSCREEN;
				}
				else if (!strcmp(option_name, "Resolution"))
				{
					cat = OPT_CAT_CLIENT;
					setting = OPT_RESOLUTION;
				}
				else if (!strcmp(option_name, "Player Names"))
				{
					cat = OPT_CAT_MAP;
					setting = OPT_PLAYER_NAMES;
				}
				else if (!strcmp(option_name, "Playfield zoom"))
				{
					cat = OPT_CAT_MAP;
					setting = OPT_MAP_ZOOM;
				}
				else if (!strcmp(option_name, "Low health warning"))
				{
					cat = OPT_CAT_MAP;
					setting = OPT_HEALTH_WARNING;
				}
				else if (!strcmp(option_name, "Low food warning"))
				{
					cat = OPT_CAT_MAP;
					setting = OPT_FOOD_WARNING;
				}
				else if (!strcmp(option_name, "Sound volume"))
				{
					cat = OPT_CAT_SOUND;
					setting = OPT_VOLUME_SOUND;
				}
				else if (!strcmp(option_name, "Music volume"))
				{
					cat = OPT_CAT_SOUND;
					setting = OPT_VOLUME_MUSIC;
				}
				else if (!strcmp(option_name, "Show Framerate"))
				{
					cat = OPT_CAT_DEVEL;
					setting = OPT_SHOW_FPS;
				}
				else if (!strcmp(option_name, "Enable quickport"))
				{
					cat = OPT_CAT_DEVEL;
					setting = OPT_OPERATOR;
				}

				if (cat != -1 && setting != -1)
				{
					setting_set_int(cat, setting, option_value);
				}
			}
		}

		settings_deinit();
		fclose(fp);
	}

	/* interface.gui and scripts_autoload changed locations in 2.5, copy
	 * them to the correct new location. */
	copy_if_exists(from, to, "interface.gui", "settings/interface.gui");
	copy_if_exists(from, to, "scripts_autoload", "settings/scripts_autoload");
	/* Copy over settings directory - in 2.0 and before only used to have
	 * ignore lists. */
	copy_if_exists(from, to, "settings", "settings");
}

/**
 * Upgrade 2.5 settings to 3.0.
 * @param from The old settings directory.
 * @param to The new setting directory. */
static void upgrade_25_to_30(const char *from, const char *to)
{
	copy_if_exists(from, to, "settings", "settings");
}

/**
 * Called before anything else on start, to check if we need to migrate
 * settings. */
void upgrader_init(void)
{
	char tmp[HUGE_BUF], tmp2[HUGE_BUF], version[MAX_BUF];
	size_t i;

	version_id_migrating = -1;
	snprintf(tmp, sizeof(tmp), "%s/.atrinik", get_config_dir());

	/* The .atrinik directory doesn't exist yet, nothing to migrate. */
	if (access(tmp, R_OK) != 0)
	{
		return;
	}

	snprintf(tmp, sizeof(tmp), "%s/.atrinik/%s", get_config_dir(), package_get_version_partial(version, sizeof(version)));

	/* If the settings directory for the current version already exists,
	 * leave. */
	if (access(tmp, R_OK) == 0)
	{
		return;
	}

	/* Look through the client versions, but skip the last entry, which
	 * should be the current version.
	 *
	 * The logic is that the upgrader will attempt to go through each
	 * version, and migrate settings into the next version. For example,
	 * 2.0 -> 2.5, 2.5 -> 3.0, etc. */
	for (i = 0; i < arraysize(client_versions) - 1; i++)
	{
		/* Construct the paths to the version we're looking at in the
		 * array, and the version after that. */
		snprintf(tmp, sizeof(tmp), "%s/.atrinik/%s", get_config_dir(), client_versions[i]);
		snprintf(tmp2, sizeof(tmp2), "%s/.atrinik/%s", get_config_dir(), client_versions[i + 1]);

		/* Only migrate if the settings for the version we're looking at
		 * exist, and the next version directory does not exist. */
		if (access(tmp, R_OK) != 0 || access(tmp2, R_OK) == 0)
		{
			continue;
		}

		/* Create the new version directory. */
		mkdir(tmp2, 0755);

		version_id_migrating = i;

		/* Migrate 2.0 to 2.5. */
		if (!strcmp(client_versions[i], "2.0"))
		{
			upgrade_20_to_25(tmp, tmp2);
		}
		else if (!strcmp(client_versions[i], "2.5"))
		{
			upgrade_25_to_30(tmp, tmp2);
		}
	}

	version_id_migrating = -1;
}

/**
 * Get the version the upgrader is currently working on.
 * @param dst Where to store the version.
 * @param dstlen Size of dst.
 * @return 'dst' or NULL if the upgrader is not working on any version. */
char *upgrader_get_version_partial(char *dst, size_t dstlen)
{
	/* No version is being migrated. */
	if (version_id_migrating == -1)
	{
		return NULL;
	}

	strncpy(dst, client_versions[version_id_migrating], dstlen - 1);
	dst[dstlen - 1] = '\0';

	return dst;
}
