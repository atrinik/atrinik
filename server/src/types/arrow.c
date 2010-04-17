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
 * @ref ARROW "Arrow" related code. */

#include <global.h>

/**
 * Fix a stopped arrow.
 * @param op The arrow object.
 * @return The arrow object. */
object *fix_stopped_arrow(object *op)
{
	object *tmp;

	if (op->type != ARROW)
	{
		return op;
	}

#if 0
	/* Small chance of breaking */
	if (rndm(0, 99) < op->stats.food)
	{
		remove_ob(op);
		return NULL;
	}
#endif

	/* Used as temp. vars to control reflection/move speed */
	op->stats.grace = 0;
	op->stats.maxgrace = 0;

	op->direction = 0;
	CLEAR_FLAG(op, FLAG_WALK_ON);
	CLEAR_FLAG(op, FLAG_FLY_ON);
	CLEAR_MULTI_FLAG(op, FLAG_FLYING);

	/* Food is a self destruct marker - that long the item will need to be destruct! */
	if ((!(tmp = get_owner(op)) || tmp->type != PLAYER) && op->stats.food && op->type == ARROW)
	{
		SET_FLAG(op, FLAG_IS_USED_UP);
		SET_FLAG(op, FLAG_NO_PICK);

		/* Important to neutralize the arrow! */
		op->type = MISC_OBJECT;
		op->speed = 0.1f;
		op->speed_left = 0.0f;
	}
	else
	{
		op->stats.wc_range = op->arch->clone.stats.wc_range;
		op->last_sp = op->arch->clone.last_sp;
		op->level = op->arch->clone.level;
		op->stats.food = op->arch->clone.stats.food;
		op->speed = 0;
	}

	update_ob_speed(op);
	op->stats.wc = op->last_heal;
	op->stats.dam = op->stats.hp;

	/* Reset these to zero, so that CAN_MERGE will work properly */
	op->last_heal = 0;
	op->stats.hp = 0;
	op->face = op->arch->clone.face;

	/* So that stopped arrows will be saved */
	clear_owner(op);
	update_object(op, UP_OBJ_FACE);

	return op;
}

/**
 * Move an arrow along its course.
 * @param op The arrow or thrown object */
void move_arrow(object *op)
{
	object *tmp = NULL, *hitter;
	int new_x, new_y;
	int flag_tmp;
	int was_reflected;
	mapstruct *m = op->map;

	if (op->map == NULL)
	{
		LOG(llevBug, "BUG: move_arrow(): Arrow %s had no map.\n", query_name(op, NULL));
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		return;
	}

	/* we need to stop thrown objects and arrows at some point. Like here. */
	if (op->type == THROWN_OBJ)
	{
		if (op->inv == NULL)
			return;
	}

	if (op->last_sp-- < 0)
	{
		stop_arrow(op);
		return;
	}

	/* Calculate target map square */
	if (op->stats.grace == 666)
	{
		/* Experimental target throwing hack. Using bresenham line algo */
		int dx = op->stats.hp, dy = op->stats.sp;

		if (dx > dy)
		{
			if (op->stats.exp >= 0)
			{
				new_y = op->y + op->stats.maxsp;
				/* same as fraction -= 2*dx */
				op->stats.exp -= dx;
			}
			else
			{
				new_y = op->y;
			}

			new_x = op->x + op->stats.maxhp;
			/* same as fraction -= 2*dy */
			op->stats.exp += dy;
		}
		else
		{
			if (op->stats.exp >= 0)
			{
				new_x = op->x + op->stats.maxhp;
				op->stats.exp -= dy;
			}
			else
			{
				new_x = op->x;
			}

			new_y = op->y + op->stats.maxsp;
			op->stats.exp += dx;
		}
	}
	else
	{
		new_x = op->x + DIRX(op);
		new_y = op->y + DIRY(op);
	}

	was_reflected = 0;

	/* check we are legal */
	if (!(m = get_map_from_coord(op->map, &new_x, &new_y)))
	{
		/* out of map... here is the end */
		stop_arrow(op);
		return;
	}

	if (!(hitter = get_owner(op)))
	{
		hitter = op;
	}

	/* ok, lets check there is something we can hit */
	if ((flag_tmp = GET_MAP_FLAGS(m, new_x, new_y)) & (P_IS_ALIVE | P_IS_PLAYER))
	{
		/* search for a vulnerable object */
		for (tmp = GET_MAP_OB_LAYER(m, new_x, new_y, 5); tmp != NULL; tmp = tmp->above)
		{
			/* Now, let friends fire through friends */
			if (is_friend_of(hitter, tmp) || tmp == hitter)
			{
				continue;
			}

			if (IS_LIVE(tmp) && (!QUERY_FLAG(tmp, FLAG_CAN_REFL_MISSILE) || (rndm(0, 99)) < 90 - op->level / 10))
			{
				/* Attack the object. */
				op = hit_with_arrow(op, tmp);

				/* the arrow has hit and is destroyed! */
				if (op == NULL)
				{
					return;
				}
			}
		}
	}

	/* if we are here, there is no target and/or we have not hit.
	 * now we do a simple reflection test. */
	if (flag_tmp & P_REFL_MISSILE)
	{
		missile_reflection_adjust(op, QUERY_FLAG(op, FLAG_WAS_REFLECTED));

		op->direction = absdir (op->direction + 4);
		op->state = 0;

		if (GET_ANIM_ID(op))
		{
			SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction);
		}

		if (wall(m, new_x, new_y))
		{
			/* Target is standing on a wall.  Let arrow turn around before
			 * the wall. */
			new_x = op->x;
			new_y = op->y;
		}

		SET_FLAG(op, FLAG_WAS_REFLECTED);
		/* skip normal movement calculations */
		was_reflected = 1;
	}

	if (!was_reflected && wall(m, new_x, new_y))
	{
		/* if the object doesn't reflect, stop the arrow from moving */
		if (!QUERY_FLAG(op, FLAG_REFLECTING) || !(rndm(0, 19)))
		{
			stop_arrow (op);
			return;
		}
		/* object is reflected */
		else
		{
			/* If one of the major directions (n,s,e,w), just reverse it */
			if (op->direction & 1)
			{
				op->direction = absdir(op->direction + 4);
			}
			else
			{
				/* The below is just logic for figuring out what direction
				 * the object should now take. */
				int left = wall(op->map, op->x + freearr_x[absdir(op->direction - 1)], op->y + freearr_y[absdir(op->direction - 1)]), right = wall(op->map, op->x + freearr_x[absdir(op->direction + 1)], op->y + freearr_y[absdir(op->direction + 1)]);

				if (left == right)
				{
					op->direction = absdir(op->direction + 4);
				}
				else if (left)
				{
					op->direction = absdir(op->direction + 2);
				}
				else if (right)
				{
					op->direction = absdir(op->direction - 2);
				}
			}

			/* Is the new direction also a wall?  If show, shuffle again */
			if (wall(op->map, op->x + DIRX(op), op->y + DIRY(op)))
			{
				int left= wall(op->map, op->x + freearr_x[absdir(op->direction - 1)], op->y + freearr_y[absdir(op->direction - 1)]), right = wall(op->map, op->x + freearr_x[absdir(op->direction + 1)], op->y + freearr_y[absdir(op->direction + 1)]);

				if (!left)
				{
					op->direction = absdir(op->direction - 1);
				}
				else if (!right)
				{
					op->direction = absdir(op->direction + 1);
				}
				/* is this possible? */
				else
				{
					stop_arrow(op);
					return;
				}
			}

			/* update object image for new facing */
			/* many thrown objects *don't* have more than one face */
			if (GET_ANIM_ID(op))
			{
				SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction);
			}
		}
	}

	/* Move the arrow. */
	remove_ob(op);

	if (check_walk_off(op, NULL, MOVE_APPLY_VANISHED) == CHECK_WALK_OK)
	{
		op->x = new_x;
		op->y = new_y;
		insert_ob_in_map(op, m, op,0);
	}
}

/**
 * What to do when a non-living flying object has to stop.
 * @param op The object. */
void stop_arrow(object *op)
{
	play_sound_map(op->map, op->x, op->y, SOUND_DROP_THROW, SOUND_NORMAL);
	CLEAR_FLAG(op, FLAG_IS_MISSILE);

	if (op->inv)
	{
		object *payload = op->inv;

		remove_ob(payload);
		check_walk_off(payload, NULL, MOVE_APPLY_VANISHED);

		/* Trigger the STOP event */
		trigger_event(EVENT_STOP, NULL, payload, op, NULL, 0, 0, 0, SCRIPT_FIX_NOTHING);

		/* we have a thrown potion here.
		 * This potion has NOT hit a target.
		 * it has hitten a wall or just dropped to the ground.
		 * 1.) its a AE spell... detonate it.
		 * 2.) its something else - shatter the potion. */
		if (payload->type == POTION)
		{
			if (payload->stats.sp != SP_NO_SPELL && spells[payload->stats.sp].flags & SPELL_DESC_DIRECTION)
			{
				cast_spell(payload, payload, payload->direction, payload->stats.sp, 1, spellPotion, NULL);
			}
		}
		else
		{
			clear_owner(payload);

			/* Gecko: if the script didn't put the payload somewhere else */
			if (!payload->env && !OBJECT_FREE(payload))
			{
				insert_ob_in_map(payload, op->map, payload, 0);
			}
		}

		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
	}
	else
	{
		op = fix_stopped_arrow(op);

		if (op)
		{
			merge_ob(op, NULL);
		}
	}
}
