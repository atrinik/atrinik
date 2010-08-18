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
 * This file handles party related code. */

#include <global.h>

/**
 * String representations of the party looting modes. */
const char *party_loot_modes[PARTY_LOOT_MAX] =
{
	"normal", "leader", "random"
};

/**
 * Explanation of the party modes. */
const char *party_loot_modes_help[PARTY_LOOT_MAX] =
{
	"everyone is able to loot the corpse",
	"only the leader can loot the corpse",
	"loot is randomly split between party members when the corpse is opened"
};

/** The party list. */
party_struct *first_party = NULL;

/**
 * Add a player to party's member list.
 * @param party Party to add the player to.
 * @param op The player to add. */
void add_party_member(party_struct *party, object *op)
{
	objectlink *ol = get_objectlink();
	unsigned char buf[MAX_BUF];
	SockList sl;

	/* Add the player to the party's linked list of members. */
	ol->objlink.ob = op;
	objectlink_link(&party->members, NULL, NULL, party->members, ol);
	/* And set up player's pointer to the party. */
	CONTR(op)->party = party;

	/* Tell the client what party we have joined. */
	if (CONTR(op)->socket.socket_version < 1032)
	{
		char tmpbuf[MAX_BUF];

		snprintf(tmpbuf, sizeof(tmpbuf), "Xjoin\nsuccess\n%s", party->name);
		Write_String_To_Socket(&CONTR(op)->socket, BINARY_CMD_PARTY, tmpbuf, strlen(tmpbuf));
	}
	else
	{
	sl.buf = buf;
	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_PARTY);
	SockList_AddChar(&sl, CMD_PARTY_JOIN);
	SockList_AddString(&sl, (char *) party->name);
	Send_With_Handling(&CONTR(op)->socket, &sl);
	}
}

/**
 * Remove player from party's member list.
 * @param party Party to remove the player from.
 * @param op The player to remove. */
void remove_party_member(party_struct *party, object *op)
{
	objectlink *ol;
	unsigned char buf[MAX_BUF];
	SockList sl;

	/* Go through the party members, and remove the player that is
	 * leaving. */
	for (ol = party->members; ol; ol = ol->next)
	{
		if (ol->objlink.ob == op)
		{
			objectlink_unlink(&party->members, NULL, ol);
			break;
		}
	}

	/* If no members left, remove the party. */
	if (!party->members)
	{
		remove_party(CONTR(op)->party);
	}
	/* Otherwise choose a new leader, if the old one left. */
	else if (op->name == party->leader)
	{
		FREE_AND_ADD_REF_HASH(party->leader, party->members->objlink.ob->name);
		new_draw_info_format(NDI_UNIQUE, party->members->objlink.ob, "You are the new leader of party %s!", party->name);
	}

	/* Tell the client that we have left the party. */
	if (CONTR(op)->socket.socket_version < 1032)
	{
		char tmpbuf[MAX_BUF];

		strncpy(tmpbuf, "Xjoin\nsuccess\n ", sizeof(tmpbuf) - 1);
		Write_String_To_Socket(&CONTR(op)->socket, BINARY_CMD_PARTY, tmpbuf, strlen(tmpbuf));
	}
	else
	{
	sl.buf = buf;
	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_PARTY);
	SockList_AddChar(&sl, CMD_PARTY_LEAVE);
	Send_With_Handling(&CONTR(op)->socket, &sl);
	}

	CONTR(op)->party = NULL;
}

/**
 * Initialize a new party structure.
 * @param name Name of the new party.
 * @return The initialized party structure. */
party_struct *make_party(char *name)
{
	party_struct *party = (party_struct *) get_poolchunk(pool_parties);

	memset(party, 0, sizeof(party_struct));
	FREE_AND_COPY_HASH(party->name, name);

	party->next = first_party;
	first_party = party;

	return party;
}

/**
 * Form a new party.
 * @param op Object forming the party.
 * @param name Name of the party. */
void form_party(object *op, char *name)
{
	party_struct *party = make_party(name);

	add_party_member(party, op);
	new_draw_info_format(NDI_UNIQUE, op, "You have formed party: %s", name);
	FREE_AND_ADD_REF_HASH(party->leader, op->name);
}

/**
 * Find a party by name.
 * @param name Party name to find.
 * @return Party if found, NULL otherwise. */
party_struct *find_party(const char *name)
{
	party_struct *tmp;

	for (tmp = first_party; tmp; tmp = tmp->next)
	{
		if (!strcmp(tmp->name, name))
		{
			return tmp;
		}
	}

	return NULL;
}

/**
 * A party member has killed a monster. In the experience sharing code we
 * call this to check for all party members to check whether they have the
 * skill that was used to kill the monster. If they don't have the skill,
 * they won't be counted as 'getting exp shared'.
 *
 * This will also correctly handle cases where one player kills with slash
 * and everyone in the party also gets the exp shared, even if they are
 * cleave or impact, for example. Same happens for archery.
 *
 * Note that for the above to work, the party member must have the weapon
 * actually applied. If someone kills with slash, and this party member is
 * using punching (no weapon), they will not get any exp.
 * @param op Party member.
 * @param skill Skill that was used to kill the monster.
 * @return Skill ID to get experience in for op, @ref NO_SKILL_READY otherwise. */
sint16 party_member_get_skill(object *op, object *skill)
{
	sint16 skill_id = skill->stats.sp;

	switch (skill_id)
	{
		/* Handle weapon types. */
		case SK_MELEE_WEAPON:
		case SK_SLASH_WEAP:
		case SK_CLEAVE_WEAP:
		case SK_PIERCE_WEAP:
			if (CONTR(op)->set_skill_weapon != NO_SKILL_READY)
			{
				return CONTR(op)->set_skill_weapon;
			}

			break;

		/* Handle archery weapon types. */
		case SK_MISSILE_WEAPON:
		case SK_XBOW_WEAP:
		case SK_SLING_WEAP:
			if (CONTR(op)->set_skill_archery != NO_SKILL_READY)
			{
				return CONTR(op)->set_skill_archery;
			}

			break;

		/* Everything else. */
		default:
			if (CONTR(op)->skill_ptr[skill_id])
			{
				return skill_id;
			}
	}

	return NO_SKILL_READY;
}

/**
 * Randomly split corpse's loot between party's members.
 * @param pl Player that opened the corpse.
 * @param corpse The corpse. */
static void party_loot_random(object *pl, object *corpse)
{
	int count = 0, pl_id;
	party_struct *party = CONTR(pl)->party;
	objectlink *ol;
	object *tmp, *tmp_next;

	for (ol = party->members; ol; ol = ol->next)
	{
		if (on_same_map(ol->objlink.ob, pl))
		{
			count++;
		}
	}

	if (count == 1)
	{
		return;
	}

	for (tmp = corpse->inv; tmp; tmp = tmp_next)
	{
		int num = 1;

		tmp_next = tmp->below;

		/* Skip unpickable objects. */
		if (!can_pick(pl, tmp))
		{
			continue;
		}

		pl_id = rndm(1, count);

		for (ol = party->members; ol; ol = ol->next)
		{
			if (on_same_map(ol->objlink.ob, pl))
			{
				if (num == pl_id)
				{
					if (player_can_carry(ol->objlink.ob, WEIGHT_NROF(tmp, tmp->nrof)))
					{
						new_draw_info_format(NDI_UNIQUE | NDI_BLUE, ol->objlink.ob, "You receive the %s.", query_name(tmp, NULL));
						esrv_del_item(CONTR(pl), tmp->count, tmp->env);
						remove_ob(tmp);
						tmp = insert_ob_in_ob(tmp, ol->objlink.ob);
						esrv_send_item(ol->objlink.ob, tmp);
					}

					break;
				}

				num++;
			}
		}
	}
}

/**
 * Check if player can open a party corpse.
 * @param pl Player trying to open the corpse.
 * @param corpse The corpse.
 * @return 1 if we can open the corpse, 0 otherwise. */
int party_can_open_corpse(object *pl, object *corpse)
{
	/* Check if the player is in the same party. */
	if (!CONTR(pl)->party || corpse->slaying != CONTR(pl)->party->name)
	{
		new_draw_info(NDI_UNIQUE, pl, "It's not your party's bounty.");
		return 0;
	}

	switch (CONTR(pl)->party->loot)
	{
		/* Normal: anyone can access it. */
		case PARTY_LOOT_NORMAL:
		default:
			return 1;

		/* Only leader can access it. */
		case PARTY_LOOT_LEADER:
			if (pl->name != CONTR(pl)->party->leader)
			{
				new_draw_info(NDI_UNIQUE, pl, "You're not the party's leader.");
				return 0;
			}

			return 1;
	}
}

/**
 * Do any special handling after a party corpse has been opened.
 * @param pl Player who opened the corpse.
 * @param corpse The corpse. */
void party_handle_corpse(object *pl, object *corpse)
{
	/* Sanity check. */
	if (!CONTR(pl)->party)
	{
		return;
	}

	switch (CONTR(pl)->party->loot)
	{
		case PARTY_LOOT_RANDOM:
			party_loot_random(pl, corpse);
			break;
	}
}

/**
 * Send a message to party.
 * @param party Party to send the message to.
 * @param msg Message to send.
 * @param flag One of @ref PARTY_MESSAGE_xxx "party message flags".
 * @param op Player sending the message. If not NULL, this player will
 * not receive the message. */
void send_party_message(party_struct *party, char *msg, int flag, object *op)
{
	objectlink *ol;

	for (ol = party->members; ol; ol = ol->next)
	{
		if (ol->objlink.ob == op)
		{
			continue;
		}

		if (flag == PARTY_MESSAGE_STATUS)
		{
			new_draw_info(NDI_UNIQUE | NDI_YELLOW, ol->objlink.ob, msg);
		}
		else if (flag == PARTY_MESSAGE_CHAT)
		{
			new_draw_info(NDI_PLAYER | NDI_UNIQUE | NDI_YELLOW, ol->objlink.ob, msg);
		}
	}
}

/**
 * Remove a party.
 * @param party The party to remove. */
void remove_party(party_struct *party)
{
	objectlink *ol;
	party_struct *tmp, *prev = NULL;

	for (ol = party->members; ol; ol = ol->next)
	{
		CONTR(ol->objlink.ob)->party = NULL;
		objectlink_unlink(&party->members, NULL, ol);
		return_poolchunk(ol, pool_objectlink);
	}

	for (tmp = first_party; tmp; prev = tmp, tmp = tmp->next)
	{
		if (tmp == party)
		{
			if (!prev)
			{
				first_party = tmp->next;
			}
			else
			{
				prev->next = tmp->next;
			}

			break;
		}
	}

	FREE_AND_CLEAR_HASH(party->name);
	FREE_AND_CLEAR_HASH(party->leader);
	return_poolchunk(party, pool_parties);
}

/**
 * Party command used from client party GUI.
 * @param buf The incoming data.
 * @param len Length of the data.
 * @param pl Player.
 * @deprecated */
void PartyCmd(char *buf, int len, player *pl)
{
	party_struct *party;
	objectlink *ol;
	StringBuffer *sb = NULL;
	char tmpbuf[MAX_BUF];

	if (!buf || !len)
	{
		return;
	}

	/* List command */
	if (!strcmp(buf, "list"))
	{
		sb = stringbuffer_new();
		stringbuffer_append_string(sb, "Xlist");

		for (party = first_party; party; party = party->next)
		{
			stringbuffer_append_printf(sb, "\nName: %s\tLeader: %s", party->name, party->leader);
		}
	}
	/* Who command */
	else if (!strcmp(buf, "who"))
	{
		if (!pl->party)
		{
			new_draw_info(NDI_UNIQUE | NDI_RED, pl->ob, "You are not a member of any party.");
			return;
		}

		sb = stringbuffer_new();
		stringbuffer_append_string(sb, "Xwho");

		for (ol = pl->party->members; ol; ol = ol->next)
		{
			stringbuffer_append_printf(sb, "\nName: %s\tMap: %s\tLevel: %d", ol->objlink.ob->name, ol->objlink.ob->map->name, ol->objlink.ob->level);
		}
	}
	/* Join command */
	else if (strncmp(buf, "join ", 5) == 0)
	{
		char partypassword[MAX_BUF];

		buf += 5;
		partypassword[0] = '\0';

		if (pl->party)
		{
			new_draw_info(NDI_UNIQUE, pl->ob, "You must leave your current party before joining another.");
			return;
		}

		/* This means we want to join a password protected party. */
		if (strstr(buf, "Password: "))
		{
			sscanf(buf, "Name: %32[^\n]\nPassword: %32[^\n]", buf, partypassword);
		}

		party = find_party(buf);

		if (!party)
		{
			new_draw_info_format(NDI_UNIQUE, pl->ob, "Party %s does not exist. You must form it first.", buf);
			return;
		}

		if (pl->party != party)
		{
			/* If party password is not set or they've typed correct password... */
			if (party->passwd[0] == '\0' || !strcmp(party->passwd, partypassword))
			{
				add_party_member(party, pl->ob);
				new_draw_info_format(NDI_UNIQUE | NDI_GREEN, pl->ob, "You have joined party: %s", party->name);
				snprintf(tmpbuf, sizeof(tmpbuf), "%s joined party %s.", pl->ob->name, party->name);
				send_party_message(party, tmpbuf, PARTY_MESSAGE_STATUS, pl->ob);

				sb = stringbuffer_new();
				stringbuffer_append_printf(sb, "Xjoin\nsuccess\n%s", party->name);
			}
			/* Party password was typed but it wasn't correct. */
			else if (partypassword[0] != '\0')
			{
				new_draw_info(NDI_UNIQUE | NDI_RED, pl->ob, "Incorrect party password.");
				return;
			}
			/* Otherwise ask them to type the password */
			else
			{
				new_draw_info_format(NDI_UNIQUE | NDI_YELLOW, pl->ob, "The party %s requires a password. Please type it now, or press ESC to cancel joining.", party->name);
				sb = stringbuffer_new();
				stringbuffer_append_printf(sb, "Xjoin\npassword\n%s", party->name);
			}
		}
	}
	/* Form command */
	else if (strncmp(buf, "form ", 5) == 0)
	{
		buf += 5;

		buf = cleanup_chat_string(buf);

		if (!buf || *buf == '\0')
		{
			new_draw_info(NDI_UNIQUE | NDI_RED, pl->ob, "Invalid party name to form.");
			return;
		}

		if (pl->party)
		{
			new_draw_info(NDI_UNIQUE | NDI_RED, pl->ob, "You must leave your current party before forming a new one.");
			return;
		}

		if (find_party(buf))
		{
			new_draw_info_format(NDI_UNIQUE, pl->ob, "The party %s already exists, pick another name.", buf);
			return;
		}

		form_party(pl->ob, buf);
		return;
	}
	/* Password command */
	else if (strncmp(buf, "password ", 9) == 0)
	{
		buf += 9;

		if (!pl->party)
		{
			new_draw_info(NDI_UNIQUE, pl->ob, "You are not a member of any party.");
			return;
		}

		strncpy(pl->party->passwd, buf, sizeof(pl->party->passwd) - 1);
		snprintf(tmpbuf, sizeof(tmpbuf), "The password for party %s changed to '%s'.", pl->party->name, pl->party->passwd);
		send_party_message(pl->party, tmpbuf, PARTY_MESSAGE_STATUS, NULL);
		return;
	}

	/* If we've got some data to send, send it. */
	if (sb)
	{
		size_t cp_len = sb->pos;
		char *cp = stringbuffer_finish(sb);

		Write_String_To_Socket(&pl->socket, BINARY_CMD_PARTY, cp, cp_len);
		free(cp);
	}
}
