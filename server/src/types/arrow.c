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
 * @ref ARROW "Arrow" and @ref BOW "bow" related code. */

#include <global.h>

/**
 * Calculate arrow's wc.
 * @param op Player.
 * @param bow Bow used.
 * @param arrow Arrow.
 * @return The arrow's wc. */
sint16 arrow_get_wc(object *op, object *bow, object *arrow)
{
    int level;

    op = HEAD(op);

    if (op->type == PLAYER) {
        object *skill;

        skill = CONTR(op)->skill_ptr[bow_get_skill(bow)];

        if (!skill) {
            return 0;
        }

        level = skill->level;
    } else {
        level = op->level;
    }

    return arrow->stats.wc + bow->magic + arrow->magic + level + thaco_bonus[op->stats.Dex] + bow->stats.wc;
}

/**
 * Calculate arrow's damage.
 * @param op Player.
 * @param bow Bow used.
 * @param arrow Arrow.
 * @return The arrow's damage. */
sint16 arrow_get_damage(object *op, object *bow, object *arrow)
{
    sint16 dam;
    int level;

    op = HEAD(op);

    if (op->type == PLAYER) {
        object *skill;

        skill = CONTR(op)->skill_ptr[bow_get_skill(bow)];

        if (!skill) {
            return 0;
        }

        level = skill->level;
    } else {
        level = op->level;
    }

    dam = arrow->stats.dam + arrow->magic;
    dam = FABS((int) ((float) (dam * LEVEL_DAMAGE(level))));
    dam += dam * (dam_bonus[op->stats.Str] / 2 + bow->stats.dam + bow->magic) / 10;

    if (bow->item_condition > arrow->item_condition) {
        dam = (sint16) (((float) dam / 100.0f) * (float) bow->item_condition);
    } else {
        dam = (sint16) (((float) dam / 100.0f) * (float) arrow->item_condition);
    }

    return dam;
}

/**
 * Extended find arrow version, using tag and containers.
 *
 * Find an arrow in the inventory and after that in the right type
 * container (quiver).
 * @param op Player.
 * @param type Type of the ammunition (arrows, bolts, etc).
 * @return Pointer to the arrow, NULL if not found. */
object *arrow_find(object *op, shstr *type)
{
    object *tmp;

    if (op->type != PLAYER) {
        object *tmp2;

        for (tmp = op->inv; tmp; tmp = tmp->below) {
            if (tmp->type == ARROW && tmp->race == type) {
                return tmp;
            } else if (tmp->type == CONTAINER && tmp->race == type && QUERY_FLAG(tmp, FLAG_APPLIED)) {
                tmp2 = arrow_find(tmp, type);

                if (tmp2) {
                    return tmp2;
                }
            }
        }

        return NULL;
    }

    tmp = CONTR(op)->equipment[PLAYER_EQUIP_AMMO];

    /* Nothing readied. */
    if (!tmp) {
        return NULL;
    }

    /* The type does not match the arrow/quiver. */
    if (tmp->race != type) {
        return NULL;
    }

    /* The readied item is an arrow, so simply return it. */
    if (tmp->type == ARROW) {
        return tmp;
    }
    else if (tmp->type == CONTAINER) {
        /* A quiver, search through it for arrows. */
        return arrow_find(tmp, type);
    }

    return NULL;
}

/** @copydoc object_methods::ranged_fire_func */
static int ranged_fire_func(object *op, object *shooter, int dir, double *delay)
{
    object *skill;

    if (dir == 0) {
        draw_info(COLOR_WHITE, shooter, "You can't throw that at yourself.");
        return OBJECT_METHOD_UNHANDLED;
    }

    if (QUERY_FLAG(op, FLAG_STARTEQUIP)) {
        draw_info(COLOR_WHITE, shooter, "The gods won't let you throw that.");
        return OBJECT_METHOD_UNHANDLED;
    }

    if (op->weight <= 0 || QUERY_FLAG(op, FLAG_NO_DROP)) {
        draw_info_format(COLOR_WHITE, shooter, "You can't throw %s.", query_base_name(op, shooter));
        return OBJECT_METHOD_UNHANDLED;
    }

    if (QUERY_FLAG(op, FLAG_APPLIED) && OBJECT_CURSED(op)) {
        draw_info_format(COLOR_WHITE, shooter, "The %s sticks to your hand!", query_base_name(op, shooter));
        return OBJECT_METHOD_UNHANDLED;
    }

    op = object_stack_get_removed(op, 1);

    /* Save original WC, damage and range. */
    op->last_heal = op->stats.wc;
    op->stats.hp = op->stats.dam;
    op->stats.sp = op->last_sp;

    /* Calculate moving speed. */
    op->speed = MIN(1.0f, ((shooter->type == PLAYER ? speed_bonus[shooter->stats.Str] : 0.0f) + 1.0f) / 1.5f);

    /* Get the used skill. */
    skill = SK_skill(shooter);

    /* If we got the skill, add in the skill's modifiers. */
    if (skill) {
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
        op->stats.wc += thaco_bonus[shooter->stats.Dex];
    } else {
        op->stats.wc += 5;
    }

    op->stats.dam = (sint16) ((double) op->stats.dam * LEVEL_DAMAGE(SK_level(shooter)));

    if (op->item_quality) {
        op->stats.dam = MAX(0, (sint16) (((double) op->stats.dam / 100.0f) * (double) op->item_condition));
    }

    if (delay) {
        *delay = op->last_grace;
    }

    op = object_projectile_fire(op, shooter, dir);

    if (op == NULL) {
        return OBJECT_METHOD_OK;
    }

    if (shooter->type == PLAYER) {
        CONTR(shooter)->stat_missiles_thrown++;
    }

    play_sound_map(shooter->map, CMD_SOUND_EFFECT, "throw.ogg", shooter->x, shooter->y, 0, 0);

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the arrow type object methods. */
void object_type_init_arrow(void)
{
    object_type_methods[ARROW].apply_func = object_apply_item;
    object_type_methods[ARROW].ranged_fire_func = ranged_fire_func;
    object_type_methods[ARROW].projectile_stop_func = common_object_projectile_stop_missile;
    object_type_methods[ARROW].process_func = common_object_projectile_process;
    object_type_methods[ARROW].projectile_move_func = common_object_projectile_move;
    object_type_methods[ARROW].projectile_fire_func = common_object_projectile_fire_missile;
    object_type_methods[ARROW].projectile_hit_func = common_object_projectile_hit;
    object_type_methods[ARROW].move_on_func = common_object_projectile_move_on;
}
