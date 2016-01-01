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
 * Handles code for @ref SPAWN_POINT "spawn points".
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <monster_guard.h>
#include <arch.h>
#include <object.h>
#include <exp.h>
#include <object_methods.h>

/**
 * Generate a monster from the spawn point.
 *
 * @param op
 * Spawn point that is generating the monster.
 * @param monster
 * Monster to generate.
 * @return
 * The generated monster, NULL on failure.
 */
static object *
spawn_point_generate (object *op, object *monster)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(monster != NULL);

    int i = find_first_free_spot(monster->arch, monster, op->map, op->x, op->y);
    if (i == -1) {
        return NULL;
    }

    object *tmp = get_object();
    monster->type = MONSTER;
    copy_object(monster, tmp, 0);
    monster->type = SPAWN_POINT_MOB;

    tmp->x = op->x + freearr_x[i];
    tmp->y = op->y + freearr_y[i];

    if (tmp->item_condition != 0) {
        int level = MAX(1, MIN(tmp->level, MAXLEVEL));
        int min, max;
        switch (tmp->item_condition) {
        case SPAWN_RELATIVE_LEVEL_GREEN:
            min = level_color[op->map->difficulty].green;
            max = level_color[op->map->difficulty].blue - 1;
            break;

        case SPAWN_RELATIVE_LEVEL_BLUE:
            min = level_color[op->map->difficulty].blue;
            max = level_color[op->map->difficulty].yellow - 1;
            break;

        case SPAWN_RELATIVE_LEVEL_YELLOW:
            min = level_color[op->map->difficulty].yellow;
            max = level_color[op->map->difficulty].orange - 1;
            break;

        case SPAWN_RELATIVE_LEVEL_ORANGE:
            min = level_color[op->map->difficulty].orange;
            max = level_color[op->map->difficulty].red - 1;
            break;

        case SPAWN_RELATIVE_LEVEL_RED:
            min = level_color[op->map->difficulty].red;
            max = level_color[op->map->difficulty].purple - 1;
            break;

        case SPAWN_RELATIVE_LEVEL_PURPLE:
            min = level_color[op->map->difficulty].purple;
            max = min + 1;
            break;

        default:
            min = level;
            max = min;
        }

        tmp->level = rndm(MAX(level, MIN(min, MAXLEVEL)),
                          MAX(level, MIN(max, MAXLEVEL)));
    }

    if (tmp->randomitems != NULL) {
        create_treasure(tmp->randomitems,
                        tmp,
                        0,
                        tmp->level,
                        T_STYLE_UNSET,
                        ART_CHANCE_UNSET,
                        0,
                        NULL);
    }

    return tmp;
}

/**
 * Check whether monster can be generated by the spawn point.
 *
 * @param op
 * The spawn point.
 * @param monster
 * The monster to check.
 * @return
 * True if the monster can be generated, false otherwise.
 */
static bool
spawn_point_can_generate (object *op, object *monster)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(monster != NULL);

    if (op->map == NULL) {
        return false;
    }

    /* Check if the time is right for the monster to be spawned. */
    shstr *spawn_time = object_get_value(monster, "spawn_time");
    if (spawn_time != NULL) {
        int hour, minute, hour2, minute2;
        if (sscanf(spawn_time,
                   "%d:%d - %d:%d",
                   &hour,
                   &minute,
                   &hour2,
                   &minute2) == 4) {
            timeofday_t tod;
            get_tod(&tod);

            if (hour <= hour2) {
                /* Same day. */
                if (tod.hour < hour || tod.hour > hour2) {
                    return false;
                }
            } else {
                /* Overnight. */
                if (tod.hour < hour && tod.hour > hour2) {
                    return false;
                }
            }

            /* Check minutes. */
            if ((tod.hour == hour && tod.minute < minute) ||
                (tod.hour == hour2 && tod.minute > minute2)) {
                return false;
            }

            return true;
        } else {
            LOG(ERROR,
                "Syntax error in spawn_time attribute: %s, monster: %s",
                spawn_time,
                object_get_str(monster));
        }
    }

    return true;
}

/** @copydoc object_methods_t::process_func */
static void
process_func (object *op)
{
    HARD_ASSERT(op != NULL);

    /* See if the spawn point should get a chance to do its processing. */
    if (op->last_sp != -1 && op->last_grace && !rndm_chance(op->last_grace)) {
        return;
    }

    /* If the spawn point already has a generated monster, check whether
     * the generated monster is still allowed to be spawned. If not,
     * remove it and proceed normally. */
    if (OBJECT_VALID(op->enemy, op->enemy_count)) {
        if (spawn_point_can_generate(op, op->enemy)) {
            return;
        }

        object_remove(op->enemy, 0);
        object_destroy(op->enemy);
        op->enemy = NULL;
    }

    /* Calculate the total chance. */
    int total_chance = 0;
    FOR_INV_PREPARE(op, tmp) {
        if (tmp->type != SPAWN_POINT_MOB) {
            continue;
        }

        total_chance += MAX(1, tmp->enemy_count);
    } FOR_INV_FINISH();

    /* No total chance, this means there are no monsters in this
     * spawn point. */
    if (total_chance == 0) {
        return;
    }

    /* Decide which monster to generate. */
    int roll = rndm(0, total_chance - 1);
    object *spawn_monster = NULL;
    FOR_INV_PREPARE(op, tmp) {
        if (tmp->type != SPAWN_POINT_MOB) {
            continue;
        }

        roll -= MAX(1, tmp->enemy_count);
        if (roll < 0) {
            spawn_monster = tmp;
            break;
        }
    } FOR_INV_FINISH();

    /* Didn't find any monster to generate, or it can't be generated. */
    if (spawn_monster == NULL || !spawn_point_can_generate(op, spawn_monster)) {
        return;
    }

    /* Try to generate the monster. */
    object *monster = spawn_point_generate(op, spawn_monster);
    if (monster == NULL) {
        return;
    }

    SET_MULTI_FLAG(monster, FLAG_SPAWN_MOB);

    /* Link the generated monster to the spawn point. */
    op->enemy = monster;
    op->enemy_count = monster->count;

    op->last_sp = 0;

    /* Clone the items the base monster had in its inventory, and insert
     * them into the generated monster. */
    FOR_INV_PREPARE(spawn_monster, tmp) {
        if (tmp->type == RANDOM_DROP) {
            if (tmp->weight_limit != 0 && !rndm_chance(tmp->weight_limit)) {
                continue;
            }

            FOR_INV_PREPARE(tmp, tmp2) {
                object *copy = get_object();
                copy_object_with_inv(tmp2, copy);
                insert_ob_in_ob(copy, monster);
            } FOR_INV_FINISH();
        } else {
            object *copy = get_object();
            copy_object_with_inv(tmp, copy);
            insert_ob_in_ob(copy, monster);
        }
    } FOR_INV_FINISH();

    /* Create spawn info. */
    object *tmp = arch_to_object(op->other_arch);
    /* Link the spawn point to the spawn info. */
    tmp->owner = op;
    tmp->ownercount = op->count;
    insert_ob_in_ob(tmp, monster);

    /* Insert the generated monster into the map. */
    monster = insert_ob_in_map(monster, op->map, op, 0);
    SOFT_ASSERT(monster != NULL,
                "Failed to insert monster, spawn point: %s",
                object_get_str(op));
    living_update_monster(monster);
    monster_guard_activate_gate(monster, 0);
}

/** @copydoc object_methods_t::trigger_func */
static int
trigger_func (object *op, object *cause, int state)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(cause != NULL);

    process_func(op);
    return OBJECT_METHOD_OK;
}

/** @copydoc object_methods_t::insert_map_func */
static void
insert_map_func (object *op)
{
    HARD_ASSERT(op != NULL);

    objectlink *ol = get_objectlink();
    ol->objlink.ob = op;

    objectlink_link(&op->map->linked_spawn_points,
                    NULL,
                    NULL,
                    op->map->linked_spawn_points,
                    ol);
}

/** @copydoc object_methods_t::remove_map_func */
static void
remove_map_func (object *op)
{
    HARD_ASSERT(op != NULL);

    for (objectlink *ol = op->map->linked_spawn_points;
         ol != NULL;
         ol = ol->next) {
        if (ol->objlink.ob == op) {
            objectlink_unlink(&op->map->linked_spawn_points, NULL, ol);
            free_objectlink_simple(ol);
            break;
        }
    }
}

/**
 * Initialize the spawn point type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(spawn_point)
{
    OBJECT_METHODS(SPAWN_POINT)->process_func = process_func;
    OBJECT_METHODS(SPAWN_POINT)->trigger_func = trigger_func;
    OBJECT_METHODS(SPAWN_POINT)->insert_map_func = insert_map_func;
    OBJECT_METHODS(SPAWN_POINT)->remove_map_func = remove_map_func;
}
