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
 * Initialize the metaserver.
 */
void
metaserver_init (void)
{
    if (*settings.server_host == '\0') {
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
    if (*settings.server_host == '\0') {
        return;
    }

    if (current_request != NULL) {
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

static void
metaserver_update_request (curl_request_t *request, void *user_data)
{
    pthread_mutex_lock(&request_lock);
    current_request = NULL;

    if (metaserver_request_process_error(request)) {
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

    char *otp = estrndup(body + (otp_pos - body), otp_length);

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
    curl_request_form_add(current_request, "players",
                          request_players);

    char buf[32];
    snprintf(VS(buf), "%" PRIu32, request_num_players);
    curl_request_form_add(current_request, "num_players", buf);

    snprintf(VS(buf), "%" PRIu16, settings.port);
    curl_request_form_add(current_request, "port", buf);

    snprintf(VS(buf), "%" PRIu16, settings.port_crypto);
    curl_request_form_add(current_request, "port_crypto", buf);

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
    if (*settings.server_host == '\0') {
        return;
    }

    if (current_request != NULL) {
        curl_state_t state = curl_request_get_state(current_request);
        if (state == CURL_STATE_INPROGRESS) {
            return;
        }

        curl_request_free(current_request);
    }

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
