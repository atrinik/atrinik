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
 * Calculate how quickly bow fires its arrow.
 * @param bow The bow.
 * @param arrow Arrow.
 * @return Firing speed. */
sint32 bow_get_ws(object *bow, object *arrow)
{
	return (((float) bow->stats.sp / (1000000 / MAX_TIME)) + ((float) arrow->last_grace / (1000000 / MAX_TIME))) * 1000;
}

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

/**
 * Player fires a bow.
 * @param op Object firing.
 * @param dir Direction to fire. */
void bow_fire(object *op, int dir)
{
	object *bow, *arrow = NULL, *tmp_op;

	/* If no dir is specified, attempt to find get the direction from
	 * player's target. */
	if (!dir && op->type == PLAYER && OBJECT_VALID(CONTR(op)->target_object, CONTR(op)->target_object_count))
	{
		rv_vector range_vector;
		dir = get_dir_to_target(op, CONTR(op)->target_object, &range_vector);
	}

	if (!dir)
	{
		draw_info(COLOR_WHITE, op, "You can't shoot yourself!");
		return;
	}

	bow = CONTR(op)->equipment[PLAYER_EQUIP_BOW];

	if (!bow)
	{
		LOG(llevBug, "bow_fire(): bow without activated bow (%s - %d).\n", op->name, dir);
	}

	if (!bow->race)
	{
		draw_info_format(COLOR_WHITE, op, "Your %s is broken.", bow->name);
		return;
	}

	if ((arrow = arrow_find(op, bow->race)) == NULL)
	{
		draw_info_format(COLOR_WHITE, op, "You have no %s left.", bow->race);
		return;
	}

	if (wall(op->map, op->x + freearr_x[dir], op->y + freearr_y[dir]))
	{
		draw_info(COLOR_WHITE, op, "Something is in the way.");
		return;
	}

	/* This should not happen, but sometimes does */
	if (arrow->nrof == 0)
	{
		LOG(llevDebug, "arrow->nrof == 0 in bow_fire() (%s)\n", query_name(arrow, NULL));
		remove_ob(arrow);
		return;
	}

	CONTR(op)->stat_arrows_fired++;

	/* These are arrows left to the player */
	arrow = get_split_ob(arrow, 1, NULL, 0);
	set_owner(arrow, op);
	arrow->direction = dir;
	arrow->x = op->x;
	arrow->y = op->y;
	arrow->speed = 1;

	/* Now the trick: we transfer the shooting speed in the used
	 * skill - that will allow us to use "set_skill_speed() as global
	 * function. */
	op->chosen_skill->stats.maxsp = bow->stats.sp + arrow->last_grace;
	update_ob_speed(arrow);
	arrow->speed_left = 0;
	SET_ANIMATION(arrow, (NUM_ANIMATIONS(arrow) / NUM_FACINGS(arrow)) * dir);
	/* Save original wc and dam */
	arrow->last_heal = arrow->stats.wc;
	/* Will be put back in fix_arrow() */
	arrow->stats.hp = arrow->stats.dam;
	/* Determine how many tiles the arrow will fly. */
	arrow->last_sp = bow->last_sp + arrow->last_sp;
	/* Get the used skill. */
	tmp_op = SK_skill(op);

	/* Now we do this: arrow wc = wc base from skill + (wc arrow + magic) + (wc range weapon bonus + magic) */
	if (tmp_op)
	{
		/* wc is in last heal */
		arrow->stats.wc += tmp_op->last_heal;
		/* Add tiles range from the skill object. */
		arrow->last_sp += tmp_op->last_sp;
	}
	else
	{
		arrow->stats.wc += 10;
	}

	/* Add in all our wc bonus */
	arrow->stats.wc = arrow_get_wc(op, bow, arrow);
	arrow->stats.wc_range = bow->stats.wc_range;
	arrow->stats.dam = arrow_get_damage(op, bow, arrow);
	arrow->level = SK_level(op);
	arrow->map = op->map;
	SET_MULTI_FLAG(arrow, FLAG_FLYING);
	SET_FLAG(arrow, FLAG_IS_MISSILE);
	SET_FLAG(arrow, FLAG_FLY_ON);
	SET_FLAG(arrow, FLAG_WALK_ON);
	/* Temporary buffer for "tiles to fly" */
	arrow->stats.grace = arrow->last_sp;
	/* Reflection timer */
	arrow->stats.maxgrace = 60 + (RANDOM() % 12);
	play_sound_map(op->map, CMD_SOUND_EFFECT, "bow1.ogg", op->x, op->y, 0, 0);

	if (insert_ob_in_map(arrow, op->map, op, 0))
	{
		move_arrow(arrow);
	}
}

/**
 * Fix a stopped arrow.
 * @param op The arrow object.
 * @return The arrow object. */
object *fix_stopped_arrow(object *op)
{
	object *owner;

	/* Small chance of breaking */
	if (op->last_eat && rndm_chance(op->last_eat))
	{
		remove_ob(op);
		return NULL;
	}

	/* Used as temp. vars to control reflection/move speed */
	op->stats.grace = 0;
	op->stats.maxgrace = 0;

	op->direction = 0;
	CLEAR_FLAG(op, FLAG_WALK_ON);
	CLEAR_FLAG(op, FLAG_FLY_ON);
	CLEAR_MULTI_FLAG(op, FLAG_FLYING);
	owner = get_owner(op);

	/* Food is a self destruct marker - that long the item will need to be destruct! */
	if ((!owner || owner->type != PLAYER) && op->stats.food && op->type == ARROW)
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

	if (owner && owner->type == PLAYER && QUERY_FLAG(op, FLAG_STAND_STILL))
	{
		pick_up(owner, op, 0);
		return NULL;
	}

	return op;
}

/**
 * Move an arrow along its course.
 * @param op The arrow or thrown object */
void move_arrow(object *op)
{
	object *tmp = NULL, *hitter;
	int x, y;
	int flags;
	int was_reflected;
	mapstruct *m = op->map;

	if (op->map == NULL)
	{
		LOG(llevBug, "move_arrow(): Arrow %s had no map.\n", query_name(op, NULL));
		remove_ob(op);
		return;
	}

	if (op->last_sp-- < 0)
	{
		stop_arrow(op);
		return;
	}

	x = op->x + DIRX(op);
	y = op->y + DIRY(op);
	was_reflected = 0;

	if (!(m = get_map_from_coord(op->map, &x, &y)))
	{
		stop_arrow(op);
		return;
	}

	if (!(hitter = get_owner(op)))
	{
		hitter = op;
	}

	if ((flags = GET_MAP_FLAGS(m, x, y)) & (P_IS_MONSTER | P_IS_PLAYER))
	{
		/* Search for a vulnerable object. */
		for (tmp = GET_MAP_OB_LAYER(m, x, y, LAYER_LIVING, 0); tmp && tmp->layer == LAYER_LIVING; tmp = tmp->above)
		{
			tmp = HEAD(tmp);

			/* Now, let friends fire through friends */
			if (!IS_LIVE(tmp) || is_friend_of(hitter, tmp) || tmp == hitter)
			{
				continue;
			}

			if (QUERY_FLAG(tmp, FLAG_REFL_MISSILE) && rndm(0, 99) < 90 - op->level / 10)
			{
				op->direction = absdir(op->direction + 4);
				op->state = 0;

				if (GET_ANIM_ID(op))
				{
					SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction);
				}

				if (wall(m, x, y))
				{
					/* Target is standing on a wall. Let arrow turn around before
					 * the wall. */
					x = op->x;
					y = op->y;
				}

				/* Skip normal movement calculations. */
				was_reflected = 1;
				break;
			}
			else
			{
				/* Attack the object. */
				op = hit_with_arrow(op, tmp);

				if (op == NULL)
				{
					return;
				}
			}
		}
	}

	if (!was_reflected && wall(m, x, y))
	{
		/* If the object doesn't reflect, stop the arrow from moving. */
		if (!QUERY_FLAG(op, FLAG_REFLECTING) || !(rndm(0, 19)))
		{
			stop_arrow(op);
			return;
		}
		else
		{
			/* If one of the major directions (n, s, e, w), just reverse it */
			if (op->direction & 1)
			{
				op->direction = absdir(op->direction + 4);
			}
			else
			{
				int left, right;

				left = wall(op->map, op->x + freearr_x[absdir(op->direction - 1)], op->y + freearr_y[absdir(op->direction - 1)]);
				right = wall(op->map, op->x + freearr_x[absdir(op->direction + 1)], op->y + freearr_y[absdir(op->direction + 1)]);

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

			x = op->x + DIRX(op);
			y = op->y + DIRY(op);

			/* Couldn't find a direction to move the arrow to - just stop
			 * it from moving. */
			if (!(m = get_map_from_coord(op->map, &x, &y)) || wall(m, x, y))
			{
				stop_arrow(op);
				return;
			}

			/* Update object image for new facing. */
			if (GET_ANIM_ID(op))
			{
				SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction);
			}
		}
	}

	/* Move the arrow. */
	remove_ob(op);

	op->x = x;
	op->y = y;
	insert_ob_in_map(op, m, op, 0);
}

/**
 * What to do when a non-living flying object has to stop.
 * @param op The object. */
void stop_arrow(object *op)
{
	play_sound_map(op->map, CMD_SOUND_EFFECT, "drop.ogg", op->x, op->y, 0, 0);
	CLEAR_FLAG(op, FLAG_IS_MISSILE);
	op = fix_stopped_arrow(op);

	if (op)
	{
		merge_ob(op, NULL);
	}
}

/**
 * Initialize the arrow type object methods. */
void object_type_init_arrow(void)
{
}
