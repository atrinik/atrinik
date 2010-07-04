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
 * This handles all attacks, magical or not. */

#include <global.h>

#define ATTACK_HIT_DAMAGE(_op, _anum)       dam = dam * ((double) _op->attack[_anum] * (double) 0.01); dam >= 1.0f ? (damage = (int) dam) : (damage = 1)
#define ATTACK_PROTECT_DAMAGE(_op, _anum)   dam = dam * ((double) (100 - _op->protection[_anum]) * (double) 0.01)

static void thrown_item_effect(object *hitter, object *victim);
static int get_attack_mode(object **target, object **hitter,int *simple_attack);
static int abort_attack(object *target, object *hitter, int simple_attack);
static int attack_ob_simple(object *op, object *hitter, int base_dam, int base_wc);
static void send_attack_msg(object *op, object *hitter, int attacknum, int dam, int damage);
static int hit_player_attacktype(object *op, object *hitter, int damage, uint32 attacknum);
static void poison_player(object *op, object *hitter, float dam);
static void slow_living(object *op);
static void blind_living(object *op, object *hitter, int dam);
static int adj_attackroll(object *hitter, object *target);
static int is_aimed_missile(object *op);

/**
 * Simple wrapper for attack_ob_simple(), will use hitter's values.
 * @param op Victim.
 * @param hitter Attacker.
 * @return Dealt damage. */
int attack_ob(object *op, object *hitter)
{
	if (op->head)
	{
		op = op->head;
	}

	if (hitter->head)
	{
		hitter = hitter->head;
	}

	return attack_ob_simple(op, hitter, hitter->stats.dam, hitter->stats.wc);
}

/**
 * Handles simple attack cases.
 * @param op Victim.
 * @param hitter Attacker.
 * @param base_dam Damage to do.
 * @param base_wc WC to hit with.
 * @return Dealt damage. */
static int attack_ob_simple(object *op, object *hitter, int base_dam, int base_wc)
{
	int simple_attack, roll, dam = 0;
	tag_t op_tag, hitter_tag;

	if (op->head)
	{
		op = op->head;
	}

	if (hitter->head)
	{
		hitter = hitter->head;
	}

	if (get_attack_mode(&op, &hitter, &simple_attack))
	{
		return 1;
	}

	/* Trigger the ATTACK event */
	trigger_event(EVENT_ATTACK, hitter, hitter, op, NULL, 0, base_dam, base_wc, SCRIPT_FIX_ALL);

	op_tag = op->count;
	hitter_tag = hitter->count;

	if (!hitter->stats.wc_range)
	{
		LOG(llevDebug, "BUG attack.c: hitter %s has wc_range == 0! (set to 20)\n", query_name(hitter, NULL));
		hitter->stats.wc_range = 20;
	}

	roll = rndm(0, hitter->stats.wc_range);

	/* Adjust roll for various situations. */
	if (!simple_attack)
	{
		roll += adj_attackroll(hitter, op);
	}

	/* So we do one swing */
	if (hitter->type == PLAYER)
	{
		CONTR(hitter)->anim_flags |= PLAYER_AFLAG_ENEMY;
	}

	/* Force player to face enemy */
	if (hitter->type == PLAYER)
	{
		rv_vector dir;

		if (get_rangevector(hitter, op, &dir, RV_NO_DISTANCE))
		{
			if (hitter->head)
			{
				hitter->head->anim_enemy_dir = dir.direction;
				hitter->head->facing = dir.direction;
			}
			else
			{
				hitter->anim_enemy_dir = dir.direction;
				hitter->facing = dir.direction;
			}
		}
	}

	/* See if we hit the creature */
	if (roll >= hitter->stats.wc_range || op->stats.ac <= base_wc + roll)
	{
		int hitdam = base_dam;

		/* At this point NO ONE will still sleep */
		CLEAR_FLAG(op, FLAG_SLEEP);

		if (hitter->type == ARROW)
		{
			play_sound_map(hitter->map, SOUND_ARROW_HIT, NULL, hitter->x, hitter->y, 0, 0);
		}
		else
		{
			if (hitter->attack[ATNR_SLASH])
			{
				play_sound_map(hitter->map, SOUND_HIT_SLASH, NULL, hitter->x, hitter->y, 0, 0);
			}
			else if (hitter->attack[ATNR_CLEAVE])
			{
				play_sound_map(hitter->map, SOUND_HIT_CLEAVE, NULL, hitter->x, hitter->y, 0, 0);
			}
			else if (hitter->attack[ATNR_IMPACT])
			{
				play_sound_map(hitter->map, SOUND_HIT_IMPACT, NULL, hitter->x, hitter->y, 0, 0);
			}
			else
			{
				play_sound_map(hitter->map, SOUND_HIT_PIERCE, NULL, hitter->x, hitter->y, 0, 0);
			}
		}

		if (!simple_attack)
		{
			/* Thrown items (hitter) will have various effects
			 * when they hit the victim. For things like thrown daggers,
			 * this sets 'hitter' to the actual dagger, and not the
			 * wrapper object. */
			thrown_item_effect(hitter, op);

			if (was_destroyed(hitter, hitter_tag) || was_destroyed(op, op_tag) || abort_attack(op, hitter, simple_attack))
			{
				return dam;
			}
		}

		/* Need to do at least 1 damage, otherwise there is no point
		 * to go further and it will cause FPE's below. */
		if (hitdam <= 0)
		{
			hitdam = 1;
		}

		/* Handle monsters that hit back */
		if (!simple_attack && QUERY_FLAG(op, FLAG_HITBACK) && IS_LIVE(hitter))
		{
			hit_player(hitter, rndm(0, op->stats.dam), op, AT_PHYSICAL);

			if (was_destroyed(op, op_tag) || was_destroyed(hitter, hitter_tag) || abort_attack(op, hitter, simple_attack))
			{
				return dam;
			}
		}

		dam = hit_player(op, rndm(hitdam / 2 + 1, hitdam), hitter, AT_PHYSICAL);

		if (was_destroyed(op, op_tag) || was_destroyed(hitter, hitter_tag) || abort_attack(op, hitter, simple_attack))
		{
			return dam;
		}
	}
	/* We missed */
	else
	{
		if (hitter->type != ARROW)
		{
			if (hitter->type == PLAYER)
			{
				play_sound_map(hitter->map, SOUND_MISS_PLAYER, NULL, hitter->x, hitter->y, 0, 0);
				new_draw_info_format(NDI_ORANGE, hitter, "You miss %s!", op->name);

				if (op->type == PLAYER)
				{
					new_draw_info_format(NDI_PURPLE, op, "%s misses you!", hitter->name);
				}
			}
			else
			{
				play_sound_map(hitter->map, SOUND_MISS_MOB, NULL, hitter->x, hitter->y, 0, 0);
				new_draw_info_format(NDI_PURPLE, op, "%s misses you!", hitter->name);
			}
		}
	}

	return dam;
}

/**
 * Object is attacked by something.
 *
 * This isn't used just for players, but in fact most objects.
 * @param op Object to be hit.
 * @param dam Base damage - protections/vulnerabilities/slaying matches
 * can modify it.
 * @param hitter What is hitting the object.
 * @param type Attacktype.
 * @return Dealt damage. */
int hit_player(object *op, int dam, object *hitter, int type)
{
	object *hit_obj, *hitter_owner, *target_obj;
	int maxdam = 0;
	int attacknum, hit_level;
	int simple_attack;
	tag_t op_tag, hitter_tag;
	int rtn_kill = 0;

	/* If our target has no_damage 1 set or is wiz, we can't hurt him. */
	if (QUERY_FLAG (op, FLAG_WIZ) || QUERY_FLAG(op, FLAG_INVULNERABLE))
	{
		return 0;
	}

	if (hitter->head)
	{
		hitter = hitter->head;
	}

	if (op->head)
	{
		op = op->head;
	}

	/* Check if the object to hit has any HP left */
	if (op->stats.hp < 0)
	{
		return 0;
	}

	/* Get the hitter's owner. */
	hitter_owner = get_owner(hitter);

	/* Sanity check: If the hitter has ownercount (so it had an owner)
	 * but the owner itself is no longer valid, we won't do any damage,
	 * otherwise player could fire an arrow, logout, and the arrow itself
	 * would cause damage to anything it hits, even friendly creatures. */
	if (hitter->ownercount && !hitter_owner)
	{
		return 0;
	}

	if (hitter_owner)
	{
		hit_obj = hitter_owner;
	}
	else
	{
		hit_obj = hitter;
	}

	if (!(target_obj = get_owner(op)))
	{
		target_obj = op;
	}

	/* Get from hitter object the right skill level. */
	if (hit_obj->type == PLAYER)
	{
		hit_level = SK_level(hit_obj);
	}
	else
	{
		hit_level = hitter->level;
	}

	/* Very useful sanity check */
	if (hit_level == 0 || target_obj->level == 0)
	{
		LOG(llevDebug, "DEBUG: hit_player(): hit or target object level == 0(h:>%s< (o:>%s<) l->%d t:>%s< (>%s<)(o:>%s<) l->%d\n", query_name(hitter, NULL), query_name(get_owner(hitter), NULL), hit_level, query_name(op, NULL), target_obj->arch->name, query_name(get_owner(op), NULL), target_obj->level);
	}

	/* Do not let friendly objects attack each other. */
	if (is_friend_of(hit_obj, op))
	{
		return 0;
	}

	if (hit_level > target_obj->level && hit_obj->type == MONSTER)
	{
		dam += (int) ((float) (dam / 2) * ((float) (hit_level - target_obj->level) / (target_obj->level > 25 ? 25.0f : (float) target_obj->level)));
	}

	/* Something hit player (can be disease or poison too), break
	 * praying. */
	if (op->type == PLAYER && CONTR(op)->was_praying)
	{
		new_draw_info(NDI_UNIQUE, op, "Your praying is disrupted.");
		CONTR(op)->praying = 0;
		CONTR(op)->was_praying = 0;
	}

	/* Check for PVP areas. */
	if (op->type == PLAYER || (get_owner(op) && op->owner->type == PLAYER))
	{
		if (hitter->type == PLAYER || (get_owner(hitter) && hitter->owner->type == PLAYER))
		{
			if (!pvp_area(op->type == PLAYER ? op : get_owner(op), hitter->type == PLAYER ? hitter : get_owner(hitter)))
			{
				return 0;
			}
		}
	}

	/* Check objects are valid, on same map and set them to head when
	 * needed. */
	if (get_attack_mode(&op, &hitter, &simple_attack))
	{
		return 0;
	}

	op_tag = op->count;
	hitter_tag = hitter->count;

	/* Go through and hit the player with each attacktype, one by one.
	 * hit_player_attacktype only figures out the damage, doesn't inflict
	 * it. It will do the appropriate action for attacktypes with
	 * effects (slow, paralization, etc). */
	for (attacknum = 0; attacknum < NROFATTACKS; attacknum++)
	{
		if (hitter->attack[attacknum])
		{
			maxdam += hit_player_attacktype(op, hitter, dam, attacknum);
		}
	}

	/* If one gets attacked, the attacker will become the enemy */
	if (!OBJECT_VALID(op->enemy, op->enemy_count) && !IS_INVISIBLE(hit_obj, op) && !QUERY_FLAG(hit_obj, FLAG_INVULNERABLE))
	{
		set_npc_enemy(op, hit_obj, NULL);
	}

	/* This is needed to send the hit number animations to the clients */
	if (op->damage_round_tag != ROUND_TAG)
	{
		op->last_damage = 0;
		op->damage_round_tag = ROUND_TAG;
	}

	op->last_damage += maxdam;

	/* Damage the target got */
	op->stats.hp -= maxdam;

	/* Check to see if monster runs away. */
	if ((op->stats.hp >= 0) && QUERY_FLAG(op, FLAG_MONSTER) && op->stats.hp < (signed short) (((float) op->run_away / 100.0f) * (float) op->stats.maxhp))
	{
		SET_FLAG(op, FLAG_RUN_AWAY);
	}

	/* rtn_kill is here negative! */
	if ((rtn_kill = kill_object(op, dam, hitter, type)))
	{
		return (maxdam + rtn_kill + 1);
	}

	/* Used to be ghosthit removal - we now use the ONE_HIT flag.  Note
	 * that before if the player was immune to ghosthit, the monster
	 * remained - that is no longer the case. */
	if (QUERY_FLAG(hitter, FLAG_ONE_HIT))
	{
		/* Remove, but don't drop inventory */
		remove_ob(hitter);
		check_walk_off(hitter, NULL, MOVE_APPLY_VANISHED);
	}
	/* Let's handle creatures that are splitting now */
	else if ((type & AT_PHYSICAL) && !OBJECT_FREE(op) && QUERY_FLAG(op, FLAG_SPLITTING))
	{
		int i;
		int unaggressive = QUERY_FLAG(op, FLAG_UNAGGRESSIVE);

		if (!op->other_arch)
		{
			LOG(llevBug, "BUG: SPLITTING without other_arch error.\n");
			return maxdam;
		}

		remove_ob(op);

		if (check_walk_off(op, NULL, MOVE_APPLY_VANISHED) == CHECK_WALK_OK)
		{
			/* This doesn't handle op->more yet */
			for (i = 0; i < op->stats.food; i++)
			{
				object *tmp = arch_to_object(op->other_arch);
				int j;

				tmp->stats.hp = op->stats.hp;

				if (unaggressive)
				{
					SET_FLAG(tmp, FLAG_UNAGGRESSIVE);
				}

				j = find_first_free_spot(tmp->arch, NULL, op->map,op->x, op->y);

				/* Found spot to put this monster */
				if (j >= 0)
				{
					tmp->x = op->x + freearr_x[j], tmp->y = op->y + freearr_y[j];
					insert_ob_in_map(tmp,op->map, NULL, 0);
				}
			}
		}
	}

	return maxdam;
}

/**
 * Attack a spot on the map.
 * @param op Object hitting the map.
 * @param dir Direction op is hitting/going.
 * @param reduce Whether to reduce the damage for multi-arch monsters.
 * This will make it so that part of 4-tiles monster only gets hit for
 * 1/4 of the damage, making storms more fair against multi-arch monsters.
 * @return 0. */
int hit_map(object *op, int dir, int reduce)
{
	object *tmp, *next, *tmp_obj, *tmp_head;
	mapstruct *map;
	int x, y, mflags;
	tag_t op_tag, next_tag = 0;

	if (OBJECT_FREE(op))
	{
		LOG(llevBug, "BUG: hit_map(): free object\n");
		return 0;
	}

	if (QUERY_FLAG(op, FLAG_REMOVED) || op->env != NULL)
	{
		LOG(llevBug, "BUG: hit_map(): hitter (arch %s, name %s) not on a map\n", op->arch->name, query_name(op, NULL));
		return 0;
	}

	if (op->head)
	{
		op = op->head;
	}

	op_tag = op->count;

	if (!op->map)
	{
		LOG(llevBug, "BUG: hit_map(): %s has no map.\n", query_name(op, NULL));
		return 0;
	}

	x = op->x + freearr_x[dir];
	y = op->y + freearr_y[dir];

	if (!(map = get_map_from_coord(op->map, &x, &y)))
	{
		return 0;
	}

	mflags = GET_MAP_FLAGS(map, x, y);

	next = get_map_ob(map, x, y);

	if (next)
	{
		next_tag = next->count;
	}

	if (!(tmp_obj = get_owner(op)))
	{
		tmp_obj = op;
	}

	if (tmp_obj->head)
	{
		tmp_obj = tmp_obj->head;
	}

	while (next)
	{
		if (was_destroyed(next, next_tag))
		{
			/* There may still be objects that were above 'next', but there is no
			 * simple way to find out short of copying all object references and
			 * tags into a temporary array before we start processing the first
			 * object.  That's why we just abort.
			 *
			 * This happens whenever attack spells (like fire) hit a pile
			 * of objects. This is not a bug - nor an error. */
			break;
		}

		tmp = next;
		next = tmp->above;

		if (next)
		{
			next_tag = next->count;
		}

		if (OBJECT_FREE(tmp))
		{
			LOG(llevBug, "BUG: hit_map(): found freed object (%s)\n", tmp->arch->name ? tmp->arch->name : "<NULL>");
			break;
		}

		/* Something could have happened to 'tmp' while 'tmp->below' was processed.
		 * For example, 'tmp' was put in an icecube.
		 * This is one of the few cases where on_same_map should not be used. */
		if (tmp->map != map || tmp->x != x || tmp->y != y)
		{
			continue;
		}

		/* First, we check player... */
		if (QUERY_FLAG(tmp, FLAG_IS_PLAYER))
		{
			hit_player(tmp, op->stats.dam, op, AT_INTERNAL);

			if (was_destroyed(op, op_tag))
			{
				break;
			}
		}
		else if (IS_LIVE(tmp))
		{
			sint16 dam = op->stats.dam;

			tmp_head = HEAD(tmp);

			if (tmp_head->type == MONSTER)
			{
				/* Monster vs. monster */
				if (tmp_obj->type == MONSTER)
				{
					if (QUERY_FLAG(tmp_head, FLAG_FRIENDLY))
					{
						if (QUERY_FLAG(tmp_obj, FLAG_FRIENDLY))
							continue;
					}
					else
					{
						if (!QUERY_FLAG(tmp_obj, FLAG_FRIENDLY))
							continue;
					}
				}
			}

			if (tmp->quick_pos && reduce)
			{
				dam /= (tmp->quick_pos >> 4) + 1;
			}

			hit_player(tmp, dam, op, AT_INTERNAL);

			if (was_destroyed(op, op_tag))
			{
				break;
			}
		}
	}

	return 0;
}

/**
 * Handles one attacktype's damage.
 *
 * This doesn't damage the creature, but returns how much it should
 * take. However, it will do other effects (paralyzation, slow, etc).
 * @param op Victim of the attack.
 * @param hitter Attacker.
 * @param damage Maximum dealt damage.
 * @param attacknum Number of the attacktype of the attack.
 * @return Damage to actually do. */
static int hit_player_attacktype(object *op, object *hitter, int damage, uint32 attacknum)
{
	double dam = (double) damage;
	int doesnt_slay = 1;

	/* Sanity check */
	if (dam < 0)
	{
		LOG(llevBug, "BUG: hit_player_attacktype called with negative damage: %d from object: %s\n", dam, query_name(op, NULL));
		return 0;
	}

	if (hitter->slaying)
	{
		if (((op->race != NULL) && strstr(hitter->slaying, op->race)) || (op->arch && (op->arch->name != NULL) &&  strstr(op->arch->name, hitter->slaying)))
		{
			doesnt_slay = 0;

			if (QUERY_FLAG(hitter, FLAG_IS_ASSASSINATION))
			{
				damage = (int) ((double) damage * 2.25);
			}
			else
			{
				damage = (int) ((double) damage * 1.75);
			}

			dam = (double) damage;
		}
	}

	/* AT_INTERNAL is supposed to do exactly dam. Put a case here so
	 * people can't mess with that or it otherwise get confused. */
	if (attacknum == ATNR_INTERNAL)
	{
		/* Adjust damage */
		dam = dam * ((double) hitter->attack[ATNR_INTERNAL] / 100.0);

		/* handle special object attacks */
		/* we have a poison force object (thats the poison we had inserted) */
		if (hitter->type == POISONING)
		{
			/* Map to poison... */
			attacknum = ATNR_POISON;

			if (op->protection[attacknum] == 100)
			{
				dam = 0;
				send_attack_msg(op, hitter, attacknum, (int) dam, damage);
				return 0;
			}

			/* Reduce to % protection */
			ATTACK_PROTECT_DAMAGE(op, attacknum);
		}

		if (damage && dam < 1.0)
		{
			dam = 1.0;
		}

		send_attack_msg(op, hitter, attacknum, (int) dam, damage);
		return (int) dam;
	}

	/* Quick check for immunity - if so, we skip here.
	 * Our formula is (100 - resist) / 100 - so test for 100 = zero division */
	if (op->protection[attacknum] == 100)
	{
		dam = 0;
		send_attack_msg(op, hitter, attacknum, (int) dam, damage);
		return 0;
	}

	switch (attacknum)
	{
		case ATNR_IMPACT:
		case ATNR_SLASH:
		case ATNR_CLEAVE:
		case ATNR_PIERCE:
			check_physically_infect(op, hitter);

			ATTACK_HIT_DAMAGE(hitter, attacknum);
			ATTACK_PROTECT_DAMAGE(op, attacknum);

			if (damage && dam < 1.0)
			{
				dam = 1.0;
			}

			send_attack_msg(op, hitter, attacknum, (int) dam, damage);
			break;

		case ATNR_POISON:
			ATTACK_HIT_DAMAGE(hitter, attacknum);
			ATTACK_PROTECT_DAMAGE(op, attacknum);

			if (damage && dam < 1.0)
			{
				dam = 1.0;
			}

			send_attack_msg(op, hitter, attacknum, (int) dam, damage);

			if (dam && IS_LIVE(op))
			{
				poison_player(op, hitter, (float)dam);
			}

			break;

		case ATNR_CONFUSION:
		case ATNR_SLOW:
		case ATNR_PARALYZE:
		case ATNR_BLIND:
		{
			int level_diff = MIN(MAXLEVEL, MAX(0, op->level - hitter->level));

			if (op->speed && (QUERY_FLAG(op, FLAG_MONSTER) || op->type == PLAYER) && !(rndm(0, (attacknum == ATNR_SLOW ? 6 : 3) - 1)) && ((rndm(1, 20) + op->protection[attacknum] / 10) < savethrow[level_diff]))
			{
				if (attacknum == ATNR_CONFUSION)
				{
					if (hitter->type == PLAYER)
					{
						new_draw_info_format(NDI_ORANGE, hitter, "You confuse %s!", op->name);
					}

					if (op->type == PLAYER)
					{
						new_draw_info_format(NDI_PURPLE, op, "%s confused you!", hitter->name);
					}

					confuse_living(op);
				}
				else if (attacknum == ATNR_SLOW)
				{
					if (hitter->type == PLAYER)
					{
						new_draw_info_format(NDI_ORANGE, hitter, "You slow %s!", op->name);
					}

					if (op->type == PLAYER)
					{
						new_draw_info_format(NDI_PURPLE, op, "%s slowed you!", hitter->name);
					}

					slow_living(op);
				}
				else if (attacknum == ATNR_PARALYZE)
				{
					if (hitter->type == PLAYER)
					{
						new_draw_info_format(NDI_ORANGE, hitter, "You paralyze %s!", op->name);
					}

					if (op->type == PLAYER)
					{
						new_draw_info_format(NDI_PURPLE, op, "%s paralyzed you!", hitter->name);
					}

					paralyze_living(op, (int) dam);
				}
				else if (attacknum == ATNR_BLIND && !QUERY_FLAG(op, FLAG_UNDEAD))
				{
					if (hitter->type == PLAYER)
					{
						new_draw_info_format(NDI_ORANGE, hitter, "You blind %s!", op->name);
					}

					if (op->type == PLAYER)
					{
						new_draw_info_format(NDI_PURPLE, op, "%s blinded you!", hitter->name);
					}

					blind_living(op, hitter, (int) dam);
				}
			}

			dam = 0;
		}

		break;

		default:
			ATTACK_HIT_DAMAGE(hitter, attacknum);
			ATTACK_PROTECT_DAMAGE(op, attacknum);

			if (damage && dam < 1.0)
			{
				dam = 1.0;
			}

			send_attack_msg(op, hitter, attacknum, (int) dam, damage);
			break;
	}

	return (int) dam;
}

/**
 * Send attack message for players.
 * @param op Victim of the attack.
 * @param hitter Attacker.
 * @param attacknum ID of the attack type.
 * @param dam Actual damage done.
 * @param damage How much damage should have been done, not counting
 * resists/protections/etc. */
static void send_attack_msg(object *op, object *hitter, int attacknum, int dam, int damage)
{
	object *orig_hitter = hitter;

	if (op->type == PLAYER)
	{
		new_draw_info_format(NDI_PURPLE, op, "%s hit you for %d (%d) damage.", hitter->name, dam, dam - damage);
	}

	if (hitter->type == PLAYER || ((hitter = get_owner(hitter)) && hitter->type == PLAYER))
	{
		new_draw_info_format(NDI_ORANGE, hitter, "You hit %s for %d (%d) with %s.", op->name, dam, dam - damage, attacknum == ATNR_INTERNAL ? orig_hitter->name : attack_name[attacknum]);
	}
}

/**
 * One player gets exp by killing a monster.
 * @param op Player. This should be the killer.
 * @param exp Experience to gain.
 * @param skill Skill that was used to kill the monster. */
static void share_kill_exp_one(object *op, sint64 exp, object *skill)
{
	if (exp)
	{
		add_exp(op, exp, skill->stats.sp);
	}
	else
	{
		new_draw_info(NDI_UNIQUE, op, "Your enemy was too low for exp.");
	}
}

/**
 * Share experience gained by killing a monster. This will fairly share
 * experience between party members, or if none are present, it will use
 * share_kill_exp_one() instead.
 * @param op Player that killed the monster.
 * @param exp Experience to share.
 * @param skill Skill that was used to kill the monster. */
static void share_kill_exp(object *op, sint64 exp, object *skill)
{
	int shares = 0, count = 0;
	party_struct *party;
	objectlink *ol;

	if (!CONTR(op)->party)
	{
		share_kill_exp_one(op, exp, skill);
		return;
	}

	party = CONTR(op)->party;

	for (ol = party->members; ol; ol = ol->next)
	{
		if (on_same_map(ol->objlink.ob, op))
		{
			sint16 skill_id = party_member_get_skill(ol->objlink.ob, skill);

			if (skill_id == NO_SKILL_READY)
			{
				continue;
			}

			count++;
			shares += (CONTR(ol->objlink.ob)->skill_ptr[skill_id]->level + 4);
		}
	}

	if (count == 1 || shares > exp)
	{
		share_kill_exp_one(op, exp, skill);
	}
	else
	{
		sint64 share = exp / shares, given = 0, nexp;

		for (ol = party->members; ol; ol = ol->next)
		{
			if (ol->objlink.ob != op && on_same_map(ol->objlink.ob, op))
			{
				sint16 skill_id = party_member_get_skill(ol->objlink.ob, skill);

				if (skill_id == NO_SKILL_READY)
				{
					continue;
				}

				nexp = (CONTR(ol->objlink.ob)->skill_ptr[skill_id]->level + 4) * share;
				add_exp(ol->objlink.ob, nexp, skill_id);
				given += nexp;
			}
		}

		exp -= given;
		share_kill_exp_one(op, exp, skill);
	}
}

/**
 * An object was killed, handle various things (logging, messages, ...).
 * @param op What is being killed.
 * @param dam Damage done to it.
 * @param hitter What is hitting it.
 * @param type The attacktype.
 * @return Dealt damage. */
int kill_object(object *op, int dam, object *hitter, int type)
{
	int maxdam, battleg;
	sint64 exp = 0;
	object *owner;

	(void) dam;

	/* Still got some HP left? */
	if (op->stats.hp > 0)
	{
		return -1;
	}

	/* Cannot kill wizards. */
	if (QUERY_FLAG(op, FLAG_WIZ))
	{
		return 0;
	}

	/* Trigger the DEATH event */
	if (trigger_event(EVENT_DEATH, hitter, op, NULL, NULL, type, 0, 0, SCRIPT_FIX_ALL))
	{
		return 0;
	}

	/* Trigger the global GKILL event */
	trigger_global_event(EVENT_GKILL, hitter, op);

	maxdam = op->stats.hp - 1;

	/* Only when some damage is stored, and we're on a map. */
	if (op->damage_round_tag == ROUND_TAG && op->map)
	{
		SET_MAP_DAMAGE(op->map, op->x, op->y, op->last_damage);
		SET_MAP_RTAG(op->map, op->x, op->y, ROUND_TAG);
	}

	if (op->map)
	{
		play_sound_map(op->map, SOUND_PLAYER_KILLS, NULL, op->x, op->y, 0, 0);
	}

	/* Figure out who to credit for the kill. */
	owner = get_owner(hitter);

	if (!owner)
	{
		owner = hitter;
	}

	/* Is the victim in PvP area? */
	battleg = pvp_area(NULL, op);

	/* Player killed something. */
	if (owner->type == PLAYER)
	{
		if (owner != hitter)
		{
			new_draw_info_format(NDI_UNIQUE, owner, "You killed %s with %s.", query_name(op, NULL), query_name(hitter, NULL));
		}
		else
		{
			new_draw_info_format(NDI_UNIQUE, owner, "You killed %s.", query_name(op, NULL));
		}
	}

	/* Killed a player in PvP area. */
	if (battleg && op->type == PLAYER && owner->type == PLAYER)
	{
		new_draw_info(NDI_UNIQUE, owner, "Your foe has fallen!\nVICTORY!!!");
	}

	/* Killed a monster and it wasn't in PvP area, so give exp. */
	if (!battleg && owner->type == PLAYER && op->type != PLAYER)
	{
		object *skill;

		/* Figure out the skill that should gain experience. If the hitter
		 * has chosen_skill set, we will use that. */
		if (hitter->chosen_skill)
		{
			skill = hitter->chosen_skill;
		}
		/* Otherwise try to use owner's chosen_skill. */
		else
		{
			skill = owner->chosen_skill;
		}

		/* Calculate how much experience to gain. */
		exp = calc_skill_exp(owner, op, skill->level);
		/* Give the experience, sharing it with party members if applicable. */
		share_kill_exp(owner, exp, skill);
	}

	/* Player has been killed. */
	if (op->type == PLAYER)
	{
		/* Tell everyone that this player has died. */
		if (get_owner(hitter))
		{
			new_draw_info_format(NDI_ALL, NULL, "%s killed %s with %s%s.", hitter->owner->name, query_name(op, NULL), query_name(hitter, NULL), battleg ? " (duel)" : "");
		}
		else
		{
			new_draw_info_format(NDI_ALL, NULL, "%s killed %s%s.", hitter->name, op->name, battleg ? " (duel)" : "");
		}

		/* Update player's killer. */
		if (owner->type == PLAYER)
		{
			char race[MAX_BUF];

			snprintf(CONTR(op)->killer, sizeof(CONTR(op)->killer), "%s the %s", owner->name, player_get_race_class(owner, race, sizeof(race)));
		}
		else
		{
			strncpy(CONTR(op)->killer, owner->name, sizeof(CONTR(op)->killer) - 1);
		}

		/* And actually kill the player. */
		kill_player(op);
	}
	/* Monster or something else has been killed. */
	else
	{
		/* Remove the monster from the active list. */
		op->speed = 0.0f;
		update_ob_speed(op);

		/* Rules:
		 * 1. Monster will drop corpse for his target, not the killer (unless killer == target).
		 * 2. NPC kill hit will overwrite player target on drop.
		 * 3. Kill hit will count if target was an NPC. */
		if (owner->type != PLAYER || !op->enemy || op->enemy->type != PLAYER)
		{
			op->enemy = owner;
			op->enemy_count = owner->count;
		}

		/* Monster killed another monster. */
		if (hitter->type == MONSTER || (get_owner(hitter) && hitter->owner->type == MONSTER))
		{
			/* No loot */
			SET_FLAG(op, FLAG_STARTEQUIP);
			/* Force an empty corpse though. */
			SET_FLAG(op, FLAG_CORPSE_FORCED);
		}
		/* No exp, no loot and no corpse. */
		else if (!exp)
		{
			SET_FLAG(op, FLAG_STARTEQUIP);
		}

		destruct_ob(op);
	}

	return maxdam;
}

/**
 * Find correct parameters for attack, do some sanity checks.
 * @param target Will point to victim's head.
 * @param hitter Will point to hitter's head.
 * @param simple_attack Will be 1 if one of victim or target isn't on a
 * map, 0 otherwise.
 * @return 0 if hitter can attack target, 1 otherwise. */
static int get_attack_mode(object **target, object **hitter, int *simple_attack)
{
	if (OBJECT_FREE(*target) || OBJECT_FREE(*hitter))
	{
		LOG(llevBug, "BUG: get_attack_mode(): freed object\n");
		return 1;
	}

	if ((*target)->head)
	{
		*target = (*target)->head;
	}

	if ((*hitter)->head)
	{
		*hitter = (*hitter)->head;
	}

	if ((*hitter)->env != NULL || (*target)->env != NULL || (*hitter)->map == NULL)
	{
		*simple_attack = 1;
		return 0;
	}

	if (QUERY_FLAG(*target, FLAG_REMOVED) || QUERY_FLAG(*hitter, FLAG_REMOVED) || !on_same_map((*hitter), (*target)))
	{
		LOG(llevBug, "BUG: hitter (arch %s, name %s) with no relation to target\n", (*hitter)->arch->name, query_name(*hitter, NULL));
		return 1;
	}

	*simple_attack = 0;
	return 0;
}

/**
 * Check if target and hitter are still in a relation similar to the one
 * determined by get_attack_mode().
 * @param target Who is attacked.
 * @param hitter Who is attacking.
 * @param simple_attack Previous mode as returned by get_attack_mode().
 * @return 1 if the relation has changed, 0 otherwise. */
static int abort_attack(object *target, object *hitter, int simple_attack)
{
	int new_mode;

	if (hitter->env == target || target->env == hitter)
	{
		new_mode = 1;
	}
	else if (QUERY_FLAG(target, FLAG_REMOVED) || QUERY_FLAG(hitter, FLAG_REMOVED) || hitter->map == NULL || !on_same_map(hitter, target))
	{
		return 1;
	}
	else
	{
		new_mode = 0;
	}

	return new_mode != simple_attack;
}

/**
 * Disassembles the missile, attacks the victim and reassembles the
 * missile.
 * @param op Missile hitting.
 * @param victim Who is hit by op.
 * @return Pointer to the reassembled missile, or NULL if the missile
 * isn't available anymore. */
object *hit_with_arrow(object *op, object *victim)
{
	object *container, *hitter;
	int hit_something = 0;
	tag_t victim_tag, hitter_tag;
	sint16 victim_x, victim_y;
	mapstruct *victim_map;

	/* Disassemble missile */
	if (op->inv)
	{
		container = op;
		hitter = op->inv;
		remove_ob(hitter);
		insert_ob_in_map(hitter, container->map, hitter, INS_NO_MERGE | INS_NO_WALK_ON);
	}
	else
	{
		container = NULL;
		hitter = op;
	}

	/* Try to hit victim */
	victim_x = victim->x;
	victim_y = victim->y;
	victim_map = victim->map;
	victim_tag = victim->count;
	hitter_tag = hitter->count;

	if (HAS_EVENT(hitter, EVENT_ATTACK))
	{
		/* Trigger the ATTACK event */
		trigger_event(EVENT_ATTACK, hitter, hitter, victim, NULL, 0, op->stats.dam, op->stats.wc, SCRIPT_FIX_ALL);
	}
	else
	{
		hit_something = attack_ob_simple(victim, hitter, op->stats.dam, op->stats.wc);
	}

	/* Arrow attacks door, rune of summoning is triggered, demon is put on
	 * arrow, move_apply() calls this function, arrow sticks in demon,
	 * attack_ob_simple() returns, and we've got an arrow that still exists
	 * but is no longer on the map. Ugh. (Beware: Such things can happen at
	 * other places as well!) */
	if (was_destroyed(hitter, hitter_tag) || hitter->env != NULL)
	{
		if (container)
		{
			remove_ob(container);
			check_walk_off(container, NULL, MOVE_APPLY_VANISHED);
		}

		return NULL;
	}

	/* Missile hit victim */
	if (hit_something)
	{
		if (container)
		{
			remove_ob(container);
			check_walk_off(container, NULL, MOVE_APPLY_VANISHED);
		}

		hitter = fix_stopped_arrow(hitter);

		if (hitter == NULL)
		{
			return NULL;
		}

		/* Trigger the STOP event */
		trigger_event(EVENT_STOP, victim, hitter, NULL, NULL, 0, 0, 0, SCRIPT_FIX_NOTHING);
		CLEAR_FLAG(hitter, FLAG_IS_MISSILE);

		/* Else try to put arrow on victim's map square */
		if ((victim_x != hitter->x || victim_y != hitter->y))
		{
			remove_ob(hitter);

			if (check_walk_off(hitter, NULL, MOVE_APPLY_DEFAULT) == CHECK_WALK_OK)
			{
				hitter->x = victim_x;
				hitter->y = victim_y;
				insert_ob_in_map(hitter, victim_map, hitter, 0);
			}
		}
		/* Else leave arrow where it is */
		else
		{
			hitter = merge_ob(hitter, NULL);
		}

		return NULL;
	}

	/* Missile missed victim - reassemble missile */
	if (container)
	{
		/* Technical remove, no walk check */
		remove_ob(hitter);
		insert_ob_in_ob(hitter, container);
	}

	return op;
}

/**
 * Poison a living thing.
 * @param op Victim.
 * @param hitter Who is attacking.
 * @param dam Damage to deal. */
static void poison_player(object *op, object *hitter, float dam)
{
	archetype *at = find_archetype("poisoning");
	object *tmp = present_arch_in_ob(at, op);

	/* We only poison players and mobs! */
	if (op->type != PLAYER && !QUERY_FLAG(op, FLAG_MONSTER))
	{
		return;
	}

	if (tmp == NULL || hitter->type == POISON)
	{
		if ((tmp = arch_to_object(at)) == NULL)
		{
			LOG(llevBug, "BUG: Failed to clone arch poisoning.\n");
			return;
		}
		else
		{
			if (hitter->type == POISON)
			{
				dam /= 2.0f;
				tmp->stats.dam = (int) (((dam + rndm(0, dam + 1)) * LEVEL_DAMAGE(hitter->level)) * 0.9f);

				if (tmp->stats.dam > op->stats.maxhp / 3)
				{
					tmp->stats.dam = op->stats.maxhp / 3;
				}

				if (tmp->stats.dam < 1)
				{
					tmp->stats.dam = 1;
				}
			}
			/* Spell or weapon will be handled different! */
			else
			{
				dam /= 2.0f;
				tmp->stats.dam = (int) ((int) dam + rndm(0, dam + 1));

				if (tmp->stats.dam > op->stats.maxhp / 3)
				{
					tmp->stats.dam = op->stats.maxhp / 3;
				}

				if (tmp->stats.dam < 1)
				{
					tmp->stats.dam = 1;
				}
			}

			tmp->level = hitter->level;
			/* So we get credit for poisoning kills */
			copy_owner(tmp, hitter);

			/* Now we adjust numbers of ticks of the DOT force and speed of DOT ticks */
			if (hitter->type == POISON)
			{
				/* # of ticks */
				tmp->stats.food = hitter->last_heal;
				/* Speed of ticks */
				tmp->speed = tmp->speed_left;
			}

			if (op->type == PLAYER)
			{
				/* Spells should add here too later */
				if (hitter->type == POISON)
				{
					/* Insert the food force in player too */
					create_food_force(op, hitter, tmp);
					new_draw_info(NDI_UNIQUE, op, "You suddenly feel very ill.");
				}
				/* We have hit with weapon or something */
				else
				{
					new_draw_info_format(NDI_UNIQUE, op, "%s has poisoned you!", query_name(hitter, NULL));
					insert_ob_in_ob(tmp, op);
					SET_FLAG(tmp, FLAG_APPLIED);
					fix_player(op);
				}
			}
			/* It's a monster */
			else
			{
				/* Monster eats poison */
				if (hitter->type == POISON)
				{
				}
				/* Hit from poison force! */
				else
				{
					insert_ob_in_ob(tmp, op);
					SET_FLAG(tmp, FLAG_APPLIED);
					fix_monster(op);

					if (hitter->type == PLAYER)
					{
						new_draw_info_format(NDI_UNIQUE, hitter, "You poisoned %s!", query_name(op, NULL));
					}
					else if (get_owner(hitter) && hitter->owner->type == PLAYER)
					{
						new_draw_info_format(NDI_UNIQUE, hitter->owner, "%s poisoned %s!", query_name(hitter, NULL), query_name(op, NULL));
					}

				}
			}
		}

		tmp->speed_left = 0;
	}
	else
	{
		tmp->stats.food++;
	}
}

/**
 * Slow a living thing.
 * @param op Victim. */
static void slow_living(object *op)
{
	archetype *at = find_archetype("slowness");
	object *tmp;

	if (at == NULL)
	{
		LOG(llevBug, "BUG: Can't find slowness archetype.\n");
	}

	if ((tmp = present_arch_in_ob(at, op)) == NULL)
	{
		tmp = arch_to_object(at);
		tmp = insert_ob_in_ob(tmp, op);
		new_draw_info(NDI_UNIQUE, op, "The world suddenly moves very fast!");
	}
	else
	{
		tmp->stats.food++;
	}

	SET_FLAG(tmp, FLAG_APPLIED);
	tmp->speed_left = 0;
	fix_player(op);
}

/**
 * Confuse a living thing.
 * @param op Victim. */
void confuse_living(object *op)
{
	object *tmp;
	int maxduration;

	tmp = present_in_ob(CONFUSION, op);

	if (!tmp)
	{
		tmp = get_archetype("confusion");
		tmp = insert_ob_in_ob(tmp, op);
	}

	/* Duration added per hit and max. duration of confusion both depend
	 * on the player's resistance */
	tmp->stats.food += MAX(1, 5 * (100 - op->protection[ATNR_CONFUSION]) / 100);
	maxduration = MAX(2, 30 * (100 - op->protection[ATNR_CONFUSION]) / 100);

	if (tmp->stats.food > maxduration)
	{
		tmp->stats.food = maxduration;
	}

	if (op->type == PLAYER && !QUERY_FLAG(op, FLAG_CONFUSED))
	{
		new_draw_info(NDI_UNIQUE, op, "You suddenly feel very confused!");
	}

	SET_FLAG(op, FLAG_CONFUSED);
}

/**
 * Blind a living thing.
 * @param op Victim.
 * @param hitter Who is attacking.
 * @param dam Damage to deal. */
static void blind_living(object *op, object *hitter, int dam)
{
	object *tmp, *owner;

	/* Save some work if we know it isn't going to affect the player */
	if (op->protection[ATNR_BLIND] == 100)
	{
		return;
	}

	tmp = present_in_ob(BLINDNESS, op);

	if (!tmp)
	{
		tmp = get_archetype("blindness");
		SET_FLAG(tmp, FLAG_BLIND);
		SET_FLAG(tmp, FLAG_APPLIED);
		/* Use floats so we don't lose too much precision due to rounding errors.
		 * speed is a float anyways. */
		tmp->speed = tmp->speed * ((float) 100.0 - (float) op->protection[ATNR_BLIND]) / (float) 100;

		tmp = insert_ob_in_ob(tmp, op);
		/* Mostly to display any messages */
		change_abil(op, tmp);
		/* This takes care of some other stuff */
		fix_player(op);

		if (hitter->owner)
		{
			owner = get_owner(hitter);
		}
		else
		{
			owner = hitter;
		}

		new_draw_info_format(NDI_UNIQUE, owner, "Your attack blinds %s!", query_name(op, NULL));
	}

	tmp->stats.food += dam;

	if (tmp->stats.food > 10)
	{
		tmp->stats.food = 10;
	}
}

/**
 * Paralyze a living thing.
 * @param op Victim.
 * @param dam Damage to deal. */
void paralyze_living(object *op, int dam)
{
	float effect, max;

	/* Do this as a float - otherwise, rounding might very well reduce this to 0 */
	effect = (float) dam * (float) 3.0 * ((float) 100.0 - (float) op->protection[ATNR_PARALYZE]) / (float) 100;

	if (effect == 0)
	{
		return;
	}

	/* We mark this object as paralyzed */
	SET_FLAG(op, FLAG_PARALYZED);

	op->speed_left -= FABS(op->speed) * effect;

	/* Max number of ticks to be affected for. */
	max = ((float) 100 - (float) op->protection[ATNR_PARALYZE]) / (float) 2;

	if (op->speed_left < -(FABS(op->speed) * max))
	{
		op->speed_left = (float) -(FABS(op->speed) * max);
	}
}

/**
 * Handles any special effects of thrown items (like attacking living
 * creatures -- a potion thrown at a monster).
 * @param hitter Thrown item.
 * @param victim Object that is hit by hitter. */
static void thrown_item_effect(object *hitter, object *victim)
{
	if (!IS_LIVE(hitter))
	{
		/* May not need a switch for just 2 types, but this makes it
		 * easier for expansion. */
		switch (hitter->type)
		{
			case POTION:
				if (hitter->stats.sp != SP_NO_SPELL && spells[hitter->stats.sp].flags&SPELL_DESC_DIRECTION)
				{
					/* apply potion ALWAYS fire on the spot the applier stands - good for healing - bad for firestorm */
					cast_spell(hitter, hitter, hitter->direction, hitter->stats.sp, 1, spellPotion, NULL);
				}

				decrease_ob(hitter);
				break;

			/* Poison drinks */
			case POISON:
				if (IS_LIVE(victim) && !QUERY_FLAG(victim, FLAG_UNDEAD))
				{
					apply_poison(victim, hitter);
				}

				break;
		}
	}
}

/**
 * Adjustments to attack rolls by various conditions.
 * @param hitter Who is hitting.
 * @param target Victim of the attack.
 * @return Adjustment to attack roll. */
static int adj_attackroll(object *hitter, object *target)
{
	object *attacker = hitter;
	int adjust = 0;

	/* Safety */
	if (!target || !hitter || !hitter->map || !target->map || !on_same_map(hitter, target))
	{
		LOG(llevBug, "BUG: adj_attackroll(): hitter and target not on same map\n");
		return 0;
	}

	/* Aimed missiles use the owning object's sight */
	if (is_aimed_missile(hitter))
	{
		if ((attacker = get_owner(hitter)) == NULL)
		{
			attacker = hitter;
		}
	}
	else if (!IS_LIVE(hitter))
	{
		return 0;
	}

	/* Invisible means, we can't see it - same for blind */
	if (IS_INVISIBLE(target, attacker) || QUERY_FLAG(attacker, FLAG_BLIND))
	{
		adjust -= 12;
	}

	if (QUERY_FLAG(attacker, FLAG_SCARED))
	{
		adjust -= 3;
	}

	if (QUERY_FLAG(target, FLAG_UNAGGRESSIVE))
	{
		adjust += 1;
	}

	if (QUERY_FLAG(target, FLAG_SCARED))
	{
		adjust += 1;
	}

	if (QUERY_FLAG(attacker, FLAG_CONFUSED))
	{
		adjust -= 3;
	}

	/* If we attack at a different 'altitude' it's harder */
	if (QUERY_FLAG(attacker, FLAG_FLYING) != QUERY_FLAG(target, FLAG_FLYING))
	{
		adjust -= 2;
	}

	return adjust;
}

/**
 * Determine if the object is an 'aimed' missile.
 * @param op Object to check.
 * @return 1 if aimed missile, 0 otherwise. */
static int is_aimed_missile(object *op)
{
	if (op && QUERY_FLAG(op, FLAG_FLYING) && (op->type == ARROW || op->type == THROWN_OBJ))
	{
		return 1;
	}

	return 0;
}

/**
 * Test if objects are in range for melee attack.
 * @param hitter Attacker.
 * @param enemy Enemy.
 * @retval 0 Enemy target is not in melee range.
 * @retval 1 Target is in range and we're facing it. */
int is_melee_range(object *hitter, object *enemy)
{
	int xt, yt, s;
	object *tmp;
	mapstruct *mt;

	/* Check squares around */
	for (s = 0; s < 9; s++)
	{
		xt = hitter->x + freearr_x[s];
		yt = hitter->y + freearr_y[s];

		if (!(mt = get_map_from_coord(hitter->map, &xt, &yt)))
		{
			continue;
		}

		for (tmp = enemy; tmp != NULL; tmp = tmp->more)
		{
			/* Strike! */
			if (tmp->map == mt && tmp->x == xt && tmp->y == yt)
			{
				return 1;
			}
		}
	}

	return 0;
}
