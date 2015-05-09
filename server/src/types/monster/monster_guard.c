/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
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
 * Handles code for @ref MONSTER "monsters" related to guards.
 *
 * @author Alex Tokar
 */

#ifndef __CPROTO__

#include <global.h>

/**
 * Make the monster activate a gate by applying a lever/switch/etc under
 * the monster's feet.
 * @param op The monster.
 * @param state 1 to activate the gate, 0 to deactivate.
 */
void monster_guard_activate_gate(object *op, int state)
{
    HARD_ASSERT(op != NULL);

    SOFT_ASSERT(op->type == MONSTER, "Object is not a monster: %s",
            object_get_str(op));
    SOFT_ASSERT(op->map != NULL, "Object has no map: %s", object_get_str(op));

    if (!(op->behavior & BEHAVIOR_GUARD)) {
        return;
    }

    FOR_MAP_PREPARE(op->map, op->x, op->y, tmp) {
        if (tmp->type != TYPE_HANDLE || tmp->slaying == NULL) {
            continue;
        }

        if (tmp->state == state) {
            continue;
        }

        object_apply(tmp, op, 0);

        if (state == 1) {
            char buf[HUGE_BUF];
            snprintf(VS(buf), "%s shouts:\nThe gate, get the gate! Let no one "
                    "leave or enter until this matter is resolved!", op->name);
            draw_info_map(CHAT_TYPE_GAME, NULL, COLOR_NAVY, op->map, op->x,
                    op->y, MAP_INFO_NORMAL, NULL, NULL, buf);
        }

        break;
    } FOR_MAP_FINISH();
}

#endif
