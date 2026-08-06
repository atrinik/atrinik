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

#ifndef MOVEMENT_H
#define MOVEMENT_H

#include <decls.h>

/** Move an object with itself as the originator. */
#define move_object(__op, __dir) move_ob((__op), (__dir), (__op))

/**
 * @file
 * Public declarations for the corresponding server module.
 */

/** Public API implemented in src/server/move.c. */

extern int get_random_dir(void);

extern int get_randomized_dir(int dir);

extern int object_move_to(object *op, int dir, object *originator, mapstruct *m, int x, int y);

extern int move_ob(object *op, int dir, object *originator);

extern int transfer_ob(object *op, int x, int y, int randomly, object *originator, object *trap);

extern int push_ob(object *op, int dir, object *pusher);

#endif
