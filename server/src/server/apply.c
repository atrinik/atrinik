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
 * Handles objects being applied, and their effect. */

#include <global.h>

/* need math lib for double-precision and pow() in dragon_eat_flesh() */
#include <math.h>

static int is_legal_2ways_exit(object* op, object *exit);

/**
 * 'victim' moves onto 'trap' (trap has FLAG_WALK_ON or FLAG_FLY_ON set) or
 * 'victim' leaves 'trap' (trap has FLAG_WALK_OFF or FLAG_FLY_OFF) set.
 *
 * I added the flags parameter to give the single events more information
 * about whats going on:
 *
 * Most important is the "MOVE_APPLY_VANISHED" flag.
 * If set, a object has left a tile but "vanished" and not moved (perhaps
 * it exploded or something). This means that some events are not
 * triggered like trapdoors or teleporter traps for example which have a
 * "FLY/MOVE_OFF" set. This will avoid that they touch invalid objects.
 * @param trap Object victim moved on.
 * @param victim The object that moved on trap.
 * @param originator Player, monster or other object that caused 'victim'
 * to move onto 'trap'. Will receive messages caused by this action. May
 * be NULL, however, some types of traps require an originator to
 * function.
 * @param flags Flags. */
void move_apply(object *trap, object *victim, object *originator, int flags)
{
	static int recursion_depth = 0;

	/* move_apply() is the most likely candidate for causing unwanted and
	 * possibly unlimited recursion. */
	/* The following was changed because it was causing perfeclty correct
	 * maps to fail.  1)  it's not an error to recurse:
	 * rune detonates, summoning monster.  monster lands on nearby rune.
	 * nearby rune detonates.  This sort of recursion is expected and
	 * proper.  This code was causing needless crashes. */
	if (recursion_depth >= 500)
	{
		LOG(llevDebug, "WARNING: move_apply(): aborting recursion [trap arch %s, name %s; victim arch %s, name %s]\n", trap->arch->name, trap->name, victim->arch->name, victim->name);
		return;
	}

	if (trap->head)
	{
		trap = trap->head;
	}

	/* Trigger the TRIGGER event */
	if (trigger_event(EVENT_TRIGGER, victim, trap, originator, NULL, 0, 0, 0, SCRIPT_FIX_NOTHING))
	{
		return;
	}

	recursion_depth++;

	switch (trap->type)
	{
		/* these objects can trigger other objects connected to them.
		 * We need to check them at map loading time and other special
		 * events to be sure to have a 100% working map state. */
		case BUTTON:
		case PEDESTAL:
			update_button(trap);
			break;

		case TRIGGER_BUTTON:
		case TRIGGER_PEDESTAL:
		case TRIGGER_ALTAR:
			check_trigger(trap, victim);
			break;

		case CHECK_INV:
			check_inv(victim, trap);
			break;

		/* these objects trigger to but they are "instant".
		 * We don't need to check them when loading. */
		case ALTAR:
			/* sacrifice victim on trap */
			apply_altar(trap, victim, originator);
			break;

		case CONVERTER:
			if (!(flags & MOVE_APPLY_VANISHED))
			{
				convert_item(victim, trap);
			}

			break;

		case PLAYERMOVER:
			break;

		/* should be walk_on/fly_on only */
		case SPINNER:
			if (victim->direction)
			{
				if ((victim->direction = victim->direction + trap->direction) > 8)
				{
					victim->direction = (victim->direction % 8) + 1;
				}

				update_turn_face(victim);
			}

			break;

		case DIRECTOR:
			if (victim->direction)
			{
				victim->direction = trap->direction;
				update_turn_face(victim);
			}

			break;

		/* no need to hit anything */
		case MMISSILE:
			if (IS_LIVE(victim) && !(flags&MOVE_APPLY_VANISHED))
			{
				tag_t trap_tag = trap->count;
				hit_player(victim, trap->stats.dam, trap, AT_MAGIC);

				if (!was_destroyed(trap, trap_tag))
				{
					remove_ob(trap);
				}

				check_walk_off(trap, NULL, MOVE_APPLY_VANISHED);
			}

			break;

		case THROWN_OBJ:
			if (trap->inv == NULL || (flags & MOVE_APPLY_VANISHED))
			{
				break;
			}

		/* fallthrough */
		case ARROW:
			/* bad bug: monster throw a object, make a step forwards, step on object ,
			 * trigger this here and get hit by own missile - and will be own enemy.
			 * Victim then is his own enemy and will start to kill herself (this is
			 * removed) but we have not synced victim and his missile. To avoid senseless
			 * action, we avoid hits here */
			if ((IS_LIVE(victim) && trap->speed) && trap->owner != victim)
			{
				hit_with_arrow(trap, victim);
			}

			break;

		case CONE:
		case LIGHTNING:
			break;

		case BULLET:
			if ((QUERY_FLAG(victim, FLAG_NO_PASS) || IS_LIVE(victim)) && !(flags & MOVE_APPLY_VANISHED))
			{
				check_fired_arch(trap);
			}

			break;

		case TRAPDOOR:
		{
			int max, sound_was_played;
			object *ab;

			if ((flags & MOVE_APPLY_VANISHED))
			{
				break;
			}

			if (!trap->value)
			{
				sint32 tot;

				for (ab = trap->above, tot = 0; ab != NULL; ab = ab->above)
				{
					if (!QUERY_FLAG(ab, FLAG_FLYING))
					{
						tot += (ab->nrof ? ab->nrof : 1) * ab->weight + ab->carrying;
					}
				}

				if (!(trap->value = (tot > trap->weight) ? 1 : 0))
				{
					break;
				}

				SET_ANIMATION(trap, (NUM_ANIMATIONS(trap) / NUM_FACINGS(trap)) * trap->direction + trap->value);
				update_object(trap, UP_OBJ_FACE);
			}

			for (ab = trap->above, max = 100, sound_was_played = 0; --max && ab && !QUERY_FLAG(ab, FLAG_FLYING); ab = ab->above)
			{
				if (!sound_was_played)
				{
					play_sound_map(trap->map, SOUND_FALL_HOLE, NULL, trap->x, trap->y, 0, 0);
					sound_was_played = 1;
				}

				if (ab->type == PLAYER)
				{
					new_draw_info(NDI_UNIQUE, ab, "You fall into a trapdoor!");
				}

				transfer_ob(ab, EXIT_X(trap), EXIT_Y(trap), trap->last_sp, ab, trap);
			}

			break;
		}


		case PIT:
			/* Pit not open? */
			if ((flags & MOVE_APPLY_VANISHED) || trap->stats.wc > 0)
			{
				break;
			}

			play_sound_map(victim->map, SOUND_FALL_HOLE, NULL, victim->x, victim->y, 0, 0);

			if (victim->type == PLAYER)
			{
				new_draw_info(NDI_UNIQUE, victim, "You fall through the hole!\n");
			}

			transfer_ob(victim->head ? victim->head : victim, EXIT_X(trap), EXIT_Y(trap), trap->last_sp, victim, trap);

			break;

		case EXIT:
			/* If no map path specified, we assume it is the map path of the exit. */
			if (!EXIT_PATH(trap))
			{
				FREE_AND_ADD_REF_HASH(EXIT_PATH(trap), trap->map->path);
			}

			if (!(flags & MOVE_APPLY_VANISHED) && victim->type == PLAYER && EXIT_PATH(trap) && EXIT_Y(trap) != -1 && EXIT_X(trap) != -1)
			{
				/* Basically, don't show exits leading to random maps the players output. */
				if (trap->msg && strncmp(EXIT_PATH(trap), "/!", 2) && strncmp(EXIT_PATH(trap), "/random/", 8))
				{
					new_draw_info(NDI_NAVY, victim, trap->msg);
				}

				enter_exit(victim, trap);
			}

			break;

		case SHOP_MAT:
			if (!(flags & MOVE_APPLY_VANISHED))
			{
				apply_shop_mat(trap, victim);
			}

			break;

		/* Drop a certain amount of gold, and have one item identified */
		case IDENTIFY_ALTAR:
			if (!(flags & MOVE_APPLY_VANISHED))
			{
				apply_identify_altar(victim, trap, originator);
			}

			break;

		case SIGN:
			/* Only player should be able read signs */
			if (victim->type == PLAYER)
			{
				apply_sign(victim, trap);
			}

			break;

		case CONTAINER:
			if (victim->type == PLAYER)
			{
				(void) esrv_apply_container(victim, trap);
			}

			break;

		case RUNE:
			if (!(flags & MOVE_APPLY_VANISHED) && trap->level && IS_LIVE(victim))
			{
				spring_trap(trap, victim);
			}

			break;

#if 0
		/* we don't have this atm. */
		case DEEP_SWAMP:
			if (!(flags & MOVE_APPLY_VANISHED))
			{
				walk_on_deep_swamp(trap, victim);
			}

			break;
#endif

		default:
			LOG(llevDebug, "name %s, arch %s, type %d with fly/walk on/off not handled in move_apply()\n", trap->name, trap->arch->name, trap->type);
			break;
	}

	recursion_depth--;
}

/**
 * Find a special prayer marker in object's inventory.
 *
 * Special prayers are granted by gods and lost when the follower decides
 * to pray to different gods. 'Force' objects keep track of which prayers
 * are special.
 * @param op Object to search in.
 * @param spell Spell ID to find.
 * @return The marker object, NULL if not found. */
object *find_special_prayer_mark(object *op, int spell)
{
	object *tmp;

	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		if (tmp->type == FORCE && tmp->slaying && strcmp(tmp->slaying, "special prayer") == 0 && tmp->stats.sp == spell)
		{
			return tmp;
		}
	}

	return NULL;
}

/**
 * Insert a special prayer marker inside an object.
 * @param op The object to insert the marker to.
 * @param spell The spell (prayer) ID. */
static void insert_special_prayer_mark(object *op, int spell)
{
	object *force = get_archetype("force");
	force->speed = 0;
	update_ob_speed(force);
	FREE_AND_COPY_HASH(force->slaying, "special prayer");
	force->stats.sp = spell;
	insert_ob_in_ob(force, op);
}

/**
 * Make player learn a new spell.
 * @param op The player object learning the new spell.
 * @param spell Spell ID.
 * @param special_prayer Is this a special prayer? */
void do_learn_spell(object *op, int spell, int special_prayer)
{
	object *tmp = find_special_prayer_mark(op, spell);

	if (op->type != PLAYER)
	{
		LOG(llevBug, "BUG: do_learn_spell(): not a player ->%s\n", op->name);
		return;
	}

	/* Upgrade special prayers to normal prayers */
	if (check_spell_known(op, spell))
	{
		new_draw_info_format(NDI_UNIQUE, op, "You already know the spell '%s'!", spells[spell].name);

		if (special_prayer || !tmp)
		{
			LOG(llevBug, "BUG: do_learn_spell(): spell already known, but can't upgrade it\n");
			return;
		}

		remove_ob(tmp);
		return;
	}

	/* Learn new spell/prayer */
	if (tmp)
	{
		LOG(llevBug, "BUG: do_learn_spell(): spell unknown, but special prayer mark present\n");
		remove_ob(tmp);
	}

	play_sound_player_only(CONTR(op), SOUND_LEARN_SPELL, NULL, 0, 0, 0, 0);
	CONTR(op)->known_spells[CONTR(op)->nrofknownspells++] = spell;

	if (CONTR(op)->nrofknownspells == 1)
	{
		CONTR(op)->chosen_spell = spell;
	}

	/* For god-given spells the player gets a reminder-mark inserted, that
	 * this spell must be removed on changing cults! */
	if (special_prayer)
	{
		insert_special_prayer_mark(op, spell);
	}

	send_spelllist_cmd(op, spells[spell].name, SPLIST_MODE_ADD);
	new_draw_info_format(NDI_UNIQUE, op, "You have learned the spell %s!", spells[spell].name);
}

/**
 * Make player forget a spell.
 * @param op Player object to make forget the spell.
 * @param spell ID of the spell. */
void do_forget_spell(object *op, int spell)
{
	object *tmp;
	int i;

	if (op->type != PLAYER)
	{
		LOG(llevBug, "BUG: do_forget_spell(): Not a player: %s (%d).\n", query_name(op, NULL), spell);
		return;
	}

	if (!check_spell_known(op, spell))
	{
		LOG(llevBug, "BUG: do_forget_spell(): Spell %d not known.\n", spell);
		return;
	}

	play_sound_player_only(CONTR(op), SOUND_LOSE_SOME,NULL, 0, 0, 0, 0);
	new_draw_info_format(NDI_UNIQUE, op, "You lose knowledge of %s.", spells[spell].name);

	send_spelllist_cmd(op, spells[spell].name, SPLIST_MODE_REMOVE);
	tmp = find_special_prayer_mark(op, spell);

	if (tmp)
	{
		remove_ob(tmp);
	}

	for (i = 0; i < CONTR(op)->nrofknownspells; i++)
	{
		if (CONTR(op)->known_spells[i] == spell)
		{
			CONTR(op)->known_spells[i] = CONTR(op)->known_spells[--CONTR(op)->nrofknownspells];
			return;
		}
	}

	LOG(llevBug, "BUG: do_forget_spell(): Couldn't find spell %d.\n", spell);
}

/**
 * This function return true if the exit is not a two ways one or it is
 * two ways, valid exit.
 *
 * A valid two way exit means:
 *  - You can come back (there is another exit on the other side)
 *  - You are
 *     - The owner of the exit
 *     - Or in the same party as the owner
 * @note An owner in a two way exit is saved as the owner's name in the
 * field exit->name cause the field exit->owner doesn't survive in the
 * swapping (in fact the whole exit doesn't survive).
 * @param op Player to check for.
 * @param exit Exit object.
 * @return 1 if exit is not two way, 0 otherwise. */
static int is_legal_2ways_exit(object* op, object *exit)
{
	object *tmp, *exit_owner;
	player *pp;
	mapstruct *exitmap;

	/* This is not a two way, so it is legal */
	if (exit->stats.exp != 1)
	{
		return 1;
	}

	/* To know if an exit has a correspondant, we look at
	 * all the exits in destination and try to find one with same path as
	 * the current exit's position */
	if (!strncmp(EXIT_PATH (exit), settings.localdir, strlen(settings.localdir)))
	{
		exitmap = ready_map_name(EXIT_PATH (exit), MAP_NAME_SHARED|MAP_PLAYER_UNIQUE);
	}
	else
	{
		exitmap = ready_map_name(EXIT_PATH (exit), MAP_NAME_SHARED);
	}

	if (exitmap)
	{
		tmp = get_map_ob(exitmap, EXIT_X(exit), EXIT_Y(exit));

		if (!tmp)
		{
			return 0;
		}

		for ((tmp = get_map_ob(exitmap, EXIT_X(exit), EXIT_Y(exit))); tmp; tmp = tmp->above)
		{
			/* Not an exit */
			if (tmp->type != EXIT)
			{
				continue;
			}

			/* Not a valid exit */
			if (!EXIT_PATH(tmp))
			{
				continue;
			}

			/* Not in the same place */
			if ((EXIT_X(tmp) != exit->x) || (EXIT_Y(tmp) != exit->y))
			{
				continue;
			}

			/* Not in the same map */
			if (!exit->race && exit->map->path == EXIT_PATH(tmp))
			{
				continue;
			}

			/* From here we have found the exit is valid. However we do
			 * here the check of the exit owner. It is important for the
			 * town portals to prevent strangers from visiting your appartments */
			/* No owner, free for all! */
			if (!exit->race)
			{
				return 1;
			}

			exit_owner = NULL;

			for (pp = first_player; pp; pp = pp->next)
			{
				if (!pp->ob)
				{
					continue;
				}

				if (pp->ob->name != exit->race)
				{
					continue;
				}

				/* We found a player which correspond to the player name */
				exit_owner = pp->ob;
				break;
			}

			/* No more owner */
			if (!exit_owner)
			{
				return 0;
			}

			/* It is your exit */
			if (CONTR(exit_owner) == CONTR(op))
			{
				return 1;
			}

			if (exit_owner && CONTR(op) && (!CONTR(exit_owner)->party || CONTR(exit_owner)->party != CONTR(op)->party))
			{
				return 0;
			}

			return 1;
		}
	}

	return 0;
}

/**
 * Main apply handler.
 *
 * Checks for unpaid items before applying.
 * @param op ::object causing tmp to be applied.
 * @param tmp ::object being applied.
 * @param aflag Special (always apply/unapply) flags. Nothing is done
 * with them in this function - they are passed to apply_special().
 * @retval 0 Player or monster can't apply objects of that type.
 * @retval 1 Has been applied, or there was an error applying the object.
 * @retval 2 Objects of that type can't be applied if not in
 * inventory. */
int manual_apply(object *op, object *tmp, int aflag)
{
	if (tmp->head)
	{
		tmp = tmp->head;
	}

	if (op->type == PLAYER)
	{
		CONTR(op)->praying = 0;
	}

	if (QUERY_FLAG(tmp, FLAG_UNPAID) && !QUERY_FLAG(tmp, FLAG_APPLIED))
	{
		if (op->type == PLAYER)
		{
			new_draw_info(NDI_UNIQUE, op, "You should pay for it first.");
			return 1;
		}
		/* Monsters just skip unpaid items */
		else
		{
			return 0;
		}
	}

	/* Monsters must not apply random chests, nor magic_mouths with a counter */
	if (op->type != PLAYER && tmp->type == TREASURE)
	{
		return 0;
	}

	/* Trigger the APPLY event */
	if (!(aflag & AP_NO_EVENT) && trigger_event(EVENT_APPLY, op, tmp, NULL, NULL, aflag, 0, 0, SCRIPT_FIX_ACTIVATOR))
	{
		return 1;
	}

	aflag &= ~AP_NO_EVENT;

	/* Control apply by controling a set exp object level or player exp level */
	if (tmp->item_level)
	{
		int tmp_lev;

		if (tmp->item_skill)
		{
			tmp_lev = find_skill_exp_level(op, tmp->item_skill);
		}
		else
		{
			tmp_lev = op->level;
		}

		if (tmp->item_level > tmp_lev)
		{
			new_draw_info(NDI_UNIQUE, op, "The item level is too high to apply.");
			return 1;
		}
	}

	switch (tmp->type)
	{
		case HOLY_ALTAR:
			new_draw_info_format(NDI_UNIQUE, op, "You touch the %s.", tmp->name);

			if (change_skill(op, SK_PRAYING))
			{
				pray_at_altar(op, tmp);
			}
			else
			{
				new_draw_info(NDI_UNIQUE, op, "Nothing happens. It seems you miss the right skill.");
			}

			return 1;
			break;

		case HANDLE:
			new_draw_info(NDI_UNIQUE, op, "You turn the handle.");
			play_sound_map(op->map, SOUND_TURN_HANDLE, NULL, op->x, op->y, 0, 0);
			tmp->value = tmp->value ? 0 : 1;
			SET_ANIMATION(tmp, ((NUM_ANIMATIONS(tmp) / NUM_FACINGS(tmp)) * tmp->direction) + tmp->value);
			update_object(tmp, UP_OBJ_FACE);
			push_button(tmp);

			return 1;

		case TRIGGER:
			if (check_trigger(tmp, op))
			{
				new_draw_info(NDI_UNIQUE, op, "You turn the handle.");
				play_sound_map(tmp->map, SOUND_TURN_HANDLE, NULL, tmp->x, tmp->y, 0, 0);
			}
			else
			{
				new_draw_info(NDI_UNIQUE, op, "The handle doesn't move.");
			}

			return 1;

		case EXIT:
			if (op->type != PLAYER || !tmp->map)
			{
				return 0;
			}

			/* If no map path specified, we assume it is the map path of the exit. */
			if (!EXIT_PATH(tmp))
			{
				FREE_AND_ADD_REF_HASH(EXIT_PATH(tmp), tmp->map->path);
			}

			if (!EXIT_PATH(tmp) || !is_legal_2ways_exit(op, tmp) || (EXIT_Y(tmp) == -1 && EXIT_X(tmp) == -1))
			{
				new_draw_info_format(NDI_UNIQUE, op, "The %s is closed.", query_name(tmp, NULL));
			}
			else
			{
				/* Don't display messages for random maps. */
				if (tmp->msg && strncmp(EXIT_PATH(tmp), "/!", 2) && strncmp(EXIT_PATH(tmp), "/random/", 8))
				{
					new_draw_info(NDI_NAVY, op, tmp->msg);
				}

				enter_exit(op, tmp);
			}

			return 1;

		case SIGN:
			apply_sign(op, tmp);
			return 1;

		case BOOK:
			if (op->type == PLAYER)
			{
				apply_book(op, tmp);
				return 1;
			}

			return 0;

		case SKILLSCROLL:
			if (op->type == PLAYER)
			{
				apply_skillscroll(op, tmp);
				return 1;
			}

			return 0;

		case SPELLBOOK:
			if (op->type == PLAYER)
			{
				apply_spellbook(op, tmp);
				return 1;
			}

			return 0;

		case SCROLL:
			apply_scroll(op, tmp);
			return 1;

		case POTION:
			(void) apply_potion(op, tmp);
			return 1;

		case LIGHT_APPLY:
			apply_player_light(op, tmp);
			return 1;

		case LIGHT_REFILL:
			apply_player_light_refill(op, tmp);
			return 1;

		/* Eneq(@csd.uu.se): Handle apply on containers. */
		case CLOSE_CON:
			if (op->type == PLAYER)
			{
				(void) esrv_apply_container(op, tmp->env);
			}

			return 1;

		case CONTAINER:
			if (op->type == PLAYER)
			{
				(void) esrv_apply_container(op, tmp);
			}

			return 1;

		case TREASURE:
			apply_treasure(op, tmp);
			return 1;

		case WEAPON:
		case ARMOUR:
		case BOOTS:
		case GLOVES:
		case AMULET:
		case GIRDLE:
		case BRACERS:
		case SHIELD:
		case HELMET:
		case RING:
		case CLOAK:
		case WAND:
		case ROD:
		case HORN:
		case SKILL:
		case BOW:
		case SKILL_ITEM:
			/* Not in inventory */
			if (tmp->env != op)
			{
				return 2;
			}

			(void) apply_special(op, tmp, aflag);
			return 1;

		case DRINK:
		case FOOD:
		case FLESH:
			apply_food(op, tmp);
			return 1;

		case POISON:
			apply_poison(op, tmp);
			return 1;

		case SAVEBED:
			if (op->type == PLAYER)
			{
				apply_savebed(op);
				return 1;
			}

			return 0;

		case ARMOUR_IMPROVER:
			if (op->type == PLAYER)
			{
				apply_armour_improver(op, tmp);
				return 1;
			}

			return 0;

		case WEAPON_IMPROVER:
			apply_weapon_improver(op, tmp);
			return 1;

		case CLOCK:
			if (op->type == PLAYER)
			{
				timeofday_t tod;

				get_tod(&tod);
				new_draw_info_format(NDI_UNIQUE, op, "It is %d minute%s past %d o'clock %s", tod.minute + 1, ((tod.minute + 1 < 2) ? "" : "s"), ((tod.hour % (HOURS_PER_DAY / 2) == 0) ? (HOURS_PER_DAY / 2) : ((tod.hour) % (HOURS_PER_DAY / 2))), ((tod.hour >= (HOURS_PER_DAY / 2)) ? "pm" : "am"));
				return 1;
			}

			return 0;

		case POWER_CRYSTAL:
			apply_power_crystal(op, tmp);
			return 1;

		/* For lighting torches/lanterns/etc */
		case LIGHTER:
			if (op->type == PLAYER)
			{
				apply_lighter(op, tmp);
				return 1;
			}

			return 0;

		/* So the below default case doesn't execute for these objects,
		 * even if they have message. */
		case DOOR:
			return 0;

		/* Nothing from the above... but show a message if it has one. */
		default:
			if (tmp->msg)
			{
				new_draw_info(NDI_UNIQUE, op, tmp->msg);
				return 1;
			}

			return 0;
	}
}

/**
 * Living thing is applying an object.
 * @param pl ::object causing op to be applied.
 * @param op ::object being applied.
 * @param aflag Special (always apply/unapply) flags. Nothing is done
 * with them in this function - they are passed to apply_special().
 * @param quiet If 1, suppresses the "don't know how to apply" and "you
 * must get it first" messages as needed by player_apply_below(). There
 * can still be "but you are floating high above the ground" messages.
 * @retval 0 Player or monster can't apply objects of that type.
 * @retval 1 Has been applied, or there was an error applying the object.
 * @retval 2 Objects of that type can't be applied if not in
 * inventory. */
int player_apply(object *pl, object *op, int aflag, int quiet)
{
	int tmp;

	if (op->env == NULL && QUERY_FLAG(pl, FLAG_FLYING))
	{
		/* Player is flying and applying object not in inventory */
		if (!QUERY_FLAG(pl, FLAG_WIZ) && !QUERY_FLAG(op, FLAG_FLYING) && !QUERY_FLAG(op, FLAG_FLY_ON))
		{
			new_draw_info(NDI_UNIQUE, pl, "But you are floating high above the ground!");
			return 0;
		}
	}

	if (op->type != PLAYER && QUERY_FLAG(op, FLAG_WAS_WIZ) && !QUERY_FLAG(pl, FLAG_WAS_WIZ))
	{
		play_sound_map(pl->map, SOUND_OB_EVAPORATE, NULL, pl->x, pl->y, 0, 0);
		new_draw_info(NDI_UNIQUE, pl, "The object disappears in a puff of smoke!");
		new_draw_info(NDI_UNIQUE, pl, "It must have been an illusion.");
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		return 1;
	}

	tmp = manual_apply(pl, op, aflag);

	if (!quiet)
	{
		if (tmp == 0)
		{
			new_draw_info_format(NDI_UNIQUE, pl, "I don't know how to apply the %s.", query_name(op, NULL));
		}
		else if (tmp == 2)
		{
			new_draw_info_format(NDI_UNIQUE, pl, "You must get it first!\n");
		}
	}

	return tmp;
}

/**
 * Attempt to apply the object 'below' the player.
 *
 * If the player has an open container, we use that for below, otherwise
 * we use the ground.
 * @param pl Player. */
void player_apply_below(object *pl)
{
	object *tmp, *next;
	int floors;

	if (pl->type != PLAYER)
	{
		LOG(llevBug, "BUG: player_apply_below() called for non player object >%s<\n", query_name(pl, NULL));
		return;
	}

	tmp = pl->below;

	/* This is perhaps more complicated.  However, I want to make sure that
	 * we don't use a corrupt pointer for the next object, so we get the
	 * next object in the stack before applying.  This is can only be a
	 * problem if player_apply() has a bug in that it uses the object but does
	 * not return a proper value. */
	for (floors = 0; tmp != NULL; tmp = next)
	{
		next = tmp->below;

		if (QUERY_FLAG(tmp, FLAG_IS_FLOOR))
		{
			floors++;
		}
		/* Process only floor objects after first floor object */
		else if (floors > 0)
		{
			return;
		}

		if (!IS_INVISIBLE(tmp, pl) || QUERY_FLAG(tmp, FLAG_WALK_ON) || QUERY_FLAG(tmp, FLAG_FLY_ON))
		{
			if (player_apply(pl, tmp, 0, 1) == 1)
			{
				return;
			}
		}

		/* Process at most two floor objects */
		if (floors >= 2)
		{
			return;
		}
	}
}

/**
 * Checks for item power restrictions when applying an item.
 * @param who The object applying the item.
 * @param op The item being applied.
 * @return Whether applying is possible. */
static int apply_check_item_power(object *who, const object *op)
{
	if (who->type != PLAYER)
	{
		return 1;
	}

	if (op->item_power == 0 || op->item_power + CONTR(who)->item_power <= settings.item_power_factor * who->level)
	{
		return 1;
	}

	new_draw_info(NDI_UNIQUE, who, "Equipping that combined with other items would consume your soul!");

	return 0;
}

/**
 * Apply an object.
 *
 * This function doesn't check for unpaid items, but checks other
 * restrictions.
 *
 * Usage example:  apply_special(who, op, AP_UNAPPLY | AP_IGNORE_CURSE)
 * @param who Object using op. It can be a monster.
 * @param op Object being used. Should be an equipment type item, eg, one
 * which you put on and keep on for a while, and not something like a
 * potion or scroll.
 * @param aflags Flags.
 * @return 1 if the action could not be completed, 0 on success. */
int apply_special(object *who, object *op, int aflags)
{
	int basic_flag = aflags & AP_BASIC_FLAGS;
	int tmp_flag = 0, i;
	object *tmp;
	char buf[HUGE_BUF];

	if (who == NULL)
	{
		LOG(llevBug, "BUG: apply_special() from object without environment.\n");
		return 1;
	}

	/* op is not in inventory */
	if (op->env != who)
	{
		return 1;
	}

	/* Needs to be initialized */
	buf[0] = '\0';

	if (!QUERY_FLAG(op, FLAG_APPLIED))
	{
		if (!apply_check_item_power(who, op))
		{
			return 1;
		}
	}
	else
	{
		/* Always apply, so no reason to unapply */
		if (basic_flag == AP_APPLY)
		{
			return 0;
		}

		if (!(aflags & AP_IGNORE_CURSE) && (QUERY_FLAG(op, FLAG_CURSED) || QUERY_FLAG(op, FLAG_DAMNED)))
		{
			new_draw_info_format(NDI_UNIQUE, who, "No matter how hard you try, you just can't remove it!");
			return 1;
		}

		if (QUERY_FLAG(op, FLAG_PERM_CURSED))
		{
			SET_FLAG(op, FLAG_CURSED);
		}

		if (QUERY_FLAG(op, FLAG_PERM_DAMNED))
		{
			SET_FLAG(op, FLAG_DAMNED);
		}

		CLEAR_FLAG(op, FLAG_APPLIED);

		switch (op->type)
		{
			case WEAPON:
				(void) change_abil(who, op);
				CLEAR_FLAG(who, FLAG_READY_WEAPON);
				snprintf(buf, sizeof(buf), "You unwield %s.", query_name(op, NULL));
				break;

			/* Allows objects to impart skills */
			case SKILL:
				if (op != who->chosen_skill)
				{
					LOG(llevBug, "BUG: apply_special(): applied skill is not chosen skill\n");
				}

				if (who->type == PLAYER)
				{
					CONTR(who)->shoottype = range_none;

					if (!IS_INVISIBLE(op, who))
					{
						/* It's a tool, need to unlink it */
						unlink_skill(op);
						new_draw_info_format(NDI_UNIQUE, who, "You stop using the %s.", query_name(op, NULL));
						new_draw_info_format(NDI_UNIQUE, who, "You can no longer use the skill: %s.", skills[op->stats.sp].name);
					}
				}

				(void) change_abil(who, op);
				who->chosen_skill = NULL;
				buf[0] = '\0';
				break;

			case ARMOUR:
			case HELMET:
			case SHIELD:
			case RING:
			case BOOTS:
			case GLOVES:
			case AMULET:
			case GIRDLE:
			case BRACERS:
			case CLOAK:
				change_abil(who, op);
				snprintf(buf, sizeof(buf), "You unwear %s.", query_name(op, NULL));
				break;

			case BOW:
			case WAND:
			case ROD:
			case HORN:
				snprintf(buf, sizeof(buf), "You unready %s.", query_name(op, NULL));

				if (who->type == PLAYER)
				{
					CONTR(who)->shoottype = range_none;
				}

				break;

			default:
				snprintf(buf, sizeof(buf), "You unapply %s.", query_name(op, NULL));
				break;
		}

		if (buf[0] != '\0' && who->type == PLAYER)
		{
			new_draw_info(NDI_UNIQUE, who, buf);
		}

		fix_player(who);

		if (!(aflags & AP_NO_MERGE))
		{
			tag_t del_tag = op->count;
			object *cont = op->env;
			tmp = merge_ob(op, NULL);

			if (who->type == PLAYER)
			{
				/* It was merged */
				if (tmp)
				{
					esrv_del_item(CONTR(who), del_tag, cont);
					op = tmp;
				}

				esrv_send_item (who, op);
			}
		}

		return 0;
	}

	if (basic_flag == AP_UNAPPLY)
	{
		return 0;
	}

	i = 0;

	if (op->type == WAND || op->type == ROD || op->type == HORN)
	{
		tmp_flag = 1;
	}

	/* This goes through and checks to see if the player already has
	 * something of that type applied - if so, unapply it. */
	for (tmp = who->inv; tmp != NULL; tmp = tmp->below)
	{
		if ((tmp->type == op->type || (tmp_flag && (tmp->type == WAND || tmp->type == ROD || tmp->type == HORN))) && QUERY_FLAG(tmp, FLAG_APPLIED) && tmp != op)
		{
			if (tmp->type == RING && !i)
			{
				i = 1;
			}
			else if (apply_special(who, tmp, 0))
			{
				return 1;
			}
		}
	}

	if (op->nrof > 1)
	{
		tmp = get_split_ob(op, op->nrof - 1, NULL, 0);
	}
	else
	{
		tmp = NULL;
	}

	switch (op->type)
	{
		case WEAPON:
		{
			if (!QUERY_FLAG(who, FLAG_USE_WEAPON))
			{
				new_draw_info_format(NDI_UNIQUE, who, "You can't use %s.", query_name(op, NULL));

				if (tmp != NULL)
				{
					(void) insert_ob_in_ob(tmp, who);
				}

				return 1;
			}

			if (!check_weapon_power(who, op->last_eat))
			{
				new_draw_info(NDI_UNIQUE, who, "That weapon is too powerful for you to use.\nIt would consume your soul!");

				if (tmp != NULL)
				{
					(void) insert_ob_in_ob(tmp, who);
				}

				return 1;
			}

			if (op->level && (strncmp(op->name, who->name, strlen(who->name))))
			{
				/* If the weapon does not have the name as the character,
				 * can't use it (Ragnarok's sword attempted to be used by
				 * Foo: won't work). */
				new_draw_info(NDI_UNIQUE, who, "The weapon does not recognize you as its owner.");

				if (tmp != NULL)
				{
					(void) insert_ob_in_ob(tmp, who);
				}

				return 1;
			}

			/* If we have applied a shield, don't allow applying of polearm or two-handed weapons */
			if ((op->sub_type >= WEAP_POLE_IMPACT || op->sub_type >= WEAP_2H_IMPACT) && who->type == PLAYER && CONTR(who) && CONTR(who)->equipment[PLAYER_EQUIP_SHIELD])
			{
				new_draw_info(NDI_UNIQUE, who, "You can't wield this weapon and a shield.");

				if (tmp != NULL)
				{
					(void) insert_ob_in_ob(tmp, who);
				}

				return 1;
			}

			if (!check_skill_to_apply(who, op))
			{
				if (tmp != NULL)
				{
					(void) insert_ob_in_ob(tmp,who);
				}

				return 1;
			}

			SET_FLAG(op, FLAG_APPLIED);
			SET_FLAG(who, FLAG_READY_WEAPON);
			(void) change_abil(who, op);
			snprintf(buf, sizeof(buf), "You wield %s.", query_name(op, NULL));
			break;
		}

		case SHIELD:
			/* Don't allow polearm or two-handed weapons with a shield */
			if ((who->type == PLAYER && CONTR(who) && CONTR(who)->equipment[PLAYER_EQUIP_WEAPON]) && (CONTR(who)->equipment[PLAYER_EQUIP_WEAPON]->sub_type >= WEAP_POLE_IMPACT || CONTR(who)->equipment[PLAYER_EQUIP_WEAPON]->sub_type >= WEAP_2H_IMPACT))
			{
				new_draw_info(NDI_UNIQUE, who, "You can't use a shield with your current weapon.");

				if (tmp != NULL)
				{
					(void) insert_ob_in_ob(tmp, who);
				}

				return 1;
			}

		case ARMOUR:
		case HELMET:
		case BOOTS:
		case GLOVES:
		case GIRDLE:
		case BRACERS:
		case CLOAK:
			if (!QUERY_FLAG(who, FLAG_USE_ARMOUR))
			{
				new_draw_info_format(NDI_UNIQUE, who, "You can't use %s.", query_name(op, NULL));

				if (tmp != NULL)
				{
					(void) insert_ob_in_ob(tmp, who);
				}

				return 1;
			}

		case RING:
		case AMULET:
			SET_FLAG(op, FLAG_APPLIED);
			(void) change_abil(who, op);
			snprintf(buf, sizeof(buf), "You wear %s.", query_name(op, NULL));
			break;

		/* This part is needed for skill-tools */
		case SKILL:
			if (who->chosen_skill)
			{
				LOG(llevBug, "BUG: apply_special(): can't apply two skills\n");
				return 1;
			}

			if (who->type == PLAYER)
			{
				CONTR(who)->shoottype = range_skill;

				if (!IS_INVISIBLE(op, who))
				{
					/* for tools */
					if (op->exp_obj)
					{
						LOG(llevBug, "BUG: apply_special(SKILL): found unapplied tool with experience object\n");
					}
					else
					{
						(void) link_player_skill(who, op);
					}

					new_draw_info_format(NDI_UNIQUE, who, "You ready %s.", query_name(op, NULL));
					new_draw_info_format(NDI_UNIQUE, who, "You can now use the skill: %s.", skills[op->stats.sp].name);
				}
				else
				{
					send_ready_skill(who, skills[op->stats.sp].name);
				}
			}

			SET_FLAG(op, FLAG_APPLIED);
			(void) change_abil(who, op);
			who->chosen_skill = op;
			buf[0] = '\0';
			break;

		case WAND:
		case ROD:
		case HORN:
		case BOW:
			if (!check_skill_to_apply(who, op))
			{
				return 1;
			}

			SET_FLAG(op, FLAG_APPLIED);
			new_draw_info_format(NDI_UNIQUE, who, "You ready %s.", query_name(op, NULL));

			if (who->type == PLAYER)
			{
				if (op->type == BOW)
				{
					new_draw_info_format(NDI_UNIQUE, who, "You will now fire %s with %s.", op->race ? op->race : "nothing", query_name(op, NULL));
				}
				else
				{
					CONTR(who)->known_spell = (QUERY_FLAG(op, FLAG_BEEN_APPLIED) || QUERY_FLAG(op, FLAG_IDENTIFIED));
				}
			}

			break;

		default:
			snprintf(buf, sizeof(buf), "You apply %s.", query_name(op, NULL));
	}

	if (!QUERY_FLAG(op, FLAG_APPLIED))
	{
		SET_FLAG(op, FLAG_APPLIED);
	}

	if (buf[0] != '\0')
	{
		new_draw_info(NDI_UNIQUE, who, buf);
	}

	if (tmp != NULL)
	{
		tmp = insert_ob_in_ob(tmp, who);
	}

	fix_player(who);

	if (op->type != WAND && who->type == PLAYER)
	{
		SET_FLAG(op, FLAG_BEEN_APPLIED);
	}

	if (QUERY_FLAG(op, FLAG_PERM_CURSED))
	{
		SET_FLAG(op, FLAG_CURSED);
	}

	if (QUERY_FLAG(op, FLAG_PERM_DAMNED))
	{
		SET_FLAG(op, FLAG_DAMNED);
	}

	if (QUERY_FLAG(op, FLAG_CURSED) || QUERY_FLAG(op, FLAG_DAMNED))
	{
		if (who->type == PLAYER)
		{
			new_draw_info(NDI_UNIQUE, who, "Oops, it feels deadly cold!");
		}
	}

	if (who->type == PLAYER)
	{
		/* If multiple objects were applied, update both slots */
		if (tmp)
		{
			esrv_send_item(who, tmp);
		}

		esrv_send_item(who, op);
	}

	return 0;
}

/**
 * Monster applies an item.
 * @param who The monster.
 * @param op The object to apply.
 * @param aflags Apply flags.
 * @return 1 if the action could not be completed, 0 on success.
 * @see apply_special() */
int monster_apply_special(object *who, object *op, int aflags)
{
	if (QUERY_FLAG(op, FLAG_UNPAID) && !QUERY_FLAG(op, FLAG_APPLIED))
	{
		return 1;
	}

	return apply_special(who, op, aflags);
}
