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

/* Initial coding: 6 Sep 1994, Nick Williams (njw@cs.city.ac.uk) */

/* Generalized code + added hiding and lockpicking skills, */
/* March 3, 1995, brian thomas (thomas@astro.psu.edu) */

/* Added more skills, fixed bug in stealing code */
/* April 21, 1995, brian thomas (thomas@astro.psu.edu) */

/* Added more skills, fixed bugs, see skills.h */
/* May/June, 1995, brian thomas (thomas@astro.psu.edu) */

/* July 95 Code re-vamped. Now we add the experience objects, all
 * player activities which gain experience will bbe through the use
 * of skillls. Thus, I added hand_weapons, missile_weapons, and
 * remove_traps skills -b.t.
 */

/* Aug 95 - Added more skills (disarm traps, spellcasting, praying).
 * Also, hand_weapons is now "melee_weapons". b.t.
 */

/* Oct 95 - changed the praying skill to accomodate MULTIPLE_GODS
 * hack - b.t.
 */

/* Dec 95 - modified the literacy and inscription (writing) skills. b.t.
 */

/* Mar 96 - modified the stealing skill. Objects with type FLESH or
 * w/o a type cannot be stolen by players. b.t.
 */

/* Sept 96 - changed parsing of params through use_skill command, also
 * added in throw skill -b.t. */

/* Oct 96 - altered hiding and stealing code for playbalance. -b.t. */

/* Sept 97 - yet another alteration to the stealing code. Lets allow
 * multiple stealing, after having alerted the victim. But only subsequent
 * steals made while we are unseen (in dark, invisible, hidden) will have
 * any chance of success. Also, on each subsequent attempt, we raise the
 * wisdom of the npc a bit, which makes it ultimately possible for the
 * npc to detect the theif, regardless of the situation. -b.t. */

#include <global.h>
#ifndef __CEXTRACT__
#include <sproto.h>
#endif
#include <book.h>
#include <sounds.h>

/*
 * When stealing: dependent on the intelligence/wisdom of whom you're
 * stealing from (op in attempt_steal), offset by your dexterity and
 * skill at stealing. They may notice your attempt, whether successful
 * or not.
 */

int attempt_steal(object* op, object* who)
{
	object *success = NULL, *tmp = NULL, *next;
	int roll = 0, chance = 0, stats_value = get_weighted_skill_stats(who) * 3;
	int victim_lvl = op->level * 3, thief_lvl = SK_level(who) * 10;

	/* if the victim is aware of a thief in the area (FLAG_NO_STEAL set on them)
	 * they will try to prevent stealing if they can. Only unseen theives will
	 * have much chance of success.  */
	if (op->type!=PLAYER && QUERY_FLAG(op,FLAG_NO_STEAL))
	{
		if (1)
		{
			/* add here a distance/can see function!! */
			/* TODO: should probaly call set_npc_enemy() here instead */
			npc_call_help(op);
			CLEAR_FLAG(op, FLAG_UNAGGRESSIVE);
			new_draw_info(NDI_UNIQUE, 0, who, "Your attempt is prevented!");
			return 0;
		}
		/* help npc to detect thief next time by raising its wisdom */
		else
			op->stats.Wis += (op->stats.Int / 5) + 1;
	}

	/* Ok then, go thru their inventory, stealing */
	for (tmp = op->inv; tmp != NULL; tmp = next)
	{
		next = tmp->below;

		/* you can't steal worn items, starting items, wiz stuff,
		 * innate abilities, or items w/o a type. Generally
		 * speaking, the invisibility flag prevents experience or
		 * abilities from being stolen since these types are currently
		 * always invisible objects. I was implicit here so as to prevent
		 * future possible problems. -b.t.
		 * Flesh items generated w/ fix_flesh_item should have FLAG_NO_STEAL
		 * already  -b.t. */
		if (QUERY_FLAG(tmp, FLAG_WAS_WIZ) || QUERY_FLAG(tmp, FLAG_APPLIED) || !(tmp->type) || tmp->type == EXPERIENCE || tmp->type == ABILITY || QUERY_FLAG(tmp, FLAG_STARTEQUIP) || QUERY_FLAG(tmp, FLAG_NO_STEAL) || IS_SYS_INVISIBLE(tmp))
			continue;

		/* Okay, try stealing this item. Dependent on dexterity of thief,
		 * skill level, see the adj_stealroll fctn for more detail. */
		/* weighted 1-100 */
		roll = die_roll(2, 100, who, PREFER_LOW) / 2;

		if ((chance = adj_stealchance(who, op, (stats_value + thief_lvl - victim_lvl))) == -1)
			return 0;
		else if (roll < chance)
		{
			if (op->type == PLAYER)
				esrv_del_item(CONTR(op), tmp->count,tmp->env);

			pick_up(who, tmp);
			if (can_pick(who,tmp))
			{
				/* for players, play_sound: steals item */
				success = tmp;
				CLEAR_FLAG(tmp, FLAG_INV_LOCKED);
			}
			break;
		}
	}

	/* If you arent high enough level, you might get something BUT
	 * the victim will notice your stealing attempt. Ditto if you
	 * attempt to steal something heavy off them, they're bound to notice  */
	if ((roll >= SK_level(who)) || !chance || (tmp && tmp->weight > (250 * (random_roll(0, stats_value + thief_lvl - 1, who, PREFER_LOW)))))
	{
		/* victim figures out where the thief is! */
		if (who->hide)
			make_visible(who);

		if (op->type != PLAYER)
		{
			/* The unaggressives look after themselves 8) */
			if (who->type == PLAYER)
			{
				/* TODO: should probaly call set_npc_enemy() here instead */
				npc_call_help(op);
				new_draw_info_format(NDI_UNIQUE, 0, who, "%s notices your attempted pilfering!", query_name(op, NULL));
			}
			CLEAR_FLAG(op, FLAG_UNAGGRESSIVE);
			/* all remaining npc items are guarded now. Set flag NO_STEAL
			 * on the victim. */
			SET_FLAG(op, FLAG_NO_STEAL);
		}
		/* stealing from another player */
		else
		{
			char buf[MAX_BUF];
			/* Notify the other player */
			if (success && who->stats.Int > random_roll(0, 19, op, PREFER_LOW))
				sprintf(buf, "Your %s is missing!", query_name(success, NULL));
			else
				sprintf(buf, "Your pack feels strangely lighter.");

			new_draw_info(NDI_UNIQUE, 0, op, buf);

			if (!success)
			{
				if (QUERY_FLAG(who, FLAG_IS_INVISIBLE))
					sprintf(buf, "You feel itchy fingers getting at your pack.");
				else
					sprintf(buf, "%s looks very shifty.", query_name(who, NULL));

				new_draw_info(NDI_UNIQUE, 0, op, buf);
			}
		}

		/* play_sound("stop! thief!"); kindofthing */
	}

	return success? 1 : 0;
}

/* adj_stealchance() - increased values indicate better attempts */
int adj_stealchance (object *op, object *victim, int roll)
{
	object *equip;
	int used_hands = 0;

	if (!op || !victim || !roll)
		return -1;

	/* ADJUSTMENTS */

	/* Its harder to steal from hostile beings! */
	if (!QUERY_FLAG(victim, FLAG_UNAGGRESSIVE))
		roll = roll / 2;

	/* Easier to steal from sleeping beings, or if the thief is
	  * unseen */

	if (QUERY_FLAG(victim, FLAG_SLEEP))
		roll = roll * 3;
	else if (QUERY_FLAG(op, FLAG_IS_INVISIBLE))
		roll = roll * 2;

	/* check stealing 'encumberance'. Having this equipment applied makes
	 * it quite a bit harder to steal. */
	for (equip = op->inv; equip; equip = equip->below)
	{
		if (equip->type == WEAPON && QUERY_FLAG(equip, FLAG_APPLIED))
		{
			roll -= equip->weight / 10000;
			used_hands++;
		}

		if (equip->type == BOW && QUERY_FLAG(equip, FLAG_APPLIED))
			roll -= equip->weight / 5000;

		if (equip->type == SHIELD && QUERY_FLAG(equip, FLAG_APPLIED))
		{
			roll -= equip->weight / 2000;
			used_hands++;
		}

		if (equip->type == ARMOUR && QUERY_FLAG(equip, FLAG_APPLIED))
			roll -= equip->weight / 5000;

		if (equip->type == GLOVES && QUERY_FLAG(equip, FLAG_APPLIED))
			roll -= equip->weight / 100;
	}

	if (roll < 0)
		roll = 0;

	if (op->type == PLAYER && used_hands >= 2)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "But you have no free hands to steal with!");
		roll = -1;
	}

	return roll;
}

int steal(object* op, int dir)
{
	object *tmp, *next;
	mapstruct *mt;

	int x = op->x + freearr_x[dir];
	int y = op->y + freearr_y[dir];

	if (dir == 0)
	{
		/* Can't steal from ourself! */
		return 0;
	}

	if (wall(op->map, x, y))
		return 0;

	if (!(mt = out_of_map(op->map, &x, &y)))
		return 0;

	/* Find the topmost object at this spot */
	for (tmp = get_map_ob(mt, x, y); tmp != NULL && tmp->above != NULL; tmp = tmp->above);

	/* For all the stacked objects at this point, attempt a steal */
	for (; tmp != NULL; tmp = next)
	{
		next = tmp->below;
		/* Minor hack--for multi square beings - make sure we get
		 * the 'head' coz 'tail' objects have no inventory! - b.t.  */
		if (tmp->head)
			tmp = tmp->head;

		if (tmp->type != PLAYER && !QUERY_FLAG(tmp, FLAG_MONSTER))
			continue;

		if (attempt_steal(tmp, op))
		{
			/* no xp for stealing from another player */
			if (tmp->type == PLAYER)
				return 0;
			else
				return calc_skill_exp(op, tmp);
		}
	}
	return 0;
}

/* Implementation by bt. (thomas@astro.psu.edu)
 * monster implementation 7-7-95 by bt. */
int pick_lock(object *pl, int dir)
{
	char buf[MAX_BUF];
	object *tmp;
	mapstruct *m;
	int x = pl->x + freearr_x[dir];
	int y = pl->y + freearr_y[dir];
	int success = 0;

	if (!dir)
		dir = pl->facing;

	/* For all the stacked objects at this point find a door*/
	sprintf(buf, "There is no lock there.");
	if (!(m = out_of_map(pl->map, &x, &y)))
	{
		new_draw_info(NDI_UNIQUE, 0, pl, buf);
		return 0;
	}

	for (tmp = get_map_ob(m, x, y); tmp; tmp = tmp->above)
	{
		if (!tmp)
			continue;

		switch (tmp->type)
		{
			case DOOR:
				if (!QUERY_FLAG(tmp, FLAG_NO_PASS))
					strcpy(buf, "The door has no lock!");
				else
				{
					if (attempt_pick_lock(tmp, pl))
					{
						success = 1;
						sprintf(buf, "You pick the lock.");
					}
					else
						sprintf(buf, "You fail to pick the lock.");
				}
				break;

			case LOCKED_DOOR:
				sprintf(buf, "You can't pick that lock!");
				break;

			default:
				break;
		}
	}

	new_draw_info(NDI_UNIQUE, 0, pl, buf);

	if (success)
		return calc_skill_exp(pl, NULL);
	else
		return 0;
}

int attempt_pick_lock(object *door, object *pl)
{
	int bonus = SK_level(pl);
	int difficulty= pl->map->difficulty ? pl->map->difficulty : 0;
	int dex = get_skill_stat1(pl) ? get_skill_stat1(pl) : 10;
	/* did we get anything? */
	int success = 0, number;

	/* If has can_pass set, then its not locked! */
	if (!QUERY_FLAG(door, FLAG_NO_PASS))
		return 0;

	/* Try to pick the lock on this item (doors only for now).
	 * Dependent on dexterity/skill SK_level of the player and
	 * the map level difficulty.  */
	number = (die_roll(2, 40, pl, PREFER_LOW) - 2) / 2;

	if (number < ((dex + bonus) - difficulty))
	{
		remove_door(door);
		success = 1;
	}
	/* set off any traps? */
	else if (door->inv && door->inv->type == RUNE)
		spring_trap(door->inv, pl);

	return success;
}

/* HIDE CODE. The user becomes undetectable (not just 'invisible') for
 * a short while (success and duration dependant on player SK_level,
 * dexterity, charisma, and map difficulty).
 * Players have a good chance of becoming 'unhidden' if they move
 * and like invisiblity will be come visible if they attack
 * Implemented by b.t. (thomas@astro.psu.edu)
 * July 7, 1995 - made hiding possible for monsters. -b.t. */
/* patched this to take terrain into consideration */
int hide(object *op)
{
	char buf[MAX_BUF];
	/* int level= SK_level(op);*/

	/* the preliminaries -- Can we really hide now? */
	/* this keeps monsters from using invisibilty spells and hiding */
	if (QUERY_FLAG(op, FLAG_SEE_INVISIBLE))
	{
		sprintf(buf, "You don't need to hide while invisible!");
		new_draw_info(NDI_UNIQUE, 0, op, buf);
		return 0;
	}
	else if (!op->hide && QUERY_FLAG(op, FLAG_IS_INVISIBLE) && op->type == PLAYER)
	{
		sprintf(buf, "Your attempt to hide breaks the invisibility spell!");
		new_draw_info(NDI_UNIQUE, 0, op, buf);
		make_visible(op);
		return 0;
	}

#if 0
	if (op->invisible > (50 * level))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You are as hidden as you can get.");
		return 0;
	}
#endif

	if (attempt_hide(op))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You hide in the shadows.");
		update_object(op, UP_OBJ_FACE);
		return calc_skill_exp(op, NULL);
	}

	new_draw_info(NDI_UNIQUE, 0, op, "You fail to conceal yourself.");
	return 0;
}

int attempt_hide(object *op)
{
	int level = SK_level(op);
	int success = 0, number, difficulty = op->map->difficulty;
	int dexterity = get_skill_stat1(op);
	int terrain = hideability(op);

	level = level > 5 ? level / 5 : 1;

	/* first things... no hiding next to a hostile monster */
	dexterity = dexterity ? dexterity : 15;

	/* not enough cover here */
	if (terrain < -10)
		return 0;

	/* Hiding success and duration dependant on SK_level,
	 * dexterity, map difficulty and terrain. */

	number = (die_roll(2, 25, op, PREFER_LOW) - 2) / 2;
	if (!stand_near_hostile(op) && number && (number < (dexterity + level + terrain - difficulty)))
	{
		success = 1;
		op->hide = 1;
	}

	return success;
}

/* stop_jump() - End of jump. Clear flags, restore the map, and
 * freeze the jumper a while to simulate the exhaustion
 * of jumping. */
static int stop_jump(object *pl, int dist, int spaces)
{
	(void) dist;
	(void) spaces;
#if 0
	int load = dist / (pl->speed * spaces);
#endif

	CLEAR_MULTI_FLAG(pl, FLAG_FLYING);
	insert_ob_in_map(pl, pl->map, pl, 0);

#if 0
	if (pl->type == PLAYER)
		draw_client_map(pl);

	pl->speed_left = (int) -FABS((load * 8) + 1);
#endif

	return 0;
}

static int attempt_jump(object *pl, int dir, int spaces)
{
	object *tmp;
	mapstruct *m;
	int i, xt, yt, exp = 0, dx = freearr_x[dir], dy = freearr_y[dir];

	/* Jump loop. Go through spaces opject wants to jump. Halt the
	 * jump if a wall or creature is in the way. We set FLAG_FLYING
	 * temporarily to allow player to aviod exits/archs that are not
	 * fly_on, fly_off. This will also prevent pickup of objects
	 * while jumping over them. */
	remove_ob(pl);

	if (check_walk_off(pl, NULL, MOVE_APPLY_VANISHED) != CHECK_WALK_OK)
		return 0;

	SET_MULTI_FLAG(pl, FLAG_FLYING);
	for (i = 0; i <= spaces; i++)
	{
		xt = pl->x + dx;
		yt = pl->y + dy;

		if (!(m = out_of_map(pl->map, &xt, &yt)))
		{
			(void) stop_jump(pl, i, spaces);
			/* no action, no exp */
			return 0;
		}

		for (tmp = get_map_ob(m, xt, yt); tmp; tmp = tmp->above)
		{
			/* Jump into wall */
			if (wall(tmp->map, xt, yt))
			{
				new_draw_info(NDI_UNIQUE, 0, pl, "Your jump is blocked.");
				(void) stop_jump(pl, i, spaces);
				return 0;
			}

			/* Jump into creature */
			if (QUERY_FLAG(tmp, FLAG_MONSTER) || tmp->type == PLAYER)
			{
				new_draw_info_format(NDI_UNIQUE, 0, pl, "You jump into%s%s.", tmp->type == PLAYER ? " " : " the ", tmp->name);

				/* pl makes an attack */
				if (tmp->type != PLAYER || (pl->type == PLAYER && CONTR(pl)->party_number == -1) || (pl->type == PLAYER && tmp->type == PLAYER && CONTR(pl)->party_number != CONTR(tmp)->party_number))
					exp = skill_attack(tmp, pl, pl->facing, "kicked");

				(void) stop_jump(pl, i, spaces);
				/* note that calc_skill_exp() is already called by skill_attack() */
				return exp;
			}

			/* If the space has fly on set (no matter what the space is),
			 * we should get the effects - after all, the player is
			 * effectively flying. */
			if (QUERY_FLAG(tmp, FLAG_FLY_ON))
			{
				pl->x += dx, pl->y += dy;
				(void) stop_jump(pl, i, spaces);
				return 0;
			}
		}

		pl->x += dx;
		pl->y += dy;
	}
	(void) stop_jump(pl, i, spaces);
	return 0;
}

/* jump() - this is both a new type of movement for player/monsters and
 * an attack as well. -b.t. */
int jump(object *pl, int dir)
{
	int spaces = 0,stats;
	int str = get_skill_stat1(pl);
	int dex = get_skill_stat2(pl);

	dex = dex ? dex : 15;
	str = str ? str : 10;

	stats = str * str * str * dex;

	/* don't want div by zero !! */
	if (pl->carrying != 0)
		spaces = (int) (stats / pl->carrying);
	/* pl has no objects - gets the far jump */
	else
		spaces = 2;

	if (spaces > 2)
		spaces = 2;
	else if (spaces == 0)
	{
		new_draw_info(NDI_UNIQUE, 0, pl, "You are carrying too much weight to jump.");
		return 0;
	}

	return attempt_jump(pl, dir, spaces);
}

/* skill_ident() - this code is supposed to allow players to identify
 * classes of objects with the various "auto-ident" skills. Player must
 * have unidentified objects of the right type in order for the skill
 * to work. While multiple classes of objects may be identified,
 * this code is kind of yucky -- it would be nice to make it a bit
 * more generalized. Right now, skill indices are embedded in this routine.
 * Returns amount of experience gained (on successful ident).
 * - b.t. (thomas@astro.psu.edu)  */
int skill_ident(object *pl)
{
	char buf[MAX_BUF];
	int success = 0;

	/* should'nt happen... */
	if (!pl->chosen_skill)
		return 0;

	/* only players will skill-identify */
	if (pl->type != PLAYER)
		return 0;

	sprintf(buf, "You look at the objects nearby...");
	new_draw_info(NDI_UNIQUE, 0, pl, buf);

	switch (pl->chosen_skill->stats.sp)
	{
		case SK_SMITH:
			success += do_skill_ident(pl, WEAPON) + do_skill_ident(pl, ARMOUR) + do_skill_ident(pl, BRACERS) + do_skill_ident(pl, CLOAK) + do_skill_ident(pl, BOOTS) + do_skill_ident(pl, SHIELD) + do_skill_ident(pl, GIRDLE) + do_skill_ident(pl, HELMET) + do_skill_ident(pl, GLOVES);
			break;

		case SK_BOWYER:
			success += do_skill_ident(pl, BOW) + do_skill_ident(pl, ARROW);
			break;

		case SK_ALCHEMY:
			success += do_skill_ident(pl, POTION) + do_skill_ident(pl, POISON) + do_skill_ident(pl, AMULET) + do_skill_ident(pl, CONTAINER) + do_skill_ident(pl, DRINK) + do_skill_ident(pl, INORGANIC);
			break;

		case SK_WOODSMAN:
			success += do_skill_ident(pl, FOOD) + do_skill_ident(pl, DRINK) + do_skill_ident(pl, FLESH);
			break;

		case SK_JEWELER:
			success += do_skill_ident(pl, GEM) + do_skill_ident(pl, TYPE_JEWEL) + do_skill_ident(pl, TYPE_NUGGET) + do_skill_ident(pl, RING);
			break;

		case SK_LITERACY:
			success += do_skill_ident(pl, SPELLBOOK) + do_skill_ident(pl, SCROLL) + do_skill_ident(pl, BOOK);
			break;

		case SK_THAUMATURGY:
			success += do_skill_ident(pl, WAND) + do_skill_ident(pl, ROD) + do_skill_ident(pl, HORN);
			break;

		case SK_DET_CURSE:
			success = do_skill_detect_curse(pl);
			if (success)
				new_draw_info(NDI_UNIQUE, 0, pl, "... and discover cursed items!");

			break;

		case SK_DET_MAGIC:
			success = do_skill_detect_magic(pl);
			if (success)
				new_draw_info(NDI_UNIQUE, 0, pl, "... and discover items imbued with mystic forces!");

			break;

		default:
			LOG(llevBug, "BUG: bad call to skill_ident()");
			return 0;
			break;
	}

	if (!success)
	{
		sprintf(buf, "... and learn nothing more.");
		new_draw_info(NDI_UNIQUE, 0, pl, buf);
	}

	return success;
}

int do_skill_detect_curse(object *pl)
{
	object *tmp;
	int success = 0;

	/* check the player inventory - stop after 1st success or
	 * run out of unidented items */
	for (tmp = pl->inv; tmp; tmp = tmp->below)
	{
		if (!QUERY_FLAG(tmp, FLAG_IDENTIFIED) && !QUERY_FLAG(tmp, FLAG_KNOWN_CURSED) && (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED)))
		{
			SET_FLAG(tmp, FLAG_KNOWN_CURSED);
			esrv_update_item(UPD_FLAGS, pl, tmp);
			success += calc_skill_exp(pl, tmp);
		}
	}

	return success;
}

int do_skill_detect_magic(object *pl)
{
	object *tmp;
	int success = 0;

	/* check the player inventory - stop after 1st success or
	 * run out of unidented items */
	for (tmp = pl->inv; tmp; tmp = tmp->below)
	{
		if (!QUERY_FLAG(tmp, FLAG_IDENTIFIED) && !QUERY_FLAG(tmp, FLAG_KNOWN_MAGICAL) && (is_magical(tmp)))
		{
			SET_FLAG(tmp, FLAG_KNOWN_MAGICAL);
			esrv_update_item(UPD_FLAGS, pl, tmp);
			success += calc_skill_exp(pl, tmp);
		}
	}

	return success;
}

/* Helper function for do_skill_ident, so that we can loop
 * over inventory AND objects on the ground conveniently. */
int do_skill_ident2(object *tmp, object *pl, int obj_class)
{
	int success = 0, chance;
	int skill_value = SK_level(pl) + get_weighted_skill_stats(pl);

	if (!QUERY_FLAG(tmp, FLAG_IDENTIFIED) && !QUERY_FLAG(tmp, FLAG_NO_SKILL_IDENT) && need_identify(tmp) && !IS_SYS_INVISIBLE(tmp) && tmp->type == obj_class)
	{
		chance = die_roll(3, 10, pl, PREFER_LOW) - 3 + rndm(0, (tmp->magic ? tmp->magic * 5 : 1) - 1);
		if (skill_value >= chance)
		{
			identify(tmp);
			if (pl->type == PLAYER)
			{
				new_draw_info_format(NDI_UNIQUE, 0, pl, "You identify %s.", long_desc(tmp, NULL));
				if (tmp->msg)
				{
					new_draw_info(NDI_UNIQUE, 0, pl, "The item has a story:");
					new_draw_info(NDI_UNIQUE, 0, pl, tmp->msg);
				}

				/* identify will take care of updating the item if
				 * it is in the players inventory.  IF on map, do it
				 * here */
				if (tmp->map)
					esrv_send_item(pl, tmp);
			}
			success += calc_skill_exp(pl,tmp);
		}
		else
			SET_FLAG(tmp, FLAG_NO_SKILL_IDENT);
	}

	return success;
}
/* do_skill_ident() - workhorse for skill_ident() -b.t. */
/* Sept 95. I put in a probability for identification of artifacts.
 * highly magical artifacts will be more difficult to ident -b.t. */
int do_skill_ident(object *pl, int obj_class)
{
	object *tmp;
	int success = 0;

	/* check the player inventory */
	for (tmp = pl->inv; tmp; tmp = tmp->below)
		success += do_skill_ident2(tmp, pl, obj_class);

	/* check the ground */
	for (tmp = get_map_ob(pl->map, pl->x, pl->y); tmp; tmp = tmp->above)
		success += do_skill_ident2(tmp, pl, obj_class);

	return success;
}

/* players using this skill can 'charm' a monster --
 * into working for them. It can only be used on
 * non-special (see below) 'neutral' creatures.
 * -b.t. (thomas@astro.psu.edu) */
int use_oratory(object *pl, int dir)
{
	int x = pl->x + freearr_x[dir], y = pl->y + freearr_y[dir], chance;
	int stat1 = get_skill_stat1(pl);
	object *tmp;
	mapstruct *m;

	/* only players use this skill */
	if (pl->type != PLAYER)
		return 0;

	if (!(m = out_of_map(pl->map, &x, &y)))
		return 0;

	for (tmp = get_map_ob(m, x, y); tmp; tmp = tmp->above)
	{
		if (!tmp)
			return 0;

		if (!QUERY_FLAG(tmp, FLAG_MONSTER))
			continue;

		/* can't persude players - return because there is nothing else
		  * on that space to charm.  Same for multi space monsters and
		  * special monsters - we don't allow them to be charmed, and there
		  * is no reason to do further processing since they should be the
		  * only monster on the space. */
		if (tmp->type == PLAYER)
			return 0;

		if (tmp->more || tmp->head)
			return 0;

		if (tmp->msg)
			return 0;

		new_draw_info_format(NDI_UNIQUE, 0, pl, "You orate to the %s.", query_name(tmp, NULL));

		/* the following conditions limit who may be 'charmed' */

		/* it's hostile! */
		if (!QUERY_FLAG(tmp, FLAG_UNAGGRESSIVE) && !QUERY_FLAG(tmp, FLAG_FRIENDLY))
		{
			new_draw_info_format(NDI_UNIQUE, 0, pl, "Too bad the %s isn't listening!\n", query_name(tmp, NULL));
			return 0;
		}

		/* it's already allied! */
		if (QUERY_FLAG(tmp, FLAG_FRIENDLY) && (tmp->move_type == PETMOVE))
		{
			if (get_owner(tmp) == pl)
			{
				new_draw_info(NDI_UNIQUE, 0, pl, "Your follower loves your speach.\n");
				return 0;
			}
			/* you steal the follower! */
			else if (SK_level(pl) > tmp->level)
			{
				set_owner(tmp,pl);
				new_draw_info_format(NDI_UNIQUE, 0, pl, "You convince the %s to follow you instead!\n", query_name(tmp, NULL));
				/* Abuse fix - don't give exp since this can otherwise
				  * be used by a couple players to gets lots of exp.*/
				return 0;
			}
		}

		chance = SK_level(pl) * 2 + (stat1 - 2 * tmp->stats.Int) / 2;

		/* Ok, got a 'sucker' lets try to make them a follower */
		if (chance > 0 && tmp->level < (random_roll(0, chance-1, pl, PREFER_HIGH) - 1))
		{
			new_draw_info_format(NDI_UNIQUE, 0, pl, "You convince the %s to become your follower.\n", query_name(tmp, NULL));
			set_owner(tmp, pl);
			SET_FLAG(tmp, FLAG_MONSTER);
			tmp->stats.exp = 0;
			SET_FLAG(tmp, FLAG_FRIENDLY);
			add_friendly_object(tmp);
			tmp->move_type = PETMOVE;
			return calc_skill_exp(pl, tmp);
		}
		/* Charm failed.  Creature may be angry now */
		else if ((SK_level(pl) + ((stat1 - 10) / 2)) < random_roll(1, 2 * tmp->level, pl, PREFER_LOW))
		{
			new_draw_info_format(NDI_UNIQUE, 0, pl, "Your speech angers the %s!\n", query_name(tmp, NULL));
			/* TODO: should probaly call set_npc_enemy() here instead/also */
			if (QUERY_FLAG(tmp, FLAG_FRIENDLY))
			{
				CLEAR_FLAG(tmp, FLAG_FRIENDLY);
				remove_friendly_object(tmp);
				/* needed? */
				tmp->move_type = 0;
			}
			CLEAR_FLAG(tmp, FLAG_UNAGGRESSIVE);
		}
	}

	/* Fall through - if we get here, we didn't charm anything */
	return 0;
}

/* Singing() -this skill allows the player to pacify nearby creatures.
 * There are few limitations on who/what kind of
 * non-player creatures that may be pacified. Right now, a player
 * may pacify creatures which have Int == 0. In this routine, once
 * successfully pacified the creature gets Int=1. Thus, a player
 * may only pacify a creature once.
 * BTW, I appologize for the naming of the skill, I couldnt think
 * of anything better! -b.t. */
int singing(object *pl, int dir)
{
	int xt, yt, i, exp = 0, stat1 = get_skill_stat1(pl), chance;
	object *tmp;
	mapstruct *m;

	/* only players use this skill */
	if (pl->type != PLAYER)
		return 0;

	new_draw_info(NDI_UNIQUE, 0, pl, "You sing.");

	for (i = dir; i < (dir + MIN(SK_level(pl), SIZEOFFREE)); i++)
	{
		xt = pl->x + freearr_x[i];
		yt = pl->y + freearr_y[i];

		if (!(m = out_of_map(pl->map, &xt, &yt)))
			continue;

		for (tmp = get_map_ob(m, xt, yt); tmp; tmp = tmp->above)
		{
			if (!tmp)
				return 0;

			if (!QUERY_FLAG(tmp, FLAG_MONSTER))
				continue;

			/* can't affect players */
			if (tmp->type == PLAYER)
				break;

			/* Only the head listens to music - not other parts.  head
			 * is only set if an object has extra parts.  This is also
			 * necessary since the other parts may not have appropriate
			 * skills/flags set. */
			if (tmp->head)
				break;

			/* the following monsters can't be calmed */

			/* have no ears! */
			if (QUERY_FLAG(tmp, FLAG_SPLITTING) || QUERY_FLAG(tmp, FLAG_HITBACK))
				break;

			/* is too smart */
			if (tmp->stats.Int > 0)
				break;

			/* too powerful */
			if (tmp->level > SK_level(pl))
				break;

			/* undead dont listen! */
			if (QUERY_FLAG(tmp, FLAG_UNDEAD))
				break;

			/* already calm */
			if (QUERY_FLAG(tmp, FLAG_UNAGGRESSIVE) || QUERY_FLAG(tmp, FLAG_FRIENDLY))
				break;

			/* stealing isn't really related (although, maybe it should
			 * be).  This is mainly to prevent singing to the same monster
			 * over and over again and getting exp for it. */
			chance = SK_level(pl) * 2 + (stat1 - 5 - tmp->stats.Int) / 2;

			if (chance && tmp->level * 2 < random_roll(0, chance - 1, pl, PREFER_HIGH))
			{
				SET_FLAG(tmp, FLAG_UNAGGRESSIVE);
				new_draw_info_format(NDI_UNIQUE, 0, pl, "You calm down the %s.\n", query_name(tmp, NULL));
				/* this prevents re-pacification */
				tmp->stats.Int = 1;
				/* Give exp only if they are not aware */
				if (!QUERY_FLAG(tmp,FLAG_NO_STEAL))
					exp += calc_skill_exp(pl,tmp);
				SET_FLAG(tmp,FLAG_NO_STEAL);
			}
			else
				new_draw_info_format(NDI_UNIQUE, 0, pl, "Too bad the %s isn't listening!\n", query_name(tmp, NULL));
		}
	}
	return exp;
}


/* pray() - when this skill is called from do_skill(), it allows
 * the player to regain lost grace points at a faster rate. -b.t.
 * This always returns 0 - return value is used by calling function
 * such that if it returns true, player gets exp in that skill.  This
 * the effect here can be done on demand, we probably don't want to
 * give infinite exp by returning true in any cases. */
int pray(object *pl)
{
	char buf[MAX_BUF];
	object *tmp;

	if (1)
	{
		LOG(llevBug, "BUG: pray() called (praying skill used) from %s\n", query_name(pl, NULL));
		return 0;
	}

	if (pl->type != PLAYER)
		return 0;

	strcpy(buf, "You pray.");
	/* Check all objects - we could stop at floor objects,
	 * but if someone buries an altar, I don't see a problem with
	 * going through all the objects, and it shouldn't be much slower
	 * than extra checks on object attributes. */
	for (tmp = pl->below; tmp != NULL; tmp = tmp->below)
	{
		/* Only if the altar actually belongs to someone do you get special benefits */
		if (tmp && tmp->type == HOLY_ALTAR && tmp->other_arch)
		{
			sprintf(buf, "You pray over the %s.", tmp->name);
			pray_at_altar(pl, tmp);
			/* Only pray at one altar */
			break;
		}
	}

	new_draw_info(NDI_WHITE, 0, pl, buf);

	if (pl->stats.grace < pl->stats.maxgrace)
	{
		pl->stats.grace++;
		pl->last_grace = -1;
	}
	else
		return 0;

	/* Is this really right?  This will basically increase food
	 * consumption, hp & sp regeneration, and everything else that
	 * do_some_living does. */
	do_some_living(pl);
	return 0;
}

/* This skill allows the player to regain a few sp or hp for a
 * brief period of concentration. No armour or weapons may be
 * wielded/applied for this to work. The amount of time needed
 * to concentrate and the # of points regained is dependant on
 * the level of the user. - b.t. thomas@astro.psu.edu  */

/* July 95 I commented out 'factor' - this should now be handled by
 * get_skill_time() -b.t. */

/* Sept 95. Now meditation is level dependant (score). User may
 * meditate w/ more armour on as they get higher level
 * Probably a better way to do this is based on overall encumberance
 * -b.t. */
void meditate (object *pl)
{
	object *tmp;
	int lvl = pl->level;
	/* int factor = 10 / (1 + (pl->level / 10) + (pl->stats.Int / 15) + (pl->stats.Wis / 15)); */

	/* players only */
	if (pl->type != PLAYER)
		return;

	/* check if pl has removed encumbering armour and weapons */
	if (QUERY_FLAG(pl, FLAG_READY_WEAPON) && (lvl < 6))
	{
		new_draw_info(NDI_UNIQUE, 0, pl, "You can't concentrate while wielding a weapon!\n");
		return;
	}
	else
	{
		for (tmp = pl->inv; tmp; tmp = tmp->below)
		{
			if (((tmp->type == ARMOUR && lvl < 12) || (tmp->type == HELMET && lvl < 10) || (tmp->type == SHIELD && lvl < 6) || (tmp->type == BOOTS && lvl < 4) || (tmp->type == GLOVES && lvl < 2) ) && QUERY_FLAG(tmp, FLAG_APPLIED))
			{
				new_draw_info(NDI_UNIQUE, 0, pl, "You can't concentrate while wearing so much armour!\n");
				return;
			}
		}
	}

	/* ok let's meditate!  Spell points are regained first, then once
	 * they are maxed we get back hp. Actual incrementing of values
	 * is handled by the do_some_living() (in player.c). This way magical
	 * bonuses for healing/sp regeneration are included properly
	 * No matter what, we will eat up some playing time trying to
	 * meditate. (see 'factor' variable for what sets the amount of time)  */

	new_draw_info(NDI_UNIQUE, 0, pl, "You meditate.");
	/*pl->speed_left -= (int) FABS(factor);*/

	if (pl->stats.sp < pl->stats.maxsp)
	{
		pl->stats.sp++;
		pl->last_sp = -1;
	}
	else if (pl->stats.hp < pl->stats.maxhp)
	{
		pl->stats.hp++;
		pl->last_heal = -1;
	}
	else
		return;

	do_some_living(pl);
}

/* write_on_item() - wrapper for write_note and write_scroll */
int write_on_item(object *pl, char *params)
{
	object *item;
	char *string = params;
	int msgtype;

	if (pl->type != PLAYER)
		return 0;

	if (!params)
	{
		params = "";
		string = params;
	}

	/* Need to be able to read before we can write! */
	if (!find_skill(pl, SK_LITERACY))
	{
		new_draw_info(NDI_UNIQUE, 0, pl, "You must learn to read before you can write!");
		return 0;
	}

	/* if skill name occurs at begining of the string
	 * we have to reset pointer to miss it and trailing space(s) */

	/* GROS: Bugfix here. if you type
	 * use_skill inscription bla
	 * params will contain "bla" only, so looking for the skill name
	 * shouldn't be done anymore. */
#if 0
	if (lookup_skill_by_name(params) >= 0)
	{
		for (i = strcspn(string, " "); i > 0; i--)
			string++;

		for (i = strspn(string, " "); i > 0; i--)
			string++;
	}
#endif

	/* if there is a message then it goes in a book and no message means
	 * write active spell into the scroll */
	msgtype = (string[0] != '\0') ? BOOK : SCROLL;

	/* find an item of correct type to write on */
	if (!(item = find_marked_object(pl)))
	{
		new_draw_info(NDI_UNIQUE, 0, pl, "You don't have anything marked.");
		return 0;
	}

	if (item)
	{
		if (QUERY_FLAG(item, FLAG_UNPAID))
		{
			new_draw_info(NDI_UNIQUE, 0, pl, "You had better pay for that before you write on it.");
			return 0;
		}

		switch (item->type)
		{
			case SCROLL:
				return write_scroll(pl, item);
				break;

			case BOOK:
			{
				return write_note(pl, item, string);
				break;
			}

			default:
				break;
		}
	}

	new_draw_info_format(NDI_UNIQUE, 0, pl, "You have no %s to write on.", msgtype == BOOK ? "book" : "scroll");
	return 0;
}

/* write_note() - this routine allows players to inscribe messages in
 * ordinary 'books' (anything that is type BOOK). b.t. */
int write_note(object *pl, object *item, char *msg)
{
	char buf[BOOK_BUF];
	object *newBook = NULL;

	/* a pair of sanity checks */
	if (!item || item->type != BOOK)
		return 0;

	if (!msg)
	{
		new_draw_info(NDI_UNIQUE, 0, pl, "No message to write!");
		new_draw_info_format(NDI_UNIQUE, 0, pl," Usage: use_skill %s <message>", skills[SK_INSCRIPTION].name);
		return 0;
	}

	if (strstr(msg, "endmsg"))
	{
		new_draw_info(NDI_UNIQUE, 0, pl, "Trying to cheat now are we?");
		return 0;
	}

	/* Trigger the TRIGGER event */
	if (trigger_event(EVENT_TRIGGER, pl, item, NULL, msg, 0, 0, 0, SCRIPT_FIX_NOTHING))
	{
		return 0;
	}

	/* add msg string to book */
	if (!book_overflow(item->msg, msg, BOOK_BUF))
	{
		if (item->msg)
		{
			strcpy(buf,item->msg);
			FREE_AND_CLEAR_HASH2(item->msg);
		}

		strcat(buf, msg);
		/* new msg needs a LF */
		strcat(buf, "\n");
		if (item->nrof > 1)
		{
			newBook = get_object();
			copy_object(item, newBook);
			decrease_ob(item);
			esrv_send_item(pl, item);
			newBook->nrof = 1;
			FREE_AND_COPY_HASH(newBook->msg, buf);
			newBook = insert_ob_in_ob(newBook, pl);
			esrv_send_item(pl, newBook);
		}
		else
		{
			FREE_AND_COPY_HASH(item->msg, buf);
			esrv_send_item(pl, item);
		}

		new_draw_info_format(NDI_UNIQUE, 0, pl, "You write in the %s.", query_short_name(item, NULL));
		return strlen(msg);
	}
	else
		new_draw_info_format(NDI_UNIQUE, 0, pl, "Your message won't fit in the %s!", query_short_name(item, NULL));

	return 0;
}

/* write_scroll() - this routine allows players to inscribe spell scrolls
 * of spells which they know. Backfire effects are possible with the
 * severity of the backlash correlated with the difficulty of the scroll
 * that is attempted. -b.t. thomas@astro.psu.edu */
int write_scroll (object *pl, object *scroll)
{
	int success = 0, confused = 0, chosen_spell = -1, stat1 = get_skill_stat1(pl);
	object *newScroll;

	/* this is a sanity check */
	if (scroll->type != SCROLL)
	{
		new_draw_info(NDI_UNIQUE, 0, pl, "A spell can only be inscribed into a scroll!");
		return 0;
	}

	/* Check if we are ready to attempt inscription */
	chosen_spell = CONTR(pl)->chosen_spell;
	if (chosen_spell < 0)
	{
		new_draw_info(NDI_UNIQUE, 0, pl, "You need a spell readied in order to inscribe!");
		return 0;
	}

	/* Tried to write non-scroll spell */
	if (!(spells[chosen_spell].spell_use & SPELL_USE_SCROLL))
	{
		new_draw_info_format(NDI_UNIQUE, 0, pl, "The spell %s cannot be inscribed.", spells[chosen_spell].name);
		return 0;
	}

	if (spells[chosen_spell].flags & SPELL_DESC_WIS && spells[chosen_spell].sp > pl->stats.grace)
	{
		new_draw_info_format(NDI_UNIQUE, 0, pl, "You don't have enough grace to write a scroll of %s.", spells[chosen_spell].name);
		return 0;
	}
	else if (spells[chosen_spell].sp > pl->stats.sp)
	{
		new_draw_info_format(NDI_UNIQUE, 0, pl, "You don't have enough mana to write a scroll of %s.", spells[chosen_spell].name);
		return 0;
	}

	/* if there is a spell already on the scroll then player could easily
	 * accidently read it while trying to write the new one.  give player
	 * a 50% chance to overwrite spell at their own level */
	if (scroll->stats.sp && random_roll(0, scroll->level * 2, pl, PREFER_LOW) > SK_level(pl))
	{
		new_draw_info_format(NDI_UNIQUE, 0, pl, "Oops! You accidently read it while trying to write on it.");
		manual_apply(pl, scroll, 0);
		change_skill(pl, SK_INSCRIPTION);
		return 0;
	}

	/* ok, we are ready to try inscription */
	if (QUERY_FLAG(pl, FLAG_CONFUSED))
		confused = 1;

	/* Lost mana/grace no matter what */
	if (spells[chosen_spell].flags & SPELL_DESC_WIS)
		pl->stats.grace -= spells[chosen_spell].sp;
	else
		pl->stats.sp -= spells[chosen_spell].sp;

	if (random_roll(0, spells[chosen_spell].level * 4 - 1, pl, PREFER_LOW) < SK_level(pl))
	{
		newScroll = get_object();
		copy_object(scroll, newScroll);
		decrease_ob(scroll);
		newScroll->nrof = 1;

		if (!confused)
			newScroll->level = (SK_level(pl) > spells[chosen_spell].level ? SK_level(pl) : spells[chosen_spell].level);
		else
		{
			/* a  confused scribe gets a random spell */
			do
			{
				chosen_spell = rndm(0, NROFREALSPELLS - 1);
				/* skip all non active spells */
				while (!spells[chosen_spell].is_active)
				{
					chosen_spell++;
					if (chosen_spell >= NROFREALSPELLS)
						chosen_spell = 0;
				}
			}
			while (!(spells[chosen_spell].spell_use & SPELL_USE_SCROLL));

			newScroll->level = SK_level(pl) > spells[chosen_spell].level ? spells[chosen_spell].level : (random_roll(1, SK_level(pl), pl, PREFER_HIGH));
		}

		if (newScroll->stats.sp == chosen_spell)
			new_draw_info(NDI_UNIQUE, 0, pl, "You overwrite the scroll.");
		else
		{
			new_draw_info(NDI_UNIQUE, 0, pl, "You succeed in writing a new scroll.");
			newScroll->stats.sp = chosen_spell;
		}

		/* wait until finished manipulating the scroll before inserting it */
		newScroll = insert_ob_in_ob(newScroll, pl);
		esrv_send_item(pl, newScroll);
		success = calc_skill_exp(pl, newScroll);
		if (!confused)
			success *= 2;

		return success;
	}
	/* Inscription has failed */
	else
	{
		/* backfire! */
		if (spells[chosen_spell].level > SK_level(pl) || confused)
		{
			new_draw_info(NDI_UNIQUE, 0, pl, "Ouch! Your attempt to write a new scroll strains your mind!");
			if (random_roll(0, 1, pl, PREFER_LOW) == 1)
				drain_specific_stat(pl,4);
			else
			{
				confuse_living(pl,pl,99);
				/*return (-3 * calc_skill_exp(pl, newScroll));*/
				return (-30 * spells[chosen_spell].level);
			}
		}
		else if (random_roll(0, stat1 - 1, pl, PREFER_HIGH) < 15)
		{
			new_draw_info(NDI_UNIQUE, 0, pl, "Your attempt to write a new scroll rattles your mind!");
			confuse_living(pl, pl, 99);
		}
		else
			new_draw_info(NDI_UNIQUE, 0, pl, "You fail to write a new scroll.");
	}

	/*return (-1 * calc_skill_exp(pl, newScroll));*/
	return (-10 * spells[chosen_spell].level);
}

/* The FIND_TRAPS skill. This routine is taken mostly from the
 * command_search loop. It seemed easier to have a separate command,
 * rather than overhaul the existing code - this makes sure things
 * still work for those people who don't want to have skill code
 * implemented. */
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
		yt =pl->y + freearr_y[i];

		if (!(m = out_of_map(pl->map, &xt, &yt)))
			continue;

		for (tmp = get_map_ob(m, xt, yt); tmp != NULL; tmp = tmp->above)
		{
			/*  And now we'd better do an inventory traversal of each
			 * of these objects' inventory */
			if (pl != tmp && (tmp->type == PLAYER || tmp->type == MONSTER))
				continue;

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
								expsum += calc_skill_exp(pl, tmp2);

							/* unhide the trap */
							tmp2->stats.Cha = 1;
						}

						if (!suc)
							suc = 1;
					}
					else
					{
						/* give out a "we have found signs of traps"
						 * if the traps level is not 1.5 times higher. */
						if (tmp2->level <= (level * 1.8f))
							suc = 2;
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
							expsum += calc_skill_exp(pl, tmp);

						/* unhide the trap */
						tmp->stats.Cha = 1;
					}
					if (!suc)
						suc = 1;
				}
				else
				{
					/* give out a "we have found signs of traps"
					 * if the traps level is not 1.5 times higher. */
					if (tmp->level <= (level * 1.8f))
						suc = 2;
				}
			}
		}
	}

	if (!suc)
		new_draw_info(NDI_UNIQUE, 0, pl, "You can't detect any trap here.");
	else if (suc == 2)
		new_draw_info(NDI_UNIQUE, 0, pl, "You detect trap signs!");

	return expsum;
}

/* remove_trap() - This skill will disarm any previously discovered trap
 * the algorithm is based (almost totally) on the old command_disarm() - b.t. */
int remove_trap(object *op, int dir, int level)
{
	object *tmp, *tmp2;
	mapstruct *m;
	int i, x, y, success = 0;

	(void) dir;
	(void) level;

	for (i = 0; i < 9; i++)
	{
		x = op->x + freearr_x[i];
		y = op->y + freearr_y[i];

		if (!(m = out_of_map(op->map, &x, &y)))
			continue;

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
						trap_show(tmp2, tmp);

					if (trap_disarm(op, tmp2, 1) && (!tmp2->owner || tmp2->owner->type != PLAYER))
					{
						tmp2->stats.exp = tmp2->stats.Cha * tmp2->level;
						success += calc_skill_exp(op, tmp2);
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
					trap_show(tmp, tmp);

				if (trap_disarm(op, tmp, 1) && (!tmp->owner || tmp->owner->type != PLAYER))
				{
					tmp->stats.exp = tmp->stats.Cha * tmp->level;
					success += calc_skill_exp(op, tmp);
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
		new_draw_info(NDI_UNIQUE, 0, op, "There is no trap to remove nearby.");

	return success;
}

int skill_throw (object *op, int dir, char *params)
{
	int success = 0;

	if (op->type == PLAYER)
		do_throw(op, find_throw_ob(op, params), dir);
	else
		do_throw(op, find_mon_throw_ob(op->head ? op->head : op), dir);

	return success;
}

/* find_throw_ob() - if we request an object, then
 * we search for it in the inventory of the owner (you've
 * got to be carrying something in order to throw it!).
 * If we didnt request an object, then the top object in inventory
 * (that is "throwable", ie no throwing your skills away!)
 * is the object of choice. Also check to see if object is
 * 'throwable' (ie not applied cursed obj, worn, etc). */
object *find_throw_ob(object *op, char *request)
{
	object *tmp;

	/* safety */
	if (!op)
	{
		LOG(llevBug, "BUG: find_throw_ob(): confused! have a NULL thrower!\n");
		return (object *) NULL;
	}

	/* look through the inventory */
	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		/* can't toss invisible or inv-locked items */
		if (IS_SYS_INVISIBLE(tmp) || QUERY_FLAG(tmp, FLAG_INV_LOCKED))
			continue;

		if (!request || !strcmp(query_name(tmp, NULL), request) || !strcmp(tmp->name, request))
			break;
	}

	/* this should prevent us from throwing away
	 * cursed items, worn armour, etc. Only weapons
	 * can be thrown from 'hand'. */
	if (tmp)
	{
		if (QUERY_FLAG(tmp, FLAG_APPLIED))
		{
			if (tmp->type != WEAPON)
			{
				new_draw_info_format(NDI_UNIQUE, 0,op, "You can't throw %s.", query_base_name(tmp, NULL));
				tmp = NULL;
			}
			else if (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED))
			{
				new_draw_info_format(NDI_UNIQUE, 0, op, "The %s sticks to your hand!", query_base_name(tmp, NULL));
				tmp = NULL;
			}
			else
			{
				if (apply_special(op, tmp, AP_UNAPPLY | AP_NO_MERGE))
				{
					LOG(llevBug, "BUG: find_throw_ob(): %s / %s - couldn't unapply\n", query_name(op, NULL), query_name(tmp, NULL));
					tmp = NULL;
				}
			}
		}
	}

	if (tmp && QUERY_FLAG(tmp, FLAG_INV_LOCKED))
	{
		LOG(llevBug, "BUG: find_throw_ob(): %s / %s object is locked\n", query_name(op, NULL), query_name(tmp, NULL));
		tmp = NULL;
	}

	return tmp;
}

/* find_throw_tag() - request an object by tag,
 * we search for it in the inventory of the owner (you've
 * got to be carrying something in order to throw it!).
 * Also check to see if object is
 * 'throwable' (ie not applied cursed obj, worn, etc). */
object *find_throw_tag(object *op, tag_t tag)
{
	object *tmp;

	/* look through the inventory */
	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		/* can't toss invisible or inv-locked items */
		if (IS_SYS_INVISIBLE(tmp) || QUERY_FLAG(tmp, FLAG_INV_LOCKED))
			continue;

		if (tmp->count == tag)
			break;
	}

	/* this should prevent us from throwing away
	 * cursed items, worn armour, etc. Only weapons
	 * can be thrown from 'hand'.  */
	if (!tmp)
	{
		return NULL;
	}

	if (QUERY_FLAG(tmp, FLAG_APPLIED))
	{
		/* we can't appy throwing stuff like darts, so this must be a weapon. skip if not OR when it
		 * can't be thrown OR when it is startequip which can't be dropped. */
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
		/* if cursed or damned, we can't unapply it - no throwing! */
		else if (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED))
		{
			new_draw_info_format(NDI_UNIQUE, 0, op, "The %s sticks to your hand!", query_base_name(tmp, NULL));
			tmp = NULL;
		}
		/* ok, its a throw hybrid weapon - unapply it. then we will fire it after this function returns */
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
		/* not weapon nor throwable - no throwing */
		if ((tmp->type != WEAPON && tmp->type != POTION) && !QUERY_FLAG(tmp, FLAG_IS_THROWN))
		{
			new_draw_info_format(NDI_UNIQUE, 0, op, "You can't throw %s.", query_base_name(tmp, NULL));
			tmp = NULL;
		}
		/* special message for throw hybrid weapons */
		else if (tmp->type == WEAPON)
		{
			new_draw_info_format(NDI_UNIQUE, 0, op, "You must apply the %s first.", query_base_name(tmp, NULL));
			tmp = NULL;
		}
	}

	return tmp;
}

/* make_throw_ob() We construct the 'carrier' object in
 * which we will insert the object that is being thrown.
 * This combination  becomes the 'thrown object'. -b.t. */
object *make_throw_ob(object *orig)
{
	object *toss_item = NULL;

	if (orig)
	{
		toss_item = get_object();

		if (QUERY_FLAG(orig, FLAG_APPLIED))
		{
			LOG(llevBug, "BUG: make_throw_ob(): ob is applied (%s)\n", query_name(orig, NULL));
			/* insufficient workaround, but better than nothing */
			CLEAR_FLAG (orig, FLAG_APPLIED);
		}

		copy_object(orig, toss_item);
		toss_item->type = THROWN_OBJ;
		SET_FLAG(toss_item, FLAG_IS_MISSILE);
		CLEAR_FLAG(toss_item, FLAG_CHANGING);
		/* default damage */
		toss_item->stats.dam = 0;
#ifdef DEBUG_THROW
		LOG(llevDebug, "DEBUG: inserting %s(%d) in toss_item(%d)\n", orig->name, orig->count, toss_item->count);
#endif
		insert_ob_in_ob(orig, toss_item);
	}

	return toss_item;
}


/* do_throw() - op throws any object toss_item. This code
 * was borrowed from fire_bow (see above), so probably these
 * two functions should be merged together since they are
 * almost the same. I left them apart for now for debugging
 * purposes, and also, so as to not screw up fire_bow()!
 * This function is useable by monsters.  -b.t. */

/* i changed alot here but let some old code in... after the throwing
 * has passed some testing, we will rewrite this */
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
			new_draw_info(NDI_UNIQUE, 0, op, "You have nothing to throw.");

		return;
	}

	if (QUERY_FLAG(throw_ob, FLAG_STARTEQUIP))
	{
		if (op->type == PLAYER)
			new_draw_info(NDI_UNIQUE, 0, op, "The gods won't let you throw that.");

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

#if 0
	/* the more we carry, the less we can throw. Limit only on players */
	maxc = max_carry[str] * 1000;
	if (op->carrying>maxc && op->type == PLAYER)
		load_factor = (float)maxc / (float) op->carrying;
#endif

	/* we need something more clever here, using weight_limit */
	load_factor = 1.0f;

	/* lighter items are thrown harder, farther, faster */
	if (throw_ob->weight <= 0)
	{
		/* 0 or negative weight?!? Odd object, can't throw it */
		new_draw_info_format(NDI_UNIQUE, 0, op, "You can't throw %s.\n", query_base_name(throw_ob, NULL));
		return;
	}

	eff_str = (int)((float)str * (load_factor < 1.0f ? load_factor : 1.0f));
	eff_str = (int) ((float) eff_str * item_factor * str_factor);

	/* alas, arrays limit us to a value of MAX_STAT (30). Use str_factor to
	 * account for super-strong throwers. */
	if (eff_str > MAX_STAT)
		eff_str = MAX_STAT;

	if (eff_str < 1)
		eff_str = 1;

#ifdef DEBUG_THROW
	LOG(llevDebug, "%s carries %d, eff_str=%d\n", op->name, op->carrying, eff_str);
	LOG(llevDebug, " max_c=%d, item_f=%f, load_f=%f, str=%d\n", maxc, item_factor, load_factor, op->stats.Str);
	LOG(llevDebug, " str_factor=%f\n", str_factor);
	LOG(llevDebug, " item %s weight= %d\n", throw_ob->name, throw_ob->weight);
#endif

	/* these are throwing objects left to the player */
	left = throw_ob;
	left_cont = left->env;
	left_tag = left->count;

	/* sometimes get_split_ob can't split an object (because op->nrof==0?)
	 * and returns NULL. We must use 'left' then */

	if ((throw_ob = get_split_ob(throw_ob, 1)) == NULL)
	{
#ifdef DEBUG_THROW
		LOG(llevDebug, " get_splt_ob faild to split throw ob %s\n", left->name);
#endif
		throw_ob = left;
		remove_ob(left);
		check_walk_off(left, NULL, MOVE_APPLY_VANISHED);
		if (op->type == PLAYER)
			esrv_del_item(CONTR(op), left->count, left->env);
	}
	else if (op->type == PLAYER)
	{
		if (was_destroyed (left, left_tag))
			esrv_del_item(CONTR(op), left_tag, left_cont);
		else
			esrv_update_item(UPD_NROF, op, left);
	}

	/* special case: throwing powdery substances like dust, dirt */
	if (QUERY_FLAG(throw_ob, FLAG_DUST))
	{
		cast_dust(op, throw_ob, dir);
		/* update the shooting speed for the player action timer.
		 * We init the used skill with it - its not calculated here.
		 * cast_dust() can change the used skill... */
		if (op->type == PLAYER)
			op->chosen_skill->stats.maxsp = throw_ob->last_grace;

		return;
	}

	/* Experimental targetting throw hack */
	if (!dir && op->type == PLAYER && OBJECT_VALID(CONTR(op)->target_object, CONTR(op)->target_object_count))
	{
		dir = get_dir_to_target(op, CONTR(op)->target_object, &range_vector);
		target_throw = 1;
	}
	else
		throw_ob->stats.grace = 0;

	/* 3 things here prevent a throw, you aimed at your feet, you
	 * have no effective throwing strength, or you threw at a wall */
	if (!dir || (eff_str <= 1) || wall(op->map, op->x + freearr_x[dir], op->y + freearr_y[dir]))
	{
		/* bounces off 'wall', and drops to feet */
		if (!QUERY_FLAG(throw_ob, FLAG_REMOVED))
		{
			remove_ob(throw_ob);
			if (check_walk_off(throw_ob, NULL, MOVE_APPLY_MOVE) != CHECK_WALK_OK)
				return;
		}

		throw_ob->x = op->x;
		throw_ob->y = op->y;

		if (!insert_ob_in_map(throw_ob, op->map, op, 0))
			return;

		if (op->type == PLAYER)
		{
			if (eff_str <= 1)
				new_draw_info_format(NDI_UNIQUE, 0, op, "Your load is so heavy you drop %s to the ground.", query_name(throw_ob, NULL));
			else if (!dir)
				new_draw_info_format(NDI_UNIQUE, 0,op, "You drop %s at the ground.", query_name(throw_ob, NULL));
			else
				new_draw_info(NDI_UNIQUE, 0, op, "Something is in the way.");
		}
		return;
	}

	/* Make a thrown object -- insert real object in a 'carrier' object.
	 * If unsuccessfull at making the "thrown_obj", we just reinsert
	 * the original object back into inventory and exit */
	if ((toss_item = make_throw_ob(throw_ob)))
		throw_ob = toss_item;
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

	/* Experimental targetting throw hack */
	if (target_throw)
	{
		/* hp */
		int dx = range_vector.distance_x;
		/* sp */
		int dy = range_vector.distance_y;
		/* maxhp, maxsp */
		int stepx, stepy;

		if (dy < 0)
		{
			dy = -dy;
			stepy = -1;
		}
		else
			stepy = 1;

		if (dx < 0)
		{
			dx = -dx;
			stepx = -1;
		}
		else
			stepx = 1;

		throw_ob->stats.hp = dx << 1;
		throw_ob->stats.sp = dy << 1;
		throw_ob->stats.maxhp = stepx;
		throw_ob->stats.maxsp = stepy;
		/* target-throw marker =) TODO: probably better to use ob->enemy instead */
		throw_ob->stats.grace = 666;

		/* fraction */
		if (dx > dy)
			/* same as 2*dy - dx */
			throw_ob->stats.exp = (dy << 1) - dx;
		else
			/* same as 2*dx - dy */
			throw_ob->stats.exp = (dx << 1) - dy;
	}

	/* the damage bonus from the force of the throw */
	dam = (int)(str_factor * (float)dam_bonus[eff_str]);

	/*LOG(llevDebug, " item %s weight= %d (dam:%d str_factor:%f bonus: %d)\n", throw_ob->name, throw_ob->weight, dam, str_factor, dam_bonus[eff_str]);*/
	/* Now, lets adjust the properties of the thrown_ob. */

	/* speed */
	throw_ob->speed = (speed_bonus[eff_str] + 1.0f) / 1.5f;
	/* no faster than an arrow! */
	throw_ob->speed = MIN(1.0f, throw_ob->speed);

	/* item damage. Eff_str and item weight influence damage done */
	weight_f = (throw_ob->weight / 2000) > MAX_STAT ? MAX_STAT : (throw_ob->weight / 2000);
	throw_ob->stats.dam += (dam / 3) + dam_bonus[weight_f] + (throw_ob->weight / 15000) - 2;

	/* chance of breaking. Proportional to force used and weight of item */
	throw_ob->stats.food = (dam / 2) + (throw_ob->weight / 60000);

#if 0
	throw_ob->stats.wc = 22 - dex_bonus[op->stats.Dex ? dex_bonus[op->stats.Dex] : 0] - thaco_bonus[eff_str] - SK_level(op);
#endif

	/* now we get the wc from the used skill! this will allow customized skill */
	if ((tmp_op = SK_skill(op)))
		throw_ob->stats.wc = tmp_op->last_heal;
	/* mobs */
	else
		throw_ob->stats.wc = 10;

	/* the properties of objects which are meant to be thrown (ie dart,
	 * throwing knife, etc) will differ from ordinary items. Lets tailor
	 * this stuff in here. */

	/* here our TRUE throw stuff is handled...
	 * in CF, you can nearly throw everything - this will give you or a mob
	 * the chance to throw with chairs or tables.
	 * BUT as cool this sounds - it have massive problems to balance and
	 * can give alot of possible exploits the chance to appear. */
	throw_ob->stats.wc_range = op->stats.wc_range;
	if (QUERY_FLAG(throw_ob->inv, FLAG_IS_THROWN))
	{
#if 0
		/* fly a little further */
		throw_ob->last_sp += eff_str / 3;
#endif
		throw_ob->stats.dam = throw_ob->inv->stats.dam + throw_ob->magic;
		throw_ob->stats.wc += throw_ob->magic + throw_ob->inv->stats.wc;

		/* adjust for players */
		if (op->type == PLAYER)
		{
			op->chosen_skill->stats.maxsp = throw_ob->last_grace;
			/* i don't want overpower the throwing - so dam_bonus / 2 */
			throw_ob->stats.dam = FABS((int)((float)(throw_ob->stats.dam + dam_bonus[op->stats.Str] / 2) * LEVEL_DAMAGE(SK_level(op))));
			/* hm, i want not give to much boni for str */
			throw_ob->stats.wc += thaco_bonus[op->stats.Dex] + SK_level(op);
		}
		/* we use level to add boni here-  as higher in level as more dangerous */
		else
		{
			throw_ob->stats.dam = FABS((int)((float)(throw_ob->stats.dam) * LEVEL_DAMAGE(op->level)));
			throw_ob->stats.wc += 10 + op->level;
		}

		throw_ob->stats.grace = throw_ob->last_sp;
		throw_ob->stats.maxgrace = 60 + (RANDOM() % 12);

		/* only throw objects get directional faces */
		if (GET_ANIM_ID(throw_ob) && NUM_ANIMATIONS(throw_ob))
			SET_ANIMATION(throw_ob, (NUM_ANIMATIONS(throw_ob) / NUM_FACINGS(throw_ob)) * dir);

		/* adjust damage with item condition */
		throw_ob->stats.dam = (sint16)(((float)throw_ob->stats.dam / 100.0f) * (float)throw_ob->item_condition);
	}
	else
	{
		/* some materials will adjust properties.. */
		if (throw_ob->material & M_LEATHER)
		{
			throw_ob->stats.dam -= 1;
			throw_ob->stats.food -= 10;
		}

		if (throw_ob->material & M_GLASS)
			throw_ob->stats.food += 60;

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

		/* light obj have more wind resistance, fly slower*/
		if (throw_ob->weight > 500)
			throw_ob->speed *= 0.8f;

		if (throw_ob->weight > 50)
			throw_ob->speed *= 0.5f;
	}

	/* some limits, and safeties (needed?) */
	if (throw_ob->stats.dam < 0)
		throw_ob->stats.dam = 0;

	/*if(throw_ob->last_sp>eff_str) throw_ob->last_sp=eff_str;*/
	if (throw_ob->stats.food < 0)
		throw_ob->stats.food = 0;

	if (throw_ob->stats.food > 100)
		throw_ob->stats.food = 100;

	if (throw_ob->stats.wc > 30)
		throw_ob->stats.wc = 30;

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
		move_arrow(throw_ob);
}
