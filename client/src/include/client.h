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
 * Various defines. */

#ifndef CLIENT_H
#define CLIENT_H

/* How many skill types server supports/client will get sent to it.
 * If more skills are added to server, this needs to get increased. */
#define MAX_SKILL   6

#define INPUT_MODE_NO		0
#define INPUT_MODE_CONSOLE	1
#define INPUT_MODE_NUMBER	2

#define NUM_MODE_GET  1
#define NUM_MODE_DROP 2

typedef struct Animations
{
	/* 0 = all fields are invalid, 1 = anim is loaded */
	int loaded;

	/* Length of one a animation frame (num_anim / facings) */
	int frame;
	uint16 *faces;

	/* Number of frames */
	uint8 facings;

	/* Number of animations. Value of 2 means
	 * only faces[0], [1] have meaningful values. */
	uint8 num_animations;
	uint8 flags;
} Animations;

typedef struct _anim_table
{
	/* Length of anim_cmd data */
	size_t len;

	/* Faked animation command */
	uint8 *anim_cmd;
}_anim_table;

/**
 * Timeout when attempting a connection in milliseconds. */
#define SOCKET_TIMEOUT_MS 4000

/**
 * One command buffer. */
typedef struct command_buffer
{
	/** Next command in queue. */
	struct command_buffer *next;

	/** Previous command in queue. */
	struct command_buffer *prev;

	/** Length of the data. */
	int len;

	/** The data. */
	uint8 data[1];
} command_buffer;

/* ClientSocket could probably hold more of the global values - it could
 * probably hold most all socket/communication related values instead
 * of globals. */
typedef struct ClientSocket
{
	int fd;
} ClientSocket;

typedef struct Stat_struct
{
	sint8 Str, Dex, Con, Wis, Cha, Int, Pow;

	/** Weapon class. */
	sint16 wc;

	/** Armour class. */
	sint16 ac;

	/** Level. */
	uint32 level;

	/** Hit points. */
	sint32 hp;

	/** Max hit points */
	sint32 maxhp;

	/** Spell points. */
	sint32 sp;

	/** Max spell points. */
	sint32 maxsp;

	/** Grace. */
	sint32 grace;

	/** Max grace. */
	sint32 maxgrace;

	/** Total experience. */
	sint64 exp;

	/** How much food in stomach. */
	sint16 food;

	/** How much damage the player does when hitting. */
	sint16 dam;

	/** Player's speed; gets converted to a float for display. */
	sint32 speed;

	/** Weapon speed; gets converted to a float for display. */
	int weapon_sp;

	/** Contains fire on/run on flags. */
	uint16 flags;

	/** Protections. */
	sint16 protection[20];

	/** Ranged weapon damage. */
	sint16 ranged_dam;

	/** Ranged weapon wc. */
	sint16 ranged_wc;

	/** Ranged weapon speed. */
	sint32 ranged_ws;
} Stats;

/** The player structure. */
typedef struct Player_Struct
{
	/** Player object. */
	object *ob;

	/** Items below the player (pl.below->inv). */
	object *below;

	/** Inventory of an open container. */
	object *sack;

	/** Tag of the open container. */
	sint32 container_tag;

	/** Player's weight limit. */
	uint32 weight_limit;

	/** Are we a DM? */
	int dm;

	/** Target mode. */
	int	target_mode;

	/** Target. */
	int	target_code;

	/** Target's color. */
	char target_color[COLOR_BUF];

	/** Target name. */
	char target_name[MAX_BUF];

	int loc;
	int tag;
	int nrof;

	/** Readied skill. */
	skill_entry_struct *skill;

	int warn_hp;

	/* Input mode: no, console (textstring), numinput */
	int input_mode;
	int	nummode;

	/** Currently marked item. */
	int mark_count;

	/** HP regeneration. */
	float gen_hp;

	/** Mana regeneration. */
	float gen_sp;

	/** Grace regeneration. */
	float gen_grace;

	/** Skill cooldown time. */
	float action_timer;

	/** 1 if fire key is pressed. */
	uint8 fire_on;

	/** 1 if run key is on. */
	uint8 run_on;

	/** Player's carrying weight. */
	float real_weight;

	int warn_statdown;
	int warn_statup;

	/** Player stats. */
	Stats stats;

	/** HP of our target in percent. */
	char target_hp;

	/** Player's name. */
	char name[40];

	/** Player's password. Only used while logging in. */
	char password[40];

	char num_text[300];
	char skill_name[128];

	/** Rank and name of char. */
	char ext_title[MAX_BUF];

	/** Party name this player is member of. */
	char partyname[MAX_BUF];

	/**
	 * Buffer for party name the player is joining, but has to enter
	 * password first. */
	char partyjoin[MAX_BUF];

	/** Information about the object/spell being dragged. */
	union
	{
		/** ID of the object being dragged. */
		int tag;

		/** Spell being dragged. */
		spell_entry_struct *spell;
	} dragging;

	/** Which inventory widget has the focus. */
	int inventory_focus;

	/** Version of the server's socket. */
	int server_socket_version;

	size_t target_object_index;

	uint8 target_is_friend;

	/**
	 * Player's gender. */
    uint8 gender;
} Client_Player;

/** Check if the keyword represents a true value. */
#define KEYWORD_IS_TRUE(_keyword) (!strcmp((_keyword), "yes") || !strcmp((_keyword), "on") || !strcmp((_keyword), "true"))
/** Check if the keyword represents a false value. */
#define KEYWORD_IS_FALSE(_keyword) (!strcmp((_keyword), "no") || !strcmp((_keyword), "off") || !strcmp((_keyword), "false"))

/** Copies information from one color structure into another. */
#define SDL_color_copy(_color, _color2) \
{ \
	(_color)->r = (_color2)->r; \
	(_color)->g = (_color2)->g; \
	(_color)->b = (_color2)->b; \
}

typedef struct socket_command_struct
{
	void (*handle_func)(uint8 *data, size_t len, size_t pos);
} socket_command_struct;

#endif
