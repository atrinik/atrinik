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
 * This file contains core skill handling. */

#include <global.h>
#include <book.h>

/**
 * Checks for traps on the spaces around the player or in certain
 * objects.
 * @param pl Player searching.
 * @param level Level of the find traps skill.
 * @return Experience gained for finding traps. */
sint64 find_traps(object *pl, int level)
{
	object *tmp, *tmp2;
	mapstruct *m;
	int xt, yt, i, suc = 0;

	/* First we search all around us for runes and traps, which are
	 * all type RUNE */
	for (i = 0; i < 9; i++)
	{
		/* Check everything in the square for trapness */
		xt = pl->x + freearr_x[i];
		yt = pl->y + freearr_y[i];

		if (!(m = get_map_from_coord(pl->map, &xt, &yt)))
		{
			continue;
		}

		for (tmp = get_map_ob(m, xt, yt); tmp != NULL; tmp = tmp->above)
		{
			/* And now we'd better do an inventory traversal of each
			 * of these objects' inventory */
			if (pl != tmp && (tmp->type == PLAYER || tmp->type == MONSTER))
			{
				continue;
			}

			for (tmp2 = tmp->inv; tmp2; tmp2 = tmp2->below)
			{
				if (tmp2->type == RUNE)
				{
					if (trap_see(pl, tmp2, level))
					{
						trap_show(tmp2, tmp);

						if (tmp2->stats.Cha > 1)
						{
							/* Unhide the trap */
							tmp2->stats.Cha = 1;
						}

						if (!suc)
						{
							suc = 1;
						}
					}
					else
					{
						/* Give out a "we have found signs of traps"
						 * if the traps level is not 1.8 times higher. */
						if (tmp2->level <= (level * 1.8f))
						{
							suc = 2;
						}
					}
				}
			}

			if (tmp->type == RUNE)
			{
				if (trap_see(pl, tmp, level))
				{
					trap_show(tmp, tmp);

					if (tmp->stats.Cha > 1)
					{
						/* Unhide the trap */
						tmp->stats.Cha = 1;
					}

					if (!suc)
					{
						suc = 1;
					}
				}
				else
				{
					/* Give out a "we have found signs of traps"
					 * if the traps level is not 1.8 times higher. */
					if (tmp->level <= (level * 1.8f))
					{
						suc = 2;
					}
				}
			}
		}
	}

	if (!suc)
	{
		new_draw_info(NDI_UNIQUE, pl, "You can't detect any trap here.");
	}
	else if (suc == 2)
	{
		new_draw_info(NDI_UNIQUE, pl, "You detect trap signs!");
	}

	return 0;
}

/**
 * This skill will disarm any previously discovered trap.
 * @param op Player disarming.
 * @return 0. */
sint64 remove_trap(object *op)
{
	object *tmp, *tmp2;
	mapstruct *m;
	int i, x, y;

	for (i = 0; i < 9; i++)
	{
		x = op->x + freearr_x[i];
		y = op->y + freearr_y[i];

		if (!(m = get_map_from_coord(op->map, &x, &y)))
		{
			continue;
		}

		/* Check everything in the square for trapness */
		for (tmp = get_map_ob(m, x, y); tmp != NULL; tmp = tmp->above)
		{
			/* And now we'd better do an inventory traversal of each
			 * of these objects' inventory */
			for (tmp2 = tmp->inv; tmp2; tmp2 = tmp2->below)
			{
				if (tmp2->type == RUNE && tmp2->stats.Cha <= 1)
				{
					if (QUERY_FLAG(tmp2, FLAG_SYS_OBJECT) || QUERY_FLAG(tmp2, FLAG_IS_INVISIBLE))
					{
						trap_show(tmp2, tmp);
					}

					trap_disarm(op, tmp2);
					return 0;
				}
			}

			if (tmp->type == RUNE && tmp->stats.Cha <= 1)
			{
				if (QUERY_FLAG(tmp, FLAG_SYS_OBJECT) || QUERY_FLAG(tmp, FLAG_IS_INVISIBLE))
				{
					trap_show(tmp, tmp);
				}

				trap_disarm(op, tmp);
				return 0;
			}
		}
	}

	new_draw_info(NDI_UNIQUE, op, "There is no trap to remove nearby.");
	return 0;
}

/**
 * Requests an object to throw by tag reported by the client.
 *
 * We search for it in the inventory of the owner (you've got to be
 * carrying something in order to throw it).
 *
 * Also checks to see if the object is throwable (ie, not applied, cursed
 * worn, etc).
 * @param op Object to search in.
 * @param tag Tag of the object we're looking for.
 * @return The found object or NULL. */
object *find_throw_tag(object *op, tag_t tag)
{
	object *tmp;

	/* Look through the inventory. */
	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		/* Can't toss invisible or inv-locked items */
		if (IS_SYS_INVISIBLE(tmp) || QUERY_FLAG(tmp, FLAG_INV_LOCKED))
		{
			continue;
		}

		if (tmp->count == tag)
		{
			break;
		}
	}

	if (!tmp)
	{
		return NULL;
	}

	if (QUERY_FLAG(tmp, FLAG_APPLIED))
	{
		/* We can't apply throwing stuff like darts, so this must be a
		 * weapon. Skip if not OR when it can't be thrown OR when it is
		 * startequip which can't be dropped. */
		if (tmp->type != WEAPON || !QUERY_FLAG(tmp, FLAG_IS_THROWN))
		{
			new_draw_info_format(NDI_UNIQUE, op, "You can't throw %s.", query_base_name(tmp, NULL));
			return NULL;
		}
		else if (QUERY_FLAG(tmp, FLAG_STARTEQUIP))
		{
			new_draw_info(NDI_UNIQUE, op, "You can't throw god-given item!");
			return NULL;
		}
		/* If cursed or damned, we can't unapply it - no throwing. */
		else if (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED))
		{
			new_draw_info_format(NDI_UNIQUE, op, "The %s sticks to your hand!", query_base_name(tmp, NULL));
			return NULL;
		}
		/* It's a throw hybrid weapon - unapply it. Then we will fire it
		 * after this function returns. */
		else
		{
			if (apply_special(op, tmp, AP_UNAPPLY | AP_NO_MERGE))
			{
				LOG(llevBug, "BUG: find_throw_ob(): couldn't unapply throwing item %s from %s\n", query_name(tmp, NULL), query_name(op, NULL));
				return NULL;
			}
		}
	}
	else
	{
		/* Not weapon nor throwable - no throwing. */
		if ((tmp->type != WEAPON && tmp->type != POTION) && !QUERY_FLAG(tmp, FLAG_IS_THROWN))
		{
			new_draw_info_format(NDI_UNIQUE, op, "You can't throw %s.", query_base_name(tmp, NULL));
			return NULL;
		}
		/* Special message for throw hybrid weapons. */
		else if (tmp->type == WEAPON)
		{
			new_draw_info_format(NDI_UNIQUE, op, "You must apply the %s first.", query_base_name(tmp, NULL));
			return NULL;
		}
	}

	return tmp;
}

/**
 * Op throws any object toss_item.
 * @param op Living thing throwing something.
 * @param toss_item Item thrown.
 * @param dir Direction to throw. */
void do_throw(object *op, object *toss_item, int dir)
{
	object *left_cont, *throw_ob = toss_item, *left = NULL, *tmp_op;
	tag_t left_tag;
	rv_vector range_vector;

	if (!throw_ob)
	{
		if (op->type == PLAYER)
		{
			new_draw_info(NDI_UNIQUE, op, "You have nothing to throw.");
		}

		return;
	}

	if (QUERY_FLAG(throw_ob, FLAG_STARTEQUIP))
	{
		if (op->type == PLAYER)
		{
			new_draw_info(NDI_UNIQUE, op, "The gods won't let you throw that.");
		}

		return;
	}

	if (throw_ob->weight <= 0)
	{
		new_draw_info_format(NDI_UNIQUE, op, "You can't throw %s.\n", query_base_name(throw_ob, NULL));
		return;
	}

	/* These are throwing objects left to the player */
	left = throw_ob;
	left_cont = left->env;
	left_tag = left->count;

	/* Sometimes get_split_ob can't split an object (because op->nrof==0?)
	 * and returns NULL. We must use 'left' then */
	if ((throw_ob = get_split_ob(throw_ob, 1, NULL, 0)) == NULL)
	{
		throw_ob = left;
		remove_ob(left);
		check_walk_off(left, NULL, MOVE_APPLY_VANISHED);

		if (op->type == PLAYER)
		{
			esrv_del_item(CONTR(op), left->count, left->env);
		}
	}
	else if (op->type == PLAYER)
	{
		if (was_destroyed(left, left_tag))
		{
			esrv_del_item(CONTR(op), left_tag, left_cont);
		}
		else
		{
			esrv_update_item(UPD_NROF, op, left);
		}
	}

	/* Special case: throwing powdery substances like dust, dirt */
	if (QUERY_FLAG(throw_ob, FLAG_DUST))
	{
		cast_dust(op, throw_ob, dir);

		/* update the shooting speed for the player action timer.
		 * We init the used skill with it - its not calculated here.
		 * cast_dust() can change the used skill... */
		if (op->type == PLAYER)
		{
			op->chosen_skill->stats.maxsp = throw_ob->last_grace;
		}

		return;
	}

	/* Targetting throwing */
	if (!dir && op->type == PLAYER && OBJECT_VALID(CONTR(op)->target_object, CONTR(op)->target_object_count))
	{
		dir = get_dir_to_target(op, CONTR(op)->target_object, &range_vector);
	}

	/* Three things here prevent a throw, you aimed at your feet, you
	 * have no effective throwing strength, or you threw at a wall */
	if (!dir || wall(op->map, op->x + freearr_x[dir], op->y + freearr_y[dir]))
	{
		/* Bounces off 'wall', and drops to feet */
		if (!QUERY_FLAG(throw_ob, FLAG_REMOVED))
		{
			remove_ob(throw_ob);

			if (check_walk_off(throw_ob, NULL, MOVE_APPLY_MOVE) != CHECK_WALK_OK)
			{
				return;
			}
		}

		if (op->type == PLAYER)
		{
			if (!dir)
			{
				new_draw_info_format(NDI_UNIQUE, op, "You drop %s at the ground.", query_name(throw_ob, NULL));
			}
			else
			{
				new_draw_info(NDI_UNIQUE, op, "Something is in the way.");
			}
		}

		throw_ob->x = op->x;
		throw_ob->y = op->y;

		if (!insert_ob_in_map(throw_ob, op->map, op, 0))
		{
			return;
		}

		return;
	}

	set_owner(throw_ob, op);
	set_owner(throw_ob->inv, op);
	throw_ob->direction = dir;
	throw_ob->x = op->x;
	throw_ob->y = op->y;

	/* Save original wc and dam */
	throw_ob->last_heal = throw_ob->stats.wc;
	throw_ob->stats.hp = throw_ob->stats.dam;

	/* Speed */
	throw_ob->speed = MIN(1.0f, (speed_bonus[op->stats.Str] + 1.0f) / 1.5f);

	/* Now we get the wc from the used skill. */
	if ((tmp_op = SK_skill(op)))
	{
		throw_ob->stats.wc += tmp_op->last_heal;
	}
	/* Monsters */
	else
	{
		throw_ob->stats.wc += 10;
	}

	throw_ob->stats.wc_range = op->stats.wc_range;

	if (QUERY_FLAG(throw_ob, FLAG_IS_THROWN))
	{
		throw_ob->stats.dam += throw_ob->magic;
		throw_ob->stats.wc += throw_ob->magic;

		/* Adjust for players */
		if (op->type == PLAYER)
		{
			op->chosen_skill->stats.maxsp = throw_ob->last_grace;
			throw_ob->stats.dam = FABS((int) ((float) (throw_ob->stats.dam + dam_bonus[op->stats.Str] / 2) * LEVEL_DAMAGE(SK_level(op))));
			throw_ob->stats.wc += thaco_bonus[op->stats.Dex] + SK_level(op);
		}
		else
		{
			throw_ob->stats.dam = FABS((int) ((float) (throw_ob->stats.dam) * LEVEL_DAMAGE(op->level)));
			throw_ob->stats.wc += 10 + op->level;
		}

		throw_ob->stats.grace = throw_ob->last_sp;
		throw_ob->stats.maxgrace = 60 + (RANDOM() % 12);

		/* Only throw objects get directional faces */
		if (GET_ANIM_ID(throw_ob) && NUM_ANIMATIONS(throw_ob))
		{
			SET_ANIMATION(throw_ob, (NUM_ANIMATIONS(throw_ob) / NUM_FACINGS(throw_ob)) * dir);
		}

		/* Adjust damage with item condition */
		throw_ob->stats.dam = (sint16) (((float) throw_ob->stats.dam / 100.0f) * (float) throw_ob->item_condition);
	}

	if (throw_ob->stats.dam < 0)
	{
		throw_ob->stats.dam = 0;
	}

	update_ob_speed(throw_ob);
	throw_ob->speed_left = 0;

	SET_MULTI_FLAG(throw_ob, FLAG_FLYING);
	SET_FLAG(throw_ob, FLAG_FLY_ON);
	SET_FLAG(throw_ob, FLAG_WALK_ON);

	play_sound_map(op->map, CMD_SOUND_EFFECT, "throw.ogg", op->x, op->y, 0, 0);

	/* Trigger the THROW event */
	trigger_event(EVENT_THROW, op, throw_ob, NULL, NULL, 0, 0, 0, SCRIPT_FIX_ACTIVATOR);

	if (insert_ob_in_map(throw_ob, op->map, op, 0))
	{
		move_arrow(throw_ob);
	}
}
