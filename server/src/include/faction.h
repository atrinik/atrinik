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
 * Structures and prototypes for the faction code.
 *
 * @author Alex Tokar
 */

#ifndef FACTION_H
#define FACTION_H

/**
 * Pointer to a faction structure.
 */
typedef struct faction * faction_t;

/* Prototypes */

void toolkit_faction_init(void);
void toolkit_faction_deinit(void);
faction_t faction_find(shstr *name);
void faction_update(faction_t faction, player *pl, double reputation);
void faction_update_kill(faction_t faction, player *pl);
bool faction_is_friend(faction_t faction, object *op);
bool faction_is_alliance(faction_t faction, faction_t faction2);
double faction_get_bounty(faction_t faction, player *pl);
void faction_clear_bounty(faction_t faction, player *pl);

#endif
