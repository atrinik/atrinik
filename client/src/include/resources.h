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
 * Resource management header file.
 *
 * @author Alex Tokar
 */

#ifndef RESOURCES_H
#define RESOURCES_H

#include <openssl/sha.h>
#include <toolkit/curl.h>

typedef struct resource {
    UT_hash_handle hh;

    char *name;

    unsigned char md[SHA512_DIGEST_LENGTH];

    unsigned char digest[SHA512_DIGEST_LENGTH * 2 + 1];

    curl_request_t *request;

    bool loaded:1;
} resource_t;

/* Function prototypes */

void
resources_init (void);
void
resources_deinit(void);
void
resources_reload(void);
resource_t *
resources_find(const char *name);
resource_t *
resources_find_by_md(const unsigned char *md);
void
socket_command_resource(uint8_t *data, size_t len, size_t pos);
bool
resources_is_ready(resource_t *resource);

#endif
