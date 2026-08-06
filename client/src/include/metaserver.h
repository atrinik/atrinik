/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
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

#ifndef METASERVER_H
#define METASERVER_H

/**
 * @file
 * Public declarations for the corresponding client module.
 */

/** Public API implemented in src/client/metaserver.c. */

extern void metaserver_init(void);

extern void metaserver_disable(void);

extern server_struct *server_get_id(size_t num);

bool metaserver_rendezvous_url(const server_struct *server, char *url, size_t url_size);

extern size_t server_get_count(void);

extern int ms_connecting(int val);

extern void metaserver_clear_data(void);

extern void metaserver_deinit(void);

extern server_struct *metaserver_add(const char *hostname,
                                     int port,
                                     const char *name,
                                     const char *version,
                                     const char *desc);

extern int metaserver_thread(void *dummy);

extern void metaserver_get_servers(void);

#endif
