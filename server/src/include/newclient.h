/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Crossfire (Multiplayer game for X-windows).                 *
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
 * This file defines various flags that both the new client and newserver
 * uses.
 *
 * These should never be changed, only expanded. Changing them will
 * likely cause all old clients to not work properly. While called
 * newclient, it is really used by both the client and server to keep
 * some values the same.
 *
 * Name format is CS_(command)_(flag)
 *
 * CS = Client/Server.
 *
 * (command) is protocol command, ie ITEM
 *
 * (flag) is the flag name */

#ifndef NEWCLIENT_H
#define NEWCLIENT_H

/** Srv client files. */
typedef struct _srv_client_files
{
    /** Compressed file data. */
    uint8 *file;

    /** Compressed file length. */
    size_t len;

    /** Original uncompressed file length */
    size_t len_ucomp;

    /** CRC32 sum. */
    unsigned long crc;
} _srv_client_files;

#endif
