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
 * Functions related to attributes, weight, experience, which concern
 * only living things. */

#include <global.h>

/** When we carry more than this of our weight_limit, we get encumbered. */
#define ENCUMBRANCE_LIMIT 65.0f

/**
 * dam_bonus, thaco_bonus, weight limit all are based on strength. */
int dam_bonus[MAX_STAT + 1] =
{
	-5, -4, -4, -3, -3, -3, -2, -2, -2, -1, -1,
	0, 0, 0, 0, 0,
	1, 1, 2, 2, 3, 3, 3, 4, 4, 5, 6, 7, 8, 10, 12
};

/** THAC0 bonus - WC bonus */
int thaco_bonus[MAX_STAT + 1] =
{
	-5, -4, -4, -3, -3, -3, -2, -2, -2, -1, -1,
	0, 0, 0, 0, 0,
	1, 1, 2, 2, 3, 3, 3, 4, 4, 5, 5, 5, 6, 7, 8
};

/**
 * Constitution bonus. */
static float con_bonus[MAX_STAT + 1] =
{
	-0.8f, -0.6f, -0.5f, -0.4f, -0.35f, -0.3f, -0.25f, -0.2f, -0.15f, -0.11f, -0.07f,
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.1f, 0.15f, 0.2f, 0.25f, 0.3f, 0.35f, 0.4f, 0.45f, 0.5f, 0.55f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f
};

/**
 * Power bonus. */
static float pow_bonus[MAX_STAT + 1] =
{
	-0.8f, -0.6f, -0.5f, -0.4f, -0.35f, -0.3f, -0.25f, -0.2f, -0.15f, -0.11f, -0.07f,
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.1f, 0.2f, 0.3f, 0.24f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.4f, 1.6f, 1.8f, 2.0f
};

/**
 * Wisdom bonus. */
static float wis_bonus[MAX_STAT + 1] =
{
	-0.8f, -0.6f, -0.5f, -0.4f, -0.35f, -0.3f, -0.25f, -0.2f, -0.15f, -0.11f, -0.07f,
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.4f, 1.6f, 1.8f, 2.0f
};

/**
 * Charisma bonus.
 *
 * As a base value, got get in a shop 20% of the real value of an item.
 * With a charisma bonus, you can go up to 25% maximum or 5% minimum. For
 * buying, they are reversely used. You can buy for 95% at best, or 115%
 * at worst. */
float cha_bonus[MAX_STAT + 1] =
{
	-0.15f,
	-0.10f, -0.08f,-0.05f, -0.03f, -0.02f,
	-0.01f, -0.005f, -0.003f, 0.001f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.003f, 0.005f, 0.009f, 0.01f, 0.012f,
	0.014f, 0.016f, 0.019f, 0.021f, 0.023f,
	0.025f, 0.03f, 0.035f, 0.04f, 0.05f
};

/**
 * Speed bonus. Uses dexterity as its stat. */
float speed_bonus[MAX_STAT + 1] =
{
	-0.4f, -0.4f, -0.3f, -0.3f, -0.2f,
	-0.2f, -0.2f, -0.1f, -0.1f, -0.1f,
	-0.05f, 0.0, 0.0f, 0.0f, 0.025f, 0.05f,
	0.075f, 0.1f, 0.125f, 0.15f, 0.175f, 0.2f,
	0.225f, 0.25f, 0.275f, 0.3f,
	0.325f, 0.35f, 0.4f, 0.45f, 0.5f
};

/**
 * The absolute most a character can carry - a character can't pick stuff
 * up if it would put him above this limit.
 *
 * Value is in grams, so we don't need to do conversion later
 *
 * These limits are probably overly generous, but being there were no
 * values before, you need to start someplace. */
uint32 weight_limit[MAX_STAT + 1] =
{
	20000,
	25000,	30000,	35000,	40000,	50000,
	60000,	70000,	80000,	90000,	100000,
	110000,	120000,	130000,	140000,	150000,
	165000,	180000,	195000,	210000,	225000,
	240000,	255000,	270000,	285000,	300000,
	325000,	350000,	375000,	400000,	450000
};

/**
 * Probability to learn a spell or skill, based on intelligence or
 * wisdom. */
int learn_spell[MAX_STAT + 1] =
{
	0, 0, 0, 1, 2, 4, 8, 12, 16, 25, 36, 45, 55, 65, 70, 75, 80, 85, 90, 95, 100, 100, 100, 100, 100,
	100, 100, 100, 100, 100, 100
};

/**
 * Probability of messing up a divine spell. Based on wisdom. */
int cleric_chance[MAX_STAT + 1] =
{
	100, 100, 100, 100, 90,
	80, 70, 60, 50, 40,
	30, 20, 10, 9, 8,
	7, 6, 5, 4, 3,
	2, 1, 0, 0, 0,
	0, 0, 0, 0, 0,
	0
};

/**
 * Probability to avoid something. */
int savethrow[MAXLEVEL + 1] =
{
	18,
	18, 17, 16, 15, 14, 14, 13, 13, 12, 12, 12, 11, 11, 11, 11, 10, 10, 10, 10, 9,
	9, 9, 9, 9, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 7, 7, 6, 6, 6,
	6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

/** Message when a player is drained of a stat. */
static const char *const drain_msg[NUM_STATS] =
{
	"Oh no! You are weakened!",
	"You're feeling clumsy!",
	"You feel less healthy",
	"You suddenly begin to lose your memory!",
	"Your face gets distorted!",
	"Watch out, your mind is going!",
	"Your spirit feels drained!"
};

/** Message when a player has a stat restored. */
const char *const restore_msg[NUM_STATS] =
{
	"You feel your strength return.",
	"You feel your agility return.",
	"You feel your health return.",
	"You feel your wisdom return.",
	"You feel your charisma return.",
	"You feel your memory return.",
	"You feel your spirits return."
};

/** Message when a player increases a stat. */
static const char *const gain_msg[NUM_STATS] =
{
	"You feel stronger.",
	"You feel more agile.",
	"You feel healthy.",
	"You feel wiser.",
	"You seem to look better.",
	"You feel smarter.",
	"You feel more potent."
};

/** Message when a player decreases a stat. */
const char *const lose_msg[NUM_STATS] =
{
	"You feel weaker!",
	"You feel clumsy!",
	"You feel less healthy!",
	"You lose some of your memory!",
	"You look ugly!",
	"You feel stupid!",
	"You feel less potent!"
};

/** Names of stats. */
const char *const statname[NUM_STATS] =
{
	"strength",
	"dexterity",
	"constitution",
	"wisdom",
	"charisma",
	"intelligence",
	"power"
};

/** Short names of stats. */
const char *const short_stat_name[NUM_STATS] =
{
	"Str",
	"Dex",
	"Con",
	"Wis",
	"Cha",
	"Int",
	"Pow"
};

/**
 * Sets Str/Dex/con/Wis/Cha/Int/Pow in stats to value, depending on what
 * attr is (STR to POW).
 * @param stats Item to modify. Must not be NULL.
 * @param attr Atribute to change.
 * @param value New value. */
void set_attr_value(living *stats, int attr, sint8 value)
{
	switch (attr)
	{
		case STR:
			stats->Str = value;
			break;

		case DEX:
			stats->Dex = value;
			break;

		case CON:
			stats->Con = value;
			break;

		case WIS:
			stats->Wis = value;
			break;

		case POW:
			stats->Pow = value;
			break;

		case CHA:
			stats->Cha = value;
			break;

		case INTELLIGENCE:
			stats->Int = value;
			break;
	}
}

/**
 * Like set_attr_value(), but instead the value (which can be negative)
 * is added to the specified stat.*
 * @param stats Item to modify. Must not be NULL.
 * @param attr Attribute to change.
 * @param value Delta (can be positive). */
void change_attr_value(living *stats, int attr, sint8 value)
{
	if (value == 0)
	{
		return;
	}

	switch (attr)
	{
		case STR:
			stats->Str += value;
			break;

		case DEX:
			stats->Dex += value;
			break;

		case CON:
			stats->Con += value;
			break;

		case WIS:
			stats->Wis += value;
			break;

		case POW:
			stats->Pow += value;
			break;

		case CHA:
			stats->Cha += value;
			break;

		case INTELLIGENCE:
			stats->Int += value;
			break;

		default:
			LOG(llevBug, "BUG: Invalid attribute in change_attr_value: %d\n", attr);
	}
}

/**
 * Gets the value of a stat.
 * @param stats Item from which to get stat.
 * @param attr Attribute to get.
 * @return Specified attribute, 0 if not found.
 * @see set_attr_value(). */
sint8 get_attr_value(living *stats, int attr)
{
	switch (attr)
	{
		case STR:
			return stats->Str;

		case DEX:
			return stats->Dex;

		case CON:
			return stats->Con;

		case WIS:
			return stats->Wis;

		case CHA:
			return stats->Cha;

		case INTELLIGENCE:
			return stats->Int;

		case POW:
			return stats->Pow;
	}

	return 0;
}

/**
 * Ensures that all stats (str/dex/con/wis/cha/int) are within the
 * passed in range of MIN_STAT and MAX_STAT.
 * @param stats Attributes to check. */
void check_stat_bounds(living *stats)
{
	if (stats->Str > MAX_STAT)
	{
		stats->Str = MAX_STAT;
	}
	else if (stats->Str < MIN_STAT)
	{
		stats->Str = MIN_STAT;
	}

	if (stats->Dex > MAX_STAT)
	{
		stats->Dex = MAX_STAT;
	}
	else if (stats->Dex < MIN_STAT)
	{
		stats->Dex = MIN_STAT;
	}

	if (stats->Con > MAX_STAT)
	{
		stats->Con = MAX_STAT;
	}
	else if (stats->Con < MIN_STAT)
	{
		stats->Con = MIN_STAT;
	}

	if (stats->Int > MAX_STAT)
	{
		stats->Int = MAX_STAT;
	}
	else if (stats->Int < MIN_STAT)
	{
		stats->Int = MIN_STAT;
	}

	if (stats->Wis > MAX_STAT)
	{
		stats->Wis = MAX_STAT;
	}
	else if (stats->Wis < MIN_STAT)
	{
		stats->Wis = MIN_STAT;
	}

	if (stats->Pow > MAX_STAT)
	{
		stats->Pow = MAX_STAT;
	}
	else if (stats->Pow < MIN_STAT)
	{
		stats->Pow = MIN_STAT;
	}

	if (stats->Cha > MAX_STAT)
	{
		stats->Cha = MAX_STAT;
	}
	else if (stats->Cha < MIN_STAT)
	{
		stats->Cha = MIN_STAT;
	}
}

/**
 * Permanently alters an object's stats/flags based on another object.
 * @return 1 if we sucessfully changed a stat, 0 if nothing was changed.
 * @note Flag is set to 1 if we are applying the object, -1 if we are
 * removing the object.
 * @note
 * It is the calling functions responsibilty to check to see if the object
 * can be applied or not. */
int change_abil(object *op, object *tmp)
{
	int flag = QUERY_FLAG(tmp, FLAG_APPLIED) ? 1 : -1, i, j, success = 0;
	object refop;
	int potion_max = 0;

	/* Remember what object was like before it was changed. Note that
	 * refop is a local copy of op only to be used for detecting changes
	 * found by fix_player. refop is not a real object. */
	memcpy(&refop, op, sizeof(object));

	if (op->type == PLAYER)
	{
		if (tmp->type == POTION)
		{
			for (j = 0; j < 7; j++)
			{
				i = get_attr_value(&(CONTR(op)->orig_stats), j);

				/* Check to see if stats are within limits such that this can be
				 * applied. */
				if (((i + flag * get_attr_value(&(tmp->stats), j)) <= (20 + tmp->stats.sp + get_attr_value(&(op->arch->clone.stats), j))) && i > 0)
				{
					change_attr_value(&(CONTR(op)->orig_stats), j, (sint8) (flag * get_attr_value(&(tmp->stats), j)));
					/* Fix it up for super potions */
					tmp->stats.sp = 0;
				}
				else
				{
					/* Potion is useless - player has already hit the natural maximum */
					potion_max = 1;
				}
			}

			/* This section of code ups the characters normal stats also.  I am not
			 * sure if this is strictly necessary, being that fix_player probably
			 * recalculates this anyway. */
			for (j = 0; j < 7; j++)
			{
				change_attr_value(&(op->stats), j, (sint8) (flag * get_attr_value(&(tmp->stats), j)));
			}

			check_stat_bounds(&(op->stats));
		}
	}

	/* Reset attributes that fix_player doesn't reset since it doesn't search
	 * everything to set */
	if (flag == -1)
	{
		op->path_attuned &= ~tmp->path_attuned, op->path_repelled &= ~tmp->path_repelled, op->path_denied &= ~tmp->path_denied;
	}

	/* call fix_player since op object could have whatever attribute due
	 * to multiple items.  if fix_player always has to be called after
	 * change_ability then might as well call it from here */
	fix_player(op);

	if (tmp->attack[ATNR_CONFUSION])
	{
		success = 1;

		if (flag > 0)
		{
			new_draw_info(NDI_UNIQUE | NDI_WHITE, op, "Your hands begin to glow red.");
		}
		else
		{
			new_draw_info(NDI_UNIQUE | NDI_GREY, op, "Your hands stop glowing red.");
		}
	}

	if (QUERY_FLAG(op, FLAG_LIFESAVE) != QUERY_FLAG(&refop, FLAG_LIFESAVE))
	{
		success = 1;

		if (flag > 0)
		{
			new_draw_info(NDI_UNIQUE | NDI_WHITE, op, "You feel very protected.");
		}
		else
		{
			new_draw_info(NDI_UNIQUE | NDI_GREY, op, "You don't feel protected anymore.");
		}
	}

	if (QUERY_FLAG(op, FLAG_REFL_MISSILE) != QUERY_FLAG(&refop, FLAG_REFL_MISSILE))
	{
		success = 1;

		if (flag > 0)
		{
			new_draw_info(NDI_UNIQUE | NDI_WHITE, op, "A magic force shimmers around you.");
		}
		else
		{
			new_draw_info(NDI_UNIQUE | NDI_GREY, op, "The magic force fades away.");
		}
	}

	if (QUERY_FLAG(op, FLAG_REFL_SPELL) != QUERY_FLAG(&refop, FLAG_REFL_SPELL))
	{
		success = 1;

		if (flag > 0)
		{
			new_draw_info(NDI_UNIQUE | NDI_WHITE, op, "You feel more safe now, somehow.");
		}
		else
		{
			new_draw_info(NDI_UNIQUE | NDI_GREY, op, "Suddenly you feel less safe, somehow.");
		}
	}

	if (QUERY_FLAG(tmp, FLAG_FLYING))
	{
		if (flag > 0)
		{
			success = 1;

			/* If we're already flying then fly higher */
			if (QUERY_FLAG(op, FLAG_FLYING) == QUERY_FLAG(&refop, FLAG_FLYING))
			{
				new_draw_info(NDI_UNIQUE | NDI_WHITE, op, "You float a little higher in the air.");
			}
			else
			{
				new_draw_info(NDI_UNIQUE | NDI_GREY, op, "You start to float in the air!");

				SET_MULTI_FLAG(op, FLAG_FLYING);

				if (op->speed > 1)
				{
					op->speed = 1;
				}
			}
		}
		else
		{
			success = 1;

			/* If we're already flying then fly lower */
			if (QUERY_FLAG(op, FLAG_FLYING) == QUERY_FLAG(&refop, FLAG_FLYING))
			{
				new_draw_info(NDI_UNIQUE | NDI_WHITE, op, "You float a little lower in the air.");
			}
			else
			{
				new_draw_info(NDI_UNIQUE | NDI_GREY, op, "You float down to the ground.");
				check_walk_on(op, op, 0);
			}
		}
	}

	/* Becoming UNDEAD... a special treatment for this flag. Only those not
	 * originally undead may change their status */
	if (!QUERY_FLAG(&op->arch->clone, FLAG_UNDEAD))
	{
		if (QUERY_FLAG(op, FLAG_UNDEAD) != QUERY_FLAG(&refop, FLAG_UNDEAD))
		{
			success = 1;

			if (flag > 0)
			{
				FREE_AND_COPY_HASH(op->race, "undead");
				new_draw_info(NDI_UNIQUE | NDI_GREY, op, "Your lifeforce drains away!");
			}
			else
			{
				FREE_AND_CLEAR_HASH(op->race);

				if (op->arch->clone.race)
				{
					FREE_AND_COPY_HASH(op->race, op->arch->clone.race);
				}

				new_draw_info(NDI_UNIQUE | NDI_WHITE, op, "Your lifeforce returns!");
			}
		}
	}

	if (QUERY_FLAG(op, FLAG_STEALTH) != QUERY_FLAG(&refop, FLAG_STEALTH))
	{
		success = 1;

		if (flag > 0)
		{
			new_draw_info(NDI_UNIQUE | NDI_WHITE, op, "You walk more quietly.");
		}
		else
		{
			new_draw_info(NDI_UNIQUE | NDI_GREY, op, "You walk more noisily.");
		}
	}

	if (QUERY_FLAG(op, FLAG_SEE_INVISIBLE) != QUERY_FLAG(&refop, FLAG_SEE_INVISIBLE))
	{
		success = 1;

		if (flag > 0)
		{
			new_draw_info(NDI_UNIQUE | NDI_WHITE, op, "You see invisible things.");
		}
		else
		{
			new_draw_info(NDI_UNIQUE | NDI_GREY, op, "Your vision becomes less clear.");
		}
	}

	if (QUERY_FLAG(op, FLAG_IS_INVISIBLE) != QUERY_FLAG(&refop, FLAG_IS_INVISIBLE))
	{
		success = 1;

		if (flag > 0)
		{
			new_draw_info(NDI_UNIQUE | NDI_WHITE, op, "You become transparent.");
		}
		else
		{
			new_draw_info(NDI_UNIQUE | NDI_GREY, op, "You can see yourself.");
		}
	}

	/* Blinded you can tell if more blinded since blinded player has minimal
	 * vision */
	if (QUERY_FLAG(tmp, FLAG_BLIND))
	{
		success = 1;

		if (flag > 0)
		{
			if (QUERY_FLAG(op, FLAG_WIZ))
			{
				new_draw_info(NDI_UNIQUE | NDI_GREY, op, "Your mortal self is blinded.");
			}
			else
			{
				new_draw_info(NDI_UNIQUE | NDI_GREY, op, "You are blinded.");
				SET_FLAG(op, FLAG_BLIND);

				if (op->type == PLAYER)
				{
					CONTR(op)->update_los = 1;
				}
			}
		}
		else
		{
			if (QUERY_FLAG(op, FLAG_WIZ))
			{
				new_draw_info(NDI_UNIQUE | NDI_WHITE, op, "Your mortal self can now see again.");
			}
			else
			{
				new_draw_info(NDI_UNIQUE | NDI_WHITE, op, "Your vision returns.");
				CLEAR_FLAG(op, FLAG_BLIND);

				if (op->type == PLAYER)
				{
					CONTR(op)->update_los = 1;
				}
			}
		}
	}

	if (QUERY_FLAG(op, FLAG_SEE_IN_DARK) != QUERY_FLAG(&refop, FLAG_SEE_IN_DARK))
	{
		success = 1;

		if (flag > 0)
		{
			new_draw_info(NDI_UNIQUE | NDI_WHITE, op, "Your vision is better in the dark.");
		}
		else
		{
			new_draw_info(NDI_UNIQUE | NDI_GREY, op, "You see less well in the dark.");
		}
	}

	if (QUERY_FLAG(op, FLAG_XRAYS) != QUERY_FLAG(&refop, FLAG_XRAYS))
	{
		success = 1;

		if (flag > 0)
		{
			if (QUERY_FLAG(op, FLAG_WIZ))
			{
				new_draw_info(NDI_UNIQUE | NDI_WHITE, op, "Your vision becomes a little clearer.");
			}
			else
			{
				new_draw_info(NDI_UNIQUE | NDI_GREY, op, "Everything becomes transparent.");

				if (op->type == PLAYER)
				{
					CONTR(op)->update_los = 1;
				}
			}
		}
		else
		{
			if (QUERY_FLAG(op, FLAG_WIZ))
			{
				new_draw_info(NDI_UNIQUE | NDI_WHITE, op, "Your vision becomes a bit out of focus.");
			}
			else
			{
				new_draw_info(NDI_UNIQUE | NDI_GREY, op, "Everything suddenly looks very solid.");

				if (op->type == PLAYER)
				{
					CONTR(op)->update_los = 1;
				}
			}
		}
	}

	if ((tmp->stats.hp || tmp->stats.maxhp) && op->type == PLAYER)
	{
		success = 1;

		if ((flag * tmp->stats.hp) > 0 || (flag * tmp->stats.maxhp) > 0)
		{
			new_draw_info(NDI_UNIQUE | NDI_WHITE, op, "You feel much more healthy!");
		}
		else
		{
			new_draw_info(NDI_UNIQUE | NDI_GREY, op, "You feel much less healthy!");
		}
	}

	if ((tmp->stats.sp || tmp->stats.maxsp) && op->type == PLAYER && tmp->type != SKILL)
	{
		success = 1;

		if ((flag * tmp->stats.sp) > 0 || (flag * tmp->stats.maxsp) > 0)
		{
			new_draw_info(NDI_UNIQUE | NDI_WHITE, op, "You feel one with the powers of magic!");
		}
		else
		{
			new_draw_info(NDI_UNIQUE | NDI_GREY, op, "You suddenly feel very mundane.");
		}
	}

	if ((tmp->stats.grace || tmp->stats.maxgrace) && op->type == PLAYER)
	{
		success = 1;

		if ((flag * tmp->stats.grace) > 0 || (flag * tmp->stats.maxgrace) > 0)
		{
			new_draw_info(NDI_UNIQUE | NDI_WHITE, op, "You feel closer to your deity!");
		}
		else
		{
			new_draw_info(NDI_UNIQUE | NDI_GREY, op, "You suddenly feel less holy.");
		}
	}

	if (tmp->stats.food && op->type == PLAYER && tmp->type != POISONING && tmp->type != POTION_EFFECT)
	{
		success = 1;

		if ((tmp->stats.food * flag) > 0)
		{
			new_draw_info(NDI_UNIQUE | NDI_WHITE, op, "You feel your digestion slowing down.");
		}
		else
		{
			new_draw_info(NDI_UNIQUE | NDI_GREY, op, "You feel your digestion speeding up.");
		}
	}

	/* Messages for changed protections */
	for (i = 0; i < NROFATTACKS; i++)
	{
		if (op->protection[i] != refop.protection[i])
		{
			success = 1;

			if (op->protection[i] > refop.protection[i])
			{
				new_draw_info_format(NDI_UNIQUE | NDI_GREEN, op, "Your protection to %s rises to %d%%.", attack_name[i], op->protection[i]);
			}
			else
			{
				new_draw_info_format(NDI_UNIQUE | NDI_BLUE, op, "Your protection to %s drops to %d%%.", attack_name[i], op->protection[i]);
			}
		}
	}

	/* If the attuned/repelled/denied paths changed, we need to update the
	 * spell list. */
	if (op->path_attuned != refop.path_attuned || op->path_repelled != refop.path_repelled || op->path_denied != refop.path_denied)
	{
		send_spelllist_cmd(op, NULL, SPLIST_MODE_UPDATE);
	}

	if (tmp->type != EXPERIENCE && !potion_max)
	{
		for (j = 0; j < 7; j++)
		{
			if ((i = get_attr_value(&(tmp->stats), j)) != 0)
			{
				success = 1;

				if ((i * flag) > 0)
				{
					new_draw_info(NDI_UNIQUE | NDI_WHITE, op, gain_msg[j]);
				}
				else
				{
					new_draw_info(NDI_UNIQUE | NDI_GREY, op, lose_msg[j]);
				}
			}
		}
	}

	return success;
}

/**
 * Drains a random stat from op.
 * @param op Object to drain. */
void drain_stat(object *op)
{
	drain_specific_stat(op, rndm(1, NUM_STATS) - 1);
}

/**
 * Drain a specified stat from op.
 * @param op Victim to drain.
 * @param deplete_stats Statistic to drain. */
void drain_specific_stat(object *op, int deplete_stats)
{
	object *tmp;
	archetype *at = find_archetype("depletion");

	if (!at)
	{
		LOG(llevBug, "BUG: Couldn't find archetype depletion.\n");
		return;
	}
	else
	{
		tmp = present_arch_in_ob(at, op);

		if (!tmp)
		{
			tmp = arch_to_object(at);
			tmp = insert_ob_in_ob(tmp, op);
			SET_FLAG(tmp, FLAG_APPLIED);
		}
	}

	new_draw_info(NDI_UNIQUE, op, drain_msg[deplete_stats]);
	change_attr_value(&tmp->stats, deplete_stats, -1);
	fix_player(op);
}

/**
 * Updates all abilities given by applied objects in the inventory
 * of the given object.
 *
 * This functions starts from base values (archetype or player object)
 * and then adjusts them according to what the player/monster has equipped.
 *
 * Note that a player always has stats reset to their initial value.
 * @param op Object to reset.
 * @todo This function is too long, and should be cleaned / split. */
void fix_player(object *op)
{
	int ring_count = 0, skill_level_max = 1;
	int tmp_item, old_glow, max_boni_hp = 0, max_boni_sp = 0, max_boni_grace = 0;
	float tmp_con;
	int i, j, inv_flag, inv_see_flag, light, weapon_weight, best_wc, best_ac, wc, ac;
	int protect_boni[NROFATTACKS], protect_mali[NROFATTACKS];
	int potion_protection_bonus[NROFATTACKS], potion_protection_malus[NROFATTACKS], potion_attack[NROFATTACKS];
	object *grace_obj = NULL, *mana_obj = NULL, *hp_obj = NULL, *wc_obj = NULL, *tmp, *skill_weapon = NULL;
	float f,max = 9, added_speed = 0, bonus_speed = 0, speed_reduce_from_disease = 1;
	player *pl;

	if (QUERY_FLAG(op, FLAG_NO_FIX_PLAYER))
	{
		return;
	}

	if (QUERY_FLAG(op, FLAG_MONSTER) && op->type != PLAYER)
	{
		fix_monster(op);
		return;
	}

	/* For secure */
	if (op->type != PLAYER)
	{
		LOG(llevDebug, "fix_player(): called from non Player/Mob object: %s (type %d)\n", query_name(op, NULL), op->type);
		return;
	}

	pl = CONTR(op);
	inv_flag = inv_see_flag = weapon_weight = best_wc = best_ac = wc = ac = 0;

	op->stats.Str = pl->orig_stats.Str;
	op->stats.Dex = pl->orig_stats.Dex;
	op->stats.Con = pl->orig_stats.Con;
	op->stats.Int = pl->orig_stats.Int;
	op->stats.Wis = pl->orig_stats.Wis;
	op->stats.Pow = pl->orig_stats.Pow;
	op->stats.Cha = pl->orig_stats.Cha;

	pl->selected_weapon = pl->skill_weapon = NULL;
	pl->digestion = 3;
	pl->gen_hp = 1;
	pl->gen_sp = 1;
	pl->gen_grace = 1;
	pl->gen_sp_armour = 0;
	pl->item_power = 0;
	/* The used skills for fast access */
	pl->set_skill_weapon = NO_SKILL_READY;
	pl->set_skill_archery = NO_SKILL_READY;

	pl->encumbrance = 0;

	/* For players, we adjust with the values */
	ac = op->arch->clone.stats.ac;
	wc = op->arch->clone.stats.wc;
	op->stats.wc = wc;
	op->stats.ac = ac;
	op->stats.dam = op->arch->clone.stats.dam;

	op->stats.maxhp = op->arch->clone.stats.maxhp;
	op->stats.maxsp = op->arch->clone.stats.maxsp;
	op->stats.maxgrace = op->arch->clone.stats.maxgrace;

	pl->levhp[1] = (char) op->stats.maxhp;
	pl->levsp[1] = (char) op->stats.maxsp;
	pl->levgrace[1] = (char) op->stats.maxgrace;

	op->stats.wc_range = op->arch->clone.stats.wc_range;

	old_glow = op->glow_radius;
	light = op->arch->clone.glow_radius;

	op->speed = op->arch->clone.speed;
	op->weapon_speed = op->arch->clone.weapon_speed;
	op->path_attuned = op->arch->clone.path_attuned;
	op->path_repelled = op->arch->clone.path_repelled;
	op->path_denied = op->arch->clone.path_denied;
	/* Reset terrain moving abilities */
	op->terrain_flag = op->arch->clone.terrain_flag;

	/* Only adjust skills which have no own level/exp values */
	if (op->chosen_skill && !op->chosen_skill->last_eat && op->chosen_skill->exp_obj)
	{
		op->chosen_skill->level = op->chosen_skill->exp_obj->level;
	}

	FREE_AND_CLEAR_HASH(op->slaying);

	if (QUERY_FLAG(op, FLAG_IS_INVISIBLE))
	{
		inv_flag = 1;
	}

	if (QUERY_FLAG(op, FLAG_SEE_INVISIBLE))
	{
		inv_see_flag = 1;
	}

	if (!QUERY_FLAG(&op->arch->clone, FLAG_XRAYS))
	{
		CLEAR_FLAG(op, FLAG_XRAYS);
	}

	if (!QUERY_FLAG(&op->arch->clone, FLAG_CAN_PASS_THRU))
	{
		CLEAR_MULTI_FLAG(op, FLAG_CAN_PASS_THRU);
	}

	if (!QUERY_FLAG(&op->arch->clone, FLAG_IS_ETHEREAL))
	{
		CLEAR_MULTI_FLAG(op, FLAG_IS_ETHEREAL);
	}

	if (!QUERY_FLAG(&op->arch->clone, FLAG_IS_INVISIBLE))
	{
		CLEAR_MULTI_FLAG(op, FLAG_IS_INVISIBLE);
	}

	if (!QUERY_FLAG(&op->arch->clone, FLAG_SEE_INVISIBLE))
	{
		CLEAR_FLAG(op, FLAG_SEE_INVISIBLE);
	}

	if (!QUERY_FLAG(&op->arch->clone, FLAG_LIFESAVE))
	{
		CLEAR_FLAG(op, FLAG_LIFESAVE);
	}

	if (!QUERY_FLAG(&op->arch->clone, FLAG_STEALTH))
	{
		CLEAR_FLAG(op, FLAG_STEALTH);
	}

	if (!QUERY_FLAG(&op->arch->clone, FLAG_BLIND))
	{
		CLEAR_FLAG(op, FLAG_BLIND);
	}

	if (!QUERY_FLAG(&op->arch->clone, FLAG_FLYING))
	{
		CLEAR_MULTI_FLAG(op, FLAG_FLYING);
	}

	if (!QUERY_FLAG(&op->arch->clone, FLAG_REFL_SPELL))
	{
		CLEAR_FLAG(op, FLAG_REFL_SPELL);
	}

	if (!QUERY_FLAG(&op->arch->clone, FLAG_REFL_MISSILE))
	{
		CLEAR_FLAG(op, FLAG_REFL_MISSILE);
	}

	if (!QUERY_FLAG(&op->arch->clone, FLAG_UNDEAD))
	{
		CLEAR_FLAG(op, FLAG_UNDEAD);
	}

	if (!QUERY_FLAG(&op->arch->clone, FLAG_SEE_IN_DARK))
	{
		CLEAR_FLAG(op, FLAG_SEE_IN_DARK);
	}

	memset(&protect_boni, 0, sizeof(protect_boni));
	memset(&protect_mali, 0, sizeof(protect_mali));
	memset(&potion_protection_bonus, 0, sizeof(potion_protection_bonus));
	memset(&potion_protection_malus, 0, sizeof(potion_protection_malus));
	memset(&potion_attack, 0, sizeof(potion_attack));

	/* Initializing player arrays from the values in player archetype clone:  */
	memset(&pl->equipment, 0, sizeof(pl->equipment));
	memcpy(&op->protection, &op->arch->clone.protection, sizeof(op->protection));
	memcpy(&op->attack, &op->arch->clone.attack, sizeof(op->attack));

	/* Now we browse the inventory... There is not only our equipment -
	 * there are all our skills, forces and hidden system objects. */
	for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
	{
		/* Add here more types we can and must skip. */
		if (tmp->type == SCROLL || tmp->type == POTION || tmp->type == CONTAINER || tmp->type == CLOSE_CON || tmp->type == LIGHT_REFILL || tmp->type == WAND || tmp->type == ROD || tmp->type == HORN)
		{
			continue;
		}

		/* This is needed, because our applied light can be overruled by a light giving
		 * object like holy glowing aura force or something */
		if (tmp->glow_radius > light)
		{
			/* Don't use this item when it is a 'not applied LIGHT_APPLY' */
			if (tmp->type != LIGHT_APPLY || QUERY_FLAG(tmp, FLAG_APPLIED))
			{
				light = tmp->glow_radius;
			}
		}

		/* All skills, not only the applied ones */
		if (tmp->type == SKILL)
		{
			/* Get highest single skill - that's our "main" skill */
			if (tmp->level > skill_level_max)
			{
				skill_level_max = tmp->level;
			}

			/* Let's remember the best bare hand skill */
			if (tmp->stats.dam > 0)
			{
				if (!skill_weapon || skill_weapon->stats.dam < tmp->stats.dam)
				{
					skill_weapon = tmp;
				}
			}

			/* Save in table for quick access */
			pl->skill_ptr[tmp->stats.sp] = tmp;
		}
		else if (tmp->type == QUEST_CONTAINER)
		{
			pl->quest_container = tmp;
		}

		/* This checks all applied items in the inventory */
		if (QUERY_FLAG(tmp, FLAG_APPLIED))
		{
			/* Still applied stuff */
			switch (tmp->type)
			{
				case LIGHT_APPLY:
					if (tmp->glow_radius > light)
					{
						light = tmp->glow_radius;
					}

					break;

				case SKILL_ITEM:
					pl->equipment[PLAYER_EQUIP_SKILL_ITEM] = tmp;
					break;

				case WEAPON:
					pl->equipment[PLAYER_EQUIP_WEAPON] = tmp;
					/* Our weapon */
					pl->selected_weapon = tmp;
					i = tmp->sub_type % 4;

					if (i == WEAP_1H_IMPACT)
					{
						pl->set_skill_weapon = SK_MELEE_WEAPON;
					}
					else if (i == WEAP_1H_SLASH)
					{
						pl->set_skill_weapon = SK_SLASH_WEAP;
					}
					else if (i == WEAP_1H_CLEAVE)
					{
						pl->set_skill_weapon = SK_CLEAVE_WEAP;
					}
					else
					{
						pl->set_skill_weapon = SK_PIERCE_WEAP;
					}

					op->weapon_speed = tmp->weapon_speed;

					if (!op->weapon_speed)
					{
						LOG(llevBug, "BUG: monster/player %s applied weapon %s without weapon speed!\n", op->name, tmp->name);
					}

					wc += (tmp->stats.wc + tmp->magic);

					if (tmp->stats.ac && tmp->stats.ac + tmp->magic > 0)
					{
						ac += tmp->stats.ac + tmp->magic;
					}

					op->stats.dam += (tmp->stats.dam + tmp->magic);
					weapon_weight = tmp->weight;

					if (tmp->slaying)
					{
						FREE_AND_COPY_HASH(op->slaying, tmp->slaying);
					}

					pl->encumbrance += (sint16) (3 * tmp->weight / 1000);
					pl->digestion += tmp->stats.food;
					pl->gen_sp += tmp->stats.sp;
					pl->gen_grace += tmp->stats.grace;
					pl->gen_hp += tmp->stats.hp;
					pl->gen_sp_armour += tmp->last_heal;
					pl->item_power += tmp->item_power;

					for (i = 0; i < 7; i++)
					{
						change_attr_value(&(op->stats), i, get_attr_value(&(tmp->stats), i));
					}

					break;

				/* All armours + rings and amulets */
				case RING:
					pl->equipment[PLAYER_EQUIP_RRING + ring_count] = tmp;
					ring_count++;
					goto fix_player_no_armour;

				case AMULET:
					pl->equipment[PLAYER_EQUIP_AMULET] = tmp;
					goto fix_player_no_armour;

				case BRACERS:
					pl->equipment[PLAYER_EQUIP_BRACER] = tmp;
					goto fix_player_jump1;

				case ARMOUR:
					pl->equipment[PLAYER_EQUIP_MAIL] = tmp;
					pl->encumbrance += (int) tmp->weight / 1000;
					goto fix_player_jump1;

				case SHIELD:
					pl->equipment[PLAYER_EQUIP_SHIELD] = tmp;
					pl->encumbrance += (int) tmp->weight / 2000;
					goto fix_player_jump1;

				case GIRDLE:
					pl->equipment[PLAYER_EQUIP_GIRDLE] = tmp;
					goto fix_player_jump1;

				case HELMET:
					pl->equipment[PLAYER_EQUIP_HELM] = tmp;
					goto fix_player_jump1;

				case BOOTS:
					pl->equipment[PLAYER_EQUIP_BOOTS] = tmp;
					goto fix_player_jump1;

				case GLOVES:
					pl->equipment[PLAYER_EQUIP_GAUNTLET] = tmp;
					goto fix_player_jump1;

				case CLOAK:
					pl->equipment[PLAYER_EQUIP_CLOAK] = tmp;

fix_player_jump1:
					/* Used for ALL armours except rings and amulets */
					if (ARMOUR_SPEED(tmp) && (float) ARMOUR_SPEED(tmp) / 10.0f < max)
					{
						max = ARMOUR_SPEED(tmp) / 10.0f;
					}

fix_player_no_armour:
					max_boni_hp += tmp->stats.maxhp;
					max_boni_sp += tmp->stats.maxsp;
					max_boni_grace += tmp->stats.maxgrace;
					pl->digestion += tmp->stats.food;
					pl->gen_sp += tmp->stats.sp;
					pl->gen_grace += tmp->stats.grace;
					pl->gen_hp += tmp->stats.hp;
					pl->gen_sp_armour += tmp->last_heal;
					pl->item_power += tmp->item_power;

					for (i = 0; i < 7; i++)
					{
						change_attr_value(&(op->stats), i, get_attr_value(&(tmp->stats), i));
					}

					if (tmp->stats.wc)
					{
						wc += (tmp->stats.wc + tmp->magic);
					}

					if (tmp->stats.dam)
					{
						op->stats.dam += (tmp->stats.dam + tmp->magic);
					}

					if (tmp->stats.ac)
					{
						ac += (tmp->stats.ac + tmp->magic);
					}

					break;

				case BOW:
					pl->equipment[PLAYER_EQUIP_BOW] = tmp;

					/* As a special bonus range weapons can be permanently applied and
					 * will add stat bonus */
					for (i = 0; i < 7; i++)
					{
						change_attr_value(&(op->stats), i, get_attr_value(&(tmp->stats), i));
					}

					if (tmp->sub_type == RANGE_WEAP_BOW)
					{
						pl->set_skill_archery = SK_MISSILE_WEAPON;
					}
					else if (tmp->sub_type == RANGE_WEAP_XBOWS)
					{
						pl->set_skill_archery = SK_XBOW_WEAP;
					}
					else
					{
						pl->set_skill_archery = SK_SLING_WEAP;
					}

					break;

				/* No protection from potion effect - resist only! */
				case POTION_EFFECT:
					for (i = 0; i < 7; i++)
					{
						change_attr_value(&(op->stats), i, get_attr_value(&(tmp->stats), i));
					}

					/* Collect highest bonus & malus - only highest one counts,
					 * no adding potion effects of same resist */
					for (i = 0; i < NROFATTACKS; i++)
					{
						/* Collect highest/lowest resistance */
						if (tmp->protection[i] > potion_protection_bonus[i])
						{
							potion_protection_bonus[i] = tmp->protection[i];
						}
						else if (tmp->protection[i] < potion_protection_malus[i])
						{
							potion_protection_malus[i] = tmp->protection[i];
						}

						if (tmp->attack[i] > potion_attack[i])
						{
							potion_attack[i] = tmp->attack[i];
						}
					}

					break;

				case EXPERIENCE:
					if (tmp->stats.Str && !wc_obj)
					{
						wc_obj = tmp;
					}

					if (tmp->stats.Con && !hp_obj)
					{
						hp_obj = tmp;
					}

					/* For spellpoint determination */
					if (tmp->stats.Pow && !mana_obj)
					{
						mana_obj = tmp;
					}

					if (tmp->stats.Wis && !grace_obj)
					{
						grace_obj = tmp;
					}

					break;

				/* Skills modifying the character */
				case SKILL:
					/* Skill is a 'weapon' */
					if (tmp->stats.dam > 0)
					{
						wc += tmp->stats.wc;
						ac += tmp->stats.ac;
						op->weapon_speed = tmp->weapon_speed;
						weapon_weight = tmp->weight;
						op->stats.dam += tmp->stats.dam;
					}
					else
					{
						op->stats.dam += tmp->stats.dam;
						wc += tmp->stats.wc;
						ac += tmp->stats.ac;
					}

					if (tmp->slaying)
					{
						FREE_AND_COPY_HASH(op->slaying, tmp->slaying);
					}

					pl->encumbrance += (int) 3 * tmp->weight / 1000;
					break;

				case CLASS:
					/* Copy some values from the class object to the
					 * player. */
					for (i = 0; i < NUM_STATS; i++)
					{
						change_attr_value(&op->stats, i, get_attr_value(&tmp->stats, i));
					}

					wc += tmp->stats.wc;
					op->stats.dam += tmp->stats.dam;
					ac += tmp->stats.ac;
					op->stats.maxhp += tmp->stats.maxhp;
					op->stats.maxsp += tmp->stats.maxsp;
					op->stats.maxgrace += tmp->stats.maxgrace;
					CONTR(op)->class_ob = tmp;
					break;

				case FORCE:
					if (ARMOUR_SPEED(tmp) && (float)ARMOUR_SPEED(tmp) / 10.0f < max)
					{
						max = ARMOUR_SPEED(tmp) / 10.0f;
					}

					for (i = 0; i < 7; i++)
					{
						change_attr_value(&(op->stats), i, get_attr_value(&(tmp->stats), i));
					}

					if (tmp->stats.wc)
					{
						wc += (tmp->stats.wc + tmp->magic);
					}

					if (tmp->stats.dam)
					{
						op->stats.dam += (tmp->stats.dam + tmp->magic);
					}

					if (tmp->stats.ac)
					{
						ac += (tmp->stats.ac + tmp->magic);
					}

					if (tmp->stats.maxhp)
					{
						op->stats.maxhp += tmp->stats.maxhp;
					}

					if (tmp->stats.maxsp)
					{
						op->stats.maxsp += tmp->stats.maxsp;
					}

					if (tmp->stats.maxgrace)
					{
						op->stats.maxgrace += tmp->stats.maxgrace;
					}

					goto fix_player_jump_resi;

				case DISEASE:
				case SYMPTOM:
					speed_reduce_from_disease = (float) tmp->last_sp / 100.0f;

					if (speed_reduce_from_disease == 0.0f)
					{
						speed_reduce_from_disease = 1.0f;
					}

				case POISONING:
					for (i = 0; i < NUM_STATS; i++)
					{
						change_attr_value(&op->stats, i, get_attr_value(&tmp->stats, i));
					}

				case BLINDNESS:
				case CONFUSION:
fix_player_jump_resi:

					for (i = 0; i < NROFATTACKS; i++)
					{
						if (tmp->protection[i] > 0)
						{
							protect_boni[i] += ((100 - protect_boni[i]) * tmp->protection[i]) / 100;
						}
						else if (tmp->protection[i] < 0)
						{
							protect_mali[i] += ((100 - protect_mali[i]) * (-tmp->protection[i])) / 100;
						}

						if (tmp->type != DISEASE && tmp->type != SYMPTOM && tmp->type != POISONING)
						{
							if (tmp->attack[i] > 0)
							{
								if ((op->attack[i] + tmp->attack[i]) <= 120)
								{
									op->attack[i] += tmp->attack[i];
								}
								else
								{
									op->attack[i] = 120;
								}
							}
						}
					}

					break;

				/* Catch items which are applied but should not be -
				 * or we forgot to catch them here. */
				default:
					LOG(llevDebug, "DEBUG: fix_player(): unexpected applied object %s (%d)(clear flag now!)\n", query_name(tmp, NULL), tmp->type);
					CLEAR_FLAG(tmp, FLAG_APPLIED);
					break;
			}

			/* We just add a given terrain */
			op->terrain_flag |= tmp->terrain_type;
			op->path_attuned |= tmp->path_attuned;
			op->path_repelled |= tmp->path_repelled;
			op->path_denied |= tmp->path_denied;

			if (QUERY_FLAG(tmp, FLAG_LIFESAVE))
			{
				SET_FLAG(op, FLAG_LIFESAVE);
			}

			if (QUERY_FLAG(tmp, FLAG_REFL_SPELL))
			{
				SET_FLAG(op, FLAG_REFL_SPELL);
			}

			if (QUERY_FLAG(tmp, FLAG_REFL_MISSILE))
			{
				SET_FLAG(op, FLAG_REFL_MISSILE);
			}

			if (QUERY_FLAG(tmp, FLAG_STEALTH))
			{
				SET_FLAG(op, FLAG_STEALTH);
			}

			if (QUERY_FLAG(tmp, FLAG_UNDEAD) && !QUERY_FLAG(&op->arch->clone, FLAG_UNDEAD))
			{
				SET_FLAG(op, FLAG_UNDEAD);
			}

			if (QUERY_FLAG(tmp, FLAG_XRAYS))
			{
				SET_FLAG(op, FLAG_XRAYS);
			}

			if (QUERY_FLAG(tmp, FLAG_BLIND))
			{
				SET_FLAG(op, FLAG_BLIND);
			}

			if (QUERY_FLAG(tmp, FLAG_SEE_IN_DARK))
			{
				SET_FLAG(op, FLAG_SEE_IN_DARK);
			}

			if (QUERY_FLAG(tmp, FLAG_SEE_INVISIBLE))
			{
				SET_FLAG(op, FLAG_SEE_INVISIBLE);
			}

			if (QUERY_FLAG(tmp, FLAG_MAKE_INVISIBLE))
			{
				SET_MULTI_FLAG(op, FLAG_IS_INVISIBLE);
			}

			if (QUERY_FLAG(tmp, FLAG_CAN_PASS_THRU))
			{
				SET_MULTI_FLAG(op, FLAG_CAN_PASS_THRU);
			}

			if (QUERY_FLAG(tmp, FLAG_MAKE_ETHEREAL))
			{
				SET_MULTI_FLAG(op, FLAG_CAN_PASS_THRU);
				SET_MULTI_FLAG(op, FLAG_IS_ETHEREAL);
			}

			if (QUERY_FLAG(tmp, FLAG_FLYING))
			{
				SET_MULTI_FLAG(op, FLAG_FLYING);

				if (!QUERY_FLAG(op, FLAG_WIZ))
				{
					max = 1;
				}
			}

			/* Slow penalty */
			if (tmp->stats.exp && tmp->type != EXPERIENCE && tmp->type != SKILL)
			{
				if (tmp->stats.exp > 0)
				{
					added_speed += (float) tmp->stats.exp / 3.0f;
					bonus_speed += 1.0f + (float) tmp->stats.exp / 3.0f;
				}
				else
				{
					added_speed += (float) tmp->stats.exp;
				}
			}
		}
	}

	/* Now collect all our real armour stuff - this will now add *after* all force
	 * or non potion effects effecting resist, attack or protection - also this will
	 * give us a sorted adding.
	 * But the most important point is that we calculate *here* and only here the equipment
	 * quality modifier for players.
	 * Note, that for bows/arrows the calculation is done "on the fly" when the
	 * throw/fire object is created! */
	for (j = 0; j < PLAYER_EQUIP_MAX; j++)
	{
		if (pl->equipment[j])
		{
			tmp = pl->equipment[j];
			tmp_con = (float) tmp->item_condition / 100.0f;

			/* Only quality adjustment for *positive* values! */
			for (i = 0; i < NROFATTACKS; i++)
			{
				if (tmp->protection[i] > 0)
				{
					tmp_item = (int) ((float) tmp->protection[i] * tmp_con);
					protect_boni[i] += ((100 - protect_boni[i]) * tmp_item) / 100;
				}
				else if (tmp->protection[i] < 0)
				{
					protect_mali[i] += ((100 - protect_mali[i]) * (-tmp->protection[i])) / 100;
				}

				if (tmp->type != BOW)
				{
					if (tmp->attack[i] > 0)
					{
						tmp_item = (int) ((float) tmp->attack[i] * tmp_con);

						if ((op->attack[i] + tmp_item) <= 120)
						{
							op->attack[i] += tmp_item;
						}
						else
						{
							op->attack[i] = 120;
						}
					}
				}
			}
		}
	}

	/* Now we add in all our values... we add in our potions effects as well as
	 * our attack boni and/or protections. */
	for (j = 1, i = 0; i < NROFATTACKS; i++, j <<= 1)
	{
		if (potion_attack[i])
		{
			if ((op->attack[i] + potion_attack[i]) > 120)
			{
				op->attack[i] = 120;
			}
			else
			{
				op->attack[i] += potion_attack[i];
			}
		}
	}

	/* Add protection bonuses.
	 * Ensure that protection is between 0 - 100. */
	for (i = 0; i < NROFATTACKS; i++)
	{
		int ptemp;

		/* Add in the potion protections. */
		if (potion_protection_bonus[i] > 0)
		{
			protect_boni[i] += ((100 - protect_boni[i]) * potion_protection_bonus[i]) / 100;
		}

		if (potion_protection_malus[i] < 0)
		{
			protect_mali[i] += ((100 - protect_mali[i]) * (-potion_protection_malus[i])) / 100;
		}

		ptemp = protect_boni[i] - protect_mali[i];

		if (ptemp < -100)
		{
			op->protection[i] = -100;
		}
		else if (ptemp > 100)
		{
			op->protection[i] = 100;
		}
		else
		{
			op->protection[i] = ptemp;
		}
	}

	check_stat_bounds(&(op->stats));

	/* Now the speed thing... */
	op->speed += speed_bonus[op->stats.Dex];

	if (added_speed >= 0)
	{
		op->speed += added_speed / 10.0f;
	}
	/* Something wrong here...: */
	else
	{
		op->speed /= 1.0f - added_speed;
	}

	if (op->speed > max)
	{
		op->speed = max;
	}

	/* Calculate real speed */
	op->speed += bonus_speed / 10.0f;

	/* Put a lower limit on speed. Note with this speed, you move once every
	 * 100 ticks or so. This amounts to once every 12 seconds of realtime. */
	op->speed = op->speed * speed_reduce_from_disease;

	/* Don't reduce under this value */
	if (op->speed < 0.01f)
	{
		op->speed = 0.01f;
	}
	else
	{
		/* Max kg we can carry */
		f = ((float) weight_limit[op->stats.Str] / 100.0f) * ENCUMBRANCE_LIMIT;

		if (((sint32) f) <= op->carrying)
		{
			if (op->carrying >= (sint32) weight_limit[op->stats.Str])
			{
				op->speed = 0.01f;
			}
			else
			{
				/* Total encumbrance weight part */
				f = ((float) weight_limit[op->stats.Str] - f);
				/* Value from 0.0 to 1.0 encumbrance */
				f = ((float) weight_limit[op->stats.Str] - op->carrying) / f;

				if (f < 0.0f)
				{
					f = 0.0f;
				}
				else if (f > 1.0f)
				{
					f = 1.0f;
				}

				op->speed *= f;

				if (op->speed < 0.01f)
				{
					op->speed = 0.01f;
				}
			}
		}
	}

	update_ob_speed(op);
	op->weapon_speed_add = op->weapon_speed;

	op->glow_radius = light;

	/* We must do -old_gow + light */
	if (op->map && old_glow != light)
	{
		adjust_light_source(op->map, op->x, op->y, light - old_glow);
	}

	/* *3 is base */
	op->stats.maxhp += op->arch->clone.stats.maxhp + op->arch->clone.stats.maxhp;

	for (i = 1; i <= op->level; i++)
	{
		op->stats.maxhp += pl->levhp[i];
	}

	if (mana_obj)
	{
		for (i = 1; i <= mana_obj->level; i++)
		{
			op->stats.maxsp += pl->levsp[i];
		}
	}

	if (grace_obj)
	{
		for (i = 1; i <= grace_obj->level; i++)
		{
			op->stats.maxgrace += pl->levgrace[i];
		}
	}

	/* Now adjust with the % of the stats mali/boni. */
	op->stats.maxhp += (int) ((float) op->stats.maxhp * con_bonus[op->stats.Con]) + max_boni_hp;
	op->stats.maxsp += (int) ((float) op->stats.maxsp * pow_bonus[op->stats.Pow]) + max_boni_sp;
	op->stats.maxgrace += (int) ((float) op->stats.maxgrace * wis_bonus[op->stats.Wis]) + max_boni_grace;

	/* HP/SP/Grace adjustements coming from class-defining object. */
	if (CONTR(op)->class_ob)
	{
		if (CONTR(op)->class_ob->stats.hp)
		{
			op->stats.maxhp += ((float) op->stats.maxhp / 100.0f) * (float) CONTR(op)->class_ob->stats.hp;
		}

		if (CONTR(op)->class_ob->stats.sp)
		{
			op->stats.maxsp += ((float) op->stats.maxsp / 100.0f) * (float) CONTR(op)->class_ob->stats.sp;
		}

		if (CONTR(op)->class_ob->stats.grace)
		{
			op->stats.maxgrace += ((float) op->stats.maxgrace / 100.0f) * (float) CONTR(op)->class_ob->stats.grace;
		}
	}

	if (op->stats.maxhp < 1)
	{
		op->stats.maxhp = 1;
	}

	if (op->stats.maxsp < 1)
	{
		op->stats.maxsp = 1;
	}

	if (op->stats.maxgrace < 1)
	{
		op->stats.maxgrace = 1;
	}

	if (op->stats.hp == -1)
	{
		op->stats.hp = op->stats.maxhp;
	}

	if (op->stats.sp == -1)
	{
		op->stats.sp = op->stats.maxsp;
	}

	if (op->stats.grace == -1)
	{
		op->stats.grace = op->stats.maxgrace;
	}

	/* Cap the pools to <= max */
	if (op->stats.hp > op->stats.maxhp)
	{
		op->stats.hp = op->stats.maxhp;
	}

	if (op->stats.sp > op->stats.maxsp)
	{
		op->stats.sp = op->stats.maxsp;
	}

	if (op->stats.grace > op->stats.maxgrace)
	{
		op->stats.grace = op->stats.maxgrace;
	}

	op->stats.ac = ac + skill_level_max;

	/* No weapon in our hand - we must use our hands */
	if (pl->set_skill_weapon == NO_SKILL_READY)
	{
		f = 1.0f;

		if (skill_weapon)
		{
			/* Now we must add this special skill attack */
			for (i = 0; i < NROFATTACKS; i++)
			{
				if (op->attack[i] + skill_weapon->attack[i] > 120)
				{
					op->attack[i] = 120;
				}
				else
				{
					op->attack[i] += skill_weapon->attack[i];
				}
			}

			pl->skill_weapon = skill_weapon;
			op->stats.wc = wc + skill_weapon->level;
			op->stats.dam = (sint16) ((float) op->stats.dam * LEVEL_DAMAGE(skill_weapon->level));
		}
		else
		{
			LOG(llevBug, "BUG: fix_player(): player %s has no hth skill!\n", op->name);
		}
	}
	/* Weapon in hand */
	else
	{
		f = (float) (pl->equipment[PLAYER_EQUIP_WEAPON]->item_condition) / 100.0f;

		/* Weapon without the skill applied... */
		if (!pl->skill_ptr[pl->set_skill_weapon])
		{
			LOG(llevBug, "BUG: fix_player(): player %s has weapon selected but not the skill #%d!!!\n", op->name, pl->set_skill_weapon);
		}
		else
		{
			op->stats.wc = wc + pl->skill_ptr[pl->set_skill_weapon]->level;
			op->stats.dam = (sint16) ((float) op->stats.dam * LEVEL_DAMAGE(pl->skill_ptr[pl->set_skill_weapon]->level));
		}
	}

	/* Now the last adds - stat bonus to damage and WC */
	op->stats.dam += dam_bonus[op->stats.Str];

	if (op->stats.dam < 0)
	{
		op->stats.dam = 0;
	}

	CONTR(op)->client_dam = (sint16) ((float) op->stats.dam * f);
	op->stats.wc += thaco_bonus[op->stats.Dex];

	/* For the client */
	pl->weapon_sp = (char) (op->weapon_speed / 0.0025f);

	if (!pl->quest_container)
	{
		object *quest_container = get_archetype(QUEST_CONTAINER_ARCHETYPE);

		LOG(llevBug, "BUG: fix_player(): Player %s had no quest container, fixing.\n", op->name);
		insert_ob_in_ob(quest_container, op);
		pl->quest_container = quest_container;
	}

	if (QUERY_FLAG(op, FLAG_IS_INVISIBLE))
	{
		/* We must reinsert us in the invisible chain */
		if (!inv_flag)
		{
			update_object(op, UP_OBJ_LAYER);
		}
	}
	/* And !FLAG_IS_INVISIBLE */
	else if (inv_flag)
	{
		update_object(op, UP_OBJ_LAYER);
	}

	if (QUERY_FLAG(op, FLAG_SEE_INVISIBLE))
	{
		if (!inv_see_flag)
		{
			pl->socket.update_tile = 0;
		}
	}
	/* And !FLAG_SEE_INVISIBLE */
	else if (inv_see_flag)
	{
		pl->socket.update_tile = 0;
	}
}

/**
 * Like fix_player(), but for monsters.
 * @param op The monster. */
void fix_monster(object *op)
{
	object *base, *tmp;
	float tmp_add;

	if (op->head)
	{
		return;
	}

	/* Will insert or/and return base info */
	base = insert_base_info_object(op);

	CLEAR_FLAG(op, FLAG_READY_BOW);

	op->stats.maxhp = (base->stats.maxhp * (op->level + 3) + (op->level / 2) * base->stats.maxhp) / 10;
	op->stats.maxsp = base->stats.maxsp * (op->level + 1);
	op->stats.maxgrace = base->stats.maxgrace * (op->level + 1);

	if (op->stats.hp == -1)
	{
		op->stats.hp = op->stats.maxhp;
	}

	if (op->stats.sp == -1)
	{
		op->stats.sp = op->stats.maxsp;
	}

	if (op->stats.grace == -1)
	{
		op->stats.grace = op->stats.maxgrace;
	}

	/* Cap the pools to <= max */
	if (op->stats.hp > op->stats.maxhp)
	{
		op->stats.hp = op->stats.maxhp;
	}

	if (op->stats.sp > op->stats.maxsp)
	{
		op->stats.sp = op->stats.maxsp;
	}

	if (op->stats.grace > op->stats.maxgrace)
	{
		op->stats.grace = op->stats.maxgrace;
	}

	op->stats.ac = base->stats.ac + op->level;
	/* + level / 4 to catch up the equipment improvements of
	 * the players in armour items. */
	op->stats.wc = base->stats.wc + op->level + (op->level / 4);
	op->stats.dam = base->stats.dam;

	if (base->stats.wc_range)
	{
		op->stats.wc_range = base->stats.wc_range;
	}
	/* Default value if not set in arch */
	else
	{
		op->stats.wc_range = 20;
	}

	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		/* Check for bow and use it! */
		if (tmp->type == BOW)
		{
			if (QUERY_FLAG(op, FLAG_USE_BOW))
			{
				SET_FLAG(tmp, FLAG_APPLIED);
				SET_FLAG(op, FLAG_READY_BOW);
			}
			else
			{
				CLEAR_FLAG(tmp, FLAG_APPLIED);
			}
		}
		else if (QUERY_FLAG(op, FLAG_USE_ARMOUR) && IS_ARMOR(tmp) && check_good_armour(op, tmp))
		{
			SET_FLAG(tmp, FLAG_APPLIED);
		}
		else if (QUERY_FLAG(op, FLAG_USE_WEAPON) && tmp->type == WEAPON && check_good_weapon(op, tmp))
		{
			SET_FLAG(tmp, FLAG_APPLIED);
		}

		if (QUERY_FLAG(tmp, FLAG_APPLIED))
		{
			int i;

			if (tmp->type == WEAPON)
			{
				op->stats.dam += tmp->stats.dam;
				op->stats.wc += tmp->stats.wc;
			}
			else if (IS_ARMOR(tmp))
			{
				for (i = 0; i < NROFATTACKS; i++)
				{
					op->protection[i] = MIN(op->protection[i] + tmp->protection[i], 15);
				}

				op->stats.ac += tmp->stats.ac;
			}
		}
	}

	if ((tmp_add = LEVEL_DAMAGE(op->level / 3) - 0.75f) < 0)
	{
		tmp_add = 0;
	}

	if (op->more && QUERY_FLAG(op, FLAG_FRIENDLY))
	{
		SET_MULTI_FLAG(op, FLAG_FRIENDLY);
	}

	op->stats.dam = (sint16) (((float) op->stats.dam * ((LEVEL_DAMAGE(op->level < 0 ? 0 : op->level) + tmp_add) * (0.925f + 0.05 * (op->level / 10)))) / 10.0f);

	/* Add a special decrease of power for monsters level 1-5 */
	if (op->level <= 5)
	{
		float d = 1.0f - ((0.35f / 5.0f) * (float) (6 - op->level));

		op->stats.dam = (int) ((float) op->stats.dam * d);

		if (op->stats.dam < 1)
		{
			op->stats.dam = 1;
		}

		op->stats.maxhp = (int) ((float) op->stats.maxhp * d);

		if (op->stats.maxhp < 1)
		{
			op->stats.maxhp = 1;
		}

		if (op->stats.hp > op->stats.maxhp)
		{
			op->stats.hp = op->stats.maxhp;
		}
	}

	set_mobile_speed(op, 0);
}

/**
 * Insert and initialize base info object in object op.
 * @param op Object.
 * @return Pointer to the inserted base info object. */
object *insert_base_info_object(object *op)
{
	object *tmp, *head = op;

	if (op->head)
	{
		head = op->head;
	}

	if (op->type == PLAYER)
	{
		LOG(llevBug, "BUG: insert_base_info_object() Try to inserting base_info in player %s!\n", query_name(head, NULL));
		return NULL;
	}

	if ((tmp = find_base_info_object(head)))
	{
		return tmp;
	}

	tmp = get_object();
	tmp->arch = op->arch;
	/* Copy without putting it on active list */
	copy_object(head, tmp, 1);
	tmp->type = BASE_INFO;
	tmp->speed_left = tmp->speed;
	/* Ensure this object will not be active in any way */
	tmp->speed = 0.0f;
	tmp->face = base_info_archetype->clone.face;
	SET_FLAG(tmp, FLAG_NO_DROP);
	CLEAR_FLAG(tmp, FLAG_ANIMATE);
	CLEAR_FLAG(tmp, FLAG_FRIENDLY);
	CLEAR_FLAG(tmp, FLAG_ALIVE);
	CLEAR_FLAG(tmp, FLAG_MONSTER);
	/* And put it in the mob */
	insert_ob_in_ob(tmp, head);

	/* Store position (for returning home after aggro is lost...) */
	tmp->x = op->x;
	tmp->y = op->y;
	FREE_AND_ADD_REF_HASH(tmp->slaying, op->map->path);

	return tmp;
}

/**
 * Find base info object in monster.
 * @param op Monster object.
 * @return Pointer to the base info if found, NULL otherwise. */
object *find_base_info_object(object *op)
{
	object *tmp;

	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		if (tmp->type == BASE_INFO)
		{
			return tmp;
		}
	}

	return NULL;
}

/**
 * Set the movement speed of a monster.
 * 1/5 = mob is slowed (by magic)
 * 2/5 = normal mob speed - moving normal
 * 3/5 = mob is moving fast
 * 4/5 = mov is running/attack speed
 * 5/5 = mob is hasted and moving full speed
 * @param op Monster.
 * @param index Index. */
void set_mobile_speed(object *op, int index)
{
	object *base;
	float speed, tmp;

	base = insert_base_info_object(op);

	speed = base->speed_left;

	tmp = op->speed;

	if (index)
	{
		op->speed = speed * index;
	}
	/* We will generate the speed by setting of the monster */
	else
	{
		/* If not slowed... */
		if (!QUERY_FLAG(op, FLAG_SLOW_MOVE))
		{
			speed += base->speed_left;
		}

		/* Valid enemy - monster is fighting! */
		if (OBJECT_VALID(op->enemy, op->enemy_count))
		{
			speed += base->speed_left * 2;
		}

		op->speed = speed;
	}

	/* Update speed if needed */
	if ((tmp && !op->speed) || (!tmp && op->speed))
	{
		update_ob_speed(op);
	}
}
