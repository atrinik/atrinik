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
 * Player related functions. */

#include <global.h>
#ifndef WIN32
#include <pwd.h>
#endif
#include <sounds.h>
#include <newclient.h>

static archetype *get_player_archetype(archetype *at);
static int save_life(object *op);
static void remove_unpaid_objects(object *op, object *env);
static object *find_arrow_ext(object *op, const char *type, int tag);

/**
 * Loop through the player list and find player specified by plname.
 * @param plname The player name to find.
 * @return Player structure if found, NULL otherwise. */
player *find_player(char *plname)
{
	player *pl;

	for (pl = first_player; pl; pl = pl->next)
	{
		if (pl->ob && pl->state == ST_PLAYING && !strncasecmp(pl->ob->name, plname, MAX_NAME))
		{
			return pl;
		}
	}

	return NULL;
}

/**
 * Grab the Message of the Day from a file.
 *
 * First motd_custom is tried, and if that doesn't exist, motd is used
 * instead.
 * @param op Player object to print the message to. */
void display_motd(object *op)
{
	char buf[MAX_BUF];
	FILE *fp;

	snprintf(buf, sizeof(buf), "%s/motd_custom", settings.localdir);

	if ((fp = fopen(buf, "r")) == NULL)
	{
		snprintf(buf, sizeof(buf), "%s/motd", settings.localdir);

		if ((fp = fopen(buf, "r")) == NULL)
		{
			return;
		}
	}

	while (fgets(buf, MAX_BUF, fp) != NULL)
	{
		char *cp;

		if (buf[0] == '#' || buf[0] == '\n')
		{
			continue;
		}

		cp = strchr(buf, '\n');

		if (cp != NULL)
		{
			*cp = '\0';
		}

		new_draw_info(NDI_UNIQUE, op, buf);
	}

	fclose(fp);
	new_draw_info(NDI_UNIQUE, op, " ");
}

/**
 * Checks if player name contains illegal characters or not.
 * @param cp The player name.
 * @return 1 if the player name doesn't contain illegal characters, 0
 * otherwise. */
int playername_ok(char *cp)
{
	for (; *cp != '\0'; cp++)
	{
		if (!((*cp >= 'a' && *cp <= 'z') || (*cp >= 'A' && *cp <= 'Z')) && *cp != '-' && *cp != '_')
		{
			return 0;
		}
	}

	return 1;
}

/**
 * Returns the player structure. If 'p' is null, we create a new one.
 * Otherwise, we recycle the one that is passed.
 * @param p Player structure to recycle or NULL for new structure.
 * @return The player structure. */
static player *get_player(player *p)
{
	object *op = arch_to_object(get_player_archetype(NULL));
	int i;

	if (!p)
	{
		p = (player *) get_poolchunk(pool_player);
		memset(p, 0, sizeof(player));

		if (p == NULL)
		{
			LOG(llevError, "ERROR: get_player(): Out of memory\n");
		}

		if (!last_player)
		{
			first_player = last_player = p;
		}
		else
		{
			last_player->next = p;
			p->prev = last_player;
			last_player = p;
		}
	}
	else
	{
		/* Clears basically the entire player structure except
		 * for next and socket. */
		memset((void *) ((char *) p + offsetof(player, maplevel)), 0, sizeof(player) - offsetof(player, maplevel));
	}

#ifdef AUTOSAVE
	p->last_save_tick = 9999999;
#endif

	/* Init. respawn position */
	strcpy(p->savebed_map, first_map_path);

	p->firemode_type = p->firemode_tag1 = p->firemode_tag2 = -1;
	/* This is where we set up initial CONTR(op) */
	op->custom_attrset = p;
	p->ob = op;
	op->speed_left = 0.5;
	op->speed = 1.0;
	op->run_away = 0;
	op->quickslot = 0;

	p->state = ST_ROLL_STAT;

	p->target_hp = -1;
	p->target_hp_p = -1;
	p->gen_sp_armour = 0;
	p->last_speed = -1;
	p->shoottype = range_none;
	p->last_weapon_sp = -1;
	p->update_los = 1;

	FREE_AND_COPY_HASH(op->race, op->arch->clone.race);

	/* Would be better if '0' was not a defined spell */
	for (i = 0; i < NROFREALSPELLS; i++)
	{
		p->known_spells[i] = -1;
	}

	for (i = 0; i < MAX_QUICKSLOT; i++)
	{
		p->spell_quickslots[i] = SP_NO_SPELL;
	}

	p->chosen_spell = -1;

	/* We need to clear these to -1 and not zero - otherwise, if a player
	 * quits and starts a new character, we won't send new values to the
	 * client, as things like exp start at zero. */
	for (i = 0; i < MAX_EXP_CAT; i++)
	{
		p->last_skill_exp[i] = -1;
		p->last_skill_level[i] = -1;
	}

	/* Quick skill reminder for select hand weapon */
	p->set_skill_weapon = NO_SKILL_READY;
	p->set_skill_archery = NO_SKILL_READY;
	p->last_skill_index = -1;
	p->last_stats.exp = -1;

	return p;
}

/**
 * Free a player structure. Takes care of removing this player from the
 * list of players, and frees the socket for this player.
 * @param pl The player structure to free. */
void free_player(player *pl)
{
	if (pl->ob)
	{
		SET_FLAG(pl->ob, FLAG_NO_FIX_PLAYER);

		if (!QUERY_FLAG(pl->ob, FLAG_REMOVED))
		{
			remove_ob(pl->ob);
			check_walk_off(pl->ob, NULL, MOVE_APPLY_VANISHED);
		}
	}

	/* Free command permissions. */
	if (pl->cmd_permissions)
	{
		int i;

		for (i = 0; i < pl->num_cmd_permissions; i++)
		{
			if (pl->cmd_permissions[i])
			{
				free(pl->cmd_permissions[i]);
			}
		}

		free(pl->cmd_permissions);
	}

	/* Now remove from list of players. */
	if (pl->prev)
	{
		pl->prev->next = pl->next;
	}
	else
	{
		first_player = pl->next;
	}

	if (pl->next)
	{
		pl->next->prev = pl->prev;
	}
	else
	{
		last_player = pl->prev;
	}

	free_newsocket(&pl->socket);

	if (pl->ob)
	{
		destroy_object(pl->ob);
	}
}

/**
 * Tries to add a player on the connection in ns.
 *
 * All we can really get in this is some settings like host and display
 * mode.
 * @param ns The socket of this player.
 * @return 0. */
int add_player(socket_struct *ns)
{
	player *p = get_player(NULL);
	memcpy(&p->socket, ns, sizeof(socket_struct));

	/* now, we start the login procedure! */
	p->socket.status = Ns_Login;
	p->socket.below_clear = 0;
	p->socket.update_tile = 0;
	p->socket.look_position = 0;
	p->socket.inbuf.len = 0;

	get_name(p->ob);

	/* Avoid gc of the player */
	insert_ob_in_ob(p->ob, &void_container);

	return 0;
}

/**
 * Returns the next player archetype from archetype list. Not very
 * efficient routine, but used only when creating new players.
 * @note There MUST be at least one player archetype!
 * @param at The archetype list.
 * @return The archetype, if not found, fatal error. */
static archetype *get_player_archetype(archetype *at)
{
	archetype *start = at;

	for (; ;)
	{
		if (at == NULL || at->next == NULL)
		{
			at = first_archetype;
		}
		else
		{
			at = at->next;
		}

		if (at->clone.type == PLAYER)
		{
			return at;
		}

		if (at == start)
		{
			LOG(llevError, "ERROR: No player achetypes\n");
			exit(-1);
		}
	}
}

/**
 * Give initial items to object pl. This is used when player creates a
 * new character.
 * @param pl The player object.
 * @param items Treasure list of items to give. */
void give_initial_items(object *pl, treasurelist *items)
{
	object *op, *next = NULL;

	if (pl->randomitems)
	{
		create_treasure(items, pl, GT_ONLY_GOOD | GT_NO_VALUE, 1, T_STYLE_UNSET, ART_CHANCE_UNSET, 0, NULL);
	}

	for (op = pl->inv; op; op = next)
	{
		next = op->below;

		/* Forces get applied by default */
		if (op->type == FORCE)
		{
			SET_FLAG(op, FLAG_APPLIED);
		}

		/* We never give weapons/armour if they cannot be used by this
		 * player due to race restrictions */
		if (pl->type == PLAYER)
		{
			if ((!QUERY_FLAG(pl, FLAG_USE_ARMOUR) && (op->type == ARMOUR || op->type == BOOTS || op->type == CLOAK || op->type == HELMET || op->type == SHIELD || op->type == GLOVES || op->type == BRACERS || op->type == GIRDLE)) || (!QUERY_FLAG(pl, FLAG_USE_WEAPON) && op->type == WEAPON))
			{
				/* Inventory action */
				remove_ob(op);
				continue;
			}
		}

		/* Give starting characters identified, uncursed, and undamned
		 * items. Just don't identify gold or silver, or it won't be
		 * merged properly. */
		if (need_identify(op))
		{
			SET_FLAG(op, FLAG_IDENTIFIED);
			CLEAR_FLAG(op, FLAG_CURSED);
			CLEAR_FLAG(op, FLAG_DAMNED);
		}

		/* Apply initial armor */
		if (IS_ARMOR(op))
		{
			manual_apply(pl, op, 0);
		}

		if (op->type == ABILITY)
		{
			CONTR(pl)->known_spells[CONTR(pl)->nrofknownspells++] = op->stats.sp;
			remove_ob(op);
			continue;
		}
	}
}

/**
 * Send query to op's socket to get player name.
 * @param op Object to send the query to. */
void get_name(object *op)
{
	CONTR(op)->write_buf[0] = '\0';
	CONTR(op)->state = ST_GET_NAME;
	send_query(&CONTR(op)->socket, 0, "What is your name?\n:");
}

/**
 * Send query to op's socket to get player's password.
 * @param op Object to send the query to. */
void get_password(object *op)
{
	CONTR(op)->write_buf[0] = '\0';
	CONTR(op)->state = ST_GET_PASSWORD;
	send_query(&CONTR(op)->socket, CS_QUERY_HIDEINPUT, "What is your password?\n:");
}

/**
 * If this is a new character, we will need to confirm the password.
 * @param op Object to send the query to. */
void confirm_password(object *op)
{
	CONTR(op)->write_buf[0] = '\0';
	CONTR(op)->state = ST_CONFIRM_PASSWORD;
	send_query(&CONTR(op)->socket, CS_QUERY_HIDEINPUT, "Please type your password again.\n:");
}

/**
 * Find an arrow in the inventory and after that in the right type
 * container (quiver).
 * @param op Object to check.
 * @param type Ammunition type (bolts, arrows, etc).
 * @return Pointer to the found object, NULL if not found. */
object *find_arrow(object *op, const char *type)
{
	object *tmp = NULL;

	for (op = op->inv; op; op = op->below)
	{
		if (!tmp && op->type == CONTAINER && op->race == type && QUERY_FLAG(op, FLAG_APPLIED))
		{
			tmp = find_arrow(op, type);
		}
		else if (op->type == ARROW && op->race == type)
		{
			return op;
		}
	}

	return tmp;
}

/**
 * Player fires a bow.
 * @param op Object firing.
 * @param dir Direction to fire. */
static void fire_bow(object *op, int dir)
{
	object *left_cont, *bow, *arrow = NULL, *left, *tmp_op;
	tag_t left_tag;

	/* If no dir is specified, attempt to find get the direction from
	 * player's target. */
	if (!dir && op->type == PLAYER && OBJECT_VALID(CONTR(op)->target_object, CONTR(op)->target_object_count))
	{
		rv_vector range_vector;
		dir = get_dir_to_target(op, CONTR(op)->target_object, &range_vector);
	}

	if (!dir)
	{
		new_draw_info(NDI_UNIQUE, op, "You can't shoot yourself!");
		return;
	}

	bow = CONTR(op)->equipment[PLAYER_EQUIP_BOW];

	if (!bow)
	{
		LOG(llevBug, "BUG: fire_bow(): bow without activated bow (%s - %d).\n", op->name, dir);
	}

	if (!bow->race)
	{
		new_draw_info_format(NDI_UNIQUE, op, "Your %s is broken.", bow->name);
		return;
	}

	if ((arrow = find_arrow_ext(op, bow->race, CONTR(op)->firemode_tag2)) == NULL)
	{
		new_draw_info_format(NDI_UNIQUE, op, "You have no %s left.", bow->race);
		return;
	}

	if (wall(op->map, op->x + freearr_x[dir], op->y + freearr_y[dir]))
	{
		new_draw_info(NDI_UNIQUE, op, "Something is in the way.");
		return;
	}

	/* This should not happen, but sometimes does */
	if (arrow->nrof == 0)
	{
		LOG(llevDebug, "BUG?: arrow->nrof == 0 in fire_bow() (%s)\n", query_name(arrow, NULL));
		remove_ob(arrow);
		return;
	}

	/* These are arrows left to the player */
	left = arrow;
	left_tag = left->count;
	left_cont = left->env;
	arrow = get_split_ob(arrow, 1, NULL, 0);
	set_owner(arrow, op);
	arrow->direction = dir;
	arrow->x = op->x;
	arrow->y = op->y;
	arrow->speed = 1;

	/* Now the trick: we transfer the shooting speed in the used
	 * skill - that will allow us to use "set_skill_speed() as global
	 * function. */
	op->chosen_skill->stats.maxsp = bow->stats.sp + arrow->last_grace;
	update_ob_speed(arrow);
	arrow->speed_left = 0;
	SET_ANIMATION(arrow, (NUM_ANIMATIONS(arrow) / NUM_FACINGS(arrow)) * dir);
	/* Save original wc and dam */
	arrow->last_heal = arrow->stats.wc;
	/* Will be put back in fix_arrow() */
	arrow->stats.hp = arrow->stats.dam;

	/* Now we do this: arrow wc = wc base from skill + (wc arrow + magic) + (wc range weapon bonus + magic) */
	if ((tmp_op = SK_skill(op)))
	{
		/* wc is in last heal */
		arrow->stats.wc += tmp_op->last_heal;
	}
	else
	{
		arrow->stats.wc += 10;
	}

	/* Now we determine how many tiles the arrow will fly. Again we use
	 * the skill base and add arrow + weapon values - but no magic add
	 * here. */
	arrow->last_sp = tmp_op->last_sp + bow->last_sp + arrow->last_sp;

	/* Add in all our wc bonus */
	arrow->stats.wc += (bow->magic + arrow->magic + SK_level(op) + thaco_bonus[op->stats.Dex] + bow->stats.wc);
	arrow->stats.wc_range = bow->stats.wc_range;
	arrow->stats.dam += dam_bonus[op->stats.Str] / 2 + bow->stats.dam + bow->magic + arrow->magic;
	arrow->stats.dam = FABS((int) ((float) (arrow->stats.dam * LEVEL_DAMAGE(SK_level(op)))));

	/* Adjust with the lower of condition */
	if (bow->item_condition > arrow->item_condition)
	{
		arrow->stats.dam = (sint16) (((float) arrow->stats.dam / 100.0f) * (float) arrow->item_condition);
	}
	else
	{
		arrow->stats.dam = (sint16) (((float) arrow->stats.dam / 100.0f) * (float) bow->item_condition);
	}

	arrow->level = SK_level(op);
	arrow->map = op->map;
	SET_MULTI_FLAG(arrow, FLAG_FLYING);
	SET_FLAG(arrow, FLAG_IS_MISSILE);
	SET_FLAG(arrow, FLAG_FLY_ON);
	SET_FLAG(arrow, FLAG_WALK_ON);
	/* Temporary buffer for "tiles to fly" */
	arrow->stats.grace = arrow->last_sp;
	/* Reflection timer */
	arrow->stats.maxgrace = 60 + (RANDOM() % 12);
	play_sound_map(op->map, op->x, op->y, SOUND_FIRE_ARROW, SOUND_NORMAL);

	if (insert_ob_in_map(arrow, op->map, op, 0))
	{
		move_arrow(arrow);
	}

	if (was_destroyed(left, left_tag))
	{
		esrv_del_item(CONTR(op), left_tag, left_cont);
	}
	else
	{
		esrv_send_item(op, left);
	}
}

/**
 * Fire command for spells, range, throwing, etc.
 * @param op Object firing this.
 * @param dir Direction to fire to. */
void fire(object *op, int dir)
{
	object *weap = NULL;
	int spellcost = 0;

	/* A check for players, make sure things are groovy. This routine
	 * will change the skill of the player as appropriate in order to
	 * fire whatever is requested. In the case of spells (range_magic)
	 * it handles whether cleric or mage spell is requested to be
	 * cast. */
	if (op->type == PLAYER)
	{
		if (CONTR(op)->firemode_type == FIRE_MODE_NONE)
		{
			return;
		}

		if (CONTR(op)->firemode_type == FIRE_MODE_BOW)
		{
			CONTR(op)->shoottype = range_bow;
		}
		else if (CONTR(op)->firemode_type == FIRE_MODE_THROW)
		{
			object *tmp;

			/* Insert here test for more throwing skills */
			if (!change_skill(op, SK_THROWING))
			{
				return;
			}

			/* Special case - we must redirect the fire cmd to throwing something */
			tmp = find_throw_tag(op, (tag_t) CONTR(op)->firemode_tag1);

			if (tmp)
			{
				if (!check_skill_action_time(op, op->chosen_skill))
				{
					return;
				}

				do_throw(op, tmp, dir);
				get_skill_time(op, op->chosen_skill->stats.sp);
				CONTR(op)->action_timer = (float) (CONTR(op)->action_range - global_round_tag) / (1000000 / MAX_TIME) * 1000.0f;

				if (CONTR(op)->last_action_timer > 0)
				{
					CONTR(op)->action_timer *= -1;
				}
			}

			return;
		}
		else if (CONTR(op)->firemode_type == FIRE_MODE_SPELL)
		{
			CONTR(op)->shoottype = range_magic;
		}
		else if (CONTR(op)->firemode_type == FIRE_MODE_WAND)
		{
			/* We do a jump in fire wand if we haven one */
			CONTR(op)->shoottype = range_wand;
		}
		else if (CONTR(op)->firemode_type == FIRE_MODE_SKILL)
		{
			command_rskill(op, CONTR(op)->firemode_name);
			CONTR(op)->shoottype = range_skill;
		}
		else if (CONTR(op)->firemode_type == FIRE_MODE_SUMMON)
		{
			CONTR(op)->shoottype = range_scroll;
		}
		else
		{
			CONTR(op)->shoottype = range_none;
		}

		if (!check_skill_to_fire(op))
		{
			return;
		}
	}

	switch (CONTR(op)->shoottype)
	{
		case range_none:
			return;

		case range_bow:
			if (CONTR(op)->firemode_tag2 != -1)
			{
				/* Still need to recover from range action? */
				if (!check_skill_action_time(op, op->chosen_skill))
				{
					return;
				}

				fire_bow(op, dir);
				get_skill_time(op, op->chosen_skill->stats.sp);
				CONTR(op)->action_timer = (float) (CONTR(op)->action_range - global_round_tag) / (1000000 / MAX_TIME) * 1000.0f;

				if (CONTR(op)->last_action_timer > 0)
				{
					CONTR(op)->action_timer *= -1;
				}
			}

			return;

		/* Casting spells */
		case range_magic:
			if (!check_skill_action_time(op, op->chosen_skill))
			{
				return;
			}

			spellcost = cast_spell(op, op, dir, CONTR(op)->chosen_spell, 0, spellNormal, NULL);

			if (spells[CONTR(op)->chosen_spell].type == SPELL_TYPE_PRIEST)
			{
				op->stats.grace -= spellcost;
			}
			else
			{
				op->stats.sp -= spellcost;
			}

			/* Only change the action timer if the spell required mana/grace cost (ie, was successful). */
			if (spellcost)
			{
				CONTR(op)->action_casting = ROUND_TAG + spells[CONTR(op)->chosen_spell].time;
				CONTR(op)->action_timer = (float) (CONTR(op)->action_casting - global_round_tag) / (1000000 / MAX_TIME) * 1000.0f;

				if (CONTR(op)->last_action_timer > 0)
				{
					CONTR(op)->action_timer *= -1;
				}
			}

			return;

		case range_wand:
			for (weap = op->inv; weap != NULL; weap = weap->below)
			{
				if (weap->type == WAND && QUERY_FLAG(weap, FLAG_APPLIED))
				{
					break;
				}
			}

			if (weap == NULL)
			{
				CONTR(op)->shoottype = range_rod;
				goto trick_jump;
			}

			if (!check_skill_action_time(op, op->chosen_skill))
			{
				return;
			}

			if (weap->stats.food <= 0)
			{
				play_sound_player_only(CONTR(op), SOUND_WAND_POOF, SOUND_NORMAL, 0, 0);
				new_draw_info(NDI_UNIQUE, op, "The wand says poof.");
				return;
			}

			new_draw_info(NDI_UNIQUE, op, "fire wand");

			if (cast_spell(op, weap, dir, weap->stats.sp, 0, spellWand, NULL))
			{
				/* You now know something about it */
				SET_FLAG(op, FLAG_BEEN_APPLIED);

				if (!(--weap->stats.food))
				{
					object *tmp;

					if (weap->arch)
					{
						CLEAR_FLAG(weap, FLAG_ANIMATE);
						weap->face = weap->arch->clone.face;
						weap->speed = 0;
						update_ob_speed(weap);
					}

					if ((tmp = is_player_inv(weap)))
					{
						esrv_update_item(UPD_ANIM, tmp, weap);
					}
				}
			}

			get_skill_time(op, op->chosen_skill->stats.sp);
			CONTR(op)->action_timer = (float) (CONTR(op)->action_range - global_round_tag) / (1000000 / MAX_TIME) * 1000.0f;

			if (CONTR(op)->last_action_timer > 0)
			{
				CONTR(op)->action_timer *= -1;
			}

			return;

		case range_rod:
		case range_horn:
trick_jump:
			for (weap = op->inv; weap != NULL; weap = weap->below)
			{
				if (QUERY_FLAG(weap, FLAG_APPLIED) && weap->type == (CONTR(op)->shoottype == range_rod ? ROD : HORN))
				{
					break;
				}
			}

			if (weap == NULL)
			{
				if (CONTR(op)->shoottype == range_rod)
				{
					CONTR(op)->shoottype = range_horn;
					goto trick_jump;
				}
				else
				{
					new_draw_info_format(NDI_UNIQUE, op, "You have no tool readied.");
					return;
				}
			}

			if (!check_skill_action_time(op, op->chosen_skill))
			{
				return;
			}

			/* If the device level is higher than player's skill + 5 */
			if (weap->level > op->chosen_skill->level + 5)
			{
				int level_difference = weap->level - (op->chosen_skill->level + 5);

				/* If the level difference isn't so high, give it a small chance to succeed */
				if (level_difference > 0 && (level_difference > 10 || RANDOM() % weap->level != RANDOM() % (op->chosen_skill->level + 5)))
				{
					new_draw_info_format(NDI_UNIQUE, op, "The %s is impossible to handle for you!", weap->name);
					return;
				}
			}

			if (weap->stats.hp < spells[weap->stats.sp].sp)
			{
				play_sound_player_only(CONTR(op), SOUND_WAND_POOF, SOUND_NORMAL, 0, 0);

				if (CONTR(op)->shoottype == range_rod)
				{
					new_draw_info(NDI_UNIQUE, op, "The rod whines for a while, but nothing happens.");
				}
				else
				{
					new_draw_info(NDI_UNIQUE, op, "No matter how hard you try you can't get another note out.");
				}

				return;
			}

			if (cast_spell(op, weap, dir, weap->stats.sp, 0, CONTR(op)->shoottype == range_rod ? spellRod : spellHorn, NULL))
			{
				/* You now know something about it */
				SET_FLAG(op, FLAG_BEEN_APPLIED);
				drain_rod_charge(weap);
			}

			get_skill_time(op, op->chosen_skill->stats.sp);
			CONTR(op)->action_timer = (float) (CONTR(op)->action_range - global_round_tag) / (1000000 / MAX_TIME) * 1000.0f;

			if (CONTR(op)->last_action_timer > 0)
			{
				CONTR(op)->action_timer *= -1;
			}

			return;

		/* Control summoned monsters from scrolls */
		case range_scroll:
			CONTR(op)->shoottype = range_none;
			CONTR(op)->chosen_spell = -1;
			return;

		case range_skill:
			if (!op->chosen_skill)
			{
				if (op->type == PLAYER)
					new_draw_info(NDI_UNIQUE, op, "You have no applicable skill to use.");
				return;
			}

			if (op->chosen_skill->sub_type != ST1_SKILL_USE)
			{
				new_draw_info(NDI_UNIQUE, op, "You can't use this skill in this way.");
			}
			else
			{
				do_skill(op, dir);
			}

			return;

		default:
			new_draw_info(NDI_UNIQUE, op, "Illegal shoot type.");
			return;
	}
}

/**
 * Move a player.
 * @param op Player object.
 * @param dir Direction to move to.
 * @return Always returns 0. */
int move_player(object *op, int dir)
{
	CONTR(op)->praying = 0;

	if (op->map == NULL || op->map->in_memory != MAP_IN_MEMORY)
	{
		return 0;
	}

	if (dir)
	{
		op->facing = dir;
	}

	if (QUERY_FLAG(op, FLAG_CONFUSED) && dir)
	{
		dir = get_randomized_dir(dir);
	}

	op->anim_moving_dir = -1;
	op->anim_enemy_dir = -1;
	op->anim_last_facing = -1;

	/* firemode is set from client command fire xx xx xx */
	if (CONTR(op)->firemode_type != -1)
	{
		fire(op, dir);

		if (dir)
		{
			op->anim_enemy_dir = dir;
		}
		else
		{
			op->anim_enemy_dir = op->facing;
		}

		CONTR(op)->fire_on = 0;
	}
	else
	{
		if (!move_ob(op, dir, op))
		{
			op->anim_enemy_dir = dir;
		}
		else
		{
			op->anim_moving_dir = dir;
		}
	}

	if (QUERY_FLAG(op, FLAG_ANIMATE))
	{
		if (op->anim_enemy_dir == -1 && op->anim_moving_dir == -1)
		{
			op->anim_last_facing = dir;
		}

		animate_object(op, 0);
	}

	return 0;
}

/**
 * This is similar to handle_player(), but is only used by the new
 * client/server stuff.
 *
 * This is sort of special, in that the new client/server actually uses
 * the new speed values for commands.
 * @param pl Player to handle.
 * @retval -1 Player is invalid.
 * @retval 0 No more actions to do.
 * @retval 1 There are more actions we can do. */
int handle_newcs_player(player *pl)
{
	object *op = pl->ob;

	if (!op || !OBJECT_ACTIVE(op))
	{
		return -1;
	}

	handle_client(&pl->socket, pl);

	if (!op || !OBJECT_ACTIVE(op) || pl->socket.status == Ns_Dead)
	{
		return -1;
	}

	/* Check speed. */
	if (op->speed_left < 0.0f)
	{
		return 0;
	}

	/* If we are here, we're never paralyzed anymore. */
	CLEAR_FLAG(op, FLAG_PARALYZED);

	if (op->direction && (CONTR(op)->run_on || CONTR(op)->fire_on))
	{
		/* All move commands take 1 tick, at least for now. */
		op->speed_left--;
		move_player(op, op->direction);

		if (op->speed_left > 0)
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}

	return 0;
}

/**
 * Can the player be saved by an item?
 * @param op Player to try to save.
 * @retval 1 Player had his life saved by an item, first item saving life
 * is removed.
 * @retval 0 Player had no life-saving item. */
static int save_life(object *op)
{
	object *tmp;

	if (!QUERY_FLAG(op, FLAG_LIFESAVE))
	{
		return 0;
	}

	for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
	{
		if (QUERY_FLAG(tmp, FLAG_APPLIED) && QUERY_FLAG(tmp, FLAG_LIFESAVE))
		{
			play_sound_map(op->map, op->x, op->y, SOUND_OB_EVAPORATE, SOUND_NORMAL);
			new_draw_info_format(NDI_UNIQUE, op, "Your %s vibrates violently, then evaporates.", query_name(tmp, NULL));

			if (CONTR(op))
			{
				esrv_del_item(CONTR(op), tmp->count, tmp->env);
			}

			remove_ob(tmp);
			CLEAR_FLAG(op, FLAG_LIFESAVE);

			if (op->stats.hp < 0)
			{
				op->stats.hp = op->stats.maxhp;
			}

			if (op->stats.food < 0)
			{
				op->stats.food = 999;
			}

			/* Bring him home. */
			enter_player_savebed(op);
			return 1;
		}
	}

	LOG(llevBug, "BUG: save_life(): LIFESAVE set without applied object.\n");
	CLEAR_FLAG(op, FLAG_LIFESAVE);
	/* Bring him home. */
	enter_player_savebed(op);
	return 0;
}

/**
 * This goes through the inventory and removes unpaid objects, and puts
 * them back in the map (location and map determined by values of env).
 * This function will descend into containers.
 * @param op Object to start the search from.
 * @param env Map location determined by this object. */
static void remove_unpaid_objects(object *op, object *env)
{
	object *next;

	while (op)
	{
		/* Make sure we have a good value, in case
		 * we remove object 'op' */
		next = op->below;

		if (QUERY_FLAG(op, FLAG_UNPAID))
		{
			remove_ob(op);
			op->x = env->x;
			op->y = env->y;
			insert_ob_in_map(op, env->map, NULL, 0);
		}
		else if (op->inv)
		{
			remove_unpaid_objects(op->inv, env);
		}

		op = next;
	}
}

/**
 * Regenerate player's hp/mana/grace, decrease food, etc.
 *
 * We will only regenerate HP and mana if the player has some food in their
 * stomach.
 * @param op Player. */
void do_some_living(object *op)
{
	int last_food = op->stats.food;
	int gen_hp, gen_sp, gen_grace;
	int rate_hp = 2000;
	int rate_sp = 1200;
	int rate_grace = 400;

	if (CONTR(op)->state != ST_PLAYING)
	{
		return;
	}

	gen_hp = (CONTR(op)->gen_hp * (rate_hp / 20)) + (op->stats.maxhp / 4);
	gen_sp = (CONTR(op)->gen_sp * (rate_sp / 20)) + op->stats.maxsp;
	gen_grace = (CONTR(op)->gen_grace * (rate_grace / 20)) + op->stats.maxgrace;

	gen_sp = gen_sp * 10 / MAX(CONTR(op)->gen_sp_armour, 10);

	/* Update client's regen rates. */
	CONTR(op)->gen_client_hp = ((float) (1000000 / MAX_TIME) / ((float) rate_hp / (MAX(gen_hp, 20) + 10))) * 10.0f;
	CONTR(op)->gen_client_sp = ((float) (1000000 / MAX_TIME) / ((float) rate_sp / (MAX(gen_sp, 20) + 10))) * 10.0f;
	CONTR(op)->gen_client_grace = ((float) (1000000 / MAX_TIME) / ((float) rate_grace / (MAX(gen_grace, 20) + 10))) * 10.0f;

	/* Regenerate hit points. */
	if (--op->last_heal < 0)
	{
		if (op->stats.hp < op->stats.maxhp && op->stats.food)
		{
			op->stats.hp++;

			/* DMs do not consume food. */
			if (!QUERY_FLAG(op, FLAG_WIZ))
			{
				op->stats.food--;

				if (CONTR(op)->digestion < 0)
				{
					op->stats.food += CONTR(op)->digestion;
				}
				else if (CONTR(op)->digestion > 0 && rndm(0, CONTR(op)->digestion))
				{
					op->stats.food = last_food;
				}
			}
		}

		op->last_heal = rate_hp / (MAX(gen_hp, 20) + 10);
	}

	/* Regenerate mana. */
	if (--op->last_sp < 0)
	{
		if (op->stats.sp < op->stats.maxsp && op->stats.food)
		{
			op->stats.sp++;

			/* DMs do not consume food. */
			if (!QUERY_FLAG(op, FLAG_WIZ))
			{
				op->stats.food--;

				if (CONTR(op)->digestion < 0)
				{
					op->stats.food += CONTR(op)->digestion;
				}
				else if (CONTR(op)->digestion > 0 && rndm(0, CONTR(op)->digestion))
				{
					op->stats.food = last_food;
				}
			}
		}

		op->last_sp = rate_sp / (MAX(gen_sp, 20) + 10);
	}

	/* Stop and pray. */
	if (CONTR(op)->praying && !CONTR(op)->was_praying)
	{
		if (op->stats.grace < op->stats.maxgrace)
		{
			object *god = find_god(determine_god(op));

			if (god)
			{
				if (CONTR(op)->combat_mode)
				{
					new_draw_info_format(NDI_UNIQUE, op, "You stop combat and start praying to %s...", god->name);
					CONTR(op)->combat_mode = 0;
					send_target_command(CONTR(op));
				}
				else
				{
					new_draw_info_format(NDI_UNIQUE, op, "You start praying to %s...", god->name);
				}

				CONTR(op)->was_praying = 1;
			}
			else
			{
				new_draw_info(NDI_UNIQUE, op, "You worship no deity to pray to!");
				CONTR(op)->praying = 0;
			}

			op->last_grace = rate_grace / (MAX(gen_grace, 20) + 10);
		}
		else
		{
			CONTR(op)->praying = 0;
			CONTR(op)->was_praying = 0;
		}
	}
	else if (!CONTR(op)->praying && CONTR(op)->was_praying)
	{
		new_draw_info(NDI_UNIQUE, op, "You stop praying.");
		CONTR(op)->was_praying = 0;
		op->last_grace = rate_grace / (MAX(gen_grace, 20) + 10);
	}

	/* Regenerate grace. */
	if (CONTR(op)->praying)
	{
		if (--op->last_grace < 0)
		{
			if (op->stats.grace < op->stats.maxgrace)
			{
				op->stats.grace++;
			}

			if (op->stats.grace >= op->stats.maxgrace)
			{
				op->stats.grace = op->stats.maxgrace;
				new_draw_info(NDI_UNIQUE, op, "You are full of grace and stop praying.");
				CONTR(op)->was_praying = 0;
			}

			op->last_grace = rate_grace / (MAX(gen_grace, 20) + 10);
		}
	}

	/* Digestion */
	if (--op->last_eat < 0)
	{
		int bonus = MAX(CONTR(op)->digestion, 0);
		int penalty = MAX(-CONTR(op)->digestion, 0);

		if (CONTR(op)->gen_hp > 0)
		{
			op->last_eat = 25 * (1 + bonus) / (CONTR(op)->gen_hp + penalty + 1);
		}
		else
		{
			op->last_eat = 25 * (1 + bonus) / (penalty + 1);
		}

		/* DMs do not consume food. */
		if (!QUERY_FLAG(op, FLAG_WIZ))
		{
			op->stats.food--;
		}
	}

	if (op->stats.food < 0 && op->stats.hp >= 0)
	{
		object *tmp, *flesh = NULL;

		for (tmp = op->inv; tmp; tmp = tmp->below)
		{
			if (!QUERY_FLAG(tmp, FLAG_UNPAID))
			{
				if (tmp->type == FOOD || tmp->type == DRINK || tmp->type == POISON)
				{
					new_draw_info(NDI_UNIQUE, op, "You blindly grab for a bite of food.");
					manual_apply(op, tmp, 0);

					if (op->stats.food >= 0 || op->stats.hp < 0)
					{
						break;
					}
				}
				else if (tmp->type == FLESH)
				{
					flesh = tmp;
				}
			}
		}

		/* If player is still starving, it means they don't have any food, so
		 * eat flesh instead. */
		if (op->stats.food < 0 && op->stats.hp >= 0 && flesh)
		{
			new_draw_info(NDI_UNIQUE, op, "You blindly grab for a bite of food.");
			manual_apply(op, flesh, 0);
		}
	}

	while (op->stats.food < 0 && op->stats.hp > 0)
	{
		op->stats.food++;
		op->stats.hp--;
	}

	if ((op->stats.hp <= 0 || op->stats.food < 0) && !QUERY_FLAG(op, FLAG_WIZ))
	{
		new_draw_info_format(NDI_ALL, NULL, "%s starved to death.", op->name);
		strcpy(CONTR(op)->killer, "starvation");
		kill_player(op);
	}
}

/**
 * If the player should die (lack of hp, food, etc), we call this.
 *
 * Will remove diseases, apply death penalties, and so on.
 * @param op The player in jeopardy. */
void kill_player(object *op)
{
	char buf[HUGE_BUF];
	int x, y, i;
	/* this is for resurrection */
	mapstruct *map;
	object *tmp;
	int z;
	int num_stats_lose;
	int lost_a_stat;
	int lose_this_stat;
	int this_stat;

	if (pvp_area(NULL, op))
	{
		new_draw_info(NDI_UNIQUE | NDI_NAVY, op, "You have been defeated in combat!");
		new_draw_info(NDI_UNIQUE | NDI_NAVY, op, "Local medics have saved your life...");

		/* Restore player */
		cast_heal(op, MAXLEVEL, op, SP_CURE_POISON);
		/* Remove any disease */
		cure_disease(op, NULL);
		op->stats.hp = op->stats.maxhp;

		if (op->stats.food <= 0)
		{
			op->stats.food = 999;
		}

		/* Create a bodypart-trophy to make the winner happy */
		tmp = arch_to_object(find_archetype("finger"));

		if (tmp != NULL)
		{
			char race[MAX_BUF];

			snprintf(buf, sizeof(buf), "%s's finger", op->name);
			FREE_AND_COPY_HASH(tmp->name, buf);
			snprintf(buf, sizeof(buf), "This finger has been cut off %s the %s, when %s was defeated at level %d by %s.", op->name, player_get_race_class(op, race, sizeof(race)), gender_subjective[object_get_gender(op)], op->level, CONTR(op)->killer[0] == '\0' ? "something nasty" : CONTR(op)->killer);
			FREE_AND_COPY_HASH(tmp->msg, buf);
			tmp->value = 0, tmp->material = 0, tmp->type = 0;
			tmp->x = op->x, tmp->y = op->y;
			insert_ob_in_map(tmp, op->map, op, 0);
		}

		/* Teleport defeated player to new destination */
		transfer_ob(op, MAP_ENTER_X(op->map), MAP_ENTER_Y(op->map), 0, NULL, NULL);
		return;
	}

	if (save_life(op))
	{
		return;
	}

	/* Trigger the DEATH event */
	if (trigger_event(EVENT_DEATH, NULL, op, NULL, NULL, 0, 0, 0, SCRIPT_FIX_ALL))
	{
		return;
	}

	/* Trigger the global GDEATH event */
	trigger_global_event(EVENT_GDEATH, NULL, op);

	play_sound_player_only(CONTR(op), SOUND_PLAYER_DIES, SOUND_NORMAL, 0, 0);

	/* Save the map location for corpse, gravestone */
	x = op->x;
	y = op->y;
	map = op->map;

	/* Basically two ways to go - remove a stat permanently, or just
	 * make it depletion.  This bunch of code deals with that aspect
	 * of death. */
	if (settings.balanced_stat_loss)
	{
		/* If stat loss is permanent, lose one stat only. */
		/* Lower level chars don't lose as many stats because they suffer more
		   if they do. */
		if (settings.stat_loss_on_death)
		{
			num_stats_lose = 1;
		}
		else
		{
			num_stats_lose = 1 + op->level / BALSL_NUMBER_LOSSES_RATIO;
		}
	}
	else
	{
		num_stats_lose = 1;
	}

	lost_a_stat = 0;

	/* Only decrease stats if you are level 3 or higher. */
	for (z = 0; z < num_stats_lose; z++)
	{
		if (settings.stat_loss_on_death && op->level > 3)
		{
			/* Pick a random stat and take a point off it. Tell the
			 * player what he lost. */
			i = rndm(1, NUM_STATS) - 1;
			change_attr_value(&(op->stats), i, -1);
			check_stat_bounds(&(op->stats));
			change_attr_value(&(CONTR(op)->orig_stats), i, -1);
			check_stat_bounds(&(CONTR(op)->orig_stats));
			new_draw_info(NDI_UNIQUE, op, lose_msg[i]);
			lost_a_stat = 1;
		}
		else if (op->level > 3)
		{
			/* Deplete a stat */
			archetype *deparch = find_archetype("depletion");
			object *dep;

			i = rndm(1, NUM_STATS) - 1;
			dep = present_arch_in_ob(deparch, op);

			if (!dep)
			{
				dep = arch_to_object(deparch);
				insert_ob_in_ob(dep, op);
			}

			lose_this_stat = 1;

			if (settings.balanced_stat_loss)
			{
				/* Get the stat that we're about to deplete. */
				this_stat = get_attr_value(&(dep->stats), i);

				if (this_stat < 0)
				{
					int loss_chance = 1 + op->level / BALSL_LOSS_CHANCE_RATIO;
					int keep_chance = this_stat * this_stat;

					/* Yes, I am paranoid. Sue me. */
					if (keep_chance < 1)
					{
						keep_chance = 1;
					}

					/* There is a maximum depletion total per level. */
					if (this_stat < -1 - op->level / BALSL_MAX_LOSS_RATIO)
					{
						lose_this_stat = 0;
					}
					else
					{
						/* Take loss chance vs keep chance to see if we retain the stat. */
						if (rndm(0, loss_chance + keep_chance - 1) < keep_chance)
						{
							lose_this_stat = 0;
						}
					}
				}
			}

			if (lose_this_stat)
			{
				this_stat = get_attr_value(&(dep->stats), i);

				/* We could try to do something clever like find another
				 * stat to reduce if this fails.  But chances are, if
				 * stats have been depleted to -50, all are pretty low
				 * and should be roughly the same, so it shouldn't make a
				 * difference. */
				if (this_stat >= -50)
				{
					change_attr_value(&(dep->stats), i, -1);
					SET_FLAG(dep, FLAG_APPLIED);
					new_draw_info(NDI_UNIQUE, op, lose_msg[i]);
					fix_player(op);
					lost_a_stat = 1;
				}
			}
		}
	}

	/* If no stat lost, tell the player. */
	if (!lost_a_stat)
	{
		const char *god = determine_god(op);

		if (god && god != shstr_cons.none)
		{
			new_draw_info_format(NDI_UNIQUE, op, "For a brief moment you feel the holy presence of %s protecting you.", god);
		}
		else
		{
			new_draw_info(NDI_UNIQUE, op, "For a brief moment you feel a holy presence protecting you.");
		}
	}

	/* Put a gravestone up where the character 'almost' died. */
	tmp = arch_to_object(find_archetype("gravestone"));
	snprintf(buf, sizeof(buf), "%s's gravestone", op->name);
	FREE_AND_COPY_HASH(tmp->name, buf);
	FREE_AND_COPY_HASH(tmp->msg, gravestone_text(op));
	tmp->x = op->x, tmp->y = op->y;
	insert_ob_in_map(tmp, op->map, NULL, 0);

	/* Subtract the experience points, if we died because of food give us
	 * food, and reset HP... */

	/* Remove any poisoning the character may be suffering. */
	cast_heal(op, MAXLEVEL, op, SP_CURE_POISON);
	/* Remove any disease */
	cure_disease(op, NULL);

	/* Apply death experience penalty. */
	apply_death_exp_penalty(op);

	if (op->stats.food < 0)
	{
		op->stats.food = 900;
	}

	op->stats.hp = op->stats.maxhp;
	op->stats.sp = op->stats.maxsp;
	op->stats.grace = op->stats.maxgrace;

	hiscore_check(op, 1);

	/* Otherwise the highscore can get entries like 'xxx was killed by pudding
	 * on map Wilderness' even if they were killed in a dungeon. */
	CONTR(op)->killer[0] = '\0';

	/* Check to see if the player is in a shop. Ii so, then check to see
	 * if the player has any unpaid items. If so, remove them and put
	 * them back in the map. */
	tmp = get_map_ob(op->map, op->x, op->y);

	if (tmp && tmp->type == SHOP_FLOOR)
	{
		remove_unpaid_objects(op->inv, op);
	}

	/* Move player to his current respawn position (last savebed). */
	enter_player_savebed(op);

	/* Show a nasty message */
	new_draw_info(NDI_UNIQUE, op, "YOU HAVE DIED.");
	save_player(op, 1);
	return;
}

/**
 * Handles object throwing objects of type "DUST".
 * @todo This function needs to be rewritten. Works for area effect
 * spells only now.
 * @param op Object throwing.
 * @param throw_ob What to throw.
 * @param dir Direction to throw into. */
void cast_dust(object *op, object *throw_ob, int dir)
{
	archetype *arch = NULL;

	if (!(spells[throw_ob->stats.sp].flags & SPELL_DESC_DIRECTION))
	{
		LOG(llevBug, "DEBUG: Warning, dust %s is not AE spell!!\n", query_name(throw_ob, NULL));
		return;
	}

	if (spells[throw_ob->stats.sp].archname)
	{
		arch = find_archetype(spells[throw_ob->stats.sp].archname);
	}

	/* Casting POTION 'dusts' is really use_magic_item skill */
	if (op->type == PLAYER && throw_ob->type == POTION && !change_skill(op, SK_USE_MAGIC_ITEM))
	{
		return;
	}

	if (throw_ob->type == POTION && arch != NULL)
	{
		cast_cone(op, throw_ob, dir, 10, throw_ob->stats.sp, arch);
	}
	/* dust_effect */
	else if ((arch = find_archetype("dust_effect")) != NULL)
	{
		cast_cone(op, throw_ob, dir, 1, 0, arch);
	}
	/* Problem occured! */
	else
	{
		LOG(llevBug, "BUG: cast_dust() can't find an archetype to use!\n");
	}

	if (op->type == PLAYER && arch)
	{
		new_draw_info_format(NDI_UNIQUE, op, "You cast %s.", query_name(throw_ob, NULL));
	}

	if (!QUERY_FLAG(throw_ob, FLAG_REMOVED))
	{
		destruct_ob(throw_ob);
	}
}

/**
 * Test for PVP area.
 *
 * If only one object is given, it tests for that. Otherwise if two
 * objects are given, both objects must be in PVP area.
 *
 * Considers parties.
 * @param attacker First object.
 * @param victim Second object.
 * @return 1 if PVP is possible, 0 otherwise. */
int pvp_area(object *attacker, object *victim)
{
	/* No attacking of party members. */
	if (attacker && victim && attacker->type == PLAYER && victim->type == PLAYER && CONTR(attacker)->party != NULL && CONTR(victim)->party != NULL && CONTR(attacker)->party == CONTR(victim)->party)
	{
		return 0;
	}

	if (attacker && victim && attacker == victim)
	{
		return 0;
	}

	if (attacker && attacker->map)
	{
		if (!(attacker->map->map_flags & MAP_FLAG_PVP) || GET_MAP_FLAGS(attacker->map, attacker->x, attacker->y) & P_NO_PVP)
		{
			return 0;
		}
	}

	if (victim && victim->map)
	{
		if (!(victim->map->map_flags & MAP_FLAG_PVP) || GET_MAP_FLAGS(victim->map, victim->x, victim->y) & P_NO_PVP)
		{
			return 0;
		}
	}

	return 1;
}

/**
 * Extended find arrow version, using tag and containers.
 *
 * Find an arrow in the inventory and after that in the right type
 * container (quiver).
 * @param op Player.
 * @param type Type of the ammunition (arrows, bolts, etc).
 * @param tag Firemode tag.
 * @return Pointer to the arrow, NULL if not found. */
static object *find_arrow_ext(object *op, const char *type, int tag)
{
	object *tmp = NULL;

	if (tag == -2)
	{
		for (op = op->inv; op; op = op->below)
		{
			if (!tmp && op->type == CONTAINER && op->race == type && QUERY_FLAG(op, FLAG_APPLIED))
			{
				tmp = find_arrow_ext(op, type, -2);
			}
			else if (op->type == ARROW && op->race == type)
			{
				return op;
			}
		}

		return tmp;
	}
	else
	{
		if (tag == -1)
		{
			return tmp;
		}

		for (op = op->inv; op; op = op->below)
		{
			if (op->count == (tag_t) tag)
			{
				/* Simple task: we have an arrow marked */
				if (op->race == type && op->type == ARROW)
				{
					return op;
				}

				/* we have container marked as missile source. Skip
				 * search when there is nothing in. Use the standard
				 * search now. */
				if (op->race == type && op->type == CONTAINER)
				{
					tmp = find_arrow_ext(op, type, -2);
					return tmp;
				}
			}
		}

		return tmp;
	}
}

/**
 * Check whether the specified player exists.
 * @param player_name Player name to check for.
 * @return 1 if the player exists, 0 otherwise. */
int player_exists(char *player_name)
{
	FILE *fp;
	char filename[HUGE_BUF];

	snprintf(filename, sizeof(filename), "%s/%s/%s/%s.pl", settings.localdir, settings.playerdir, player_name, player_name);
	fp = fopen(filename, "r");

	if (fp)
	{
		fclose(fp);
		return 1;
	}

	return 0;
}

/**
 * Looks for the skill and returns a pointer to it if found.
 * @param op The object to look for the skill in.
 * @param skillnr Skill ID.
 * @return The skill if found, NULL otherwise. */
object *find_skill(object *op, int skillnr)
{
	object *tmp;

	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		if (tmp->type == SKILL && tmp->stats.sp == skillnr)
		{
			return tmp;
		}
	}

	return NULL;
}

/**
 * Check whether player can carry an object.
 * @param pl Player.
 * @param ob The object player wants to carry.
 * @param nrof Number of objects.
 * @return 1 if the player can carry that number of objects, 0 otherwise. */
int player_can_carry(object *pl, object *ob, uint32 nrof)
{
	uint32 weight, effective_weight_limit;

	weight = ob->weight * nrof;

	if (ob->inv)
	{
		weight += ob->carrying;
	}

	if (pl->stats.Str <= MAX_STAT)
	{
		effective_weight_limit = weight_limit[pl->stats.Str];
	}
	else
	{
		effective_weight_limit = weight_limit[MAX_STAT];
	}

	return (pl->carrying + weight) < effective_weight_limit;
}

/**
 * Combine player's race with their class (if there is one).
 * @param op Player.
 * @param buf Buffer to write into.
 * @param size Size of 'buf'.
 * @return 'buf'. */
char *player_get_race_class(object *op, char *buf, size_t size)
{
	strncpy(buf, op->race, size - 1);

	if (CONTR(op)->class_ob)
	{
		strncat(buf, " ", size - strlen(buf) - 1);
		strncat(buf, CONTR(op)->class_ob->name, size - strlen(buf) - 1);
	}

	return buf;
}
