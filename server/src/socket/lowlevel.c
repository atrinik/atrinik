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
 * Low level socket related functions. */

#include <global.h>
#include <newclient.h>
#include <sproto.h>

#ifdef NO_ERRNO_H
extern int errno;
#else
#   include <errno.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

/**
 * Add a NULL terminated string.
 * @param sl SockList instance to add to.
 * @param data The string to add. */
void SockList_AddString(SockList *sl, char *data)
{
	char c;

	while ((c = *data++))
	{
		sl->buf[sl->len] = c;
		sl->len++;
	}

	sl->buf[sl->len] = c;
	sl->len++;
}

/**
 * Reads from file descriptor and puts the data in a SockList instance.
 *
 * The only processing we do is remove the initial size value.
 *
 * We make the assumption the buffer is at least 2 bytes long.
 * @param fd The file descriptor
 * @param sl SockList instance
 * @param len Size of the buffer allocated in the SockList.
 * @return 1 if we think we have a full packet, 0 if we have a partial
 * packet. */
int SockList_ReadPacket(int fd, SockList *sl, int len)
{
	int stat, toread;

	/* Sanity check - shouldn't happen */
	if (sl->len < 0)
	{
		LOG(llevDebug, "FATAL SOCKET ERROR: sl->len < 0 (%d)\n", sl->len);
	}

	/* We already have a partial packet */
	if (sl->len < 2)
	{
#ifdef WIN32
		stat = recv(fd, sl->buf + sl->len, 2 - sl->len, 0);
#else
		do
		{
			stat = read(fd, sl->buf + sl->len, 2 - sl->len);
		}
		while ((stat == -1) && (errno == EINTR));
#endif

		if (stat < 0)
		{
			/* EAGAIN is set in non blocking mode when no data available. */
#ifdef WIN32
			if ((stat == -1) && WSAGetLastError() != WSAEWOULDBLOCK)
			{
				if (WSAGetLastError() == WSAECONNRESET)
				{
					LOG(llevDebug, "Connection closed by client\n");
				}
				else
				{
					LOG(llevDebug, "ReadPacket got error %d, returning 0\n", WSAGetLastError());
				}

				return -1;
			}
#else
			if (errno != EAGAIN && errno != EWOULDBLOCK)
			{
				LOG(llevDebug, "ReadPacket got error %d, returning 0\n", errno);

				return -1;
			}
#endif

			/* Error */
			return 0;
		}

		if (stat == 0)
		{
			return -1;
		}

		sl->len += stat;

#if CS_LOGSTATS
		cst_tot.ibytes += stat;
		cst_lst.ibytes += stat;
#endif

		/* Still don't have a full packet */
		if (stat < 2)
		{
			return 0;
		}
	}

	/* Figure out how much more data we need to read.  Add 2 from the end
	 * of this - size header information is not included. */
	toread = 2 + (sl->buf[0] << 8) + sl->buf[1] - sl->len;

	if ((toread + sl->len) >= len)
	{
		LOG(llevDebug, "SockList_ReadPacket: Want to read more bytes than will fit in buffer (%d>=%d).\n", toread + sl->len, len);

		/* Return error so the socket is closed */
		return -1;
	}

	do
	{
#ifdef WIN32
		stat = recv(fd, sl->buf + sl->len, toread, 0);
#else
		do
		{
			stat = read(fd, sl->buf + sl->len, toread);
		}
		while ((stat < 0) && (errno == EINTR));
#endif

		if (stat < 0)
		{
#ifdef WIN32
			if ((stat == -1) && WSAGetLastError() != WSAEWOULDBLOCK)
			{
				if (WSAGetLastError() == WSAECONNRESET)
				{
					LOG(llevDebug, "Connection closed by client\n");
				}
				else
				{
					LOG(llevDebug, "ReadPacket got error %d, returning 0\n", WSAGetLastError());
				}

				return -1;
			}
#else
			if (errno != EAGAIN && errno != EWOULDBLOCK)
			{
				LOG(llevDebug, "ReadPacket got error %d, returning 0\n", errno);

				return -1;
			}
#endif

			/* Error */
			return 0;
		}

		if (stat == 0)
		{
			return -1;
		}

		sl->len += stat;

#if CS_LOGSTATS
		cst_tot.ibytes += stat;
		cst_lst.ibytes += stat;
#endif

		toread -= stat;

		if (toread == 0)
		{
			return 1;
		}

		if (toread < 0)
		{
			LOG(llevDebug, "SockList_ReadPacket: Read more bytes than desired.\n");

			return 1;
		}
	}
	while (toread > 0);

	return 0;
}

/**
 * Adds data to a socket buffer for whatever reason.
 * @param ns The socket we are adding the data to.
 * @param buf Start of the data.
 * @param len Number of bytes to add. */
static void add_to_buffer(NewSocket *ns, unsigned char *buf, int len)
{
	int avail, end;

	if ((len + ns->outputbuffer.len) > MAXSOCKBUF)
	{
		LOG(llevDebug, "Socket on fd %d has overrun internal buffer - marking as dead\n", ns->fd);
		ns->status = Ns_Dead;

		return;
	}

	/* data + end is where we start putting the new data.  The last byte
	 * currently in use is actually data + end -1 */

	end = ns->outputbuffer.start + ns->outputbuffer.len;

	/* The buffer is already in a wrapped state, so adjust end */
	if (end >= MAXSOCKBUF)
	{
		end -= MAXSOCKBUF;
	}

	avail = MAXSOCKBUF - end;

	/* We can all fit it behind the current data without wrapping */
	if (avail >= len)
	{
		memcpy(ns->outputbuffer.data + end, buf, len);
	}
	else
	{
		memcpy(ns->outputbuffer.data + end, buf, avail);
		memcpy(ns->outputbuffer.data, buf+avail, len - avail);
	}

	ns->outputbuffer.len += len;

#if 0
	LOG(llevDebug, "Added %d to output buffer, total length now %d, start=%d\n", len, ns->outputbuffer.len, ns->outputbuffer.start);
#endif
}

/**
 * Write data to socket.
 *
 * When the socket is clear to write, and we have backlogged data, this
 * is called to write it out.
 * @param ns The socket we are writing to. */
void write_socket_buffer(NewSocket *ns)
{
	int amt, max;

	if (ns->outputbuffer.len == 0)
	{
		return;
	}

	do
	{
		max = MAXSOCKBUF - ns->outputbuffer.start;

		if (ns->outputbuffer.len < max)
		{
			max = ns->outputbuffer.len;
		}

		amt = send(ns->fd, ns->outputbuffer.data + ns->outputbuffer.start, max, MSG_DONTWAIT);

		/* We got an error */
		if (amt < 0)
		{
#ifdef WIN32
			if (amt == -1 && WSAGetLastError() != WSAEWOULDBLOCK)
			{
				LOG(llevDebug, "New socket write failed (wsb) (%d).\n", WSAGetLastError());
#else
			if (errno != EWOULDBLOCK)
			{
				LOG(llevDebug, "New socket write failed (wsb) (%d: %s).\n", errno, strerror_local(errno));
#endif

				ns->status = Ns_Dead;

				return;
			}
			else
			{
				/* Can't write it, so store it away. */
				ns->can_write = 0;

				return;
			}
		}

		ns->outputbuffer.start += amt;

		/* wrap back to start of buffer */
		if (ns->outputbuffer.start == MAXSOCKBUF)
		{
			ns->outputbuffer.start = 0;
		}

		ns->outputbuffer.len -= amt;

#if CS_LOGSTATS
		cst_tot.obytes += amt;
		cst_lst.obytes += amt;
#endif
	}
	while (ns->outputbuffer.len > 0);
}

/**
 * This writes data to the socket.
 *
 * It is very low level - all we try to do is write out the data to the
 * socket provided.
 *
 * The function itself doesn't return anything - rather, it updates the
 * ns structure if we get an error.
 * @param ns The socket to write to
 * @param buf Data to write
 * @param len Number of bytes to write */
void Write_To_Socket(NewSocket *ns, unsigned char *buf, int len)
{
	int amt = 0;
	unsigned char *pos = buf;

	if (ns->status == Ns_Dead || !buf)
	{
		LOG(llevDebug, "Write_To_Socket called with dead socket\n");
		return;
	}

	if (!ns->can_write)
	{
		add_to_buffer(ns, buf, len);
		return;
	}

	/* If we manage to write more than we wanted, take it as a bonus */
	while (len > 0)
	{
		amt = send(ns->fd, pos, len, MSG_DONTWAIT);

		/* We got an error */
		if (amt < 0)
		{
#ifdef WIN32
			if (amt == -1 && WSAGetLastError() != WSAEWOULDBLOCK)
			{
				LOG(llevDebug, "New socket write failed WTS (%d).\n", WSAGetLastError());
#else
			if (errno != EWOULDBLOCK)
			{
				LOG(llevDebug, "New socket write failed WTS (%d: %s).\n", errno, strerror_local(errno));
#endif

				ns->status = Ns_Dead;
				return;
			}
			else
			{
				/* Can't write it, so store it away. */
				add_to_buffer(ns, pos, len);
				ns->can_write = 0;

				return;
			}
		}
		/* amt gets set to 0 above in blocking code, so we do this as
		 * an else if to make sure we don't reprocess it. */
		else if (amt == 0)
		{
			LOG(llevDebug, "Write_To_Socket: No data written out.\n");
		}

		len -= amt;
		pos += amt;

#if CS_LOGSTATS
		cst_tot.obytes += amt;
		cst_lst.obytes += amt;
#endif
	}
}

/**
 * Calls Write_To_Socket to send data to the client.
 *
 * The only difference in this function is that we take a SockList, and
 * we prepend the length information.
 * @param ns Socket to send the data to
 * @param msg The SockList instance */
void Send_With_Handling(NewSocket *ns, SockList *msg)
{
	unsigned char sbuf[4];

	if (ns->status == Ns_Dead || !msg)
	{
		return;
	}

	/* Almost certainly we've overflowed a buffer, so quit now to make
	 * it easier to debug. */
	if (msg->len >= MAXSOCKBUF)
	{
		LOG(llevError, "Trying to send a buffer beyond properly size, len =%d\n", msg->len);
	}

	/* If more than 32kb use 3 bytes header and set the high bit to show
	 * it to the client. */
	if (msg->len > 32 * 1024 - 1)
	{
		sbuf[0] = ((uint32) (msg->len) >> 16) & 0xFF;
		/* High bit marker for the client */
		sbuf[0] |= 0x80;
		sbuf[1] = ((uint32) (msg->len) >> 8) & 0xFF;
		sbuf[2] = ((uint32) (msg->len)) & 0xFF;

		Write_To_Socket(ns, sbuf, 3);
	}
	else
	{
		sbuf[0] = ((uint32) (msg->len) >> 8) & 0xFF;
		sbuf[1] = ((uint32) (msg->len)) & 0xFF;

		Write_To_Socket(ns, sbuf, 2);
	}

	Write_To_Socket(ns, msg->buf, msg->len);
}

/**
 * Takes a string of data, and writes it out to the socket. A very handy
 * shortcut function. */
void Write_String_To_Socket(NewSocket *ns, char cmd, char *buf, int len)
{
	SockList sl;

	sl.len = len;
	sl.buf = (uint8 *) buf;
	*((char *) buf) = cmd;

	Send_With_Handling(ns, &sl);
}

#if CS_LOGSTATS

/** Life of the server. */
CS_Stats cst_tot;

/** Last series of stats. */
CS_Stats cst_lst;

/**
 * Writes out the gathered stats.
 *
 * We clear ::cst_lst. */
void write_cs_stats()
{
	time_t now = time(NULL);

	/* If no connections recently, don't bother to log anything */
	if (cst_lst.ibytes == 0 && cst_lst.obytes == 0)
	{
		return;
	}

	/* CSSTAT is put in so scripts can easily find the line */
	LOG(llevInfo, "CSSTAT: %.16s tot in:%d out:%d maxc:%d time:%d last block-> in:%d out:%d maxc:%d time:%d\n", ctime(&now), cst_tot.ibytes, cst_tot.obytes, cst_tot.max_conn, now - cst_tot.time_start, cst_lst.ibytes, cst_lst.obytes, cst_lst.max_conn, now - cst_lst.time_start);

	LOG(llevInfo, "SYSINFO: objs: %d used, %d free, arch-srh:%d (%d cmp)\n", mempools[POOL_OBJECT].nrof_used, mempools[POOL_OBJECT].nrof_free, arch_search, arch_cmp);

	cst_lst.ibytes = 0;
	cst_lst.obytes = 0;
	cst_lst.max_conn = socket_info.nconns;
	cst_lst.time_start = now;
}
#endif
