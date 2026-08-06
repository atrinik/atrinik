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
 * Manages server file updates.
 *
 * @author Zoey Rose
 */

#include <global.h>
#include <animations.h>
#include <wrapper.h>
#include <toolkit/string.h>
#include <toolkit/path.h>
#include <toolkit/curl.h>

/** The server files. */
static server_files_struct *server_files;

/** HTTP-first listing request with in-band QUIC fallback. */
static asset_source_t *listing_source;

/**
 * Initialize the server files API.
 */
void server_files_init(void) {
    server_files_struct *tmp;

    server_files = NULL;

    tmp = server_files_create(SERVER_FILE_BMAPS);
    tmp->parse_func = image_bmaps_init;

    tmp = server_files_create(SERVER_FILE_UPDATES);
    tmp->parse_func = file_updates_parse;

    tmp = server_files_create(SERVER_FILE_SETTINGS);
    tmp->parse_func = server_settings_init;

    tmp = server_files_create(SERVER_FILE_ANIMS);
    tmp->parse_func = read_anims;
    tmp->reload_func = anims_reset;

    tmp = server_files_create(SERVER_FILE_EFFECTS);
    tmp->parse_func = effects_init;
    tmp->reload_func = effects_reinit;

    tmp = server_files_create(SERVER_FILE_HFILES);
    tmp->parse_func = hfiles_init;
    tmp->init_func = hfiles_init;

    server_files_init_all();
}

/**
 * De-initialize the server files API.
 */
void server_files_deinit(void) {
    asset_source_free(listing_source);
    listing_source = NULL;

    server_files_struct *curr, *tmp;
    HASH_ITER(hh, server_files, curr, tmp) {
        HASH_DEL(server_files, curr);
        asset_source_free(curr->source);
        free(curr->name);
        free(curr);
    }
}

/**
 * Init all of the available server files.
 */
void server_files_init_all(void) {
    server_files_struct *curr, *tmp;
    HASH_ITER(hh, server_files, curr, tmp) {
        if (curr->init_func) {
            curr->init_func();
        }
    }
}

/**
 * Create a server file with the specified name.
 *
 * The server file will be added to a hash table automatically.
 *
 * @param name
 * Name of the server file.
 * @return
 * Created server file.
 */
server_files_struct *server_files_create(const char *name) {
    server_files_struct *tmp;

    tmp = xcalloc(1, sizeof(*tmp));
    tmp->name = xstrdup(name);
    HASH_ADD_KEYPTR(hh, server_files, tmp->name, strlen(tmp->name), tmp);

    return tmp;
}

/**
 * Find the specified server file in the hash table.
 *
 * @param name
 * Name of the server file.
 * @return
 * Server file if found, NULL otherwise.
 */
server_files_struct *server_files_find(const char *name) {
    server_files_struct *tmp;
    HASH_FIND(hh, server_files, name, strlen(name), tmp);
    return tmp;
}

/**
 * Load the server files. If they haven't changed since last load, no
 * loading will be done.
 *
 * @param post_load
 * Unless 1, (re-)parsing the server files will not be done.
 */
void server_files_load(int post_load) {
    server_files_struct *curr, *tmp;
    FILE *fp;
    struct stat sb;
    size_t st_size, numread;
    char *contents;

    HASH_ITER(hh, server_files, curr, tmp) {
        curr->update = 0;

        if (post_load && curr->loaded) {
            if (curr->reload_func) {
                curr->reload_func();
            }

            continue;
        }

        /* Open the file. */
        fp = server_file_open(curr);

        if (fp == NULL) {
            continue;
        }

        /* Get and store the size. */
        fstat(fileno(fp), &sb);
        st_size = sb.st_size;
        curr->size = st_size;

        /* Allocate temporary buffer and read into it the file. */
        contents = xmalloc(st_size);
        numread = fread(contents, 1, st_size, fp);

        /* Calculate and store the checksum, free the temporary buffer
         * and close the file pointer. */
        curr->crc32 = crc32(1L, (const unsigned char FAR *)contents, numread);
        free(contents);
        fclose(fp);

        if (post_load) {
            /* Mark that we have loaded this file. */
            curr->loaded = 1;

            if (curr->parse_func) {
                curr->parse_func();
            }
        }
    }
}

/**
 * Begin downloading the listing file.
 */
void server_files_listing_retrieve(void) {
    asset_source_free(listing_source);
    listing_source = asset_source_start("data/listing.txt", NULL);
    if (asset_source_get_state(listing_source) == ASSET_SOURCE_ERROR) {
        LOG(ERROR, "No CDN URL or QUIC asset transport is available");
        cpl.state = ST_INIT;
    }
}

/**
 * Check if the listing file has been downloaded and processed.
 * @return
 * 1 if it has been processed, 0 otherwise.
 */
int server_files_listing_processed(void) {
    const uint8_t *body = NULL;
    size_t body_size = 0;

    if (listing_source == NULL) {
        return 0;
    }
    asset_source_state_t state = asset_source_get_state(listing_source);
    if (state == ASSET_SOURCE_PENDING) {
        return 0;
    }
    if (state == ASSET_SOURCE_COMPLETE) {
        body = asset_source_get_data(listing_source, &body_size);
    }

    if (body == NULL) {
        LOG(ERROR, "Could not retrieve the server asset manifest");
        cpl.state = ST_INIT;
        return 0;
    }

    char *manifest = xstrndup((const char *)body, body_size);
    char word[HUGE_BUF];
    size_t pos = 0;
    while (string_get_word(manifest, &pos, '\n', VS(word), 0)) {
        char *cps[3];
        if (string_split(word, cps, arraysize(cps), ':') != arraysize(cps)) {
            continue;
        }

        server_files_struct *tmp = server_files_find(cps[0]);
        if (tmp == NULL) {
            continue;
        }

        uint64_t crc_value;
        uint64_t size_value;
        if (!string_parse_uint64(cps[1], 16, 0, UINT32_MAX, &crc_value) ||
            !string_parse_uint64(cps[2], 16, 0, SIZE_MAX, &size_value)) {
            LOG(ERROR, "Invalid asset manifest entry for %s", cps[0]);
            continue;
        }
        unsigned long crc = (unsigned long)crc_value;
        size_t fsize = (size_t)size_value;

        if (tmp->crc32 != crc || tmp->size != fsize) {
            tmp->update = 1;
        }

        LOG(DEVEL,
            "%-10s CRC32: %lu (local: %lu) Size: %" PRIu64 " (local: %" PRIu64 ") Update: %d",
            tmp->name,
            crc,
            tmp->crc32,
            (uint64_t)fsize,
            (uint64_t)tmp->size,
            tmp->update);

        tmp->crc32 = crc;
        tmp->size = fsize;
    }
    free(manifest);

    asset_source_free(listing_source);
    listing_source = NULL;

    return 1;
}

/**
 * Process a single server file.
 *
 * @param tmp
 * What to process.
 * @return
 * 1 if the file is being processed, 0 otherwise.
 */
static int server_file_process(server_files_struct *tmp) {
    if (tmp->update == 0) {
        return 0;
    }

    if (tmp->update == 1) {
        char asset[MAX_BUF];
        snprintf(VS(asset), "data/%s.zz", tmp->name);
        tmp->source = asset_source_start(asset, NULL);
        tmp->update = -1;
        return 1;
    }

    const uint8_t *body = NULL;
    size_t body_size = 0;
    asset_source_state_t source_state = asset_source_get_state(tmp->source);
    if (source_state == ASSET_SOURCE_PENDING) {
        return 1;
    }
    if (source_state == ASSET_SOURCE_COMPLETE) {
        body = asset_source_get_data(tmp->source, &body_size);
    }

    if (body == NULL) {
        LOG(ERROR, "Could not download required server file %s", tmp->name);
        cpl.state = ST_INIT;
    } else {
        unsigned long len_ucomp = tmp->size;
        unsigned char *dest = xmalloc(len_ucomp);
        uint64_t started = SDL_GetTicksNS();
        int result =
            uncompress((Bytef *)dest, (uLongf *)&len_ucomp, (const Bytef *)body, (uLong)body_size);
        double elapsed_ms = (double)(SDL_GetTicksNS() - started) / 1000000.0;
        if (result != Z_OK || len_ucomp != tmp->size) {
            LOG(ERROR, "Invalid compressed server file %s after %.3f ms", tmp->name, elapsed_ms);
            cpl.state = ST_INIT;
        } else {
            LOG(DEBUG,
                "Decompressed required server file %s (%" PRIu64 " -> %lu bytes) in %.3f ms",
                tmp->name,
                (uint64_t)body_size,
                len_ucomp,
                elapsed_ms);
            if (server_file_save(tmp, dest, len_ucomp)) {
                tmp->loaded = 0;
            }
        }
        free(dest);
    }

    tmp->update = 0;
    asset_source_free(tmp->source);
    tmp->source = NULL;

    return 0;
}

/**
 * Check if all of the server files have been processed.
 *
 * @return
 * 1 if they all have been processed, 0 otherwise.
 */
int server_files_processed(void) {
    server_files_struct *curr, *tmp;
    bool pending = false;
    /* Check all files. */
    HASH_ITER(hh, server_files, curr, tmp) {
        if (server_file_process(curr)) {
            pending = true;
        }
        if (cpl.state == ST_INIT) {
            return 0;
        }
    }

    return !pending;
}

/**
 * Construct a path to the specified server file.
 *
 * @param tmp
 * Server file.
 * @param[out] buf Will contain the constructed path.
 * @param buf_size
 * Size of 'buf'.
 * @return
 * 'buf'.
 */
static char *server_file_path(server_files_struct *tmp, char *buf, size_t buf_size) {
    snprintf(buf, buf_size, "srv_files/%s", tmp->name);
    return buf;
}

/**
 * Open a server file for reading.
 *
 * @param tmp
 * Server file.
 * @return
 * The file pointer, or NULL on failure of opening the file.
 */
FILE *server_file_open(server_files_struct *tmp) {
    if (tmp == NULL) {
        return NULL;
    }

    char buf[MAX_BUF];
    server_file_path(tmp, VS(buf));
    return path_fopen(buf, "rb");
}

/**
 * Wrapper for server_file_open(), allows opening a server file by its
 * name.
 * @param name
 * Name of the server file.
 * @return
 * Opened file pointer, NULL on failure.
 */
FILE *server_file_open_name(const char *name) {
    return server_file_open(server_files_find(name));
}

/**
 * We have received the server file we asked for, so save it to disk.
 *
 * @param tmp
 * Server file.
 * @param data
 * The data to save.
 * @param len
 * Length of 'data'.
 * @return
 * True on success, false on failure.
 */
bool server_file_save(server_files_struct *tmp, unsigned char *data, size_t len) {
    char path[MAX_BUF];
    server_file_path(tmp, VS(path));
    char *resolved = file_path(path, "wb");
    bool ok = path_write_atomic(resolved, data, len, 0600);
    free(resolved);
    if (!ok) {
        LOG(ERROR, "Could not atomically write %s.", path);
    }
    return ok;
}
