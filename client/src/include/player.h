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
 * Player related header file. */

#ifndef PLAYER_H
#define PLAYER_H

/**
 * Contains information about the maximum level the server supports, and
 * the experience needed to reach every single level. */
typedef struct _server_level
{
	int level;
	uint32 exp[500];
} _server_level;

/** IDs of the player doll items. */
typedef enum _player_doll_enum
{
	PDOLL_ARMOUR,
	PDOLL_HELM,
	PDOLL_GIRDLE,
	PDOLL_BOOT,
	PDOLL_RHAND,
	PDOLL_LHAND,
	PDOLL_RRING,
	PDOLL_LRING,
	PDOLL_BRACER,
	PDOLL_AMULET,
	PDOLL_SKILL,
	PDOLL_WAND,
	PDOLL_BOW,
	PDOLL_GAUNTLET,
	PDOLL_ROBE,
	PDOLL_LIGHT,

	/* Must be last element */
	PDOLL_INIT
}_player_doll_enum;

/** Player doll item position structure. */
typedef struct _player_doll_pos
{
	/** X position. */
	int xpos;

	/** Y position. */
	int ypos;
}_player_doll_pos;

extern _server_level server_level;

extern void clear_player();
extern void new_player(long tag, char *name, long weight, short face);
extern void new_char(struct _server_char *nc);
extern void client_send_apply(int tag);
extern void client_send_examine(int tag);
extern void client_send_move(int loc, int tag, int nrof);
extern void send_command(const char *command, int repeat, int must_send);
extern void CompleteCmd(unsigned char *data, int len);
extern void set_weight_limit(uint32 wlim);
extern void init_player_data();
extern void widget_show_player_doll(int x, int y);
extern void widget_player_stats(int x, int y);
extern void widget_show_main_lvl(int x, int y);
extern void widget_show_skill_exp(int x, int y);
extern void widget_show_regeneration(int x, int y);
extern void widget_skillgroups(int x, int y);
extern void widget_menubuttons(int x, int y);
extern void widget_menubuttons_event(int x, int y);
extern void widget_skill_exp_event();
extern void widget_player_data_event(int x, int y);
extern void widget_show_player_doll_event();
extern void widget_show_player_data(int x, int y);

#endif
