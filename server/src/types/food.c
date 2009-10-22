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
 * Handles code used for @ref FOOD "food", @ref DRINK "drinks" and
 * @ref FLESH "flesh". */

#include <global.h>
#include <sproto.h>
#include <math.h>

/** Maximum allowed food value. */
#define FOOD_MAX 999

/**
 * Apply a food/drink/flesh object.
 * @param op The object applying this.
 * @param tmp The object to apply. */
void apply_food(object *op, object *tmp)
{
	if (op->type != PLAYER)
	{
		op->stats.hp = op->stats.maxhp;
	}
	else
	{
		char buf[MAX_BUF];

		/* check if this is a dragon (player), eating some flesh */
		if (tmp->type == FLESH && is_dragon_pl(op) && dragon_eat_flesh(op, tmp))
		{
		}
		else
		{
			if (op->stats.food + tmp->stats.food > FOOD_MAX)
			{
				if ((op->stats.food + tmp->stats.food) - FOOD_MAX > tmp->stats.food / 5)
				{
					new_draw_info(NDI_UNIQUE, 0, op, "You are too full to eat this right now!");
					return;
				}

				if (tmp->type == FOOD || tmp->type == FLESH)
				{
					new_draw_info(NDI_UNIQUE, 0, op, "You feel full, but what a waste of food!");
				}
				else
				{
					new_draw_info(NDI_UNIQUE, 0, op, "Most of the drink goes down your face not your throat!");
				}
			}

			if (!QUERY_FLAG(tmp, FLAG_CURSED) && !QUERY_FLAG(tmp, FLAG_DAMNED))
			{
				if (!is_dragon_pl(op))
				{
					/* eating message for normal players */
					if (tmp->type == DRINK)
					{
						snprintf(buf, sizeof(buf), "Ahhh...that %s tasted good.", tmp->name);
					}
					/* tasted "good" if food - tasted "terrible" for flesh for all non dragon players */
					else
					{
						snprintf(buf, sizeof(buf), "The %s tasted %s", tmp->name, tmp->type == FLESH ? "terrible!" : "good.");
					}
				}
				/* eating message for dragon players - they like it bloody */
				else
				{
					snprintf(buf, sizeof(buf), "The %s tasted terrible!", tmp->name);
				}

				op->stats.food += tmp->stats.food;
			}
			/* cursed/damned = food is decreased instead of increased */
			else
			{
				int ft = tmp->stats.food;

				snprintf(buf, sizeof(buf), "The %s tasted terrible!", tmp->name);

				if (ft > 0)
				{
					ft =- ft;
				}

				op->stats.food += ft;
			}

			new_draw_info(NDI_UNIQUE, 0, op, buf);

			/* adjust food to borders when needed */
			if (op->stats.food > FOOD_MAX)
			{
				op->stats.food = FOOD_MAX;
			}
			else if (op->stats.food < 0)
			{
				op->stats.food = 0;
			}

			/* special food hack -b.t. */
			if (tmp->title || QUERY_FLAG(tmp, FLAG_CURSED)|| QUERY_FLAG(tmp, FLAG_DAMNED))
			{
				eat_special_food(op, tmp);
			}
		}
	}

	decrease_ob(tmp);
}

/**
 * Create a food force to include buff/debuff effects of stats and
 * resists to the player.
 * @param who The player object.
 * @param food The food.
 * @param force The force object. */
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
	{
		force->resist[i] = food->resist[i];
	}

	/* if damned, set all negative if not and double or triple them */
	if (QUERY_FLAG(food, FLAG_CURSED) || QUERY_FLAG(food, FLAG_DAMNED))
	{
		int stat_multiplier = QUERY_FLAG(food, FLAG_CURSED) ? 2 : 3;

		if (force->stats.Str > 0)
		{
			force->stats.Str =- force->stats.Str;
		}

		force->stats.Str *= stat_multiplier;

		if (force->stats.Dex > 0)
		{
			force->stats.Dex =- force->stats.Dex;
		}

		force->stats.Dex *= stat_multiplier;

		if (force->stats.Con > 0)
		{
			force->stats.Con =- force->stats.Con;
		}

		force->stats.Con *= stat_multiplier;

		if (force->stats.Int > 0)
		{
			force->stats.Int =- force->stats.Int;
		}

		force->stats.Int *= stat_multiplier;

		if (force->stats.Wis > 0)
		{
			force->stats.Wis =- force->stats.Wis;
		}

		force->stats.Wis *= stat_multiplier;

		if (force->stats.Pow > 0)
		{
			force->stats.Pow =- force->stats.Pow;
		}

		force->stats.Pow *= stat_multiplier;

		if (force->stats.Cha > 0)
		{
			force->stats.Cha =- force->stats.Cha;
		}

		force->stats.Cha *= stat_multiplier;

		for (i = 0; i < NROFATTACKS; i++)
		{
			if (force->resist[i] > 0)
			{
				force->resist[i] =- force->resist[i];
			}

			force->resist[i] *= stat_multiplier;
		}
	}

	if (food->speed_left)
	{
		force->speed = food->speed_left;
	}

	SET_FLAG(force, FLAG_APPLIED);

	force = insert_ob_in_ob(force, who);
	/* Mostly to display any messages */
	change_abil(who, force);
	/* This takes care of some stuff that change_abil() */
	fix_player(who);
}

/**
 * The food gives specials, like +/- hp or sp, resistance and stats.
 *
 * Food can be good or bad (good effect or bad effect), and cursed or
 * not. If food is "good" (for example, Str +1 and Dex +1), then it puts
 * those effects as force in the player for some time.
 *
 * If good food is cursed, all positive values are turned to negative
 * values.
 *
 * If bad food (Str -1, Dex -1) is uncursed, it gives just those values.
 *
 * If bad food is cursed, all negative values are doubled.
 *
 * Food effects can stack. For really powerful food, a high food value
 * should be set, so the player can't eat a lot of such food, as his
 * stomach will be full.
 * @param who Object eating the food.
 * @param food The food object. */
void eat_special_food(object *who, object *food)
{
	/* if there is any stat or resist value - create force for the object! */
	if (food->stats.Pow ||food->stats.Str || food->stats.Dex || food->stats.Con || food->stats.Int || food->stats.Wis || food->stats.Cha)
	{
		create_food_force(who, food, get_archetype("force"));
	}
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
			{
				tmp = -tmp;
			}

			strcpy(CONTR(who)->killer, food->name);

			if (QUERY_FLAG(food, FLAG_CURSED))
			{
				who->stats.hp += tmp * 2;
			}
			else
			{
				who->stats.hp += tmp * 3;
			}

			new_draw_info(NDI_UNIQUE, 0, who, "Eck!... that was rotten food!");
		}
		else
		{
			if (food->stats.hp > 0)
			{
				new_draw_info(NDI_UNIQUE, 0, who, "You begin to feel better.");
			}
			else
			{
				new_draw_info(NDI_UNIQUE, 0, who, "Eck!... that was rotten food!");
			}

			who->stats.hp += food->stats.hp;
		}
	}

	if (food->stats.sp != 0)
	{
		if (QUERY_FLAG(food, FLAG_CURSED) || QUERY_FLAG(food, FLAG_DAMNED))
		{
			int tmp = food->stats.sp;

			if (tmp > 0)
			{
				tmp = -tmp;
			}

			new_draw_info(NDI_UNIQUE, 0, who, "Your mana is drained!");

			if (QUERY_FLAG(food, FLAG_CURSED))
			{
				who->stats.sp += tmp * 2;
			}
			else
			{
				who->stats.sp += tmp * 3;
			}

			if (who->stats.sp < 0)
			{
				who->stats.sp = 0;
			}
		}
		else
		{
			new_draw_info(NDI_UNIQUE, 0, who, "You feel a rush of magical energy!");
			who->stats.sp += food->stats.sp;
		}
	}
}

/**
 * A dragon is eating some flesh. If the flesh contains resistances,
 * there is a chance for the dragon's skin to get improved.
 * @param op the object (dragon player) eating the flesh
 * @param meal the flesh item, getting chewed in dragon's mouth
 * @return 1 if eating successful, 0 if it doesn't work */
int dragon_eat_flesh(object *op, object *meal)
{
	object *skin = NULL, *abil = NULL, *tmp = NULL;
	char buf[MAX_BUF];
	/* improvement-chance of one resistance type */
	double chance;
	/* highest chance of any type */
	double maxchance = 0;
	/* level bonus (improvement is easier at lowlevel) */
	double bonus = 0;
	/* monster bonus */
	double mbonus = 0;
	/* winning candidates for resistance improvement */
	int atnr_winner[NROFATTACKS];
	/* number of winners */
	int winners = 0;
	int i;

	/* let's make sure and doublecheck the parameters */
	if (meal->type != FLESH || !is_dragon_pl(op))
	{
		return 0;
	}

	/* now grab the 'dragon_skin'- and 'dragon_ability'-forces from the player's inventory */
	for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
	{
		if (tmp->type == FORCE)
		{
			if (strcmp(tmp->arch->name, "dragon_skin_force") == 0)
			{
				skin = tmp;
			}
			else if (strcmp(tmp->arch->name, "dragon_ability_force") == 0)
			{
				abil = tmp;
			}
		}
	}

	/* if either skin or ability are missing, this is an old player
	 * which is not to be considered a dragon -> bail out */
	if (skin == NULL || abil == NULL)
		return 0;

	/* now start by filling stomach and health, according to food-value */
	if ((FOOD_MAX - op->stats.food) < meal->stats.food)
	{
		op->stats.hp += (FOOD_MAX - op->stats.food) / 50;
	}
	else
	{
		op->stats.hp += meal->stats.food / 50;
	}

	if (op->stats.hp > op->stats.maxhp)
	{
		op->stats.hp = op->stats.maxhp;
	}

	op->stats.food = MIN(FOOD_MAX, op->stats.food + meal->stats.food);

	/* on to the interesting part: chances for adding resistance */
	for (i = 0; i < NROFATTACKS; i++)
	{
		/* got positive resistance, now calculate improvement chance (0-100) */
		if (meal->resist[i] > 0 && atnr_is_dragon_enabled(i))
		{
			/* this bonus makes resistance increase easier at lower levels */
			bonus = (MAXLEVEL - op->level) * 30. / ((double) MAXLEVEL);

			/* additional bonus for resistance of ability-focus */
			if (i == abil->stats.exp)
			{
				bonus += 5;
			}

			/* monster bonus increases with level, because high-level flesh is too rare */
			mbonus = op->level * 20. / ((double) MAXLEVEL);

			chance = (((double) MIN(op->level + bonus, meal->level + bonus + mbonus)) * 100. / ((double) MAXLEVEL)) - skin->resist[i];

			if (chance >= 0.)
			{
				chance += 1.;
			}
			else
			{
				chance = (chance < -12) ? 0. : 1. / pow(2., -chance);
			}

			/* chance is proportional to amount of resistance (max. 50) */
			chance *= ((double) (MIN(meal->resist[i], 50))) / 50.;

			/* doubled chance for resistance of ability-focus */
			if (i == abil->stats.exp)
			{
				chance = MIN(100., chance * 2.);
			}

			/* now make the throw and save all winners (Don't insert luck bonus here!) */
			if (RANDOM() % 10000 < (int) (chance * 100))
			{
				atnr_winner[winners] = i;
				winners++;
			}

			if (chance > maxchance)
			{
				maxchance = chance;
			}
		}
	}

	/* print message according to maxchance */
	if (maxchance > 50.)
	{
		snprintf(buf, sizeof(buf),  "Hmm! The %s tasted delicious!", meal->name);
	}
	else if (maxchance > 10.)
	{
		snprintf(buf, sizeof(buf),  "The %s tasted very good.", meal->name);
	}
	else if (maxchance > 1.)
	{
		snprintf(buf, sizeof(buf),  "The %s tasted good.", meal->name);
	}
	else if (maxchance > 0.0001)
	{
		snprintf(buf, sizeof(buf),  "The %s had a boring taste.", meal->name);
	}
	else if (meal->last_eat > 0 && atnr_is_dragon_enabled(meal->last_eat))
	{
		snprintf(buf, sizeof(buf),  "The %s tasted strange.", meal->name);
	}
	else
	{
		snprintf(buf, sizeof(buf), "The %s had no taste.", meal->name);
	}

	new_draw_info(NDI_UNIQUE, 0, op, buf);

	/* now choose a winner if we have any */
	i = -1;

	if (winners > 0)
	{
		i = atnr_winner[RANDOM() % winners];
	}

	if (i >= 0 && i < NROFATTACKS && skin->resist[i] < 95)
	{
		/* resistance increased! */
		skin->resist[i]++;
		fix_player(op);

		new_draw_info_format(NDI_UNIQUE | NDI_RED, 0, op, "Your skin is now more resistant to %s!", change_resist_msg[i]);
	}

	/* if this flesh contains a new ability focus, we mark it
	 * into the ability_force and it will take effect on next level */
	if (meal->last_eat > 0 && atnr_is_dragon_enabled(meal->last_eat) && meal->last_eat != abil->last_eat)
	{
		/* write: last_eat <new attnr focus> */
		abil->last_eat = meal->last_eat;

		if (meal->last_eat != abil->stats.exp)
		{
			new_draw_info_format(NDI_UNIQUE, 0, op, "Your metabolism prepares to focus on %s!", change_resist_msg[meal->last_eat]);
			new_draw_info_format(NDI_UNIQUE, 0, op, "The change will happen at level %d.", abil->level + 1);
		}
		else
		{
			new_draw_info_format(NDI_UNIQUE, 0, op, "Your metabolism will continue to focus on %s.", change_resist_msg[meal->last_eat]);
			abil->last_eat = 0;
		}
	}

	return 1;
}
