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

#if !defined(__MENU_H)
#define __MENU_H

#define MENU_NO       1
#define MENU_KEYBIND  2
#define MENU_STATUS   4
#define MENU_SPELL    8
#define MENU_SKILL   16
#define MENU_OPTION  32
#define MENU_CREATE  64
#define MENU_BOOK    128
#define MENU_PARTY   256

#define MENU_ALL (MENU_NO & MENU_KEYBIND & MENU_SPELL & MENU_STATUS & MENU_OPTION)

#define MENU_SOUND_VOL 40
struct _skill_list skill_list[SKILL_LIST_MAX];
extern _dialog_list_set skill_list_set;

struct _spell_list spell_list[SPELL_LIST_MAX]; /* skill list entries */
extern _dialog_list_set spell_list_set;

extern _dialog_list_set option_list_set;

struct _bindkey_list bindkey_list[BINDKEY_LIST_MAX];
extern _dialog_list_set bindkey_list_set;

extern _dialog_list_set create_list_set;
extern int keybind_status;

#define MAX_QUICK_SLOTS 8
typedef struct _quickslot
{
   Boolean spell; /* do we have an item or a spell in quickslot */
   int invSlot;
   int nr;
   int tag;     /* what item/spellNr in quickslot */
   int spellNr;     /* */
   int groupNr; /* spellgroup */
   int classNr; /* spellclass */
}_quickslot;
extern _quickslot quick_slots[MAX_QUICK_SLOTS];

typedef struct _media_file
{
        char name[256];		/* file name */
        void *data;			/* data buffer */
        int type;			/* what is this? (what loaded in buffer) */
        int p1;				/* parameter 1 */
        int p2;
}_media_file;


typedef enum _media_type
{
	MEDIA_TYPE_NO, MEDIA_TYPE_PNG
}_media_type;

#define MEDIA_MAX 10
#define MEDIA_SHOW_NO -1

extern _media_file media_file[MEDIA_MAX];

extern int media_count;	/* buffered media files */
extern int media_show;
extern int media_show_update ;

extern void do_console(int x, int y);
extern void do_number(int x, int y);
extern void show_number(int x, int y);
extern void show_console(int x, int y);
extern void show_resist(int x, int y);
extern void show_keybind(void);
extern void show_status(void);
extern void show_spelllist(void);
extern void show_skilllist(void);
extern void show_help(char *helpfile);

extern void show_menu(void);
extern void show_media(int x, int y);
extern void show_range(int x, int y);
extern int init_media_tag(char *tag);
extern void blt_inventory_face_from_tag(int tag, int x, int y);
extern int blt_window_slider(_Sprite *slider, int max_win, int winlen, int off, int len, int x, int y);
extern void do_keybind_input(void);

extern int read_anim_tmp(void);
extern int read_bmap_tmp(void);
extern void read_anims(void);
extern void read_bmaps_p0(void);
extern void delete_bmap_tmp(void);
extern void read_bmaps(void);
extern void delete_server_chars(void);
extern void load_settings(void);
extern void read_settings(void);
extern void read_spells(void);
extern void read_skills(void);
extern void read_help_files(void);
extern Boolean blt_face_centered(int face, int x, int y);
extern int get_quickslot(int x, int y);
extern void show_quickslots(int x, int y);
extern void update_quickslots(int del_item);
extern void load_quickslots_entrys();
extern void save_quickslots_entrys();

extern int client_command_check(char *cmd);

extern void show_target(int x, int y);

#endif
