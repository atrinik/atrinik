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
 * Metaserver updating related code.
 */

#include <global.h>
#include <toolkit/string.h>
#include <toolkit/curl.h>
#include <toolkit/datetime.h>
#include <toolkit/socket_crypto.h>
#include <player.h>
#include <server.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/err.h>
#include <curl/curl.h>
#include <ctype.h>

/**
 * Used to hold metaserver statistics.
 */
static struct {
    uint64_t num; ///< Number of successful updates.

    uint64_t num_failed; ///< Number of failed updates.

    uint64_t rendezvous_reconnects; ///< Rendezvous reconnect attempts.

    time_t last; ///< Last successful update.

    time_t last_failed; ///< Last failed update.
} stats;

/**
 * Where the metaserver key file is located.
 */
#define METASERVER_KEY_FILE "metaserver_key"
#define METASERVER_DIRECT_KEY_FILE "metaserver_key_direct"

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

static bool
metaserver_direct_mode (void)
{
    return strcmp(settings.connectivity_mode, "legacy_tcp") != 0;
}

static bool
metaserver_identity (char *identity, size_t identity_size)
{
    if (!metaserver_direct_mode()) {
        if (*settings.server_host == '\0') {
            return false;
        }
        return snprintf(identity,
                        identity_size,
                        "%s",
                        settings.server_host) < (int) identity_size;
    }

    char host[MAX_BUF];
    uint16_t port;
    return socket_server_quic_info(VS(host), &port, identity);
}

static void
metaserver_key_path (char *path, size_t path_size)
{
    snprintf(path,
             path_size,
             "%s/%s",
             settings.datapath,
             metaserver_direct_mode()
                 ? METASERVER_DIRECT_KEY_FILE
                 : METASERVER_KEY_FILE);
}

bool
metaserver_rendezvous_token_parse (const char *body,
                                   size_t      body_size,
                                   char        token[65])
{
    HARD_ASSERT(token != NULL);

    OPENSSL_cleanse(token, 65);
    static const char prefix[] = "\"rendezvousToken\":\"";
    const size_t required = sizeof(prefix) - 1 + 65;
    if (body == NULL || body_size < required) {
        return false;
    }

    for (size_t offset = 0; offset <= body_size - required; offset++) {
        if (memcmp(body + offset, prefix, sizeof(prefix) - 1) != 0) {
            continue;
        }
        const char *value = body + offset + sizeof(prefix) - 1;
        if (value[64] != '\"') {
            continue;
        }
        memcpy(token, value, 64);
        token[64] = '\0';
        if (string_is_hex_fixed(token, 64, true)) {
            return true;
        }
        OPENSSL_cleanse(token, 65);
    }

    return false;
}


#if LIBCURL_VERSION_NUM >= 0x075600
#define RENDEZVOUS_PUNCH_JOBS_MAX 64
#define RENDEZVOUS_PUNCH_GRACE_MS 200

static pthread_mutex_t rendezvous_lock;
static pthread_cond_t rendezvous_condition;
static pthread_t rendezvous_thread;
typedef enum rendezvous_thread_state {
    RENDEZVOUS_THREAD_STOPPED,
    RENDEZVOUS_THREAD_RUNNING,
    RENDEZVOUS_THREAD_EXITED
} rendezvous_thread_state_t;
static rendezvous_thread_state_t rendezvous_thread_state;
static bool rendezvous_shutdown;
static uint64_t rendezvous_generation;

typedef struct rendezvous_args {
    char url[HUGE_BUF];
    char token[65];
    uint64_t generation;
} rendezvous_args_t;

typedef struct rendezvous_punch_job {
    socket_punch_pacer_t pacer;
    unsigned int punches_sent;
    uint16_t port;
    char host[65];
    char ticket[65];
} rendezvous_punch_job_t;

static bool
metaserver_rendezvous_send_complete (CURL *curl, const char *ticket)
{
    char complete[128];
    if (!socket_rendezvous_message_render(VS(complete),
                                          "complete",
                                          NULL,
                                          0,
                                          SOCKET_CANDIDATE_NUM,
                                          ticket)) {
        return false;
    }
    size_t sent = 0;
    return curl_ws_send(curl,
                        complete,
                        strlen(complete),
                        &sent,
                        0,
                        CURLWS_TEXT) == CURLE_OK &&
           sent == strlen(complete);
}

static bool
metaserver_rendezvous_punch_update (CURL                     *curl,
                                    rendezvous_punch_job_t *jobs)
{
    uint64_t now = datetime_monotonic_ms();
    for (size_t i = 0; i < RENDEZVOUS_PUNCH_JOBS_MAX; i++) {
        rendezvous_punch_job_t *job = &jobs[i];
        socket_punch_action_t action = socket_punch_pacer_poll(&job->pacer,
                                                               now);
        if (action == SOCKET_PUNCH_WAIT) {
            continue;
        }

        if (action == SOCKET_PUNCH_SEND) {
            if (socket_server_quic_punch(job->host, job->port)) {
                job->punches_sent++;
            }
            socket_punch_pacer_advance(&job->pacer, now, action);
            continue;
        }

        if (!metaserver_rendezvous_send_complete(curl, job->ticket)) {
            return false;
        }
        LOG(DEBUG,
            "Completed rendezvous UDP punch window to %s:%" PRIu16
            " (sent %d/%d probes)",
            job->host,
            job->port,
            job->punches_sent,
            job->pacer.attempts);
        socket_punch_pacer_advance(&job->pacer, now, action);
    }
    return true;
}

static bool
metaserver_rendezvous_punch_schedule (rendezvous_punch_job_t *jobs,
                                      const char              *host,
                                      uint16_t                 port,
                                      const char              *ticket)
{
    rendezvous_punch_job_t *available = NULL;
    for (size_t i = 0; i < RENDEZVOUS_PUNCH_JOBS_MAX; i++) {
        if (jobs[i].pacer.active && strcmp(jobs[i].ticket, ticket) == 0) {
            available = &jobs[i];
            break;
        }
        if (!jobs[i].pacer.active && available == NULL) {
            available = &jobs[i];
        }
    }
    if (available == NULL) {
        return false;
    }

    snprintf(VS(available->host), "%s", host);
    snprintf(VS(available->ticket), "%s", ticket);
    available->port = port;
    available->punches_sent = 0;
    socket_punch_pacer_start(&available->pacer,
                             datetime_monotonic_ms(),
                             RENDEZVOUS_PUNCH_GRACE_MS);
    return true;
}

static bool
metaserver_rendezvous_current (uint64_t generation)
{
    pthread_mutex_lock(&rendezvous_lock);
    bool current = !rendezvous_shutdown &&
                   generation == rendezvous_generation;
    pthread_mutex_unlock(&rendezvous_lock);
    return current;
}

static bool
metaserver_rendezvous_wait (uint64_t generation, unsigned int timeout_ms)
{
    struct timeval now;
    GETTIMEOFDAY(&now);
    uint64_t deadline_ns = (uint64_t) now.tv_usec * 1000 +
                           (uint64_t) timeout_ms * 1000000;
    struct timespec deadline = {
        .tv_sec = now.tv_sec + (time_t) (deadline_ns / 1000000000),
        .tv_nsec = (long) (deadline_ns % 1000000000)
    };

    pthread_mutex_lock(&rendezvous_lock);
    if (!rendezvous_shutdown && generation == rendezvous_generation) {
        pthread_cond_timedwait(&rendezvous_condition,
                               &rendezvous_lock,
                               &deadline);
    }
    bool current = !rendezvous_shutdown &&
                   generation == rendezvous_generation;
    pthread_mutex_unlock(&rendezvous_lock);
    return current;
}

static int
metaserver_rendezvous_progress (void       *data,
                                curl_off_t  download_total,
                                curl_off_t  download_now,
                                curl_off_t  upload_total,
                                curl_off_t  upload_now)
{
    (void) download_total;
    (void) download_now;
    (void) upload_total;
    (void) upload_now;

    rendezvous_args_t *args = data;
    return metaserver_rendezvous_current(args->generation) ? 0 : 1;
}

static void *
metaserver_rendezvous_thread (void *data)
{
    rendezvous_args_t *args = data;

reconnect:
    ;
    CURL *curl = curl_easy_init();
    if (curl == NULL) {
        goto done;
    }

    char authorization[sizeof("Authorization: Bearer ") + 64];
    snprintf(VS(authorization), "Authorization: Bearer %s", args->token);
    struct curl_slist *headers = curl_slist_append(NULL, authorization);
    if (headers == NULL) {
        OPENSSL_cleanse(authorization, sizeof(authorization));
        curl_easy_cleanup(curl);
        goto done;
    }

    curl_easy_setopt(curl, CURLOPT_URL, args->url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 2L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl,
                     CURLOPT_XFERINFOFUNCTION,
                     metaserver_rendezvous_progress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, args);
#ifdef WIN32
    curl_easy_setopt(curl, CURLOPT_CAINFO, "ca-bundle.crt");
#endif
    CURLcode result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        LOG(ERROR, "Rendezvous connection failed: %s",
            curl_easy_strerror(result));
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        OPENSSL_cleanse(authorization, sizeof(authorization));
        if (metaserver_rendezvous_wait(args->generation, 2000)) {
            pthread_mutex_lock(&stats_lock);
            stats.rendezvous_reconnects++;
            pthread_mutex_unlock(&stats_lock);
            goto reconnect;
        }
        goto done;
    }

    rendezvous_punch_job_t punch_jobs[RENDEZVOUS_PUNCH_JOBS_MAX] = {0};
    char message[513];
    size_t used = 0;
    while (metaserver_rendezvous_current(args->generation)) {
        if (!metaserver_rendezvous_punch_update(curl, punch_jobs)) {
            break;
        }

        socket_websocket_receive_state_t receive_state =
            socket_websocket_receive(curl, VS(message), &used);
        if (receive_state == SOCKET_WEBSOCKET_EMPTY) {
            if (!metaserver_rendezvous_wait(args->generation, 20)) {
                break;
            }
            continue;
        }
        if (receive_state == SOCKET_WEBSOCKET_PARTIAL) {
            continue;
        }
        if (receive_state != SOCKET_WEBSOCKET_MESSAGE) {
            break;
        }

        char host[65], ticket[65];
        uint16_t port;
        if (socket_rendezvous_client_candidate_parse(message,
                                                      VS(host),
                                                      &port,
                                                      ticket)) {
            socket_direct_candidate_t
                candidates[SOCKET_DIRECT_MAX_CANDIDATES];
            size_t count = socket_server_quic_candidates(
                candidates,
                arraysize(candidates));
            for (size_t i = 0; i < count; i++) {
                char response[256];
                if (!socket_rendezvous_message_render(
                        VS(response),
                        "server_candidate",
                        candidates[i].host,
                        candidates[i].port,
                        candidates[i].kind,
                        ticket)) {
                    break;
                }
                size_t sent = 0;
                if (curl_ws_send(curl,
                                 response,
                                 strlen(response),
                                 &sent,
                                 0,
                                 CURLWS_TEXT) != CURLE_OK ||
                    sent != strlen(response)) {
                    break;
                }
            }

            LOG(INFO,
                "Opening a rendezvous UDP path to client candidate %s:%" PRIu16,
                host,
                port);
            if (!metaserver_rendezvous_punch_schedule(punch_jobs,
                                                       host,
                                                       port,
                                                       ticket)) {
                LOG(ERROR, "Rendezvous UDP punch queue is full");
                metaserver_rendezvous_send_complete(curl, ticket);
            }
        }
        used = 0;
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    OPENSSL_cleanse(authorization, sizeof(authorization));
    if (metaserver_rendezvous_wait(args->generation, 2000)) {
        pthread_mutex_lock(&stats_lock);
        stats.rendezvous_reconnects++;
        pthread_mutex_unlock(&stats_lock);
        goto reconnect;
    }

done:
    pthread_mutex_lock(&rendezvous_lock);
    rendezvous_thread_state = RENDEZVOUS_THREAD_EXITED;
    pthread_cond_broadcast(&rendezvous_condition);
    pthread_mutex_unlock(&rendezvous_lock);
    OPENSSL_cleanse(args->token, sizeof(args->token));
    efree(args);
    return NULL;
}

static bool
metaserver_rendezvous_url (char       *url,
                           size_t      url_size)
{
    char host[MAX_BUF], quic_fingerprint[65];
    uint16_t port;
    if (!settings.server_public ||
        !socket_server_quic_info(VS(host), &port, quic_fingerprint)) {
        return false;
    }

    return socket_rendezvous_url(settings.metaserver_url,
                                 quic_fingerprint,
                                 "server",
                                 url,
                                 url_size);
}

static void
metaserver_rendezvous_start (const char *token)
{
    rendezvous_args_t *args = ecalloc(1, sizeof(*args));
    snprintf(VS(args->token), "%s", token);
    if (!metaserver_rendezvous_url(VS(args->url))) {
        OPENSSL_cleanse(args->token, sizeof(args->token));
        efree(args);
        return;
    }

    pthread_mutex_lock(&rendezvous_lock);
    rendezvous_generation++;
    pthread_cond_broadcast(&rendezvous_condition);
    args->generation = rendezvous_generation;
    bool join_old = rendezvous_thread_state != RENDEZVOUS_THREAD_STOPPED;
    pthread_t old_thread = rendezvous_thread;
    pthread_mutex_unlock(&rendezvous_lock);

    if (join_old) {
        pthread_join(old_thread, NULL);
    }

    pthread_mutex_lock(&rendezvous_lock);
    rendezvous_thread_state = RENDEZVOUS_THREAD_STOPPED;
    if (rendezvous_shutdown || args->generation != rendezvous_generation) {
        pthread_mutex_unlock(&rendezvous_lock);
        OPENSSL_cleanse(args->token, sizeof(args->token));
        efree(args);
        return;
    }
    int error = pthread_create(&rendezvous_thread,
                               NULL,
                               metaserver_rendezvous_thread,
                               args);
    if (error != 0) {
        LOG(ERROR, "Failed to start the rendezvous thread");
        rendezvous_thread_state = RENDEZVOUS_THREAD_STOPPED;
        pthread_mutex_unlock(&rendezvous_lock);
        OPENSSL_cleanse(args->token, sizeof(args->token));
        efree(args);
        return;
    }
    rendezvous_thread_state = RENDEZVOUS_THREAD_RUNNING;
    pthread_mutex_unlock(&rendezvous_lock);
}

static void
metaserver_rendezvous_response (curl_request_t *request)
{
    size_t body_size = 0;
    char *body = curl_request_get_body(request, &body_size);
    char value[65];
    if (!metaserver_rendezvous_token_parse(body, body_size, value)) {
        return;
    }
    metaserver_rendezvous_start(value);
    OPENSSL_cleanse(value, sizeof(value));
}
#endif

/**
 * Figure out whether the meta-server is enabled or not.
 *
 * @return
 * True if the meta-server is enabled, false otherwise.
 */
static bool
metaserver_enabled (void)
{
    char identity[MAX_BUF];
    if (!metaserver_identity(VS(identity))) {
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

    pthread_mutex_init(&stats_lock, NULL);
    pthread_mutex_init(&request_lock, NULL);
#if LIBCURL_VERSION_NUM >= 0x075600
    pthread_mutex_init(&rendezvous_lock, NULL);
    pthread_cond_init(&rendezvous_condition, NULL);
    rendezvous_thread_state = RENDEZVOUS_THREAD_STOPPED;
    rendezvous_shutdown = false;
    rendezvous_generation = 0;
#endif
    metaserver_info_update();
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

#if LIBCURL_VERSION_NUM >= 0x075600
    pthread_mutex_lock(&rendezvous_lock);
    rendezvous_shutdown = true;
    rendezvous_generation++;
    pthread_cond_broadcast(&rendezvous_condition);
    bool join_rendezvous =
        rendezvous_thread_state != RENDEZVOUS_THREAD_STOPPED;
    pthread_t thread = rendezvous_thread;
    pthread_mutex_unlock(&rendezvous_lock);
    if (join_rendezvous) {
        pthread_join(thread, NULL);
    }
    pthread_mutex_lock(&rendezvous_lock);
    rendezvous_thread_state = RENDEZVOUS_THREAD_STOPPED;
    pthread_mutex_unlock(&rendezvous_lock);
    pthread_cond_destroy(&rendezvous_condition);
    pthread_mutex_destroy(&rendezvous_lock);
#endif

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
            metaserver_key_path(VS(path));

            if (unlink(path) != 0) {
                LOG(ERROR, "Failed to unlink %s: %s (%d)",
                    path, strerror(errno), errno);
            }

            key_is_new = false;
        }

        goto out;
    }

#if LIBCURL_VERSION_NUM >= 0x075600
    metaserver_rendezvous_response(request);
#endif

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
    SHA512_CTX ctx = {0};

    char path[HUGE_BUF];
    metaserver_key_path(VS(path));
    FILE *fp = fopen(path, "rb");
    if (fp == NULL && errno == ENOENT) {
        int fd = open(path,
                      O_WRONLY | O_CREAT | O_EXCL,
                      S_IRUSR | S_IWUSR);
        if (fd == -1) {
            LOG(ERROR, "Failed to create %s: %s (%d)",
                path, strerror(errno), errno);
            return false;
        }

        fp = fdopen(fd, "wb");
        if (fp == NULL) {
            int saved_errno = errno;
            close(fd);
            if (unlink(path) != 0) {
                LOG(ERROR, "Failed to unlink %s: %s (%d)",
                    path, strerror(errno), errno);
            }
            LOG(ERROR, "Failed to open %s for writing: %s (%d)",
                path, strerror(saved_errno), saved_errno);
            return false;
        }

        unsigned char bytes[64];

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

        int close_result = fclose(fp);
        fp = NULL;
        if (close_result != 0) {
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

        if (fp != NULL) {
            fclose(fp);
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

    char identity[65];
    if (!metaserver_identity(VS(identity)) ||
        SHA512_Update(&ctx, identity, strlen(identity)) != 1) {
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

    int close_result = fclose(fp);
    fp = NULL;
    if (close_result != 0) {
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
    if (fp != NULL) {
        fclose(fp);
    }
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
    current_request = curl_request_create(url, CURL_PKEY_TRUST_SYSTEM);
    curl_request_set_cb(current_request, metaserver_update_request, NULL);

    if (*settings.server_host != '\0') {
        curl_request_form_add(current_request,
                              "hostname",
                              settings.server_host);
    }
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
    curl_request_form_add(current_request, "registration",
                          key_is_new ? "1" : "0");
    curl_request_form_add(current_request, "ptr_check",
                          "");
    curl_request_form_add(current_request, "players",
                          request_players);

    char buf[32];
    snprintf(VS(buf), "%" PRIu32, request_num_players);
    curl_request_form_add(current_request, "num_players", buf);
    curl_request_form_add(current_request,
                          "public",
                          settings.server_public ? "1" : "0");
    curl_request_form_add(current_request,
                          "connectivity_mode",
                          settings.connectivity_mode);
    curl_request_form_add(current_request,
                          "password_required",
                          *settings.join_password != '\0' ? "1" : "0");

    char quic_host[MAX_BUF];
    uint16_t quic_port;
    char quic_fingerprint[65];
    if (socket_server_quic_info(VS(quic_host),
                                &quic_port,
                                quic_fingerprint)) {
        curl_request_form_add(current_request, "server_id", quic_fingerprint);
        curl_request_form_add(current_request,
                              "quic_host",
                              quic_host);
        snprintf(VS(buf), "%" PRIu16, quic_port);
        curl_request_form_add(current_request, "quic_port", buf);
        curl_request_form_add(current_request,
                              "quic_cert_sha256",
                              quic_fingerprint);
    }

    snprintf(VS(buf), "%" PRIu16, settings.port);
    curl_request_form_add(current_request, "port", buf);

    if (socket_crypto_enabled()) {
        snprintf(VS(buf), "%" PRIu16, settings.port_crypto);
        curl_request_form_add(current_request, "port_crypto", buf);

        const char *cert_pubkey = socket_crypto_get_cert_pubkey();
        if (cert_pubkey != NULL) {
            curl_request_form_add(current_request, "cert_pubkey", cert_pubkey);
        }

        /* Add the server certificate and its signature, if configured. */
        if (settings.server_cert != NULL &&
            settings.server_cert_sig != NULL) {
            curl_request_form_add(current_request,
                                  "server_cert",
                                  settings.server_cert);
            curl_request_form_add(current_request,
                                  "server_cert_sig",
                                  settings.server_cert_sig);
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
    current_request = curl_request_create(url, CURL_PKEY_TRUST_SYSTEM);
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
    snprintfcat(buf,
                size,
                "\nRendezvous reconnects: %" PRIu64,
                stats.rendezvous_reconnects);

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
