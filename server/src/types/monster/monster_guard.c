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
#include <monster_guard.h>
#include <faction.h>
#include <plugin.h>
#include <monster_data.h>
#include <packet.h>
#include <player.h>
#include <object.h>

/**
 * Make the monster activate a gate by applying a lever/switch/etc under
 * the monster's feet.
 * @param op
 * The monster.
 * @param state
 * 1 to activate the gate, 0 to deactivate.
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

        char buf[HUGE_BUF];

        if (state == 1) {
            snprintf(VS(buf), "%s shouts:\nThe gate, get the gate! Let no one "
                    "leave or enter until this matter is resolved!", op->name);
        } else {
            snprintf(VS(buf), "%s shouts:\nAll clear, open the gate!",
                    op->name);
        }

        draw_info_map(CHAT_TYPE_GAME, NULL, COLOR_NAVY, op->map, op->x, op->y,
                MAP_INFO_NORMAL, NULL, NULL, buf);

        break;
    } FOR_MAP_FINISH();
}

/**
 * Acquire player's bounty.
 * @param op
 * Guard.
 * @param pl
 * Player.
 * @param[out] bounty Where to store the bounty.
 * @return
 * Whether the bounty was successfully acquired.
 */
static bool monster_guard_get_bounty(object *op, player *pl, double *bounty)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(pl != NULL);
    HARD_ASSERT(bounty != NULL);

    shstr *faction_name = object_get_value(op, "faction");

    if (faction_name == NULL) {
        LOG(ERROR, "Monster has no faction: %s", object_get_str(op));
        return false;
    }

    faction_t faction = faction_find(faction_name);

    if (faction == NULL) {
        LOG(ERROR, "Monster has invalid faction '%s': %s", faction_name,
                object_get_str(op));
        return false;
    }

    *bounty = faction_get_bounty(faction, pl);

    /* No bounty, nothing to do. */
    if (*bounty <= 0.0) {
        return false;
    }

    return true;
}

/**
 * Check a potential target's bounty.
 * @param op
 * Guard.
 * @param target
 * Target to check.
 * @param msg
 * What the target is saying to the guard.
 * @param distance
 * How far away the target is.
 * @return
 * Whether the target was stopped.
 */
bool monster_guard_check(object *op, object *target, const char *msg,
        uint32_t distance)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(target != NULL);

    SOFT_ASSERT_RC(op->type == MONSTER, false, "Object is not a monster: %s",
            object_get_str(op));
    SOFT_ASSERT_RC(op->map != NULL, false, "Object has no map: %s",
            object_get_str(op));

    if (!(op->behavior & BEHAVIOR_GUARD)) {
        return false;
    }

    /* Only care about players breaking the law. */
    if (target->type != PLAYER) {
        return false;
    }

    /* Target is too far away for an interface, ignore. */
    if (distance > MONSTER_DATA_INTERFACE_DISTANCE) {
        return false;
    }

    player *pl = CONTR(target);

    /* Player is talking to someone, don't disturb them... yet. */
    if (OBJECT_VALID(pl->talking_to, pl->talking_to_count) &&
            pl->talking_to != op) {
        return false;
    }

    /* Already talking to the guard, skip the beginning of the arrest. */
    if (msg == NULL && pl->talking_to == op) {
        return false;
    }

    double bounty;
    if (!monster_guard_get_bounty(op, pl, &bounty)) {
        return false;
    }

    pl->run_on = false;
    pl->combat = false;
    pl->combat_force = false;

    player_path_clear(pl);
    pl->socket.packet_recv_cmd->len = 0;
    player_set_talking_to(pl, op);

    int ret = trigger_event(EVENT_AI,
                            target,
                            op,
                            NULL,
                            msg != NULL ? msg : "hello",
                            EVENT_AI_GUARD_STOP,
                            0,
                            0,
                            0);
    uint32_t secs = INTERFACE_TIMEOUT(ret);
    monster_data_dialogs_add(op, target, MIN(secs, INTERFACE_TIMEOUT_MAX));

    return true;
}

/**
 * Handle closing an interface for a guard.
 * @param op
 * Guard.
 * @param target
 * Who is closing the interface.
 */
void monster_guard_check_close(object *op, object *target)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(target != NULL);

    SOFT_ASSERT(op->type == MONSTER, "Object is not a monster: %s",
            object_get_str(op));
    SOFT_ASSERT(op->map != NULL, "Object has no map: %s", object_get_str(op));

    if (!(op->behavior & BEHAVIOR_GUARD)) {
        return;
    }

    /* Already have an enemy. */
    if (OBJECT_VALID(op->enemy, op->enemy_count)) {
        return;
    }

    /* Only care about players breaking the law. */
    if (target->type != PLAYER) {
        return;
    }

    player *pl = CONTR(target);
    double bounty;
    if (!monster_guard_get_bounty(op, pl, &bounty)) {
        return;
    }

    set_npc_enemy(op, target, NULL);
    draw_info_format(COLOR_NAVY, target, "%s says:\nThen pay with your blood!",
            op->name);
}

#endif
