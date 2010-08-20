/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
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
 * @defgroup plugin_arena Arena plugin
 * This plugin is used to control arena map exits, teleporters, triggers,
 * etc.
 *
 * It stores a linked list of arena maps and number of players inside.
 * If player attempts to enter the arena and the limit is reached, the
 * entrance will not work for that player. For validation purposes,
 * it also stores a linked list of player objects on the arena map. When
 * decreasing the amount of players on the arena map, this list is
 * checked to see if the player is really on that arena map. If not, no
 * decreasing of the count is made.
 *
 * The plugin also supports party arenas, which is similar to limiting
 * maximum number of players allowed, but will limit the number of
 * different parties to enter.
 *
 * The plugin has an own configuration script files to determine maximum
 * number of players, parties, etc. Those files generally have ".arena"
 * extension, but any other will work as well. In the future this might
 * be limited to ".arena" files, however. The supported syntax can be
 * used:
 *
 * - <b>max_players \<int\></b>: Maximum number of players to allow.
 * - <b>max_parties \<int\></b>: Only usable if party arena is enabled.
 *   Instead of counting maximum players, it will count maximum number of
 *   different parties allowed in.
 * - <b>party \<bool\></b>: If set, the arena is a party arena
 * - <b>party_players \<bool\></b>: If set, the arena is a party
 *   players arena, which means, both number of players AND number of
 *   parties will be taken into consideration. If either hits the max, the
 *   arena will be considered full.
 * - <b>message_full \<string\></b>: Allows you to customize the
 *   message displayed when the arena is full.
 * - <b>message_party \<string\></b>: Allows you to customize the
 *   message displayed when you need to join party in order to enter the
 *   arena.
 *
 * To determine when to decrease number of players or parties in the
 * arena, it uses MAPLEAVE, LOGOUT and GDEATH global events.
 *
 * The arena map MUST have a map event object with plugin name "Arena" and
 * event set to "player leaves".
 *
 * It is also possible to make arena signs. These signs can be places
 * ANYWHERE and still work. They are simply created by placing any object
 * (preferably a sign) on a map, and attaching APPLY or TRIGGER event to
 * it. The event must have plugin name "Arena" and a script name "Arena"
 * or anything else, the script name doesn't matter.
 *
 * Give the sign event's options like this:
 * <pre>sign|/arena/arena</pre>
 * The above will show players currently in the /arena/arena
 * arena map. The "sign|" part is necessary for the sign to work.
 *@{*/

/**
 * @file
 * This file handles the @ref plugin_arena "Arena plugin" functions. */

#include <global.h>
#include <stdarg.h>

#undef MODULEAPI

#ifdef WIN32
#ifdef PYTHON_PLUGIN_EXPORTS
#define MODULEAPI __declspec(dllexport)
#else
#define MODULEAPI __declspec(dllimport)
#endif

#else
#define MODULEAPI
#endif

/** Plugin name */
#define PLUGIN_NAME "Arena"

/** Plugin version */
#define PLUGIN_VERSION "Arena plugin 1.0"

/** Players currently in an arena map */
typedef struct arena_map_players
{
	/** The player object. */
	object *op;

	/** Next player in this list */
	struct arena_map_players *next;
} arena_map_players;

/** Arena maps linked list */
typedef struct arena_maps_struct
{
	/** The arena map path */
	char path[HUGE_BUF];

	/** Current number of players in this arena */
	int players;

	/** Current number of different parties in this arena */
	int parties;

	/** Linked list of players in this arena */
	arena_map_players *player_list;

	/** Maximum number of players for this arena */
	int max_players;

	/** Maximum number of different parties for this arena */
	int max_parties;

	/** Option flags */
	uint32 flags;

	/** Message when the arena is full */
	char message_arena_full[MAX_BUF];

	/** Message when you need to join a party to enter the arena */
	char message_arena_party[MAX_BUF];

	/** Next arena map */
	struct arena_maps_struct *next;
} arena_maps_struct;

/**
 * @defgroup arena_map_flags Arena map flags
 * Flags used to determine various usages of the Arena plugin.
 *@{*/
/** No flags. */
#define ARENA_FLAG_NONE             0
/** The arena is a party arena. */
#define ARENA_FLAG_PARTY            1
/** The arena is a party players arena. */
#define ARENA_FLAG_PARTY_PLAYERS    2
/*@}*/

/** The arena maps. */
arena_maps_struct *arena_maps;

/** Hooks. */
struct plugin_hooklist *hooks;

#undef LOG
#define LOG hooks->LOG

MODULEAPI void initPlugin(struct plugin_hooklist *hooklist)
{
	hooks = hooklist;

	LOG(llevDebug, "Atrinik Arena Plugin loading...\n");
	LOG(llevDebug, "[Done]\n");
}

MODULEAPI void closePlugin()
{
	LOG(llevInfo, "Arena Plugin closing.\n");
}

MODULEAPI void *getPluginProperty(int *type, ...)
{
	va_list args;
	const char *propname;
	int size;
	char *buf;

	va_start(args, type);
	propname = va_arg(args, const char *);

	if (!strcmp(propname, "Identification"))
	{
		buf = va_arg(args, char *);
		size = va_arg(args, int);
		va_end(args);
		snprintf(buf, size, PLUGIN_NAME);
		return NULL;
	}
	else if (!strcmp(propname, "FullName"))
	{
		buf = va_arg(args, char *);
		size = va_arg(args, int);
		va_end(args);
		snprintf(buf, size, PLUGIN_VERSION);
		return NULL;
	}

	va_end(args);
	return NULL;
}

MODULEAPI void postinitPlugin()
{
	LOG(llevDebug, "Start postinitPlugin.\n");

	hooks->register_global_event(PLUGIN_NAME, GEVENT_LOGOUT);
}

/**
 * Check a player list to see if player is in it.
 * @param op The player object to check for.
 * @param player_list Player list to check.
 * @return 1 if the player is in the list, 0 otherwise. */
static int check_arena_player(object *op, arena_map_players *player_list)
{
	arena_map_players *player_list_tmp;

	/* Go through the list of players. */
	for (player_list_tmp = player_list; player_list_tmp; player_list_tmp = player_list_tmp->next)
	{
		if (player_list_tmp->op == op)
		{
			return 1;
		}
	}

	return 0;
}

/**
 * Remove player from arena map's list of players.
 * @param op The player object to find and remove.
 * @param player_list The player list from where to remove. */
static void remove_arena_player(object *op, arena_map_players **player_list)
{
	arena_map_players *currP, *prevP = NULL;

	for (currP = *player_list; currP; prevP = currP, currP = currP->next)
	{
		if (currP->op == op)
		{
			if (!prevP)
			{
				*player_list = currP->next;
			}
			else
			{
				prevP->next = currP->next;
			}

			free(currP);
			break;
		}
	}
}

/**
 * Parse a single line inside an .arena config script.
 * @param arena_map The arena map structure.
 * @param line The line to parse. */
static void arena_map_parse_line(arena_maps_struct *arena_map, const char *line)
{
	/* Maximum number of players */
	if (strncmp(line, "max_players ", 12) == 0)
	{
		arena_map->max_players = atoi(line + 12);
	}
	/* Maximum number of parties */
	else if (strncmp(line, "max_parties ", 12) == 0)
	{
		arena_map->max_parties = atoi(line + 12);
	}
	/* Whether to allow arena party mode */
	else if (strncmp(line, "party ", 6) == 0)
	{
		line += 6;

		if (!strcmp(line, "true") || *line == '1')
		{
			arena_map->flags |= ARENA_FLAG_PARTY;
		}
	}
	/* Or even party players? */
	else if (strncmp(line, "party_players ", 14) == 0)
	{
		line += 14;

		if (!strcmp(line, "true") || *line == '1')
		{
			arena_map->flags |= ARENA_FLAG_PARTY_PLAYERS;
		}
	}
	/* Message for when the arena is full */
	else if (strncmp(line, "message_full ", 13) == 0)
	{
		strncpy(arena_map->message_arena_full, line + 13, sizeof(arena_map->message_arena_full) - 1);
	}
	/* Message when you need to join a party to enter */
	else if (strncmp(line, "message_party ", 14) == 0)
	{
		strncpy(arena_map->message_arena_party, line + 13, sizeof(arena_map->message_arena_party) - 1);
	}
}

/**
 * Parse an .arena script for the arena map.
 * @param arena_script The script path
 * @param exit The exit object used to trigger the event
 * @param arena_map The arena map structure */
static void arena_map_parse_script(const char *arena_script, object *exit, arena_maps_struct *arena_map)
{
	FILE *fh;
	char buf[MAX_BUF], tmp_path[HUGE_BUF];
	char *arena_script_path;

	/* Normalize the path to the script, allowing relative paths */
	hooks->normalize_path(exit->map->path, arena_script, tmp_path);

	/* Create path name to the script in maps directory */
	arena_script_path = hooks->create_pathname(tmp_path);

	/* Initialize defaults */
	arena_map->max_players = 0;
	arena_map->max_parties = 0;
	arena_map->players = 0;
	arena_map->parties = 0;
	arena_map->flags = ARENA_FLAG_NONE;
	strncpy(arena_map->message_arena_full, "Sorry, this arena seems to be full.", sizeof(arena_map->message_arena_full) - 1);
	strncpy(arena_map->message_arena_party, "You must be in a party in order to enter this arena.", sizeof(arena_map->message_arena_party) - 1);

	fh = fopen(arena_script_path, "r");

	if (!fh)
	{
		LOG(llevBug, "BUG: Plugin Arena: Could not open arena script: %s\n", arena_script_path);
		return;
	}

	while (fgets(buf, sizeof(buf), fh))
	{
		/* Ignore comments and empty lines */
		if (*buf == '#' || *buf == '\n')
		{
			continue;
		}

		/* Remove newline and parse the line */
		buf[strlen(buf) - 1] = '\0';
		arena_map_parse_line(arena_map, buf);
	}

	fclose(fh);
}

/**
 * Check if an arena map is full or not.
 *
 * Does checking for party arena, party player arenas, etc.
 * @param arena_map The arena map structure.
 * @return 1 if the arena is full, 0 otherwise. */
static int arena_full(arena_maps_struct *arena_map)
{
	/* Simple case: The map has nothing to do with parties. */
	if (!(arena_map->flags & ARENA_FLAG_PARTY) && !(arena_map->flags & ARENA_FLAG_PARTY_PLAYERS) && arena_map->players == arena_map->max_players)
	{
		return 1;
	}
	/* Otherwise a party map. */
	else if (arena_map->flags & ARENA_FLAG_PARTY)
	{
		/* If this is party players arena, count in players. */
		if (arena_map->flags & ARENA_FLAG_PARTY_PLAYERS && arena_map->players == arena_map->max_players)
		{
			return 1;
		}

		/* Always check for maximum parties, even if this is party players arena. */
		if (arena_map->parties == arena_map->max_parties)
		{
			return 1;
		}
	}

	return 0;
}

/**
 * Enter an arena entrance.
 * @param who The object entering this arena entrance.
 * @param exit The entrance object.
 * @param arena_script Configuration script for this arena.
 * @return 0 to operate the entrance (teleport the player), 1 otherwise. */
int arena_enter(object *who, object *exit, const char *arena_script)
{
	char tmp_path[HUGE_BUF];
	arena_maps_struct *arena_maps_tmp;

	/* The exit must have a path */
	if (!exit->slaying)
	{
		return 0;
	}

	/* Normalize the map's path */
	hooks->normalize_path(exit->map->path, EXIT_PATH(exit), tmp_path);

	/* Go through the list of arenas */
	for (arena_maps_tmp = arena_maps; arena_maps_tmp; arena_maps_tmp = arena_maps_tmp->next)
	{
		/* If the exit's path matches this arena */
		if (!strcmp(arena_maps_tmp->path, tmp_path))
		{
			/* If the arena is full, show a message to the player */
			if (arena_full(arena_maps_tmp))
			{
				hooks->new_draw_info(NDI_UNIQUE, who, arena_maps_tmp->message_arena_full);
				return 1;
			}
			/* Not full but it's party arena and the player is not in a party? */
			else if (arena_maps_tmp->flags & ARENA_FLAG_PARTY && !CONTR(who)->party)
			{
				hooks->new_draw_info(NDI_UNIQUE, who, arena_maps_tmp->message_arena_party);
				return 1;
			}
			/* Add the player to the list of players and increase the number of players/parties */
			else
			{
				arena_map_players *player_list_tmp = (arena_map_players *) malloc(sizeof(arena_map_players));

				/* For party arenas, also increase the parties count */
				if (arena_maps_tmp->flags & ARENA_FLAG_PARTY)
				{
					arena_map_players *player_list_party;
					int new_party = 1;

					/* Loop through the player list */
					for (player_list_party = arena_maps_tmp->player_list; player_list_party; player_list_party = player_list_party->next)
					{
						/* If we found a match for this party number, do not increase the count */
						if (CONTR(who)->party && CONTR(who)->party == CONTR(player_list_party->op)->party)
						{
							new_party = 0;
							break;
						}
					}

					/* Increase the count, if this is a new party in the arena */
					if (new_party)
					{
						arena_maps_tmp->parties++;
					}
				}

				/* Increase the number of players */
				arena_maps_tmp->players++;

				player_list_tmp->next = arena_maps_tmp->player_list;
				arena_maps_tmp->player_list = player_list_tmp;

				/* Store the player object */
				player_list_tmp->op = who;
				return 0;
			}
		}
	}

	/* If we are here, the arena doesn't have an entry in the linked list -- create it */
	arena_maps_tmp = (arena_maps_struct *) malloc(sizeof(arena_maps_struct));
	strncpy(arena_maps_tmp->path, tmp_path, sizeof(arena_maps_tmp->path) - 1);

	/* Parse script options */
	arena_map_parse_script(arena_script, exit, arena_maps_tmp);

	/* If this arena is full, show a message and return */
	if (arena_full(arena_maps_tmp))
	{
		hooks->new_draw_info(NDI_UNIQUE, who, arena_maps_tmp->message_arena_full);
		free(arena_maps_tmp);
		return 1;
	}
	/* Otherwise if not full and the player is not in party */
	else if (arena_maps_tmp->flags & ARENA_FLAG_PARTY && !CONTR(who)->party)
	{
		hooks->new_draw_info(NDI_UNIQUE, who, arena_maps_tmp->message_arena_party);
		free(arena_maps_tmp);
		return 1;
	}

	/* Add this player to the player count */
	arena_maps_tmp->players++;

	/* Add to the party count, if this is party arena */
	if (arena_maps_tmp->flags & ARENA_FLAG_PARTY)
	{
		arena_maps_tmp->parties++;
	}

	/* Make a list of players in this arena */
	arena_maps_tmp->player_list = (arena_map_players *) malloc(sizeof(arena_map_players));

	/* Store the player */
	arena_maps_tmp->player_list->op = who;

	arena_maps_tmp->player_list->next = NULL;

	arena_maps_tmp->next = arena_maps;
	arena_maps = arena_maps_tmp;

	return 0;
}

/**
 * Apply or trigger an arena sign.
 * @param who The object applying this sign.
 * @param path The map path of the arena.
 * @return Always returns 1, to never output sign message. */
int arena_sign(object *who, const char *path)
{
	arena_maps_struct *arena_maps_tmp;

	/* Sanity check */
	if (!path || path[0] == '\0')
	{
		return 1;
	}

	for (arena_maps_tmp = arena_maps; arena_maps_tmp; arena_maps_tmp = arena_maps_tmp->next)
	{
		/* If the path matches */
		if (!strcmp(arena_maps_tmp->path, path) && arena_maps_tmp->player_list)
		{
			arena_map_players *player_list_tmp;

			hooks->new_draw_info(NDI_UNIQUE | NDI_YELLOW, who, "This arena has the following players in:\n");

			/* Now go through the list of players in this arena */
			for (player_list_tmp = arena_maps_tmp->player_list; player_list_tmp; player_list_tmp = player_list_tmp->next)
			{
				hooks->new_draw_info_format(NDI_UNIQUE | NDI_YELLOW, who, "%s (level %d)", player_list_tmp->op->name, player_list_tmp->op->level);
			}

			if (!(arena_maps_tmp->flags & ARENA_FLAG_PARTY) || (arena_maps_tmp->flags & ARENA_FLAG_PARTY && arena_maps_tmp->flags & ARENA_FLAG_PARTY_PLAYERS))
			{
				hooks->new_draw_info_format(NDI_UNIQUE | NDI_YELLOW, who, "\nTotal players: %d\nMaximum players:  %d", arena_maps_tmp->players, arena_maps_tmp->max_players);
			}

			if (arena_maps_tmp->flags & ARENA_FLAG_PARTY)
			{
				hooks->new_draw_info_format(NDI_UNIQUE | NDI_YELLOW, who, "\nTotal parties: %d\nMaximum parties:  %d", arena_maps_tmp->parties, arena_maps_tmp->max_parties);
			}

			return 1;
		}
	}

	hooks->new_draw_info(NDI_UNIQUE | NDI_YELLOW, who, "This arena is currently empty.");
	return 1;
}

/**
 * Handles APPLY and TRIGGER events for the Arena.
 * @return 1 to stop normal execution of the object, 0 to continue. */
static int arena_event(object *who, object *exit, const char *event_options, const char *arena_script)
{
	/* If the first 5 characters are "sign|", this is an arena sign */
	if (event_options && !strncmp(event_options, "sign|", 5))
	{
		event_options += 5;
		return arena_sign(who, event_options);
	}
	/* Otherwise arena entrance */
	else
	{
		return arena_enter(who, exit, arena_script);
	}
}

/**
 * Leave arena map. Decreases number of players for the arena map, but
 * first validates that the player is in the list of that arena map's
 * players.
 * @return Always returns 0. */
static int arena_leave(object *who)
{
	arena_maps_struct *arena_maps_tmp;

	/* Sanity checks */
	if (!who || !who->map || !who->map->path || !who->name)
	{
		return 0;
	}

	/* Go through the list of arenas */
	for (arena_maps_tmp = arena_maps; arena_maps_tmp; arena_maps_tmp = arena_maps_tmp->next)
	{
		/* If it matches, and the player really is in the arena */
		if (!strcmp(arena_maps_tmp->path, who->map->path) && check_arena_player(who, arena_maps_tmp->player_list))
		{
			/* If this is party arena, we will want to see if we have to decrease the parties count */
			if (arena_maps_tmp->flags & ARENA_FLAG_PARTY)
			{
				arena_map_players *player_list_party;
				int remove_party = 1;

				/* Loop through the player list for this map */
				for (player_list_party = arena_maps_tmp->player_list; player_list_party; player_list_party = player_list_party->next)
				{
					/* If the party number matches, we're not going to remove this party */
					if (player_list_party->op != who && CONTR(who)->party && CONTR(who)->party == CONTR(player_list_party->op)->party)
					{
						remove_party = 0;
						break;
					}
				}

				/* Removing the party? Then decrease the count. */
				if (remove_party)
				{
					arena_maps_tmp->parties--;
				}
			}

			/* Decrease the count of players */
			arena_maps_tmp->players--;

			/* Remove the player from this the arena's player list */
			remove_arena_player(who, &arena_maps_tmp->player_list);
			return 0;
		}
	}

	return 0;
}

MODULEAPI void *triggerEvent(int *type, ...)
{
	object *activator, *who, *other, *event;
	va_list args;
	int eventcode, event_type;
	static int result = 0;

	va_start(args, type);
	event_type = va_arg(args, int);
	eventcode = va_arg(args, int);
	LOG(llevDebug, "Plugin Arena: triggerEvent(): eventcode %d\n", eventcode);

	activator = va_arg(args, object *);

	switch (eventcode)
	{
		case EVENT_APPLY:
		case EVENT_TRIGGER:
		{
			char *text, *script, *options;
			int parm1, parm2, parm3, parm4;

			who = va_arg(args, object *);
			other = va_arg(args, object *);
			event = va_arg(args, object *);
			text = va_arg(args, char *);
			parm1 = va_arg(args, int);
			parm2 = va_arg(args, int);
			parm3 = va_arg(args, int);
			parm4 = va_arg(args, int);
			script = va_arg(args, char *);
			options = va_arg(args, char *);

			result = arena_event(activator, who, options, script);
			break;
		}

		case GEVENT_PLAYER_DEATH:
		case MEVENT_LEAVE:
		case GEVENT_LOGOUT:
			result = arena_leave(activator);
			break;
	}

	va_end(args);
	return &result;
}

/*@}*/
