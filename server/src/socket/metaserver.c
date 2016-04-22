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
 * cURL request structure.
 */
static curl_request_t *request = NULL;

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

    if (request != NULL) {
        curl_request_free(request);
        request = NULL;
    }
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

    if (request != NULL) {
        curl_state_t state = curl_request_get_state(request);
        if (state == CURL_STATE_INPROGRESS) {
            return;
        }

        int http_code = curl_request_get_http_code(request);
        if (state == CURL_STATE_ERROR ||
            http_code != 200) {
            char *body = curl_request_get_body(request, NULL);
            LOG(SYSTEM,
                "Failed to update metaserver information "
                "(HTTP code: %d), response: %s",
                http_code,
                body != NULL ? body : "<empty>");

            stats.last_failed = time(NULL);
            stats.num_failed++;
        } else if (state == CURL_STATE_OK) {
            stats.last = time(NULL);
            stats.num++;
        }

        curl_request_free(request);
    }

    uint32_t num_players = 0;
    StringBuffer *sb = stringbuffer_new();
    for (player *pl = first_player; pl != NULL; pl = pl->next) {
        if (stringbuffer_length(sb) != 0) {
            stringbuffer_append_string(sb, ":");
        }

        stringbuffer_append_string(sb, pl->quick_name);
        num_players++;
    }

    request = curl_request_create(settings.metaserver_url,
                                  CURL_PKEY_TRUST_ULTIMATE);
    curl_request_form_add(request, "hostname", settings.server_host);
    curl_request_form_add(request, "version", PACKAGE_VERSION);
    curl_request_form_add(request, "text_comment", settings.server_desc);
    curl_request_form_add(request, "name", settings.server_name);

    char *players = stringbuffer_finish(sb);
    curl_request_form_add(request, "players", players);
    efree(players);

    char buf[32];
    snprintf(VS(buf), "%" PRIu32, num_players);
    curl_request_form_add(request, "num_players", buf);

    snprintf(VS(buf), "%" PRIu16, settings.port);
    curl_request_form_add(request, "port", buf);

    snprintf(VS(buf), "%" PRIu16, settings.port_crypto);
    curl_request_form_add(request, "port_crypto", buf);

    /* Send off the POST request */
    curl_request_start_post(request);
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
}
