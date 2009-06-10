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
* the Free Software Foundation; either version 3 of the License, or     *
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

#include <global.h>
#ifndef __CEXTRACT__
#include <sproto.h>
#endif

/* Keeps track of first party in list */
static partylist *firstparty = NULL;
/* Keeps track of last party in list */
static partylist *lastparty = NULL;

partylist *form_party(object *op, char *params, partylist *firstparty, partylist *lastparty)
{
    partylist *newparty;
    int nextpartyid;

    if (firstparty == NULL)
        nextpartyid = 1;
    else
	{
        nextpartyid = lastparty->partyid;
        nextpartyid++;
    }

    newparty = (partylist *)malloc(sizeof(partylist));
    newparty->partyid = nextpartyid;
    newparty->partyname = strdup_local(params);
    newparty->total_exp = 0;
    newparty->kills = 0;

    newparty->passwd[0] = '\0';
    newparty->next = NULL;
    newparty->partyleader = add_string(op->name);

    if (firstparty != NULL)
        lastparty->next = newparty;

    new_draw_info_format(NDI_UNIQUE, 0, op, "You have formed party: %s", newparty->partyname);
	CONTR(op)->party_number = nextpartyid;

    return newparty;
}

char *find_party(int partynumber, partylist *party)
{
    while (party != NULL)
	{
        if (party->partyid == partynumber)
        	return party->partyname;
        else
            party = party->next;
    }

    return NULL;
}

partylist *find_party_struct(int partynumber)
{
  	partylist *party;

  	party = firstparty;

  	while (party != NULL)
    {
      	if (party->partyid == partynumber)
			return party;
      	else
			party = party->next;
    }

  	return NULL;
}

void remove_party(partylist *target_party)
{
    partylist *tmpparty;
    partylist *previousparty;
    partylist *nextparty;
    player *pl;

    if (firstparty == NULL)
	{
    	LOG(llevBug, "BUG: remove_party(): I was asked to remove party %s, but no parties are defined!\n", target_party->partyname);
        return;
    }

	for (pl = first_player; pl != NULL; pl = pl->next)
        if (pl->party_number == target_party->partyid)
            pl->party_number = -1;

    /* special case-ism for parties at the beginning and end of the list */
    if (target_party == firstparty)
	{
        if (lastparty == target_party)
            lastparty = NULL;

        firstparty = firstparty->next;
        free(target_party->partyname);
        free(target_party);
        return;
    }
	else if (target_party == lastparty)
	{
        for (tmpparty = firstparty; tmpparty->next != NULL; tmpparty = tmpparty->next)
		{
            if (tmpparty->next == target_party)
			{
                lastparty = tmpparty;
                free(target_party->partyname);
                free(target_party);
                lastparty->next = NULL;
                return;
            }
        }
    }

    for (tmpparty = firstparty; tmpparty->next != NULL; tmpparty = tmpparty->next)
        if (tmpparty->next == target_party)
		{
            previousparty = tmpparty;
            nextparty = tmpparty->next->next;
            /* this should be safe, because we already dealt with the lastparty case */
            previousparty->next = nextparty;
            free(target_party->partyname);
            free(target_party);
            return;
        }
}

/* Remove unused parties (no players), this could be made to scale a lot better. */
void obsolete_parties(void)
{
    int player_count;
    player *pl;
    partylist *party;
    partylist *next = NULL;

	/* we can't obsolete parties if there aren't any */
    if (!firstparty)
        return;

    for (party = firstparty; party != NULL; party = next)
	{
        next = party->next;
        player_count = 0;
        for (pl = first_player; pl != NULL; pl = pl->next)
            if (pl->party_number == party->partyid)
                player_count++;

        if (player_count == 0)
            remove_party(party);
    }
}

#ifdef PARTY_KILL_LOG
void add_kill_to_party(int numb, char *killer, char *dead, long exp)
{
  	partylist *party;
  	int i, pos;

  	party = find_party_struct(numb);

  	if (party == NULL)
		return;

  	if (party->kills >= PARTY_KILL_LOG)
    {
      	pos = PARTY_KILL_LOG - 1;
      	for (i = 0; i < PARTY_KILL_LOG - 1; i++)
			memcpy(&(party->party_kills[i]), &(party->party_kills[i + 1]), sizeof(party->party_kills[0]));
    }
  	else
    	pos = party->kills;

	party->kills++;
	party->total_exp += exp;
	party->party_kills[pos].exp = exp;
	strncpy(party->party_kills[pos].killer, killer, MAX_NAME);
	strncpy(party->party_kills[pos].dead, dead, MAX_NAME);
	party->party_kills[pos].killer[MAX_NAME] = 0;
	party->party_kills[pos].dead[MAX_NAME] = 0;
}
#endif

void send_party_message(object *op, char *msg)
{
 	player *pl;
  	int no = CONTR(op)->party_number;
  	for (pl = first_player; pl != NULL; pl = pl->next)
    	if (CONTR(pl->ob)->party_number == no && pl->ob != op)
			new_draw_info(NDI_PLAYER | NDI_UNIQUE | NDI_YELLOW, 0, pl->ob, msg);
}

int command_gsay(object *op, char *params)
{
  	char party_params[MAX_BUF];

  	if (!params)
		return 0;

  	strcpy(party_params, "say ");
  	strcat(party_params, params);
  	command_party(op, party_params);
  	return 0;
}

int command_party(object *op, char *params)
{
  	char buf[MAX_BUF];
	/* For iterating over linked list */
  	partylist *tmpparty, *party;
	/* For iterating over linked list */
  	char *currentparty;
	int player_count = 0;
	player *pl;

  	if (params == NULL)
	{
        if (CONTR(op)->party_number <= 0)
		{
          	new_draw_info(NDI_UNIQUE, 0, op, "You are not a member of any party.");
          	new_draw_info(NDI_UNIQUE, 0, op, "For help try: /party help");
        }
        else
		{
          	currentparty = find_party(CONTR(op)->party_number, firstparty);
	  		new_draw_info_format(NDI_UNIQUE, 0, op, "You are a member of party %s.", currentparty);
        }
        return 1;
	}

  	if (strcmp(params, "help") == 0)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "To form a party type: /party form <partyname>");
		new_draw_info(NDI_UNIQUE, 0, op, "To join a party type: /party join <partyname>");
		new_draw_info(NDI_UNIQUE, 0, op, "If the party has a password, it will prompt you for it.");
		new_draw_info(NDI_UNIQUE, 0, op, "For a list of current parties type: /party list");
		new_draw_info(NDI_UNIQUE, 0, op, "To leave a party type: /party leave");
		new_draw_info(NDI_UNIQUE, 0, op, "To change a password for a party type: /party password <password>");
		new_draw_info(NDI_UNIQUE, 0, op, "There is a 8 character max and only leader can change password.");
		new_draw_info(NDI_UNIQUE, 0, op, "To talk to party members type: /party say <msg>");
		new_draw_info(NDI_UNIQUE, 0, op, "To see who is in your party: /party who");
#ifdef PARTY_KILL_LOG
		new_draw_info(NDI_UNIQUE, 0, op, "To see what you've killed, type: /party kills");
#endif
		return 1;
  	}

#ifdef PARTY_KILL_LOG
  	if (!strncmp(params, "kills", 5))
    {
		int i,max;
		char chr;
		char buffer[80];
		float exp;

      	if (CONTR(op)->party_number <= 0)
		{
	  		new_draw_info(NDI_UNIQUE, 0, op, "You are not a member of any party.");
	  		return 1;
		}

      	tmpparty = find_party_struct(CONTR(op)->party_number);

      	if (!tmpparty->kills)
		{
	  		new_draw_info(NDI_UNIQUE, 0, op, "You haven't killed anything yet.");
	  		return 1;
		}

		max = tmpparty->kills - 1;
      	if (max > PARTY_KILL_LOG - 1)
			max = PARTY_KILL_LOG - 1;

      	new_draw_info(NDI_UNIQUE, 0, op, "Killed          |          Killer|     Exp");
      	new_draw_info(NDI_UNIQUE, 0, op, "----------------+----------------+--------");

		for (i = 0; i <= max; i++)
		{
			exp = tmpparty->party_kills[i].exp;
			chr = ' ';
			if (exp > 1000000)
			{
				exp /= 1000000;
				chr = 'M';
			}
			else if (exp > 1000)
			{
				exp /= 1000;
				chr = 'k';
			}

			sprintf(buffer, "%16s|%16s|%6.1f%c", tmpparty->party_kills[i].dead, tmpparty->party_kills[i].killer, exp, chr);
			new_draw_info(NDI_UNIQUE, 0, op, buffer);
		}

      	exp = tmpparty->total_exp;
      	chr = ' ';

      	if (exp > 1000000)
		{
			exp /= 1000000;
			chr='M';
		}
      	else if (exp > 1000)
		{
			exp /= 1000;
			chr='k';
		}
      	new_draw_info(NDI_UNIQUE, 0, op, "----------------+----------------+--------");
      	sprintf(buffer, "Totals: %d kills, %.1f%c exp", tmpparty->kills, exp, chr);
      	new_draw_info(NDI_UNIQUE, 0, op, buffer);
      	return 1;
    }
#endif

  	if (strncmp(params, "say ", 4) == 0)
    {
		if (CONTR(op)->party_number <= 0)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "You are not a member of any party.");
			return 1;
		}
      	params += 4;
		tmpparty = find_party_struct(CONTR(op)->party_number);
      	sprintf(buf, "[%s] %s says: %s", tmpparty->partyname, op->name, params);
        send_party_message(op, buf);
		LOG(llevInfo, "CLOG PARTY:%s [%s] >%s<\n", query_name(op, NULL), tmpparty->partyname, params);
        new_draw_info(NDI_PLAYER | NDI_UNIQUE | NDI_YELLOW, 0, op, buf);
        return 1;
	}

  	if (strncmp(params, "form ", 5) == 0)
	{
		int oldparty = -1;
		params += 5;

		if (CONTR(op)->party_number)
            oldparty = CONTR(op)->party_number;

		if (firstparty == NULL)
		{
			firstparty = form_party(op, params, firstparty, lastparty);
			lastparty = firstparty;
			return 0;
		}
		else
			tmpparty = firstparty->next;

		if (tmpparty == NULL)
		{
			if (strcmp(firstparty->partyname, params) != 0)
			{
				lastparty = form_party(op, params, firstparty, lastparty);
				return 0;
			}
			else
			{
				new_draw_info_format(NDI_UNIQUE, 0, op, "The party %s already exists, pick another name.", params);
				return 1;
			}
		}
		tmpparty = firstparty;

		while (tmpparty != NULL)
		{
			if (strcmp(tmpparty->partyname, params) == 0)
			{
				new_draw_info_format(NDI_UNIQUE, 0, op, "The party %s already exists, pick another name.", params);
				return 1;
			}
			tmpparty = tmpparty->next;
		}

		lastparty = form_party(op, params, firstparty, lastparty);

		/* The player might have previously been a member of a party, if so, he will be leaving
         * it, so check if there are any other members and if not, delete the party */
        if (oldparty != -1)
		{
            for (pl = first_player; pl->next != NULL; pl = pl->next)
			{
                if (pl->party_number == oldparty)
                    player_count++;
            }
            if (player_count == 0)
                remove_party(tmpparty);
        }

		return 0;
  	}

	if (strcmp(params, "leave") == 0)
	{
		if (CONTR(op)->party_number <= 0)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "You are not a member of any party.");
			return 1;
		}

		currentparty = find_party(CONTR(op)->party_number, firstparty);
		new_draw_info_format(NDI_UNIQUE, 0, op, "You leave party %s.", currentparty);
		sprintf(buf, "%s leaves party %s.", op->name, currentparty);
		send_party_message(op, buf);

  		party = find_party_struct(CONTR(op)->party_number);

		/* Choose new leader... if no leader, remove the party - we assume it has no members anymore. */
		if (strcmp(op->name, party->partyleader) == 0)
		{
			player *pl;
			int party_remove = 1, no = CONTR(op)->party_number;
			for (pl = first_player; pl != NULL; pl = pl->next)
			{
				if (CONTR(pl->ob)->party_number == no && pl->ob != op)
				{
					if (strcmp(party->partyleader, pl->ob->name))
					{
						party->partyleader = add_string(pl->ob->name);
						new_draw_info_format(NDI_UNIQUE, 0, pl->ob, "You are the new leader of party %s!", currentparty);
					}
					party_remove = 0;
					break;
				}
			}

			if (party_remove == 1)
				remove_party(party);
		}

		CONTR(op)->party_number = -1;
		return 1;
	}

  	if (strcmp(params, "who") == 0)
	{
		player *pl;
		int no = CONTR(op)->party_number;
		if (no <= 0)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "You are not a member of any party.");
			return 1;
		}

		new_draw_info_format(NDI_UNIQUE, 0, op, "Members of party: %s.", find_party(no, firstparty));
		for (pl = first_player; pl != NULL; pl = pl->next)
		{
			if (CONTR(pl->ob)->party_number == no)
			{
				sprintf(buf, "%3d %s the %s", pl->ob->level, pl->ob->name, pl->ob->race);
				new_draw_info(NDI_UNIQUE, 0, op, buf);
			}
		}
		return 1;
  	}

	if (strncmp(params, "password ", 9) == 0)
	{
		partylist *tmplist;

		params += 9;

		if (CONTR(op)->party_number <= 0)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "You are not a member of a party.");
			return 1;
		}

		tmplist = firstparty;
		while (tmplist != NULL)
		{
			if (tmplist->partyid == CONTR(op)->party_number)
			{
				if (strcmp(tmplist->partyleader, op->name))
				{
					new_draw_info(NDI_UNIQUE, 0, op, "You are not the leader of this party so you can't change the password.");
					return 1;
				}

				strncpy(tmplist->passwd, params, 10);
				new_draw_info_format(NDI_UNIQUE, 0, op, "The password for party %s changed to '%s'.", tmplist->partyname, tmplist->passwd);
				send_party_message(op, buf);
				return 0;
			}

			tmplist = tmplist->next;
		}
		return 0;
	}

	if (strcmp(params, "list") == 0)
	{
		partylist *tmplist;

		tmplist = firstparty;

		if (firstparty == NULL)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "There are no parties active right now.");
			return 1;
		}

		new_draw_info(NDI_UNIQUE, 0, op, "Party name                       Leader");
		new_draw_info(NDI_UNIQUE, 0, op, "----------                       ------");

		while (tmplist != NULL)
		{
			new_draw_info_format(NDI_UNIQUE, 0, op, "%-32s %s", tmplist->partyname, tmplist->partyleader);
			tmplist = tmplist->next;
		}

		return 0;
	}

	if (strncmp(params, "join ", 5) == 0)
	{
		params += 5;

		/* Can't join a party cause non exist */
		if (firstparty == NULL)
		{
			new_draw_info_format(NDI_UNIQUE, 0, op, "Party: %s does not exist. You must form it first.", params);
			return 1;
		}

		/* Special case if there is only one party */
		if (firstparty->next == NULL)
		{
			if (!strstr(params, firstparty->partyname))
			{
				new_draw_info_format(NDI_UNIQUE, 0, op, "Party: %s does not exist. You must form it first.", params);
				return 1;
			}
			else
			{
				if (CONTR(op)->party_number == firstparty->partyid)
				{
					new_draw_info_format(NDI_UNIQUE, 0, op, "You are already in party: %s", firstparty->partyname);
					return 1;
				}

				/* found party player wants to join */
				if (firstparty->passwd[0] == '\0' || strstr(params, firstparty->passwd))
				{
					CONTR(op)->party_number = firstparty->partyid;
					new_draw_info_format(NDI_UNIQUE, 0, op, "You have joined party: %s", firstparty->partyname);
					sprintf(buf, "%s joined party %s.", op->name, firstparty->partyname);
					send_party_message(op, buf);
					return 0;
				}
				else
				{
					new_draw_info_format(NDI_UNIQUE, 0, op, "Party requires you to present password. Type '/party join %s <password>' to join.", firstparty->partyname);
					return 0;
				}
			}
		}

		tmpparty = firstparty;
		while (tmpparty != NULL)
		{
			if (strstr(params, tmpparty->partyname))
			{
				if (CONTR(op)->party_number == tmpparty->partyid)
				{
					new_draw_info_format(NDI_UNIQUE, 0, op, "You are already a member of party: %s", tmpparty->partyname);
					return 1;
				}
				else
				{
					if (tmpparty->passwd[0] == '\0' || strstr(params, tmpparty->passwd))
					{
						CONTR(op)->party_number = tmpparty->partyid;
						new_draw_info_format(NDI_UNIQUE, 0, op, "You have joined party: %s", tmpparty->partyname);
						sprintf(buf, "%s joined party %s.", op->name, tmpparty->partyname);
						send_party_message(op, buf);
						return 0;
					}
					else
					{
						new_draw_info_format(NDI_UNIQUE, 0, op, "Party requires you to present password. Type '/party join %s <password>' to join.", tmpparty->partyname);
						return 0;
					}
				}
			}
			else
				tmpparty = tmpparty->next;
		}

		new_draw_info_format(NDI_UNIQUE, 0, op, "Party %s does not exist. You must form it first.", params);
		return 1;
	}

	return 1;
}
