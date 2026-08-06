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
 * Persistent per-server direct connection preferences.
 */

#include <global.h>
#include <wrapper.h>
#include <connection_preferences.h>
#include <toolkit/memory.h>
#include <toolkit/path.h>
#include <toolkit/string.h>

#define FILE_CONNECTION_PREFERENCES "settings/connection-preferences.dat"

typedef struct connection_preference_entry {
    struct connection_preference_entry *next;
    struct connection_preference_entry *prev;
    char *key;
    socket_connection_preference_t preference;
} connection_preference_entry_t;

static connection_preference_entry_t *connection_preferences;

static const char *const preference_keys[SOCKET_CONNECTION_PREFERENCE_NUM] =
    {"automatic", "lan", "ipv6", "mapped", "stun", "directory"};

static bool connection_preference_key(const server_struct *server, char *key, size_t key_size) {
    HARD_ASSERT(server != NULL);
    HARD_ASSERT(key != NULL);

    int written;
    if (!string_isempty(server->server_id)) {
        written = snprintf(key, key_size, "id:%s", server->server_id);
        return written >= 0 && (size_t)written < key_size;
    }
    if (string_isempty(server->hostname)) {
        return false;
    }
    written = snprintf(key, key_size, "address:%s:%d", server->hostname, server->port);
    return written >= 0 && (size_t)written < key_size;
}

static connection_preference_entry_t *connection_preference_find(const char *key) {
    connection_preference_entry_t *entry;
    DL_FOREACH(connection_preferences, entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry;
        }
    }
    return NULL;
}

static void connection_preferences_save(void) {
    StringBuffer *output = stringbuffer_new();
    stringbuffer_append_string(output,
                               "# Per-server preferred direct QUIC route. Automatic entries "
                               "are omitted.\n");
    connection_preference_entry_t *entry;
    DL_FOREACH(connection_preferences, entry) {
        stringbuffer_append_printf(output,
                                   "%s\t%s\n",
                                   entry->key,
                                   preference_keys[entry->preference]);
    }
    char *contents = stringbuffer_finish(output);
    char *path = file_path(FILE_CONNECTION_PREFERENCES, "w");
    bool ok = path_write_atomic(path, contents, strlen(contents), 0600);
    free(path);
    free(contents);
    if (!ok) {
        LOG(ERROR, "Could not atomically write %s", FILE_CONNECTION_PREFERENCES);
    }
}

void connection_preferences_init(void) {
    FILE *fp = path_fopen(FILE_CONNECTION_PREFERENCES, "r");
    if (fp == NULL) {
        return;
    }

    char buf[HUGE_BUF];
    while (fgets(VS(buf), fp) != NULL) {
        char key[HUGE_BUF];
        char preference_key[32];
        if (buf[0] == '#' || sscanf(buf, "%4095s %31s", key, preference_key) != 2 ||
            connection_preference_find(key) != NULL) {
            continue;
        }

        socket_connection_preference_t preference = SOCKET_CONNECTION_PREFERENCE_AUTO;
        for (int i = 1; i < SOCKET_CONNECTION_PREFERENCE_NUM; i++) {
            if (strcmp(preference_key, preference_keys[i]) == 0) {
                preference = (socket_connection_preference_t)i;
                break;
            }
        }
        if (preference == SOCKET_CONNECTION_PREFERENCE_AUTO) {
            continue;
        }

        connection_preference_entry_t *entry = xcalloc(1, sizeof(*entry));
        entry->key = xstrdup(key);
        entry->preference = preference;
        DL_APPEND(connection_preferences, entry);
    }
    fclose(fp);
}

void connection_preferences_deinit(void) {
    connection_preference_entry_t *entry, *tmp;
    DL_FOREACH_SAFE(connection_preferences, entry, tmp) {
        DL_DELETE(connection_preferences, entry);
        free(entry->key);
        free(entry);
    }
}

socket_connection_preference_t connection_preference_get(const server_struct *server) {
    char key[HUGE_BUF];
    if (!connection_preference_key(server, VS(key))) {
        return SOCKET_CONNECTION_PREFERENCE_AUTO;
    }

    connection_preference_entry_t *entry = connection_preference_find(key);
    return entry != NULL ? entry->preference : SOCKET_CONNECTION_PREFERENCE_AUTO;
}

void connection_preference_set(const server_struct *server,
                               socket_connection_preference_t preference) {
    char key[HUGE_BUF];
    if (!connection_preference_key(server, VS(key)) ||
        preference < SOCKET_CONNECTION_PREFERENCE_AUTO ||
        preference >= SOCKET_CONNECTION_PREFERENCE_NUM) {
        return;
    }

    connection_preference_entry_t *entry = connection_preference_find(key);
    if (preference == SOCKET_CONNECTION_PREFERENCE_AUTO) {
        if (entry != NULL) {
            DL_DELETE(connection_preferences, entry);
            free(entry->key);
            free(entry);
        }
    } else if (entry != NULL) {
        entry->preference = preference;
    } else {
        entry = xcalloc(1, sizeof(*entry));
        entry->key = xstrdup(key);
        entry->preference = preference;
        DL_APPEND(connection_preferences, entry);
    }

    connection_preferences_save();
}
