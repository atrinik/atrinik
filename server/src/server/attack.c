/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Crossfire (Multiplayer game for X-windows).                 *
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

/**
 * Names of attack types to use when saving them to file.
 * @warning Cannot contain spaces. Use underscores instead. */
char *attack_save[NROFATTACKS] =
{
	"impact",   "slash", "cleave",      "pierce",    "weaponmagic",
	"fire",     "cold",  "electricity", "poison",    "acid",
	"magic",    "mind",  "blind",       "paralyze",  "force",
	"godpower", "chaos", "drain",       "slow",      "confusion",
	"internal"
};

/**
 * Short description of names of the attack types. */
char *attack_name[NROFATTACKS] =
{
	"impact",   "slash", "cleave",      "pierce",    "weapon magic",
	"fire",     "cold",  "electricity", "poison",    "acid",
	"magic",    "mind",  "blind",       "paralyze",  "force",
	"godpower", "chaos", "drain",       "slow",      "confusion",
	"internal"
};

/**
 * Poison a living thing.
 * @param op Victim.
 * @param hitter Who is attacking.
 * @param dam Damage to deal. */
static void poison_player(object *op, object *hitter, float dam)
{
	archetype *at;
	object *tmp;
	int dam2;

	/* We only poison players and mobs! */
	if (op->type != PLAYER && !QUERY_FLAG(op, FLAG_MONSTER))
	{
		return;
	}

	if (hitter->type == POISONING)
	{
		return;
	}

	at = find_archetype("poisoning");
	tmp = present_arch_in_ob(at, op);

	dam /= 2.0f;
	dam2 = (int) ((int) dam + rndm(0, dam + 1));

	if (dam2 > op->stats.maxhp / 3)
	{
		dam2 = op->stats.maxhp / 3;
	}
	else if (dam2 < 1)
	{
		dam2 = 1;
	}

	if (tmp == NULL)
	{
		if ((tmp = arch_to_object(at)) == NULL)
		{
			logger_print(LOG(BUG), "Failed to clone arch poisoning.");
			return;
		}

		tmp->level = hitter->level;
		tmp->stats.dam = dam2;
		/* So we get credit for poisoning kills */
		copy_owner(tmp, hitter);

		if (op->type == PLAYER)
		{
			draw_info_format(COLOR_WHITE, op, "%s has poisoned you!", query_name(hitter, NULL));
			SET_FLAG(tmp, FLAG_APPLIED);
			insert_ob_in_ob(tmp, op);
			fix_player(op);
		}
		/* It's a monster */
		else
		{
			SET_FLAG(tmp, FLAG_APPLIED);
			insert_ob_in_ob(tmp, op);
			fix_monster(op);

			if (hitter->type == PLAYER)
			{
				draw_info_format(COLOR_WHITE, hitter, "You poisoned %s!", query_name(op, NULL));
			}
			else if (get_owner(hitter) && hitter->owner->type == PLAYER)
			{
				draw_info_format(COLOR_WHITE, hitter->owner, "%s poisoned %s!", query_name(hitter, NULL), query_name(op, NULL));
			}
		}

		tmp->speed_left = 0;
	}
	else
	{
		tmp->stats.food++;
		esrv_update_item(UPD_EXTRA, tmp);
		
		if (dam2 > tmp->stats.dam)
		{
			tmp->stats.dam = dam2;
		}
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
		logger_print(LOG(BUG), "Can't find slowness archetype.");
		return;
	}

	if ((tmp = present_arch_in_ob(at, op)) == NULL)
	{
		tmp = arch_to_object(at);
		tmp = insert_ob_in_ob(tmp, op);
		draw_info(COLOR_WHITE, op, "The world suddenly moves very fast!");
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
		draw_info(COLOR_WHITE, op, "You suddenly feel very confused!");
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

		draw_info_format(COLOR_WHITE, owner, "Your attack blinds %s!", query_name(op, NULL));
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

static void attack_absorb_damage(object *hitter, object *target, object *item, int dam[])
{
	int attacktype;
	object *skill;

	if (!item)
	{
		return;
	}

	skill = NULL;

	if (item->type == SHIELD || item->type == WEAPON)
	{
		int level;

		level = SK_level(hitter);
		skill = skill_get(target, SK_BLOCKING);

		if (skill)
		{
			int chance;

			chance = level - skill->level + rndm(0, 20);

			if (chance > 1 && !rndm_chance(chance))
			{
				return;
			}
		}
	}
	else if (IS_ARMOR(item))
	{
		if (item->item_skill)
		{
			skill = skill_get(target, item->item_skill - 1);
		}
	}

	if (!skill)
	{
		return;
	}

	for (attacktype = 0; attacktype < NROFATTACKS; attacktype++)
	{
		if (dam[attacktype])
		{
			dam[attacktype] = dam[attacktype] * ((double) (100 - (item->type == WEAPON ? 50 : item->protection[attacktype])) * 0.01);
		}
	}
}

static int attack_perform_attacktype(object *hitter, object *target, object *hitter_owner, int attacktype, int dam)
{
	if (!dam)
	{
		return 0;
	}

	switch (attacktype)
	{
		case ATNR_CONFUSION:
		case ATNR_SLOW:
		case ATNR_PARALYZE:
		case ATNR_BLIND:
			dam = 0;

			if (IS_LIVE(target) && rndm_chance(attacktype == ATNR_SLOW ? 6 : 3))
			{
				if (attacktype == ATNR_CONFUSION)
				{
					if (target->type == PLAYER)
					{
						draw_info_format(COLOR_PURPLE, target, "%s confused you!", hitter->name);
					}

					if (hitter->type == PLAYER)
					{
						draw_info_format(COLOR_ORANGE, hitter, "You confuse %s!", target->name);
					}

					confuse_living(target);
				}
				else if (attacktype == ATNR_SLOW)
				{
					if (target->type == PLAYER)
					{
						draw_info_format(COLOR_PURPLE, target, "%s slowed you!", hitter->name);
					}

					if (hitter->type == PLAYER)
					{
						draw_info_format(COLOR_ORANGE, hitter, "You slow %s!", target->name);
					}

					slow_living(target);
				}
				else if (attacktype == ATNR_PARALYZE)
				{
					if (target->type == PLAYER)
					{
						draw_info_format(COLOR_PURPLE, target, "%s paralyzed you!", hitter->name);
					}

					if (hitter->type == PLAYER)
					{
						draw_info_format(COLOR_ORANGE, hitter, "You paralyze %s!", target->name);
					}

					paralyze_living(target, dam);
				}
				else if (attacktype == ATNR_BLIND && !QUERY_FLAG(target, FLAG_UNDEAD))
				{
					if (target->type == PLAYER)
					{
						draw_info_format(COLOR_PURPLE, target, "%s blinded you!", hitter->name);
					}

					if (hitter->type == PLAYER)
					{
						draw_info_format(COLOR_ORANGE, hitter, "You blind %s!", target->name);
					}

					blind_living(target, hitter, dam);
				}
			}

			break;

		default:
			if (target->type == PLAYER)
			{
				draw_info_format(COLOR_PURPLE, target, "%s hit you for %d damage.", hitter->name, dam);
			}

			if (hitter_owner->type == PLAYER)
			{
				draw_info_format(COLOR_ORANGE, hitter_owner, "You hit %s for %d with %s.", target->name, dam, attacktype == ATNR_INTERNAL ? hitter->name : attack_name[attacktype]);
			}

			break;
	}

	switch (attacktype)
	{
		case ATNR_IMPACT:
		case ATNR_SLASH:
		case ATNR_CLEAVE:
		case ATNR_PIERCE:
			if (hitter->type == ARROW)
			{
				play_sound_map(hitter->map, CMD_SOUND_EFFECT, "arrow_hit.ogg", hitter->x, hitter->y, 0, 0);
			}
			else
			{
				char buf[MAX_BUF];

				snprintf(buf, sizeof(buf), "hit_%s.ogg", attack_name[attacktype]);
				play_sound_map(hitter->map, CMD_SOUND_EFFECT, buf, hitter->x, hitter->y, 0, 0);
			}

			check_physically_infect(target, hitter);
			break;

		case ATNR_POISON:
			poison_player(target, hitter, dam);
			break;

		default:
			break;
	}

	return dam;
}

int attack_perform(object *hitter, object *target)
{
	int dam, damage[NROFATTACKS], roll, attacktype, bodypart, maxdam;
	object *hitter_ob, *hitter_owner, *weapon, *hand_main, *hand_off, *shield;

	if ((target->type == PLAYER && CONTR(target)->tgm) || QUERY_FLAG(target, FLAG_INVULNERABLE))
	{
		return -1;
	}

	hitter = HEAD(hitter);
	target = HEAD(target);

	hitter_owner = get_owner(hitter);

	if (!hitter_owner)
	{
		hitter_owner = hitter;
	}

	if (is_friend_of(hitter_owner, target))
	{
		return -1;
	}

	/* Check for PVP areas. */
	if (target->type == PLAYER && hitter_owner->type == PLAYER)
	{
		if (!pvp_area(target, hitter_owner))
		{
			return -1;
		}
	}

	if (trigger_event(EVENT_ATTACK, hitter, hitter, target, NULL, 0, 0, 0, SCRIPT_FIX_NOTHING))
	{
		return -1;
	}

	CLEAR_FLAG(target, FLAG_SLEEP);

	/* If one gets attacked, the attacker will become the enemy */
	if (!OBJECT_VALID(target->enemy, target->enemy_count) && !IS_INVISIBLE(hitter_owner, target) && !QUERY_FLAG(hitter_owner, FLAG_INVULNERABLE))
	{
		set_npc_enemy(target, hitter_owner, NULL);
	}

	/* Swing animation. */
	if (hitter->type == PLAYER)
	{
		CONTR(hitter)->anim_flags |= PLAYER_AFLAG_ENEMY;
	}

	/* Face the target. */
	if (IS_LIVE(hitter))
	{
		rv_vector rv;

		if (get_rangevector(hitter, target, &rv, RV_NO_DISTANCE))
		{
			hitter->anim_enemy_dir = rv.direction;
			hitter->facing = rv.direction;
		}
	}

	weapon = NULL;

	/* Get the items in both hands. */
	hand_main = player_equipment_get(hitter, PLAYER_EQUIP_HAND_MAIN);
	hand_off = player_equipment_get(hitter, PLAYER_EQUIP_HAND_OFF);

	/* If the main hand has a weapon, use it as the hitting object. */
	if (hand_main && hand_main->type == WEAPON)
	{
		hitter_ob = weapon = hand_main;
	}
	/* No weapon, so the hitter is also the hitting object (spell/arrow/etc). */
	else
	{
		hitter_ob = hitter;
	}

	/* Offhand has a weapon, use it if main hand has no weapon or it does
	 * but we roll 50/50 chance to use the offhand weapon. */
	if (hand_off && hand_off->type == WEAPON && (!hand_main || rndm_chance(2)))
	{
		hitter_ob = weapon = hand_off;
	}

	/* Still no weapon; try to use the unarmed skill. */
	if (!weapon)
	{
		weapon = skill_get(hitter, SK_UNARMED);

		if (weapon)
		{
			hitter_ob = weapon;
		}
	}

	dam = rndm(hitter_ob->stats.dam / 2 + 1, hitter_ob->stats.dam);

	if (weapon)
	{
		object *skill;

		skill = weapon_get_skill(weapon, hitter_owner);

		if (skill)
		{
			dam *= LEVEL_DAMAGE(skill->level);
		}
	}

	if (hitter_ob != hitter && hitter->stats.dam)
	{
		dam += rndm(hitter->stats.dam / 2 + 1, hitter->stats.dam);
	}

	if (hitter_ob->slaying)
	{
		if ((target->race && strstr(hitter_ob->slaying, target->race)) || strstr(target->arch->name, hitter_ob->slaying))
		{
			if (QUERY_FLAG(hitter_ob, FLAG_IS_ASSASSINATION))
			{
				dam = (int) ((double) dam * 2.25);
			}
			else
			{
				dam = (int) ((double) dam * 1.75);
			}
		}
	}

	memset(&damage, 0, sizeof(damage));

	roll = rndm(1, 100);

	for (attacktype = ATTACK_PHYSICAL_START; attacktype <= ATTACK_PHYSICAL_END; attacktype++)
	{
		roll -= hitter_ob->attack[attacktype];

		if (roll <= 0)
		{
			damage[attacktype] = dam;
			break;
		}
	}

	for (attacktype = ATTACK_MAGICAL_START; attacktype <= ATTACK_MAGICAL_END; attacktype++)
	{
		if (hitter_ob->attack[attacktype])
		{
			damage[attacktype] = MAX(1, dam * ((double) hitter_ob->attack[attacktype] * 0.01));
		}

		if (hitter != hitter_ob && hitter->attack[attacktype])
		{
			damage[attacktype] += MAX(1, dam * ((double) hitter->attack[attacktype] * 0.01));
		}
	}

	attack_absorb_damage(hitter, target, target, damage);

	/* Try to use the shield in the offhand. */
	shield = player_equipment_get(target, PLAYER_EQUIP_HAND_OFF);

	/* Nothing in offhand, try to use main hand's weapon as a shield. */
	if (!shield)
	{
		shield = player_equipment_get(target, PLAYER_EQUIP_HAND_MAIN);

		/* Main hand is not a weapon, so no shield. */
		if (shield && shield->type != WEAPON)
		{
			shield = NULL;
		}
	}
	/* Offhand is not a shield, so no shield. */
	else if (shield->type != SHIELD)
	{
		shield = NULL;
	}

	attack_absorb_damage(hitter, target, shield, damage);

	if (rndm_chance(10))
	{
		bodypart = PLAYER_EQUIP_HELM;
	}
	else if (rndm_chance(15))
	{
		bodypart = PLAYER_EQUIP_BOOTS;
	}
	else if (rndm_chance(10))
	{
		if (rndm(1, 10) > 5)
		{
			bodypart = PLAYER_EQUIP_GAUNTLETS;
		}
		else
		{
			bodypart = PLAYER_EQUIP_BRACERS;
		}
	}
	else
	{
		if (rndm(1, 10) > 5)
		{
			bodypart = PLAYER_EQUIP_GREAVES;
		}
		else
		{
			bodypart = PLAYER_EQUIP_ARMOUR;
		}
	}

	attack_absorb_damage(hitter, target, player_equipment_get(target, bodypart), damage);

	if (bodypart == PLAYER_EQUIP_GREAVES || bodypart == PLAYER_EQUIP_ARMOUR)
	{
		attack_absorb_damage(hitter, target, player_equipment_get(target, PLAYER_EQUIP_CLOAK), damage);
	}

	if (bodypart == PLAYER_EQUIP_GREAVES && rndm_chance(25))
	{
		attack_absorb_damage(hitter, target, player_equipment_get(target, PLAYER_EQUIP_BELT), damage);
	}

	if (bodypart == PLAYER_EQUIP_GAUNTLETS && rndm_chance(50))
	{
		attack_absorb_damage(hitter, target, player_equipment_get(target, rndm_chance(2) ? PLAYER_EQUIP_RING_LEFT : PLAYER_EQUIP_RING_RIGHT), damage);
	}

	if (bodypart == PLAYER_EQUIP_ARMOUR && rndm_chance(75))
	{
		attack_absorb_damage(hitter, target, player_equipment_get(target, PLAYER_EQUIP_AMULET), damage);
	}

	for (attacktype = 0; attacktype < NROFATTACKS; attacktype++)
	{
		damage[attacktype] = attack_perform_attacktype(hitter, target, hitter_owner, attacktype, damage[attacktype]);

		if (!damage[attacktype])
		{
			continue;
		}

		if (target->damage_round_tag != global_round_tag)
		{
			target->last_damage = 0;
			target->damage_round_tag = global_round_tag;
		}

		if (hitter_owner->type == PLAYER)
		{
			CONTR(hitter_owner)->stat_damage_dealt += damage[attacktype];
		}

		if (target->type == PLAYER)
		{
			CONTR(target)->stat_damage_taken += damage[attacktype];
		}

		target->last_damage += damage[attacktype];
		target->stats.hp -= damage[attacktype];
		maxdam += damage[attacktype];
	}

	/* Target is still alive (for now). */
	if (target->stats.hp > 0)
	{
		/* Check to see if monster runs away. */
		if (QUERY_FLAG(target, FLAG_MONSTER) && target->stats.hp < (sint16) (((double) target->run_away / 100.0f) * (double) target->stats.maxhp))
		{
			SET_FLAG(target, FLAG_RUN_AWAY);
		}
	}
	/* Target has reached the death threshold. */
	else
	{
		int pvp;

		/* Trigger the death event. */
		if (trigger_event(EVENT_DEATH, hitter, target, NULL, NULL, 0, 0, 0, SCRIPT_FIX_ALL))
		{
			return -1;
		}

		play_sound_map(target->map, CMD_SOUND_EFFECT, "kill.ogg", target->x, target->y, 0, 0);

		/* Only when some damage is stored, and we're on a map. */
		if (target->damage_round_tag == global_round_tag)
		{
			SET_MAP_DAMAGE(target->map, target->x, target->y, target->last_damage);
			SET_MAP_RTAG(target->map, target->x, target->y, global_round_tag);
		}

		/* Is the victim in PvP area? */
		pvp = pvp_area(NULL, target);

		/* Player killed something. */
		if (hitter_owner->type == PLAYER)
		{
			if (hitter_owner != hitter)
			{
				draw_info_format(COLOR_WHITE, hitter_owner, "You killed %s with %s.", target->name, query_name(hitter, hitter_owner));
			}
			else
			{
				draw_info_format(COLOR_WHITE, hitter_owner, "You killed %s.", target->name);
			}

			if (target->type == MONSTER)
			{
				shstr *faction, *faction_kill_penalty;

				CONTR(hitter_owner)->stat_kills_mob++;
				statistic_update("kills", hitter_owner, 1, target->name);

				if ((faction = object_get_value(target, "faction")) && (faction_kill_penalty = object_get_value(target, "faction_kill_penalty")))
				{
					player_faction_reputation_update(CONTR(hitter_owner), faction, -atoll(faction_kill_penalty));
				}
			}
			else if (target->type == PLAYER)
			{
				CONTR(hitter_owner)->stat_kills_pvp++;
			}
		}

		/* Killed a player. */
		if (target->type == PLAYER)
		{
			if (pvp)
			{
				draw_info(COLOR_WHITE, hitter_owner, "Your foe has fallen!\nVICTORY!!!");
			}

			/* Tell everyone that this player has died. */
			if (hitter_owner != hitter)
			{
				draw_info_format(COLOR_WHITE, NULL, "%s killed %s with %s%s.", hitter_owner->name, target->name, query_name(hitter, hitter_owner), pvp ? " (duel)" : "");
			}
			else
			{
				draw_info_format(COLOR_WHITE, NULL, "%s killed %s%s.", hitter->name, target->name, pvp ? " (duel)" : "");
			}

			/* Update player's killer. */
			if (hitter_owner->type == PLAYER)
			{
				char race[MAX_BUF];

				snprintf(CONTR(target)->killer, sizeof(CONTR(target)->killer), "%s the %s", hitter_owner->name, player_get_race_class(hitter_owner, race, sizeof(race)));
			}
			else
			{
				strncpy(CONTR(target)->killer, hitter_owner->name, sizeof(CONTR(target)->killer) - 1);
			}

			/* And actually kill the player. */
			kill_player(target);
		}
		/* Monster or something else has been killed. */
		else
		{
			/* Rules:
			 * 1. Monster will drop corpse for his target, not the killer (unless killer == target).
			 * 2. NPC kill hit will override player target on drop.
			 * 3. Kill hit will count if target was an NPC. */
			if (hitter_owner->type != PLAYER || !target->enemy || target->enemy->type != PLAYER)
			{
				target->enemy = hitter_owner;
				target->enemy_count = hitter_owner->count;
			}

			/* Monster killed another monster, so no loot, but force an empty
			 * corpse anyway. */
			if (hitter_owner->type == MONSTER)
			{
				SET_FLAG(target, FLAG_STARTEQUIP);
				SET_FLAG(target, FLAG_CORPSE_FORCED);
			}

			destruct_ob(target);
		}
	}

	return maxdam;
}

/**
 * Attack a spot on the map.
 * @param op Object hitting the map.
 * @param dir Direction op is hitting/going. */
void hit_map(object *op, int dir)
{
	object *tmp;
	mapstruct *m;
	int x, y;

	if (OBJECT_FREE(op))
	{
		return;
	}

	op = HEAD(op);

	if (!op->map || !op->stats.dam)
	{
		return;
	}

	x = op->x + freearr_x[dir];
	y = op->y + freearr_y[dir];
	m = get_map_from_coord(op->map, &x, &y);

	if (!m)
	{
		return;
	}

	FOR_MAP_LAYER_BEGIN(m, x, y, LAYER_LIVING, -1, tmp)
	{
		tmp = HEAD(tmp);

		/* Cones with race set can only damage members of that race. */
		if (op->type == CONE && op->race && tmp->race != op->race)
		{
			continue;
		}

		attack_perform(op, tmp);
	}
	FOR_MAP_LAYER_END
}

/**
 * Test if objects are in range for melee attack.
 * @param hitter Attacker.
 * @param enemy Enemy.
 * @retval 0 Enemy target is not in melee range.
 * @retval 1 Target is in range and we're facing it. */
int is_melee_range(object *hitter, object *enemy)
{
	int xt, yt, i;
	object *tmp;
	mapstruct *mt;

	/* Check squares around */
	for (i = 0; i < SIZEOFFREE1 + 1; i++)
	{
		xt = hitter->x + freearr_x[i];
		yt = hitter->y + freearr_y[i];

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
