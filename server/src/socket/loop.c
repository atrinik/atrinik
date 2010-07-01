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

#ifndef WIN32
#	include <sys/types.h>
#	include <sys/time.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <netdb.h>
#endif

#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif

#ifdef HAVE_ARPA_INET_H
#	include <arpa/inet.h>
#endif

#include <newserver.h>

static fd_set tmp_read, tmp_exceptions, tmp_write;

/** Prototype for functions the client sends without player interaction. */
typedef void (*func_uint8_int_ns)(char *, int, socket_struct *);

/** Definition of a function the client sends without player interaction. */
struct client_cmd_mapping
{
	/** The command name. */
	char *cmdname;

	/** Function to call. */
	func_uint8_int_ns cmdproc;
};

/** Prototype for functions used to handle player actions. */
typedef void (*func_uint8_int_pl)(char *, int, player *);

/** Definition of a function called in reaction to player's action. */
struct player_cmd_mapping
{
	/** The command name. */
	char *cmdname;

	/** Function to call. */
	func_uint8_int_pl cmdproc;

	/** Command flags. */
	int flags;
};

/** Commands sent by the client, based on player's actions. */
static const struct player_cmd_mapping player_commands[] =
{
	{"ex",          ExamineCmd, 0},
	{"ap",          ApplyCmd, CMD_FLAG_NO_PLAYER_SHOP},
	{"mv",          MoveCmd, CMD_FLAG_NO_PLAYER_SHOP},
	{"reply",       ReplyCmd, 0},
	{"cm",          PlayerCmd, 0},
	{"lock",        (func_uint8_int_pl) LockItem, CMD_FLAG_NO_PLAYER_SHOP},
	{"mark",        (func_uint8_int_pl) MarkItem, 0},
	{"/fire",       command_fire, 0},
	{"nc",          command_new_char, 0},
	{"pt",          PartyCmd, 0},
	{"qs",          QuickSlotCmd, CMD_FLAG_NO_PLAYER_SHOP},
	{"shop",        ShopCmd, 0},
	{"qlist",       QuestListCmd, 0},
	{NULL, NULL, 0}
};

/** Commands sent directly by the client, when connecting or needed. */
static const struct client_cmd_mapping client_commands[] =
{
	{"addme",       AddMeCmd},
	{"askface",     SendFaceCmd},
	{"setup",       SetUp},
	{"version",     VersionCmd},
	{"rf",          RequestFileCmd},
	{"clr", command_clear_cmds},
	{"setsound", SetSound},
	{NULL, NULL}
};

/**
 * Used to check whether read data is a client command in fill_command_buffer().
 * @param ns Socket.
 * @return 1 if the read data is a client command, 0 otherwise. */
static int check_client_command(socket_struct *ns)
{
	unsigned char *data;
	int i, data_len;

	for (i = 0; client_commands[i].cmdname; i++)
	{
		if ((int) strlen(client_commands[i].cmdname) <= ns->inbuf.len - 2 && !strncmp((char *) ns->inbuf.buf + 2, client_commands[i].cmdname, strlen(client_commands[i].cmdname)))
		{
			/* Pre-process the command. */
			data = (unsigned char *) strchr((char *) ns->inbuf.buf + 2, ' ');

			if (data)
			{
				*data = '\0';
				data++;
				data_len = ns->inbuf.len - (data - ns->inbuf.buf);
			}
			else
			{
				data_len = 0;
			}

			client_commands[i].cmdproc((char *) data, data_len, ns);

			/* We have successfully added this client. */
			if (ns->addme)
			{
				ns->addme = 0;
			}

			return 1;
		}
	}

	return 0;
}

/**
 * Fill a command buffer with data we read from socket.
 * @param ns Socket. */
static void fill_command_buffer(socket_struct *ns)
{
	int rr;

	do
	{
		if ((rr = SockList_ReadCommand(&ns->readbuf, &ns->inbuf)))
		{
			/* Terminate buffer - useful for string data. */
			ns->inbuf.buf[ns->inbuf.len] = '\0';

			if (check_client_command(ns))
			{
				if (ns->status == Ns_Dead)
				{
					return;
				}
			}
			else
			{
				memcpy(ns->cmdbuf.buf + ns->cmdbuf.len, ns->inbuf.buf, rr);
				ns->cmdbuf.len += rr;
			}
		}
	}
	while (rr);
}

/**
 * Used to check whether parsed data is valid client or player command in handle_client().
 * @param ns Socket.
 * @param pl Player. Can be NULL, in which case player commands are not considered.
 * @return 1 if the parsed data is a valid command, 0 otherwise. */
static int check_command(socket_struct *ns, player *pl)
{
	int i, len = 0;
	unsigned char *data;

	/* Terminate buffer - useful for string data */
	ns->inbuf.buf[ns->inbuf.len] = '\0';

	/* First, break out beginning word. There are at least
	 * a few commands that do not have any parameters. If
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

	for (i = 0; client_commands[i].cmdname; i++)
	{
		if (strcmp((char *) ns->inbuf.buf + 2, client_commands[i].cmdname) == 0)
		{
			client_commands[i].cmdproc((char *) data, len, ns);
			ns->inbuf.len = 0;

			/* We have successfully added this connect! */
			if (ns->addme)
			{
				ns->addme = 0;
			}

			return 1;
		}
	}

	/* Only valid players can use these commands */
	if (pl)
	{
		for (i = 0; player_commands[i].cmdname; i++)
		{
			if (strcmp((char *) ns->inbuf.buf + 2, player_commands[i].cmdname) == 0)
			{
				if (player_commands[i].flags & CMD_FLAG_NO_PLAYER_SHOP && QUERY_FLAG(pl->ob, FLAG_PLAYER_SHOP))
				{
					new_draw_info(NDI_UNIQUE, pl->ob, "You can't do that while in player shop.");
					ns->inbuf.len = 0;
					return 1;
				}

				player_commands[i].cmdproc((char *) data, len, pl);
				ns->inbuf.len = 0;
				return 1;
			}
		}
	}

	LOG(llevDebug, "Bad command from client ('%s') (%s)\n", ns->inbuf.buf + 2, STRING_SAFE((char *) data));
	return 0;
}

/**
 * Handle client commands.
 *
 * We only get here once there is input, and only do basic connection
 * checking.
 * @param ns Socket sending the command.
 * @param pl Player associated to the socket. If NULL, only commands in
 * client_commands will be checked. */
void handle_client(socket_struct *ns, player *pl)
{
	int cmd_count = 0;

	/* Loop through this - maybe we have several complete packets here. */
	while (1)
	{
		/* If it is a player, and they don't have any speed left, we
		 * return, and will parse the data when they do have time. */
		if (ns->status == Ns_Zombie || ns->status == Ns_Dead || (pl && pl->state == ST_PLAYING && pl->ob != NULL && pl->ob->speed_left < 0))
		{
			return;
		}

		if (!SockList_ReadCommand(&ns->cmdbuf, &ns->inbuf))
		{
			return;
		}

		/* Reset idle counter. */
		if (pl && pl->state == ST_PLAYING)
		{
			ns->login_count = 0;
		}

		if (check_command(ns, pl))
		{
			if (cmd_count++ <= 8 && ns->status != Ns_Dead)
			{
				continue;
			}
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
void remove_ns_dead_player(player *pl)
{
	if (pl == NULL || pl->ob->type == DEAD_OBJECT)
	{
		return;
	}

	if (pl->state == ST_PLAYING)
	{
		/* Trigger the global LOGOUT event */
		trigger_global_event(EVENT_LOGOUT, pl->ob, pl->socket.host);

		if (!pl->dm_stealth)
		{
			new_draw_info_format(NDI_UNIQUE | NDI_ALL | NDI_DK_ORANGE, NULL, "%s left the game.", query_name(pl->ob, NULL));
		}

		/* If this player is in a party, leave the party */
		if (pl->party)
		{
			command_party(pl->ob, "leave");
		}

		strncpy(pl->killer, "left", MAX_BUF - 1);
		hiscore_check(pl->ob, 1);

		/* Be sure we have closed container when we leave */
		container_unlink(pl, NULL);

		save_player(pl->ob, 0);

		if (!QUERY_FLAG(pl->ob, FLAG_REMOVED))
		{
			leave_map(pl->ob);
		}

		if (pl->ob->map)
		{
			if (pl->ob->map->in_memory == MAP_IN_MEMORY)
			{
				pl->ob->map->timeout = MAP_TIMEOUT(pl->ob->map);
			}

			pl->ob->map = NULL;
		}
	}

	LOG(llevInfo, "LOGOUT: >%s< from IP %s\n", pl->ob->name, pl->socket.host);

	/* To avoid problems with inventory window */
	pl->ob->type = DEAD_OBJECT;
	free_player(pl);
}

/**
 * Checks if file descriptor is valid.
 * @param fd File descriptor to check.
 * @return 1 if fd is valid, 0 else. */
static int is_fd_valid(int fd)
{
#ifndef WIN32
	return fcntl(fd, F_GETFL) != -1 || errno != EBADF;
#else
	return 1;
#endif
}

/**
 * Common way to free a socket. Frees the socket from ::init_sockets,
 * sets its status to Ns_Avail and decrements number of connections. */
#define FREE_SOCKET(i) \
	free_newsocket(&init_sockets[(i)]); \
	init_sockets[(i)].status = Ns_Avail; \
	socket_info.nconns--;

/**
 * This checks the sockets for input and exceptions, does the right
 * thing.
 *
 * There are 2 lists we need to look through - init_sockets is a list */
void doeric_server()
{
	int i, pollret, rr;
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	player *pl, *next;

#if CS_LOGSTATS
	if ((time(NULL) - cst_lst.time_start) >= CS_LOGTIME)
	{
		write_cs_stats();
	}
#endif

	FD_ZERO(&tmp_read);
	FD_ZERO(&tmp_write);
	FD_ZERO(&tmp_exceptions);

	for (i = 0; i < socket_info.allocated_sockets; i++)
	{
		if (init_sockets[i].status == Ns_Add && !is_fd_valid(init_sockets[i].fd))
		{
			LOG(llevDebug, "doeric_server(): Invalid waiting fd %d\n", i);
			init_sockets[i].status = Ns_Dead;
		}

		if (init_sockets[i].status == Ns_Dead)
		{
			FREE_SOCKET(i);
		}
		else if (init_sockets[i].status == Ns_Zombie)
		{
			if (init_sockets[i].login_count++ >= 1000000 / MAX_TIME)
			{
				init_sockets[i].status = Ns_Dead;
			}
		}
		else if (init_sockets[i].status != Ns_Avail)
		{
			if (init_sockets[i].status > Ns_Wait)
			{
				/* Kill this socket after being 3 minutes idle */
				if (init_sockets[i].login_count++ >= 60 * 4 * (1000000 / MAX_TIME))
				{
					FREE_SOCKET(i);
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
		if (pl->socket.status != Ns_Dead && !is_fd_valid(pl->socket.fd))
		{
			LOG(llevDebug, "doeric_server(): Invalid file descriptor for player %s [%s]: %d\n", (pl->ob && pl->ob->name) ? pl->ob->name : "(unnamed player?)", (pl->socket.host) ? pl->socket.host : "(unknown ip?)", pl->socket.fd);
			pl->socket.status = Ns_Dead;
		}

		if (pl->socket.status == Ns_Dead)
		{
			player *npl = pl->next;

			remove_ns_dead_player(pl);
			pl = npl;
		}
		else if (pl->socket.status == Ns_Zombie)
		{
			if (pl->socket.login_count++ >= 1000000 / MAX_TIME)
			{
				pl->socket.status = Ns_Dead;
			}
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
		LOG(llevDebug, "DEBUG: doeric_server(): select failed: %s\n", strerror_local(errno));
		return;
	}

	/* Following adds a new connection */
	if (pollret && FD_ISSET(init_sockets[0].fd, &tmp_read))
	{
		int newsocknum = 0;

		/* If this is the case, all sockets are currently in use */
		if (socket_info.allocated_sockets <= socket_info.nconns)
		{
			init_sockets = realloc(init_sockets, sizeof(socket_struct) * (socket_info.nconns + 1));

			if (!init_sockets)
			{
				LOG(llevError, "ERROR: doeric_server(): Out of memory\n");
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

		init_sockets[newsocknum].fd = accept(init_sockets[0].fd, (struct sockaddr *) &addr, &addrlen);

		if (init_sockets[newsocknum].fd == -1)
		{
			LOG(llevDebug, "doeric_server(): accept failed: %s\n", strerror_local(errno));
		}
		else
		{
			char buf[MAX_BUF];
			long ip = ntohl(addr.sin_addr.s_addr);

			snprintf(buf, sizeof(buf), "%ld.%ld.%ld.%ld", (ip >> 24) & 255, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255);

			if (checkbanned(NULL, buf))
			{
				LOG(llevInfo, "BAN: Banned IP tried to connect. [%s]\n", buf);
#ifndef WIN32
				close(init_sockets[newsocknum].fd);
#else
				shutdown(init_sockets[newsocknum].fd, SD_BOTH);
				closesocket(init_sockets[newsocknum].fd);
#endif
				init_sockets[newsocknum].fd = -1;
			}
			else
			{
				init_connection(&init_sockets[newsocknum], buf);
				socket_info.nconns++;
			}
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
				FREE_SOCKET(i);
				continue;
			}

			if (FD_ISSET(init_sockets[i].fd, &tmp_read))
			{
				rr = SockList_ReadPacket(&init_sockets[i], MAXSOCKBUF_IN - 1);

				if (rr < 0)
				{
					LOG(llevDebug, "Drop connection: %s\n", STRING_SAFE(init_sockets[i].host));
					init_sockets[i].status = Ns_Dead;
				}
				else
				{
					fill_command_buffer(&init_sockets[i]);
				}
			}

			if (init_sockets[i].status == Ns_Avail)
			{
				continue;
			}

			if (init_sockets[i].status == Ns_Dead)
			{
				FREE_SOCKET(i);
				continue;
			}

			if (FD_ISSET(init_sockets[i].fd, &tmp_write))
			{
				socket_buffer_write(&init_sockets[i]);
			}

			if (init_sockets[i].status == Ns_Dead)
			{
				FREE_SOCKET(i);
			}
		}
	}

	/* This does roughly the same thing, but for the players now */
	for (pl = first_player; pl; pl = next)
	{
		next = pl->next;

		/* Kill players if we have problems */
		if (pl->socket.status == Ns_Dead || FD_ISSET(pl->socket.fd, &tmp_exceptions))
		{
			remove_ns_dead_player(pl);
			continue;
		}

		if (FD_ISSET(pl->socket.fd, &tmp_read))
		{
			rr = SockList_ReadPacket(&pl->socket, MAXSOCKBUF_IN - 1);

			if (rr < 0)
			{
				LOG(llevDebug, "Drop connection: %s (%s)\n", STRING_OBJ_NAME(pl->ob), STRING_SAFE(pl->socket.host));
				pl->socket.status = Ns_Dead;
			}
			else
			{
				fill_command_buffer(&pl->socket);
			}
		}

		/* Perhaps something was bad in handle_client(), or player has
		 * left the game. */
		if (pl->socket.status == Ns_Dead)
		{
			remove_ns_dead_player(pl);
			continue;
		}

		if (FD_ISSET(pl->socket.fd, &tmp_write))
		{
			socket_buffer_write(&pl->socket);
		}
	}
}

/**
 * Write to players' sockets. */
void doeric_server_write()
{
	player *pl, *next;
	uint32 update_below;

	/* This does roughly the same thing, but for the players now */
	for (pl = first_player; pl; pl = next)
	{
		next = pl->next;

		if (pl->socket.status == Ns_Dead || FD_ISSET(pl->socket.fd, &tmp_exceptions))
		{
			remove_ns_dead_player(pl);
			continue;
		}

		esrv_update_stats(pl);

		if (pl->update_skills)
		{
			esrv_update_skills(pl);
			pl->update_skills = 0;
		}

		draw_client_map(pl->ob);

		if (pl->ob->map && (update_below = GET_MAP_UPDATE_COUNTER(pl->ob->map, pl->ob->x, pl->ob->y)) >= pl->socket.update_tile)
		{
			esrv_draw_look(pl->ob);
			pl->socket.update_tile = update_below + 1;
		}

		if (FD_ISSET(pl->socket.fd, &tmp_write))
		{
			socket_buffer_write(&pl->socket);
		}
	}
}
