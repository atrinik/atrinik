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
 * Header file for things that are generally used in many places. */

#ifndef MAIN_H
#define MAIN_H

#define COLOR_BUF 7

#define SDL_DEFAULT_REPEAT_INTERVAL 30

/* For hash table (bmap, ...) */
#define MAXSTRING 20

/** The servers list, as given by the metaserver. */
typedef struct server_struct
{
    /** Next server in the list. */
    struct server_struct *next;

    /** Previous server in the list. */
    struct server_struct *prev;

    /** IP of the server. */
    char *ip;

    /** Name of the server. */
    char *name;

    /** Server version. */
    char *version;

    /** Server description. */
    char *desc;

    /** Number of players online. */
    int player;

    /** Server port. */
    int port;
} server_struct;

/**
 * Message animation structure. Used when NDI_ANIM is passed to
 * DrawInfoCmd2(). */
typedef struct msg_anim_struct
{
    /** The message to play. */
    char message[MAX_BUF];

    /** Tick when it started. */
    uint32 tick;

    /** Color of the message animation. */
    char color[COLOR_BUF];
} msg_anim_struct;

#define FILE_ATRINIK_P0 "data/atrinik.p0"

/* Face requested from server - do it only one time */
#define FACE_REQUESTED      16

typedef struct _face_struct
{
    /* Our face data. if != null, face is loaded */
    struct sprite_struct *sprite;

    /* Our face name. if != null, face is requested */
    char *name;

    /* Checksum of face */
    uint32 checksum;

    int flags;
}_face_struct;

#define NUM_STATS 7

typedef struct spell_entry_struct
{
    /**
     * The spell object in player's inventory. */
    object *spell;

    /**
     * Cost of spell. */
    uint16 cost;

    /**
     * Spell's flags. */
    uint32 flags;

    /**
     * Spell's path. */
    uint32 path;

    /**
     * Description of the spell. */
    char msg[MAX_BUF];
} spell_entry_struct;

/**
 * Maximum number of spell paths. The last one is always 'all' and holds
 * pointers to spells in the other spell paths. */
#define SPELL_PATH_NUM 21

typedef struct skill_entry_struct
{
    object *skill;

    uint8 level;

    sint64 exp;
} skill_entry_struct;

/** Fire mode structure */
typedef struct _fire_mode
{
    /** Item */
    int item;

    /** Ammunition */
    int amun;

    spell_entry_struct *spell;

    /** Skill */
    skill_entry_struct *skill;

    /** Name */
    char name[128];
}_fire_mode;

/**
 * A single help file entry. */
typedef struct hfile_struct
{
    char *key;

    char *msg;

    size_t msg_len;

    uint8 autocomplete;

    uint8 autocomplete_wiz;

    UT_hash_handle hh;
} hfile_struct;

/**
 * Player's state. */
typedef enum player_state_t
{
    /**
     * Just initialized the client. */
    ST_INIT,

    /**
     * Re-download metaserver list. */
    ST_META,

    /**
     * Close opened socket if any, prepare for connection. */
    ST_START,

    /**
     * Waiting to select a server to play on. */
    ST_WAITLOOP,

    /**
     * Selected a server, so start the connection procedure. */
    ST_STARTCONNECT,

    /**
     * Open a connection to the server. */
    ST_CONNECT,

    /**
     * Wait for version information from the server. */
    ST_WAITVERSION,

    /**
     * Server version received. */
    ST_VERSION,

    /**
     * Wait for setup command from the server. */
    ST_WAITSETUP,

    /**
     * Request files listing. */
    ST_REQUEST_FILES_LISTING,

    /**
     * Wait for files listing request to complete. */
    ST_WAITREQUEST_FILES_LISTING,

    /**
     * Request files as necessary. */
    ST_REQUEST_FILES,

    /**
     * Choosing which account to login with. */
    ST_LOGIN,

    /**
     * Wait for login response. */
    ST_WAITLOGIN,

    /**
     * Choosing which character to play with. */
    ST_CHARACTERS,

    /**
     * Waiting for the relevant data packets to start playing. */
    ST_WAITFORPLAY,

    /**
     * Playing. */
    ST_PLAY
} player_state_t;

/* With this, we overrule bitmap loading params.
 * For example, we need for fonts an attached palette, and not the native vid
 * mode */

/** Use this when you want a colkey in a true color picture - color should be 0
 * */
#define SURFACE_FLAG_COLKEY_16M 2
#define SURFACE_FLAG_DISPLAYFORMAT 4
#define SURFACE_FLAG_DISPLAYFORMATALPHA 8

/* For custom cursors */
enum
{
    MSCURSOR_MOVE = 1
};

#define IS_ENTER(_keysym) ((_keysym) == SDLK_RETURN || (_keysym) == SDLK_KP_ENTER)
#define IS_NEXT(_keysym) ((_keysym) == SDLK_TAB || IS_ENTER((_keysym)))

#endif
