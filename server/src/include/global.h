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
 * Global definitions, u/sint8, things like that. */

#ifndef GLOBAL_H
#define GLOBAL_H

#include "includes.h"

/**
 * @defgroup MONEYSTRING_xxx Money string modes
 * Modes used for ::_money_block and get_money_from_string().
 *@{*/

/** Invalid string (did not include any valid amount). */
#define MONEYSTRING_NOTHING 0
/** Got a valid amount of money from string. */
#define MONEYSTRING_AMOUNT 1
/** The string was "all". */
#define MONEYSTRING_ALL -1
/*@}*/

/**
 * Used for depositing/withdrawing money from bank, and using string
 * to get information about how much money to deposit/withdraw. */
typedef struct _money_block
{
    /** One of @ref MONEYSTRING_xxx. */
    int mode;

    /** Number of mithril coins. */
    sint64 mithril;

    /** Number of gold coins. */
    sint64 gold;

    /** Number of silver coins. */
    sint64 silver;

    /** Number of copper coins. */
    sint64 copper;
} _money_block;

/**
 * @defgroup BANK_xxx Bank return values
 * Meaningful constants of values returned by bank_withdraw() and
 * bank_deposit().
 *
 * BANK_WITHDRAW_xxx constants can only be returned by bank_withdraw(),
 * while BANK_DEPOSIT_xxx constants can only be returned by bank_deposit().
 * Any other constants can be returned by both functions.
 *@{*/
/** Syntax error: did not get text in expected format. */
#define BANK_SYNTAX_ERROR -1
/** Successfully withdrawn/deposited money. */
#define BANK_SUCCESS 0

/** Withdraw value was too high. */
#define BANK_WITHDRAW_HIGH 1
/** Player wanted to withdraw more than they have in bank. */
#define BANK_WITHDRAW_MISSING 2
/** Withdrawing that much money would make the player overweight. */
#define BANK_WITHDRAW_OVERWEIGHT 3

/** Player doesn't have enough copper coins on hand. */
#define BANK_DEPOSIT_COPPER 1
/** Player doesn't have enough silver coins on hand. */
#define BANK_DEPOSIT_SILVER 2
/** Player doesn't have enough gold coins on hand. */
#define BANK_DEPOSIT_GOLD 3
/** Player doesn't have enough mithril coins on hand. */
#define BANK_DEPOSIT_MITHRIL 4
/*@}*/

#define POW2(x) ((x) * (x))

/**
 * @defgroup MAP_INFO_xxx Map info modes
 * Modes used for new_info_map().
 *@{*/

/** Normal map info, to everyone in range of 12 tiles. */
#define MAP_INFO_NORMAL 12
/** To everyone on a map; this is a special value. */
#define MAP_INFO_ALL 9999
/*@}*/

/**
 * @defgroup shared_string_macros Shared string macros
 * Common macros used for manipulating shared strings.
 *@{*/

/**
 * Free old shared string and add new string.
 * @param _sv_ Shared string.
 * @param _nv_ String to copy to the shared string. */
#define FREE_AND_COPY_HASH(_sv_, _nv_) \
    {                                      \
        if (_sv_)                          \
        {                                  \
            free_string_shared(_sv_);      \
        }                                  \
                                       \
        _sv_ = add_string(_nv_);           \
    }
/**
 * Free old hash and add a reference to the new one.
 * @param _sv_ Pointer to shared string.
 * @param _nv_ String to add reference to. Must be a shared string. */
#define FREE_AND_ADD_REF_HASH(_sv_, _nv_) \
    {                                         \
        if (_sv_)                             \
        {                                     \
            free_string_shared(_sv_);         \
        }                                     \
                                          \
        _sv_ = add_refcount(_nv_);            \
    }
/**
 * Free and NULL a shared string.
 * @param _nv_ Shared string to free and NULL. */
#define FREE_AND_CLEAR_HASH(_nv_) \
    {                                 \
        if (_nv_)                     \
        {                             \
            free_string_shared(_nv_); \
            _nv_ = NULL;              \
        }                             \
    }
/**
 * Free a shared string.
 * @param _nv_ Shared string to free. */
#define FREE_ONLY_HASH(_nv_)  \
    if (_nv_)                     \
    {                             \
        free_string_shared(_nv_); \
    }
/**
 * Add reference to a non-null hash.
 * @param _nv_ Pointer to shared string. */
#define ADD_REF_NOT_NULL_HASH(_nv_) \
    if (_nv_)                           \
    {                                   \
        add_refcount(_nv_);             \
    }
/**
 * @copydoc FREE_AND_CLEAR_HASH
 * @warning Like FREE_AND_CLEAR_HASH(), but without { and }. */
#define FREE_AND_CLEAR_HASH2(_nv_) \
    if (_nv_)                          \
    {                                  \
        free_string_shared(_nv_);      \
        _nv_ = NULL;                   \
    }
/*@}*/

#define SPAWN_RANDOM_RANGE 10000

#define T_STYLE_UNSET (-2)
#define ART_CHANCE_UNSET (-1)

/** Minimum monster detection radius */
#define MIN_MON_RADIUS 2
/** If target of mob is out of this range (or stats.Wis if higher). */
#define MAX_AGGRO_RANGE 9
/** Until this time - then it skip target */
#define MAX_AGGRO_TIME 12

/**
 * @defgroup TILED_xxx Tiled map constants
 * Constants of tiled map IDs.
 *@{*/
/** North. */
#define TILED_NORTH 0
/** East. */
#define TILED_EAST 1
/** South. */
#define TILED_SOUTH 2
/** West. */
#define TILED_WEST 3
/** Northeast. */
#define TILED_NORTHEAST 4
/** Southeast. */
#define TILED_SOUTHEAST 5
/** Southwest. */
#define TILED_SOUTHWEST 6
/** Northwest. */
#define TILED_NORTHWEST 7
/** Maximum number of tiled maps. */
#define TILED_NUM 8
/*@}*/

#define EXP_AGILITY 1
#define EXP_MENTAL 2
#define EXP_MAGICAL 3
#define EXP_PERSONAL 4
#define EXP_PHYSICAL 5
#define EXP_WISDOM 6

/** The maximum level. */
#define MAXLEVEL 115

/** Maximum experience. */
#define MAX_EXPERIENCE new_levels[MAXLEVEL]

/**
 * Used to link together shared strings. */
typedef struct linked_char
{
    /** Shared string. */
    shstr *name;

    /** Next shared string. */
    struct linked_char *next;
} linked_char;

#include <face.h>
#include <attack.h>
#include <material.h>
#include <living.h>
#include <object.h>
#include <object_methods.h>
#include <arch.h>
#include <map.h>
#include <tod.h>
#include <pathfinder.h>
#include <newserver.h>
#include <skills.h>
#include <party.h>
#include <player.h>
#include <treasure.h>
#include <commands.h>
#include <artifact.h>
#include <race.h>
#include <recipe.h>
#include <spells.h>

/**
 * Special potions are identified by the last_eat value.
 * last_eat == 0 is no special potion - means they are used
 * as spell effect carrier. */
#define special_potion(__op_sp) (__op_sp)->last_eat

/** Move an object. */
#define move_object(__op, __dir) move_ob(__op, __dir, __op)
/** Is the object magical? */
#define is_magical(__op_) QUERY_FLAG(__op_, FLAG_IS_MAGICAL)

#define NROF_COMPRESS_METHODS 4

/** Use to get a safe string, even if the string is NULL. */
#define STRING_SAFE(__string__) (__string__ ? __string__ : ">NULL<")
/** Use to get a safe name of arch, even if the arch name is NULL. */
#define STRING_ARCH_NAME(__arch__) ((__arch__)->name ? (__arch__)->name : ">NULL<")
/** Use to get a safe name of object, even if the object name is NULL. */
#define STRING_OBJ_NAME(__ob__) ((__ob__) && (__ob__)->name ? (__ob__)->name : ">NULL<")
/** Use to get a safe arch name of object, even if the object arch name is NULL.
 * */
#define STRING_OBJ_ARCH_NAME(__ob__) ((__ob__)->arch ? ((__ob__)->arch->name ? (__ob__)->arch->name : ">NULL<") : ">NULL<")
/** Use to get a safe slaying value of an object, even if the slaying value is
 * NULL. */
#define STRING_OBJ_SLAYING(__ob__) ((__ob__)->slaying ? (__ob__)->slaying : ">NULL<")

/**
 * Set object's face by its animation ID.
 * @param ob Object.
 * @param newanim Animation ID to set. */
#define SET_ANIMATION(ob, newanim) (ob)->face = &new_faces[animations[(ob)->animation_id].faces[(newanim)]]
/**
 * Set object's animation depending on its number of animations/facings,
 * direction and animation state. */
#define SET_ANIMATION_STATE(ob) \
    if ((ob)->animation_id && NUM_FACINGS((ob)) && (QUERY_FLAG((ob), FLAG_IS_TURNABLE) || QUERY_FLAG((ob), FLAG_ANIMATE))) \
    { \
        SET_ANIMATION((ob), (NUM_ANIMATIONS((ob)) / NUM_FACINGS((ob))) * (QUERY_FLAG((ob), FLAG_IS_TURNABLE) ? (ob)->direction : 0) + (ob)->state); \
        update_object((ob), UP_OBJ_FACE); \
    }
/** Get object's animation ID. */
#define GET_ANIM_ID(ob) (ob->animation_id)
/** Get object's inventory animation ID. */
#define GET_INV_ANIM_ID(ob) (ob->inv_animation_id)
/** Get the number of possible animations for an object. */
#define NUM_ANIMATIONS(ob) (animations[ob->animation_id].num_animations)
/** Get the number of possible facings for object's animation. */
#define NUM_FACINGS(ob) (animations[ob->animation_id].facings)

/** Free and NULL a pointer. */
#define FREE_AND_NULL_PTR(_xyz_) \
    {                                \
        if (_xyz_)                   \
        {                            \
            free(_xyz_);             \
        }                            \
                                 \
        _xyz_ = NULL;                \
    }

enum
{
    ALLOWED_CHARS_ACCOUNT,
    ALLOWED_CHARS_CHARNAME,
    ALLOWED_CHARS_PASSWORD,

    ALLOWED_CHARS_NUM
};

/**
 * The server settings. */
typedef struct settings_struct
{
    /**
     * Port to use for client/server communication. */
    uint16 port;

    /**
     * Read only data files, such as the collected archetypes. */
    char libpath[MAX_BUF];

    /**
     * Player data, unique maps, etc. */
    char datapath[MAX_BUF];

    /**
     * Where the map files are. */
    char mapspath[MAX_BUF];

    /**
     * HTTP URL of the metaserver. */
    char metaserver_url[MAX_BUF];

    /**
     * Hostname of this server. */
    char server_host[MAX_BUF];

    /**
     * Name of this server. */
    char server_name[MAX_BUF];

    /**
     * Comment about the server we send to the metaserver. */
    char server_desc[MAX_BUF];

    /**
     * Location of the client maps. */
    char client_maps_url[MAX_BUF];

    /**
     * Executing the world maker? */
    uint8 world_maker;

    /**
     * Where to store the world maker images. */
    char world_maker_dir[MAX_BUF];

    /**
     * Adjustment to maximum magical device level the player may use. */
    sint8 magic_devices_level;

    /**
     * See note in server.cfg. */
    double item_power_factor;

    /**
     * Whether to reload Python modules whenever Python script executes. */
    uint8 python_reload_modules;

    /**
     * Comma-delimited list of permission groups each player
     * automatically has. */
    char default_permission_groups[MAX_BUF];

    /**
     * Allowed characters for certain strings. */
    char allowed_chars[ALLOWED_CHARS_NUM][MAX_BUF];

    /**
     * Limits on the allowed characters. */
    size_t limits[ALLOWED_CHARS_NUM][2];

    /**
     * IPs allowed to remotely control the client. */
    char control_allowed_ips[HUGE_BUF];

    /**
     * Which player the remote command goes through, if applicable. */
    char control_player[MAX_BUF];
} settings_struct;

/** Constant shared string pointers. */
typedef struct shstr_constants
{
    const char *none;
    const char *NONE;
    const char *home;
    const char *force;
    const char *portal_destination_name;
    const char *portal_active_name;

    const char *player_info;
    const char *BANK_GENERAL;

    const char *of_poison;
    const char *of_hideous_poison;
} shstr_constants;

/** Ban structure. */
typedef struct ban_struct
{
    /** Name of the banned player. */
    const char *name;

    /** Banned IP/hostname. */
    char *ip;
} _ban_struct;

/**
 * @defgroup CACHE_FLAG_xxx Cache flags
 * The various cache flag bitmasks.
 *@{*/
/** The cache contains pointer to a PyObject. */
#define CACHE_FLAG_PYOBJ 1
/** Automatically free() the pointer in the cache entry. */
#define CACHE_FLAG_AUTOFREE 2
/** Does the cache entry want plugin global event when it's removed? */
#define CACHE_FLAG_GEVENT 4
/*@}*/

/** One cache entry. */
typedef struct cache_struct
{
    /** Key name this entry is identified by. */
    shstr *key;

    /** The cached data. */
    void *ptr;

    /** Type of the data, one of @ref CACHE_TYPE_xxx. */
    uint32 flags;

    /** ID in the array. */
    size_t id;
} cache_struct;

#ifndef tolower
#   define tolower(C) (((C) >= 'A' && (C) <= 'Z') ? (C) - 'A' + 'a' : (C))
#endif

#define GETTIMEOFDAY(last_time) gettimeofday(last_time, (struct timezone *) NULL);

/**
 * @defgroup SCRIPT_FIX_xxx For plugin events
 * These are used by plugin events.
 *@{*/
/** Fix the event activator. */
#define SCRIPT_FIX_ACTIVATOR 2
/** Fix all objects related to the event. */
#define SCRIPT_FIX_ALL 1
/** Don't do any fixing. */
#define SCRIPT_FIX_NOTHING 0
/*@}*/

#include "random_map.h"

#ifndef GLOBAL_NO_PROTOTYPES
#   include "proto.h"
#endif

#include "plugin.h"

#endif
