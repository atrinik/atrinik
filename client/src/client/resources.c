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
 * Resource files management.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <resources.h>
#include <toolkit_string.h>
#include <path.h>

/**
 * Hash table of the resource files.
 */
static resource_t *resources = NULL;

/**
 * Initialize the resource files management sub-system.
 */
void
resources_init (void)
{
}

static void
resources_free (void)
{
    resource_t *resource, *tmp;

    HASH_ITER(hh, resources, resource, tmp) {
        HASH_DEL(resources, resource);

        if (resource->request != NULL) {
            curl_request_free(resource->request);
        }

        efree(resource->name);
        efree(resource);
    }

    resources = NULL;
}

/**
 * Deinitialize the resource files management sub-system.
 */
void
resources_deinit (void)
{
    resources_free();
}

/**
 * Reload the resource files management sub-system.
 *
 * This should be done when switching servers.
 */
void
resources_reload (void)
{
    resources_free();
}

resource_t *
resources_find (const char *name)
{
    resource_t *resource;
    HASH_FIND(hh, resources, name, strlen(name), resource);
    return resource;
}

/** @copydoc socket_command_struct::handle_func */
void
socket_command_resource (uint8_t *data, size_t len, size_t pos)
{
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
    if (len - pos != sizeof(((resource_t *) NULL)->md)) {
        LOG(PACKET, "Invalid remaining packet size");
        return;
    }

    char digest[sizeof(((resource_t *) NULL)->digest)];
    SOFT_ASSERT(string_tohex(md,
                             len - pos,
                             VS(digest),
                             false) == sizeof(digest) - 1,
                "string_tohex failed");
    string_tolower(digest);

    resource_t *resource = ecalloc(1, sizeof(*resource));
    resource->name = estrdup(resource_name);
    memcpy(resource->md, md, sizeof(resource->md));
    memcpy(resource->digest, digest, sizeof(resource->digest));
    HASH_ADD_KEYPTR(hh,
                    resources,
                    resource->name,
                    strlen(resource->name),
                    resource);

    char path[HUGE_BUF];
    snprintf(VS(path), "resources/%s", resource->digest);
    FILE *fp = path_fopen(path, "r");
    if (fp != NULL) {
        fclose(fp);
        resource->loaded = true;
        return;
    }

    char url[HUGE_BUF];
    snprintf(VS(url), "%s/resources/%s", cpl.http_url, resource_name);
    resource->request = curl_request_create(url, CURL_PKEY_TRUST_APPLICATION);
    curl_request_start_get(resource->request);
}

/**
 * Checks if the specified resource is ready for use.
 *
 * @param resource
 * Resource to check.
 * @return
 * True if the resource is ready, false otherwise.
 */
bool
resources_is_ready (resource_t *resource)
{
    if (resource->loaded) {
        return true;
    }

    if (curl_request_get_state(resource->request) == CURL_STATE_INPROGRESS) {
        return false;
    }

    if (curl_request_get_http_code(resource->request) != 200) {
        LOG(ERROR, "Failed to download painting %s",
            curl_request_get_url(resource->request));
        goto error;
    }

    size_t body_size;
    char *body = curl_request_get_body(resource->request, &body_size);
    if (body == NULL) {
        LOG(ERROR, "Failed to download painting %s",
            curl_request_get_url(resource->request));
        goto error;
    }

    unsigned char md[SHA512_DIGEST_LENGTH];
    if (SHA512((unsigned char *) body, body_size, md) == NULL) {
        LOG(ERROR, "SHA512() failed");
        goto error;
    }

    if (memcmp(md, resource->md, sizeof(md)) != 0) {
        LOG(ERROR, "!!! SHA512 digests do not match for resource %s !!!",
            resource->name);
        goto error;
    }

    char path[HUGE_BUF];
    snprintf(VS(path), "resources/%s", resource->digest);
    FILE *fp = path_fopen(path, "wb");
    if (fp == NULL) {
        LOG(ERROR, "Failed to open %s: %s (%d)",
            path,
            strerror(errno),
            errno);
        goto error;
    }

    if (fwrite(body, 1, body_size, fp) != body_size) {
        LOG(ERROR, "Failed to write enough bytes to %s: %s (%d)",
            path,
            strerror(errno),
            errno);
        fclose(fp);
        goto error;
    }

    fclose(fp);
    resource->loaded = true;

    bool ret = true;
    goto out;

error:
    ret = false;
    curl_request_free(resource->request);
    resource->request = NULL;

out:
    return ret;
}
