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
 * Socket crypto header file.
 *
 * @author Alex Tokar
 */

#ifndef SOCKET_CRYPTO_H
#define SOCKET_CRYPTO_H

#include <toolkit.h>

/** Opaque typedef for the ::socket_crypto structure. */
typedef struct socket_crypto socket_crypto_t;

/* Prototypes */

TOOLKIT_FUNCS_DECLARE(socket_crypto);

bool
socket_crypto_enabled(void);
bool
socket_crypto_has_curves(void);
bool
socket_crypto_curve_supported(const char *name, int *nid);
bool
socket_crypto_create(socket_t *sc, int nid);
void
socket_crypto_destroy(socket_crypto_t *crypto);

#endif
