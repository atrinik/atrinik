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
 * entrance will not work for that player.
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
 * @author Alex Tokar
 *
 * @todo Arena information signs. Place a sign on a map, give it event
 * options as sign|arena_map_path or something like that, and an APPLY
 * event. Upon applying the sign, it will list that arena map's players
 * if any and if it is full or not.
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

#define PLUGIN_NAME "Arena"
#define PLUGIN_VERSION "Arena plugin 0.1"

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

	arena_map_players *player_list;

	/** Maximum number of players for this arena */
	int max_players;

	/** Next arena map */
	struct arena_maps_struct *next;
} arena_maps_struct;

arena_maps_struct *arena_maps;

f_plugin PlugHooks[1024];

CFParm GCFP;

MODULEAPI CFParm *initPlugin(CFParm* PParm)
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

	i = EVENT_MAPLEAVE;
    GCFP.Value[0] = (void *)(&i);
    (PlugHooks[HOOK_REGISTEREVENT])(&GCFP);

	i = EVENT_GDEATH;
    GCFP.Value[0] = (void *)(&i);
    (PlugHooks[HOOK_REGISTEREVENT])(&GCFP);

	i = EVENT_LOGOUT;
    GCFP.Value[0] = (void *)(&i);
    (PlugHooks[HOOK_REGISTEREVENT])(&GCFP);

    return NULL;
}

static int check_arena_player(const char *player, arena_map_players *player_list)
{
	arena_map_players *player_list_tmp;

	for (player_list_tmp = player_list; player_list_tmp; player_list_tmp = player_list_tmp->next)
	{
		if (strcmp(player, player_list_tmp->name) == 0 && strlen(player) == strlen(player_list_tmp->name))
		{
			return 1;
		}
	}

	return 0;
}

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

MODULEAPI int arena_enter(CFParm* PParm)
{
	int max_players = atoi((char *) (PParm->Value[10])), val = NDI_UNIQUE, zero = 0;
	char *message = "Sorry, this arena seems to be full.", tmp_path[HUGE_BUF];
	object *who = (object *) (PParm->Value[1]), *exit = (object *) (PParm->Value[2]);
	arena_maps_struct *arena_maps_tmp;

	if (!exit->slaying)
		return 0;

	normalize_path(exit->map->path, EXIT_PATH(exit), tmp_path);

	for (arena_maps_tmp = arena_maps; arena_maps_tmp; arena_maps_tmp = arena_maps_tmp->next)
	{
		if (strcmp(arena_maps_tmp->path, tmp_path) == 0)
		{
			if (arena_maps_tmp->players == max_players)
			{
				GCFP.Value[0] = (void *)(&val);
				GCFP.Value[1] = (void *)(&zero);
				GCFP.Value[2] = (void *)(who);
				GCFP.Value[3] = (void *)(message);

				(PlugHooks[HOOK_NEWDRAWINFO])(&GCFP);

				return 1;
			}
			else
			{
				arena_map_players *player_list_tmp = (arena_map_players *) malloc(sizeof(arena_map_players));

				arena_maps_tmp->players++;

				player_list_tmp->next = arena_maps_tmp->player_list;
				arena_maps_tmp->player_list = player_list_tmp;

				snprintf(player_list_tmp->name, sizeof(player_list_tmp->name), "%s", who->name);

				return 0;
			}
		}
	}

	arena_maps_tmp = (arena_maps_struct *) malloc(sizeof(arena_maps_struct));

	arena_maps_tmp->next = arena_maps;
	arena_maps = arena_maps_tmp;
	snprintf(arena_maps_tmp->path, sizeof(arena_maps_tmp->path), "%s", tmp_path);
	arena_maps_tmp->max_players = max_players;
	arena_maps_tmp->players = 1;

	arena_maps_tmp->player_list = (arena_map_players *) malloc(sizeof(arena_map_players));
	snprintf(arena_maps_tmp->player_list->name, sizeof(arena_maps_tmp->player_list->name), "%s", who->name);
	arena_maps_tmp->player_list->next = NULL;

	return 0;
}

MODULEAPI int arena_leave(CFParm* PParm)
{
	object *who = (object *)(PParm->Value[1]);
	arena_maps_struct *arena_maps_tmp;

	/* Sanity checks */
	if (!who || !who->map || !who->map->path || !who->name)
	{
		return 0;
	}

	for (arena_maps_tmp = arena_maps; arena_maps_tmp; arena_maps_tmp = arena_maps_tmp->next)
	{
		if (strcmp(arena_maps_tmp->path, who->map->path) == 0 && check_arena_player(who->name, arena_maps_tmp->player_list))
		{
			arena_maps_tmp->players--;

			remove_arena_player(who->name, &arena_maps_tmp->player_list);

			if (arena_maps_tmp->players < 1)
				remove_arena_map(arena_maps_tmp->path);

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
			result = arena_enter(PParm);
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
