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

#include <global.h>
#include <tod.h>

#ifndef __CEXTRACT__
#include <sproto.h>
#endif

/* Want this regardless of rplay. */
#include <sounds.h>

/* need math lib for double-precision and pow() in dragon_eat_flesh() */
#include <math.h>

#if defined(vax) || defined(ibm032)
size_t strftime(char *, size_t, const char *, const struct tm *);
time_t mktime(struct tm *);
#endif

void draw_find(object *op, object *find)
{
	new_draw_info_format(NDI_UNIQUE, 0, op, "You find %s in the chest.", query_name(find, NULL));
}

/* Return value: 1 if money was destroyed, 0 if not. */
static int apply_id_altar(object *money, object *altar, object *pl)
{
	object *id, *marked;
	int success = 0;

	if (pl == NULL || pl->type != PLAYER)
		return 0;

	/* Check for MONEY type is a special hack - it prevents 'nothing needs
	 * identifying' from being printed out more than it needs to be. */
	if (!check_altar_sacrifice(altar, money) || money->type != MONEY)
		return 0;

	marked = find_marked_object(pl);
	/* if the player has a marked item, identify that if it needs to be
	 * identified.  IF it doesn't, then go through the player inventory. */
	if (marked && !QUERY_FLAG(marked, FLAG_IDENTIFIED) && need_identify(marked))
	{
		if (operate_altar(altar, &money))
		{
			identify(marked);
			new_draw_info_format(NDI_UNIQUE, 0, pl, "You have %s.", long_desc(marked, pl));
			if (marked->msg)
			{
				new_draw_info(NDI_UNIQUE, 0, pl, "The item has a story:");
				new_draw_info(NDI_UNIQUE, 0, pl, marked->msg);
			}
			return money == NULL;
		}
	}

	for (id = pl->inv; id; id = id->below)
	{
		if (!QUERY_FLAG(id, FLAG_IDENTIFIED) && !IS_INVISIBLE(id, pl) && need_identify(id))
		{
			if (operate_altar(altar, &money))
			{
				identify(id);
				new_draw_info_format(NDI_UNIQUE, 0, pl, "You have %s.", long_desc(id, pl));
				if (id->msg)
				{
					new_draw_info(NDI_UNIQUE, 0,pl, "The item has a story:");
					new_draw_info(NDI_UNIQUE, 0,pl, id->msg);
				}
				success = 1;
				/* If no more money, might as well quit now */
				if (money == NULL || !check_altar_sacrifice(altar, money))
					break;
			}
			else
			{
				LOG(llevBug, "check_id_altar: Couldn't do sacrifice when we should have been able to.\n");
				break;
			}
		}
	}

	if (!success)
		new_draw_info(NDI_UNIQUE, 0, pl, "You have nothing that needs identifying.");

	return money == NULL;
}

int apply_potion(object *op, object *tmp)
{
	int i;

	/* some sanity checks */
	if (!op || !tmp || (op->type == PLAYER && (!CONTR(op) || !CONTR(op)->sp_exp_ptr || !CONTR(op)->grace_exp_ptr)))
	{
		LOG(llevBug,"apply_potion() called with invalid objects! obj: %s (%s - %s) tmp:%s\n", query_name(op, NULL), CONTR(op) ? query_name(CONTR(op)->sp_exp_ptr, NULL) : "<no contr>", CONTR(op) ? query_name(CONTR(op)->grace_exp_ptr, NULL) : "<no contr>", query_name(tmp, NULL));
		return 0;
	}


	if (op->type==PLAYER)
	{
		/* set chosen_skill to "magic device" - thats used when we "use" a potion */
		if (!change_skill(op, SK_USE_MAGIC_ITEM))
			return 0; /* no skill, no potion use (dust & balm too!) */

		if (!QUERY_FLAG(tmp, FLAG_IDENTIFIED))
			identify(tmp);

		/* special potions. Only players get this */
		if (tmp->last_eat == -1) /* create a force and copy the effects in */
		{
			object *force = get_archetype("force");

			if (!force)
			{
				LOG(llevBug, "apply_potion: Can't create force object?\n");
				return 0;
			}

			force->type = POTION_EFFECT;
			SET_FLAG(force, FLAG_IS_USED_UP); /* or it will auto destroy with first tick */
			force->stats.food += tmp->stats.food; /* how long this force will stay */
			if (force->stats.food <= 0)
				force->stats.food = 1;

			/* negative effects because cursed or damned */
			if (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED))
			{
				/* now we have a bit work because we change (multiply,...) the
				 * base values of the potion - that can invoke out of bounce
				 * values we must catch here.
				 */

				force->stats.food *= 3; /* effects stays 3 times longer */
				for (i = 0; i < NROFATTACKS; i++)
				{
					int tmp_r, tmp_a;

					tmp_r = tmp->resist[i] > 0 ? -tmp->resist[i] : tmp->resist[i];
					tmp_a = tmp->attack[i] > 0 ? -tmp->attack[i] : tmp->attack[i];

					/* double bad effect when damned */
					if (QUERY_FLAG(tmp, FLAG_DAMNED))
					{
						tmp_r *= 2;
						tmp_a *= 2;
					}

					/* we don't want out of bound values ... */
					if ((int)force->resist[i] + tmp_r > 100)
						force->resist[i] = 100;
					else if ((int)force->resist[i] + tmp_r < -100)
						force->resist[i] = -100;
					else
						force->resist[i] += (sint8) tmp_r;

					if ((int)force->attack[i] + tmp_a > 100)
						force->attack[i] = 100;
					else if ((int)force->attack[i] + tmp_a < 0)
						force->attack[i] = 0;
					else
						force->attack[i] += tmp_a;
				}

				insert_spell_effect("meffect_purple", op->map, op->x, op->y);
				play_sound_map(op->map, op->x, op->y, SOUND_DRINK_POISON, SOUND_NORMAL);
			}
			else /* all positive (when not on default negative) */
			{
				/* we don't must do the hard way like cursed/damned (no multiplication or
				 * sign change).
				 */
				memcpy(force->resist, tmp->resist, sizeof(tmp->resist));
				memcpy(force->attack, tmp->attack, sizeof(tmp->attack));
				insert_spell_effect("meffect_green", op->map, op->x, op->y);
				play_sound_map(op->map, op->x, op->y, SOUND_MAGIC_DEFAULT, SOUND_SPELL);
			}

			/* now copy stats values */
			force->stats.Str = QUERY_FLAG(tmp, FLAG_CURSED) ? (tmp->stats.Str > 0 ? -tmp->stats.Str : tmp->stats.Str) : (QUERY_FLAG(tmp, FLAG_DAMNED) ? (tmp->stats.Str > 0 ? (-tmp->stats.Str) * 2 : tmp->stats.Str * 2) : tmp->stats.Str);
			force->stats.Con = QUERY_FLAG(tmp, FLAG_CURSED) ? (tmp->stats.Con > 0 ? -tmp->stats.Con : tmp->stats.Con) : (QUERY_FLAG(tmp, FLAG_DAMNED) ? (tmp->stats.Con > 0 ? (-tmp->stats.Con) * 2 : tmp->stats.Con * 2) : tmp->stats.Con);
			force->stats.Dex = QUERY_FLAG(tmp, FLAG_CURSED) ? (tmp->stats.Dex > 0 ? -tmp->stats.Dex : tmp->stats.Dex) : (QUERY_FLAG(tmp, FLAG_DAMNED) ? (tmp->stats.Dex > 0 ? (-tmp->stats.Dex) * 2 : tmp->stats.Dex * 2) : tmp->stats.Dex);
			force->stats.Int = QUERY_FLAG(tmp, FLAG_CURSED) ? (tmp->stats.Int > 0 ? -tmp->stats.Int : tmp->stats.Int) : (QUERY_FLAG(tmp, FLAG_DAMNED) ? (tmp->stats.Int > 0 ? (-tmp->stats.Int) * 2 : tmp->stats.Int * 2) : tmp->stats.Int);
			force->stats.Wis = QUERY_FLAG(tmp, FLAG_CURSED) ? (tmp->stats.Wis > 0 ? -tmp->stats.Wis : tmp->stats.Wis) : (QUERY_FLAG(tmp, FLAG_DAMNED) ? (tmp->stats.Wis > 0 ? (-tmp->stats.Wis) * 2 : tmp->stats.Wis * 2) : tmp->stats.Wis);
			force->stats.Pow = QUERY_FLAG(tmp, FLAG_CURSED) ? (tmp->stats.Pow > 0 ? -tmp->stats.Pow : tmp->stats.Pow) : (QUERY_FLAG(tmp, FLAG_DAMNED) ? (tmp->stats.Pow > 0 ? (-tmp->stats.Pow) * 2 : tmp->stats.Pow * 2) : tmp->stats.Pow);
			force->stats.Cha = QUERY_FLAG(tmp, FLAG_CURSED) ? (tmp->stats.Cha > 0 ? -tmp->stats.Cha : tmp->stats.Cha) : (QUERY_FLAG(tmp, FLAG_DAMNED) ? (tmp->stats.Cha > 0 ? (-tmp->stats.Cha) * 2 : tmp->stats.Cha * 2) : tmp->stats.Cha);

			/* kick the force in, and apply it to player */
			force->speed_left = -1;
			force = insert_ob_in_ob(force, op);
			CLEAR_FLAG(tmp, FLAG_APPLIED);
			SET_FLAG(force, FLAG_APPLIED);
			if (!change_abil(op, force)) /* implicit fix_player() here */
				new_draw_info(NDI_UNIQUE, 0, op, "Nothing happened.");
			decrease_ob(tmp);
			return 1;
		}

		if (tmp->last_eat == 1) /* Potion of minor restoration */
		{
			object *depl;
			archetype *at;
			if (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED))
			{
				if (QUERY_FLAG(tmp, FLAG_DAMNED))
				{
					drain_stat(op);
					drain_stat(op);
					drain_stat(op);
					drain_stat(op);
				}
				else
				{
					drain_stat(op);
					drain_stat(op);
				}
				fix_player(op);
				decrease_ob(tmp);
				insert_spell_effect("meffect_purple", op->map, op->x, op->y);
				play_sound_map(op->map, op->x, op->y, SOUND_DRINK_POISON, SOUND_NORMAL);
				return 1;
			}
			if ((at = find_archetype("depletion")) == NULL)
			{
				LOG(llevBug, "BUG: Could not find archetype depletion.");
				return 0;
			}
			depl = present_arch_in_ob(at, op);
			if (depl != NULL)
			{
				for (i = 0; i < 7; i++)
				{
					if (get_attr_value(&depl->stats, i))
						new_draw_info(NDI_UNIQUE, 0, op, restore_msg[i]);
				}
				remove_ob(depl); /* in inventory of ... */
				fix_player(op);
			}
			else
				new_draw_info(NDI_UNIQUE, 0, op, "You feel a great loss...");
			decrease_ob(tmp);
			insert_spell_effect("meffect_green", op->map, op->x, op->y);
			play_sound_map(op->map, op->x, op->y, SOUND_MAGIC_DEFAULT, SOUND_SPELL);
			return 1;
		}
		else if (tmp->last_eat == 2)    /* improvement potion */
		{
			int success_flag = 0, hp_flag = 0, sp_flag = 0, grace_flag = 0;

			if (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED))
			{
				/* jump in by random - goto power */
				if (RANDOM() % 2)
					goto hp_jump;
				else if (RANDOM() % 2)
					goto sp_jump;
				else
					goto grace_jump;

				while (!hp_flag || !sp_flag || !grace_flag)
				{
hp_jump:
					hp_flag = 1; /* mark we have checked hp chain */
					for (i = 2; i <= op->level; i++)
					{
						/* move one value to max */
						if (CONTR(op)->levhp[i] != 1)
						{
							CONTR(op)->levhp[i] = 1;
							success_flag = 2;
							goto improve_done;
						}
					}
sp_jump:
					sp_flag = 1; /* mark we have checked sp chain */
					for (i = 2; i <= CONTR(op)->sp_exp_ptr->level; i++)
					{
						/* move one value to max */
						if (CONTR(op)->levsp[i] != 1)
						{
							CONTR(op)->levsp[i] = 1;
							success_flag = 2;
							goto improve_done;
						}
					}
grace_jump:
					grace_flag = 1; /* mark we have checked grace chain */
					for (i = 2; i <= CONTR(op)->grace_exp_ptr->level; i++)
					{
						/* move one value to max */
						if (CONTR(op)->levgrace[i] != 1)
						{
							CONTR(op)->levgrace[i] = 1;
							success_flag = 2;
							goto improve_done;
						}
					}
				};
				success_flag = 3;
			}
			else
			{
				/* jump in by random - goto power */
				if (RANDOM() % 2)
					goto hp_jump2;
				else if (RANDOM() % 2)
					goto sp_jump2;
				else
					goto grace_jump2;

				while (!hp_flag || !sp_flag || !grace_flag)
				{
hp_jump2:
					hp_flag = 1; /* mark we have checked hp chain */
					for (i = 1; i <= op->level; i++)
					{
						/* move one value to max */
						if (CONTR(op)->levhp[i] != (char)op->arch->clone.stats.maxhp)
						{
							CONTR(op)->levhp[i] = (char)op->arch->clone.stats.maxhp;
							success_flag = 1;
							goto improve_done;
						}
					}
sp_jump2:
					sp_flag = 1; /* mark we have checked sp chain */
					for (i = 1; i <= CONTR(op)->sp_exp_ptr->level; i++)
					{
						/* move one value to max */
						if (CONTR(op)->levsp[i] != (char)op->arch->clone.stats.maxsp)
						{
							CONTR(op)->levsp[i] = (char)op->arch->clone.stats.maxsp;
							success_flag = 1;
							goto improve_done;
						}
					}
grace_jump2:
					grace_flag = 1; /* mark we have checked grace chain */
					for (i = 1; i <= CONTR(op)->grace_exp_ptr->level; i++)
					{
						/* move one value to max */
						if (CONTR(op)->levgrace[i] != (char)op->arch->clone.stats.maxgrace)
						{
							CONTR(op)->levgrace[i] = (char)op->arch->clone.stats.maxgrace;
							success_flag = 1;
							goto improve_done;
						}

					}
				};
			}

improve_done:
			CLEAR_FLAG(tmp, FLAG_APPLIED);
			if (!success_flag)
			{
				new_draw_info(NDI_UNIQUE, 0, op, "The potion had no effect - you are already perfect.");
				play_sound_map(op->map, op->x, op->y, SOUND_MAGIC_DEFAULT, SOUND_SPELL);
			}
			else if (success_flag == 1)
			{
				fix_player(op);
				insert_spell_effect("meffect_yellow", op->map, op->x, op->y);
				play_sound_map(op->map, op->x, op->y, SOUND_MAGIC_DEFAULT, SOUND_SPELL);
				new_draw_info(NDI_UNIQUE, 0, op, "You feel a little more perfect!");
			}
			else if (success_flag == 2)
			{
				fix_player(op);
				insert_spell_effect("meffect_purple", op->map, op->x, op->y);
				play_sound_map(op->map, op->x, op->y, SOUND_DRINK_POISON, SOUND_NORMAL);
				new_draw_info(NDI_UNIQUE, 0, op, "The foul potion burns like fire in you!");
			}
			else /* bad potion but all values of this player are 1! poor poor guy.... */
			{
				insert_spell_effect("meffect_purple", op->map, op->x, op->y);
				play_sound_map(op->map, op->x, op->y, SOUND_DRINK_POISON, SOUND_NORMAL);
				new_draw_info(NDI_UNIQUE, 0, op, "The potion was foul but had no effect on your tortured body.");
			}
			decrease_ob(tmp);
			return 1;
		}
	}

	if (tmp->stats.sp == SP_NO_SPELL)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Nothing happens as you apply it.");
		decrease_ob(tmp);
		return 0;
	}


	/* A potion that casts a spell.  Healing, restore spellpoint (power potion)
	 * and heroism all fit into this category. */
	if (tmp->stats.sp != SP_NO_SPELL)
	{
		/* apply potion fires in player's facing direction, unless the spell is SELF one, ie, healing or cure ilness. */
		cast_spell(op, tmp, spells[tmp->stats.sp].flags & SPELL_DESC_SELF ? 0 : op->facing, tmp->stats.sp, 1, spellPotion, NULL);
		decrease_ob(tmp);
		/* if you're dead, no point in doing this... */
		if (!QUERY_FLAG(op, FLAG_REMOVED))
			fix_player(op);
		return 1;
	}

	/* CLEAR_FLAG is so that if the character has other potions
	 * that were grouped with the one consumed, his
	 * stat will not be raised by them.  fix_player just clears
	 * up all the stats.
	 */
	CLEAR_FLAG(tmp, FLAG_APPLIED);
	fix_player(op);
	decrease_ob(tmp);
	return 1;
}

/****************************************************************************
 * Weapon improvement code follows
 ****************************************************************************/

int check_item(object *op, const char *item)
{
	int count=0;

	if (item == NULL)
		return 0;

	op = op->below;
	while (op != NULL)
	{
		if (strcmp(op->arch->name, item) == 0)
		{
			if (!QUERY_FLAG(op, FLAG_CURSED) && !QUERY_FLAG(op, FLAG_DAMNED) && !QUERY_FLAG(op, FLAG_UNPAID))
			{
				/* this is necessary for artifact sacrifices --FD-- */
				if (op->nrof == 0)
					count++;
				else
					count += op->nrof;
			}
		}
		op = op->below;
	}
	return count;
}

void eat_item(object *op, const char *item)
{
	object *prev;

	prev = op;
	op = op->below;

	while (op != NULL)
	{
		if (strcmp(op->arch->name, item) == 0)
		{
			decrease_ob_nr(op, op->nrof);
			op = prev;
		}
		prev = op;
		op = op->below;
	}
}

/* This checks to see of the player (who) is sufficient level to use a weapon
 * with improvs improvements (typically last_eat).  We take an int here
 * instead of the object so that the improvement code can pass along the
 * increased value to see if the object is usuable.
 * we return 1 (true) if the player can use the weapon.
 */
int check_weapon_power(object *who, int improvs)
{
	int level=who->level;

	/* The skill system hands out wc and dam bonuses to fighters
	 * more generously than the old system (see fix_player). Thus
	 * we need to curtail the power of player enchanted weapons.
	 * I changed this to 1 improvement per "fighter" level/5 -b.t.
	 * Note:  Nothing should break by allowing this ratio to be different or
	 * using normal level - it is just a matter of play balance.
	 */
	if (who->type == PLAYER)
	{
		object *wc_obj = NULL;

		for (wc_obj = who->inv; wc_obj; wc_obj = wc_obj->below)
			if (wc_obj->type == EXPERIENCE && wc_obj->stats.Str)
				break;

		if (!wc_obj)
			LOG(llevBug, "BUG: Player: %s lacks wc experience object.\n", who->name);
		else
			level = wc_obj->level;
	}
	return (improvs <= ((level / 5) + 5));
}

/* Returns the object count that of the number of objects found that
 * improver wants.
 */
static int check_sacrifice(object *op, object *improver)
{
	int count = 0;

	if (improver->slaying != NULL)
	{
		count = check_item(op, improver->slaying);
		if (count < 1)
		{
			char buf[200];
			sprintf(buf, "The gods want more %ss.", improver->slaying);
			new_draw_info(NDI_UNIQUE, 0, op, buf);
			return 0;
		}
	}
	else
		count = 1;

	return count;
}

int improve_weapon_stat(object *op, object *improver, object *weapon, signed char *stat, int sacrifice_count, char *statname)
{
	new_draw_info(NDI_UNIQUE, 0, op, "Your sacrifice was accepted.");
	*stat += sacrifice_count;
	weapon->last_eat++;
	new_draw_info_format(NDI_UNIQUE, 0, op, "Weapon's bonus to %s improved by %d.", statname, sacrifice_count);
	decrease_ob(improver);

	/* So it updates the players stats and the window */
	fix_player(op);
	return 1;
}

/* Types of improvements, hidden in the sp field. */
#define IMPROVE_PREPARE 1
#define IMPROVE_DAMAGE 2
#define IMPROVE_WEIGHT 3
#define IMPROVE_ENCHANT 4
#define IMPROVE_STR 5
#define IMPROVE_DEX 6
#define IMPROVE_CON 7
#define IMPROVE_WIS 8
#define IMPROVE_CHA 9
#define IMPROVE_INT 10
#define IMPROVE_POW 11


/* This does the prepare weapon scroll */
int prepare_weapon(object *op, object *improver, object *weapon)
{
	int sacrifice_count, i;
	char buf[MAX_BUF];

	if (weapon->level != 0)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Weapon already prepared.");
		return 0;
	}
	for (i = 0; i < NROFATTACKS; i++)
		if (weapon->resist[i])
			break;

	/* If we break out, i will be less than nrofattacks, preventing
	 * improvement of items that already have protections.
	 */
	if (i < NROFATTACKS || weapon->stats.hp || weapon->stats.sp || weapon->stats.exp || weapon->stats.ac)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Cannot prepare magic weapons.");
		return 0;
	}
	sacrifice_count = check_sacrifice(op, improver);
	if (sacrifice_count <= 0)
		return 0;
	sacrifice_count = isqrt(sacrifice_count);
	weapon->level = sacrifice_count;
	new_draw_info(NDI_UNIQUE, 0, op, "Your sacrifice was accepted.");
	eat_item(op, improver->slaying);
	new_draw_info_format(NDI_UNIQUE, 0, op,"Your *%s may be improved %d times.", weapon->name, sacrifice_count);
	sprintf(buf, "%s's %s", op->name, weapon->name);
	FREE_AND_COPY_HASH(weapon->name, buf);

	/* prevents preparing n weapons in the same slot at once! */
	weapon->nrof = 0;
	decrease_ob(improver);
	weapon->last_eat = 0;
	return 1;
}


/* This is the new improve weapon code */
/* build_weapon returns 0 if it was not able to work. */
/* #### We are hiding extra information about the weapon in the level and
   last_eat numbers for an object.  Hopefully this won't break anything ??
   level == max improve last_eat == current improve*/
int improve_weapon(object *op, object *improver, object *weapon)
{
	int sacrifice_count, sacrifice_needed = 0;

	if (improver->stats.sp == IMPROVE_PREPARE)
		return prepare_weapon(op, improver, weapon);
	if (weapon->level==0)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "This weapon has not been prepared.");
		return 0;
	}
	if (weapon->level == weapon->last_eat)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "This weapon cannot be improved any more.");
		return 0;
	}
	if (QUERY_FLAG(weapon, FLAG_APPLIED) && !check_weapon_power(op, weapon->last_eat + 1))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Improving the weapon will make it too");
		new_draw_info(NDI_UNIQUE, 0, op, "powerful for you to use.  Unready it if you");
		new_draw_info(NDI_UNIQUE, 0, op, "really want to improve it.");
		return 0;
	}
	/* This just increases damage by 5 points, no matter what.  No sacrifice
	 * is needed.  Since stats.dam is now a 16 bit value and not 8 bit,
	 * don't put any maximum value on damage - the limit is how much the
	 * weapon  can be improved. */
	if (improver->stats.sp == IMPROVE_DAMAGE)
	{
		weapon->stats.dam += 5;
		/* 5 KG's */
		weapon->weight += 5000;
		new_draw_info_format(NDI_UNIQUE, 0, op, "Damage has been increased by 5 to %d", weapon->stats.dam);
		weapon->last_eat++;
		decrease_ob(improver);
		return 1;
	}
	if (improver->stats.sp == IMPROVE_WEIGHT)
	{
		/* Reduce weight by 20% */
		weapon->weight = (weapon->weight * 8) / 10;
		if (weapon->weight < 1)
			weapon->weight = 1;
		new_draw_info_format(NDI_UNIQUE, 0, op, "Weapon weight reduced to %6.1f kg", (float)weapon->weight / 1000.0);
		weapon->last_eat++;
		decrease_ob(improver);
		return 1;
	}
	if (improver->stats.sp == IMPROVE_ENCHANT)
	{
		weapon->magic++;
		weapon->last_eat++;
		new_draw_info_format(NDI_UNIQUE, 0, op, "Weapon magic increased to %d", weapon->magic);
		decrease_ob(improver);
		return 1;
	}

	sacrifice_needed = weapon->stats.Str + weapon->stats.Int + weapon->stats.Dex + weapon->stats.Pow + weapon->stats.Con + weapon->stats.Cha + weapon->stats.Wis;

	if (sacrifice_needed < 1)
		sacrifice_needed = 1;
	sacrifice_needed *= 2;

	sacrifice_count = check_sacrifice(op, improver);
	if (sacrifice_count < sacrifice_needed)
	{
		new_draw_info_format(NDI_UNIQUE, 0, op, "You need at least %d %s", sacrifice_needed, improver->slaying);
		return 0;
	}
	eat_item(op, improver->slaying);

	switch (improver->stats.sp)
	{
		case IMPROVE_STR:
			return improve_weapon_stat(op, improver, weapon, (signed char *) &(weapon->stats.Str), 1, (char *) "strength");
		case IMPROVE_DEX:
			return improve_weapon_stat(op, improver, weapon, (signed char *) &(weapon->stats.Dex), 1, (char *) "dexterity");
		case IMPROVE_CON:
			return improve_weapon_stat(op, improver, weapon, (signed char *) &(weapon->stats.Con), 1, (char *) "constitution");
		case IMPROVE_WIS:
			return improve_weapon_stat(op, improver, weapon, (signed char *) &(weapon->stats.Wis), 1,(char *) "wisdom");
		case IMPROVE_CHA:
			return improve_weapon_stat(op, improver, weapon, (signed char *) &(weapon->stats.Cha), 1,(char *) "charisma");
		case IMPROVE_INT:
			return improve_weapon_stat(op, improver, weapon, (signed char *) &(weapon->stats.Int), 1,(char *) "intelligence");
		case IMPROVE_POW:
			return improve_weapon_stat(op, improver, weapon, (signed char *) &(weapon->stats.Pow), 1,(char *) "power");
		default:
			new_draw_info(NDI_UNIQUE, 0, op, "Unknown improvement type.");
	}
	LOG(llevBug, "BUG: improve_weapon: Got to end of function!\n");
	return 0;
}

int check_improve_weapon (object *op, object *tmp)
{
	object *otmp;

	if (op->type!=PLAYER)
		return 0;
	if (blocks_magic(op->map, op->x, op->y))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Something blocks the magic of the scroll.");
		return 0;
	}
	otmp = find_marked_object(op);
	if (!otmp)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You need to mark a weapon object.");
		return 0;
	}
	if (otmp->type != WEAPON)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Marked item is not a weapon");
		return 0;
	}
	new_draw_info(NDI_UNIQUE, 0, op, "Applied weapon builder.");
	improve_weapon(op, tmp, otmp);
	esrv_send_item(op, otmp);
	return 1;
}

/* this code is by b.t. (thomas@nomad.astro.psu.edu) -
 * only 'enchantment' of armour is possible - improving
 * the stats of a player w/ armour as well as a weapon
 * will probably horribly unbalance the game. Magic enchanting
 * depends on the level of the character - ie the plus
 * value (magic) of the armour can never be increased beyond
 * the level of the character / 10 -- rounding upish, nor may
 * the armour value of the piece of equipment exceed either
 * the users level or 90)
 * Modified by MSW for partial resistance.  Only support
 * changing of physical area right now.
 */

int improve_armour(object *op, object *improver, object *armour)
{
	int new_armour;

	new_armour = armour->resist[ATNR_PHYSICAL] + armour->resist[ATNR_PHYSICAL] / 25 + op->level / 20 + 1;
	if (new_armour > 90)
		new_armour = 90;

	if (armour->magic >= (op->level / 10 + 1) || new_armour > op->level)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You are not yet powerful enough");
		new_draw_info(NDI_UNIQUE, 0, op, "to improve this armour.");
		return 0;
	}

	if (new_armour > armour->resist[ATNR_PHYSICAL])
	{
		armour->resist[ATNR_PHYSICAL] = new_armour;
		armour->weight += (unsigned long) ((double) armour->weight * (double) 0.05);
	}
	else
	{
		new_draw_info(NDI_UNIQUE, 0, op, "The armour value of this equipment");
		new_draw_info(NDI_UNIQUE, 0, op, "cannot be further improved.");
	}
	armour->magic++;
	if (op->type == PLAYER)
	{
		esrv_send_item(op, armour);
		if (QUERY_FLAG(armour, FLAG_APPLIED))
			fix_player(op);
	}
	decrease_ob(improver);
	return 1;
}


/*
 * convert_item() returns 1 if anything was converted, otherwise 0
 */
#define CONV_FROM(xyz)	(xyz->slaying)
#define CONV_TO(xyz)	(xyz->other_arch)
#define CONV_NR(xyz)	((unsigned long) xyz->stats.sp)      /* receive number */
#define CONV_NEED(xyz)	((unsigned long) xyz->stats.food)    /* cost number */

int convert_item(object *item, object *converter)
{
	int nr = 0;
	object *tmp;

	/* We make some assumptions - we assume if it takes money as it type,
	 * it wants some amount.  We don't make change (ie, if something costs
	 * 3 gp and player drops a platinum, tough luck) */
	if (!strcmp(CONV_FROM(converter), "money"))
	{
		int cost;
		nr = (item->nrof * item->value) / CONV_NEED(converter);
		if (!nr)
			return 0;
		cost = nr * CONV_NEED(converter) / item->value;
		/* take into account rounding errors */
		if (nr * CONV_NEED(converter) % item->value)
			cost++;
		decrease_ob_nr(item, cost);
	}
	else
	{
		if (item->type == PLAYER || CONV_FROM(converter) != item->arch->name || (CONV_NEED(converter) && CONV_NEED(converter) > item->nrof))
			return 0;

		if (CONV_NEED(converter))
		{
			nr = item->nrof / CONV_NEED(converter);
			decrease_ob_nr(item, nr * CONV_NEED(converter));
		}
		else
		{
			remove_ob(item);
			check_walk_off(item, NULL, MOVE_APPLY_VANISHED);
		}
	}
	item = arch_to_object(converter->other_arch);
	if (CONV_NR(converter))
		item->nrof = CONV_NR(converter);

	if (nr)
		item->nrof *= nr;

	for (tmp = get_map_ob(converter->map, converter->x, converter->y); tmp != NULL; tmp = tmp->above)
	{
		if (tmp->type == SHOP_FLOOR)
			break;
	}
	if (tmp != NULL)
		SET_FLAG(item, FLAG_UNPAID);

	item->x = converter->x;
	item->y = converter->y;
	insert_ob_in_map(item, converter->map, converter, 0);
	return 1;
}


/* a player has opened a container - link him to the
 * list of player which have (perhaps) it opened too.
 */
int container_link(player *pl, object *sack)
{
	int ret = 0;

	/* for safety reasons, lets check this is valid */
	if (sack->attacked_by)
	{
		if (sack->attacked_by->type != PLAYER || !CONTR(sack->attacked_by) || CONTR(sack->attacked_by)->container != sack)
		{
			LOG(llevBug, "BUG: container_link() - invalid player linked: <%s>\n", query_name(sack->attacked_by, NULL));
			sack->attacked_by = NULL;
		}
	}

	/* the open/close logic should be handled elsewhere.
	 * for that reason, this function should only be called
	 * when valid - broken open/close logic elsewhere is bad.
	 * so, give a bug warning out!
	 */
	if (pl->container)
	{
		LOG(llevBug, "BUG: container_link() - called from player with open container!: <%s> sack:<%s>\n", query_name(sack->attacked_by, NULL), query_name(sack, NULL));
		container_unlink(pl, sack);
	}

	pl->container = sack;
	pl->container_count = sack->count;

	pl->container_above = sack->attacked_by;

	if (sack->attacked_by)
		CONTR(sack->attacked_by)->container_below = pl->ob;
	else /* we are the first one opening this container */
	{
		SET_FLAG (sack, FLAG_APPLIED);
		/* faking open container face */
		if (sack->other_arch) /* faking open container face */
		{
			sack->face = sack->other_arch->clone.face;
			sack->animation_id = sack->other_arch->clone.animation_id;
			if (sack->animation_id)
				SET_ANIMATION(sack, (NUM_ANIMATIONS(sack) / NUM_FACINGS(sack)) * sack->direction);
			update_object(sack, UP_OBJ_FACE);
		}
		update_object(sack, UP_OBJ_FACE);
		esrv_update_item (UPD_FLAGS|UPD_FACE, pl->ob, sack);
		/* search & explode a rune in the container */
		container_trap(pl->ob,sack);
		ret = 1;
	}

	esrv_send_inventory(pl->ob, sack);
	/* we are first element */
	pl->container_below = NULL;
	sack->attacked_by = pl->ob;
	sack->attacked_by_count = pl->ob->count;

	return ret;
}

/* remove a player from the container list.
 * unlink is a bit more tricky - pl OR sack can be NULL.
 * if pl == NULL, we unlink ALL players from sack.
 * if sack == NULL, we unlink the current container from pl.
 */
int container_unlink(player *pl, object *sack)
{
	object *tmp, *tmp2;

	if (pl == NULL && sack == NULL)
	{
		LOG(llevBug, "BUG: container_unlink() - *pl AND *sack == NULL!\n");
		return 0;
	}

	if (pl)
	{
		if (!pl->container)
			return 0;

		if (pl->container->count != pl->container_count)
		{
			pl->container = NULL;
			pl->container_count = 0;
			return 0;
		}

		sack = pl->container;
		update_object(sack, UP_OBJ_FACE);
		esrv_close_container(pl->ob);
		/* ok, there is a valid container - unlink the player now */
		if (!pl->container_below && !pl->container_above) /* we are only applier */
		{
			if (pl->container->attacked_by != pl->ob) /* we should be that object... */
			{
				LOG(llevBug, "BUG: container_unlink() - container link don't match player!: <%s> sack:<%s> (%s)\n", query_name(pl->ob, NULL), query_name(sack->attacked_by, NULL), query_name(sack, NULL));
				return 0;
			}

			pl->container = NULL;
			pl->container_count = 0;

			CLEAR_FLAG(sack, FLAG_APPLIED);
			if (sack->other_arch)
			{
				sack->face = sack->arch->clone.face;
				sack->animation_id = sack->arch->clone.animation_id;
				if (sack->animation_id)
					SET_ANIMATION(sack, (NUM_ANIMATIONS(sack) / NUM_FACINGS(sack)) * sack->direction);
				update_object(sack, UP_OBJ_FACE);
			}
			sack->attacked_by = NULL;
			sack->attacked_by_count = 0;
			esrv_update_item (UPD_FLAGS|UPD_FACE, pl->ob, sack);
			return 1;
		}

		/* because there is another player applying that container, we don't close it */
		if (!pl->container_below) /* we are first player in list */
		{
			/* mark above as first player applying this container */
			sack->attacked_by = pl->container_above;
			sack->attacked_by_count = pl->container_above->count;
			CONTR(pl->container_above)->container_below = NULL;

			pl->container_above=NULL;
			pl->container = NULL;
			pl->container_count = 0;
			return 0;
		}

		/* we are somehwere in the middle or last one - it don't matter */
		CONTR(pl->container_below)->container_above = pl->container_above;
		if (pl->container_above)
			CONTR(pl->container_above)->container_below = pl->container_below;

		pl->container_below=NULL;
		pl->container_above=NULL;
		pl->container = NULL;
		pl->container_count = 0;
		return 0;
	}

	CLEAR_FLAG(sack, FLAG_APPLIED);
	if (sack->other_arch)
	{
		sack->face = sack->arch->clone.face;
		sack->animation_id = sack->arch->clone.animation_id;
		if (sack->animation_id)
			SET_ANIMATION(sack, (NUM_ANIMATIONS(sack) / NUM_FACINGS(sack)) * sack->direction);
		update_object(sack, UP_OBJ_FACE);
	}
	tmp = sack->attacked_by;
	sack->attacked_by = NULL;
	sack->attacked_by_count = 0;

	/* if we are here, we are called with (NULL,sack) */
	while (tmp)
	{
		if (!CONTR(tmp) || CONTR(tmp)->container != sack) /* valid player in list? */
		{
			LOG(llevBug,"BUG: container_unlink() - container link list mismatch!: player?:<%s> sack:<%s> (%s)\n", query_name(tmp, NULL), query_name(sack, NULL), query_name(sack->attacked_by, NULL));
			return 1;
		}

		tmp2 = CONTR(tmp)->container_above;
		CONTR(tmp)->container = NULL;
		CONTR(tmp)->container_count = 0;
		CONTR(tmp)->container_below = NULL;
		CONTR(tmp)->container_above = NULL;
		esrv_update_item(UPD_FLAGS|UPD_FACE, tmp, sack);
		esrv_close_container(tmp);
		tmp = tmp2;
	}
	return 1;
}

/*
 * Eneq(@csd.uu.se): Handle apply on containers.
 * op is the player, sack is the container the player is opening or closing.
 * return 1 if an object is apllied somehow or another, 0 if error/no apply
 *
 * Reminder - there are three states for any container - closed (non applied),
 * applied (not open, but objects that match get tossed into it), and open
 * (applied flag set, and op->container points to the open container)
 * I added mutiple apply of one container with a player list. MT 07.02.2004
 */

int esrv_apply_container (object *op, object *sack)
{
	object *cont, *tmp;

	if (op->type!=PLAYER)
	{
		LOG(llevBug, "BUG: esrv_apply_container: called from non player: <%s>!\n", query_name(op, NULL));
		return 0;
	}

	/* cont is NULL or the container player already has opened */
	cont = CONTR(op)->container;

	if (sack == NULL || sack->type != CONTAINER || (cont && cont->type != CONTAINER))
	{
		LOG(llevBug, "BUG: esrv_apply_container: object *sack = %s is not container (cont:<%s>)!\n", query_name(sack, NULL), query_name(cont, NULL));
		return 0;
	}

	/* close container? */
	if (cont) /* if cont != sack || cont == sack - in both cases we close cont */
	{
		/* Trigger the CLOSE event */
		if (trigger_event(EVENT_CLOSE, op, cont, NULL, NULL, 0, 0, 0, SCRIPT_FIX_ALL))
		{
			return 1;
		}

		if (container_unlink(CONTR(op), cont))
			new_draw_info_format(NDI_UNIQUE, 0, op, "You close %s.", query_name(cont, op));
		else
			new_draw_info_format(NDI_UNIQUE, 0, op, "You leave %s.", query_name(cont, op));

		/* we closing the one we applied */
		if (cont == sack)
			return 1;
	}

	/* at this point we ready a container OR we open it! */

	/* If the player is trying to open it (which he must be doing if we got here),
	 * and it is locked, check to see if player has the equipment to open it.
	 */
	/* it's locked or personalized*/
	if (sack->slaying || sack->stats.maxhp)
	{
		if (sack->sub_type1 == ST1_CONTAINER_NORMAL)
		{
			tmp = find_key(op, sack);
			if (tmp)
				new_draw_info_format(NDI_UNIQUE, 0, op, "You unlock %s with %s.", query_name(sack, op), query_name(tmp, op));
			else
			{
				new_draw_info_format(NDI_UNIQUE, 0, op, "You don't have the key to unlock %s.", query_name(sack, op));
				return 0;
			}
		}
		else
		{
			/* party corpse */
			if (sack->sub_type1 == ST1_CONTAINER_CORPSE_party && (CONTR(op)->party_number == -1 || CONTR(op)->party_number != sack->stats.maxhp))
			{
				new_draw_info_format(NDI_UNIQUE, 0, op, "It's not your party's bounty.");
				return 0;
			}
			/* only give player with right name access */
			else if (sack->sub_type1 == ST1_CONTAINER_CORPSE_player && strcmp(sack->slaying, op->name))
			{
				new_draw_info_format(NDI_UNIQUE, 0, op, "It's not your bounty.");
				return 0;
			}
		}
	}
	SET_FLAG(sack, FLAG_BEEN_APPLIED);

	/* By the time we get here, we have made sure any other container has been closed and
	 * if this is a locked container, the player they key to open it. */

	/* There are really two cases - the sack is either on the ground, or the sack is
	 * part of the players inventory.  If on the ground, we assume that the player is
	 * opening it, since if it was being closed, that would have been taken care of above.
	 * If it in the players inventory, we can READY the container. */
	/* container is NOT in players inventory */
	if (sack->env != op)
	{
		/* this is not possible - opening a container inside another container or a another player */
		if (sack->env)
		{
			new_draw_info_format(NDI_UNIQUE, 0, op, "You can't open %s.", query_name(sack, op));
			return 0;
		}

		new_draw_info_format(NDI_UNIQUE, 0, op, "You open %s.", query_name(sack, op));
		container_link(CONTR(op), sack);
	}
	/* sack is in players inventory */
	else
	{
		if (QUERY_FLAG (sack, FLAG_APPLIED)) /* readied sack becoming open */
		{
			new_draw_info_format(NDI_UNIQUE, 0, op, "You open %s.", query_name(sack, op));
			container_link(CONTR(op), sack);
		}
		else
		{
			CLEAR_FLAG (sack, FLAG_APPLIED);
			new_draw_info_format(NDI_UNIQUE, 0, op, "You readied %s.", query_name(sack, op));
			SET_FLAG (sack, FLAG_APPLIED);
			update_object(sack, UP_OBJ_FACE);
			esrv_update_item(UPD_FLAGS, op, sack);
			/* search & explode a rune in the container */
			container_trap(op, sack);
		}
	}
	return 1;
}

/* Frees a monster trapped in container when opened by a player */
void free_container_monster(object *monster, object *op)
{
	int i;
	object *container = monster->env;

	if (container == NULL)
		return;

	/* in container, no walk off check */
	remove_ob(monster);
	monster->x = container->x;
	monster->y = container->y;
	i = find_free_spot(monster->arch, NULL, op->map, monster->x, monster->y, 0, 9);
	if (i != -1)
	{
		monster->x += freearr_x[i];
		monster->y += freearr_y[i];
	}
	fix_monster(monster);
	if (insert_ob_in_map (monster, op->map, monster, 0))
		new_draw_info_format(NDI_UNIQUE, 0, op, "A %s jumps out of the %s.", query_name(monster, NULL), query_name(container, NULL));
}

/* examine the items in a container which gets readied or opened by player.
 * Explode or trigger every trap & rune in there and free trapped monsters.
 */
int container_trap(object *op, object *container)
{
	int ret = 0;
	object *tmp;

	for (tmp = container->inv; tmp; tmp = tmp->below)
	{
		/* search for traps & runes */
		if (tmp->type == RUNE)
		{
			ret++;
			spring_trap(tmp, op);
		}
		/* search for monsters living in containers */
		else if (tmp->type == MONSTER)
		{
			ret++;
			free_container_monster(tmp, op);
		}
	}

	return ret;/* ret=0 -> no trap or monster found/exploded/freed */
}

/*
 * Returns pointer a static string containing gravestone text
 */
char *gravestone_text(object *op)
{
	static char buf2[MAX_BUF];
	char buf[MAX_BUF];
	time_t now = time(NULL);

	strcpy(buf2, "R.I.P.\n\n");

	if (op->type == PLAYER)
		sprintf(buf, "Here rests the hero %s the %s\n", op->name, op->race);
	else
		sprintf(buf, "%s\n", op->name);

	strcat(buf2, buf);

	if (op->type == PLAYER)
		sprintf(buf, "who was killed at level %d\n", op->level);
	else
		sprintf(buf, "who died at level %d.\n\n", op->level);

	strcat(buf2, buf);

	if (op->type == PLAYER)
	{
		sprintf(buf, "by %s.\n\n", strcmp(CONTR(op)->killer, "") ? CONTR(op)->killer : "something nasty");
		strcat(buf2, buf);
	}

	strftime(buf, MAX_BUF, "%b %d %Y", localtime(&now));
	strcat(buf2, buf);

	return buf2;
}

/* Returns true if sacrifice was accepted. */
static int apply_altar(object *altar, object *sacrifice, object *originator)
{
	/* Only players can make sacrifices on spell casting altars. */
	if (altar->stats.sp != -1 && (!originator || originator->type != PLAYER))
		return 0;

	if (operate_altar(altar, &sacrifice))
	{
		/* Simple check.
		 * with an altar.  We call it a Potion - altars are stationary - it
		 * is up to map designers to use them properly.
		 * Change: I changed .sp from 0 = no spell to -1. So we can cast first
		 * spell too... No idea why this was not done in crossfire. ;T-2003 */
		if (altar->stats.sp != -1)
		{
			new_draw_info_format(NDI_WHITE, 0, originator, "The altar casts %s.", spells[altar->stats.sp].name);
			cast_spell(originator, altar, altar->last_sp, altar->stats.sp, 0, spellPotion, NULL);
			/* If it is connected, push the button.  Fixes some problems with
			 * old maps. */
			push_button(altar);
		}
		else
		{
			/* works only once */
			altar->value = 1;
			push_button (altar);
		}

		return sacrifice == NULL;
	}
	else
		return 0;
}


/*
 * Returns 1 if 'op' was destroyed, 0 if not.
 * Largely re-written to not use nearly as many gotos, plus
 * some of this code just looked plain out of date.
 * MSW 2001-08-29
 */
static int apply_shop_mat (object *shop_mat, object *op)
{
	int rv = 0;
	object *tmp;

	/* prevent loops */
	SET_FLAG(op, FLAG_NO_APPLY);

	if (op->type != PLAYER)
	{
		if (QUERY_FLAG(op, FLAG_UNPAID))
		{

			/* Somebody dropped an unpaid item, just move to an adjacent place. */
			int i = find_free_spot(op->arch, NULL, op->map, op->x, op->y, 1, 9);
			if (i != -1)
				rv = transfer_ob(op, op->x + freearr_x[i], op->y + freearr_y[i], 0, shop_mat, NULL);
		}

		rv = teleport(shop_mat, SHOP_MAT, op);
	}
	/* immediate block below is only used for players */
	else if (get_payment(op))
	{
		rv = teleport(shop_mat, SHOP_MAT, op);
		if (shop_mat->msg)
			new_draw_info(NDI_UNIQUE, 0, op, shop_mat->msg);
		/* This check below is a bit simplistic - generally it should be correct,
		 * but there is never a guarantee that the bottom space on the map is
		 * actually the shop floor. */
		else if (!rv && (tmp = get_map_ob(op->map, op->x, op->y)) != NULL && tmp->type != SHOP_FLOOR)
			new_draw_info(NDI_UNIQUE, 0, op, "Thank you for visiting our shop.");
	}
	else
	{
		/* if we get here, a player tried to leave a shop but was not able
		 * to afford the items he has.  We try to move the player so that
		 * they are not on the mat anymore */

		int i = find_free_spot(op->arch, NULL, op->map, op->x, op->y, 1, 9);
		if (i == -1)
			LOG(llevBug, "BUG: Internal shop-mat problem (map:%s object:%s pos: %d,%d).\n", op->map->name, op->name, op->x, op->y);
		else
		{
			remove_ob(op);
			check_walk_off(op, NULL, MOVE_APPLY_DEFAULT);
			op->x += freearr_x[i];
			op->y += freearr_y[i];
			rv = (insert_ob_in_map(op, op->map, shop_mat, 0) == NULL);
			if (op->type == PLAYER)
			{
				/* shop */
				esrv_map_scroll(&CONTR(op)->socket, freearr_x[i],freearr_y[i]);
				CONTR(op)->socket.update_tile = 0;
				CONTR(op)->socket.look_position = 0;
			}
		}
	}

	CLEAR_FLAG(op, FLAG_NO_APPLY);
	return rv;
}

static void apply_sign (object *op, object *sign)
{
	if (sign->msg == NULL)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Nothing is written on it.");
		return;
	}

	if (sign->stats.food)
	{
		if (sign->last_eat >= sign->stats.food)
		{
			if (!QUERY_FLAG(sign, FLAG_WALK_ON) && !QUERY_FLAG(sign, FLAG_FLY_ON))
				new_draw_info(NDI_UNIQUE, 0, op, "You cannot read it anymore.");
			return;
		}
		sign->last_eat++;
	}

	/* Sign or magic mouth?  Do we need to see it, or does it talk to us?
	 * No way to know for sure.
	 *
	 * This check fails for signs with FLAG_WALK_ON/FLAG_FLY_ON.  Checking
	 * for FLAG_INVISIBLE instead of FLAG_WALK_ON/FLAG_FLY_ON would fail
	 * for magic mouths that have been made visible.
	 */
	if (QUERY_FLAG(op, FLAG_BLIND) && !QUERY_FLAG(op, FLAG_WIZ) && !QUERY_FLAG(sign, FLAG_WALK_ON) && !QUERY_FLAG(sign, FLAG_FLY_ON))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You are unable to read while blind.");
		return;
	}

	new_draw_info(NDI_UNIQUE | NDI_NAVY, 0, op, sign->msg);
}


/* 'victim' moves onto 'trap' (trap has FLAG_WALK_ON or FLAG_FLY_ON set) or
 * 'victim' leaves 'trap' (trap has FLAG_WALK_OFF or FLAG_FLY_OFF) set.
 *
 * originator: Player, monster or other object that caused 'victim' to move
 * onto 'trap'.  Will receive messages caused by this action.  May be NULL.
 * However, some types of traps require an originator to function.
 *
 * I added the flags parameter to give the single events more information
 * about whats going on:
 * Most important is the "MOVE_APPLY_VANISHED" flag.
 * If set, a object has leaved a tile but "vanished" and not moved (perhaps
 * its exploded or whatever). This means that some events are not triggered
 * like trapdoors or teleporter traps for example which have a "FLY/MOVE_OFF"
 * set. This will avoid that they touch invalid objects.
 */
void move_apply(object *trap, object *victim, object *originator, int flags)
{
	static int recursion_depth = 0;

	/* move_apply() is the most likely candidate for causing unwanted and
	 * possibly unlimited recursion. */
	/* The following was changed because it was causing perfeclty correct
	 * maps to fail.  1)  it's not an error to recurse:
	 * rune detonates, summoning monster.  monster lands on nearby rune.
	 * nearby rune detonates.  This sort of recursion is expected and
	 * proper.  This code was causing needless crashes. */
	if (recursion_depth >= 500)
	{
		LOG(llevDebug, "WARNING: move_apply(): aborting recursion [trap arch %s, name %s; victim arch %s, name %s]\n", trap->arch->name, trap->name, victim->arch->name, victim->name);
		return;
	}
	recursion_depth++;

	if (trap->head)
		trap = trap->head;

	/* Trigger the TRIGGER event */
	if (trigger_event(EVENT_TRIGGER, victim, trap, originator, NULL, 0, 0, 0, SCRIPT_FIX_NOTHING))
	{
		return;
	}

	switch (trap->type)
	{
			/* these objects can trigger other objects connected to them.
			 * We need to check them at map loading time and other special
			 * events to be sure to have a 100% working map state. */
		case BUTTON:
		case PEDESTAL:
			update_button(trap);
			goto leave;

		case TRIGGER_BUTTON:
		case TRIGGER_PEDESTAL:
		case TRIGGER_ALTAR:
			check_trigger (trap, victim);
			goto leave;

		case CHECK_INV:
			check_inv (victim, trap);
			goto leave;

			/* these objects trigger to but they are "instant".
			 * We don't need to check them when loading. */
		case ALTAR:
			/* sacrifice victim on trap */
			apply_altar(trap, victim, originator);
			goto leave;

		case CONVERTER:
			if (!(flags&MOVE_APPLY_VANISHED))
				convert_item (victim, trap);
			goto leave;

		case PLAYERMOVER:
			/*
			if (trap->attacktype && (trap->level || victim->type != PLAYER))
			{
				if (!trap->stats.maxsp)
					trap->stats.maxsp = 2;
				victim->speed_left = -FABS(trap->stats.maxsp * victim->speed / trap->speed);
				if (victim->speed_left <- 50.0)
					victim->speed_left =- 50.0;
			}
			*/
			goto leave;

			/* should be walk_on/fly_on only */
		case SPINNER:
			if (victim->direction)
			{
				if ((victim->direction = victim->direction + trap->direction) > 8)
					victim->direction = (victim->direction % 8) + 1;
				update_turn_face(victim);
			}
			goto leave;

		case DIRECTOR:
			if (victim->direction)
			{
				if (QUERY_FLAG(victim, FLAG_IS_MISSILE))
				{
					SET_FLAG(victim, FLAG_WAS_REFLECTED);
					if (!missile_reflection_adjust(victim, 0))
						goto leave;
				}

				victim->direction = trap->direction;
				update_turn_face(victim);
			}
			goto leave;

			/* no need to hit anything */
		case MMISSILE:
			if (IS_LIVE(victim) && !(flags&MOVE_APPLY_VANISHED))
			{
				tag_t trap_tag = trap->count;
				hit_player(victim, trap->stats.dam, trap, AT_MAGIC);
				if (!was_destroyed(trap, trap_tag))
					remove_ob(trap);
				check_walk_off(trap, NULL, MOVE_APPLY_VANISHED);
			}
			goto leave;

		case THROWN_OBJ:
			if (trap->inv == NULL || (flags&MOVE_APPLY_VANISHED))
				goto leave;

			/* fallthrough */
		case ARROW:
			/* bad bug: monster throw a object, make a step forwards, step on object ,
			 * trigger this here and get hit by own missile - and will be own enemy.
			 * Victim then is his own enemy and will start to kill herself (this is
			 * removed) but we have not synced victim and his missile. To avoid senseless
			 * action, we avoid hits here */
			if ((IS_LIVE(victim) && trap->speed) && trap->owner != victim)
				hit_with_arrow(trap, victim);
			goto leave;

		case CANCELLATION:
		case BALL_LIGHTNING:
			if (IS_LIVE(victim) && !(flags&MOVE_APPLY_VANISHED))
				hit_player(victim, trap->stats.dam, trap, trap->attacktype);
			else if (victim->material && !(flags&MOVE_APPLY_VANISHED))
				save_throw_object(victim, trap);
			goto leave;

		case CONE:
			/*
			if (IS_LIVE(victim) && trap->speed)
			{
				uint32 attacktype = trap->attacktype & ~AT_COUNTERSPELL;
				if (attacktype)
					hit_player(victim, trap->stats.dam, trap, attacktype);
			}
			*/
			goto leave;

		case FBULLET:
		case BULLET:
			if ((QUERY_FLAG(victim, FLAG_NO_PASS) || IS_LIVE(victim)) && !(flags&MOVE_APPLY_VANISHED))
				check_fired_arch(trap);
			goto leave;

		case TRAPDOOR:
		{
			int max, sound_was_played;
			object *ab;

			if ((flags&MOVE_APPLY_VANISHED))
				goto leave;

			if (!trap->value)
			{
				sint32 tot;
				for (ab = trap->above, tot = 0; ab != NULL; ab = ab->above)
					if (!QUERY_FLAG(ab, FLAG_FLYING))
						tot += (ab->nrof ? ab->nrof : 1) * ab->weight + ab->carrying;

				if (!(trap->value = (tot > trap->weight) ? 1 : 0))
					goto leave;

				SET_ANIMATION(trap, (NUM_ANIMATIONS(trap) / NUM_FACINGS(trap)) * trap->direction + trap->value);
				update_object(trap, UP_OBJ_FACE);
			}
			for (ab = trap->above, max = 100, sound_was_played = 0; --max && ab && !QUERY_FLAG(ab, FLAG_FLYING); ab = ab->above)
			{
				if (!sound_was_played)
				{
					play_sound_map(trap->map, trap->x, trap->y, SOUND_FALL_HOLE, SOUND_NORMAL);
					sound_was_played = 1;
				}
				if (ab->type == PLAYER)
					new_draw_info(NDI_UNIQUE, 0, ab, "You fall into a trapdoor!");

				transfer_ob(ab, (int)EXIT_X(trap), (int)EXIT_Y(trap), trap->last_sp, ab, trap);
			}
			goto leave;
		}


		case PIT:
			/* Pit not open? */
			if ((flags & MOVE_APPLY_VANISHED) || trap->stats.wc > 0)
				goto leave;
			play_sound_map(victim->map, victim->x, victim->y, SOUND_FALL_HOLE, SOUND_NORMAL);

			if (victim->type == PLAYER)
				new_draw_info(NDI_UNIQUE, 0, victim, "You fall through the hole!\n");

			transfer_ob(victim->head ? victim->head : victim, EXIT_X (trap), EXIT_Y (trap), trap->last_sp, victim, trap);
			goto leave;

		case EXIT:
			/* If no map path specified, we assume it is the map path of the exit. - A.T. 2009 */
			if (!EXIT_PATH(trap))
				trap->slaying = trap->map->path;

			if (!(flags & MOVE_APPLY_VANISHED) && victim->type == PLAYER && EXIT_PATH(trap) && EXIT_Y(trap) != -1 && EXIT_X(trap) != -1)
			{
				/* Basically, don't show exits leading to random maps the players output. */
				if (trap->msg && strncmp(EXIT_PATH(trap), "/!", 2) && strncmp(EXIT_PATH(trap), "/random/", 8))
					new_draw_info(NDI_NAVY, 0, victim, trap->msg);
				enter_exit(victim, trap);
			}
			goto leave;

		case SHOP_MAT:
			if (!(flags&MOVE_APPLY_VANISHED))
				apply_shop_mat(trap, victim);
			goto leave;

			/* Drop a certain amount of gold, and have one item identified */
		case IDENTIFY_ALTAR:
			if (!(flags&MOVE_APPLY_VANISHED))
				apply_id_altar(victim, trap, originator);
			goto leave;

		case SIGN:
			/* only player should be able read signs */
			if (victim->type == PLAYER)
				apply_sign(victim, trap);
			goto leave;

		case CONTAINER:
			if (victim->type==PLAYER)
				(void) esrv_apply_container(victim, trap);
			goto leave;

		case RUNE:
			if (!(flags&MOVE_APPLY_VANISHED) && trap->level && IS_LIVE(victim))
				spring_trap(trap, victim);
			goto leave;

			/* we don't have this atm.
			case DEEP_SWAMP:
				if (!(flags&MOVE_APPLY_VANISHED))
					walk_on_deep_swamp(trap, victim);
				goto leave;
			*/
		default:
			LOG(llevDebug, "name %s, arch %s, type %d with fly/walk on/off not handled in move_apply()\n", trap->name, trap->arch->name, trap->type);
			goto leave;
	}

leave:
	recursion_depth--;
}

static void apply_book(object *op, object *tmp)
{
	int lev_diff;

	if (QUERY_FLAG(op, FLAG_BLIND) && !QUERY_FLAG(op,FLAG_WIZ))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You are unable to read while blind.");
		return;
	}

	if (tmp->msg == NULL)
	{
		new_draw_info_format(NDI_UNIQUE, 0, op, "You open the %s and find it empty.", tmp->name);
		return;
	}

	/* need a literacy skill to read stuff! */
	if (!change_skill(op, SK_LITERACY))
	{
		new_draw_info(NDI_UNIQUE, 0,op, "You are unable to decipher the strange symbols.");
		return;
	}

	lev_diff = tmp->level - (SK_level(op) + 5);
	if (!QUERY_FLAG(op, FLAG_WIZ) && lev_diff > 0)
	{
		if (lev_diff < 2)
			new_draw_info(NDI_UNIQUE, 0, op, "This book is just barely beyond your comprehension.");
		else if (lev_diff < 3)
			new_draw_info(NDI_UNIQUE, 0, op, "This book is slightly beyond your comprehension.");
		else if (lev_diff < 5)
			new_draw_info(NDI_UNIQUE, 0, op, "This book is beyond your comprehension.");
		else if (lev_diff < 8)
			new_draw_info(NDI_UNIQUE, 0, op, "This book is quite a bit beyond your comprehension.");
		else if (lev_diff < 15)
			new_draw_info(NDI_UNIQUE, 0, op, "This book is way beyond your comprehension.");
		else
			new_draw_info(NDI_UNIQUE, 0, op, "This book is totally beyond your comprehension.");
		return;
	}

	new_draw_info_format(NDI_UNIQUE, 0, op, "You open the %s and start reading.", tmp->name);

	if (tmp->event_flags & EVENT_FLAG_APPLY)
	{
		/* Trigger the APPLY event */
		trigger_event(EVENT_APPLY, op, tmp, NULL, NULL, 0, 0, 0, SCRIPT_FIX_ALL);
	}
	else
	{
		SockList sl;
		unsigned char sock_buf[MAXSOCKBUF];
		sl.buf = sock_buf;
		SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_BOOK);
		SockList_AddInt(&sl, tmp->weight_limit);
		strcpy((char *)sl.buf + sl.len, tmp->msg);
		sl.len += strlen(tmp->msg) + 1;
		Send_With_Handling(&CONTR(op)->socket, &sl);
	}

	/* gain xp from reading but only if not read before */
	if (!QUERY_FLAG(tmp, FLAG_NO_SKILL_IDENT))
	{
		int exp_gain = calc_skill_exp(op, tmp);
		if (!QUERY_FLAG(tmp, FLAG_IDENTIFIED))
		{
			/* because they just identified it too */
			exp_gain *= 2;
			SET_FLAG(tmp, FLAG_IDENTIFIED);
			/* If in a container, update how it looks */
			if (tmp->env)
				esrv_update_item(UPD_FLAGS|UPD_NAME, op, tmp);
			else
				CONTR(op)->socket.update_tile = 0;
		}
		add_exp(op, exp_gain, op->chosen_skill->stats.sp);
		/* so no more xp gained from this book */
		SET_FLAG(tmp, FLAG_NO_SKILL_IDENT);
	}
}

static void apply_skillscroll(object *op, object *tmp)
{
	switch ((int) learn_skill (op, tmp, NULL, 0, 1))
	{
		case 0:
			new_draw_info(NDI_UNIQUE, 0, op, "You already possess the knowledge ");
			new_draw_info_format(NDI_UNIQUE, 0, op, "held within the %s.\n", query_name(tmp, NULL));
			return;

		case 1:
			new_draw_info_format(NDI_UNIQUE, 0, op, "You succeed in learning %s", skills[tmp->stats.sp].name);
			new_draw_info_format(NDI_UNIQUE, 0, op, "Type 'bind ready_skill %s", skills[tmp->stats.sp].name);
			new_draw_info(NDI_UNIQUE, 0, op, "to store the skill in a key.");
			/* to immediately link new skill to exp object */
			fix_player(op);
			decrease_ob(tmp);
			return;

		default:
			new_draw_info_format(NDI_UNIQUE, 0, op, "You fail to learn the knowledge of the %s.\n", query_name(tmp, NULL));
			decrease_ob(tmp);
			return;
	}
}


/* Special prayers are granted by gods and lost when the follower decides
 * to pray to different gods.  'Force' objects keep track of which
 * prayers are special.
 */

static object *find_special_prayer_mark(object *op, int spell)
{
	object *tmp;

	for (tmp = op->inv; tmp; tmp = tmp->below)
		if (tmp->type == FORCE && tmp->slaying && strcmp(tmp->slaying, "special prayer") == 0 && tmp->stats.sp == spell)
			return tmp;
	return 0;
}

static void insert_special_prayer_mark(object *op, int spell)
{
	object *force = get_archetype("force");
	force->speed = 0;
	update_ob_speed(force);
	FREE_AND_COPY_HASH(force->slaying, "special prayer");
	force->stats.sp = spell;
	insert_ob_in_ob(force, op);
}

extern void do_learn_spell(object *op, int spell, int special_prayer)
{
	object *tmp = find_special_prayer_mark(op, spell);

	if (op->type != PLAYER)
	{
		LOG(llevBug, "BUG: do_learn_spell(): not a player ->%s\n", op->name);
		return;
	}

	/* Upgrade special prayers to normal prayers */
	if (check_spell_known (op, spell))
	{
		new_draw_info_format (NDI_UNIQUE, 0, op, "You already know the spell '%s'!", spells[spell].name);

		if (special_prayer || !tmp)
		{
			LOG(llevBug, "BUG: do_learn_spell(): spell already known, but can't upgrade it\n");
			return;
		}
		remove_ob(tmp);
		return;
	}

	/* Learn new spell/prayer */
	if (tmp)
	{
		LOG(llevBug, "BUG: do_learn_spell(): spell unknown, but special prayer mark present\n");
		remove_ob(tmp);
	}
	play_sound_player_only(CONTR(op), SOUND_LEARN_SPELL, SOUND_NORMAL, 0, 0);
	CONTR(op)->known_spells[CONTR(op)->nrofknownspells++] = spell;
	if (CONTR(op)->nrofknownspells == 1)
		CONTR(op)->chosen_spell = spell;

	/* For godgiven spells the player gets a reminder-mark inserted, that this spell must be removed on changing cults! */
	if (special_prayer)
		insert_special_prayer_mark (op, spell);

	send_spelllist_cmd(op, spells[spell].name, SPLIST_MODE_ADD);
	new_draw_info_format(NDI_UNIQUE, 0, op, "You have learned the spell %s!", spells[spell].name);
}

extern void do_forget_spell(object *op, int spell)
{
	object *tmp;
	int i;

	if (op->type != PLAYER)
	{
		LOG(llevBug, "BUG: do_forget_spell(): not a player: %s (%d)\n", query_name(op, NULL), spell);
		return;
	}

	if (!check_spell_known(op, spell))
	{
		LOG(llevBug, "BUG: do_forget_spell(): spell %d not known\n", spell);
		return;
	}

	play_sound_player_only(CONTR(op), SOUND_LOSE_SOME,SOUND_NORMAL,0,0);
	new_draw_info_format (NDI_UNIQUE, 0, op, "You lose knowledge of %s.", spells[spell].name);

	send_spelllist_cmd(op, spells[spell].name, SPLIST_MODE_REMOVE);
	tmp = find_special_prayer_mark (op, spell);
	if (tmp)
		remove_ob(tmp);

	for (i = 0; i < CONTR(op)->nrofknownspells; i++)
	{
		if (CONTR(op)->known_spells[i] == spell)
		{
			CONTR(op)->known_spells[i] = CONTR(op)->known_spells[--CONTR(op)->nrofknownspells];
			return;
		}
	}

	LOG(llevBug, "BUG: do_forget_spell(): couldn't find spell %d\n", spell);
}

static void apply_spellbook(object *op, object *tmp)
{
	if (QUERY_FLAG(op, FLAG_BLIND) && !QUERY_FLAG(op,FLAG_WIZ))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You are unable to read while blind.");
		return;
	}

	/* artifact_spellbooks have 'slaying' field point to a spell name,
	** instead of having their spell stored in stats.sp.  We should update
	** stats->sp to point to that spell */
	if (tmp->slaying != NULL)
	{
		if ((tmp->stats.sp = look_up_spell_name(tmp->slaying)) < 0)
		{
			tmp->stats.sp = -1;
			new_draw_info_format(NDI_UNIQUE, 0, op, "The book's formula for %s is incomplete", tmp->slaying);
			return;
		}
		/* now clear tmp->slaying since we no longer need it */
		FREE_AND_CLEAR_HASH2(tmp->slaying);
	}

	/* need a literacy skill to learn spells. Also, having a literacy level
	 * lower than the spell will make learning the spell more difficult */
	if (!change_skill(op, SK_LITERACY))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You can't read! Your attempt fails.");
		return;
	}

	if (tmp->stats.sp < 0 || tmp->stats.sp > NROFREALSPELLS || spells[tmp->stats.sp].level > (SK_level(op) + 10))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You are unable to decipher the strange symbols.");
		return;
	}

	new_draw_info_format(NDI_UNIQUE, 0, op, "The spellbook contains the %s level spell %s.", get_levelnumber(spells[tmp->stats.sp].level), spells[tmp->stats.sp].name);

	if (!QUERY_FLAG(tmp, FLAG_IDENTIFIED))
	{
		identify(tmp);
		if (tmp->env)
			esrv_update_item(UPD_FLAGS|UPD_NAME,op,tmp);
		else
			CONTR(op)->socket.update_tile = 0;
	}

	if (check_spell_known(op, tmp->stats.sp) && (tmp->stats.Wis || find_special_prayer_mark(op, tmp->stats.sp) == NULL))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You already know that spell.\n");
		return;
	}

	/* I changed spell learning in 3 ways:
	 *
	 *  1- MU spells use Int to learn, Cleric spells use Wisdom
	 *
	 *  2- The learner's level (in skills sytem level==literacy level; if no
	 *     skills level == overall level) impacts the chances of spell learning.
	 *
	 *  3 -Automatically fail to learn if you read while confused
	 *
	 * Overall, chances are the same but a player will find having a high
	 * literacy rate very useful!  -b.t.
	 */
	if (QUERY_FLAG(op, FLAG_CONFUSED))
	{
		new_draw_info(NDI_UNIQUE,0, op, "In your confused state you flub the wording of the text!");
		/* this needs to be a - number [garbled] */
		scroll_failure(op, 0 - random_roll(0, spells[tmp->stats.sp].level, op, PREFER_LOW), spells[tmp->stats.sp].sp);
	}
	else if (QUERY_FLAG(tmp, FLAG_STARTEQUIP) || random_roll(0, 149, op, PREFER_LOW) - (2 * SK_level(op)) < learn_spell[spells[tmp->stats.sp].flags&SPELL_DESC_WIS ? op->stats.Wis : op->stats.Int])
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You succeed in learning the spell!");
		do_learn_spell(op, tmp->stats.sp, 0);

		/* xp gain to literacy for spell learning */
		if (!QUERY_FLAG(tmp, FLAG_STARTEQUIP))
			add_exp(op, calc_skill_exp(op, tmp), op->chosen_skill->stats.sp);
	}
	else
	{
		play_sound_player_only(CONTR(op), SOUND_FUMBLE_SPELL, SOUND_NORMAL, 0, 0);
		new_draw_info(NDI_UNIQUE, 0, op, "You fail to learn the spell.\n");
	}
	decrease_ob(tmp);
}


static void apply_scroll(object *op, object *tmp)
{
	object *old_skill;
	int scroll_spell = tmp->stats.sp, old_spell = 0;
	rangetype old_shoot = range_none;

	if (QUERY_FLAG(op, FLAG_BLIND) && !QUERY_FLAG(op, FLAG_WIZ))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You are unable to read while blind.");
		return;
	}

	if (!QUERY_FLAG(tmp, FLAG_IDENTIFIED))
		identify(tmp);

	if (scroll_spell < 0 || scroll_spell >= NROFREALSPELLS)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "The scroll just doesn't make sense!");
		return;
	}

	if (op->type == PLAYER)
	{
		old_skill = op->chosen_skill;
		/* players need a literacy skill to read stuff! */
		if (!change_skill(op, SK_LITERACY))
		{
			new_draw_info(NDI_UNIQUE, 0, op, "You are unable to decipher the strange symbols.");
			op->chosen_skill = old_skill;
			return;
		}

		/* thats new: literacy for reading but a player need also the
		 * right spellcasting spell. Reason: the exp goes then in that
		 * skill. This makes scroll different from wands or potions.
		 */
		if (!change_skill(op, (spells[scroll_spell].type == SPELL_TYPE_PRIEST ? SK_PRAYING : SK_SPELL_CASTING)))
		{
			new_draw_info(NDI_UNIQUE, 0, op, "You can read the scroll but you don't understand it.");
			op->chosen_skill = old_skill;
			return;
		}

		/* Now, call here so the right skill is readied -- literacy
		 * isnt necesarily connected to the exp obj to which the xp
		 * will go (for kills made by the magic of the scroll)  */
		old_shoot = CONTR(op)->shoottype;
		old_spell = CONTR(op)->chosen_spell;
		CONTR(op)->shoottype = range_scroll;
		CONTR(op)->chosen_spell = scroll_spell;
	}

	new_draw_info_format(NDI_WHITE, 0, op, "The scroll of %s turns to dust.", spells[tmp->stats.sp].name);

	cast_spell(op, tmp, op->facing ? op->facing : 4, scroll_spell, 0, spellScroll, NULL);
	decrease_ob(tmp);
	if (op->type == PLAYER)
	{
		if (CONTR(op)->golem == NULL)
		{
			CONTR(op)->shoottype = old_shoot;
			CONTR(op)->chosen_spell = old_spell;
		}
		op->chosen_skill = old_skill;
	}
}

/* op opens treasure chest tmp */
static void apply_treasure (object *op, object *tmp)
{
	object *treas;
	tag_t tmp_tag = tmp->count, op_tag = op->count;

	/*  Nice side effect of new treasure creation method is that the treasure
	    for the chest is done when the chest is created, and put into the chest
	    inventory.  So that when the chest burns up, the items still exist.  Also
	    prevents people from moving chests to more difficult maps to get better
	    treasure
	*/
	treas = tmp->inv;

	if (tmp->map)
		play_sound_map(tmp->map, tmp->x, tmp->y, SOUND_OPEN_CONTAINER, SOUND_NORMAL);

	/* msg like "the chest crumbles to dust" */
	if (tmp->msg)
		new_draw_info(NDI_UNIQUE, 0, op, tmp->msg);

	if (treas == NULL)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "The chest was empty.");
		decrease_ob(tmp);
		return;
	}
	do
	{
		remove_ob(treas);
		check_walk_off(treas, NULL, MOVE_APPLY_VANISHED);
		draw_find(op, treas);
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
			spring_trap(treas, op);

		if (was_destroyed(op, op_tag) || was_destroyed(tmp, tmp_tag))
			break;
	}
	while ((treas = tmp->inv) != NULL);

	if (!was_destroyed(tmp, tmp_tag) && tmp->inv == NULL)
		decrease_ob (tmp);
}

void apply_poison(object *op, object *tmp)
{
	if (op->type == PLAYER)
	{
		play_sound_player_only(CONTR(op), SOUND_DRINK_POISON,SOUND_NORMAL, 0, 0);
		new_draw_info(NDI_UNIQUE, 0, op, "Yech! That tasted poisonous!");
		strcpy(CONTR(op)->killer, "poisonous food");
	}
	if (tmp->stats.dam)
	{
		LOG(llevDebug, "Trying to poison player/monster for %d hp\n", tmp->stats.hp);
		/* internal damage part will take care about our poison */
		hit_player(op, tmp->stats.dam, tmp, AT_POISON);
	}
	op->stats.food -= op->stats.food / 4;
	decrease_ob(tmp);
}

static void apply_food(object *op, object *tmp)
{
	if (op->type != PLAYER)
		op->stats.hp = op->stats.maxhp;
	else
	{
		char buf[MAX_BUF];

		/* check if this is a dragon (player), eating some flesh */
		if (tmp->type == FLESH && is_dragon_pl(op) && dragon_eat_flesh(op, tmp))
		{
		}
		else
		{
			/* i don't want power eating - this disallow stacking effects
			 * for food or flesh. */
			if (op->stats.food + tmp->stats.food > 999)
			{
				/*if((op->stats.food+tmp->stats.food)-999>tmp->stats.food/5)
				{
					new_draw_info(NDI_UNIQUE, 0,op,"You are to full to eat this right now!");
					return;
				}*/
				if (tmp->type == FOOD || tmp->type == FLESH)
					new_draw_info(NDI_UNIQUE, 0, op, "You feel full, but what a waste of food!");
				else
					new_draw_info(NDI_UNIQUE, 0, op, "Most of the drink goes down your face not your throat!");
			}

			if (!QUERY_FLAG(tmp, FLAG_CURSED) && !QUERY_FLAG(tmp, FLAG_DAMNED))
			{
				if (!is_dragon_pl(op))
				{
					/* eating message for normal players */
					if (tmp->type==DRINK)
						sprintf(buf, "Ahhh...that %s tasted good.", tmp->name);
					/* tasted "good" if food - tasted "terrible" for flesh for all non dragon players */
					else
						sprintf(buf, "The %s tasted %s", tmp->name, tmp->type == FLESH ? "terrible!" : "good.");
				}
				/* eating message for dragon players - they like it bloody */
				else
					sprintf(buf,"The %s tasted terrible!",tmp->name);

				op->stats.food += tmp->stats.food;
			}
			else /* cursed/damned = food is decreased instead of increased */
			{
				int ft = tmp->stats.food;

				sprintf(buf, "The %s tasted terrible!", tmp->name);
				if (ft > 0)
					ft =- ft;
				op->stats.food += ft;
			}
			new_draw_info(NDI_UNIQUE, 0, op, buf);

			/* adjust food to borders when needed */
			if (op->stats.food > 999)
				op->stats.food = 999;
			else if (op->stats.food < 0)
				op->stats.food = 0;

			/* special food hack -b.t. */
			if (tmp->title || QUERY_FLAG(tmp, FLAG_CURSED)|| QUERY_FLAG(tmp, FLAG_DAMNED))
				eat_special_food(op, tmp);
		}
	}
	decrease_ob(tmp);
}

/* this is used from DRINK, FOOD & POISON forces now - to include buff/debuff
 * effects of stats & resists to the player. Cursed & damned effects are in too
 */
void create_food_force(object* who, object *food, object *force)
{
	int i;

	force->stats.Str = food->stats.Str;
	force->stats.Pow = food->stats.Pow;
	force->stats.Dex = food->stats.Dex;
	force->stats.Con = food->stats.Con;
	force->stats.Int = food->stats.Int;
	force->stats.Wis = food->stats.Wis;
	force->stats.Cha = food->stats.Cha;

	for (i = 0; i < NROFATTACKS; i++)
		force->resist[i] = food->resist[i];

	/* if damned, set all negative if not and double or triple them */
	if (QUERY_FLAG(food, FLAG_CURSED))
	{
		if (force->stats.Str > 0)
			force->stats.Str =- force->stats.Str;
		force->stats.Str *= 2;
		if (force->stats.Dex > 0)
			force->stats.Dex =- force->stats.Dex;
		force->stats.Dex *= 2;
		if (force->stats.Con > 0)
			force->stats.Con =- force->stats.Con;
		force->stats.Con *= 2;
		if (force->stats.Int > 0)
			force->stats.Int =- force->stats.Int;
		force->stats.Int *= 2;
		if (force->stats.Wis > 0)
			force->stats.Wis =- force->stats.Wis;
		force->stats.Wis *= 2;
		if (force->stats.Pow > 0)
			force->stats.Pow =- force->stats.Pow;
		force->stats.Pow *= 2;
		if (force->stats.Cha > 0)
			force->stats.Cha =- force->stats.Cha;
		force->stats.Cha *= 2;

		for (i = 0; i < NROFATTACKS; i++)
		{
			if (force->resist[i] > 0)
				force->resist[i] =- force->resist[i];
			force->resist[i] *= 2;
		}
	}

	if (QUERY_FLAG(food, FLAG_DAMNED))
	{
		if (force->stats.Pow >  0)
			force->stats.Pow =-force->stats.Pow;
		force->stats.Pow *= 3;
		if (force->stats.Str > 0)
			force->stats.Str =- force->stats.Str;
		force->stats.Str *= 3;
		if (force->stats.Dex > 0)
			force->stats.Dex =- force->stats.Dex;
		force->stats.Dex *= 3;
		if (force->stats.Con > 0)
			force->stats.Con =- force->stats.Con;
		force->stats.Con *= 3;
		if (force->stats.Int > 0)
			force->stats.Int =- force->stats.Int;
		force->stats.Int *= 3;
		if (force->stats.Wis > 0)
			force->stats.Wis =- force->stats.Wis;
		force->stats.Wis *= 3;
		if (force->stats.Cha > 0)
			force->stats.Cha =- force->stats.Cha;
		force->stats.Cha *= 3;

		for (i = 0; i < NROFATTACKS; i++)
		{
			if (force->resist[i] > 0)
				force->resist[i] =- force->resist[i];
			force->resist[i] *= 3;
		}
	}

	if (food->speed_left)
		force->speed = food->speed_left;

	SET_FLAG(force, FLAG_APPLIED);
	force = insert_ob_in_ob(force, who);
	/* Mostly to display any messages */
	change_abil(who,force);
	/* This takes care of some stuff that change_abil() */
	fix_player(who);
}

/* OUTDATED: eat_special_food() - some food may (temporarily) alter
 * player status. We do it w/ this routine and cast_change_attr().
 * Note the dircection is set to "99"  so that cast_change_attr()
 * will only modify the user's status. We shouldnt be able to
 * effect others by eating food!
 * -b.t.
 */
/* NEW: if we are here, the food gives specials. +-hp or sp,
 * resistance or stats.
 * Food can be good or bad (means good effect or bad effects) and
 * cursed or not. If a food is "good" (for example Str+1 and Dex+1),
 * then it put this effects as force in the player for some time.
 * If a good effect food is cursed, all '+' values are turned to '-'.
 * Is a bad food (Str -1, Dex -1) is uncursed, it gives just this values.
 * Is a bad food is cursed, all '-' are doubled.
 * sp/hp will be add/sub directly. It will no poison effect inflicted -
 * for this POISON food should be used.
 * All bad effect food should be generated on default as cursed.
 * food effects can stack. For really powerful food, a high food value should
 * be used - a player can't eat it then when he is full and he will be full
 * fast.
 * For DAMNED food, its the same like cursed - except the negative effects are 3 times worser.
 * On the other side include for a "remove poison" herb a food of 1 to avoid
 * that the player can't eat it when full. MT-2003
 */
void eat_special_food(object *who, object *food)
{
	/* if there is any stat or resist value - create force for the object! */
	if (food->stats.Pow ||food->stats.Str || food->stats.Dex || food->stats.Con || food->stats.Int || food->stats.Wis || food->stats.Cha)
		create_food_force(who, food, get_archetype("force"));
	else
	{
		int i;
		for (i = 0; i < NROFATTACKS; i++)
		{
			if (food->resist[i] > 0)
			{
				create_food_force(who, food, get_archetype("force"));
				break;
			}
		}
	}

	/* check for hp, sp change */
	if (food->stats.hp != 0)
	{
		if (QUERY_FLAG(food, FLAG_CURSED) || QUERY_FLAG(food, FLAG_DAMNED))
		{
			int tmp = food->stats.hp;

			if (tmp > 0)
				tmp =- tmp;
			strcpy(CONTR(who)->killer, food->name);
			if (QUERY_FLAG(food, FLAG_CURSED))
				who->stats.hp += tmp * 2;
			else
				who->stats.hp += tmp * 3;

			new_draw_info(NDI_UNIQUE, 0, who, "Eck!... that was rotten food!");
		}
		else
		{
			if (food->stats.hp > 0)
				new_draw_info(NDI_UNIQUE, 0, who, "You begin to feel better.");
			else
				new_draw_info(NDI_UNIQUE, 0, who, "Eck!... that was rotten food!");
			who->stats.hp += food->stats.hp;
		}
	}

	if (food->stats.sp != 0)
	{
		if (QUERY_FLAG(food, FLAG_CURSED) || QUERY_FLAG(food, FLAG_DAMNED))
		{
			int tmp = food->stats.sp;

			if (tmp > 0)
				tmp=-tmp;

			new_draw_info(NDI_UNIQUE, 0, who, "You are drained of mana!");
			if (QUERY_FLAG(food, FLAG_CURSED))
				who->stats.sp += tmp * 2;
			else
				who->stats.sp += tmp * 3;

			if (who->stats.sp < 0)
				who->stats.sp = 0;
		}
		else
		{
			new_draw_info(NDI_UNIQUE, 0, who, "You feel a rush of magical energy!");
			who->stats.sp += food->stats.sp;
			/*TODO: place limit on max sp from food? */
		}
	}
}

/*
 * A dragon is eating some flesh. If the flesh contains resistances,
 * there is a chance for the dragon's skin to get improved.
 *
 * attributes:
 *     object *op        the object (dragon player) eating the flesh
 *     object *meal      the flesh item, getting chewed in dragon's mouth
 * return:
 *     int               1 if eating successful, 0 if it doesn't work
 */
int dragon_eat_flesh(object *op, object *meal)
{
	object *skin = NULL;    /* pointer to dragon skin force*/
	object *abil = NULL;    /* pointer to dragon ability force*/
	object *tmp = NULL;     /* tmp. object */

	char buf[MAX_BUF];            /* tmp. string buffer */
	double chance;                /* improvement-chance of one resistance type */
	double maxchance = 0;           /* highest chance of any type */
	double bonus = 0;               /* level bonus (improvement is easier at lowlevel) */
	double mbonus = 0;              /* monster bonus */
	int atnr_winner[NROFATTACKS]; /* winning candidates for resistance improvement */
	int winners = 0;                /* number of winners */
	int i;                        /* index */

	/* let's make sure and doublecheck the parameters */
	if (meal->type != FLESH || !is_dragon_pl(op))
		return 0;

	/* now grab the 'dragon_skin'- and 'dragon_ability'-forces from the player's inventory */
	for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
	{
		if (tmp->type == FORCE)
		{
			if (strcmp(tmp->arch->name, "dragon_skin_force") == 0)
				skin = tmp;
			else if (strcmp(tmp->arch->name, "dragon_ability_force") == 0)
				abil = tmp;
		}
	}

	/* if either skin or ability are missing, this is an old player
		which is not to be considered a dragon -> bail out */
	if (skin == NULL || abil == NULL)
		return 0;

	/* now start by filling stomache and health, according to food-value */
	if ((999 - op->stats.food) < meal->stats.food)
		op->stats.hp += (999 - op->stats.food) / 50;
	else
		op->stats.hp += meal->stats.food / 50;

	if (op->stats.hp > op->stats.maxhp)
		op->stats.hp = op->stats.maxhp;

	op->stats.food = MIN(999, op->stats.food + meal->stats.food);

	/* on to the interesting part: chances for adding resistance */
	for (i = 0; i < NROFATTACKS; i++)
	{
		/* got positive resistance, now calculate improvement chance (0-100) */
		if (meal->resist[i] > 0 && atnr_is_dragon_enabled(i))
		{
			/* this bonus makes resistance increase easier at lower levels */
			bonus = (MAXLEVEL - op->level) * 30. / ((double)MAXLEVEL);
			/* additional bonus for resistance of ability-focus */
			if (i == abil->stats.exp)
				bonus += 5;

			/* monster bonus increases with level, because high-level flesh is too rare */
			mbonus = op->level * 20. / ((double)MAXLEVEL);

			chance = (((double)MIN(op->level + bonus, meal->level + bonus + mbonus)) * 100. / ((double)MAXLEVEL)) - skin->resist[i];

			if (chance >= 0.)
				chance += 1.;
			else
				chance = (chance < -12) ? 0. : 1. / pow(2., -chance);

			/* chance is proportional to amount of resistance (max. 50) */
			chance *= ((double)(MIN(meal->resist[i], 50))) / 50.;

			/* doubled chance for resistance of ability-focus */
			if (i == abil->stats.exp)
				chance = MIN(100., chance * 2.);

			/* now make the throw and save all winners (Don't insert luck bonus here!) */
			if (RANDOM() % 10000 < (int)(chance * 100))
			{
				atnr_winner[winners] = i;
				winners++;
			}

			if (chance > maxchance)
				maxchance = chance;
		}
	}

	/* print message according to maxchance */
	if (maxchance > 50.)
		sprintf(buf, "Hmm! The %s tasted delicious!", meal->name);
	else if (maxchance > 10.)
		sprintf(buf, "The %s tasted very good.", meal->name);
	else if (maxchance > 1.)
		sprintf(buf, "The %s tasted good.", meal->name);
	else if (maxchance > 0.0001)
		sprintf(buf, "The %s had a boring taste.", meal->name);
	else if (meal->last_eat > 0 && atnr_is_dragon_enabled(meal->last_eat))
		sprintf(buf, "The %s tasted strange.", meal->name);
	else
		sprintf(buf, "The %s had no taste.", meal->name);

	new_draw_info(NDI_UNIQUE, 0, op, buf);

	/* now choose a winner if we have any */
	i = -1;
	if (winners > 0)
		i = atnr_winner[RANDOM() % winners];

	if (i >= 0 && i < NROFATTACKS && skin->resist[i] < 95)
	{
		/* resistance increased! */
		skin->resist[i]++;
		fix_player(op);

		sprintf(buf, "Your skin is now more resistant to %s!", change_resist_msg[i]);
		new_draw_info(NDI_UNIQUE|NDI_RED, 0, op, buf);
	}

	/* if this flesh contains a new ability focus, we mark it
		into the ability_force and it will take effect on next level */
	if (meal->last_eat > 0 && atnr_is_dragon_enabled(meal->last_eat) && meal->last_eat != abil->last_eat)
	{
		/* write: last_eat <new attnr focus> */
		abil->last_eat = meal->last_eat;

		if (meal->last_eat != abil->stats.exp)
		{
			sprintf(buf, "Your metabolism prepares to focus on %s!", change_resist_msg[meal->last_eat]);
			new_draw_info(NDI_UNIQUE, 0, op, buf);
			sprintf(buf, "The change will happen at level %d", abil->level + 1);
			new_draw_info(NDI_UNIQUE, 0, op, buf);
		}
		else
		{
			sprintf(buf, "Your metabolism will continue to focus on %s.", change_resist_msg[meal->last_eat]);
			new_draw_info(NDI_UNIQUE, 0, op, buf);
			abil->last_eat = 0;
		}
	}
	return 1;
}

static void apply_savebed(object *pl)
{
	if (!CONTR(pl)->name_changed || !pl->stats.exp)
	{
		new_draw_info(NDI_UNIQUE, 0, pl, "You don't deserve to save your character yet.");
		return;
	}

#if 0
	leave_map(pl);
	pl->direction = 0;
	new_draw_info_format(NDI_UNIQUE | NDI_ALL, 5, pl, "%s leaves the game.", pl->name);
#endif
	/* update respawn position */
	strcpy(CONTR(pl)->savebed_map, pl->map->path);
	CONTR(pl)->bed_x = pl->x;
	CONTR(pl)->bed_y = pl->y;

	strcpy(CONTR(pl)->killer, "left");
	/* Always check score */
	check_score(pl, 0);

	new_draw_info(NDI_UNIQUE, 0, pl, "You save and leave the game. Bye!\nLeaving...");
	CONTR(pl)->socket.status = Ns_Dead;
}

static void apply_armour_improver(object *op, object *tmp)
{
	object *armor;

	if (blocks_magic(op->map, op->x, op->y))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Something blocks the magic of the scroll.");
		return;
	}

	armor = find_marked_object(op);
	if (!armor)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You need to mark an armor object.");
		return;
	}

	if (armor->type != ARMOUR && armor->type != CLOAK && armor->type != BOOTS && armor->type != GLOVES && armor->type != BRACERS && armor->type != SHIELD && armor->type != HELMET)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Your marked item is not armour!\n");
		return;
	}

	new_draw_info(NDI_UNIQUE, 0, op, "Applying armour enchantment.");
	improve_armour(op, tmp, armor);
}



/* is_legal_2ways_exit (object* op, object *exit)
 * this fonction return true if the exit
 * is not a 2 ways one or it is 2 ways, valid exit.
 * A valid 2 way exit means:
 *   -You can come back (there is another exit at the other side)
 *   -You are
 *         - the owner of the exit
 *         - or in the same party as the owner
 *
 * Note: a owner in a 2 way exit is saved as the owner's name
 * in the field exit->name cause the field exit->owner doesn't
 * survive in the swapping (in fact the whole exit doesn't survive).
 */
int is_legal_2ways_exit(object* op, object *exit)
{
	object *tmp;
	object *exit_owner;
	player *pp;
	mapstruct *exitmap;
	/* This is not a 2 way, so it is legal */
	if (exit->stats.exp != 1)
		return 1;
	/* To know if an exit has a correspondant, we look at
	 * all the exits in destination and try to find one with same path as
	 * the current exit's position */
	if (!strncmp(EXIT_PATH (exit), settings.localdir, strlen(settings.localdir)))
		exitmap = ready_map_name(EXIT_PATH (exit), MAP_NAME_SHARED|MAP_PLAYER_UNIQUE);
	else
		exitmap = ready_map_name(EXIT_PATH (exit), MAP_NAME_SHARED);

	if (exitmap)
	{
		tmp = get_map_ob(exitmap, EXIT_X(exit), EXIT_Y(exit));
		if (!tmp)
			return 0;

		for ((tmp = get_map_ob(exitmap, EXIT_X(exit), EXIT_Y(exit))); tmp; tmp = tmp->above)
		{
			/* Not an exit */
			if (tmp->type != EXIT)
				continue;

			/* Not a valid exit */
			if (!EXIT_PATH(tmp))
				continue;

			/* Not in the same place */
			if ((EXIT_X(tmp) != exit->x) || (EXIT_Y(tmp) != exit->y))
				continue;

			/* Not in the same map */
			if (!exit->race && exit->map->path == EXIT_PATH(tmp))
				continue;

			/* From here we have found the exit is valid. However we do
			 * here the check of the exit owner. It is important for the
			 * town portals to prevent strangers from visiting your appartments */
			/* No owner, free for all! */
			if (!exit->race)
				return 1;

			exit_owner = NULL;
			for (pp = first_player; pp; pp = pp->next)
			{
				if (!pp->ob)
					continue;

				if (pp->ob->name != exit->race)
					continue;

				/* We found a player which correspond to the player name */
				exit_owner= pp->ob;
				break;
			}

			/* No more owner */
			if (!exit_owner)
				return 0;

			/* It is your exit */
			if (CONTR(exit_owner) == CONTR(op))
				return 1;

			if (exit_owner && (CONTR(op)) && ((CONTR(exit_owner)->party_number <= 0) || (CONTR(exit_owner)->party_number != CONTR(op)->party_number)))
				return 0;

			return 1;
		}
	}
	return 0;
}

/* Return value:
 *   0: player or monster can't apply objects of that type
 *   2: objects of that type can't be applied if not in inventory
 *   1: has been applied, or there was an error applying the object
 *
 * op is the object that is causing object to be applied, tmp is the object
 * being applied.
 *
 * aflag is special (always apply/unapply) flags.  Nothing is done with
 * them in this function - they are passed to apply_special
 */

int manual_apply(object *op, object *tmp, int aflag)
{
	if (tmp->head)
		tmp = tmp->head;

	if (QUERY_FLAG(tmp, FLAG_UNPAID) && !QUERY_FLAG(tmp, FLAG_APPLIED))
	{
		if (op->type == PLAYER)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "You should pay for it first.");
			return 1;
		}
		/* monsters just skip unpaid items */
		else
			return 0;
	}

	if (tmp->type != CONTAINER && tmp->env != op && op->type == PLAYER && !check_map_owner(op->map, op))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You can't apply that here.");
		return 1;
	}
	else if (tmp->type == CONTAINER && tmp->sub_type1 == 1 && tmp->env == op)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "This is a special apartment extension - place it in your apartment!");
		return 1;
	}

	/* monsters must not apply random chests, nor magic_mouths with a counter */
	if (op->type != PLAYER && tmp->type == TREASURE)
		return 0;

	/* Trigger the APPLY event */
	if (trigger_event(EVENT_APPLY, op, tmp, NULL, NULL, &aflag, 0, 0, SCRIPT_FIX_ACTIVATOR))
	{
		return 1;
	}

	/* control apply by controling a set exp object level or player exp level */
	if (tmp->item_level)
	{
		int tmp_lev;

		if (tmp->item_skill)
			tmp_lev = find_skill_exp_level(op, tmp->item_skill);
		else
			tmp_lev = op->level;

		if (tmp->item_level > tmp_lev)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "The item level is too high to apply.");
			return 1;
		}
	}

	switch (tmp->type)
	{
		case HOLY_ALTAR:
			new_draw_info_format(NDI_UNIQUE, 0, op, "You touch the %s.", tmp->name);
			if (change_skill(op, SK_PRAYING))
				pray_at_altar(op, tmp);
			else
				new_draw_info(NDI_UNIQUE, 0, op, "Nothing happens. It seems you miss the right skill.");
			return 1;
			break;

		case CF_HANDLE:
			new_draw_info(NDI_UNIQUE, 0, op, "You turn the handle.");
			play_sound_map(op->map, op->x, op->y, SOUND_TURN_HANDLE, SOUND_NORMAL);
			tmp->value = tmp->value ? 0 : 1;
			SET_ANIMATION(tmp, ((NUM_ANIMATIONS(tmp) / NUM_FACINGS(tmp)) * tmp->direction) + tmp->value);
			update_object(tmp, UP_OBJ_FACE);
			push_button(tmp);
			return 1;

		case TRIGGER:
			if (check_trigger(tmp, op))
			{
				new_draw_info(NDI_UNIQUE, 0, op, "You turn the handle.");
				play_sound_map(tmp->map, tmp->x, tmp->y, SOUND_TURN_HANDLE, SOUND_NORMAL);
			}
			else
				new_draw_info(NDI_UNIQUE, 0, op, "The handle doesn't move.");
			return 1;

		case EXIT:
			if (op->type != PLAYER)
				return 0;

			/* If no map path specified, we assume it is the map path of the exit. - A.T. 2009 */
			if (!EXIT_PATH(tmp))
				tmp->slaying = tmp->map->path;

			if (!EXIT_PATH(tmp) || !is_legal_2ways_exit(op, tmp) || (EXIT_Y(tmp) == -1 && EXIT_X(tmp) == -1))
				new_draw_info_format(NDI_UNIQUE, 0, op, "The %s is closed.", query_name(tmp, NULL));
			else
			{
				/* Don't display messages for random maps. */
				if (tmp->msg && strncmp(EXIT_PATH(tmp), "/!", 2) && strncmp(EXIT_PATH(tmp), "/random/", 8))
					new_draw_info(NDI_NAVY, 0, op, tmp->msg);
				enter_exit(op, tmp);
			}
			return 1;

		case SIGN:
			apply_sign(op, tmp);
			return 1;

		case BOOK:
			if (op->type == PLAYER)
			{
				apply_book(op, tmp);
				return 1;
			}
			else
				return 0;

		case SKILLSCROLL:
			if (op->type == PLAYER)
			{
				apply_skillscroll(op, tmp);
				return 1;
			}
			return 0;

		case SPELLBOOK:
			if (op->type == PLAYER)
			{
				apply_spellbook(op, tmp);
				return 1;
			}
			return 0;

		case SCROLL:
			apply_scroll(op, tmp);
			return 1;

		case POTION:
			(void) apply_potion(op, tmp);
			return 1;

		case TYPE_LIGHT_APPLY:
			apply_player_light(op, tmp);
			return 1;

		case TYPE_LIGHT_REFILL:
			apply_player_light_refill(op, tmp);
			return 1;

			/* Eneq(@csd.uu.se): Handle apply on containers. */
		case CLOSE_CON:
			if (op->type == PLAYER)
				(void) esrv_apply_container(op, tmp->env);
			return 1;

		case CONTAINER:
			if (op->type == PLAYER)
				(void) esrv_apply_container(op, tmp);
			return 1;

		case TREASURE:
			apply_treasure(op, tmp);
			return 1;

		case WEAPON:
		case ARMOUR:
		case BOOTS:
		case GLOVES:
		case AMULET:
		case GIRDLE:
		case BRACERS:
		case SHIELD:
		case HELMET:
		case RING:
		case CLOAK:
		case WAND:
		case ROD:
		case HORN:
		case SKILL:
		case BOW:
			/* not in inventory */
			if (tmp->env != op)
				return 2;
			(void) apply_special(op, tmp, aflag);
			return 1;

		case DRINK:
		case FOOD:
		case FLESH:
			apply_food(op, tmp);
			return 1;

		case POISON:
			apply_poison(op, tmp);
			return 1;

		case SAVEBED:
			if (op->type == PLAYER)
			{
				apply_savebed(op);
				return 1;
			}
			else
				return 0;

		case ARMOUR_IMPROVER:
			if (op->type == PLAYER)
			{
				apply_armour_improver(op, tmp);
				return 1;
			}
			else
				return 0;

		case WEAPON_IMPROVER:
			(void) check_improve_weapon(op, tmp);
			return 1;

		case CLOCK:
			if (op->type == PLAYER)
			{
				char buf[MAX_BUF];
				timeofday_t tod;

				get_tod(&tod);
				sprintf(buf, "It is %d minute%s past %d o'clock %s", tod.minute + 1, ((tod.minute + 1 < 2) ? "" : "s"), ((tod.hour % (HOURS_PER_DAY / 2) == 0) ? (HOURS_PER_DAY / 2) : ((tod.hour) % (HOURS_PER_DAY / 2))), ((tod.hour >= (HOURS_PER_DAY / 2)) ? "pm" : "am"));

				new_draw_info(NDI_UNIQUE, 0, op, buf);
				return 1;
			}
			else
				return 0;

		case MENU:
			if (op->type == PLAYER)
			{
				shop_listing(op);
				return 1;
			}
			else
				return 0;

		case POWER_CRYSTAL:
			/* see egoitem.c */
			apply_power_crystal(op,tmp);
			return 1;

			/* for lighting torches/lanterns/etc */
		case LIGHTER:
			if (op->type == PLAYER)
			{
				apply_lighter(op, tmp);
				return 1;
			}
			else
				return 0;

			/* Nothing from the above... but show a message if it has one. */
		default:
			if (tmp->msg)
			{
				new_draw_info(NDI_UNIQUE, 0, op, tmp->msg);
				return 1;
			}
			return 0;
	}
}


/* quiet suppresses the "don't know how to apply" and "you must get it first"
 * messages as needed by player_apply_below().  But there can still be
 * "but you are floating high above the ground" messages.
 *
 * Same return value as apply() function.
 */
int player_apply(object *pl, object *op, int aflag, int quiet)
{
	int tmp;

	if (op->env == NULL && QUERY_FLAG(pl, FLAG_FLYING))
	{
		/* player is flying and applying object not in inventory */
		if (!QUERY_FLAG(pl, FLAG_WIZ) && !QUERY_FLAG(op, FLAG_FLYING) && !QUERY_FLAG(op, FLAG_FLY_ON))
		{
			new_draw_info(NDI_UNIQUE, 0, pl, "But you are floating high above the ground!");
			return 0;
		}
	}

	if (QUERY_FLAG(op, FLAG_WAS_WIZ) && !QUERY_FLAG(pl, FLAG_WAS_WIZ) && op->type != PLAYER)
	{
		play_sound_map(pl->map, pl->x, pl->y, SOUND_OB_EVAPORATE, SOUND_NORMAL);
		new_draw_info(NDI_UNIQUE, 0, pl, "The object disappears in a puff of smoke!");
		new_draw_info(NDI_UNIQUE, 0, pl, "It must have been an illusion.");
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		return 1;
	}

	/* see last_used in player
	CONTR(pl)->last_used = op;
	CONTR(pl)->last_used_id = op->count;
	*/

	tmp = manual_apply(pl, op, aflag);
	if (!quiet)
	{
		if (tmp == 0)
			new_draw_info_format (NDI_UNIQUE, 0, pl, "I don't know how to apply the %s.", query_name(op, NULL));
		else if (tmp == 2)
			new_draw_info_format (NDI_UNIQUE, 0, pl, "You must get it first!\n");
	}
	return tmp;
}

/* player_apply_below attempts to apply the object 'below' the player.
 * If the player has an open container, we use that for below, otherwise
 * we use the ground.
 */
void player_apply_below (object *pl)
{
	object *tmp, *next;
	int floors;

	if (pl->type != PLAYER)
	{
		LOG(llevBug, "BUG: player_apply_below() called for non player object >%s<\n", query_name(pl, NULL));
		return;
	}
	/* If using a container, set the starting item to be the top
	 * item in the container.  Otherwise, use the map.
	 */
	/* i removed this... it can lead in very evil situations like
	 * someone hammers on the /apply macro for fast leaving a map but
	 * invokes dozens of potions from a open bag.....
	 */
	/*tmp = (pl->contr->container != NULL) ? pl->contr->container->inv : pl->below;*/
	tmp = pl->below;

	/* This is perhaps more complicated.  However, I want to make sure that
	 * we don't use a corrupt pointer for the next object, so we get the
	 * next object in the stack before applying.  This is can only be a
	 * problem if player_apply() has a bug in that it uses the object but does
	 *  not return a proper value.
	 */
	for (floors = 0; tmp != NULL; tmp = next)
	{
		next = tmp->below;
		if (QUERY_FLAG(tmp, FLAG_IS_FLOOR))
			floors++;
		/* process only floor objects after first floor object */
		else if (floors > 0)
			return;

		if (!IS_INVISIBLE(tmp, pl) || QUERY_FLAG(tmp, FLAG_WALK_ON) || QUERY_FLAG(tmp, FLAG_FLY_ON))
		{
			if (player_apply(pl, tmp, 0, 1) == 1)
				return;
		}

		/* process at most two floor objects */
		if (floors >= 2)
			return;
	}
}


/* who is the object using the object.
 * op is the object they are using.
 * aflags is special flags (0 - normal/toggle, AP_APPLY=always apply,
 * AP_UNAPPLY=always unapply).
 *
 * Optional flags:
 *   AP_NO_MERGE: don't merge an unapplied object with other objects
 *   AP_IGNORE_CURSE: unapply cursed items
 *
 * Usage example:  apply_special (who, op, AP_UNAPPLY | AP_IGNORE_CURSE)
 *
 * apply_special() doesn't check for unpaid items.
 */
int apply_special(object *who, object *op, int aflags)
{
	/* wear/wield */
	int basic_flag = aflags & AP_BASIC_FLAGS;
	int tmp_flag = 0;
	object *tmp;
	char buf[HUGE_BUF];
	int i;

	if (who == NULL)
	{
		LOG(llevBug,"BUG: apply_special() from object without environment.\n");
		return 1;
	}

	/* op is not in inventory */
	if (op->env != who)
		return 1;

	/* Needs to be initialized */
	buf[0] = '\0';

	if (QUERY_FLAG(op, FLAG_APPLIED))
	{
		/* always apply, so no reason to unapply */
		if (basic_flag == AP_APPLY)
			return 0;
		if (!(aflags & AP_IGNORE_CURSE) && (QUERY_FLAG(op, FLAG_CURSED) || QUERY_FLAG(op, FLAG_DAMNED)))
		{
			new_draw_info_format(NDI_UNIQUE, 0, who, "No matter how hard you try, you just can't remove it!");
			return 1;
		}

		if (QUERY_FLAG(op, FLAG_PERM_CURSED))
			SET_FLAG(op, FLAG_CURSED);
		if (QUERY_FLAG(op, FLAG_PERM_DAMNED))
			SET_FLAG(op, FLAG_DAMNED);

		CLEAR_FLAG(op, FLAG_APPLIED);
		switch (op->type)
		{
			case WEAPON:
				(void) change_abil(who, op);
				CLEAR_FLAG(who, FLAG_READY_WEAPON);
				sprintf(buf, "You unwield %s.", query_name(op, NULL));
				break;

				/* allows objects to impart skills */
			case SKILL:
				if (op != who->chosen_skill)
					LOG(llevBug, "BUG: apply_special(): applied skill is not chosen skill\n");
				if (who->type == PLAYER)
				{
					CONTR(who)->shoottype = range_none;
					CONTR(who)->last_value = -1;
					if (!IS_INVISIBLE(op, who))
					{
						/* its a tool, need to unlink it */
						unlink_skill(op);
						new_draw_info_format (NDI_UNIQUE, 0, who, "You stop using the %s.", query_name(op, NULL));
						new_draw_info_format (NDI_UNIQUE, 0, who, "You can no longer use the skill: %s.", skills[op->stats.sp].name);
					}
				}
				(void) change_abil (who, op);
				who->chosen_skill = NULL;
				CLEAR_FLAG (who, FLAG_READY_SKILL);
				buf[0] = '\0';
				break;

			case ARMOUR:
			case HELMET:
			case SHIELD:
			case RING:
			case BOOTS:
			case GLOVES:
			case AMULET:
			case GIRDLE:
			case BRACERS:
			case CLOAK:
				(void) change_abil (who, op);
				sprintf(buf, "You unwear %s.", query_name(op, NULL));
				break;
			case BOW:
			case WAND:
			case ROD:
			case HORN:
				sprintf(buf, "You unready %s.", query_name(op, NULL));
				if (who->type == PLAYER)
				{
					CONTR(who)->shoottype = range_none;
					CONTR(who)->last_value = -1;
				}
				else
				{
					switch (op->type)
					{
						case ROD:
						case HORN:
						case WAND:
							CLEAR_FLAG (who, FLAG_READY_RANGE);
							break;
						case BOW:
							CLEAR_FLAG (who, FLAG_READY_BOW);
							break;
					}
				}
				break;
			default:
				sprintf(buf, "You unapply %s.", query_name(op, NULL));
				break;
		}

		if (buf[0] != '\0' && who->type == PLAYER)
			new_draw_info(NDI_UNIQUE, 0, who, buf);

		fix_player(who);

		if (!(aflags & AP_NO_MERGE))
		{
			tag_t del_tag = op->count;
			object *cont = op->env;
			tmp = merge_ob(op, NULL);
			if (who->type == PLAYER)
			{
				/* it was merged */
				if (tmp)
				{
					esrv_del_item(CONTR(who), del_tag, cont);
					op = tmp;
				}
				esrv_send_item (who, op);
			}
		}

		return 0;
	}

	if (basic_flag == AP_UNAPPLY)
		return 0;

	i = 0;
	/* This goes through and checks to see if the player already has something
	 * of that type applied - if so, unapply it.
	 */
	if (op->type == WAND || op->type == ROD || op->type == HORN)
		tmp_flag = 1;

	for (tmp = who->inv; tmp != NULL; tmp = tmp->below)
	{
		if ((tmp->type == op->type || (tmp_flag && (tmp->type == WAND || tmp->type == ROD || tmp->type == HORN))) && QUERY_FLAG(tmp, FLAG_APPLIED) && tmp != op)
		{
			if (tmp->type == RING && !i)
				i = 1;
			else if (apply_special(who, tmp, 0))
				return 1;
		}
	}

	if (op->nrof > 1)
		tmp = get_split_ob(op, op->nrof - 1);
	else
		tmp = NULL;

	switch (op->type)
	{
		case WEAPON:
		{
			if (!QUERY_FLAG(who, FLAG_USE_WEAPON))
			{
				sprintf(buf, "You can't use %s.", query_name(op, NULL));
				if (tmp != NULL)
					(void) insert_ob_in_ob(tmp, who);
				new_draw_info(NDI_UNIQUE, 0, who, buf);
				return 1;
			}

			if (!check_weapon_power(who, op->last_eat))
			{
				new_draw_info(NDI_UNIQUE, 0, who, "That weapon is too powerful for you to use.");
				new_draw_info(NDI_UNIQUE, 0, who, "It would consume your soul!");

				if (tmp != NULL)
					(void) insert_ob_in_ob(tmp, who);
				return 1;
			}

			if (op->level && (strncmp(op->name, who->name, strlen(who->name))))
			{
				/* if the weapon does not have the name as the character, can't use it. */
				/* (Ragnarok's sword attempted to be used by Foo: won't work) */
				new_draw_info(NDI_UNIQUE, 0, who, "The weapon does not recognize you as its owner.");
				if (tmp != NULL)
					(void) insert_ob_in_ob(tmp, who);
				return 1;
			}

			/* if we have applied a shield, don't allow apply of polearm or 2hand weapon */
			if ((op->sub_type1 >= WEAP_POLE_IMPACT || op->sub_type1 >= WEAP_2H_IMPACT) && who->type == PLAYER && CONTR(who) && CONTR(who)->equipment[PLAYER_EQUIP_SHIELD])
			{
				new_draw_info(NDI_UNIQUE, 0, who, "You can't wield this weapon and a shield.");
				if (tmp != NULL)
					(void) insert_ob_in_ob(tmp, who);
				return 1;
			}

			if (!check_skill_to_apply(who, op))
			{
				if (tmp != NULL)
					(void) insert_ob_in_ob(tmp,who);
				return 1;
			}

			SET_FLAG(op, FLAG_APPLIED);
			SET_FLAG(who, FLAG_READY_WEAPON);
			(void) change_abil(who, op);
			sprintf(buf, "You wield %s.", query_name(op, NULL));
			break;
		}
		case SHIELD:
			/* don't allow of polearm or 2hand weapon with a shield */
			if ((who->type == PLAYER && CONTR(who) && CONTR(who)->equipment[PLAYER_EQUIP_WEAPON1]) && (CONTR(who)->equipment[PLAYER_EQUIP_WEAPON1]->sub_type1 >= WEAP_POLE_IMPACT || CONTR(who)->equipment[PLAYER_EQUIP_WEAPON1]->sub_type1 >= WEAP_2H_IMPACT))
			{
				new_draw_info(NDI_UNIQUE, 0, who, "You can't wield this weapon and a shield.");
				if (tmp != NULL)
					(void) insert_ob_in_ob(tmp, who);
				return 1;
			}

		case ARMOUR:
		case HELMET:
		case BOOTS:
		case GLOVES:
		case GIRDLE:
		case BRACERS:
		case CLOAK:
			if (!QUERY_FLAG(who, FLAG_USE_ARMOUR))
			{
				sprintf(buf, "You can't use %s.", query_name(op, NULL));
				new_draw_info(NDI_UNIQUE, 0, who, buf);
				if (tmp != NULL)
					(void) insert_ob_in_ob(tmp, who);
				return 1;
			}
		case RING:
		case AMULET:
			SET_FLAG(op, FLAG_APPLIED);
			(void) change_abil (who, op);
			sprintf(buf, "You wear %s.", query_name(op, NULL));
			break;

			/* this part is needed for skill-tools */
		case SKILL:
			if (who->chosen_skill)
			{
				LOG(llevBug, "BUG: apply_special(): can't apply two skills\n");
				return 1;
			}

			if (who->type == PLAYER)
			{
				CONTR(who)->shoottype = range_skill;
				if (!IS_INVISIBLE(op, who))
				{
					/* for tools */
					if (op->exp_obj)
						LOG(llevBug, "BUG: apply_special(SKILL): found unapplied tool with experience object\n");
					else
						(void) link_player_skill (who, op);

					new_draw_info_format(NDI_UNIQUE, 0, who, "You ready %s.", query_name(op, NULL));
					new_draw_info_format(NDI_UNIQUE, 0, who, "You can now use the skill: %s.", skills[op->stats.sp].name);
				}
				else
					send_ready_skill(who, skills[op->stats.sp].name);
			}
			SET_FLAG(op, FLAG_APPLIED);
			(void) change_abil(who, op);
			who->chosen_skill = op;
			SET_FLAG (who, FLAG_READY_SKILL);
			buf[0] = '\0';
			break;

		case WAND:
		case ROD:
		case HORN:
		case BOW:
			if (!check_skill_to_apply(who, op))
				return 1;

			SET_FLAG(op, FLAG_APPLIED);
			new_draw_info_format(NDI_UNIQUE, 0, who, "You ready %s.", query_name(op, NULL));
			if (who->type == PLAYER)
			{
				if (op->type == BOW)
					new_draw_info_format (NDI_UNIQUE, 0, who, "You will now fire %s with %s.", op->race ? op->race : "nothing", query_name(op, NULL));
				else
				{
					CONTR(who)->chosen_item_spell = op->stats.sp;
					CONTR(who)->known_spell = (QUERY_FLAG (op, FLAG_BEEN_APPLIED) || QUERY_FLAG (op, FLAG_IDENTIFIED));
				}
			}
			else
			{
				switch (op->type)
				{
					case ROD:
					case HORN:
					case WAND:
						SET_FLAG (who, FLAG_READY_RANGE);
						break;
					case BOW:
						SET_FLAG (who, FLAG_READY_BOW);
						break;
				}
			}
			break;

		default:
			sprintf(buf, "You apply %s.", query_name(op, NULL));
	}

	if (!QUERY_FLAG(op, FLAG_APPLIED))
		SET_FLAG(op, FLAG_APPLIED);

	if (buf[0] != '\0')
		new_draw_info(NDI_UNIQUE, 0, who, buf);

	if (tmp != NULL)
		tmp = insert_ob_in_ob(tmp, who);

	fix_player(who);

	if (op->type != WAND && who->type == PLAYER)
		SET_FLAG(op, FLAG_BEEN_APPLIED);

	if (QUERY_FLAG(op, FLAG_PERM_CURSED))
		SET_FLAG(op, FLAG_CURSED);
	if (QUERY_FLAG(op, FLAG_PERM_DAMNED))
		SET_FLAG(op, FLAG_DAMNED);

	if (QUERY_FLAG(op, FLAG_CURSED) || QUERY_FLAG(op, FLAG_DAMNED))
	{
		if (who->type == PLAYER)
		{
			new_draw_info(NDI_UNIQUE, 0, who, "Oops, it feels deadly cold!");
			SET_FLAG(op, FLAG_KNOWN_CURSED);
		}
	}

	if (who->type==PLAYER)
	{
		/* if multiple objects were applied, update both slots */
		if (tmp)
			esrv_send_item(who, tmp);

		esrv_send_item(who, op);
	}

	return 0;
}

int monster_apply_special(object *who, object *op, int aflags)
{
	if (QUERY_FLAG(op, FLAG_UNPAID) && !QUERY_FLAG(op, FLAG_APPLIED))
		return 1;

	return apply_special(who, op, aflags);
}

/* apply_player_light_refill() - refill lamps and all refill type light sources
 * from apply_player_light().
 * The light source must be in the inventory of the player, then he must mark the
 * light source and apply the refill item (lamp oil for example).
 */
void apply_player_light_refill(object *who, object *op)
{
	object *item;
	int tmp;

	item = find_marked_object(who);
	if (!item)
	{
		new_draw_info_format(NDI_UNIQUE, 0, who, "Mark a light source first you want refill.");
		return;
	}

	if (item->type != TYPE_LIGHT_APPLY || !item->race || strstr(item->race, op->race))
	{
		new_draw_info_format(NDI_UNIQUE, 0, who, "You can't refill the %s with the %s.", query_name(item, NULL), query_name(op, NULL));
		return;
	}

	/* ok, all is legal - now we refill the light source = settings item->food
	 * = op-food. Then delete op or if its a stack, decrease nrof.
	 * no idea about unidentified or cursed/damned effects for both items.
	 */

	tmp = (int) item->stats.maxhp - item->stats.food;
	if (!tmp)
	{
		new_draw_info_format(NDI_UNIQUE, 0, who, "The %s is full and can't be refilled.", query_name(item, NULL));
		return;
	}

	if (op->stats.food <= tmp)
	{
		item->stats.food += op->stats.food;
		new_draw_info_format(NDI_UNIQUE, 0, who, "You refill the %s with %d units %s.", query_name(item, NULL), op->stats.food, query_name(op, NULL));
		decrease_ob(op);
	}
	else
	{
		item->stats.food += tmp;
		op->stats.food -= tmp;
		new_draw_info_format(NDI_UNIQUE, 0, who, "You refill the %s with %d units %s.", query_name(item, NULL), tmp, query_name(op, NULL));
		esrv_send_item(who, op);
	}

	esrv_send_item(who, item);
	fix_player(who);
}

/* apply_player_light() - the new player light. old style torches will be
 * removed from arches but still in game. */
void apply_player_light(object *who, object *op)
{
	object *tmp;

	if (QUERY_FLAG(op, FLAG_APPLIED))
	{
		if ((QUERY_FLAG(op, FLAG_CURSED) || QUERY_FLAG(op, FLAG_DAMNED)))
		{
			new_draw_info_format(NDI_UNIQUE, 0, who, "No matter how hard you try, you just can't remove it!");
			return;
		}

		if (QUERY_FLAG(op, FLAG_PERM_CURSED))
			SET_FLAG(op, FLAG_CURSED);

		if (QUERY_FLAG(op, FLAG_PERM_DAMNED))
			SET_FLAG(op, FLAG_DAMNED);

		new_draw_info_format(NDI_UNIQUE, 0, who, "You unlight the %s.", query_name(op, NULL));

		if (!op->env && op->glow_radius)
			adjust_light_source(op->map, op->x, op->y, -(op->glow_radius));

		op->glow_radius = 0;
		CLEAR_FLAG(op, FLAG_APPLIED);
		CLEAR_FLAG(op, FLAG_CHANGING);

		if (op->other_arch && op->other_arch->clone.sub_type1 & 1)
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
		 * with the second, we apply it. if we unapply a light source, we always unlight
		 * them implicit.
		 */

		/* TYPE_LIGHT_APPLY light sources with last_sp (aka glow_radius) 0 are useless -
		 * for example burnt out torches. The burnt out lights are still from same type
		 * because they are perhaps applied from the player as they burnt out
		 * and we don't want a player applying a illegal item.
		 */
		if (!op->last_sp)
		{
			new_draw_info_format(NDI_UNIQUE, 0, who, "The %s can't be light.", query_name(op, NULL));
			return;
		}


		/* if glow_radius == 0, we have a unlight light source.
		 * before we can put it in the hand to use it, we have to turn
		 * the light on.
		 */
		if (!op->glow_radius)
		{
			object *op_old;
			int tricky_flag= FALSE; /* to delay insertion of object - or it simple remerge! */

			if (op->last_eat) /* we have a non permanent source */
			{
				if (!op->stats.food) /* if not permanent, this is "filled" counter */
				{
					/* no food charges, we can't light it up-
					 * Note that light sources with other_arch set
					 * are non rechargable lights - like torches.
					 * they destroy
					 */
					new_draw_info_format(NDI_UNIQUE, 0, who, "You must first refill or recharge the %s.", query_name(op, NULL));
					return;
				}
			}

			/* now we have a filled or permanent, unlight light source
			 * lets light it - BUT we still have light_radius not active
			 * when we not drop or apply the source.
			 */

			/* the old split code has some side effects -
			 * i force now first a split of #1 per hand
			 */
			op_old = op;

			if (op->nrof > 1)
			{
				object *one = get_object();
				copy_object(op, one);
				op->nrof -= 1;
				one->nrof = 1;
				if (op->env && (op->env->type == PLAYER || op->env->type == CONTAINER))
					esrv_update_item(UPD_NROF, op->env, op);
				else if (!op->env)
					update_object(op, UP_OBJ_FACE);

				tricky_flag = TRUE;
				op = one;
			}

			/* light is applied in player inventory - so we
			 * start the 3 apply chain - because it can be taken
			 * in hand.
			 */
			if (op_old->env && op_old->env->type == PLAYER)
			{
				new_draw_info_format(NDI_UNIQUE, 0, who, "You prepare %s to light.", query_name(op, NULL));

				/* we have a non permanent source */
				if (op->last_eat)
					SET_FLAG(op, FLAG_CHANGING);

				if (op->speed)
				{
					SET_FLAG(op, FLAG_ANIMATE);
					op->animation_id = op->arch->clone.animation_id; /* be sure to get the right anim */
					SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction);
				}

				if (tricky_flag)
					op = insert_ob_in_ob(op, op_old->env);

				op->glow_radius = (sint8) op->last_sp;
				fix_player(who);
			}
			else /* we are not in a player inventory - so simple turn it on */
			{
				new_draw_info_format(NDI_UNIQUE, 0, who, "You prepare %s to light.", query_name(op, NULL));
				/* we have a non permanent source */
				if (op->last_eat)
					SET_FLAG(op, FLAG_CHANGING);

				if (op->speed)
				{
					SET_FLAG(op, FLAG_ANIMATE);
					op->animation_id = op->arch->clone.animation_id;
					SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction);
				}

				if (QUERY_FLAG(op, FLAG_PERM_CURSED))
					SET_FLAG(op, FLAG_CURSED);
				if (QUERY_FLAG(op, FLAG_PERM_DAMNED))
					SET_FLAG(op, FLAG_DAMNED);

				if (tricky_flag)
				{
					if (!op_old->env)
						/* the item WAS before this on this spot - we only turn it on but we don't moved it */
						insert_ob_in_map(op, op_old->map, op_old, INS_NO_WALK_ON);
					else
						op = insert_ob_in_ob(op, op_old->env);
				}

				op->glow_radius = (sint8) op->last_sp;
				if (!op->env && op->glow_radius)
					adjust_light_source(op->map, op->x, op->y, op->glow_radius);

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
							new_draw_info(NDI_UNIQUE, 0, who, "No matter how hard you try, you just can't remove it!");
							return;
						}

						if (QUERY_FLAG(tmp, FLAG_PERM_CURSED))
							SET_FLAG(tmp, FLAG_CURSED);
						if (QUERY_FLAG(tmp, FLAG_PERM_DAMNED))
							SET_FLAG(tmp, FLAG_DAMNED);

						new_draw_info_format(NDI_UNIQUE, 0, who, "You unlight the %s.", query_name(tmp, NULL));

						/* on map */
						if (!tmp->env && tmp->glow_radius)
							adjust_light_source(tmp->map, tmp->x, tmp->y, -(tmp->glow_radius));

						tmp->glow_radius = 0;
						CLEAR_FLAG(tmp, FLAG_APPLIED);
						CLEAR_FLAG(tmp, FLAG_CHANGING);
						if (op->other_arch && op->other_arch->clone.sub_type1 & 1)
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
						esrv_send_item(who, tmp);
					}
				}

				new_draw_info_format(NDI_UNIQUE, 0, who, "You apply %s as light.", query_name(op, NULL));
				SET_FLAG(op, FLAG_APPLIED);
				fix_player(who);
				update_object(who, UP_OBJ_FACE);
			}
			else /* not part of player inv - turn light off ! */
			{
				if (QUERY_FLAG(op, FLAG_PERM_CURSED))
					SET_FLAG(op, FLAG_CURSED);
				if (QUERY_FLAG(op, FLAG_PERM_DAMNED))
					SET_FLAG(op, FLAG_DAMNED);

				new_draw_info_format(NDI_UNIQUE, 0, who, "You unlight the %s.", query_name(op, NULL));
				/* on map */
				if (!op->env && op->glow_radius)
					adjust_light_source(op->map, op->x, op->y, -(op->glow_radius));

				op->glow_radius = 0;
				CLEAR_FLAG(op, FLAG_APPLIED);
				CLEAR_FLAG(op, FLAG_CHANGING);
				if (op->other_arch && op->other_arch->clone.sub_type1 & 1)
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

	if (op->env && (op->env->type == PLAYER || op->env->type == CONTAINER))
		esrv_send_item(op->env, op);
}


/* apply_lighter() - designed primarily to light torches/lanterns/etc.
 * Also burns up burnable material too. First object in the inventory is
 * the selected object to "burn". -b.t.
 */
/* i have this item type not include in daimonin atm - MT-2004 */
void apply_lighter(object *who, object *lighter)
{
	object *item;
	tag_t count;
	uint32 nrof;
	int is_player_env = 0;
	char item_name[MAX_BUF];

	item = find_marked_object(who);
	if (item)
	{
		/* lighter gets used up */
		if (lighter->last_eat && lighter->stats.food)
		{
			/* Split multiple lighters if they're being used up.  Otherwise	*
			   * one charge from each would be used up.  --DAMN		*/
			if (lighter->nrof > 1)
			{
				object *oneLighter = get_object();
				copy_object(lighter, oneLighter);
				lighter->nrof -= 1;
				oneLighter->nrof = 1;
				oneLighter->stats.food--;
				esrv_send_item(who, lighter);
				oneLighter=insert_ob_in_ob(oneLighter, who);
				esrv_send_item(who, oneLighter);
			}
			else
				lighter->stats.food--;
		}
		/* no charges left in lighter */
		else if (lighter->last_eat)
		{
			new_draw_info_format(NDI_UNIQUE, 0, who, "You attempt to light the %s with a used up %s.", item->name, lighter->name);
			return;
		}
		/* Perhaps we should split what we are trying to light on fire?
		 * I can't see many times when you would want to light multiple
		 * objects at once. */
		nrof = item->nrof;
		count = item->count;
		/* If the item is destroyed, we don't have a valid pointer to the
		 * name object, so make a copy so the message we print out makes
		 * some sense. */
		strcpy(item_name, item->name);
		if (who == is_player_inv(item))
			is_player_env = 1;

		save_throw_object(item, who);
		/* Change to check count and not freed, since the object pointer
		 * may have gotten recycled */
		if ((nrof != item->nrof ) || (count != item->count))
		{
			new_draw_info_format(NDI_UNIQUE, 0, who, "You light the %s with the %s.", item_name, lighter->name);
			if (is_player_env)
				fix_player(who);
		}
		else
			new_draw_info_format(NDI_UNIQUE, 0, who, "You attempt to light the %s with the %s and fail.", item->name, lighter->name);

	}
	/* nothing to light */
	else
		new_draw_info(NDI_UNIQUE, 0, who, "You need to mark a lightable object.");
}

/* scroll_failure()- hacked directly from spell_failure */
void scroll_failure(object *op, int failure, int power)
{
	/* set minimum effect */
	if (abs(failure / 4) > power)
		power = abs(failure / 4);

	/* wonder */
	if (failure<= -1 && failure > -15)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Your spell warps!");
#if 0
		cast_cone(op, op, 0, 10, SP_WOW, spellarch[SP_WOW], 0);
#endif
	}
	else if (failure <= -15&&failure > -35) /* drain mana */
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Your mana is drained!");
		op->stats.sp -= random_roll(0, power-1, op, PREFER_LOW);
		if (op->stats.sp < 0)
			op->stats.sp = 0;
	}

	/* even nastier effects continue...*/
#ifdef SPELL_FAILURE_EFFECTS
	/* removed this - but perhaps we want add some of this nasty effects */
	/* confusion */
	else if (failure <= -35 && failure > -60)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "The magic recoils on you!");
		confuse_living(op, op, power);
	}
	/* paralysis */
	else if (failure <= -60 && failure> -70)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "The magic recoils and paralyzes you!");
		paralyze_living(op, op, power);
	}
	/* blind */
	else if (failure <= -70 && failure> -80)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "The magic recoils on you!");
		blind_living(op, op, power);
	}
	/* blast the immediate area */
	else if (failure <= -80)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You unlease uncontrolled mana!");
		cast_mana_storm(op, power);
	}
#endif
}
