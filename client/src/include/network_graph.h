/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
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
 * Network graph widget header file.
 *
 * @author Alex Tokar
 */

#ifndef NETWORK_GRAPH_H
#define NETWORK_GRAPH_H

enum {
    NETWORK_GRAPH_TYPE_GAME, ///< Game data.
    NETWORK_GRAPH_TYPE_HTTP, ///< HTTP data.

    NETWORK_GRAPH_TYPE_MAX ///< Maximum number of data types.
};

enum {
    NETWORK_GRAPH_TRAFFIC_RX, ///< Incoming traffic.
    NETWORK_GRAPH_TRAFFIC_TX, ///< Outgoing traffic.

    NETWORK_GRAPH_TRAFFIC_MAX ///< Number of traffic types.
};

/* Prototypes */

void network_graph_update(int type, int traffic, size_t bytes);
void widget_network_graph_init(widgetdata *widget);

#endif
