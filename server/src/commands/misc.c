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
 * Handles misc. input request - things like hash table, malloc, maps,
 * who, etc. */

#include <global.h>

/**
 * Show maps information.
 * @param op The object to show the information to. */
void map_info(object *op)
{
	mapstruct *m;
	char map_path[MAX_BUF];
	long sec = seconds();

	new_draw_info_format(NDI_UNIQUE, op, "Current time is: %02ld:%02ld:%02ld.", (sec % 86400) / 3600, (sec % 3600) / 60, sec % 60);
	new_draw_info(NDI_UNIQUE, op, "Path               Pl PlM IM   TO Dif Reset");

	for (m = first_map; m != NULL; m = m->next)
	{
		/* Print out the last 18 characters of the map name... */
		if (strlen(m->path) <= 18)
		{
			strcpy(map_path, m->path);
		}
		else
		{
			strcpy(map_path, m->path + strlen(m->path) - 18);
		}

		new_draw_info_format(NDI_UNIQUE, op, "%-18.18s %2d   %c %4d %2d  %02d:%02d:%02d", map_path, players_on_map(m), m->in_memory ? (m->in_memory == MAP_IN_MEMORY ? 'm' : 's') : 'X', m->timeout, m->difficulty, (MAP_WHEN_RESET(m) % 86400) / 3600, (MAP_WHEN_RESET(m) % 3600) / 60, MAP_WHEN_RESET(m) % 60);
	}
}

/**
 * Show message of the day.
 * @param op The object calling this.
 * @param params Parameters.
 * @return Always returns 1. */
int command_motd(object *op, char *params)
{
	(void) params;

	display_motd(op);
	return 1;
}

/**
 * Counts the number of objects on the list of active objects.
 * @return The number of active objects. */
static int count_active()
{
	int i = 0;
	object *tmp = active_objects;

	while (tmp != NULL)
	{
		tmp = tmp->active_next, i++;
	}

	return i;
}

/**
 * Give out of info about memory usage.
 * @param op The object requesting this. */
void malloc_info(object *op)
{
	int players, nrofmaps;
	int nrm = 0, mapmem = 0, anr, anims, sum_alloc = 0, sum_used = 0, i, tlnr, alnr;
	treasurelist *tl;
	player *pl;
	mapstruct *m;
	archetype *at;
	artifactlist *al;

	for (tl = first_treasurelist, tlnr = 0; tl != NULL; tl = tl->next, tlnr++)
	{
	}

	for (al = first_artifactlist, alnr = 0; al != NULL; al = al->next, alnr++)
	{
	}

	for (at = first_archetype, anr = 0, anims = 0; at != NULL; at = at->more == NULL ? at->next : at->more, anr++)
	{
	}

	for (i = 1; i < num_animations; i++)
	{
		anims += animations[i].num_animations;
	}

	for (pl = first_player, players = 0; pl != NULL; pl = pl->next, players++)
	{
	}

	for (m = first_map, nrofmaps = 0; m != NULL; m = m->next, nrofmaps++)
	{
		if (m->in_memory == MAP_IN_MEMORY)
		{
			mapmem += MAP_WIDTH(m) * MAP_HEIGHT(m) * (sizeof(object *) + sizeof(MapSpace));
			nrm++;
		}
	}

	new_draw_info_format(NDI_UNIQUE, op, "Sizeof: object=%ld  player=%ld  map=%ld", (long) sizeof(object), (long) sizeof(player), (long) sizeof(mapstruct));

	dump_mempool_statistics(op, &sum_used, &sum_alloc);

	new_draw_info_format(NDI_UNIQUE, op, "%4d active objects", count_active());

	new_draw_info_format(NDI_UNIQUE, op, "%4d maps allocated:  %8d", nrofmaps, i = (nrofmaps * sizeof(mapstruct)));
	sum_alloc += i;
	sum_used += nrm * sizeof(mapstruct);

	new_draw_info_format(NDI_UNIQUE, op, "%4d maps in memory:  %8d", nrm, mapmem);
	sum_alloc += mapmem;
	sum_used += mapmem;

	new_draw_info_format(NDI_UNIQUE, op, "%4d archetypes:      %8d", anr, i = (anr * sizeof(archetype)));
	sum_alloc += i;
	sum_used += i;

	new_draw_info_format(NDI_UNIQUE, op, "%4d animations:      %8d", anims, i = (anims * sizeof(Fontindex)));
	sum_alloc += i;
	sum_used += i;

	new_draw_info_format(NDI_UNIQUE, op, "%4d spells:          %8d", NROFREALSPELLS, i = (NROFREALSPELLS * sizeof(spell)));
	sum_alloc += i;
	sum_used += i;

	new_draw_info_format(NDI_UNIQUE, op, "%4d treasurelists    %8d", tlnr, i = (tlnr * sizeof(treasurelist)));
	sum_alloc += i;
	sum_used += i;

	new_draw_info_format(NDI_UNIQUE, op, "%4ld treasures        %8d", nroftreasures, i = (nroftreasures * sizeof(treasure)));
	sum_alloc += i;
	sum_used += i;

	new_draw_info_format(NDI_UNIQUE, op, "%4ld artifacts        %8d", nrofartifacts, i = (nrofartifacts * sizeof(artifact)));
	sum_alloc += i;
	sum_used += i;

	new_draw_info_format(NDI_UNIQUE, op, "%4ld artifacts strngs %8d", nrofallowedstr, i = (nrofallowedstr * sizeof(linked_char)));
	sum_alloc += i;
	sum_used += i;

	new_draw_info_format(NDI_UNIQUE, op, "%4d artifactlists    %8d", alnr, i = (alnr * sizeof(artifactlist)));
	sum_alloc += i;
	sum_used += i;

	new_draw_info_format(NDI_UNIQUE, op, "Total space allocated:%8d", sum_alloc);
	new_draw_info_format(NDI_UNIQUE, op, "Total space used:     %8d", sum_used);
}

/**
 * Give out some info about the map op is located at.
 * @param op The object requesting this information. */
static void current_map_info(object *op)
{
	mapstruct *m = op->map;

	if (!m)
	{
		return;
	}

	new_draw_info_format(NDI_UNIQUE, op, "%s (%s, x: %d, y: %d)", m->name, m->path, op->x, op->y);

	if (QUERY_FLAG(op, FLAG_WIZ))
	{
		new_draw_info_format(NDI_UNIQUE, op, "Players: %d difficulty: %d size: %dx%d start: %dx%d", players_on_map(m), MAP_DIFFICULTY(m), MAP_WIDTH(m), MAP_HEIGHT(m), MAP_ENTER_X(m), MAP_ENTER_Y(m));
	}

	if (m->msg)
	{
		new_draw_info(NDI_UNIQUE, op, m->msg);
	}
}

/**
 * Print out a list of all logged in players in the game.
 * @param op The object requesting this
 * @param params Command parameters
 * @return Always returns 1.
 * @todo Perhaps make a GUI from this like the party GUI
 * and you would be able to press enter on player name
 * and that would bring up the /tell console? */
int command_who(object *op, char *params)
{
	player *pl;
	int ip = 0, il = 0, wiz;
	char buf[MAX_BUF], race[MAX_BUF];

	if (!op)
	{
		return 1;
	}

	new_draw_info(NDI_UNIQUE, op, " ");

	wiz = QUERY_FLAG(op, FLAG_WIZ);

	(void) params;

	for (pl = first_player; pl; pl = pl->next)
	{
		if (pl->dm_stealth && !wiz)
		{
			continue;
		}

		if (!pl->ob->map)
		{
			il++;
			continue;
		}

		ip++;

		if (pl->state == ST_PLAYING)
		{
			if (wiz)
			{
				snprintf(buf, sizeof(buf), "%s (%s) [%s] (#%d)", pl->ob->name, pl->socket.host, pl->ob->map->path, pl->ob->count);
			}
			else
			{
				snprintf(buf, sizeof(buf), "%s the %s %s (lvl %d)", pl->ob->name, gender_noun[object_get_gender(pl->ob)], player_get_race_class(pl->ob, race, sizeof(race)), pl->ob->level);

				if (QUERY_FLAG(pl->ob, FLAG_WIZ))
				{
					strncat(buf, " [WIZ]", sizeof(buf) - strlen(buf) - 1);
				}

				if (pl->afk)
				{
					strncat(buf, " [AFK]", sizeof(buf) - strlen(buf) - 1);
				}

				if (pl->socket.is_bot)
				{
					strncat(buf, " [BOT]", sizeof(buf) - strlen(buf) - 1);
				}

				if (pl->class_ob && pl->class_ob->title)
				{
					strncat(buf, " ", sizeof(buf) - strlen(buf) - 1);
					strncat(buf, pl->class_ob->title, sizeof(buf) - strlen(buf) - 1);
				}
			}

			new_draw_info(NDI_UNIQUE, op, buf);
		}
	}

	new_draw_info_format(NDI_UNIQUE, op, "There %s %d player%s online (%d in login).", ip + il > 1 ? "are" : "is", ip + il, ip + il > 1 ? "s" : "", il);

	return 1;
}

/**
 * Map info command.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
int command_mapinfo(object *op, char *params)
{
	(void) params;

	current_map_info(op);
	return 1;
}

/**
 * Time command. Print out game time information.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
int command_time(object *op, char *params)
{
	(void) params;

	time_info(op);
	return 1;
}

/**
 * Highscore command, shows the highscore.
 * @param op Object requesting this.
 * @param params Parameters.
 * @return 1. */
int command_hiscore(object *op, char *params)
{
	hiscore_display(op, 25, params);
	return 1;
}

/**
 * Output version information.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_version(object *op, char *params)
{
	(void) params;

	version(op);

	return 1;
}

/**
 * Pray command, used to start praying to your god.
 * @param op Object requesting this
 * @param params Command parameters
 * @return Always returns 1 */
int command_praying(object *op, char *params)
{
	(void) params;

	CONTR(op)->praying = 1;
	return 1;
}

/**
 * Scans input and returns if this is ON value (1) or OFF (0).
 * @param line The input string.
 * @return 1 for ON, 0 for OFF. */
int onoff_value(char *line)
{
	int i;

	if (sscanf(line, "%d", &i))
	{
		return (i != 0);
	}

	switch (line[0])
	{
		case 'o':
			switch (line[1])
			{
				/* on */
				case 'n':
					return 1;

				/* o[ff] */
				default:
					return 0;
			}

		/* y[es] */
		case 'y':
		/* k[ylla] */
		case 'k':
		case 's':
		case 'd':
			return 1;

		/* n[o] */
		case 'n':
		/* e[i] */
		case 'e':
		case 'u':
		default:
			return 0;
	}
}

/**
 * Receive a player name, and force the first letter to be uppercase.
 * @param op Object. */
void receive_player_name(object *op)
{
	adjust_player_name(CONTR(op)->write_buf + 1);

	if (!check_name(CONTR(op), CONTR(op)->write_buf + 1))
	{
		get_name(op);
		return;
	}

	FREE_AND_COPY_HASH(op->name, CONTR(op)->write_buf + 1);

	get_password(op);
}

/**
 * Receive player password.
 * @param op Object. */
void receive_player_password(object *op)
{
	unsigned int pwd_len = strlen(CONTR(op)->write_buf);

	if (pwd_len <= 1 || pwd_len > 17)
	{
		get_name(op);
		return;
	}

	if (CONTR(op)->state == ST_CONFIRM_PASSWORD)
	{
		char cmd_buf[] = "X";

		if (!check_password(CONTR(op)->write_buf + 1, CONTR(op)->password))
		{
			send_socket_message(NDI_RED, &CONTR(op)->socket, "The passwords did not match.");
			get_name(op);
			return;
		}

		esrv_new_player(CONTR(op), 0);
		Write_String_To_Socket(&CONTR(op)->socket, BINARY_CMD_NEW_CHAR, cmd_buf, 1);
		LOG(llevInfo, "NewChar send for %s\n", op->name);
		CONTR(op)->state = ST_ROLL_STAT;

		return;
	}

	strcpy(CONTR(op)->password, crypt_string(CONTR(op)->write_buf + 1, NULL));
	CONTR(op)->state = ST_ROLL_STAT;
	check_login(op);
	return;
}

/**
 * Save command.
 *
 * Cannot be used on unholy ground or if you don't have any experience.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
int command_save(object *op, char *params)
{
	(void) params;

	if (MAP_PLAYER_NO_SAVE(op->map))
	{
		new_draw_info(NDI_UNIQUE, op, "You cannot save here.");
	}
	else if (save_player(op, 1))
	{
		new_draw_info(NDI_UNIQUE, op, "You have been saved.");
	}
	else
	{
		new_draw_info(NDI_UNIQUE, op, "SAVE FAILED!");
	}

	return 1;
}

/**
 * Away from keyboard command.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 1. */
int command_afk(object *op, char *params)
{
	(void) params;

	if (CONTR(op)->afk)
	{
		CONTR(op)->afk = 0;
		new_draw_info(NDI_UNIQUE, op, "You are no longer AFK.");
	}
	else
	{
		CONTR(op)->afk = 1;
		new_draw_info(NDI_UNIQUE, op, "You are now AFK.");
	}

	CONTR(op)->socket.ext_title_flag = 1;

	return 1;
}

/**
 * Command shortcut to /party say.
 * @param op Object requesting this.
 * @param params Command paramaters (the message).
 * @return 1 on success, 0 on failure. */
int command_gsay(object *op, char *params)
{
	char party_params[MAX_BUF];

	params = cleanup_chat_string(params);

	if (!params || *params == '\0')
	{
		return 0;
	}

	strcpy(party_params, "say ");
	strcat(party_params, params);
	command_party(op, party_params);
	return 0;
}

/**
 * Party command, handling things like /party help, /party say, /party
 * leave, etc.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return 1. */
int command_party(object *op, char *params)
{
	char buf[MAX_BUF];

	if (!params)
	{
		if (!CONTR(op)->party)
		{
			new_draw_info(NDI_UNIQUE, op, "You are not a member of any party.");
			new_draw_info(NDI_UNIQUE, op, "For help try: /party help");
		}
		else
		{
			new_draw_info_format(NDI_UNIQUE, op, "You are a member of party %s.", CONTR(op)->party->name);
		}

		return 1;
	}

	if (!strcmp(params, "help"))
	{
		new_draw_info(NDI_UNIQUE, op, "To form a party type: /party form <partyname>");
		new_draw_info(NDI_UNIQUE, op, "To join a party type: /party join <partyname>");
		new_draw_info(NDI_UNIQUE, op, "If the party has a password, it will prompt you for it.");
		new_draw_info(NDI_UNIQUE, op, "For a list of current parties type: /party list");
		new_draw_info(NDI_UNIQUE, op, "To leave a party type: /party leave");
		new_draw_info(NDI_UNIQUE, op, "To change a password for a party type: /party password <password>");
		new_draw_info(NDI_UNIQUE, op, "There is a 8 character max for password.");
		new_draw_info(NDI_UNIQUE, op, "To talk to party members type: /party say <msg> or /gsay <msg>");
		new_draw_info(NDI_UNIQUE, op, "To see who is in your party: /party who");
		new_draw_info(NDI_UNIQUE, op, "To change the party's looting mode: /party loot mode");
		new_draw_info(NDI_UNIQUE, op, "To kick another player from your party: /party kick <name>");
		return 1;
	}
	else if (!strncmp(params, "say ", 4))
	{
		if (!CONTR(op)->party)
		{
			new_draw_info(NDI_UNIQUE, op, "You are not a member of any party.");
			return 1;
		}

		params += 4;
		params = cleanup_chat_string(params);

		if (!params || *params == '\0')
		{
			return 1;
		}

		snprintf(buf, sizeof(buf), "[%s] %s says: %s", CONTR(op)->party->name, op->name, params);
		send_party_message(CONTR(op)->party, buf, PARTY_MESSAGE_CHAT, NULL);
		LOG(llevInfo, "CLOG PARTY: %s [%s] >%s<\n", query_name(op, NULL), CONTR(op)->party->name, params);
		return 1;
	}
	else if (!strcmp(params, "leave"))
	{
		if (!CONTR(op)->party)
		{
			new_draw_info(NDI_UNIQUE, op, "You are not a member of any party.");
			return 1;
		}

		new_draw_info_format(NDI_UNIQUE, op, "You leave party %s.", CONTR(op)->party->name);
		snprintf(buf, sizeof(buf), "%s leaves party %s.", op->name, CONTR(op)->party->name);
		send_party_message(CONTR(op)->party, buf, PARTY_MESSAGE_STATUS, op);

		remove_party_member(CONTR(op)->party, op);
		return 1;
	}
	else if (!strncmp(params, "password ", 9))
	{
		if (!CONTR(op)->party)
		{
			new_draw_info(NDI_UNIQUE | NDI_RED, op, "You are not a member of any party.");
			return 1;
		}

		strncpy(CONTR(op)->party->passwd, params + 9, sizeof(CONTR(op)->party->passwd) - 1);
		snprintf(buf, sizeof(buf), "The password for party %s changed to '%s'.", CONTR(op)->party->name, CONTR(op)->party->passwd);
		send_party_message(CONTR(op)->party, buf, PARTY_MESSAGE_STATUS, NULL);
		return 1;
	}
	else if (!strncmp(params, "form ", 5))
	{
		params = cleanup_chat_string(params + 5);

		if (!params || *params == '\0')
		{
			new_draw_info(NDI_UNIQUE | NDI_RED, op, "Invalid party name to form.");
			return 1;
		}

		if (CONTR(op)->party)
		{
			new_draw_info(NDI_UNIQUE | NDI_RED, op, "You must leave your current party before forming a new one.");
			return 1;
		}

		if (find_party(params))
		{
			new_draw_info_format(NDI_UNIQUE, op, "The party %s already exists, pick another name.", params);
			return 1;
		}

		form_party(op, params);
		return 1;
	}
	else if (!strncmp(params, "loot", 4))
	{
		size_t i;

		params += 4;

		if (!CONTR(op)->party)
		{
			new_draw_info(NDI_UNIQUE | NDI_RED, op, "You are not a member of any party.");
			return 1;
		}

		if (!params || !*params || !++params)
		{
			new_draw_info_format(NDI_UNIQUE, op, "Current looting mode: ~%s~.", party_loot_modes[CONTR(op)->party->loot]);
			return 1;
		}

		if (CONTR(op)->party->leader != op->name)
		{
			new_draw_info(NDI_UNIQUE | NDI_RED, op, "Only the party's leader can change the looting mode.");
			return 1;
		}

		for (i = 0; i < PARTY_LOOT_MAX; i++)
		{
			if (!strcmp(params, party_loot_modes[i]))
			{
				CONTR(op)->party->loot = i;
				snprintf(buf, sizeof(buf), "Party looting mode changed to '%s'.", party_loot_modes[i]);
				send_party_message(CONTR(op)->party, buf, PARTY_MESSAGE_STATUS, NULL);
				return 1;
			}
		}

		new_draw_info(NDI_UNIQUE, op, "Invalid looting mode. Valid modes are:");

		for (i = 0; i < PARTY_LOOT_MAX; i++)
		{
			new_draw_info_format(NDI_UNIQUE, op, "~%s~: %s.", party_loot_modes[i], party_loot_modes_help[i]);
		}

		return 1;
	}
	else if (!strncmp(params, "kick", 4))
	{
		objectlink *ol;

		if (!CONTR(op)->party)
		{
			new_draw_info(NDI_UNIQUE | NDI_RED, op, "You are not a member of any party.");
			return 1;
		}

		if (CONTR(op)->party->leader != op->name)
		{
			new_draw_info(NDI_UNIQUE | NDI_RED, op, "Only the party's leader can kick other members of the party.");
			return 1;
		}

		params = cleanup_chat_string(params + 4);

		if (!params || *params == '\0')
		{
			new_draw_info(NDI_UNIQUE, op, "Whom do you want to kick from the party?");
			return 1;
		}

		if (!strncasecmp(op->name, params, MAX_NAME))
		{
			new_draw_info(NDI_UNIQUE | NDI_RED, op, "You cannot kick yourself.");
			return 1;
		}

		for (ol = CONTR(op)->party->members; ol; ol = ol->next)
		{
			if (!strncasecmp(ol->objlink.ob->name, params, MAX_NAME))
			{
				remove_party_member(CONTR(op)->party, ol->objlink.ob);
				snprintf(buf, sizeof(buf), "%s has been kicked from the party.", ol->objlink.ob->name);
				send_party_message(CONTR(op)->party, buf, PARTY_MESSAGE_STATUS, NULL);
				new_draw_info_format(NDI_UNIQUE | NDI_RED, ol->objlink.ob, "You have been kicked from the party '%s'.", CONTR(op)->party->name);
				return 1;
			}
		}

		new_draw_info(NDI_UNIQUE | NDI_RED, op, "There's no player with that name in your party.");
		return 1;
	}
	else
	{
		party_struct *party;
		SockList sl;
		unsigned char sock_buf[MAXSOCKBUF];

		sl.buf = sock_buf;
		SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_PARTY);

		if (!strcmp(params, "list"))
		{
			SockList_AddChar(&sl, CMD_PARTY_LIST);

			for (party = first_party; party; party = party->next)
			{
				SockList_AddString(&sl, (char *) party->name);
				SockList_AddString(&sl, (char *) party->leader);
			}
		}
		else if (!strcmp(params, "who"))
		{
			objectlink *ol;

			if (!CONTR(op)->party)
			{
				new_draw_info(NDI_UNIQUE | NDI_RED, op, "You are not a member of any party.");
				return 1;
			}

			SockList_AddChar(&sl, CMD_PARTY_WHO);

			for (ol = CONTR(op)->party->members; ol; ol = ol->next)
			{
				SockList_AddString(&sl, (char *) ol->objlink.ob->name);
				SockList_AddString(&sl, ol->objlink.ob->map->name);
				SockList_AddChar(&sl, (char) ol->objlink.ob->level);
			}
		}
		else if (!strncmp(params, "join ", 5))
		{
			char *party_name, *party_password;

			if (CONTR(op)->party)
			{
				new_draw_info(NDI_UNIQUE, op, "You must leave your current party before joining another.");
				return 1;
			}

			params += 5;

			if (!params)
			{
				return 1;
			}

			party_name = strtok(params, "\t");
			party_password = strtok(NULL, "\t");

			party = find_party(party_name);

			if (!party)
			{
				new_draw_info(NDI_UNIQUE, op, "No such party.");
				return 1;
			}

			if (CONTR(op)->party != party)
			{
				/* If party password is not set or they've typed correct password... */
				if (party->passwd[0] == '\0' || (party_password && !strcmp(party->passwd, party_password)))
				{
					add_party_member(party, op);
					new_draw_info_format(NDI_UNIQUE | NDI_GREEN, op, "You have joined party: %s.", party->name);
					snprintf(buf, sizeof(buf), "%s joined party %s.", op->name, party->name);
					send_party_message(party, buf, PARTY_MESSAGE_STATUS, op);
					return 1;
				}
				/* Party password was typed but it wasn't correct. */
				else if (party_password)
				{
					new_draw_info(NDI_UNIQUE | NDI_RED, op, "Incorrect party password.");
					return 1;
				}
				/* Otherwise ask them to type the password */
				else
				{
					new_draw_info(NDI_UNIQUE | NDI_YELLOW, op, "That party requires a password. Type it now, or press ESC to cancel joining.");
					SockList_AddChar(&sl, CMD_PARTY_PASSWORD);
					SockList_AddString(&sl, (char *) party->name);
				}
			}
		}

		if (sl.len > 1)
		{
			Send_With_Handling(&CONTR(op)->socket, &sl);
		}
	}

	return 1;
}

/**
 * The '/whereami' command will display some information about the region
 * the player is located in.
 * @param op Player.
 * @param params Parameters.
 * @return 1. */
int command_whereami(object *op, char *params)
{
	if (!op->map->region)
	{
		new_draw_info(NDI_UNIQUE, op, "You appear to be lost somewhere...");
		return 1;
	}

	(void) params;

	new_draw_info_format(NDI_UNIQUE, op, "You are in %s.\n%s", get_region_longname(op->map->region), get_region_msg(op->map->region));
	return 1;
}

/**
 * Toggle metaserver privacy on/off.
 * @param op Player.
 * @param params Parameters.
 * @return 1. */
int command_ms_privacy(object *op, char *params)
{
	if (CONTR(op)->ms_privacy)
	{
		CONTR(op)->ms_privacy = 0;
		new_draw_info(NDI_UNIQUE, op, "Metaserver privacy turned off.");
	}
	else
	{
		CONTR(op)->ms_privacy = 1;
		new_draw_info(NDI_UNIQUE, op, "Metaserver privacy turned on.");
	}

	(void) params;
	return 1;
}

/**
 * Show some statistics to the player.
 * @param op Player.
 * @param params Parameters.
 * @return 1. */
int command_statistics(object *op, char *params)
{
	size_t i;

	(void) params;

	new_draw_info_format(NDI_UNIQUE, op, "Experience: %s", format_number_comma(op->stats.exp));

	if (op->level < MAXLEVEL)
	{
		char *cp = strdup_local(format_number_comma(level_exp(op->level + 1, 1.0)));

		new_draw_info_format(NDI_UNIQUE, op, "Next Level:  %s (%s)", cp, format_number_comma(level_exp(op->level + 1, 1.0) - op->stats.exp));
		free(cp);
	}

	new_draw_info(NDI_UNIQUE, op, "\nStat: Natural (Real)");

	for (i = 0; i < NUM_STATS; i++)
	{
		new_draw_info_format(NDI_UNIQUE, op, "~%s:~ %d (%d)", short_stat_name[i], get_attr_value(&CONTR(op)->orig_stats, i), get_attr_value(&op->stats, i));
	}

	new_draw_info_format(NDI_UNIQUE, op, "\nYour equipped item power is %d out of %d.", CONTR(op)->item_power, op->level);

	return 1;
}
