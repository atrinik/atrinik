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
 * This file contains various #defines that select various options.
 * Some may not be desirable, and some just may not work.
 *
 * In theory, most of the values here should just be defaults, and
 * everything here should just be selectable by different run time
 * flags. However, for some things, that would just be too messy. */

/** Location of read-only machine independent data. */
#ifndef DATADIR
#define DATADIR "./lib"
#endif

/** Location of the map files. */
#ifndef MAPDIR
#define MAPDIR  "../maps"
#endif

/** Location of changeable single system data (temp maps, etc). */
#ifndef LOCALDIR
#define LOCALDIR "./data"
#endif

/** Player directory. */
#ifndef PLAYERDIR
#define PLAYERDIR "players"
#endif

/** Location of plugins */
#ifndef PLUGINDIR
#define PLUGINDIR "./plugins"
#endif

/** Where the DM file is located. */
#ifndef DMFILE
#define DMFILE "dmfile"
#endif

/** Where bans are stored. */
#ifndef BANFILE
#define BANFILE "bans"
#endif

/**
 * Your tmp-directory should be large enough to hold the uncompressed
 * map-files for all who are playing.
 *
 * It ought to be locally mounted, since the function used to generate
 * unique temporary filenames isn't guaranteed to work over NFS or AFS.
 *
 * On the other hand, if you know that only one Atrinik server will be
 * running using this temporary directory, it is likely to be safe to use
 * something that is NFS mounted (but performance may suffer as NFS is
 * slower than local disk). */
#ifndef TMPDIR
#define TMPDIR LOCALDIR"/tmp"
#endif

/**
 * Directory to use for unique items. This is placed into the 'lib'
 * directory.  Changing this will cause any old unique items file
 * not to be used. */
#ifndef UNIQUE_DIR
#define UNIQUE_DIR "unique-items"
#endif

/**
 * Use balanced stat loss code?
 * This code is a little more merciful with repeated stat loss at lower
 * levels. Basically, the more stats you have lost, the less likely that
 * you will lose more. Additionally, lower level characters are shown
 * a lot more mercy (there are caps on how much of a stat you can lose too).
 * On the nasty side, if you are higher level, you can lose mutiple stats
 * _at_once_ and are shown less mercy when you die. But when you're higher
 * level, it is much easier to buy back your stats with potions.
 * Turn this on if you want death-based stat loss to be more merciful
 * at low levels and more cruel at high levels.
 * Only works when stats are depleted rather than lost. This option has
 * no effect if you are using genuine stat loss.
 *
 * The BALSL_.. values control this behaviour.
 * BALSL_NUMBER_LOSSES_RATIO determines the number of stats to lose.
 * the character level is divided by that value, and that is how many
 * stats are lost.
 *
 * BALSL_MAX_LOSS_RATIO puts the upper limit on depletion of a stat -
 * basically, level/max_loss_ratio is the most a stat can be depleted.
 *
 * BALSL_LOSS_CHANCE_RATIO controls how likely it is a stat is depleted.
 * The chance not to lose a stat is
 * depleteness^2 / (depletedness^2+ level/ratio).
 * ie, if the stats current depleted value is 2 and the character is level
 * 15, the chance not to lose the stat is 4/(4+3) or 4/7.  The higher the
 * level, the more likely it is a stat can get really depleted, but
 * this gets more offset as the stat gets more depleted. */
#define BALANCED_STAT_LOSS 0
#define BALSL_LOSS_CHANCE_RATIO 4
#define BALSL_NUMBER_LOSSES_RATIO 6
#define BALSL_MAX_LOSS_RATIO 2

/**
 * CS_LOGSTATS will cause the server to log various usage stats
 * (number of connections, amount of data sent, amount of data received,
 * and so on). This can be very useful if you are trying to measure
 * server/bandwidth usage. It will periodially dump out information
 * which contains usage stats for the last X amount of time.
 * CS_LOGTIME is how often it will print out stats. */
#define CS_LOGSTATS 0

#if CS_LOGSTATS
#define CS_LOGTIME 600
#endif

/**
 * DEBUG generates copious amounts of output. By default, you probably
 * don't want this defined. */
#ifndef WIN32
#ifndef DEBUG
#define DEBUG
#endif
#endif

/**
 * This determines the maximum map size the client can request (and
 * thus what the server will send to the client.
 *
 * Client can still request a smaller map size (for bandwidth reasons
 * or display size of whatever else).
 *
 * The larger this number, the more cpu time and memory the server will
 * need to spend to figure this out in addition to bandwidth needs.
 * The server cpu time should be pretty trivial. */
#define MAP_CLIENT_X 17
#define MAP_CLIENT_Y 17

/**
 * If you feel the game is too fast or too slow, change MAX_TIME.
 * You can experiment with the /speed \<new_max_time\> command first.
 *
 * The length of a tick is MAX_TIME microseconds. During a tick,
 * players, monsters, or items with speed 1 can do one thing. */
#define MAX_TIME 125000

/**
 * Calling this REAL_WIZ is probably not really good. Something like
 * MUD_WIZ might be a better name.
 *
 * Basically, if REAL_WIZ is defined then the WIZ/WAS_WIZ flags for objects
 * are not set - instead, wizard created/manipulated objects appear as
 * normal objects.  This makes the wizard a little more mudlike, since
 * object manipulations will be usable for other objects. */
#define REAL_WIZ

/**
 * Set this if you want the temporary maps to be saved and reused across
 * Atrinik runs.  This can be especially useful for single player
 * servers, but even holds use for multiplayer servers.  The file used
 * is updated each time a temp map is updated.
 * Note that the file used to store this information is stored in
 * the LIB directory. Running multiple Atrinik servers with the same LIB
 * directory will cause serious problems, simply because in order for
 * this to really work, the filename must be constant so the next run
 * knows where to find the information. */
#define RECYCLE_TMP_MAPS 0

/**
 * Set this to 0 if you don't want characters to loose a random stat when
 * they die - instead, they just get deplete.
 *
 * Setting it to 1 keeps the old behaviour. This can be
 * changed at run time via -stat_loss_on_death or +stat_loss_on_death.
 * In theory, this can be changed on a running server, but so glue code
 * in the wiz stuff would need to be added for that to happen. */
#define STAT_LOSS_ON_DEATH 0

/* If you get a complaint about O_NDELAY not being known/undefined, try
 * uncommenting this.
 * This may cause problems - O_NONBLOCK will return -1 on blocking writes
 * and set error to EAGAIN.  O_NDELAY returns 0.  This is only if no bytes
 * can be written - otherwise, the number of bytes written will be returned
 * for both modes. */
/*
#define O_NDELAY O_NONBLOCK
*/

/**
 * CSPORT is the port used for the new client/server code. Change
 * if desired. */
#define CSPORT 13327

/**
 * MAP_DEFAULTTIMEOUT is the default number of ticks until a map is swapped out
 * after a player has left it. If it is set to 0, maps are
 * swapped out the instant the last player leaves it.
 * If you are low on memory, you should set this to 0.
 * Note that depending on the map timeout variable, the number of
 * objects can get quite high. This is because depending on the maps,
 * a player could be having the objects of several maps in memory
 * (the map he is in right now, and the ones he left recently).
 *
 * Having a nonzero value can be useful: If a player leaves a map (and thus
 * is on a new map), and realizes they want to go back pretty quickly, the
 * old map is still in memory, so don't need to go disk and get it. */
#define MAP_DEFAULTTIMEOUT 500
/**
 * Maximum map timeout value. */
#define MAP_MAXTIMEOUT 1000

/**
 * MAP_MAXRESET is the maximum time a map can have before being reset. It
 * will override the time value set in the map, if that time is longer than
 * MAP_MAXRESET. This value is in seconds. If you are low on space on the
 * TMPDIR device, set this value to something small.
 *
 * The default value in the map object is 7200 (2 hours).
 *
 * Comment out MAP_MAXRESET time if you always want to use the value
 * in the map archetype. */
#define MAP_MAXRESET 7200
/** Default time to reset. */
#define MAP_DEFAULTRESET 7200

/**
 * If 1, used memory is freed on shutdown, allowing easier memory leak
 * checking with tools like Valgrind. */
#define MEMORY_DEBUG 0

/**
 * If undefined, malloc is always used.
 * It looks like this can be obsoleted.  However, it can be useful to
 * track down some bugs, as it will make sure that the entire data structure
 * is set to 0, at the expense of speed.
 *
 * Rupert Goldie has run Purify against the code, and if this is disabled,
 * apparantly there are a lot of uninitialized memory reads - I haven't
 * seen any problem (maybe the memory reads are copies, and the destination
 * doesn't actually use the garbage values either?), but the impact on speed
 * of using this probably isn't great, and should make things more stable.
 * Msw 8-9-97 */
#define USE_CALLOC

/**
 * These define the players starting map and location on that map, and where
 * emergency saves are defined. This should be left as is unless you make
 * major changes to the map. */
#define EMERGENCY_MAPPATH "/emergency"
#define EMERGENCY_X 0
#define EMERGENCY_Y 0

/**
 * These defines tells where, relative to LIBDIR, the archetypes, highscore and
 * treaures files and directories can be found. */
#define ARCHETYPES "archetypes"
#define TREASURES "treasures"
#define SETTINGS "settings"
/** Where the highscore file is located. */
#define HIGHSCORE "highscore"

/** Bail out if more are received during tick. */
#define MAX_ERRORS 25

/** How big steps to use when expanding object array. */
#define OBJECT_EXPAND 2500

/** How many entries there is room for. */
#define HIGHSCORE_LENGTH 1000

/** Used when hashing archetypes. */
#define ARCHTABLE 8192
#define MAXSTRING 20

/**
 * This is the access rights for the players savefiles.
 *
 * I think it is usefull to restrict access to the savefiles for the
 * game admin. So if you make Atrinik set-uid, use 0600.
 *
 * If you are running the game set-gid (to a games-group, for instance),
 * you must remember to make it writeable for the group (ie 0660). */
#define	SAVE_MODE	0660

/**
 * How often (in seconds) the player is saved if he drops things. If it is
 * set to 0, the player will be saved for every item he drops. Otherwise,
 * if the player drops an item, and the last time he was saved due to item
 * drop is longer the SAVE_INTERVAL seconds, he is then saved. Depending on
 * your playing environment, you may want to set this to a higher value, so
 * that you are not spending too much time saving the characters. */
/*#define SAVE_INTERVAL 300*/

/**
 * AUTOSAVE saves the player every AUTOSAVE ticks. A value of
 * 5000 with MAX_TIME set at 120,000 means that the player will be
 * saved every 10 minutes. Some effort should probably be made to
 * spread out these saves, but that might be more effort than it is
 * worth (depending on the spacing, if enough players log on, the spacing
 * may not be large enough to save all of them). As it is now, it will
 * just set the base tick of when they log on, which should keep the
 * saves pretty well spread out (in a fairly random fashion). */
#define AUTOSAVE 5000

/**
 * Often, emergency save fails because the memory corruption that caused
 * the crash has trashed the characters too. Define NO_EMERGENCY_SAVE
 * to disable emergency saves. */
#define NO_EMERGENCY_SAVE

/** Socket version. */
#define SOCKET_VERSION 1038
