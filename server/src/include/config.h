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
 * This file contains various #defines that select various options.
 * Some may not be desirable, and some just may not work.
 *
 * In theory, most of the values here should just be defaults, and
 * everything here should just be selectable by different run time
 * flags. However, for some things, that would just be too messy.
 */

/** Location of plugins */
#ifndef PLUGINDIR
#define PLUGINDIR "./"
#endif


/** Where bans are stored. */
#ifndef BANFILE
#define BANFILE "bans"
#endif


#define BALSL_LOSS_CHANCE_RATIO 4
#define BALSL_NUMBER_LOSSES_RATIO 6
#define BALSL_MAX_LOSS_RATIO 2

/**
 * This determines the maximum map size the client can request (and
 * thus what the server will send to the client.
 *
 * Client can still request a smaller map size (for bandwidth reasons
 * or display size of whatever else).
 *
 * The larger this number, the more CPU time and memory the server will
 * need to spend to figure this out in addition to bandwidth needs.
 * The server CPU time should be pretty trivial.
 */
#define MAP_CLIENT_X 17
#define MAP_CLIENT_Y 17

/**
 * If you feel the game is too fast or too slow, change MAX_TIME.
 * You can experiment with the /speed \<new_max_time\> command first.
 *
 * The length of a tick is MAX_TIME microseconds. During a tick,
 * players, monsters, or items with speed 1 can do one thing.
 */
#define MAX_TIME 125000

/**
 * Multiplier for important operations like socket reading, writing, sending
 * the map, etc.
 *
 * Increasing this will increase the server's CPU usage, but is recommended for
 * remote servers, or players connecting from remote locations to reduce the
 * artificial delay of 125ms between each server tick.
 */
#define MAX_TIME_MULTIPLIER 1

/* If you get a complaint about O_NDELAY not being known/undefined, try
 * uncommenting this.
 * This may cause problems - O_NONBLOCK will return -1 on blocking writes
 * and set error to EAGAIN.  O_NDELAY returns 0.  This is only if no bytes
 * can be written - otherwise, the number of bytes written will be returned
 * for both modes. */
/*
 * #define O_NDELAY O_NONBLOCK
 */

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
 * old map is still in memory, so don't need to go disk and get it.
 */
#define MAP_DEFAULTTIMEOUT 500
/**
 * Maximum map timeout value.
 */
#define MAP_MAXTIMEOUT 10000

/**
 * MAP_MAXRESET is the maximum time a map can have before being reset. It
 * will override the time value set in the map, if that time is longer than
 * MAP_MAXRESET. This value is in seconds. If you are low on space on the
 * TMPDIR device, set this value to something small.
 *
 * The default value in the map object is 7200 (2 hours).
 *
 * Comment out MAP_MAXRESET time if you always want to use the value
 * in the map archetype.
 */
#define MAP_MAXRESET 7200
/** Default time to reset. */
#define MAP_DEFAULTRESET 7200

/**
 * These define the players starting map and location on that map, and where
 * emergency saves are defined. This should be left as is unless you make
 * major changes to the map.
 */
#define EMERGENCY_MAPPATH "/emergency"
#define EMERGENCY_X 0
#define EMERGENCY_Y 0

/** How big steps to use when expanding object array. */
#define OBJECT_EXPAND 2500

/** How many entries there is room for. */
#define HIGHSCORE_LENGTH 1000

#define MAXSTRING 20

/**
 * This is the access rights for the players savefiles.
 *
 * I think it is usefull to restrict access to the savefiles for the
 * game admin. So if you make Atrinik set-uid, use 0600.
 *
 * If you are running the game set-gid (to a games-group, for instance),
 * you must remember to make it writeable for the group (ie 0660).
 */
#define SAVE_MODE   0660

/**
 * How often (in seconds) the player is saved if he drops things. If it is
 * set to 0, the player will be saved for every item he drops. Otherwise,
 * if the player drops an item, and the last time he was saved due to item
 * drop is longer the SAVE_INTERVAL seconds, he is then saved. Depending on
 * your playing environment, you may want to set this to a higher value, so
 * that you are not spending too much time saving the characters.
 */
/*#define SAVE_INTERVAL 300*/

/**
 * AUTOSAVE saves the player every AUTOSAVE ticks. A value of
 * 5000 with MAX_TIME set at 120,000 means that the player will be
 * saved every 10 minutes. Some effort should probably be made to
 * spread out these saves, but that might be more effort than it is
 * worth (depending on the spacing, if enough players log on, the spacing
 * may not be large enough to save all of them). As it is now, it will
 * just set the base tick of when they log on, which should keep the
 * saves pretty well spread out (in a fairly random fashion).
 */
#define AUTOSAVE 5000

/** Socket version. */
#define SOCKET_VERSION 1066

/**
 * If 1, all data packets that are longer than @ref COMPRESS_DATA_PACKETS_SIZE
 * will be compressed by zlib using the best compression available before
 * being sent. The client will then decompress the previously compressed
 * data and treat it normally.
 *
 * This option is useful for bandwidth-limited servers, as it has nearly
 * no CPU impact, but halves the bandwidth usage of most common data
 * packets.
 */
#define COMPRESS_DATA_PACKETS 0

/**
 * Only data packets longer than this will be compressed if @ref
 * COMPRESS_DATA_PACKETS
 * is 1. This option shouldn't be too high, since in such case it would have
 * little effect, and not too low either, in which case it would do more
 * harm than good.
 *
 * The default of '128' is a reasonable value, as such data packets will
 * get compressed by zlib nicely (a data packet of size 20 would actually
 * be bigger when compressed).
 */
#define COMPRESS_DATA_PACKETS_SIZE 128
