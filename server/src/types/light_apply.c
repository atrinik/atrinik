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
 * Handles code related to @ref LIGHT_APPLY "applyable lights".
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc object_methods::apply_func */
static int apply_func(object *op, object *applier, int aflags)
{
	/* If lit and in player's inventory, handle as normal item apply. */
	if (op->glow_radius && op->env && op->env->type == PLAYER)
	{
		object_apply_item(op, applier, aflags);

		/* If the light is applied now, we don't want to go on and
		 * extinguish it. */
		if (QUERY_FLAG(op, FLAG_APPLIED))
		{
			return OBJECT_METHOD_OK;
		}
	}

	if (op->glow_radius)
	{
		op = object_stack_get_reinsert(op, 1);

		draw_info_format(COLOR_WHITE, applier, "You extinguish the %s.", query_name(op, applier));

		CLEAR_FLAG(op, FLAG_CHANGING);

		if (op->other_arch && op->other_arch->clone.sub_type & 1)
		{
			op->animation_id = op->other_arch->clone.animation_id;
			op->state = 0;
			SET_ANIMATION_STATE(op);
			esrv_update_item(UPD_FACE | UPD_ANIM, op);
		}
		else
		{
			CLEAR_FLAG(op, FLAG_ANIMATE);
			op->face = op->arch->clone.face;
			esrv_update_item(UPD_FACE | UPD_ANIMSPEED, op);
		}

		if (op->map)
		{
			adjust_light_source(op->map, op->x, op->y, -(op->glow_radius));
			update_object(op, UP_OBJ_FACE);
		}

		op->glow_radius = 0;
	}
	else if (op->last_sp)
	{
		op = object_stack_get_reinsert(op, 1);

		draw_info_format(COLOR_WHITE, applier, "You light the %s.", query_name(op, NULL));

		/* Light source that burns out... */
		if (op->last_eat)
		{
			SET_FLAG(op, FLAG_CHANGING);
		}

		if (op->speed)
		{
			SET_FLAG(op, FLAG_ANIMATE);
			op->animation_id = op->arch->clone.animation_id;
			SET_ANIMATION_STATE(op);
			esrv_update_item(UPD_FACE | UPD_ANIM | UPD_ANIMSPEED, op);
		}

		op->glow_radius = op->last_sp;

		if (op->map)
		{
			adjust_light_source(op->map, op->x, op->y, op->glow_radius);
			update_object(op, UP_OBJ_FACE);
		}
	}
	else
	{
		draw_info_format(COLOR_WHITE, applier, "The %s can't be lit.", query_name(op, applier));
	}

	return OBJECT_METHOD_OK;
}

/**
 * Initialize the applyable light type object methods. */
void object_type_init_light_apply(void)
{
	object_type_methods[LIGHT_APPLY].apply_func = apply_func;
}
