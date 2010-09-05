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
 * Handles god related code. */

#include <global.h>

/* define this if you want to allow gods to assign more gifts
 * and limitations to priests */
#define MORE_PRIEST_GIFTS

static int lookup_god_by_name(const char *name);
static int worship_forbids_use(object *op, object *exp_obj, uint32 flag, char *string);
static void stop_using_item(object *op, int type, int number);
static void update_priest_flag(object *god, object *exp_ob, uint32 flag);
static int god_gives_present(object *op, object *god, treasure *tr);
static int god_examines_priest(object *op, object *god);
static int god_examines_item(object *god, object *item);
static void lose_priest_exp(object *pl, int loss);
static void god_intervention(object *op, object *god);

/**
 * Returns the ID of specified god.
 * @param name God to search for.
 * @return Identifier of god, -1 if not found. */
static int lookup_god_by_name(const char *name)
{
	int godnr = -1;
	size_t nmlen = strlen(name);

	if (name && strcmp(name, "none"))
	{
		godlink *gl;

		for (gl = first_god; gl; gl = gl->next)
		{
			if (!strncmp(name, gl->name, MIN(strlen(gl->name), nmlen)))
			{
				break;
			}
		}

		if (gl)
		{
			godnr = gl->id;
		}
	}

	return godnr;
}

/**
 * Returns pointer to specified god's object through pntr_to_god_obj().
 * @param name God's name.
 * @return Pointer to god's object, NULL if doesn't match any god. */
object *find_god(const char *name)
{
	object *god = NULL;

	if (name)
	{
		godlink *gl;

		for (gl = first_god; gl; gl = gl->next)
		{
			if (!strcmp(name, gl->name))
			{
				break;
			}
		}

		if (gl)
		{
			god = pntr_to_god_obj(gl);
		}
	}

	return god;
}

/**
 * Player prays at altar.
 *
 * Checks for god changing, divine intervention, and so on.
 * @param pl Player praying.
 * @param altar Altar player's praying on. Doesn't need to be
 * consecrated. */
void pray_at_altar(object *pl, object *altar)
{
	object *pl_god = find_god(determine_god(pl));

	/* Trigger the APPLY event */
	if (trigger_event(EVENT_APPLY, pl, altar, NULL, NULL, 0, 0, 0, SCRIPT_FIX_ALL))
	{
		return;
	}

	/* If non consecrate altar, don't do anything */
	if (!altar->other_arch)
	{
		return;
	}

	/* new convert */
	if (!pl_god)
	{
		become_follower(pl, &altar->other_arch->clone);
		return;
	}
	/* Pray at your gods altar */
	else if (!strcmp(pl_god->name, altar->other_arch->clone.name))
	{
		int bonus = ((pl->stats.Wis / 10) + (SK_level(pl) / 10));

		new_draw_info_format(NDI_UNIQUE, pl, "You feel the powers of your deity %s.", pl_god->name);

		/* We can get negative grace up faster */
		if (pl->stats.grace < 0)
		{
			pl->stats.grace += (bonus > -1 * (pl->stats.grace / 10) ? bonus : -1 * (pl->stats.grace / 10));
		}

		/* We can super-charge grace to 2x max */
		if (pl->stats.grace < (2 * pl->stats.maxgrace))
		{
			pl->stats.grace += bonus / 2;
		}

		/* I think there was a bug here in that this was nested
		 * in the if immediately above */
		if (pl->stats.grace > pl->stats.maxgrace)
		{
			pl->stats.grace = pl->stats.maxgrace;
		}

		/* Every once in a while, the god decides to checkup on their
		 * follower, and may intervene to help them out. */
		bonus = MAX(1, bonus);

		if (((rndm(0, 399)) - bonus) < 0)
		{
			god_intervention(pl, pl_god);
		}
	}
	/* Praying to another god! */
	else
	{
		int loss = 0, angry = 1;

		/* I believe the logic for detecting opposing gods was completely
		 * broken - I think it should work now.  altar->other_arch
		 * points to the god of this altar (which we have
		 * already verified is non null).  pl_god->other_arch
		 * is the opposing god - we need to verify that exists before
		 * using its values. */
		if (pl_god->other_arch && (altar->other_arch->name == pl_god->other_arch->name))
		{
			angry = 2;

			if (rndm(0, SK_level(pl) + 2) - 5 > 0)
			{
				/* you really screwed up */
				angry = 3;
				new_draw_info_format(NDI_UNIQUE | NDI_NAVY, pl, "Foul priest! %s punishes you!", pl_god->name);
				cast_magic_storm(pl, get_archetype("loose_magic"), pl_god->level + 20);
			}

			new_draw_info_format(NDI_UNIQUE | NDI_NAVY, pl, "Foolish heretic! %s is livid!", pl_god->name);
		}
		else
		{
			new_draw_info_format(NDI_UNIQUE | NDI_NAVY, pl, "Heretic! %s is angered!", pl_god->name);
		}

		/* whether we will be successfull in defecting or not -
		 * we lose experience from the clerical experience obj */
		loss = (int) ((float)0.1 * (float) pl->chosen_skill->exp_obj->stats.exp);

		if (loss)
		{
			lose_priest_exp(pl, rndm(0, loss * angry - 1));
		}

		/* May switch Gods, but it's random chance based on our current
		 * level. Note it gets harder to swap gods the higher we get */
		if ((angry == 1) && !(rndm(0, pl->chosen_skill->exp_obj->level)))
		{
			become_follower(pl, &altar->other_arch->clone);
		}
		/* If angry... switching gods */
		else
		{
			/* toss this player off the altar.  He can try again. */
			new_draw_info_format(NDI_UNIQUE | NDI_NAVY, pl, "A divine force pushes you off the altar.");
			/* back him off the way he came. */
			move_player(pl, absdir(pl->facing + 4));
		}
	}
}

/**
 * Get spell number from an object's slaying or liv::sp field.
 * @param op The object to get the spell number from.
 * @return The spell number. */
static int get_spell_number(object *op)
{
	int spell;

	if (op->slaying && (spell = look_up_spell_name(op->slaying)) >= 0)
	{
		return spell;
	}
	else
	{
		return op->stats.sp;
	}
}

/**
 * Ensure that 'op' doesn't know any special prayers that are not granted
 * by 'god'.
 * @param op The object.
 * @param god The god. */
static void check_special_prayers(object *op, object *god)
{
	treasure *tr;
	object *tmp, *next_tmp;
	int spell;

	/* Outer loop iterates over all special prayer marks */
	for (tmp = op->inv; tmp; tmp = next_tmp)
	{
		next_tmp = tmp->below;

		if (tmp->type != FORCE || tmp->slaying == NULL || strcmp(tmp->slaying, "special prayer"))
		{
			continue;
		}

		spell = tmp->stats.sp;

		if (god->randomitems == NULL)
		{
			LOG(llevBug, "BUG: check_special_prayers(): %s without randomitems\n", query_name(god, NULL));
			do_forget_spell(op, spell);
			continue;
		}

		/* Inner loop tries to find the special prayer in the god's treasure
		 * list. */
		for (tr = god->randomitems->items; tr; tr = tr->next)
		{
			object *item;

			if (tr->item == NULL)
			{
				continue;
			}

			item = &tr->item->clone;

			if (item->type == SPELLBOOK && get_spell_number(item) == spell)
			{
				/* Current god allows this special prayer. */
				spell = -1;
				break;
			}
		}

		if (spell >= 0)
		{
			do_forget_spell(op, spell);
		}
	}
}

/**
 * This function is called whenever a player has switched to a new god.
 * It basically handles all the stat changes that happen to the player,
 * including the removal of god-given items (from the former cult).
 * @param op Player switching cults.
 * @param new_god New god to worship. */
void become_follower(object *op, object *new_god)
{
	/* obj. containing god data */
	object *exp_obj = op->chosen_skill->exp_obj;
	/* old god */
	object *old_god = NULL;
	treasure *tr;
	int i;

	CONTR(op)->socket.ext_title_flag = 1;

	/* get old god */
	if (exp_obj->title)
	{
		old_god = find_god(exp_obj->title);
	}

	/* give the player any special god-characteristic-items. */
	for (tr = new_god->randomitems->items; tr != NULL; tr = tr->next)
	{
		if (tr->item && IS_SYS_INVISIBLE(&tr->item->clone) && tr->item->clone.type != SPELLBOOK && tr->item->clone.type != BOOK)
		{
			god_gives_present(op, new_god, tr);
		}
	}

	if (!op || !new_god)
	{
		return;
	}

	if (op->race && new_god->slaying && strstr(op->race, new_god->slaying))
	{
		new_draw_info_format(NDI_UNIQUE | NDI_NAVY, op, "Fool! %s detests your kind!", new_god->name);

		if (rndm(0, op->level - 1) - 5 > 0)
		{
			cast_magic_storm(op, get_archetype("loose_magic"), new_god->level + 10);
		}

		return;
	}

	new_draw_info_format(NDI_UNIQUE | NDI_NAVY, op, "You become a follower of %s!", new_god->name);

	/* get rid of old god */
	if (exp_obj->title)
	{
		new_draw_info_format(NDI_UNIQUE, op, "%s's blessing is withdrawn from you.", exp_obj->title);
		CLEAR_FLAG(exp_obj, FLAG_APPLIED);
		(void) change_abil(op, exp_obj);
		FREE_AND_CLEAR_HASH2(exp_obj->title);
	}

	/* now change to the new gods attributes to exp_obj */
	FREE_AND_COPY_HASH(exp_obj->title, new_god->name);
	exp_obj->path_attuned = new_god->path_attuned;
	exp_obj->path_repelled = new_god->path_repelled;
	exp_obj->path_denied = new_god->path_denied;
	/* copy god's protections */
	memcpy(exp_obj->protection, new_god->protection, sizeof(new_god->protection));

	/* make sure that certain immunities do NOT get passed to the
	 * follower! */
	for (i = 0; i < NROFATTACKS; i++)
	{
		if (exp_obj->protection[i] > 30 && (i == ATNR_FIRE || i == ATNR_COLD || i == ATNR_ELECTRICITY || i == ATNR_POISON))
		{
			exp_obj->protection[i] = 30;
		}
	}

#ifdef MORE_PRIEST_GIFTS
	exp_obj->stats.hp = (sint16) new_god->last_heal;
	exp_obj->stats.sp = (sint16) new_god->last_sp;
	exp_obj->stats.grace = (sint16) new_god->last_grace;
	exp_obj->stats.food = (sint16) new_god->last_eat;
	/* gods may pass on certain flag properties */
	update_priest_flag(new_god, exp_obj, FLAG_SEE_IN_DARK);
	update_priest_flag(new_god, exp_obj, FLAG_REFL_SPELL);
	update_priest_flag(new_god, exp_obj, FLAG_REFL_MISSILE);
	update_priest_flag(new_god, exp_obj, FLAG_STEALTH);
	update_priest_flag(new_god, exp_obj, FLAG_SEE_INVISIBLE);
	update_priest_flag(new_god, exp_obj, FLAG_UNDEAD);
	update_priest_flag(new_god, exp_obj, FLAG_BLIND);
	/* better have this if blind! */
	update_priest_flag(new_god, exp_obj, FLAG_XRAYS);
#endif

	new_draw_info_format(NDI_UNIQUE, op, "You are bathed in %s's aura.", new_god->name);

#ifdef MORE_PRIEST_GIFTS
	/* Weapon/armour use are special...handle flag toggles here as this can
	 * only happen when gods are worshipped and if the new priest could
	 * have used armour/weapons in the first place */
	update_priest_flag(new_god,exp_obj, FLAG_USE_WEAPON);
	update_priest_flag(new_god,exp_obj, FLAG_USE_ARMOUR);

	if (worship_forbids_use(op, exp_obj, FLAG_USE_WEAPON, "weapons"))
	{
		stop_using_item(op, WEAPON, 2);
	}

	if (worship_forbids_use(op, exp_obj, FLAG_USE_ARMOUR, "armour"))
	{
		stop_using_item(op, ARMOUR, 1);
		stop_using_item(op, HELMET, 1);
		stop_using_item(op, BOOTS, 1);
		stop_using_item(op, GLOVES, 1);
		stop_using_item(op, SHIELD, 1);
	}
#endif

	SET_FLAG(exp_obj, FLAG_APPLIED);
	(void) change_abil(op, exp_obj);

	check_special_prayers(op, new_god);
}

/**
 * Forbids or lets player use something item type.
 * @param op Player.
 * @param exp_obj Praying skill.
 * @param flag FLAG_xxx to check against.
 * @param string What flag corresponds to ("weapons", "shield", ...).
 * @return 1 if player was changed, 0 if no change. */
static int worship_forbids_use(object *op, object *exp_obj, uint32 flag, char *string)
{
	if (QUERY_FLAG(&op->arch->clone, flag))
	{
		if (QUERY_FLAG(op, flag) != QUERY_FLAG(exp_obj, flag))
		{
			update_priest_flag(exp_obj, op, flag);

			if (QUERY_FLAG(op, flag))
			{
				new_draw_info_format(NDI_UNIQUE, op, "You may use %s again.", string);
			}
			else
			{
				new_draw_info_format(NDI_UNIQUE, op, "You are forbidden to use %s.", string);
				return 1;
			}
		}
	}

	return 0;
}

/**
 * Unapplies up to number worth of items of type type, ignoring curse
 * status.
 *
 * This is used when the player gets forbidden to use eg weapons.
 * @param op Player we're considering.
 * @param type Item type to remove.
 * @param number Maximum number of items to unapply. */
static void stop_using_item(object *op, int type, int number)
{
	object *tmp;

	for (tmp = op->inv; tmp && number; tmp = tmp->below)
	{
		if (tmp->type == type && QUERY_FLAG(tmp, FLAG_APPLIED))
		{
			apply_special(op, tmp, AP_UNAPPLY | AP_IGNORE_CURSE);
			number--;
		}
	}
}

/**
 * If the god does/doesn't have this flag, we give/remove it from the
 * experience object if it doesn't/does already exist.
 * @param god God object.
 * @param exp_ob Player's praying skill object.
 * @param flag Flag to consider. */
static void update_priest_flag(object *god, object *exp_ob, uint32 flag)
{
	if (QUERY_FLAG(god, flag) && !QUERY_FLAG(exp_ob, flag))
	{
		SET_FLAG(exp_ob, flag);
	}
	else if (QUERY_FLAG(exp_ob, flag) && !QUERY_FLAG(god, flag))
	{
		CLEAR_FLAG(exp_ob, flag);
	}
}

/**
 * Determines if op worships a god. Returns the godname if they do or
 * "none" if they have no god. In the case of an NPC, if they have no
 * god, we give them a random one.
 * @param op Object to get name of.
 * @return God name, "none" if nothing suitable. */
const char *determine_god(object *op)
{
	int godnr = -1;

	/* spells */
	if ((op->type == CONE || op->type == SWARM_SPELL) && op->title)
	{
		if (lookup_god_by_name(op->title) >= 0)
		{
			return op->title;
		}
	}

	if (op->type != PLAYER && QUERY_FLAG(op, FLAG_ALIVE))
	{
		if (!op->title)
		{
			godlink *gl = first_god;

			godnr = rndm(1, gl->id);

			while (gl)
			{
				if (gl->id == godnr)
				{
					break;
				}

				gl = gl->next;
			}

			FREE_AND_COPY_HASH(op->title, gl->name);
		}

		return op->title;
	}

	/* If we are player, let's search a bit harder for the god. This
	 * is a fix for perceive self (before, we just looked at the active
	 * skill.) */
	if (op->type == PLAYER)
	{
		object *tmp;

		for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
		{
			if (tmp->type == EXPERIENCE && tmp->stats.Wis)
			{
				if (tmp->title)
				{
					return tmp->title;
				}
				else
				{
					return shstr_cons.none;
				}
			}
		}
	}

	return shstr_cons.none;
}

/**
 * Determines the archetype for holy servant and god avatar.
 *
 * Possible monsters are stored as invisible books in god's inventory,
 * one having the right name is selected.
 * @param god God for which we want something.
 * @param type What the summon type is.
 * @return Archetype matching the type, NULL if none found. */
archetype *determine_holy_arch(object *god, const char *type)
{
	treasure *tr;

	if (!god || !god->randomitems)
	{
		LOG(llevBug, "BUG: determine_holy_arch(): no god or god without randomitems\n");
		return NULL;
	}

	for (tr = god->randomitems->items; tr != NULL; tr = tr->next)
	{
		object *item;

		if (!tr->item)
		{
			continue;
		}

		item = &tr->item->clone;

		if (item->type == BOOK && IS_SYS_INVISIBLE(item) && strcmp(item->name, type) == 0)
		{
			return item->other_arch;
		}
	}

	return NULL;
}

/**
 * God helps player by removing curse and/or damnation.
 * @param op Player to help.
 * @param remove_damnation If set, also removes damned items.
 * @return 1 if at least one item was uncursed, 0 otherwise. */
static int god_removes_curse(object *op, int remove_damnation)
{
	object *tmp;
	int success = 0;

	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		if (QUERY_FLAG(tmp, FLAG_DAMNED) && !remove_damnation)
		{
			continue;
		}

		if (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED))
		{
			success = 1;
			CLEAR_FLAG(tmp, FLAG_DAMNED);
			CLEAR_FLAG(tmp, FLAG_CURSED);

			if (op->type == PLAYER)
			{
				esrv_send_item(op, tmp);
			}
		}
	}

	if (success)
	{
		new_draw_info(NDI_UNIQUE, op, "You feel like someone is helping you.");
	}

	return success;
}

/**
 * Converts a level and difficulty to a magic/enchantment value for eg
 * weapons.
 * @param level Level.
 * @param difficulty Difficulty. Must be 1 or more.
 * @return Number of enchantments for level and difficulty. */
static int follower_level_to_enchantments(int level, int difficulty)
{
	if (difficulty < 1)
	{
		LOG(llevBug, "BUG: follower_level_to_enchantments(): difficulty %d is invalid\n", difficulty);
		return 0;
	}

	if (level <= 20)
	{
		return level / difficulty;
	}

	if (level <= 40)
	{
		return (20 + (level - 20) / 2) / difficulty;
	}

	return (30 + (level - 40) / 4) / difficulty;
}

/**
 * God wants to enchant weapon.
 *
 * Affected weapon is the applied one (weapon or bow). It's checked to
 * make sure it isn't a weapon for another god. If all is all right,
 * update weapon with attacktype, slaying and such.
 * @param op Player.
 * @param god God enchanting weapon.
 * @param tr Treasure list item for enchanting weapon, contains the
 * enchantment level.
 * @return 0 if weapon wasn't changed, 1 if changed. */
static int god_enchants_weapon(object *op, object *god, object *tr)
{
	char buf[MAX_BUF];
	object *weapon;
	int tmp;

	for (weapon = op->inv; weapon; weapon = weapon->below)
	{
		if (weapon->type == WEAPON && QUERY_FLAG(weapon, FLAG_APPLIED))
		{
			break;
		}
	}

	if (weapon == NULL || god_examines_item(god, weapon) <= 0)
	{
		return 0;
	}

	/* First give it a title, so other gods won't touch it */
	if (!weapon->title)
	{
		snprintf(buf, sizeof(buf), "of %s", god->name);
		FREE_AND_COPY_HASH(weapon->title, buf);

		if (op->type == PLAYER)
		{
			esrv_update_item(UPD_NAME, op, weapon);
		}

		new_draw_info(NDI_UNIQUE, op, "Your weapon quivers as if struck!");
	}

	/* Allow the weapon to slay enemies */
	if (!weapon->slaying && god->slaying)
	{
		FREE_AND_COPY_HASH(weapon->slaying, god->slaying);
		new_draw_info_format(NDI_UNIQUE, op, "Your %s now hungers to slay enemies of your god!", weapon->name);
		return 1;
	}

	/* Higher magic value */
	tmp = follower_level_to_enchantments(SK_level(op), tr->level);

	if (weapon->magic < tmp)
	{
		new_draw_info(NDI_UNIQUE, op, "A phosphorescent glow envelops your weapon!");
		weapon->magic++;

		if (op->type == PLAYER)
		{
			esrv_update_item(UPD_NAME, op, weapon);
		}

		return 1;
	}

	return 0;
}

/**
 * Compares two strings.
 * @param s1 The first string to compare.
 * @param s2 The second string to compare.
 * @return 1 if s1 and s2 are the same - either both NULL, or
 * strcmp() == 0. */
static int same_string(const char *s1, const char *s2)
{
	if (s1 == NULL)
	{
		if (s2 == NULL)
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		if (s2 == NULL)
		{
			return 0;
		}
		else
		{
			return strcmp(s1, s2) == 0;
		}
	}
}

/**
 * Checks for any occurrence of the given 'item' in the inventory of 'op'
 * (recursively).
 * @param op Object to check.
 * @param item Object to check for.
 * @return 1 if found, 0 otherwise. */
static int follower_has_similar_item(object *op, object *item)
{
	object *tmp;

	for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
	{
		if (tmp->type == item->type && same_string(tmp->name, item->name) && same_string(tmp->title, item->title) && same_string(tmp->msg, item->msg) && same_string(tmp->slaying, item->slaying))
		{
			return 1;
		}

		if (tmp->inv && follower_has_similar_item(tmp, item))
		{
			return 1;
		}
	}

	return 0;
}

/**
 * God gives an item to the player. Inform player of the present.
 * @param op Who is getting the treasure.
 * @param god God giving the present.
 * @param tr Object to give. Should be a single object on list.
 * @return 0 if nothing was given, 1 otherwise. */
static int god_gives_present(object *op, object *god, treasure *tr)
{
	object *tmp;

	if (follower_has_similar_item(op, &tr->item->clone))
	{
		return 0;
	}

	tmp = arch_to_object(tr->item);
	new_draw_info_format(NDI_UNIQUE, op, "%s lets %s appear in your hands.", god->name, query_short_name(tmp, NULL));
	tmp = insert_ob_in_ob(tmp, op);

	if (op->type == PLAYER)
	{
		esrv_send_item(op, tmp);
	}

	return 1;
}

/**
 * Every once in a while the god will intervene to help the worshiper.
 * Later, this function can be used to supply quests, etc for the priest.
 * @param op Player praying.
 * @param god God player is praying to. */
static void god_intervention(object *op, object *god)
{
	int level = SK_level(op);
	treasure *tr;

	if (!god || !god->randomitems)
	{
		LOG(llevBug, "BUG: god_intervention(): (p:%s) no god %s or god without randomitems\n", query_name(op, NULL), query_name(god, NULL));
		return;
	}

	check_special_prayers(op, god);

	/* Let's do some checks of whether we are kosher with our god */
	if (god_examines_priest(op, god) < 0)
	{
		return;
	}

	new_draw_info(NDI_UNIQUE, op, "You feel a holy presence!");

	for (tr = god->randomitems->items; tr != NULL; tr = tr->next)
	{
		object *item;

		if (tr->chance <= rndm(0, 99))
		{
			continue;
		}

		/* Treasurelist - generate some treasure for the follower */
		if (tr->name)
		{
			treasurelist *tl = find_treasurelist(tr->name);

			if (tl == NULL)
			{
				continue;
			}

			new_draw_info(NDI_UNIQUE, op, "Something appears before your eyes. You catch it before it falls to the ground.");
			create_treasure(tl, op, GT_STARTEQUIP | GT_ONLY_GOOD | GT_UPDATE_INV, level, T_STYLE_UNSET, ART_CHANCE_UNSET, 0, NULL);
			return;
		}

		if (!tr->item)
		{
			LOG(llevBug, "BUG: Empty entry in %s's treasure list\n", query_name(god, NULL));
			continue;
		}

		item = &tr->item->clone;

		/* Grace limit */
		if (item->type == BOOK && IS_SYS_INVISIBLE(item) && item->name == shstr_cons.grace_limit)
		{
			if (op->stats.grace < item->stats.grace || op->stats.grace < op->stats.maxgrace)
			{
#if 0
				/* Follower lacks the required grace for the following
				 * treasure list items. */
				(void) cast_change_attr(op, op, op, SP_HOLY_POSSESSION);
#endif
				return;
			}

			continue;
		}

		/* Restore grace */
		if (item->type == BOOK && IS_SYS_INVISIBLE(item) && item->name == shstr_cons.restore_grace)
		{
			if (op->stats.grace >= 0)
			{
				continue;
			}

			op->stats.grace = rndm(0, 9);
			new_draw_info(NDI_UNIQUE, op, "You are returned to a state of grace.");
			return;
		}

		/* Heal damage */
		if (item->type == BOOK && IS_SYS_INVISIBLE(item) && item->name == shstr_cons.restore_hitpoints)
		{
			if (op->stats.hp >= op->stats.maxhp)
			{
				continue;
			}

			new_draw_info(NDI_UNIQUE, op, "A white light surrounds and heals you!");
			op->stats.hp = op->stats.maxhp;
			return;
		}

		/* Restore spellpoints */
		if (item->type == BOOK && IS_SYS_INVISIBLE(item) && item->name == shstr_cons.restore_hitpoints)
		{
			int max = (int) ((float) op->stats.maxsp * ((float) item->stats.maxsp / (float) 100.0));
			/* Restore to 50 .. 100%, if sp < 50% */
			int new_sp = (int) ((float) rndm(1000, 1999) / (float) 2000.0 * (float) max);

			if (op->stats.sp >= max / 2)
			{
				continue;
			}

			new_draw_info(NDI_UNIQUE, op, "A blue lightning strikes your head but doesn't hurt you!");
			op->stats.sp = new_sp;
		}

		/* Various heal spells */
		if (item->type == BOOK && IS_SYS_INVISIBLE(item) && item->name == shstr_cons.heal_spell)
		{
			if (cast_heal(op, 1, op, get_spell_number(item)))
			{
				return;
			}
			else
			{
				continue;
			}
		}

		/* Remove curse */
		if (item->type == BOOK && IS_SYS_INVISIBLE(item) && item->name == shstr_cons.remove_curse)
		{
			if (god_removes_curse(op, 0))
			{
				return;
			}
			else
			{
				continue;
			}
		}

		/* Remove damnation */
		if (item->type == BOOK && IS_SYS_INVISIBLE(item) && item->name == shstr_cons.remove_damnation)
		{
			if (god_removes_curse(op, 1))
			{
				return;
			}
			else
			{
				continue;
			}
		}

		/* Heal depletion */
		if (item->type == BOOK && IS_SYS_INVISIBLE(item) && item->name == shstr_cons.heal_depletion)
		{
			object *depl;
			archetype *at;
			int i;

			if ((at = find_archetype("depletion")) == NULL)
			{
				LOG(llevBug, "BUG: Could not find archetype depletion.\n");
				continue;
			}

			depl = present_arch_in_ob(at, op);

			if (depl == NULL)
			{
				continue;
			}

			new_draw_info(NDI_UNIQUE, op, "Shimmering light surrounds and restores you!");

			for (i = 0; i < NUM_STATS; i++)
			{
				if (get_attr_value(&depl->stats, i))
				{
					new_draw_info(NDI_UNIQUE, op, restore_msg[i]);
				}
			}

			remove_ob(depl);
			fix_player(op);
			return;
		}

		/* Messages */
		if (item->type == BOOK && IS_SYS_INVISIBLE(item) && item->name == shstr_cons.message)
		{
			new_draw_info(NDI_UNIQUE, op, item->msg);
			return;
		}

		/* Enchant weapon */
		if (item->type == BOOK && IS_SYS_INVISIBLE(item) && item->name == shstr_cons.enchant_weapon)
		{
			if (god_enchants_weapon(op, god, item))
			{
				return;
			}
			else
			{
				continue;
			}
		}

		/* Spellbooks - works correctly only for prayers */
		if (item->type == SPELLBOOK)
		{
			int spell = get_spell_number(item);

			if (check_spell_known(op, spell))
			{
				continue;
			}

			if (spells[spell].level > level)
			{
				continue;
			}

			if (IS_SYS_INVISIBLE(item))
			{
				new_draw_info_format(NDI_UNIQUE, op, "%s grants you use of a special prayer!", god->name);
				do_learn_spell(op, spell, 1);
				return;
			}

			if (!QUERY_FLAG(item, FLAG_STARTEQUIP))
			{
				LOG(llevBug, "BUG: visible spellbook in %s's treasure list lacks FLAG_STARTEQUIP\n", query_name(god, NULL));
				continue;
			}

			if (!item->stats.Wis)
			{
				LOG(llevBug, "BUG: visible spellbook in %s's treasure list doesn't contain a special prayer\n", query_name(god, NULL));
				continue;
			}

			if (god_gives_present(op, god, tr))
			{
				return;
			}
			else
			{
				continue;
			}
		}

		/* Other gifts */
		if (!IS_SYS_INVISIBLE(item))
		{
			if (god_gives_present(op, god, tr))
			{
				return;
			}
			else
			{
				continue;
			}
		}
	}

	new_draw_info(NDI_UNIQUE, op, "You feel rapture.");
}

/**
 * Checks and maybe punishes someone praying.
 *
 * All applied items are examined, if player is using more items of other
 * gods, s/he loses experience in praying or general experience if no
 * praying.
 * @param op Player the god examines.
 * @param god God examining the player.
 * @return Begative value is god is not pleased, otherwise positive
 * value, the higher the better. */
static int god_examines_priest(object *op, object *god)
{
	int reaction = 1;
	object *item = NULL;

	for (item = op->inv; item; item = item->below)
	{
		if (QUERY_FLAG(item, FLAG_APPLIED))
		{
			reaction += god_examines_item(god, item) * (item->magic ? abs(item->magic) : 1);
		}
	}

	/* Well, well. Looks like we screwed up. Time for god's revenge */
	if (reaction < 0)
	{
		int loss = 10000000, angry = abs(reaction);

		if (op->chosen_skill->exp_obj)
		{
			loss = (int) ((float) 0.05 * (float) op->chosen_skill->exp_obj->stats.exp);
		}

		lose_priest_exp(op, rndm(0, loss * angry - 1));

		if (rndm(0, angry))
		{
			cast_magic_storm(op, get_archetype("loose_magic"), SK_level(op) + (angry * 3));
		}

		new_draw_info_format(NDI_UNIQUE | NDI_NAVY, op, "%s becomes angry and punishes you!", god->name);
	}

	return reaction;
}

/**
 * God checks item the player is using. If you are using the item of an
 * enemy god, it can be bad...
 * @param god God checking.
 * @param item Item to check.
 * @retval -1 Item is bad.
 * @retval 0 Item is neutral.
 * @retval 1 Item is good. */
static int god_examines_item(object *god, object *item)
{
	char buf[MAX_BUF];

	if (!god || !item)
	{
		return 0;
	}

	/* unclaimed item are ok */
	if (!item->title)
	{
		return 1;
	}

	snprintf(buf, sizeof(buf), "of %s", god->name);

	/* belongs to that God */
	if (!strcmp(item->title, buf))
	{
		return 1;
	}

	/* check if we have any enemy blessed item */
	if (god->title)
	{
		snprintf(buf, sizeof(buf), "of %s", god->title);

		if (!strcmp(item->title, buf))
		{
			if (item->env)
			{
				new_draw_info_format(NDI_UNIQUE | NDI_NAVY, item->env, "Heretic! You are using %s!", query_name(item, NULL));
			}

			return -1;
		}
	}

	/* item is sacred to a non-enemy god/or is otherwise magical */
	return 0;
}

/**
 * Lose some priest experience.
 * @param pl Player.
 * @param loss How much to lose.
 * @todo Make work again? */
static void lose_priest_exp(object *pl, int loss)
{
	(void) pl;
	(void) loss;
#if 0
	if (!pl || pl->type != PLAYER || !pl->chosen_skill || !pl->chosen_skill->exp_obj)
	{
		LOG(llevBug, "BUG: Bad call to lose_priest_exp()\n");
		return;
	}

	if ((loss = check_dm_add_exp_to_obj(pl->chosen_skill->exp_obj, loss)))
	{
		add_exp(pl, -loss, pl->chosen_skill->stats.sp);
	}
#endif
}
