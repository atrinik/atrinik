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
 * Handles code for @ref MONSTER "monsters" related to monster data.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <monster_data.h>
#include <packet.h>

#ifndef __CPROTO__

static void monster_data_interfaces_free(monster_data_interface_t *interface);

/**
 * Initialize monster data for the specified object.
 * @param op Monster.
 */
void monster_data_init(object *op)
{
    HARD_ASSERT(op != NULL);
    SOFT_ASSERT(op->type == MONSTER, "Object is not a monster: %s",
                object_get_str(op));

    op->custom_attrset = ecalloc(1, sizeof(monster_data_t));
}

/**
 * Deinitialize monster data for the specified object.
 * @param op Monster.
 */
void monster_data_deinit(object *op)
{
    HARD_ASSERT(op != NULL);
    SOFT_ASSERT(op->type == MONSTER, "Object is not a monster: %s",
                object_get_str(op));

    monster_data_t *monster_data = MONSTER_DATA(op);
    SOFT_ASSERT(monster_data != NULL, "Missing monster data for: %s",
                object_get_str(op));

    monster_data_interface_t *interface, *tmp;

    DL_FOREACH_SAFE(monster_data->interfaces, interface, tmp) {
        monster_data_interfaces_free(interface);
    }

    efree(monster_data);
}

/**
 * Update the monster's remembered enemy coordinates.
 * @param op Monster.
 * @param enemy Enemy. Can be NULL, in which case the coordinates will be
 * cleared.
 */
void monster_data_enemy_update(object *op, object *enemy)
{
    HARD_ASSERT(op != NULL);
    SOFT_ASSERT(op->type == MONSTER, "Object is not a monster: %s",
                object_get_str(op));

    monster_data_t *monster_data = MONSTER_DATA(op);
    SOFT_ASSERT(monster_data != NULL, "Missing monster data for: %s",
                object_get_str(op));

    if (enemy == NULL) {
        monster_data->enemy_coords.map = NULL;
    } else {
        monster_data->enemy_coords.x = enemy->x;
        monster_data->enemy_coords.y = enemy->y;
        monster_data->enemy_coords.map = enemy->map;
    }
}

/**
 * Acquire last coordinates the monster's enemy was spotted at.
 * @warning If this function returns true, the out parameters will NOT be
 * modified!
 * @param op Monster.
 * @param[out] map Enemy's map.
 * @param[out] x Enemy's X.
 * @param[out] y Enemy's Y.
 * @return True if the coordinates were acquired, false otherwise.
 */
bool monster_data_enemy_get_coords(object *op, mapstruct **map, uint16_t *x,
        uint16_t *y)
{
    HARD_ASSERT(op != NULL);
    SOFT_ASSERT_RC(op->type == MONSTER, false, "Object is not a monster: %s",
                   object_get_str(op));

    monster_data_t *monster_data = MONSTER_DATA(op);
    SOFT_ASSERT_RC(monster_data != NULL, false, "Missing monster data for: %s",
                   object_get_str(op));

    if (monster_data->enemy_coords.map == NULL) {
        return false;
    }

    *map = monster_data->enemy_coords.map;
    *x = monster_data->enemy_coords.x;
    *y = monster_data->enemy_coords.y;

    return true;
}

/**
 * Frees the data associated with the specified ::monster_data_interface_t.
 * @param interface The interface.
 */
static void monster_data_interfaces_free(monster_data_interface_t *interface)
{
    HARD_ASSERT(interface != NULL);
    efree(interface);
}

/**
 * Verify that the specified interface is still valid (the activator wasn't
 * destroyed, the interface hasn't expired, etc).
 * @param monster_data Monster's data.
 * @param interface Interface to verify.
 * @return True if the interface is still OK, false otherwise.
 */
static bool monster_data_interfaces_verify(monster_data_t *monster_data,
        monster_data_interface_t *interface)
{
    HARD_ASSERT(monster_data != NULL);
    HARD_ASSERT(interface != NULL);

    if (!OBJECT_VALID(interface->ob, interface->count)) {
        goto invalid;
    }

    if (pticks > interface->expire) {
        /* Close the interface for players. */
        if (interface->ob->type == PLAYER) {
            packet_struct *packet = packet_new(CLIENT_CMD_INTERFACE, 32, 0);
            socket_send_packet(&CONTR(interface->ob)->socket, packet);
        }

        goto invalid;
    }

    return true;

invalid:
    DL_DELETE(monster_data->interfaces, interface);
    monster_data_interfaces_free(interface);
    return false;
}

/**
 * Add an interface to the monster's database of open interfaces.
 * @param op Monster.
 * @param activator Who opened the interface with the monster.
 * @param secs Seconds the interface should remain open for.
 * @ref MONSTER_DATA_INTERFACE_TIMEOUT is added to this value automatically.
 */
void monster_data_interfaces_add(object *op, object *activator, uint32_t secs)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(activator != NULL);

    monster_data_t *monster_data = MONSTER_DATA(op);
    SOFT_ASSERT(monster_data != NULL, "Missing monster data for: %s",
                object_get_str(op));

    monster_data_interface_t *interface, *tmp;
    long expire = pticks + (secs + MONSTER_DATA_INTERFACE_TIMEOUT) * MAX_TICKS;

    DL_FOREACH_SAFE(monster_data->interfaces, interface, tmp) {
        if (!monster_data_interfaces_verify(monster_data, interface)) {
            continue;
        }

        if (interface->ob == activator &&
                interface->count == activator->count) {
            interface->expire = expire;
            return;
        }
    }

    interface = ecalloc(1, sizeof(*interface));
    interface->ob = activator;
    interface->count = activator->count;
    interface->expire = expire;
    DL_APPEND(monster_data->interfaces, interface);
}

/**
 * Search through the monster's database of interfaces and remove the interface
 * that was opened by the @p activator.
 *
 * It's NOT an error if there is no interface for the specified object.
 * @param op Monster.
 * @param activator Interface activator to try and remove.
 */
void monster_data_interfaces_remove(object *op, object *activator)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(activator != NULL);

    monster_data_t *monster_data = MONSTER_DATA(op);
    SOFT_ASSERT(monster_data != NULL, "Missing monster data for: %s",
                object_get_str(op));

    monster_data_interface_t *interface, *tmp;

    DL_FOREACH_SAFE(monster_data->interfaces, interface, tmp) {
        if (!monster_data_interfaces_verify(monster_data, interface)) {
            continue;
        }

        if (interface->ob == activator &&
                interface->count == activator->count) {
            DL_DELETE(monster_data->interfaces, interface);
            monster_data_interfaces_free(interface);
            break;
        }
    }
}

/**
 * Determine whether the monster has an interface open with the specified @p
 * activator.
 * @param op Monster.
 * @param activator Interface activator to attempt to find.
 * @return True if there is an interface open with the specified object,
 * false otherwise.
 */
bool monster_data_interfaces_check(object *op, object *activator)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(activator != NULL);

    monster_data_t *monster_data = MONSTER_DATA(op);
    SOFT_ASSERT_RC(monster_data != NULL, false, "Missing monster data for: %s",
                   object_get_str(op));

    monster_data_interface_t *interface, *tmp;

    DL_FOREACH_SAFE(monster_data->interfaces, interface, tmp) {
        if (!monster_data_interfaces_verify(monster_data, interface)) {
            continue;
        }

        if (interface->ob == activator &&
                interface->count == activator->count) {
            return true;
        }
    }

    return false;
}

/**
 * Determine the number of interfaces that the specified monster has open.
 * @param op Monster.
 * @return Number of open interfaces.
 */
size_t monster_data_interfaces_num(object *op)
{
    HARD_ASSERT(op != NULL);

    monster_data_t *monster_data = MONSTER_DATA(op);
    SOFT_ASSERT_RC(monster_data != NULL, 0, "Missing monster data for: %s",
                   object_get_str(op));

    monster_data_interface_t *interface, *tmp;
    size_t num = 0;

    DL_FOREACH_SAFE(monster_data->interfaces, interface, tmp) {
        if (!monster_data_interfaces_verify(monster_data, interface)) {
            continue;
        }

        num++;
    }

    return num;
}

/**
 * Cleanup stale and invalid interfaces for the specified monster. Only executes
 * if @ref MONSTER_DATA_INTERFACE_CLEANUP number of seconds have passed.
 * @param op Monster.
 */
void monster_data_interfaces_cleanup(object *op)
{
    HARD_ASSERT(op != NULL);

    monster_data_t *monster_data = MONSTER_DATA(op);
    SOFT_ASSERT(monster_data != NULL, "Missing monster data for: %s",
                object_get_str(op));

    if (pticks - monster_data->last_cleanup < MONSTER_DATA_INTERFACE_CLEANUP *
            MAX_TICKS) {
        return;
    }

    monster_data->last_cleanup = pticks;

    monster_data_interface_t *interface, *tmp;

    DL_FOREACH_SAFE(monster_data->interfaces, interface, tmp) {
        if (!monster_data_interfaces_verify(monster_data, interface)) {
            continue;
        }

        rv_vector rv;

        if (!get_rangevector(op, interface->ob, &rv, RV_MANHATTAN_DISTANCE) ||
                rv.distance > MONSTER_DATA_INTERFACE_DISTANCE) {
            DL_DELETE(monster_data->interfaces, interface);
            monster_data_interfaces_free(interface);
            break;
        }
    }
}

#endif
