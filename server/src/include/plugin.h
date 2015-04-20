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

/**
 * The plugin hook list.
 *
 * If you need a function or variable from server accessed by a plugin,
 * add it here and to ::hooklist in plugins.c. */
struct plugin_hooklist {
    char *(*query_name)(object *, object *);
    const char *(*re_cmp)(const char *, const char *);
    object *(*present_in_ob)(unsigned char, object *);
    int (*players_on_map)(mapstruct *);
    char *(*create_pathname)(const char *);
    void (*free_string_shared)(const char *);
    const char *(*add_string)(const char *);
    void (*object_remove)(object *, int);
    void (*object_destroy)(object *);
    int (*living_update)(object *);
    object *(*insert_ob_in_ob)(object *, object *);
    void (*draw_info_map)(uint8_t, const char *, const char *, mapstruct *, int, int, int, object *, object *, const char *);
    void (*rune_spring)(object *, object *);
    int (*cast_spell)(object *, object *, int, int, int, int, const char *);
    void (*update_ob_speed)(object *);
    int (*change_skill)(object *, int);
    void (*pick_up)(object *, object *, int);
    mapstruct *(*get_map_from_coord)(mapstruct *, int *, int *);
    void (*esrv_send_item)(object *);
    player *(*find_player)(const char *);
    int (*manual_apply)(object *, object *, int);
    void (*command_drop)(object *, const char *, char *);
    int (*transfer_ob)(object *, int, int, int, object *, object *);
    int (*kill_object)(object *, int, object *, int);
    void (*esrv_send_inventory)(object *, object *);
    object *(*get_archetype)(const char *);
    mapstruct *(*ready_map_name)(const char *, mapstruct *, int);
    int64_t(*add_exp)(object *, int64_t, int, int);
    const char *(*determine_god)(object *);
    object *(*find_god)(const char *);
    void (*register_global_event)(const char *, int);
    void (*unregister_global_event)(const char *, int);
    object *(*load_object_str)(char *);
    int64_t(*query_cost)(object *, object *, int);
    int64_t(*query_money)(object *);
    int (*pay_for_item)(object *, object *);
    int (*pay_for_amount)(int64_t, object *);
    object *(*object_create_clone)(object *);
    object *(*get_object)(void);
    void (*copy_object)(object *, object *, int);
    int (*object_enter_map)(object *, object *, mapstruct *, int, int, uint8_t);
    void (*play_sound_map)(mapstruct *, int, const char *, int, int, int, int);
    object *(*find_marked_object)(object *);
    int (*cast_identify)(object *, int, object *, int);
    archetype *(*find_archetype)(const char *);
    object *(*arch_to_object)(archetype *);
    object *(*insert_ob_in_map)(object *, mapstruct *, object *, int);
    char *(*cost_string_from_value)(int64_t);
    int (*bank_deposit)(object *, const char *, int64_t *value);
    int (*bank_withdraw)(object *, const char *, int64_t *value);
    int64_t(*bank_get_balance)(object *);
    int (*swap_apartments)(const char *, const char *, int, int, object *);
    int (*player_exists)(const char *);
    void (*get_tod)(timeofday_t *);
    const char *(*object_get_value)(const object *, const char *const);
    int (*object_set_value)(object *, const char *, const char *, int);
    void (*drop)(object *, object *, int);
    char *(*query_short_name)(object *, object *);
    object *(*beacon_locate)(shstr *);
    void (*player_cleanup_name)(char *);
    party_struct *(*find_party)(const char *);
    void (*add_party_member)(party_struct *, object *);
    void (*remove_party_member)(party_struct *, object *);
    void (*send_party_message)(party_struct *, const char *, int, object *, object *);
    void (*dump_object)(object *, StringBuffer *);
    StringBuffer *(*stringbuffer_new)(void);
    void (*stringbuffer_append_string)(StringBuffer *, const char *);
    void (*stringbuffer_append_printf)(StringBuffer *, const char *, ...);
    char *(*stringbuffer_finish)(StringBuffer *);
    int (*find_face)(char *, int);
    int (*find_animation)(char *);
    void (*play_sound_player_only)(player *, int, const char *, int, int, int, int);
    int (*was_destroyed)(object *, tag_t);
    int (*object_get_gender)(object *);
    object *(*decrease_ob_nr)(object *, uint32_t);
    int (*wall)(mapstruct *, int, int);
    int (*blocked)(object *, mapstruct *, int, int, int);
    int (*get_rangevector)(object *, object *, rv_vector *, int);
    int (*get_rangevector_from_mapcoords)(mapstruct *, int, int, mapstruct *, int, int, rv_vector *, int);
    int (*player_can_carry)(object *, uint32_t);
    cache_struct *(*cache_find)(shstr *);
    int (*cache_add)(const char *, void *, uint32_t);
    int (*cache_remove)(shstr *);
    void (*cache_remove_by_flags)(uint32_t);
    shstr *(*find_string)(const char *);
    void (*command_take)(object *, const char *, char *);
    void (*esrv_update_item)(int, object *);
    void (*commands_handle)(object *, char *);
    treasurelist *(*find_treasurelist)(const char *);
    void (*create_treasure)(treasurelist *, object *, int, int, int, int, int, struct _change_arch *);
    void (*dump_object_rec)(object *, StringBuffer *);
    int (*hit_player)(object *, int, object *, int);
    int (*move_ob)(object *, int, object *);
    mapstruct *(*get_empty_map)(int, int);
    void (*set_map_darkness)(mapstruct *, int);
    int (*find_free_spot)(archetype *, object *, mapstruct *, int, int, int, int);
    void (*send_target_command)(player *);
    void (*examine)(object *, object *, StringBuffer *sb_capture);
    void (*draw_info)(const char *, object *, const char *);
    void (*draw_info_format)(const char *, object *, const char *, ...);
    void (*draw_info_type)(uint8_t, const char *, const char *, object *, const char *);
    void (*draw_info_type_format)(uint8_t, const char *, const char *, object *, const char *, ...);
    artifactlist *(*find_artifactlist)(int);
    void (*give_artifact_abilities)(object *, artifact *);
    int (*connection_object_get_value)(object *);
    void (*connection_object_add)(object *, mapstruct *, int);
    void (*connection_trigger)(object *, int);
    void (*connection_trigger_button)(object *, int);
    packet_struct *(*packet_new)(uint8_t, size_t, size_t);
    void (*packet_free)(packet_struct *);
    void (*packet_compress)(packet_struct *);
    void (*packet_enable_ndelay)(packet_struct *);
    void (*packet_set_pos)(packet_struct *, size_t);
    size_t(*packet_get_pos)(packet_struct *);
    void (*packet_append_uint8)(packet_struct *, uint8_t);
    void (*packet_append_sint8)(packet_struct *, int8_t);
    void (*packet_append_uint16)(packet_struct *, uint16_t);
    void (*packet_append_sint16)(packet_struct *, int16_t);
    void (*packet_append_uint32)(packet_struct *, uint32_t);
    void (*packet_append_sint32)(packet_struct *, int32_t);
    void (*packet_append_uint64)(packet_struct *, uint64_t);
    void (*packet_append_sint64)(packet_struct *, int64_t);
    void (*packet_append_data_len)(packet_struct *, const uint8_t *, size_t);
    void (*packet_append_string)(packet_struct *, const char *);
    void (*packet_append_string_terminated)(packet_struct *, const char *);
    void (*packet_append_map_name)(packet_struct *, object *, object *);
    void (*packet_append_map_music)(packet_struct *, object *, object *);
    void (*packet_append_map_weather)(packet_struct *, object *, object *);
    void (*socket_send_packet)(socket_struct *, packet_struct *);
    void (*logger_print)(logger_level, const char *, uint64_t, const char *, ...);
    logger_level(*logger_get_level)(const char *);
    void (*commands_add)(const char *, command_func, double, uint64_t);
    int (*map_get_darkness)(mapstruct *, int, int, object **);
    char *(*map_get_path)(mapstruct *, const char *, uint8_t, const char *);
    int (*map_path_isabs)(const char *);
    char *(*path_dirname)(const char *);
    char *(*path_basename)(const char *);
    char *(*string_join)(const char *delim, ...);
    object *(*get_env_recursive)(object *);
    int (*set_variable)(object *, const char *);
    uint64_t(*level_exp)(int, double);
    int (*string_endswith)(const char *, const char *);
    char *(*string_sub)(const char *, ssize_t, ssize_t MEMORY_DEBUG_PROTO);
    char *(*path_join)(const char *, const char *);
    void (*monster_enemy_signal)(object *, object *);
    void (*map_redraw)(mapstruct *, int, int, int, int);
#ifndef NDEBUG
    void *(*memory_emalloc)(size_t, const char *, uint32_t);
    void (*memory_efree)(void *, const char *, uint32_t);
    void *(*memory_ecalloc)(size_t, size_t, const char *, uint32_t);
    void *(*memory_erealloc)(void *, size_t, const char *, uint32_t);
    void *(*memory_reallocz)(void *, size_t, size_t, const char *, uint32_t);
    char *(*string_estrdup)(const char *, const char *, uint32_t);
    char *(*string_estrndup)(const char *, size_t , const char *, uint32_t);
#else
    void *(*memory_emalloc)(size_t);
    void (*memory_efree)(void *);
    void *(*memory_ecalloc)(size_t, size_t);
    void *(*memory_erealloc)(void *, size_t);
    void *(*memory_reallocz)(void *, size_t, size_t);
    char *(*string_estrdup)(const char *);
    char *(*string_estrndup)(const char *, size_t);
#endif

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
    region_struct **first_region;
    long *pticks;
    settings_struct *settings;
    skill_struct *skills;
};

/** General API function. */
typedef void *(*f_plug_api)(int *type, ...);
/** First function called in a plugin. */
typedef void *(*f_plug_init)(struct plugin_hooklist *hooklist);
/** Function called after the plugin was initialized. */
typedef void *(*f_plug_pinit)(void);

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
