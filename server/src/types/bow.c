/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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
 * Handles code for @ref BOW "bows".
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Calculate how quickly bow fires its arrow.
 * @param bow The bow.
 * @param arrow Arrow.
 * @return Firing speed. */
sint32 bow_get_ws(object *bow, object *arrow)
{
    return (((float) bow->stats.sp / (1000000 / MAX_TIME)) + ((float) arrow->last_grace / (1000000 / MAX_TIME))) * 1000;
}

/**
 * Get skill required to use the specified bow object.
 * @param bow The bow (could actually be a crossbow/sling/etc).
 * @return Required skill to use the object. */
int bow_get_skill(object *bow)
{
    if (bow->item_skill) {
        return bow->item_skill - 1;
    }

    return SK_BOW_ARCHERY;
}

/** @copydoc object_methods::ranged_fire_func */
static int ranged_fire_func(object *op, object *shooter, int dir, double *delay)
{
    object *arrow, *skill;

    arrow = arrow_find(shooter, op->race);

    if (!arrow) {
        draw_info_format(COLOR_WHITE, shooter, "You have no %s left.", op->race);
        return OBJECT_METHOD_OK;
    }

    if (wall(shooter->map, shooter->x + freearr_x[dir], shooter->y + freearr_y[dir])) {
        draw_info(COLOR_WHITE, shooter, "Something is in the way.");
        return OBJECT_METHOD_OK;
    }

    if (QUERY_FLAG(arrow, FLAG_SYS_OBJECT)) {
        object *copy;

        copy = get_object();
        copy_object(arrow, copy, 0);
        CLEAR_FLAG(copy, FLAG_SYS_OBJECT);
        SET_FLAG(copy, FLAG_NO_PICK);
        copy->nrof = 0;
        arrow = copy;
    }
    else {
        arrow = object_stack_get_removed(arrow, 1);
    }

    /* Save original WC, damage and range. */
    arrow->last_heal = arrow->stats.wc;
    arrow->stats.hp = arrow->stats.dam;
    arrow->stats.sp = arrow->last_sp;

    /* Determine how many tiles the arrow will fly. */
    arrow->last_sp = op->last_sp + arrow->last_sp;

    /* Get the used skill. */
    skill = SK_skill(shooter);

    /* If we got the skill, add in the skill's modifiers. */
    if (skill) {
        /* Add WC. */
        arrow->stats.wc += skill->last_heal;
        /* Add tiles range. */
        arrow->last_sp += skill->last_sp;
    }

    /* Add WC and damage bonuses. */
    arrow->stats.wc = arrow_get_wc(shooter, op, arrow);
    arrow->stats.dam = arrow_get_damage(shooter, op, arrow);

    /* Use the bow's WC range. */
    arrow->stats.wc_range = op->stats.wc_range;

    if (delay) {
        *delay = op->stats.sp + arrow->last_grace;
    }

    arrow = object_projectile_fire(arrow, shooter, dir);

    if (!arrow) {
        return OBJECT_METHOD_OK;
    }

    if (shooter->type == PLAYER) {
        CONTR(shooter)->stat_arrows_fired++;
    }

    play_sound_map(shooter->map, CMD_SOUND_EFFECT, "bow1.ogg", shooter->x, shooter->y, 0, 0);
    return OBJECT_METHOD_OK;
}

/**
 * Initialize the bow type object methods. */
void object_type_init_bow(void)
{
    object_type_methods[BOW].apply_func = object_apply_item;
    object_type_methods[BOW].ranged_fire_func = ranged_fire_func;
}
