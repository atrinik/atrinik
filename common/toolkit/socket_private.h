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
 ************************************************************************/

/**
 * @file
 * Private socket implementation declarations.
 */

#ifndef TOOLKIT_SOCKET_PRIVATE_H
#define TOOLKIT_SOCKET_PRIVATE_H

#include "socket.h"

#include <openssl/opensslv.h>
#if OPENSSL_VERSION_NUMBER >= 0x30500000L
#include <openssl/ssl.h>
#endif

struct sock_struct {
    enum {
        SOCKET_TRANSPORT_TCP,
        SOCKET_TRANSPORT_QUIC_LISTENER,
        SOCKET_TRANSPORT_QUIC_CONNECTION,
    } transport;

    int handle;
    struct sockaddr_storage addr;
    char *host;
    uint16_t port;
    socket_crypto_t *crypto;
    bool secure:1;
    socket_role_t role;
    socket_connection_mode_t connection_mode;

#if OPENSSL_VERSION_NUMBER >= 0x30500000L
    SSL_CTX *quic_ctx;
    SSL *quic;
#endif

    /** Whether this object owns and must close handle. */
    bool owns_handle:1;
};

size_t
socket_rendezvous_client(socket_t                    *sc,
                         const char                  *url,
                         const char                  *stun_endpoint,
                         socket_direct_candidate_t  *candidates,
                         size_t                       capacity);

#endif
