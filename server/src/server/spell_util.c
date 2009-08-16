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

#ifdef NO_ERRNO_H
extern int errno;
#else
#   include <errno.h>
#endif
#ifndef __CEXTRACT__
#include <sproto.h>
#endif
#include <sounds.h>

static int cardinal_adjust[9] = {-3, -2, -1, 0, 0, 0, 1, 2, 3};
static int diagonal_adjust[10] = {-3, -2, -2, -1, 0, 0, 1, 2, 2, 3};

archetype *spellarch[NROFREALSPELLS];
static int SP_level_gracepoint_cost(object *op, object *caster, int spell_type);

void init_spells()
{
	static int init_spells_done = 0;
	int i;

	if (init_spells_done)
		return;

	LOG(llevDebug, "Initializing spells...");
	init_spells_done = 1;

	for (i = 0; i < NROFREALSPELLS; i++)
	{
		if (spells[i].archname)
		{
			if ((spellarch[i] = find_archetype(spells[i].archname)) == NULL)
				LOG(llevError, "Error: Spell %s needs arch %s, your archetype file is out of date.\n", spells[i].name,spells[i].archname);
		}
		else
			spellarch[i] = (archetype *) NULL;
	}

	LOG(llevDebug, "done.\n");
}

void dump_spells()
{
	int i;

	for (i = 0; i < NROFREALSPELLS; i++)
	{
		const char *name1 = NULL, *name2 = NULL;

		if (spellarch[i])
		{
			name1 = spellarch[i]->name;

			if (spellarch[i]->clone.other_arch)
				name2 = spellarch[i]->clone.other_arch->name;
		}

		LOG(llevInfo, "%s: %s: %s\n", spells[i].name, (name1 ? name1 : "null"), (name2 ? name2 : "null"));
	}
}

/* this must be adjusted if we ever include multi tile effects! */
int insert_spell_effect(char *archname, mapstruct *m, int x, int y)
{
	archetype *effect_arch;
	object *effect_ob;

	if (!archname || !m)
	{
		LOG(llevBug, "BUG: insert_spell_effect: archname or map NULL.\n");
		return 1;
	}

	if (!(effect_arch = find_archetype(archname)))
	{
		LOG(llevBug, "BUG: insert_spell_effect: Couldn't find effect arch (%s).\n", archname);
		return 1;
	}

	/* prepare effect */
	effect_ob = arch_to_object(effect_arch);
	effect_ob->map = m;
	effect_ob->x = x;
	effect_ob->y = y;

	if (!insert_ob_in_map(effect_ob, m, NULL, 0))
	{
		LOG(llevBug, "BUG: insert_spell_effect: effect arch (%s) out of map (%s) (%d,%d) or failed insertion.\n", archname, effect_ob->map->name, x, y);
		/* something is wrong - kill object */
		if (!QUERY_FLAG(effect_ob, FLAG_REMOVED))
		{
			remove_ob(effect_ob);
			check_walk_off(effect_ob, NULL, MOVE_APPLY_VANISHED);
		}
		return 1;
	}


	return 0;
}

spell *find_spell(int spelltype)
{
	if (spelltype < 0 || spelltype > NROFREALSPELLS)
		return NULL;

	return &spells[spelltype];
}

/* base_level: level before considering attuned/repelled paths
 * Returns modified level. */
int path_level_mod(object *caster, int base_level, int spell_type)
{
	spell *s = find_spell(spell_type);
	int new_level;

	if (!s)
		return 1;

	if (caster->path_denied & s->path)
	{
		/* This case is not a bug, just the fact that this function is
		 * usually called BEFORE checking for path_deny. -AV
		LOG(llevBug, "BUG: path_level_mod (arch %s, name %s): casting denied spell\n", caster->arch->name, query_name(caster)); */
		return 1;
	}

	new_level = base_level + ((caster->path_repelled & s->path) ? -5 : 0) + ((caster->path_attuned & s->path) ? 5 : 0);
	return (new_level < 1) ? 1 : new_level;
}

int casting_level(object *caster, int spell_type)
{
	return path_level_mod(caster, SK_level(caster), spell_type);
}


int check_spell_known(object *op, int spell_type)
{
	int i;

	for (i = 0; i < (int)CONTR(op)->nrofknownspells; i++)
		if (CONTR(op)->known_spells[i] == spell_type)
			return 1;

	return 0;
}


/* cast_spell():
 * Fires spell "type" in direction "dir".
 * If "ability" is true, the spell is the innate ability of a monster.
 * (ie, don't check for blocks_magic(), and don't add AT_MAGIC to attacktype.
 *
 * op is the creature that is owner of the object that is casting the spell
 * caster is the actual object (wand, potion) casting the spell. can be
 *    same as op.
 * dir is the direction to cast in.
 * ability is true if it is an ability casting the spell.  These can be
 *    cast in no magic areas.
 * item is the type of object that is casting the spell.
 * stringarg is any options that are being used. */

/* Oct 95 - added cosmetic touches for MULTIPLE_GODS hack -b.t. */
int cast_spell(object *op, object *caster, int dir, int type, int ability, SpellTypeFrom item, char *stringarg)
{
	spell *s = find_spell(type);
	const char *godname = NULL;
	object *target = NULL, *cast_op;
	int success = 0, duration, points_used = 0;
	rv_vector rv;

	if (s == NULL)
	{
		LOG(llevBug, "BUG: unknown spell: %d\n", type);
		return 0;
	}

	/*  get the base duration */
	duration = spells[type].bdur;

	if (!op)
		op = caster;

	/* script NPC can ALWAYS cast - even in no spell areas! */
	if (item == spellNPC)
	{
		/* if spellNPC, this comes useally from a script */
		target = op;
		/* and caster is the NPC and op the target */
		op = caster;
		/* change the pointers to fit this function and jump */
		goto dirty_jump;
	}

	/* It looks like the only properties we ever care about from the casting
	 * object (caster) is spell paths and level. */
	cast_op = op;
	if (!caster)
	{
		if (item == spellNormal)
			caster = op;
	}
	else
	{
		/* caster has a map? then we use caster */
		if (caster->map)
			cast_op = caster;
	}

	/* Now check we can cast this spell! */
	/* we need to ad here floor flags too! */

	/* no magic & not a prayer... */
	if (MAP_NOMAGIC(cast_op->map) && !(spells[type].flags & SPELL_DESC_WIS))
	{
		if (op->type == PLAYER)
			new_draw_info(NDI_UNIQUE, 0, op, "Powerful countermagic cancels all spellcasting here!");

		return 0;
	}

	/* no prayer & and a prayer... */
	if (MAP_NOPRIEST(cast_op->map) && (spells[type].flags & SPELL_DESC_WIS))
	{
		if (op->type == PLAYER)
			new_draw_info(NDI_UNIQUE, 0, op, "Powerful countermagic cancels all prayer spells here!");

		return 0;
	}

	/* no harm spell & not town safe */
	if (MAP_NOHARM(cast_op->map) && !(spells[type].flags & SPELL_DESC_TOWN))
	{
		if (op->type == PLAYER)
			new_draw_info(NDI_UNIQUE, 0, op, "Powerful countermagic cancels all harmful magic here!");

		return 0;
	}

	/* no summon & a summon cast */
	if (MAP_NOSUMMON(cast_op->map) && (spells[type].flags & SPELL_DESC_SUMMON))
	{
		if (op->type == PLAYER )
			new_draw_info(NDI_UNIQUE, 0, op, "Powerful countermagic cancels all summoning here!");

		return 0;
	}

	/* ok... its item == spellNPC then op is the target of this spell  */
	if (op->type == PLAYER)
	{
		CONTR(op)->praying = 0;
		/* cancel player spells which are denied - only real spells (not potion, wands, ...) */
		if (item == spellNormal)
		{
			if (caster->path_denied & s->path)
			{
				new_draw_info(NDI_UNIQUE, 0, op, "It is denied for you to cast that spell.");
				return 0;
			}

			if (!(QUERY_FLAG(op, FLAG_WIZ)))
			{
				if (!(spells[type].flags & SPELL_DESC_WIS) && op->stats.sp < (points_used = SP_level_spellpoint_cost(op, caster, type)))
				{
					new_draw_info(NDI_UNIQUE, 0, op, "You don't have enough mana.");
					return 0;
				}

				if ((spells[type].flags & SPELL_DESC_WIS) && op->stats.grace < (points_used = SP_level_gracepoint_cost(op, caster, type)))
				{
					new_draw_info(NDI_UNIQUE, 0, op, "You don't have enough grace.");
					return 0;
				}
			}
		}

		/* if it a prayer, grab the players god - if we have non, we can't cast - except potions */
		if (spells[type].flags & SPELL_DESC_WIS && item != spellPotion)
		{
			if (!strcmp((godname = determine_god(op)), "none"))
			{
				new_draw_info(NDI_UNIQUE, 0, op, "You need a deity to cast a prayer!");
				return 0;
			}
		}
	}

	/* If it is an ability, assume that the designer of the archetype knows what they are doing.*/
	if (item == spellNormal && !ability && SK_level(caster) < s->level && !QUERY_FLAG(op, FLAG_WIZ))
	{
		if (op->type == PLAYER)
			new_draw_info(NDI_UNIQUE, 0, op, "You lack enough skill to cast that spell.");

		return 0;
	}

	/* ok.... now we are sure we are able to cast.
	 * perhaps some else happens but first we look for
	 * a valid target. */

	/* applying potions always go in the applier itself (aka drink or break) */
	if (item == spellPotion)
	{
		/* if the potion casts an onself spell, don't use the facing direction (given by apply.c)*/
		if (spells[type].flags & SPELL_DESC_SELF)
		{
			target = op;
			dir = 0;
		}
	}
	else if (find_target_for_spell(op, caster, &target, dir, spells[type].flags) == FALSE)
	{
		/* little trick - if we fail we set target== NULL - that marks it "yourself" */
		if (op->type == PLAYER)
			new_draw_info_format(NDI_UNIQUE, 0, op, "You can't cast this spell on %s!", target ? target->name : "yourself");

		return 0;
	}

	/*LOG(llevInfo, "TARGET: op: %s target: %s\n", op->name, target ? target->name : "<none>");*/

	/* if valid target is not in range for selected spell, skip here casting */
	if (target)
	{
		get_rangevector_from_mapcoords(op->map, op->x, op->y, target->map, target->x, target->y, &rv, 0);
		if ((abs(rv.distance_x) > abs(rv.distance_y) ? abs(rv.distance_x) : abs(rv.distance_y)) > spells[type].range)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "Your target is out of range!");
			return 0;
		}
	}

	/* tell player his spell is redirect to himself */
	if (op->type == PLAYER && target == op && CONTR(op)->target_object != op)
		new_draw_info(NDI_UNIQUE, 0, op, "You auto-target yourself with this spell!");

	/*  ban removed on clerical spells in no-magic areas */
	if (!ability && (((!s->flags & SPELL_DESC_WIS) && blocks_magic(op->map, op->x, op->y)) || ((s->flags & SPELL_DESC_WIS) && blocks_cleric(op->map, op->x, op->y))))
	{
		if (op->type != PLAYER)
			return 0;

		if (s->flags & SPELL_DESC_WIS)
			new_draw_info_format(NDI_UNIQUE, 0, op, "This ground is unholy! %s ignores you.", godname);
		else
		{
			switch (CONTR(op)->shoottype)
			{
				case range_magic:
					new_draw_info(NDI_UNIQUE, 0, op, "Something blocks your spellcasting.");
					break;

				case range_wand:
					new_draw_info(NDI_UNIQUE, 0, op, "Something blocks the magic of your wand.");
					break;

				case range_rod:
					new_draw_info(NDI_UNIQUE, 0, op, "Something blocks the magic of your rod.");
					break;

				case range_horn:
					new_draw_info(NDI_UNIQUE, 0, op, "Something blocks the magic of your horn.");
					break;

				case range_scroll:
					new_draw_info(NDI_UNIQUE, 0, op, "Something blocks the magic of your scroll.");
					break;

				case range_potion:
					new_draw_info(NDI_UNIQUE, 0, op, "Something blocks the magic of your potion.");
					break;

				case range_dust:
					new_draw_info(NDI_UNIQUE, 0, op, "Something blocks the magic of your dust.");
					break;

				default:
					break;
			}
		}

		return 0;
	}

	/* chance to fumble the spell by to low wisdom */
	if (item == spellNormal && op->type == PLAYER && s->flags & SPELL_DESC_WIS && random_roll(0, 99, op, PREFER_HIGH) < s->level / (float)MAX(1, op->chosen_skill->level) * cleric_chance[op->stats.Wis])
	{
		play_sound_player_only(CONTR(op), SOUND_FUMBLE_SPELL, SOUND_NORMAL, 0, 0);
		new_draw_info(NDI_UNIQUE, 0, op, "You fumble the prayer because your wisdom is low.");

		/* Shouldn't happen... */
		if (s->sp == 0)
			return 0;

		return random_roll(1, SP_level_spellpoint_cost(op, caster, type), op, PREFER_LOW);
	}

	if (item == spellNormal && op->type == PLAYER && (!s->flags & SPELL_DESC_WIS))
	{
		int failure = random_roll(0, 199, op, PREFER_LOW) - CONTR(op)->encumbrance + op->chosen_skill->level -s->level + 35;

		if (failure < 0)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "You bungle the spell because you have too much heavy equipment in use.");
			return random_roll(0, SP_level_spellpoint_cost(op, caster, type), op, PREFER_LOW);
		}
	}

	/* now lets talk about action/shooting speed */
	if (op->type == PLAYER)
	{
		switch (CONTR(op)->shoottype)
		{
			case range_wand:
			case range_rod:
			case range_horn:
				op->chosen_skill->stats.maxsp = caster->last_grace;
				break;

#if 0
			case range_scroll:
			case range_potion:
			case range_magic:
			case range_dust:
#endif
			default:
				break;
		}
	}

dirty_jump:
	/* a last sanity check: are caster and target *really* valid? */
	if ((caster && !OBJECT_ACTIVE(caster)) || (target && !OBJECT_ACTIVE(target)))
		return 0;

	switch ((enum spellnrs) type)
	{
#if 0
		case SP_RESTORATION:
		case SP_HEAL:
		case SP_MED_HEAL:
		case SP_MAJOR_HEAL:
		case SP_CURE_CONFUSION:
		case SP_CURE_BLINDNESS:
#endif
		case SP_MINOR_HEAL:
		case SP_CURE_POISON:
		case SP_CURE_DISEASE:
			success = cast_heal(op, SK_level(caster), target, type);
			break;

		case SP_REMOVE_DEPLETION:
			success = remove_depletion(op, target);
			break;

		case SP_REMOVE_CURSE:
		case SP_REMOVE_DAMNATION:
			success = remove_curse(op, target, type, item);
			break;

		case SP_STRENGTH:
			success = cast_change_attr(op, caster, target, dir, type);
			break;

		case SP_DETECT_MAGIC:
		case SP_DETECT_CURSE:
			success = cast_detection(op, target, type);
			break;

		case SP_IDENTIFY:
			success = cast_identify(target, SK_level(caster), NULL, IDENTIFY_MODE_NORMAL);
			break;

			/* spells after this use direction and not a target */
		case SP_ICESTORM:
		case SP_FIRESTORM:
			success = cast_cone(op, caster, dir, duration, type, spellarch[type], !ability);
			break;

#if 0
		case SP_BULLET:
		case SP_LARGE_BULLET:
			success = fire_arch(op, caster, dir, spellarch[type], type, 1);
			break;

		case SP_HOLY_ORB:
			success = fire_arch(op, caster, dir, spellarch[type], type, 0);
			break;

		case SP_S_FIREBALL:
		case SP_M_FIREBALL:
		case SP_L_FIREBALL:
		case SP_S_SNOWSTORM:
		case SP_M_SNOWSTORM:
		case SP_L_SNOWSTORM:
		case SP_HELLFIRE:
		case SP_POISON_CLOUD:
		case SP_M_MISSILE:
		case SP_S_MANABALL:
		case SP_M_MANABALL:
		case SP_L_MANABALL:
		case SP_PROBE:
			success = fire_arch(op, caster, dir, spellarch[type], type, !ability);
			break;

		case SP_VITRIOL:
			success = fire_arch(op, caster, dir, spellarch[type], type, 0);
			break;

		case SP_MASS_CONFUSION:
		case SP_SHOCKWAVE:
		case SP_COLOR_SPRAY:
		case SP_FACE_OF_DEATH:
		case SP_COUNTER_SPELL:
		case SP_PARALYZE:
		case SP_SLOW:
		case SP_ICESTORM:
		case SP_FIREBREATH:
		case SP_LARGE_ICESTORM:
		case SP_BANISHMENT:
		case SP_MANA_BLAST:
		case SP_WINDSTORM:
		case SP_PEACE:
		case SP_SPIDERWEB:
		case SP_VITRIOL_SPLASH:
		case SP_WRATHFUL_EYE:
			success = cast_cone(op, caster, dir, duration, type, spellarch[type], !ability);
			break;

		case SP_TURN_UNDEAD:
			/* the undead *don't* cast this */
			if (QUERY_FLAG(op,FLAG_UNDEAD))
			{
				new_draw_info(NDI_UNIQUE, 0, op, "Your undead nature prevents you from turning undead!");
				success = 0;
				break;
			}

		case SP_HOLY_WORD:
			success = cast_cone(op, caster, dir, duration + (turn_bonus[op->stats.Wis] / 5), type, spellarch[type], 0);
			break;

		case SP_HOLY_WRATH:
		case SP_INSECT_PLAGUE:
		case SP_RETRIBUTION:
			success = cast_smite_spell(op, caster, dir, type);
			break;

		case SP_SUNSPEAR:
		case SP_FIREBOLT:
		case SP_FROSTBOLT:
		case SP_S_LIGHTNING:
		case SP_L_LIGHTNING:
		case SP_FORKED_LIGHTNING:
		case SP_STEAMBOLT:
		case SP_MANA_BOLT:
			success = fire_bolt(op, caster, dir, type, !ability);
			break;

		case SP_BOMB:
			success = create_bomb(op, caster, dir, type, "bomb");
			break;

		case SP_GOLEM:
		case SP_FIRE_ELEM:
		case SP_WATER_ELEM:
		case SP_EARTH_ELEM:
		case SP_AIR_ELEM:
			success = summon_monster(op, caster, dir, spellarch[type], type);
			break;

		case SP_FINGER_DEATH:
			success = finger_of_death(op, caster, dir);
			break;

		case SP_SUMMON_AVATAR:
		case SP_HOLY_SERVANT:
		{
			archetype *spat = find_archetype((type == SP_SUMMON_AVATAR) ? "avatar" : "holy_servant");
			success = summon_avatar(op, caster, dir, spat, type);
			break;
		}

		case SP_CONSECRATE:
			success = cast_consecrate(op);
			break;

		case SP_SUMMON_CULT:
			success = summon_cult_monsters(op, dir);
			break;

		case SP_PET:
			success = summon_pet(op, dir, item);
			break;

		case SP_D_DOOR:
			/* dimension door needs the actual caster, because that is what is moved. */
			success = dimension_door(op, dir);
			break;

		case SP_DARKNESS:
		case SP_WALL_OF_THORNS:
		case SP_CHAOS_POOL:
		case SP_COUNTERWALL:
		case SP_FIRE_WALL:
		case SP_FROST_WALL:
		case SP_EARTH_WALL:
			success = magic_wall(op, caster, dir, type);
			break;

		case SP_MAGIC_MAPPING:
			break;

		case SP_FEAR:
			if (op->type == PLAYER)
				bonus = fear_bonus[op->stats.Cha];
			else
				bonus = caster->head == NULL ? caster->level / 3 + 1 : caster->head->level / 3 + 1;

			success = cast_cone(op, caster, dir, duration + bonus, SP_FEAR, spellarch[type], !ability);
			break;

		case SP_WOW:
			success = cast_wow(op, dir, ability, item);
			break;

		case SP_DESTRUCTION:
			success = cast_destruction(op, caster, 5 + op->stats.Int, AT_MAGIC);
			break;

		case SP_PERCEIVE:
			success = perceive_self(op);
			break;

		case SP_INVIS:
		case SP_INVIS_UNDEAD:
		case SP_IMPROVED_INVIS:
			success = cast_invisible(op, caster, type);
			break;

		case SP_EARTH_DUST:
			success = cast_earth2dust(op, caster);
			break;

		case SP_REGENERATION:
		case SP_BLESS:
		case SP_CURSE:
		case SP_HOLY_POSSESSION:
		case SP_DEXTERITY:
		case SP_CONSTITUTION:
		case SP_CHARISMA:
		case SP_ARMOUR:
		case SP_IRONWOOD_SKIN:
		case SP_PROT_COLD:
		case SP_PROT_FIRE:
		case SP_PROT_ELEC:
		case SP_PROT_POISON:
		case SP_PROT_SLOW:
		case SP_PROT_DRAIN:
		case SP_PROT_PARALYZE:
		case SP_PROT_ATTACK:
		case SP_PROT_MAGIC:
		case SP_PROT_CONFUSE:
		case SP_PROT_CANCEL:
		case SP_PROT_DEPLETE:
		case SP_LEVITATE:
		case SP_HEROISM:
		case SP_CONFUSION:
		case SP_XRAY:
		case SP_DARK_VISION:
		case SP_RAGE:
			success = cast_change_attr(op, caster, dir, type);
			break;

		case SP_REGENERATE_SPELLPOINTS:
			success = cast_regenerate_spellpoints(op);
			break;

		case SP_SMALL_SPEEDBALL:
		case SP_LARGE_SPEEDBALL:
			success = cast_speedball(op, dir, type);
			break;

		case SP_CANCELLATION:
			success = fire_cancellation(op, dir, spellarch[type], !ability);
			break;

		case SP_ALCHEMY:
			success = alchemy(op);
			break;

		case SP_DETECT_MONSTER:
		case SP_DETECT_EVIL:
		case SP_SHOW_INVIS:
			success = cast_detection(op, type);
			break;

		case SP_AGGRAVATION:
			aggravate_monsters(op);
			success = 1;
			break;

		case SP_BALL_LIGHTNING:
		case SP_DIVINE_SHOCK:
		case SP_POISON_FOG:
			success = fire_arch(op, caster, dir, spellarch[type], type, !ability);
			break;

		case SP_METEOR_SWARM:
		{
			success = 1;
			fire_swarm(op, caster, dir, spellarch[type], SP_METEOR, die_roll(3, 3, op, PREFER_HIGH) + SP_level_strength_adjust(op, caster, type), 0);
			break;
		}

		case SP_BULLET_SWARM:
		{
			success = 1;
			fire_swarm(op, caster, dir, spellarch[type], SP_BULLET, die_roll(3, 3, op, PREFER_HIGH) + SP_level_strength_adjust(op, caster, type), 0);
			break;
		}

		case SP_BULLET_STORM:
		{
			success = 1;
			fire_swarm(op, caster, dir, spellarch[type], SP_LARGE_BULLET, die_roll(3, 3, op, PREFER_HIGH) + SP_level_strength_adjust(op, caster, type), 0);
			break;
		}

		case SP_CAUSE_MANY:
		{
			success = 1;
			fire_swarm(op, caster, dir, spellarch[type], SP_CAUSE_HEAVY, die_roll(3, 3, op, PREFER_HIGH) + SP_level_strength_adjust(op, caster, type), 0);
			break;
		}

		case SP_METEOR:
			success = fire_arch(op, caster, dir, find_archetype("meteor"), type, 0);
			break;

		case SP_MYSTIC_FIST:
			success = summon_monster(op, caster, dir, spellarch[type], type);
			break;

		case SP_RAISE_DEAD:
		case SP_RESURRECTION:
			success = cast_raise_dead_spell(op, dir, type, NULL);
			break;

		case SP_IMMUNE_COLD:
		case SP_IMMUNE_FIRE:
		case SP_IMMUNE_ELEC:
		case SP_IMMUNE_POISON:
		case SP_IMMUNE_SLOW:
		case SP_IMMUNE_DRAIN:
		case SP_IMMUNE_PARALYZE:
		case SP_IMMUNE_ATTACK:
		case SP_IMMUNE_MAGIC:
		case SP_INVULNERABILITY:
		case SP_PROTECTION:
		case SP_HASTE:
			success = cast_change_attr(op, caster, dir, type);
			break;

		case SP_BUILD_DIRECTOR:
		case SP_BUILD_BWALL:
		case SP_BUILD_LWALL:
		case SP_BUILD_FWALL:
			success = create_the_feature(op, caster, dir, type);
			break;

		case SP_RUNE_FIRE:
		case SP_RUNE_FROST:
		case SP_RUNE_SHOCK:
		case SP_RUNE_BLAST:
		case SP_RUNE_DEATH:
		case SP_RUNE_ANTIMAGIC:
			success = write_rune(op, dir, 0, caster->level, s->archname);
			break;

		case SP_RUNE_DRAINSP:
			success = write_rune(op, dir, SP_MAGIC_DRAIN, caster->level, s->archname);
			break;

		case SP_RUNE_TRANSFER:
			success = write_rune(op, dir, SP_TRANSFER, caster->level, s->archname);
			break;

		case SP_TRANSFER:
			success = cast_transfer(op, dir);
			break;

		case SP_MAGIC_DRAIN:
			success = drain_magic(op, dir);
			break;

		case SP_DISPEL_RUNE:
			/* 0 means no risk of detonating rune */
			success = dispel_rune(op, dir, 0);
			break;

		case SP_SUMMON_EVIL_MONST:
			if (op->type == PLAYER)
				return 0;

			success = summon_hostile_monsters(op, op->stats.maxhp, op->race);
			break;

		case SP_REINCARNATION:
		{
			object * dummy;
			if (stringarg == NULL)
			{
				new_draw_info(NDI_UNIQUE, 0,op, "Reincarnate WHO?");
				success = 0;
				break;
			}
			dummy = get_object();
			FREE_AND_COPY_HASH(dummy->name, stringarg);
			success = cast_raise_dead_spell(op, dir, type, dummy);
			break;
		}

		case SP_RUNE_MAGIC:
		{
			int total_sp_cost, spellinrune;
			spellinrune = look_up_spell_by_name(op, stringarg);
			if (spellinrune != -1)
			{
				total_sp_cost = SP_level_spellpoint_cost(op, caster, spellinrune) + spells[spellinrune].sp;
				if (op->stats.sp < total_sp_cost)
				{
					new_draw_info(NDI_UNIQUE, 0, op, "Not enough spellpoints.");
					return 0;
				}

				success = write_rune(op, dir, spellinrune, caster->level, stringarg);
				return (success ? total_sp_cost : 0);
			}

			return 0;
		}

		case SP_RUNE_MARK:
			if (caster->type == PLAYER)
				success = write_rune(op, dir, 0, -2, stringarg);
			else
				success = 0;

			break;

		case SP_LIGHT:
			success = cast_light(op, caster, dir);
			break;

		case SP_DAYLIGHT:
			success = cast_daylight(op);
			break;

		case SP_NIGHTFALL:
			success = cast_nightfall(op);
			break;

		case SP_FAERY_FIRE:
			success = cast_faery_fire(op, caster);
			break;

		case SP_SUMMON_FOG:
			success = summon_fog(op, caster, dir, type);
			break;

		case SP_PACIFY:
			cast_pacify(op, caster, spellarch[type], type);
			success = 1;
			break;

		case SP_COMMAND_UNDEAD:
			cast_charm_undead(op, caster, spellarch[type], type);
			success = 1;
			break;

		case SP_CHARM:
			cast_charm(op, caster, spellarch[type], type);
			success = 1;
			break;

		case SP_CAUSE_COLD:
		case SP_CAUSE_EBOLA:
		case SP_CAUSE_FLU:
		case SP_CAUSE_PLAGUE:
		case SP_CAUSE_LEPROSY:
		case SP_CAUSE_SMALLPOX:
		case SP_CAUSE_PNEUMONIC_PLAGUE:
		case SP_CAUSE_ANTHRAX:
		case SP_CAUSE_TYPHOID:
		case SP_CAUSE_RABIES:
			success = cast_cause_disease(op, caster, dir, spellarch[type], type);
			break;

		case SP_DANCING_SWORD:
		case SP_STAFF_TO_SNAKE:
		case SP_ANIMATE_WEAPON:
			success = animate_weapon(op, caster, dir, spellarch[type], type);
			break;

		case SP_SANCTUARY:
		case SP_FLAME_AURA:
			success = create_aura(op, caster, spellarch[type], type, 0);
			break;

		case SP_CONFLICT:
			success = cast_cause_conflict(op, caster, spellarch[type], type);
			break;

		case SP_MISSILE_SWARM:
		{
			success = 1;
			fire_swarm(op, caster, dir, spellarch[type], SP_M_MISSILE, die_roll(3, 3, op, PREFER_HIGH) + SP_level_strength_adjust(op, caster, type), 0);
			break;
		}
#endif

#if 0
		case SP_CAUSE_HEAVY:
		case SP_CAUSE_MEDIUM:
		case SP_CAUSE_CRITICAL:
		case SP_LARGE_BULLET:
#endif

		case SP_BULLET:
		case SP_CAUSE_LIGHT:
		case SP_PROBE:
			success = fire_arch(op, caster, dir, spellarch[type], type, 1);
			break;

		case SP_TOWN_PORTAL:
			success = cast_create_town_portal(op, caster, dir);
			break;

		case SP_CREATE_MISSILE:
			success = cast_create_missile(op, caster, dir, stringarg);
			break;

		case SP_WOR:
			success = cast_wor(op, caster);
			break;

		case SP_CREATE_FOOD:
			success = cast_create_food(op, caster, dir, stringarg);
			break;

		case SP_CHARGING:
			success = recharge(op);
			break;

		default:
			LOG(llevBug, "BUG: cast_spell() has invalid spell nr. %d, town portal is %d\n", type, SP_TOWN_PORTAL);
			break;
	}

	play_sound_map(op->map, op->x, op->y, spells[type].sound, SOUND_SPELL);

	if (item == spellNPC)
		return success;

#ifdef SPELLPOINT_LEVEL_DEPEND
	return success ? SP_level_spellpoint_cost(op, caster, type) : 0;
#else
	return success ? (s->sp * PATH_SP_MULT(op, s)) : 0;
#endif
}

int cast_create_obj(object *op, object *caster, object *new_op, int dir)
{
	mapstruct *mt;
	int xt, yt;

	(void) caster;

	xt = op->x + freearr_x[dir];
	yt = op->y + freearr_y[dir];
	if (!(mt = out_of_map(op->map, &xt, &yt)))
		return 0;

	if (dir && wall_blocked(mt, xt, yt))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Something is in the way.");
		new_draw_info(NDI_UNIQUE, 0, op, "You cast it at your feet.");
		dir = 0;
	}

	xt = op->x + freearr_x[dir];
	yt = op->y + freearr_y[dir];
	if (!(mt = out_of_map(op->map, &xt, &yt)))
		return 0;

	new_op->x = xt;
	new_op->y = yt;
	new_op->map = mt;
	insert_ob_in_map(new_op, mt, op, 0);
	return dir;
}

int summon_monster(object *op, object *caster, int dir, archetype *at, int spellnum)
{
	object *tmp;
	mapstruct *mt;
	int xt, yt;

	if (op->type == PLAYER)
	{
		if (CONTR(op)->golem != NULL && !OBJECT_FREE(CONTR(op)->golem))
		{
			control_golem(CONTR(op)->golem, dir);
			return 0;
		}
	}

	if (!dir)
		dir = find_free_spot(NULL, NULL, op->map, op->x, op->y, 1, 9);

	if (dir != -1)
	{
		xt = op->x + freearr_x[dir];
		yt = op->y + freearr_y[dir];
		if (!(mt = out_of_map(op->map, &xt, &yt)))
			return 0;

		tmp = arch_to_object(at);
	}

	if ((dir == -1) || blocked(tmp, mt, xt, yt, tmp->terrain_flag))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "There is something in the way.");
		return 0;
	}

	if (op->type == PLAYER)
	{
		CLEAR_FLAG(tmp, FLAG_MONSTER);
		SET_FLAG(tmp, FLAG_FRIENDLY);
		tmp->stats.exp = 0;
		add_friendly_object(tmp);
		tmp->type = GOLEM;
		/* Don't see any point in setting this when monsters summon monsters: */
		set_owner(tmp, op);
		CONTR(op)->golem = tmp;
		/* give the player control of the golem */
		send_golem_control(tmp, GOLEM_CTR_ADD);
	}
	else
	{
		if (QUERY_FLAG(op, FLAG_FRIENDLY))
		{
			object *owner = get_owner(op);

			/* For now, we transfer ownership */
			if (owner != NULL)
			{
				set_owner(tmp, owner);
				tmp->move_type = PETMOVE;
				add_friendly_object(tmp);
				SET_FLAG(tmp, FLAG_FRIENDLY);
			}
		}

		SET_FLAG(tmp, FLAG_MONSTER);
	}

	/* make the speed positive.*/
	if (tmp->speed < 0)
		tmp->speed = -tmp->speed;

	/*  This sets the level dependencies on dam and hp for monsters */
	if (op->type == PLAYER)
	{
		/* players can't cope with too strong summonings,
		 * but monsters can.  reserve these for players. */
		tmp->stats.hp = spells[spellnum].bdur + 10 * SP_level_strength_adjust(op, caster, spellnum);
		tmp->stats.dam= spells[spellnum].bdam + 2 * SP_level_dam_adjust(op, caster, spellnum);
		tmp->speed += 0.02f * (int)SP_level_dam_adjust(op, caster, spellnum);
		tmp->speed = MIN(tmp->speed, 1.0f);
	}

	tmp->stats.wc += SP_level_dam_adjust(op, caster, spellnum);

	/* limit the speed to 0.3 for non-players, 1 for players. */

	/* seen this go negative! */
	if (tmp->stats.dam < 0)
		tmp->stats.dam = 127;

	/* make experience increase in proportion to the strength of the summoned creature. */
	tmp->stats.exp *= SP_level_spellpoint_cost(op, caster, spellnum) / spells[spellnum].sp;
	tmp->speed_left = -1;
	tmp->x = xt;
	tmp->y = yt;
	tmp->map = mt;
	tmp->direction = dir;
	insert_ob_in_map(tmp, mt, op, 0);
	return 1;
}

/* Returns true if it is ok to put spell *op on the space/may provided.
 * Not sure what immune_stop was supposed to do - the only functions that
 * current call this are move_cone and explosion, and both are just
 * passing op->attacktype on.
 * My guess judging from the old code is that if the space has a monster
 * immune to immune_stop, the spell should not be placed.  But that
 * doesn't make a lot of sense to me (a dragon immune to fire should not
 * cause a fireball not to explode
 *
 * 0.94.2 - rewrote this to include the wall check instead of the calling
 * function doing so. */
static inline int ok_to_put_more(mapstruct *m, int x, int y, object *op, int immune_stop)
{
	object *tmp;

	(void) immune_stop;

	/* we must check map here or we will go in trouble some line down */
	if (!(m = out_of_map(m, &x, &y)))
		return 0;

	/* nasty ... only REAL walls will block this - except player only tiles */
	if (wall(m, x, y))
		return 0;

	for (tmp = get_map_ob(m, x, y); tmp != NULL; tmp = tmp->above)
	{
		/* this *should* be enough! but for merging, we REALLY should
		 * compare the owners too! */

		/* only one part for cone/explosion per tile! */
		if (op->type == tmp->type && op->weight_limit == tmp->weight_limit)
			return 0;

#if 0
		/* old code for counterspell */
		if ((tmp->attacktype & AT_COUNTERSPELL) && (tmp->type != PLAYER) && !QUERY_FLAG(tmp, FLAG_MONSTER) && (tmp->type != WEAPON) && (tmp->type != BOW) && (tmp->type != ARROW) && (tmp->type != GOLEM) && (immune_stop & AT_MAGIC))
			return 0;
#endif
	}

	/* If it passes the above tests, it must be OK */
	return 1;
}

int fire_bolt(object *op, object *caster, int dir, int type, int magic)
{
	object *tmp = NULL;

	(void) magic;

	if (!spellarch[type])
		return 0;

	tmp = arch_to_object(spellarch[type]);
	if (tmp == NULL)
		return 0;
	/* peterm: level dependency for bolts */

#if 0
	tmp->stats.dam = spells[type].bdam + SP_level_dam_adjust(op, caster, type);

	if (magic)
		tmp->attacktype |= AT_MAGIC;
#endif

	/* we should have from the arch type the default damage - we add our new damage profil */
	tmp->stats.dam = (sint16) SP_lvl_dam_adjust2(caster, type, tmp->stats.dam);

	/* i use for duration the old formula */
	tmp->stats.hp = spells[type].bdur + SP_level_strength_adjust(op, caster, type);

	tmp->x = op->x, tmp->y = op->y;
	tmp->direction = dir;

	if (QUERY_FLAG(tmp, FLAG_IS_TURNABLE))
		SET_ANIMATION(tmp, (NUM_ANIMATIONS(tmp) / NUM_FACINGS(tmp)) * tmp->direction);

	set_owner(tmp, op);
	tmp->level = SK_level(caster);
	tmp->x += DIRX(tmp), tmp->y += DIRY(tmp);

	if (wall(op->map, tmp->x, tmp->y))
	{
		if (!QUERY_FLAG(tmp, FLAG_REFLECTING))
			return 0;

		tmp->x = op->x, tmp->y = op->y;
		tmp->direction = absdir(tmp->direction + 4);
	}

	if ((tmp = insert_ob_in_map(tmp, op->map, op, 0)) != NULL)
		move_bolt(tmp);

	return 1;
}

/* peterm  added a type field to fire_arch.  Needed it for making
   fireball etall level dependent.
   Later added a ball-lightning firing routine.
 * dir is direction, at is spell we are firing.  Type is index of spell
 * array.  If magic is 1, then add magical attacktype to spell.
 * op is either the owner of the spell (player who gets exp) or the
 * casting object owned by the owner.  caster is the casting object. */
int fire_arch (object *op, object *caster, int dir, archetype *at, int type, int magic)
{
	return fire_arch_from_position(op, caster, op->x, op->y, dir, at, type, magic);
}

int fire_arch_from_position (object *op, object *caster, sint16 x, sint16 y, int dir, archetype *at, int type, int magic)
{
	object *tmp, *env;

	if (at == NULL)
		return 0;

	for (env = op; env->env != NULL; env = env->env);

	if (env->map == NULL)
		return 0;

	tmp = arch_to_object(at);
	if (tmp == NULL)
		return 0;

	tmp->stats.sp=type;
	tmp->stats.dam = (sint16) SP_lvl_dam_adjust2(caster, type, tmp->stats.dam);
	tmp->stats.hp = spells[type].bdur + SP_level_strength_adjust(op, caster, type);
	tmp->x = x, tmp->y = y;
	tmp->direction = dir;
	tmp->stats.grace = tmp->last_sp;
	tmp->stats.maxgrace = 60 + (RANDOM() % 12);

	if (get_owner(op) != NULL)
		copy_owner(tmp, op);
	else
		set_owner(tmp, op);

	tmp->level = casting_level(caster, type);

	/* needed for AT_HOLYWORD,AT_GODPOWER stuff */
	if (tmp->attacktype & AT_HOLYWORD || tmp->attacktype & AT_GODPOWER)
	{
		if (!tailor_god_spell(tmp, op))
			return 0;
	}
	else
	{
		if (magic)
			tmp->attacktype |= AT_MAGIC;
	}

	if (QUERY_FLAG(tmp, FLAG_IS_TURNABLE))
		SET_ANIMATION(tmp, (NUM_ANIMATIONS(tmp) / NUM_FACINGS(tmp)) * dir);

	if ((tmp = insert_ob_in_map(tmp, op->map, op, 0)) == NULL)
		return 1;

	switch (type)
	{
		case SP_M_MISSILE:
			move_missile(tmp);
			break;

		case SP_POISON_FOG:
		case SP_DIVINE_SHOCK:
		case SP_BALL_LIGHTNING:
#if 0
			tmp->stats.food = spells[type].bdur + 4 * SP_level_strength_adjust(op, caster, type);
#endif
			move_ball_lightning(tmp);
			break;

		default:
			move_fired_arch(tmp);
	}

	return 1;
}

int cast_cone(object *op, object *caster,int dir, int strength, int spell_type, archetype *spell_arch, int magic)
{
	object *tmp;
	int i, success = 0, range_min = -1, range_max = 1;
	uint32 count_ref;

	(void) magic;

	if (!dir)
		range_min = -3, range_max = 4, strength /= 2;

	/* thats our initial spell object */
	tmp = arch_to_object(spell_arch);
	if (!tmp)
	{
		LOG(llevBug, "BUG: cast_cone(): arch_to_object() failed!? (%s)\n", spell_arch->name);
		return 0;
	}

	count_ref = tmp->count;
	for (i = range_min; i <= range_max; i++)
	{
		int x = op->x + freearr_x[absdir(dir + i)], y = op->y + freearr_y[absdir(dir + i)];

		if (wall(op->map, x, y))
			continue;

		success = 1;

		if (!tmp)
			tmp = arch_to_object(spell_arch);

		set_owner(tmp, op);
		copy_owner(tmp, op);
		/* *very* important - miss this and the spells go really wild! */
		tmp->weight_limit = count_ref;

		tmp->level = casting_level(caster, spell_type);
		tmp->x = x, tmp->y = y;

#if 0
		/* old stuff - outdated! */
		if ((tmp->attacktype & AT_HOLYWORD) || (tmp->attacktype & AT_GODPOWER))
		{
			if (!tailor_god_spell(tmp, op))
				return 0;
		}
		else if (magic)
			tmp->attacktype |= AT_MAGIC;
#endif

		if (dir)
			tmp->stats.sp = dir;
		else
			tmp->stats.sp = i;

		tmp->stats.hp = strength + (SP_level_strength_adjust(op, caster, spell_type) / 2);
		tmp->stats.dam = (sint16) SP_lvl_dam_adjust2(caster, spell_type, tmp->stats.dam);

		tmp->stats.maxhp = tmp->count;
		if (!QUERY_FLAG (tmp, FLAG_FLYING))
			LOG(llevDebug, "DEBUG: cast_cone(): arch %s doesn't have flying 1\n", spell_arch->name);

		if ((!QUERY_FLAG (tmp, FLAG_WALK_ON) || !QUERY_FLAG (tmp, FLAG_FLY_ON)) && tmp->stats.dam)
			LOG(llevDebug, "DEBUG: cast_cone(): arch %s doesn't have walk_on 1 and fly_on 1\n", spell_arch->name);

		if (!insert_ob_in_map(tmp, op->map, op, 0))
			return 0;

		if (tmp->other_arch)
			cone_drop(tmp);

		tmp = NULL;
	}

	/* can happen when we can't drop anything */
	if (tmp)
	{
		/* was not inserted */
		if (!QUERY_FLAG(tmp, FLAG_REMOVED))
			remove_ob(tmp);
	}

	return success;
}

/* this function checks to see if the cone pushes objects as well
 * as flies over and damages them */
void check_cone_push(object *op)
{
	/* object on the map */
	object *tmp, *tmp2;
	int weight_move;

	weight_move = 1000 + 1000 * op->level;

	for (tmp = get_map_ob(op->map, op->x, op->y); tmp != NULL; tmp = tmp->above)
	{
		int num_sections = 1;

		/* don't move parts of objects */
		if (tmp->head)
			continue;

		/* don't move floors or immobile objects */
		if (QUERY_FLAG(tmp, FLAG_IS_FLOOR) || (!QUERY_FLAG(tmp, FLAG_ALIVE) && QUERY_FLAG(tmp, FLAG_NO_PICK)))
			continue;

		/* count the object's sections */
		for (tmp2 = tmp; tmp2 != NULL; tmp2 = tmp2->more)
			num_sections++;

		/* move it. */
		if (rndm(0, weight_move - 1) > tmp->weight / num_sections)
		{
			/* move_object is really for monsters, but looking at
			 * the move_object function, it appears that it should
			 * also be safe for objects.
			 * This does return if successful or not, but
			 * I don't see us doing anything useful with that information
			 * right now. */
			move_object(tmp, absdir(op->stats.sp));
		}

#if 0
		int nx, ny;
		/* This block of code doesn't work for multispaced monsters (or potentially
		 * other multispaced objects).  Since we already have move_ob which will
		 * do most of this work for us, might as well use that. */

		nx = op->x + freearr_x[absdir(op->stats.sp)];
		ny = op->y + freearr_y[absdir(op->stats.sp)];

		/* don't try to move something someplace where it can't go */
		if (arch_blocked(tmp->arch, tmp, op->map, nx, ny))
			continue;

		/* OK, now we decide if we're going to move it */
		/* assume a weightless thing is a spell or whatever */
		if (tmp->weight == 0)
			continue;

		/* count the object's sections */
		for (tmp2 = tmp; tmp2 != NULL; tmp2 = tmp2->more)
			num_sections++;

		/* move it. */
		if (rndm(0, weight_move - 1) > tmp->weight / num_sections)
		{
			remove_ob(tmp);
			tmp->x = nx;
			tmp->y = ny;
			insert_ob_in_map(tmp, op->map, op, 0);
		}
#endif
	}
}

/* drops an object based on what is in the cone's "other_arch" */
void cone_drop(object *op)
{
	object *new_ob = arch_to_object(op->other_arch);
	new_ob->x = op->x;
	new_ob->y = op->y;
	new_ob->stats.food = op->stats.hp;
	new_ob->level = op->level;
	set_owner(new_ob, op->owner);

	if (op->chosen_skill)
	{
		new_ob->chosen_skill = op->chosen_skill;
		new_ob->exp_obj = op->chosen_skill->exp_obj;
	}

	insert_ob_in_map(new_ob, op->map, op, 0);
}

void move_cone(object *op)
{
	int i;
	tag_t tag;

	/* if no map then hit_map will crash so just ignore object */
	if (!op->map)
	{
		LOG(llevBug, "BUG: Tried to move_cone object %s without a map.\n", query_name(op, NULL));
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		return;
	}

	/* lava saves it's life, but not yours :) */
	if (QUERY_FLAG(op, FLAG_LIFESAVE))
	{
		hit_map(op, 0, op->attacktype);
		return;
	}

	/* If no owner left, the spell dies out. */
	if (get_owner(op) == NULL)
	{
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		return;
	}

	/* Hit map returns 1 if it hits a monster.  If it does, set
	 * food to 1, which will stop the cone from progressing. */
	tag = op->count;
	op->stats.food |= hit_map(op, 0, op->attacktype);
	/* Check to see if we should push anything.
	 * Cones with AT_PHYSICAL push whatever is in them to some
	 * degree.  */
	if (op->attacktype & AT_PHYSICAL)
		check_cone_push(op);

	if (was_destroyed(op, tag))
		return;

	if ((op->stats.hp -= 2) < 0)
	{
		if (op->stats.exp)
		{
			op->speed = 0;
			update_ob_speed(op);
			op->stats.exp = 0;
			/* so they will join */
			op->stats.sp = 0;
		}
		else
		{
			remove_ob(op);
			check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		}
		return;
	}

	if (op->stats.food)
		return;

	op->stats.food = 1;

	for (i = -1; i < 2; i++)
	{
		int x = op->x + freearr_x[absdir(op->stats.sp + i)], y = op->y + freearr_y[absdir(op->stats.sp + i)];

		if (ok_to_put_more(op->map, x, y, op, op->attacktype))
		{
			object *tmp = arch_to_object(op->arch);

			copy_owner (tmp, op);
			/* *very* important - this is the count value of the
			 * *first* object we created with this cone spell.
			 * we use it for identify this spell. Miss this
			 * and ok_to_put_more will allow to create 1000th
			 * in a single tile! */
			tmp->weight_limit = op->weight_limit;
			tmp->x = x, tmp->y = y;

#if 0
			/* holy word stuff */
			if (tmp->attacktype & AT_HOLYWORD || tmp->attacktype & AT_GODPOWER)
				if (!tailor_god_spell(tmp, op))
					return;
#endif

			tmp->level = op->level;
			tmp->stats.sp = op->stats.sp, tmp->stats.hp = op->stats.hp + 1;
			tmp->stats.maxhp = op->stats.maxhp;
			tmp->stats.dam = op->stats.dam;
			tmp->attacktype = op->attacktype;

			if (!insert_ob_in_map(tmp, op->map, op, 0))
				return;

			if (tmp->other_arch)
				cone_drop(tmp);
		}
	}
}

void fire_a_ball(object *op, int dir, int strength)
{
	object *tmp;

	if (!op->other_arch)
	{
		LOG(llevBug, "BUG: fire_a_ball(): %s no other_arch\n", op->name);
		return;
	}

	if (!dir)
	{
		LOG(llevBug, "BUG: fire_a_ball(): %s no direction\n", op->name);
		return;
	}

	tmp = arch_to_object(op->other_arch);
	set_owner(tmp, op);
	tmp->direction = dir;
	tmp->x = op->x, tmp->y = op->y;
	tmp->speed = 1;
	update_ob_speed(tmp);
	tmp->stats.hp = strength;
	tmp->level = op->level;
	SET_ANIMATION(tmp, (NUM_ANIMATIONS(tmp) / NUM_FACINGS(tmp)) * dir);
	SET_MULTI_FLAG(tmp, FLAG_FLYING);
	if ((tmp = insert_ob_in_map(tmp, op->map, op, 0)) != NULL)
		move_fired_arch(tmp);
}

void explosion(object *op)
{
	object *tmp;
	/* In case we free b */
	mapstruct *m = op->map;
	int i;

	if (--(op->stats.hp) < 0)
	{
		destruct_ob(op);
		return;
	}

	if (op->above != NULL && op->above->type != PLAYER)
	{
		SET_FLAG (op, FLAG_NO_APPLY);
		remove_ob(op);

		if (check_walk_off(op, NULL, MOVE_APPLY_VANISHED) != CHECK_WALK_OK)
			return;

		if (!insert_ob_in_map(op, op->map, op, 0))
			return;

		CLEAR_FLAG(op, FLAG_NO_APPLY);
	}

	hit_map(op, 0, op->attacktype);
	if (op->stats.hp > 2 && !op->value)
	{
		op->value = 1;
		for (i = 1; i < 9; i++)
		{
			int dx, dy;

			if (wall(op->map, dx = op->x + freearr_x[i], dy = op->y + freearr_y[i]))
				continue;

			if (blocks_view(op->map, dx, dy))
				continue;

			if (ok_to_put_more(op->map, dx, dy, op, op->attacktype))
			{
				tmp = get_object();
				/* This is probably overkill on slow computers.. */
				copy_object(op, tmp);
				tmp->state = 0;
				tmp->speed_left = -0.21f;
				tmp->stats.hp--;
				tmp->value = 0;
				tmp->x = dx, tmp->y = dy;
				insert_ob_in_map(tmp, m, op, 0);
			}
		}
	}
}

void forklightning(object *op, object *tmp)
{
	mapstruct *m;
	/* direction or -1 for left, +1 for right 0 if no new bolt */
	int xt, yt, new_dir = 1;
	/* stores temporary dir calculation */
	int t_dir;

	/* pick a fork direction.  tmp->stats.Con is the left bias
	 * i.e., the chance in 100 of forking LEFT
	 * Should start out at 50, down to 25 for one already going left
	 * down to 0 for one going 90 degrees left off original path*/

	/* fork left */
	if (rndm(0, 99) < tmp->stats.Con)
		new_dir = -1;

	/* check the new dir for a wall and in the map*/
	t_dir = absdir(tmp->direction + new_dir);

	xt = tmp->x + freearr_x[t_dir];
	yt = tmp->y + freearr_y[t_dir];

	if (!(m = out_of_map(tmp->map, &xt, &yt)))
		new_dir = 0;

	if (wall(m, xt, yt))
		new_dir = 0;

	/* OK, we made a fork */
	if (new_dir)
	{
		object *new_bolt = get_object();
		copy_object(tmp, new_bolt);

		/* reduce chances of subsequent forking */
		new_bolt->stats.Dex -= 10;
		/* less forks from main bolt too */
		tmp->stats.Dex -= 10;
		/* adjust the left bias */
		new_bolt->stats.Con += 25 * new_dir;
		new_bolt->speed_left = -0.1f;
		new_bolt->direction = t_dir;
		new_bolt->stats.hp++;
		new_bolt->x = xt;
		new_bolt->y = yt;
		/* reduce daughter bolt damage */
		new_bolt->stats.dam /= 2;
		new_bolt->stats.dam++;
		/* reduce father bolt damage */
		tmp->stats.dam /= 2;
		tmp->stats.dam++;

		if (!insert_ob_in_map(new_bolt, m, op, 0))
			return;

		update_turn_face(new_bolt);
	}
}

/* reflwall - decides weither the (spell-)object sp_op will
 * be reflected from the given mapsquare. Returns 1 if true.
 * (Note that for living creatures there is a small chance that
 * reflect_spell fails.)
 * This function don't scale up now - it uses map tile flags. MT-2004 */
int reflwall(mapstruct *m, int x, int y, object *sp_op)
{
	/* no reflection when we have a illegal space and/or non reflection flag set */
	if (!(m = out_of_map(m, &x, &y)) || !(GET_MAP_FLAGS(m, x, y) & P_REFL_SPELLS))
		return 0;

	/* we have reflection. But there is a small chance it will fail.
	 * test it. */

	/* reflect always */
	if (sp_op->type == LIGHTNING)
		return 1;

	if (!missile_reflection_adjust(sp_op, QUERY_FLAG(sp_op, FLAG_WAS_REFLECTED)))
		return 0;

	/* we get resisted - except a small fail chance */
	if ((rndm(0, 99)) < 90 - sp_op->level / 10)
	{
		SET_FLAG(sp_op, FLAG_WAS_REFLECTED);
		return 1;
	}

	return 0;
}

void move_bolt(object *op)
{
	object *tmp;
	int w, r;
	if (--(op->stats.hp) < 0)
	{
		destruct_ob(op);
		return;
	}

	hit_map(op, 0, op->attacktype);

	if (!op->value && --(op->stats.exp) > 0)
	{
		op->value = 1;
		if (!op->direction)
			return;

		if (blocks_view(op->map, op->x + DIRX(op), op->y + DIRY(op)))
			return;

		w = wall(op->map, op->x + DIRX(op), op->y + DIRY(op));
		r = reflwall(op->map, op->x + DIRX(op), op->y + DIRY(op), op);

		if (w && !QUERY_FLAG(op, FLAG_REFLECTING))
			return;

		/* We're about to bounce */
		if (w || r)
		{
			if (!QUERY_FLAG(op, FLAG_REFLECTING))
				return;

			op->value = 0;
			if (op->direction & 1)
				op->direction = absdir(op->direction + 4);
			else
			{
				int left= wall(op->map, op->x + freearr_x[absdir(op->direction - 1)], op->y + freearr_y[absdir(op->direction - 1)]), right = wall(op->map, op->x + freearr_x[absdir(op->direction + 1)], op->y + freearr_y[absdir(op->direction + 1)]);

				if (left == right)
					op->direction = absdir(op->direction + 4);
				else if (left)
					op->direction = absdir(op->direction + 2);
				else if (right)
					op->direction = absdir(op->direction - 2);
			}

			/* A bolt *must* be IS_TURNABLE */
			update_turn_face(op);
			return;
		}
		/* Create a copy of this object and put it ahead */
		else
		{
			tmp = get_object();
			copy_object(op, tmp);
			tmp->speed_left = -0.1f;
			tmp->value = 0;
			tmp->stats.hp++;
			tmp->x += DIRX(tmp), tmp->y += DIRY(tmp);

			if (!insert_ob_in_map(tmp, op->map, op, 0))
				return;

			/* New forking code.  Possibly create forks of this object
			 * going off in other directions. */

			/* stats.Dex % of forking */
			if (rndm(0, 99) < tmp->stats.Dex)
				forklightning(op, tmp);

			if (tmp)
			{
				if (!tmp->stats.food)
				{
					tmp->stats.food = 1;
					move_bolt(tmp);
				}
				else
					tmp->stats.food = 0;
			}
		}
	}
}

/* updated this to allow more than the golem 'head' to attack */
/* op is the golem to be moved. */
void move_golem(object *op)
{
	int x, y, made_attack = 0;
	object *tmp;
	tag_t tag;
	object *victim;
	mapstruct *m;

	/* Has already been moved */
	if (QUERY_FLAG(op, FLAG_MONSTER) && op->stats.hp)
		return;

	if (get_owner(op) == NULL)
	{
		LOG(llevDebug, "Golem without owner destructed.\n");
		destruct_ob(op);
		return;
	}

	/* It would be nice to have a cleaner way of what message to print
	 * when the golem expires than these hard coded entries. */
	if (--op->stats.hp < 0)
	{
		char buf[MAX_BUF];
		if (op->exp_obj && op->exp_obj->stats.Wis)
		{
			if (op->inv)
				strcpy(buf, "Your staff stops slithering around and lies still.");
			else
				sprintf(buf, "Your %s departed this plane.", op->name);
		}
		else if (!strncmp(op->name, "animated ", 9))
			sprintf(buf, "Your %s falls to the ground.", op->name);
		else
			sprintf(buf, "Your %s dissolved.", op->name);

		new_draw_info(NDI_UNIQUE, 0, op->owner, buf);
		send_golem_control(op, GOLEM_CTR_RELEASE);
		remove_friendly_object(op);
		CONTR(op->owner)->golem = NULL;
		destruct_ob(op);
		return;
	}

	/* Do golem attacks/movement for single & multisq golems.
	 * Assuming here that op is the 'head' object. Pass only op to
	 * move_ob (makes recursive calls to other parts)
	 * move_ob returns 0 if the creature was not able to move. */
	tag = op->count;

	if (move_ob(op, op->direction, op))
		return;

	if (was_destroyed(op, tag))
		return;

	/* multipart golems */
	for (tmp = op; tmp; tmp = tmp->more)
	{
		x = tmp->x + freearr_x[op->direction];
		y = tmp->y + freearr_y[op->direction];

		if (!(m = out_of_map(op->map, &x, &y)))
			continue;

		for (victim = get_map_ob(m, x, y); victim; victim = victim->above)
		{
			if (IS_LIVE(victim))
				break;
		}

		/* We used to call will_hit_self to make sure we don't
		 * hit ourselves, but that didn't work, and I don't really
		 * know if that was more efficient anyways than this.
		 * This at least works.  Note that victim->head can be NULL,
		 * but since we are not trying to dereferance that pointer,
		 * that isn't a problem. */
		if (victim && victim != op && victim->head != op)
		{
			/* for golems with race fields, we don't attack
			 * aligned races */
			if (victim->race && op->race && strstr(op->race, victim->race))
			{
				if (op->owner)
					new_draw_info_format(NDI_UNIQUE, 0, op->owner, "%s avoids damaging %s.", op->name, victim->name);
			}
			else if (op->exp_obj && victim == op->owner)
			{
				if (op->owner)
					new_draw_info_format(NDI_UNIQUE, 0, op->owner, "%s avoids damaging you.", op->name);
			}
			else
			{
				/* I think using hit_map here is just wrong -
				 * we are not attacking a space - we have a specific
				 * creature we are attacking, attack_ob seems more
				 * appropriate. */

				attack_ob(victim, op);
				/*hit_map(tmp, op->direction, op->attacktype);*/
				made_attack = 1;
			}
		}
	}

	if (made_attack)
		update_object(op, UP_OBJ_FACE);
}

void control_golem(object *op, int dir)
{
	op->direction = dir;
}

void move_missile(object *op)
{
	int i;
	object *owner;
	int new_x, new_y;
	mapstruct *mt;

	/* .... hm... no "instant missiles?" */
	owner = get_owner(op);

	if (owner == NULL)
	{
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		return;
	}

	new_x = op->x + DIRX(op);
	new_y = op->y + DIRY(op);

	if (!(mt = out_of_map(op->map, &new_x, &new_y)))
	{
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		return;
	}

	if (blocked(op, mt, new_x, new_y, op->terrain_flag))
	{
		tag_t tag = op->count;
		hit_map(op, op->direction, AT_MAGIC);
		if (!was_destroyed(op, tag))
		{
			remove_ob(op);
			check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		}
		return;
	}

	remove_ob(op);
	check_walk_off(op, NULL, MOVE_APPLY_VANISHED);

	if (!op->direction || wall (mt, new_x, new_y) || blocks_view(mt, new_x, new_y))
		return;

	op->x = new_x;
	op->y = new_y;
	op->map = mt;
	i = find_dir(mt, op->x, op->y, get_owner(op));

	if (i && i != op->direction)
	{
		op->direction = absdir(op->direction + ((op->direction - i + 8) % 8 < 4 ? -1 : 1));
		SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction);
	}

	insert_ob_in_map(op, mt, op, 0);
}

/* explode object will remove the exploding object from a container and set on map.
 * for this action, out_of_map() is called.
 * If the object is on a map, we assume that the position is controlled when object
 * is inserted or moved, so no need to recontrol. MT. */
void explode_object(object *op)
{
	tag_t op_tag = op->count;
	int xt, yt;
	object *tmp, *env;
	mapstruct *m;

	play_sound_map(op->map, op->x, op->y, SOUND_OB_EXPLODE, SOUND_NORMAL);
	if (op->other_arch == NULL)
	{
		LOG(llevBug, "BUG: explode_object(): op %s without other_arch\n", query_name(op, NULL));
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		return;
	}

	/* object is in container, try to drop on map! */
	if (op->env)
	{
		for (env = op; env->env != NULL; env = env->env);

		xt = env->x;
		yt = env->y;
		if (!(m = out_of_map(env->map, &xt, &yt)))
		{
			LOG(llevBug, "BUG: explode_object(): env out of map (%s)\n", query_name(op, NULL));
			remove_ob(op);
			check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
			return;
		}
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		op->x = xt;
		op->y = yt;

		if (!insert_ob_in_map(op, m, op, 0))
			return;
	}

	if (op->attacktype)
	{
		hit_map(op, 0, op->attacktype);
		if (was_destroyed(op, op_tag))
			return;
	}

	/*  peterm:  hack added to make fireballs and other explosions level
	*  dependent: */
	/* op->stats.sp stores the spell which made this object here. */

	tmp = arch_to_object(op->other_arch);
	switch (tmp->type)
	{
		case POISONCLOUD:
		case FBALL:
		{
			tmp->stats.dam = (sint16) SP_lvl_dam_adjust2(op, op->stats.sp, tmp->stats.dam);

			/* I have to fix this. This code is for marking the object as "magic". Using
			 * the attacktype for it, is somewhat brain dead. We have now the IS_MAGIC flag
			 * for it. MT. */

			/* tmp->stats.dam += SP_level_dam_adjust(op, op, op->stats.sp);*/
#if 0
			if (op->attacktype & AT_MAGIC)
				tmp->attacktype |= AT_MAGIC;
#endif

			copy_owner(tmp, op);
			if (op->stats.hp)
				tmp->stats.hp = op->stats.hp;

			/* Unique ID */
			tmp->stats.maxhp = op->count;
			tmp->x = op->x;
			tmp->y = op->y;

			/* needed for AT_HOLYWORD stuff -b.t. */
			if (tmp->attacktype & AT_HOLYWORD || tmp->attacktype & AT_GODPOWER)
			{
				if (!tailor_god_spell(tmp, op))
				{
					remove_ob(op);
					check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
					return;
				}
			}

			/* Prevent recursion */
			CLEAR_FLAG(op, FLAG_WALK_ON);
			CLEAR_FLAG(op, FLAG_FLY_ON);

			insert_ob_in_map(tmp, op->map, op, 0);
			break;
		}

		case CONE:
		{
			int type = tmp->stats.sp;

			if (!type)
				type = op->stats.sp;

			copy_owner(tmp, op);
			cast_cone(op, op, 0, spells[type].bdur, type, op->other_arch, op->attacktype & AT_MAGIC);
			break;
		}
	}

	/* remove the firebullet */
	if (!was_destroyed(op, op_tag))
	{
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
	}
}

/* if we are here, the arch (spell) we check was able to move
 * to this place. Wall() has failed include reflection.
 * now we look for a target. */
void check_fired_arch(object *op)
{
	tag_t op_tag = op->count, tmp_tag;
	object *tmp, *hitter;
	int dam, flag;

	/* we return here if we have NOTHING blocking here */
	if (!blocked(op, op->map, op->x, op->y, op->terrain_flag))
		return;

	if (op->other_arch)
	{
		explode_object(op);
		return;
	}

	if (op->stats.sp == SP_PROBE && op->type == BULLET)
	{
		probe(op);
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		return;
	}

	hitter = get_owner(op);
	if (!hitter)
		hitter = op;

	flag = GET_MAP_FLAGS(op->map, op->x, op->y) & P_IS_PVP;

	for (tmp = get_map_ob(op->map, op->x, op->y); tmp != NULL; tmp = tmp->above)
	{
		if (IS_LIVE(tmp) && ((tmp->type != PLAYER && hitter->type == PLAYER) || (tmp->type == PLAYER && hitter->type != PLAYER) || (hitter->type == PLAYER && (flag || tmp->map->map_flags & MAP_FLAG_PVP))))
		{
			tmp_tag = tmp->count;

			dam = hit_player (tmp, op->stats.dam, op, op->attacktype);
			if (was_destroyed(op, op_tag) || !was_destroyed (tmp, tmp_tag) || (op->stats.dam -= dam) < 0)
			{
				if (!QUERY_FLAG(op, FLAG_REMOVED))
				{
					remove_ob(op);
					check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
					return;
				}
			}
		}
	}
}


void move_fired_arch(object *op)
{
	mapstruct *m;
	tag_t op_tag = op->count;
	int new_x, new_y;

	/* peterm: added to make comet leave a trail of burnouts
	 * it's an unadulterated hack, but the effect is cool. */
	if (op->stats.sp == SP_METEOR)
	{
		replace_insert_ob_in_map("fire_trail", op);
		if (was_destroyed(op, op_tag))
			return;
	}

	new_x = op->x + DIRX(op);
	new_y = op->y + DIRY(op);
	if (!(m = out_of_map(op->map, &new_x, &new_y)))
	{
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		return;
	}

	/* the spell has reached a wall and/or the end of its moving points */
	if (!op->last_sp-- || (! op->direction || wall(m, new_x, new_y)))
	{
		if (op->other_arch)
		{
			explode_object(op);
		}
		else
		{
			remove_ob(op);
			check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		}
		return;
	}

	remove_ob(op);
	check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
	op->x = new_x;
	op->y = new_y;

	if (insert_ob_in_map(op, m, op, 0) == NULL)
		return;

	if (reflwall(op->map, op->x, op->y, op))
	{
		if (op->type == BULLET && op->stats.sp == SP_PROBE)
		{
			if (GET_MAP_FLAGS(op->map, op->x, op->y) & (P_IS_ALIVE | P_IS_PLAYER))
			{
				probe(op);
				remove_ob(op);
				check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
				return;
			}
		}

		op->direction = absdir(op->direction + 4);
		update_turn_face (op);
	}
	else
		check_fired_arch(op);
}


void drain_rod_charge(object *rod)
{
	rod->stats.hp -= spells[rod->stats.sp].sp;

	if (QUERY_FLAG(rod, FLAG_ANIMATE))
		fix_rod_speed(rod);
}

void fix_rod_speed(object *rod)
{
	rod->speed = (FABS(rod->arch->clone.speed)*rod->stats.hp) / (float)rod->stats.maxhp;

	if (rod->speed < 0.02f)
		rod->speed = 0.02f;

	update_ob_speed(rod);
}

/* i changed this function to take care about target system.
 * This function examines target & (real) hitter and the
 * given spell flags. We have to decide some flags including
 * PvP flags - note that normally player are friendly to other
 * players except for pvp - there they are enemys.
 * op is owner/object which casted the spell, item is the object
 * which was used (scroll, potion, pet). */
int find_target_for_spell(object *op,object *item, object **target, int dir, uint32 flags)
{
	object *tmp;

	(void) item;
	(void) dir;

	/*LOG(llevInfo, "FIND_TARGET_for_spell: op: %s used: %s dir %d flags: %x\n", op->name, item ? item->name : "<none>", dir, flags);*/

	/* default target is - nothing! */
	*target = NULL;

	/* we cast something on the map... no target */
	if (flags & SPELL_DESC_DIRECTION)
		return TRUE;

	/* a player has invoked this spell */
	if (op->type == PLAYER)
	{
		/* try to cast on self but only when really no friendly or enemy is set */
		if ((flags & SPELL_DESC_SELF) && !(flags & (SPELL_DESC_ENEMY | SPELL_DESC_FRIENDLY)))
		{
			/* self ... and no other tests */
			*target = op;
			return TRUE;
		}

		tmp = CONTR(op)->target_object;

		/* lets check our target - we have one? friend or enemy? */
		if (!tmp || !OBJECT_ACTIVE(tmp) || tmp == CONTR(op)->ob || CONTR(op)->target_object_count != tmp->count)
		{
			/* no valid target, or we target self! */

			/* can we cast this on self? */
			if (flags & SPELL_DESC_SELF)
			{
				/* right, we are target */
				*target = op;
				return TRUE;
			}
		}
		/* we have a target and its not self */
		else
		{
			/* player? */
			if (tmp->type == PLAYER)
			{
				/* now it will be tricky...
				 * a.) friendly spell - always allowed to cast
				 * b.) enemy spell - only allowed when op AND target
				 * are in a pvp area.
				 * c.) if not a. and b. AND self - cast on self */
				if ((flags & SPELL_DESC_FRIENDLY) && !pvp_area(op, tmp))
				{
					*target = tmp;
					return TRUE;
				}

				if (flags & SPELL_DESC_ENEMY)
				{
					/* ok... now op AND tmp must be in PvP - if one not,
					 * this is not allowed. */
					if (op->map && tmp->map && pvp_area(op->type == PLAYER ? op : get_owner(op), tmp->type == PLAYER ? tmp : get_owner(tmp)))
					{
						/* here we go... PvP! */
						*target = tmp;
						return TRUE;
					}
				}

				if (flags & SPELL_DESC_SELF)
				{
					*target = op;
					return TRUE;
				}
			}
			/* friendly NPC? */
			else if (QUERY_FLAG(tmp, FLAG_FRIENDLY))
			{
				if (flags & SPELL_DESC_FRIENDLY)
				{
					*target = tmp;
					return TRUE;
				}

				if (flags & SPELL_DESC_SELF)
				{
					*target = op;
					return TRUE;
				}
			}
			/* ok, is a bad guy */
			else
			{
				if (flags & SPELL_DESC_ENEMY)
				{
					*target = tmp;
					return TRUE;
				}

				if (flags & SPELL_DESC_SELF)
				{
					*target = op;
					return TRUE;
				}
			}
		}
	}
	/* thats a mob OR rune/firewall/.. OR a pet/summon controlled from player */
	else
	{
		/* we use op->enemy as target from non player caster.
		 * we need to set this from outside and for healing spells,
		 * we must set from outside temporary the enemy to a friendly unit.
		 * This is safe because we do no AI stuff here - we simply USE the
		 * target here even the stuff above looks like we select one...
		 * its only a fallback. */

		/* sanity check for a legal target */
		if (op->enemy && OBJECT_ACTIVE(op->enemy) && op->enemy->count == op->enemy_count)
		{
			*target = op;
			return TRUE;
		}
	}

	/* invalid target/spell or whatever */
	return FALSE;

#if 0
	if (op->type != PLAYER && op->type != RUNE)
	{
		tmp = get_owner(op);
		if (!tmp || !QUERY_FLAG(tmp, FLAG_MONSTER))
			tmp = op;
	}
	else
	{
		xt = op->x + freearr_x[dir];
		yt = op->y + freearr_y[dir];

		if (!(m = out_of_map(op->map, &xt, &yt)))
			tmp = NULL;
		else
		{
			for (tmp = get_map_ob(m, xt, yt); tmp != NULL; tmp = tmp->above)
			{
				if (tmp->type == PLAYER)
					break;
			}
		}
	}

	if (tmp == NULL)
	{
		for (tmp = get_map_ob(op->map, op->x, op->y); tmp != NULL; tmp = tmp->above)
		{
			if (tmp->type == PLAYER)
				break;
		}
	}

	return tmp;
#endif
}


/* peterm: ball lightning mover.  */
/* ball lightning automatically seeks out a victim, if
 * it sees any monsters close enough.  */
void move_ball_lightning(object *op)
{
	int i, nx, ny, j, dam_save, dir;
	mapstruct *m;
	object *owner;

	owner = get_owner(op);

	/* Only those attuned to PATH_ELEC may use ball lightning with AT_GODPOWER */
	if (owner && (!(owner->path_attuned & PATH_ELEC)) && (op->attacktype & AT_GODPOWER))
	{
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		new_draw_info_format(NDI_UNIQUE, 0, owner, "The ball lightning dispells immediately. Perhaps you need attunement to the spell path?");
		return;
	}

	/* the following logic makes sure that the ball
	 * doesn't move into a wall, and makes
	 * sure that it will move along a wall to try and
	 * get at it's victim.  */
	dir = 0;

	if (!(rndm(0, 3)))
		j = rndm(0, 1);
	else
		j = 0;

	for (i = 1; i < 9; i++)
	{
		/* i bit 0: alters sign of offset
		 * otther bits (i / 2): absolute value of offset */
		int offset = ((i ^ j) & 1) ? (i / 2) : - (i / 2);
		int tmpdir = absdir (op->direction + offset);
		nx = op->x + freearr_x[tmpdir];
		ny = op->y + freearr_y[tmpdir];
		if (!wall(op->map, nx, ny))
		{
			dir = tmpdir;
			break;
		}
	}

	if (dir == 0)
	{
		nx = op->x;
		ny = op->y;
	}

	remove_ob(op);
	check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
	op->y = ny;
	op->x = nx;

	if (!insert_ob_in_map(op, op->map, op, 0))
		return;

	ny = op->y;
	nx = op->x;

	/* save the original dam: we do halfdam on
	 * surrounding squares */
	dam_save = op->stats.dam;

	/* loop over current square and neighbors to hit. */
	for (j = 0; j < 9; j++)
	{
		/* hit these squares */
		int hx, hy;
		object *new_ob;

		hx = nx + freearr_x[j];
		hy = ny + freearr_y[j];
		/* out of map - always skip */
		if (!(m = out_of_map(op->map, &hx, &hy)))
			continue;

		/* if there is a wall - skip too */
		if (wall(m, hx, hy))
			continue;

		if (j)
			op->stats.dam = dam_save / 2;

		/* Hm, i not sure this is always correct, but we will see
		 * perhaps we must add more checks to avoid bad hits on the map */
		hit_map(op, j, op->attacktype);

		/* insert the other arch */
		if (op->other_arch)
		{
			new_ob = arch_to_object(op->other_arch);
			new_ob->x = hx;
			new_ob->y = hy;
			insert_ob_in_map(new_ob, m, op, 0);
		}
	}

	/* restore to the center location and damage*/
	op->stats.dam = dam_save;
	i = spell_find_dir(op->map, op->x, op->y, get_owner(op));

	/* we have a preferred direction!  */
	if (i >= 0)
	{
		/* pick another direction if the preferred dir is blocked. */
		if (wall(op->map, nx + freearr_x[i], ny + freearr_y[i]))
		{
			/* -1, 0, +1 */
			i += rndm(0, 2) - 1;

			if (i == 0)
				i = 8;

			if (i == 9)
				i = 1;
		}

		op->direction = i;
	}
}



/* peterm:
 * do LOS stuff for ball lightning.  Go after the closest VISIBLE monster.
 * Basically, we step back until dir is 0 and fail if we're blocked on the
 * way. */

int reduction_dir[SIZEOFFREE][3] =
{
	/* 0 */
	{0, 0, 0},
	/* 1 */
	{0, 0, 0},
	/* 2*/
	{0, 0, 0},
	/* 3*/
	{0, 0, 0},
	/* 4 */
	{0, 0, 0},
	/* 5 */
	{0, 0, 0},
	/* 6 */
	{0, 0, 0},
	/* 7 */
	{0, 0, 0},
	/* 8 */
	{0, 0, 0},
	/* 9 */
	{8, 1, 2},
	/* 10 */
	{1, 2, -1},
	/* 11 */
	{2, 10, 12},
	/* 12 */
	{2, 3, -1},
	/* 13 */
	{2, 3, 4},
	/* 14 */
	{3, 4, -1},
	/* 15 */
	{4, 14, 16},
	/* 16 */
	{5, 4, -1},
	/* 17 */
	{4, 5, 6},
	/* 18 */
	{6, 5, -1},
	/* 19 */
	{6, 20, 18},
	/* 20 */
	{7, 6, -1},
	/* 21 */
	{6, 7, 8},
	/* 22 */
	{7, 8, -1},
	/* 23 */
	{8, 22, 24},
	/* 24 */
	{8, 1, -1},
	/* 25 */
	{24, 9, 10},
	/* 26 */
	{9, 10, -1},
	/* 27 */
	{10, 11, -1},
	/* 28 */
	{27, 11, 29},
	/* 29 */
	{11, 12, -1},
	/* 30 */
	{12, 13, -1},
	/* 31 */
	{12, 13, 14},
	/* 32 */
	{13, 14, -1},
	/* 33 */
	{14, 15, -1},
	/* 34 */
	{33, 15, 35},
	/* 35 */
	{16, 15, -1},
	/* 36 */
	{17, 16, -1},
	/* 37 */
	{18, 17, 16},
	/* 38 */
	{18, 17, -1},
	/* 39 */
	{18, 19, -1},
	/* 40 */
	{41, 19, 39},
	/* 41 */
	{19, 20, -1},
	/* 42 */
	{20, 21, -1},
	/* 43 */
	{20, 21, 22},
	/* 44 */
	{21, 22, -1},
	/* 45 */
	{23, 22, -1},
	/* 46 */
	{45, 47, 23},
	/* 47 */
	{23, 24, -1},
	/* 48 */
	{24, 9, -1}
};

/* Recursive routine to step back and see if we can
 * find a path to that monster that we found.  If not,
 * we don't bother going toward it.  Returns 1 if we
 * can see a direct way to get it
 * Modified to be map tile aware -.MSW */
/* hm, how fast is this function? MT-2004 */
int can_see_monsterP(mapstruct *m, int x, int y, int dir)
{
	int dx, dy;

	/* exit condition: invalid direction */
	if (dir < 0)
		return 0;

	dx = x + freearr_x[dir];
	dy = y + freearr_y[dir];

	if (!(m = out_of_map(m, &dx, &dy)))
		return 0;

	if (wall(m, dx, dy))
		return 0;

	/* yes, can see. */
	if (dir < 9)
		return 1;

	return can_see_monsterP(m, x, y, reduction_dir[dir][0]) | can_see_monsterP(m, x, y, reduction_dir[dir][1]) | can_see_monsterP(m, x, y, reduction_dir[dir][2]);
}

/* raytrace:
 * spell_find_dir(map, x, y, exclude) will search first the center square
 * then some close squares in the given map at the given coordinates for
 * live objects.
 * It will not consider the object given as exlude (= caster) among possible
 * live objects. If the caster is a player, the spell will go after
 * monsters/generators only. If not, the spell will hunt players only.
 * It returns the direction toward the first/closest live object if it finds
 * any, otherwise -1. */
int spell_find_dir(mapstruct *m, int x, int y, object *exclude)
{
	int i, max = SIZEOFFREE;
	int nx, ny;
	int owner_type = 0;
	object *tmp;

	if (exclude && exclude->head)
		exclude = exclude->head;

	if (exclude && exclude->type)
		owner_type = exclude->type;

	for (i = rndm(1, 8); i < max; i++)
	{
		nx = x + freearr_x[i];
		ny = y + freearr_y[i];

		if ((m = out_of_map(m, &nx, &ny)))
		{
			tmp = get_map_ob(m, nx, ny);

			while (tmp != NULL && (((owner_type == PLAYER && !QUERY_FLAG(tmp, FLAG_MONSTER) && !QUERY_FLAG(tmp, FLAG_GENERATOR)) || (owner_type != PLAYER && tmp->type != PLAYER)) || (tmp == exclude || (tmp->head && tmp->head == exclude))))
				tmp = tmp->above;

			if (tmp != NULL && can_see_monsterP(m, nx, ny, i) && !blocks_view(m, nx, ny))
				return freedir[i];
		}
	}

	/* flag for "keep going the way you were" */
	return -1;
}

/* peterm: */

/* peterm:  the following defines the parameters for all the
 * spells.
 * bdam:  base damage or hp of spell or summoned monster
 * bdur:  base duration of spell or base range
 * ldam:  levels you need over the min for the spell to gain one dam
 * ldur:  levels you need over the min for the spell to gain one dur */

/*  The following adjustments to spell strength are done in the
 * philosophy that the longer one knows a spell, the better one
 * should get at it.  So the more experience levels you are above
 * the minimum for knowing a spell, the more effective it becomes.
 * most of the following adjustments are for damage only, some are
 * for turning undead and whatnot.
 *
 * The following functions assume that casting the spell is not
 * denied.  Denied spells have an undefined path level modifier.
 * There wouldn't be a meaningful result anyway.
 *
 * The arrays are defined in spells.h */

/* July 1995 - I changed the next 3 functions slightly by replacing
 * the casters level (op->level) with the skill level (SK_level(op))
 * instead for when we have compiled with ALLOW_SKILLS - b.t. */

/* now based on caster's level instead of on op's level and caster's
 * path modifiers. 	--DAMN */
int SP_level_dam_adjust(object *op, object *caster, int spell_type)
{
	int level = casting_level(caster, spell_type);
	int adj = (level - spells[spell_type].level);

	(void) op;

	if (adj < 0)
		adj = 0;

	if (spells[spell_type].ldam)
		adj /= spells[spell_type].ldam;
	else
		adj = 0;

	return adj;
}

/* July 1995 - changed slightly (SK_level) for ALLOW_SKILLS - b.t. */

/* now based on caster's level instead of on op's level and caster's
 * path modifiers. 	--DAMN */
int SP_level_strength_adjust(object *op, object *caster, int spell_type)
{
	int level = casting_level(caster, spell_type);
	int adj = (level - spells[spell_type].level);

	(void) op;

	if (adj < 0)
		adj = 0;

	if (spells[spell_type].ldur)
		adj /= spells[spell_type].ldur;
	else
		adj = 0;

	return adj;
}

/* The following function scales the spellpoint cost of
 * a spell by it's increased effectiveness.  Some of the
 * lower level spells become incredibly vicious at high
 * levels.  Very cheap mass destruction.  This function is
 * intended to keep the sp cost related to the effectiveness. */

/* July 1995 - changed slightly (SK_level) for ALLOW_SKILLS - b.t. */
int SP_level_spellpoint_cost(object *op, object *caster, int spell_type)
{
	spell *s = find_spell(spell_type);
	int level = casting_level(caster, spell_type);
#ifdef SPELLPOINT_LEVEL_DEPEND
	int sp;
#endif

	(void) op;

#ifdef SPELLPOINT_LEVEL_DEPEND
	if (spells[spell_type].spl)
		sp = (int) (spells[spell_type].sp * (1.0 + (MAX(0, (float)(level - spells[spell_type].level) / (float)spells[spell_type].spl))));
	else
		sp = spells[spell_type].sp;

	sp = (int)((float)sp * (float)PATH_SP_MULT(caster, s));

	return MIN(sp, (spells[spell_type].sp + 50));
#else
	return s->sp * PATH_SP_MULT(caster, s);
#endif
}

static int SP_level_gracepoint_cost(object *op, object *caster, int spell_type)
{
	spell *s=find_spell(spell_type);
	int level = casting_level(caster, spell_type);
#ifdef SPELLPOINT_LEVEL_DEPEND
	int grace;
#endif

	(void) op;

#ifdef SPELLPOINT_LEVEL_DEPEND
	if (spells[spell_type].spl)
		grace = (int) (spells[spell_type].sp * (1.0 + (MAX(0, (float)(level-spells[spell_type].level) / (float)spells[spell_type].spl))));
	else
		grace = spells[spell_type].sp;

	grace = (int)((float)grace * (float)PATH_SP_MULT(caster, s));

	return MIN(grace, (spells[spell_type].sp + 50));
#else
	return s->grace * PATH_SP_MULT(caster, s);
#endif
}

/* move_swarm_spell:  peterm  */
/* This is an implementation of the swarm spell.  It was written for
 * meteor swarm, but it could be used for any swarm.  A swarm spell
 * is a special type of object that casts swarms of other types
 * of spells.  Which spell it casts is flexible.  It fires the spells
 * from a set of squares surrounding the caster, in a given direction. */
void move_swarm_spell(object *op)
{
	int xt, yt;
	int basedir, adjustdir;
	sint16 target_x, target_y, origin_x, origin_y;

	if (op->stats.hp == 0 || get_owner(op) == NULL)
	{
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		return;
	}

	op->stats.hp--;

	basedir = op->direction;
	if (basedir == 0)
	{
		/* spray in all directions! 8) */
		basedir = rndm(1, 8);
	}

	/* new offset calculation to make swarm element distribution
	 * more uniform */
	if (op->stats.hp)
	{
		if (basedir & 1)
			adjustdir = cardinal_adjust[rndm(0, 8)];
		else
			adjustdir = diagonal_adjust[rndm(0, 9)];
	}
	/* fire the last one from forward. */
	else
		adjustdir = 0;

	target_x = op->x + freearr_x[absdir(basedir + adjustdir)];
	target_y = op->y + freearr_y[absdir(basedir + adjustdir)];

	/* back up one space so we can hit point-blank targets, but this
	 * necessitates extra out_of_map check below */
	origin_x = target_x - freearr_x[basedir];
	origin_y = target_y - freearr_y[basedir];

	/* for level dependence, we need to know what spell is fired.  */
	/* that's stored in op->stats.sp  by fire_swarm  */
	/* this can be optimized... fire_arch_ there call out_of_map() too in insert_object() */
	xt = (int)origin_x;
	yt = (int)origin_y;

	if (!out_of_map(op->map, &xt, &yt))
		return;

	if (!wall(op->map, target_x, target_y))
		fire_arch_from_position(op, op, origin_x, origin_y, basedir, op->other_arch, op->stats.sp, op->magic);
}

/* fire_swarm:  peterm */
/* The following routine creates a swarm of objects.  It actually
 * sets up a specific swarm object, which then fires off all
 * the parts of the swarm.
 *
 * Interface:
 *  op:  the owner
 *  caster: the caster (owner, wand, rod, scroll)
 *  dir: the direction everything will be fired in
 *  swarm_type:  the archetype that will be fired
 *  spell_type:  the spell type of the archetype that's fired
 *  n:  the number to be fired. */
void fire_swarm(object *op, object *caster, int dir, archetype *swarm_type, int spell_type, int n, int magic)
{
	object *tmp;
	tmp = get_archetype("swarm_spell");
	tmp->x = op->x;
	tmp->y = op->y;
	/* needed so that if swarm elements kill, caster gets xp.*/
	set_owner(tmp, op);
	/* needed later, to get level dep. right.*/
	tmp->level = casting_level(caster, spell_type);
	/* needed later, see move_swarm_spell */
	tmp->stats.sp = spell_type;
	tmp->attacktype = swarm_type->clone.attacktype;

	if (tmp->attacktype & AT_HOLYWORD || tmp->attacktype & AT_GODPOWER)
	{
		if (!tailor_god_spell(tmp, op))
			return;
	}

	tmp->magic = magic;
	/* n in swarm*/
	tmp->stats.hp = n;
	/* the archetype of the things to be fired */
	tmp->other_arch = swarm_type;
	tmp->direction = dir;
	/*SET_FLAG(tmp, FLAG_IS_INVISIBLE);*/
	insert_ob_in_map(tmp, op->map, op, 0);
}

/* create an aura spell object and put it in the player's inventory. */
int create_aura(object *op, object *caster, archetype *aura_arch, int spell_type, int magic)
{
	object *new_aura = arch_to_object(aura_arch);
	new_aura->stats.food = spells[spell_type].bdur + 10 * SP_level_strength_adjust(op, caster, spell_type);
	new_aura->stats.dam = spells[spell_type].bdam + SP_level_dam_adjust(op, caster, spell_type);
	set_owner(new_aura, op);

	if (magic)
		new_aura->attacktype |= AT_MAGIC;

	if (new_aura->owner)
	{
		new_aura->chosen_skill = op->chosen_skill;

		if (new_aura->chosen_skill)
			new_aura->exp_obj = op->chosen_skill->exp_obj;
	}

	new_aura->level = SK_level(caster);
	insert_ob_in_ob(new_aura, op);
	return 1;
}

/* look_up_spell_by_name: peterm
 * this function attempts to find the spell spname in spells[].
 * if it doesn't exist, or if the op cannot cast that spname,
 * -1 is returned.  */
int look_up_spell_by_name(object *op, const char *spname)
{
	int numknown;
	int spnum;
	int plen;
	int spellen;
	int i;

	if (spname == NULL)
		return -1;

	if (op == NULL)
		numknown = NROFREALSPELLS;
	else if (QUERY_FLAG(op, FLAG_WIZ))
		numknown = NROFREALSPELLS;
	else
		numknown = CONTR(op)->nrofknownspells;

	plen = strlen(spname);
	for (i = 0; i < numknown; i++)
	{
		if (op == NULL)
			spnum = i;
		else if (QUERY_FLAG(op, FLAG_WIZ))
			spnum = i;
		else
			spnum = CONTR(op)->known_spells[i];

		spellen = strlen(spells[spnum].name);

		if (strncmp(spname, spells[spnum].name, MIN(spellen, plen)) == 0)
			return spnum;
	}

	return -1;
}

void put_a_monster(object *op, const char *monstername)
{
	object *tmp, *head = NULL, *prev = NULL;
	archetype *at;
	int dir;

	/* find a free square nearby */
	/* first we check the closest square for free squares */
	if ((at = find_archetype(monstername)) == NULL)
		return;

	dir = find_first_free_spot(at, op->map, op->x, op->y);

	if (dir != -1)
	{
		/* This is basically grabbed for generate monster.  Fixed 971225 to
		 * insert multipart monsters properly */
		while (at != NULL)
		{
			tmp = arch_to_object(at);
			tmp->x = op->x + freearr_x[dir] + at->clone.x;
			tmp->y = op->y + freearr_y[dir] + at->clone.y;
			if (head)
			{
				tmp->head = head;
				prev->more = tmp;
			}

			if (!head)
				head = tmp;
			prev = tmp;
			at = at->more;
		}

		if (head->randomitems)
			create_treasure(head->randomitems, head, GT_INVISIBLE, op->level ? op->level : op->map->difficulty, T_STYLE_UNSET, ART_CHANCE_UNSET, 0, NULL);

		insert_ob_in_map(head, op->map, op, 0);

		/* thought it'd be cool to insert a burnout, too.*/
		tmp = get_archetype("burnout");
		tmp->map = op->map;
		tmp->x = op->x + freearr_x[dir];
		tmp->y = op->y + freearr_y[dir];
		insert_ob_in_map(tmp, op->map, op, 0);
	}
}


/*  Some local definitions for shuffle-attack */
struct
{
	int attacktype;
	int face;
}

ATTACKS[22] =
{
	{AT_PHYSICAL, 0},
	/* face = explosion */
	{AT_PHYSICAL, 0},
	{AT_PHYSICAL, 0},
	{AT_MAGIC, 1},
	/* face = last-burnout */
	{AT_MAGIC, 1},
	{AT_MAGIC, 1},
	{AT_FIRE, 2},
	/* face = fire....  */
	{AT_FIRE, 2},
	{AT_FIRE, 2},
	{AT_ELECTRICITY, 3},
	/* ball_lightning */
	{AT_ELECTRICITY, 3},
	{AT_ELECTRICITY, 3},
	{AT_COLD, 4},
	/* face = icestorm*/
	{AT_COLD, 4},
	{AT_COLD, 4},
	{AT_CONFUSION, 5},
	{AT_POISON, 7},
	/* face = acid sphere.  generator */
	{AT_POISON, 7},
	/* poisoncloud face */
	{AT_POISON, 7},
	{AT_SLOW, 8},
	{AT_PARALYZE, 9},
	{AT_FEAR, 10}
};

/* shuffle_attack:  peterm */
/* This routine shuffles the attack of op to one of the
 * ones in the list.  It does this at random.  It also
 * chooses a face appropriate to the attack that is
 * being committed by that square at the moment.
 * right now it's being used by color spray and create pool of
 * chaos.  */

/* hmhmhm... this will not fit in our ext anim system... But i let it unchanged until
 * i rework the spells using it. MT-2003. */
void shuffle_attack(object *op, int change_face)
{
	int i;
	i = rndm(0, 21);
	op->attacktype |= ATTACKS[i].attacktype | AT_MAGIC;

	if (change_face)
		SET_ANIMATION(op, ATTACKS[i].face);
}


/* the following function reads from the file 'spell_params' in
 * the lib dir, and resets the array in memory to reflect the values
 * in spell_parameters.  The format in there MUST be:
 * (reworked for Daimonin, the crossfire part here was outdated)
 *  spell name (SP_P = SP_PARAMTERS)
 *  spells.level spells.sp  SP_P.bdam SP_P.bdur SP_P.ldam SP_P.ldur SP_P.spl */

/* get_pointed_target() - this is used by finger of death
 * and the 'smite' spells. Returns the pointer to the first
 * monster in the direction which is pointed to by op. b.t. */
/* TODO - remove this check stuff from the for next */
object *get_pointed_target(object *op, int dir)
{
	(void) op;
	(void) dir;

#if 0
	object *target;
	int x,y;

	if (dir == 0)
		return NULL;

	for (x = op->x + freearr_x[dir], y = op->y + freearr_y[dir]; !out_of_map(op->map, x, y) && !blocks_view(op->map, x, y) && !wall(op->map, x, y); x += freearr_x[dir], y += freearr_y[dir])
	{
		for (target = get_map_ob(op->map, x, y); target; target = target->above)
		{
			if (QUERY_FLAG(target->head ? target->head : target, FLAG_MONSTER))
			{
				if (!blocks_magic(op->map, x, y))
					return target;
				else
					break;
			}
		}
	}
#endif
	return ((object *) NULL);
}

/* cast_smite_arch() - the priest points to a creature and causes
 * a 'godly curse' to decend. I generalized this a bit so that several
 * spells will be possible to use w/ this code (eg fire_arch, cast_cone).
 * -b.t. */
int cast_smite_spell(object *op, object *caster, int dir, int type)
{
	object *effect, *target = get_pointed_target(op, dir);
	object *god = find_god(determine_god(op));

	/* if we don't worship a god, or target a creature
	 * of our god, the spell will fail. */
	if (!target || QUERY_FLAG(target, FLAG_CAN_REFL_SPELL) || !god || (target->title && !strcmp(target->title, god->name)) || (target->race && strstr(target->race, god->race)))
	{
		new_draw_info(NDI_UNIQUE, 0,op, "Your request is unheeded.");
		return 0;
	}

	if (spellarch[type] != (archetype *) NULL)
		effect = arch_to_object(spellarch[type]);
	else
		return 0;

	/* tailor the effect by priest level and worshipped God */
	effect->level = casting_level(caster, type);
	if (effect->attacktype & AT_HOLYWORD || effect->attacktype & AT_GODPOWER)
	{
		if (tailor_god_spell(effect, op))
			new_draw_info_format(NDI_UNIQUE, 0, op, "%s answers your call!", determine_god(op));
		else
		{
			new_draw_info(NDI_UNIQUE, 0, op, "Your request is ignored.");
			return 0;
		}
	}

	/* size of the area of destruction */
	effect->stats.hp = spells[type].bdur + SP_level_strength_adjust(op, caster, type);
	/* how much woe to inflict :) */
	effect->stats.dam = spells[type].bdam + SP_level_dam_adjust(op, caster, type);

	if (effect->stats.dam < 0)
		effect->stats.dam = 127;

	effect->stats.maxhp = effect->count;
	set_owner(effect, op);

	/* ok, tell it where to be, and insert! */
	effect->x = target->x;
	effect->y = target->y;
	insert_ob_in_map(effect, op->map, op, 0);

	return 1;
}

/* we use our damage/level tables to adjust the base_dam. Normally, the damage increase
 * with the level of the caster - or if the caster is a living object, with the level
 * of the used skill.
 * Is the base dam = -1, we use the default spell table setting with spell_type to get
 * a valid base damage. */
int SP_lvl_dam_adjust2(object *caster, int spell_type, int base_dam)
{
	float tmp_add;
	int dam_adj, level = SK_level(caster);

	/* sanity check */
	if (level <=0 || level > 110)
	{
		LOG(llevBug, "SP_lvl_dam_adjust2(): object %s has invalid level %d\n", query_name(caster, NULL), level);
		if (level <= 0)
			level = 1;
		else
			level = 110;
	}

	/* get a base damage when we don't have one from caller */
	if (base_dam == -1)
		base_dam = spells[spell_type].bdam;

	if ((tmp_add = lev_damage[level / 3] - 0.75f) < 0)
		tmp_add = 0;

	dam_adj = (sint16) ((float)base_dam * (lev_damage[level] + tmp_add));

	return dam_adj;
}
