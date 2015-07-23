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
 * Ban API header file.
 *
 * @author Alex Tokar
 */

#ifndef BAN_H
#define	BAN_H

/**
 * Used as return codes for ban API functions.
 *
 * Use ban_strerror() to get a string representation of returned error codes.
 */
typedef enum ban_error {
    BAN_OK, ///< Success.
    BAN_EXIST, ///< Ban entry exists.
    BAN_NOTEXIST, ///< Ban entry doesn't exist.
    BAN_REMOVED, ///< Ban was already removed.
    BAN_BADID, ///< Bad ban ID.
    BAN_BADIP, ///< Invalid IP address.
    BAN_BADPLEN, ///< Invalid prefix length.
    BAN_BADSYNTAX, ///< Invalid syntax.

    BAN_MAX ///< Number of ban error codes.
} ban_error_t;

/* Prototypes */
void toolkit_ban_init(void);
void toolkit_ban_deinit(void);
ban_error_t ban_add(const char *str);
ban_error_t ban_remove(const char *str);
bool ban_check(socket_struct *ns, const char *name);
void ban_list(object *op);
const char *ban_strerror(ban_error_t errnum);

#endif
