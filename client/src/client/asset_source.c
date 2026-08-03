/**
 * @file
 * Asynchronous HTTP-first asset source with in-band QUIC fallback.
 */

#include <global.h>
#include <asset_source.h>
#include <toolkit/string.h>
#include <curl/curl.h>

struct asset_source {
    char *asset_path;
    char *cache_path;
    char error[128];
    curl_request_t *http;
    asset_request_t *inband;
    asset_source_state_t state;
};

static bool
asset_source_url (const char *asset_path, char *url, size_t size)
{
    CURLU *parsed = curl_url();
    char *base_path = NULL;
    char *rendered = NULL;
    bool ok = parsed != NULL &&
              curl_url_set(parsed, CURLUPART_URL, cpl.http_url, 0) == CURLUE_OK;
    if (ok && curl_url_get(parsed,
                           CURLUPART_PATH,
                           &base_path,
                           CURLU_URLDECODE) != CURLUE_OK) {
        ok = false;
    }
    char path[HUGE_BUF];
    if (ok) {
        int length = snprintf(VS(path),
                              "%s%s%s",
                              base_path != NULL ? base_path : "",
                              base_path != NULL && *base_path != '\0' &&
                              base_path[strlen(base_path) - 1] != '/'
                                  ? "/" : "",
                              asset_path);
        ok = length >= 0 && (size_t) length < sizeof(path) &&
             curl_url_set(parsed, CURLUPART_PATH, path, 0) == CURLUE_OK &&
             curl_url_set(parsed, CURLUPART_QUERY, NULL, 0) == CURLUE_OK &&
             curl_url_set(parsed, CURLUPART_FRAGMENT, NULL, 0) == CURLUE_OK &&
             curl_url_get(parsed, CURLUPART_URL, &rendered, 0) == CURLUE_OK;
    }
    if (ok) {
        int length = snprintf(url, size, "%s", rendered);
        ok = length >= 0 && (size_t) length < size;
    }
    curl_free(base_path);
    curl_free(rendered);
    if (parsed != NULL) {
        curl_url_cleanup(parsed);
    }
    return ok;
}

static bool
asset_source_start_inband (asset_source_t *source)
{
    source->inband = source->cache_path != NULL
        ? asset_request_start_cached(source->asset_path, source->cache_path)
        : asset_request_start(source->asset_path);
    return source->inband != NULL;
}

asset_source_t *
asset_source_start (const char *asset_path, const char *cache_path)
{
    HARD_ASSERT(asset_path != NULL);

    asset_source_t *source = ecalloc(1, sizeof(*source));
    source->asset_path = estrdup(asset_path);
    source->cache_path = cache_path != NULL ? estrdup(cache_path) : NULL;
    source->state = ASSET_SOURCE_PENDING;

    if (*cpl.http_url != '\0') {
        char url[HUGE_BUF];
        if (asset_source_url(asset_path, VS(url))) {
            source->http = curl_request_create(url,
                                               CURL_PKEY_TRUST_APPLICATION);
            curl_request_set_max_body(source->http, ASSET_MAX_SIZE);
            if (cache_path != NULL) {
                curl_request_set_path(source->http, cache_path);
            }
            curl_request_start_get(source->http);
            return source;
        }
    }
    if (!asset_source_start_inband(source)) {
        source->state = ASSET_SOURCE_ERROR;
        snprintf(VS(source->error), "No available asset transport");
    }
    return source;
}

asset_source_state_t
asset_source_get_state (asset_source_t *source)
{
    HARD_ASSERT(source != NULL);
    if (source->state != ASSET_SOURCE_PENDING) {
        return source->state;
    }

    if (source->http != NULL) {
        if (curl_request_get_state(source->http) == CURL_STATE_INPROGRESS) {
            return source->state;
        }
        size_t size = 0;
        const void *body = curl_request_get_body(source->http, &size);
        if (curl_request_get_http_code(source->http) == 200 &&
                body != NULL && size <= ASSET_MAX_SIZE) {
            source->state = ASSET_SOURCE_COMPLETE;
            return source->state;
        }
        curl_request_free(source->http);
        source->http = NULL;
        if (cpl.asset_transport && asset_source_start_inband(source)) {
            return source->state;
        }
        source->state = ASSET_SOURCE_ERROR;
        snprintf(VS(source->error),
                 "HTTP failed and QUIC fallback is unavailable");
        return source->state;
    }

    if (source->inband != NULL) {
        asset_request_state_t state = asset_request_get_state(source->inband);
        if (state == ASSET_REQUEST_COMPLETE) {
            source->state = ASSET_SOURCE_COMPLETE;
        } else if (state == ASSET_REQUEST_ERROR) {
            source->state = ASSET_SOURCE_ERROR;
            snprintf(VS(source->error), "In-band asset transfer failed");
        }
    }
    return source->state;
}

const uint8_t *
asset_source_get_data (asset_source_t *source, size_t *size)
{
    if (asset_source_get_state(source) != ASSET_SOURCE_COMPLETE) {
        return NULL;
    }
    if (source->http != NULL) {
        return (const uint8_t *) curl_request_get_body(source->http, size);
    }
    return asset_request_get_data(source->inband, size);
}

const char *
asset_source_get_error (const asset_source_t *source)
{
    HARD_ASSERT(source != NULL);
    return source->error;
}

char *
asset_source_speedinfo (asset_source_t *source, char *buffer, size_t size)
{
    HARD_ASSERT(source != NULL);
    HARD_ASSERT(buffer != NULL);
    if (source->http != NULL) {
        return curl_request_speedinfo(source->http, buffer, size);
    }
    snprintf(buffer,
             size,
             "%s",
             source->inband != NULL
                 ? "Using the game connection"
                 : source->error);
    return buffer;
}

void
asset_source_free (asset_source_t *source)
{
    if (source == NULL) {
        return;
    }
    if (source->http != NULL) {
        curl_request_free(source->http);
    }
    if (source->inband != NULL) {
        asset_request_free(source->inband);
    }
    efree(source->cache_path);
    efree(source->asset_path);
    efree(source);
}
