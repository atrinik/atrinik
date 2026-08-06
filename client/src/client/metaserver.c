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
 ************************************************************************/

/**
 * @file
 * Handles the QUIC server directory and its client-side server list.
 */

#include <global.h>
#include <metaserver.h>
#include <openssl/crypto.h>
#include <toolkit/curl.h>
#include "metaserver_private.h"

/** Are we connecting to the metaserver? */
static int metaserver_connecting = 1;
/** Mutex to protect ::metaserver_connecting. */
static SDL_mutex *metaserver_connecting_mutex;
/** The list of the servers. */
static server_struct *server_head;
/** Number of the servers. */
static size_t server_count;
/** Mutex to protect ::server_head and ::server_count. */
static SDL_mutex *server_head_mutex;
/** Is metaserver enabled? */
static uint8_t enabled = 1;

void metaserver_init(void) {
    server_head = NULL;
    server_count = 0;
    metaserver_connecting_mutex = SDL_CreateMutex();
    server_head_mutex = SDL_CreateMutex();
}

void metaserver_disable(void) {
    enabled = 0;
    metaserver_connecting = 0;
}

void metaserver_server_free(server_struct *server) {
    HARD_ASSERT(server != NULL);

    free(server->hostname);
    free(server->server_id);
    free(server->quic_certificate_sha256);
    free(server->rendezvous_origin);
    if (server->join_password != NULL) {
        OPENSSL_cleanse(server->join_password, strlen(server->join_password));
        free(server->join_password);
    }
    free(server->name);
    free(server->version);
    free(server->desc);
    free(server);
}

void metaserver_server_add(server_struct *server) {
    HARD_ASSERT(server != NULL);

    SDL_LockMutex(server_head_mutex);
    DL_PREPEND(server_head, server);
    server_count++;
    SDL_UnlockMutex(server_head_mutex);
}

bool metaserver_rendezvous_url(const server_struct *server, char *url, size_t url_size) {
    if (server == NULL || server->server_id == NULL || server->rendezvous_origin == NULL) {
        return false;
    }
    return socket_rendezvous_url(server->rendezvous_origin,
                                 server->server_id,
                                 "client",
                                 url,
                                 url_size);
}

server_struct *server_get_id(size_t num) {
    server_struct *node;
    size_t i;

    SDL_LockMutex(server_head_mutex);
    for (node = server_head, i = 0; node; node = node->next, i++) {
        if (i == num) {
            break;
        }
    }
    SDL_UnlockMutex(server_head_mutex);
    return node;
}

size_t server_get_count(void) {
    SDL_LockMutex(server_head_mutex);
    size_t count = server_count;
    SDL_UnlockMutex(server_head_mutex);
    return count;
}

int ms_connecting(int val) {
    SDL_LockMutex(metaserver_connecting_mutex);
    int connecting = metaserver_connecting;
    if (val != -1) {
        metaserver_connecting = val;
    }
    SDL_UnlockMutex(metaserver_connecting_mutex);
    return connecting;
}

void metaserver_clear_data(void) {
    SDL_LockMutex(server_head_mutex);
    server_struct *node, *tmp;
    DL_FOREACH_SAFE(server_head, node, tmp) {
        DL_DELETE(server_head, node);
        metaserver_server_free(node);
    }
    server_count = 0;
    SDL_UnlockMutex(server_head_mutex);
}

server_struct *metaserver_add(const char *hostname,
                              int port,
                              const char *name,
                              const char *version,
                              const char *desc) {
    server_struct *node = xcalloc(1, sizeof(*node));
    node->player = -1;
    node->port = port;
    node->hostname = xstrdup(hostname);
    node->name = xstrdup(name);
    node->version = xstrdup(version);
    node->desc = xstrdup(desc);

    metaserver_server_add(node);
    return node;
}

int metaserver_thread(void *dummy) {
    (void)dummy;

    for (size_t i = clioption_settings.metaservers_num; i > 0; i--) {
        char direct_url[MAX_BUF];
        metaserver_direct_url(clioption_settings.metaservers[i - 1], VS(direct_url));
        if (*direct_url == '\0') {
            continue;
        }

        curl_request_t *request = curl_request_create(direct_url, CURL_PKEY_TRUST_SYSTEM);
        curl_request_do_get(request);
        size_t body_size;
        char *body = curl_request_get_body(request, &body_size);
        bool parsed =
            curl_request_get_http_code(request) == 200 && body != NULL &&
            metaserver_direct_parse(body, body_size, clioption_settings.metaservers[i - 1]);
        curl_request_free(request);
        if (parsed) {
            break;
        }
    }

    SDL_LockMutex(metaserver_connecting_mutex);
    metaserver_connecting = 0;
    SDL_UnlockMutex(metaserver_connecting_mutex);
    return 0;
}

void metaserver_get_servers(void) {
    if (!enabled) {
        return;
    }

    SDL_LockMutex(metaserver_connecting_mutex);
    metaserver_connecting = 1;
    SDL_UnlockMutex(metaserver_connecting_mutex);

    SDL_Thread *thread = SDL_CreateThread(metaserver_thread, NULL);
    if (thread == NULL) {
        LOG(ERROR, "Thread creation failed.");
        exit(1);
    }
}
