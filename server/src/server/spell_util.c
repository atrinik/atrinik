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
 * Spell related helper functions. */

#include <global.h>
#ifndef __CEXTRACT__
#include <sproto.h>
#endif

/**
 * Array of pointers to archetypes used by the spells for quick
 * access. */
archetype *spellarch[NROFREALSPELLS];

static int SP_level_gracepoint_cost(object *caster, int spell_type);

/**
 * Initialize spells. */
void init_spells()
{
	static int init_spells_done = 0;
	int i;

	if (init_spells_done)
	{
		return;
	}

	LOG(llevDebug, "Initializing spells...");
	init_spells_done = 1;

	for (i = 0; i < NROFREALSPELLS; i++)
	{
		if (spells[i].archname)
		{
			if ((spellarch[i] = find_archetype(spells[i].archname)) == NULL)
			{
				LOG(llevError, "Error: Spell %s needs arch %s, your archetypes file is out of date.\n", spells[i].name,spells[i].archname);
			}
		}
		else
		{
			spellarch[i] = (archetype *) NULL;
		}
	}

	LOG(llevDebug, "done.\n");
}

/**
 * Dumps all the spells. */
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
			{
				name2 = spellarch[i]->clone.other_arch->name;
			}
		}

		LOG(llevInfo, "%s: %s: %s\n", spells[i].name, (name1 ? name1 : "null"), (name2 ? name2 : "null"));
	}
}

/**
 * Inserts a spell effect on map.
 * @param archname Spell effect arch.
 * @param m Map.
 * @param x X position on map.
 * @param y Y position on map.
 * @return 1 on failure, 0 otherwise.
 * @todo Does not support multi arch effects yet. */
int insert_spell_effect(char *archname, mapstruct *m, int x, int y)
{
	archetype *effect_arch;
	object *effect_ob;

	if (!archname || !m)
	{
		LOG(llevBug, "BUG: insert_spell_effect(): archname or map NULL.\n");
		return 1;
	}

	if (!(effect_arch = find_archetype(archname)))
	{
		LOG(llevBug, "BUG: insert_spell_effect(): Couldn't find effect arch (%s).\n", archname);
		return 1;
	}

	/* Prepare effect */
	effect_ob = arch_to_object(effect_arch);
	effect_ob->map = m;
	effect_ob->x = x;
	effect_ob->y = y;

	if (!insert_ob_in_map(effect_ob, m, NULL, 0))
	{
		LOG(llevBug, "BUG: insert_spell_effect(): effect arch (%s) out of map (%s) (%d,%d) or failed insertion.\n", archname, effect_ob->map->name, x, y);

		/* Something is wrong - kill object */
		if (!QUERY_FLAG(effect_ob, FLAG_REMOVED))
		{
			remove_ob(effect_ob);
			check_walk_off(effect_ob, NULL, MOVE_APPLY_VANISHED);
		}

		return 1;
	}

	return 0;
}

/**
 * Find a spell in the ::spells array.
 * @param spelltype ID of the spell to find.
 * @return The spell from the ::spells array, NULL if not found. */
spell *find_spell(int spelltype)
{
	if (spelltype < 0 || spelltype > NROFREALSPELLS)
	{
		return NULL;
	}

	return &spells[spelltype];
}

/**
 * Get the casting level of a spell based on the caster's
 * repelled/attuned paths.
 * @param caster Caster.
 * @param base_level Level before modification.
 * @param spell_type Spell ID.
 * @return The casting level, always at least 1. */
int casting_level(object *caster, int base_level, int spell_type)
{
	spell *s = find_spell(spell_type);
	int new_level;

	if (!s || caster->path_denied & s->path)
	{
		return 1;
	}

	new_level = base_level + ((caster->path_repelled & s->path) ? -5 : 0) + ((caster->path_attuned & s->path) ? 5 : 0);

	return (new_level < 1) ? 1 : new_level;
}

/**
 * Checks to see if player knows the spell.
 * @param op Object we're checking.
 * @param name Spell ID.
 * @return 1 if op knows the spell, 0 otherwise. */
int check_spell_known(object *op, int spell_type)
{
	int i;

	for (i = 0; i < CONTR(op)->nrofknownspells; i++)
	{
		if (CONTR(op)->known_spells[i] == spell_type)
		{
			return 1;
		}
	}

	return 0;
}

/**
 * Cast a spell.
 * @param op The creature that is owner of the object that is casting the
 * spell.
 * @param caster The actual object (wand, potion) casting the spell. Can
 * be same as op.
 * @param dir Direction to cast in.
 * @param type Spell ID.
 * @param ability If true, the spell is the innate ability of a monster
 * (ie, don't check for blocks_magic(), and don't add AT_MAGIC to attacktype).
 * @param item The type of object that is casting the spell.
 * @param stringarg Any options that are being used.
 * @return 0 on failure, non-zero on success and is used by caller to
 * drain mana/grace. */
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

	/* Get the base duration */
	duration = spells[type].bdur;

	if (!op)
	{
		op = caster;
	}

	/* Script NPCs can ALWAYS cast - even in no spell areas! */
	if (item == spellNPC)
	{
		/* If spellNPC, this usually comes from a script */
		target = op;
		/* And caster is the NPC and op the target */
		op = caster;
		/* Change the pointers to fit this function and jump */
		goto dirty_jump;
	}

	/* It looks like the only properties we ever care about from the casting
	 * object (caster) is spell paths and level. */
	cast_op = op;

	if (!caster)
	{
		if (item == spellNormal)
		{
			caster = op;
		}
	}
	else
	{
		/* Caster has a map? Then we use caster */
		if (caster->map)
		{
			cast_op = caster;
		}
	}

	/* Now check we can cast this spell! */

	/* No magic and not a prayer. */
	if (MAP_NOMAGIC(cast_op->map) && !(spells[type].flags & SPELL_DESC_WIS))
	{
		if (op->type == PLAYER)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "Powerful countermagic cancels all spellcasting here!");
		}

		return 0;
	}

	/* No prayer and a prayer. */
	if (MAP_NOPRIEST(cast_op->map) && (spells[type].flags & SPELL_DESC_WIS))
	{
		if (op->type == PLAYER)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "Powerful countermagic cancels all prayer spells here!");
		}

		return 0;
	}

	/* No harm spell and not town safe. */
	if (MAP_NOHARM(cast_op->map) && !(spells[type].flags & SPELL_DESC_TOWN))
	{
		if (op->type == PLAYER)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "Powerful countermagic cancels all harmful magic here!");
		}

		return 0;
	}

	/* No summon and a summon cast. */
	if (MAP_NOSUMMON(cast_op->map) && (spells[type].flags & SPELL_DESC_SUMMON))
	{
		if (op->type == PLAYER)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "Powerful countermagic cancels all summoning here!");
		}

		return 0;
	}

	if (op->type == PLAYER)
	{
		CONTR(op)->praying = 0;

		/* Cancel player spells which are denied - only real spells (not
		 * potion, wands, ...) */
		if (item == spellNormal)
		{
			if (caster->path_denied & s->path)
			{
				new_draw_info(NDI_UNIQUE, 0, op, "It is denied for you to cast that spell.");
				return 0;
			}

			if (!(QUERY_FLAG(op, FLAG_WIZ)))
			{
				if (!(spells[type].flags & SPELL_DESC_WIS) && op->stats.sp < (points_used = SP_level_spellpoint_cost(caster, type)))
				{
					new_draw_info(NDI_UNIQUE, 0, op, "You don't have enough mana.");
					return 0;
				}

				if ((spells[type].flags & SPELL_DESC_WIS) && op->stats.grace < (points_used = SP_level_gracepoint_cost(caster, type)))
				{
					new_draw_info(NDI_UNIQUE, 0, op, "You don't have enough grace.");
					return 0;
				}
			}
		}

		/* If it a prayer, grab the player's god - if we have none, we
		 * can't cast, except potions */
		if (spells[type].flags & SPELL_DESC_WIS && item != spellPotion)
		{
			if ((godname = determine_god(op)) == shstr_cons.none)
			{
				new_draw_info(NDI_UNIQUE, 0, op, "You need a deity to cast a prayer!");
				return 0;
			}
		}
	}

	/* If it is an ability, assume that the designer of the archetype
	 * knows what they are doing. */
	if (item == spellNormal && !ability && SK_level(caster) < s->level && !QUERY_FLAG(op, FLAG_WIZ))
	{
		if (op->type == PLAYER)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "You lack enough skill to cast that spell.");
		}

		return 0;
	}

	/* Applying potions always go in the applier itself (aka drink or
	 * break) */
	if (item == spellPotion)
	{
		/* If the potion casts an onself spell, don't use the facing
		 * direction (given by apply.c) */
		if (spells[type].flags & SPELL_DESC_SELF)
		{
			target = op;
			dir = 0;
		}
	}
	else if (find_target_for_spell(op, &target, spells[type].flags) == 0)
	{
		/* Little trick - if we fail we set target = NULL, which marks it
		 * "yourself". */
		if (op->type == PLAYER)
		{
			new_draw_info_format(NDI_UNIQUE, 0, op, "You can't cast this spell on %s!", target ? target->name : "yourself");
		}

		return 0;
	}

	/* If valid target is not in range for selected spell, skip here
	 * casting. */
	if (target)
	{
		get_rangevector_from_mapcoords(op->map, op->x, op->y, target->map, target->x, target->y, &rv, 0);

		if ((abs(rv.distance_x) > abs(rv.distance_y) ? abs(rv.distance_x) : abs(rv.distance_y)) > spells[type].range)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "Your target is out of range!");
			return 0;
		}
	}

	/* Tell player that his spell has been redirected to himself. */
	if (op->type == PLAYER && target == op && CONTR(op)->target_object != op)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You auto-target yourself with this spell!");
	}

	/* Ban removed on clerical spells in no-magic areas. */
	if (!ability && ((!(s->flags & SPELL_DESC_WIS) && blocks_magic(op->map, op->x, op->y)) || ((s->flags & SPELL_DESC_WIS) && blocks_cleric(op->map, op->x, op->y))))
	{
		if (op->type != PLAYER)
		{
			return 0;
		}

		if (s->flags & SPELL_DESC_WIS)
		{
			new_draw_info_format(NDI_UNIQUE, 0, op, "This ground is unholy! %s ignores you.", godname);
		}
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

	/* Chance to fumble the spell by too low wisdom. */
	if (item == spellNormal && op->type == PLAYER && s->flags & SPELL_DESC_WIS && random_roll(0, 99, op, PREFER_HIGH) < s->level / (float) MAX(1, op->chosen_skill->level) * cleric_chance[op->stats.Wis])
	{
		play_sound_player_only(CONTR(op), SOUND_FUMBLE_SPELL, SOUND_NORMAL, 0, 0);
		new_draw_info(NDI_UNIQUE, 0, op, "You fumble the prayer because your wisdom is low.");

		/* Shouldn't happen... */
		if (s->sp == 0)
		{
			return 0;
		}

		return random_roll(1, SP_level_spellpoint_cost(caster, type), op, PREFER_LOW);
	}

	if (item == spellNormal && op->type == PLAYER && !(s->flags & SPELL_DESC_WIS))
	{
		int failure = random_roll(0, 199, op, PREFER_LOW) - CONTR(op)->encumbrance + op->chosen_skill->level - s->level + 35;

		if (failure < 0)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "You bungle the spell because you have too much heavy equipment in use.");
			return random_roll(0, SP_level_spellpoint_cost(caster, type), op, PREFER_LOW);
		}
	}

	/* Now let's talk about action/shooting speed */
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
	/* A last sanity check: are caster and target *really* valid? */
	if ((caster && !OBJECT_ACTIVE(caster)) || (target && !OBJECT_ACTIVE(target)))
	{
		return 0;
	}

	switch ((enum spellnrs) type)
	{
		case SP_RESTORATION:
		case SP_CURE_CONFUSION:
		case SP_MINOR_HEAL:
		case SP_GREATER_HEAL:
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
		case SP_PROT_COLD:
		case SP_PROT_FIRE:
		case SP_PROT_ELEC:
		case SP_PROT_POISON:
			success = cast_change_attr(op, caster, target, type);
			break;

		case SP_DETECT_MAGIC:
		case SP_DETECT_CURSE:
			success = cast_detection(op, target, type);
			break;

		case SP_IDENTIFY:
			success = cast_identify(target, SK_level(caster), NULL, IDENTIFY_MODE_NORMAL);
			break;

		/* Spells after this use direction and not a target */
		case SP_ICESTORM:
		case SP_FIRESTORM:
			success = cast_cone(op, caster, dir, duration, type, spellarch[type]);
			break;

		case SP_BULLET:
		case SP_CAUSE_LIGHT:
		case SP_PROBE:
			success = fire_arch_from_position(op, caster, op->x, op->y, dir, spellarch[type], type);
			break;

		case SP_TOWN_PORTAL:
			success = cast_create_town_portal(op);
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

		case SP_CONSECRATE:
			success = cast_consecrate(op);
			break;

		case SP_CAUSE_COLD:
		case SP_CAUSE_FLU:
		case SP_CAUSE_LEPROSY:
		case SP_CAUSE_SMALLPOX:
		case SP_CAUSE_PNEUMONIC_PLAGUE:
			success = cast_cause_disease(op, caster, dir, spellarch[type], type);
			break;

		case SP_FINGER_DEATH:
			success = finger_of_death(op);
			break;

		case SP_METEOR:
			success = fire_arch_from_position(op, caster, op->x, op->y, dir, find_archetype("meteor"), type);
			break;

		case SP_POISON_FOG:
			success = fire_arch_from_position(op, caster, op->x, op->y, dir, spellarch[type], type);
			break;

		case SP_METEOR_SWARM:
			success = 1;
			fire_swarm(op, caster, dir, spellarch[type], SP_METEOR, die_roll(3, 3, op, PREFER_HIGH) + SP_level_strength_adjust(caster, type), 0);
			break;

		case SP_BULLET_SWARM:
			success = 1;
			fire_swarm(op, caster, dir, spellarch[type], SP_BULLET, die_roll(3, 3, op, PREFER_HIGH) + SP_level_strength_adjust(caster, type), 0);
			break;

		case SP_BULLET_STORM:
			success = 1;
			fire_swarm(op, caster, dir, spellarch[type], SP_BULLET, die_roll(3, 3, op, PREFER_HIGH) + SP_level_strength_adjust(caster, type), 0);
			break;

		case SP_DESTRUCTION:
			success = cast_destruction(op, caster, 5 + op->stats.Int, AT_MAGIC);
			break;

		case SP_BOMB:
			success = create_bomb(op, caster, dir, type, "bomb");
			break;

		default:
			LOG(llevBug, "BUG: cast_spell() has invalid spell nr. %d\n", type);
			break;
	}

	play_sound_map(op->map, op->x, op->y, spells[type].sound, SOUND_SPELL);

	if (item == spellNPC)
	{
		return success;
	}

#ifdef SPELLPOINT_LEVEL_DEPEND
	return success ? SP_level_spellpoint_cost(caster, type) : 0;
#else
	return success ? (s->sp * PATH_SP_MULT(op, s)) : 0;
#endif
}

/**
 * Creates object new_op in direction dir or if that is blocked, beneath
 * the player (op).
 * @param op Who is casting.
 * @param new_op Object to insert.
 * @param dir Direction to insert into. Can be 0.
 * @return Direction that the object was actually placed in. */
int cast_create_obj(object *op, object *new_op, int dir)
{
	mapstruct *mt;
	int xt, yt;

	xt = op->x + freearr_x[dir];
	yt = op->y + freearr_y[dir];

	if (!(mt = get_map_from_coord(op->map, &xt, &yt)))
	{
		return 0;
	}

	if (dir && wall_blocked(mt, xt, yt))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Something is in the way.");
		new_draw_info(NDI_UNIQUE, 0, op, "You cast it at your feet.");
		dir = 0;
	}

	xt = op->x + freearr_x[dir];
	yt = op->y + freearr_y[dir];

	if (!(mt = get_map_from_coord(op->map, &xt, &yt)))
	{
		return 0;
	}

	new_op->x = xt;
	new_op->y = yt;
	new_op->map = mt;
	insert_ob_in_map(new_op, mt, op, 0);
	return dir;
}

/**
 * Summon a monster.
 * @param op Player.
 * @param caster Caster.
 * @param dir Direction.
 * @param at Monster archetype.
 * @param spellnum Spell ID.
 * @return 1 on success, 0 otherwise. */
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
	{
		dir = find_free_spot(NULL, NULL, op->map, op->x, op->y, 1, 9);
	}

	if (dir != -1)
	{
		xt = op->x + freearr_x[dir];
		yt = op->y + freearr_y[dir];

		if (!(mt = get_map_from_coord(op->map, &xt, &yt)))
		{
			return 0;
		}

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

	/* Make the speed positive. */
	if (tmp->speed < 0)
	{
		tmp->speed = -tmp->speed;
	}

	/* This sets the level dependencies on dam and hp for monsters */
	if (op->type == PLAYER)
	{
		/* Players can't cope with too strong summonings, but monsters
		 * can. Reserve these for players. */
		tmp->stats.hp = spells[spellnum].bdur + 10 * SP_level_strength_adjust(caster, spellnum);
		tmp->stats.dam = spells[spellnum].bdam + 2 * SP_level_dam_adjust(caster, spellnum);
		tmp->speed += 0.02f * (int) SP_level_dam_adjust(caster, spellnum);
		tmp->speed = MIN(tmp->speed, 1.0f);
	}

	tmp->stats.wc += SP_level_dam_adjust(caster, spellnum);

	/* Seen this go negative! */
	if (tmp->stats.dam < 0)
	{
		tmp->stats.dam = 127;
	}

	/* Make experience increase in proportion to the strength of the summoned creature. */
	tmp->stats.exp *= SP_level_spellpoint_cost(caster, spellnum) / spells[spellnum].sp;

	tmp->speed_left = -1;
	tmp->x = xt;
	tmp->y = yt;
	tmp->map = mt;
	tmp->direction = dir;
	insert_ob_in_map(tmp, mt, op, 0);
	return 1;
}

/**
 * Returns true if it is ok to put spell op on the space/may provided.
 *
 * @param m Map.
 * @param x X position on map.
 * @param y Y position on map.
 * @param op Object to test for.
 * @return 1 if we can add op, 0 else. */
static int ok_to_put_more(mapstruct *m, int x, int y, object *op)
{
	object *tmp;

	/* We must check map here or we will go in trouble some line down */
	if (!(m = get_map_from_coord(m, &x, &y)))
	{
		return 0;
	}

	/* Only REAL walls will block this - except player only tiles */
	if (wall(m, x, y))
	{
		return 0;
	}

	for (tmp = get_map_ob(m, x, y); tmp != NULL; tmp = tmp->above)
	{
		/* Only one part for cone/explosion per tile! */
		if (op->type == tmp->type && op->weight_limit == tmp->weight_limit)
		{
			return 0;
		}
	}

	/* If it passes the above tests, it must be OK */
	return 1;
}

/**
 * Cast a bolt-like spell.
 * @param op Who is casting the spell.
 * @param caster What object is casting the spell (rod, ...).
 * @param dir Firing direction.
 * @param type Spell ID.
 * @retval 0 No bolt could be fired.
 * @retval 1 Bolt was fired (but may have been destroyed already). */
int fire_bolt(object *op, object *caster, int dir, int type)
{
	object *tmp = NULL;

	if (!spellarch[type])
	{
		return 0;
	}

	tmp = arch_to_object(spellarch[type]);

	if (tmp == NULL)
	{
		return 0;
	}

	/* We should have from the arch type the default damage - we add our
	 * new damage profile. */
	tmp->stats.dam = (sint16) SP_level_dam_adjust2(caster, type, tmp->stats.dam);

	/* For duration use the old formula */
	tmp->stats.hp = spells[type].bdur + SP_level_strength_adjust(caster, type);

	tmp->x = op->x, tmp->y = op->y;
	tmp->direction = dir;

	if (QUERY_FLAG(tmp, FLAG_IS_TURNABLE))
	{
		SET_ANIMATION(tmp, (NUM_ANIMATIONS(tmp) / NUM_FACINGS(tmp)) * tmp->direction);
	}

	set_owner(tmp, op);
	tmp->level = SK_level(caster);
	tmp->x += DIRX(tmp), tmp->y += DIRY(tmp);

	if (wall(op->map, tmp->x, tmp->y))
	{
		if (!QUERY_FLAG(tmp, FLAG_REFLECTING))
		{
			return 0;
		}

		tmp->x = op->x, tmp->y = op->y;
		tmp->direction = absdir(tmp->direction + 4);
	}

	if ((tmp = insert_ob_in_map(tmp, op->map, op, 0)) != NULL)
	{
		move_bolt(tmp);
	}

	return 1;
}

/**
 * Fires an archetype.
 * @param op Person firing the object.
 * @param caster Object casting the spell.
 * @param x X position where to fire the spell.
 * @param y Y position where to fire the spell.
 * @param dir Direction to fire in.
 * @param at The archetype to fire.
 * @param type Spell ID.
 * @return 0 on failure, 1 on success. */
int fire_arch_from_position(object *op, object *caster, sint16 x, sint16 y, int dir, archetype *at, int type)
{
	object *tmp, *env;

	if (at == NULL)
	{
		return 0;
	}

	for (env = op; env->env != NULL; env = env->env)
	{
	}

	if (env->map == NULL)
	{
		return 0;
	}

	tmp = arch_to_object(at);

	if (tmp == NULL)
	{
		return 0;
	}

	tmp->stats.sp = type;
	tmp->stats.dam = (sint16) SP_level_dam_adjust2(caster, type, tmp->stats.dam);
	tmp->stats.hp = spells[type].bdur + SP_level_strength_adjust(caster, type);
	tmp->x = x, tmp->y = y;
	tmp->direction = dir;
	tmp->stats.grace = tmp->last_sp;
	tmp->stats.maxgrace = 60 + (RANDOM() % 12);

	if (get_owner(op) != NULL)
	{
		copy_owner(tmp, op);
	}
	else
	{
		set_owner(tmp, op);
	}

	tmp->level = casting_level(caster, SK_level(caster), type);

	if (QUERY_FLAG(tmp, FLAG_IS_TURNABLE))
	{
		SET_ANIMATION(tmp, (NUM_ANIMATIONS(tmp) / NUM_FACINGS(tmp)) * dir);
	}

	if ((tmp = insert_ob_in_map(tmp, op->map, op, 0)) == NULL)
	{
		return 1;
	}

	switch (type)
	{
		case SP_POISON_FOG:
#if 0
			tmp->stats.food = spells[type].bdur + 4 * SP_level_strength_adjust(caster, type);
#endif
			move_ball_lightning(tmp);
			break;

		default:
			move_fired_arch(tmp);
	}

	return 1;
}

/**
 * Casts a cone spell.
 * @param op Person firing the object.
 * @param caster Object casting the spell.
 * @param dir Direction to fire in.
 * @param strength Strength of the spell.
 * @param spell_type ID of the spell.
 * @param spell_arch Spell's arch.
 * @retval 0 Couldn't cast.
 * @retval 1 Successful cast. */
int cast_cone(object *op, object *caster, int dir, int strength, int spell_type, archetype *spell_arch)
{
	object *tmp;
	int i, success = 0, range_min = -1, range_max = 1;
	uint32 count_ref;

	if (!dir)
	{
		range_min = -3, range_max = 4, strength /= 2;
	}

	/* Our initial spell object */
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
		{
			continue;
		}

		success = 1;

		if (!tmp)
		{
			tmp = arch_to_object(spell_arch);
		}

		set_owner(tmp, op);
		copy_owner(tmp, op);
		/* *very* important - miss this and the spells go really wild! */
		tmp->weight_limit = count_ref;

		tmp->level = casting_level(caster, SK_level(caster), spell_type);
		tmp->x = x, tmp->y = y;

		if (dir)
		{
			tmp->stats.sp = dir;
		}
		else
		{
			tmp->stats.sp = i;
		}

		tmp->stats.hp = strength + (SP_level_strength_adjust(caster, spell_type) / 2);
		tmp->stats.dam = (sint16) SP_level_dam_adjust2(caster, spell_type, tmp->stats.dam);
		tmp->stats.maxhp = tmp->count;

		if (!QUERY_FLAG(tmp, FLAG_FLYING))
		{
			LOG(llevDebug, "DEBUG: cast_cone(): arch %s doesn't have flying 1\n", spell_arch->name);
		}

		if ((!QUERY_FLAG(tmp, FLAG_WALK_ON) || !QUERY_FLAG(tmp, FLAG_FLY_ON)) && tmp->stats.dam)
		{
			LOG(llevDebug, "DEBUG: cast_cone(): arch %s doesn't have walk_on 1 and fly_on 1\n", spell_arch->name);
		}

		if (!insert_ob_in_map(tmp, op->map, op, 0))
		{
			return 0;
		}

		if (tmp->other_arch)
		{
			cone_drop(tmp);
		}

		tmp = NULL;
	}

	/* Can happen when we can't drop anything */
	if (tmp)
	{
		/* Was not inserted */
		if (!QUERY_FLAG(tmp, FLAG_REMOVED))
		{
			remove_ob(tmp);
		}
	}

	return success;
}

/**
 * Checks to see if the cone pushes objects as well as flies over and
 * damages them.
 * @param op The object. */
void check_cone_push(object *op)
{
	/* object on the map */
	object *tmp, *tmp2;
	int weight_move;

	weight_move = 1000 + 1000 * op->level;

	for (tmp = get_map_ob(op->map, op->x, op->y); tmp != NULL; tmp = tmp->above)
	{
		int num_sections = 1;

		/* Don't move parts of objects */
		if (tmp->head)
		{
			continue;
		}

		/* Don't move floors or immobile objects */
		if (QUERY_FLAG(tmp, FLAG_IS_FLOOR) || (!QUERY_FLAG(tmp, FLAG_ALIVE) && QUERY_FLAG(tmp, FLAG_NO_PICK)))
		{
			continue;
		}

		/* Count the object's sections */
		for (tmp2 = tmp; tmp2 != NULL; tmp2 = tmp2->more)
		{
			num_sections++;
		}

		/* Move it. */
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
	}
}

/**
 * Drops an object based on what is in the cone's "other_arch".
 * @param op The object. */
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

/**
 * Causes cone object 'op' to move a space/hit creatures.
 * @param op Cone object moving. */
void move_cone(object *op)
{
	int i;
	tag_t tag;

	/* If no map then hit_map will crash so just ignore object */
	if (!op->map)
	{
		LOG(llevBug, "BUG: Tried to move_cone object %s without a map.\n", query_name(op, NULL));
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		return;
	}

	/* Lava saves it's life, but not yours :) */
	if (QUERY_FLAG(op, FLAG_LIFESAVE))
	{
		hit_map(op, 0);
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
	op->stats.food |= hit_map(op, 0);

	if (was_destroyed(op, tag))
	{
		return;
	}

	if ((op->stats.hp -= 2) < 0)
	{
		if (op->stats.exp)
		{
			op->speed = 0;
			update_ob_speed(op);
			op->stats.exp = 0;
			/* So they will join */
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
	{
		return;
	}

	op->stats.food = 1;

	for (i = -1; i < 2; i++)
	{
		int x = op->x + freearr_x[absdir(op->stats.sp + i)], y = op->y + freearr_y[absdir(op->stats.sp + i)];

		if (ok_to_put_more(op->map, x, y, op))
		{
			object *tmp = arch_to_object(op->arch);

			copy_owner(tmp, op);

			/* *very* important - this is the count value of the
			 * *first* object we created with this cone spell.
			 * we use it for identify this spell. Miss this
			 * and ok_to_put_more will allow to create 1000th
			 * in a single tile! */
			tmp->weight_limit = op->weight_limit;
			tmp->x = x, tmp->y = y;

			tmp->level = op->level;
			tmp->stats.sp = op->stats.sp, tmp->stats.hp = op->stats.hp + 1;
			tmp->stats.maxhp = op->stats.maxhp;
			tmp->stats.dam = op->stats.dam;

			if (!insert_ob_in_map(tmp, op->map, op, 0))
			{
				return;
			}

			if (tmp->other_arch)
			{
				cone_drop(tmp);
			}
		}
	}
}

/**
 * Fire a ball.
 * @param op Object.
 * @param dir Direction to fire.
 * @param strength Strength. */
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
	{
		move_fired_arch(tmp);
	}
}

/**
 * Expands an explosion.
 * @param op Piece of explosion expanding. */
void explosion(object *op)
{
	object *tmp;
	mapstruct *m = op->map;
	int i;

	if (--(op->stats.hp) < 0)
	{
		destruct_ob(op);
		return;
	}

	if (op->above != NULL && op->above->type != PLAYER)
	{
		SET_FLAG(op, FLAG_NO_APPLY);
		remove_ob(op);

		if (check_walk_off(op, NULL, MOVE_APPLY_VANISHED) != CHECK_WALK_OK)
		{
			return;
		}

		if (!insert_ob_in_map(op, op->map, op, 0))
		{
			return;
		}

		CLEAR_FLAG(op, FLAG_NO_APPLY);
	}

	hit_map(op, 0);

	if (op->stats.hp > 2 && !op->value)
	{
		op->value = 1;

		for (i = 1; i < 9; i++)
		{
			int dx, dy;

			if (wall(op->map, dx = op->x + freearr_x[i], dy = op->y + freearr_y[i]))
			{
				continue;
			}

			if (blocks_view(op->map, dx, dy))
			{
				continue;
			}

			if (ok_to_put_more(op->map, dx, dy, op))
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

/**
 * Causes op to fork.
 * @param op Original bolt.
 * @param tmp First piece of the fork. */
void forklightning(object *op, object *tmp)
{
	mapstruct *m;
	/* Direction or -1 for left, +1 for right 0 if no new bolt */
	int xt, yt, new_dir = 1;
	/* Stores temporary dir calculation */
	int t_dir;

	/* pick a fork direction.  tmp->stats.Con is the left bias
	 * i.e., the chance in 100 of forking LEFT
	 * Should start out at 50, down to 25 for one already going left
	 * down to 0 for one going 90 degrees left off original path*/

	/* Fork left */
	if (rndm(0, 99) < tmp->stats.Con)
	{
		new_dir = -1;
	}

	/* Check the new dir for a wall and in the map*/
	t_dir = absdir(tmp->direction + new_dir);

	xt = tmp->x + freearr_x[t_dir];
	yt = tmp->y + freearr_y[t_dir];

	if (!(m = get_map_from_coord(tmp->map, &xt, &yt)) || wall(m, xt, yt))
	{
		new_dir = 0;
	}

	/* OK, we made a fork */
	if (new_dir)
	{
		object *new_bolt = get_object();

		copy_object(tmp, new_bolt);
		/* Reduce chances of subsequent forking */
		new_bolt->stats.Dex -= 10;
		/* Less forks from main bolt too */
		tmp->stats.Dex -= 10;
		/* Adjust the left bias */
		new_bolt->stats.Con += 25 * new_dir;
		new_bolt->speed_left = -0.1f;
		new_bolt->direction = t_dir;
		new_bolt->stats.hp++;
		new_bolt->x = xt;
		new_bolt->y = yt;
		/* Reduce daughter bolt damage */
		new_bolt->stats.dam /= 2;
		new_bolt->stats.dam++;
		/* Reduce father bolt damage */
		tmp->stats.dam /= 2;
		tmp->stats.dam++;

		if (!insert_ob_in_map(new_bolt, m, op, 0))
		{
			return;
		}

		update_turn_face(new_bolt);
	}
}

/**
 * Decides whether the (spell-)object sp_op will be reflected from the
 * given mapsquare. Returns 1 if true.
 *
 * (Note that for living creatures there is a small chance that
 * reflect_spell fails.)
 * @param m Map.
 * @param x X position.
 * @param y Y position.
 * @param sp_op Spell object to test.
 * @return 1 if reflected, 0 otherwise. */
int reflwall(mapstruct *m, int x, int y, object *sp_op)
{
	/* No reflection when we have a illegal space and/or non reflection flag set */
	if (!(m = get_map_from_coord(m, &x, &y)) || !(GET_MAP_FLAGS(m, x, y) & P_REFL_SPELLS))
	{
		return 0;
	}

	/* we have reflection. But there is a small chance it will fail.
	 * test it. */

	/* Reflect always */
	if (sp_op->type == LIGHTNING)
	{
		return 1;
	}

	if (!missile_reflection_adjust(sp_op, QUERY_FLAG(sp_op, FLAG_WAS_REFLECTED)))
	{
		return 0;
	}

	/* We get resisted - except a small fail chance */
	if ((rndm(0, 99)) < 90 - sp_op->level / 10)
	{
		SET_FLAG(sp_op, FLAG_WAS_REFLECTED);
		return 1;
	}

	return 0;
}

/**
 * Moves bolt 'op'. Basically, it just advances a space, and checks for
 * various things that may stop it.
 * @param op The bolt object moving. */
void move_bolt(object *op)
{
	object *tmp;
	int w, r;

	if (--(op->stats.hp) < 0)
	{
		destruct_ob(op);
		return;
	}

	hit_map(op, 0);

	if (!op->value && --(op->stats.exp) > 0)
	{
		op->value = 1;

		if (!op->direction)
		{
			return;
		}

		if (blocks_view(op->map, op->x + DIRX(op), op->y + DIRY(op)))
		{
			return;
		}

		w = wall(op->map, op->x + DIRX(op), op->y + DIRY(op));
		r = reflwall(op->map, op->x + DIRX(op), op->y + DIRY(op), op);

		if (w && !QUERY_FLAG(op, FLAG_REFLECTING))
		{
			return;
		}

		/* We're about to bounce */
		if (w || r)
		{
			if (!QUERY_FLAG(op, FLAG_REFLECTING))
			{
				return;
			}

			op->value = 0;

			if (op->direction & 1)
			{
				op->direction = absdir(op->direction + 4);
			}
			else
			{
				int left = wall(op->map, op->x + freearr_x[absdir(op->direction - 1)], op->y + freearr_y[absdir(op->direction - 1)]), right = wall(op->map, op->x + freearr_x[absdir(op->direction + 1)], op->y + freearr_y[absdir(op->direction + 1)]);

				if (left == right)
				{
					op->direction = absdir(op->direction + 4);
				}
				else if (left)
				{
					op->direction = absdir(op->direction + 2);
				}
				else if (right)
				{
					op->direction = absdir(op->direction - 2);
				}
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
			{
				return;
			}

			/* New forking code.  Possibly create forks of this object
			 * going off in other directions. */

			/* stats.Dex % of forking */
			if (rndm(0, 99) < tmp->stats.Dex)
			{
				forklightning(op, tmp);
			}

			if (tmp)
			{
				if (!tmp->stats.food)
				{
					tmp->stats.food = 1;
					move_bolt(tmp);
				}
				else
				{
					tmp->stats.food = 0;
				}
			}
		}
	}
}

/**
 * Move a golem object.
 * @param op Object. */
void move_golem(object *op)
{
	int x, y, made_attack = 0;
	object *tmp;
	tag_t tag;
	object *victim;
	mapstruct *m;

	/* Has already been moved */
	if (QUERY_FLAG(op, FLAG_MONSTER) && op->stats.hp)
	{
		return;
	}

	if (get_owner(op) == NULL)
	{
		LOG(llevDebug, "DEBUG: Golem without owner destructed.\n");
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
			{
				strcpy(buf, "Your staff stops slithering around and lies still.");
			}
			else
			{
				snprintf(buf, sizeof(buf), "Your %s departed this plane.", op->name);
			}
		}
		else if (!strncmp(op->name, "animated ", 9))
		{
			snprintf(buf, sizeof(buf), "Your %s falls to the ground.", op->name);
		}
		else
		{
			snprintf(buf, sizeof(buf), "Your %s dissolved.", op->name);
		}

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
	{
		return;
	}

	if (was_destroyed(op, tag))
	{
		return;
	}

	/* Multipart golems */
	for (tmp = op; tmp; tmp = tmp->more)
	{
		x = tmp->x + freearr_x[op->direction];
		y = tmp->y + freearr_y[op->direction];

		if (!(m = get_map_from_coord(op->map, &x, &y)))
		{
			continue;
		}

		for (victim = get_map_ob(m, x, y); victim; victim = victim->above)
		{
			if (IS_LIVE(victim))
			{
				break;
			}
		}

		/* We used to call will_hit_self to make sure we don't
		 * hit ourselves, but that didn't work, and I don't really
		 * know if that was more efficient anyways than this.
		 * This at least works.  Note that victim->head can be NULL,
		 * but since we are not trying to dereferance that pointer,
		 * that isn't a problem. */
		if (victim && victim != op && victim->head != op)
		{
			/* For golems with race fields, we don't attack
			 * aligned races. */
			if (victim->race && op->race && strstr(op->race, victim->race))
			{
				if (op->owner)
				{
					new_draw_info_format(NDI_UNIQUE, 0, op->owner, "%s avoids damaging %s.", op->name, victim->name);
				}
			}
			else if (op->exp_obj && victim == op->owner)
			{
				if (op->owner)
				{
					new_draw_info_format(NDI_UNIQUE, 0, op->owner, "%s avoids damaging you.", op->name);
				}
			}
			else
			{
				attack_ob(victim, op);
				made_attack = 1;
			}
		}
	}

	if (made_attack)
	{
		update_object(op, UP_OBJ_FACE);
	}
}

/**
 * Control golem's movement direction.
 * @param op Golem.
 * @param dir Direction to change. */
void control_golem(object *op, int dir)
{
	op->direction = dir;
}

/**
 * Move a missile object.
 * @param op The missile that needs to be moved. */
void move_missile(object *op)
{
	int i, new_x, new_y;
	object *owner;
	mapstruct *mt;

	owner = get_owner(op);

	if (owner == NULL)
	{
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		return;
	}

	new_x = op->x + DIRX(op);
	new_y = op->y + DIRY(op);

	if (!(mt = get_map_from_coord(op->map, &new_x, &new_y)))
	{
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		return;
	}

	if (blocked(op, mt, new_x, new_y, op->terrain_flag))
	{
		tag_t tag = op->count;
		hit_map(op, op->direction);

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
	{
		return;
	}

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

/**
 * Causes an object to explode, eg, a firebullet, poison cloud ball, etc.
 * @param op The object to explode. */
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

	/* Object is in container, try to drop on map! */
	if (op->env)
	{
		for (env = op; env->env != NULL; env = env->env)
		{
		}

		xt = env->x;
		yt = env->y;

		if (!(m = get_map_from_coord(env->map, &xt, &yt)))
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
		{
			return;
		}
	}

	tmp = arch_to_object(op->other_arch);

	switch (tmp->type)
	{
		case POISONCLOUD:
		case FBALL:
		{
			tmp->stats.dam = (sint16) SP_level_dam_adjust2(op, op->stats.sp, tmp->stats.dam);

			copy_owner(tmp, op);

			if (op->stats.hp)
			{
				tmp->stats.hp = op->stats.hp;
			}

			/* Unique ID */
			tmp->stats.maxhp = op->count;
			tmp->x = op->x;
			tmp->y = op->y;

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
			{
				type = op->stats.sp;
			}

			copy_owner(tmp, op);
			cast_cone(op, op, 0, spells[type].bdur, type, op->other_arch);
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

/**
 * If we are here, the arch (spell) we check was able to move to this
 * place. wall() has failed, including reflection checking.
 *
 * Look for a target.
 * @param op The spell object. */
void check_fired_arch(object *op)
{
	tag_t op_tag = op->count, tmp_tag;
	object *tmp, *hitter, *head;
	int dam, flag;

	/* we return here if we have NOTHING blocking here */
	if (!blocked(op, op->map, op->x, op->y, op->terrain_flag))
	{
		return;
	}

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
	{
		hitter = op;
	}
	else if (hitter->head)
	{
		hitter = hitter->head;
	}

	flag = GET_MAP_FLAGS(op->map, op->x, op->y) & P_IS_PVP;

	for (tmp = get_map_ob(op->map, op->x, op->y); tmp != NULL; tmp = tmp->above)
	{
		head = tmp->head;

		if (!head)
		{
			head = tmp;
		}

		if (!IS_LIVE(tmp))
		{
			continue;
		}

		/* Let friends fire through friends */
		if (is_friend_of(hitter, head) || head == hitter)
		{
			continue;
		}

		tmp_tag = tmp->count;

		dam = hit_player(tmp, op->stats.dam, op, AT_INTERNAL);

		if (was_destroyed(op, op_tag) || !was_destroyed(tmp, tmp_tag) || (op->stats.dam -= dam) < 0)
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

/**
 * Move a fired arch.
 * @param op Object. */
void move_fired_arch(object *op)
{
	mapstruct *m;
	tag_t op_tag = op->count;
	int new_x, new_y;

	if (op->stats.sp == SP_METEOR)
	{
		replace_insert_ob_in_map("fire_trail", op);

		if (was_destroyed(op, op_tag))
		{
			return;
		}
	}

	new_x = op->x + DIRX(op);
	new_y = op->y + DIRY(op);

	if (!(m = get_map_from_coord(op->map, &new_x, &new_y)))
	{
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		return;
	}

	/* the spell has reached a wall and/or the end of its moving points */
	if (!op->last_sp-- || (!op->direction || wall(m, new_x, new_y)))
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
	{
		return;
	}

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
		update_turn_face(op);
	}
	else
	{
		check_fired_arch(op);
	}
}

/**
 * Drain charges from a rod.
 * @param rod Rod to drain. */
void drain_rod_charge(object *rod)
{
	rod->stats.hp -= spells[rod->stats.sp].sp;

	if (QUERY_FLAG(rod, FLAG_ANIMATE))
	{
		fix_rod_speed(rod);
	}
}

/**
 * Fix the speed of the rod, based on its hp.
 * @param rod Rod to fix. */
void fix_rod_speed(object *rod)
{
	rod->speed = (FABS(rod->arch->clone.speed) * rod->stats.hp) / (float) rod->stats.maxhp;

	if (rod->speed < 0.02f)
	{
		rod->speed = 0.02f;
	}

	update_ob_speed(rod);
}
/**
 * Check if the object can cast the spell on the target.
 * @param op Object.
 * @param target Target
 * @param flags Flags.
 * @return 1 if the object can cast the spell on the target, 0
 * otherwise. */
int find_target_for_spell(object *op, object **target, uint32 flags)
{
	object *tmp;

	/* Default target is nothing. */
	*target = NULL;

	/* We cast something on the map... No target */
	if (flags & SPELL_DESC_DIRECTION)
	{
		return 1;
	}

	/* A player has invoked this spell. */
	if (op->type == PLAYER)
	{
		/* Try to cast on self but only when really no friendly or enemy is set. */
		if ((flags & SPELL_DESC_SELF) && !(flags & (SPELL_DESC_ENEMY | SPELL_DESC_FRIENDLY)))
		{
			/* Self... and no other tests */
			*target = op;
			return 1;
		}

		tmp = CONTR(op)->target_object;

		/* Let's check our target - we have one? friend or enemy? */
		if (!tmp || !OBJECT_ACTIVE(tmp) || tmp == CONTR(op)->ob || CONTR(op)->target_object_count != tmp->count)
		{
			/* Can we cast this on self? */
			if (flags & SPELL_DESC_SELF)
			{
				/* Right, we are target */
				*target = op;
				return 1;
			}
		}
		/* We have a target and it's not self */
		else
		{
			/* Player? */
			if (tmp->type == PLAYER)
			{
				/* Now it will be tricky...
				 * a.) Friendly spell - always allowed to cast
				 * b.) Enemy spell - only allowed when op AND target are
				 *     in a pvp area.
				 * c.) If not a. and b. AND self - cast on self */
				if ((flags & SPELL_DESC_FRIENDLY) && !pvp_area(op, tmp))
				{
					*target = tmp;
					return 1;
				}

				if (flags & SPELL_DESC_ENEMY)
				{
					/* Ok... now op AND tmp must be in PvP - if one not,
					 * this is not allowed. */
					if (op->map && tmp->map && pvp_area(op->type == PLAYER ? op : get_owner(op), tmp->type == PLAYER ? tmp : get_owner(tmp)))
					{
						/* Here we go... PvP! */
						*target = tmp;
						return 1;
					}
				}

				if (flags & SPELL_DESC_SELF)
				{
					*target = op;
					return 1;
				}
			}
			/* Friendly NPC? */
			else if (QUERY_FLAG(tmp, FLAG_FRIENDLY))
			{
				if (flags & SPELL_DESC_FRIENDLY)
				{
					*target = tmp;
					return 1;
				}

				if (flags & SPELL_DESC_SELF)
				{
					*target = op;
					return 1;
				}
			}
			/* Ok, it is a bad guy */
			else
			{
				if (flags & SPELL_DESC_ENEMY)
				{
					*target = tmp;
					return 1;
				}

				if (flags & SPELL_DESC_SELF)
				{
					*target = op;
					return 1;
				}
			}
		}
	}
	/* A mob OR rune/firewall/.. OR a pet/summon controlled from player */
	else
	{
		/* we use op->enemy as target from non player caster.
		 * we need to set this from outside and for healing spells,
		 * we must set from outside temporary the enemy to a friendly unit.
		 * This is safe because we do no AI stuff here - we simply USE the
		 * target here even the stuff above looks like we select one...
		 * its only a fallback. */

		/* Sanity check for a legal target */
		if (op->enemy && OBJECT_ACTIVE(op->enemy) && op->enemy->count == op->enemy_count)
		{
			*target = op;
			return 1;
		}
	}

	/* Invalid target/spell or whatever */
	return 0;
}

/**
 * Move ball of lightning.
 * @param op The ball of lightning to move. */
void move_ball_lightning(object *op)
{
	int i, nx, ny, j, dam_save, dir;
	mapstruct *m;

	/* The following logic makes sure that the ball doesn't move into a
	 * wall, and makes sure that it will move along a wall to try and get
	 * at it's victim.  */
	dir = 0;

	if (!(rndm(0, 3)))
	{
		j = rndm(0, 1);
	}
	else
	{
		j = 0;
	}

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
	{
		return;
	}

	ny = op->y;
	nx = op->x;

	/* Save the original dam: we do halfdam on surrounding squares */
	dam_save = op->stats.dam;

	/* Loop over current square and neighbors to hit. */
	for (j = 0; j < 9; j++)
	{
		/* Hit these squares */
		int hx, hy;
		object *new_ob;

		hx = nx + freearr_x[j];
		hy = ny + freearr_y[j];

		/* Out of map - always skip */
		if (!(m = get_map_from_coord(op->map, &hx, &hy)))
		{
			continue;
		}

		/* If there is a wall - skip too */
		if (wall(m, hx, hy))
		{
			continue;
		}

		if (j)
		{
			op->stats.dam = dam_save / 2;
		}

		/* Hmm, I not sure this is always correct, but we will see
		 * perhaps we must add more checks to avoid bad hits on the
		 * map. */
		hit_map(op, j);

		/* Insert the other arch */
		if (op->other_arch)
		{
			new_ob = arch_to_object(op->other_arch);
			new_ob->x = hx;
			new_ob->y = hy;
			insert_ob_in_map(new_ob, m, op, 0);
		}
	}

	/* Restore to the center location and damage*/
	op->stats.dam = dam_save;
	i = spell_find_dir(op->map, op->x, op->y, get_owner(op));

	/* we have a preferred direction! */
	if (i >= 0)
	{
		/* Pick another direction if the preferred dir is blocked. */
		if (wall(op->map, nx + freearr_x[i], ny + freearr_y[i]))
		{
			/* -1, 0, +1 */
			i += rndm(0, 2) - 1;

			if (i == 0)
			{
				i = 8;
			}

			if (i == 9)
			{
				i = 1;
			}
		}

		op->direction = i;
	}
}

/**
 * Search what direction a spell should go in, first the center square
 * then some close squares in the given map at the given coordinates for
 * live objects.
 *
 * It will not consider the object given as exclude (= caster) among
 * possible live objects. If the caster is a player, the spell will go
 * after monsters only. If not, the spell will hunt players only.
 *
 * Exception is player in arena, who will be targeted unless excluded.
 * @param m Map to search from.
 * @param x X position where to search from.
 * @param y Y position where to search from.
 * @param exclude What object to avoid. Can be NULL, in which case all
 * bets are off.
 * @return Direction toward the first/closest live object if it finds
 * any, otherwise -1. */
int spell_find_dir(mapstruct *m, int x, int y, object *exclude)
{
	int i, max = SIZEOFFREE, nx, ny;
	int owner_type = 0;
	object *tmp;

	if (exclude && exclude->head)
	{
		exclude = exclude->head;
	}

	if (exclude && exclude->type)
	{
		owner_type = exclude->type;
	}

	for (i = rndm(1, 8); i < max; i++)
	{
		nx = x + freearr_x[i];
		ny = y + freearr_y[i];

		if ((m = get_map_from_coord(m, &nx, &ny)))
		{
			tmp = get_map_ob(m, nx, ny);

			while (tmp != NULL && (((owner_type == PLAYER && !QUERY_FLAG(tmp, FLAG_MONSTER) && !QUERY_FLAG(tmp, FLAG_GENERATOR) && !(tmp->type == PLAYER && pvp_area(NULL, tmp))) || (owner_type != PLAYER && tmp->type != PLAYER)) || (tmp == exclude || (tmp->head && tmp->head == exclude))))
			{
				tmp = tmp->above;
			}

			if (tmp != NULL && can_see_monsterP(m, nx, ny, i) && !blocks_view(m, nx, ny))
			{
				return freedir[i];
			}
		}
	}

	/* Flag for "keep going the way you were" */
	return -1;
}

/**
 * Returns adjusted damage based on the caster.
 * @param caster Who is casting.
 * @param spell_type Spell ID we're adjusting.
 * @return Adjusted damage. */
int SP_level_dam_adjust(object *caster, int spell_type)
{
	int level = casting_level(caster, SK_level(caster), spell_type);
	int adj = (level - spells[spell_type].level);

	if (adj < 0)
	{
		adj = 0;
	}

	if (spells[spell_type].ldam)
	{
		adj /= spells[spell_type].ldam;
	}
	else
	{
		adj = 0;
	}

	return adj;
}

/**
 * Similar to SP_level_dam_adjust(), but use LEVEL_DAMAGE macro to adjust
 * base_dam.
 * @param caster Who is casting.
 * @param spell_type Spell ID we're adjusting.
 * @param base_dam Base damage.
 * @return Adjusted damage. */
int SP_level_dam_adjust2(object *caster, int spell_type, int base_dam)
{
	float tmp_add;
	int dam_adj, level = SK_level(caster);

	/* Sanity check */
	if (level <= 0 || level > MAXLEVEL)
	{
		LOG(llevBug, "SP_level_dam_adjust2(): object %s has invalid level %d\n", query_name(caster, NULL), level);

		if (level <= 0)
		{
			level = 1;
		}
		else
		{
			level = MAXLEVEL;
		}
	}

	/* get a base damage when we don't have one from caller */
	if (base_dam == -1)
	{
		base_dam = spells[spell_type].bdam;
	}

	if ((tmp_add = LEVEL_DAMAGE(level / 3) - 0.75f) < 0)
	{
		tmp_add = 0;
	}

	dam_adj = (sint16) ((float) base_dam * (LEVEL_DAMAGE(level) + tmp_add));

	return dam_adj;
}

/**
 * Adjust the strength of the spell based on level.
 * @param caster Who is casting.
 * @param spell_type Spell ID we're adjusting.
 * @return Adjusted strength. */
int SP_level_strength_adjust(object *caster, int spell_type)
{
	int level = casting_level(caster, SK_level(caster), spell_type);
	int adj = (level - spells[spell_type].level);

	if (adj < 0)
	{
		adj = 0;
	}

	if (spells[spell_type].ldur)
	{
		adj /= spells[spell_type].ldur;
	}
	else
	{
		adj = 0;
	}

	return adj;
}

/**
 * Scales the spellpoint cost of a spell by it's increased effectiveness.
 * Some of the lower level spells become incredibly vicious at high
 * levels. Very cheap mass destruction. This function is intended to keep
 * the sp cost related to the effectiveness.
 * @param caster What is casting the spell.
 * @param spell_type Spell ID.
 * @return Spell points cost. */
int SP_level_spellpoint_cost(object *caster, int spell_type)
{
	spell *s = find_spell(spell_type);
#ifdef SPELLPOINT_LEVEL_DEPEND
	int level = casting_level(caster, SK_level(caster), spell_type), sp;

	if (spells[spell_type].spl)
	{
		sp = (int) (spells[spell_type].sp * (1.0 + (MAX(0, (float) (level - spells[spell_type].level) / (float) spells[spell_type].spl))));
	}
	else
	{
		sp = spells[spell_type].sp;
	}

	sp = (int) ((float) sp * (float) PATH_SP_MULT(caster, s));

	return MIN(sp, (spells[spell_type].sp + 50));
#else
	return s->sp * PATH_SP_MULT(caster, s);
#endif
}

/**
 * Like SP_level_spellpoint_cost(), but calculates grace cost.
 * @param caster What is casting the spell.
 * @param spell_type Spell ID.
 * @return Grace cost. */
static int SP_level_gracepoint_cost(object *caster, int spell_type)
{
	spell *s = find_spell(spell_type);
#ifdef SPELLPOINT_LEVEL_DEPEND
	int level = casting_level(caster, SK_level(caster), spell_type), grace;

	if (spells[spell_type].spl)
	{
		grace = (int) (spells[spell_type].sp * (1.0 + (MAX(0, (float) (level-spells[spell_type].level) / (float) spells[spell_type].spl))));
	}
	else
	{
		grace = spells[spell_type].sp;
	}

	grace = (int) ((float) grace * (float) PATH_SP_MULT(caster, s));

	return MIN(grace, (spells[spell_type].sp + 50));
#else
	return s->sp * PATH_SP_MULT(caster, s);
#endif
}

/**
 * This is an implementation of the swarm spell. It was written for
 * meteor swarm, but it could be used for any swarm. A swarm spell is a
 * special type of object that casts swarms of other types of spells.
 * Which spell it casts is flexible. It fires the spells from a set of
 * squares surrounding the caster, in a given direction.
 * @param op The spell effect. */
void move_swarm_spell(object *op)
{
	int cardinal_adjust[9] = {-3, -2, -1, 0, 0, 0, 1, 2, 3};
	int diagonal_adjust[10] = {-3, -2, -2, -1, 0, 0, 1, 2, 2, 3};
	int xt, yt, basedir, adjustdir;
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
		/* Spray in all directions! 8) */
		basedir = rndm(1, 8);
	}

	/* New offset calculation to make swarm element distribution
	 * more uniform */
	if (op->stats.hp)
	{
		if (basedir & 1)
		{
			adjustdir = cardinal_adjust[rndm(0, 8)];
		}
		else
		{
			adjustdir = diagonal_adjust[rndm(0, 9)];
		}
	}
	/* Fire the last one from forward. */
	else
	{
		adjustdir = 0;
	}

	target_x = op->x + freearr_x[absdir(basedir + adjustdir)];
	target_y = op->y + freearr_y[absdir(basedir + adjustdir)];

	/* Back up one space so we can hit point-blank targets, but this
	 * necessitates extra get_map_from_coord check below */
	origin_x = target_x - freearr_x[basedir];
	origin_y = target_y - freearr_y[basedir];

	xt = (int) origin_x;
	yt = (int) origin_y;

	if (!get_map_from_coord(op->map, &xt, &yt))
	{
		return;
	}

	if (!wall(op->map, target_x, target_y))
	{
		fire_arch_from_position(op, op, origin_x, origin_y, basedir, op->other_arch, op->stats.sp);
	}
}

/**
 * The following routine creates a swarm of objects. It actually sets up
 * a specific swarm object, which then fires off all the parts of the
 * swarm.
 * @param op Who is casting.
 * @param caster What object is casting.
 * @param dir Cast direction.
 * @param swarm_type Archetype of the swarm.
 * @param spell_type ID of the spell.
 * @param n The number to be fired.
 * @param magic Magic. */
void fire_swarm(object *op, object *caster, int dir, archetype *swarm_type, int spell_type, int n, int magic)
{
	object *tmp = get_archetype("swarm_spell");

	tmp->x = op->x;
	tmp->y = op->y;
	/* Needed so that if swarm elements kill, caster gets xp. */
	set_owner(tmp, op);
	/* Needed later, to get level dep. right.*/
	tmp->level = casting_level(caster, SK_level(caster), spell_type);
	/* Needed later, see move_swarm_spell */
	tmp->stats.sp = spell_type;

	tmp->magic = magic;
	/* n in swarm */
	tmp->stats.hp = n;
	/* The archetype of the things to be fired */
	tmp->other_arch = swarm_type;
	tmp->direction = dir;

	insert_ob_in_map(tmp, op->map, op, 0);
}

/**
 * Create an aura spell object and put it in the player's inventory.
 * @param op Who is casting.
 * @param caster What is casting.
 * @param aura_arch Archetype of the aura spell.
 * @param spell_type ID of the spell.
 * @param magic Whether to add AT_MAGIC to the aura's attacktype.
 * @return 1. */
int create_aura(object *op, object *caster, archetype *aura_arch, int spell_type)
{
	object *new_aura = arch_to_object(aura_arch);

	new_aura->stats.food = spells[spell_type].bdur + 10 * SP_level_strength_adjust(caster, spell_type);
	new_aura->stats.dam = spells[spell_type].bdam + SP_level_dam_adjust(caster, spell_type);
	set_owner(new_aura, op);

	if (new_aura->owner)
	{
		new_aura->chosen_skill = op->chosen_skill;

		if (new_aura->chosen_skill)
		{
			new_aura->exp_obj = op->chosen_skill->exp_obj;
		}
	}

	new_aura->level = SK_level(caster);
	insert_ob_in_ob(new_aura, op);
	return 1;
}

/**
 * Attempts to find the spell name in ::spells.
 * @param op Object trying to cast the spell.
 * @param spname Spell name to find.
 * @return -1 if it doesn't exist or if the object cannot cast that
 * spell, the spell ID otherwise. */
int look_up_spell_by_name(object *op, const char *spname)
{
	int numknown, spnum, plen, spellen, i;

	if (spname == NULL)
	{
		return -1;
	}

	if (op == NULL || QUERY_FLAG(op, FLAG_WIZ))
	{
		numknown = NROFREALSPELLS;
	}
	else
	{
		numknown = CONTR(op)->nrofknownspells;
	}

	plen = strlen(spname);

	for (i = 0; i < numknown; i++)
	{
		if (op == NULL || QUERY_FLAG(op, FLAG_WIZ))
		{
			spnum = i;
		}
		else
		{
			spnum = CONTR(op)->known_spells[i];
		}

		spellen = strlen(spells[spnum].name);

		if (strncmp(spname, spells[spnum].name, MIN(spellen, plen)) == 0)
		{
			return spnum;
		}
	}

	return -1;
}

/**
 * Puts a monster named monstername near by op. This creates the
 * treasures for the monsters, and also deals with multipart monsters
 * properly.
 * @param op Victim.
 * @param monstername Monster's archetype name. */
void put_a_monster(object *op, const char *monstername)
{
	object *tmp, *head = NULL, *prev = NULL;
	archetype *at;
	int dir;

	/* Find a free square nearby.
	 * First we check the closest square for free squares */
	if ((at = find_archetype(monstername)) == NULL)
	{
		return;
	}

	dir = find_first_free_spot(at, op->map, op->x, op->y);

	if (dir != -1)
	{
		/* This is basically grabbed for generate monster. */
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
			{
				head = tmp;
			}

			prev = tmp;
			at = at->more;
		}

		if (head->randomitems)
		{
			create_treasure(head->randomitems, head, GT_INVISIBLE, op->level ? op->level : op->map->difficulty, T_STYLE_UNSET, ART_CHANCE_UNSET, 0, NULL);
		}

		insert_ob_in_map(head, op->map, op, 0);

		/* Thought it'd be cool to insert a burnout, too. */
		tmp = get_archetype("burnout");
		tmp->map = op->map;
		tmp->x = op->x + freearr_x[dir];
		tmp->y = op->y + freearr_y[dir];
		insert_ob_in_map(tmp, op->map, op, 0);
	}
}

/**
 * The priest points to a creature and causes a 'godly curse' to descend.
 * @param op Who is casting.
 * @param caster What object is casting.
 * @param type ID of the spell to cast.
 * @retval 0 Spell had no effect.
 * @retval 1 Something was affected by the spell. */
int cast_smite_spell(object *op, object *caster, int type)
{
	object *effect, *target = NULL;
	object *god = find_god(determine_god(op));

	if (op->type == PLAYER)
	{
		target = CONTR(op)->target_object;
	}
	else if (op->enemy)
	{
		target = op->enemy;
	}

	/* if we don't worship a god, or target a creature
	 * of our god, the spell will fail. */
	if (!target || QUERY_FLAG(target, FLAG_CAN_REFL_SPELL) || !god || (target->title && !strcmp(target->title, god->name)) || (target->race && strstr(target->race, god->race)))
	{
		new_draw_info(NDI_UNIQUE, 0,op, "Your request is unheeded.");
		return 0;
	}

	if (spellarch[type])
	{
		effect = arch_to_object(spellarch[type]);
	}
	else
	{
		return 0;
	}

	/* Tailor the effect by priest level and worshipped God */
	effect->level = casting_level(caster, SK_level(caster), type);

	/* Size of the area of destruction */
	effect->stats.hp = spells[type].bdur + SP_level_strength_adjust(caster, type);
	/* How much woe to inflict :) */
	effect->stats.dam = spells[type].bdam + SP_level_dam_adjust(caster, type);

	if (effect->stats.dam < 0)
	{
		effect->stats.dam = 127;
	}

	effect->stats.maxhp = effect->count;
	set_owner(effect, op);

	/* Ok, tell it where to be, and insert! */
	effect->x = target->x;
	effect->y = target->y;
	insert_ob_in_map(effect, op->map, op, 0);

	return 1;
}

/**
 * Summons hostile monsters and places them in nearby squares.
 * @param op The summoner.
 * @param n Number of monsters.
 * @param monstername Name of the monster to summon, should be a valid
 * archetype.
 * @return Number of monsters actually put on the map. */
int summon_hostile_monsters(object *op, int n, const char *monstername)
{
	int i;

	for (i = 0; i < n; i++)
	{
		put_a_monster(op, monstername);
	}

	return n;
}
