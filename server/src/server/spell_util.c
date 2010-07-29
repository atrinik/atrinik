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
 * Spell related helper functions. */

#include <global.h>

/**
 * Array of pointers to archetypes used by the spells for quick
 * access. */
archetype *spellarch[NROFREALSPELLS];

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

	LOG(llevDebug, "Initializing spells... ");
	init_spells_done = 1;

	for (i = 0; i < NROFREALSPELLS; i++)
	{
		char spellname[MAX_BUF], tmpresult[MAX_BUF];
		archetype *at;

		replace(spells[i].name, " ", "_", tmpresult, sizeof(spellname));
		snprintf(spellname, sizeof(spellname), "spell_%s", tmpresult);

		if ((at = find_archetype(spellname)))
		{
			object *tmp = arch_to_object(at);
			const char *value;

			if ((value = object_get_value(tmp, "spell_type")))
			{
				spells[i].type = !strcmp(value, "wizard") ? SPELL_TYPE_WIZARD : SPELL_TYPE_PRIEST;
			}

			if ((value = object_get_value(tmp, "spell_level")))
			{
				spells[i].level = atoi(value);
			}

			if ((value = object_get_value(tmp, "spell_cost")))
			{
				spells[i].sp = atoi(value);
			}

			if ((value = object_get_value(tmp, "spell_time")))
			{
				spells[i].time = atoi(value);
			}

			if ((value = object_get_value(tmp, "spell_scrolls")))
			{
				spells[i].charges = atoi(value);
			}

			if ((value = object_get_value(tmp, "spell_charges")))
			{
				spells[i].charges = atoi(value);
			}

			if ((value = object_get_value(tmp, "spell_range")))
			{
				spells[i].range = atoi(value);
			}

			if ((value = object_get_value(tmp, "spell_value_mul")))
			{
				spells[i].value_mul = atof(value);
			}

			if ((value = object_get_value(tmp, "spell_bdam")))
			{
				spells[i].bdam = atoi(value);
			}

			if ((value = object_get_value(tmp, "spell_bdur")))
			{
				spells[i].bdur = atoi(value);
			}

			if ((value = object_get_value(tmp, "spell_ldam")))
			{
				spells[i].ldam = atoi(value);
			}

			if ((value = object_get_value(tmp, "spell_ldur")))
			{
				spells[i].ldur = atoi(value);
			}

			if ((value = object_get_value(tmp, "spell_spl")))
			{
				spells[i].spl = atoi(value);
			}

			if ((value = object_get_value(tmp, "spell_archname")))
			{
				spells[i].archname = strdup_local(value);
			}
		}

		if (spells[i].archname)
		{
			if ((spellarch[i] = find_archetype(spells[i].archname)) == NULL)
			{
				LOG(llevError, "ERROR: Spell %s needs arch %s, your archetypes file is out of date.\n", spells[i].name, spells[i].archname);
			}
		}
		else
		{
			spellarch[i] = NULL;
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
		if (!settings.dumparg)
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

			LOG(llevInfo, "%d: %s: %s: %s\n", i, spells[i].name, (name1 ? name1 : "null"), (name2 ? name2 : "null"));
		}
		else if (!strcmp(settings.dumparg, "all") || !strcmp(settings.dumparg, spells[i].name))
		{
			int j;
			object *caster, *tmp = NULL;

			LOG(llevInfo, "\nInformation about '%s' (ID: %d):\n", spells[i].name, i);
			caster = get_object();

			if (spellarch[i])
			{
				tmp = arch_to_object(spellarch[i]);
			}

			for (j = 1; j <= MAXLEVEL; j++)
			{
				caster->level = j;
				LOG(llevInfo, " Level: %3d, Mana: %4d, Dam: %4d, Dam2: %4d\n", j, SP_level_spellpoint_cost(caster, i, -1), SP_level_dam_adjust(caster, i, -1), tmp ? SP_level_dam_adjust(caster, i, tmp->stats.dam) : 0);
			}

			if (strcmp(settings.dumparg, "all"))
			{
				exit(0);
			}
		}
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
	if (spelltype < 0 || spelltype >= NROFREALSPELLS)
	{
		return NULL;
	}

	return &spells[spelltype];
}

/**
 * Checks to see if player knows the spell.
 * @param op Object we're checking.
 * @param spell_type Spell ID.
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
	shstr *godname = NULL;
	object *target = NULL;
	int success = 0, duration, spell_cost = 0;

	if (!s)
	{
		LOG(llevBug, "BUG: cast_spell(): Unknown spell: %d\n", type);
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
		/* If spellNPC, this usually comes from a script,
		 * and caster is the NPC and op the target. */
		target = op;
		op = caster;
	}
	else
	{
		/* It looks like the only properties we ever care about from the casting
		 * object (caster) is spell paths and level. */
		object *cast_op = op;

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

		/* No magic and not a prayer. */
		if (MAP_NOMAGIC(cast_op->map) && spells[type].type == SPELL_TYPE_WIZARD)
		{
			new_draw_info(NDI_UNIQUE, op, "Powerful countermagic cancels all spellcasting here!");
			return 0;
		}

		/* No prayer and a prayer. */
		if (MAP_NOPRIEST(cast_op->map) && spells[type].type == SPELL_TYPE_PRIEST)
		{
			new_draw_info(NDI_UNIQUE, op, "Powerful countermagic cancels all prayer spells here!");
			return 0;
		}

		/* No harm spell and not town safe. */
		if (MAP_NOHARM(cast_op->map) && !(spells[type].flags & SPELL_DESC_TOWN))
		{
			new_draw_info(NDI_UNIQUE, op, "Powerful countermagic cancels all harmful magic here!");
			return 0;
		}

		if (op->type == PLAYER)
		{
			CONTR(op)->praying = 0;

			/* Cancel player spells which are denied, but only real spells (not
			 * potions, wands, etc). */
			if (item == spellNormal)
			{
				if (caster->path_denied & s->path)
				{
					new_draw_info(NDI_UNIQUE, op, "It is denied for you to cast that spell.");
					return 0;
				}

				if (!(QUERY_FLAG(op, FLAG_WIZ)))
				{
					if (spells[type].type == SPELL_TYPE_WIZARD && op->stats.sp < SP_level_spellpoint_cost(caster, type, -1))
					{
						new_draw_info(NDI_UNIQUE, op, "You don't have enough mana.");
						return 0;
					}

					if (spells[type].type == SPELL_TYPE_PRIEST && op->stats.grace < SP_level_spellpoint_cost(caster, type, -1))
					{
						new_draw_info(NDI_UNIQUE, op, "You don't have enough grace.");
						return 0;
					}
				}
			}

			/* If it a prayer, grab the player's god - if we have none, we
			 * can't cast, except for potions. */
			if (spells[type].type == SPELL_TYPE_PRIEST && item != spellPotion)
			{
				if ((godname = determine_god(op)) == shstr_cons.none)
				{
					new_draw_info(NDI_UNIQUE, op, "You need a deity to cast a prayer!");
					return 0;
				}
			}
		}

		/* If it is an ability, assume that the designer of the archetype
		 * knows what they are doing. */
		if (item == spellNormal && !ability && SK_level(caster) < s->level && !QUERY_FLAG(op, FLAG_WIZ))
		{
			new_draw_info(NDI_UNIQUE, op, "You lack enough skill to cast that spell.");
			return 0;
		}

		if (item == spellPotion)
		{
			/* If the potion casts a self spell, don't use the facing
			 * direction. */
			if (spells[type].flags & SPELL_DESC_SELF)
			{
				target = op;
				dir = 0;
			}
		}
		else if (find_target_for_spell(op, &target, spells[type].flags) == 0)
		{
			new_draw_info_format(NDI_UNIQUE, op, "You can't cast that spell on %s!", target ? target->name : "yourself");
			return 0;
		}

		/* If valid target is not in range for selected spell, skip casting. */
		if (target)
		{
			rv_vector rv;

			if (!get_rangevector_from_mapcoords(op->map, op->x, op->y, target->map, target->x, target->y, &rv, RV_DIAGONAL_DISTANCE) || rv.distance > (unsigned int) spells[type].range)
			{
				new_draw_info(NDI_UNIQUE, op, "Your target is out of range!");
				return 0;
			}
		}

		if (op->type == PLAYER && target == op && CONTR(op)->target_object != op)
		{
			new_draw_info(NDI_UNIQUE, op, "You auto-target yourself with this spell!");
		}

		if (!ability && ((s->type == SPELL_TYPE_WIZARD && blocks_magic(op->map, op->x, op->y)) || (s->type == SPELL_TYPE_PRIEST && blocks_cleric(op->map, op->x, op->y))))
		{
			if (op->type != PLAYER)
			{
				return 0;
			}

			if (s->type == SPELL_TYPE_PRIEST)
			{
				new_draw_info_format(NDI_UNIQUE, op, "This ground is unholy! %s ignores you.", godname);
			}
			else
			{
				switch (CONTR(op)->shoottype)
				{
					case range_magic:
						new_draw_info(NDI_UNIQUE, op, "Something blocks your spellcasting.");
						break;

					case range_wand:
						new_draw_info(NDI_UNIQUE, op, "Something blocks the magic of your wand.");
						break;

					case range_rod:
						new_draw_info(NDI_UNIQUE, op, "Something blocks the magic of your rod.");
						break;

					case range_horn:
						new_draw_info(NDI_UNIQUE, op, "Something blocks the magic of your horn.");
						break;

					case range_scroll:
						new_draw_info(NDI_UNIQUE, op, "Something blocks the magic of your scroll.");
						break;

					default:
						break;
				}
			}

			return 0;
		}

		if (item == spellNormal && op->type == PLAYER)
		{
			/* Chance to fumble the spell by too low wisdom. */
			if (s->type == SPELL_TYPE_PRIEST && rndm(0, 99) < s->level / (float) MAX(1, op->chosen_skill->level) * cleric_chance[op->stats.Wis])
			{
				play_sound_player_only(CONTR(op), CMD_SOUND_EFFECT, "missspell.ogg", 0, 0, 0, 0);
				new_draw_info(NDI_UNIQUE, op, "You fumble the prayer because your wisdom is low.");

				/* Shouldn't happen... */
				if (s->sp == 0)
				{
					return 0;
				}

				return rndm(1, SP_level_spellpoint_cost(caster, type, -1));
			}

			if (s->type == SPELL_TYPE_WIZARD)
			{
				int failure = rndm(0, 199) - CONTR(op)->encumbrance + op->chosen_skill->level - s->level + 35;

				if (failure < 0)
				{
					new_draw_info(NDI_UNIQUE, op, "You bungle the spell because you have too much heavy equipment in use.");
					return rndm(0, SP_level_spellpoint_cost(caster, type, -1));
				}
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

				default:
					break;
			}
		}
	}

	/* A last sanity check: are caster and target *really* valid? */
	if ((caster && !OBJECT_ACTIVE(caster)) || (target && !OBJECT_ACTIVE(target)))
	{
		return 0;
	}

	/* We need to calculate the spell point cost before the spell actually
	 * does something, otherwise the following can happen (example):
	 * Player has 7 mana left, kills a monster with magic bullet (which costs 7
	 * mana) while standing right next to it, magic bullet kills the monster before
	 * we reach the return here, player levels up, cost of magic bullet increases
	 * from 7 to 8. So the function would return 8 instead of 7, resulting in the
	 * player's mana being -1. */
	if (item != spellNPC)
	{
		spell_cost = SP_level_spellpoint_cost(caster, type, -1);
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

		case SP_IDENTIFY:
			success = cast_identify(target, SK_level(caster), NULL, IDENTIFY_MODE_NORMAL);
			break;

		/* Spells after this use direction and not a target */
		case SP_ICESTORM:
		case SP_FIRESTORM:
			success = cast_cone(op, caster, dir, duration, type, spellarch[type]);
			break;

		case SP_PROBE:
			if (!dir)
			{
				examine(op, op);
				success = 1;
			}
			else
			{
				success = fire_arch_from_position(op, caster, op->x, op->y, dir, spellarch[type], type, NULL);
			}

			break;

		case SP_BULLET:
		case SP_CAUSE_LIGHT:
		case SP_MAGIC_MISSILE:
			success = fire_arch_from_position(op, caster, op->x, op->y, dir, spellarch[type], type, target);
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
			success = finger_of_death(op, target);
			break;

		case SP_POISON_FOG:
		case SP_METEOR:
		case SP_ASTEROID:
			success = fire_arch_from_position(op, caster, op->x, op->y, dir, spellarch[type], type, NULL);
			break;

		case SP_METEOR_SWARM:
			success = 1;
			fire_swarm(op, caster, dir, spellarch[type], SP_METEOR, 3, 0);
			break;

		case SP_FROST_NOVA:
			success = 1;
			fire_swarm(op, caster, dir, spellarch[type], SP_ASTEROID, 3, 0);
			break;

		case SP_BULLET_SWARM:
			success = 1;
			fire_swarm(op, caster, dir, spellarch[type], SP_BULLET, 5, 0);
			break;

		case SP_BULLET_STORM:
			success = 1;
			fire_swarm(op, caster, dir, spellarch[type], SP_BULLET, 3, 0);
			break;

		case SP_DESTRUCTION:
			success = cast_destruction(op, caster, 5 + op->stats.Int, AT_MAGIC);
			break;

		case SP_BOMB:
			success = create_bomb(op, caster, dir, type);
			break;

		case SP_TRANSFORM_WEALTH:
			success = cast_transform_wealth(op);
			break;

		case SP_RAIN_HEAL:
		case SP_PARTY_HEAL:
			success = cast_heal_around(op, SK_level(caster), type);
			break;

		case SP_FROSTBOLT:
		case SP_FIREBOLT:
		case SP_LIGHTNING:
		case SP_FORKED_LIGHTNING:
		case SP_NEGABOLT:
			success = fire_bolt(op, caster, dir, type);
			break;

		default:
			LOG(llevBug, "BUG: cast_spell(): Invalid invalid spell: %d\n", type);
			break;
	}

	play_sound_map(op->map, CMD_SOUND_EFFECT, spells[type].sound, op->x, op->y, 0, 0);

	if (item == spellNPC)
	{
		return success;
	}

	return success ? spell_cost : 0;
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

	if (dir && blocked(op, mt, xt, yt, op->terrain_flag))
	{
		new_draw_info(NDI_UNIQUE, op, "Something is in the way.\nYou cast it at your feet.");
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
	object *tmp;
	int w;

	if (!spellarch[type])
	{
		return 0;
	}

	tmp = arch_to_object(spellarch[type]);

	if (!tmp)
	{
		return 0;
	}

	if (!dir)
	{
		new_draw_info(NDI_UNIQUE, op, "You can't fire that at yourself!");
		return 0;
	}

	tmp->stats.dam = (sint16) SP_level_dam_adjust(caster, type, tmp->stats.dam);
	tmp->stats.hp = spells[type].bdur + SP_level_strength_adjust(caster, type);

	tmp->direction = dir;
	tmp->x = op->x + DIRX(tmp);
	tmp->y = op->y + DIRY(tmp);

	if (QUERY_FLAG(tmp, FLAG_IS_TURNABLE))
	{
		SET_ANIMATION(tmp, (NUM_ANIMATIONS(tmp) / NUM_FACINGS(tmp)) * tmp->direction);
	}

	set_owner(tmp, op);
	tmp->level = SK_level(caster);
	w = wall(op->map, tmp->x, tmp->y);

	if (w && !QUERY_FLAG(tmp, FLAG_REFLECTING))
	{
		return 0;
	}

	if (w || reflwall(op->map, tmp->x, tmp->y, tmp))
	{
		tmp->direction = absdir(tmp->direction + 4);
		tmp->x = op->x + DIRX(tmp);
		tmp->y = op->y + DIRY(tmp);
	}

	if (wall(op->map, tmp->x, tmp->y))
	{
		new_draw_info(NDI_UNIQUE, op, "There is something in the way.");
		return 0;
	}

	tmp = insert_ob_in_map(tmp, op->map, op, 0);

	if (tmp)
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
int fire_arch_from_position(object *op, object *caster, sint16 x, sint16 y, int dir, archetype *at, int type, object *target)
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
	tmp->stats.dam = (sint16) SP_level_dam_adjust(caster, type, tmp->stats.dam);
	tmp->stats.hp = spells[type].bdur + SP_level_strength_adjust(caster, type);
	tmp->x = x, tmp->y = y;
	tmp->direction = dir;
	tmp->stats.grace = tmp->last_sp;
	tmp->stats.maxgrace = 60 + (RANDOM() % 12);

	if (target)
	{
		tmp->enemy = target;
		tmp->enemy_count = target->count;
	}

	if (get_owner(op) != NULL)
	{
		copy_owner(tmp, op);
	}
	else
	{
		set_owner(tmp, op);
	}

	tmp->level = SK_level(caster);

	if (QUERY_FLAG(tmp, FLAG_IS_TURNABLE))
	{
		SET_ANIMATION(tmp, (NUM_ANIMATIONS(tmp) / NUM_FACINGS(tmp)) * dir);
	}

	if ((tmp = insert_ob_in_map(tmp, op->map, op, 0)) == NULL)
	{
		return 1;
	}

	move_fired_arch(tmp);
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

		tmp->level = SK_level(caster);
		tmp->x = x, tmp->y = y;

		if (dir)
		{
			tmp->stats.sp = dir;
		}
		else
		{
			tmp->stats.sp = i;
		}

		tmp->stats.hp = strength;
		tmp->stats.dam = (sint16) SP_level_dam_adjust(caster, spell_type, tmp->stats.dam);
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
		hit_map(op, 0, 0);
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
	op->stats.food |= hit_map(op, 0, 1);

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

		copy_object(tmp, new_bolt, 0);
		new_bolt->stats.food = 0;
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
	object *tmp;

	if (!(m = get_map_from_coord(m, &x, &y)))
	{
		return 0;
	}

	for (tmp = GET_MAP_OB_LAYER(m, x, y, 5); tmp && tmp->layer == 6; tmp = tmp->above)
	{
		if (QUERY_FLAG(tmp->head ? tmp->head : tmp, FLAG_REFL_SPELL) && (rndm(0, 99)) < 90 - (sp_op->level / 10))
		{
			return 1;
		}
	}

	return 0;
}

/**
 * Moves bolt 'op'. Basically, it just advances a space, and checks for
 * various things that may stop it.
 * @param op The bolt object moving. */
void move_bolt(object *op)
{
	int w, r;
	object *tmp;

	if (--(op->stats.hp) < 0)
	{
		destruct_ob(op);
		return;
	}

	if (!op->direction)
	{
		return;
	}

	if (blocks_magic(op->map, op->x + DIRX(op), op->y + DIRY(op)))
	{
		return;
	}

	check_fired_arch(op);

	if (!OBJECT_ACTIVE(op))
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

		update_turn_face(op);
		return;
	}

	if (op->stats.food || !op->stats.hp)
	{
		return;
	}

	op->stats.food = 1;

	/* Create a copy of this object and put it ahead */
	tmp = get_object();
	copy_object(op, tmp, 0);
	tmp->speed_left = -0.1f;
	tmp->x += DIRX(tmp);
	tmp->y += DIRY(tmp);

	if (!insert_ob_in_map(tmp, op->map, op, 0))
	{
		return;
	}

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

/**
 * Causes an object to explode, eg, a firebullet, poison cloud ball, etc.
 * @param op The object to explode. */
void explode_object(object *op)
{
	tag_t op_tag = op->count;
	object *tmp;
	int type;

	play_sound_map(op->map, CMD_SOUND_EFFECT, "explosion.ogg", op->x, op->y, 0, 0);

	if (op->other_arch == NULL)
	{
		LOG(llevBug, "BUG: explode_object(): op %s without other_arch\n", query_name(op, NULL));
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		return;
	}

	tmp = arch_to_object(op->other_arch);
	type = tmp->stats.sp;

	if (!type)
	{
		type = op->stats.sp;
	}

	copy_owner(tmp, op);
	cast_cone(op, op, 0, spells[type].bdur, type, op->other_arch);
	hit_map(op, 0, 0);

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
	int dam;

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

	if (op->stats.sp == SP_MAGIC_MISSILE)
	{
		rv_vector rv;

		if (!OBJECT_VALID(op->enemy, op->enemy_count) || !get_rangevector(op, op->enemy, &rv, 0))
		{
			remove_ob(op);
			check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
			return;
		}

		op->direction = rv.direction;
		update_turn_face(op);
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
 * Detect target for casting a spell.
 * @param op Caster.
 * @param[out] target Will contain target for the spell we're casting.
 * @param flags Spell flags.
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

				/* Can't cast unfriendly spells on friendly NPCs, but we set target
				 * so the message player gets is accurate. */
				if (flags & SPELL_DESC_ENEMY)
				{
					*target = tmp;
					return 0;
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
	/* A monster or rune/firewall/etc */
	else
	{
		if ((flags & SPELL_DESC_SELF) && !(flags & (SPELL_DESC_ENEMY | SPELL_DESC_FRIENDLY)))
		{
			*target = op;
			return 1;
		}
		else if ((flags & SPELL_DESC_ENEMY) && op->enemy && OBJECT_ACTIVE(op->enemy) && op->enemy->count == op->enemy_count)
		{
			*target = op->enemy;
			return 1;
		}
		else
		{
			*target = op;
			return 1;
		}
	}

	/* Invalid target/spell or whatever */
	return 0;
}

/**
 * Returns adjusted damage based on the caster.
 * @param caster Who is casting.
 * @param spell_type Spell ID we're adjusting.
 * @param base_dam Base damage.
 * @return Adjusted damage. */
int SP_level_dam_adjust(object *caster, int spell_type, int base_dam)
{
	int level = SK_level(caster);
	sint16 dam;

	/* Sanity check */
	if (level <= 0 || level > MAXLEVEL)
	{
		LOG(llevBug, "SP_level_dam_adjust(): object %s has invalid level %d\n", query_name(caster, NULL), level);

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

	dam = (sint16) ((float) base_dam * LEVEL_DAMAGE(level) * PATH_DMG_MULT(caster, find_spell(spell_type)));

	return rndm(dam / 1.42857f + 1, dam);
}

/**
 * Adjust the strength of the spell based on level.
 * @param caster Who is casting.
 * @param spell_type Spell ID we're adjusting.
 * @return Adjusted strength. */
int SP_level_strength_adjust(object *caster, int spell_type)
{
	int level = SK_level(caster);
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
 * @param caster_level Level of caster. If -1, will use SK_level() to
 * determine caster's level.
 * @return Spell points cost. */
int SP_level_spellpoint_cost(object *caster, int spell_type, int caster_level)
{
	spell *s = find_spell(spell_type);
	int level = (caster_level == -1 ? SK_level(caster) : caster_level), sp;

	if (spells[spell_type].spl)
	{
		sp = (int) (spells[spell_type].sp * (1.0 + (MAX(0, (float) (level - spells[spell_type].level) / (float) spells[spell_type].spl))));
	}
	else
	{
		sp = spells[spell_type].sp;
	}

	return (int) ((float) sp * (float) PATH_SP_MULT(caster, s));
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
		basedir = get_random_dir();
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
		fire_arch_from_position(op, op, origin_x, origin_y, basedir, op->other_arch, op->stats.sp, NULL);
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
	tmp->level = SK_level(caster);
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
