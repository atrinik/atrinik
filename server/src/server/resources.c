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
 * Resource files.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <toolkit/string.h>
#include <toolkit/packet.h>
#include <resources.h>

/**
 * Hash table containing the resources IDs and SHA512 sums.
 */
static resource_t *resources = NULL;

/**
 * Traverse the specified directory looking for resource files.
 *
 * @param dir
 * Directory.
 * @param path
 * Path to the directory.
 */
static void
resources_traverse (DIR *dir, const char *path)
{
    HARD_ASSERT(dir != NULL);
    HARD_ASSERT(path != NULL);

    struct dirent *d;
    while ((d = readdir(dir)) != NULL) {
        if (d->d_name[0] == '.') {
            continue;
        }

        char path_curr[HUGE_BUF];
        snprintf(VS(path_curr), "%s/%s", path, d->d_name);
        DIR *dir_curr = opendir(path_curr);
        if (dir_curr == NULL) {
            if (errno != ENOTDIR) {
                LOG(ERROR, "Failed to open %s: %s (%d)",
                    path_curr, strerror(errno), errno);
                exit(1);
            }
        } else {
            resources_traverse(dir_curr, path_curr);
            continue;
        }

        SOFT_ASSERT(string_startswith(path_curr, settings.resourcespath),
                    "File path doesn't start with the resource path");

        /* Plus one for the forward slash */
        char *cp = string_sub(path_curr, strlen(settings.resourcespath) + 1, 0);

        resource_t *resource = ecalloc(1, sizeof(*resource));
        resource->name = cp;
        HASH_ADD_KEYPTR(hh,
                        resources,
                        resource->name,
                        strlen(resource->name),
                        resource);

        SHA512_CTX ctx;
        SOFT_ASSERT(SHA512_Init(&ctx) == 1, "SHA512_Init() failed");

        FILE *fp = fopen(path_curr, "rb");
        if (fp == NULL) {
            LOG(ERROR, "Failed to open %s for reading: %s (%d)",
                path_curr, strerror(errno), errno);
            exit(1);
        }

        size_t num_read;
        unsigned char buffer[1024 * 64];
        while ((num_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
            SOFT_ASSERT(SHA512_Update(&ctx, buffer, num_read) == 1,
                        "SHA512_Update() failed");
        }

        fclose(fp);
        SOFT_ASSERT(SHA512_Final(resource->md, &ctx) == 1,
                    "SHA512_Final() failed");

        char key[sizeof(resource->md) * 2 + 1];
        SOFT_ASSERT(string_tohex(VS(resource->md),
                                       VS(key),
                                       false) == sizeof(key) - 1,
                          "string_tohex failed");
        string_tolower(key);
    }

    closedir(dir);
}

/**
 * Initialize the resource files database.
 */
void
resources_init (void)
{
    DIR *dir = opendir(settings.resourcespath);
    if (dir == NULL) {
        LOG(INFO,
            "Resources directory cannot be accessed, resource files will "
            "not be available.");
        return;
    }

    resources_traverse(dir, settings.resourcespath);
}

/**
 * Deinitialize the resource files database.
 */
void
resources_deinit (void)
{
    resource_t *resource, *tmp;
    HASH_ITER(hh, resources, resource, tmp) {
        HASH_DEL(resources, resource);
        efree(resource->name);
        efree(resource);
    }
}

/**
 * Find a resource identified by its name.
 *
 * @param name
 * The name identifier.
 * @return
 * Resource if found, NULL otherwise.
 */
resource_t *
resources_find (const char *name)
{
    if (name == NULL) {
        return NULL;
    }

    resource_t *resource;
    HASH_FIND_STR(resources, name, resource);
    return resource;
}

/**
 * Send information about the specified resource to a game client.
 *
 * @param resource
 * Resource to send information about.
 * @param ns
 * Client to send to.
 */
void
resources_send (resource_t    *resource,
                socket_struct *ns)
{
    HARD_ASSERT(resource != NULL);
    HARD_ASSERT(ns != NULL);

    packet_struct *packet = packet_new(CLIENT_CMD_RESOURCE, 256, 0);
    packet_append_string_terminated(packet, resource->name);
    packet_append_data_len(packet, VS(resource->md));
    socket_send_packet(ns, packet);
}
