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
 * Common range firing code.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc object_methods::throw_func */
int common_object_throw(object *op, object *shooter, int dir)
{
	object *skill, *copy;

	if (!dir)
	{
		draw_info(COLOR_WHITE, shooter, "You can't throw that at yourself.");
		return OBJECT_METHOD_UNHANDLED;
	}

	if (QUERY_FLAG(op, FLAG_STARTEQUIP))
	{
		draw_info(COLOR_WHITE, shooter, "The gods won't let you throw that.");
		return OBJECT_METHOD_UNHANDLED;
	}

	if (op->weight <= 0 || QUERY_FLAG(op, FLAG_NO_DROP))
	{
		draw_info_format(COLOR_WHITE, shooter, "You can't throw %s.", query_base_name(op, shooter));
		return OBJECT_METHOD_UNHANDLED;
	}

	if (QUERY_FLAG(op, FLAG_APPLIED))
	{
		if (OBJECT_CURSED(op))
		{
			draw_info_format(COLOR_WHITE, shooter, "The %s sticks to your hand!", query_base_name(op, shooter));
			return OBJECT_METHOD_UNHANDLED;
		}

		if (object_apply_item(op, shooter, AP_UNAPPLY | AP_NO_MERGE) != OBJECT_METHOD_OK)
		{
			return OBJECT_METHOD_UNHANDLED;
		}
	}

	if (QUERY_FLAG(op, FLAG_DUST))
	{
		if ((shooter->type == PLAYER && !change_skill(shooter, SK_USE_MAGIC_ITEM)) || op->stats.sp < 0 || op->stats.sp >= NROFREALSPELLS || !spells[op->stats.sp].archname || !(spells[op->stats.sp].flags & SPELL_DESC_DIRECTION))
		{
			draw_info_format(COLOR_WHITE, shooter, "You scatter the %s, but nothing happens.", query_short_name(op, shooter));
		}
		else
		{
			if (shooter->chosen_skill)
			{
				shooter->chosen_skill->stats.maxsp = op->last_grace;
			}

			draw_info_format(COLOR_WHITE, shooter, "You scatter the %s.", query_short_name(op, shooter));
			cast_cone(shooter, op, dir, 10, op->stats.sp, spellarch[op->stats.sp]);
		}

		decrease_ob(op);

		return OBJECT_METHOD_OK;
	}

	op = object_stack_get_removed(op, 1);

	copy = get_object();
	copy_object(op, copy, 0);
	copy->type = MISC_OBJECT;
	CLEAR_FLAG(copy, FLAG_CAN_STACK);
	copy->layer = LAYER_EFFECT;
	insert_ob_in_ob(op, copy);

	/* Calculate moving speed. */
	copy->speed = MIN(1.0f, (speed_bonus[shooter->stats.Str] + 1.0f) / 1.5f);

	if (op->type != ARROW)
	{
		int i;
		uint8 has_attack;

		copy->stats.dam = 0;
		copy->stats.wc = 0;
		copy->last_sp = 5;
		copy->last_eat = 0;
		copy->last_grace = 20;

		has_attack = 0;

		/* Figure out whether the item has an attack. */
		for (i = 0; i < NROFATTACKS; i++)
		{
			if (copy->attack[i])
			{
				has_attack = 1;
				break;
			}
		}

		/* No attack, add default attack. */
		if (!has_attack)
		{
			copy->attack[ATNR_IMPACT] = 100;
		}
	}

	skill = SK_skill(shooter);

	/* Add skill bonuses. */
	if (skill)
	{
		copy->stats.wc += skill->last_heal;
	}

	copy->stats.dam += copy->magic;
	copy->stats.wc += SK_level(shooter);
	copy->stats.wc += copy->magic;

	if (shooter->type == PLAYER)
	{
		copy->stats.dam += dam_bonus[shooter->stats.Str] / 2;
		copy->stats.wc += thaco_bonus[shooter->stats.Dex];
	}
	else
	{
		copy->stats.wc += 5;
	}

	copy->stats.dam = (sint16) ((double) copy->stats.dam * LEVEL_DAMAGE(SK_level(shooter)));

	if (copy->item_quality)
	{
		copy->stats.dam = MAX(0, (sint16) (((double) copy->stats.dam / 100.0f) * (double) copy->item_condition));
	}

	/* Not meant for throwing, so it's not as efficient. */
	if (op->type != ARROW)
	{
		copy->stats.dam /= 2;
		copy->stats.wc /= 2;
	}

	if (shooter->chosen_skill)
	{
		shooter->chosen_skill->stats.maxsp = copy->last_grace;
	}

	copy = object_projectile_fire(copy, shooter, dir);

	if (!copy)
	{
		return OBJECT_METHOD_OK;
	}

	if (shooter->type == PLAYER)
	{
		CONTR(shooter)->stat_missiles_thrown++;
	}

	play_sound_map(shooter->map, CMD_SOUND_EFFECT, "throw.ogg", shooter->x, shooter->y, 0, 0);

	return OBJECT_METHOD_OK;
}
