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
 * This file implements all of the goo on the server side for handling
 * clients.  It's got a bunch of global variables for keeping track of
 * each of the clients.
 *
 * @note All functions that are used to process data from the client
 * have the prototype of (char *data, int datalen, int client_num).  This
 * way, we can use one dispatch table. */

#include <global.h>

#define GET_CLIENT_FLAGS(_O_)	((_O_)->flags[0] & 0x7f)
#define NO_FACE_SEND (-1)

/**
 * Parse server file command from client (amf, hpf, etc).
 * @param param Parameter for the command.
 * @param cmdback Buffer that will be sent back to the client.
 * @param type ID of the server file. */
static void parse_srv_setup(char *param, char *cmdback, int type)
{
	char *cp;
	size_t x = 0;
	unsigned long y = 0;

	/* is x our files len and y the crc */
	for (cp = param; *cp != '\0'; cp++)
	{
		if (*cp == '|')
		{
			*cp = '\0';
			x = atoi(param);
			y = strtoul(cp + 1, NULL, 16);
			break;
		}
	}

	if (SrvClientFiles[type].len_ucomp != x || SrvClientFiles[type].crc != y)
	{
		char tmpbuf[MAX_BUF];

		snprintf(tmpbuf, sizeof(tmpbuf), "%"FMT64U"|%lx", (uint64) SrvClientFiles[type].len_ucomp, SrvClientFiles[type].crc);
		strcat(cmdback, tmpbuf);
	}
	else
	{
		strcat(cmdback, "OK");
	}
}

/**
 * The Setup command.
 *
 * The setup syntax is:
 * <pre>setup \<cmdname1\> \<parameter\> \<cmdname2\> \<parameter\>...</pre>
 *
 * We send the status of the command back, or zero if the command is
 * unknown. The client must then sort it out. */
void SetUp(char *buf, int len, socket_struct *ns)
{
	int s;
	char *cmd, *param, tmpbuf[MAX_BUF], cmdback[HUGE_BUF];

	if (!buf || !len)
	{
		return;
	}

	LOG(llevInfo, "Get SetupCmd:: %s\n", buf);
	cmdback[0] = BINARY_CMD_SETUP;
	cmdback[1] = 0;

	for (s = 0; s < len; )
	{
		cmd = &buf[s];

		/* Find the next space, and put a null there */
		for (; s < len && buf[s] && buf[s] != ' '; s++)
		{
		}

		buf[s++] = 0;

		while (s < len && buf[s] == ' ')
		{
			s++;
		}

		if (s >= len)
		{
			break;
		}

		param = &buf[s];

		for (;s < len && buf[s] && buf[s] != ' '; s++)
		{
		}

		buf[s++] = 0;

		while (s < len && buf[s] == ' ')
		{
			s++;
		}

		strcat(cmdback, " ");
		strcat(cmdback, cmd);
		strcat(cmdback, " ");

		if (!strcmp(cmd, "sound"))
		{
			ns->sound = atoi(param);
			strcat(cmdback, param);
		}
		else if (!strcmp(cmd, "faceset"))
		{
			int q = atoi(param);

			if (is_valid_faceset(q))
			{
				ns->faceset = q;
			}

			snprintf(tmpbuf, sizeof(tmpbuf), "%d", ns->faceset);
			strcat(cmdback, tmpbuf);
		}
		else if (!strcmp(cmd, "mapsize"))
		{
			int x, y = 0;
			char *cp;

			x = atoi(param);

			for (cp = param; *cp != 0; cp++)
			{
				if (*cp == 'x' || *cp == 'X')
				{
					y = atoi(cp + 1);

					break;
				}
			}

			if (x < 9 || y < 9 || x > MAP_CLIENT_X || y > MAP_CLIENT_Y)
			{
				snprintf(tmpbuf, sizeof(tmpbuf), " %dx%d", MAP_CLIENT_X, MAP_CLIENT_Y);
				strcat(cmdback, tmpbuf);
			}
			else
			{
				ns->mapx = x;
				ns->mapy = y;
				ns->mapx_2 = x / 2;
				ns->mapy_2 = y / 2;

				/* better to send back what we are really using and not
				 * the param as given to us in case it gets parsed
				 * differently. */
				snprintf(tmpbuf, sizeof(tmpbuf), "%dx%d", x, y);
				strcat(cmdback, tmpbuf);
			}
		}
		else if (!strcmp(cmd, "skf"))
		{
			parse_srv_setup(param, cmdback, SRV_CLIENT_SKILLS);
		}
		else if (!strcmp(cmd, "spf"))
		{
			parse_srv_setup(param, cmdback, SRV_CLIENT_SPELLS);
		}
		else if (!strcmp(cmd, "spfv2"))
		{
			parse_srv_setup(param, cmdback, SRV_FILE_SPELLS_V2);
		}
		else if (!strcmp(cmd, "skfv2"))
		{
			parse_srv_setup(param, cmdback, SRV_CLIENT_SKILLS_V2);
		}
		else if (!strcmp(cmd, "stf"))
		{
			parse_srv_setup(param, cmdback, SRV_CLIENT_SETTINGS);
		}
		else if (!strcmp(cmd, "bpf"))
		{
			parse_srv_setup(param, cmdback, SRV_CLIENT_BMAPS);
		}
		else if (!strcmp(cmd, "amf"))
		{
			parse_srv_setup(param, cmdback, SRV_CLIENT_ANIMS);
		}
		else if (!strcmp(cmd, "amfv2"))
		{
			parse_srv_setup(param, cmdback, SRV_CLIENT_ANIMS_V2);
		}
		else if (!strcmp(cmd, "hpf"))
		{
			parse_srv_setup(param, cmdback, SRV_CLIENT_HFILES);
		}
		else if (!strcmp(cmd, "hpfv2"))
		{
			parse_srv_setup(param, cmdback, SRV_CLIENT_HFILES_V2);
		}
		else if (!strcmp(cmd, "upf"))
		{
			parse_srv_setup(param, cmdback, SRV_FILE_UPDATES);
		}
		else if (!strcmp(cmd, "ssf"))
		{
			parse_srv_setup(param, cmdback, SRV_SERVER_SETTINGS);
		}
		else if (!strcmp(cmd, "eff"))
		{
			parse_srv_setup(param, cmdback, SRV_CLIENT_EFFECTS);
		}
		else if (!strcmp(cmd, "bot"))
		{
			int is_bot = atoi(param);

			if (is_bot != 0 && is_bot != 1)
			{
				strcat(cmdback, "FALSE");
			}
			else
			{
				ns->is_bot = is_bot;
				snprintf(tmpbuf, sizeof(tmpbuf), "%d", is_bot);
				strcat(cmdback, tmpbuf);
			}
		}
		else
		{
			/* Didn't get a setup command we understood - report a
			 * failure to the client. */
			strcat(cmdback, "FALSE");
		}
	}

	Write_String_To_Socket(ns, BINARY_CMD_SETUP, cmdback, strlen(cmdback));
}

/**
 * The client has requested to be added to the game. This is what takes
 * care of it.
 *
 * We tell the client how things worked out. */
void AddMeCmd(char *buf, int len, socket_struct *ns)
{
	Settings oldsettings;
	char cmd_buf[2] = "X";
	oldsettings = settings;

	(void) buf;
	(void) len;

	if (ns->status != Ns_Add || add_player(ns))
	{
		Write_String_To_Socket(ns, BINARY_CMD_ADDME_FAIL, cmd_buf, 1);
		ns->status = Ns_Dead;
	}
	else
	{
		/* Basically, the add_player copies the socket structure into
		 * the player structure, so this one (which is from init_sockets)
		 * is not needed anymore. */
		ns->addme = 1;
		/* Reset idle counter */
		ns->login_count = 0;
		ns->keepalive = 0;
		socket_info.nconns--;
		ns->status = Ns_Avail;
	}

	settings = oldsettings;
}

/**
 * This handles the general commands from client (ie, north, fire, cast,
 * etc).
 * @param buf
 * @param len
 * @param pl */
void PlayerCmd(char *buf, int len, player *pl)
{
	char command[MAX_BUF];

	if (!buf || len < 1)
	{
		return;
	}

	if (len >= MAX_BUF)
	{
		len = MAX_BUF - 1;
	}

	strncpy(command, buf, len);
	command[len] = '\0';

	/* The following should never happen with a proper or honest client.
	 * Therefore, the error message doesn't have to be too clear - if
	 * someone is playing with a hacked/non working client, this gives
	 * them an idea of the problem, but they deserve what they get. */
	if (pl->state != ST_PLAYING)
	{
		draw_info_format(COLOR_WHITE, pl->ob, "You can not issue commands - state is not ST_PLAYING (%s)", buf);
		return;
	}

	execute_newserver_command(pl->ob, command);
}

/**
 * This is a reply to a previous query. */
void ReplyCmd(char *buf, int len, player *pl)
{
	(void) len;

	if (!buf || pl->socket.status == Ns_Dead)
	{
		return;
	}

	strcpy(pl->write_buf, ":");
	strncat(pl->write_buf, buf, 250);
	pl->write_buf[250] = 0;
	pl->socket.ext_title_flag = 1;

	switch (pl->state)
	{
		case ST_PLAYING:
			pl->socket.status = Ns_Dead;
			LOG(llevBug, "Got reply message with ST_PLAYING input state (player %s)\n", query_name(pl->ob, NULL));
			break;

		case ST_GET_NAME:
			receive_player_name(pl->ob);
			break;

		case ST_GET_PASSWORD:
		case ST_CONFIRM_PASSWORD:
			receive_player_password(pl->ob);
			break;

		default:
			pl->socket.status = Ns_Dead;
			LOG(llevBug, "Unknown input state: %d\n", pl->state);
			break;
	}
}

/**
 * Send a version mismatch message.
 * @param ns The socket to send the message to */
static void version_mismatch_msg(socket_struct *ns)
{
	send_socket_message(COLOR_RED, ns, "This is Atrinik Server.\nYour client version is outdated!\nGo to http://www.atrinik.org and download the latest Atrinik client!\nGoodbye.");
}

/**
 * Request a srv file. */
void RequestFileCmd(char *buf, int len, socket_struct *ns)
{
	int id;

	/* *only* allow this command between the first login and the "addme" command! */
	if (ns->status != Ns_Add || !buf || !len)
	{
		ns->status = Ns_Dead;
		return;
	}

	id = atoi(buf);

	if (id < 0 || id >= SRV_CLIENT_FILES)
	{
		ns->status = Ns_Dead;
		return;
	}

	switch (id)
	{
		case SRV_CLIENT_SKILLS:
		case SRV_CLIENT_SKILLS_V2:
			if (ns->rf_skills)
			{
				ns->status = Ns_Dead;
				return;
			}
			else
			{
				ns->rf_skills = 1;
			}

			break;

		case SRV_CLIENT_SPELLS:
		case SRV_FILE_SPELLS_V2:
			if (ns->rf_spells)
			{
				ns->status = Ns_Dead;
				return;
			}
			else
			{
				ns->rf_spells = 1;
			}

			break;

		case SRV_CLIENT_SETTINGS:
		case SRV_SERVER_SETTINGS:
			if (ns->rf_settings)
			{
				ns->status = Ns_Dead;
				return;
			}
			else
			{
				ns->rf_settings = 1;
			}

			break;

		case SRV_CLIENT_BMAPS:
			if (ns->rf_bmaps)
			{
				ns->status = Ns_Dead;
				return;
			}
			else
			{
				ns->rf_bmaps = 1;
			}

			break;

		case SRV_CLIENT_ANIMS:
		case SRV_CLIENT_ANIMS_V2:
			if (ns->rf_anims)
			{
				ns->status = Ns_Dead;
				return;
			}
			else
			{
				ns->rf_anims = 1;
			}

			break;

		case SRV_CLIENT_HFILES:
		case SRV_CLIENT_HFILES_V2:
			if (ns->rf_hfiles)
			{
				ns->status = Ns_Dead;
				return;
			}
			else
			{
				ns->rf_hfiles = 1;
			}

			break;
	}

	LOG(llevDebug, "Client %s rf #%d\n", ns->host, id);
	send_srv_file(ns, id);
}

/**
 * Client tells its its version. */
void VersionCmd(char *buf, int len, socket_struct *ns)
{
	char *cp;

	if (!buf || !len || ns->version)
	{
		version_mismatch_msg(ns);
		ns->status = Ns_Dead;
		return;
	}

	ns->version = 1;
	ns->socket_version = atoi(buf);
	cp = strchr(buf + 1, ' ');

	if (!cp)
	{
		version_mismatch_msg(ns);
		LOG(llevDebug, "VersionCmd(): Connection from false client (invalid name)\n");
		ns->status = Ns_Zombie;
		return;
	}

	if (ns->socket_version == 991017 || ns->socket_version < 1055)
	{
		version_mismatch_msg(ns);
		ns->status = Ns_Zombie;
		return;
	}

	if (ns->socket_version > SOCKET_VERSION)
	{
		send_socket_message(COLOR_RED, ns, "This Atrinik server is outdated and incompatible with your client's version. Try another server.");
		ns->status = Ns_Zombie;
		return;
	}
}

/**
 * Moves an object (typically, container to inventory).
 *
 * Syntax:
 * <pre>move \<to\> \<tag\> \<nrof\></pre>
 * @param buf Data.
 * @param len Length of data.
 * @param pl Player. */
void MoveCmd(char *buf, int len, player *pl)
{
	int vals[3];

	if (!buf || !len)
	{
		return;
	}

	if (sscanf(buf, "%d %d %d", &vals[0], &vals[1], &vals[2]) != 3)
	{
		return;
	}

	esrv_move_object(pl->ob, vals[0], vals[1], vals[2]);
}

/**
 * Asks the client to query the user.
 *
 * This way, the client knows it needs to send something back (vs just
 * printing out a message). */
void send_query(socket_struct *ns, uint8 flags, char *text)
{
	char buf[MAX_BUF];

	snprintf(buf, sizeof(buf), "X%d %s", flags, text ? text : "");

	Write_String_To_Socket(ns, BINARY_CMD_QUERY, buf, strlen(buf));
}

#define AddIfInt(Old, New, Type)                            \
	if (Old != New)                                         \
	{                                                       \
		Old = New;                                          \
		SockList_AddChar(&sl, (char) (Type));               \
		SockList_AddInt(&sl, (uint32) (New));               \
	}

#define AddIfInt64(Old, New, Type)                          \
	if (Old != New)                                         \
	{                                                       \
		Old = New;                                          \
		SockList_AddChar(&sl, Type);                        \
		SockList_AddInt64(&sl, New);                        \
	}

#define AddIfShort(Old, New, Type)                          \
	if (Old != New)                                         \
	{                                                       \
		Old = New;                                          \
		SockList_AddChar(&sl, (char) (Type));               \
		SockList_AddShort(&sl, (uint16) (New));             \
	}

#define AddIfChar(Old, New, Type)                           \
	if (Old != New)                                         \
	{                                                       \
		Old = New;                                          \
		SockList_AddChar(&sl, (char) (Type));               \
		SockList_AddChar(&sl, (char) (New));                \
	}

#define AddIfFloat(Old, New, Type)                          \
	if (Old != New)                                         \
	{                                                       \
		Old = New;                                          \
		SockList_AddChar(&sl, (char) Type);                 \
		SockList_AddInt(&sl, (long) (New * FLOAT_MULTI));   \
	}

#define AddIfString(Old, New, Type)                         \
	if (Old == NULL || strcmp(Old, New))                    \
	{                                                       \
		if (Old)                                            \
		{                                                   \
			free(Old);                                      \
		}                                                   \
                                                            \
		Old = strdup_local(New);                            \
		SockList_AddChar(&sl, (char) Type);                 \
		SockList_AddChar(&sl, (char) strlen(New));          \
		strcpy((char *) sl.buf + sl.len, New);              \
		sl.len += strlen(New);                              \
	}

/**
 * Helper function for send_skilllist_cmd() and esrv_update_skills(), adds
 * one skill to buffer which is then sent to the client as the skill list
 * command.
 * @param skill Skill object to add.
 * @param sb StringBuffer instance to add to. */
void add_skill_to_skilllist(object *skill, StringBuffer *sb)
{
	/* Normal skills */
	if (skill->last_eat == 1)
	{
		stringbuffer_append_printf(sb, "/%s|%d|%"FMT64, skill->name, skill->level, skill->stats.exp);
	}
	/* 'Buy level' skills */
	else if (skill->last_eat == 2)
	{
		stringbuffer_append_printf(sb, "/%s|%d|-2", skill->name, skill->level);
	}
	/* No level skills */
	else
	{
		stringbuffer_append_printf(sb, "/%s|%d|-1", skill->name, skill->level);
	}
}

/**
 * Send the player skills to the client.
 * @param pl Player to send the skills to. */
void esrv_update_skills(player *pl)
{
	int i;
	StringBuffer *sb = stringbuffer_new();
	char *cp;
	size_t cp_len;

	stringbuffer_append_printf(sb, "X%d ", SPLIST_MODE_UPDATE);

	for (i = 0; i < NROFSKILLS; i++)
	{
		if (pl->skill_ptr[i] && pl->skill_ptr[i]->last_eat)
		{
			object *tmp = pl->skill_ptr[i];

			/* Send only when something has changed */
			if (tmp->stats.exp != pl->skill_exp[i] || tmp->level != pl->skill_level[i])
			{
				add_skill_to_skilllist(tmp, sb);
				pl->skill_exp[i] = tmp->stats.exp;
				pl->skill_level[i] = tmp->level;
			}
		}
	}

	cp_len = sb->pos;
	cp = stringbuffer_finish(sb);
	Write_String_To_Socket(&pl->socket, BINARY_CMD_SKILL_LIST, cp, cp_len);
	free(cp);
}

/**
 * Sends player statistics update.
 *
 * We look at the old values, and only send what has changed.
 *
 * Stat mapping values are in newclient.h
 *
 * Since this gets sent a lot, this is actually one of the few binary
 * commands for now. */
void esrv_update_stats(player *pl)
{
	static char sock_buf[HUGE_BUF];
	SockList sl;
	int i;
	uint16 flags;

	sl.buf = (unsigned char *) sock_buf;
	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_STATS);

	/* small trick: we want send the hp bar of our target to the player.
	 * We want send a char with x% the target has of full hp.
	 * To avoid EVERY time the % calculation, we store the real HP
	 * - if it has changed, we calc the % and use them normal.
	 * this simple compare will not deal in speed but we safe
	 * some unneeded calculations. */

	/* never send our own status - client will sort this out */
	if (pl->target_object != pl->ob)
	{
		/* we don't care about count - target function will readjust itself */
		if (pl->target_object && pl->target_object->stats.hp != pl->target_hp)
		{
			char hp = (char) MAX(1, (((float) pl->target_object->stats.hp / (float) pl->target_object->stats.maxhp) * 100.0f));

			pl->target_hp = pl->target_object->stats.hp;
			AddIfChar(pl->target_hp_p, hp, CS_STAT_TARGET_HP);
		}
	}

	AddIfShort(pl->last_gen_hp, pl->gen_client_hp, CS_STAT_REG_HP);
	AddIfShort(pl->last_gen_sp, pl->gen_client_sp, CS_STAT_REG_MANA);
	AddIfShort(pl->last_gen_grace,pl->gen_client_grace, CS_STAT_REG_GRACE);
	AddIfChar(pl->last_level, pl->ob->level, CS_STAT_LEVEL);
	AddIfFloat(pl->last_speed, pl->ob->speed, CS_STAT_SPEED);
	AddIfInt(pl->last_weight_limit, weight_limit[pl->ob->stats.Str], CS_STAT_WEIGHT_LIM);
	AddIfChar(pl->last_weapon_sp, pl->weapon_sp, CS_STAT_WEAP_SP);

	if (pl->ob)
	{
		object *arrow;

		AddIfInt(pl->last_stats.hp, pl->ob->stats.hp, CS_STAT_HP);
		AddIfInt(pl->last_stats.maxhp, pl->ob->stats.maxhp, CS_STAT_MAXHP);
		AddIfShort(pl->last_stats.sp, pl->ob->stats.sp, CS_STAT_SP);
		AddIfShort(pl->last_stats.maxsp, pl->ob->stats.maxsp, CS_STAT_MAXSP);
		AddIfShort(pl->last_stats.grace, pl->ob->stats.grace, CS_STAT_GRACE);
		AddIfShort(pl->last_stats.maxgrace, pl->ob->stats.maxgrace, CS_STAT_MAXGRACE);
		AddIfChar(pl->last_stats.Str, pl->ob->stats.Str, CS_STAT_STR);
		AddIfChar(pl->last_stats.Int, pl->ob->stats.Int, CS_STAT_INT);
		AddIfChar(pl->last_stats.Pow, pl->ob->stats.Pow, CS_STAT_POW);
		AddIfChar(pl->last_stats.Wis, pl->ob->stats.Wis, CS_STAT_WIS);
		AddIfChar(pl->last_stats.Dex, pl->ob->stats.Dex, CS_STAT_DEX);
		AddIfChar(pl->last_stats.Con, pl->ob->stats.Con, CS_STAT_CON);
		AddIfChar(pl->last_stats.Cha, pl->ob->stats.Cha, CS_STAT_CHA);
		AddIfInt64(pl->last_stats.exp, pl->ob->stats.exp, CS_STAT_EXP);
		AddIfShort(pl->last_stats.wc, pl->ob->stats.wc, CS_STAT_WC);
		AddIfShort(pl->last_stats.ac, pl->ob->stats.ac, CS_STAT_AC);
		AddIfShort(pl->last_stats.dam, pl->ob->stats.dam, CS_STAT_DAM);
		AddIfShort(pl->last_stats.food, pl->ob->stats.food, CS_STAT_FOOD);
		AddIfInt(pl->last_action_timer, pl->action_timer, CS_STAT_ACTION_TIME);

		if (pl->equipment[PLAYER_EQUIP_BOW] && (arrow = arrow_find(pl->ob, pl->equipment[PLAYER_EQUIP_BOW]->race)))
		{
			AddIfShort(pl->last_ranged_dam, arrow_get_damage(pl->ob, pl->equipment[PLAYER_EQUIP_BOW], arrow), CS_STAT_RANGED_DAM);
			AddIfShort(pl->last_ranged_wc, arrow_get_wc(pl->ob, pl->equipment[PLAYER_EQUIP_BOW], arrow), CS_STAT_RANGED_WC);
			AddIfInt(pl->last_ranged_ws, bow_get_ws(pl->equipment[PLAYER_EQUIP_BOW], arrow), CS_STAT_RANGED_WS);
		}
		else
		{
			AddIfShort(pl->last_ranged_dam, 0, CS_STAT_RANGED_DAM);
			AddIfShort(pl->last_ranged_wc, 0, CS_STAT_RANGED_WC);
			AddIfInt(pl->last_ranged_ws, 0, CS_STAT_RANGED_WS);
		}
	}

	for (i = 0; i < pl->last_skill_index; i++)
	{
		AddIfInt64(pl->last_skill_exp[i], pl->last_skill_ob[i]->stats.exp, pl->last_skill_id[i]);
		AddIfChar(pl->last_skill_level[i], (pl->last_skill_ob[i]->level), pl->last_skill_id[i] + 1);
	}

	flags = 0;

	if (pl->run_on)
	{
		flags |= SF_RUNON;
	}

	/* we add additional player status flags - in old style, you got a msg
	 * in the text windows when you get xray of get blinded - we will skip
	 * this and add the info here, so the client can track it down and make
	 * it the user visible in it own, server independent way. */

	/* player is blind */
	if (QUERY_FLAG(pl->ob, FLAG_BLIND))
	{
		flags |= SF_BLIND;
	}

	/* player has xray */
	if (QUERY_FLAG(pl->ob, FLAG_XRAYS))
	{
		flags |= SF_XRAYS;
	}

	/* player has infravision */
	if (QUERY_FLAG(pl->ob, FLAG_SEE_IN_DARK))
	{
		flags |= SF_INFRAVISION;
	}

	AddIfShort(pl->last_flags, flags, CS_STAT_FLAGS);

	for (i = 0; i < NROFATTACKS; i++)
	{
		/* If there are more attacks, but we reached CS_STAT_PROT_END,
		 * we stop now. */
		if (CS_STAT_PROT_START + i > CS_STAT_PROT_END)
		{
			break;
		}

		AddIfChar(pl->last_protection[i], pl->ob->protection[i], CS_STAT_PROT_START + i);
	}

	if (pl->socket.ext_title_flag)
	{
		generate_ext_title(pl);
		SockList_AddChar(&sl, (char) CS_STAT_EXT_TITLE);
		i = (int) strlen(pl->ext_title);
		SockList_AddChar(&sl, (char) i);
		strcpy((char *) sl.buf + sl.len, pl->ext_title);
		sl.len += i;
		pl->socket.ext_title_flag = 0;
	}

	/* Only send it away if we have some actual data */
	if (sl.len > 1)
	{
		Send_With_Handling(&pl->socket, &sl);
	}
}

/**
 * Tells the client that here is a player it should start using. */
void esrv_new_player(player *pl, uint32 weight)
{
	SockList sl;

	sl.buf = malloc(MAXSOCKBUF);

	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_PLAYER);
	SockList_AddInt(&sl, pl->ob->count);
	SockList_AddInt(&sl, weight);
	SockList_AddInt(&sl, pl->ob->face->number);

	SockList_AddChar(&sl, (char) strlen(pl->ob->name));
	strcpy((char *) sl.buf + sl.len, pl->ob->name);
	sl.len += strlen(pl->ob->name);

	Send_With_Handling(&pl->socket, &sl);
	free(sl.buf);
}

/**
 * Get ID of a tiled map by checking player::last_update.
 * @param pl Player.
 * @param map Tiled map.
 * @return ID of the tiled map, 0 if there is no match. */
static inline int get_tiled_map_id(player *pl, struct mapdef *map)
{
	int i;

	if (!pl->last_update)
	{
		return 0;
	}

	for (i = 0; i < TILED_NUM; i++)
	{
		if (pl->last_update->tile_path[i] == map->path)
		{
			return i + 1;
		}
	}

	return 0;
}

/**
 * Copy socket's last map according to new coordinates.
 * @param ns Socket.
 * @param dx X.
 * @param dy Y. */
static inline void copy_lastmap(socket_struct *ns, int dx, int dy)
{
	struct Map newmap;
	int x, y;

	for (x = 0; x < ns->mapx; x++)
	{
		for (y = 0; y < ns->mapy; y++)
		{
			if (x + dx < 0 || x + dx >= ns->mapx || y + dy < 0 || y + dy >= ns->mapy)
			{
				memset(&(newmap.cells[x][y]), 0, sizeof(MapCell));
				continue;
			}

			memcpy(&(newmap.cells[x][y]), &(ns->lastmap.cells[x + dx][y + dy]), sizeof(MapCell));
		}
	}

	memcpy(&(ns->lastmap), &newmap, sizeof(struct Map));
}

/**
 * Do some checks, map name and LOS and then draw the map.
 * @param pl Whom to draw map for. */
void draw_client_map(object *pl)
{
	int redraw_below = 0;

	if (pl->type != PLAYER)
	{
		LOG(llevBug, "draw_client_map(): Called with non-player: %s\n", pl->name);
		return;
	}

	/* IF player is just joining the game, he isn't on a map,
	 * If so, don't try to send them a map.  All will
	 * be OK once they really log in. */
	if (!pl->map || pl->map->in_memory != MAP_IN_MEMORY)
	{
		return;
	}

	CONTR(pl)->map_update_cmd = MAP_UPDATE_CMD_SAME;

	/* If we changed somewhere the map, prepare map data */
	if (CONTR(pl)->last_update != pl->map)
	{
		int tile_map = get_tiled_map_id(CONTR(pl), pl->map);

		/* Are we on a new map? */
		if (!CONTR(pl)->last_update || !tile_map)
		{
			CONTR(pl)->map_update_cmd = MAP_UPDATE_CMD_NEW;
			memset(&(CONTR(pl)->socket.lastmap), 0, sizeof(struct Map));
			CONTR(pl)->last_update = pl->map;
			redraw_below = 1;
		}
		else
		{
			CONTR(pl)->map_update_cmd = MAP_UPDATE_CMD_CONNECTED;
			CONTR(pl)->map_update_tile = tile_map;
			redraw_below = 1;

			/* We have moved to a tiled map. Let's calculate the offsets. */
			switch (tile_map - 1)
			{
				case 0:
					CONTR(pl)->map_off_x = pl->x - CONTR(pl)->map_tile_x;
					CONTR(pl)->map_off_y = -(CONTR(pl)->map_tile_y + (MAP_HEIGHT(pl->map) - pl->y));
					break;

				case 1:
					CONTR(pl)->map_off_y = pl->y - CONTR(pl)->map_tile_y;
					CONTR(pl)->map_off_x = (MAP_WIDTH(pl->map) - CONTR(pl)->map_tile_x) + pl->x;
					break;

				case 2:
					CONTR(pl)->map_off_x = pl->x - CONTR(pl)->map_tile_x;
					CONTR(pl)->map_off_y = (MAP_HEIGHT(pl->map) - CONTR(pl)->map_tile_y) + pl->y;
					break;

				case 3:
					CONTR(pl)->map_off_y = pl->y - CONTR(pl)->map_tile_y;
					CONTR(pl)->map_off_x = -(CONTR(pl)->map_tile_x + (MAP_WIDTH(pl->map) - pl->x));
					break;

				case 4:
					CONTR(pl)->map_off_y = -(CONTR(pl)->map_tile_y + (MAP_HEIGHT(pl->map) - pl->y));
					CONTR(pl)->map_off_x = (MAP_WIDTH(pl->map) - CONTR(pl)->map_tile_x) + pl->x;
					break;

				case 5:
					CONTR(pl)->map_off_x = (MAP_WIDTH(pl->map) - CONTR(pl)->map_tile_x) + pl->x;
					CONTR(pl)->map_off_y = (MAP_HEIGHT(pl->map) - CONTR(pl)->map_tile_y) + pl->y;
					break;

				case 6:
					CONTR(pl)->map_off_y = (MAP_HEIGHT(pl->map) - CONTR(pl)->map_tile_y) + pl->y;
					CONTR(pl)->map_off_x = -(CONTR(pl)->map_tile_x + (MAP_WIDTH(pl->map) - pl->x));
					break;

				case 7:
					CONTR(pl)->map_off_x = -(CONTR(pl)->map_tile_x + (MAP_WIDTH(pl->map) - pl->x));
					CONTR(pl)->map_off_y = -(CONTR(pl)->map_tile_y + (MAP_HEIGHT(pl->map) - pl->y));
					break;
			}

			copy_lastmap(&CONTR(pl)->socket, CONTR(pl)->map_off_x, CONTR(pl)->map_off_y);
			CONTR(pl)->last_update = pl->map;
		}
	}
	else
	{
		if (CONTR(pl)->map_tile_x != pl->x || CONTR(pl)->map_tile_y != pl->y)
		{
			copy_lastmap(&CONTR(pl)->socket, pl->x - CONTR(pl)->map_tile_x, pl->y - CONTR(pl)->map_tile_y);
			redraw_below = 1;
		}
	}

	/* Redraw below window and backbuffer new positions? */
	if (redraw_below)
	{
		/* Backbuffer position so we can determine whether we have moved or not */
		CONTR(pl)->map_tile_x = pl->x;
		CONTR(pl)->map_tile_y = pl->y;
		CONTR(pl)->socket.below_clear = 1;
		/* Redraw it */
		CONTR(pl)->socket.update_tile = 0;
		CONTR(pl)->socket.look_position = 0;
	}

	/* Do LOS after calls to update_position */
	if (!QUERY_FLAG(pl, FLAG_WIZ) && CONTR(pl)->update_los)
	{
		update_los(pl);
		CONTR(pl)->update_los = 0;
	}

	draw_client_map2(pl);

	/* If we moved on the same map, check for map name/music to update. */
	if (redraw_below && CONTR(pl)->map_update_cmd == MAP_UPDATE_CMD_SAME)
	{
		SockList sl;
		unsigned char sock_buf[MAXSOCKBUF];
		MapSpace *msp;

		sl.buf = sock_buf;
		SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_MAPSTATS);
		msp = GET_MAP_SPACE_PTR(pl->map, pl->x, pl->y);

		/* Is there a map info object on this square? */
		if (msp->map_info && OBJECT_VALID(msp->map_info, msp->map_info_count))
		{
			/* Check if there is map info name, but only update if it hasn't changed. */
			if (msp->map_info->race && strcmp(msp->map_info->race, CONTR(pl)->map_info_name))
			{
				SockList_AddChar(&sl, CMD_MAPSTATS_NAME);
				SockList_AddMapName(&sl, pl, pl->map, msp->map_info);

				strncpy(CONTR(pl)->map_info_name, msp->map_info->race, sizeof(CONTR(pl)->map_info_name) - 1);
				CONTR(pl)->map_info_name[sizeof(CONTR(pl)->map_info_name) - 1] = '\0';
			}

			/* Likewise for map info music. */
			if (msp->map_info->slaying && strcmp(msp->map_info->slaying, CONTR(pl)->map_info_music))
			{
				SockList_AddChar(&sl, CMD_MAPSTATS_MUSIC);
				SockList_AddMapMusic(&sl, pl, pl->map, msp->map_info);

				strncpy(CONTR(pl)->map_info_music, msp->map_info->slaying, sizeof(CONTR(pl)->map_info_music) - 1);
				CONTR(pl)->map_info_music[sizeof(CONTR(pl)->map_info_music) - 1] = '\0';
			}

			/* And weather... */
			if (msp->map_info->title && strcmp(msp->map_info->title, CONTR(pl)->map_info_weather))
			{
				SockList_AddChar(&sl, CMD_MAPSTATS_WEATHER);
				SockList_AddMapWeather(&sl, pl, pl->map, msp->map_info);

				strncpy(CONTR(pl)->map_info_weather, msp->map_info->title, sizeof(CONTR(pl)->map_info_weather) - 1);
				CONTR(pl)->map_info_weather[sizeof(CONTR(pl)->map_info_weather) - 1] = '\0';
			}
		}
		/* There isn't map info object, check if we need to update previously
		 * overriden values. */
		else
		{
			/* Update map name... */
			if (CONTR(pl)->map_info_name[0] != '\0')
			{
				SockList_AddChar(&sl, CMD_MAPSTATS_NAME);
				SockList_AddMapName(&sl, pl, pl->map, NULL);

				CONTR(pl)->map_info_name[0] = '\0';
			}

			/* Update map music... */
			if (CONTR(pl)->map_info_music[0] != '\0')
			{
				SockList_AddChar(&sl, CMD_MAPSTATS_MUSIC);
				SockList_AddMapMusic(&sl, pl, pl->map, NULL);

				CONTR(pl)->map_info_music[0] = '\0';
			}

			/* Update map weather... */
			if (CONTR(pl)->map_info_weather[0] != '\0')
			{
				SockList_AddChar(&sl, CMD_MAPSTATS_WEATHER);
				SockList_AddMapWeather(&sl, pl, pl->map, NULL);

				CONTR(pl)->map_info_weather[0] = '\0';
			}
		}

		/* Anything to send? */
		if (sl.len > 1)
		{
			Send_With_Handling(&CONTR(pl)->socket, &sl);
		}
	}
}

/**
 * Figure out player name color for the client to show, in HTML notation.
 *
 * As you can see in this function, it is easy to add new player name
 * colors, just check for the match and make it return the correct color.
 * @param pl Player object that will get the map data sent to.
 * @param op Player object on the map, to get the name from.
 * @return The color. */
static const char *get_playername_color(object *pl, object *op)
{
	if (CONTR(pl)->party != NULL && CONTR(op)->party != NULL && CONTR(pl)->party == CONTR(op)->party)
	{
		return COLOR_GREEN;
	}
	else if (pl != op && pvp_area(pl, op))
	{
		return COLOR_RED;
	}

	return COLOR_WHITE;
}

/** Darkness table */
static int darkness_table[] = {0, 10, 30, 60, 120, 260, 480, 960};

/** Clear a map cell. */
#define map_clearcell(_cell_) \
{ \
	memset((void *) ((char *) (_cell_) + offsetof(MapCell, count)), 0, sizeof(MapCell) - offsetof(MapCell, count)); \
	(_cell_)->count = -1; \
}

/** Clear a map cell, but only if it has not been cleared before. */
#define map_if_clearcell() \
{ \
	if (CONTR(pl)->socket.lastmap.cells[ax][ay].count != -1) \
	{ \
		SockList_AddShort(&sl, mask | MAP2_MASK_CLEAR); \
		map_clearcell(&CONTR(pl)->socket.lastmap.cells[ax][ay]); \
	} \
}

/** Draw the client map. */
void draw_client_map2(object *pl)
{
	static uint32 map2_count = 0;
	MapCell *mp;
	MapSpace *msp;
	mapstruct *m;
	int x, y, ax, ay, d, nx, ny;
	int x_start, dm_light = 0;
	int special_vision;
	uint16 mask;
	SockList sl, sl_layer, sl_sound;
	unsigned char sock_buf[MAXSOCKBUF], sock_buf_layer[MAXSOCKBUF], sock_buf_sound[MAXSOCKBUF];
	int wdark;
	int layer, dark;
	int anim_value, anim_type, ext_flags;
	int num_layers;
	int oldlen, outdoor;
	object *mirror = NULL;
	uint8 have_sound_ambient;

	/* Do we have dm_light? */
	if (CONTR(pl)->dm_light)
	{
		dm_light = global_darkness_table[MAX_DARKNESS];
	}

	wdark = darkness_table[world_darkness];
	/* Any kind of special vision? */
	special_vision = (QUERY_FLAG(pl, FLAG_XRAYS) ? 1 : 0) | (QUERY_FLAG(pl, FLAG_SEE_IN_DARK) ? 2 : 0);
	map2_count++;

	sl.buf = sock_buf;
	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_MAP2);

	sl_sound.buf = sock_buf_sound;
	SOCKET_SET_BINARY_CMD(&sl_sound, BINARY_CMD_SOUND_AMBIENT);

	/* Marker */
	SockList_AddChar(&sl, (char) CONTR(pl)->map_update_cmd);

	if (CONTR(pl)->map_update_cmd != MAP_UPDATE_CMD_SAME)
	{
		msp = GET_MAP_SPACE_PTR(pl->map, pl->x, pl->y);

		SockList_AddMapName(&sl, pl, pl->map, msp->map_info);
		SockList_AddMapMusic(&sl, pl, pl->map, msp->map_info);
		SockList_AddMapWeather(&sl, pl, pl->map, msp->map_info);

		if (msp->map_info && OBJECT_VALID(msp->map_info, msp->map_info_count))
		{
			if (msp->map_info->race)
			{
				strncpy(CONTR(pl)->map_info_name, msp->map_info->race, sizeof(CONTR(pl)->map_info_name) - 1);
				CONTR(pl)->map_info_name[sizeof(CONTR(pl)->map_info_name) - 1] = '\0';
			}

			if (msp->map_info->slaying)
			{
				strncpy(CONTR(pl)->map_info_music, msp->map_info->slaying, sizeof(CONTR(pl)->map_info_music) - 1);
				CONTR(pl)->map_info_music[sizeof(CONTR(pl)->map_info_music) - 1] = '\0';
			}

			if (msp->map_info->title)
			{
				strncpy(CONTR(pl)->map_info_weather, msp->map_info->title, sizeof(CONTR(pl)->map_info_weather) - 1);
				CONTR(pl)->map_info_weather[sizeof(CONTR(pl)->map_info_weather) - 1] = '\0';
			}
		}

		if (CONTR(pl)->map_update_cmd == MAP_UPDATE_CMD_CONNECTED)
		{
			SockList_AddChar(&sl, (char) CONTR(pl)->map_update_tile);
			SockList_AddChar(&sl, (char) CONTR(pl)->map_off_x);
			SockList_AddChar(&sl, (char) CONTR(pl)->map_off_y);
		}
		else
		{
			SockList_AddChar(&sl, (char) pl->map->width);
			SockList_AddChar(&sl, (char) pl->map->height);
		}
	}

	SockList_AddChar(&sl, (char) pl->x);
	SockList_AddChar(&sl, (char) pl->y);

	x_start = (pl->x + (CONTR(pl)->socket.mapx + 1) / 2) - 1;

	for (ay = CONTR(pl)->socket.mapy - 1, y = (pl->y + (CONTR(pl)->socket.mapy + 1) / 2) - 1; y >= pl->y - CONTR(pl)->socket.mapy_2; y--, ay--)
	{
		ax = CONTR(pl)->socket.mapx - 1;

		for (x = x_start; x >= pl->x - CONTR(pl)->socket.mapx_2; x--, ax--)
		{
			d = CONTR(pl)->blocked_los[ax][ay];
			/* Form the data packet for x and y positions. */
			mask = (ax & 0x1f) << 11 | (ay & 0x1f) << 6;
			mp = &(CONTR(pl)->socket.lastmap.cells[ax][ay]);

			/* Space is out of map or blocked. Update space and clear values if needed. */
			if (d & BLOCKED_LOS_OUT_OF_MAP)
			{
				map_if_clearcell();
				continue;
			}

			nx = x;
			ny = y;

			if (!(m = get_map_from_coord(pl->map, &nx, &ny)))
			{
				map_if_clearcell();
				continue;
			}

			msp = GET_MAP_SPACE_PTR(m, nx, ny);
			/* Check whether there is ambient sound effect on this tile. */
			have_sound_ambient = msp->sound_ambient && OBJECT_VALID(msp->sound_ambient, msp->sound_ambient_count);

			/* If there is an ambient sound effect but it cannot be heard
			 * through walls due to its configuration, we will pretend
			 * there is no sound effect here. */
			if (have_sound_ambient && !QUERY_FLAG(msp->sound_ambient, FLAG_XRAYS) && d & BLOCKED_LOS_BLOCKED)
			{
				have_sound_ambient = 0;
			}

			/* If there is an ambient sound effect and we haven't sent it
			 * before, or there isn't one but it was sent before, send an
			 * update. */
			if ((have_sound_ambient && mp->sound_ambient_count != msp->sound_ambient->count) || (!have_sound_ambient && mp->sound_ambient_count))
			{
				SockList_AddChar(&sl_sound, ax);
				SockList_AddChar(&sl_sound, ay);
				SockList_AddInt(&sl_sound, mp->sound_ambient_count);

				if (have_sound_ambient)
				{
					SockList_AddInt(&sl_sound, msp->sound_ambient->count);
					SockList_AddString(&sl_sound, msp->sound_ambient->race);
					SockList_AddChar(&sl_sound, msp->sound_ambient->item_condition);

					if (CONTR(pl)->socket.socket_version >= 1057)
					{
						SockList_AddChar(&sl_sound, msp->sound_ambient->item_level);
					}

					mp->sound_ambient_count = msp->sound_ambient->count;
				}
				else
				{
					SockList_AddInt(&sl_sound, 0);

					mp->sound_ambient_count = 0;
				}
			}

			if (d & BLOCKED_LOS_BLOCKED)
			{
				map_if_clearcell();
				continue;
			}

			/* Border tile, we can ignore every LOS change */
			if (!(d & BLOCKED_LOS_IGNORE))
			{
				/* Tile has blocksview set? */
				if (msp->flags & P_BLOCKSVIEW)
				{
					if (!d)
					{
						CONTR(pl)->update_los = 1;
					}
				}
				else
				{
					if (d & BLOCKED_LOS_BLOCKSVIEW)
					{
						CONTR(pl)->update_los = 1;
					}
				}
			}

			outdoor = MAP_OUTDOORS(m) || (msp->map_info && OBJECT_VALID(msp->map_info, msp->map_info_count) && msp->map_info->item_power == -2);

			/* Calculate the darkness/light value for this tile. */
			if (((outdoor && !(GET_MAP_FLAGS(m, nx, ny) & P_OUTDOOR)) || (!outdoor && GET_MAP_FLAGS(m, nx, ny) & P_OUTDOOR)) && (!msp->map_info || !OBJECT_VALID(msp->map_info, msp->map_info_count) || msp->map_info->item_power < 0))
			{
				d = msp->light_value + wdark + dm_light;
			}
			else
			{
				/* Check if map info object bound to this tile has a darkness. */
				if (msp->map_info && OBJECT_VALID(msp->map_info, msp->map_info_count) && msp->map_info->item_power != -1)
				{
					int dark_value = msp->map_info->item_power;

					if (dark_value < 0 || dark_value > MAX_DARKNESS)
					{
						dark_value = MAX_DARKNESS;
					}

					d = global_darkness_table[dark_value] + msp->light_value + dm_light;
				}
				else
				{
					d = m->light_value + msp->light_value + dm_light;
				}
			}

			if (GET_MAP_FLAGS(m, nx, ny) & P_MAGIC_MIRROR)
			{
				object *mirror_tmp;
				magic_mirror_struct *m_data;
				mapstruct *mirror_map;

				/* Try to find the magic mirror, but only search on layer 0. */
				for (mirror_tmp = GET_MAP_OB(m, nx, ny); mirror_tmp && mirror_tmp->layer == LAYER_SYS; mirror_tmp = mirror_tmp->above)
				{
					if (mirror_tmp->type == MAGIC_MIRROR)
					{
						mirror = mirror_tmp;
						break;
					}
				}

				m_data = MMIRROR(mirror);

				if (m_data && (mirror_map = magic_mirror_get_map(mirror)) && !OUT_OF_MAP(mirror_map, m_data->x, m_data->y))
				{
					MapSpace *mirror_msp = GET_MAP_SPACE_PTR(mirror_map, m_data->x, m_data->y);

					if ((MAP_OUTDOORS(mirror_map) && !(GET_MAP_FLAGS(mirror_map, m_data->x, m_data->y) & P_OUTDOOR)) || (!MAP_OUTDOORS(mirror_map) && GET_MAP_FLAGS(mirror_map, m_data->x, m_data->y) & P_OUTDOOR))
					{
						d = mirror_msp->light_value + wdark + dm_light;
					}
					else
					{
						d = mirror_map->light_value + mirror_msp->light_value + dm_light;
					}
				}
			}

			/* Tile is not normally visible */
			if (d <= 0)
			{
				/* Xray or infravision? */
				if (special_vision & 1 || (special_vision & 2 && msp->flags & (P_IS_PLAYER | P_IS_MONSTER)))
				{
					d = 100;
				}
				else
				{
					map_if_clearcell();
					continue;
				}
			}

			if (d > 640)
			{
				d = 210;
			}
			else if (d > 320)
			{
				d = 180;
			}
			else if (d > 160)
			{
				d = 150;
			}
			else if (d > 80)
			{
				d = 120;
			}
			else if (d > 40)
			{
				d = 90;
			}
			else if (d > 20)
			{
				d = 60;
			}
			else
			{
				d = 30;
			}

			/* Initialize default values for some variables. */
			dark = NO_FACE_SEND;
			ext_flags = 0;
			oldlen = sl.len;
			anim_type = 0;
			anim_value = 0;

			/* Do we need to send the darkness? */
			if (mp->count != d)
			{
				mask |= MAP2_MASK_DARKNESS;
				dark = d;
				mp->count = d;
			}

			/* Add the mask. Any mask changes should go above this line. */
			SockList_AddShort(&sl, mask);

			/* If we have darkness to send, send it. */
			if (dark != NO_FACE_SEND)
			{
				SockList_AddChar(&sl, (char) dark);
			}

			/* We will use a temporary SockList instance to add any layers we find.
			 * If we don't find any, there is no reason to send the data about them
			 * to the client. */
			sl_layer.buf = sock_buf_layer;
			sl_layer.len = 0;
			num_layers = 0;

			/* Go through the visible layers. */
			for (layer = LAYER_FLOOR; layer <= NUM_LAYERS; layer++)
			{
				int sub_layer, socket_layer;

				for (sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++)
				{
					object *tmp = GET_MAP_SPACE_LAYER(msp, layer, sub_layer);

					/* Double check that we can actually see this object. */
					if (tmp && QUERY_FLAG(tmp, FLAG_HIDDEN))
					{
						tmp = NULL;
					}

					/* This is done so that the player image is always shown
					* to the player, even if they are standing on top of another
					* player or monster. */
					if (tmp && tmp->layer == LAYER_LIVING && pl->x == nx && pl->y == ny)
					{
						tmp = pl;
					}

					/* Still nothing, but there's a magic mirror on this tile? */
					if (!tmp && mirror)
					{
						magic_mirror_struct *m_data = MMIRROR(mirror);
						mapstruct *mirror_map;

						if (m_data && (mirror_map = magic_mirror_get_map(mirror)) && !OUT_OF_MAP(mirror_map, m_data->x, m_data->y))
						{
							tmp = GET_MAP_SPACE_LAYER(GET_MAP_SPACE_PTR(mirror_map, m_data->x, m_data->y), layer, sub_layer);
						}
					}

					/* Handle objects that are shown based on their direction
					* and the player's position. */
					if (tmp && QUERY_FLAG(tmp, FLAG_DRAW_DIRECTION))
					{
						/* If the object is dir [0124568] and not in the top
						* or right quadrant or on the central square, do not
						* show it. */
						if ((!tmp->direction || tmp->direction == NORTH || tmp->direction == NORTHEAST || tmp->direction == SOUTHEAST || tmp->direction == SOUTH || tmp->direction == SOUTHWEST || tmp->direction == NORTHWEST) && !((ax <= CONTR(pl)->socket.mapx_2) && (ay <= CONTR(pl)->socket.mapy_2)) && !((ax > CONTR(pl)->socket.mapx_2) && (ay < CONTR(pl)->socket.mapy_2)))
						{
							tmp = NULL;
						}
						/* If the object is dir [0234768] and not in the top
						* or left quadrant or on the central square, do not
						* show it. */
						else if ((!tmp->direction || tmp->direction == NORTHEAST || tmp->direction == EAST || tmp->direction == SOUTHEAST || tmp->direction == SOUTHWEST || tmp->direction == WEST || tmp->direction == NORTHWEST) && !((ax <= CONTR(pl)->socket.mapx_2) && (ay <= CONTR(pl)->socket.mapy_2)) && !((ax < CONTR(pl)->socket.mapx_2) && (ay > CONTR(pl)->socket.mapy_2)))
						{
							tmp = NULL;
						}
					}

					socket_layer = NUM_LAYERS * sub_layer + layer - 1;

					/* Found something. */
					if (tmp)
					{
						sint16 face;
						uint8 quick_pos = tmp->quick_pos;
						uint8 flags = 0, probe_val = 0;
						uint32 flags2 = 0;
						object *head = tmp->head ? tmp->head : tmp;

						/* If we have a multi-arch object. */
						if (quick_pos)
						{
							flags |= MAP2_FLAG_MULTI;

							/* Tail? */
							if (tmp->head)
							{
								/* If true, we have sent a part of this in this map
								 * update before, so skip it. */
								if (head->update_tag == map2_count)
								{
									face = 0;
								}
								else
								{
									/* Mark this object as sent. */
									head->update_tag = map2_count;
									face = head->face->number;
								}
							}
							/* Head. */
							else
							{
								if (tmp->update_tag == map2_count)
								{
									face = 0;
								}
								else
								{
									tmp->update_tag = map2_count;
									face = tmp->face->number;
								}
							}
						}
						else
						{
							face = tmp->face->number;
						}

						/* Player? So we want to send their name. */
						if (tmp->type == PLAYER)
						{
							flags |= MAP2_FLAG_NAME;
						}

						/* If our player has this object as their target, we want to
						 * know its HP percent. */
						if (head->count == CONTR(pl)->target_object_count)
						{
							flags |= MAP2_FLAG_PROBE;
							probe_val = MAX(1, ((double) head->stats.hp / ((double) head->stats.maxhp / 100.0)));
						}

						/* Z position set? */
						if (head->z)
						{
							flags |= MAP2_FLAG_HEIGHT;
						}

						/* Check if the object has zoom, or check if the magic mirror
						 * should affect the zoom value of this layer. */
						if ((head->zoom_x && head->zoom_x != 100) || (head->zoom_y && head->zoom_y != 100) || (mirror && mirror->last_heal && mirror->last_heal != 100 && mirror->path_attuned & (1U << (layer - 1))))
						{
							flags |= MAP2_FLAG_ZOOM;
						}

						if (head->align || (mirror && mirror->align))
						{
							flags |= MAP2_FLAG_ALIGN;
						}

						/* Draw the object twice if set, but only if it's not
						 * in the bottom quadrant of the map. */
						if ((QUERY_FLAG(tmp, FLAG_DRAW_DOUBLE) && (ax < CONTR(pl)->socket.mapx_2 || ay < CONTR(pl)->socket.mapy_2)) || QUERY_FLAG(tmp, FLAG_DRAW_DOUBLE_ALWAYS))
						{
							flags |= MAP2_FLAG_DOUBLE;
						}

						if (head->alpha)
						{
							flags2 |= MAP2_FLAG2_ALPHA;
						}

						if (head->rotate)
						{
							flags2 |= MAP2_FLAG2_ROTATE;
						}

						if (QUERY_FLAG(pl, FLAG_SEE_IN_DARK) && ((head->layer == LAYER_LIVING && d < 150) || (head->type == CONTAINER && (head->sub_type & 1) == ST1_CONTAINER_CORPSE && QUERY_FLAG(head, FLAG_IS_USED_UP) && (float) head->stats.food / head->last_eat >= CORPSE_INFRAVISION_PERCENT / 100.0)))
						{
							flags2 |= MAP2_FLAG2_INFRAVISION;
						}

						if (flags2)
						{
							flags |= MAP2_FLAG_MORE;
						}

						/* Damage animation? Store it for later. */
						if (tmp->last_damage && tmp->damage_round_tag == global_round_tag)
						{
							ext_flags |= MAP2_FLAG_EXT_ANIM;
							anim_type = ANIM_DAMAGE;
							anim_value = tmp->last_damage;
						}

						/* Now, check if we have cached this. */
						if (mp->faces[socket_layer] == face && mp->quick_pos[socket_layer] == quick_pos && mp->flags[socket_layer] == flags && mp->probe == probe_val)
						{
							continue;
						}

						/* Different from cache, add it to the cache now. */
						mp->faces[socket_layer] = face;
						mp->quick_pos[socket_layer] = quick_pos;
						mp->flags[socket_layer] = flags;

						if (layer == LAYER_LIVING)
						{
							mp->probe = probe_val;
						}

						if (OBJECT_IS_HIDDEN(pl, head))
						{
							/* Update target if applicable. */
							if (flags & MAP2_FLAG_PROBE)
							{
								CONTR(pl)->target_object = NULL;
								CONTR(pl)->target_object_count = 0;
								send_target_command(CONTR(pl));
							}

							if (mp->faces[socket_layer])
							{
								SockList_AddChar(&sl_layer, MAP2_LAYER_CLEAR);
								SockList_AddChar(&sl_layer, (char) socket_layer);
								num_layers++;
							}

							continue;
						}

						num_layers++;

						/* Add its layer. */
						SockList_AddChar(&sl_layer, (char) socket_layer);
						/* The face. */
						SockList_AddShort(&sl_layer, face);
						/* Get the first several flags of this object (like paralyzed,
						* sleeping, etc). */
						SockList_AddChar(&sl_layer, (char) GET_CLIENT_FLAGS(head));
						/* Flags we figured out above. */
						SockList_AddChar(&sl_layer, flags);

						/* Multi-arch? Add it's quick pos. */
						if (flags & MAP2_FLAG_MULTI)
						{
							SockList_AddChar(&sl_layer, (char) quick_pos);
						}

						/* Player name? Add the player's name, and their player name color. */
						if (flags & MAP2_FLAG_NAME)
						{
							SockList_AddString(&sl_layer, CONTR(tmp)->quick_name);
							SockList_AddString(&sl_layer, get_playername_color(pl, tmp));
						}

						/* Target's HP bar. */
						if (flags & MAP2_FLAG_PROBE)
						{
							SockList_AddChar(&sl_layer, (char) probe_val);
						}

						/* Z position. */
						if (flags & MAP2_FLAG_HEIGHT)
						{
							if (mirror && mirror->last_eat)
							{
								SockList_AddShort(&sl_layer, head->z + mirror->last_eat);
							}
							else
							{
								SockList_AddShort(&sl_layer, head->z);
							}
						}

						if (flags & MAP2_FLAG_ZOOM)
						{
							/* First check mirror, even if the object *does* have custom zoom. */
							if (mirror && mirror->last_heal)
							{
								SockList_AddShort(&sl_layer, mirror->last_heal);
								SockList_AddShort(&sl_layer, mirror->last_heal);
							}
							else
							{
								SockList_AddShort(&sl_layer, head->zoom_x);
								SockList_AddShort(&sl_layer, head->zoom_y);
							}
						}

						if (flags & MAP2_FLAG_ALIGN)
						{
							if (mirror && mirror->align)
							{
								SockList_AddShort(&sl_layer, head->align + mirror->align);
							}
							else
							{
								SockList_AddShort(&sl_layer, head->align);
							}
						}

						if (flags & MAP2_FLAG_MORE)
						{
							SockList_AddInt(&sl_layer, flags2);

							if (flags2 & MAP2_FLAG2_ALPHA)
							{
								SockList_AddChar(&sl_layer, head->alpha);
							}

							if (flags2 & MAP2_FLAG2_ROTATE)
							{
								SockList_AddShort(&sl_layer, head->rotate);
							}
						}
					}
					/* Didn't find anything. Now, if we have previously seen a face
					 * on this layer, we will want the client to clear it. */
					else if (mp->faces[socket_layer])
					{
						mp->faces[socket_layer] = 0;
						mp->quick_pos[socket_layer] = 0;
						SockList_AddChar(&sl_layer, MAP2_LAYER_CLEAR);
						SockList_AddChar(&sl_layer, (char) socket_layer);
						num_layers++;
					}
				}
			}

			SockList_AddChar(&sl, (char) num_layers);

			/* Do we have any data about layers? If so, copy it to the main SockList instance. */
			if (sl_layer.len)
			{
				memcpy(sl.buf + sl.len, sl_layer.buf, sl_layer.len);
				sl.len += sl_layer.len;
			}

			/* Kill animation? */
			if (GET_MAP_RTAG(m, nx, ny) == global_round_tag)
			{
				ext_flags |= MAP2_FLAG_EXT_ANIM;
				anim_type = ANIM_KILL;
				anim_value = GET_MAP_DAMAGE(m, nx, ny);
			}

			/* Add flags for this tile. */
			SockList_AddChar(&sl, (char) ext_flags);

			/* Animation? Add its type and value. */
			if (ext_flags & MAP2_FLAG_EXT_ANIM)
			{
				SockList_AddChar(&sl, (char) anim_type);
				SockList_AddShort(&sl, (sint16) anim_value);
			}

			/* If nothing has really changed, go back to old SockList length. */
			if (!(mask & 0x3f) && !num_layers && !ext_flags)
			{
				sl.len = oldlen;
			}

			/* Set 'mirror' back to NULL, so we'll try to re-find it on another tile. */
			mirror = NULL;
		}
	}

	/* Verify that we in fact do need to send this. */
	if (sl.len > 4)
	{
		Send_With_Handling(&CONTR(pl)->socket, &sl);
	}

	if (sl_sound.len > 1 && CONTR(pl)->socket.socket_version >= 1056)
	{
		Send_With_Handling(&CONTR(pl)->socket, &sl_sound);
	}
}

/**
 * Handles shop commands received from the player's client.
 * @param buf The buffer
 * @param len Length of the buffer
 * @param pl The player */
void ShopCmd(char *buf, int len, player *pl)
{
	(void) buf;
	(void) len;
	(void) pl;
}

/**
 * Client has requested quest list of a player.
 * @param data Data.
 * @param len Length of the data.
 * @param pl The player. */
void QuestListCmd(char *data, int len, player *pl)
{
	object *quest_container = pl->quest_container, *tmp;
	StringBuffer *sb = stringbuffer_new();
	char *cp;
	size_t cp_len;

	(void) data;
	(void) len;

	if (!quest_container || !quest_container->inv)
	{
		stringbuffer_append_string(sb, "qlist <title>No quests to speak of.</title>");

		cp_len = sb->pos;
		cp = stringbuffer_finish(sb);
		Write_String_To_Socket(&pl->socket, BINARY_CMD_QLIST, cp, cp_len);
		free(cp);
		return;
	}

	stringbuffer_append_string(sb, "qlist <book=Quest List><title>Incomplete quests:</title>\n");

	/* First show incomplete quests */
	for (tmp = quest_container->inv; tmp; tmp = tmp->below)
	{
		if (tmp->type != QUEST_CONTAINER || tmp->magic == QUEST_STATUS_COMPLETED)
		{
			continue;
		}

		stringbuffer_append_printf(sb, "\n<title>%s</title>\n%s%s", tmp->name, tmp->msg ? tmp->msg : "", tmp->msg ? "\n" : "");

		if (tmp->sub_type == QUEST_TYPE_MULTI)
		{
			object *tmp2, *last;

			/* Find the last entry. */
			for (last = tmp->inv; last && last->below; last = last->below)
			{
			}

			/* Show the quest parts. */
			for (tmp2 = last; tmp2; tmp2 = tmp2->above)
			{
				if (tmp2->msg)
				{
					stringbuffer_append_printf(sb, "\n- %s", tmp2->msg);

					if (tmp2->magic == QUEST_STATUS_COMPLETED)
					{
						stringbuffer_append_string(sb, " [done]");
					}
				}

				switch (tmp2->sub_type)
				{
					case QUEST_TYPE_KILL:
						stringbuffer_append_printf(sb, "\n<x=10>Status: %d/%d", MIN(tmp2->last_sp, tmp2->last_grace), tmp2->last_grace);
						break;
				}
			}

			stringbuffer_append_string(sb, "\n");
		}
		else
		{
			switch (tmp->sub_type)
			{
				case QUEST_TYPE_KILL:
					stringbuffer_append_printf(sb, "Status: %d/%d\n", MIN(tmp->last_sp, tmp->last_grace), tmp->last_grace);
					break;
			}
		}
	}

	stringbuffer_append_string(sb, "<p>\n<title>Completed quests:</title>\n");

	/* Now show completed quests */
	for (tmp = quest_container->inv; tmp; tmp = tmp->below)
	{
		if (tmp->type != QUEST_CONTAINER || tmp->magic != QUEST_STATUS_COMPLETED)
		{
			continue;
		}

		stringbuffer_append_printf(sb, "\n<title>%s</title>\n%s%s", tmp->name, tmp->msg ? tmp->msg : "", tmp->msg ? "\n" : "");
	}

	cp_len = sb->pos;
	cp = stringbuffer_finish(sb);
	Write_String_To_Socket(&pl->socket, BINARY_CMD_QLIST, cp, cp_len);
	free(cp);
}

/**
 * Clears the commands cache.
 * @param buf Data.
 * @param len Length.
 * @param ns Socket. */
void command_clear_cmds(char *buf, int len, socket_struct *ns)
{
	(void) buf;
	(void) len;

	ns->cmdbuf.len = 0;
	ns->cmdbuf.buf[0] = '\0';
}

/**
 * Sound related functions. */
void SetSound(char *buf, int len, socket_struct *ns)
{
	if (!buf || !len)
	{
		return;
	}

	ns->sound = atoi(buf);
}

/**
 * Player wants to move to a specified x,y (of the map view).
 * @param buf Data.
 * @param len Length of 'buf'.
 * @param pl Player. */
void command_move_path(uint8 *buf, int len, player *pl)
{
	sint8 x, y;
	mapstruct *m;
	int xt, yt;
	path_node *node, *tmp;

	if (!buf || len < 2)
	{
		return;
	}

	x = buf[0];
	y = buf[1];

	/* Validate the passed x/y. */
	if (x < 0 || x >= pl->socket.mapx || y < 0 || y >= pl->socket.mapy)
	{
		return;
	}

	/* If this is the middle of the screen where the player is already,
	 * there isn't much to do. */
	if (x == pl->socket.mapx_2 && y == pl->socket.mapy_2)
	{
		return;
	}

	/* The x/y we got above is from the client's map, so 0,0 is
	 * actually topmost (northwest) corner of the map in the client,
	 * and not 0,0 of the actual map, so we need to transform it to
	 * actual map coordinates. */
	xt = pl->ob->x + (x - pl->socket.mapx / 2);
	yt = pl->ob->y + (y - pl->socket.mapy / 2);
	m = get_map_from_coord(pl->ob->map, &xt, &yt);

	/* Invalid x/y. */
	if (!m)
	{
		return;
	}

	/* Find and compress the path to the destination. */
	node = compress_path(find_path(pl->ob, pl->ob->map, pl->ob->x, pl->ob->y, m, xt, yt));

	/* No path available. */
	if (!node)
	{
		return;
	}

	/* Clear any previously queued paths. */
	player_path_clear(pl);

	/* 'node' now actually points to where the player is standing, so
	 * skip that. */
	if (node->next)
	{
		for (tmp = node->next; tmp; tmp = tmp->next)
		{
			player_path_add(pl, tmp->map, tmp->x, tmp->y);
		}
	}

	/* The last x,y where we wanted to move is not included in the
	 * above paths finding, so we have to add it manually. */
	player_path_add(pl, m, xt, yt);
}

/**
 * The ready command; client informs the server about new object to
 * mark as ready.
 * @param buf Data.
 * @param len Length of the data.
 * @param pl Player. */
void cmd_ready(uint8 *buf, int len, player *pl)
{
	tag_t tag;
	object *tmp;
	int type;

	if (!buf || len < 4)
	{
		return;
	}

	/* Get the ID of the object this is being done for. */
	tag = GetInt_String(buf);

	/* Search for the object in the player's inventory. */
	for (tmp = pl->ob->inv; tmp; tmp = tmp->below)
	{
		if (tmp->count == tag)
		{
			/* Determine whether this object can be readied. */
			type = cmd_ready_determine(tmp);

			if (type == -1)
			{
				return;
			}

			/* If it's already readied, just unready it. */
			if (QUERY_FLAG(tmp, FLAG_IS_READY))
			{
				CLEAR_FLAG(tmp, FLAG_IS_READY);
				pl->ready_object[type] = NULL;
				/* Inform the client about the change. */
				cmd_ready_send(pl, -1, type);

				draw_info_format(COLOR_WHITE, pl->ob, "Unready %s.", query_base_name(tmp, pl->ob));
			}
			/* Otherwise ready it. */
			else
			{
				/* Unready previously readied object of this type, if any. */
				cmd_ready_clear(pl->ob, type);
				SET_FLAG(tmp, FLAG_IS_READY);
				pl->ready_object[type] = tmp;
				pl->ready_object_tag[type] = tmp->count;
				/* Inform the client about the change. */
				cmd_ready_send(pl, tag, type);

				if (type == READY_OBJ_ARROW)
				{
					draw_info_format(COLOR_WHITE, pl->ob, "Ready %s as ammunition.", query_base_name(tmp, pl->ob));
				}
				else if (type == READY_OBJ_THROW)
				{
					draw_info_format(COLOR_WHITE, pl->ob, "Ready %s for throwing.", query_base_name(tmp, pl->ob));
				}
			}

			break;
		}
	}
}

/**
 * The fire command used by client's range widget, and triggered by the
 * player using ctrl + numpad.
 * @param buf Data.
 * @param len Length of the data.
 * @param pl Player. */
void command_fire(uint8 *buf, int len, player *pl)
{
	int dir, pos = 0;
	uint8 type;

	if (!buf || len < 2)
	{
		return;
	}

	/* Get the direction for firing and the fire mode (type). */
	dir = buf[pos++];
	dir = MAX(0, MIN(dir, 8));
	type = buf[pos++];

	fire(pl->ob, dir, type, (char *) buf + pos);
}

/**
 * Client notifies the server that it is still alive.
 * @param buf Data.
 * @param len Length of data.
 * @param ns The client's socket. */
void cmd_keepalive(char *buf, int len, socket_struct *ns)
{
	(void) buf;
	(void) len;
	ns->keepalive = 0;
}

/**
 * Client requests a password change for a player.
 * @param buf Data.
 * @param len Length of data.
 * @param pl The player. */
void cmd_password_change(uint8 *buf, int len, player *pl)
{
	char pswd_current[MAX_BUF], pswd_new[MAX_BUF];
	int pos = 0;
	size_t pswd_len;

	/* This makes the assumption that there is at least 1 character
	 * in both current and new password. */
	if (len < 4)
	{
		return;
	}

	/* Get the current and new password. */
	GetString_String(buf, len, &pos, pswd_current, sizeof(pswd_current));
	GetString_String(buf, len, &pos, pswd_new, sizeof(pswd_new));

	/* Make sure there are no untypeable characters... */
	cleanup_chat_string(pswd_current);
	cleanup_chat_string(pswd_new);

	/* Make sure there is current and new password. */
	if (!*pswd_current || !*pswd_new)
	{
		return;
	}

	pswd_len = strlen(pswd_new);

	/* Make sure the new password has a valid length. */
	if (pswd_len < PLAYER_PASSWORD_MIN || pswd_len > PLAYER_PASSWORD_MAX)
	{
		draw_info_format(COLOR_RED, pl->ob, "That password has an invalid length (must be %d-%d).", PLAYER_PASSWORD_MIN, PLAYER_PASSWORD_MAX);
		return;
	}

	/* Ensure the current password matches, but silently ignore if it
	 * doesn't as the client should handle this. */
	if (!strcmp(crypt_string(pswd_current, NULL), pl->password))
	{
		return;
	}

	/* Update the player's password. */
	strcpy(pl->password, crypt_string(pswd_new, NULL));
	draw_info(COLOR_GREEN, pl->ob, "Your password has been changed successfully.");
}
