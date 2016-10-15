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
 * @ref ARROW "Arrow" related code.
 */

#include <global.h>
#include <player.h>
#include <object.h>
#include <object_methods.h>
#include <arrow.h>
#include <bow.h>

#include "common/process_treasure.h"

/** @copydoc object_methods_t::ranged_fire_func */
static int
ranged_fire_func (object *op, object *shooter, int dir, double *delay)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(shooter != NULL);

    if (dir == 0) {
        draw_info(COLOR_WHITE, shooter, "You can't throw that at yourself.");
        return OBJECT_METHOD_UNHANDLED;
    }

    if (QUERY_FLAG(op, FLAG_STARTEQUIP)) {
        draw_info(COLOR_WHITE, shooter, "The gods won't let you throw that.");
        return OBJECT_METHOD_UNHANDLED;
    }

    if (op->weight <= 0 || QUERY_FLAG(op, FLAG_NO_DROP)) {
        char *name = object_get_base_name_s(op, shooter);
        draw_info_format(COLOR_WHITE, shooter, "You can't throw %s.", name);
        efree(name);
        return OBJECT_METHOD_UNHANDLED;
    }

    if (QUERY_FLAG(op, FLAG_APPLIED) && OBJECT_CURSED(op)) {
        char *name = object_get_base_name_s(op, shooter);
        draw_info_format(COLOR_WHITE, shooter, "The %s sticks to your hand!",
                         name);
        efree(name);
        return OBJECT_METHOD_UNHANDLED;
    }

    op = object_stack_get_removed(op, 1);

    /* Save original WC, damage and range. */
    op->last_heal = op->stats.wc;
    op->stats.hp = op->stats.dam;
    op->stats.sp = op->last_sp;

    /* Calculate moving speed. */
    int str = shooter->type == PLAYER ? shooter->stats.Str : MAX_STAT / 2;
    op->speed = MIN(1.0f, (speed_bonus[str] + 1.0) / 1.5);

    /* Get the used skill. */
    object *skill = SK_skill(shooter);
    /* If we got the skill, add in the skill's modifiers. */
    if (skill != NULL) {
        /* Add WC. */
        op->stats.wc += skill->last_heal;
        /* Add tiles range. */
        op->last_sp += skill->last_sp;
    }

    op->stats.dam += op->magic;
    op->stats.wc += SK_level(shooter);
    op->stats.wc += op->magic;

    if (shooter->type == PLAYER) {
        op->stats.dam += dam_bonus[shooter->stats.Str] / 2;
        op->stats.wc += wc_bonus[shooter->stats.Dex];
    } else {
        op->stats.wc += 5;
    }

    double dam = op->stats.dam * LEVEL_DAMAGE(SK_level(shooter));
    if (op->item_quality) {
        dam *= op->item_condition / 100.0;
    }
    op->stats.dam = MAX(1.0, dam);

    if (delay != NULL) {
        *delay = op->last_grace;
    }

    op = object_projectile_fire(op, shooter, dir);
    if (op == NULL) {
        return OBJECT_METHOD_OK;
    }

    if (shooter->type == PLAYER) {
        CONTR(shooter)->stat_missiles_thrown++;
    }

    play_sound_map(shooter->map,
                   CMD_SOUND_EFFECT,
                   "throw.ogg",
                   shooter->x,
                   shooter->y,
                   0,
                   0);
    return OBJECT_METHOD_OK;
}

/** @copydoc object_methods_t::process_treasure_func */
static int
process_treasure_func (object              *op,
                       object             **ret,
                       int                  difficulty,
                       treasure_affinity_t *affinity,
                       int                  flags)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(difficulty > 0);

    /* Avoid processing if the item is not special. */
    if (!process_treasure_is_special(op)) {
        return OBJECT_METHOD_UNHANDLED;
    }

    /* Only handle adding a slaying race for arrows of assassination or
     * slaying. */
    if (op->slaying != shstr_cons.none) {
        return OBJECT_METHOD_UNHANDLED;
    }

    ob_race *race = race_get_random();
    if (race != NULL) {
        FREE_AND_COPY_HASH(op->slaying, race->name);
    }

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the arrow type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(arrow)
{
    OBJECT_METHODS(ARROW)->apply_func =
        object_apply_item;
    OBJECT_METHODS(ARROW)->ranged_fire_func =
        ranged_fire_func;
    OBJECT_METHODS(ARROW)->projectile_stop_func =
        common_object_projectile_stop_missile;
    OBJECT_METHODS(ARROW)->process_func =
        common_object_projectile_process;
    OBJECT_METHODS(ARROW)->projectile_move_func =
        common_object_projectile_move;
    OBJECT_METHODS(ARROW)->projectile_fire_func =
        common_object_projectile_fire_missile;
    OBJECT_METHODS(ARROW)->projectile_hit_func =
        common_object_projectile_hit;
    OBJECT_METHODS(ARROW)->move_on_func =
        common_object_projectile_move_on;
    OBJECT_METHODS(ARROW)->process_treasure_func =
        process_treasure_func;
}

/**
 * Calculate arrow's wc.
 *
 * @param op
 * Player.
 * @param bow
 * Bow used.
 * @param arrow
 * Arrow.
 * @return
 * The arrow's wc.
 */
int16_t
arrow_get_wc (object *op, object *bow, object *arrow)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(bow != NULL);
    HARD_ASSERT(arrow != NULL);

    op = HEAD(op);

    int level;
    if (op->type == PLAYER) {
        object *skill = CONTR(op)->skill_ptr[bow_get_skill(bow)];
        if (skill == NULL) {
            return 0;
        }

        level = skill->level;
    } else {
        level = op->level;
    }

    return (arrow->stats.wc + bow->magic + arrow->magic + level +
            wc_bonus[op->stats.Dex] + bow->stats.wc);
}

/**
 * Calculate arrow's damage.
 *
 * @param op
 * Player.
 * @param bow
 * Bow used.
 * @param arrow
 * Arrow.
 * @return
 * The arrow's damage.
 */
int16_t
arrow_get_damage (object *op, object *bow, object *arrow)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(bow != NULL);
    HARD_ASSERT(arrow != NULL);

    op = HEAD(op);

    int level;
    if (op->type == PLAYER) {
        object *skill = CONTR(op)->skill_ptr[bow_get_skill(bow)];
        if (skill == NULL) {
            return 0;
        }

        level = skill->level;
    } else {
        level = op->level;
    }

    double dam = arrow->stats.dam + arrow->magic;
    dam += bow->stats.dam + bow->magic;
    if (op->type == PLAYER) {
        dam += CONTR(op)->dam_bonus;
    }
    dam *= LEVEL_DAMAGE(level);
    dam = ABS(dam);
    if (op->type == PLAYER) {
        dam += dam * dam_bonus[op->stats.Str] / 10.0;
    }

    int item_condition;
    if (bow->item_condition > arrow->item_condition) {
        item_condition = bow->item_condition;
    } else {
        item_condition = arrow->item_condition;
    }

    dam = dam / 100.0 * item_condition;

    return dam;
}

/**
 * Extended find arrow version, using tag and containers.
 *
 * Find an arrow in the inventory and after that in the right type
 * container (quiver).
 *
 * @param op
 * Player.
 * @param type
 * Type of the ammunition (arrows, bolts, etc). Can be NULL.
 * @return
 * Pointer to the arrow, NULL if not found.
 */
object *
arrow_find (object *op, shstr *type)
{
    HARD_ASSERT(op != NULL);

    /* For non-players, little more work is necessary to find an arrow. */
    if (op->type != PLAYER) {
        FOR_INV_PREPARE(op, tmp) {
            if (tmp->type == ARROW && tmp->race == type) {
                return tmp;
            }

            if (tmp->type == CONTAINER && tmp->race == type) {
                object *arrow = arrow_find(tmp, type);
                if (arrow != NULL) {
                    return arrow;
                }
            }
        } FOR_INV_FINISH();

        return NULL;
    }

    object *tmp = CONTR(op)->equipment[PLAYER_EQUIP_AMMO];
    /* Nothing readied. */
    if (tmp == NULL) {
        return NULL;
    }

    /* The type does not match the arrow/quiver. */
    if (tmp->race != type) {
        return NULL;
    }

    /* The readied item is an arrow, so simply return it. */
    if (tmp->type == ARROW) {
        return tmp;
    } else if (tmp->type == CONTAINER) {
        /* A quiver, search through it for arrows. */
        return arrow_find(tmp, type);
    }

    return NULL;
}
