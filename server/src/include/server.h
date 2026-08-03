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

/**
 * @file
 * Socket server header file.
 */

#ifndef SOCKET_SERVER_H
#define SOCKET_SERVER_H

#include <toolkit/toolkit.h>

/**
 * Function to call when a specific command type is received.
 *
 * @param cs
 * Client socket.
 * @param pl
 * Player associated with the client. May be NULL if the client has not
 * logged in yet.
 * @param data
 * Network data buffer that contains the command.
 * @param len
 * Number of bytes in the command.
 * @param pos
 * Position where to start parsing command-specific data.
 */
typedef void (
    *socket_command_func)(socket_struct *cs, player *pl, uint8_t *data, size_t len, size_t pos);

/* Prototypes */

TOOLKIT_FUNCS_DECLARE(socket_server);

void socket_server_handle_client(player *pl);
bool socket_server_remove(socket_struct *cs);
void socket_server_process(void);
void socket_server_post_process(void);
bool socket_server_quic_info(char *host,
                             size_t host_size,
                             uint16_t *port,
                             char certificate_sha256[65]);
bool socket_server_quic_punch(const char *host, uint16_t port);
size_t socket_server_quic_candidates(socket_direct_candidate_t *candidates, size_t capacity);
bool socket_port_mapping_init(uint16_t port, char *host, size_t host_size, uint16_t *external_port);
void socket_port_mapping_process(void);
void socket_port_mapping_deinit(void);

#endif
