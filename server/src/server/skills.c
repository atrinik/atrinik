/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
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
#include <sproto.h>
#include <book.h>

/**
 * Checks for traps on the spaces around the player or in certain
 * objects.
 * @param pl Player searching.
 * @param level Level of the find traps skill.
 * @return Experience gained for finding traps. */
int find_traps(object *pl, int level)
{
	object *tmp, *tmp2;
	mapstruct *m;
	int xt, yt, i, suc = 0, expsum = 0;

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

			for (tmp2 = tmp->inv; tmp2 != NULL; tmp2 = tmp2->below)
			{
				if (tmp2->type == RUNE)
				{
					if (trap_see(pl, tmp2, level))
					{
						trap_show(tmp2, tmp);

						if (tmp2->stats.Cha > 1)
						{
							if (!tmp2->owner || tmp2->owner->type != PLAYER)
							{
								expsum += calc_skill_exp(pl, tmp2, -1);
							}

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
						if (!tmp->owner || tmp->owner->type != PLAYER)
						{
							expsum += calc_skill_exp(pl, tmp, -1);
						}

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
		new_draw_info(NDI_UNIQUE, 0, pl, "You can't detect any trap here.");
	}
	else if (suc == 2)
	{
		new_draw_info(NDI_UNIQUE, 0, pl, "You detect trap signs!");
	}

	return expsum;
}

/**
 * This skill will disarm any previously discovered trap.
 * @param op Player disarming. Must be on a map.
 * @param skill Disarming skill.
 * @return Experience gained to disarm. */
int remove_trap(object *op)
{
	object *tmp, *tmp2;
	mapstruct *m;
	int i, x, y, success = 0;

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
			for (tmp2 = tmp->inv; tmp2 != NULL; tmp2 = tmp2->below)
			{
				if (tmp2->type == RUNE && tmp2->stats.Cha <= 1)
				{
					if (QUERY_FLAG(tmp2, FLAG_SYS_OBJECT) || QUERY_FLAG(tmp2, FLAG_IS_INVISIBLE))
					{
						trap_show(tmp2, tmp);
					}

					if (trap_disarm(op, tmp2, 1) && (!tmp2->owner || tmp2->owner->type != PLAYER))
					{
						tmp2->stats.exp = tmp2->stats.Cha * tmp2->level;
						success += calc_skill_exp(op, tmp2, -1);
					}
					/* Can't continue to disarm after failure */
					else
					{
						return success;
					}
				}
			}

			if (tmp->type == RUNE && tmp->stats.Cha <= 1)
			{
				if (QUERY_FLAG(tmp, FLAG_SYS_OBJECT) || QUERY_FLAG(tmp, FLAG_IS_INVISIBLE))
				{
					trap_show(tmp, tmp);
				}

				if (trap_disarm(op, tmp, 1) && (!tmp->owner || tmp->owner->type != PLAYER))
				{
					tmp->stats.exp = tmp->stats.Cha * tmp->level;
					success += calc_skill_exp(op, tmp, -1);
				}
				/* Can't continue to disarm after failure */
				else
				{
					return success;
				}
			}
		}
	}

	if (!success)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "There is no trap to remove nearby.");
	}

	return success;
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
			new_draw_info_format(NDI_UNIQUE, 0, op, "You can't throw %s.", query_base_name(tmp, NULL));
			tmp = NULL;
		}
		else if (QUERY_FLAG(tmp, FLAG_STARTEQUIP))
		{
			new_draw_info(NDI_UNIQUE, 0, op, "You can't throw god given item!");
			tmp = NULL;
		}
		/* If cursed or damned, we can't unapply it - no throwing. */
		else if (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED))
		{
			new_draw_info_format(NDI_UNIQUE, 0, op, "The %s sticks to your hand!", query_base_name(tmp, NULL));
			tmp = NULL;
		}
		/* It's a throw hybrid weapon - unapply it. Then we will fire it
		 * after this function returns. */
		else
		{
			if (apply_special(op, tmp, AP_UNAPPLY | AP_NO_MERGE))
			{
				LOG(llevBug, "BUG: find_throw_ob(): couldn't unapply throwing item %s from %s\n", query_name(tmp, NULL), query_name(op, NULL));
				tmp = NULL;
			}
		}
	}
	else
	{
		/* Not weapon nor throwable - no throwing. */
		if ((tmp->type != WEAPON && tmp->type != POTION) && !QUERY_FLAG(tmp, FLAG_IS_THROWN))
		{
			new_draw_info_format(NDI_UNIQUE, 0, op, "You can't throw %s.", query_base_name(tmp, NULL));
			tmp = NULL;
		}
		/* Special message for throw hybrid weapons. */
		else if (tmp->type == WEAPON)
		{
			new_draw_info_format(NDI_UNIQUE, 0, op, "You must apply the %s first.", query_base_name(tmp, NULL));
			tmp = NULL;
		}
	}

	return tmp;
}

/**
 * We construct the 'carrier' object in which we will insert the object
 * that is being thrown. This combination  becomes the 'thrown object'.
 * @param orig Object to wrap.
 * @return Object to throw. */
static object *make_throw_ob(object *orig)
{
	object *toss_item;

	if (!orig)
	{
		return NULL;
	}

	toss_item = get_object();

	if (QUERY_FLAG(orig, FLAG_APPLIED))
	{
		LOG(llevBug, "BUG: make_throw_ob(): ob is applied (%s)\n", query_name(orig, NULL));
		/* Insufficient workaround, but better than nothing. */
		CLEAR_FLAG(orig, FLAG_APPLIED);
	}

	copy_object(orig, toss_item);
	toss_item->type = THROWN_OBJ;
	SET_FLAG(toss_item, FLAG_IS_MISSILE);
	CLEAR_FLAG(toss_item, FLAG_CHANGING);
	/* Default damage */
	toss_item->stats.dam = 0;
#ifdef DEBUG_THROW
	LOG(llevDebug, "DEBUG: inserting %s(%d) in toss_item(%d)\n", orig->name, orig->count, toss_item->count);
#endif
	insert_ob_in_ob(orig, toss_item);

	return toss_item;
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
	int eff_str = 0, str = op->stats.Str, dam = 0, weight_f = 0;
	int target_throw = 0;
	rv_vector range_vector;
	float str_factor = 1.0f, load_factor = 1.0f, item_factor = 1.0f;

	if (throw_ob == NULL)
	{
		if (op->type == PLAYER)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "You have nothing to throw.");
		}

		return;
	}

	if (QUERY_FLAG(throw_ob, FLAG_STARTEQUIP))
	{
		if (op->type == PLAYER)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "The gods won't let you throw that.");
		}

		return;
	}

	/* Because throwing effectiveness must be reduced by the
	 * encumbrance of the thrower and weight of the object. Thus,
	 * we use the concept of 'effective strength' as defined below.  */
	/* if str exceeds MAX_STAT (30, eg giants), lets assign a str_factor > 1 */
	if (str > MAX_STAT)
	{
		str_factor = (float) str / (float) MAX_STAT;
		str = MAX_STAT;
	}

	/* we need something more clever here, using weight_limit */
	load_factor = 1.0f;

	/* Lighter items are thrown harder, farther, faster. */
	if (throw_ob->weight <= 0)
	{
		new_draw_info_format(NDI_UNIQUE, 0, op, "You can't throw %s.\n", query_base_name(throw_ob, NULL));
		return;
	}

	eff_str = (int) ((float) str * (load_factor < 1.0f ? load_factor : 1.0f));
	eff_str = (int) ((float) eff_str * item_factor * str_factor);

	/* alas, arrays limit us to a value of MAX_STAT (30). Use str_factor to
	 * account for super-strong throwers. */
	if (eff_str > MAX_STAT)
	{
		eff_str = MAX_STAT;
	}

	if (eff_str < 1)
	{
		eff_str = 1;
	}

#ifdef DEBUG_THROW
	LOG(llevDebug, "%s carries %d, eff_str=%d\n", op->name, op->carrying, eff_str);
	LOG(llevDebug, " max_c=%d, item_f=%f, load_f=%f, str=%d\n", maxc, item_factor, load_factor, op->stats.Str);
	LOG(llevDebug, " str_factor=%f\n", str_factor);
	LOG(llevDebug, " item %s weight= %d\n", throw_ob->name, throw_ob->weight);
#endif

	/* These are throwing objects left to the player */
	left = throw_ob;
	left_cont = left->env;
	left_tag = left->count;

	/* Sometimes get_split_ob can't split an object (because op->nrof==0?)
	 * and returns NULL. We must use 'left' then */
	if ((throw_ob = get_split_ob(throw_ob, 1, NULL, 0)) == NULL)
	{
#ifdef DEBUG_THROW
		LOG(llevDebug, " get_split_ob(): failed to split throw ob %s\n", left->name);
#endif
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
		target_throw = 1;
	}
	else
	{
		throw_ob->stats.grace = 0;
	}

	/* Three things here prevent a throw, you aimed at your feet, you
	 * have no effective throwing strength, or you threw at a wall */
	if (!dir || eff_str <= 1 || wall(op->map, op->x + freearr_x[dir], op->y + freearr_y[dir]))
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

		throw_ob->x = op->x;
		throw_ob->y = op->y;

		if (!insert_ob_in_map(throw_ob, op->map, op, 0))
		{
			return;
		}

		if (op->type == PLAYER)
		{
			if (eff_str <= 1)
			{
				new_draw_info_format(NDI_UNIQUE, 0, op, "Your load is so heavy you drop %s to the ground.", query_name(throw_ob, NULL));
			}
			else if (!dir)
			{
				new_draw_info_format(NDI_UNIQUE, 0,op, "You drop %s at the ground.", query_name(throw_ob, NULL));
			}
			else
			{
				new_draw_info(NDI_UNIQUE, 0, op, "Something is in the way.");
			}
		}

		return;
	}

	/* Make a thrown object -- insert real object in a 'carrier' object.
	 * If unsuccessfull at making the "thrown_obj", we just reinsert
	 * the original object back into inventory and exit */
	if ((toss_item = make_throw_ob(throw_ob)))
	{
		throw_ob = toss_item;
	}
	else
	{
		insert_ob_in_ob(throw_ob, op);
		return;
	}

	/* At some point in the attack code, the actual real object (op->inv)
	 * becomes the hitter.  As such, we need to make sure that has a proper
	 * owner value so exp goes to the right place. */
	set_owner(throw_ob, op);
	set_owner(throw_ob->inv, op);
	throw_ob->direction = dir;
	throw_ob->x = op->x;
	throw_ob->y = op->y;

	/* Targetting throwing */
	if (target_throw)
	{
		int dx = range_vector.distance_x, dy = range_vector.distance_y, stepx, stepy;

		if (dy < 0)
		{
			dy = -dy;
			stepy = -1;
		}
		else
		{
			stepy = 1;
		}

		if (dx < 0)
		{
			dx = -dx;
			stepx = -1;
		}
		else
		{
			stepx = 1;
		}

		throw_ob->stats.hp = dx << 1;
		throw_ob->stats.sp = dy << 1;
		throw_ob->stats.maxhp = stepx;
		throw_ob->stats.maxsp = stepy;
		throw_ob->stats.grace = 666;

		/* Fraction */
		if (dx > dy)
		{
			/* Same as 2*dy - dx */
			throw_ob->stats.exp = (dy << 1) - dx;
		}
		else
		{
			/* Same as 2*dx - dy */
			throw_ob->stats.exp = (dx << 1) - dy;
		}
	}

	/* The damage bonus from the force of the throw */
	dam = (int) (str_factor * (float) dam_bonus[eff_str]);

	/* Now, lets adjust the properties of the thrown_ob. */

	/* Speed */
	throw_ob->speed = (speed_bonus[eff_str] + 1.0f) / 1.5f;
	/* No faster than an arrow! */
	throw_ob->speed = MIN(1.0f, throw_ob->speed);

	/* Item damage. Eff_str and item weight influence damage done */
	weight_f = (throw_ob->weight / 2000) > MAX_STAT ? MAX_STAT : (throw_ob->weight / 2000);
	throw_ob->stats.dam += (dam / 3) + dam_bonus[weight_f] + (throw_ob->weight / 15000) - 2;

	/* Chance of breaking. Proportional to force used and weight of item */
	throw_ob->stats.food = (dam / 2) + (throw_ob->weight / 60000);

	/* Now we get the wc from the used skill! this will allow customized skill */
	if ((tmp_op = SK_skill(op)))
	{
		throw_ob->stats.wc = tmp_op->last_heal;
	}
	/* Mobs */
	else
	{
		throw_ob->stats.wc = 10;
	}

	throw_ob->stats.wc_range = op->stats.wc_range;

	if (QUERY_FLAG(throw_ob->inv, FLAG_IS_THROWN))
	{
		throw_ob->stats.dam = throw_ob->inv->stats.dam + throw_ob->magic;
		throw_ob->stats.wc += throw_ob->magic + throw_ob->inv->stats.wc;

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
	else
	{
		/* Some materials will adjust properties.. */
		if (throw_ob->material & M_LEATHER)
		{
			throw_ob->stats.dam -= 1;
			throw_ob->stats.food -= 10;
		}

		if (throw_ob->material & M_GLASS)
		{
			throw_ob->stats.food += 60;
		}

		if (throw_ob->material & M_ORGANIC)
		{
			throw_ob->stats.dam -= 3;
			throw_ob->stats.food += 55;
		}

		if (throw_ob->material & M_PAPER || throw_ob->material & M_CLOTH)
		{
			throw_ob->stats.dam -= 5;
			throw_ob->speed *= 0.8f;
			throw_ob->stats.wc += 3;
			throw_ob->stats.food -= 30;
		}

		/* Light obj have more wind resistance, fly slower */
		if (throw_ob->weight > 500)
		{
			throw_ob->speed *= 0.8f;
		}

		if (throw_ob->weight > 50)
		{
			throw_ob->speed *= 0.5f;
		}
	}

	if (throw_ob->stats.dam < 0)
	{
		throw_ob->stats.dam = 0;
	}

	if (throw_ob->stats.food < 0)
	{
		throw_ob->stats.food = 0;
	}

	if (throw_ob->stats.food > 100)
	{
		throw_ob->stats.food = 100;
	}

	if (throw_ob->stats.wc > 30)
	{
		throw_ob->stats.wc = 30;
	}

	update_ob_speed(throw_ob);
	throw_ob->speed_left = 0;
	throw_ob->map = op->map;

	SET_MULTI_FLAG(throw_ob, FLAG_FLYING);
	SET_FLAG(throw_ob, FLAG_FLY_ON);
	SET_FLAG(throw_ob, FLAG_WALK_ON);

	play_sound_map(op->map, op->x, op->y, SOUND_THROW, SOUND_NORMAL);

	/* Trigger the THROW event */
	trigger_event(EVENT_THROW, op, throw_ob->inv, throw_ob, NULL, 0, 0, 0, SCRIPT_FIX_ACTIVATOR);

#ifdef DEBUG_THROW
	LOG(llevDebug, " pause_f=%d \n", pause_f);
	LOG(llevDebug, " %s stats: wc=%d dam=%d dist=%d spd=%f break=%d\n", throw_ob->name, throw_ob->stats.wc, throw_ob->stats.dam, throw_ob->last_sp, throw_ob->speed, throw_ob->stats.food);
	LOG(llevDebug, "inserting tossitem (%d) into map\n", throw_ob->count);
#endif

	if (insert_ob_in_map(throw_ob, op->map, op, 0))
	{
		move_arrow(throw_ob);
	}
}
