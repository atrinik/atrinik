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
 * Header file related to monster data.
 *
 * @author Alex Tokar
 */

#ifndef MONSTER_DATA_H
#define MONSTER_DATA_H

/**
 * How often to clean up stale dialog entries in the monster's database,
 * in seconds (every X seconds).
 */
#define MONSTER_DATA_INTERFACE_CLEANUP 10
/**
 * Base number of seconds before a dialog is considered as stale. This is
 * mostly a protection against malicious clients.
 */
#define MONSTER_DATA_INTERFACE_TIMEOUT 60 * 15
/**
 * Maximum distance the activator can be (from the monster) before their
 * dialog is considered stale.
 *
 * @todo This should really be a define somewhere else; we need a constant here
 * only because socket_command_talk() does not define one and simply uses
 * ::SIZEOFFREE.
 */
#define MONSTER_DATA_INTERFACE_DISTANCE 2

/**
 * Structure that holds information about dialogs the monster has open.
 */
typedef struct monster_data_dialog {
    struct monster_data_dialog *next; ///< Next dialog.
    struct monster_data_dialog *prev; ///< Previous dialog.

    object *ob; ///< Object that is talking to the monster.
    tag_t count; ///< ID of the object.

    /**
     * When the dialog expires and is considered stale (in ticks).

 */
    long expire;
} monster_data_dialog_t;

/**
 * Structure that holds monster data, ie, the monster's brain.
 */
typedef struct monster_data {
    /**
     * Last coordinates the monster's enemy was spotted at.

 */
    struct {
        uint16_t x; ///< X.
        uint16_t y; ///< Y.
        mapstruct *map; ///< The map.
    } enemy_coords;

    /**
     * All the conversations the NPC is having.

 */
    monster_data_dialog_t *dialogs;

    /**
     * Last time a cleanup was performed.

 */
    long last_cleanup;
} monster_data_t;

/**
 * Acquire monster data structure from an object structure.
 */
#define MONSTER_DATA(_obj) ((monster_data_t *) (_obj)->custom_attrset)

/* Prototypes */

void monster_data_init(object *op);
void monster_data_deinit(object *op);
void monster_data_enemy_update(object *op, object *enemy);
bool monster_data_enemy_get_coords(object *op, mapstruct **map, uint16_t *x,
        uint16_t *y);
void monster_data_dialogs_add(object *op, object *activator,
        uint32_t secs);
void monster_data_dialogs_remove(object *op, object *activator);
bool monster_data_dialogs_check(object *op, object *activator);
size_t monster_data_dialogs_num(object *op);
void monster_data_dialogs_cleanup(object *op);
void monster_data_dialogs_purge(object *op);

#endif
