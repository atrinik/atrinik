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
 * @author Alex Tokar */

#include <global.h>

/**
 * Signal all linked spawn points on the specified map about a possible enemy.
 * @param op Spawn point that is signalling.
 * @param map Map to signal on.
 */
static void spawn_point_enemy_signal_map(object *op, mapstruct *map)
{
    objectlink *ol;

    for (ol = map->linked_spawn_points; ol != NULL; ol = ol->next) {
        if (ol->objlink.ob != op && ol->objlink.ob->title == op->title &&
            OBJECT_VALID(ol->objlink.ob->enemy, ol->objlink.ob->enemy_count) &&
            !OBJECT_VALID(ol->objlink.ob->enemy->enemy, ol->objlink.ob->enemy->enemy_count)) {
            set_npc_enemy(ol->objlink.ob->enemy, op->enemy->enemy, NULL);
        }
    }
}

/**
 * Signal a possible enemy to all linked spawn points.
 * @param op Spawn point that is signalling. */
void spawn_point_enemy_signal(object *op)
{
    int i;

    /* Spawn point has no map or no title (linked group name), nothing to do. */
    if (op->map == NULL || op->title == NULL) {
        return;
    }

    /* Signal the map the spawn point is on. */
    spawn_point_enemy_signal_map(op, op->map);

    /* Signal all the tiled maps that are in memory. */
    for (i = 0; i < TILED_NUM; i++) {
        if (op->map->tile_map[i] != NULL && op->map->tile_map[i]->in_memory == MAP_IN_MEMORY) {
            spawn_point_enemy_signal_map(op, op->map->tile_map[i]);
        }
    }
}

/**
 * Generate a monster from the spawn point.
 * @param op Spawn point that is generating the monster.
 * @param monster Monster to generate.
 * @return The generated monster, NULL on failure. */
static object *spawn_point_generate(object *op, object *monster)
{
    int i;
    object *tmp;

    i = find_first_free_spot(monster->arch, monster, op->map, op->x, op->y);

    if (i == -1) {
        return NULL;
    }

    tmp = get_object();
    monster->type = MONSTER;
    copy_object(monster, tmp, 0);
    monster->type = SPAWN_POINT_MOB;

    tmp->x = op->x + freearr_x[i];
    tmp->y = op->y + freearr_y[i];

    if (tmp->item_condition) {
        int level, min, max, diff;

        level = MAX(1, MIN(tmp->level, MAXLEVEL));
        diff = op->map->difficulty;

        switch (tmp->item_condition) {
            case SPAWN_RELATIVE_LEVEL_GREEN:
                min = level_color[diff].green;
                max = level_color[diff].blue - 1;
                break;

            case SPAWN_RELATIVE_LEVEL_BLUE:
                min = level_color[diff].blue;
                max = level_color[diff].yellow - 1;
                break;

            case SPAWN_RELATIVE_LEVEL_YELLOW:
                min = level_color[diff].yellow;
                max = level_color[diff].orange - 1;
                break;

            case SPAWN_RELATIVE_LEVEL_ORANGE:
                min = level_color[diff].orange;
                max = level_color[diff].red - 1;
                break;

            case SPAWN_RELATIVE_LEVEL_RED:
                min = level_color[diff].red;
                max = level_color[diff].purple - 1;
                break;

            case SPAWN_RELATIVE_LEVEL_PURPLE:
                min = level_color[diff].purple;
                max = min + 1;
                break;

            default:
                min = level;
                max = min;
        }

        tmp->level = rndm(MAX(level, MIN(min, MAXLEVEL)), MAX(level, MIN(max, MAXLEVEL)));
    }

    if (tmp->randomitems) {
        create_treasure(tmp->randomitems, tmp, 0, tmp->level, T_STYLE_UNSET, ART_CHANCE_UNSET, 0, NULL);
    }

    return tmp;
}

/**
 * Check whether monster can be generated by the spawn point.
 * @param op The spawn point.
 * @param monster The monster to check.
 * @return 1 if the monster can be generated, 0 otherwise. */
static int spawn_point_can_generate(object *op, object *monster)
{
    shstr *spawn_time;

    if (!op->map) {
        return 0;
    }

    spawn_time = object_get_value(monster, "spawn_time");

    /* Check if the time is right for the monster to be spawned. */
    if (spawn_time) {
        int hour, minute, hour2, minute2;

        if (sscanf(spawn_time, "%d:%d - %d:%d", &hour, &minute, &hour2, &minute2) == 4) {
            timeofday_t tod;

            get_tod(&tod);

            /* Same day. */
            if (hour <= hour2) {
                if (tod.hour < hour || tod.hour > hour2) {
                    return 0;
                }
            }
            /* Overnight. */
            else {
                if (tod.hour < hour && tod.hour > hour2) {
                    return 0;
                }
            }

            /* Check minutes. */
            if ((tod.hour == hour && tod.minute < minute) || (tod.hour == hour2 && tod.minute > minute2)) {
                return 0;
            }

            return 1;
        }
        else {
            logger_print(LOG(BUG), "Syntax error in spawn_time attribute: %s", spawn_time);
        }
    }

    return 1;
}

/** @copydoc object_methods::process_func */
static void process_func(object *op)
{
    int total_chance, roll;
    object *tmp, *tmp2, *monster, *copy;

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
    total_chance = 0;

    for (tmp = op->inv; tmp; tmp = tmp->below) {
        if (tmp->type != SPAWN_POINT_MOB) {
            continue;
        }

        total_chance += MAX(1, tmp->enemy_count);
    }

    /* No total chance, this means there are no monsters in this spawn point. */
    if (!total_chance) {
        return;
    }

    /* Decide which monster to generate. */
    roll = rndm(1, total_chance) - 1;

    for (tmp = op->inv; tmp; tmp = tmp->below) {
        if (tmp->type != SPAWN_POINT_MOB) {
            continue;
        }

        roll -= MAX(1, tmp->enemy_count);

        if (roll < 0) {
            break;
        }
    }

    /* Didn't find any monster to generate, or it can't be generated. */
    if (!tmp || !spawn_point_can_generate(op, tmp)) {
        return;
    }

    /* Try to generate the monster. */
    monster = spawn_point_generate(op, tmp);

    if (!monster) {
        return;
    }

    SET_MULTI_FLAG(monster, FLAG_SPAWN_MOB);

    /* Link the generated monster to the spawn point. */
    op->enemy = monster;
    op->enemy_count = monster->count;

    op->last_sp = 0;

    /* Clone the items the base monster had in its inventory, and insert
     * them into the generated monster. */
    for (tmp = tmp->inv; tmp; tmp = tmp->below) {
        /* Process random drops... */
        if (tmp->type == RANDOM_DROP) {
            if (tmp->weight_limit && !rndm_chance(tmp->weight_limit)) {
                continue;
            }

            for (tmp2 = tmp->inv; tmp2; tmp2 = tmp2->below) {
                copy = get_object();
                copy_object_with_inv(tmp2, copy);
                insert_ob_in_ob(copy, monster);
            }
        }
        else {
            copy = get_object();
            copy_object_with_inv(tmp, copy);
            insert_ob_in_ob(copy, monster);
        }
    }

    /* Create spawn info. */
    tmp = arch_to_object(op->other_arch);
    /* Link the spawn point to the spawn info. */
    tmp->owner = op;
    tmp->ownercount = op->count;
    insert_ob_in_ob(tmp, monster);

    /* Insert the generated monster into the map. */
    insert_ob_in_map(monster, op->map, op, 0);
    fix_monster(monster);
}

/** @copydoc object_methods::trigger_func */
static int trigger_func(object *op, object *cause, int state)
{
    (void) cause;
    (void) state;
    process_func(op);

    return OBJECT_METHOD_OK;
}

/** @copydoc object_methods::insert_map_func */
static void insert_map_func(object *op)
{
    objectlink *ol;

    if (op->title == NULL) {
        return;
    }

    ol = get_objectlink();
    ol->objlink.ob = op;

    objectlink_link(&op->map->linked_spawn_points, NULL, NULL, op->map->linked_spawn_points, ol);
}

/** @copydoc object_methods::remove_map_func */
static void remove_map_func(object *op)
{
    objectlink *ol;

    for (ol = op->map->linked_spawn_points; ol; ol = ol->next) {
        if (ol->objlink.ob == op) {
            objectlink_unlink(&op->map->linked_spawn_points, NULL, ol);
            break;
        }
    }
}

/**
 * Initialize the spawn point type object methods. */
void object_type_init_spawn_point(void)
{
    object_type_methods[SPAWN_POINT].process_func = process_func;
    object_type_methods[SPAWN_POINT].trigger_func = trigger_func;
    object_type_methods[SPAWN_POINT].insert_map_func = insert_map_func;
    object_type_methods[SPAWN_POINT].remove_map_func = remove_map_func;
}
