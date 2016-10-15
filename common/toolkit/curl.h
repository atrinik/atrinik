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
 * CURL API header file.
 *
 * @author Alex Tokar
 */

#ifndef CURL_H
#define CURL_H

/**
 * Connection timeout in seconds.
 */
#define CURL_TIMEOUT 15

/**
 * Possible cURL request states.
 */
typedef enum curl_state {
    CURL_STATE_NONE, ///< No state.
    CURL_STATE_INPROGRESS, ///< Requesting data.
    CURL_STATE_OK, ///< cURL thread finished and the data is ready to be used.
    CURL_STATE_ERROR, ///< An error occurred trying to process the request.
} curl_state_t;

typedef enum curl_request_process {
    CURL_REQUEST_PROCESS_RX,
    CURL_REQUEST_PROCESS_TX,
} curl_request_process_t;

typedef enum curl_pkey_trust {
    /**
     * Ultimate trust public keys (specified with the --trusted_pin option).
     */
    CURL_PKEY_TRUST_ULTIMATE,
    /**
     * Application provided public keys (eg, the Atrinik server).
     */
    CURL_PKEY_TRUST_APPLICATION,

    /**
     * Number of trust types.
     */
    CURL_PKEY_TRUST_NUM
} curl_pkey_trust_t;

/**
 * Possible values to use with curl_request_sizeinfo().
 */
typedef enum curl_info {
    CURL_INFO_DL_LENGTH, ///< Size of request in bytes. Can be -1.
    CURL_INFO_DL_SPEED, ///< Request speed in Bps.
    CURL_INFO_DL_SIZE, ///< Number of bytes requested so far.
} curl_info_t;

typedef void (*curl_request_process_cb)(curl_request_process_t type,
                                        size_t                 size);

/**
 * Opaque definition for the ::curl_request structure.
 */
typedef struct curl_request curl_request_t;

typedef void (*curl_request_cb)(curl_request_t *request, void *user_data);

/* Prototypes */

TOOLKIT_FUNCS_DECLARE(curl);

void
curl_set_user_agent(const char *user_agent);
bool
curl_set_trust_application(const char *pubkey);
void
curl_set_data_dir(const char *dir);
curl_request_t *
curl_request_create(const char *url, curl_pkey_trust_t trust);
void
curl_request_form_add(curl_request_t *request,
                      const char     *key,
                      const char     *value);
void
curl_request_set_path(curl_request_t *request, const char *path);
void
curl_request_set_cb(curl_request_t *request,
                    curl_request_cb cb,
                    void           *user_data);
void
curl_request_set_delay(curl_request_t *request,
                       uint32_t        delay);
curl_state_t
curl_request_get_state(curl_request_t *request);
char *
curl_request_get_body(curl_request_t *request, size_t *body_size);
char *
curl_request_get_header(curl_request_t *request, size_t *header_size);
int
curl_request_get_http_code(curl_request_t *request);
const char *
curl_request_get_url(curl_request_t *request);
int64_t
curl_request_sizeinfo(curl_request_t *request, curl_info_t info);
char *
curl_request_speedinfo(curl_request_t *request, char *buf, size_t bufsize);
void
curl_request_free(curl_request_t *request);
void *
curl_request_do_get(void *user_data);
void
curl_request_start_get(curl_request_t *request);
void *
curl_request_do_post(void *user_data);
void
curl_request_start_post(curl_request_t *request);
bool
curl_verify(curl_pkey_trust_t    trust,
            const char          *msg,
            size_t               msg_len,
            const unsigned char *sig,
            size_t               sig_len);

#endif
