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
	object *skill = CONTR(op)->skill_ptr[bow_get_skill(bow)];

	if (!skill)
	{
		return 0;
	}

	return arrow->stats.wc + bow->magic + arrow->magic + skill->level + thaco_bonus[op->stats.Dex] + bow->stats.wc;
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
	object *skill = CONTR(op)->skill_ptr[bow_get_skill(bow)];

	if (!skill)
	{
		return 0;
	}

	dam = arrow->stats.dam + arrow->magic;
	dam = FABS((int) ((float) (dam * LEVEL_DAMAGE(skill->level))));
	dam += dam * (dam_bonus[op->stats.Str] / 2 + bow->stats.dam + bow->magic) / 10;

	if (bow->item_condition > arrow->item_condition)
	{
		dam = (sint16) (((float) dam / 100.0f) * (float) bow->item_condition);
	}
	else
	{
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

	tmp = CONTR(op)->ready_object[READY_OBJ_ARROW];

	/* Nothing readied. */
	if (!tmp || !OBJECT_VALID(tmp, CONTR(op)->ready_object_tag[READY_OBJ_ARROW]))
	{
		return NULL;
	}

	/* The type does not match the arrow/quiver. */
	if (tmp->race != type)
	{
		return NULL;
	}

	/* The readied item is an arrow, so simply return it. */
	if (tmp->type == ARROW)
	{
		return tmp;
	}
	/* A quiver, search through it for arrows. */
	else if (tmp->type == CONTAINER)
	{
		for (tmp = tmp->inv; tmp; tmp = tmp->below)
		{
			if (tmp->race == type && tmp->type == ARROW)
			{
				return tmp;
			}
		}
	}

	return NULL;
}

/** @copydoc object_methods::projectile_stop_func */
static object *projectile_stop_func(object *op, int reason)
{
	object *owner;

	owner = get_owner(op);

	op = common_object_projectile_stop_missile(op, reason);

	if (!op)
	{
		return NULL;
	}

	/* Restore WC, damage and range. */
	op->stats.wc = op->last_heal;
	op->stats.dam = op->stats.hp;
	op->last_sp = op->stats.sp;

	op->last_heal = op->stats.hp = op->stats.sp = 0;

	op->stats.wc_range = op->arch->clone.stats.wc_range;

	if (owner && owner->type == PLAYER && QUERY_FLAG(op, FLAG_STAND_STILL))
	{
		pick_up(owner, op, 0);
		return op;
	}

	object_merge(op);

	return op;
}

/**
 * Initialize the arrow type object methods. */
void object_type_init_arrow(void)
{
	object_type_methods[ARROW].projectile_stop_func = projectile_stop_func;

	object_type_methods[ARROW].process_func = common_object_projectile_process;
	object_type_methods[ARROW].projectile_move_func = common_object_projectile_move;
	object_type_methods[ARROW].projectile_fire_func = common_object_projectile_fire_missile;
	object_type_methods[ARROW].projectile_hit_func = common_object_projectile_hit;
	object_type_methods[ARROW].move_on_func = common_object_projectile_move_on;
}
