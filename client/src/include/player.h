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
* the Free Software Foundation; either version 3 of the License, or     *
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

#if !defined(__PLAYER_H)
#define __PLAYER_H

typedef struct _server_level {
	int level;
	uint32 exp[500];
} _server_level;

extern _server_level server_level;

extern void CompleteCmd(unsigned char *data, int len);

extern void new_player(long tag, char *name, long weight, short face);
extern void new_char(struct _server_char *nc);
extern void look_at(int x, int y);
extern void client_send_apply(int tag);
extern void client_send_examine(int tag);
extern void client_send_move(int loc, int tag, int nrof);
extern void move_player(int dir);
extern void stop_fire(void);
extern void clear_fire_run(void);
extern void clear_fire(void);
extern void clear_run(void);
extern void fire_dir(int dir);
extern void stop_run(void);
extern void run_dir(int dir);
extern int send_command(const char *command, int repeat, int must_send);
extern char *complete_command(char *command);

extern void init_player_data(void);

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

extern void set_weight_limit(uint32 wlim);
extern void clear_player(void);

#endif
