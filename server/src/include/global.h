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
 * @file
 * Global definitions, u/sint8, things like that. */

#ifndef GLOBAL_H
#define GLOBAL_H

#ifndef EXTERN
#define EXTERN extern
#endif

/* If we're not using GNU C, ignore __attribute__ */
#ifndef __GNUC__
#	define  __attribute__(x)
#endif

#include "includes.h"

/* Type defines for specific signed/unsigned variables of a certain number
 * of bits.  Not really used anyplace, but if a certain number of bits
 * is required, these type defines should then be used.  This will make
 * porting to systems that have different sized data types easier.
 *
 * Note: The type defines should just mean that the data type has at
 * least that many bits.  if a uint16 is actually 32 bits, no big deal,
 * it is just a waste of space.
 *
 * Note2:  When using something that is normally stored in a character
 * (ie strings), don't use the uint8/sint8 typdefs, use 'char' instead.
 * The signedness for char is probably not universal, and using char
 * will probably be more portable than sint8/unit8 */

/** uint32 */
typedef unsigned int uint32;
#ifndef UINT32_MAX
#	define UINT32_MAX (4294967295U)
#endif

/** sint32 */
typedef signed int sint32;
#define SINT32_MIN (-2147483647 - 1)
#define SINT32_MAX 2147483647

/** uint16 */
typedef unsigned short uint16;
#ifndef UINT16_MAX
#	define UINT16_MAX (65535U)
#endif

/** sint16 */
typedef signed short sint16;
#define SINT16_MIN (-32767 - 1)
#define SINT16_MAX (32767)

/** uint8 */
typedef unsigned char uint8;
#ifndef UINT8_MAX
#	define UINT8_MAX (255U)
#endif

/** sint8 */
typedef signed char sint8;
#define SINT8_MIN (-128)
#define SINT8_MAX (127)

/** Used for faces. */
typedef unsigned short Fontindex;

/** Object unique IDs. */
typedef unsigned int tag_t;

/** This should be used to differentiate shared strings from normal strings. */
typedef const char shstr;

#ifdef WIN32
	/* Python plugin stuff defines SIZEOF_LONG_LONG as 8, and besides __int64
	 * is a 64b type on MSVC... So let's force the typedef. */
	typedef unsigned __int64            uint64;
	typedef signed __int64              sint64;
#	define atoll                        _atoi64

#	define FMT64                        "I64d"
#	define FMT64U                       "I64u"
#	define FMT64HEX                     "I64x"
#else
#	if SIZEOF_LONG == 8
		typedef unsigned long           uint64;
		typedef signed long             sint64;
#		define FMT64                    "ld"
#		define FMT64U                   "lu"
#		define FMT64HEX                 "lx"

#	elif SIZEOF_LONG_LONG == 8
		typedef unsigned long long      uint64;
		typedef signed long long        sint64;
#		define FMT64                    "lld"
#		define FMT64U                   "llu"
#		define FMT64HEX                 "llx"

#	else
#		error Do not know how to get a 64 bit value on this system.
#		error Correct and send email to the Atrinik Team on how to do this.
#	endif
#endif

#ifndef UINT64_MAX
#	define UINT64_MAX (18446744073709551615LLU)
#endif

#define SINT64_MIN (-9223372036854775807LL - 1)
#define SINT64_MAX (9223372036854775807LL)

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
/** To everyone on a map; a value of 9999 should be enough. */
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
 * @defgroup SEND_FACE_xxx Send face return values
 * Return values used in esrv_send_face().
 *@{*/

/** The face was sent OK. */
#define SEND_FACE_OK             0
/** Face number was out of bounds. */
#define SEND_FACE_OUT_OF_BOUNDS  1
/** Face data not available. */
#define SEND_FACE_NO_DATA        2
/*@}*/

/** Number of connected maps from a tiled map */
#define TILED_MAPS 8

#define EXP_AGILITY 1
#define EXP_MENTAL 2
#define EXP_MAGICAL 3
#define EXP_PERSONAL 4
#define EXP_PHYSICAL 5
#define EXP_WISDOM 6
/** Number of experience categories. */
#define MAX_EXP_CAT 7

/** Null experience object. */
#define EXP_NONE 0

/** The maximum level. */
#define MAXLEVEL 115
extern uint64 new_levels[MAXLEVEL + 2];

/**
 * Used to link together shared strings. */
typedef struct linked_char
{
	/** Shared string. */
	shstr *name;

	/** Next shared string. */
	struct linked_char *next;
} linked_char;

#include "face.h"
#include "attack.h"
#include "material.h"
#include "living.h"
#include "object.h"
#include "arch.h"
#include "map.h"
#include "mempool.h"
#include "tod.h"
#include "pathfinder.h"
#include "newserver.h"
#include "skills.h"
#include "player_shop.h"
#include "party.h"
#include "player.h"
#include "treasure.h"
#include "commands.h"
#include "artifact.h"
#include "god.h"
#include "race.h"
#include "sounds.h"
#include "recipe.h"
#include "spells.h"
#include "stringbuffer.h"

/**
 * Special potions are identifed by the last_eat value.
 * last_eat == 0 is no special potion - means they are used
 * as spell effect carrier. */
#define special_potion(__op_sp) (__op_sp)->last_eat

extern int arch_cmp;
extern int arch_search;

/** Move an object. */
#define move_object(__op, __dir) move_ob(__op, __dir, __op)
/** Is the object magical? */
#define is_magical(__op_) QUERY_FLAG(__op_, FLAG_IS_MAGICAL)

extern New_Face *new_faces;

/**
 * @defgroup first_xxx Beginnings of linked lists.
 *@{*/
/** First player. */
player *first_player;
/** First map. */
mapstruct *first_map;
/** First treasure. */
treasurelist *first_treasurelist;
/** First artifact. */
artifactlist *first_artifactlist;
/** God list. */
godlink *first_god;
/*@}*/

/** Last player. */
player *last_player;

#define NROF_COMPRESS_METHODS 4
EXTERN char *uncomp[NROF_COMPRESS_METHODS][3];

/** Ignores signals until init_done is true. */
EXTERN long init_done;
/** If this exceeds MAX_ERRORS, the server will shut down. */
EXTERN long nroferrors;

/** Used by various function to determine how often to save the character. */
extern long pticks;

/** Log file to use. */
EXTERN FILE *logfile;
/** Number of treasures. */
EXTERN long nroftreasures;
/** Number of artifacts. */
EXTERN long nrofartifacts;
/** Number of allowed treasure combinations. */
EXTERN long nrofallowedstr;

extern object void_container;

/** The starting map. */
EXTERN char first_map_path[MAX_BUF];
/**
 * Progressive object counter (every new object will increase this, even
 * if that object is later removed). */
EXTERN long ob_count;

/** Round ticker. */
EXTERN uint32 global_round_tag;
#define ROUND_TAG global_round_tag /* put this here because the DIFF */

/** Global race counter. */
EXTERN int global_race_counter;

/** Used for main loop timing. */
EXTERN struct timeval last_time;
EXTERN Animations *animations;
EXTERN int num_animations, animations_allocated;

/** Use to get a safe string, even if the string is NULL. */
#define STRING_SAFE(__string__) (__string__ ? __string__ : ">NULL<")
/** Use to get a safe name of arch, even if the arch name is NULL. */
#define STRING_ARCH_NAME(__arch__) ((__arch__)->name ? (__arch__)->name : ">NULL<")
/** Use to get a safe name of object, even if the object name is NULL. */
#define STRING_OBJ_NAME(__ob__) ((__ob__) && (__ob__)->name ? (__ob__)->name : ">NULL<")
/** Use to get a safe arch name of object, even if the object arch name is NULL. */
#define STRING_OBJ_ARCH_NAME(__ob__) ((__ob__)->arch ? ((__ob__)->arch->name ? (__ob__)->arch->name : ">NULL<") : ">NULL<")
/** Use to get a safe slaying value of an object, even if the slaying value is NULL. */
#define STRING_OBJ_SLAYING(__ob__) ((__ob__)->slaying ? (__ob__)->slaying : ">NULL<")

/**
 * Set object's face by its animation ID.
 * @param ob Object.
 * @param newanim Animation ID to set. */
#define SET_ANIMATION(ob, newanim) ob->face = &new_faces[animations[ob->animation_id].faces[newanim]]
/** Get object's animation ID. */
#define GET_ANIM_ID(ob) (ob->animation_id)
/** Get object's inventory animation ID. */
#define GET_INV_ANIM_ID(ob) (ob->inv_animation_id)
/** Get the number of possible animations for an object. */
#define NUM_ANIMATIONS(ob) (animations[ob->animation_id].num_animations)
/** Get the number of possible facings for object's animation. */
#define NUM_FACINGS(ob) (animations[ob->animation_id].facings)

extern int freearr_x[SIZEOFFREE], freearr_y[SIZEOFFREE];
extern int maxfree[SIZEOFFREE], freedir[SIZEOFFREE];

extern New_Face *blank_face, *next_item_face, *prev_item_face;

extern long max_time;
extern socket_struct *init_sockets;
extern unsigned long todtick;
extern int world_darkness;

/** Pointer to waypoint archetype. */
EXTERN archetype *wp_archetype;
/** Pointer to empty_archetype archetype. */
EXTERN archetype *empty_archetype;
/** Pointer to base_info archetype. */
EXTERN archetype *base_info_archetype;
EXTERN archetype *level_up_arch;

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

#ifdef CALLOC
#undef CALLOC
#endif

#ifdef USE_CALLOC
#	define CALLOC(x, y) calloc(x, y)
#	define CFREE(x) free(x)
#else
#	define CALLOC(x, y) malloc(x * y)
#	define CFREE(x) free(x)
#endif

/** Settings structure */
typedef struct Settings
{
	/** Logfile to use */
	char *logfilename;

	/** Port for new client/server */
	uint16 csport;

	/** Default debugging level */
	LogLevel debug;

	/** Set to dump various values/tables */
	uint8 dumpvalues;

	/** Additional arguments for some dump functions */
	char *dumparg;

	/** If true, detach and become daemon */
	uint8 daemonmode;

	/** Read only data files */
	char *datadir;

	/** Read/write data files */
	char *localdir;

	/** Where the map files are. */
	char *mapdir;

	/** Where the players are saved. */
	char *playerdir;

	/** Name of the archetypes file - libdir is prepended. */
	char *archetypes;

	/** Location of the treasures file. */
	char *treasures;

	/** Directory for the unique items. */
	char *uniquedir;

	/** Directory to use for temporary files. */
	char *tmpdir;

	/** If true, players lose a random stat when they die. */
	uint8 stat_loss_on_death;

	/** If true, Death stat depletion based on level etc. */
	uint8 balanced_stat_loss;

	/** True if we should send updates */
	unsigned int meta_on:1;

	/** HTTP URL of the metaserver. */
	char meta_server[MAX_BUF];

	/** Hostname of this server. */
	char meta_host[MAX_BUF];

	/** Name of this server. */
	char meta_name[MAX_BUF];

	/** Comment about the server we send to the metaserver. */
	char meta_comment[MAX_BUF];

	/** Use watchdog? */
	uint8 watchdog;

	/** Interactive mode on? */
	uint8 interactive;

	/** Are we going to run unit tests? */
	uint8 unit_tests;

	/** See note in setings file. */
	float item_power_factor;
} Settings;

extern Settings settings;

/** Constant shared string pointers. */
EXTERN struct shstr_constants
{
	const char *none;
	const char *NONE;
	const char *home;
	const char *force;
	const char *portal_destination_name;
	const char *portal_active_name;
	const char *spell_quickslot;

	const char *GUILD_FORCE;
	const char *guild_force;
	const char *RANK_FORCE;
	const char *rank_force;
	const char *ALIGNMENT_FORCE;
	const char *alignment_force;

	const char *grace_limit;
	const char *restore_grace;
	const char *restore_hitpoints;
	const char *restore_spellpoints;
	const char *heal_spell;
	const char *remove_curse;
	const char *remove_damnation;
	const char *heal_depletion;
	const char *message;
	const char *enchant_weapon;

	const char *player_info;
	const char *BANK_GENERAL;

	const char *of_poison;
	const char *of_hideous_poison;
} shstr_cons;

EXTERN void (*object_initializers[256])(object *);

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
} cache_struct;

#ifndef tolower
#	define tolower(C) (((C) >= 'A' && (C) <= 'Z') ? (C) - 'A' + 'a': (C))
#endif

#ifdef GETTIMEOFDAY_TWO_ARGS
#	define GETTIMEOFDAY(last_time) gettimeofday(last_time, (struct timezone *) NULL);
#else
#	define GETTIMEOFDAY(last_time) gettimeofday(last_time);
#endif

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
#include "proto.h"
#include "plugin.h"

#endif
