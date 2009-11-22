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
 * Functions used by DMs. */

#include <global.h>
#ifndef __CEXTRACT__
#include <sproto.h>
#endif

/**
 * Search for player other than the searcher in game.
 * @param op Player searching someone.
 * @param name Name to search for.
 * @return Player, or NULL if player can't be found. */
static player *get_other_player_from_name(object *op, const char *name)
{
	player *pl;

	if (!name)
	{
		return NULL;
	}

	for (pl = first_player; pl != NULL; pl = pl->next)
	{
		if (!strncasecmp(pl->ob->name, name, MAX_NAME))
		{
			break;
		}
	}

	if (pl == NULL)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "No such player.");
		return NULL;
	}

	if (pl->ob == op)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You can't do that to yourself.");
		return NULL;
	}

	if (pl->state != ST_PLAYING)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "That player is in no state for that right now.");
		return NULL;
	}

	return pl;
}

/**
 * This finds and returns the object which matches the name or object
 * number (specified via num #whatever).
 * @param params Name or ID of object to find.
 * @return The object if found, NULL otherwise. */
static object *find_object_both(char *params)
{
	if (!params)
	{
		return NULL;
	}

	if (params[0] == '#')
	{
		return find_object(atol(params + 1));
	}
	else
	{
		return find_object_name(params);
	}
}

/**
 * Sets the god for some objects.
 * @param op The DM.
 * @param params Should contain two values - first the object to change,
 * followed by the god to change it to.
 * @return 0 on syntax error, 1 otherwise. */
int command_setgod(object *op, char *params)
{
	object *ob, *god;
	char *str;

	if (!params || !(str = strchr(params, ' ')))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Usage: /setgod object god");
		return 0;
	}

	/* Kill the space, and set string to the next param */
	*str++ = '\0';

	if (!(ob = find_object_both(params)))
	{
		new_draw_info_format(NDI_UNIQUE, 0, op, "Set whose god - can not find object %s?", params);
		return 1;
	}

	/* Perhaps this is overly restrictive? Should we perhaps be able to
	 * rebless altars and the like? */
	if (ob->type != PLAYER)
	{
		new_draw_info_format(NDI_UNIQUE, 0, op, "%s is not a player - can not change its god", ob->name);
		return 1;
	}

	change_skill(ob, SK_PRAYING);

	if (!ob->chosen_skill || ob->chosen_skill->stats.sp != SK_PRAYING)
	{
		new_draw_info_format(NDI_UNIQUE, 0, op, "%s doesn't have praying skill.", ob->name);
		return 1;
	}

	god = find_god(str);

	if (god == NULL)
	{
		new_draw_info_format(NDI_UNIQUE, 0, op, "No such god %s.", str);
		return 1;
	}

	become_follower(ob, god);

	return 1;
}

/**
 * Kicks a player from the server.
 *
 * If both parameters are NULL, will kick all players.
 * @param op DM kicking.
 * @param params Player to kick. Must be a full name match.
 * @return 1. */
int command_kick(object *ob, char *params)
{
	player *pl;

	if (ob && params == NULL)
	{
		new_draw_info_format(NDI_UNIQUE, 0, ob, "Use: /kick <name>");
		return 1;
	}

	if (ob && ob->name && !strncasecmp(ob->name, params, MAX_NAME))
	{
		new_draw_info_format(NDI_UNIQUE, 0, ob, "You can't /kick yourself!");
		return 1;
	}

	for (pl = first_player; pl != NULL; pl = pl->next)
	{
		if (!ob || (pl->ob != ob && pl->ob->name && !strncasecmp(pl->ob->name, params, MAX_NAME)))
		{
			object *op = pl->ob;

			remove_ob(op);
			check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
			op->direction = 0;

			if (params)
			{
				new_draw_info_format(NDI_UNIQUE | NDI_ALL, 5, ob, "%s was kicked out of the game.", op->name);
			}

			LOG(llevInfo, "%s was kicked out of the game by %s.\n", op->name, ob ? ob->name : "a shutdown");

			CONTR(op)->socket.status = Ns_Dead;
#if MAP_MAXTIMEOUT
			op->map->timeout = MAP_TIMEOUT(op->map);
#endif
		}
	}

	return 1;
}

/**
 * Totally shuts down the server.
 * @param op DM shutting down the server.
 * @param params Ignored.
 * @return 1. */
int command_shutdown(object *op, char *params)
{
	(void) params;

	LOG(llevSystem, "Server shutdown started by %s\n", op->name);
	command_kick(NULL, NULL);
	cleanup();

	/* Not reached */
	return 1;
}

/**
 * DM teleports to a map.
 * @param op DM teleporting.
 * @param params Map to teleport to. Can be absolute or relative path.
 * @return 1 unless op is NULL. */
int command_goto(object *op, char *params)
{
	char name[MAX_BUF] = {"\0"};
	int x = -1, y = -1;
	object *dummy;

	if (!op)
	{
		return 0;
	}

	if (params == NULL)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Go to what map?\nUsage: /goto <map> x y");
		return 1;
	}

	sscanf(params, "%s %d %d", name, &x, &y);

	dummy = get_object();
	dummy->map = op->map;
	dummy->stats.hp = x;
	dummy->stats.sp = y;
	FREE_AND_COPY_HASH(EXIT_PATH(dummy), name);
	FREE_AND_COPY_HASH(dummy->name, name);

	enter_exit(op, dummy);

	new_draw_info_format(NDI_UNIQUE, 0, op, "Difficulty: %d.", op->map->difficulty);

	return 1;
}

/**
 * Freezes a player for a specified tick count, 100 by default.
 * @param op DM freezing the player.
 * @param params Optional tick count, followed by player name.
 * @return 1. */
int command_freeze(object *op, char *params)
{
	int ticks;
	player *pl;

	if (!params)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Usage: /freeze [ticks] <player>");
		return 1;
	}

	ticks = atoi(params);

	if (ticks)
	{
		while ((isdigit(*params) || isspace(*params)) && *params != 0)
		{
			params++;
		}

		if (*params == 0)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "Usage: /freeze [ticks] <player>");
			return 1;
		}
	}
	else
	{
		ticks = 100;
	}

	pl = get_other_player_from_name(op, params);

	if (!pl)
	{
		return 1;
	}

	new_draw_info(NDI_UNIQUE | NDI_RED, 0, pl->ob, "You have been frozen by the DM!");

	new_draw_info_format(NDI_UNIQUE, 0, op, "You freeze %s for %d ticks.", pl->ob->name, ticks);

	pl->ob->speed_left = -(pl->ob->speed * ticks);
	return 1;
}

/**
 * Summons player near DM.
 * @param op DM.
 * @param params Player to summon.
 * @return 1 unless op is NULL. */
int command_summon(object *op, char *params)
{
	int i;
	object *dummy;
	player *pl;

	if (!op)
	{
		return 0;
	}

	if (params == NULL)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Usage: /summon <player>.");
		return 1;
	}

	pl = get_other_player_from_name(op, params);

	if (!pl)
	{
		return 1;
	}

	i = find_free_spot(op->arch, NULL, op->map, op->x, op->y, 1, 8);

	if (i == -1)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Can not find a free spot to place summoned player.");
		return 1;
	}

	dummy = get_object();
	FREE_AND_ADD_REF_HASH(EXIT_PATH(dummy), op->map->path);
	EXIT_X(dummy) = op->x + freearr_x[i];
	EXIT_Y(dummy) = op->y + freearr_y[i];
	enter_exit(pl->ob, dummy);
	pl->ob->map = op->map;
	new_draw_info(NDI_UNIQUE, 0, pl->ob, "You are summoned.");
	new_draw_info(NDI_UNIQUE, 0, op, "OK.");
	return 1;
}

/**
 * Teleport next to target player.
 * @param op DM teleporting.
 * @param params Player to teleport to.
 * @return 0 if couldn't teleport, 1 otherwise. */
int command_teleport(object *op, char *params)
{
	int i;
	object *dummy;
	player *pl;

	if (!op)
	{
		return 0;
	}

	if (params == NULL)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Usage: /teleport <player>.");
		return 1;
	}

	pl = get_other_player_from_name(op, params);

	if (!pl)
	{
		return 1;
	}

	i = find_free_spot(pl->ob->arch, NULL, pl->ob->map, pl->ob->x, pl->ob->y, 1, 8);

	if (i == -1)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Can not find a free spot to teleport to.");
		return 1;
	}

	dummy = get_object();
	FREE_AND_ADD_REF_HASH(EXIT_PATH(dummy), pl->ob->map->path);
	EXIT_X(dummy) = pl->ob->x + freearr_x[i];
	EXIT_Y(dummy) = pl->ob->y + freearr_y[i];
	enter_exit(op, dummy);
	op->map = pl->ob->map;

	if (!CONTR(op)->dm_stealth)
	{
		new_draw_info(NDI_UNIQUE, 0, pl->ob, "You see a portal open.");
	}

	new_draw_info(NDI_UNIQUE, 0, op, "OK.");

	return 1;
}

/**
 * DM wants to create an object.
 * @param op DM.
 * @param params Object variables.
 * @return 1 unless op is NULL. */
int command_create(object *op, char *params)
{
	object *tmp = NULL;
	int nrof, i, magic, set_magic = 0, set_nrof = 0, gotquote, gotspace;
	char buf[MAX_BUF], *cp, *bp = buf, *bp2, *bp3, *bp4 = NULL, *obp, *cp2;
	archetype *at;
	artifact *art = NULL;

	if (!op)
	{
		return 0;
	}

	if (params == NULL)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Usage: /create [nr] [magic] <archetype> [ of <artifact>] [variable_to_patch setting]");
		return 1;
	}

	bp = params;

	if (sscanf(bp, "%d ", &nrof))
	{
		if ((bp = strchr(params, ' ')) == NULL)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "Usage: /create [nr] [magic] <archetype> [ of <artifact>] [variable_to_patch setting]");
			return 1;
		}

		bp++;
		set_nrof = 1;
		LOG(llevDebug, "%s creates: (%d) %s\n", op->name, nrof, bp);
	}

	if (sscanf(bp, "%d ", &magic))
	{
		if ((bp = strchr(bp, ' ')) == NULL)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "Usage: create [nr] [magic] <archetype> [ of <artifact>] [variable_to_patch setting]");
			return 1;
		}

		bp++;
		set_magic = 1;
		LOG(llevDebug, "%s creates: (%d) (%d) %s\n", op->name, nrof, magic, bp);
	}

	if ((cp = strstr(bp, " of ")) != NULL)
	{
		*cp = '\0';
		cp += 4;
	}

	for (bp2 = bp; *bp2; bp2++)
	{
		if (*bp2 == ' ')
		{
			*bp2 = '\0';
			bp2++;
			break;
		}
	}

	/* First step: browse the archetypes for the name. */
	if ((at = find_archetype(bp)) == NULL)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "No such archetype or artifact name.");
		return 1;
	}

	if (cp)
	{
		for (cp2 = cp; *cp2; cp2++)
		{
			if (*cp2 == ' ')
			{
				*cp2 = '\0';
				break;
			}
		}

		if (find_artifactlist(at->clone.type) == NULL)
		{
			new_draw_info_format(NDI_UNIQUE, 0, op, "No artifact list for type %d\n", at->clone.type);
		}
		else
		{
			art = find_artifact(cp);

			if (!art)
			{
				new_draw_info_format(NDI_UNIQUE, 0, op, "No such artifact ([%d] of %s)", at->clone.type, cp);
			}
		}

		LOG(llevDebug, "%s creates: (%d) (%d) (%s) of (%s)\n", op->name, set_nrof ? nrof : 0, set_magic ? magic : 0, bp, cp);
	}

	if (at->clone.nrof)
	{
		tmp = arch_to_object(at);
		tmp->x = op->x, tmp->y = op->y;

		if (set_nrof)
		{
			tmp->nrof = nrof;
		}

		tmp->map = op->map;

		if (set_magic)
		{
			set_abs_magic(tmp, magic);
		}

		if (art)
		{
			give_artifact_abilities(tmp, art);
		}

		if (need_identify(tmp))
		{
			SET_FLAG(tmp, FLAG_IDENTIFIED);
			CLEAR_FLAG(tmp, FLAG_KNOWN_MAGICAL);
		}

		while (*bp2)
		{
			bp4 = NULL;

			/* Find the first quote. */
			for (bp3 = bp2, gotquote = 0, gotspace = 0; *bp3 && gotspace < 2; bp3++)
			{
				if (*bp3 == '"')
				{
					*bp3 = ' ';
					gotquote++;
					bp3++;

					for (bp4 = bp3; *bp4; bp4++)
					{
						if (*bp4 == '"')
						{
							*bp4 = '\0';
							break;
						}
					}

					break;
				}
				else if (*bp3 == ' ')
				{
					gotspace++;
				}
			}

			if (!gotquote)
			{
				/* then find the second space */
				for (bp3 = bp2; *bp3; bp3++)
				{
					if (*bp3 == ' ')
					{
						bp3++;

						for (bp4 = bp3; *bp4; bp4++)
						{
							if (*bp4 == ' ')
							{
								*bp4 = '\0';
								break;
							}
						}

						break;
					}
				}
			}

			if (bp4 == NULL)
			{
				new_draw_info_format(NDI_UNIQUE, 0, op, "No parameter value for variable %s", bp2);
				break;
			}

			/* Now bp3 should be the argument, and bp2 the whole command. */
			if (set_variable(tmp, bp2) == -1)
			{
				new_draw_info_format(NDI_UNIQUE, 0, op, "Unknown variable %s", bp2);
			}
			else
			{
				new_draw_info_format(NDI_UNIQUE, 0, op, "(%s#%d)->%s=%s", tmp->name, tmp->count, bp2, bp3);
			}

			if (gotquote)
			{
				bp2 = bp4 + 2;
			}
			else
			{
				bp2 = bp4 + 1;
			}

			obp = bp2;
		}

		tmp = insert_ob_in_ob(tmp, op);
		esrv_send_item(op, tmp);
		return 1;
	}

	for (i = 0 ; i < (set_nrof ? nrof : 1); i++)
	{
		archetype *atmp;
		object *prev = NULL, *head = NULL;

		for (atmp = at; atmp != NULL; atmp = atmp->more)
		{
			tmp = arch_to_object(atmp);

			if (head == NULL)
			{
				head = tmp;
			}

			tmp->x = op->x + tmp->arch->clone.x;
			tmp->y = op->y + tmp->arch->clone.y;
			tmp->map = op->map;

			if (set_magic)
			{
				set_abs_magic(tmp, magic);
			}

			if (art)
			{
				give_artifact_abilities(tmp, art);
			}

			if (need_identify(tmp))
			{
				SET_FLAG(tmp, FLAG_IDENTIFIED);
				CLEAR_FLAG(tmp, FLAG_KNOWN_MAGICAL);
			}

			while (*bp2)
			{
				bp4 = NULL;

				/* Find the first quote */
				for (bp3 = bp2, gotquote = 0, gotspace = 0; *bp3 && gotspace < 2; bp3++)
				{
					if (*bp3 == '"')
					{
						*bp3 = ' ';
						gotquote++;
						bp3++;

						for (bp4 = bp3; *bp4; bp4++)
						{
							if (*bp4 == '"')
							{
								*bp4 = '\0';
								break;
							}
						}

						break;
					}
					else if (*bp3 == ' ')
					{
						gotspace++;
					}
				}

				if (!gotquote)
				{
					/* Fhen find the second space */
					for (bp3 = bp2; *bp3; bp3++)
					{
						if (*bp3 == ' ')
						{
							bp3++;

							for (bp4 = bp3; *bp4; bp4++)
							{
								if (*bp4 == ' ')
								{
									*bp4 = '\0';
									break;
								}
							}

							break;
						}
					}
				}

				if (bp4 == NULL)
				{
					new_draw_info_format(NDI_UNIQUE, 0, op, "No parameter value for variable %s", bp2);
					break;
				}

				/* Now bp3 should be the argument, and bp2 the whole command */
				if (set_variable(tmp, bp2) == -1)
				{
					new_draw_info_format(NDI_UNIQUE, 0, op, "Unknown variable '%s'", bp2);
				}
				else
				{
					new_draw_info_format(NDI_UNIQUE, 0, op, "(%s#%d)->%s=%s", tmp->name, tmp->count, bp2, bp3);
				}

				if (gotquote)
				{
					bp2 = bp4 + 2;
				}
				else
				{
					bp2 = bp4 + 1;
				}

				obp = bp2;
			}

			if (head != tmp)
			{
				tmp->head = head, prev->more = tmp;
			}

			prev = tmp;
		}

		if (at->clone.randomitems)
		{
			create_treasure(at->clone.randomitems, head, GT_APPLY, head->type == MONSTER ? head->level : get_enviroment_level(head), T_STYLE_UNSET, ART_CHANCE_UNSET, 0, NULL);
		}

		/* If the created object is alive or is multi arch, insert it on
		 * the map. */
		if (IS_LIVE(head) || head->more)
		{
			if (head->type == MONSTER)
			{
				fix_monster(head);
			}

			insert_ob_in_map(head, op->map, op, INS_NO_MERGE | INS_NO_WALK_ON);
		}
		/* Into the DM's inventory otherwise. */
		else
		{
			head = insert_ob_in_ob(head, op);
			esrv_send_item(op, head);
		}
	}

	return 1;
}

/**
 * Shows the inventory or some item.
 * @param op Player.
 * @param params Object count to get the inventory of. If NULL then
 * defaults to op.
 * @return 1 unless params is NULL. */
int command_inventory(object *op, char *params)
{
	object *tmp;
	int i;

	if (!params)
	{
		inventory(op, NULL);
		return 0;
	}

	if (!sscanf(params, "%d", &i) || (tmp = find_object(i)) == NULL)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Inventory of what object (nr)?");
		return 1;
	}

	inventory(op, tmp);
	return 1;
}

/**
 * Dumps the difference between an object and its archetype.
 * @param op DM.
 * @param params Object to dump.
 * @return 1. */
int command_dump(object *op, char *params)
{
	int i;
	object *tmp;

	if (params != NULL && !strcmp(params, "me"))
	{
		tmp = op;
	}
	else if (params == NULL || !sscanf(params, "%d", &i) || (tmp = find_object(i)) == NULL)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Dump what object (nr)?");
		return 1;
	}

	dump_object(tmp);
	new_draw_info(NDI_UNIQUE, 0, op, errmsg);
	return 1;
}

/**
 * DM wants to alter an object.
 * @param op DM.
 * @param params Object and what to patch.
 * @return 1. */
int command_patch(object *op, char *params)
{
	int i;
	char *arg, *arg2, buf[MAX_BUF];
	object *tmp = NULL;

	if (params != NULL)
	{
		if (!strncmp(params, "me", 2))
		{
			tmp = op;
		}
		else if (sscanf(params, "%d", &i))
		{
			tmp = find_object(i);
		}
		else if (sscanf(params, "%s", buf))
		{
			tmp = find_object_name(buf);
		}
	}

	if (tmp == NULL)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Patch what object (nr)?");
		return 1;
	}

	arg = strchr(params, ' ');

	if (arg == NULL)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Patch what values?");
		return 1;
	}

	if ((arg2 = strchr(++arg, ' ')))
	{
		arg2++;
	}

	if (set_variable(tmp, arg) == -1)
	{
		new_draw_info_format(NDI_UNIQUE, 0, op, "Unknown variable %s", arg);
	}
	else
	{
		new_draw_info_format(NDI_UNIQUE, 0, op, "(%s#%d)->%s=%s", tmp->name, tmp->count, arg, arg2);
	}

	return 1;
}

/**
 * Remove an object from its position.
 * @param op DM.
 * @param params Object to remove.
 * @return 1. */
int command_remove(object *op, char *params)
{
	int i;
	object *tmp;

	if (params == NULL || !sscanf(params, "%d", &i) || (tmp = find_object(i)) == NULL)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Remove what object (nr)?");
		return 1;
	}

	if (tmp->type == PLAYER)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Cannot remove a player!");
		return 1;
	}

	if (QUERY_FLAG(tmp, FLAG_REMOVED))
	{
		new_draw_info_format(NDI_UNIQUE, 0, op, "%s is already removed!", query_name(tmp, NULL));
		return 1;
	}

	/* Ensure we have head. */
	if (tmp->head)
	{
		tmp = tmp->head;
	}

	if (tmp->speed != 0)
	{
		tmp->speed = 0;
		update_ob_speed(tmp);
	}

	remove_ob(tmp);
	check_walk_off(tmp, NULL, MOVE_APPLY_VANISHED);
	return 1;
}

/**
 * Adds experience to a player.
 * @param op DM.
 * @param params Should be "\<who\> \<skill nr\> \<exp\>".
 * @return 1. */
int command_addexp(object *op, char *params)
{
	char buf[MAX_BUF];
	int exp, snr;
	object *exp_skill, *exp_ob;
	player *pl;

	if (params == NULL || sscanf(params, "%s %d %d", buf, &snr, &exp) != 3)
	{
		int i;

		new_draw_info(NDI_UNIQUE, 0, op, "Usage: /addexp <who> <skill nr> <exp>\nSkills/Nr: ");

		for (i = 0; i < NROFSKILLS; i++)
		{
			new_draw_info_format(NDI_UNIQUE, 0, op, "%d: %s", i, skills[i].name);
		}

		return 1;
	}

	for (pl = first_player; pl != NULL; pl = pl->next)
	{
		if (!strncasecmp(pl->ob->name, buf, MAX_NAME))
		{
			break;
		}
	}

	if (pl == NULL)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "No such player.");
		return 1;
	}

	/* Safety check */
	if (snr < 0 || snr >= NROFSKILLS)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "No such skill.");
		return 1;
	}

	exp_skill = pl->skill_ptr[snr];

	/* Safety check */
	if (!exp_skill)
	{
		/* Our player doesn't have this skill? */
		new_draw_info_format(NDI_UNIQUE, 0, op, "Player %s does not know the skill '%s'.", pl->ob->name, skills[snr]);
		return 0;
	}

	/* If we are full in this skill, there is nothing is to do */
	if (exp_skill->level >= MAXLEVEL)
	{
		return 0;
	}

	/* We will sure change skill exp, mark for update */
	pl->update_skills = 1;
	exp_ob = exp_skill->exp_obj;

	if (!exp_ob)
	{
		LOG(llevBug, "BUG: add_exp() skill:%s - no exp_ob found!\n", query_name(exp_skill, NULL));
		return 0;
	}

	/* First we see what we can add to our skill */
	exp = adjust_exp(pl->ob, exp_skill, exp);

	/* adjust_exp has adjust the skill and all exp_obj and player exp */
	/* now lets check for level up in all categories */
	player_lvl_adj(pl->ob, exp_skill);
	player_lvl_adj(pl->ob, exp_ob);
	player_lvl_adj(pl->ob, NULL);

	return 1;
}

/**
 * Changes the server speed.
 * @param op DM.
 * @param params New speed, or NULL to see the speed.
 * @return 1. */
int command_speed(object *op, char *params)
{
	int i;

	if (params == NULL || !sscanf(params, "%d", &i))
	{
		new_draw_info_format(NDI_UNIQUE, 0, op, "Current speed is %ld.", max_time);
		return 1;
	}

	set_max_time(i);
	reset_sleep();
	new_draw_info(NDI_UNIQUE, 0, op, "The speed has changed.");
	return 1;
}

/**
 * Displays the statistics of a player.
 * @param op DM.
 * @param params Player's name.
 * @return 1. */
int command_stats(object *op, char *params)
{
	player *pl;

	if (params == NULL)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Who?");
		return 1;
	}

	for (pl = first_player; pl != NULL; pl = pl->next)
	{
		if (!strcmp(pl->ob->name, params))
		{
			new_draw_info_format(NDI_UNIQUE, 0, op, "Str : %-2d      H.P.   : %-4d  MAX : %d", pl->ob->stats.Str, pl->ob->stats.hp, pl->ob->stats.maxhp);
			new_draw_info_format(NDI_UNIQUE, 0, op, "Dex : %-2d      S.P.   : %-4d  MAX : %d", pl->ob->stats.Dex, pl->ob->stats.sp, pl->ob->stats.maxsp);
			new_draw_info_format(NDI_UNIQUE, 0, op, "Con : %-2d      AC     : %-4d  WC  : %d", pl->ob->stats.Con, pl->ob->stats.ac, pl->ob->stats.wc);
			new_draw_info_format(NDI_UNIQUE, 0, op, "Wis : %-2d      EXP    : %d", pl->ob->stats.Wis, pl->ob->stats.exp);
			new_draw_info_format(NDI_UNIQUE, 0, op, "Cha : %-2d      Food   : %d", pl->ob->stats.Cha, pl->ob->stats.food);
			new_draw_info_format(NDI_UNIQUE, 0, op, "Int : %-2d      Damage : %d", pl->ob->stats.Int, pl->ob->stats.dam);
			new_draw_info_format(NDI_UNIQUE, 0, op, "Pow : %-2d      Grace  : %d", pl->ob->stats.Pow, pl->ob->stats.grace);
			return 1;
		}
	}

	if (pl == NULL)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "No such player.");
	}

	return 1;
}

/**
 * Resets a map.
 * @param op DM.
 * @param params Map to reset. Can be NULL for current op's map, or a map
 * path.
 * @return 1. */
int command_reset(object *op, char *params)
{
	int count;
	mapstruct *m;
	player *pl;
	object *dummy = NULL;
	const char *mapfile_sh;

	if (params == NULL)
	{
		m = has_been_loaded_sh(op->map->path);
	}
	else
	{
		mapfile_sh = add_string(params);
		m = has_been_loaded_sh(mapfile_sh);
		free_string_shared(mapfile_sh);
	}

	if (m == NULL)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "No such map.");
		return 1;
	}

	if (MAP_UNIQUE(m) && MAP_NOSAVE(m))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Cannot reset unique no-save map.");
		return 1;
	}

	dummy = get_object();
	dummy->map = NULL;
	FREE_AND_ADD_REF_HASH(EXIT_PATH(dummy), m->path);

	if (m->in_memory != MAP_SWAPPED)
	{
		if (m->in_memory != MAP_IN_MEMORY)
		{
			LOG(llevBug, "BUG: Tried to swap out map which was not in memory.\n");
			return 0;
		}

		new_draw_info_format(NDI_UNIQUE, 0, op, "Start resetting map %s.", m->path ? m->path : ">NULL<");

		/* Now remove all players from this map - flag them so we can put
		 * them back later. */
		count = 0;

		for (pl = first_player; pl != NULL; pl = pl->next)
		{
			if (pl->ob->map == m)
			{
				count++;
				/* No walk off check */
				remove_ob(pl->ob);
				pl->dm_removed_from_map = 1;
			}
			else
			{
				pl->dm_removed_from_map = 0;
			}
		}

		new_draw_info_format(NDI_UNIQUE, 0, op, "Removed %d players from map. Swap map.", count);
		swap_map(m, 1);
	}

	if (m->in_memory == MAP_SWAPPED)
	{
		LOG(llevDebug, "Resetting map %s.\n", m->path);
		clean_tmp_map(m);

		if (m->tmpname)
		{
			free(m->tmpname);
		}

		m->tmpname = NULL;
		/* Setting this effectively causes an immediate reload */
		m->reset_time = 1;
		new_draw_info(NDI_UNIQUE, 0, op, "Swap successful. Inserting players.");

		for (pl = first_player; pl != NULL; pl = pl->next)
		{
			if (pl->dm_removed_from_map)
			{
				EXIT_X(dummy) = pl->ob->x;
				EXIT_Y(dummy) = pl->ob->y;
				enter_exit(pl->ob, dummy);

				if (pl->ob != op)
				{
					if (QUERY_FLAG(pl->ob, FLAG_WIZ))
					{
						new_draw_info_format(NDI_UNIQUE, 0, pl->ob, "Map reset by %s.", op->name);
					}
					/* Write a nice little confusing message to the players */
					else
					{
						new_draw_info(NDI_UNIQUE, 0, pl->ob, "Your surroundings seem different but still familiar. Haven't you been here before?");
					}
				}
			}
		}

		new_draw_info(NDI_UNIQUE, 0, op, "Resetmap done.");
	}
	else
	{
		/* Need to re-insert players if swap failed for some reason. */
		for (pl = first_player; pl != NULL; pl = pl->next)
		{
			if (pl->dm_removed_from_map)
			{
				insert_ob_in_map(pl->ob, m, NULL, INS_NO_MERGE | INS_NO_WALK_ON);
			}
		}

		new_draw_info(NDI_UNIQUE, 0, op, "Reset failed, couldn't swap map!");
	}

	return 1;
}

/**
 * Remove DM from the list of DMs.
 * @param op The DM object to remove. */
void remove_active_DM(object *op)
{
	objectlink *this;

	/* Safety */
	if (!dm_list)
	{
		return;
	}

	/* Easier if the first object in the list matches */
	if (dm_list->ob == op)
	{
		this = dm_list;
		dm_list = this->next;
		free(this);
	}
	else
	{
		objectlink *prev = dm_list;

		for (this = dm_list->next; this != NULL; this = this->next)
		{
			if (this->ob == op)
			{
				break;
			}

			prev = this;
		}

		if (this)
		{
			prev->next = this->next;
			free(this);
		}
	}
}

/**
 * Steps down from DM mode.
 * @param op DM.
 * @param params Ignored.
 * @return 1. */
int command_nowiz(object *op, char *params)
{
	(void) params;

	CLEAR_FLAG(op, FLAG_WIZ);

	/* Clear this DM from DMs list. */
	remove_active_DM(op);

	CLEAR_FLAG(op, FLAG_WIZPASS);
	CLEAR_MULTI_FLAG(op, FLAG_FLYING);
	fix_player(op);
	CONTR(op)->socket.update_tile = 0;
	esrv_send_inventory(op, op);
	CONTR(op)->update_los = 1;
	new_draw_info(NDI_UNIQUE, 0, op, "DM mode deactivated.");

	return 1;
}

/**
 * Check to see if player is allowed to become a DM.
 * @param op The player object trying to become a DM
 * @param pl_passwd Password can be used to become a DM if it matches one
 * of passwords in the database.
 * @return 1 if the object can become a DM, 0 otherwise */
static int checkdm(object *op, char *pl_passwd)
{
	sqlite3 *db;
	sqlite3_stmt *statement;
	char name[160], passwd[160], host[160];

	/* Open the database */
	db_open(DB_DEFAULT, &db);

	/* Prepare the SQL */
	if (!db_prepare(db, "SELECT name, passwd, host FROM dms;", &statement))
	{
		LOG(llevBug, "BUG: checkdm(): Failed to prepare SQL query! (%s)\n", db_errmsg(db));
		return 0;
	}

	/* Loop through all the results */
	while (db_step(statement) == SQLITE_ROW)
	{
		snprintf(name, sizeof(name), "%s", db_column_text(statement, 0));
		snprintf(passwd, sizeof(passwd), "%s", db_column_text(statement, 1));
		snprintf(host, sizeof(host), "%s", db_column_text(statement, 2));

		if ((!strcmp(name, "*") || (op->name && !strcmp(op->name, name))) && (!strcmp(passwd, "*") || !strcmp(passwd, pl_passwd)) && (!strcmp(host, "*") || !strcmp(host, CONTR(op)->socket.host)))
		{
			db_finalize(statement);
			db_close(db);

			return 1;
		}
	}

	/* Finalize it */
	db_finalize(statement);

	/* Close the database */
	db_close(db);

	return 0;
}

/**
 * Actual command to become a DM.
 * @param op The player object.
 * @param params Params may include password needed to become a DM.
 * @return 1 on success, 0 on failure (caller not a player, etc). */
int command_dm(object *op, char *params)
{
	CONTR(op)->socket.ext_title_flag = 1;

	/* IF we are DM, then turn mode off */
	if (QUERY_FLAG(op, FLAG_WIZ) && op->type == PLAYER)
	{
		command_nowiz(op, params);
		return 1;
	}

	if (op->type != PLAYER || !CONTR(op))
	{
		return 0;
	}

	if (checkdm(op, (params ? params : "*")))
	{
		objectlink *ol;

		/* Add this DM to the DMs list */
		ol = dm_list;
		dm_list = get_objectlink();
		dm_list->ob = op;
		dm_list->id = op->count;
		dm_list->next = ol;

		SET_FLAG(op, FLAG_WIZ);
		SET_FLAG(op, FLAG_WAS_WIZ);
		SET_FLAG(op, FLAG_WIZPASS);

		new_draw_info_format(NDI_UNIQUE, 0, op, "DM mode activated for %s!", op->name);
		SET_MULTI_FLAG(op, FLAG_FLYING);

		esrv_send_inventory(op, op);

		/* Send all the spells for this DM */
		send_spelllist_cmd(op, NULL, SPLIST_MODE_ADD);

		clear_los(op);

		/* force a draw_look() */
		CONTR(op)->socket.update_tile = 0;
		CONTR(op)->update_los = 1;
		CONTR(op)->write_buf[0] = '\0';

		return 1;
	}
	else
	{
		CONTR(op)->write_buf[0] = '\0';

		return 1;
	}
}

/**
 * DM wants to learn a spell.
 * @param op DM.
 * @param params Spell name to learn.
 * @param special_prayer If set, special (god-given) prayer.
 * @return 0 if the spell wasn't learned or was already learned, 1
 * otherwise. */
static int command_learn_spell_or_prayer(object *op, char *params, int special_prayer)
{
	int spell;

	if (op->type != PLAYER || CONTR(op) == NULL || params == NULL)
	{
		return 0;
	}

	if ((spell = look_up_spell_name(params)) < 0)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Unknown spell.");
		return 1;
	}

	if (check_spell_known(op, spell))
	{
		new_draw_info_format(NDI_UNIQUE, 0, op, "You already know the spell %s.", params);
		return 0;
	}

	do_learn_spell(op, spell, special_prayer);
	return 1;
}

/**
 * DM wants to learn a regular spell.
 * @param op DM.
 * @param params Spell name.
 * @return 0 on failure, 1 on success. */
int command_learn_spell(object *op, char *params)
{
	return command_learn_spell_or_prayer(op, params, 0);
}

/**
 * DM wants to learn a god-given spell.
 * @param op DM.
 * @param params Spell name.
 * @return 0 on failure, 1 on success. */
int command_learn_special_prayer(object *op, char *params)
{
	return command_learn_spell_or_prayer(op, params, 1);
}

/**
 * DM wishes to forget a spell.
 * @param op DM.
 * @param params Spell name to forget.
 * @return 0 if no spell was forgotten, 1 otherwise. */
int command_forget_spell(object *op, char *params)
{
	int spell;

	if (op->type != PLAYER || CONTR(op) == NULL || params == NULL)
	{
		return 0;
	}

	if ((spell = look_up_spell_name(params)) < 0)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Unknown spell.");
		return 1;
	}

	do_forget_spell(op, spell);
	return 1;
}

/**
 * Lists all plugins currently loaded with their IDs and full names.
 * @param op DM.
 * @param params Ignored.
 * @return 1. */
int command_listplugins(object *op, char *params)
{
	(void) params;

	displayPluginsList(op);
	return 1;
}

/**
 * Loads the given plugin. The DM specifies the name of the library to
 * load (no pathname is needed). Do not ever attempt to load the same
 * plugin more than once at a time, or bad things could happen.
 * @param op DM loading a plugin.
 * @param params Should be the plugin's name, eg plugin_python.so.
 * @return 1. */
int command_loadplugin(object *op, char *params)
{
	char buf[MAX_BUF];

	if (!params)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Load what plugin?");
		return 1;
	}

	strcpy(buf, DATADIR);
	strcat(buf, "/../plugins/");
	strcat(buf, params);
	printf("Requested plugin file is %s\n", buf);
	initOnePlugin(buf);

	return 1;
}

/**
 * Unloads the given plugin. The DM specified the ID of the library to
 * unload. Note that some things may behave strangely if the correct
 * plugins are not loaded.
 * @param op DM unloading a plugin.
 * @param params Should be the plugin's internal name, eg Python.
 * @return 1. */
int command_unloadplugin(object *op, char *params)
{
	if (!params)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Unload what plugin?");
		return 1;
	}

	removeOnePlugin(params);

	return 1;
}

/**
 * Start the shutdown agent.
 * @param timer If -1 count the shutdown timer, reset the shutdown value
 * otherwise.
 * @param reason The reason for a shutdown. */
void shutdown_agent(int timer, char *reason)
{
	static int sd_timer = -1, m_count, real_count = -1;
	static struct timeval tv1, tv2;

	if (timer == -1 && sd_timer == -1)
	{
		if (real_count > 0)
		{
			if (--real_count <= 0)
			{
				LOG(llevSystem, "Server shutdown started.\n");
				command_kick(NULL, NULL);
				cleanup();
			}
		}

		/* Nothing to do */
		return;
	}

	/* reset shutdown count */
	if (timer != -1)
	{
		int t_min = timer / 60, t_sec = timer - (int) (timer / 60) * 60;

		sd_timer = timer;

		new_draw_info(NDI_PLAYER | NDI_UNIQUE | NDI_ALL | NDI_GREEN, 5, NULL, "[Server]: ** SERVER SHUTDOWN STARTED **");

		if (reason)
		{
			new_draw_info_format(NDI_PLAYER | NDI_UNIQUE | NDI_ALL | NDI_GREEN, 5, NULL, "[Server]: %s", reason);
		}

		if (t_sec)
		{
			new_draw_info_format(NDI_PLAYER | NDI_UNIQUE | NDI_ALL | NDI_GREEN, 5, NULL, "[Server]: SERVER REBOOT in %d minutes and %d seconds", t_min, t_sec);
		}
		else
		{
			new_draw_info_format(NDI_PLAYER | NDI_UNIQUE | NDI_ALL | NDI_GREEN, 5, NULL, "[Server]: SERVER REBOOT in %d minutes", t_min);
		}

		GETTIMEOFDAY(&tv1);
		m_count = timer / 60 - 1;
		real_count = -1;
	}
	/* Count the shutdown timer */
	else
	{
		int t_min, t_sec = 0;

		GETTIMEOFDAY(&tv2);

		/* End countdown */
		if ((int) (tv2.tv_sec - tv1.tv_sec) >= sd_timer)
		{
			new_draw_info(NDI_PLAYER | NDI_UNIQUE | NDI_ALL | NDI_GREEN, 5, NULL, "[Server]: ** SERVER GOES DOWN NOW!!! **");

			if (reason)
			{
				new_draw_info_format(NDI_PLAYER | NDI_UNIQUE | NDI_ALL | NDI_GREEN, 5, NULL, "[Server]: %s", reason);
			}

			sd_timer = -1;
			real_count = 30;
		}

		t_min = (sd_timer - (int) (tv2.tv_sec - tv1.tv_sec)) / 60;
		t_sec = (sd_timer - (int) (tv2.tv_sec - tv1.tv_sec)) - (int) ((sd_timer - (int) (tv2.tv_sec - tv1.tv_sec)) / 60) * 60;

		if ((t_min == m_count && !t_sec))
		{
			m_count = t_min - 1;

			if (t_sec)
			{
				new_draw_info_format(NDI_PLAYER | NDI_UNIQUE | NDI_ALL | NDI_GREEN, 5, NULL, "[Server]: SERVER REBOOT in %d minutes and %d seconds", t_min, t_sec);
			}
			else
			{
				new_draw_info_format(NDI_PLAYER | NDI_UNIQUE | NDI_ALL | NDI_GREEN, 5, NULL, "[Server]: SERVER REBOOT in %d minutes", t_min);
			}
		}
	}
}

/**
 * Set a custom Message of the Day in the database.
 * @param op DM.
 * @param params The new message of the day, if "original" revert to the
 * original MotD.
 * @return 1 on success, 0 on failure. */
int command_motd_set(object *op, char *params)
{
	sqlite3 *db;
	sqlite3_stmt *statement;

	/* No params, show usage. */
	if (params == NULL)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Usage: /motd_set <original | new motd>");
		return 0;
	}

	/* Open database */
	db_open(DB_DEFAULT, &db);

	/* Prepare the SQL query to first delete the old custom MotD (if there is any) */
	if (!db_prepare(db, "DELETE FROM settings WHERE name = 'motd_custom';", &statement))
	{
		LOG(llevBug, "BUG: Failed to prepare SQL query to delete custom MotD! (%s)\n", db_errmsg(db));
		db_close(db);
		return 0;
	}

	/* Run the query */
	db_step(statement);

	/* Finalize it */
	db_finalize(statement);

	/* If we are not reverting to original MotD */
	if (strcmp(params, "original"))
	{
		/* Prepare the SQL query to insert new custom MotD */
		if (!db_prepare_format(db, &statement, "INSERT INTO settings (name, data) VALUES ('motd_custom', '%s');", db_sanitize_input(params)))
		{
			LOG(llevBug, "BUG: Failed to prepare SQL query to insert custom MotD! (%s)\n", db_errmsg(db));
			db_close(db);
			return 0;
		}

		/* Run the query */
		db_step(statement);

		/* Finalize it */
		db_finalize(statement);

		/* Show message that everything went ok */
		new_draw_info(NDI_UNIQUE | NDI_GREEN, 0, op, "New Message of the Day has been set!");
	}
	/* Otherwise we are reverting. As we already deleted the custom MotD,
	 * there is not much to do besides show that it was reverted. */
	else
	{
		new_draw_info(NDI_UNIQUE | NDI_GREEN, 0, op, "Reverted original Message of the Day.");
	}

	/* Close the databas e*/
	db_close(db);

	return 1;
}

/**
 * Ban command, used to ban IP or player from the game.
 * @param op DM.
 * @param params Command parameters.
 * @return Always returns 1. */
int command_ban(object *op, char *params)
{
	if (params == NULL)
	{
		return 1;
	}

	/* Add a new ban */
	if (strncmp(params, "add ", 4) == 0)
	{
		params += 4;

		if (add_ban(params))
		{
			new_draw_info(NDI_UNIQUE | NDI_GREEN, 0, op, "Added new ban successfully.");
		}
		else
		{
			new_draw_info(NDI_UNIQUE | NDI_RED, 0, op, "Failed to add new ban!");
		}
	}
	/* Remove ban */
	else if (strncmp(params, "remove ", 7) == 0)
	{
		params += 7;

		if (remove_ban(params))
		{
			new_draw_info(NDI_UNIQUE | NDI_GREEN, 0, op, "Removed ban successfully.");
		}
		else
		{
			new_draw_info(NDI_UNIQUE | NDI_RED, 0, op, "Failed to remove ban!");
		}
	}
	/* List bans */
	else if (strncmp(params, "list", 4) == 0)
	{
		list_bans(op);
	}

	return 1;
}
