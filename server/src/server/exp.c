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
 * Experience management. */

#include <global.h>

/**
 * Experience needed for each level.
 *
 * Around level 11 you need to kill 38 + (2 * (your_level - 11)) yellow
 * monsters with a base exp of 125 to level up. */
uint64 new_levels[MAXLEVEL + 2] =
{
	0,          0,          1500,       4000,       8000,
	16000,      32000,      64000,      125000,     250000,
	500000,     1100000,    2300000,    3600000,    5000000,
	6500000,    8100000,    9800000,    11600000,   13500000,
	15500000,   17600000,   19800000,   22100000,   24500000,
	27000000,   29600000,   32300000,   35100000,   38000000,
	41000000,   44100000,   47300000,   50600000,   54000000,
	57500000,   61100000,   64800000,   68600000,   72500000,
	76500000,   80600000,   84800000,   89100000,   93500000,
	98000000,   102600000,  107300000,  112100000,  117000000,
	122000000,  127100000,  132300000,  137600000,  143000000,
	148500000,  154100000,  159800000,  165600000,  171500000,
	177500000,  183600000,  189800000,  196100000,  202500000,
	209000000,  215600000,  222300000,  229100000,  236000000,
	243000000,  250100000,  257300000,  264600000,  272000000,
	280200000,  294800000,  310200000,  326300000,  343200000,
	361000000,  379700000,  399300000,  419900000,  441500000,
	464200000,  488100000,  513100000,  539400000,  567000000,
	596000000,  626400000,  658300000,  691900000,  727100000,
	764100000,  802900000,  843700000,  886500000,  931500000,
	978700000,  1028200000, 1080300000, 1134900000, 1192300000,
	1252500000, 1315800000, 1382200000, 1451900000, 1525100000,
	2100000000LLU, 4200000000LLU, 8400000000LLU, 16800000000LLU, 33600000000LLU,
	67200000000LLU, 134400000000LLU
};

/**
 * Level colors. */
_level_color level_color[201] =
{
	{-2, -1, 0, 1, 2, 3},
	{-1, 0, 1, 2, 3, 4},
	{0, 1, 2, 3, 4, 5},
	{1, 2, 3, 4, 5, 6},
	{2, 3, 4, 5, 6, 7},
	{3, 4, 5, 6, 7, 8},
	{4, 5, 6, 7, 8, 9},
	{5, 6, 7, 8, 9, 10},
	{6, 7, 8, 9, 10, 11},
	{7, 8, 9, 10, 11, 12},
	{7, 9, 10, 11, 12, 14},
	{8, 9, 11, 12, 13, 15},
	{9, 10, 12, 13, 14, 16},
	{9, 11, 13, 14, 15, 17},
	{10, 11, 14, 15, 16, 18},
	{11, 12, 15, 16, 17, 19},
	{11, 13, 16, 17, 18, 20},
	{12, 14, 17, 18, 19, 21},
	{13, 15, 18, 19, 20, 22},
	{14, 16, 19, 20, 21, 23},
	{14, 17, 20, 21, 22, 24},
	{15, 17, 21, 22, 24, 26},
	{16, 18, 22, 23, 25, 27},
	{16, 19, 23, 24, 26, 28},
	{17, 19, 24, 25, 27, 30},
	{18, 20, 25, 26, 28, 31},
	{19, 21, 26, 27, 29, 32},
	{19, 22, 27, 28, 30, 33},
	{20, 23, 28, 29, 31, 35},
	{21, 24, 29, 30, 32, 36},
	{22, 25, 30, 31, 33, 37},
	{22, 25, 31, 32, 34, 38},
	{23, 26, 32, 33, 35, 39},
	{24, 27, 32, 35, 37, 41},
	{25, 28, 33, 36, 38, 42},
	{25, 28, 34, 37, 39, 43},
	{26, 29, 35, 38, 40, 44},
	{27, 30, 36, 39, 41, 45},
	{28, 31, 37, 40, 42, 46},
	{28, 32, 38, 41, 44, 48},
	{29, 33, 39, 42, 45, 49},
	{30, 34, 40, 43, 46, 50},
	{30, 34, 41, 44, 47, 52},
	{31, 35, 42, 45, 48, 53},
	{32, 36, 43, 46, 49, 54},
	{33, 37, 44, 47, 50, 55},
	{33, 37, 45, 48, 51, 57},
	{34, 38, 46, 49, 52, 58},
	{35, 39, 47, 50, 53, 59},
	{36, 40, 48, 51, 54, 60},
	{36, 41, 49, 52, 55, 61},
	{37, 42, 50, 53, 56, 62},
	{38, 43, 51, 54, 57, 63},
	{38, 43, 52, 55, 58, 65},
	{39, 44, 53, 56, 59, 66},
	{40, 45, 54, 57, 60, 67},
	{41, 46, 55, 58, 61, 68},
	{41, 47, 56, 59, 63, 70},
	{42, 48, 57, 60, 64, 71},
	{43, 49, 58, 61, 65, 72},
	{44, 50, 59, 62, 66, 73},
	{44, 50, 60, 63, 67, 75},
	{45, 51, 61, 64, 68, 76},
	{46, 52, 62, 65, 69, 77},
	{47, 53, 63, 66, 70, 78},
	{47, 53, 64, 67, 71, 79},
	{48, 54, 64, 69, 73, 81},
	{49, 55, 65, 70, 74, 82},
	{50, 56, 66, 71, 75, 83},
	{50, 56, 67, 72, 76, 84},
	{51, 57, 68, 73, 77, 85},
	{52, 58, 69, 74, 78, 86},
	{53, 59, 70, 75, 79, 87},
	{53, 60, 71, 76, 80, 89},
	{54, 61, 72, 77, 81, 90},
	{55, 62, 73, 78, 82, 91},
	{56, 63, 74, 79, 83, 92},
	{56, 63, 75, 80, 85, 94},
	{57, 64, 76, 81, 86, 95},
	{58, 65, 77, 82, 87, 96},
	{59, 66, 78, 83, 88, 97},
	{59, 67, 79, 84, 89, 99},
	{60, 68, 80, 85, 90, 100},
	{61, 69, 81, 86, 91, 101},
	{62, 70, 82, 87, 92, 102},
	{62, 70, 83, 88, 93, 103},
	{63, 71, 84, 89, 94, 104},
	{64, 72, 85, 90, 95, 105},
	{65, 73, 86, 91, 96, 106},
	{65, 73, 87, 92, 97, 108},
	{66, 74, 88, 93, 98, 109},
	{67, 75, 89, 94, 99, 110},
	{68, 76, 90, 95, 100, 111},
	{69, 77, 91, 96, 101, 112},
	{69, 78, 92, 97, 103, 114},
	{70, 79, 93, 98, 104, 115},
	{71, 80, 94, 99, 105, 116},
	{72, 81, 95, 100, 106, 117},
	{72, 81, 96, 101, 107, 119},
	{73, 82, 96, 103, 109, 120},
	{74, 83, 97, 104, 110, 121},
	{75, 84, 98, 105, 111, 122},
	{75, 84, 99, 106, 112, 124},
	{76, 85, 100, 107, 113, 125},
	{77, 86, 101, 108, 114, 126},
	{78, 87, 102, 109, 115, 127},
	{79, 88, 103, 110, 116, 128},
	{79, 89, 104, 111, 117, 129},
	{80, 90, 105, 112, 118, 130},
	{81, 91, 106, 113, 119, 131},
	{82, 92, 107, 114, 120, 132},
	{82, 92, 108, 115, 121, 134},
	{83, 93, 109, 116, 122, 135},
	{84, 94, 110, 117, 123, 136},
	{85, 95, 111, 118, 124, 137},
	{86, 96, 112, 119, 125, 138},
	{86, 96, 113, 120, 126, 140},
	{87, 97, 114, 121, 127, 141},
	{88, 98, 115, 122, 128, 142},
	{89, 99, 116, 123, 129, 143},
	{90, 100, 117, 124, 130, 144},
	{90, 101, 118, 125, 132, 146},
	{91, 102, 119, 126, 133, 147},
	{92, 103, 120, 127, 134, 148},
	{93, 104, 121, 128, 135, 149},
	{94, 105, 122, 129, 136, 150},
	{94, 105, 123, 130, 137, 151},
	{95, 106, 124, 131, 138, 152},
	{96, 107, 125, 132, 139, 153},
	{97, 108, 126, 133, 140, 154},
	{97, 109, 127, 134, 141, 156},
	{98, 110, 128, 135, 142, 157},
	{99, 110, 128, 137, 144, 158},
	{100, 111, 129, 138, 145, 159},
	{101, 112, 130, 139, 146, 160},
	{101, 113, 131, 140, 147, 162},
	{102, 114, 132, 141, 148, 163},
	{103, 115, 133, 142, 149, 164},
	{104, 116, 134, 143, 150, 165},
	{105, 117, 135, 144, 151, 166},
	{106, 118, 136, 145, 152, 167},
	{106, 118, 137, 146, 153, 169},
	{107, 119, 138, 147, 154, 170},
	{108, 120, 139, 148, 155, 171},
	{109, 121, 140, 149, 156, 172},
	{110, 122, 141, 150, 157, 173},
	{110, 122, 142, 151, 159, 175},
	{111, 123, 143, 152, 160, 176},
	{112, 124, 144, 153, 161, 177},
	{113, 125, 145, 154, 162, 178},
	{114, 126, 146, 155, 163, 179},
	{114, 127, 147, 156, 164, 180},
	{115, 128, 148, 157, 165, 181},
	{116, 129, 149, 158, 166, 182},
	{117, 130, 150, 159, 167, 183},
	{118, 131, 151, 160, 168, 184},
	{119, 132, 152, 161, 169, 185},
	{119, 132, 153, 162, 170, 187},
	{120, 133, 154, 163, 171, 188},
	{121, 134, 155, 164, 172, 189},
	{122, 135, 156, 165, 173, 190},
	{123, 136, 157, 166, 174, 191},
	{123, 137, 158, 167, 175, 193},
	{124, 138, 159, 168, 176, 194},
	{125, 139, 160, 169, 177, 195},
	{126, 139, 160, 171, 179, 196},
	{127, 140, 161, 172, 180, 197},
	{128, 141, 162, 173, 181, 198},
	{128, 142, 163, 174, 182, 200},
	{129, 143, 164, 175, 183, 201},
	{130, 144, 165, 176, 184, 202},
	{131, 145, 166, 177, 185, 203},
	{132, 146, 167, 178, 186, 204},
	{133, 147, 168, 179, 187, 205},
	{133, 147, 169, 180, 189, 207},
	{134, 148, 170, 181, 190, 208},
	{135, 149, 171, 182, 191, 209},
	{136, 150, 172, 183, 192, 210},
	{137, 151, 173, 184, 193, 211},
	{138, 152, 174, 185, 194, 212},
	{139, 153, 175, 186, 195, 213},
	{139, 153, 176, 187, 196, 214},
	{140, 154, 177, 188, 197, 215},
	{141, 155, 178, 189, 198, 216},
	{142, 156, 179, 190, 199, 217},
	{143, 157, 180, 191, 200, 218},
	{144, 158, 181, 192, 201, 219},
	{144, 159, 182, 193, 202, 221},
	{145, 160, 183, 194, 203, 222},
	{146, 161, 184, 195, 204, 223},
	{147, 162, 185, 196, 205, 224},
	{148, 163, 186, 197, 206, 225},
	{149, 164, 187, 198, 207, 226},
	{150, 165, 188, 199, 208, 227},
	{150, 165, 189, 200, 209, 229},
	{151, 166, 190, 201, 210, 230},
	{152, 167, 191, 202, 211, 231},
	{153, 168, 192, 203, 212, 232},
	{154, 169, 192, 205, 214, 233},
	{155, 170, 193, 206, 215, 234},
	{156, 171, 194, 207, 216, 235}
};

/** Maximum experience. */
#define MAX_EXPERIENCE new_levels[MAXLEVEL]

#define MAX_EXP_IN_OBJ new_levels[MAXLEVEL] / (MAX_EXP_CAT - 1)

/**
 * Calculates how much experience is needed for a player to become the
 * given level.
 * @param level Level to become.
 * @param expmul Experience multiplicator.
 * @return The experience needed.
 * @todo Remove, since the param expmul seems to always be passed as
 * '1.0'? */
uint64 level_exp(int level, double expmul)
{
	return (uint64) (expmul * (double) new_levels[level]);
}

/**
 * Add experience to player.
 * @param op The player.
 * @param exp How much experience to add (or, in case the value being
 * negative, subtract).
 * @param skill_nr Skill ID.
 * @return 0 on failure, experience gained on success. */
sint64 add_exp(object *op, sint64 exp, int skill_nr)
{
	object *exp_ob = NULL, *exp_skill = NULL;
	sint64 limit = 0;

	/* Sanity check */
	if (!op)
	{
		LOG(llevBug, "BUG: add_exp(): Called for NULL object.\n");
		return 0;
	}

	/* No exp gain for monsters */
	if (op->type != PLAYER)
	{
		return 0;
	}

	if (skill_nr == CHOSEN_SKILL_NO)
	{
		LOG(llevDebug, "TODO: add_exp(): called for %s with exp %d. CHOSEN_SKILL_NO set. TODO: select skill.\n", query_name(op, NULL), exp);
		return 0;
	}

	/* Now we grab the skill experience object from the player's shortcut
	 * pointer array. */
	exp_skill = CONTR(op)->skill_ptr[skill_nr];

	/* Sanity */
	if (!exp_skill)
	{
		LOG(llevDebug, "DEBUG: add_exp(): called for %s with skill nr %d / %d exp - object has not this skill.\n", query_name(op, NULL), skill_nr, exp);
		return 0;
	}

	/* If we are full in this skill, there nothing is to do. */
	if (exp_skill->level >= MAXLEVEL)
	{
		return 0;
	}

	/* Mark the skills for update */
	CONTR(op)->update_skills = 1;
	exp_ob = exp_skill->exp_obj;

	if (!exp_ob)
	{
		LOG(llevBug, "BUG: add_exp() skill: %s - no exp_ob found!!\n", query_name(exp_skill, NULL));
		return 0;
	}

	/* General adjustments for playbalance */
	limit = (new_levels[exp_skill->level + 1] - new_levels[exp_skill->level]) / 4;

	if (exp > limit)
	{
		exp = limit;
	}

	/* First we see what we can add to our skill */
	exp = adjust_exp(op, exp_skill, exp);

	/* adjust_exp() has adjusted the skill and all exp_obj and player
	 * experience. Now let's check for level up in all categories. */
	player_lvl_adj(op, exp_skill);
	player_lvl_adj(op, exp_ob);
	player_lvl_adj(op, NULL);

	if (op->exp_obj)
	{
		op->exp_obj = NULL;
	}

	/* Notify the player of the exp gain */
	new_draw_info_format(NDI_UNIQUE, op, "You got %"FMT64" exp in skill %s.", exp, skills[skill_nr].name);

	/* The real experience we have added to our skill */
	return exp;
}

/**
 * For the new experience system. We are concerned with whether the
 * player gets more hp, sp and new levels.
 *
 * Will tell the player about changed levels.
 * @param who Player.
 * @param op What we are checking to gain the level (eg, skill). */
void player_lvl_adj(object *who, object *op)
{
	char buf[MAX_BUF];

	/* When rolling stats */
	if (!op)
	{
		op = who;
	}

	/* No exp gain for indirect skills */
	if (op->type == SKILL && !op->last_eat)
	{
		LOG(llevBug,"BUG: player_lvl_adj() called for indirect skill %s (who: %s)\n", query_name(op, NULL), who == NULL ? "<null>" : query_name(who, NULL));
		return;
	}

	if (op->level < MAXLEVEL && op->stats.exp >= (sint64) level_exp(op->level + 1, 1.0))
	{
		op->level++;

		/* Show the player some effects. */
		if (op->type == SKILL && who && who->type == PLAYER && who->map)
		{
			object *effect_ob;

			play_sound_player_only(CONTR(who), SOUND_LEVEL_UP, NULL, 0, 0, 0, 0);

			if (level_up_arch)
			{
				/* Prepare effect */
				effect_ob = arch_to_object(level_up_arch);
				effect_ob->map = who->map;
				effect_ob->x = who->x;
				effect_ob->y = who->y;

				if (!insert_ob_in_map(effect_ob, effect_ob->map, NULL, INS_NO_MERGE | INS_NO_WALK_ON))
				{
					/* Something is wrong - remove object */
					if (!QUERY_FLAG(effect_ob, FLAG_REMOVED))
					{
						remove_ob(effect_ob);
					}
				}
			}
		}

		if (who && who->type == PLAYER && op->type != EXPERIENCE && op->type != SKILL && who->level > 1)
		{
			if (who->level > 4)
			{
				CONTR(who)->levhp[who->level] = (char) rndm(1, who->arch->clone.stats.maxhp);
			}
			else if (who->level > 2)
			{
				CONTR(who)->levhp[who->level] = (char) rndm(1, who->arch->clone.stats.maxhp / 2) + (who->arch->clone.stats.maxhp / 2);
			}
			else
			{
				CONTR(who)->levhp[who->level] = (char) who->arch->clone.stats.maxhp;
			}
		}

		if (op->level > 1 && op->type == EXPERIENCE)
		{
			if (who && who->type == PLAYER)
			{
				/* Mana */
				if (op->stats.Pow)
				{
					if (op->level > 4)
					{
						CONTR(who)->levsp[op->level] = (char) rndm(1, who->arch->clone.stats.maxsp);
					}
					else
					{
						CONTR(who)->levsp[op->level] = (char) who->arch->clone.stats.maxsp;
					}
				}
				/* Grace */
				else if (op->stats.Wis)
				{
					if (op->level > 4)
					{
						CONTR(who)->levgrace[op->level] = (char) rndm(1, who->arch->clone.stats.maxgrace);
					}
					else
					{
						CONTR(who)->levgrace[op->level] = (char) who->arch->clone.stats.maxgrace;
					}
				}
			}

			if (who)
			{
				snprintf(buf, sizeof(buf), "You are now level %d in %s based skills.", op->level, op->name);
				new_draw_info(NDI_UNIQUE | NDI_RED, who, buf);
			}
		}
		else if (op->level > 1 && op->type == SKILL)
		{
			if (who)
			{
				/* If we leveled up praying or wizardry, we need to send a spell list update */
				if (op->stats.sp == SK_PRAYING || op->stats.sp == SK_SPELL_CASTING)
				{
					send_spelllist_cmd(who, NULL, SPLIST_MODE_UPDATE);
				}

				snprintf(buf, sizeof(buf), "You are now level %d in the skill %s.", op->level, op->name);
				new_draw_info(NDI_UNIQUE | NDI_RED, who, buf);
			}
		}
		else
		{
			if (who)
			{
				snprintf(buf, sizeof(buf), "You are now level %d.", op->level);
				new_draw_info(NDI_UNIQUE | NDI_RED, who, buf);
			}
		}

		if (who)
		{
			fix_player(who);
		}

		/* To increase more levels. */
		player_lvl_adj(who, op);
	}
	else if (op->level > 1 && op->stats.exp < (sint64) level_exp(op->level, 1.0))
	{
		op->level--;

		if (who)
		{
			fix_player(who);
		}

		if (op->type == EXPERIENCE)
		{
			if (who)
			{
				snprintf(buf, sizeof(buf), "-You are now level %d in %s based skills.", op->level, op->name);
				new_draw_info(NDI_UNIQUE | NDI_RED, who, buf);
			}
		}
		else if (op->type == SKILL)
		{
			if (who)
			{
				snprintf(buf, sizeof(buf), "-You are now level %d in the skill %s.", op->level, op->name);
				new_draw_info(NDI_UNIQUE | NDI_RED, who, buf);
			}
		}
		else
		{
			if (who)
			{
				snprintf(buf, sizeof(buf), "-You are now level %d.", op->level);
				new_draw_info(NDI_UNIQUE | NDI_RED, who, buf);
			}
		}

		/* To decrease more levels. */
		player_lvl_adj(who, op);
	}
}

/**
 * Make sure that we don't exceed max or min set on experience.
 *
 * This function also adjusts skill category experience, and overall
 * player experience based on the skill category that has the most
 * experience.
 * @param pl Player.
 * @param op Skill object.
 * @param exp Experience.
 * @return 0 on failure, experience we added otherwise. */
sint64 adjust_exp(object *pl, object *op, sint64 exp)
{
	object *tmp;
	int i, sk_nr;
	sint64 sk_exp, pl_exp;

	/* Be sure this is a skill object from a player. */
	if (op->type != SKILL || !pl || pl->type != PLAYER)
	{
		LOG(llevBug, "BUG: adjust_exp() - called for non player or non skill: skill: %s -> player: %s\n", query_name(op, NULL), query_name(pl, NULL));
		return 0;
	}

	/* Add or sub the exp and cap it. Must be >= 0 and <= MAX_EXPERIENCE */
	op->stats.exp += exp;

	if (op->stats.exp < 0)
	{
		exp -= op->stats.exp;
		op->stats.exp = 0;
	}

	if (op->stats.exp > (sint64) MAX_EXPERIENCE)
	{
		exp = exp - (op->stats.exp - MAX_EXPERIENCE);
		op->stats.exp = MAX_EXPERIENCE;
	}

	/* Now we collect the experience of all skills which are in the same
	 * experience object category. */
	sk_nr = skills[op->stats.sp].category;
	sk_exp = 0;

	for (tmp = pl->inv; tmp; tmp = tmp->below)
	{
		if (tmp->type == SKILL && skills[tmp->stats.sp].category == sk_nr)
		{
			if (tmp->stats.exp > sk_exp)
			{
				sk_exp = tmp->stats.exp;
			}
		}
	}

	/* Set the experience of the experience object to our best skill of
	 * this group. */
	op->exp_obj->stats.exp = sk_exp;

	pl_exp = 0;

	for (i = 0; i < MAX_EXP_CAT - 1; i++)
	{
		if (CONTR(pl)->last_skill_ob[i]->stats.exp > pl_exp)
		{
			pl_exp = CONTR(pl)->last_skill_ob[i]->stats.exp;
		}
	}

	/* Set our player exp to highest category experience. */
	pl->stats.exp = pl_exp;

	return exp;
}

/**
 * Applies death experience penalty.
 *
 * We never lose a level, just percent of the experience we gained for
 * the next level.
 * @param op Victim of the penalty. Must not be NULL. */
void apply_death_exp_penalty(object *op)
{
	object *tmp;
	float loss_p;
	sint64 level_exp, loss_exp;

	/* Mark the skills for update */
	CONTR(op)->update_skills = 1;

	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		/* Only adjust skills with level and a positive exp value,
		 * negative exp has special meaning. */
		if (tmp->type == SKILL && tmp->level && tmp->last_eat == 1)
		{
			/* Ceck there is experience we can drain. */
			level_exp = tmp->stats.exp - new_levels[tmp->level];

			/* Sanity check */
			if (level_exp < 0)
			{
				LOG(llevBug, "apply_death_exp_penalty(): Skill %s (%d %d) for player %s -> less exp as level need!\n", query_name(tmp, NULL), tmp->level, tmp->stats.exp, query_name(op, NULL));
			}

			if (!level_exp)
			{
				continue;
			}

			if (tmp->level < 2)
			{
				loss_exp = level_exp - (int) ((float) level_exp * 0.9);
			}
			else if (tmp->level < 3)
			{
				loss_exp = level_exp - (int) ((float) level_exp * 0.85);
			}
			else
			{
				loss_p = 0.927f - (((float) tmp->level / 5.0f) * 0.00337f);
				loss_exp = (new_levels[tmp->level + 1] - new_levels[tmp->level]) - (int) ((float) (new_levels[tmp->level + 1] - new_levels[tmp->level]) * loss_p);
			}

			if (loss_exp < 0)
			{
				loss_exp = 0;
			}

			if (loss_exp > level_exp)
			{
				loss_exp = level_exp;
			}

			if (loss_exp > 0)
			{
				adjust_exp(op, tmp, -loss_exp);
				player_lvl_adj(op, tmp);
			}
		}
	}

	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		/* Adjust experience object levels. */
		if (tmp->type == EXPERIENCE && tmp->stats.exp)
		{
			player_lvl_adj(op, tmp);
		}
	}

	/* Adjust the player level. */
	player_lvl_adj(op, NULL);
}

/**
 * Calculate level difference.
 *
 * We will get a bonus or malus value here, unless both levels match.
 *
 * Yellow does not always mean same level, but in equal range.
 *
 * Experience multiplication based on target color range:
 *
 * - <b>yellow</b>: Between 0.8 and 1.1 (80% - 110%).
 * - <b>blue</b>: Between 0.4 and 0.6 (40% - 60%).
 * - <b>green</b>: Between 0.25 and 0.3 (25% - 30%).
 * - <b>orange</b>: Between 1.2 and 1.4 (120% - 140%).
 * - <b>red, purple</b>: 1.4 + 0.1% per level.
 *
 * If the target is in yellow range, the experience multiplication is
 * between 0.8 and 1.1 (80% - 110%).
 *
 * If the target is in blue range, the experience
 * @param who_lvl Player.
 * @param op_lvl Victim.
 * @return Level difference. */
float calc_level_difference(int who_lvl, int op_lvl)
{
	int r;
	float v, tmp = 1.0f;

	/* Sanity checks */
	if (who_lvl < 0 || who_lvl > 200 || op_lvl < 0 || op_lvl > 200)
	{
		LOG(llevBug, "BUG: calc_level_difference(): Level out of range! (%d - %d)\n", who_lvl, op_lvl);
		return 0.0f;
	}

	/* Grey, no experience */
	if (op_lvl < level_color[who_lvl].green)
	{
		return 0.0f;
	}

	/* Yellow, blue or green */
	if (who_lvl > op_lvl)
	{
		if (op_lvl >= level_color[who_lvl].yellow)
		{
			r = who_lvl - level_color[who_lvl].yellow;

			if (r < 1)
			{
				r = 1;
			}

			v = 0.2f / (float) r;
			tmp = 1.0f - (v * (float) (who_lvl - op_lvl));
		}
		else if (op_lvl >= level_color[who_lvl].blue)
		{
			r = level_color[who_lvl].yellow - level_color[who_lvl].blue;

			if (r < 1)
			{
				r = 1;
			}

			v = 0.3f / (float) r;
			tmp = 0.4f + (v * (float) (op_lvl - level_color[who_lvl].blue + 1));
		}
		/* Green */
		else
		{
			r = level_color[who_lvl].blue - level_color[who_lvl].green;

			if (r < 1)
			{
				r = 1;
			}

			v = 0.05f / (float) r;
			tmp = 0.25f + (v * (float) (op_lvl - level_color[who_lvl].green + 1));
		}
	}
	/* Yellow, orange, red, purple */
	else if (who_lvl < op_lvl)
	{
		/* Still yellow */
		if (op_lvl < level_color[who_lvl].orange)
		{
			r = level_color[who_lvl].orange - who_lvl - 1;

			if (r < 1)
			{
				r = 1;
			}

			v = 0.1f / (float) r;
			tmp = 1.0f + (v * (float) (op_lvl - who_lvl));

		}
		/* Orange */
		else if (op_lvl < level_color[who_lvl].red)
		{
			r = level_color[who_lvl].red - who_lvl - 1;

			if (r < 1)
			{
				r = 1;
			}

			v = 0.2f / (float) r;
			tmp = 1.2f + (v * (float) (op_lvl - who_lvl));
		}
		/* Red or purple */
		else
		{
			r = (op_lvl + 1) - level_color[who_lvl].red;
			v = 0.1f * (float) r;
			tmp = 1.4f + v;
		}
	}

	return tmp;
}

/**
 * Calculate total experience of player, based on all skills they know.
 * @param op Player.
 * @return The total experience. */
uint64 calculate_total_exp(object *op)
{
	uint64 exp = 0;
	int i;

	for (i = 0; i < NROFSKILLS; i++)
	{
		if (CONTR(op)->skill_ptr[i])
		{
			exp += CONTR(op)->skill_ptr[i]->stats.exp;
		}
	}

	return exp;
}
