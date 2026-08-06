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
 * Commands header file.
 */

#ifndef COMMANDS_H
#define COMMANDS_H

/**
 * @defgroup SPLIST_MODE_xxx SPLIST_MODE_xxx
 * Spell list commands for client's spell list.
 *@{*/
/** Remove existing spell. */
#define SPLIST_MODE_REMOVE 1
/*@}*/

/** Public API implemented in src/client/cmd_aliases.c. */

extern void cmd_aliases_init(void);

extern void cmd_aliases_deinit(void);

extern int cmd_aliases_handle(const char *cmd);

/** Public API implemented in src/client/commands.c. */

extern void socket_command_book(uint8_t *data, size_t len, size_t pos);

extern void socket_command_setup(uint8_t *data, size_t len, size_t pos);

extern void socket_command_anim(uint8_t *data, size_t len, size_t pos);

extern void socket_command_image(uint8_t *data, size_t len, size_t pos);

extern void socket_command_drawinfo(uint8_t *data, size_t len, size_t pos);

extern void socket_command_target(uint8_t *data, size_t len, size_t pos);

extern void socket_command_stats(uint8_t *data, size_t len, size_t pos);

extern void socket_command_player(uint8_t *data, size_t len, size_t pos);

extern void command_item_update(struct packet_reader *reader, uint32_t flags, object *tmp);

extern void socket_command_item(uint8_t *data, size_t len, size_t pos);

extern void socket_command_item_update(uint8_t *data, size_t len, size_t pos);

extern void socket_command_item_delete(uint8_t *data, size_t len, size_t pos);

extern void socket_command_mapstats(uint8_t *data, size_t len, size_t pos);

extern void socket_command_map(uint8_t *data, size_t len, size_t pos);

extern void socket_command_version(uint8_t *data, size_t len, size_t pos);

extern void socket_command_compressed(uint8_t *data, size_t len, size_t pos);

extern void socket_command_control(uint8_t *data, size_t len, size_t pos);

#endif
