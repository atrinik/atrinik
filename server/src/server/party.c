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
#include <sproto.h>

/** The party list. */
static partylist_struct *partylist = NULL;

/**
 * Add a player to party's member list.
 * @param party Party to add the player to.
 * @param op The player to add. */
void add_party_member(partylist_struct *party, object *op)
{
	objectlink *ol = get_objectlink();

	ol->objlink.ob = op;
	objectlink_link(&party->members, NULL, NULL, party->members, ol);
	CONTR(op)->party = party;
}

/**
 * Remove player from party's member list.
 * @param party Party to remove the player from.
 * @param op The player to remove. */
void remove_party_member(partylist_struct *party, object *op)
{
	objectlink *ol;

	for (ol = party->members; ol; ol = ol->next)
	{
		if (ol->objlink.ob == op)
		{
			objectlink_unlink(&party->members, NULL, ol);
			break;
		}
	}

	/* If no members left, remove the party */
	if (!party->members)
	{
		remove_party(CONTR(op)->party);
	}
	/* Otherwise choose a new leader, if the old one left */
	else if (op->name == party->leader)
	{
		party->leader = party->members->objlink.ob->name;
		new_draw_info_format(NDI_UNIQUE, 0, party->members->objlink.ob, "You are the new leader of party %s!", party->name);
	}

	CONTR(op)->party = NULL;
}

/**
 * Initialize a new party structure.
 * @param name Name of the new party.
 * @return The initialized party structure. */
partylist_struct *make_party(char *name)
{
	partylist_struct *party = (partylist_struct *) get_poolchunk(pool_parties);

	party->passwd[0] = '\0';
	party->name = strdup_local(name);
	party->leader = NULL;
	party->members = NULL;

	party->next = partylist;
	partylist = party;

	return party;
}

/**
 * Form a new party.
 * @param op Object forming the party.
 * @param name Name of the party. */
void form_party(object *op, char *name)
{
	partylist_struct *party = make_party(name);
	char tmp[MAX_BUF];

	add_party_member(party, op);
	new_draw_info_format(NDI_UNIQUE, 0, op, "You have formed party: %s", name);
	party->leader = op->name;

	snprintf(tmp, sizeof(tmp), "Xformsuccess %s", name);
	Write_String_To_Socket(&CONTR(op)->socket, BINARY_CMD_PARTY, tmp, strlen(tmp));
}

/**
 * Find a party by name.
 * @param name Party name to find.
 * @return Party if found, NULL otherwise. */
partylist_struct *find_party(char *name)
{
	partylist_struct *tmp;

	for (tmp = partylist; tmp; tmp = tmp->next)
	{
		if (!strcmp(tmp->name, name))
		{
			return tmp;
		}
	}

	return NULL;
}

/**
 * Send a message to party.
 * @param party Party to send the message to.
 * @param msg Message to send.
 * @param flag One of @ref PARTY_MESSAGE_xxx "party message flags".
 * @param op Player sending the message. If not NULL, this player will
 * not receive the message. */
void send_party_message(partylist_struct *party, char *msg, int flag, object *op)
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
			new_draw_info(NDI_UNIQUE | NDI_YELLOW, 0, ol->objlink.ob, msg);
		}
		else if (flag == PARTY_MESSAGE_CHAT)
		{
			new_draw_info(NDI_PLAYER | NDI_UNIQUE | NDI_YELLOW, 0, ol->objlink.ob, msg);
		}
	}
}

/**
 * Remove a party.
 * @param party The party to remove. */
void remove_party(partylist_struct *party)
{
	objectlink *ol;
	partylist_struct *tmp, *prev = NULL;

	for (ol = party->members; ol; ol = ol->next)
	{
		CONTR(ol->objlink.ob)->party = NULL;
		objectlink_unlink(&party->members, NULL, ol);
		return_poolchunk(ol, pool_objectlink);
	}

	for (tmp = partylist; tmp; prev = tmp, tmp = tmp->next)
	{
		if (tmp == party)
		{
			if (!prev)
			{
				partylist = tmp->next;
			}
			else
			{
				prev->next = tmp->next;
			}

			break;
		}
	}

	free(party->name);
	return_poolchunk(party, pool_parties);
}

/**
 * Party command used from client party GUI.
 * @param buf The incoming data.
 * @param len Length of the data.
 * @param pl Player. */
void PartyCmd(char *buf, int len, player *pl)
{
	char tmp[HUGE_BUF * 16], tmpbuf[MAX_BUF];
	partylist_struct *party;
	objectlink *ol;

	(void) len;
	tmp[0] = '\0';

	/* List command */
	if (!strcmp(buf, "list"))
	{
		sprintf(tmp, "Xlist");

		for (party = partylist; party; party = party->next)
		{
			snprintf(tmpbuf, sizeof(tmpbuf), "\nName: %s\tLeader: %s", party->name, party->leader);
			strncat(tmp, tmpbuf, sizeof(tmp) - strlen(tmp) - 1);
		}
	}
	/* Who command */
	else if (!strcmp(buf, "who"))
	{
		sprintf(tmp, "Xwho");

		if (!pl->party)
		{
			new_draw_info(NDI_UNIQUE | NDI_RED, 0, pl->ob, "You are not a member of any party.");
			return;
		}

		for (ol = pl->party->members; ol; ol = ol->next)
		{
			snprintf(tmpbuf, sizeof(tmpbuf), "\nName: %s\tMap: %s\tLevel: %d", ol->objlink.ob->name, ol->objlink.ob->map->name, ol->objlink.ob->level);
			strncat(tmp, tmpbuf, sizeof(tmp) - strlen(tmp) - 1);
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
			new_draw_info(NDI_UNIQUE, 0, pl->ob, "You must leave your current party before joining another.");
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
			new_draw_info_format(NDI_UNIQUE, 0, pl->ob, "Party %s does not exist. You must form it first.", buf);
			return;
		}

		sprintf(tmp, "Xjoin");

		if (pl->party != party)
		{
			/* If party password is not set or they've typed correct password... */
			if (party->passwd[0] == '\0' || !strcmp(party->passwd, partypassword))
			{
				add_party_member(party, pl->ob);
				new_draw_info_format(NDI_UNIQUE | NDI_GREEN, 0, pl->ob, "You have joined party: %s", party->name);
				snprintf(tmpbuf, sizeof(tmpbuf), "%s joined party %s.", pl->ob->name, party->name);
				send_party_message(party, tmpbuf, PARTY_MESSAGE_STATUS, pl->ob);
				snprintf(tmpbuf, sizeof(tmpbuf), "\nsuccess\n%s", party->name);
				strncat(tmp, tmpbuf, sizeof(tmp) - strlen(tmp) - 1);
			}
			/* Party password was typed but it wasn't correct. */
			else if (partypassword[0] != '\0')
			{
				new_draw_info(NDI_UNIQUE | NDI_RED, 0, pl->ob, "Incorrect party password.");
				return;
			}
			/* Otherwise ask them to type the password */
			else
			{
				new_draw_info_format(NDI_UNIQUE | NDI_YELLOW, 0, pl->ob, "The party %s requires a password. Please type it now, or press ESC to cancel joining.", party->name);
				snprintf(tmpbuf, sizeof(tmpbuf), "\npassword\n%s", party->name);
				strncat(tmp, tmpbuf, sizeof(tmp) - strlen(tmp) - 1);
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
			new_draw_info(NDI_UNIQUE | NDI_RED, 0, pl->ob, "Invalid party name to form.");
			return;
		}

		if (pl->party)
		{
			new_draw_info(NDI_UNIQUE | NDI_RED, 0, pl->ob, "You must leave your current party before forming a new one.");
			return;
		}

		if (find_party(buf))
		{
			new_draw_info_format(NDI_UNIQUE, 0, pl->ob, "The party %s already exists, pick another name.", buf);
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
			new_draw_info(NDI_UNIQUE, 0, pl->ob, "You are not a member of any party.");
			return;
		}

		strncpy(pl->party->passwd, buf, sizeof(pl->party->passwd) - 1);
		snprintf(tmpbuf, sizeof(tmpbuf), "The password for party %s changed to '%s'.", pl->party->name, pl->party->passwd);
		send_party_message(pl->party, tmpbuf, PARTY_MESSAGE_STATUS, NULL);
		return;
	}

	/* If we've got some data to send, send it. */
	if (tmp[0] != '\0')
	{
		Write_String_To_Socket(&pl->socket, BINARY_CMD_PARTY, tmp, strlen(tmp));
	}
}
