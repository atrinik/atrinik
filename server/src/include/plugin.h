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
 * Atrinik plugin support header file.
 *
 * @author Yann Chachkoff */

#ifndef PLUGIN_H
#define PLUGIN_H

#ifndef WIN32
#	include <dlfcn.h>
#endif

#undef MODULEAPI

#ifdef WIN32
#	ifdef PYTHON_PLUGIN_EXPORTS
#		define MODULEAPI __declspec(dllexport)
#	else
#		define MODULEAPI __declspec(dllimport)
#	endif
#else
#	define MODULEAPI
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
#define EVENT_CLOSE	11
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
/** Number of global events. */
#define GEVENT_NUM 5
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

/**
 * The plugin hook list.
 *
 * If you need a function or variable from server accessed by a plugin,
 * add it here and to ::hooklist in plugins.c. */
struct plugin_hooklist
{
	char *(*query_name)(object *, object *);
	const char *(*re_cmp)(const char *, const char *);
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
	void (*draw_info_map)(int , const char *color, mapstruct *, int, int, int, object *, object *, const char *);
	void (*spring_trap)(object *, object *);
	int (*cast_spell)(object *, object *, int, int, int, int, const char *);
	void (*update_ob_speed)(object *);
	int (*command_rskill)(object *, char *);
	void (*become_follower)(object *, object *);
	void (*pick_up)(object *, object *, int);
	mapstruct *(*get_map_from_coord)(mapstruct *, int *, int *);
	void (*esrv_send_item)(object *);
	player *(*find_player)(const char *);
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
	sint64 (*add_exp)(object *, sint64, int, int);
	const char *(*determine_god)(object *);
	object *(*find_god)(const char *);
	void (*register_global_event)(const char *, int);
	void (*unregister_global_event)(const char *, int);
	object *(*load_object_str)(char *);
	sint64 (*query_cost)(object *, object *, int);
	sint64 (*query_money)(object *);
	int (*pay_for_item)(object *, object *);
	int (*pay_for_amount)(sint64, object *);
	void (*communicate)(object *, char *);
	object *(*object_create_clone)(object *);
	object *(*get_object)(void);
	void (*copy_object)(object *, object *, int);
	void (*enter_exit)(object *, object *);
	void (*play_sound_map)(mapstruct *, int, const char *, int, int, int, int);
	int (*learn_skill)(object *, object *, char *, int, int);
	object *(*find_marked_object)(object *);
	int (*cast_identify)(object *, int, object *, int);
	int (*lookup_skill_by_name)(const char *);
	int (*check_skill_known)(object *, int);
	archetype *(*find_archetype)(const char *);
	object *(*arch_to_object)(archetype *);
	object *(*insert_ob_in_map)(object *, mapstruct *, object *, int);
	char *(*cost_string_from_value)(sint64);
	int (*bank_deposit)(object *, const char *, sint64 *value);
	int (*bank_withdraw)(object *, const char *, sint64 *value);
	sint64 (*bank_get_balance)(object *);
	int (*swap_apartments)(const char *, const char *, int, int, object *);
	int (*player_exists)(char *);
	void (*get_tod)(timeofday_t *);
	const char *(*object_get_value)(const object *, const char *const);
	int (*object_set_value)(object *, const char *, const char *, int);
	void (*drop)(object *, object *, int);
	char *(*query_short_name)(object *, object *);
	object *(*beacon_locate)(shstr *);
	char *(*strdup_local)(const char *);
	void (*adjust_player_name)(char *);
	party_struct *(*find_party)(const char *);
	void (*add_party_member)(party_struct *, object *);
	void (*remove_party_member)(party_struct *, object *);
	void (*send_party_message)(party_struct *, const char *, int, object *);
	void (*Write_String_To_Socket)(socket_struct *, char, const char *, int);
	void (*dump_object)(object *, StringBuffer *);
	StringBuffer *(*stringbuffer_new)(void);
	void (*stringbuffer_append_string)(StringBuffer *, const char *);
	void (*stringbuffer_append_printf)(StringBuffer *, const char *, ...);
	char *(*stringbuffer_finish)(StringBuffer *);
	char *(*cleanup_chat_string)(char *);
	int (*find_face)(char *, int);
	int (*find_animation)(char *);
	void (*play_sound_player_only)(player *, int, const char *, int, int, int, int);
	int (*was_destroyed)(object *, tag_t);
	int (*object_get_gender)(object *);
	int (*change_abil)(object *, object *);
	object *(*decrease_ob_nr)(object *, uint32);
	int (*wall)(mapstruct *, int, int);
	int (*blocked)(object *, mapstruct *, int, int, int);
	int (*get_rangevector)(object *, object *, rv_vector *, int);
	int (*get_rangevector_from_mapcoords)(mapstruct *, int, int, mapstruct *, int, int, rv_vector *, int);
	int (*player_can_carry)(object *, uint32);
	cache_struct *(*cache_find)(shstr *);
	int (*cache_add)(const char *, void *, uint32);
	int (*cache_remove)(shstr *);
	void (*cache_remove_by_flags)(uint32);
	shstr *(*find_string)(const char *);
	int (*command_take)(object *, char *);
	void (*esrv_update_item)(int, object *);
	int (*execute_newserver_command)(object *, char *);
	treasurelist *(*find_treasurelist)(const char *);
	void (*create_treasure)(treasurelist *, object *, int, int, int, int, int, struct _change_arch *);
	void (*dump_object_rec)(object *, StringBuffer *);
	int (*hit_player)(object *, int, object *, int);
	int (*move_ob)(object *, int, object *);
	int (*move_player)(object *, int);
	mapstruct *(*get_empty_map)(int, int);
	void (*set_map_darkness)(mapstruct *, int);
	int (*find_free_spot)(archetype *, object *, mapstruct *, int, int, int, int);
	void (*send_target_command)(player *);
	void (*examine)(object *, object *, StringBuffer *sb_capture);
	void (*push_button)(object *);
	void (*draw_info)(const char *, object *, const char *);
	void (*draw_info_format)(const char *, object *, const char *, ...);
	void (*draw_info_flags)(int, const char *, object *, const char *);
	void (*draw_info_flags_format)(int, const char *, object *, const char *, ...);
	void (*Send_With_Handling)(socket_struct *, SockList *);
	void (*SockList_AddString)(SockList *, const char *);
	artifactlist *(*find_artifactlist)(int);
	void (*give_artifact_abilities)(object *, artifact *);

	const char **season_name;
	const char **weekdays;
	const char **month_name;
	const char **periodsofday;
	spell_struct *spells;
	struct shstr_constants *shstr_cons;
	const char **gender_noun;
	const char **gender_subjective;
	const char **gender_subjective_upper;
	const char **gender_objective;
	const char **gender_possessive;
	const char **gender_reflexive;
	const char **object_flag_names;
	int *freearr_x;
	int *freearr_y;
	player **first_player;
	New_Face **new_faces;
	int *nrofpixmaps;
	Animations **animations;
	int *num_animations;
	archetype **first_archetype;
	mapstruct **first_map;
	party_struct **first_party;
	region **first_region;
	FILE **logfile;
	long *pticks;
};

/** General API function. */
typedef void *(*f_plug_api) (int *type, ...);
/** First function called in a plugin. */
typedef void *(*f_plug_init) (struct plugin_hooklist *hooklist);
/** Function called after the plugin was initialized. */
typedef void *(*f_plug_pinit) (void);

#ifndef WIN32
	/** Library handle. */
#	define LIBPTRTYPE void *
	/** Load a shared library. */
#	define plugins_dlopen(fname) dlopen(fname, RTLD_NOW | RTLD_GLOBAL)
	/** Unload a shared library. */
#	define plugins_dlclose(lib) dlclose(lib)
	/** Get a function from a shared library. */
#	define plugins_dlsym(lib, name) dlsym(lib, name)
	/** Library error. */
#	define plugins_dlerror() dlerror()
#else
#	define LIBPTRTYPE HMODULE
#	define plugins_dlopen(fname) LoadLibrary(fname)
#	define plugins_dlclose(lib) FreeLibrary(lib)
#	define plugins_dlsym(lib, name) GetProcAddress(lib, name)
#endif

/** Check if the specified filename is a plugin file. */
#define FILENAME_IS_PLUGIN(_path) (strstr((_path), "plugin_") && !strcmp((_path) + strlen((_path)) - strlen(PLUGIN_SUFFIX), PLUGIN_SUFFIX))

/** One loaded plugin. */
typedef struct atrinik_plugin
{
	/** Event handler function. */
	f_plug_api eventfunc;

	/** Plugin getProperty function. */
	f_plug_api propfunc;

	/** Plugin closePlugin function. */
	f_plug_pinit closefunc;

	/** Pointer to the plugin library. */
	LIBPTRTYPE libptr;

	/** Plugin identification string. */
	char id[MAX_BUF];

	/** Plugin's full name. */
	char fullname[MAX_BUF];

	/** Global events registered. */
	sint8 gevent[GEVENT_NUM];

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
extern MODULEAPI void *getPluginProperty(int *type, ...);

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
