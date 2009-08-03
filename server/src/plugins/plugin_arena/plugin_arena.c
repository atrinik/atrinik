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
 * @defgroup plugin_arena Arena plugin
 * This plugin is used to control arena map exits, teleporters, triggers,
 * etc.
 *
 * It stores a linked list of arena maps and number of players inside.
 * If player attempts to enter the arena and the limit is reached, the
 * entrance will not work for that player. For validation purposes,
 * it also stores a linked list of player names on the arena map. When
 * decreasing the amount of players on the arena map, this list is
 * checked to see if the player is really on that arena map. If not, no
 * decreasing of the count is made.
 *
 * Limit is controlled by event's options in the entrance. Event object
 * MUST be put into the entrance, like exit, teleporter, or even a rock
 * the player has to apply. The plugin event object must have name Arena
 * and have a script name like "Arena", otherwise Gridarta will complain
 * about it and remove it. The script name doesn't really do anything.
 * The event object also must have (obviously) event trigger. This plugin
 * currently supports APPLY and TRIGGER events.
 *
 * To determine when to decrease number of players on the arena, it uses
 * MAPLEAVE, LOGOUT and GDEATH global events.
 *
 * The arena map MUST have plugins 1 map attribute set for MAPLEAVE to
 * work.
 *
 * It is also possible to make arena signs. These signs can be places
 * ANYWHERE and still work. They are simply created by placing any object
 * (preferably a sign) on a map, and attaching APPLY or TRIGGER event to
 * it. The event must have plugin name "Arena" and a script name "Arena"
 * or anything else, the script name doesn't matter.
 *
 * Give the sign event's options like this:
 * <pre>sign|/stoneglow/arena/arena</pre>
 * The above will show players currently in the /stoneglow/arena/arena
 * arena map. The "sign|" part is necessary for the sign to work.
 *
 * @author Alex Tokar
 * @{ */

/**
 * @file
 * This file handles the Arena plugin functions.
 * @see plugin_arena */

#include <global.h>

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
#define PLUGIN_VERSION "Arena plugin 0.2"

/** Player names of players currently in an arena map */
typedef struct arena_map_players {
	/** The player name */
	char name[MAX_BUF];

	/** Next player in this list */
	struct arena_map_players *next;
} arena_map_players;

/** Arena maps linked list */
typedef struct arena_maps_struct {
	/** The arena map path */
	char path[HUGE_BUF];

	/** Current number of players in this arena */
	int players;

	/** Linked list of player names in this arena */
	arena_map_players *player_list;

	/** Maximum number of players for this arena */
	int max_players;

	/** Next arena map */
	struct arena_maps_struct *next;
} arena_maps_struct;

/** The arena maps */
arena_maps_struct *arena_maps;

f_plugin PlugHooks[1024];

CFParm GCFP;

MODULEAPI CFParm *initPlugin(CFParm *PParm)
{
	(void) PParm;

    LOG(llevDebug, "Atrinik Arena Plugin loading...\n");
    LOG(llevDebug, "[Done]\n");

	GCFP.Value[0] = (void *) PLUGIN_NAME;
    GCFP.Value[1] = (void *) PLUGIN_VERSION;

    return &GCFP;
}

MODULEAPI CFParm *removePlugin(CFParm* PParm)
{
	(void) PParm;

	return NULL;
}

MODULEAPI CFParm *getPluginProperty(CFParm* PParm)
{
	(void) PParm;

    return NULL;
}

MODULEAPI CFParm *registerHook(CFParm* PParm)
{
    int Pos;
    f_plugin Hook;

    Pos = *(int*)(PParm->Value[0]);
    Hook = (f_plugin)(PParm->Value[1]);
    PlugHooks[Pos] = Hook;
    return NULL;
}

MODULEAPI CFParm *postinitPlugin(CFParm* PParm)
{
	int i;

	(void) PParm;

    LOG(llevDebug, "Start postinitPlugin.\n");

    GCFP.Value[1] = (void *) PLUGIN_NAME;

	/* Register MAPLEAVE global event */
	i = EVENT_MAPLEAVE;
    GCFP.Value[0] = (void *)(&i);
    (PlugHooks[HOOK_REGISTEREVENT])(&GCFP);

	/* Register GDEATH global event */
	i = EVENT_GDEATH;
    GCFP.Value[0] = (void *)(&i);
    (PlugHooks[HOOK_REGISTEREVENT])(&GCFP);

	/* Register LOGOUT global event */
	i = EVENT_LOGOUT;
    GCFP.Value[0] = (void *)(&i);
    (PlugHooks[HOOK_REGISTEREVENT])(&GCFP);

    return NULL;
}

/**
 * Check a player list to see if player is in it.
 * @param player The player name to check
 * @param player_list Player list to check
 * @return 1 if the player is in the list, 0 otherwise */
static int check_arena_player(const char *player, arena_map_players *player_list)
{
	arena_map_players *player_list_tmp;

	/* Go through the list of players */
	for (player_list_tmp = player_list; player_list_tmp; player_list_tmp = player_list_tmp->next)
	{
		/* If it matches, for secure check lengths too */
		if (strcmp(player, player_list_tmp->name) == 0 && strlen(player) == strlen(player_list_tmp->name))
		{
			return 1;
		}
	}

	return 0;
}

/**
 * Remove player from arena map's list of players.
 * @param player The player name to find and remove
 * @param player_list The player list from where to remove */
static void remove_arena_player(const char *player, arena_map_players **player_list)
{
	arena_map_players *currP, *prevP;

	/* For 1st node, indicate there is no previous. */
	prevP = NULL;

	/* Visit each node, maintaining a pointer to
	 * the previous node we just visited. */
	for (currP = *player_list; currP != NULL; prevP = currP, currP = currP->next)
	{
		/* Found it. */
		if (strcmp(currP->name, player) == 0)
		{
			if (prevP == NULL)
			{
				/* Fix beginning pointer. */
				*player_list = currP->next;
			}
			else
			{
				/* Fix previous node's next to
				 * skip over the removed node. */
				prevP->next = currP->next;
			}

			/* Deallocate the node. */
			free(currP);

			/* Done searching. */
			break;
		}
	}
}

/**
 * Remove arena map, identified by path.
 * @param path The map path to remove from the linked list */
static void remove_arena_map(const char *path)
{
	arena_maps_struct *currP, *prevP;

	/* For 1st node, indicate there is no previous. */
	prevP = NULL;

	/* Visit each node, maintaining a pointer to
	 * the previous node we just visited. */
	for (currP = arena_maps; currP != NULL; prevP = currP, currP = currP->next)
	{
		/* Found it. */
		if (strcmp(currP->path, path) == 0)
		{
			if (prevP == NULL)
			{
				/* Fix beginning pointer. */
				arena_maps = currP->next;
			}
			else
			{
				/* Fix previous node's next to
				 * skip over the removed node. */
				prevP->next = currP->next;
			}

			/* Deallocate the node. */
			free(currP);

			/* Done searching. */
			break;
		}
	}
}

/**
 * Enter an arena entrance.
 * @param who The object entering this arena entrance
 * @param exit The entrance object
 * @param max_players Maximum players allowed in this arena
 * @return 0 to operate the entrance (teleport the player), 1 otherwise */
int arena_enter(object *who, object *exit, int max_players)
{
	int val = NDI_UNIQUE, zero = 0;
	char *message = "Sorry, this arena seems to be full.", tmp_path[HUGE_BUF];
	arena_maps_struct *arena_maps_tmp;

	/* The exit must have a path */
	if (!exit->slaying)
	{
		return 0;
	}

	/* Normalize the map's path */
	normalize_path(exit->map->path, EXIT_PATH(exit), tmp_path);

	/* Go through the list of arenas */
	for (arena_maps_tmp = arena_maps; arena_maps_tmp; arena_maps_tmp = arena_maps_tmp->next)
	{
		/* If the exit's path matches this arena */
		if (strcmp(arena_maps_tmp->path, tmp_path) == 0)
		{
			/* If limit was reached, show a message to the player */
			if (arena_maps_tmp->players == arena_maps_tmp->max_players)
			{
				GCFP.Value[0] = (void *)(&val);
				GCFP.Value[1] = (void *)(&zero);
				GCFP.Value[2] = (void *)(who);
				GCFP.Value[3] = (void *)(message);

				(PlugHooks[HOOK_NEWDRAWINFO])(&GCFP);

				return 1;
			}
			/* Otherwise add him to the list of players and increase the number of players */
			else
			{
				arena_map_players *player_list_tmp = (arena_map_players *) malloc(sizeof(arena_map_players));

				/* Increase the number of players */
				arena_maps_tmp->players++;

				player_list_tmp->next = arena_maps_tmp->player_list;
				arena_maps_tmp->player_list = player_list_tmp;

				/* Store the player's name */
				snprintf(player_list_tmp->name, sizeof(player_list_tmp->name), "%s", who->name);

				return 0;
			}
		}
	}

	/* If we are here, the arena doesn't have an entry in the linked list -- create it */
	arena_maps_tmp = (arena_maps_struct *) malloc(sizeof(arena_maps_struct));

	arena_maps_tmp->next = arena_maps;
	arena_maps = arena_maps_tmp;

	/* Store the map path */
	snprintf(arena_maps_tmp->path, sizeof(arena_maps_tmp->path), "%s", tmp_path);

	/* Store maximum of players */
	arena_maps_tmp->max_players = max_players;

	/* Count of players will be 1 now */
	arena_maps_tmp->players = 1;

	/* Make a list of player names in this arena */
	arena_maps_tmp->player_list = (arena_map_players *) malloc(sizeof(arena_map_players));

	/* Store the player's name */
	snprintf(arena_maps_tmp->player_list->name, sizeof(arena_maps_tmp->player_list->name), "%s", who->name);

	arena_maps_tmp->player_list->next = NULL;

	return 0;
}

/**
 * Apply or trigger an arena sign.
 * @param who The object applying this sign
 * @param path The map path of the arena
 * @return Always returns 1, to never output sign message. */
int arena_sign(object *who, const char *path)
{
	char sign_message[HUGE_BUF];
	int val = NDI_UNIQUE | NDI_YELLOW, zero = 0;
	arena_maps_struct *arena_maps_tmp;

	/* Sanity check */
	if (!path || path[0] == '\0')
	{
		return 1;
	}

	/* The default message, if the arena is empty */
	snprintf(sign_message, sizeof(sign_message), "This arena is currently empty.");

	/* Go through the list of arenas */
	for (arena_maps_tmp = arena_maps; arena_maps_tmp; arena_maps_tmp = arena_maps_tmp->next)
	{
		/* If the path matches */
		if (strcmp(arena_maps_tmp->path, path) == 0)
		{
			arena_map_players *player_list_tmp;
			char buf[MAX_BUF];

			snprintf(sign_message, sizeof(sign_message), "This arena has the following players in:");

			/* Now go through the list of players in this arena */
			for (player_list_tmp = arena_maps_tmp->player_list; player_list_tmp; player_list_tmp = player_list_tmp->next)
			{
				char name_buf[MAX_BUF];

				/* Store the name in a temporary buffer, and append it to the end of sign_message */
				snprintf(name_buf, sizeof(name_buf), "\n%s", player_list_tmp->name);
				strncat(sign_message, name_buf, sizeof(sign_message) - strlen(sign_message) - 1);
			}

			/* Store the counts in temporary buffer and append it to the end of sign_message */
			snprintf(buf, sizeof(buf), "\n\nTotal: %d\nFree:  %d", arena_maps_tmp->players, arena_maps_tmp->max_players - arena_maps_tmp->players);
			strncat(sign_message, buf, sizeof(sign_message) - strlen(sign_message) - 1);
		}
	}

	GCFP.Value[0] = (void *)(&val);
	GCFP.Value[1] = (void *)(&zero);
	GCFP.Value[2] = (void *)(who);
	GCFP.Value[3] = (void *)(sign_message);

	/* Draw the sign's message to the player */
	(PlugHooks[HOOK_NEWDRAWINFO])(&GCFP);

	return 1;
}

/**
 * Handles APPLY and TRIGGER events for the Arena.
 * @param PParm Plugin Parameters
 * @return Returns value of arena_sign() or arena_enter().
 * 1 means to stop execution of the object that triggered
 * this event (block arena entrance), 0 to continue (do
 * not block the arena entrance). */
MODULEAPI int arena_event(CFParm *PParm)
{
	object *who = (object *) (PParm->Value[1]), *exit = (object *) (PParm->Value[2]);
	char *event_options = (char *) (PParm->Value[10]);

	/* If the first 5 characters are "sign|", this is an arena sign */
	if (strncmp(event_options, "sign|", 5) == 0)
	{
		event_options += 5;

		return arena_sign(who, event_options);
	}
	/* Otherwise arena entrance */
	else
	{
		return arena_enter(who, exit, atoi(event_options));
	}
}

/**
 * Leave arena map. Decreases number of players for the
 * arena map, but first validates that the player is
 * in the list of that arena map's players. If the count
 * of players on this map reaches 0, remove the map from
 * the list of arena maps.
 * @param PParm Plugin parameters
 * @return Always returns 0. */
MODULEAPI int arena_leave(CFParm *PParm)
{
	object *who = (object *)(PParm->Value[1]);
	arena_maps_struct *arena_maps_tmp;

	/* Sanity checks */
	if (!who || !who->map || !who->map->path || !who->name)
	{
		return 0;
	}

	/* Go through the list of arenas */
	for (arena_maps_tmp = arena_maps; arena_maps_tmp; arena_maps_tmp = arena_maps_tmp->next)
	{
		/* If it matches, and the player name really is in the arena */
		if (strcmp(arena_maps_tmp->path, who->map->path) == 0 && check_arena_player(who->name, arena_maps_tmp->player_list))
		{
			/* Decrease the count of players */
			arena_maps_tmp->players--;

			/* Remove the player from this the arena's player list */
			remove_arena_player(who->name, &arena_maps_tmp->player_list);

			/* If the player count of this arena reaches 0, remove the arena entry */
			if (arena_maps_tmp->players < 1)
			{
				remove_arena_map(arena_maps_tmp->path);
			}

			return 0;
		}
	}

	return 0;
}

MODULEAPI CFParm *triggerEvent(CFParm* PParm)
{
    int eventcode;
    static int result = 0;

    eventcode = *(int *)(PParm->Value[0]);
    LOG(llevDebug, "Plugin Arena: triggerEvent(): eventcode %d\n", eventcode);

    switch (eventcode)
    {
        case EVENT_NONE:
            LOG(llevDebug, "Plugin Arena: Warning: EVENT_NONE requested\n");
            break;

        case EVENT_APPLY:
        case EVENT_TRIGGER:
			result = arena_event(PParm);
            break;

        case EVENT_GDEATH:
		case EVENT_MAPLEAVE:
		case EVENT_LOGOUT:
            result = arena_leave(PParm);
            break;
    }

    GCFP.Value[0] = (void *)(&result);
    return &GCFP;
}

/*@}*/
