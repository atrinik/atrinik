/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
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
	if (bow->sub_type == RANGE_WEAP_BOW)
	{
		return SK_MISSILE_WEAPON;
	}
	else if (bow->sub_type == RANGE_WEAP_XBOWS)
	{
		return SK_XBOW_WEAP;
	}
	else
	{
		return SK_SLING_WEAP;
	}
}

/**
 * Player fires a bow.
 * @param op Object firing.
 * @param dir Direction to fire. */
void bow_fire(object *op, int dir)
{
	object *bow, *arrow, *skill;

	if (op->type != PLAYER)
	{
		return;
	}

	/* If no dir is specified, attempt to find get the direction from
	 * player's target. */
	if (!dir && OBJECT_VALID(CONTR(op)->target_object, CONTR(op)->target_object_count))
	{
		rv_vector rv;

		dir = get_dir_to_target(op, CONTR(op)->target_object, &rv);
	}

	if (!dir)
	{
		draw_info(COLOR_WHITE, op, "You can't shoot yourself!");
		return;
	}

	bow = CONTR(op)->equipment[PLAYER_EQUIP_BOW];

	if (!bow)
	{
		return;
	}

	if (!bow->race)
	{
		draw_info_format(COLOR_WHITE, op, "Your %s is broken.", bow->name);
		return;
	}

	arrow = arrow_find(op, bow->race);

	if (!arrow)
	{
		draw_info_format(COLOR_WHITE, op, "You have no %s left.", bow->race);
		return;
	}

	if (wall(op->map, op->x + freearr_x[dir], op->y + freearr_y[dir]))
	{
		draw_info(COLOR_WHITE, op, "Something is in the way.");
		return;
	}

	/* Skill action time. */
	op->chosen_skill->stats.maxsp = bow->stats.sp + arrow->last_grace;

	arrow = object_stack_get_removed(arrow, 1);

	/* Save original WC, damage and range. */
	arrow->last_heal = arrow->stats.wc;
	arrow->stats.hp = arrow->stats.dam;
	arrow->stats.sp = arrow->last_sp;

	/* Determine how many tiles the arrow will fly. */
	arrow->last_sp = bow->last_sp + arrow->last_sp;

	/* Get the used skill. */
	skill = SK_skill(op);

	/* If we got the skill, add in the skill's modifiers. */
	if (skill)
	{
		/* Add WC. */
		arrow->stats.wc += skill->last_heal;
		/* Add tiles range. */
		arrow->last_sp += skill->last_sp;
	}

	/* Add WC and damage bonuses. */
	arrow->stats.wc = arrow_get_wc(op, bow, arrow);
	arrow->stats.dam = arrow_get_damage(op, bow, arrow);

	/* Use the bow's WC range. */
	arrow->stats.wc_range = bow->stats.wc_range;

	arrow = object_projectile_fire(arrow, op, dir);

	if (!arrow)
	{
		return;
	}

	CONTR(op)->stat_arrows_fired++;

	play_sound_map(op->map, CMD_SOUND_EFFECT, "bow1.ogg", op->x, op->y, 0, 0);
}

/**
 * Initialize the bow type object methods. */
void object_type_init_bow(void)
{
	object_type_methods[BOW].apply_func = object_apply_item;
}
