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
 * Handles misc. input request - things like hash table, malloc, maps,
 * who, etc. */

#include <global.h>

/**
 * Show maps information.
 * @param op The object to show the information to. */
void maps_info(object *op)
{
	mapstruct *m;
	char map_path[MAX_BUF];
	long sec = seconds();

	draw_info_format(COLOR_WHITE, op, "Current time is: %02ld:%02ld:%02ld.", (sec % 86400) / 3600, (sec % 3600) / 60, sec % 60);
	draw_info(COLOR_WHITE, op, "Path               Pl PlM IM   TO Dif Reset");

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

		draw_info_format(COLOR_WHITE, op, "%-18.18s %2d   %c %4d %2d  %02d:%02d:%02d", map_path, players_on_map(m), m->in_memory ? (m->in_memory == MAP_IN_MEMORY ? 'm' : 's') : 'X', m->timeout, m->difficulty, (MAP_WHEN_RESET(m) % 86400) / 3600, (MAP_WHEN_RESET(m) % 3600) / 60, MAP_WHEN_RESET(m) % 60);
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
 * Give out some info about the map op is located at.
 * @param op The object requesting this information. */
static void current_map_info(object *op)
{
	mapstruct *m = op->map;
	MapSpace *msp;

	if (!m)
	{
		return;
	}

	msp = GET_MAP_SPACE_PTR(m, op->x, op->y);
	draw_info_format(COLOR_WHITE, op, "%s (%s, %s, x: %d, y: %d)", msp->map_info && OBJECT_VALID(msp->map_info, msp->map_info_count) && msp->map_info->race ? msp->map_info->race : m->name, msp->map_info && OBJECT_VALID(msp->map_info, msp->map_info_count) && msp->map_info->slaying ? msp->map_info->slaying : (m->bg_music ? m->bg_music : "no_music"), m->path, op->x, op->y);

	if (QUERY_FLAG(op, FLAG_WIZ))
	{
		draw_info_format(COLOR_WHITE, op, "Players: %d difficulty: %d size: %dx%d start: %dx%d", players_on_map(m), MAP_DIFFICULTY(m), MAP_WIDTH(m), MAP_HEIGHT(m), MAP_ENTER_X(m), MAP_ENTER_Y(m));
	}

	if (m->msg)
	{
		draw_info(COLOR_WHITE, op, m->msg);
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

	draw_info(COLOR_WHITE, op, " ");

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

			draw_info(COLOR_WHITE, op, buf);
		}
	}

	draw_info_format(COLOR_WHITE, op, "There %s %d player%s online (%d in login).", ip + il > 1 ? "are" : "is", ip + il, ip + il > 1 ? "s" : "", il);

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
	int results = 0;

	if (params)
	{
		results = atoi(params);

		/* If it was a number, don't bother using params to search in /hiscore. */
		if (results != 0)
		{
			params = NULL;
		}
		else if (strlen(params) < PLAYER_NAME_MIN)
		{
			draw_info_format(COLOR_WHITE, op, "Your search term must be at least %d characters long.", PLAYER_NAME_MIN);
			return 1;
		}
	}

	/* Add some limits. */
	if (results <= 0)
	{
		results = 25;
	}
	else if (results > 50)
	{
		results = 50;
	}

	hiscore_display(op, results, params);
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
	unsigned int pwd_len = strlen(CONTR(op)->write_buf + 1);

	if (pwd_len < PLAYER_PASSWORD_MIN || pwd_len > PLAYER_PASSWORD_MAX)
	{
		send_socket_message(COLOR_RED, &CONTR(op)->socket, "That password has an invalid length.");
		get_name(op);
		return;
	}

	if (CONTR(op)->state == ST_CONFIRM_PASSWORD)
	{
		char cmd_buf[] = "X";

		if (!check_password(CONTR(op)->write_buf + 1, CONTR(op)->password))
		{
			send_socket_message(COLOR_RED, &CONTR(op)->socket, "The passwords did not match.");
			get_name(op);
			return;
		}

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
		draw_info(COLOR_WHITE, op, "You cannot save here.");
	}
	else if (save_player(op, 1))
	{
		draw_info(COLOR_WHITE, op, "You have been saved.");
	}
	else
	{
		draw_info(COLOR_WHITE, op, "SAVE FAILED!");
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
		draw_info(COLOR_WHITE, op, "You are no longer AFK.");
	}
	else
	{
		CONTR(op)->afk = 1;
		CONTR(op)->stat_afk_used++;
		draw_info(COLOR_WHITE, op, "You are now AFK.");
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
			draw_info(COLOR_WHITE, op, "You are not a member of any party.");
			draw_info(COLOR_WHITE, op, "For help try: /party help");
		}
		else
		{
			draw_info_format(COLOR_WHITE, op, "You are a member of party %s (leader: %s).", CONTR(op)->party->name, CONTR(op)->party->leader);
		}

		return 1;
	}

	if (!strcmp(params, "help"))
	{
		draw_info(COLOR_WHITE, op, "To form a party type: /party form <partyname>");
		draw_info(COLOR_WHITE, op, "To join a party type: /party join <partyname>");
		draw_info(COLOR_WHITE, op, "If the party has a password, it will prompt you for it.");
		draw_info(COLOR_WHITE, op, "For a list of current parties type: /party list");
		draw_info(COLOR_WHITE, op, "To leave a party type: /party leave");
		draw_info(COLOR_WHITE, op, "To change a password for a party type: /party password <password>");
		draw_info(COLOR_WHITE, op, "There is a 8 character max for password.");
		draw_info(COLOR_WHITE, op, "To talk to party members type: /party say <msg> or /gsay <msg>");
		draw_info(COLOR_WHITE, op, "To see who is in your party: /party who");
		draw_info(COLOR_WHITE, op, "To change the party's looting mode: /party loot mode");
		draw_info(COLOR_WHITE, op, "To kick another player from your party: /party kick <name>");
		draw_info(COLOR_WHITE, op, "To change party leader: /party leader <name>");
		return 1;
	}
	else if (!strncmp(params, "say ", 4))
	{
		if (!CONTR(op)->party)
		{
			draw_info(COLOR_WHITE, op, "You are not a member of any party.");
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
		LOG(llevChat, "Party: %s [%s]: %s\n", op->name, CONTR(op)->party->name, params);
		return 1;
	}
	else if (!strcmp(params, "leave"))
	{
		if (!CONTR(op)->party)
		{
			draw_info(COLOR_WHITE, op, "You are not a member of any party.");
			return 1;
		}

		draw_info_format(COLOR_WHITE, op, "You leave party %s.", CONTR(op)->party->name);
		snprintf(buf, sizeof(buf), "%s leaves party %s.", op->name, CONTR(op)->party->name);
		send_party_message(CONTR(op)->party, buf, PARTY_MESSAGE_STATUS, op);

		remove_party_member(CONTR(op)->party, op);
		return 1;
	}
	else if (!strncmp(params, "password ", 9))
	{
		if (!CONTR(op)->party)
		{
			draw_info(COLOR_RED, op, "You are not a member of any party.");
			return 1;
		}

		if (CONTR(op)->party->leader != op->name)
		{
			draw_info(COLOR_RED, op, "Only the party's leader can change the password.");
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
			draw_info(COLOR_RED, op, "Invalid party name to form.");
			return 1;
		}

		if (CONTR(op)->party)
		{
			draw_info(COLOR_RED, op, "You must leave your current party before forming a new one.");
			return 1;
		}

		if (find_party(params))
		{
			draw_info_format(COLOR_WHITE, op, "The party %s already exists, pick another name.", params);
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
			draw_info(COLOR_RED, op, "You are not a member of any party.");
			return 1;
		}

		if (!params || !*params || !++params)
		{
			draw_info_format(COLOR_WHITE, op, "Current looting mode: <green>%s</green>.", party_loot_modes[CONTR(op)->party->loot]);
			return 1;
		}

		if (CONTR(op)->party->leader != op->name)
		{
			draw_info(COLOR_RED, op, "Only the party's leader can change the looting mode.");
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

		draw_info(COLOR_WHITE, op, "Invalid looting mode. Valid modes are:");

		for (i = 0; i < PARTY_LOOT_MAX; i++)
		{
			draw_info_format(COLOR_WHITE, op, "<green>%s</green>: %s.", party_loot_modes[i], party_loot_modes_help[i]);
		}

		return 1;
	}
	else if (!strncmp(params, "kick", 4))
	{
		objectlink *ol;

		if (!CONTR(op)->party)
		{
			draw_info(COLOR_RED, op, "You are not a member of any party.");
			return 1;
		}

		if (CONTR(op)->party->leader != op->name)
		{
			draw_info(COLOR_RED, op, "Only the party's leader can kick other members of the party.");
			return 1;
		}

		params = cleanup_chat_string(params + 4);

		if (!params || *params == '\0')
		{
			draw_info(COLOR_WHITE, op, "Whom do you want to kick from the party?");
			return 1;
		}

		if (!strncasecmp(op->name, params, MAX_NAME))
		{
			draw_info(COLOR_RED, op, "You cannot kick yourself.");
			return 1;
		}

		for (ol = CONTR(op)->party->members; ol; ol = ol->next)
		{
			if (!strncasecmp(ol->objlink.ob->name, params, MAX_NAME))
			{
				remove_party_member(CONTR(op)->party, ol->objlink.ob);
				snprintf(buf, sizeof(buf), "%s has been kicked from the party.", ol->objlink.ob->name);
				send_party_message(CONTR(op)->party, buf, PARTY_MESSAGE_STATUS, NULL);
				draw_info_format(COLOR_RED, ol->objlink.ob, "You have been kicked from the party '%s'.", CONTR(op)->party->name);
				return 1;
			}
		}

		draw_info(COLOR_RED, op, "There's no player with that name in your party.");
		return 1;
	}
	else if (!strncmp(params, "leader ", 7))
	{
		player *pl;

		if (!CONTR(op)->party)
		{
			draw_info(COLOR_RED, op, "You are not a member of any party.");
			return 1;
		}

		if (CONTR(op)->party->leader != op->name)
		{
			draw_info(COLOR_RED, op, "Only the party's leader can change the leader.");
			return 1;
		}

		pl = find_player(params + 7);

		if (!pl)
		{
			draw_info(COLOR_RED, op, "No such player.");
			return 1;
		}

		if (pl->ob == op)
		{
			draw_info(COLOR_RED, op, "You are already the party leader.");
			return 1;
		}

		if (!pl->party || pl->party != CONTR(op)->party)
		{
			draw_info(COLOR_RED, op, "That player is not a member of your party.");
			return 1;
		}

		FREE_AND_ADD_REF_HASH(pl->party->leader, pl->ob->name);
		draw_info_format(COLOR_WHITE, pl->ob, "You are the new leader of party %s!", pl->party->name);
		draw_info_format(COLOR_GREEN, op, "%s is the new leader of your party.", pl->ob->name);
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
				SockList_AddString(&sl, party->name);
				SockList_AddString(&sl, party->leader);
			}
		}
		else if (!strcmp(params, "who"))
		{
			objectlink *ol;

			if (!CONTR(op)->party)
			{
				draw_info(COLOR_RED, op, "You are not a member of any party.");
				return 1;
			}

			SockList_AddChar(&sl, CMD_PARTY_WHO);

			for (ol = CONTR(op)->party->members; ol; ol = ol->next)
			{
				SockList_AddString(&sl, ol->objlink.ob->name);
				SockList_AddChar(&sl, MAX(1, MIN((double) ol->objlink.ob->stats.hp / ol->objlink.ob->stats.maxhp * 100.0f, 100)));
				SockList_AddChar(&sl, MAX(1, MIN((double) ol->objlink.ob->stats.sp / ol->objlink.ob->stats.maxsp * 100.0f, 100)));
				SockList_AddChar(&sl, MAX(1, MIN((double) ol->objlink.ob->stats.grace / ol->objlink.ob->stats.maxgrace * 100.0f, 100)));
			}
		}
		else if (!strncmp(params, "join ", 5))
		{
			char *party_name, *party_password;

			if (CONTR(op)->party)
			{
				draw_info(COLOR_WHITE, op, "You must leave your current party before joining another.");
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
				draw_info(COLOR_WHITE, op, "No such party.");
				return 1;
			}

			if (CONTR(op)->party != party)
			{
				/* If party password is not set or they've typed correct password... */
				if (party->passwd[0] == '\0' || (party_password && !strcmp(party->passwd, party_password)))
				{
					add_party_member(party, op);
					CONTR(op)->stat_joined_party++;
					draw_info_format(COLOR_GREEN, op, "You have joined party: %s.", party->name);
					snprintf(buf, sizeof(buf), "%s joined party %s.", op->name, party->name);
					send_party_message(party, buf, PARTY_MESSAGE_STATUS, op);
					return 1;
				}
				/* Party password was typed but it wasn't correct. */
				else if (party_password)
				{
					draw_info(COLOR_RED, op, "Incorrect party password.");
					return 1;
				}
				/* Otherwise ask them to type the password */
				else
				{
					draw_info(COLOR_YELLOW, op, "That party requires a password. Type it now, or press ESC to cancel joining.");
					SockList_AddChar(&sl, CMD_PARTY_PASSWORD);
					SockList_AddString(&sl, party->name);
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
		draw_info(COLOR_WHITE, op, "You appear to be lost somewhere...");
		return 1;
	}

	(void) params;

	draw_info_format(COLOR_WHITE, op, "You are in %s.\n%s", get_region_longname(op->map->region), get_region_msg(op->map->region));
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
		draw_info(COLOR_WHITE, op, "Metaserver privacy turned off.");
	}
	else
	{
		CONTR(op)->ms_privacy = 1;
		draw_info(COLOR_WHITE, op, "Metaserver privacy turned on.");
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

	draw_info_format(COLOR_WHITE, op, "Experience: %s", format_number_comma(op->stats.exp));

	if (op->level < MAXLEVEL)
	{
		char *cp = strdup_local(format_number_comma(level_exp(op->level + 1, 1.0)));

		draw_info_format(COLOR_WHITE, op, "Next Level:  %s (%s)", cp, format_number_comma(level_exp(op->level + 1, 1.0) - op->stats.exp));
		free(cp);
	}

	draw_info(COLOR_WHITE, op, "\nStat: Natural (Real)");

	for (i = 0; i < NUM_STATS; i++)
	{
		draw_info_format(COLOR_WHITE, op, "<green>%s:</green> %d (%d)", short_stat_name[i], get_attr_value(&CONTR(op)->orig_stats, i), get_attr_value(&op->stats, i));
	}

	draw_info_format(COLOR_WHITE, op, "\nYour equipped item power is %d out of %d.", CONTR(op)->item_power, op->level);

	return 1;
}

/**
 * The /region_map command.
 * @param op Player.
 * @param params Optional region ID that will be shown as the region map
 * instead of the player's map region. Only possible if the player is a
 * DM or has region_map command permission.
 * @return 1. */
int command_region_map(object *op, char *params)
{
	region *r;
	SockList sl;
	uint8 sock_buf[HUGE_BUF], params_check;

	if (!op->map)
	{
		return 1;
	}

	/* Server has not configured client maps URL. */
	if (settings.client_maps_url[0] == '\0')
	{
		draw_info(COLOR_WHITE, op, "This server does not support that command.");
		return 1;
	}

	/* Check if params were given and whether the player is allowed to
	 * see map of any region they want. */
	params_check = params && can_do_wiz_command(CONTR(op), "region_map");

	if (params_check)
	{
		/* Search for the region the player wants. */
		for (r = first_region; r; r = r->next)
		{
			if (!strcasecmp(r->name, params))
			{
				break;
			}
		}

		/* Not found, try partial region names. */
		if (!r)
		{
			size_t params_len = strlen(params);

			for (r = first_region; r; r = r->next)
			{
				if (!strncasecmp(r->name, params, params_len))
				{
					break;
				}
			}
		}

		if (!r)
		{
			draw_info(COLOR_WHITE, op, "No such region.");
			return 1;
		}
	}
	else
	{
		r = op->map->region;
	}

	/* Try to find a region that should have had a client map
	 * generated. */
	for (; r; r = r->parent)
	{
		if (r->map_first)
		{
			break;
		}
	}

	if (!r && params_check)
	{
		draw_info(COLOR_WHITE, op, "That region doesn't have a map.");
		return 1;
	}

	if (!r || (r->map_quest && !params_check && !player_has_region_map(CONTR(op), r)))
	{
		draw_info(COLOR_WHITE, op, "You don't have a map of this region.");
		return 1;
	}

	sl.buf = sock_buf;
	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_REGION_MAP);
	SockList_AddString(&sl, op->map->path);
	SockList_AddShort(&sl, op->x);
	SockList_AddShort(&sl, op->y);
	SockList_AddString(&sl, r->name);
	SockList_AddString(&sl, settings.client_maps_url);

	if (CONTR(op)->socket.socket_version >= 1058)
	{
		SockList_AddString(&sl, r->longname);
		SockList_AddChar(&sl, op->direction);
	}

	Send_With_Handling(&CONTR(op)->socket, &sl);

	return 1;
}
