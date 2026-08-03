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
 * Resource files management.
 *
 * @author Zoey Rose
 */

#include <global.h>
#include <resources.h>
#include <toolkit/string.h>
#include <toolkit/path.h>
#include <toolkit/packet.h>

/**
 * Hash table of the resource files.
 */
static resource_t *resources = NULL;

/**
 * Initialize the resource files management sub-system.
 */
void resources_init(void) {}

static void resources_free(void) {
    resource_t *resource, *tmp;

    HASH_ITER(hh, resources, resource, tmp) {
        HASH_DEL(resources, resource);

        asset_source_free(resource->source);

        efree(resource->name);
        efree(resource);
    }

    resources = NULL;
}

/**
 * Deinitialize the resource files management sub-system.
 */
void resources_deinit(void) {
    resources_free();
}

/**
 * Reload the resource files management sub-system.
 *
 * This should be done when switching servers.
 */
void resources_reload(void) {
    resources_free();
}

resource_t *resources_find(const char *name) {
    resource_t *resource;
    HASH_FIND(hh, resources, name, strlen(name), resource);
    return resource;
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_resource(uint8_t *data, size_t len, size_t pos) {
    char resource_name[HUGE_BUF];
    packet_to_string(data, len, &pos, VS(resource_name));
    if (string_isempty(resource_name)) {
        LOG(PACKET, "Received empty resource name");
        return;
    }

    if (resources_find(resource_name) != NULL) {
        return;
    }

    const unsigned char *md = data + pos;
    if (len - pos != sizeof(((resource_t *)NULL)->md)) {
        LOG(PACKET, "Invalid remaining packet size");
        return;
    }

    char digest[sizeof(((resource_t *)NULL)->digest)];
    SOFT_ASSERT(string_tohex(md, len - pos, VS(digest), false) == sizeof(digest) - 1,
                "string_tohex failed");
    string_tolower(digest);

    resource_t *resource = ecalloc(1, sizeof(*resource));
    resource->name = estrdup(resource_name);
    memcpy(resource->md, md, sizeof(resource->md));
    memcpy(resource->digest, digest, sizeof(resource->digest));
    HASH_ADD_KEYPTR(hh, resources, resource->name, strlen(resource->name), resource);

    char path[HUGE_BUF];
    snprintf(VS(path), "resources/%s", resource->digest);
    FILE *fp = path_fopen(path, "r");
    if (fp != NULL) {
        fclose(fp);
        resource->loaded = true;
        return;
    }

    char asset[HUGE_BUF];
    snprintf(VS(asset), "resources/%s", resource_name);
    resource->source = asset_source_start(asset, NULL);
}

/**
 * Checks if the specified resource is ready for use.
 *
 * @param resource
 * Resource to check.
 * @return
 * True if the resource is ready, false otherwise.
 */
bool resources_is_ready(resource_t *resource) {
    if (resource->loaded) {
        return true;
    }

    const uint8_t *body = NULL;
    size_t body_size = 0;
    asset_source_state_t state = asset_source_get_state(resource->source);
    if (state == ASSET_SOURCE_PENDING) {
        return false;
    }
    if (state == ASSET_SOURCE_COMPLETE) {
        body = asset_source_get_data(resource->source, &body_size);
    }

    if (body == NULL) {
        LOG(ERROR, "Failed to download resource %s", resource->name);
        goto error;
    }

    unsigned char md[SHA512_DIGEST_LENGTH];
    if (SHA512(body, body_size, md) == NULL) {
        LOG(ERROR, "SHA512() failed");
        goto error;
    }

    if (memcmp(md, resource->md, sizeof(md)) != 0) {
        LOG(ERROR, "!!! SHA512 digests do not match for resource %s !!!", resource->name);
        goto error;
    }

    char path[HUGE_BUF];
    snprintf(VS(path), "resources/%s", resource->digest);
    char *resolved = file_path(path, "wb");
    bool saved = path_write_atomic(resolved, body, body_size, 0600);
    efree(resolved);
    if (!saved) {
        LOG(ERROR, "Failed to atomically write %s", path);
        goto error;
    }
    resource->loaded = true;

    bool ret = true;
    goto out;

error:
    ret = false;

out:
    asset_source_free(resource->source);
    resource->source = NULL;
    return ret;
}
