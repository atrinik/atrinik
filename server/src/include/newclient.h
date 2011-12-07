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
 * This file defines various flags that both the new client and newserver
 * uses.
 *
 * These should never be changed, only expanded. Changing them will
 * likely cause all old clients to not work properly. While called
 * newclient, it is really used by both the client and server to keep
 * some values the same.
 *
 * Name format is CS_(command)_(flag)
 *
 * CS = Client/Server.
 *
 * (command) is protocol command, ie ITEM
 *
 * (flag) is the flag name */

#ifndef NEWCLIENT_H
#define NEWCLIENT_H

/**
 * Maximum size of any packet we expect. Using this makes it so we don't
 * need to allocate and deallocate the same buffer over and over again
 * at the price of using a bit of extra memory.
 *
 * It also makes the code simpler. */
#define MAXSOCKBUF (64 * 1024)

/**
 * Maximum size of socket input buffer we can read/fill when reading from
 * socket. This is raw data until we sort it out and put it in the player
 * command queue. */
#define MAXSOCKBUF_IN (3 * 1024)

/**
 * @defgroup CS_QUERY_xxx Client/server queries
 * Client/server queries
 *@{*/
/** Yes/no question */
#define CS_QUERY_YESNO      0x1
/** Single character response expected */
#define CS_QUERY_SINGLECHAR 0x2
/** Hide input being entered */
#define CS_QUERY_HIDEINPUT  0x4
/*@}*/

/**
 * @defgroup FLOAT_MULTx Float multiplication
 * These are multiplication values that should be used when changing
 * floats to ints, and vice versa.
 *@{*/
/** Integer representation (float to int) */
#define FLOAT_MULTI	100000
/** Float representation (int to float) */
#define FLOAT_MULTF	100000.0
/*@}*/

/**
 * @defgroup CS_STAT_xx Client/server stats
 * IDs for the various stats that get sent across.
 *@{*/
#define CS_STAT_HP	 			1
#define CS_STAT_MAXHP			2
#define CS_STAT_SP	 			3
#define CS_STAT_MAXSP			4
#define CS_STAT_STR	 			5
#define CS_STAT_INT	 			6
#define CS_STAT_WIS	 			7
#define CS_STAT_DEX	 			8
#define CS_STAT_CON	 			9
#define CS_STAT_CHA				10
#define CS_STAT_EXP				11
#define CS_STAT_LEVEL			12
#define CS_STAT_WC				13
#define CS_STAT_AC				14
#define CS_STAT_DAM				15
#define CS_STAT_ARMOUR			16
#define CS_STAT_SPEED			17
#define CS_STAT_FOOD			18
#define CS_STAT_WEAP_SP 		19
#define CS_STAT_RANGE			20
#define CS_STAT_TITLE			21
#define CS_STAT_POW				22
#define CS_STAT_GRACE			23
#define CS_STAT_MAXGRACE		24
#define CS_STAT_FLAGS			25
#define CS_STAT_WEIGHT_LIM		26
#define CS_STAT_EXT_TITLE 		27
#define CS_STAT_REG_HP 			28
#define CS_STAT_REG_MANA 		29
#define CS_STAT_REG_GRACE 		30
#define CS_STAT_TARGET_HP 		31

#define CS_STAT_ACTION_TIME		36
#define CS_STAT_RANGED_DAM 37
#define CS_STAT_RANGED_WC 38
#define CS_STAT_RANGED_WS 39

#define CS_STAT_RESIST_START	100
#define CS_STAT_RESIST_END		117
#define CS_STAT_RES_PHYS		100
#define CS_STAT_RES_MAG			101
#define CS_STAT_RES_FIRE		102
#define CS_STAT_RES_ELEC		103
#define CS_STAT_RES_COLD		104
#define CS_STAT_RES_CONF		105
#define CS_STAT_RES_ACID		106
#define CS_STAT_RES_DRAIN		107
#define CS_STAT_RES_GHOSTHIT	108
#define CS_STAT_RES_POISON		109
#define CS_STAT_RES_SLOW		110
#define CS_STAT_RES_PARA		111
#define CS_STAT_TIME			112
#define CS_STAT_RES_FEAR		113
#define CS_STAT_RES_DEPLETE		114
#define CS_STAT_RES_DEATH		115
#define CS_STAT_RES_HOLYWORD	116
#define CS_STAT_RES_BLIND		117

#define CS_STAT_SKILLEXP_START 		118
#define CS_STAT_SKILLEXP_END 		129
#define CS_STAT_SKILLEXP_AGILITY 	118
#define CS_STAT_SKILLEXP_AGLEVEL 	119
#define CS_STAT_SKILLEXP_PERSONAL 	120
#define CS_STAT_SKILLEXP_PELEVEL 	121
#define CS_STAT_SKILLEXP_MENTAL 	122
#define CS_STAT_SKILLEXP_MELEVEL 	123
#define CS_STAT_SKILLEXP_PHYSIQUE 	124
#define CS_STAT_SKILLEXP_PHLEVEL 	125
#define CS_STAT_SKILLEXP_MAGIC 		126
#define CS_STAT_SKILLEXP_MALEVEL 	127
#define CS_STAT_SKILLEXP_WISDOM 	128
#define CS_STAT_SKILLEXP_WILEVEL 	129

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
 * @defgroup cs_item_flags Client/server item flags
 * Flags for the item command. Used in query_flags().
 *@{*/

/** These flags are used if the item is applied. */
enum
{
	/** No flag. */
	a_none,
	/** The item is readied. */
	a_readied,
	/** The item is wielded. */
	a_wielded,
	/** The item is worn. */
	a_worn,
	/** The item is active. */
	a_active,
	/** The item is applied. */
	a_applied
};

/** @copydoc FLAG_APPLIED */
#define F_APPLIED       0x000F
/** @copydoc FLAG_IS_ETHEREAL */
#define F_ETHEREAL      0x0080
/** @copydoc FLAG_IS_INVISIBLE */
#define F_INVISIBLE     0x0100
/** @copydoc FLAG_UNPAID */
#define F_UNPAID        0x0200
/** @copydoc FLAG_IS_MAGICAL */
#define F_MAGIC         0x0400
/** @copydoc FLAG_CURSED */
#define F_CURSED        0x0800
/** @copydoc FLAG_DAMNED */
#define F_DAMNED        0x1000
/** @copydoc CONTAINER */
#define F_OPEN          0x2000
/** @copydoc FLAG_NO_PICK */
#define F_NOPICK        0x4000
/** @copydoc FLAG_INV_LOCKED */
#define F_LOCKED        0x8000
/** @copydoc FLAG_IS_TRAPPED */
#define F_TRAPPED       0x10000
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
/** If set, do not use object's inventory animation instead of normal animation. */
#define UPD_ANIM_NO_INV 1024
/*@}*/

/**
 * Contains the base information we use to make up a packet we want to
 * send. */
typedef struct SockList
{
	/** Length of the packet */
	int len;

	/** The packet */
	unsigned char *buf;
} SockList;

/** Statistics of server. */
typedef struct CS_Stats
{
	/** Bytes in */
	int	ibytes;

	/** Bytes out */
	int obytes;

	/** Maximum connections received */
	short max_conn;

	/** When we started logging this */
	time_t time_start;
} CS_Stats;

/** Srv client files. */
typedef struct _srv_client_files
{
	/** Compressed file data. */
	char *file;

	/** Compressed file length. */
	size_t len;

	/** Original uncompressed file length */
	size_t len_ucomp;

	/** CRC32 sum. */
	unsigned long crc;
} _srv_client_files;

/** Name of the client spells file. */
#define SRV_FILE_SPELLS_FILENAME "client_spells_v2"
/** Name of the client skills file. */
#define SRV_CLIENT_SKILLS_FILENAME "client_skills_v2"

/** Srv client files. */
enum
{
	SRV_CLIENT_SKILLS,
	SRV_CLIENT_SPELLS,
	SRV_CLIENT_SETTINGS,
	SRV_CLIENT_ANIMS,
	SRV_CLIENT_BMAPS,
	SRV_CLIENT_HFILES,
	SRV_FILE_UPDATES,
	SRV_FILE_SPELLS_V2,
	SRV_SERVER_SETTINGS,
	SRV_CLIENT_ANIMS_V2,
	SRV_CLIENT_EFFECTS,
	SRV_CLIENT_SKILLS_V2,
	SRV_CLIENT_HFILES_V2,
	/* last index */
	SRV_CLIENT_FILES
};

/** Set binary socket command. */
#define SOCKET_SET_BINARY_CMD(__s__, __bc__) \
	(__s__)->buf[0] = __bc__;                \
	(__s__)->len = 1

/**
 * All the possible binary commands.
 *
 * Note that the 0 is reserved to mark compressed data packets that the
 * server sends to client if @ref COMPRESS_DATA_PACKETS is 1 and the data
 * packet has more than @ref COMPRESS_DATA_PACKETS_SIZE bytes. */
enum
{
	BINARY_CMD_COMC = 1,
	BINARY_CMD_MAP2,
	/** @deprecated */
	BINARY_CMD_DRAWINFO_OLD,
	BINARY_CMD_DRAWINFO,
	BINARY_CMD_FILE_UPD,
	BINARY_CMD_ITEMX,
	BINARY_CMD_SOUND,
	BINARY_CMD_TARGET,
	BINARY_CMD_UPITEM,
	BINARY_CMD_DELITEM,
	BINARY_CMD_STATS,
	BINARY_CMD_IMAGE,
	BINARY_CMD_FACE1,
	BINARY_CMD_ANIM,
	BINARY_CMD_SKILLRDY,
	BINARY_CMD_PLAYER,
	BINARY_CMD_MAPSTATS,
	BINARY_CMD_SPELL_LIST,
	BINARY_CMD_SKILL_LIST,
	BINARY_CMD_CLEAR,
	BINARY_CMD_ADDME_SUC,
	BINARY_CMD_ADDME_FAIL,
	BINARY_CMD_VERSION,
	BINARY_CMD_BYE,
	BINARY_CMD_SETUP,
	BINARY_CMD_QUERY,
	BINARY_CMD_DATA,
	BINARY_CMD_NEW_CHAR,
	BINARY_CMD_ITEMY,
	BINARY_CMD_BOOK,
	BINARY_CMD_PARTY,
	BINARY_CMD_QUICKSLOT,
	/** @deprecated. */
	BINARY_CMD_SHOP,
	BINARY_CMD_QLIST,
	BINARY_CMD_REGION_MAP,
	/**
	 * Used to exchange information about readied objects such as arrows,
	 * bolts, quivers, throwing knives, etc. */
	BINARY_CMD_READY,
	BINARY_CMD_KEEPALIVE,
	BINARY_CMD_SOUND_AMBIENT,
	BINARY_CMD_INTERFACE,
	BINARY_CMD_NOTIFICATION,

	/* old, unused or outdated crossfire cmds! */
	BINARY_CMD_MAGICMAP,
	BINARY_CMD_DELINV,
	BINARY_CMD_REPLYINFO,
	BINARY_CMD_IMAGE2,
	BINARY_CMD_FACE,
	BINARY_CMD_FACE2,

	/* last entry */
	BINAR_CMD
};

#define MAP_UPDATE_CMD_SAME 0
#define MAP_UPDATE_CMD_NEW 1
#define MAP_UPDATE_CMD_CONNECTED 2

/**
 * @defgroup CMD_QUICKSLOT_xxx Quickslot commands
 * The various quickslot commands.
 *@{*/
/** Set an item as a quickslot. Uses object::quickslot. */
#define CMD_QUICKSLOT_SET 1
/** Set a spell as a quickslot. Uses player::spell_quickslots. */
#define CMD_QUICKSLOT_SETSPELL 2
/** Unset a quickslot, be it spell or item. */
#define CMD_QUICKSLOT_UNSET 3
/*@}*/

/**
 * @defgroup QUICKSLOT_TYPE_xxx Quickslot data types
 * Quickslot data types.
 *@{*/
/** Item quickslot. */
#define QUICKSLOT_TYPE_ITEM 1
/** Spell quickslot. */
#define QUICKSLOT_TYPE_SPELL 2
/*@}*/

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

#endif
