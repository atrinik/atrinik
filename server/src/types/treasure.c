/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
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
 * Handles code for @ref TREASURE "treasure" objects. */

#include <global.h>

/**
 * Apply a treasure chest.
 * @param op The object applying the chest.
 * @param tmp The chest object. */
void apply_treasure(object *op, object *tmp)
{
	object *treas;
	tag_t tmp_tag = tmp->count, op_tag = op->count;

	/* Nice side effect of new treasure creation method is that the
	 * treasure for the chest is done when the chest is created, and put
	 * into the chest's inventory. So that when the chest burns up, the
	 * items still exist. Also prevents people from moving chests to more
	 * difficult maps to get better treasure. */
	treas = tmp->inv;

	if (tmp->map)
	{
		play_sound_map(tmp->map, CMD_SOUND_EFFECT, "open_container.ogg", tmp->x, tmp->y, 0, 0);
	}

	/* msg like "the chest crumbles to dust" */
	if (tmp->msg)
	{
		new_draw_info(NDI_UNIQUE, op, tmp->msg);
	}

	if (treas == NULL)
	{
		new_draw_info(NDI_UNIQUE, op, "The chest was empty.");
		decrease_ob(tmp);
		return;
	}
	do
	{
		remove_ob(treas);
		check_walk_off(treas, NULL, MOVE_APPLY_VANISHED);
		new_draw_info_format(NDI_UNIQUE, op, "You find %s in the chest.", query_name(treas, NULL));
		treas->x = op->x,treas->y = op->y;

		/* Monsters can be trapped in treasure chests */
		if (treas->type == MONSTER)
		{
			int i = find_free_spot(treas->arch, NULL, op->map, treas->x, treas->y, 0, 9);

			if (i != -1)
			{
				treas->x += freearr_x[i];
				treas->y += freearr_y[i];
			}

			fix_monster(treas);
		}

		treas = insert_ob_in_map(treas, op->map, op, 0);

		if (treas && treas->type == RUNE && treas->level && IS_LIVE(op))
		{
			spring_trap(treas, op);
		}

		if (was_destroyed(op, op_tag) || was_destroyed(tmp, tmp_tag))
		{
			break;
		}
	}
	while ((treas = tmp->inv) != NULL);

	if (!was_destroyed(tmp, tmp_tag) && tmp->inv == NULL)
	{
		decrease_ob(tmp);
	}
}
