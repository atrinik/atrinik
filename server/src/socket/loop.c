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
 * Mainly deals with initialization and higher level socket maintenance
 * (checking for lost connections and if data has arrived). */

#include <global.h>
#include <sproto.h>
#include <sockproto.h>

#ifndef WIN32
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <newserver.h>

static fd_set tmp_read, tmp_exceptions, tmp_write;

/**
 * Socket command, used when the player has not fully connected yet.
 * @param char The data from client.
 * @param int Length of the data.
 * @param NewSocket The client's socket. */
typedef void (*func_uint8_int_ns) (char *, int, NewSocket *);

/** Socket command structure. */
struct NsCmdMapping
{
	/** The command name. */
	char *cmdname;

	/** Function to call for the command. */
	func_uint8_int_ns cmdproc;
};

/**
 * Player command, used after the player has logged to the game.
 * @param char The data from client.
 * @param int Length of the data.
 * @param player The player that is sending the command. */
typedef void (*func_uint8_int_pl) (char *, int, player *);

/** Player command structure. */
struct PlCmdMapping
{
	/** The command name. */
	char *cmdname;

	/** Function to call for the command. */
	func_uint8_int_pl cmdproc;
};

/**
 * @defgroup dispatch_tables Dispatch tables
 * Dispatch tables for the server.
 *
 *
 * CmdMapping is the dispatch table for the server, used in HandleClient,
 * which gets called when the client has input.  All commands called here
 * use the same parameter form (char* data, int len, int clientnum.
 * We do implicit casts, because the data that is being passed is
 * unsigned (pretty much needs to be for binary data), however, most
 * of these treat it only as strings, so it makes things easier
 * to cast it here instead of a bunch of times in the function itself.
 *@{*/

/** Commands sent by the client, based on player's actions. */
static struct PlCmdMapping plcommands[] =
{
	{"ex",          ExamineCmd},
	{"ap",          ApplyCmd},
	{"mv",          MoveCmd},
	{"reply",       ReplyCmd},
	{"cm",          (func_uint8_int_pl)PlayerCmd},
	{"mapredraw",   MapRedrawCmd},
	{"lock",        (func_uint8_int_pl)LockItem},
	{"mark",        (func_uint8_int_pl)MarkItem},
	{"/fire",       command_fire},
	{"fr",          command_face_request},
	{"nc",          command_new_char},
	{"pt",          PartyCmd},
	{"qs",          QuickSlotCmd},
	{"shop",        ShopCmd},
	{"qlist",       QuestListCmd},
	{NULL, NULL}
};

/** Commands sent directly by the client, when connecting or needed. */
static struct NsCmdMapping nscommands[] =
{
	{"addme",       AddMeCmd},
	{"askface",     SendFaceCmd},
	{"requestinfo", RequestInfo},
	{"setfacemode", SetFaceMode},
	{"setsound",    SetSound},
	{"setup",       SetUp},
	{"version",     VersionCmd},
	{"rf",          RequestFileCmd},
	{NULL, NULL}
};
/*@}*/

/**
 * RequestInfo is sort of a meta command - there is some specific request
 * of information, but we call other functions to provide that
 * information. */
void RequestInfo(char *buf, int len, NewSocket *ns)
{
	char *params = NULL, *cp;
	char bigbuf[MAX_BUF];
	int slen;

	/* Set up replyinfo before we modify any of the buffers - this is
	 * used if we don't find a match. */
	slen = 1;
	bigbuf[0] = BINARY_CMD_REPLYINFO;
	bigbuf[1] = 0;
	safe_strcat(bigbuf, buf, &slen, sizeof(bigbuf));

	/* Find the first space, make it NULL, and update the params
	 * pointer. */
	for (cp = buf; *cp != '\0'; cp++)
	{
		if (*cp == ' ')
		{
			*cp = '\0';
			params = cp + 1;

			break;
		}
	}

	if (!strcmp(buf, "image_info"))
	{
		send_image_info(ns, params);
	}
	else if (!strcmp(buf, "image_sums"))
	{
		send_image_sums(ns, params);
	}
	else
	{
		Write_String_To_Socket(ns, BINARY_CMD_REPLYINFO, bigbuf, len);
	}
}

/**
 * Handle client commands.
 *
 * We only get here once there is input, and only do basic connection
 * checking.
 * @param ns Socket sending the command.
 * @param pl Player associated to the socket, NULL if no player (one of
 * the init_sockets for just starting a connection). */
void HandleClient(NewSocket *ns, player *pl)
{
	int len = 0, i, cmd_count = 0;
	unsigned char *data;

	/* Loop through this - maybe we have several complete packets here. */
	while (1)
	{
		/* If it is a player, and they don't have any speed left, we
		 * return, and will read in the data when they do have time. */
		if (pl && pl->state == ST_PLAYING && pl->ob != NULL && pl->ob->speed_left < 0)
		{
			return;
		}

		i = SockList_ReadPacket(ns->fd, &ns->inbuf, MAXSOCKBUF - 1);

		if (i < 0)
		{
			LOG(llevDebug, "Drop Connection: %s (%s)\n", (pl ? pl->ob->name : "NONE"), ns->host ? ns->host : "NONE");
			/* Caller will take care of cleaning this up */
			ns->status = Ns_Dead;

			return;
		}

		/* Still don't have a full packet */
		if (i == 0)
		{
			return;
		}

		/* reset idle counter */
		if (pl && pl->state == ST_PLAYING)
		{
			ns->login_count = 0;
		}

		/* First, break out beginning word.  There are at least
		 * a few commands that do not have any parameters.  If
		 * we get such a command, don't worry about trying
		 * to break it up. */
		data = (unsigned char *) strchr((char *) ns->inbuf.buf + 2, ' ');

		if (data)
		{
			*data = '\0';
			data++;
			len = ns->inbuf.len - (data - ns->inbuf.buf);
		}
		else
		{
			len = 0;
		}

		/* Terminate buffer - useful for string data */
		ns->inbuf.buf[ns->inbuf.len] = '\0';

		for (i = 0; nscommands[i].cmdname != NULL; i++)
		{
			if (strcmp((char *) ns->inbuf.buf + 2, nscommands[i].cmdname) == 0)
			{
				nscommands[i].cmdproc((char *) data, len, ns);
				ns->inbuf.len = 0;

				/* We have successfully added this connect! */
				if (ns->addme)
				{
					ns->addme = 0;
					return;
				}

				goto next_command;
			}
		}

		/* Only valid players can use these commands */
		if (pl)
		{
			for (i = 0; plcommands[i].cmdname != NULL; i++)
			{
				if (strcmp((char *) ns->inbuf.buf + 2, plcommands[i].cmdname) == 0)
				{
					plcommands[i].cmdproc((char *) data, len, pl);
					ns->inbuf.len = 0;

					goto next_command;
				}
			}
		}

		/* If we get here, we didn't find a valid command.  Logging
		 * this might be questionable, because a broken client/malicious
		 * user could certainly send a whole bunch of invalid commands. */
		LOG(llevDebug, "CRACK: Bad command from client (%s) (%s)\n", ns->inbuf.buf + 2, data);
		ns->status = Ns_Dead;
		return;

next_command:
		if (cmd_count++ <= 8 && ns->status != Ns_Dead)
		{
			/* LOG(llevDebug,"MultiCmd: #%d /%s)\n", cmd_count, (char*)ns->inbuf.buf+2); */
			continue;
		}

		return;
	}
}

/**
 * Tell the Atrinik watchdog program that we are still alive by sending
 * datagrams to port 13325 on localhost.
 * @see atrinik_watchdog */
void watchdog()
{
	static int fd = -1;
	static struct sockaddr_in insock;

	if (fd == -1)
	{
		struct protoent *protoent;

		if ((protoent = getprotobyname("udp")) == NULL || (fd = socket(PF_INET, SOCK_DGRAM, protoent->p_proto)) == -1)
		{
			return;
		}

		insock.sin_family = AF_INET;
		insock.sin_port = htons((unsigned short) 13325);
		insock.sin_addr.s_addr = inet_addr("127.0.0.1");
	}

	sendto(fd, (void *) &fd, 1, 0, (struct sockaddr *) &insock, sizeof(insock));
}

/**
 * Remove a player from the game that has been disconnected by logging
 * out, the socket connection was interrupted, etc.
 * @param pl The player to remove. */
static void remove_ns_dead_player(player *pl)
{
	if (pl == NULL || pl->ob->type == DEAD_OBJECT)
	{
		return;
	}

	/* Remove DM entry */
	if (QUERY_FLAG(pl->ob, FLAG_WIZ))
	{
		remove_active_DM(pl->ob);
	}

	/* Trigger the global LOGOUT event */
	trigger_global_event(EVENT_LOGOUT, pl->ob, pl->socket.host);

	if (dm_list)
	{
		player *pl_tmp;
		int players;
		objectlink *tmp_dm_list;

		/* Count the players */
		for (pl_tmp = first_player, players = 0; pl_tmp != NULL; pl_tmp = pl_tmp->next, players++)
		{
		}

		for (tmp_dm_list = dm_list; tmp_dm_list != NULL; tmp_dm_list = tmp_dm_list->next)
		{
			new_draw_info_format(NDI_UNIQUE, tmp_dm_list->objlink.ob, "%s leaves the game (%d still playing).", query_name(pl->ob, NULL), players - 1);
		}
	}

	/* If this player is in a party, leave the party */
	if (pl->party)
	{
		command_party(pl->ob, "leave");
	}

	strncpy(pl->killer, "left", MAX_BUF - 1);
	check_score(pl->ob, 1);

	/* Be sure we have closed container when we leave */
	container_unlink(pl, NULL);

	save_player(pl->ob, 0);

	if (!QUERY_FLAG(pl->ob, FLAG_REMOVED))
	{
		leave_map(pl->ob);
	}

	LOG(llevInfo, "LOGOUT: >%s< from ip %s\n", pl->ob->name, pl->socket.host);

	/* All players should be on the friendly list - remove this one */
	if (pl->ob->type == PLAYER)
	{
		remove_friendly_object(pl->ob);
	}

	if (pl->ob->map)
	{
		if (pl->ob->map->in_memory == MAP_IN_MEMORY)
		{
			pl->ob->map->timeout = MAP_TIMEOUT(pl->ob->map);
		}

		pl->ob->map = NULL;
	}

	/* To avoid problems with inventory window */
	pl->ob->type = DEAD_OBJECT;
}

/**
 * This checks the sockets for input and exceptions, does the right
 * thing.
 *
 * There are 2 lists we need to look through - init_sockets is a list */
void doeric_server()
{
	int i, pollret;
	uint32 update_below;
	struct sockaddr_in addr;
	unsigned int addrlen = sizeof(addr);
	player *pl, *next;

#ifdef CS_LOGSTATS
	if ((time(NULL) - cst_lst.time_start) >= CS_LOGTIME)
	{
		write_cs_stats();
	}
#endif

	/* Would it not be possible to use FD_CLR too and avoid the resetting
	 * every time? */
	FD_ZERO(&tmp_read);
	FD_ZERO(&tmp_write);
	FD_ZERO(&tmp_exceptions);

	for (i = 0; i < socket_info.allocated_sockets; i++)
	{
		if (init_sockets[i].status == Ns_Dead)
		{
			free_newsocket(&init_sockets[i]);
			init_sockets[i].status = Ns_Avail;
			socket_info.nconns--;
		}
		/* ns_add... */
		else if (init_sockets[i].status != Ns_Avail)
		{
			/* exclude socket #0 which listens for new connects */
			if (init_sockets[i].status > Ns_Wait)
			{
				/* kill this after 3 minutes idle... */
				if (init_sockets[i].login_count++ == 60 * 4 * (1000000 / MAX_TIME))
				{
					free_newsocket(&init_sockets[i]);
					init_sockets[i].status = Ns_Avail;
					socket_info.nconns--;

					continue;
				}
			}

			FD_SET((uint32) init_sockets[i].fd, &tmp_read);
			FD_SET((uint32) init_sockets[i].fd, &tmp_write);
			FD_SET((uint32) init_sockets[i].fd, &tmp_exceptions);
		}
	}

	/* Go through the players. Let the loop set the next pl value, since
	 * we may remove some. */
	for (pl = first_player; pl != NULL; )
	{
		if (pl->socket.status == Ns_Dead)
		{
			player *npl = pl->next;

			remove_ns_dead_player(pl);
			pl = npl;
		}
		else
		{
			FD_SET((uint32) pl->socket.fd, &tmp_read);
			FD_SET((uint32) pl->socket.fd, &tmp_write);
			FD_SET((uint32) pl->socket.fd, &tmp_exceptions);
			pl = pl->next;
		}
	}

	/* our one and only select() - after this call, every player socket
	 * has signaled us in the tmp_xxxx objects the signal status:
	 * FD_ISSET will check socket for socket for thats signal and trigger
	 * read, write or exception (error on socket). */
	pollret = select(socket_info.max_filedescriptor, &tmp_read, &tmp_write, &tmp_exceptions, &socket_info.timeout);

	if (pollret == -1)
	{
		LOG(llevDebug, "DEBUG: doeric_server(): Error on select\n");

		return;
	}

	/* Following adds a new connection */
	if (pollret && FD_ISSET(init_sockets[0].fd, &tmp_read))
	{
		int newsocknum = 0;

		LOG(llevInfo, "CONNECT from... ");

		/* If this is the case, all sockets currently in used */
		if (socket_info.allocated_sockets <= socket_info.nconns)
		{
			init_sockets = realloc(init_sockets, sizeof(NewSocket) * (socket_info.nconns + 1));

			if (!init_sockets)
			{
				LOG(llevError, "\nERROR: doeric_server(): Out of memory\n");
			}

			newsocknum = socket_info.allocated_sockets;
			socket_info.allocated_sockets++;
			init_sockets[newsocknum].status = Ns_Avail;
		}
		else
		{
			int j;

			for (j = 1; j < socket_info.allocated_sockets; j++)
			{
				if (init_sockets[j].status == Ns_Avail)
				{
					newsocknum = j;
					break;
				}
			}
		}

		init_sockets[newsocknum].fd = accept(init_sockets[0].fd, (struct sockaddr *)&addr, &addrlen);
		i = ntohl(addr.sin_addr.s_addr);

		if (init_sockets[newsocknum].fd != -1)
		{
			LOG(llevDebug, " ip %d.%d.%d.%d (add to sock #%d)\n", (i >> 24) & 255, (i >> 16) & 255, (i >> 8) & 255, i & 255, newsocknum);

			InitConnection(&init_sockets[newsocknum], i);
			socket_info.nconns++;
		}
		else
		{
			LOG(llevDebug, "Error on accept! (S#:%d IP:%d.%d.%d.%d)", newsocknum, (i >> 24) & 255, (i >> 16) & 255, (i >> 8) & 255, i & 255);
		}
	}

	/* Check for any exceptions/input on the sockets */
	if (pollret)
	{
		for (i = 1; i < socket_info.allocated_sockets; i++)
		{
			if (init_sockets[i].status == Ns_Avail)
			{
				continue;
			}

			if (FD_ISSET(init_sockets[i].fd, &tmp_exceptions))
			{
				free_newsocket(&init_sockets[i]);
				init_sockets[i].status = Ns_Avail;
				socket_info.nconns--;

				continue;
			}

			if (FD_ISSET(init_sockets[i].fd, &tmp_read))
			{
				HandleClient(&init_sockets[i], NULL);
			}

			if (init_sockets[i].status == Ns_Dead)
			{
				free_newsocket(&init_sockets[i]);
				init_sockets[i].status = Ns_Avail;
				socket_info.nconns--;
				continue;
			}

            if (FD_ISSET(init_sockets[i].fd, &tmp_write))
			{
                write_socket_buffer(&init_sockets[i]);
			}

			if (init_sockets[i].status == Ns_Dead)
			{
				free_newsocket(&init_sockets[i]);
				init_sockets[i].status = Ns_Avail;
				socket_info.nconns--;
			}

			if (FD_ISSET(init_sockets[i].fd, &tmp_write))
			{
				init_sockets[i].can_write = 1;
			}
		}
	}

	/* This does roughly the same thing, but for the players now */
	for (pl = first_player; pl != NULL; pl = next)
	{
		next = pl->next;

		/* Kill players if we have problems */
		if (pl->socket.status == Ns_Dead || FD_ISSET(pl->socket.fd, &tmp_exceptions))
		{
			remove_ns_dead_player(pl);
		}
		else
		{
			/* This will be triggered when it's POSSIBLE to read from the
			 * socket - this tells us not there is really something! */
			if (FD_ISSET(pl->socket.fd, &tmp_read))
			{
				HandleClient(&pl->socket, pl);
			}

			/* Perhaps something was bad in HandleClient(), or player has
			 * left the game */
			if (pl->socket.status == Ns_Dead)
			{
				remove_ns_dead_player(pl);
			}
			else
			{
				/* Update the players stats once per tick. More efficient
				 * than sending them whenever they change, and probably
				 * just as useful */
				esrv_update_stats(pl);

				if (pl->update_skills)
				{
					esrv_update_skills(pl);
				}

				pl->update_skills = 0;
				draw_client_map(pl->ob);

				if (pl->ob->map && (update_below = GET_MAP_UPDATE_COUNTER(pl->ob->map, pl->ob->x, pl->ob->y)) >= pl->socket.update_tile)
				{
					esrv_draw_look(pl->ob);
					pl->socket.update_tile = update_below + 1;
				}

				/* and do a quick write ... */
				if (FD_ISSET(pl->socket.fd, &tmp_write))
				{
					if (!pl->socket.can_write)
					{
						pl->socket.can_write = 1;
						write_socket_buffer(&pl->socket);
					}
				}
				else
				{
					pl->socket.can_write = 0;
				}
			}
		}
	}
}

/**
 * Write to players' sockets. */
void doeric_server_write()
{
	player *pl, *next;

	/* This does roughly the same thing, but for the players now */
	for (pl = first_player; pl != NULL; pl = next)
	{
		next = pl->next;

		/* We don't care about problems here... let remove player at start of next loop! */
		if (pl->socket.status == Ns_Dead || FD_ISSET(pl->socket.fd, &tmp_exceptions))
		{
			remove_ns_dead_player(pl);
			continue;
		}

		/* And *now* write back to player */
		if (FD_ISSET(pl->socket.fd, &tmp_write))
		{
			if (!pl->socket.can_write)
			{
				pl->socket.can_write = 1;

				write_socket_buffer(&pl->socket);
			}
			else
			{
				pl->socket.can_write = 0;
			}
		}
	}
}
