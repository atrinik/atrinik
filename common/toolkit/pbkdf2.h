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
 * PBKDF2 API header file.
 *
 * @author Alex Tokar
 */

#ifndef TOOLKIT_PBKDF2_H
#define TOOLKIT_PBKDF2_H

#include "toolkit.h"

/* Prototypes */

TOOLKIT_FUNCS_DECLARE(pbkdf2);

extern void
PKCS5_PBKDF2_HMAC_SHA2(const unsigned char *password,
                       size_t               plen,
                       unsigned char       *salt,
                       size_t               slen,
                       const unsigned long  iteration_count,
                       const unsigned long  key_length,
                       unsigned char       *output);

#endif
