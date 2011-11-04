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
 * Various spell effects. */

#include <global.h>

/**
 * This is really used mostly for spell fumbles at the like.
 * @param op What is casting this.
 * @param tmp Object to propagate.
 * @param lvl How nasty should the propagation be. */
void cast_magic_storm(object *op, object *tmp, int lvl)
{
	/* Error */
	if (!tmp)
	{
		return;
	}

	tmp->level = SK_level(op);
	tmp->x = op->x;
	tmp->y = op->y;

	/* increase the area of destruction */
	tmp->stats.hp += lvl / 5;
	/* nasty recoils! */
	tmp->stats.dam = lvl;
	tmp->stats.maxhp = tmp->count;
	insert_ob_in_map(tmp, op->map, op, 0);
}

/**
 * Recharge wands.
 * @param op Who is casting.
 * @retval 0 Nothing happened.
 * @retval 1 Wand was recharged, or destroyed. */
int recharge(object *op)
{
	object *wand = find_marked_object(op);
	int cap;

	if (wand == NULL || wand->type != WAND)
	{
		draw_info(COLOR_RED, op, "You need to mark the wand you want to recharge.");
		return 0;
	}

	if (wand->stats.sp < 0 || wand->stats.sp >= NROFREALSPELLS || !spells[wand->stats.sp].charges)
	{
		draw_info_format(COLOR_RED, op, "The %s cannot be recharged.", query_name(wand, NULL));
		return 0;
	}

	if (!(rndm(0, 6)))
	{
		draw_info_format(COLOR_WHITE, op, "The %s vibrates violently, then explodes!", query_name(wand, NULL));
		play_sound_map(op->map, CMD_SOUND_EFFECT, "explosion.ogg", op->x, op->y, 0, 0);
		remove_ob(wand);
		return 1;
	}

	draw_info_format(COLOR_WHITE, op, "The %s glows with power.", query_name(wand, NULL));

	wand->stats.food += 12 + rndm(1, spells[wand->stats.sp].charges);
	cap = spells[wand->stats.sp].charges + 12;

	/* Place a cap on it. */
	if (wand->stats.food > cap)
	{
		wand->stats.food = cap;
	}

	if (wand->arch && QUERY_FLAG(&wand->arch->clone, FLAG_ANIMATE))
	{
		SET_FLAG(wand, FLAG_ANIMATE);
		wand->speed = wand->arch->clone.speed;
		update_ob_speed(wand);
	}

	return 1;
}

/**
 * Create food.
 *
 * Allows the choice of what sort of food object to make.
 * If stringarg is NULL, it will create food dependent on level.
 * @param op Who is casting.
 * @param caster What is casting.
 * @param dir Casting direction.
 * @param stringarg Optional parameter specifying what kind of items to
 * create.
 * @retval 0 No food created.
 * @retval 1 Food was created. */
int cast_create_food(object *op, object *caster, int dir, const char *stringarg)
{
	int food_value;
	archetype *at = NULL;
	object *new_op;

	food_value = 50 * SP_level_dam_adjust(caster, SP_CREATE_FOOD, -1, 0);

	if (stringarg)
	{
		at = find_archetype(stringarg);

		if (at == NULL || ((at->clone.type != FOOD && at->clone.type != DRINK) || (at->clone.stats.food > food_value)))
		{
			stringarg = NULL;
		}
	}

	if (!stringarg)
	{
		archetype *at_tmp;

		/* We try to find the archetype with the maximum food value.
		 * This removes the dependency of hard coded food values in this
		 * function, and addition of new food types is automatically added.
		 * We don't use flesh types because the weight values of those need
		 * to be altered from the donor. */

		/* We assume the food items don't have multiple parts */
		for (at_tmp = first_archetype; at_tmp != NULL; at_tmp = at_tmp->next)
		{
			if (at_tmp->clone.type == FOOD || at_tmp->clone.type == DRINK)
			{
				/* Basically, if the food value is something that is creatable
				 * under the limits of the spell and it is higher than
				 * the item we have now, take it instead. */
				if (at_tmp->clone.stats.food <= food_value && (!at || at_tmp->clone.stats.food > at->clone.stats.food))
				{
					at = at_tmp;
				}
			}
		}
	}

	/* Pretty unlikely (there are some very low food items), but you
	 * never know */
	if (!at)
	{
		draw_info(COLOR_WHITE, op, "You don't have enough experience to create any food.");
		return 0;
	}

	food_value /= at->clone.stats.food;
	new_op = get_object();
	copy_object(&at->clone, new_op, 0);
	new_op->nrof = food_value;

	new_op->value = 0;
	SET_FLAG(new_op, FLAG_STARTEQUIP);
	SET_FLAG(new_op, FLAG_IDENTIFIED);

	if (new_op->nrof < 1)
	{
		new_op->nrof = 1;
	}

	cast_create_obj(op, new_op, dir);
	return 1;
}

/**
 * Try to get information about a living thing.
 * @param op Who is casting.
 * @retval 0 Nothing probed.
 * @retval 1 Something was probed. */
int probe(object *op)
{
	object *tmp;

	for (tmp = GET_MAP_OB(op->map, op->x, op->y); tmp != NULL; tmp = tmp->above)
	{
		if (IS_LIVE(tmp))
		{
			if (op->owner && op->owner->type == PLAYER)
			{
				draw_info_format(COLOR_WHITE, op->owner, "Your probe analyzes %s.", tmp->name);

				if (tmp->head != NULL)
				{
					tmp = tmp->head;
				}

				examine(op->owner, tmp, NULL);
				return 1;
			}
		}
	}

	return 0;
}

/**
 * Word of recall causes the player to return 'home'.
 *
 * We put a force into the player object, so that there is a time delay
 * effect.
 * @param op Who is casting.
 * @param caster What is casting.
 * @return 1 on success, 0 otherwise. */
int cast_wor(object *op, object *caster)
{
	object *dummy;

	if (op->type != PLAYER)
	{
		return 0;
	}

	if (blocks_magic(op->map, op->x, op->y))
	{
		draw_info(COLOR_WHITE, op, "Something blocks your spell.");
		return 0;
	}

	dummy = get_archetype("force");

	if (dummy == NULL)
	{
		LOG(llevBug, "cast_wor(): get_archetype failed (%s - %s)!\n", query_name(op, NULL), query_name(caster, NULL));
		return 0;
	}

	/* Better insert the spell in the player */
	if (op->owner)
	{
		op = op->owner;
	}

	dummy->speed = 0.002f * ((float) (spells[SP_WOR].bdur + SP_level_strength_adjust(caster, SP_WOR)));
	update_ob_speed(dummy);
	dummy->speed_left = -1;
	dummy->type = WORD_OF_RECALL;

	FREE_AND_COPY_HASH(EXIT_PATH(dummy), CONTR(op)->savebed_map);
	EXIT_X(dummy) = CONTR(op)->bed_x;
	EXIT_Y(dummy) = CONTR(op)->bed_y;

	insert_ob_in_ob(dummy, op);
	draw_info(COLOR_WHITE, op, "You feel a force starting to build up inside you.");

	return 1;
}

/**
 * Hit all enemies around the caster.
 * @param op Who is casting.
 * @param caster What object is casting.
 * @param dam Base damage to do.
 * @param attacktype Attacktype.
 * @return 1. */
int cast_destruction(object *op, object *caster, int dam, int attacktype)
{
	int i, j, range, xt, yt;
	object *tmp, *hitter;
	mapstruct *m;

	/* The hitter object. */
	hitter = arch_to_object(spellarch[SP_DESTRUCTION]);
	set_owner(hitter, op);
	hitter->level = SK_level(caster);

	/* Calculate maximum range of the spell */
	range = MAX(SP_level_strength_adjust(caster, SP_DESTRUCTION), spells[SP_DESTRUCTION].bdur);
	dam += SP_level_dam_adjust(caster, SP_DESTRUCTION, -1, 0);

	for (i = -range; i < range + 1; i++)
	{
		for (j = -range; j < range + 1; j++)
		{
			xt = op->x + i;
			yt = op->y + j;

			if (!(m = get_map_from_coord(op->map, &xt, &yt)))
			{
				continue;
			}

			/* Nothing alive here? Move on... */
			if (!(GET_MAP_FLAGS(m, xt, yt) & (P_IS_MONSTER | P_IS_PLAYER)))
			{
				continue;
			}

			/* Try to get an object to hit */
			for (tmp = GET_MAP_OB(m, xt, yt); tmp; tmp = tmp->above)
			{
				/* Get head. */
				if (tmp->head)
				{
					tmp = tmp->head;
				}

				/* Skip the caster and not alive objects. */
				if (tmp == caster || !IS_LIVE(tmp) || spell_attack_missed(hitter, tmp))
				{
					continue;
				}

				if (!is_friend_of(op, tmp))
				{
					sint16 damage = dam;

					if (tmp->quick_pos)
					{
						damage /= (tmp->quick_pos >> 4) + 1;
					}

					hit_player(tmp, damage, hitter, attacktype);
					break;
				}
			}
		}
	}

	return 1;
}

/**
 * Cast an area of effect healing spell.
 * @param op Object.
 * @param level Level of the spell being cast.
 * @param type ID of the spell.
 * @return 1 on success, 0 on failure. */
int cast_heal_around(object *op, int level, int type)
{
	int success = 0;

	switch (type)
	{
		case SP_RAIN_HEAL:
		{
			int i, x, y;
			mapstruct *m;
			object *tmp;

			for (i = 0; i <= SIZEOFFREE1; i++)
			{
				x = op->x + freearr_x[i];
				y = op->y + freearr_y[i];

				if (!(m = get_map_from_coord(op->map, &x, &y)))
				{
					continue;
				}

				if (!(GET_MAP_FLAGS(m, x, y) & (P_IS_MONSTER | P_IS_PLAYER)))
				{
					continue;
				}

				for (tmp = GET_MAP_OB_LAYER(m, x, y, LAYER_LIVING, 0); tmp && tmp->layer == LAYER_LIVING; tmp = tmp->above)
				{
					tmp = HEAD(tmp);

					if (tmp == op || !IS_LIVE(tmp) || !is_friend_of(op, tmp))
					{
						continue;
					}

					cast_heal(op, level, tmp, SP_MINOR_HEAL);
					success = 1;
				}
			}

			break;
		}

		case SP_PARTY_HEAL:
		{
			objectlink *ol;

			if (op->type != PLAYER)
			{
				return 0;
			}
			else if (!CONTR(op)->party)
			{
				draw_info(COLOR_WHITE, op, "You need to be in a party to cast this spell.");
				return 0;
			}

			for (ol = CONTR(op)->party->members; ol; ol = ol->next)
			{
				if (on_same_map(ol->objlink.ob, op))
				{
					cast_heal(op, level, ol->objlink.ob, SP_MINOR_HEAL);
				}
			}

			success = 1;
			break;
		}
	}

	return success;
}

/**
 * Heals something.
 * @param op Who is casting.
 * @param level Level of the skill.
 * @param target Target.
 * @param spell_type ID of the spell. */
int cast_heal(object *op, int level, object *target, int spell_type)
{
	archetype *at;
	object *temp;
	int heal = 0, success = 0;

	if (!op || !target)
	{
		LOG(llevBug, "cast_heal(): target or caster NULL (op: %s target: %s)\n", query_name(op, NULL), query_name(target, NULL));
		return 0;
	}

	switch (spell_type)
	{
		case SP_CURE_DISEASE:
			if (cure_disease(target, op))
			{
				success = 1;
			}

			break;

		case SP_CURE_POISON:
			at = find_archetype("poisoning");

			if (op != target && target->type == PLAYER)
			{
				draw_info_format(COLOR_WHITE, target, "%s casts cure poison on you!", op->name ? op->name : "Someone");
			}

			if (op != target && op->type == PLAYER)
			{
				draw_info_format(COLOR_WHITE, op, "You cast cure poison on %s!", target->name ? target->name : "someone");
			}

			for (temp = target->inv; temp != NULL; temp = temp->below)
			{
				if (temp->arch == at)
				{
					success = 1;
					temp->stats.food = 1;
				}
			}

			if (success)
			{
				if (target->type == PLAYER)
				{
					draw_info(COLOR_WHITE, target, "Your body feels cleansed.");
				}

				if (op != target && op->type == PLAYER)
				{
					draw_info_format(COLOR_WHITE, op, "%s's body seems cleansed.", target->name ? target->name : "Someone");
				}
			}
			else
			{
				if (target->type == PLAYER)
				{
					draw_info(COLOR_WHITE, target, "You are not poisoned.");
				}

				if (op != target && op->type == PLAYER)
				{
					draw_info_format(COLOR_WHITE, op, "%s is not poisoned.", target->name ? target->name : "Someone");
				}
			}

			break;

		case SP_CURE_CONFUSION:
			at = find_archetype("confusion");

			if (op != target && target->type == PLAYER)
			{
				draw_info_format(COLOR_WHITE, target, "%s casts cure confusion on you!", op->name ? op->name : "Someone");
			}

			if (op != target && op->type == PLAYER)
			{
				draw_info_format(COLOR_WHITE, op, "You cast cure confusion on %s!", target->name ? target->name : "someone");
			}

			for (temp = target->inv; temp != NULL; temp = temp->below)
			{
				if (temp->arch == at)
				{
					success = 1;
					temp->stats.food = 1;
				}
			}

			if (success)
			{
				if (target->type == PLAYER)
				{
					draw_info(COLOR_WHITE, target, "Your mind feels clearer.");
				}

				if (op != target && op->type == PLAYER)
				{
					draw_info_format(COLOR_WHITE, op, "%s's mind seems clearer.", target->name ? target->name : "Someone");
				}
			}
			else
			{
				if (target->type == PLAYER)
				{
					draw_info(COLOR_WHITE, target, "You are not confused.");
				}

				if (op != target && op->type == PLAYER)
				{
					draw_info_format(COLOR_WHITE, op, "%s is not confused.", target->name ? target->name : "Someone");
				}
			}

			break;

		case SP_MINOR_HEAL:
			success = 1;
			heal = rndm(2, 5 + level) + 6;

			if (op->type == PLAYER)
			{
				if (heal > 0)
				{
					draw_info_format(COLOR_WHITE, op, "The prayer heals %s for %d hp!", op == target ? "you" : (target ? target->name : "NULL"), heal);
				}
				else
				{
					draw_info(COLOR_WHITE, op, "The healing prayer fails!");
				}
			}

			if (op != target && target->type == PLAYER)
			{
				if (heal > 0)
				{
					draw_info_format(COLOR_WHITE, target, "%s casts minor healing on you healing %d hp!", op->name, heal);
				}
				else
				{
					draw_info_format(COLOR_WHITE, target, "%s casts minor healing on you but it fails!", op->name);
				}
			}

			break;

		case SP_GREATER_HEAL:
			success = 1;
			heal = rndm(4, 5 + level) + rndm(4, 5 + level) + 12;

			if (op->type == PLAYER)
			{
				if (heal > 0)
				{
					draw_info_format(COLOR_WHITE, op, "The prayer heals %s for %d hp!", op == target ? "you" : (target ? target->name : "NULL"), heal);
				}
				else
				{
					draw_info(COLOR_WHITE, op, "The healing prayer fails!");
				}
			}

			if (op != target && target->type == PLAYER)
			{
				if (heal > 0)
				{
					draw_info_format(COLOR_WHITE, target, "%s casts greater healing on you healing %d hp!", op->name, heal);
				}
				else
				{
					draw_info_format(COLOR_WHITE, target, "%s casts greater healing on you but it fails!", op->name);
				}
			}

			break;

		case SP_RESTORATION:
			if (cast_heal(op, level, target, SP_CURE_POISON))
			{
				success = 1;
			}

			if (cast_heal(op, level, target, SP_CURE_CONFUSION))
			{
				success = 1;
			}

			if (cast_heal(op, level, target, SP_CURE_DISEASE))
			{
				success = 1;
			}

			if (target->stats.food < 999)
			{
				success = 1;
				target->stats.food = 999;
			}

			if (cast_heal(op, level, target, SP_MINOR_HEAL))
			{
				success = 1;
			}

			return success;
	}

	if (heal > 0)
	{
		if (reduce_symptoms(target, heal))
		{
			success = 1;
		}

		if (target->stats.hp < target->stats.maxhp)
		{
			if (target == op)
			{
				if (op->type == PLAYER)
				{
					CONTR(op)->stat_damage_healed += MIN(heal, target->stats.maxhp - target->stats.hp);
				}
			}
			else
			{
				if (op->type == PLAYER)
				{
					CONTR(op)->stat_damage_healed_other += MIN(heal, target->stats.maxhp - target->stats.hp);
				}

				if (target->type == PLAYER)
				{
					CONTR(target)->stat_damage_heal_received += MIN(heal, target->stats.maxhp - target->stats.hp);
				}
			}

			success = 1;
			target->stats.hp += heal;

			if (target->stats.hp > target->stats.maxhp)
			{
				target->stats.hp = target->stats.maxhp;
			}
		}

		if (target->damage_round_tag != global_round_tag)
		{
			target->last_damage = 0;
			target->damage_round_tag = global_round_tag;
		}

		target->last_damage -= heal;
	}

	if (success)
	{
		op->speed_left = -FABS(op->speed) * 3;
	}

	if (insert_spell_effect(spells[spell_type].archname, target->map, target->x, target->y))
	{
		LOG(llevDebug, "insert_spell_effect() failed: spell:%d, obj:%s target:%s\n", spell_type, query_name(op, NULL), query_name(target, NULL));
	}

	return success;
}

/**
 * Cast some stat-improving spell.
 * @param op Who is casting.
 * @param caster What is casting.
 * @param target Target of the caster; who is receiving the spell.
 * @param spell_type ID of the spell.
 * @retval 0 Spell failed.
 * @retval 1 Spell was successful. */
int cast_change_attr(object *op, object *caster, object *target, int spell_type)
{
	object *tmp = target, *tmp2 = NULL, *force = NULL;
	int is_refresh = 0, msg_flag = 1, i = 0;

	if (tmp == NULL)
	{
		return 0;
	}

	/* We ID the buff force with spell_type... if we find one, we have
	 * old effect. If not, we create a fresh force. */
	for (tmp2 = tmp->inv; tmp2 != NULL; tmp2 = tmp2->below)
	{
		if (tmp2->type == FORCE)
		{
			if (tmp2->value == spell_type)
			{
				/* The old effect will be "refreshed" */
				force = tmp2;
				is_refresh = 1;
				draw_info(COLOR_WHITE, op, "You recast the spell while in effect.");
			}
		}
	}

	if (force == NULL)
	{
		force = get_archetype("force");
	}

	/* Mark this force with the originating spell */
	force->value = spell_type;

	switch (spell_type)
	{
		case SP_STRENGTH:
			force->speed_left = -1;

			if (tmp->type != PLAYER)
			{
				if (op->type == PLAYER)
				{
					draw_info(COLOR_WHITE, op, "You can't cast this kind of spell on your target.");
				}

				return 0;
			}
			else if (op->type == PLAYER && op != tmp)
			{
				draw_info_format(COLOR_WHITE, tmp, "%s casts strength on you!", op->name ? op->name : "Someone");
			}

			if (force->stats.Str < 2)
			{
				force->stats.Str++;

				if (op->type == PLAYER && op != tmp)
				{
					draw_info_format(COLOR_WHITE, op, "%s gets stronger.", tmp->name ? tmp->name : "Someone");
				}
			}
			else
			{
				msg_flag = 0;
				draw_info(COLOR_WHITE, tmp, "You don't grow stronger but the spell is refreshed.");

				if (op->type == PLAYER && op != tmp)
				{
					draw_info_format(COLOR_WHITE, op, "%s doesn't grow stronger but the spell is refreshed.", tmp->name ? tmp->name : "Someone");
				}
			}

			if (insert_spell_effect(spells[SP_STRENGTH].archname, target->map, target->x, target->y))
			{
				LOG(llevDebug, "insert_spell_effect() failed: spell:%d, obj:%s caster:%s target:%s\n", spell_type, query_name(op, NULL), query_name(caster, NULL), query_name(target, NULL));
			}

			break;

		/* Attacktype protection spells */
		case SP_PROT_COLD:
			i = ATNR_COLD;
			break;

		case SP_PROT_FIRE:
			i = ATNR_FIRE;
			break;

		case SP_PROT_ELEC:
			i = ATNR_ELECTRICITY;
			break;

		case SP_PROT_POISON:
			i = ATNR_POISON;
			break;
	}

	if (i)
	{
		draw_info_format(COLOR_WHITE, op, "Your protection to %s grows.", attack_name[i]);
		force->protection[i] = MIN(SP_level_dam_adjust(caster, spell_type, -1, 0), 50);
	}

	force->speed_left = -1 - SP_level_strength_adjust(caster, spell_type) * 0.1f;

	if (!is_refresh)
	{
		SET_FLAG(force, FLAG_APPLIED);
		force = insert_ob_in_ob(force, tmp);
	}

	if (msg_flag)
	{
		/* Mostly to display any messages */
		change_abil(tmp, force);
		/* This takes care of some stuff that change_abil() */
		fix_player(tmp);
	}

	return 1;
}

/**
 * Cast remove depletion spell.
 * @param op Object casting this.
 * @param target Target.
 * @return 0 on failure / no depletion, number of stats cured
 * otherwise. */
int remove_depletion(object *op, object *target)
{
	archetype *at;
	object *depl;
	int i, success = 0;

	if ((at = find_archetype("depletion")) == NULL)
	{
		LOG(llevBug, "Could not find archetype depletion");
		return 0;
	}

	if (!op || !target)
	{
		return 0;
	}

	if (target->type != PLAYER)
	{
		/* Fake messages for non player... */
		if (op->type == PLAYER)
		{
			draw_info_format(COLOR_WHITE, op, "You cast depletion on %s.", query_base_name(target, NULL));
			draw_info(COLOR_WHITE, op, "There is no depletion.");
		}

		return 0;
	}

	if (op != target)
	{
		if (op->type == PLAYER)
		{
			draw_info_format(COLOR_WHITE, op, "You cast depletion on %s.", query_base_name(target, NULL));
		}
		else if (target->type == PLAYER)
		{
			draw_info_format(COLOR_WHITE, target, "%s casts remove depletion on you.", query_base_name(op, NULL));
		}
	}

	if ((depl = present_arch_in_ob(at, target)) != NULL)
	{
		for (i = 0; i < NUM_STATS; i++)
		{
			if (get_attr_value(&depl->stats, i))
			{
				success++;
				draw_info(COLOR_WHITE, target, restore_msg[i]);
			}
		}

		remove_ob(depl);
		fix_player(target);
	}

	if (op != target && op->type == PLAYER)
	{
		if (success)
		{
			draw_info(COLOR_WHITE, op, "Your prayer removes some depletion.");
		}
		else
		{
			draw_info(COLOR_WHITE, op, "There is no depletion.");
		}
	}

	/* If success, target got info before */
	if (op != target && target->type == PLAYER && !success)
	{
		draw_info(COLOR_WHITE, target, "There is no depletion.");
	}

	insert_spell_effect(spells[SP_REMOVE_DEPLETION].archname, target->map, target->x, target->y);

	return success;
}

/**
 * Cast remove curse or remove damnation.
 * @param op Caster object.
 * @param target Target.
 * @param type ID of the spell.
 * @param src Where the spell comes from.
 * @return 0 on failure / no cursed items, number of objects uncursed
 * otherwise. */
int remove_curse(object *op, object *target, int type, int src)
{
	object *tmp;
	int success = 0;

	if (!op || !target)
	{
		return 0;
	}

	if (op != target)
	{
		if (op->type == PLAYER)
		{
			draw_info_format(COLOR_WHITE, op, "You cast remove %s on %s.", type == SP_REMOVE_CURSE ? "curse" : "damnation", query_base_name(target, NULL));
		}
		else if (target->type == PLAYER)
		{
			draw_info_format(COLOR_WHITE, target, "%s casts remove %s on you.", query_base_name(op, NULL), type == SP_REMOVE_CURSE ? "curse" : "damnation");
		}
	}

	/* Player remove xx only removes applied stuff, npc remove clears ALL */
	for (tmp = target->inv; tmp; tmp = tmp->below)
	{
		if ((src == CAST_NPC || QUERY_FLAG(tmp, FLAG_APPLIED)) && (QUERY_FLAG(tmp, FLAG_CURSED) || (type == SP_REMOVE_DAMNATION && QUERY_FLAG(tmp, FLAG_DAMNED))))
		{
			if (tmp->level <= SK_level(op))
			{
				success++;

				if (type == SP_REMOVE_DAMNATION)
				{
					CLEAR_FLAG(tmp, FLAG_DAMNED);
				}

				CLEAR_FLAG(tmp, FLAG_CURSED);
				esrv_send_item(tmp);
			}
			/* Level of the items is too high for this remove curse */
			else
			{
				if (target->type == PLAYER)
				{
					draw_info_format(COLOR_WHITE, target, "The %s's curse is stronger than the prayer!", query_base_name(tmp, NULL));
				}
				else if (op != target && op->type == PLAYER)
				{
					draw_info_format(COLOR_WHITE, op, "The %s's curse of %s is stronger than your prayer!", query_base_name(tmp, NULL), query_base_name(target, NULL));
				}
			}
		}
	}

	if (op != target && op->type == PLAYER)
	{
		if (success)
		{
			draw_info(COLOR_WHITE, op, "Your prayer removes some curses.");
		}
		else
		{
			draw_info_format(COLOR_WHITE, op, "%s's items seem uncursed.", query_base_name(target, NULL));
		}
	}

	if (target->type == PLAYER)
	{
		if (success)
		{
			draw_info(COLOR_WHITE, target, "You feel like someone is helping you.");
		}
		else
		{
			if (src == CAST_NORMAL)
			{
				draw_info(COLOR_WHITE, target, "You are not using any cursed items.");
			}
			else
			{
				draw_info(COLOR_WHITE, target, "You hear maniacal laughter in the distance.");
			}
		}
	}

	insert_spell_effect(spells[SP_REMOVE_CURSE].archname, target->map, target->x, target->y);

	return success;
}

/**
 * Actually identify an object when casting identify.
 * @param tmp What to identify.
 * @param op Who is receiving the spell effect.
 * @param mode One of @ref identify_modes.
 * @param[out] done Contains the number of objects identified so far.
 * @param level Maximum level of items we can identify.
 * @return 1 if we can keep identifying items, 0 otherwise. */
int do_cast_identify(object *tmp, object *op, int mode, int *done, int level)
{
	if (QUERY_FLAG(tmp, FLAG_IDENTIFIED) || IS_SYS_INVISIBLE(tmp) || !need_identify(tmp))
	{
		return 1;
	}

	if (level < tmp->level)
	{
		if (op->type == PLAYER)
		{
			draw_info_format(COLOR_WHITE, op, "The %s is too powerful for this identify!", query_base_name(tmp, NULL));
		}
	}
	else
	{
		identify(tmp);

		if (op->type == PLAYER)
		{
			draw_info_format(COLOR_WHITE, op, "You have %s.", long_desc(tmp, NULL));

			if (tmp->msg)
			{
				draw_info(COLOR_WHITE, op, "The item has a story:");
				draw_info(COLOR_WHITE, op, tmp->msg);
			}
		}

		*done += 1;
	}

	if (mode == IDENTIFY_NORMAL && op->type == PLAYER && *done > CONTR(op)->skill_ptr[SK_LITERACY]->level + op->stats.Int)
	{
		return 0;
	}

	return 1;
}

/**
 * Cast identify spell.
 * @param op Object receiving the spell effects.
 * @param level Level of the identification.
 * @param single_ob If set, and mode is @ref IDENTIFY_MARKED, only
 * this object will be identified, otherwise contents of this object.
 * If NULL, the inventory of 'op' will be identified.
 * @param mode One of @ref identify_modes.
 * @return Number of objects identified. */
int cast_identify(object *op, int level, object *single_ob, int mode)
{
	int done = 0;

	insert_spell_effect(spells[SP_IDENTIFY].archname, op->map, op->x, op->y);

	if (mode == IDENTIFY_MARKED)
	{
		do_cast_identify(single_ob, op, mode, &done, level);
	}
	else
	{
		object *tmp = op->inv;

		if (single_ob && single_ob->type == CONTAINER)
		{
			tmp = single_ob->inv;
		}

		for ( ; tmp; tmp = tmp->below)
		{
			if (!do_cast_identify(tmp, op, mode, &done, level))
			{
				break;
			}
		}
	}

	if (op->type == PLAYER && !done)
	{
		draw_info(COLOR_WHITE, op, "You can't reach anything unidentified in your inventory.");
	}

	return done;
}

/**
 * A spell to make an altar your god's.
 * @param op Who is casting.
 * @retval 0 No consecration happened.
 * @retval 1 An altar was consecrated. */
int cast_consecrate(object *op)
{
	object *tmp, *god = find_god(determine_god(op));

	if (!god)
	{
		draw_info(COLOR_WHITE, op, "You can't consecrate anything if you don't worship a god!");
		return 0;
	}

	for (tmp = op->below; tmp; tmp = tmp->below)
	{
		if (QUERY_FLAG(tmp, FLAG_IS_FLOOR))
		{
			break;
		}

		if (tmp->type == HOLY_ALTAR)
		{
			/* We use SK_level here instead of path_level mod because I think
			 * all the gods should give equal chance of re-consecrating altars */
			if (tmp->level > SK_level(op))
			{
				draw_info_format(COLOR_WHITE, op, "You are not powerful enough to reconsecrate the %s.", tmp->name);
				return 0;
			}
			else if (tmp->other_arch == god->arch)
			{
				draw_info_format(COLOR_WHITE, op, "That altar is already consecrated to %s.", god->name);
				return 0;
			}
			else
			{
				char buf[MAX_BUF], *cp;
				object *new_altar;

				snprintf(buf, sizeof(buf), "altar_%s", god->name);

				for (cp = buf; *cp != '\0'; cp++)
				{
					*cp = tolower(*cp);
				}

				new_altar = get_archetype(buf);
				new_altar->level = tmp->level;
				new_altar->x = tmp->x;
				new_altar->y = tmp->y;
				new_altar->direction = tmp->direction;

				if (QUERY_FLAG(new_altar, FLAG_IS_TURNABLE))
				{
					SET_ANIMATION(new_altar, (NUM_ANIMATIONS(new_altar) / NUM_FACINGS(new_altar)) * new_altar->direction);
				}

				if (QUERY_FLAG(tmp, FLAG_IS_BUILDABLE))
				{
					SET_FLAG(new_altar, FLAG_IS_BUILDABLE);
				}

				insert_ob_in_map(new_altar, tmp->map, NULL, 0);
				remove_ob(tmp);

				draw_info_format(COLOR_WHITE, op, "You consecrated the altar to %s!", god->name);
				return 1;
			}
		}
	}

	draw_info(COLOR_WHITE, op, "You are not standing over an altar!");
	return 0;
}

/**
 * Finger of death spell.
 *
 * If target is undead, the spell will restore target to max health
 * instead of damaging it.
 * @param op Caster.
 * @param target Target.
 * @return 1. */
int finger_of_death(object *op, object *target)
{
	object *hitter;
	int dam;

	if (QUERY_FLAG(target, FLAG_UNDEAD))
	{
		draw_info_format(COLOR_WHITE, op, "The %s looks stronger!", query_name(target, NULL));
		target->stats.hp = target->stats.maxhp;

		if (!OBJECT_VALID(target->enemy, target->enemy_count))
		{
			set_npc_enemy(target, op, NULL);
		}

		return 1;
	}

	/* We create a hitter object -- the spell */
	hitter = arch_to_object(spellarch[SP_FINGER_DEATH]);
	hitter->level = SK_level(op);
	set_owner(hitter, op);
	hitter->x = target->x;
	hitter->y = target->y;
	insert_ob_in_map(hitter, target->map, op, 0);

	if (spell_attack_missed(hitter, target))
	{
		draw_info_format(COLOR_ORANGE, op, "Your finger of death misses %s!", target->name);
		remove_ob(hitter);
		return 1;
	}

	dam = SP_level_dam_adjust(op, SP_FINGER_DEATH, spells[SP_FINGER_DEATH].bdam, 0);
	hit_player(target, dam, hitter, AT_INTERNAL);
	remove_ob(hitter);

	return 1;
}

/**
 * Let's try to infect something.
 * @param op Who is casting.
 * @param caster What object is casting.
 * @param dir Cast direction.
 * @param disease_arch Archetype of the disease.
 * @param type ID of the spell.
 * @retval 0 No one caught anything.
 * @retval 1 At least one living was affected. */
int cast_cause_disease(object *op, object *caster, int dir, archetype *disease_arch, int type)
{
	int x = op->x, y = op->y, i, xt, yt;
	object *walk;
	mapstruct *m;

	/* Search in a line for a victim */
	for (i = 0; i < 5; i++)
	{
		x += freearr_x[dir];
		y += freearr_y[dir];
		xt = x;
		yt = y;

		if (!(m = get_map_from_coord(op->map, &xt, &yt)))
		{
			continue;
		}

		/* Check map flags for alive object */
		if (!(GET_MAP_FLAGS(m, xt, yt) & P_IS_MONSTER))
		{
			continue;
		}

		/* Search this square for a victim */
		for (walk = GET_MAP_OB(m, xt, yt); walk; walk = walk->above)
		{
			object *disease;
			int dam, strength;

			/* Found a victim */
			if (!QUERY_FLAG(walk, FLAG_MONSTER) && (walk->type != PLAYER || !pvp_area(op, walk)))
			{
				continue;
			}

			disease = arch_to_object(disease_arch);
			dam = SP_level_dam_adjust(caster, type, spells[type].bdam, 0);
			strength = SP_level_strength_adjust(caster, type);

			set_owner(disease, op);
			disease->stats.exp = 0;
			disease->level = SK_level(caster);

			/* Try to get the experience into the correct category */
			if (op->chosen_skill && op->chosen_skill->exp_obj)
			{
				disease->exp_obj = op->chosen_skill->exp_obj;
			}

			/* Do level adjustments */
			if (disease->stats.wc)
			{
				disease->stats.wc += strength / 2;
			}

			if (disease->magic > 0)
			{
				disease->magic += strength / 4;
			}

			if (disease->stats.maxhp > 0)
			{
				disease->stats.maxhp += strength;
			}

			if (disease->stats.maxgrace > 0)
			{
				disease->stats.maxgrace += strength;
			}

			if (disease->stats.dam)
			{
				if (disease->stats.dam > 0)
				{
					disease->stats.dam += dam;
				}
				else
				{
					disease->stats.dam -= dam;
				}
			}

			if (disease->last_sp)
			{
				disease->last_sp -= 2 * dam;

				if (disease->last_sp < 1)
				{
					disease->last_sp = 1;
				}
			}

			if (disease->stats.maxsp)
			{
				if (disease->stats.maxsp > 0)
				{
					disease->stats.maxsp += dam;
				}
				else
				{
					disease->stats.maxsp -= dam;
				}
			}

			if (disease->stats.ac)
			{
				disease->stats.ac += dam;
			}

			if (disease->last_eat)
			{
				disease->last_eat -= dam;
			}

			if (disease->stats.hp)
			{
				disease->stats.hp -= dam;
			}

			if (disease->stats.sp)
			{
				disease->stats.sp -= dam;
			}

			if (infect_object(walk, disease, 1))
			{
				draw_info_format(COLOR_WHITE, op, "You inflict %s on %s!", disease->name, walk->name);
				return 1;
			}
		}

		/* No more infecting through walls. */
		if (wall(m, xt, yt))
		{
			return 0;
		}
	}

	draw_info(COLOR_WHITE, op, "No one caught anything!");
	return 0;
}

/**
 * Transform wealth spell.
 * @param op Who is casting.
 * @return 1 on success, 0 otherwise. */
int cast_transform_wealth(object *op)
{
	object *marked;
	sint64 val;

	if (op->type != PLAYER)
	{
		return 0;
	}

	/* Find the marked wealth. */
	marked = find_marked_object(op);

	if (!marked)
	{
		draw_info(COLOR_WHITE, op, "You need to mark an object to cast this spell.");
		return 0;
	}

	/* Check that it's really money. */
	if (marked->type != MONEY)
	{
		draw_info(COLOR_WHITE, op, "You can only cast this spell on wealth objects.");
		return 0;
	}

	/* Only allow coppers and silvers to be transformed. */
	if (strcmp(marked->arch->name, coins[NUM_COINS - 1]) && strcmp(marked->arch->name, coins[NUM_COINS - 2]))
	{
		draw_info_format(COLOR_WHITE, op, "You don't see a way to transform %s.", query_name(marked, op));
		return 0;
	}

	/* Figure out our value of money to give to player. */
	val = (marked->value * (marked->nrof ? marked->nrof : 1)) * TRANSFORM_WEALTH_SACRIFICE;
	/* We remove the money. */
	remove_ob(marked);
	/* Now give the player the new money. */
	insert_coins(op, val);
	draw_info_format(COLOR_WHITE, op, "You transform %s into %s.", query_name(marked, op), cost_string_from_value(val));
	return 1;
}
