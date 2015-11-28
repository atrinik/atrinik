/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
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
 * Atrinik plugin support header file.
 *
 * @author Yann Chachkoff */

#ifndef PLUGIN_H
#define PLUGIN_H

#ifndef WIN32
#include <dlfcn.h>
#endif

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

/**
 * @defgroup PLUGIN_EVENT_xxx Plugin event types
 * The plugin event types.
 *@{*/
/**
 * Normal event: the event is attached directly to the object in
 * question. */
#define PLUGIN_EVENT_NORMAL 1
/** Map-wide event. */
#define PLUGIN_EVENT_MAP 2
/** Global event, requires no attaching of event. */
#define PLUGIN_EVENT_GLOBAL 3
/** Unit tests event. */
#define PLUGIN_EVENT_UNIT 4
/*@}*/

/**
 * @defgroup event_numbers Event number codes
 * Event ID codes.
 *@{*/
/** No event. */
#define EVENT_NONE 0
/** Object applied/unapplied. */
#define EVENT_APPLY 1
/** Monster attacked or scripted weapon was used. */
#define EVENT_ATTACK 2
/** Player or monster was killed. */
#define EVENT_DEATH 3
/** Object dropped on the floor. */
#define EVENT_DROP 4
/** Object picked up. */
#define EVENT_PICKUP 5
/** Someone speaks. */
#define EVENT_SAY 6
/** Thrown object stopped. */
#define EVENT_STOP 7
/** Triggered each time the object can react/move. */
#define EVENT_TIME 8
/** Object is thrown. */
#define EVENT_THROW 9
/** Button pushed, lever pulled, etc. */
#define EVENT_TRIGGER 10
/** Container closed. */
#define EVENT_CLOSE 11
/** Marks that we should process quests in this object. */
#define EVENT_QUEST 13
/** Ask script whether to show this object on map. */
#define EVENT_ASK_SHOW 14
/** AI related event. One of @ref EVENT_AI_xxx. */
#define EVENT_AI 15
/*@}*/

/**
 * @defgroup EVENT_AI_xxx AI events
 * AI related events.
 *@{*/
/** Random movement. */
#define EVENT_AI_RANDOM_MOVE 1
/** Guard stops someone with a bounty. */
#define EVENT_AI_GUARD_STOP 2
/*@}*/

/**
 * @defgroup MEVENT_xxx Map event numbers
 * Map-wide events.
 *@{*/
/** A player entered a map. */
#define MEVENT_ENTER 1
/** A player left a map. */
#define MEVENT_LEAVE 2
/** A map is resetting. */
#define MEVENT_RESET 3
/** A spell is being cast. */
#define MEVENT_SPELL_CAST 4
/** A skill is being used. */
#define MEVENT_SKILL_USED 5
/** Player is dropping an item. */
#define MEVENT_DROP 6
/** Player is trying to pick up an item. */
#define MEVENT_PICK 7
/** Player is trying to put an item to a container on map. */
#define MEVENT_PUT 8
/** An item is being applied. */
#define MEVENT_APPLY 9
/** Player has logged in. */
#define MEVENT_LOGIN 10
/** The /drop command was used. */
#define MEVENT_CMD_DROP 11
/** The /take command was used. */
#define MEVENT_CMD_TAKE 12
/** Item was examined. */
#define MEVENT_EXAMINE 13
/*@}*/

/**
 * @defgroup GEVENT_xxx Global event numbers
 * Global event IDs.
 *@{*/
/** A new character has been created. */
#define GEVENT_BORN 0
/** Player login. */
#define GEVENT_LOGIN 1
/** Player logout. */
#define GEVENT_LOGOUT 2
/** Player was killed. */
#define GEVENT_PLAYER_DEATH 3
/** Cache entry was removed. */
#define GEVENT_CACHE_REMOVED 4
/** A server tick has occurred. */
#define GEVENT_TICK 5
/** Number of global events. */
#define GEVENT_NUM 6
/*@}*/

/**
 * Get an event flag from event number code.
 * @see event_numbers */
#define EVENT_FLAG(x) (1U << (x - 1))

/**
 * Check to see if object has an event in its object::event_flags.
 * @param ob Object.
 * @param event Event to check. */
#define HAS_EVENT(ob, event) (ob->event_flags & EVENT_FLAG(event))

struct plugin_hooklist;

/** Event function. */
typedef void *(*f_plug_event)(int *type, ...);
/** Property function. */
typedef void (*f_plug_prop)(int *type, ...);
/** First function called in a plugin. */
typedef void (*f_plug_init)(struct plugin_hooklist *hooklist);
/** Function called after the plugin was initialized. */
typedef void (*f_plug_pinit)(void);

#ifndef WIN32
/** Library handle. */
#define LIBPTRTYPE void *
/** Load a shared library. */
#define plugins_dlopen(fname) dlopen(fname, RTLD_NOW | RTLD_GLOBAL)
/** Unload a shared library. */
#define plugins_dlclose(lib) dlclose(lib)
/** Get a function from a shared library. */
#define plugins_dlsym(lib, name, type) dlsym(lib, name)
/** Library error. */
#define plugins_dlerror() dlerror()
#else
#define LIBPTRTYPE HMODULE
#define plugins_dlopen(fname) LoadLibrary(fname)
#define plugins_dlclose(lib) FreeLibrary(lib)
#define plugins_dlsym(lib, name, type) (type) GetProcAddress(lib, name)
#endif

/** Check if the specified filename is a plugin file. */
#define FILENAME_IS_PLUGIN(_path) (strstr((_path), "plugin_") && !strcmp((_path) + strlen((_path)) - strlen(PLUGIN_SUFFIX), PLUGIN_SUFFIX))

/** One loaded plugin. */
typedef struct atrinik_plugin {
    /** Event handler function. */
    f_plug_event eventfunc;

    /** Plugin closePlugin function. */
    f_plug_pinit closefunc;

    /** Pointer to the plugin library. */
    LIBPTRTYPE libptr;

    /** Plugin identification string. */
    char id[MAX_BUF];

    /** Plugin's full name. */
    char fullname[MAX_BUF];

    /** Global events registered. */
    int8_t gevent[GEVENT_NUM];

    /** Next plugin in list. */
    struct atrinik_plugin *next;
} atrinik_plugin;

/**
 * @defgroup exportable_plugin_functions Exportable plugin functions
 * Exportable functions. Any plugin should define all these.
 *@{*/
/**
 * Called when the plugin initialization process starts.
 * @param hooklist Plugin hooklist to register. */
extern MODULEAPI void initPlugin(struct plugin_hooklist *hooklist);

/**
 * Called to ask various information about the plugin.
 * @param type Integer pointer for va_start().
 * @return Return value depends on the type of information requested.
 * Can be NULL. */
extern MODULEAPI void getPluginProperty(int *type, ...);

/**
 * Called whenever an event occurs.
 * @param type Integer pointer for va_start().
 * @return Integer containing the event's return value. */
extern MODULEAPI void *triggerEvent(int *type, ...);

/**
 * Called by the server when the plugin loading is completed. */
extern MODULEAPI void postinitPlugin(void);

/**
 * Called when the plugin is about to be unloaded. */
extern MODULEAPI void closePlugin(void);
/*@}*/

#endif
