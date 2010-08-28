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
 * Functions used by DMs. */

#include <global.h>
#undef SS_STATISTICS
#include <shstr.h>

/**
 * Search for player other than the searcher in game.
 * @param op Player searching someone.
 * @param name Name to search for.
 * @return Player, or NULL if player can't be found. */
static player *get_other_player_from_name(object *op, char *name)
{
	player *pl;

	if (!name)
	{
		return NULL;
	}

	adjust_player_name(name);

	for (pl = first_player; pl; pl = pl->next)
	{
		if (!strncasecmp(pl->ob->name, name, MAX_NAME))
		{
			break;
		}
	}

	if (pl == NULL)
	{
		new_draw_info(NDI_UNIQUE, op, "No such player.");
		return NULL;
	}

	if (pl->ob == op)
	{
		new_draw_info(NDI_UNIQUE, op, "You can't do that to yourself.");
		return NULL;
	}

	if (pl->state != ST_PLAYING)
	{
		new_draw_info(NDI_UNIQUE, op, "That player is in no state for that right now.");
		return NULL;
	}

	return pl;
}

/**
 * Recursively find an object by its name or ID.
 * @param ob Object to start searching from.
 * @param name Name to search for. Must be a shared string. Can be NULL.
 * @param count ID of the object to search for. Can be 0.
 * @return Found object, NULL if no matching object found. */
static object *find_object_rec(object *ob, const char *name, tag_t count)
{
	object *tmp;

	for (tmp = ob; tmp; tmp = tmp->below)
	{
		if ((name && tmp->name == name) || (count && tmp->count == count))
		{
			return tmp;
		}
		else if (tmp->inv)
		{
			object *tmp2 = find_object_rec(tmp->inv, name, count);

			if (tmp2)
			{
				return tmp2;
			}
		}
	}

	return NULL;
}

/**
 * This finds and returns the object which matches the name or object
 * number (specified via num \#whatever).
 * @param op The DM requesting this.
 * @param params Name or ID of object to find.
 * @return The object if found, NULL otherwise. */
static object *find_object_both(object *op, char *params)
{
	tag_t count = 0;
	const char *name = NULL;
	player *pl;
	object *tmp, *tmp2;
	int x, y;

	if (!params)
	{
		return NULL;
	}

	if (params[0] == '#')
	{
		count = atol(params + 1);
	}
	else
	{
		name = find_string(params);
	}

	/* If find_string() can't find the string, then it's impossible that
	 * op->name will match. */
	if (!name && !count)
	{
		return NULL;
	}

	/* First search through the players */
	for (pl = first_player; pl; pl = pl->next)
	{
		if ((name && pl->ob->name == name) || (count && pl->ob->count == count))
		{
			return pl->ob;
		}
	}

	/* Otherwise search below the DM */
	for (tmp = get_map_ob(op->map, op->x, op->y); tmp; tmp = tmp->above)
	{
		tmp2 = find_object_rec(tmp, name, count);

		if (tmp2)
		{
			return tmp2;
		}
	}

	/* No match? Search through the entire map... */
	for (x = 0; x < MAP_WIDTH(op->map); x++)
	{
		for (y = 0; y < MAP_HEIGHT(op->map); y++)
		{
			for (tmp = get_map_ob(op->map, x, y); tmp; tmp = tmp->above)
			{
				tmp2 = find_object_rec(tmp, name, count);

				if (tmp2)
				{
					return tmp2;
				}
			}
		}
	}

	return NULL;
}

/**
 * Remove all players from the specified map, marking them with
 * player::dm_removed_from_map so we can re-insert them later.
 * @param m Map to remove players from.
 * @return How many players were removed. */
static int dm_map_remove_players(mapstruct *m)
{
	int count = 0;
	player *pl;

	for (pl = first_player; pl; pl = pl->next)
	{
		if (pl->ob->map == m)
		{
			count++;
			remove_ob(pl->ob);
			pl->dm_removed_from_map = 1;
			pl->ob->map = NULL;
		}
	}

	return count;
}

/**
 * Re-insert players from previous reset of a map.
 * @param m Map to place the players to.
 * @param op Player object doing the reset. */
static void dm_map_reinsert_players(mapstruct *m, object *op)
{
	player *pl;

	for (pl = first_player; pl; pl = pl->next)
	{
		if (pl->dm_removed_from_map)
		{
			pl->dm_removed_from_map = 0;
			insert_ob_in_map(pl->ob, m, NULL, INS_NO_MERGE);
			/* So that we don't access invalid values of old player's last_update map
			 * pointer when sending map to the client. */
			pl->last_update = NULL;

			if (pl->ob != op)
			{
				if (QUERY_FLAG(pl->ob, FLAG_WIZ))
				{
					new_draw_info_format(NDI_UNIQUE, pl->ob, "Map reset by %s.", op->name);
				}
				/* Write a nice little confusing message to the players */
				else
				{
					new_draw_info(NDI_UNIQUE, pl->ob, "Your surroundings seem different but still familiar. Haven't you been here before?");
				}
			}
		}
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
		new_draw_info(NDI_UNIQUE, op, "Usage: /setgod object god");
		return 0;
	}

	/* Kill the space, and set string to the next param */
	*str++ = '\0';

	if (!(ob = find_object_both(op, params)))
	{
		new_draw_info_format(NDI_UNIQUE, op, "Set whose god - can not find object %s?", params);
		return 1;
	}

	/* Perhaps this is overly restrictive? Should we perhaps be able to
	 * rebless altars and the like? */
	if (ob->type != PLAYER)
	{
		new_draw_info_format(NDI_UNIQUE, op, "%s is not a player - can not change its god", ob->name);
		return 1;
	}

	change_skill(ob, SK_PRAYING);

	if (!ob->chosen_skill || ob->chosen_skill->stats.sp != SK_PRAYING)
	{
		new_draw_info_format(NDI_UNIQUE, op, "%s doesn't have praying skill.", ob->name);
		return 1;
	}

	god = find_god(str);

	if (god == NULL)
	{
		new_draw_info_format(NDI_UNIQUE, op, "No such god %s.", str);
		return 1;
	}

	become_follower(ob, god);

	return 1;
}

/**
 * Kicks a player from the server.
 *
 * If both parameters are NULL, will kick all players.
 * @param ob DM kicking.
 * @param params Player to kick. Must be a full name match.
 * @return 1. */
int command_kick(object *ob, char *params)
{
	player *pl, *pl_next;

	if (ob && params == NULL)
	{
		new_draw_info_format(NDI_UNIQUE, ob, "Use: /kick <name>");
		return 1;
	}

	if (ob && ob->name && !strncasecmp(ob->name, params, MAX_NAME))
	{
		new_draw_info_format(NDI_UNIQUE, ob, "You can't /kick yourself!");
		return 1;
	}

	for (pl = first_player; pl; pl = pl_next)
	{
		pl_next = pl->next;

		/* Ignore players not playing. */
		if (pl->state != ST_PLAYING)
		{
			continue;
		}

		if (!ob || (pl->ob != ob && pl->ob->name && !strncasecmp(pl->ob->name, params, MAX_NAME)))
		{
			object *op = pl->ob;

			remove_ob(op);
			check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
			op->direction = 0;

			if (params)
			{
				new_draw_info_format(NDI_UNIQUE | NDI_ALL, ob, "%s was kicked out of the game.", op->name);
			}

			LOG(llevInfo, "%s was kicked out of the game by %s.\n", op->name, ob ? ob->name : "a shutdown");

			CONTR(op)->socket.status = Ns_Dead;
			remove_ns_dead_player(CONTR(op));
		}
	}

	return 1;
}

/**
 * Totally shuts down the server.
 * @param op DM shutting down the server.
 * @param params Ignored.
 * @return 1. */
int command_shutdown_now(object *op, char *params)
{
	(void) params;

	LOG(llevSystem, "Server shutdown started by %s\n", op->name);
	command_kick(NULL, NULL);
	cleanup();
	exit(0);

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
		new_draw_info(NDI_UNIQUE, op, "Go to what map?\nUsage: /goto <map> x y");
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

	new_draw_info_format(NDI_UNIQUE, op, "Difficulty: %d.", op->map->difficulty);

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
		new_draw_info(NDI_UNIQUE, op, "Usage: /freeze [ticks] <player>");
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
			new_draw_info(NDI_UNIQUE, op, "Usage: /freeze [ticks] <player>");
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

	new_draw_info(NDI_UNIQUE | NDI_RED, pl->ob, "You have been frozen by the DM!");

	new_draw_info_format(NDI_UNIQUE, op, "You freeze %s for %d ticks.", pl->ob->name, ticks);

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
	player *pl;

	if (!op)
	{
		return 0;
	}

	if (params == NULL)
	{
		new_draw_info(NDI_UNIQUE, op, "Usage: /summon <player>.");
		return 1;
	}

	pl = get_other_player_from_name(op, params);

	if (!pl)
	{
		return 1;
	}

	i = find_free_spot(op->arch, op, op->map, op->x, op->y, 1, SIZEOFFREE1 + 1);

	if (i == -1 || op->x + freearr_x[i] < 0 || op->y + freearr_y[i] < 0 || op->x + freearr_x[i] >= MAP_WIDTH(op->map) || op->y + freearr_y[i] >= MAP_HEIGHT(op->map))
	{
		new_draw_info(NDI_UNIQUE, op, "Can not find a free spot to place summoned player.");
		return 1;
	}

	remove_ob(pl->ob);
	pl->ob->x = op->x + freearr_x[i];
	pl->ob->y = op->y + freearr_y[i];
	insert_ob_in_map(pl->ob, op->map, NULL, INS_NO_MERGE);
	new_draw_info(NDI_UNIQUE, pl->ob, "You are summoned.");
	new_draw_info_format(NDI_UNIQUE, op, "You summon %s.", pl->ob->name);
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
	player *pl;

	if (!op)
	{
		return 0;
	}

	if (params == NULL)
	{
		new_draw_info(NDI_UNIQUE, op, "Usage: /teleport <player>.");
		return 1;
	}

	pl = get_other_player_from_name(op, params);

	if (!pl)
	{
		return 1;
	}

	i = find_free_spot(pl->ob->arch, pl->ob, pl->ob->map, pl->ob->x, pl->ob->y, 1, SIZEOFFREE1 + 1);

	if (i == -1 || pl->ob->x + freearr_x[i] < 0 || pl->ob->y + freearr_y[i] < 0 || pl->ob->x + freearr_x[i] >= MAP_WIDTH(pl->ob->map) || pl->ob->y + freearr_y[i] >= MAP_HEIGHT(pl->ob->map))
	{
		new_draw_info(NDI_UNIQUE, op, "Can not find a free spot to teleport to.");
		return 1;
	}

	remove_ob(op);
	op->x = pl->ob->x + freearr_x[i];
	op->y = pl->ob->y + freearr_y[i];
	insert_ob_in_map(op, pl->ob->map, NULL, INS_NO_MERGE);

	if (!CONTR(op)->dm_stealth)
	{
		new_draw_info(NDI_UNIQUE, pl->ob, "You see a portal open.");
	}

	new_draw_info_format(NDI_UNIQUE, op, "You teleport to %s.", pl->ob->name);

	return 1;
}

/**
 * DM wants to create an object.
 * @param op DM.
 * @param params Object variables.
 * @return 1. */
int command_create(object *op, char *params)
{
	object *tmp = NULL;
	uint32 i;
	int magic, set_magic = 0, set_nrof = 0, gotquote, gotspace;
	uint32 nrof;
	char *cp, *bp, *bp2, *bp3, *bp4, *endline;
	archetype *at;
	artifact *art = NULL;

	if (params == NULL)
	{
		new_draw_info(NDI_UNIQUE, op, "Usage: /create [nr] [magic] <archetype> [ of <artifact>] [variable_to_patch setting]");
		return 1;
	}

	bp = params;

	/* We need to know where the line ends */
	endline = bp + strlen(bp);

	if (sscanf(bp, "%u ", &nrof))
	{
		if (!(bp = strchr(params, ' ')))
		{
			new_draw_info(NDI_UNIQUE, op, "Usage: /create [nr] [magic] <archetype> [ of <artifact>] [variable_to_patch setting]");
			return 1;
		}

		bp++;
		set_nrof = 1;
	}

	if (sscanf(bp, "%d ", &magic))
	{
		if (!(bp = strchr(bp, ' ')))
		{
			new_draw_info(NDI_UNIQUE, op, "Usage: /create [nr] [magic] <archetype> [ of <artifact>] [variable_to_patch setting]");
			return 1;
		}

		bp++;
		set_magic = 1;
	}

	if ((cp = strstr(bp, " of ")))
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
	if (!(at = find_archetype(bp)))
	{
		new_draw_info(NDI_UNIQUE, op, "No such archetype or artifact name.");
		return 1;
	}

	if (cp)
	{
		if (find_artifactlist(at->clone.type) == NULL)
		{
			new_draw_info_format(NDI_UNIQUE, op, "No artifact list for type %d\n", at->clone.type);
		}
		else
		{
			art = find_artifactlist(at->clone.type)->items;

			do
			{
				if (!strcmp(art->name, cp))
				{
					break;
				}

				art = art->next;
			} while (art);

			if (!art)
			{
				new_draw_info_format(NDI_UNIQUE, op, "No such artifact ([%d] of %s)", at->clone.type, cp);
			}
		}
	}

	/* Rather than have two different blocks with a lot of similar code,
	 * just create one object, do all the processing, and then determine
	 * if that one object should be inserted or if we need to make copies. */
	tmp = arch_to_object(at);

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
	}

	/* This entire block here tries to find variable pairings,
	 * eg, 'hp 4' or the like. The mess here is that values
	 * can be quoted (eg "my cool sword"); So the basic logic
	 * is we want to find two spaces, but if we got a quote,
	 * any spaces there don't count. */
	while (*bp2 && bp2 <= endline)
	{
		bp4 = NULL;
		gotspace = 0;
		gotquote = 0;

		/* Find the first quote. */
		for (bp3 = bp2; *bp3 && gotspace < 2; bp3++)
		{
			/* Found a quote. */
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
			}
			else if (*bp3 == ' ')
			{
				gotspace++;
			}
		}

		if (!gotquote)
		{
			/* Then find the second space. */
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

		if (!bp4)
		{
			/* Unfortunately, we've clobbered lots of values, so printing
			 * out what we have probably isn't useful. Break out, because
			 * trying to recover is probably won't get anything useful
			 * anyways, and we'd be confused about end of line pointers
			 * anyways. */
			new_draw_info_format(NDI_UNIQUE, op, "Malformed create line: %s", bp2);
			break;
		}

		/* bp2 should still point to the start of this line,
		 * with bp3 pointing to the end. */
		if (set_variable(tmp, bp2) == -1)
		{
			new_draw_info_format(NDI_UNIQUE, op, "Unknown variable %s", bp2);
		}
		else
		{
			new_draw_info_format(NDI_UNIQUE, op, "(%s#%d)->%s", tmp->name, tmp->count, bp2);
		}

		if (gotquote)
		{
			bp2 = bp4 + 2;
		}
		else
		{
			bp2 = bp4 + 1;
		}
	}

	if (at->clone.nrof)
	{
		if (set_nrof)
		{
			tmp->nrof = nrof;
		}

		if (tmp->randomitems)
		{
			create_treasure(tmp->randomitems, tmp, GT_APPLY, tmp->type == MONSTER ? tmp->level : get_enviroment_level(tmp), T_STYLE_UNSET, ART_CHANCE_UNSET, 0, NULL);
		}

		/* If the created object is alive or is multi arch, insert it on
		 * the map. */
		if (IS_LIVE(tmp) || tmp->more)
		{
			if (tmp->type == MONSTER)
			{
				fix_monster(tmp);
			}

			insert_ob_in_map(tmp, op->map, op, INS_NO_MERGE | INS_NO_WALK_ON);
		}
		/* Into the DM's inventory otherwise. */
		else
		{
			tmp = insert_ob_in_ob(tmp, op);
			esrv_send_item(op, tmp);
		}

		return 1;
	}

	for (i = 0; i < (set_nrof ? nrof : 1); i++)
	{
		archetype *atmp;
		object *prev = NULL, *head = NULL, *dup;

		for (atmp = at; atmp; atmp = atmp->more)
		{
			dup = arch_to_object(atmp);

			/* The head is what contains all the important bits,
			 * so just copying it over should be fine. */
			if (head == NULL)
			{
				head = dup;
				copy_object(tmp, dup, 0);
			}

			dup->x = op->x + dup->arch->clone.x;
			dup->y = op->y + dup->arch->clone.y;
			dup->map = op->map;

			if (head != dup)
			{
				dup->head = head;
				prev->more = dup;
			}

			prev = dup;
		}

		if (head->randomitems)
		{
			create_treasure(head->randomitems, head, GT_APPLY, head->type == MONSTER ? head->level : get_enviroment_level(head), T_STYLE_UNSET, ART_CHANCE_UNSET, 0, NULL);
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
 * Shows the inventory of some object.
 * @param op Player.
 * @param params Object count to get the inventory of. If NULL then
 * defaults to op.
 * @return 1 unless params is NULL. */
int command_inventory(object *op, char *params)
{
	object *ob = NULL, *tmp;
	char *cp;

	if (!params)
	{
		new_draw_info(NDI_UNIQUE, op, "Inventory of what object?");
		return 0;
	}

	params = strtok(params, "$");
	cp = strtok(NULL, "$");

	if (!strncmp(params, "me", 2))
	{
		ob = op;
	}
	else
	{
		ob = find_object_both(op, params);

		if (!ob)
		{
			new_draw_info(NDI_UNIQUE, op, "No such object.");
			return 0;
		}
	}

	new_draw_info_format(NDI_UNIQUE, op, "\nInventory of '%s':\n", query_name(ob, op));

	for (tmp = ob->inv; tmp; tmp = tmp->below)
	{
		if (cp && !item_matched_string(op, tmp, cp))
		{
			continue;
		}

		new_draw_info_format(NDI_UNIQUE, op, "#~%d~: %s", tmp->count, query_name(tmp, op));
	}

	return 1;
}

/**
 * Dumps the difference between an object and its archetype.
 * @param op DM.
 * @param params Object to dump.
 * @return 1. */
int command_dump(object *op, char *params)
{
	object *tmp;
	StringBuffer *sb;
	char *diff;

	if (params != NULL && !strcmp(params, "me"))
	{
		tmp = op;
	}
	else if (params == NULL || !(tmp = find_object_both(op, params)))
	{
		new_draw_info(NDI_UNIQUE, op, "Dump what object?");
		return 1;
	}

	sb = stringbuffer_new();
	stringbuffer_append_printf(sb, "count %d\n", tmp->count);
	dump_object(tmp, sb);
	diff = stringbuffer_finish(sb);
	new_draw_info(NDI_UNIQUE, op, diff);
	free(diff);
	return 1;
}

/**
 * DM wants to alter an object.
 * @param op DM.
 * @param params Object and what to patch.
 * @return 1. */
int command_patch(object *op, char *params)
{
	char *arg, *arg2;
	object *tmp = NULL;

	if (params != NULL)
	{
		if (!strncmp(params, "me", 2))
		{
			tmp = op;
		}
		else
		{
			char name[MAX_BUF];

			if (sscanf(params, "%s", name) == 1)
			{
				tmp = find_object_both(op, name);
			}
		}
	}

	if (tmp == NULL)
	{
		new_draw_info(NDI_UNIQUE, op, "Patch what object?");
		return 1;
	}

	arg = strchr(params, ' ');

	if (arg == NULL)
	{
		new_draw_info(NDI_UNIQUE, op, "Patch what values?");
		return 1;
	}

	if ((arg2 = strchr(++arg, ' ')))
	{
		arg2++;
	}

	if (!strncmp(arg, "msg", 3))
	{
		char buf[HUGE_BUF / 2];

		arg += 4;

		if (!arg || *arg == '\0')
		{
			FREE_AND_CLEAR_HASH(tmp->msg);
			new_draw_info_format(NDI_UNIQUE, op, "(%s#%d)->msg=", tmp->name, tmp->count);
			return 1;
		}

		buf[0] = '\0';

		if (tmp->msg)
		{
			strncpy(buf, tmp->msg, sizeof(buf) - 1);
		}

		convert_newline(arg);

		if (buf_overflow(buf, arg, sizeof(buf) - 1))
		{
			new_draw_info(NDI_UNIQUE | NDI_RED, op, "Message string would overflow.");
			return 1;
		}

		strncat(buf, arg, sizeof(buf) - strlen(buf) - 1);
		FREE_AND_COPY_HASH(tmp->msg, buf);
		new_draw_info_format(NDI_UNIQUE, op, "(%s#%d)->msg=%s", tmp->name, tmp->count, buf);
	}
	else if (set_variable(tmp, arg) == -1)
	{
		new_draw_info_format(NDI_UNIQUE, op, "Unknown variable %s", arg);
	}
	else
	{
		new_draw_info_format(NDI_UNIQUE, op, "(%s#%d)->%s=%s", tmp->name, tmp->count, arg, arg2);
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
	object *tmp;

	if (params == NULL || !(tmp = find_object_both(op, params)))
	{
		new_draw_info(NDI_UNIQUE, op, "Remove what object?");
		return 1;
	}

	if (tmp->type == PLAYER)
	{
		new_draw_info(NDI_UNIQUE, op, "Cannot remove a player!");
		return 1;
	}

	if (QUERY_FLAG(tmp, FLAG_REMOVED))
	{
		new_draw_info_format(NDI_UNIQUE, op, "%s is already removed!", query_name(tmp, NULL));
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
	int snr;
	sint64 exp;
	object *exp_skill, *exp_ob;
	player *pl;

	if (params == NULL || sscanf(params, "%s %d %"FMT64, buf, &snr, &exp) != 3)
	{
		int i;

		new_draw_info(NDI_UNIQUE, op, "Usage: /addexp <who> <skill nr> <exp>\nSkills/Nr: ");

		for (i = 0; i < NROFSKILLS; i++)
		{
			new_draw_info_format(NDI_UNIQUE, op, "%d: %s", i, skills[i].name);
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
		new_draw_info(NDI_UNIQUE, op, "No such player.");
		return 1;
	}

	/* Safety check */
	if (snr < 0 || snr >= NROFSKILLS)
	{
		new_draw_info(NDI_UNIQUE, op, "No such skill.");
		return 1;
	}

	exp_skill = pl->skill_ptr[snr];

	/* Our player doesn't have this skill? */
	if (!exp_skill)
	{
		new_draw_info_format(NDI_UNIQUE, op, "Player %s does not know the skill '%s'.", pl->ob->name, skills[snr].name);
		return 0;
	}

	/* If we are full in this skill, there is nothing to do */
	if (exp_skill->level >= MAXLEVEL && exp > 0)
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
		new_draw_info_format(NDI_UNIQUE, op, "Current speed is %ld.", max_time);
		return 1;
	}

	set_max_time(i);
	reset_sleep();
	new_draw_info(NDI_UNIQUE, op, "The speed has changed.");
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
		new_draw_info(NDI_UNIQUE, op, "Who?");
		return 1;
	}

	for (pl = first_player; pl != NULL; pl = pl->next)
	{
		if (!strcmp(pl->ob->name, params))
		{
			new_draw_info_format(NDI_UNIQUE, op, "Str : %-2d      H.P.   : %-4d  MAX : %d", pl->ob->stats.Str, pl->ob->stats.hp, pl->ob->stats.maxhp);
			new_draw_info_format(NDI_UNIQUE, op, "Dex : %-2d      S.P.   : %-4d  MAX : %d", pl->ob->stats.Dex, pl->ob->stats.sp, pl->ob->stats.maxsp);
			new_draw_info_format(NDI_UNIQUE, op, "Con : %-2d      AC     : %-4d  WC  : %d", pl->ob->stats.Con, pl->ob->stats.ac, pl->ob->stats.wc);
			new_draw_info_format(NDI_UNIQUE, op, "Wis : %-2d      EXP    : %"FMT64, pl->ob->stats.Wis, pl->ob->stats.exp);
			new_draw_info_format(NDI_UNIQUE, op, "Cha : %-2d      Food   : %d", pl->ob->stats.Cha, pl->ob->stats.food);
			new_draw_info_format(NDI_UNIQUE, op, "Int : %-2d      Damage : %d", pl->ob->stats.Int, pl->ob->stats.dam);
			new_draw_info_format(NDI_UNIQUE, op, "Pow : %-2d      Grace  : %d", pl->ob->stats.Pow, pl->ob->stats.grace);
			return 1;
		}
	}

	if (pl == NULL)
	{
		new_draw_info(NDI_UNIQUE, op, "No such player.");
	}

	return 1;
}

/**
 * Resets a map.
 * @param op DM.
 * @param params Map to reset. Can be NULL for current op's map, or a map
 * path.
 * @return 1. */
int command_resetmap(object *op, char *params)
{
	mapstruct *m;
	shstr *path;

	if (params == NULL)
	{
		m = has_been_loaded_sh(op->map->path);
	}
	else
	{
		shstr *mapfile_sh = add_string(params);

		m = has_been_loaded_sh(mapfile_sh);
		free_string_shared(mapfile_sh);
	}

	if (m == NULL)
	{
		new_draw_info(NDI_UNIQUE, op, "No such map.");
		return 1;
	}

	if (MAP_UNIQUE(m) && MAP_NOSAVE(m))
	{
		new_draw_info(NDI_UNIQUE, op, "Cannot reset unique no-save map.");
		return 1;
	}

	if (!strncmp(m->path, "/random/", 8))
	{
		new_draw_info(NDI_UNIQUE, op, "You cannot reset a random map.");
		return 1;
	}

	if (m->in_memory != MAP_IN_MEMORY)
	{
		LOG(llevBug, "BUG: Tried to swap out map which was not in memory.\n");
		return 0;
	}

	new_draw_info_format(NDI_UNIQUE, op, "Start resetting map %s.", m->path);
	new_draw_info_format(NDI_UNIQUE, op, "Removed %d players from map. Reset map.", dm_map_remove_players(m));
	m->reset_time = seconds();
	m->map_flags |= MAP_FLAG_FIXED_RTIME;
	/* Store the path, so we can load it after swapping is done. */
	path = add_refcount(m->path);
	swap_map(m, 1);

	m = ready_map_name(path, MAP_NAME_SHARED | (MAP_UNIQUE(m) ? MAP_PLAYER_UNIQUE : 0));
	free_string_shared(path);
	new_draw_info(NDI_UNIQUE, op, "Resetmap done.");
	dm_map_reinsert_players(m, op);

	return 1;
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
	CONTR(op)->followed_player[0] = '\0';
	CLEAR_FLAG(op, FLAG_WIZPASS);
	CLEAR_MULTI_FLAG(op, FLAG_FLYING);
	fix_player(op);
	CONTR(op)->socket.update_tile = 0;
	esrv_send_inventory(op, op);
	CONTR(op)->update_los = 1;
	new_draw_info(NDI_UNIQUE, op, "DM mode deactivated.");

	return 1;
}

/**
 * Check to see if player is allowed to become a DM.
 * @param op The player object trying to become a DM
 * @param pl_passwd Password can be used to become a DM if it matches one
 * of passwords in the file.
 * @return 1 if the object can become a DM, 0 otherwise.
 * @todo Should be rewritten to use objectlinks, similar to ban.c. */
static int checkdm(object *op, char *pl_passwd)
{
	char name[MAX_BUF], passwd[MAX_BUF], host[MAX_BUF], buf[MAX_BUF], filename[MAX_BUF];
	FILE *fp;

	snprintf(filename, sizeof(filename), "%s/%s", settings.localdir, DMFILE);

	if ((fp = fopen(filename, "r")) == NULL)
	{
		LOG(llevDebug, "Could not read DM file.\n");
		return 0;
	}

	while(fgets(buf, sizeof(buf), fp) != NULL)
	{
		if (buf[0] == '#' || buf[0] == '\n')
		{
			continue;
		}

		if (sscanf(buf, "%[^:]:%[^:]:%s\n", name, passwd, host) != 3)
		{
			LOG(llevBug, "BUG: malformed dm file entry: %s", buf);
		}
		else if ((!strcmp(name, "*") || (op->name && !strcmp(op->name, name))) && (!strcmp(passwd, "*") || !strcmp(passwd, pl_passwd)) && (!strcmp(host, "*") || !strcmp(host, CONTR(op)->socket.host)))
		{
			fclose(fp);
			return 1;
		}
	}

	fclose(fp);

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
		/* First remove all the spells */
		send_spelllist_cmd(op, NULL, SPLIST_MODE_REMOVE);
		command_nowiz(op, params);
		/* Now that we are out of DM mode, add the known ones back */
		send_spelllist_cmd(op, NULL, SPLIST_MODE_ADD);
		return 1;
	}

	if (op->type != PLAYER || !CONTR(op))
	{
		return 0;
	}

	if (checkdm(op, (params ? params : "*")))
	{
		SET_FLAG(op, FLAG_WIZ);
		SET_FLAG(op, FLAG_WAS_WIZ);
		SET_FLAG(op, FLAG_WIZPASS);

		new_draw_info_format(NDI_UNIQUE, op, "DM mode activated for %s!", op->name);
		SET_MULTI_FLAG(op, FLAG_FLYING);

		esrv_send_inventory(op, op);

		/* Send all the spells */
		send_spelllist_cmd(op, NULL, SPLIST_MODE_ADD);

		clear_los(op);

		/* force a draw_look() */
		CONTR(op)->socket.update_tile = 0;
		CONTR(op)->update_los = 1;
	}

	return 1;
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
		new_draw_info(NDI_UNIQUE, op, "Unknown spell.");
		return 1;
	}

	if (check_spell_known(op, spell))
	{
		new_draw_info_format(NDI_UNIQUE, op, "You already know the spell %s.", params);
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
		new_draw_info(NDI_UNIQUE, op, "Unknown spell.");
		return 1;
	}

	do_forget_spell(op, spell);
	return 1;
}

/**
 * Lists all plugins currently loaded with their IDs and full names.
 * @param op DM.
 * @param params Unused.
 * @return 1. */
int command_listplugins(object *op, char *params)
{
	(void) params;

	display_plugins_list(op);
	return 1;
}

/**
 * Loads the given plugin. The DM specifies the name of the library to
 * load (no pathname is needed). Do not ever attempt to load the same
 * plugin more than once at a time, or undefined behavior could happen.
 * @param op DM loading a plugin.
 * @param params Should be the plugin's name, eg plugin_python.so.
 * @return 1. */
int command_loadplugin(object *op, char *params)
{
	char buf[MAX_BUF];

	if (!params)
	{
		new_draw_info(NDI_UNIQUE, op, "Load what plugin?");
		return 1;
	}

	snprintf(buf, sizeof(buf), "%s/%s", PLUGINDIR, params);
	init_plugin(buf);

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
		new_draw_info(NDI_UNIQUE, op, "Unload what plugin?");
		return 1;
	}

	remove_plugin(params);

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
				exit(0);
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

		new_draw_info(NDI_PLAYER | NDI_UNIQUE | NDI_ALL | NDI_GREEN, NULL, "[Server]: ** SERVER SHUTDOWN STARTED **");

		if (reason)
		{
			new_draw_info_format(NDI_PLAYER | NDI_UNIQUE | NDI_ALL | NDI_GREEN, NULL, "[Server]: %s", reason);
		}

		if (t_sec)
		{
			new_draw_info_format(NDI_PLAYER | NDI_UNIQUE | NDI_ALL | NDI_GREEN, NULL, "[Server]: SERVER REBOOT in %d minutes and %d seconds", t_min, t_sec);
		}
		else
		{
			new_draw_info_format(NDI_PLAYER | NDI_UNIQUE | NDI_ALL | NDI_GREEN, NULL, "[Server]: SERVER REBOOT in %d minutes", t_min);
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
			new_draw_info(NDI_PLAYER | NDI_UNIQUE | NDI_ALL | NDI_GREEN, NULL, "[Server]: ** SERVER GOES DOWN NOW!!! **");

			if (reason)
			{
				new_draw_info_format(NDI_PLAYER | NDI_UNIQUE | NDI_ALL | NDI_GREEN, NULL, "[Server]: %s", reason);
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
				new_draw_info_format(NDI_PLAYER | NDI_UNIQUE | NDI_ALL | NDI_GREEN, NULL, "[Server]: SERVER REBOOT in %d minutes and %d seconds", t_min, t_sec);
			}
			else
			{
				new_draw_info_format(NDI_PLAYER | NDI_UNIQUE | NDI_ALL | NDI_GREEN, NULL, "[Server]: SERVER REBOOT in %d minutes", t_min);
			}
		}
	}
}

/**
 * Set a custom Message of the Day in the file.
 * @param op DM.
 * @param params The new message of the day, if "original" revert to the
 * original MotD.
 * @return 1 on success, 0 on failure. */
int command_motd_set(object *op, char *params)
{
	char filename[MAX_BUF];

	/* No params, show usage. */
	if (params == NULL)
	{
		new_draw_info(NDI_UNIQUE, op, "Usage:\nRevert to original MotD: /motd_set original\nAppend to custom MotD: /motd_set message");
		return 0;
	}

	snprintf(filename, sizeof(filename), "%s/motd_custom", settings.localdir);

	/* If we are not reverting to original MotD */
	if (strcmp(params, "original"))
	{
		FILE *fp;

		if (!(fp = fopen(filename, "a")))
		{
			new_draw_info(NDI_UNIQUE | NDI_RED, op, "Could not open file for appending data.");
			return 0;
		}

		fprintf(fp, "%s\n", params);
		fclose(fp);

		new_draw_info(NDI_UNIQUE | NDI_GREEN, op, "Appended to custom Message of the Day.");
	}
	else
	{
		unlink(filename);
		new_draw_info(NDI_UNIQUE | NDI_GREEN, op, "Reverted original Message of the Day.");
	}

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
		if (add_ban(params + 4))
		{
			new_draw_info(NDI_UNIQUE | NDI_GREEN, op, "Added new ban successfully.");
		}
		else
		{
			new_draw_info(NDI_UNIQUE | NDI_RED, op, "Failed to add new ban!");
		}
	}
	/* Remove ban */
	else if (strncmp(params, "remove ", 7) == 0)
	{
		if (remove_ban(params + 7))
		{
			new_draw_info(NDI_UNIQUE | NDI_GREEN, op, "Removed ban successfully.");
		}
		else
		{
			new_draw_info(NDI_UNIQUE | NDI_RED, op, "Failed to remove ban!");
		}
	}
	/* List bans */
	else if (strncmp(params, "list", 4) == 0)
	{
		list_bans(op);
	}

	return 1;
}

/**
 * Set debug level.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return 1. */
int command_debug(object *op, char *params)
{
	int i;

	if (params == NULL || !sscanf(params, "%d", &i))
	{
		new_draw_info_format(NDI_UNIQUE, op, "Debug level is %d.", settings.debug);
		return 1;
	}

	settings.debug = (enum LogLevel) FABS(i);
	new_draw_info_format(NDI_UNIQUE, op, "Set debug level to %d.", i);
	return 1;
}

/**
 * Full dump of objects below the DM.
 * @param op Object requesting this.
 * @param params Unused.
 * @return 1. */
int command_dumpbelowfull(object *op, char *params)
{
	object *tmp;
	StringBuffer *sb;
	char *diff;

	(void) params;

	new_draw_info(NDI_UNIQUE, op, "OBJECTS ON THIS TILE");
	new_draw_info(NDI_UNIQUE, op, "-------------------");

	for (tmp = get_map_ob(op->map, op->x, op->y); tmp; tmp = tmp->above)
	{
		/* Exclude the DM player object */
		if (tmp == op)
		{
			continue;
		}

		sb = stringbuffer_new();
		stringbuffer_append_printf(sb, "count %d\n", tmp->count);
		dump_object(tmp, sb);
		diff = stringbuffer_finish(sb);
		new_draw_info(NDI_UNIQUE, op, diff);
		free(diff);

		if (tmp->above && tmp->above != op)
		{
			new_draw_info(NDI_UNIQUE, op, ">next object<");
		}
	}

	new_draw_info(NDI_UNIQUE, op, "------------------");

	return 1;
}

/**
 * Dump objects below the DM.
 * @param op Object requesting this.
 * @param params Unused.
 * @return 1. */
int command_dumpbelow(object *op, char *params)
{
	object *tmp;
	int i = 0;

	(void) params;

	new_draw_info(NDI_UNIQUE, op, "OBJECTS ON THIS TILE");
	new_draw_info(NDI_UNIQUE, op, "-------------------");

	for (tmp = get_map_ob(op->map, op->x, op->y); tmp; tmp = tmp->above, i++)
	{
		/* Exclude the DM player object */
		if (tmp == op)
		{
			continue;
		}

		new_draw_info_format(NDI_UNIQUE, op, "#%d  >%s<  >%s<  >%s<", i, query_name(tmp, NULL), tmp->arch ? (tmp->arch->name ? tmp->arch->name : "no arch name") : "NO ARCH", tmp->env ? query_name(tmp->env, NULL) : "");
	}

	new_draw_info(NDI_UNIQUE, op, "------------------");

	return 1;
}

/**
 * Wizpass command. Used by DMs to toggle walking through walls on/off.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return 1 on success, 0 on failure. */
int command_wizpass(object *op, char *params)
{
	int i;

	if (!op)
	{
		return 0;
	}

	if (!params)
	{
		i = !QUERY_FLAG(op, FLAG_WIZPASS);
	}
	else
	{
		i = onoff_value(params);
	}

	if (i)
	{
		new_draw_info(NDI_UNIQUE, op, "You will now walk through walls.");
		SET_FLAG(op, FLAG_WIZPASS);
	}
	else
	{
		new_draw_info(NDI_UNIQUE, op, "You will now be stopped by walls.");
		CLEAR_FLAG(op, FLAG_WIZPASS);
	}

	return 1;
}

/**
 * Dumps all archetypes.
 * @param op Unused.
 * @param params Unused.
 * @return 1. */
int command_dumpallarchetypes(object *op, char *params)
{
	(void) params;
	(void) op;
	dump_all_archetypes();
	return 1;
}

/**
 * DM stealth command. Used by DMs to make the DM hidden from all other
 * players. It also works when DM logs in without DM flag set or leaves
 * the DM mode.
 * @param op Object requesting this.
 * @param params Unused.
 * @return 1. */
int command_dm_stealth(object *op, char *params)
{
	(void) params;

	if (op->type != PLAYER || !CONTR(op))
	{
		return 1;
	}

	if (CONTR(op)->dm_stealth)
	{
		CONTR(op)->dm_stealth = 0;
	}
	else
	{
		CONTR(op)->dm_stealth = 1;
	}

	new_draw_info_format(NDI_UNIQUE, op, "Toggled dm_stealth to %d.", CONTR(op)->dm_stealth);
	return 1;
}

/**
 * Toggle DM light on/off. DM light will light up all maps for the DM.
 * @param op Object requesting this.
 * @param params Unused.
 * @return 1. */
int command_dm_light(object *op, char *params)
{
	(void) params;

	if (op->type != PLAYER || !CONTR(op))
	{
		return 1;
	}

	if (CONTR(op)->dm_light)
	{
		CONTR(op)->dm_light = 0;
	}
	else
	{
		CONTR(op)->dm_light = 1;
	}

	new_draw_info_format(NDI_UNIQUE, op, "Toggled dm_light to %d.", CONTR(op)->dm_light);
	return 1;
}

/**
 * /dm_password command.
 * @param op DM.
 * @param params Command parameters.
 * @return 0. */
int command_dm_password(object *op, char *params)
{
	FILE *fp, *fpout;
	const char *name_hash;
	char filename[MAX_BUF], bufall[MAX_BUF], outfile[MAX_BUF];
	char name[MAX_BUF], password[MAX_BUF];

	if (!params || sscanf(params, "%s %s", name, password) != 2)
	{
		new_draw_info(NDI_UNIQUE | NDI_RED, op, "Usage: /dm_password <player name> <new password>");
		return 0;
	}

	adjust_player_name(name);
	snprintf(filename, sizeof(filename), "%s/%s/%s/%s.pl", settings.localdir, settings.playerdir, name, name);

	if (!player_exists(name))
	{
		new_draw_info_format(NDI_UNIQUE, op, "Player %s doesn't exist.", name);
	}

	strncpy(outfile, filename, sizeof(outfile));
	strncat(outfile, ".tmp", sizeof(outfile) - strlen(outfile) - 1);

	if (!(fp = fopen(filename, "r")))
	{
		new_draw_info_format(NDI_UNIQUE | NDI_RED, op, "Error opening file %s.", filename);
		return 0;
	}

	if (!(fpout = fopen(outfile, "w")))
	{
		new_draw_info_format(NDI_UNIQUE | NDI_RED, op, "Error opening file %s.", outfile);
		return 0;
	}

	while (fgets(bufall, sizeof(bufall) - 1, fp))
	{
		if (!strncmp(bufall, "password ", 9))
		{
			fprintf(fpout, "password %s\n", crypt_string(password, NULL));
		}
		else
		{
			fputs(bufall, fpout);
		}
	}

	if ((name_hash = find_string(name)))
	{
		player *pl;

		for (pl = first_player; pl; pl = pl->next)
		{
			if (pl->ob && pl->ob->name == name_hash)
			{
				strcpy(pl->password, crypt_string(password, NULL));
				break;
			}
		}
	}

	fclose(fp);
	fclose(fpout);
	unlink(filename);
	rename(outfile, filename);

	new_draw_info_format(NDI_UNIQUE | NDI_GREEN, op, "Done. Changed password of %s to %s!", name, password);
	return 0;
}

/**
 * Dump active list.
 * @param op Object requesting this.
 * @param params Unused.
 * @return 1. */
int command_dumpactivelist(object *op, char *params)
{
	int count = 0;
	object *tmp;

	(void) params;

	for (tmp = active_objects; tmp; tmp = tmp->active_next)
	{
		count++;
		LOG(llevSystem, "%08d %03d %f %s (%s)\n", tmp->count, tmp->type, tmp->speed, query_short_name(tmp, NULL), tmp->arch->name ? tmp->arch->name : "<NA>");
	}

	new_draw_info_format(NDI_UNIQUE, op, "Active objects: %d (dumped to log)", count);
	LOG(llevSystem, "Active objects: %d\n", count);

	return 1;
}

/**
 * Starts server shutdown timer.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return 1. */
int command_shutdown(object *op, char *params)
{
	char *bp = NULL;
	int i = -2;

	if (params == NULL)
	{
		new_draw_info(NDI_UNIQUE, op, "Usage: /shutdown <seconds> [reason]");
		return 1;
	}

	sscanf(params, "%d ", &i);

	if ((bp = strchr(params, ' ')) != NULL)
	{
		bp++;
	}

	if (bp && bp == 0)
	{
		bp = NULL;
	}

	if (i < -1)
	{
		new_draw_info(NDI_UNIQUE, op, "Usage: /shutdown <seconds> [reason]");
		return 1;
	}

	LOG(llevSystem, "Shutdown agent started!\n");
	shutdown_agent(i, bp);
	new_draw_info_format(NDI_UNIQUE | NDI_GREEN, op, "Shutdown agent started! Timer set to %d seconds.", i);

	return 1;
}

/**
 * Set map light by DM.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return 1 on success, 0 on failure. */
int command_setmaplight(object *op, char *params)
{
	int i;

	if (params == NULL || !sscanf(params, "%d", &i))
	{
		return 0;
	}

	set_map_darkness(op->map, i);

	new_draw_info_format(NDI_UNIQUE, op, "WIZ: set map darkness: %d -> map:%s (%d)", i, op->map->path, MAP_OUTDOORS(op->map));

	return 1;
}

/**
 * Dump map information.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_dumpmap(object *op, char *params)
{
	(void) params;

	if (op)
	{
		dump_map(op->map);
	}

	return 1;
}

/**
 * Dump information about all maps.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_dumpallmaps(object *op, char *params)
{
	(void) params;
	(void) op;

	dump_all_maps();

	return 1;
}

/**
 * Malloc info command.
 *
 * If MEMPOOL_TRACKING is defined, parameters are used to free (and force
 * freeing) empty puddles. Otherwise, malloc_info() is used to display
 * information about memory usage.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
int command_malloc(object *op, char *params)
{
#ifdef MEMPOOL_TRACKING
	if (params)
	{
		int force_flag = 0, i;

		if (strcmp(params, "free") && strcmp(params, "force"))
		{
			new_draw_info(NDI_UNIQUE, op, "Usage: /malloc [free | force]");
			return 1;
		}

		if (strcmp(params, "force") == 0)
		{
			force_flag = 1;
		}

		for (i = 0; i < nrof_mempools; i++)
		{
			if (force_flag == 1 || mempools[i]->flags & MEMPOOL_ALLOW_FREEING)
			{
#if 0
				free_empty_puddles(mempools[i]);
#endif
			}
		}
	}
#else
	(void) params;
#endif

	malloc_info(op);
	return 1;
}

/**
 * Maps command.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
int command_maps(object *op, char *params)
{
	(void) params;

	map_info(op);
	return 1;
}

/**
 * Strings command.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
int command_strings(object *op, char *params)
{
	char buf[HUGE_BUF];

	(void) params;

	LOG(llevSystem, "HASH TABLE DUMP\n");

	ss_dump_statistics(buf, sizeof(buf));
	new_draw_info(NDI_UNIQUE, op, buf);
	LOG(llevSystem, "%s\n", buf);

	ss_dump_table(SS_DUMP_TOTALS, buf, sizeof(buf));
	new_draw_info(NDI_UNIQUE, op, buf);
	LOG(llevSystem, "%s\n", buf);

	return 1;
}

/**
 * Dump the strings table.
 * @param op Wizard.
 * @param params Parameters.
 * @return 1. */
int command_ssdumptable(object *op, char *params)
{
	(void) params;
	(void) op;

	ss_dump_table(SS_DUMP_TABLE, NULL, 0);
	return 1;
}

/**
 * DM wants to follow a player, or stop following a player.
 * @param op Wizard.
 * @param params Player to follow. If NULL, stop following player.
 * @return 0. */
int command_follow(object *op, char *params)
{
	player *pl;

	if (!params)
	{
		if (CONTR(op)->followed_player[0])
		{
			new_draw_info_format(NDI_UNIQUE, op, "You stop following %s.", CONTR(op)->followed_player);
			CONTR(op)->followed_player[0] = '\0';
		}

		return 0;
	}

	pl = get_other_player_from_name(op, params);

	if (!pl)
	{
		return 0;
	}

	CONTR(op)->followed_player[0] = '\0';
	strncpy(CONTR(op)->followed_player, params, sizeof(CONTR(op)->followed_player));

	new_draw_info_format(NDI_UNIQUE | NDI_GREEN, op, "Following %s.", params);
	return 0;
}

/**
 * Insert marked object into another, specified by params.
 * @param op Wizard.
 * @param params Object name or \#ID to insert something into.
 * @return 0. */
int command_insert_into(object *op, char *params)
{
	object *ob, *marked;

	if (!params)
	{
		new_draw_info(NDI_UNIQUE | NDI_RED, op, "What object to insert something into?");
		return 0;
	}

	marked = find_marked_object(op);

	if (!marked)
	{
		new_draw_info(NDI_UNIQUE | NDI_RED, op, "You need to mark the object to insert.");
		return 0;
	}

	ob = find_object_both(op, params);

	if (!ob)
	{
		new_draw_info(NDI_UNIQUE | NDI_RED, op, "No such object.");
		return 0;
	}

	remove_ob(marked);
	insert_ob_in_ob(marked, ob);
	esrv_send_item(op, marked);
	fix_player(op);
	new_draw_info_format(NDI_UNIQUE | NDI_GREEN, op, "Successfully inserted '%s' into '%s'.", query_name(marked, NULL), ob->name);
	return 0;
}

/**
 * Wizard jails player.
 * @param op Wizard.
 * @param params Player to jail.
 * @return 1. */
int command_arrest(object *op, char *params)
{
	object *dummy;
	player *pl;

	if (!params)
	{
		new_draw_info(NDI_UNIQUE | NDI_RED, op, "Usage: /arrest <player>.");
		return 1;
	}

	pl = get_other_player_from_name(op, params);

	if (!pl)
	{
		return 1;
	}

	dummy = get_jail_exit(pl->ob);

	if (!dummy)
	{
		/* We have nowhere to send the prisoner....*/
		new_draw_info(NDI_UNIQUE | NDI_RED, op, "Can't jail player, there is no map to hold them.");
		return 1;
	}

	enter_exit(pl->ob, dummy);
	new_draw_info_format(NDI_UNIQUE | NDI_GREEN, op, "Jailed %s.", pl->ob->name);
	LOG(llevInfo, "Player %s arrested by %s\n", pl->ob->name, op->name);
	return 1;
}

/**
 * /cmd_permission command.
 * @param op Wizard.
 * @param params Parameters.
 * @return 1. */
int command_cmd_permission(object *op, char *params)
{
	char *cp;
	player *pl;
	int i;

	if (!params)
	{
		new_draw_info(NDI_UNIQUE | NDI_RED, op, "Usage: /cmd_permission <player> <add-remove-list> [command]");
		return 1;
	}

	cp = strchr(params, ' ');

	if (!cp)
	{
		return 1;
	}

	*(cp++) = '\0';

	pl = find_player(params);

	if (!pl)
	{
		new_draw_info(NDI_UNIQUE, op, "No such player.");
		return 1;
	}

	if (!strncmp(cp, "add ", 4))
	{
		cp += 4;

		if (cp[0] == '/')
		{
			cp++;
		}

		for (i = 0; i < pl->num_cmd_permissions; i++)
		{
			if (pl->cmd_permissions[i] && !strcmp(pl->cmd_permissions[i], cp))
			{
				new_draw_info_format(NDI_UNIQUE | NDI_RED, op, "%s already has permission for /%s.", pl->ob->name, cp);
				return 1;
			}
		}

		pl->num_cmd_permissions++;
		pl->cmd_permissions = realloc(pl->cmd_permissions, sizeof(char *) * pl->num_cmd_permissions);
		pl->cmd_permissions[pl->num_cmd_permissions - 1] = strdup_local(cp);
		new_draw_info_format(NDI_UNIQUE | NDI_GREEN, op, "%s has been granted permission for /%s.", pl->ob->name, cp);
	}
	else if (!strncmp(cp, "remove ", 7))
	{
		cp += 7;

		if (cp[0] == '/')
		{
			cp++;
		}

		for (i = 0; i < pl->num_cmd_permissions; i++)
		{
			if (pl->cmd_permissions[i] && !strcmp(pl->cmd_permissions[i], cp))
			{
				FREE_AND_NULL_PTR(pl->cmd_permissions[i]);
				new_draw_info_format(NDI_UNIQUE | NDI_GREEN, op, "%s has had permission for /%s command removed.", pl->ob->name, cp);
				return 1;
			}
		}

		new_draw_info_format(NDI_UNIQUE | NDI_RED, op, "%s does not have permission for /%s.", pl->ob->name, cp);
	}
	else if (!strncmp(cp, "list", 4))
	{
		if (pl->cmd_permissions)
		{
			new_draw_info_format(NDI_UNIQUE, op, "%s has permissions for the following commands:\n", pl->ob->name);

			for (i = 0; i < pl->num_cmd_permissions; i++)
			{
				if (pl->cmd_permissions[i])
				{
					new_draw_info_format(NDI_UNIQUE, op, "/%s", pl->cmd_permissions[i]);
				}
			}
		}
		else
		{
			new_draw_info_format(NDI_UNIQUE, op, "%s has no command permissions.", pl->ob->name);
		}
	}

	return 1;
}

/**
 * Saves the DM's current map to the original map file.
 * @param op Player.
 * @param params Parameters.
 * @return 1. */
int command_map_save(object *op, char *params)
{
	char buf[MAX_BUF], path[MAX_BUF], map_path[MAX_BUF];
	struct stat stats;
	mapstruct *m;

	(void) params;

	/* Don't allow doing this for unique or random maps. */
	if (MAP_UNIQUE(op->map) || !strncmp(op->map->path, "/random/", 8))
	{
		new_draw_info(NDI_UNIQUE, op, "Cannot be used on unique or random maps.");
		return 1;
	}

	/* Store the map's path. */
	strncpy(map_path, op->map->path, sizeof(map_path));
	/* Create a path name to the actual map file. */
	strncpy(path, create_pathname(op->map->path), sizeof(path) - 1);
	/* Path to the original file. */
	snprintf(buf, sizeof(buf), "%s.map_old", path);

	/* No original file yet? Create one. */
	if (stat(buf, &stats))
	{
		FILE *fp = fopen(buf, "w");

		if (!fp)
		{
			LOG(llevBug, "BUG: command_map_save(): Could not open '%s' for writing.\n", buf);
			new_draw_info_format(NDI_UNIQUE, op, "ERROR: Could not open '%s' for writing.", buf);
			return 1;
		}

		copy_file(path, fp);
		fclose(fp);
	}

	/* Remove players from the map. */
	m = op->map;
	dm_map_remove_players(m);

	/* Try to save the map. */
	if (new_save_map(m, 1) == -1)
	{
		unlink(path);
		rename(buf, path);
		new_draw_info(NDI_UNIQUE, op, "Map save error!");
	}
	else
	{
		new_draw_info(NDI_UNIQUE, op, "Current map has been saved to the original map file.");
	}

	free_map(m, 1);
	/* Reload the map and re-insert players. */
	m = ready_map_name(map_path, 0);
	dm_map_reinsert_players(m, op);
	return 1;
}

/**
 * Resets DM's current map to the original version.
 * @param op Player.
 * @param params Parameters.
 * @return 1. */
int command_map_reset(object *op, char *params)
{
	char buf[MAX_BUF], path[MAX_BUF];
	struct stat stats;

	(void) params;

	/* Create a path name to the actual map file. */
	strncpy(path, create_pathname(op->map->path), sizeof(path) - 1);
	/* Path to the original file. */
	snprintf(buf, sizeof(buf), "%s.map_old", path);

	/* Is there the original map file? */
	if (!stat(buf, &stats))
	{
		unlink(path);
		rename(buf, path);
		new_draw_info(NDI_UNIQUE, op, "Current map reset.");
	}
	else
	{
		new_draw_info(NDI_UNIQUE, op, "There is no original map to reset to.");
	}

	return 1;
}

/**
 * Patch map header variables of a map.
 * @param op Player.
 * @param params Parameters.
 * @return 1. */
int command_map_patch(object *op, char *params)
{
	if (!params)
	{
		new_draw_info(NDI_UNIQUE, op, "Patch what values?");
		return 1;
	}

	if (map_set_variable(op->map, params) == -1)
	{
		new_draw_info_format(NDI_UNIQUE, op, "Unknown value for map header: %s", params);
	}
	else
	{
		new_draw_info_format(NDI_UNIQUE, op, "(%s)->%s", op->map->name, params);
	}

	return 1;
}

/**
 * The /no_shout command.
 * @param op Wizard.
 * @param params Parameters.
 * @return 1. */
int command_no_shout(object *op, char *params)
{
	player *pl;

	if (!params)
	{
		new_draw_info(NDI_UNIQUE | NDI_RED, op, "Usage: /no_shout <player>");
		return 1;
	}

	pl = find_player(params);

	if (!pl)
	{
		new_draw_info(NDI_UNIQUE, op, "No such player.");
		return 1;
	}

	if (pl->no_shout)
	{
		new_draw_info_format(NDI_UNIQUE, op, "%s is able to shout again.", pl->ob->name);
		pl->no_shout = 0;
	}
	else
	{
		new_draw_info_format(NDI_UNIQUE, op, "%s is now not able to shout.", pl->ob->name);
		pl->no_shout = 1;
	}

	return 1;
}

/**
 * Take an object and put it in DM's inventory.
 * @param op DM.
 * @param params Object to take.
 * @return 1. */
int command_dmtake(object *op, char *params)
{
	object *tmp;

	if (!params)
	{
		new_draw_info(NDI_UNIQUE, op, "Take what object?");
		return 1;
	}

	tmp = find_object_both(op, params);

	if (!tmp)
	{
		new_draw_info(NDI_UNIQUE, op, "No such object.");
		return 1;
	}

	if (tmp->env == op)
	{
		return 1;
	}

	pick_up(op, tmp, 0);
	return 1;
}
