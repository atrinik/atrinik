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
 * HTTP server module.
 *
 * @author
 * Alex Tokar
 */

#include <global.h>
#include <http_server.h>
#include <toolkit/process.h>
#include <toolkit/curl.h>
#include <toolkit/packet.h>

TOOLKIT_API(DEPENDS(process), IMPORTS(logger));

/**
 * Current cURL request.
 */
static curl_request_t *current_request;

/**
 * Mutex for the current request pointer.
 */
static pthread_mutex_t request_lock;

/** @copydoc process_data_callback_t */
static void
http_data_cb (process_t *process, uint8_t *data, size_t len)
{
    char buf[HUGE_BUF * 4];
    size_t pos = 0;
    while (packet_to_string(data, len, &pos, VS(buf)) != NULL) {
        LOG(HTTP, "%s", buf);
    }
}

/** @copydoc curl_request_cb */
static void
http_curl_cb (curl_request_t *request, void *user_data)
{
    pthread_mutex_lock(&request_lock);
    current_request = NULL;
    pthread_mutex_unlock(&request_lock);

    if (curl_request_get_state(request) != CURL_STATE_OK) {
        int code = curl_request_get_http_code(request);
        LOG(ERROR,
            "Failed to connect to local HTTP server at %s; ensure the HTTP "
            "server is running and accessible (HTTP code: %d).",
            settings.http_url,
            code);
    }

    curl_request_free(request);
}

/**
 * Initialize the HTTP server.
 */
TOOLKIT_INIT_FUNC(http_server)
{
    if (settings.http_server) {
        process_t *process = process_create("python");
        process_add_arg(process, "tools/http_server.py");
        process_set_data_out_cb(process, http_data_cb);
        process_set_data_err_cb(process, http_data_cb);
        process_set_restart(process, true);

        if (!process_start(process)) {
            LOG(ERROR, "Failed to start the HTTP server process.");
            exit(EXIT_FAILURE);
        }

        process_check(process);

        if (!process_is_running(process)) {
            LOG(ERROR,
                "Failed to start up the HTTP server; please consult the "
                "README file.");
        }
    }

    pthread_mutex_init(&request_lock, NULL);

    pthread_mutex_lock(&request_lock);
    /* Verify the HTTP server is running. */
    current_request = curl_request_create(settings.http_url,
                                          CURL_PKEY_TRUST_APPLICATION);
    curl_request_set_cb(current_request, http_curl_cb, NULL);
    curl_request_set_delay(current_request, 100000);
    curl_request_start_get(current_request);
    pthread_mutex_unlock(&request_lock);
}
TOOLKIT_INIT_FUNC_FINISH

/**
 * Deinitialize the HTTP server.
 */
TOOLKIT_DEINIT_FUNC(http_server)
{
    pthread_mutex_lock(&request_lock);
    if (current_request != NULL) {
        pthread_mutex_unlock(&request_lock);
        curl_state_t state;
        do {
            pthread_mutex_lock(&request_lock);
            if (current_request == NULL) {
                pthread_mutex_unlock(&request_lock);
                break;
            }
            state = curl_request_get_state(current_request);
            pthread_mutex_unlock(&request_lock);
            sleep(1);
        } while (state == CURL_STATE_INPROGRESS);

        /* No other thread is working with the current request at this
         * point. */
        if (current_request != NULL) {
            curl_request_free(current_request);
            current_request = NULL;
        }
    } else {
        pthread_mutex_unlock(&request_lock);
    }

    pthread_mutex_destroy(&request_lock);
}
TOOLKIT_DEINIT_FUNC_FINISH
