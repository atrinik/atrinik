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
 * Handles code for light related object types, like
 * @ref LIGHTER "lighters", @ref LIGHT_SOURCE "light sources", etc. */

#include <global.h>

/**
 * Refill lamps and all refill type light sources from
 * apply_player_light().
 *
 * The light source must be in the inventory of the player, then he must
 * mark the light source and apply the refill item (lamp oil for
 * example).
 * @param who The player.
 * @param op The light source.  */
void apply_player_light_refill(object *who, object *op)
{
	object *item = find_marked_object(who);
	int tmp;

	if (!item)
	{
		draw_info_format(COLOR_WHITE, who, "You need to mark a light source you want refill.");
		return;
	}

	if (item->type != LIGHT_APPLY || !item->race || !strstr(item->race, op->race))
	{
		draw_info_format(COLOR_WHITE, who, "You can't refill the %s with the %s.", query_name(item, NULL), query_name(op, NULL));
		return;
	}

	tmp = (int) item->stats.maxhp - item->stats.food;

	if (!tmp)
	{
		draw_info_format(COLOR_WHITE, who, "The %s is full and can't be refilled.", query_name(item, NULL));
		return;
	}

	if (op->stats.food <= tmp)
	{
		item->stats.food += op->stats.food;
		draw_info_format(COLOR_WHITE, who, "You refill the %s with %d units of %s.", query_name(item, NULL), op->stats.food, query_name(op, NULL));
		decrease_ob(op);
	}
	else
	{
		object *filler = op;

		if (filler->nrof > 1)
		{
			filler = get_split_ob(filler, 1, NULL, 0);
			filler->stats.food -= tmp;
			insert_ob_in_ob(filler, who);
		}
		else
		{
			filler->stats.food -= tmp;
		}

		item->stats.food += tmp;
		draw_info_format(COLOR_WHITE, who, "You refill the %s with %d units of %s.", query_name(item, NULL), tmp, query_name(filler, NULL));
	}

	fix_player(who);
}

/**
 * Apply a player light object.
 * @param who The player.
 * @param op The light object. */
void apply_player_light(object *who, object *op)
{
	object *tmp;

	if (QUERY_FLAG(op, FLAG_APPLIED))
	{
		if ((QUERY_FLAG(op, FLAG_CURSED) || QUERY_FLAG(op, FLAG_DAMNED)))
		{
			draw_info_format(COLOR_WHITE, who, "No matter how hard you try, you just can't remove it!");
			return;
		}

		if (QUERY_FLAG(op, FLAG_PERM_CURSED))
		{
			SET_FLAG(op, FLAG_CURSED);
		}

		if (QUERY_FLAG(op, FLAG_PERM_DAMNED))
		{
			SET_FLAG(op, FLAG_DAMNED);
		}

		draw_info_format(COLOR_WHITE, who, "You extinguish the %s.", query_name(op, NULL));

		if (!op->env && op->glow_radius)
		{
			adjust_light_source(op->map, op->x, op->y, -(op->glow_radius));
		}

		op->glow_radius = 0;
		CLEAR_FLAG(op, FLAG_APPLIED);
		CLEAR_FLAG(op, FLAG_CHANGING);

		if (op->other_arch && op->other_arch->clone.sub_type & 1)
		{
			op->animation_id = op->other_arch->clone.animation_id;
			SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction);
		}
		else
		{
			CLEAR_FLAG(op, FLAG_ANIMATE);
			op->face = op->arch->clone.face;
		}

		update_object(who, UP_OBJ_FACE);
		fix_player(who);
	}
	else
	{
		/* now the tricky thing: with the first apply cmd, we enlight the light source.
		 * with the second, we apply it. if we unapply a light source, we always extinguish
		 * them implicit. */

		/* LIGHT_APPLY light sources with last_sp (aka glow_radius) 0 are useless -
		 * for example burnt out torches. The burnt out lights are still from same type
		 * because they are perhaps applied from the player as they burnt out
		 * and we don't want a player applying an illegal item. */
		if (!op->last_sp)
		{
			draw_info_format(COLOR_WHITE, who, "The %s can't be lit.", query_name(op, NULL));
			return;
		}

		/* if glow_radius == 0, we have a extinguish light source.
		 * before we can put it in the hand to use it, we have to turn
		 * the light on. */
		if (!op->glow_radius)
		{
			object *op_old;
			/* to delay insertion of object - or it simple re-merge! */
			int tricky_flag = 0;

			/* we have a non permanent source */
			if (op->last_eat)
			{
			/* if not permanent, this is "filled" counter */
				if (!op->stats.food)
				{
					/* no food charges, we can't light it up.
					 * Note that light sources with other_arch set are
					 * non rechargeable lights - like torches. */
					draw_info_format(COLOR_WHITE, who, "You must first refill or recharge the %s.", query_name(op, NULL));
					return;
				}
			}

			/* now we have a filled or permanent, extinguish light source
			 * lets light it - BUT we still have light_radius not active
			 * when we not drop or apply the source. */

			/* the old split code has some side effects -
			 * i force now first a split of #1 per hand */
			op_old = op;

			if (op->nrof > 1)
			{
				object *one = get_object();
				copy_object(op, one, 0);
				op->nrof -= 1;
				one->nrof = 1;

				if (!op->env)
				{
					update_object(op, UP_OBJ_FACE);
				}

				tricky_flag = 1;
				op = one;
			}

			/* light is applied in player inventory - so we
			 * start the 3 apply chain - because it can be taken
			 * in hand. */
			if (op_old->env && op_old->env->type == PLAYER)
			{
				draw_info_format(COLOR_WHITE, who, "You prepare %s to light.", query_name(op, NULL));

				/* we have a non permanent source */
				if (op->last_eat)
				{
					SET_FLAG(op, FLAG_CHANGING);
				}

				if (op->speed)
				{
					SET_FLAG(op, FLAG_ANIMATE);
					/* be sure to get the right anim */
					op->animation_id = op->arch->clone.animation_id;
					SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction);
				}

				if (tricky_flag)
				{
					op = insert_ob_in_ob(op, op_old->env);
				}

				op->glow_radius = (sint8) op->last_sp;
				fix_player(who);
			}
			/* we are not in a player inventory - so simple turn it on */
			else
			{
				draw_info_format(COLOR_WHITE, who, "You light the %s.", query_name(op, NULL));

				/* we have a non permanent source */
				if (op->last_eat)
				{
					SET_FLAG(op, FLAG_CHANGING);
				}

				if (op->speed)
				{
					SET_FLAG(op, FLAG_ANIMATE);
					op->animation_id = op->arch->clone.animation_id;
					SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction);
				}

				if (QUERY_FLAG(op, FLAG_PERM_CURSED))
				{
					SET_FLAG(op, FLAG_CURSED);
				}

				if (QUERY_FLAG(op, FLAG_PERM_DAMNED))
				{
					SET_FLAG(op, FLAG_DAMNED);
				}

				if (tricky_flag)
				{
					if (!op_old->env)
					{
						/* the item WAS before this on this spot - we only turn it on but we don't moved it */
						insert_ob_in_map(op, op_old->map, op_old, INS_NO_WALK_ON);
					}
					else
					{
						op = insert_ob_in_ob(op, op_old->env);
					}
				}

				op->glow_radius = (sint8) op->last_sp;

				if (!op->env && op->glow_radius)
				{
					adjust_light_source(op->map, op->x, op->y, op->glow_radius);
				}

				update_object(op, UP_OBJ_FACE);
			}
		}
		else
		{
			if (op->env && op->env->type == PLAYER)
			{
				/* remove any other applied light source first */
				for (tmp = who->inv; tmp != NULL; tmp = tmp->below)
				{
					if (tmp->type == op->type && QUERY_FLAG(tmp, FLAG_APPLIED) && tmp != op)
					{
						if ((QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED)))
						{
							draw_info(COLOR_WHITE, who, "No matter how hard you try, you just can't remove it!");
							return;
						}

						if (QUERY_FLAG(tmp, FLAG_PERM_CURSED))
						{
							SET_FLAG(tmp, FLAG_CURSED);
						}

						if (QUERY_FLAG(tmp, FLAG_PERM_DAMNED))
						{
							SET_FLAG(tmp, FLAG_DAMNED);
						}

						draw_info_format(COLOR_WHITE, who, "You extinguish the %s.", query_name(tmp, NULL));

						/* on map */
						if (!tmp->env && tmp->glow_radius)
						{
							adjust_light_source(tmp->map, tmp->x, tmp->y, -(tmp->glow_radius));
						}

						tmp->glow_radius = 0;
						CLEAR_FLAG(tmp, FLAG_APPLIED);
						CLEAR_FLAG(tmp, FLAG_CHANGING);

						if (op->other_arch && op->other_arch->clone.sub_type & 1)
						{
							op->animation_id = op->other_arch->clone.animation_id;
							SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction);
						}
						else
						{
							CLEAR_FLAG(op, FLAG_ANIMATE);
							op->face = op->arch->clone.face;
						}

						update_object(tmp, UP_OBJ_FACE);
					}
				}

				draw_info_format(COLOR_WHITE, who, "You apply %s as light.", query_name(op, NULL));
				SET_FLAG(op, FLAG_APPLIED);
				fix_player(who);
				update_object(who, UP_OBJ_FACE);
			}
			/* not part of player inv - turn light off ! */
			else
			{
				if (QUERY_FLAG(op, FLAG_PERM_CURSED))
				{
					SET_FLAG(op, FLAG_CURSED);
				}

				if (QUERY_FLAG(op, FLAG_PERM_DAMNED))
				{
					SET_FLAG(op, FLAG_DAMNED);
				}

				draw_info_format(COLOR_WHITE, who, "You extinguish the %s.", query_name(op, NULL));

				/* on map */
				if (!op->env && op->glow_radius)
				{
					adjust_light_source(op->map, op->x, op->y, -(op->glow_radius));
				}

				op->glow_radius = 0;
				CLEAR_FLAG(op, FLAG_APPLIED);
				CLEAR_FLAG(op, FLAG_CHANGING);

				if (op->other_arch && op->other_arch->clone.sub_type & 1)
				{
					op->animation_id = op->other_arch->clone.animation_id;
					SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction);
				}
				else
				{
					CLEAR_FLAG(op, FLAG_ANIMATE);
					op->face = op->arch->clone.face;
				}

				update_object(op, UP_OBJ_FACE);
			}
		}
	}
}
