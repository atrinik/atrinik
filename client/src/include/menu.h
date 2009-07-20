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

#if !defined(__MENU_H)
#define __MENU_H

#define MENU_NO       1
#define MENU_KEYBIND  2
#define MENU_SPELL    4
#define MENU_SKILL    8
#define MENU_OPTION   16
#define MENU_CREATE   32
#define MENU_BOOK     64
#define MENU_PARTY    128

#define MENU_ALL (MENU_NO & MENU_KEYBIND & MENU_SPELL & MENU_OPTION & MENU_BOOK & MENU_PARTY)

#define MENU_SOUND_VOL 40
struct _skill_list skill_list[SKILL_LIST_MAX];
extern _dialog_list_set skill_list_set;

/* Skill list entries */
struct _spell_list spell_list[SPELL_LIST_MAX];
extern _dialog_list_set spell_list_set;

extern _dialog_list_set option_list_set;

struct _bindkey_list bindkey_list[BINDKEY_LIST_MAX];
extern _dialog_list_set bindkey_list_set;

extern _dialog_list_set create_list_set;
extern int keybind_status;

#define MAX_QUICK_SLOTS 		8
#define MAX_QUICKSLOT_GROUPS	4
typedef struct _quickslot
{
	/* Do we have an item or a spell in quickslot */
	int spell;

	int invSlot;
	int nr;

	/* What item/spellNr in quickslot */
	int tag;

	int spellNr;
	int groupNr;
	int classNr;
}_quickslot;

int quickslot_group;

extern _quickslot quick_slots[MAX_QUICK_SLOTS * MAX_QUICKSLOT_GROUPS];

extern void do_console();
extern void do_number();
extern void show_number(int x, int y);
extern void show_console(int x, int y);
extern void widget_show_resist(int x, int y);
extern void show_keybind();
extern void show_spelllist();
extern void show_skilllist();
extern void show_help(char *helpfile);

extern void show_menu();
extern void show_media(int x, int y);
extern void show_range(int x, int y);
extern int init_media_tag(char *tag);
extern void blt_inventory_face_from_tag(int tag, int x, int y);
extern void blt_window_slider(_Sprite *slider, int max_win, int winlen, int off, int len, int x, int y);
extern void do_keybind_input();

extern int read_anim_tmp();
extern int read_bmap_tmp();
extern void read_anims();
extern void read_bmaps_p0();
extern void delete_bmap_tmp();
extern void read_bmaps();
extern void delete_server_chars();
extern void load_settings();
extern void read_settings();
extern void read_spells();
extern void read_skills();
extern void read_help_files();
extern int blt_face_centered(int face, int x, int y);
extern int get_quickslot(int x, int y);
extern void show_quickslots(int x, int y);
extern void update_quickslots(int del_item);

extern void widget_quickslots_mouse_event(int x, int y, int MEvent);
extern void widget_range_event(int x, int y, SDL_Event event, int MEvent);
extern void widget_event_target(int x, int y);
extern void widget_number_event(int x, int y);
extern void widget_quickslots(int x, int y);
extern void widget_show_range(int x, int y);
extern void widget_show_target(int x, int y);
extern void widget_show_mapname(int x, int y);
extern void widget_show_console(int x, int y);
extern void widget_show_number(int x, int y);

extern int client_command_check(char *cmd);

#endif
