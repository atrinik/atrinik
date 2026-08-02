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
#include <toolkit/string.h>
#include <toolkit/path.h>
#include <toolkit/curl.h>

/** The server files. */
static server_files_struct *server_files;

/** CDN listing request. */
static curl_request_t *listing_request = NULL;
/** In-band QUIC listing request. */
static asset_request_t *listing_asset_request = NULL;

/**
 * Initialize the server files API.
 */
void
server_files_init (void)
{
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
void
server_files_deinit (void)
{
    if (listing_request != NULL) {
        curl_request_free(listing_request);
        listing_request = NULL;
    }
    if (listing_asset_request != NULL) {
        asset_request_free(listing_asset_request);
        listing_asset_request = NULL;
    }

    server_files_struct *curr, *tmp;
    HASH_ITER(hh, server_files, curr, tmp) {
        HASH_DEL(server_files, curr);
        if (curr->request != NULL) {
            curl_request_free(curr->request);
        }
        if (curr->asset_request != NULL) {
            asset_request_free(curr->asset_request);
        }
        efree(curr->name);
        efree(curr);
    }
}

/**
 * Init all of the available server files.
 */
void
server_files_init_all (void)
{
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
server_files_struct *
server_files_create (const char *name)
{
    server_files_struct *tmp;

    tmp = ecalloc(1, sizeof(*tmp));
    tmp->name = estrdup(name);
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
server_files_struct *
server_files_find (const char *name)
{
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
void
server_files_load (int post_load)
{
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
        contents = emalloc(st_size);
        numread = fread(contents, 1, st_size, fp);

        /* Calculate and store the checksum, free the temporary buffer
         * and close the file pointer. */
        curr->crc32 = crc32(1L, (const unsigned char FAR *) contents, numread);
        efree(contents);
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
void
server_files_listing_retrieve (void)
{
    if (listing_request != NULL) {
        curl_request_free(listing_request);
        listing_request = NULL;
    }
    if (listing_asset_request != NULL) {
        asset_request_free(listing_asset_request);
        listing_asset_request = NULL;
    }

    if (*cpl.http_url != '\0') {
        char url[HUGE_BUF];
        snprintf(VS(url), "%s/%s/%s",
                 cpl.http_url,
                 SERVER_FILES_HTTP_DIR,
                 SERVER_FILES_HTTP_LISTING);
        listing_request =
            curl_request_create(url, CURL_PKEY_TRUST_APPLICATION);
        curl_request_start_get(listing_request);
    } else {
        listing_asset_request = asset_request_start("data/listing.txt");
        if (listing_asset_request == NULL) {
            LOG(ERROR, "No CDN URL or QUIC asset transport is available");
            cpl.state = ST_INIT;
        }
    }
}

/**
 * Check if the listing file has been downloaded and processed.
 * @return
 * 1 if it has been processed, 0 otherwise.
 */
int
server_files_listing_processed (void)
{
    const uint8_t *body = NULL;
    size_t body_size = 0;

    if (listing_request != NULL) {
        curl_state_t state = curl_request_get_state(listing_request);
        if (state == CURL_STATE_INPROGRESS) {
            return 0;
        }
        if (state == CURL_STATE_OK) {
            body = (const uint8_t *)
                curl_request_get_body(listing_request, &body_size);
        } else if (cpl.asset_transport) {
            curl_request_free(listing_request);
            listing_request = NULL;
            listing_asset_request = asset_request_start("data/listing.txt");
            return 0;
        }
    } else if (listing_asset_request != NULL) {
        asset_request_state_t state =
            asset_request_get_state(listing_asset_request);
        if (state == ASSET_REQUEST_PENDING) {
            return 0;
        }
        if (state == ASSET_REQUEST_COMPLETE) {
            body = asset_request_get_data(listing_asset_request, &body_size);
        }
    }

    if (body == NULL) {
        LOG(ERROR, "Could not retrieve the server asset manifest");
        cpl.state = ST_INIT;
        return 0;
    }

    char *manifest = estrndup((const char *) body, body_size);
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

        unsigned long crc = strtoul(cps[1], NULL, 16);
        size_t fsize = strtoul(cps[2], NULL, 16);

        if (tmp->crc32 != crc || tmp->size != fsize) {
            tmp->update = 1;
        }

        LOG(DEVEL,
            "%-10s CRC32: %lu (local: %lu) Size: %" PRIu64 " (local: %" PRIu64
            ") Update: %d",
            tmp->name,
            crc,
            tmp->crc32,
            (uint64_t) fsize,
            (uint64_t) tmp->size,
            tmp->update);

        tmp->crc32 = crc;
        tmp->size = fsize;
    }
    efree(manifest);

    if (listing_request != NULL) {
        curl_request_free(listing_request);
        listing_request = NULL;
    }
    if (listing_asset_request != NULL) {
        asset_request_free(listing_asset_request);
        listing_asset_request = NULL;
    }

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
static int
server_file_process (server_files_struct *tmp)
{
    if (tmp->update == 0) {
        return 0;
    }

    if (tmp->update == 1) {
        if (*cpl.http_url != '\0') {
            char url[MAX_BUF];
            snprintf(VS(url), "%s/%s/%s.zz",
                     cpl.http_url,
                     SERVER_FILES_HTTP_DIR,
                     tmp->name);
            tmp->request =
                curl_request_create(url, CURL_PKEY_TRUST_APPLICATION);
            curl_request_start_get(tmp->request);
        } else {
            char asset[MAX_BUF];
            snprintf(VS(asset), "data/%s.zz", tmp->name);
            tmp->asset_request = asset_request_start(asset);
        }
        tmp->update = -1;
        return 1;
    }

    const uint8_t *body = NULL;
    size_t body_size = 0;
    if (tmp->request != NULL) {
        curl_state_t state = curl_request_get_state(tmp->request);
        if (state == CURL_STATE_INPROGRESS) {
            return 1;
        }
        if (state == CURL_STATE_OK) {
            body = (const uint8_t *)
                curl_request_get_body(tmp->request, &body_size);
        } else if (cpl.asset_transport) {
            curl_request_free(tmp->request);
            tmp->request = NULL;
            char asset[MAX_BUF];
            snprintf(VS(asset), "data/%s.zz", tmp->name);
            tmp->asset_request = asset_request_start(asset);
            return 1;
        }
    } else if (tmp->asset_request != NULL) {
        asset_request_state_t state =
            asset_request_get_state(tmp->asset_request);
        if (state == ASSET_REQUEST_PENDING) {
            return 1;
        }
        if (state == ASSET_REQUEST_COMPLETE) {
            body = asset_request_get_data(tmp->asset_request, &body_size);
        }
    }

    if (body == NULL) {
        LOG(ERROR, "Could not download required server file %s", tmp->name);
        cpl.state = ST_INIT;
    } else {
        unsigned long len_ucomp = tmp->size;
        unsigned char *dest = emalloc(len_ucomp);
        int result = uncompress((Bytef *) dest,
                                (uLongf *) &len_ucomp,
                                (const Bytef *) body,
                                (uLong) body_size);
        if (result != Z_OK || len_ucomp != tmp->size) {
            LOG(ERROR, "Invalid compressed server file %s", tmp->name);
            cpl.state = ST_INIT;
        } else if (server_file_save(tmp, dest, len_ucomp)) {
            tmp->loaded = 0;
        }
        efree(dest);
    }

    tmp->update = 0;
    if (tmp->request != NULL) {
        curl_request_free(tmp->request);
        tmp->request = NULL;
    }
    if (tmp->asset_request != NULL) {
        asset_request_free(tmp->asset_request);
        tmp->asset_request = NULL;
    }

    return 0;
}

/**
 * Check if all of the server files have been processed.
 *
 * @return
 * 1 if they all have been processed, 0 otherwise.
 */
int
server_files_processed (void)
{
    server_files_struct *curr, *tmp;
    /* Check all files. */
    HASH_ITER(hh, server_files, curr, tmp) {
        if (server_file_process(curr)) {
            return 0;
        }
        if (cpl.state == ST_INIT) {
            return 0;
        }
    }

    return 1;
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
static char
*server_file_path (server_files_struct *tmp, char *buf, size_t buf_size)
{
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
FILE *
server_file_open (server_files_struct *tmp)
{
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
FILE *
server_file_open_name (const char *name)
{
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
bool
server_file_save (server_files_struct *tmp, unsigned char *data, size_t len)
{
    char path[MAX_BUF];
    server_file_path(tmp, VS(path));

    FILE *fp = path_fopen(path, "wb");
    if (fp == NULL) {
        LOG(ERROR, "Could not open %s for writing.", path);
        return false;
    }

    bool ret = true;

    if (fwrite(data, 1, len, fp) != len) {
        LOG(ERROR, "Failed to write to %s.", path);
        ret = false;
    }

    if (fclose(fp) != 0) {
        LOG(ERROR, "Could not close %s.", path);
        ret = false;
    }

    return ret;
}
