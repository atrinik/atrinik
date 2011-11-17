/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
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
		LOG(llevBug, "do_learn_spell(): not a player ->%s\n", op->name);
		return;
	}

	/* Upgrade special prayers to normal prayers */
	if (check_spell_known(op, spell))
	{
		draw_info_format(COLOR_WHITE, op, "You already know the spell '%s'!", spells[spell].name);

		if (special_prayer || !tmp)
		{
			LOG(llevBug, "do_learn_spell(): spell already known, but can't upgrade it\n");
			return;
		}

		remove_ob(tmp);
		object_destroy(tmp);
		return;
	}

	/* Learn new spell/prayer */
	if (tmp)
	{
		LOG(llevBug, "do_learn_spell(): spell unknown, but special prayer mark present\n");
		remove_ob(tmp);
		object_destroy(tmp);
	}

	play_sound_player_only(CONTR(op), CMD_SOUND_EFFECT, "learnspell.ogg", 0, 0, 0, 0);
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
	draw_info_format(COLOR_WHITE, op, "You have learned the spell %s!", spells[spell].name);
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
		LOG(llevBug, "do_forget_spell(): Not a player: %s (%d).\n", query_name(op, NULL), spell);
		return;
	}

	if (!check_spell_known(op, spell))
	{
		LOG(llevBug, "do_forget_spell(): Spell %d not known.\n", spell);
		return;
	}

	play_sound_player_only(CONTR(op), CMD_SOUND_EFFECT, "lose_some.ogg", 0, 0, 0, 0);
	draw_info_format(COLOR_WHITE, op, "You lose knowledge of %s.", spells[spell].name);

	send_spelllist_cmd(op, spells[spell].name, SPLIST_MODE_REMOVE);
	tmp = find_special_prayer_mark(op, spell);

	if (tmp)
	{
		remove_ob(tmp);
		object_destroy(tmp);
	}

	for (i = 0; i < CONTR(op)->nrofknownspells; i++)
	{
		if (CONTR(op)->known_spells[i] == spell)
		{
			CONTR(op)->known_spells[i] = CONTR(op)->known_spells[--CONTR(op)->nrofknownspells];
			return;
		}
	}

	LOG(llevBug, "do_forget_spell(): Couldn't find spell %d.\n", spell);
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
	tmp = HEAD(tmp);

	if (op->type == PLAYER)
	{
		CONTR(op)->praying = 0;
	}

	if (QUERY_FLAG(tmp, FLAG_UNPAID) && !QUERY_FLAG(tmp, FLAG_APPLIED))
	{
		if (op->type == PLAYER)
		{
			draw_info(COLOR_WHITE, op, "You should pay for it first.");
			return OBJECT_METHOD_OK;
		}
		/* Monsters just skip unpaid items */
		else
		{
			return OBJECT_METHOD_UNHANDLED;
		}
	}

	/* Monsters must not apply random chests. */
	if (op->type != PLAYER && tmp->type == TREASURE)
	{
		return OBJECT_METHOD_UNHANDLED;
	}

	/* Trigger the APPLY event */
	if (!(aflag & AP_NO_EVENT) && trigger_event(EVENT_APPLY, op, tmp, NULL, NULL, aflag, 0, 0, SCRIPT_FIX_ACTIVATOR))
	{
		return OBJECT_METHOD_OK;
	}

	/* Trigger the map-wide apply event. */
	if (!(aflag & AP_NO_EVENT) && op->map && op->map->events)
	{
		int retval = trigger_map_event(MEVENT_APPLY, op->map, op, tmp, NULL, NULL, aflag);

		if (retval != -1)
		{
			return retval;
		}
	}

	aflag &= ~AP_NO_EVENT;

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
			draw_info(COLOR_WHITE, op, "The item level is too high to apply.");
			return OBJECT_METHOD_OK;
		}
	}

	return object_apply(tmp, op, aflag);
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
			draw_info(COLOR_WHITE, pl, "But you are floating high above the ground!");
			return 0;
		}
	}

	tmp = manual_apply(pl, op, aflag);

	if (!quiet)
	{
		if (tmp == OBJECT_METHOD_UNHANDLED)
		{
			draw_info_format(COLOR_WHITE, pl, "I don't know how to apply the %s.", query_name(op, NULL));
		}
		else if (tmp == OBJECT_METHOD_ERROR)
		{
			draw_info_format(COLOR_WHITE, pl, "You must get it first!\n");
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
		LOG(llevBug, "player_apply_below() called for non player object >%s<\n", query_name(pl, NULL));
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

	draw_info(COLOR_WHITE, who, "Equipping that combined with other items would consume your soul!");

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
		LOG(llevBug, "apply_special() from object without environment.\n");
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
			draw_info_format(COLOR_WHITE, who, "No matter how hard you try, you just can't remove it!");
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
					LOG(llevBug, "apply_special(): applied skill is not chosen skill\n");
				}

				if (who->type == PLAYER)
				{
					CONTR(who)->shoottype = range_none;

					if (!IS_INVISIBLE(op, who))
					{
						/* It's a tool, need to unlink it */
						unlink_skill(op);
						draw_info_format(COLOR_WHITE, who, "You stop using the %s.", query_name(op, NULL));
						draw_info_format(COLOR_WHITE, who, "You can no longer use the skill: %s.", skills[op->stats.sp].name);
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
			draw_info(COLOR_WHITE, who, buf);
		}

		fix_player(who);

		if (!(aflags & AP_NO_MERGE))
		{
			merge_ob(op, NULL);
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
				draw_info_format(COLOR_WHITE, who, "You can't use %s.", query_name(op, NULL));

				if (tmp != NULL)
				{
					insert_ob_in_ob(tmp, who);
				}

				return 1;
			}

			if (op->level && (strncmp(op->name, who->name, strlen(who->name))))
			{
				/* If the weapon does not have the name as the character,
				 * can't use it (Ragnarok's sword attempted to be used by
				 * Foo: won't work). */
				draw_info(COLOR_WHITE, who, "The weapon does not recognize you as its owner.");

				if (tmp != NULL)
				{
					insert_ob_in_ob(tmp, who);
				}

				return 1;
			}

			/* If we have applied a shield, don't allow applying of polearm or two-handed weapons */
			if ((op->sub_type >= WEAP_POLE_IMPACT || op->sub_type >= WEAP_2H_IMPACT) && who->type == PLAYER && CONTR(who) && CONTR(who)->equipment[PLAYER_EQUIP_SHIELD])
			{
				draw_info(COLOR_WHITE, who, "You can't wield this weapon and a shield.");

				if (tmp != NULL)
				{
					insert_ob_in_ob(tmp, who);
				}

				return 1;
			}

			if (!check_skill_to_apply(who, op))
			{
				if (tmp != NULL)
				{
					insert_ob_in_ob(tmp,who);
				}

				return 1;
			}

			snprintf(buf, sizeof(buf), "You wield %s.", query_name(op, NULL));
			SET_FLAG(op, FLAG_APPLIED);
			SET_FLAG(who, FLAG_READY_WEAPON);
			change_abil(who, op);
			break;
		}

		case SHIELD:
			/* Don't allow polearm or two-handed weapons with a shield */
			if ((who->type == PLAYER && CONTR(who) && CONTR(who)->equipment[PLAYER_EQUIP_WEAPON]) && (CONTR(who)->equipment[PLAYER_EQUIP_WEAPON]->sub_type >= WEAP_POLE_IMPACT || CONTR(who)->equipment[PLAYER_EQUIP_WEAPON]->sub_type >= WEAP_2H_IMPACT))
			{
				draw_info(COLOR_WHITE, who, "You can't use a shield with your current weapon.");

				if (tmp != NULL)
				{
					insert_ob_in_ob(tmp, who);
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
				draw_info_format(COLOR_WHITE, who, "You can't use %s.", query_name(op, NULL));

				if (tmp != NULL)
				{
					insert_ob_in_ob(tmp, who);
				}

				return 1;
			}

		case RING:
		case AMULET:
			snprintf(buf, sizeof(buf), "You wear %s.", query_name(op, NULL));
			SET_FLAG(op, FLAG_APPLIED);
			change_abil(who, op);
			break;

		/* This part is needed for skill-tools */
		case SKILL:
			if (who->chosen_skill)
			{
				LOG(llevBug, "apply_special(): can't apply two skills\n");
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
						LOG(llevBug, "apply_special(SKILL): found unapplied tool with experience object\n");
					}
					else
					{
						link_player_skill(who, op);
					}

					draw_info_format(COLOR_WHITE, who, "You ready %s.", query_name(op, NULL));
					draw_info_format(COLOR_WHITE, who, "You can now use the skill: %s.", skills[op->stats.sp].name);
				}
				else
				{
					send_ready_skill(who, skills[op->stats.sp].name);
				}
			}

			SET_FLAG(op, FLAG_APPLIED);
			change_abil(who, op);
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

			draw_info_format(COLOR_WHITE, who, "You ready %s.", query_name(op, NULL));
			SET_FLAG(op, FLAG_APPLIED);

			if (op->type == BOW)
			{
				draw_info_format(COLOR_WHITE, who, "You will now fire %s with %s.", op->race ? op->race : "nothing", query_name(op, NULL));
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
		draw_info(COLOR_WHITE, who, buf);
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
			draw_info(COLOR_WHITE, who, "Oops, it feels deadly cold!");
		}
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
