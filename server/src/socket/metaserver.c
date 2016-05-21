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
 * Metaserver updating related code.
 */

#include <global.h>
#include <toolkit_string.h>
#include <curl.h>
#include <player.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/err.h>

/**
 * Used to hold metaserver statistics.
 */
static struct {
    uint64_t num; ///< Number of successful updates.

    uint64_t num_failed; ///< Number of failed updates.

    time_t last; ///< Last successful update.

    time_t last_failed; ///< Last failed update.
} stats;

/**
 * Where the metaserver key file is located.
 */
#define METASERVER_KEY_FILE "metaserver_key"

/**
 * Mutex for the metaserver stats.
 */
static pthread_mutex_t stats_lock;

/**
 * cURL request structure.
 */
static curl_request_t *current_request = NULL;
/**
 * Mutex for the current request pointer.
 */
static pthread_mutex_t request_lock;
/**
 * Temporary string used to send a list of players to the metaserver.
 */
static char *request_players = NULL;
/**
 * Number of players.
 */
static uint32_t request_num_players = 0;
/**
 * Keeps track of whether the generate metaserver key is new or not.
 */
static bool key_is_new = false;

/**
 * Figure out whether the meta-server is enabled or not.
 *
 * @return
 * True if the meta-server is enabled, false otherwise.
 */
static bool
metaserver_enabled (void)
{
    if (*settings.server_host == '\0') {
        return false;
    }

    if (settings.unit_tests) {
        return false;
    }

    return true;
}

/**
 * Initialize the metaserver.
 */
void
metaserver_init (void)
{
    if (!metaserver_enabled()) {
        return;
    }

    metaserver_info_update();
    pthread_mutex_init(&stats_lock, NULL);
    pthread_mutex_init(&request_lock, NULL);
}

/**
 * Deinitialize the metaserver.
 */
void
metaserver_deinit (void)
{
    if (!metaserver_enabled()) {
        return;
    }

    if (current_request != NULL) {
        curl_state_t state;
        do {
            state = curl_request_get_state(current_request);
            sleep(1);
        } while (state == CURL_STATE_INPROGRESS);

        curl_request_free(current_request);
        current_request = NULL;
    }

    if (request_players != NULL) {
        efree(request_players);
        request_players = NULL;
    }

    pthread_mutex_destroy(&stats_lock);
    pthread_mutex_destroy(&request_lock);
}

/**
 * Check if the specified cURL request resulted in an error.
 *
 * @param request
 * Request to check.
 * @return
 * True if an error was processed, false otherwise.
 */
static bool
metaserver_request_process_error (curl_request_t *request)
{
    HARD_ASSERT(request != NULL);

    curl_state_t state = curl_request_get_state(request);
    int http_code = curl_request_get_http_code(request);
    if (state == CURL_STATE_OK && http_code == 200) {
        return false;
    }

    char *body = curl_request_get_body(request, NULL);
    LOG(SYSTEM,
        "Failed to update metaserver information "
        "(HTTP code: %d), response: %s",
        http_code,
        body != NULL ? body : "<empty>");

    pthread_mutex_lock(&stats_lock);
    stats.last_failed = time(NULL);
    stats.num_failed++;
    pthread_mutex_unlock(&stats_lock);
    return true;
}

/**
 * Callback received for publishing a metaserver update.
 *
 * @param request
 * cURL request.
 * @param user_data
 * NULL.
 */
static void
metaserver_update_request (curl_request_t *request, void *user_data)
{
    pthread_mutex_lock(&request_lock);
    current_request = NULL;

    if (metaserver_request_process_error(request)) {
        /* If we had a new key generated, remove it so that it will be
         * re-created, since it was rejected. */
        if (key_is_new) {
            char path[HUGE_BUF];
            snprintf(VS(path), "%s/" METASERVER_KEY_FILE, settings.datapath);

            if (unlink(path) != 0) {
                LOG(ERROR, "Failed to unlink %s: %s (%d)",
                    path, strerror(errno), errno);
            }

            key_is_new = false;
        }

        goto out;
    }

    pthread_mutex_lock(&stats_lock);
    stats.last = time(NULL);
    stats.num++;
    pthread_mutex_unlock(&stats_lock);

out:
    curl_request_free(request);
    pthread_mutex_unlock(&request_lock);
}

/**
 * Acquires the key to use for metaserver authentication.
 *
 * @param[out] key
 * Will contain the key on success.
 * @param key_size
 * Size of the 'key' buffer.
 * @param otp
 * OTP from the metaserver.
 * @param cotp
 * Generated COTP.
 * @return
 * True on success, false on failure.
 */
static bool
metaserver_get_key (char       *key,
                    size_t      key_size,
                    const char *otp,
                    const char *cotp)
{
    HARD_ASSERT(key != NULL);
    HARD_ASSERT(key_size == SHA512_DIGEST_LENGTH * 2 + 1);

    unsigned char tmp_key[SHA512_DIGEST_LENGTH];

    char path[HUGE_BUF];
    snprintf(VS(path), "%s/" METASERVER_KEY_FILE, settings.datapath);
    FILE *fp = fopen(path, "rb");
    if (fp == NULL && errno == ENOENT) {
        fp = fopen(path, "wb");
        if (fp == NULL) {
            LOG(ERROR, "Failed to open %s for writing: %s (%d)",
                path, strerror(errno), errno);
            return false;
        }

        unsigned char bytes[64];

        if (chmod(path, S_IRUSR | S_IWUSR) != 0) {
            LOG(ERROR, "Failed to chmod %s: %s (%d)",
                path, strerror(errno), errno);
            goto error_creating;
        }

        if (RAND_bytes(VS(bytes)) != 1) {
            LOG(ERROR, "RAND_bytes() failed: %s",
                ERR_error_string(ERR_get_error(), NULL));
            goto error_creating;
        }

        if (SHA512(VS(bytes), tmp_key) == NULL) {
            LOG(ERROR, "SHA512() failed: %s",
                ERR_error_string(ERR_get_error(), NULL));
            goto error_creating;
        }

        memset(&bytes, 0, sizeof(bytes));
        key[SHA512_DIGEST_LENGTH] = '\0';

        if (fwrite(VS(tmp_key), 1, fp) != 1) {
            LOG(ERROR, "Failed to write to %s: %s (%d)",
                path, strerror(errno), errno);
            goto error_creating;
        }

        if (fclose(fp) != 0) {
            LOG(ERROR, "Failed to close %s: %s (%d)",
                path, strerror(errno), errno);
            goto error_creating;
        }

        SOFT_ASSERT_LABEL(string_tohex(VS(tmp_key),
                                       key,
                                       key_size,
                                       false) == key_size - 1,
                          error_creating,
                          "string_tohex failed");
        string_tolower(key);
        key_is_new = true;

        return true;

error_creating:
        if (unlink(path) != 0) {
            LOG(ERROR, "Failed to unlink %s: %s (%d)",
                path, strerror(errno), errno);
        }

        memset(&bytes, 0, sizeof(bytes));
        memset(&tmp_key, 0, sizeof(tmp_key));
        memset(key, 0, key_size);
        return false;
    } else if (fp == NULL) {
        LOG(ERROR, "Failed to open %s for reading: %s (%d)",
            path, strerror(errno), errno);
        return false;
    }

    key_is_new = false;

    if (fread(VS(tmp_key), 1, fp) != 1) {
        LOG(ERROR, "Failed to read from %s: %s (%d)",
            path, strerror(errno), errno);
        goto error_reading;
    }

    SOFT_ASSERT_LABEL(string_tohex(VS(tmp_key),
                                   key,
                                   key_size,
                                   false) == key_size - 1,
                      error_reading,
                      "string_tohex failed");
    string_tolower(key);

    SHA512_CTX ctx;
    if (SHA512_Init(&ctx) != 1) {
        LOG(ERROR, "SHA512_Init() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error_reading;
    }

    if (SHA512_Update(&ctx, key, key_size - 1) != 1) {
        LOG(ERROR, "SHA512_Update() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error_reading;
    }

    if (SHA512_Update(&ctx,
                      settings.server_host,
                      strlen(settings.server_host)) != 1) {
        LOG(ERROR, "SHA512_Update() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error_reading;
    }

    if (SHA512_Final(tmp_key, &ctx) != 1) {
        LOG(ERROR, "SHA512_Final() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error_reading;
    }

    SOFT_ASSERT_LABEL(string_tohex(VS(tmp_key),
                                   key,
                                   key_size,
                                   false) == key_size - 1,
                      error_reading,
                      "string_tohex failed");
    string_tolower(key);

    if (fclose(fp) != 0) {
        LOG(ERROR, "Failed to close %s: %s (%d)",
            path, strerror(errno), errno);
        goto error_reading;
    }

    if (SHA512_Init(&ctx) != 1) {
        LOG(ERROR, "SHA512_Init() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error_reading;
    }

    if (SHA512_Update(&ctx, otp, strlen(otp)) != 1) {
        LOG(ERROR, "SHA512_Update() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error_reading;
    }

    if (SHA512_Update(&ctx, key, key_size - 1) != 1) {
        LOG(ERROR, "SHA512_Update() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error_reading;
    }

    if (SHA512_Update(&ctx, cotp, strlen(cotp)) != 1) {
        LOG(ERROR, "SHA512_Update() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error_reading;
    }

    if (SHA512_Final(tmp_key, &ctx) != 1) {
        LOG(ERROR, "SHA512_Final() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error_reading;
    }

    SOFT_ASSERT_LABEL(string_tohex(VS(tmp_key),
                                   key,
                                   key_size,
                                   false) == key_size - 1,
                      error_reading,
                      "string_tohex failed");
    string_tolower(key);

    return true;

error_reading:
    memset(key, 0, key_size);
    memset(&tmp_key, 0, sizeof(tmp_key));
    memset(&ctx, 0, sizeof(ctx));

    return false;
}

/**
 * Process the OTP GET request reply.
 *
 * @param request
 * cURL request.
 * @param user_data
 * NULL.
 */
static void
metaserver_otp_request (curl_request_t *request, void *user_data)
{
    pthread_mutex_lock(&request_lock);
    current_request = NULL;

    if (metaserver_request_process_error(request)) {
        goto out;
    }

    char *body = curl_request_get_body(request, NULL);
    if (body == NULL) {
        LOG(ERROR, "Failed to receive an OTP from metaserver");
        goto out;
    }

    const char *otp_identifier = "\"otp\": \"";
    const char *otp_pos = strstr(body, otp_identifier);
    if (otp_pos == NULL) {
        LOG(ERROR, "Malformed OTP response");
        goto out;
    }

    /* Jump over the OTP identifier */
    otp_pos += strlen(otp_identifier);

    const char *otp_end_pos = strstr(otp_pos, "\"");
    if (otp_end_pos == NULL) {
        LOG(ERROR, "Malformed OTP response");
        goto out;
    }

    size_t otp_length = otp_end_pos - otp_pos;
    if (otp_length == 0) {
        LOG(ERROR, "Malformed OTP response");
        goto out;
    }

    unsigned char cotp[32];
    if (RAND_bytes(VS(cotp)) != 1) {
        LOG(ERROR, "RAND_bytes() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto out;
    }

    unsigned char cotp_digest[SHA512_DIGEST_LENGTH];
    if (SHA512(VS(cotp), cotp_digest) == NULL) {
        LOG(ERROR, "SHA512() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto out;
    }

    char cotp_hash[SHA512_DIGEST_LENGTH * 2 + 1];
    SOFT_ASSERT_LABEL(string_tohex(VS(cotp_digest),
                                   VS(cotp_hash),
                                   false) == sizeof(cotp_hash) - 1,
                      out,
                      "string_tohex failed");
    string_tolower(cotp_hash);

    char *otp = estrndup(body + (otp_pos - body), otp_length);

    char key[SHA512_DIGEST_LENGTH * 2 + 1];
    if (!metaserver_get_key(VS(key), otp, cotp_hash)) {
        efree(otp);
        goto out;
    }

    char url[MAX_BUF];
    snprintf(VS(url), "%s/update", settings.metaserver_url);
    current_request = curl_request_create(url, CURL_PKEY_TRUST_ULTIMATE);
    curl_request_set_cb(current_request, metaserver_update_request, NULL);

    curl_request_form_add(current_request, "hostname",
                          settings.server_host);
    curl_request_form_add(current_request, "version",
                          PACKAGE_VERSION);
    curl_request_form_add(current_request, "text_comment",
                          settings.server_desc);
    curl_request_form_add(current_request, "name",
                          settings.server_name);
    curl_request_form_add(current_request, "otp",
                          otp);
    curl_request_form_add(current_request, "cotp",
                          cotp_hash);
    curl_request_form_add(current_request, "key",
                          key);
    curl_request_form_add(current_request, "ptr_check",
                          "");
    curl_request_form_add(current_request, "players",
                          request_players);

    char buf[32];
    snprintf(VS(buf), "%" PRIu32, request_num_players);
    curl_request_form_add(current_request, "num_players", buf);

    snprintf(VS(buf), "%" PRIu16, settings.port);
    curl_request_form_add(current_request, "port", buf);

    if (socket_crypto_enabled()) {
        snprintf(VS(buf), "%" PRIu16, settings.port_crypto);
        curl_request_form_add(current_request, "port_crypto", buf);

        const char *cert_pubkey = socket_crypto_get_cert_pubkey();
        if (cert_pubkey != NULL) {
            curl_request_form_add(current_request, "cert_pubkey", cert_pubkey);
        }
    }

    /* Send off the POST request */
    curl_request_start_post(current_request);

    efree(otp);

out:
    curl_request_free(request);
    pthread_mutex_unlock(&request_lock);
}

/**
 * Updates the metaserver information.
 */
void
metaserver_info_update (void)
{
    if (!metaserver_enabled()) {
        return;
    }

    pthread_mutex_lock(&request_lock);

    if (current_request != NULL) {
        curl_state_t state = curl_request_get_state(current_request);
        if (state == CURL_STATE_INPROGRESS) {
            pthread_mutex_unlock(&request_lock);
            return;
        }

        curl_request_free(current_request);
    }

    pthread_mutex_unlock(&request_lock);

    request_num_players = 0;
    StringBuffer *sb = stringbuffer_new();
    for (player *pl = first_player; pl != NULL; pl = pl->next) {
        if (stringbuffer_length(sb) != 0) {
            stringbuffer_append_string(sb, ":");
        }

        stringbuffer_append_string(sb, pl->quick_name);
        request_num_players++;
    }

    if (request_players != NULL) {
        efree(request_players);
    }
    request_players = stringbuffer_finish(sb);

    char url[MAX_BUF];
    snprintf(VS(url), "%s/otp", settings.metaserver_url);
    /* If we're at this point, no other thread is currently working with
     * the current request and thus a lock is not necessary. */
    /* coverity[missing_lock] */
    current_request = curl_request_create(url, CURL_PKEY_TRUST_ULTIMATE);
    curl_request_set_cb(current_request, metaserver_otp_request, NULL);
    curl_request_start_get(current_request);
}

/**
 * Construct metaserver statistics.
 *
 * @param[out] buf
 * Buffer to use for writing. Must end with a NUL.
 * @param size
 * Size of 'buf'.
 */
void
metaserver_stats (char *buf, size_t size)
{
    pthread_mutex_lock(&stats_lock);
    snprintfcat(buf, size, "\n=== METASERVER ===\n");
    snprintfcat(buf, size, "\nUpdates: %" PRIu64, stats.num);
    snprintfcat(buf, size, "\nFailed: %" PRIu64, stats.num_failed);

    if (stats.last != 0) {
        snprintfcat(buf, size, "\nLast update: %.19s", ctime(&stats.last));
    }

    if (stats.last_failed != 0) {
        snprintfcat(buf, size,
                    "\nLast failure: %.19s",
                    ctime(&stats.last_failed));
    }

    snprintfcat(buf, size, "\n");
    pthread_mutex_unlock(&stats_lock);
}
