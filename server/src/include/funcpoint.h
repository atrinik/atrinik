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

#ifndef FUNCPOINT_H
#define FUNCPOINT_H

/* Some function types */
typedef int  (*type_container_unlink_func) 			(player *, object *);
typedef void (*type_move_apply_func) 				(object *, object *, object *, int);
typedef void (*type_func_int)						(int);
typedef void (*type_func_int_int)					(int, int);
typedef void (*type_func_void)						(void);
typedef void (*type_func_map)						(mapstruct *);
typedef void (*type_func_map_char)					(mapstruct *, const char *);
typedef void (*type_func_int_map_int_int_int_char)	(int, mapstruct *, int, int, int, const char *);
typedef void (*type_func_ob)						(object *);
typedef void (*type_func_ob_char)					(object *, char *);
typedef void (*type_func_ob_cchar)					(object *, const char *);
typedef void (*type_func_int_int_ob_cchar)			(int, int, object *, const char *);
typedef void (*type_func_ob_ob)						(object *, object *);
typedef void (*type_func_ob_int)					(object *, int);
typedef int (*type_int_func_ob_ob)					(object *, object *);
typedef void (*type_func_char_int)					(char *, int);
typedef void (*type_func_int_ob_ob)					(int, object *, object *);
typedef void (*type_func_player_int_ob)				(player *, int, object *);
typedef void (*type_func_dragon_gain)				(object *who, int atnr, int level);

/* These function-pointers are defined in common/glue.c
 * The functions used to set and initialise them are also there. */
extern int	(*container_unlink_func)				(player *, object *);
extern void	(*move_apply_func)						(object *, object *, object *, int);
extern void	(*draw_info_func)						(int, int, object *, const char *);
extern void	(*emergency_save_func)					(int);
extern void	(*init_blocksview_players_func)			();
extern void	(*monster_check_apply_func)				(object *, object *);
extern void	(*remove_friendly_object_func)			(object *);
extern void	(*update_buttons_func)					(mapstruct *);
extern void	(*info_map_func)						(int, mapstruct *, int, int, int, const char *);
extern void	(*move_teleporter_func)					(object *);
extern void	(*move_firewall_func)					(object *);
extern void	(*move_creator_func)					(object *);
extern void (*trap_adjust_func)						(object *, int);
extern void	(*esrv_send_item_func)					(object *, object *);
extern void	(*esrv_del_item_func)					(player *, int, object *);
extern void	(*esrv_update_item_func)				(int, object *, object *);
extern void (*dragon_gain_func)             		(object *, int, int);
extern void	(*send_golem_control_func) 				(object *, int);

#endif
