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

#include <global.h>

#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <../random_maps/random_map.h>
#include <../random_maps/rproto.h>

#ifndef WIN32
#include <dirent.h>
#endif

/**
 * @defgroup event_numbers Event number codes
 * Event ID codes.
 *@{*/

/** No event. This exists only to reserve the "0". */
#define EVENT_NONE		0
/** Object applied-unapplied. */
#define EVENT_APPLY    1
/** Monster attacked or Scripted Weapon used. */
#define EVENT_ATTACK   2
/** Player or monster dead. */
#define EVENT_DEATH    3
/** Object dropped on the floor. */
#define EVENT_DROP     4
/** Object picked up. */
#define EVENT_PICKUP   5
/** Someone speaks. */
#define EVENT_SAY      6
/** Thrown object stopped. */
#define EVENT_STOP     7
/** Triggered each time the object can react/move. */
#define EVENT_TIME     8
/** Object is thrown. */
#define EVENT_THROW    9
/** Button pushed, lever pulled, etc. */
#define EVENT_TRIGGER  10
/** Container closed. */
#define EVENT_CLOSE	   11
/** Timer connected triggered it. */
#define EVENT_TIMER    12

/** A new character has been created. */
#define EVENT_BORN     13
/** Global time event. */
#define EVENT_CLOCK    14
/** Triggered when the server crashes. Not recursive */
#define EVENT_CRASH    15
/** Global Death event */
#define EVENT_GDEATH   16
/** Triggered when anything got killed by anyone. */
#define EVENT_GKILL    17
/** Player login. */
#define EVENT_LOGIN    18
/** Player logout. */
#define EVENT_LOGOUT   19
/** A player entered a map. */
#define EVENT_MAPENTER 20
/** A player left a map. */
#define EVENT_MAPLEAVE 21
/** A map is resetting. */
#define EVENT_MAPRESET 22
/** A player character has been removed. */
#define EVENT_REMOVE   23
/** A player shouts something. */
#define EVENT_SHOUT    24
/** A player tells something. */
#define EVENT_TELL     25
/*@}*/

/** Number of local events */
#define NR_LOCAL_EVENTS 13

/** Number of events */
#define NR_EVENTS       26

/**
 * Get an event flag from event number code.
 * @see event_numbers */
#define EVENT_FLAG(x) (1 << (x - 1))

/**
 * @defgroup EVENT_FLAG_xxx Event flags
 * Event flags. Uses EVENT_FLAG() to get the actual flags.
 *@{*/
#define EVENT_FLAG_NONE     0
#define EVENT_FLAG_APPLY    EVENT_FLAG(EVENT_APPLY)
#define EVENT_FLAG_ATTACK   EVENT_FLAG(EVENT_ATTACK)
#define EVENT_FLAG_DEATH    EVENT_FLAG(EVENT_DEATH)
#define EVENT_FLAG_DROP     EVENT_FLAG(EVENT_DROP)
#define EVENT_FLAG_PICKUP   EVENT_FLAG(EVENT_PICKUP)
#define EVENT_FLAG_SAY      EVENT_FLAG(EVENT_SAY)
#define EVENT_FLAG_STOP     EVENT_FLAG(EVENT_STOP)
#define EVENT_FLAG_TIME     EVENT_FLAG(EVENT_TIME)
#define EVENT_FLAG_THROW    EVENT_FLAG(EVENT_THROW)
#define EVENT_FLAG_TRIGGER  EVENT_FLAG(EVENT_TRIGGER)
#define EVENT_FLAG_CLOSE    EVENT_FLAG(EVENT_CLOSE)
#define EVENT_FLAG_TIMER    EVENT_FLAG(EVENT_TIMER)
/*@}*/

/**
 * CFParm is the data type used to pass informations between the server
 * and the plugins.
 *
 * Using CFParm allows a greater flexibility, at the cost of a "manual"
 * function parameters handling and the need of "wrapper" functions.
 *
 * Each CFParm can contain up to 15 different values, stored as (void *). */
typedef struct _CFParm
{
	/** Currently unused, but may prove useful later. */
	int Type[15];

	/** The values contained in the CFParm structure. */
	const void *Value[15];
} CFParm;

/** Generic plugin function prototype. All hook functions follow this. */
typedef CFParm* (*f_plugin) (CFParm* PParm);

#ifndef WIN32
#define LIBPTRTYPE void *
#else
#define LIBPTRTYPE HMODULE
#endif

/**
 * CFPlugin contains all pertinent informations about one plugin. The
 * server maintains a list of CFPlugins in memory.
 *
 * Note that the library pointer is a (void *) in general, but a HMODULE
 * under Win32, due to the specific DLL management. */
typedef struct _CFPlugin
{
	/** Event Handler function */
	f_plugin eventfunc;

	/** Plugin Initialization function. */
	f_plugin initfunc;

	/** Plugin Post-Init. function. */
	f_plugin pinitfunc;

	/** Plugin Closing function. */
	f_plugin endfunc;

	/** Plugin getProperty function */
	f_plugin propfunc;

	/** Pointer to the plugin library */
	LIBPTRTYPE libptr;

	/** Plugin identification string */
	char id[MAX_BUF];

	/** Plugin full name */
	char fullname[MAX_BUF];

	/** Global events registered */
	int gevent[NR_EVENTS];
} CFPlugin;

/**
 * @defgroup exportable_plugin_functions Exportable plugin functions
 * Exportable functions. Any plugin should define all these.
 *@{*/
/** Called when the plugin initialization process starts. */
extern MODULEAPI CFParm *initPlugin(CFParm *PParm);

/** Called before the plugin gets unloaded from memory. */
extern MODULEAPI CFParm *endPlugin(CFParm *PParm);

/** Currently unused. */
extern MODULEAPI CFParm *getPluginProperty(CFParm *PParm);

/** Called whenever an event occurs. */
extern MODULEAPI CFParm *triggerEvent(CFParm *PParm);
/*@}*/

extern CFPlugin PlugList[32];

extern int PlugNR;

/** The plugin hook list. */
struct plugin_hooklist
{
	char *(*query_name)(object *, object *);
	char *(*re_cmp)(char *, char *);
	object *(*present_in_ob)(unsigned char, object *);
	int (*players_on_map)(mapstruct *);
	char *(*create_pathname)(const char *);
	char *(*normalize_path)(const char *, const char *, char *);
	void (*LOG)(LogLevel, const char *, ...);
	void (*free_string_shared)(const char *);
	const char *(*add_string)(const char *);
	void (*remove_ob)(object *);
	void (*fix_player)(object *);
	object *(*insert_ob_in_ob)(object *, object *);
	void (*new_info_map)(int, mapstruct *, int, int, int, const char *);
	void (*new_info_map_except)(int , mapstruct *, int, int, int, object *, object *, const char *);
	void (*spring_trap)(object *, object *);
	int (*cast_spell)(object *, object *, int, int, int, SpellTypeFrom, char *);
	void (*update_ob_speed)(object *);
	int (*command_rskill)(object *, char *);
	void (*become_follower)(object *, object *);
	void (*pick_up)(object *, object *);
	mapstruct *(*out_of_map)(mapstruct *, int *, int *);
	void (*esrv_send_item)(object *, object *);
	player *(*find_player)(char *);
	int (*manual_apply)(object *, object *, int);
	int (*command_drop)(object *, char *);
	int (*transfer_ob)(object *, int, int, int, object *, object *);
	int (*kill_object)(object *, int, object *, int);
	void (*do_learn_spell)(object *, int, int);
	void (*do_forget_spell)(object *, int);
	int (*look_up_spell_name)(const char *);
	int (*check_spell_known)(object *, int);
	void (*esrv_send_inventory)(object *, object *);
	object *(*get_archetype)(const char *);
	mapstruct *(*ready_map_name)(const char *, int);
	sint32 (*add_exp)(object *, int, int);
	const char *(*determine_god)(object *);
	object *(*find_god)(const char *);
	void (*register_global_event)(char *, int);
	void (*unregister_global_event)(char *, int);
	void (*dump_me)(object *, char *);
	object *(*load_object_str)(char *);
	sint64 (*query_cost)(object *, object *, int);
	sint64 (*query_money)(object *);
	int (*pay_for_item)(object *, object *);
	int (*pay_for_amount)(sint64, object *);
	void (*new_draw_info)(int, int, object *, const char *);
	void (*send_plugin_custom_message)(object *, char, char *);
	void (*communicate)(object *, char *);
	object *(*object_create_clone)(object *);
	object *(*get_object)();
	void (*copy_object)(object *, object *);
	void (*enter_exit)(object *, object *);
	void (*play_sound_map)(mapstruct *, int, int, int, int);
	int (*learn_skill)(object *, object *, char *, int, int);
	object *(*find_marked_object)(object *);
	int (*cast_identify)(object *, int, object *, int);
	int (*lookup_skill_by_name)(char *);
	int (*check_skill_known)(object *, int);
	archetype *(*find_archetype)(const char *);
	object *(*arch_to_object)(archetype *);
	object *(*insert_ob_in_map)(object *, mapstruct *, object *, int);
	char *(*cost_string_from_value)(sint64);
	int (*bank_deposit)(object *, object *, char *);
	int (*bank_withdraw)(object *, object *, char *);
	int (*swap_apartments)(char *, char *, int, int, object *);
	int (*player_exists)(char *);
	void (*get_tod)(timeofday_t *);
	const char *(*get_ob_key_value)(const object *, const char *const);
	int (*set_ob_key_value)(object *, const char *, const char *, int);

	const char **season_name;
	const char **weekdays;
	const char **month_name;
	const char **periodsofday;
};

#endif
