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
 * Monster memory, NPC interaction, AI, and other related functions
 * are in this file, all used by the @ref MONSTER "monster" type
 * objects. */

#include <global.h>
#include <sproto.h>

/**
 * When parsing a message-struct, the msglang struct is used to contain
 * the values.
 *
 * This struct will be expanded as new features are added.
 * When things are stable, it will be parsed only once. */
typedef struct _msglang
{
	/** An array of messages */
	char **messages;

	/** For each message, an array of strings to match */
	char ***keywords;
} msglang;

extern spell spells[NROFREALSPELLS];

static int can_detect_enemy(object *op, object *enemy, rv_vector *rv);
static object *find_nearest_living_creature(object *npc);
static int move_randomly(object *op);
static int can_hit(object *ob1, rv_vector *rv);
static object *monster_choose_random_spell(object *monster);
static int monster_cast_spell(object *head, object *part, int dir, rv_vector *rv);
static int monster_use_skill(object *head, object *part, object *pl, int dir);
static int monster_use_range(object *head, object *part, object *pl, int dir);
static int monster_use_bow(object *head, object *part, int dir);
static int dist_att(int dir, object *part, rv_vector *rv);
static int run_att(int dir, object *ob, object *part, rv_vector *rv);
static int hitrun_att(int dir, object *ob);
static int wait_att(int dir, object *ob, object *part, rv_vector *rv);
static int disthit_att(int dir, object *ob, object *part, rv_vector *rv);
static int wait_att2(int dir, rv_vector *rv);
static void circ1_move(object *ob);
static void circ2_move(object *ob);
static void pace_movev(object *ob);
static void pace_moveh(object *ob);
static void pace2_movev(object *ob);
static void pace2_moveh(object *ob);
static void rand_move(object *ob);
static int talk_to_wall(object *npc, char *txt);

/**
 * Update (or clear) an NPC's enemy. Perform most of the housekeeping
 * related to switchign enemies
 *
 * You should always use this method to set (or clear) an NPC's enemy.
 *
 * If enemy is given an aggro wp may be set up.
 * If rv is given, it will be filled out with the vector to enemy
 *
 * enemy and/or rv may be NULL.
 * @param npc The NPC object we're setting enemy for.
 * @param enemy The enemy object, NULL if we're clearing the enemy
 * for this NPC.
 * @param rv Range vector of the enemy. */
void set_npc_enemy(object *npc, object *enemy, rv_vector *rv)
{
	object *aggro_wp;
	rv_vector rv2;

	/* Do nothing if new enemy == old enemy */
	if (enemy == npc->enemy && (enemy == NULL || enemy->count == npc->enemy_count))
	{
		return;
	}

	/* Players don't need waypoints, speed updates or aggro counters */
	if (npc->type == PLAYER)
	{
		npc->enemy = enemy;
		npc->enemy_count = enemy->count;

		return;
	}

	/* Non-aggro-waypoint related stuff */
	if (enemy)
	{
		if (rv == NULL)
		{
			rv = &rv2;
		}

		get_rangevector(npc, enemy, rv, RV_DIAGONAL_DISTANCE);
		npc->enemy_count = enemy->count;

		/* important: that's our "we lose aggro count" - reset to zero here */
		npc->last_eat = 0;

		/* Monster has changed status from normal to attack - let's hear it! */
		if (npc->enemy == NULL && !QUERY_FLAG(npc, FLAG_FRIENDLY))
		{
			play_sound_map(npc->map, npc->x, npc->y, SOUND_GROWL, SOUND_NORMAL);
		}

		if (QUERY_FLAG(npc, FLAG_UNAGGRESSIVE))
		{
			/* The unaggressives look after themselves 8) */
			CLEAR_FLAG(npc, FLAG_UNAGGRESSIVE);
			npc_call_help(npc);
		}
	}
	else
	{
		object *base = insert_base_info_object(npc);
		object *wp = get_active_waypoint(npc);

		if (base && !wp && wp_archetype)
		{
			object *return_wp = get_return_waypoint(npc);

#ifdef DEBUG_PATHFINDING
			LOG(llevDebug, "set_npc_enemy(): %s lost aggro and is returning home (%s:%d,%d)\n", STRING_OBJ_NAME(npc), base->slaying, base->x, base->y);
#endif
			if (!return_wp)
			{
				return_wp = arch_to_object(wp_archetype);
				insert_ob_in_ob(return_wp, npc);
				return_wp->owner = npc;
				return_wp->ownercount = npc->count;
				FREE_AND_ADD_REF_HASH(return_wp->name, shstr_cons.home);
				/* mark as return-home wp */
				SET_FLAG(return_wp, FLAG_REFLECTING);
				/* mark as best-effort wp */
				SET_FLAG(return_wp, FLAG_NO_ATTACK);
			}

			return_wp->stats.hp = base->x;
			return_wp->stats.sp = base->y;
			FREE_AND_ADD_REF_HASH(return_wp->slaying, base->slaying);
			/* Activate wp */
			SET_FLAG(return_wp, FLAG_CURSED);
			/* reset best-effort timer */
			return_wp->stats.Int = 0;

			/* setup move_type to use waypoints */
			return_wp->move_type = npc->move_type;
			npc->move_type = (npc->move_type & LO4) | WPOINT;

			wp = return_wp;
		}

		/* TODO: add a little pause to the active waypoint */
	}

	npc->enemy = enemy;
	/* Update speed */
	set_mobile_speed(npc, 0);

	/* Setup aggro waypoint */
	if (!wp_archetype)
	{
#ifdef DEBUG_PATHFINDING
		LOG(llevDebug, "set_npc_enemy(): Aggro waypoints disabled\n");
#endif
		return;
	}

	/* TODO: check intelligence against lower limit to allow pathfind */
	aggro_wp = get_aggro_waypoint(npc);

	/* Create a new aggro wp for npc? */
	if (!aggro_wp && enemy)
	{
		aggro_wp = arch_to_object(wp_archetype);
		insert_ob_in_ob(aggro_wp, npc);
		/* Mark as aggro WP */
		SET_FLAG(aggro_wp, FLAG_DAMNED);
		aggro_wp->owner = npc;
#ifdef DEBUG_PATHFINDING
		LOG(llevDebug, "set_npc_enemy(): created wp for '%s'\n", STRING_OBJ_NAME(npc));
#endif
	}

	/* Set up waypoint target (if we actually got a waypoint) */
	if (aggro_wp)
	{
		if (enemy)
		{
			aggro_wp->enemy_count = npc->enemy_count;
			aggro_wp->enemy = enemy;
			FREE_AND_ADD_REF_HASH(aggro_wp->name, enemy->name);
#ifdef DEBUG_PATHFINDING
			LOG(llevDebug, "set_npc_enemy(): got wp for '%s' -> '%s'\n", npc->name, enemy->name);
#endif
		}
		else
		{
			aggro_wp->enemy = NULL;
#ifdef DEBUG_PATHFINDING
			LOG(llevDebug, "set_npc_enemy(): cleared aggro wp for '%s'\n", npc->name);
#endif
		}
	}
}

/**
 * Checks if NPC's enemy is still valid.
 * @param npc The NPC object.
 * @param rv Range vector of the enemy.
 * @return Enemy object if valid, NULL otherwise. */
object *check_enemy(object *npc, rv_vector *rv)
{
	/* if this is pet, let him attack the same enemy as his owner
	 * TODO: when there is no ower enemy, try to find a target,
	 * which CAN attack the owner. */
	if ((npc->move_type & HI4) == PETMOVE)
	{
		if (npc->owner != NULL)
		{
			/* if owner enemy != pet enemy, change it! */
			if (npc->owner->enemy && (npc->enemy != npc->owner->enemy || npc->enemy_count != npc->enemy->count))
			{
				set_npc_enemy(npc, npc->owner->enemy, NULL);
			}
		}
		else
		{
			if (npc->enemy)
			{
				set_npc_enemy(npc, NULL, NULL);
			}
		}
	}

	if (npc->enemy == NULL)
	{
		return NULL;
	}

	if (!OBJECT_VALID(npc->enemy, npc->enemy_count) || npc == npc->enemy)
	{
		set_npc_enemy(npc, NULL, NULL);

		return NULL;
	}

	/* Check flags for friendly npc and aggressive mobs (unaggressives
	 * will not be handled here). */
	if (QUERY_FLAG(npc, FLAG_FRIENDLY))
	{
		/* NPC should not attack players or other friendly units on purpose */
		if (npc->enemy->type == PLAYER || QUERY_FLAG(npc->enemy, FLAG_FRIENDLY))
		{
			set_npc_enemy(npc, NULL, NULL);
			return NULL;
		}
	}
	else
	{
		/* This is an important check - without this, a single area spell
		 * from a mob will aggravate all other mobs to him - they will
		 * slaughter themselves and not the player. */
		if (!QUERY_FLAG(npc->enemy, FLAG_FRIENDLY) && npc->enemy->type != PLAYER)
		{
			set_npc_enemy(npc, NULL, NULL);
			return NULL;
		}
	}

	return can_detect_enemy(npc, npc->enemy, rv) ? npc->enemy : NULL;
}

/**
 * Tries to find an enemy for NPC. We pass the range vector since
 * our called will find the information useful.
 *
 * Currently, only move_monster() calls this function.
 * @param npc The NPC object.
 * @param rv Range vector.
 * @return Enemy object if found, 0 otherwise
 * @note This doesn't find an enemy - it only checks if the old enemy is
 * still valid and hittable, otherwise changes target to a better enemy
 * when possible.
 * @todo Use is_friend_of in the good vs. good and evil vs. evil
 * check? */
object *find_enemy(object *npc, rv_vector *rv)
{
	object *tmp = NULL;

	/* If we berserk, we don't care about others - we attack all we can
	 * find. */
	if (QUERY_FLAG(npc, FLAG_BERSERK))
	{
		/* Always clear the attacker entry */
		npc->attacked_by = NULL;
		tmp = find_nearest_living_creature(npc);

		if (tmp)
		{
			get_rangevector(npc, tmp, rv, 0);
		}

		return tmp;
	}

	/* Here is the main enemy selection.
	 * We want this: if there is an enemy, attack him until its not possible or
	 * one of both is dead.
	 * If we have no enemy and we are...
	 * a monster: try to find a player, a pet or a friendly monster
	 * a friendly: only target a monster which is targeting you first or targeting a player
	 * a pet: attack player enemy or a monster */

	/* Pet move */
	if ((npc->move_type & HI4) == PETMOVE)
	{
		/* Always clear the attacker entry */
		npc->attacked_by = NULL;
		tmp= get_pet_enemy(npc, rv);
		npc->last_eat = 0;

		if (tmp)
		{
			get_rangevector(npc, tmp, rv, 0);
		}

		return tmp;
	}

	/* We check our old enemy.
	 * If tmp != 0, we have succesfully callled get_rangevector() too. */
	tmp = check_enemy(npc, rv);

	if (!tmp || (npc->attacked_by && npc->attacked_by_distance < (int) rv->distance))
	{
		/* If we have an attacker, check him */
		if (OBJECT_VALID(npc->attacked_by, npc->attacked_by_count))
		{
			/* We don't want a fight evil vs evil or good against non evil. */
			if ((QUERY_FLAG(npc, FLAG_FRIENDLY) && QUERY_FLAG(npc->attacked_by, FLAG_FRIENDLY)) || (!QUERY_FLAG(npc, FLAG_FRIENDLY) && (!QUERY_FLAG(npc->attacked_by, FLAG_FRIENDLY) && npc->attacked_by->type != PLAYER)))
			{
				/* Skip it, but let's wakeup */
				CLEAR_FLAG(npc, FLAG_SLEEP);
			}
			/* The only thing we must know... */
			else if (on_same_map(npc, npc->attacked_by))
			{
				CLEAR_FLAG(npc, FLAG_SLEEP);
				set_npc_enemy(npc, npc->attacked_by, rv);
				/* Always clear the attacker entry */
				npc->attacked_by = NULL;

				/* Face our attacker */
				return npc->enemy;
			}
		}

		/* We have no legal enemy or attacker, so we try to target a new
		 * one. */
		if (!QUERY_FLAG(npc, FLAG_UNAGGRESSIVE))
		{
			if (QUERY_FLAG(npc, FLAG_FRIENDLY))
			{
				tmp = find_nearest_living_creature(npc);
			}
			else
			{
				tmp = get_nearest_player(npc);
			}

			if (tmp != npc->enemy)
			{
				set_npc_enemy(npc, tmp, rv);
			}
		}
		else if (npc->enemy)
		{
			/* Make sure to clear the enemy, even if FLAG_UNAGRESSIVE is true */
			set_npc_enemy(npc, NULL, NULL);
		}
	}

	/* Always clear the attacker entry */
	npc->attacked_by = NULL;
	return tmp;
}

/**
 * Check if target is a possible valid enemy.
 *
 * This includes possibility to see, reach, etc.
 *
 * This must be used BEFORE we assign target as op enemy.
 * @param op The object.
 * @param target The target to check.
 * @param range Range this object can see.
 * @param srange Stealth range this object can see.
 * @param rv Range vector.
 * @return 1 if valid enemy, 0 otherwise. */
int can_detect_target(object *op, object *target, int range, int srange, rv_vector *rv)
{
	/* Will check for legal maps too */
	if (!op || !target || !on_same_map(op, target))
	{
		return 0;
	}

	/* We check for sys_invisible and normal */
	if (IS_INVISIBLE(target, op))
	{
		return 0;
	}

	get_rangevector(op, target, rv, 0);

	if (QUERY_FLAG(target, FLAG_STEALTH) && !QUERY_FLAG(op, FLAG_XRAYS))
	{
		if (srange < (int) rv->distance)
		{
			return 0;
		}
	}
	else
	{
		if (range < (int) rv->distance)
		{
			return 0;
		}
	}

	return 1;
}

/**
 * Controls if monster still can see/detect its enemy.
 *
 * Includes visibility but also map and area control.
 * @param op The monster object.
 * @param enemy Monster object's enemy.
 * @param rv Range vector.
 * @return 1 if can see/detect, 0 otherwise. */
static int can_detect_enemy(object *op, object *enemy, rv_vector *rv)
{
	/* Will check for legal maps too */
	if (!op || !enemy || !on_same_map(op, enemy))
	{
		return 0;
	}

	/* We check for sys_invisible and normal */
	if (IS_INVISIBLE(enemy, op))
	{
		return 0;
	}

	/* If our enemy is too far away ... */
	get_rangevector(op, enemy, rv, 0);

	if ((int) rv->distance >= MAX(MAX_AGGRO_RANGE, op->stats.Wis))
	{
		/* Then start counting until our mob loses aggro... */
		if (++op->last_eat > MAX_AGGRO_TIME)
		{
			set_npc_enemy(op, NULL, NULL);

			return 0;
		}
	}
	/* Our mob is aggroed again - because target is in range again */
	else
	{
		op->last_eat = 0;
	}

	return 1;
}

/**
 * Move a monster object.
 * @param op The monster object to be moved.
 * @return 1 if the object has been freed, 0 otherwise. */
int move_monster(object *op)
{
	int dir, special_dir = 0, diff;
	object *owner, *enemy, *part, *tmp;
	rv_vector rv;

	if (op->head)
	{
		LOG(llevBug, "BUG: move_monster(): called from tail part. (%s -- %s)\n", query_name(op, NULL), op->arch->name);
		return 0;
	}

	/* Monsters not on maps don't do anything. */
	if (!op->map)
	{
		return 0;
	}

	/* If we are here, we're never paralyzed anymore */
	CLEAR_FLAG(op, FLAG_PARALYZED);

	/* For target facing, we copy this value here for fast access */
	op->anim_enemy_dir = -1;
	op->anim_moving_dir = -1;

	/* Here is the heart of the mob attack and target area.
	 * find_enemy() checks the old enemy or gets us a new one. */
	tmp = op->enemy;

	/* We never ever attack */
	if (QUERY_FLAG(op, FLAG_NO_ATTACK))
	{
		if (op->enemy)
		{
			set_npc_enemy(op, NULL, NULL);
		}

		enemy = NULL;
	}
	else if ((enemy = find_enemy(op, &rv)))
	{
		CLEAR_FLAG(op, FLAG_SLEEP);
		op->anim_enemy_dir = rv.direction;

		if (!enemy->attacked_by || (enemy->attacked_by && enemy->attacked_by_distance > (int)rv.distance))
		{
			/* We have an enemy, just tell him we want him dead */
			enemy->attacked_by = op;
			enemy->attacked_by_count = op->count;
			/* Now the attacked foe knows how near we are */
			enemy->attacked_by_distance = (sint16) rv.distance;
		}
	}

	/* Generate hp, if applicable */
	if (op->stats.Con && op->stats.hp < op->stats.maxhp)
	{
		if (++op->last_heal > 5)
		{
			op->last_heal = 0;
			op->stats.hp += op->stats.Con;

			if (op->stats.hp > op->stats.maxhp)
			{
				op->stats.hp = op->stats.maxhp;
			}
		}

		/* So if the monster has gained enough HP that they are no longer afraid */
		if (QUERY_FLAG(op, FLAG_RUN_AWAY) && op->stats.hp >= (signed short) (((float) op->run_away / (float) 100) * (float) op->stats.maxhp))
		{
			CLEAR_FLAG(op, FLAG_RUN_AWAY);
		}
	}

	/* Generate sp, if applicable */
	if (op->stats.Pow && op->stats.sp < op->stats.maxsp)
	{
		op->last_sp += (int) ((float) (8 * op->stats.Pow) / FABS(op->speed));
		/* causes Pow/16 sp/tick */
		op->stats.sp += op->last_sp / 128;
		op->last_sp %= 128;

		if (op->stats.sp > op->stats.maxsp)
		{
			op->stats.sp = op->stats.maxsp;
		}
	}

	/* Time to regain some "guts"... */
	if (QUERY_FLAG(op, FLAG_SCARED) && !(RANDOM() % 20))
	{
		CLEAR_FLAG(op, FLAG_SCARED);
	}

	/* If we don't have an enemy, do special movement or the like */
	if (!enemy)
	{
		if (QUERY_FLAG(op, FLAG_ONLY_ATTACK))
		{
			remove_ob(op);
			check_walk_off(op, NULL, MOVE_APPLY_DEFAULT);
			return 1;
		}

		if (!QUERY_FLAG(op, FLAG_STAND_STILL))
		{
			if (op->move_type & HI4)
			{
				switch (op->move_type & HI4)
				{
					case PETMOVE:
						pet_move(op);
						break;

					case CIRCLE1:
						circ1_move(op);
						break;

					case CIRCLE2:
						circ2_move(op);
						break;

					case PACEV:
						pace_movev(op);
						break;

					case PACEH:
						pace_moveh(op);
						break;

					case PACEV2:
						pace2_movev(op);
						break;

					case PACEH2:
						pace2_moveh(op);
						break;

					case RANDO:
						rand_move(op);
						break;

					case RANDO2:
						move_randomly(op);
						break;

					case WPOINT:
						waypoint_move(op, get_active_waypoint(op));
						break;
				}

				return 0;
			}
			else if (QUERY_FLAG(op, FLAG_RANDOM_MOVE))
			{
				move_randomly(op);
			}
		}

		return 0;
	}

	/* We have an enemy. Block immediately below is for pets */
	if ((op->type & HI4) == PETMOVE && (owner = get_owner(op)) != NULL && !on_same_map(op, owner))
	{
		follow_owner(op, owner);

		/* Gecko: The following block seems buggy, but I'm not sure... */
		if (QUERY_FLAG(op, FLAG_REMOVED) && FABS(op->speed) > MIN_ACTIVE_SPEED)
		{
			remove_friendly_object(op);
			return 1;
		}

		return 0;
	}

	/* Move the check for scared up here - if the monster was scared,
	 * we were not doing any of the logic below, so might as well save
	 * a few cpu cycles. */
	if (!QUERY_FLAG(op, FLAG_SCARED))
	{
		rv_vector rv1;

		/* Test every part of an object */
		for (part = op; part != NULL; part = part->more)
		{
			get_rangevector(part, enemy, &rv1, 0x1);
			dir = rv1.direction;

			if (QUERY_FLAG(op, FLAG_RUN_AWAY))
			{
				dir = absdir(dir + 4);
			}

			if (QUERY_FLAG(op, FLAG_CONFUSED))
			{
				dir = absdir(dir + RANDOM() % 3 + RANDOM() % 3 - 2);
			}

			if (op->last_grace)
			{
				op->last_grace--;
			}

			if (op->stats.Dex && !(RANDOM() % op->stats.Dex))
			{
				if (QUERY_FLAG(op, FLAG_CAST_SPELL) && !op->last_grace)
				{
					if (monster_cast_spell(op, part, dir, &rv1))
					{
						/* Add monster casting delay */
						op->last_grace += op->magic;
						return 0;
					}
				}

				if (QUERY_FLAG(op, FLAG_READY_RANGE) && !(RANDOM() % 3))
				{
					if (monster_use_range(op, part, enemy, dir))
					{
						return 0;
					}
				}

				if (QUERY_FLAG(op, FLAG_READY_SKILL) && !(RANDOM() % 3))
				{
					/* Allow skill use AND melee attack */
					monster_use_skill(op, part, enemy, dir);
				}

				if (QUERY_FLAG(op, FLAG_READY_BOW) && !(RANDOM() % 4))
				{
					if (monster_use_bow(op, part, dir) && !(RANDOM() % 2))
					{
						return 0;
					}
				}
			}
		}
	}

	/* TODO: haven't we already done this in check_enemy? */
	get_rangevector(op, enemy, &rv, 0);
	part = rv.part;
	/* dir is now direction towards enemy */
	dir = rv.direction;

	if (QUERY_FLAG(op, FLAG_SCARED) || QUERY_FLAG(op, FLAG_RUN_AWAY))
	{
		dir = absdir(dir + 4);
	}

	if (QUERY_FLAG(op, FLAG_CONFUSED))
	{
		dir = absdir(dir + RANDOM() % 3 + RANDOM() % 3 - 2);
	}

	if (!QUERY_FLAG(op, FLAG_SCARED))
	{
		if (op->attack_move_type & LO4)
		{
			switch (op->attack_move_type & LO4)
			{
				case DISTATT:
					special_dir = dist_att(dir, part, &rv);
					break;

				case RUNATT:
					special_dir = run_att(dir, op, part, &rv);
					break;

				case HITRUN:
					special_dir = hitrun_att(dir, op);
					break;

				case WAITATT:
					special_dir = wait_att(dir, op, part, &rv);
					break;

				case RUSH:
				case ALLRUN:
					break;

				case DISTHIT:
					special_dir = disthit_att(dir, op, part, &rv);
					break;

				case WAIT2:
					special_dir = wait_att2(dir, &rv);
					break;

				default:
					LOG(llevDebug, "Illegal low mon-move: %d\n", op->attack_move_type & LO4);
			}

			if (!special_dir)
			{
				return 0;
			}
		}
	}

	/* Try to move closer to enemy, or follow whatever special attack behaviour is */
	if (!QUERY_FLAG(op, FLAG_STAND_STILL) && (QUERY_FLAG(op, FLAG_SCARED) || QUERY_FLAG(op, FLAG_RUN_AWAY) || !can_hit(part, &rv) || ((op->attack_move_type & LO4) && special_dir != dir)))
	{
		object *aggro_wp = get_aggro_waypoint(op);

		/* TODO: make (intelligent) monsters go to last known position of enemy if out of range/sight */

		/* If special attack move -> follow it instead of going towards enemy */
		if (((op->attack_move_type & LO4) && special_dir != dir))
		{
			aggro_wp = NULL;
			dir = special_dir;
		}

		/* If valid aggro wp (and no special attack), and not scared, use it for movement */
		if (aggro_wp && aggro_wp->enemy && aggro_wp->enemy == op->enemy && rv.distance > 1 && !QUERY_FLAG(op, FLAG_SCARED) && !QUERY_FLAG(op, FLAG_RUN_AWAY))
		{
			waypoint_move(op, aggro_wp);
			return 0;
		}
		else
		{
			int maxdiff = (QUERY_FLAG(op, FLAG_ONLY_ATTACK) || RANDOM() & 1) ? 1 : 2;

			/* Can the monster move directly toward player? */
			if (move_object(op, dir))
			{
				return 0;
			}

			/* Try move around corners if !close */
			for (diff = 1; diff <= maxdiff; diff++)
			{
				/* try different detours */
				/* Try left or right first? */
				int m = 1 - (RANDOM() & 2);

				if (move_object(op, absdir(dir + diff * m)) || move_object(op, absdir(dir - diff * m)))
				{
					return 0;
				}
			}
		}
	}

	/* Eneq(@csd.uu.se): Patch to make RUN_AWAY or SCARED monsters move a random
	 * direction if they can't move away. */
	if (!QUERY_FLAG(op, FLAG_ONLY_ATTACK) && (QUERY_FLAG(op, FLAG_RUN_AWAY) || QUERY_FLAG(op, FLAG_SCARED)))
	{
		if (move_randomly(op))
		{
			return 0;
		}
	}

	/* Hit enemy if possible */
	if (!QUERY_FLAG(op, FLAG_SCARED) && can_hit(part, &rv))
	{
		if (QUERY_FLAG(op, FLAG_RUN_AWAY))
		{
			part->stats.wc -= 10;

			/* As long we are > 0, we are not ready to swing */
			if (op->weapon_speed_left <= 0)
			{
				skill_attack(enemy, part, 0, NULL);
				op->weapon_speed_left += FABS((int) op->weapon_speed_left) + 1;
			}

			part->stats.wc += 10;
		}
		else
		{
			/* As long we are > 0, we are not ready to swing */
			if (op->weapon_speed_left <= 0)
			{
				skill_attack(enemy, part, 0, NULL);
				op->weapon_speed_left += FABS((int) op->weapon_speed_left) + 1;
			}
		}
	}

	/* Might be freed by ghost-attack or hit-back */
	if (OBJECT_FREE(part))
	{
		return 1;
	}

	if (QUERY_FLAG(op, FLAG_ONLY_ATTACK))
	{
		destruct_ob(op);
		return 1;
	}

	return 0;
}

/**
 * Returns the nearest living creature (monster or generator).
 *
 * Modified to deal with tiled maps properly.
 * Also fixed logic so that monsters in the lower directions were more
 * likely to be skipped - instead of just skipping the 'start' number
 * of direction, revisit them after looking at all the other spaces.
 *
 * Note that being this may skip some number of spaces, it will
 * not necessarily find the nearest living creature - it basically
 * chooses one from within a 3 space radius, and since it skips
 * the first few directions, it could very well choose something
 * 3 spaces away even though something directly north is closer.
 * @param npc The NPC object looking for nearest living creature.
 * @return Nearest living creature, NULL if none nearby.
 * @todo can_see_monsterP() is pathfinding function, it does
 * not check visibility. Use obj_in_line_of_sight()? */
static object *find_nearest_living_creature(object *npc)
{
	int i, j = 0, start;
	int nx, ny, friendly_attack = 1;
	mapstruct *m;
	object *tmp;

	if (!QUERY_FLAG(npc, FLAG_BERSERK) && QUERY_FLAG(npc, FLAG_FRIENDLY))
	{
		friendly_attack = 0;
	}

	start = (RANDOM() % 8) + 1;

	for (i = start; j < SIZEOFFREE; j++, i = (i + 1) % SIZEOFFREE)
	{
		nx = npc->x + freearr_x[i];
		ny = npc->y + freearr_y[i];

		if (!(m = get_map_from_coord(npc->map, &nx, &ny)))
		{
			continue;
		}

		/* Quick check - if nothing alive or player skip test for targets */
		if (!(GET_MAP_FLAGS(m, nx, ny) & (P_IS_ALIVE | P_IS_PLAYER)))
		{
			continue;
		}

		tmp = get_map_ob(m, nx, ny);

		/* Attack player and friendly */
		if (friendly_attack)
		{
			while (tmp != NULL && !QUERY_FLAG(tmp, FLAG_MONSTER) && tmp->type != PLAYER)
			{
				tmp = tmp->above;
			}
		}
		else
		{
			while (tmp != NULL && (!QUERY_FLAG(tmp, FLAG_MONSTER) || tmp->type == PLAYER || (QUERY_FLAG(tmp, FLAG_FRIENDLY))))
			{
				tmp = tmp->above;
			}
		}

		if (tmp && can_see_monsterP(m, nx, ny, i))
		{
			return tmp;
		}
	}

	/* Nothing found */
	return NULL;
}

/**
 * Randomly move a monster.
 * @param op The monster object to move.
 * @return 1 if the monster was moved, 0 otherwise. */
static int move_randomly(object *op)
{
	int i, r;
	int dirs[8] = {1, 2, 3, 4, 5, 6, 7, 8};
	object *base = find_base_info_object(op);
	mapstruct *basemap = NULL;
	rv_vector rv;

	if (op->item_race != 255 && op->item_level != 255)
	{
		if ((basemap = ready_map_name(base->slaying, MAP_NAME_SHARED)))
		{
			if (!get_rangevector_from_mapcoords(basemap, base->x, base->y, op->map, op->x, op->y, &rv, RV_NO_DISTANCE))
			{
				basemap = NULL;
			}
		}
	}

	/* Give up to 8 chances for a monster to move randomly */
	for (i = 0; i < 8; i++)
	{
		int t = dirs[i];

		/* Perform a single random shuffle of the remaining directions */
		r = i + (RANDOM() % (8 - i));
		dirs[i] = dirs[r];
		dirs[r] = t;

		r = dirs[i];

		/* Check x and y direction of possible move against limit parameters */
		if (basemap)
		{
			if (op->item_race != 255 && SGN(rv.distance_x) == SGN(freearr_x[r]) && abs(rv.distance_x + freearr_x[r]) > op->item_race)
			{
				continue;
			}

			if (op->item_level != 255 && SGN(rv.distance_y) == SGN(freearr_y[r]) && abs(rv.distance_y + freearr_y[r]) > op->item_level)
			{
				continue;
			}
		}

		if (!blocked_link(op, freearr_x[r], freearr_y[r]) && move_object(op, r))
		{
			return 1;
		}
	}

	return 0;
}

/**
 * Check if object can hit another object.
 * @param ob1 Monster object.
 * @param rv Range vector.
 * @return 1 if can hit, 0 otherwise. */
static int can_hit(object *ob1, rv_vector *rv)
{
	if (QUERY_FLAG(ob1, FLAG_CONFUSED) && !(RANDOM() % 3))
	{
		return 0;
	}

	return abs(rv->distance_x) < 2 && abs(rv->distance_y) < 2;
}

#define MAX_KNOWN_SPELLS 20

/**
 * Choose a random spell this monster could cast.
 * @param monster The monster object.
 * @return Random spell object, NULL if no spell found. */
static object *monster_choose_random_spell(object *monster)
{
	object *altern[MAX_KNOWN_SPELLS], *tmp;
	spell *spell;
	int i = 0, j;

	for (tmp = monster->inv; tmp != NULL; tmp = tmp->below)
	{
		if (tmp->type == ABILITY || tmp->type == SPELLBOOK)
		{
			/* Check and see if it's actually a useful spell */
			if ((spell = find_spell(tmp->stats.sp)) != NULL && !(spell->path & (PATH_INFO | PATH_TRANSMUTE | PATH_TRANSFER | PATH_LIGHT)))
			{
				if (tmp->stats.maxsp)
				{
					for (j = 0; i < MAX_KNOWN_SPELLS && j < tmp->stats.maxsp; j++)
					{
						altern[i++] = tmp;
					}
				}
				else
				{
					altern[i++] = tmp;
				}

				if (i == MAX_KNOWN_SPELLS)
				{
					break;
				}
			}

		}
	}

	if (!i)
	{
		return NULL;
	}

	return altern[RANDOM() % i];
}

/**
 * Tries to make a monster cast a spell.
 * @param head Head of the monster.
 * @param part Part of the monster that we use to cast.
 * @param pl Target.
 * @param dir Direction to cast.
 * @param rv Vector describing where the enemy is.
 * @return 1 if monster casted a spell, 0 otherwise. */
static int monster_cast_spell(object *head, object *part, int dir, rv_vector *rv)
{
	object *spell_item;
	spell *sp;
	int sp_typ, ability;

	if (QUERY_FLAG(head, FLAG_CONFUSED))
	{
		dir = absdir(dir + RANDOM() % 3 + RANDOM() % 3 - 2);
	}

	if ((spell_item = monster_choose_random_spell(head)) == NULL)
	{
		LOG(llevDebug, "DEBUG: monster_cast_spell: No spell found! Turned off spells in %s (%s) (%d,%d)\n", query_name(head, NULL), head->map ? (head->map->name ? head->map->name : "<no map name>") : "<no map!>", head->x, head->y );
		/* Will be turned on when picking up book */
		CLEAR_FLAG(head, FLAG_CAST_SPELL);
		return 0;
	}

	if (spell_item->stats.hp)
	{
		/* Alternate long-range spell: check how far away enemy is */
		if (rv->distance > 6)
		{
			sp_typ = spell_item->stats.hp;
		}
		else
		{
			sp_typ = spell_item->stats.sp;
		}
	}
	else
	{
		sp_typ = spell_item->stats.sp;
	}

	if ((sp = find_spell(sp_typ)) == NULL)
	{
		LOG(llevDebug,"DEBUG: monster_cast_spell: Can't find spell #%d for mob %s (%s) (%d,%d)\n", sp_typ, query_name(head, NULL), head->map ? (head->map->name ? head->map->name : "<no map name>") : "<no map!>", head->x, head->y);
		return 0;
	}

	/* Spell should be cast on caster (ie, heal, strength) */
	if (sp->flags & SPELL_DESC_SELF)
	{
		dir = 0;
	}

	/* Monster doesn't have enough spell-points */
	if (head->stats.sp < SP_level_spellpoint_cost(head, sp_typ))
	{
		return 0;
	}

	ability = (spell_item->type == ABILITY && QUERY_FLAG(spell_item, FLAG_IS_MAGICAL));

	head->stats.sp -= SP_level_spellpoint_cost(head, sp_typ);
	/* Add default cast time from spell force to monster */
	head->last_grace += spell_item->last_grace;

	return cast_spell(part, part, dir, sp_typ, ability, spellNormal, NULL);
}

/**
 * Allow monster to use a skill.
 * @param head Head of the monster.
 * @param part Part of the monster that may use a skill.
 * @param pl Target.
 * @param dir Direction to use the skill.
 * @return 1 if monster used a skill, 0 otherwise. */
static int monster_use_skill(object *head, object *part, object *pl, int dir)
{
	object *skill, *owner;
	rv_vector rv;

	if (!(dir = path_to_player(part, pl, 0)))
	{
		return 0;
	}

	if (QUERY_FLAG(head, FLAG_FRIENDLY) && (owner = get_owner(head)) != NULL)
	{
		/* Might hit owner with skill - thrown rocks for example? */
		if (get_rangevector(head, owner, &rv, 0) && dirdiff(dir, rv.direction) < 1)
		{
			return 0;
		}
	}

	if (QUERY_FLAG(head, FLAG_CONFUSED))
	{
		dir = absdir(dir + RANDOM() % 3 + RANDOM() % 3 - 2);
	}

	/* skill selection - monster will use the next unused skill.
	 * well...the following scenario will allow the monster to
	 * toggle between 2 skills. One day it would be nice to make
	 * more skills available to monsters. */
	for (skill = head->inv; skill != NULL; skill = skill->below)
	{
		if (skill->type == SKILL && skill != head->chosen_skill)
		{
			head->chosen_skill = skill;
			break;
		}
	}

	if (!skill && !head->chosen_skill)
	{
		LOG(llevDebug, "Error: Monster %s (%d) has FLAG_READY_SKILL without skill.\n", head->name, head->count);
		CLEAR_FLAG(head, FLAG_READY_SKILL);
		return 0;
	}

	/* Use skill */
	return do_skill(head, dir);
}

/**
 * Monster will use a ranged attack (HORN, WAND, ...).
 * @param head Head of the monster.
 * @param part Part of the monster that can do a range attack.
 * @param pl Target.
 * @param dir Direction to fire.
 * @return 1 if monster casted a spell, 0 otherwise. */
static int monster_use_range(object *head, object *part, object *pl, int dir)
{
	object *range, *owner;
	int found_one = 0;
	rv_vector rv;

	if (!(dir = path_to_player(part, pl, 0)))
	{
		return 0;
	}

	if (QUERY_FLAG(head, FLAG_FRIENDLY) && (owner = get_owner(head)) != NULL)
	{
		/* Might hit owner with spell */
		if (get_rangevector(head, owner, &rv, 0) && dirdiff(dir, rv.direction) < 2)
		{
			return 0;
		}
	}

	if (QUERY_FLAG(head, FLAG_CONFUSED))
	{
		dir = absdir(dir + RANDOM() % 3 + RANDOM() % 3 - 2);
	}

	for (range = head->inv; range; range = range->below)
	{
		if (!QUERY_FLAG(range, FLAG_APPLIED))
		{
			continue;
		}

		if (range->type == WAND)
		{
			found_one = 1;

			if (range->stats.food <= 0)
			{
				manual_apply(head, range, 0);
				CLEAR_FLAG(head, FLAG_READY_RANGE);

				if (range->arch)
				{
					CLEAR_FLAG(range, FLAG_ANIMATE);
					range->face = range->arch->clone.face;
					range->speed = 0;
					update_ob_speed(range);
				}

				continue;
			}

			cast_spell(part, range, dir, range->stats.sp, 0, spellWand, NULL);
			range->stats.food--;

			return 1;
		}
		else if (range->type == ROD || range->type == HORN)
		{
			found_one = 1;

			/* Not recharged enough yet */
			if (range->stats.hp < spells[range->stats.sp].sp)
			{
				continue;
			}

			cast_spell(part, range, dir, range->stats.sp, 0, range->type == ROD ? spellRod : spellHorn, NULL);
		}
	}

	if (found_one)
	{
		return 0;
	}

	LOG(llevBug, "BUG: Monster %s (%d) HAS_READY_WAND() without wand.\n", query_name(head, NULL), head->count);
	CLEAR_FLAG(head, FLAG_READY_RANGE);
	return 0;
}

/**
 * Tries to make a (part of a) monster fire a bow.
 * @param head Head of the monster.
 * @param part Part of the monster that we use to fire.
 * @param pl Target.
 * @param dir Direction to cast.
 * @return 1 if monster fired something, 0 otherwise. */
static int monster_use_bow(object *head, object *part, int dir)
{
	object *bow, *arrow;
	int tag;

	if (QUERY_FLAG(head, FLAG_CONFUSED))
	{
		dir = absdir(dir + RANDOM() % 3 + RANDOM() % 3 - 2);
	}

	for (bow = head->inv; bow != NULL; bow = bow->below)
	{
		if (bow->type == BOW && QUERY_FLAG(bow, FLAG_APPLIED))
		{
			break;
		}
	}

	if (bow == NULL)
	{
		LOG(llevBug, "BUG: Monster %s (%d) HAS_READY_BOW() without bow.\n", query_name(head, NULL), head->count);
		CLEAR_FLAG(head, FLAG_READY_BOW);
		return 0;
	}

	if ((arrow = find_arrow(head, bow->race)) == NULL)
	{
		/* Out of arrows */
		manual_apply(head, bow, 0);
		CLEAR_FLAG(head, FLAG_READY_BOW);
		return 0;
	}

	/* An infinite arrow, dupe it. */
	if (QUERY_FLAG(arrow, FLAG_SYS_OBJECT))
	{
		object *new_arrow = get_object();
		copy_object(arrow, new_arrow);
		CLEAR_FLAG(new_arrow, FLAG_SYS_OBJECT);
		new_arrow->nrof = 0;

		/* Setup the self destruction */
		new_arrow->stats.food = 20;
		arrow = new_arrow;
	}
	else
	{
		arrow = get_split_ob(arrow, 1, NULL, 0);
	}

	set_owner(arrow, head);
	arrow->direction = dir;
	arrow->x = part->x, arrow->y = part->y;
	arrow->speed = 1;
	update_ob_speed(arrow);
	arrow->speed_left = 0;
	SET_ANIMATION(arrow, (NUM_ANIMATIONS(arrow) / NUM_FACINGS(arrow)) * dir);
	arrow->level = head->level;
	/* Save original wc and dam */
	arrow->last_heal = arrow->stats.wc;
	arrow->stats.hp = arrow->stats.dam;
	arrow->stats.dam += bow->stats.dam + bow->magic + arrow->magic;
	arrow->stats.dam = FABS((int) ((float) (arrow->stats.dam * LEVEL_DAMAGE(head->level))));
	arrow->stats.wc = 10 + (bow->magic + bow->stats.wc + arrow->magic + arrow->stats.wc-head->level);
	arrow->stats.wc_range = bow->stats.wc_range;
	arrow->map = head->map;
	/* We use fixed value for mobs */
	arrow->last_sp = 12;
	SET_FLAG(arrow, FLAG_FLYING);
	SET_FLAG(arrow, FLAG_IS_MISSILE);
	SET_FLAG(arrow, FLAG_FLY_ON);
	SET_FLAG(arrow, FLAG_WALK_ON);
	tag = arrow->count;
	arrow->stats.grace = arrow->last_sp;
	arrow->stats.maxgrace = 60 + (RANDOM() % 12);
	play_sound_map(arrow->map, arrow->x, arrow->y, SOUND_THROW, SOUND_NORMAL);

	if (!insert_ob_in_map(arrow, head->map, head, 0))
	{
		return 1;
	}

	if (!was_destroyed(arrow, tag))
	{
		move_arrow(arrow);
	}

	return 1;
}

/**
 * NPC calls for help.
 * @param op NPC. */
void npc_call_help(object *op)
{
	int x, y, xt, yt;
	mapstruct *m;
	object *npc;

	for (x = -3; x < 4; x++)
	{
		for (y = -3; y < 4; y++)
		{
			xt = op->x + x;
			yt = op->y + y;

			if (!(m = get_map_from_coord(op->map, &xt, &yt)))
			{
				continue;
			}

			for (npc = get_map_ob(m, xt, yt); npc != NULL; npc = npc->above)
			{
				if (QUERY_FLAG(npc, FLAG_ALIVE) && QUERY_FLAG(npc, FLAG_UNAGGRESSIVE))
				{
					set_npc_enemy(npc, op->enemy, NULL);
				}
			}
		}
	}
}

/**
 * Monster does a distance attack.
 * @param dir Direction.
 * @param enemy Enemy.
 * @param part Part of the object.
 * @param rv Range vector.
 * @return New direction. */
static int dist_att(int dir, object *part, rv_vector *rv)
{
	if (can_hit(part, rv))
	{
		return dir;
	}

	if (rv->distance < 10)
	{
		return absdir(dir + 4);
	}
	else if (rv->distance > 18)
	{
		return dir;
	}

	return 0;
}

/**
 * Monster runs.
 * @param dir Direction.
 * @param ob The monster.
 * @param enemy Enemy.
 * @param part Part of the monster.
 * @param rv Range vector.
 * @return New direction. */
static int run_att(int dir, object *ob, object *part, rv_vector *rv)
{
	if ((can_hit(part, rv) && ob->move_status < 20) || ob->move_status < 20)
	{
		ob->move_status++;
		return dir;
	}
	else if (ob->move_status > 20)
	{
		ob->move_status = 0;
	}

	return absdir(dir + 4);
}

/**
 * Hit and run type of attack.
 * @param dir Direction.
 * @param ob Monster.
 * @return New direction. */
static int hitrun_att(int dir, object *ob)
{
	if (ob->move_status++ < 25)
	{
		return dir;
	}
	else if (ob->move_status < 50)
	{
		return absdir(dir + 4);
	}
	else
	{
		ob->move_status = 0;
	}

	return absdir(dir + 4);
}

/**
 * Wait, and attack.
 * @param dir Direction.
 * @param ob Monster.
 * @param enemy Enemy.
 * @param part Part of the monster.
 * @param rv Range vector.
 * @return New direction. */
static int wait_att(int dir, object *ob, object *part, rv_vector *rv)
{
	if (ob->move_status || can_hit(part, rv))
	{
		ob->move_status++;
	}

	if (ob->move_status == 0)
	{
		return 0;
	}
	else if (ob->move_status < 10)
	{
		return dir;
	}
	else if (ob->move_status < 15)
	{
		return absdir(dir + 4);
	}

	ob->move_status = 0;
	return 0;
}

/**
 * Distance hit attack.
 * @param dir Direction.
 * @param ob Monster.
 * @param enemy Enemy.
 * @param part Part of the monster.
 * @param rv Range vector.
 * @return New direction. */
static int disthit_att(int dir, object *ob, object *part, rv_vector *rv)
{
	if (ob->stats.maxhp && (ob->stats.hp * 100) / ob->stats.maxhp < ob->run_away)
	{
		return absdir(dir + 4);
	}

	return dist_att(dir, part, rv);
}

/**
 * Wait and attack.
 * @param dir Direction.
 * @param rv Range vector.
 * @return New direction. */
static int wait_att2(int dir, rv_vector *rv)
{
	if (rv->distance < 9)
	{
		return absdir(dir + 4);
	}

	return 0;
}

/**
 * Circle type of move.
 * @param ob Monster. */
static void circ1_move(object *ob)
{
	static const int circle[12] = {3, 3, 4, 5, 5, 6, 7, 7, 8, 1, 1, 2};

	if (++ob->move_status > 11)
	{
		ob->move_status = 0;
	}

	if (!(move_object(ob, circle[ob->move_status])))
	{
		move_object(ob, RANDOM() % 8 + 1);
	}
}

/**
 * Different type of circle type move.
 * @param ob Monster. */
static void circ2_move(object *ob)
{
	static const int circle[20] = {3, 3, 3, 4, 4, 5, 5, 5, 6, 6, 7, 7, 7, 8, 8, 1, 1, 1, 2, 2};

	if (++ob->move_status > 19)
	{
		ob->move_status = 0;
	}

	if (!(move_object(ob, circle[ob->move_status])))
	{
		move_object(ob, RANDOM() % 8 + 1);
	}
}

/**
 * Vertical pace movement.
 * @param ob Monster. */
static void pace_movev(object *ob)
{
	if (ob->move_status++ > 6)
	{
		ob->move_status = 0;
	}

	if (ob->move_status < 4)
	{
		move_object(ob, 5);
	}
	else
	{
		move_object(ob, 1);
	}
}

/**
 * Horizontal pace movement.
 * @param ob Monster. */
static void pace_moveh(object *ob)
{
	if (ob->move_status++ > 6)
	{
		ob->move_status = 0;
	}

	if (ob->move_status < 4)
	{
		move_object(ob, 3);
	}
	else
	{
		move_object(ob, 7);
	}
}

/**
 * Another type of vertical pace movement.
 * @param ob Monster. */
static void pace2_movev(object *ob)
{
	if (ob->move_status++ > 16)
	{
		ob->move_status = 0;
	}

	if (ob->move_status < 6)
	{
		move_object(ob, 5);
	}
	else if (ob->move_status < 8)
	{
		return;
	}
	else if (ob->move_status < 13)
	{
		move_object(ob, 1);
	}
}

/**
 * Another type of horizontal pace movement.
 * @param ob Monster. */
static void pace2_moveh(object *ob)
{
	if (ob->move_status++ > 16)
	{
		ob->move_status = 0;
	}

	if (ob->move_status < 6)
	{
		move_object(ob, 3);
	}
	else if (ob->move_status < 8)
	{
		return;
	}
	else if (ob->move_status < 13)
	{
		move_object(ob, 7);
	}
}

/**
 * Random movement.
 * @param ob Monster. */
static void rand_move(object *ob)
{
	int i;

	if (ob->move_status < 1 || ob->move_status > 8 || !(move_object(ob, ob->move_status || !(RANDOM() % 9))))
	{
		for (i = 0; i < 5; i++)
		{
			if (move_object(ob, ob->move_status = RANDOM() % 8 + 1))
			{
				return;
			}
		}
	}
}

/**
 * Free NPC's messages.
 * @param msgs Messages. */
static void free_messages(msglang *msgs)
{
	int messages, keywords;

	if (!msgs)
	{
		return;
	}

	for (messages = 0; msgs->messages[messages]; messages++)
	{
		if (msgs->keywords[messages])
		{
			for (keywords = 0; msgs->keywords[messages][keywords]; keywords++)
			{
				free(msgs->keywords[messages][keywords]);
			}

			free(msgs->keywords[messages]);
		}

		free(msgs->messages[messages]);
	}

	free(msgs->messages);
	free(msgs->keywords);
	free(msgs);
}

/**
 * Parse NPC's message.
 * @param msg Message to parse.
 * @return Newly allocated ::msglang structure. */
static msglang *parse_message(const char *msg)
{
	msglang *msgs;
	int nrofmsgs, msgnr, i;
	char *cp, *line, *last, *tmp;
	char *buf = strdup_local(msg);

	/* First find out how many messages there are. A @ for each. */
	for (nrofmsgs = 0, cp = buf; *cp; cp++)
	{
		if (*cp == '@')
		{
			nrofmsgs++;
		}
	}

	if (!nrofmsgs)
	{
		free(buf);
		return NULL;
	}

	msgs = (msglang *) malloc(sizeof(msglang));
	msgs->messages = (char **) malloc(sizeof(char *) * (nrofmsgs + 1));
	msgs->keywords = (char ***) malloc(sizeof(char **) * (nrofmsgs + 1));

	for (i = 0; i <= nrofmsgs; i++)
	{
		msgs->messages[i] = NULL;
		msgs->keywords[i] = NULL;
	}

	for (last = NULL, cp = buf, msgnr = 0;*cp; cp++)
	{
		if (*cp == '@')
		{
			int nrofkeywords, keywordnr;

			*cp = '\0';
			cp++;

			if (last != NULL)
			{
				msgs->messages[msgnr++] = strdup_local(last);
				tmp = msgs->messages[msgnr - 1];

				for (i = (int) strlen(tmp); i; i--)
				{
					if (*(tmp + i) && *(tmp + i) != 0x0a && *(tmp + i) != 0x0d)
					{
						break;
					}

					*(tmp + i) = 0;
				}
			}

			if (strncmp(cp, "match", 5))
			{
				LOG(llevBug, "BUG: parse_message(): Unsupported command in message.\n");
				free(buf);
				return NULL;
			}

			for (line = cp + 6, nrofkeywords = 0; *line != '\n' && *line; line++)
			{
				if (*line == '|')
				{
					nrofkeywords++;
				}
			}

			if (line > cp + 6)
			{
				nrofkeywords++;
			}

			if (nrofkeywords < 1)
			{
				LOG(llevBug, "BUG: parse_message(): Too few keywords in message.\n");
				free(buf);
				free_messages(msgs);
				return NULL;
			}

			msgs->keywords[msgnr] = (char **) malloc(sizeof(char **) * (nrofkeywords +1));
			msgs->keywords[msgnr][nrofkeywords] = NULL;
			last = cp + 6;
			cp = strchr(cp, '\n');

			if (cp != NULL)
			{
				cp++;
			}

			for (line = last, keywordnr = 0; line < cp && *line; line++)
			{
				if (*line == '\n' || *line == '|')
				{
					*line = '\0';

					if (last != line)
					{
						msgs->keywords[msgnr][keywordnr++] = strdup_local(last);
					}
					else
					{
						if (keywordnr < nrofkeywords)
						{
							/* Whoops, Either got || or |\n in @match. Not good */
							msgs->keywords[msgnr][keywordnr++] = strdup_local("xxxx");
							/* We need to set the string to something sensible to
							 * prevent crashes later. Unfortunately, we can't set to
							 * NULL, as that's used to terminate the for loop in
							 * talk_to_npc.  Using xxxx should also help map
							 * developers track down the problem cases. */
							LOG(llevBug, "BUG: parse_message(): Tried to set a zero length message in parse_message\n");
							/* I think this is a error worth reporting at a reasonably
							 * high level. When logging gets redone, this should
							 * be something like MAP_ERROR, or whatever gets put in
							 * place. */
							if (keywordnr > 1)
							{
								/* This is purely addtional information, should only be gieb if asked */
								LOG(llevDebug, "Msgnr %d, after keyword %s\n", msgnr + 1, msgs->keywords[msgnr][keywordnr - 2]);
							}
							else
							{
								LOG(llevDebug, "Msgnr %d, first keyword\n", msgnr + 1);
							}
						}
					}

					last = line + 1;
				}
			}

			/* Your eyes aren't decieving you, this is code repetition. However,
			 * the above code doesn't catch the case where line < cp going into the
			 * for loop, skipping the above code completely, and leaving undefined
			 * data in the keywords array. This patches it up and solves a crash
			 * bug.  */
			if (keywordnr < nrofkeywords)
			{
				LOG(llevBug, "BUG: parse_message(): Map developer screwed up match statement in parse_message\n");

				if (keywordnr > 1)
				{
					LOG(llevDebug, "Msgnr %d, after keyword %s\n", msgnr + 1, msgs->keywords[msgnr][keywordnr - 2]);
				}
				else
				{
					LOG(llevDebug, "Msgnr %d, first keyword\n", msgnr + 1);
				}
			}

			last = cp;
		}
	}

	if (last != NULL)
	{
		msgs->messages[msgnr++] = strdup_local(last);
	}

	tmp = msgs->messages[msgnr - 1];

	for (i = (int) strlen(tmp); i; i--)
	{
		if (*(tmp + i) && *(tmp + i) != 0x0a && *(tmp + i) != 0x0d)
		{
			break;
		}

		*(tmp + i) = '\0';
	}

	free(buf);
	return msgs;
}

/**
 * Communication between NPC and player.
 * @param op Who is saying something.
 * @param txt What was said. */
void communicate(object *op, char *txt)
{
	object *npc;
	mapstruct *m;
	int i, xt, yt;
	char buf[HUGE_BUF];

	if (!txt)
	{
		return;
	}

	/* Makes it possible to do something like this in Python:
	 * monster.Communicate("/dance") */
	if (*txt == '/' && op->type != PLAYER)
	{
		CommArray_s *csp;
		char *cp = NULL;

		/* Remove the command from the parameters */
		strncpy(buf, txt, HUGE_BUF - 1);
		buf[HUGE_BUF - 1] = '\0';

		cp = strchr(buf, ' ');

		if (cp)
		{
			*(cp++) = '\0';
			cp = cleanup_string(cp);

			if (cp && *cp == '\0')
			{
				cp = NULL;
			}
		}

		csp = find_command_element(buf, CommunicationCommands, CommunicationCommandSize);

		if (csp)
		{
			csp->func(op, cp);
			return;
		}

		return;
	}

	snprintf(buf, sizeof(buf), "%s says: ", query_name(op, NULL));
	strncat(buf, txt, MAX_BUF - strlen(buf) - 1);
	buf[MAX_BUF - 1] = '\0';

	if (op->type == PLAYER)
	{
		new_info_map(NDI_WHITE | NDI_PLAYER | NDI_SAY, op->map, op->x, op->y, MAP_INFO_NORMAL, buf);
	}
	else
	{
		new_info_map(NDI_WHITE, op->map, op->x, op->y, MAP_INFO_NORMAL, buf);
	}

	for (i = 0; i <= SIZEOFFREE2; i++)
	{
		xt = op->x + freearr_x[i];
		yt = op->y + freearr_y[i];

		if ((m = get_map_from_coord(op->map, &xt, &yt)))
		{
			/* Quick check if we have a magic ear. */
			if (GET_MAP_FLAGS(m, xt, yt) & (P_MAGIC_EAR | P_IS_ALIVE))
			{
				/* Browse only on demand */
				for (npc = get_map_ob(m, xt, yt); npc != NULL; npc = npc->above)
				{
					/* Avoid talking to self */
					if (op != npc)
					{
						/* The ear. */
						if (npc->type == MAGIC_EAR)
						{
							talk_to_wall(npc, txt);
						}
						else if (QUERY_FLAG(npc, FLAG_ALIVE))
						{
							talk_to_npc(op, npc, txt);
						}
					}
				}
			}
		}
	}
}

/**
 * Give an object the chance to handle something being said.
 *
 * Plugin hooks will be called.
 * @param op Who is talking.
 * @param npc Object to try to talk to. Can be an NPC or a MAGIC_EAR.
 * @param txt What op is saying.
 * @return 0 if text was handled by a plugin or not handled, 1 if handled
 * internally by the server. */
int talk_to_npc(object *op, object *npc, char *txt)
{
	msglang *msgs;
	int i, j;
	object *cobj;

	if (npc->event_flags & EVENT_FLAG_SAY)
	{
		/* Trigger the SAY event */
		trigger_event(EVENT_SAY, op, npc, NULL, txt, 0, 0, 0, SCRIPT_FIX_ACTIVATOR);
		return 0;
	}

	/* Here we let the objects inside inventories hear and answer, too.
	 * This allows the existence of "intelligent" weapons you can discuss
	 * with. */
	for (cobj = npc->inv; cobj != NULL; )
	{
		if (cobj->event_flags & EVENT_FLAG_SAY)
		{
			/* Trigger the SAY event */
			trigger_event(EVENT_SAY, op, cobj, npc, txt, 0, 0, 0, SCRIPT_FIX_ACTIVATOR);
			return 0;
		}

		cobj = cobj->below;
	}

	if (npc->msg == NULL || *npc->msg != '@')
	{
		return 0;
	}

	if ((msgs = parse_message(npc->msg)) == NULL)
	{
		return 0;
	}

	for (i = 0; msgs->messages[i]; i++)
	{
		for (j = 0; msgs->keywords[i][j]; j++)
		{
			if (msgs->keywords[i][j][0] == '*' || re_cmp(txt, msgs->keywords[i][j]))
			{
				char buf[MAX_BUF];

				/* NPC talks to another one - show both in white. */
				if (op->type != PLAYER)
				{
					/* If message starts with '/', we assume an emote. */
					if (*msgs->messages[i] == '/')
					{
						CommArray_s *csp;
						char *cp = NULL, buf[MAX_BUF];

						strncpy(buf, msgs->messages[i], MAX_BUF - 1);
						buf[MAX_BUF - 1] = '\0';
						cp = strchr(buf, ' ');

						if (cp)
						{
							*(cp++) = '\0';
							cp = cleanup_string(cp);

							if (cp && *cp == '\0')
							{
								cp = NULL;
							}

							if (cp && *cp == '%')
							{
								cp = (char *) op->name;
							}
						}

						csp = find_command_element(buf, CommunicationCommands, CommunicationCommandSize);

						if (csp)
						{
							csp->func(npc, cp);
						}
					}
					else
					{
						snprintf(buf, sizeof(buf), "\n%s says: %s", query_name(npc, NULL), msgs->messages[i]);
						new_info_map_except(NDI_UNIQUE, op->map, op->x, op->y, MAP_INFO_NORMAL,op, op, buf);
					}
				}
				/* If npc is talking to a player, show in navy and with
				 * separate "xx says:" lines. */
				else
				{
					/* If message starts with '/', we assume an emote. */
					if (*msgs->messages[i] == '/')
					{
						CommArray_s *csp;
						char *cp = NULL, buf[MAX_BUF];

						strncpy(buf, msgs->messages[i], MAX_BUF - 1);
						buf[MAX_BUF - 1] = '\0';
						cp = strchr(buf, ' ');

						if (cp)
						{
							*(cp++) = '\0';
							cp = cleanup_string(cp);

							if (cp && *cp == '\0')
							{
								cp = NULL;
							}

							if (cp && *cp == '%')
							{
								cp = (char *) op->name;
							}
						}

						csp = find_command_element(buf, CommunicationCommands, CommunicationCommandSize);

						if (csp)
						{
							csp->func(npc, cp);
						}
					}
					else
					{
						new_draw_info_format(NDI_NAVY | NDI_UNIQUE, 0, op, "\n%s says:", query_name(npc, NULL));
						new_draw_info(NDI_NAVY | NDI_UNIQUE, 0, op, msgs->messages[i]);
						sprintf(buf, "%s talks to %s.", query_name(npc, NULL), query_name(op, NULL));
						new_info_map_except(NDI_UNIQUE, op->map, op->x, op->y, MAP_INFO_NORMAL, op, op, buf);
					}
				}

				free_messages(msgs);
				return 1;
			}
		}
	}

	free_messages(msgs);
	return 0;
}

/**
 * Talk to a magic ear.
 * @param npc The magic ear.
 * @param txt Text said.
 * @return 1 if text matches something, 0 otherwise. */
static int talk_to_wall(object *npc, char *txt)
{
	msglang *msgs;
	int i, j;

	if (npc->msg == NULL || *npc->msg != '@')
	{
		return 0;
	}

	if ((msgs = parse_message(npc->msg)) == NULL)
	{
		return 0;
	}

	for (i = 0; msgs->messages[i]; i++)
	{
		for (j = 0; msgs->keywords[i][j]; j++)
		{
			if (msgs->keywords[i][j][0] == '*' || re_cmp(txt, msgs->keywords[i][j]))
			{
				if (msgs->messages[i] && *msgs->messages[i] != 0)
				{
					new_info_map(NDI_NAVY | NDI_UNIQUE, npc->map, npc->x, npc->y, MAP_INFO_NORMAL, msgs->messages[i]);
				}

				free_messages(msgs);
				use_trigger(npc);
				return 1;
			}
		}
	}

	free_messages(msgs);
	return 0;
}

/**
 * Check if object op is friend of obj.
 * @param op The first object
 * @param obj The second object to check against the first one
 * @return 1 if both objects are friends, 0 otherwise */
int is_friend_of(object *op, object *obj)
{
	/* TODO: Add a few other odd types here, such as god and golem */
	if (!obj->type == PLAYER || !obj->type == MONSTER || !op->type == PLAYER || !op->type == MONSTER || op == obj)
	{
		return 0;
	}

	/* If on PVP area, they won't be friendly */
	if (pvp_area(op, obj))
	{
		return 0;
	}

	/* TODO: This needs to be sorted out better */
	if (QUERY_FLAG(op, FLAG_FRIENDLY) || op->type == PLAYER)
	{
		if (!QUERY_FLAG(obj, FLAG_MONSTER) || QUERY_FLAG(obj, FLAG_FRIENDLY) || obj->type == PLAYER)
		{
			return 1;
		}
	}
	else
	{
		if (!QUERY_FLAG(obj, FLAG_FRIENDLY) && obj->type != PLAYER)
		{
			return 1;
		}
	}

	return 0;
}
