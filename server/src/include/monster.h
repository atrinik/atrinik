/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
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

#ifndef MONSTER_H
#define MONSTER_H

#include <decls.h>

/**
 * @file
 * Public declarations for the corresponding server module.
 */

/** Public API implemented in src/types/monster.c. */

extern void set_npc_enemy(object *npc, object *enemy, rv_vector *rv);

extern void monster_enemy_signal(object *npc, object *enemy);

extern object *check_enemy(object *npc, rv_vector *rv);

extern object *find_enemy(object *npc, rv_vector *rv);

extern int talk_to_npc(object *op, object *npc, char *txt);

extern int is_friend_of(object *op, object *obj);

extern int check_good_weapon(object *who, object *item);

extern int check_good_armour(object *who, object *item);

extern _Bool monster_is_ally_of(object *op, object *target);

extern void monster_drop_arrows(object *op);

#endif
