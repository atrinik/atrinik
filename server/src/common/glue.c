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

#include <global.h>

/* All this glue is currently needed to connect the game with the
 * server.  I'll try to make the library more able to "stand on it's
 * own legs" later; not done in 5 minutes to separate two parts of
 * the code which were connected, well, can you say "spagetti"? */

type_func_int 						emergency_save_func;
type_func_void 						clean_tmp_files_func;
type_func_ob 						remove_friendly_object_func;
type_func_map 						update_buttons_func;
type_func_int_int_ob_cchar 			draw_info_func;
type_container_unlink_func 			container_unlink_func;
type_move_apply_func 				move_apply_func;
type_func_void 						init_blocksview_players_func;
type_func_int_map_int_int_int_char 	info_map_func;
type_func_ob 						move_teleporter_func;
type_func_ob 						move_firewall_func;
type_func_ob_int 					trap_adjust_func;
type_func_ob 						move_creator_func;
type_func_ob_ob 					esrv_send_item_func;
type_func_player_int_ob 			esrv_del_item_func;
type_func_int_ob_ob 				esrv_update_item_func;
type_func_dragon_gain 				dragon_gain_func;
type_func_ob_int 					send_golem_control_func;


/* Initialise all function-pointers to dummy-functions which will
 * do nothing and return nothing (of value at least).
 * Very healthy to do this when using the library, since
 * function pointers are being called throughout the library, without
 * being checked first.
 * init_library() calls this function. */
void init_function_pointers()
{
	emergency_save_func = 			dummy_function_int;
	clean_tmp_files_func = 			dummy_function;
	remove_friendly_object_func = 	dummy_function_ob;
	update_buttons_func = 			dummy_function_map;
	draw_info_func = 				dummy_draw_info;
	container_unlink_func = 		dummy_container_unlink_func;
	move_apply_func = 				dummy_move_apply_func;
	init_blocksview_players_func = 	dummy_function;
	info_map_func = 				dummy_function_mapstr;
	move_teleporter_func = 			dummy_function_ob;
	move_firewall_func = 			dummy_function_ob;
	trap_adjust_func = 				dummy_function_ob_int;
	move_creator_func = 			dummy_function_ob;
	esrv_send_item_func = 			dummy_function_ob2;
	esrv_del_item_func = 			dummy_function_player_int_ob;
	esrv_update_item_func = 		dummy_function_int_ob_ob;
	dragon_gain_func = 				dummy_function_dragongain;
	send_golem_control_func = 		dummy_function_ob_int;
}

/* Specifies which function to call when there is an emergency save. */
void set_emergency_save(type_func_int addr)
{
	emergency_save_func = addr;
}

/* Specifies which function to call to clean temporary files. */
void set_clean_tmp_files(type_func_void addr)
{
	clean_tmp_files_func = addr;
}

/* Specifies which function to call to remove an object in the
 * linked list of friendly objects. */
void set_remove_friendly_object(type_func_ob addr)
{
	remove_friendly_object_func = addr;
}

/* Specify which function to call to recoordinate all buttons. */
void set_update_buttons(type_func_map addr)
{
	update_buttons_func = addr;
}

/* Specify which function to call to draw text to the window
 * of a player. */
void set_draw_info(type_func_int_int_ob_cchar addr)
{
	draw_info_func = addr;
}

/* Specify which function to call to unlink_container. */
void set_container_unlink(type_container_unlink_func addr)
{
	container_unlink_func = addr;
}

/* Specify which function to call to apply an object. */
void set_move_apply(type_move_apply_func addr)
{
	move_apply_func = addr;
}

/* Specify which functino to call to initialise the blocksview[] array. */
void set_init_blocksview_players(type_func_void addr)
{
	init_blocksview_players_func = addr;
}

void set_info_map(type_func_int_map_int_int_int_char addr)
{
	info_map_func = addr;
}

void set_move_teleporter(type_func_ob addr)
{
	move_teleporter_func = addr;
}

void set_move_firewall(type_func_ob addr)
{
	move_firewall_func = addr;
}

void set_trap_adjust(type_func_ob_int addr)
{
	trap_adjust_func = addr;
}

void set_move_creator(type_func_ob addr)
{
	move_creator_func = addr;
}

void set_esrv_send_item (type_func_ob_ob addr)
{
	esrv_send_item_func = addr;
}

void set_esrv_update_item (type_func_int_ob_ob addr)
{
	esrv_update_item_func = addr;
}

void set_esrv_del_item(type_func_player_int_ob addr)
{
	esrv_del_item_func = addr;
}

void set_dragon_gain_func(type_func_dragon_gain addr)
{
	dragon_gain_func = addr;
}

void set_send_golem_control_func (type_func_ob_int addr)
{
	send_golem_control_func = addr;
}

/* fatal() is meant to be called whenever a fatal signal is intercepted.
 * It will call the emergency_save and the clean_tmp_files functions. */
void fatal(int err)
{
	LOG(llevSystem, "Fatal: Shutdown server. Reason: %s\n", err == llevError ? "Fatal Error" : "BUG flood");

	if (init_done)
	{
		(*emergency_save_func)(0);
		(*clean_tmp_files_func)();
	}

	abort();
	LOG(llevSystem, "Exiting...\n");
	exit(-1);
}

#ifndef __Making_docs__ /* Don't want documentation on these */

/* These are only dummy functions, to avoid having any function-pointers
 * having the possibility of pointing to NULL (or random location),
 * thus I don't have to check the contents of a function-pointer each
 * time I want to jump to it. */
void dummy_function_int(int i)
{
	(void) i;
}

void dummy_function_int_int(int i, int j)
{
	(void) i;
	(void) j;
}

void dummy_function_player_int(player *p, int j)
{
	(void) p;
	(void) j;
}

void dummy_function_player_int_ob(player *p, int c, object *ob)
{
	(void) p;
	(void) c;
	(void) ob;
}

void dummy_function()
{
}

void dummy_function_map(mapstruct *m)
{
	(void) m;
}

void dummy_function_ob(object *ob)
{
	(void) ob;
}

void dummy_function_ob2(object *ob, object *ob2)
{
	(void) ob;
	(void) ob2;
}

int dummy_function_ob2int(object *ob, object *ob2)
{
	(void) ob;
	(void) ob2;
	return 0;
}

void dummy_function_ob_int(object *ob, int i)
{
	(void) ob;
	(void) i;
}

void dummy_function_txtnr(char *txt, int nr)
{
	(void) txt;
	(void) nr;
}

void dummy_draw_info(int a, int b, object *ob, const char *txt)
{
	(void) a;
	(void) b;
	(void) ob;
	(void) txt;
}

void dummy_function_mapstr(int a, mapstruct *map, int x, int y, int dist, const char *str)
{
	(void) a;
	(void) map;
	(void) x;
	(void) y;
	(void) dist;
	(void) str;
}

void dummy_function_int_ob_ob (int n, object *ob, object *ob2)
{
	(void) n;
	(void) ob;
	(void) ob2;
}

int dummy_container_unlink_func (player *ob, object *ob2)
{
	(void) ob;
	(void) ob2;
	return 0;
}

void dummy_move_apply_func (object *ob, object *ob2, object *ob3, int flags)
{
	(void) ob;
	(void) ob2;
	(void) ob3;
	(void) flags;
}

void dummy_function_dragongain (object *ob, int a1, int a2)
{
	(void) ob;
	(void) a1;
	(void) a2;
}

#endif
