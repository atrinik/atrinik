/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 ************************************************************************/

/**
 * @file
 * Private metaserver implementation declarations.
 */

#ifndef CLIENT_METASERVER_PRIVATE_H
#define CLIENT_METASERVER_PRIVATE_H

#include <global.h>

void
metaserver_server_add(server_struct *server);

void
metaserver_server_free(server_struct *server);

bool
metaserver_direct_parse(const char *body,
                        size_t      body_size,
                        const char *origin);

void
metaserver_direct_url(const char *legacy_url, char *url, size_t url_size);

#endif
