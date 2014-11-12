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
 * Socket API. */

#ifndef SOCKET_H
#define SOCKET_H

/**
 * Commands used for sending data from client to server. */
enum
{
    SERVER_CMD_CONTROL,
    SERVER_CMD_ASK_FACE,
    SERVER_CMD_SETUP,
    SERVER_CMD_VERSION,
    /** @deprecated */
    SERVER_CMD_REQUEST_FILE,
    SERVER_CMD_CLEAR,
    SERVER_CMD_REQUEST_UPDATE,
    SERVER_CMD_KEEPALIVE,
    SERVER_CMD_ACCOUNT,
    SERVER_CMD_ITEM_EXAMINE,
    SERVER_CMD_ITEM_APPLY,
    SERVER_CMD_ITEM_MOVE,
    /** @deprecated */
    SERVER_CMD_REPLY,
    SERVER_CMD_PLAYER_CMD,
    SERVER_CMD_ITEM_LOCK,
    SERVER_CMD_ITEM_MARK,
    SERVER_CMD_FIRE,
    SERVER_CMD_QUICKSLOT,
    SERVER_CMD_QUESTLIST,
    SERVER_CMD_MOVE_PATH,
    /** @deprecated */
    SERVER_CMD_ITEM_READY,
    SERVER_CMD_TALK,
    SERVER_CMD_MOVE,
    SERVER_CMD_TARGET,

    SERVER_CMD_NROF
};

/**
 * All the possible socket commands. */
enum
{
    CLIENT_CMD_MAP,
    CLIENT_CMD_DRAWINFO,
    CLIENT_CMD_FILE_UPDATE,
    CLIENT_CMD_ITEM,
    CLIENT_CMD_SOUND,
    CLIENT_CMD_TARGET,
    CLIENT_CMD_ITEM_UPDATE,
    CLIENT_CMD_ITEM_DELETE,
    CLIENT_CMD_STATS,
    CLIENT_CMD_IMAGE,
    CLIENT_CMD_ANIM,
    /** @deprecated */
    CLIENT_CMD_SKILL_READY,
    CLIENT_CMD_PLAYER,
    CLIENT_CMD_MAPSTATS,
    /** @deprecated */
    CLIENT_CMD_SKILL_LIST,
    CLIENT_CMD_VERSION,
    CLIENT_CMD_SETUP,
    CLIENT_CMD_CONTROL,
    /** @deprecated */
    CLIENT_CMD_DATA,
    CLIENT_CMD_CHARACTERS,
    CLIENT_CMD_BOOK,
    CLIENT_CMD_PARTY,
    CLIENT_CMD_QUICKSLOT,
    CLIENT_CMD_COMPRESSED,
    CLIENT_CMD_REGION_MAP,
    CLIENT_CMD_SOUND_AMBIENT,
    CLIENT_CMD_INTERFACE,
    CLIENT_CMD_NOTIFICATION,

    CLIENT_CMD_NROF
};

#define MAP_UPDATE_CMD_SAME 0
#define MAP_UPDATE_CMD_NEW 1
#define MAP_UPDATE_CMD_CONNECTED 2

/**
 * @defgroup CMD_TARGET_xxx Target command types
 * Target command types; informs the client about whether the target is a
 * friend, enemy, etc.
 *@{*/
/** Self (the player). */
#define CMD_TARGET_SELF 0
/** Enemy. */
#define CMD_TARGET_ENEMY 1
/** Friend. */
#define CMD_TARGET_FRIEND 2
/*@}*/

/**
 * @defgroup CMD_INTERFACE_xxx Interface command types
 * Interface command types.
 *@{*/
/** Text; the NPC message contents. */
#define CMD_INTERFACE_TEXT 0
/**
 * Link, follows the actual text, but is a special command in order to
 * support link shortcuts. */
#define CMD_INTERFACE_LINK 1
/** Icon; the image in the upper left corner square. */
#define CMD_INTERFACE_ICON 2
/** Title; text next to the icon. */
#define CMD_INTERFACE_TITLE 3
/**
 * If found in the command, will open the console with any text followed
 * by this. */
#define CMD_INTERFACE_INPUT 4
/*@}*/

/**
 * @defgroup CMD_NOTIFICATION_xxx Notification command types
 * Notification command types.
 *@{*/
/** The notification contents. */
#define CMD_NOTIFICATION_TEXT 0
/** What macro or command to execute. */
#define CMD_NOTIFICATION_ACTION 1
/** Macro temporarily assigned to this notification. */
#define CMD_NOTIFICATION_SHORTCUT 2
/**
 * How many milliseconds must pass before the notification is
 * dismissed. */
#define CMD_NOTIFICATION_DELAY 3
/*@}*/

/**
 * @defgroup CMD_SETUP_xxx Setup command types
 * Setup command types
 *@{*/
/** Enable/disable sound. */
#define CMD_SETUP_SOUND 0
/**  Set the map size. */
#define CMD_SETUP_MAPSIZE 1
/** Enable/disable bot flag. */
#define CMD_SETUP_BOT 2
/** URL of the data files to use. */
#define CMD_SETUP_DATA_URL 3
/*@}*/

/**
 * @defgroup CMD_QUERY_xxx Query command types
 * Query command types.
 *@{*/
/** Get character name. */
#define CMD_QUERY_GET_NAME 1
/** Get character password. */
#define CMD_QUERY_GET_PASSWORD 2
/** Confirm password for character creation. */
#define CMD_QUERY_CONFIRM_PASSWORD 3
/*@}*/

/**
 * @defgroup MAP2_FLAG_xxx Map2 layer flags
 * Flags used to mark what kind of data there is on layers
 * in map2 protocol.
 *@{*/
/** Multi-arch object. */
#define MAP2_FLAG_MULTI      1
/** Player name. */
#define MAP2_FLAG_NAME       2
/** Animation instead of a face. */
#define MAP2_FLAG_ANIMATION  4
/** Tile's Z position. */
#define MAP2_FLAG_HEIGHT     8
/** Zoom. */
#define MAP2_FLAG_ZOOM 16
/** X align. */
#define MAP2_FLAG_ALIGN 32
/** Draw the object twice. */
#define MAP2_FLAG_DOUBLE 64
/** More flags from @ref MAP2_FLAG2_xxx. */
#define MAP2_FLAG_MORE 128
/*@}*/

/**
 * @defgroup MAP2_FLAG2_xxx Extended map2 layer flags
 * Extended flags used to mark what kind of data there is on layers
 * in map2 protocol.
 *@{*/
/** Custom alpha value. */
#define MAP2_FLAG2_ALPHA 1
/** Custom rotate value in degrees. */
#define MAP2_FLAG2_ROTATE 2
/** The object should be highlighted in red. */
#define MAP2_FLAG2_INFRAVISION 4
/** Possible target. */
#define MAP2_FLAG2_TARGET 8
/** Target's HP bar. */
#define MAP2_FLAG2_PROBE 16
/*@}*/

/**
 * @defgroup MAP2_FLAG_EXT_xxx Map2 tile flags
 * Flags used to mark what kind of data there is on different
 * tiles in map2 protocol.
 *@{*/
/** An animation. */
#define MAP2_FLAG_EXT_ANIM   1
/*@}*/

/**
 * @defgroup ANIM_xxx Animation types
 * Animation types.
 *@{*/
/** Damage animation. */
#define ANIM_DAMAGE     1
/** Kill animation. */
#define ANIM_KILL       2
/*@}*/

/**
 * @defgroup MAP2_MASK_xxx Map2 mask flags
 * Flags used for masks in map2 protocol.
 *@{*/
/** Clear cell, with all layers. */
#define MAP2_MASK_CLEAR      0x2
/** Add darkness. */
#define MAP2_MASK_DARKNESS   0x4
/*@}*/

/**
 * @defgroup MAP2_LAYER_xxx Map2 layer types
 *@{*/
/** Clear this layer. */
#define MAP2_LAYER_CLEAR    255
/*@}*/

/**
 * @defgroup CMD_MAPSTATS_xxx Mapstats command types
 * Mapstats command types.
 *@{*/
/** Change map name. */
#define CMD_MAPSTATS_NAME 1
/** Change map music. */
#define CMD_MAPSTATS_MUSIC 2
/** Change map weather. */
#define CMD_MAPSTATS_WEATHER 3
/** Text animation. */
#define CMD_MAPSTATS_TEXT_ANIM 4
/*@}*/

/**
 * @defgroup CMD_SOUND_xxx Sound command types
 * The sound command types.
 *@{*/
/** A sound effect, like poison, melee/range hit, spell sound, etc. */
#define CMD_SOUND_EFFECT 1
/** Background music. */
#define CMD_SOUND_BACKGROUND 2
/** Path to sound effect with an absolute filename. MIDI is not supported. */
#define CMD_SOUND_ABSOLUTE 3
/*@}*/

/**
 * @defgroup CMD_TARGET_xxx Target command types
 * Target command types.
 *@{*/
/**
 * Target something at the specified X/Y position on the map. */
#define CMD_TARGET_MAPXY 1
/**
 * Clear target. */
#define CMD_TARGET_CLEAR 2
/*@}*/

/**
 * @defgroup CMD_ACCOUNT_xxx Account command types
 * Account command types.
 *@{*/
/**
 * Login as the specified account. */
#define CMD_ACCOUNT_LOGIN 1
/**
 * Create the specified account. */
#define CMD_ACCOUNT_REGISTER 2
/**
 * Login with a character. */
#define CMD_ACCOUNT_LOGIN_CHAR 3
/**
 * Create a new character. */
#define CMD_ACCOUNT_NEW_CHAR 4
/**
 * Change the current account's password. */
#define CMD_ACCOUNT_PSWD 5
/*@}*/

/**
 * @defgroup COLOR_xxx Color HTML notations
 * HTML notations of various common collors.
 *@{*/
/** White. */
#define COLOR_WHITE "ffffff"
/** Orange. */
#define COLOR_ORANGE "ff9900"
/** Navy (most used for NPC messages). */
#define COLOR_NAVY "00ffff"
/** Red. */
#define COLOR_RED "ff3030"
/** Green. */
#define COLOR_GREEN "00ff00"
/** Blue. */
#define COLOR_BLUE "0080ff"
/** Gray. */
#define COLOR_GRAY "999999"
/** Brown. */
#define COLOR_BROWN "c07f40"
/** Purple. */
#define COLOR_PURPLE "cc66ff"
/** Pink. */
#define COLOR_PINK "ff9999"
/** Yellow. */
#define COLOR_YELLOW "ffff33"
/** Dark navy. */
#define COLOR_DK_NAVY "00c4c2"
/** Dark green. */
#define COLOR_DK_GREEN "006600"
/** Dark orange. */
#define COLOR_DK_ORANGE "ff6600"
/** Bright purple. */
#define COLOR_BRIGHT_PURPLE "ff66ff"
/** Gold. */
#define COLOR_HGOLD "d4d553"
/** Dark gold. */
#define COLOR_DGOLD "999900"
/** Black. */
#define COLOR_BLACK "000000"
/*@}*/

/**
 * @defgroup NDI_xxx New draw info flags
 * Various flags for draw_info().
 *@{*/
/** Say command. */
#define NDI_SAY 0x0100
/** The message is a shout. */
#define NDI_SHOUT 0x0200
/** The message is a tell. */
#define NDI_TELL 0x0400
/** This message comes from a player. */
#define NDI_PLAYER 0x0800
/** Message is an emote command. */
#define NDI_EMOTE 0x01000
/**
 * Message will be played as animation in the middle of the client
 * screen. */
#define NDI_ANIM 0x02000
/** Inform all players of this message. */
#define NDI_ALL 0x20000
/*@}*/

/**
 * @defgroup CS_FLAG_xxx Client/server item flags
 * Flags for the item command. Used in query_flags().
 *@{*/
/**
 * The item is applied. */
#define CS_FLAG_APPLIED 1
/**
 * The item is unpaid. */
#define CS_FLAG_UNPAID 2
/**
 * The item is magical. */
#define CS_FLAG_IS_MAGICAL 4
/**
 * The item is cursed. */
#define CS_FLAG_CURSED 8
/**
 * The item is damned. */
#define CS_FLAG_DAMNED 16
/**
 * The item is an opened container. */
#define CS_FLAG_CONTAINER_OPEN 32
/**
 * The item is locked. */
#define CS_FLAG_LOCKED 64
/**
 * The item is trapped. */
#define CS_FLAG_IS_TRAPPED 128
/**
 * The item is a two-handed weapon. */
#define CS_FLAG_WEAPON_2H 256
/*@}*/

/**
 * @defgroup UPD_XXX Item update flags
 * Client/server item update flags.
 *@{*/
/** Update ID location where this object is. */
#define UPD_LOCATION 1
/** Update object's flags. */
#define UPD_FLAGS 2
/** Update object's weight. */
#define UPD_WEIGHT 4
/** Update object's face. */
#define UPD_FACE 8
/** Update object's name. */
#define UPD_NAME 16
/** Update object's animation. */
#define UPD_ANIM 32
/** Update object's animation speed. */
#define UPD_ANIMSPEED 64
/** Update object's nrof. */
#define UPD_NROF 128
/** Update object's facing direction. */
#define UPD_DIRECTION 256
/** Update object's type, subtype, qua/con, level and skill. */
#define UPD_TYPE 512
/** Extra data, such as spell/skill data. */
#define UPD_EXTRA 1024
/*@}*/

/**
 * @defgroup CS_STAT_xx Client/server stats
 * IDs for the various stats that get sent across.
 *@{*/
#define CS_STAT_HP              1
#define CS_STAT_MAXHP           2
#define CS_STAT_SP              3
#define CS_STAT_MAXSP           4
#define CS_STAT_STR             5
#define CS_STAT_DEX             6
#define CS_STAT_CON             7
#define CS_STAT_INT             8
#define CS_STAT_POW             9
#define CS_STAT_EXP             11
#define CS_STAT_LEVEL           12
#define CS_STAT_WC              13
#define CS_STAT_AC              14
#define CS_STAT_DAM             15
#define CS_STAT_ARMOUR          16
#define CS_STAT_SPEED           17
#define CS_STAT_FOOD            18
#define CS_STAT_WEAPON_SPEED    19
#define CS_STAT_RANGE           20
#define CS_STAT_TITLE           21
#define CS_STAT_FLAGS           25
#define CS_STAT_WEIGHT_LIM      26
#define CS_STAT_REG_HP          28
#define CS_STAT_REG_MANA        29
#define CS_STAT_TARGET_HP       31

#define CS_STAT_GENDER 35
#define CS_STAT_ACTION_TIME 36
#define CS_STAT_RANGED_DAM 37
#define CS_STAT_RANGED_WC 38
#define CS_STAT_RANGED_WS 39
#define CS_STAT_PATH_ATTUNED 40
#define CS_STAT_PATH_REPELLED 41
#define CS_STAT_PATH_DENIED 42

#define CS_STAT_RESIST_START    100
#define CS_STAT_RESIST_END      117
#define CS_STAT_RES_PHYS        100
#define CS_STAT_RES_MAG         101
#define CS_STAT_RES_FIRE        102
#define CS_STAT_RES_ELEC        103
#define CS_STAT_RES_COLD        104
#define CS_STAT_RES_CONF        105
#define CS_STAT_RES_ACID        106
#define CS_STAT_RES_DRAIN       107
#define CS_STAT_RES_GHOSTHIT    108
#define CS_STAT_RES_POISON      109
#define CS_STAT_RES_SLOW        110
#define CS_STAT_RES_PARA        111
#define CS_STAT_TIME            112
#define CS_STAT_RES_FEAR        113
#define CS_STAT_RES_DEPLETE     114
#define CS_STAT_RES_DEATH       115
#define CS_STAT_RES_HOLYWORD    116
#define CS_STAT_RES_BLIND       117

#define CS_STAT_EQUIP_START 100
#define CS_STAT_EQUIP_END 115

#define CS_STAT_PROT_START 130
#define CS_STAT_PROT_END 149
/*@}*/

/**
 * @defgroup cs_state_flags Client/server state flags
 * These are used with @ref CS_STAT_FLAGS to determine the
 * server thinks the fire-on and run-on states are.
 *@{*/
#define SF_FIREON           1
#define SF_RUNON            2
#define SF_BLIND            4
#define SF_XRAYS            8
#define SF_INFRAVISION      16
/*@}*/

/**
 * @defgroup FLOAT_MULTx Float multiplication
 * These are multiplication values that should be used when changing
 * floats to ints, and vice versa.
 *@{*/
/** Integer representation (float to int) */
#define FLOAT_MULTI 100000
/** Float representation (int to float) */
#define FLOAT_MULTF 100000.0
/*@}*/

/**
 * @defgroup map_face_flags Map face flags
 * These are the 'face flags' we grab out of the flags object structure
 * 1:1.
 *
 * We use a macro to get them from the object, doing it fast AND to mask the
 * bigger
 * object flags to uint8.
 *@{*/

/** Object is sleeping */
#define FFLAG_SLEEP     0x01
/** Object is confused */
#define FFLAG_CONFUSED  0x02
/** Object is paralyzed */
#define FFLAG_PARALYZED 0x04
/** Object is scared - it will run away */
#define FFLAG_SCARED    0x08
/** Object is blinded */
#define FFLAG_BLINDED   0x10
/** Object is invisible (can be seen with "see invisible" on) */
#define FFLAG_INVISIBLE 0x20
/** Object is ethereal */
#define FFLAG_ETHEREAL  0x40
/** Object is probed */
#define FFLAG_PROBE     0x80
/*@}*/

/**
 * @defgroup anim_flags Animation flags
 * Used to indicate what stage the animation is in.
 *@{*/
#define ANIM_FLAG_MOVING 0x01 ///< Moving.
#define ANIM_FLAG_ATTACKING 0x02 ///< Attacking.
#define ANIM_FLAG_STOP_MOVING 0x04 ///< Stop moving.
#define ANIM_FLAG_STOP_ATTACKING 0x08 ///< Stop attacking.
/*@}*/

#define CHAT_TYPE_ALL 1
#define CHAT_TYPE_GAME 2
#define CHAT_TYPE_CHAT 3
#define CHAT_TYPE_LOCAL 4
#define CHAT_TYPE_PRIVATE 5
#define CHAT_TYPE_GUILD 6
#define CHAT_TYPE_PARTY 7
#define CHAT_TYPE_OPERATOR 8

#define CMD_TALK_NPC 1
#define CMD_TALK_INV 2
#define CMD_TALK_BELOW 3
#define CMD_TALK_CONTAINER 4
#define CMD_TALK_NPC_NAME 5

/**
 * @defgroup CMD_CONTROL_xxx Control command types
 *@{*/
/**
 * Control command concerning a map. */
#define CMD_CONTROL_MAP 1
/**
 * Control command concerning a player. */
#define CMD_CONTROL_PLAYER 2
/*@}*/


/**
 * @defgroup CMD_CONTROL_MAP_xxx Map control command types
 *@{*/
/**
 * Reset the specified map. */
#define CMD_CONTROL_MAP_RESET 1
/*@}*/

/**
 * @defgroup CMD_CONTROL_PLAYER_xxx Player control command types
 *@{*/
/**
 * Teleport player to the specified map. */
#define CMD_CONTROL_PLAYER_TELEPORT 1
/*@}*/

/**
 * Player equipment.
 * @anchor PLAYER_EQUIP_xxx */
enum
{
    /**
     * Ammunition. */
    PLAYER_EQUIP_AMMO,
    /**
     * Amulet. */
    PLAYER_EQUIP_AMULET,
    /**
     * Weapon. */
    PLAYER_EQUIP_WEAPON,
    /**
     * Shield. */
    PLAYER_EQUIP_SHIELD,
    /**
     * Gauntlets. */
    PLAYER_EQUIP_GAUNTLETS,
    /**
     * Right ring. */
    PLAYER_EQUIP_RING_RIGHT,
    /**
     * Helm. */
    PLAYER_EQUIP_HELM,
    /**
     * Armor. */
    PLAYER_EQUIP_ARMOUR,
    /**
     * Belt. */
    PLAYER_EQUIP_BELT,
    /**
     * Greaves. */
    PLAYER_EQUIP_GREAVES,
    /**
     * Boots. */
    PLAYER_EQUIP_BOOTS,
    /**
     * Cloak. */
    PLAYER_EQUIP_CLOAK,
    /**
     * Bracers. */
    PLAYER_EQUIP_BRACERS,
    /**
     * Ranged weapon. */
    PLAYER_EQUIP_WEAPON_RANGED,
    /**
     * Light (lantern, torch). */
    PLAYER_EQUIP_LIGHT,
    /**
     * Left ring. */
    PLAYER_EQUIP_RING_LEFT,

    /**
     * Maximum number of equipment. */
    PLAYER_EQUIP_MAX
};

typedef struct socket_t
{
    /**
     * Actual socket handle, as returned by socket() call. */
    int handle;

    /**
     * Hostname that the socket connection will use.
     */
    char *host;

    /**
     * Port that the socket connection will use.
     */
    uint16 port;

    /**
     * SSL socket handle.
     */
    SSL *ssl_handle;
} socket_t;

#endif
