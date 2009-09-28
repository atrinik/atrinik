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

#ifndef PLUGIN_H_
#define PLUGIN_H_

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

/** Number of local events*/
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
 * @defgroup HOOK_xxx Hook codes
 * Hook codes. A hook is a function pointer passed from the server to the
 * plugin, so the plugin can call a server/crosslib functionality. Some
 * may call them "callbacks", although I don't like that term, which is
 * too closely bound to C and pointers.
 *
 * I didn't add comments for all those hooks, but it should be quite easy
 * to find out to what function they are pointing at. Also consult the
 * plugins.c source file in the server subdirectory to see the hook
 * "wrappers".
 *@{*/
#define HOOK_NONE                0
#define HOOK_LOG                 1
#define HOOK_NEWINFOMAP          2
#define HOOK_SPRINGTRAP          3
#define HOOK_CASTSPELL           4
#define HOOK_CMDRSKILL           5
#define HOOK_BECOMEFOLLOWER      6
#define HOOK_PICKUP              7
#define HOOK_GETMAPOBJECT        8
#define HOOK_ESRVSENDITEM        9
#define HOOK_FINDPLAYER          10
#define HOOK_MANUALAPPLY         11
#define HOOK_CMDDROP             12
#define HOOK_CMDTAKE             13
#define HOOK_CMDTITLE            14
#define HOOK_TRANSFEROBJECT      15
#define HOOK_KILLOBJECT          16
#define HOOK_LEARNSPELL          17
#define HOOK_CHECKFORSPELLNAME   18
#define HOOK_CHECKFORSPELL       19
#define HOOK_ESRVSENDINVENTORY   20
#define HOOK_CREATEARTIFACT      21
#define HOOK_GETARCHETYPE        22
#define HOOK_UPDATESPEED         23
#define HOOK_UPDATEOBJECT        24
#define HOOK_FINDANIMATION       25
#define HOOK_GETARCHBYOBJNAME    26
#define HOOK_INSERTOBJECTINMAP   27
#define HOOK_READYMAPNAME        28
#define HOOK_ADDEXP              29
#define HOOK_DETERMINEGOD        30
#define HOOK_FINDGOD             31
#define HOOK_REGISTEREVENT       32
#define HOOK_UNREGISTEREVENT     33
#define HOOK_DUMPOBJECT          34
#define HOOK_LOADOBJECT          35
#define HOOK_REMOVEOBJECT        36
#define HOOK_ADDSTRING           37
#define HOOK_FREESTRING          38
#define HOOK_ADDREFCOUNT         39
#define HOOK_GETFIRSTMAP         40
#define HOOK_GETFIRSTPLAYER      41
#define HOOK_GETFIRSTARCHETYPE   42
#define HOOK_QUERYCOST           43
#define HOOK_QUERYMONEY          44
#define HOOK_PAYFORITEM          45
#define HOOK_PAYFORAMOUNT        46
#define HOOK_NEWDRAWINFO         47
#define HOOK_SENDCUSTOMCOMMAND   48
#define HOOK_CFTIMERCREATE       49
#define HOOK_CFTIMERDESTROY      50
#define HOOK_MOVEPLAYER          51
#define HOOK_MOVEOBJECT          52
#define HOOK_SETANIMATION        53
#define HOOK_COMMUNICATE         54
#define HOOK_FINDBESTOBJECTMATCH 55
#define HOOK_APPLYBELOW          56
#define HOOK_FREEOBJECT          57
#define HOOK_CLONEOBJECT         58
#define HOOK_TELEPORTOBJECT      59
#define HOOK_LEARNSKILL          60
#define HOOK_FINDMARKEDOBJECT    61
#define HOOK_IDENTIFYOBJECT      62
#define HOOK_CHECKFORSKILLNAME   63
#define HOOK_CHECKFORSKILLKNOWN	 64
#define HOOK_NEWINFOMAPEXCEPT    65
#define HOOK_INSERTOBJECTINOB    66
#define HOOK_FIXPLAYER           67
#define HOOK_PLAYSOUNDMAP        68
#define HOOK_OUTOFMAP            69
#define HOOK_CREATEOBJECT        70
#define HOOK_SHOWCOST            71
#define HOOK_DEPOSIT             72
#define HOOK_WITHDRAW            73
#define HOOK_SWAPAPARTMENTS      74
#define HOOK_PLAYEREXISTS		 75
/*@}*/

/**
 * Number of hooks. This should be the last value in
 * @ref HOOK_xxx + 1. */
#define NR_OF_HOOKS              76

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
#define LIBPTRTYPE void*
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

	/** Plugin CF-funct. hooker function */
	f_plugin hookfunc;

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

/** Used to transmit hook pointers from server to plugin. */
extern MODULEAPI CFParm *registerHook(CFParm *PParm);

/** Called whenever an event occurs. */
extern MODULEAPI CFParm *triggerEvent(CFParm *PParm);
/*@}*/

extern CFPlugin PlugList[32];

extern int PlugNR;

#endif
