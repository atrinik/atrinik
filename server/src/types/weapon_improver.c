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
 * Handles the code for using @ref WEAPON_IMPROVER "weapon improvers".
 * @todo Test how the code works and perhaps make use of it for
 * Atrinik. */

#include <global.h>
#include <sproto.h>

/**
 * @defgroup weapon_improve_types Weapon Improvement Types
 * Types of improvements, hidden in the sp field.
 *@{*/

/** Prepare the weapon. */
#define IMPROVE_PREPARE 1
/** Increase damage. */
#define IMPROVE_DAMAGE 2
/** Decrease weight. */
#define IMPROVE_WEIGHT 3
/** Increase magic. */
#define IMPROVE_ENCHANT 4
/** Increase strength bonus. */
#define IMPROVE_STR 5
/** Increase dexterity bonus. */
#define IMPROVE_DEX 6
/** Increase constitution bonus. */
#define IMPROVE_CON 7
/** Increase wisdom bonus. */
#define IMPROVE_WIS 8
/** Increase charisma bonus. */
#define IMPROVE_CHA 9
/** Increase intelligence bonus. */
#define IMPROVE_INT 10
/** Increase power bonus. */
#define IMPROVE_POW 11
/*@}*/

static int improve_weapon_stat(object *op, object *improver, object *weapon, signed char *stat, int sacrifice_count, char *statname);
static int prepare_weapon(object *op, object *improver, object *weapon);
static int improve_weapon(object *op, object *improver, object *weapon);
static void eat_item(object *op, const char *item);
static int check_item(object *op, const char *item);
static int check_sacrifice(object *op, object *improver);

/**
 * Apply a @ref WEAPON_IMPROVER "weapon improver".
 * @param op The player object applying the improver.
 * @param tmp The weapon improver scroll. */
void apply_weapon_improver(object *op, object *tmp)
{
	object *otmp;

	if (op->type != PLAYER)
	{
		return;
	}

	if (blocks_magic(op->map, op->x, op->y))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Something blocks the magic of the scroll.");
		return;
	}

	otmp = find_marked_object(op);

	if (!otmp || otmp->type != WEAPON)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You need to mark a weapon object.");
		return;
	}

	new_draw_info(NDI_UNIQUE, 0, op, "Applied weapon builder.");
	improve_weapon(op, tmp, otmp);
	esrv_send_item(op, otmp);
}

/**
 * This checks to see of the player (who) is sufficient level to use a
 * weapon with improvs improvements (typically last_eat).
 *
 * We take an int here instead of the object so that the improvement code
 * can pass along the increased value to see if the object is usuable.
 * we return 1 (true) if the player can use the weapon.
 * @param who The player object.
 * @param improvs The improvement value.
 * @return 1 if the player can use the weapon, 0 otherwise. */
int check_weapon_power(object *who, int improvs)
{
	int level = who->level;

	/* The skill system hands out wc and dam bonuses to fighters
	 * more generously than the old system (see fix_player). Thus
	 * we need to curtail the power of player enchanted weapons.
	 * I changed this to 1 improvement per "fighter" level/5 -b.t.
	 * Note:  Nothing should break by allowing this ratio to be different or
	 * using normal level - it is just a matter of play balance. */
	if (who->type == PLAYER)
	{
		object *wc_obj = NULL;

		for (wc_obj = who->inv; wc_obj; wc_obj = wc_obj->below)
		{
			if (wc_obj->type == EXPERIENCE && wc_obj->stats.Str)
			{
				break;
			}
		}

		if (!wc_obj)
		{
			LOG(llevBug, "BUG: Player: %s lacks wc experience object.\n", who->name);
		}
		else
		{
			level = wc_obj->level;
		}
	}

	return (improvs <= ((level / 5) + 5));
}

/**
 * Actually improves the weapon, and tells the player.
 * @param op Player improving.
 * @param improver Scroll used to improve.
 * @param weapon Improved weapon.
 * @param stat What statistic to improve.
 * @param sacrifice_count How much to improve stat by.
 * @param statname Name of stat to display to player.
 * @return Always returns 1. */
static int improve_weapon_stat(object *op, object *improver, object *weapon, signed char *stat, int sacrifice_count, char *statname)
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

/**
 * This does the prepare weapon scroll.
 *
 * Checks for sacrifice, and so on. Will inform the player of failures or
 * success.
 * @param op Player using the scroll.
 * @param improver The improvement scroll.
 * @param weapon Weapon to improve.
 * @return 1 if weapon was prepared, 0 otherwise. */
static int prepare_weapon(object *op, object *improver, object *weapon)
{
	int sacrifice_count, i;
	char buf[MAX_BUF];

	if (weapon->level != 0)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Weapon already prepared.");
		return 0;
	}

	for (i = 0; i < NROFATTACKS; i++)
	{
		if (weapon->resist[i])
		{
			break;
		}
	}

	/* If we break out, i will be less than nrofattacks, preventing
	 * improvement of items that already have protections. */
	if (i < NROFATTACKS || weapon->stats.hp || weapon->stats.sp || weapon->stats.exp || weapon->stats.ac)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Cannot prepare magic weapons.");
		return 0;
	}

	sacrifice_count = check_sacrifice(op, improver);

	if (sacrifice_count <= 0)
	{
		return 0;
	}

	sacrifice_count = isqrt(sacrifice_count);
	weapon->level = sacrifice_count;
	new_draw_info(NDI_UNIQUE, 0, op, "Your sacrifice was accepted.");
	eat_item(op, improver->slaying);
	new_draw_info_format(NDI_UNIQUE, 0, op, "Your *%s may be improved %d times.", weapon->name, sacrifice_count);
	snprintf(buf, sizeof(buf), "%s's %s", op->name, weapon->name);
	FREE_AND_COPY_HASH(weapon->name, buf);

	/* prevents preparing n weapons in the same slot at once! */
	weapon->nrof = 0;
	decrease_ob(improver);
	weapon->last_eat = 0;
	return 1;
}

/**
 * Does the dirty job for 'improve weapon' scroll, prepare or add
 * something.
 *
 * Checks if weapon was prepared, if enough potions on the floor, etc.
 *
 * We are hiding extra information about the weapon in the level and
 * last_eat numbers for an object. Hopefully this won't break anything?
 *
 * level == max improve, last_eat == current improve
 * @param op Player improving.
 * @param improver The scroll that was read.
 * @param weapon Weapon to improve.
 * @return 1 if weapon was improved, 0 otherwise. */
static int improve_weapon(object *op, object *improver, object *weapon)
{
	int sacrifice_count, sacrifice_needed = 0;

	if (improver->stats.sp == IMPROVE_PREPARE)
	{
		return prepare_weapon(op, improver, weapon);
	}

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
		new_draw_info(NDI_UNIQUE, 0, op, "Improving the weapon will make it too powerful for you to use.\nUnready it if you really want to improve it.");
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
		{
			weapon->weight = 1;
		}

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
	{
		sacrifice_needed = 1;
	}

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
			return improve_weapon_stat(op, improver, weapon, (signed char *) &(weapon->stats.Str), 1, "strength");

		case IMPROVE_DEX:
			return improve_weapon_stat(op, improver, weapon, (signed char *) &(weapon->stats.Dex), 1, "dexterity");

		case IMPROVE_CON:
			return improve_weapon_stat(op, improver, weapon, (signed char *) &(weapon->stats.Con), 1, "constitution");

		case IMPROVE_WIS:
			return improve_weapon_stat(op, improver, weapon, (signed char *) &(weapon->stats.Wis), 1, "wisdom");

		case IMPROVE_CHA:
			return improve_weapon_stat(op, improver, weapon, (signed char *) &(weapon->stats.Cha), 1, "charisma");

		case IMPROVE_INT:
			return improve_weapon_stat(op, improver, weapon, (signed char *) &(weapon->stats.Int), 1, "intelligence");

		case IMPROVE_POW:
			return improve_weapon_stat(op, improver, weapon, (signed char *) &(weapon->stats.Pow), 1, "power");

		default:
			new_draw_info(NDI_UNIQUE, 0, op, "Unknown improvement type.");
	}

	LOG(llevBug, "BUG: improve_weapon(): Got to end of function!\n");

	return 0;
}

/**
 * Removes specified item.
 * @param op Item at the bottom to check.
 * @param item Archetype to look for. */
static void eat_item(object *op, const char *item)
{
	object *prev = op;

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

/**
 * Counts suitable items with specified archetype name. Will not consider
 * unpaid/cursed items.
 * @param op Object just before the bottom of the pile, others will be
 * checked through object->below.
 * @param item What archetype to check for.
 * @return Count of matching items. */
static int check_item(object *op, const char *item)
{
	int count = 0;

	if (item == NULL)
	{
		return 0;
	}

	op = op->below;

	while (op != NULL)
	{
		if (strcmp(op->arch->name, item) == 0)
		{
			if (!QUERY_FLAG(op, FLAG_CURSED) && !QUERY_FLAG(op, FLAG_DAMNED) && !QUERY_FLAG(op, FLAG_UNPAID))
			{
				/* this is necessary for artifact sacrifices --FD-- */
				if (op->nrof == 0)
				{
					count++;
				}
				else
				{
					count += op->nrof;
				}
			}
		}

		op = op->below;
	}

	return count;
}

/**
 * Returns how many items of type improver->slaying there are under op.
 *
 * Will display a message if none found.
 * @param op Item just below the bottom of the pile.
 * @param improver Sacrifice object.
 * @return Count of matching items. */
static int check_sacrifice(object *op, object *improver)
{
	int count = 0;

	if (improver->slaying != NULL)
	{
		count = check_item(op, improver->slaying);

		if (count < 1)
		{
			new_draw_info_format(NDI_UNIQUE, 0, op, "The gods want more %ss.", improver->slaying);
			return 0;
		}
	}
	else
	{
		count = 1;
	}

	return count;
}
