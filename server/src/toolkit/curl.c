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
 * cURL API.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <toolkit_string.h>
#include <curl.h>
#include <curl/curl.h>
#include <path.h>
#include <clioptions.h>

TOOLKIT_API(DEPENDS(clioptions), IMPORTS(memory));

/**
 * Wrapper around curl_easy_setopt() that performs error checking.
 *
 * @param handle
 * cURL handle.
 * @param option
 * Option to set.
 * @param value
 * Value to set.
 */
#define CURL_SETOPT(handle, option, value)                              \
do {                                                                    \
    CURLcode res = curl_easy_setopt(handle, option, value);             \
    if (res != CURLE_OK) {                                              \
        LOG(ERROR, "Failed to set " STRINGIFY(option) ": %s (%d)",      \
            curl_easy_strerror(res), res);                              \
        return CURL_STATE_ERROR;                                        \
    }                                                                   \
} while (0)

/**
 * cURL request structure.
 */
struct curl_request {
    /** The data. Can be NULL in case we got no data from the URL. */
    char *body;

    /** Size of the data. */
    size_t body_size;

    /** HTTP headers. */
    char *header;

    /** Size of HTTP headers. */
    size_t header_size;

    /** URL used. */
    char *url;

    /** Path to cached file. */
    char *path;

    /**
     * Mutex to protect the data in this structure when accessed across
     * threads.
     */
    pthread_mutex_t mutex;

    /**
     * The thread ID.
     */
    pthread_t thread_id;

    /**
     * State of the request.
     */
    curl_state_t state;

    /**
     * Trust store to use.
     */
    curl_pkey_trust_t trust;

    /**
     * Will contain HTTP code.
     */
    int http_code;

    /**
     * cURL handle being used.
     */
    CURL *handle;

    /**
     * POST form data.
     */
    struct curl_httppost *form_post;

    /**
     * Pointer to last form data.
     */
    struct curl_httppost *form_post_last;

    /**
     * Used to keep track of which certificate is being processed.
     */
    uint8_t cert_id;

    /**
     * Certificate public key verification chain.
     */
    uint32_t cert_chain;

    /**
     * Certificate's common name.
     */
    char *cert_cn;

    /**
     * True if the thread is quitting.
     */
    bool finished:1;

    /**
     * Whether the peer certificate is untrusted.
     */
    bool untrusted:1;
};

/**
 * cURL public key trust store database structure.
 */
typedef struct curl_trust_store {
    struct curl_trust_store *next; ///< Next key.
    EVP_PKEY *key; ///< The public key.
} curl_trust_store_t;

/** Shared handle. */
static CURLSH *handle_share = NULL;
/** Mutex to protect the shared handle. */
static pthread_mutex_t handle_share_mutex;
/** Data processing callback function. Can be used for statistics. */
static curl_request_process_cb process_cb;
/** User agent to use for cURL requests.. */
static char *curl_user_agent = NULL;
/** The trusted public keys store. */
static curl_trust_store_t *curl_trust_pkeys[CURL_PKEY_TRUST_NUM] = {};
/** cURL data directory. */
static char *curl_data_dir = NULL;

/**
 * Lock the share handle.
 */
static void
curl_share_lock (CURL            *handle,
                 curl_lock_data   data,
                 curl_lock_access lock_access,
                 void            *userptr)
{
    pthread_mutex_lock(userptr);
}

/**
 * Unlock the share handle.
 */
static void
curl_share_unlock (CURL          *handle,
                   curl_lock_data data,
                   void          *userptr)
{
    pthread_mutex_unlock(userptr);
}

/**
 * Frees a single cURL trust store entry.
 *
 * @param store
 * Entry to free.
 */
static void
curl_free_store (curl_trust_store_t *store)
{
    HARD_ASSERT(store != NULL);

    if (store->key != NULL) {
        EVP_PKEY_free(store->key);
    }

    efree(store);
}

/**
 * Load a public key in PEM format to the trust store.
 *
 * @param pubkey
 * Public key. Must be in PEM format and NUL-terminated.
 * @param trust
 * Trust store to load into.
 * @param[out] errmsg
 * Will contain an error message on failure.
 * @return
 * True on success, false on failure.
 */
static bool
curl_load_pem (const char *pubkey, curl_pkey_trust_t trust, char **errmsg)
{
    HARD_ASSERT(pubkey != NULL);

    bool ret = false;
    /* Older versions of OpenSSL do not have const-correctness for
     * BIO_new_mem_buf(), so make a copy of it. */
    char *cp = estrdup(pubkey);
    curl_trust_store_t *store = NULL;

    RSA *key = NULL;
    BIO *key_bio = BIO_new_mem_buf(cp, -1);
    if (key_bio == NULL) {
        *errmsg = estrdup("Failed to create a BIO");
        goto out;
    }

    store = ecalloc(1, sizeof(*store));
    store->key = EVP_PKEY_new();
    if (store->key == NULL) {
        *errmsg = estrdup("Failed to create an EVP_PKEY");
        goto out;
    }

    key = PEM_read_bio_RSA_PUBKEY(key_bio, NULL, NULL, NULL);
    if (key == NULL) {
        *errmsg = estrdup("Failed to load RSA public key; ensure it's in a "
                          "valid PEM format");
        goto out;
    }

    if (!EVP_PKEY_set1_RSA(store->key, key)) {
        string_fmt(*errmsg, "Setting RSA key to trusted store %d failed",
                   trust);
        goto out;
    }

    LL_APPEND(curl_trust_pkeys[trust], store);

    ret = true;

out:
    efree(cp);

    if (key_bio != NULL) {
        BIO_free(key_bio);
    }

    if (key != NULL) {
        RSA_free(key);
    }

    if (!ret && store != NULL) {
        curl_free_store(store);
    }

    return ret;
}

/**
 * Description of the --trusted_pin command.
 */
static const char *const clioptions_option_trusted_pin_desc =
"Adds a new trusted public key (pin) that will be used to validate HTTPS "
"certificates (in a fashion similar to HPKP).\n\n"
"Usage:\n"
" --trusted_pin=<public-key>";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_trusted_pin (const char *arg,
                               char      **errmsg)
{
    return curl_load_pem(arg, CURL_PKEY_TRUST_ULTIMATE, errmsg);
}

/**
 * Initialize the cURL API.
 */
TOOLKIT_INIT_FUNC(curl)
{
    curl_global_init(CURL_GLOBAL_ALL);
    pthread_mutex_init(&handle_share_mutex, NULL);

    handle_share = curl_share_init();
    curl_share_setopt(handle_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
    curl_share_setopt(handle_share, CURLSHOPT_USERDATA, handle_share_mutex);
    curl_share_setopt(handle_share, CURLSHOPT_LOCKFUNC, curl_share_lock);
    curl_share_setopt(handle_share, CURLSHOPT_UNLOCKFUNC, curl_share_unlock);

    /* Set a default user agent. */
    curl_set_user_agent("Atrinik cURL API");
    /* Set a default data directory */
    curl_set_data_dir("data/");

    clioption_t *cli;
    CLIOPTIONS_CREATE_ARGUMENT(cli, trusted_pin, "Add a trusted public key");
}
TOOLKIT_INIT_FUNC_FINISH

/**
 * Deinitialize the cURL API.
 */
TOOLKIT_DEINIT_FUNC(curl)
{
    curl_share_cleanup(handle_share);
    pthread_mutex_destroy(&handle_share_mutex);
    curl_global_cleanup();

    for (curl_pkey_trust_t trust = 0; trust < CURL_PKEY_TRUST_NUM; trust++) {
        curl_trust_store_t *store, *tmp;
        LL_FOREACH_SAFE(curl_trust_pkeys[trust], store, tmp) {
            curl_free_store(store);
        }

        curl_trust_pkeys[trust] = NULL;
    }

    efree(curl_user_agent);
    curl_user_agent = NULL;
    efree(curl_data_dir);
    curl_data_dir = NULL;
}
TOOLKIT_DEINIT_FUNC_FINISH

/**
 * Change the user agent used by the cURL API.
 *
 * @param user_agent
 * User agent to use.
 */
void
curl_set_user_agent (const char *user_agent)
{
    HARD_ASSERT(user_agent != NULL);

    if (curl_user_agent != NULL) {
        efree(curl_user_agent);
    }

    curl_user_agent = estrdup(user_agent);
}

/**
 * Set directory where to store data.
 *
 * @param dir
 * Directory.
 */
void
curl_set_data_dir (const char *dir)
{
    HARD_ASSERT(dir != NULL);

    if (curl_data_dir != NULL) {
        efree(curl_data_dir);
    }

    curl_data_dir = estrdup(dir);
}

/**
 * Set public key for application-related cURL requests.
 *
 * @param pubkey
 * Public key. NULL to merely clear the last public key that was set using
 * this call.
 * @return
 * True on success, false on failure.
 */
bool
curl_set_trust_application (const char *pubkey)
{
    if (curl_trust_pkeys[CURL_PKEY_TRUST_APPLICATION] != NULL) {
        curl_free_store(curl_trust_pkeys[CURL_PKEY_TRUST_APPLICATION]);
        curl_trust_pkeys[CURL_PKEY_TRUST_APPLICATION] = NULL;
    }

    if (pubkey == NULL) {
        return false;
    }

    char *errmsg;
    if (!curl_load_pem(pubkey, CURL_PKEY_TRUST_APPLICATION, &errmsg)) {
        LOG(ERROR, "Failed to load public key: %s", errmsg);
        efree(errmsg);
        return false;
    }

    return true;
}

/**
 * Attempts to load an ETag for the requested file.
 *
 * @param request
 * cURL request structure.
 * @return
 * ETag on success, NULL on failure. Must be freed.
 */
static char *
curl_load_etag (curl_request_t *request)
{
    HARD_ASSERT(request != NULL);

    if (request->path == NULL) {
        /* No file cache location specified */
        return NULL;
    }

    char path[HUGE_BUF];
    snprintf(VS(path), "%s.etag", request->path);
    FILE *fp = path_fopen(path, "r");
    char *etag = NULL;

    if (fp == NULL) {
        /* File doesn't exist. */
        goto fail;
    }

    struct stat statbuf;
    if (fstat(fileno(fp), &statbuf) == -1) {
        LOG(ERROR, "Could not stat %s: %d (%s)", path, errno, strerror(errno));
        goto fail;
    }

    etag = emalloc(sizeof(*etag) * (statbuf.st_size + 1));

    if (fgets(etag, statbuf.st_size + 1, fp) == NULL) {
        LOG(ERROR, "Could not read %s: %d (%s)", path, errno, strerror(errno));
        goto fail;
    }

    goto done;

fail:
    /* Free the etag on failure, if any. */
    if (etag != NULL) {
        efree(etag);
        etag = NULL;
    }

done:
    if (fp != NULL) {
        fclose(fp);
    }

    return etag;
}

/**
 * Fills up the cURL request structure from cached data.
 *
 * @param request
 * cURL request structure.
 * @return
 * True on success, false on failure.
 * @warning
 * This function expects the cURL request mutex to be locked.
 */
static bool
curl_load_cache (curl_request_t *request)
{
    HARD_ASSERT(request != NULL);

    FILE *fp = NULL;
    char *buffer = NULL;

    if (request->path == NULL) {
        LOG(ERROR, "No cache location specified for %s", request->url);
        goto fail;
    }

    fp = path_fopen(request->path, "rb");
    if (fp == NULL) {
        LOG(ERROR, "Could not open %s: %d (%s)", request->path, errno,
            strerror(errno));
        goto fail;
    }

    struct stat statbuf;
    if (fstat(fileno(fp), &statbuf) == -1) {
        LOG(ERROR, "Could not stat %s: %d (%s)", request->path, errno,
            strerror(errno));
        goto fail;
    }

    size_t size = statbuf.st_size;
    buffer = emalloc(size + 1);
    if (fread(buffer, 1, size, fp) != size) {
        LOG(ERROR, "Could not read %s: %d (%s)", request->path, errno,
            strerror(errno));
        goto fail;
    }

    request->body_size = size;
    request->body = buffer;
    request->body[size] = '\0';
    buffer = NULL;

    bool ret = true;
    goto done;

fail:
    ret = false;
done:
    if (fp != NULL) {
        fclose(fp);
    }

    if (buffer != NULL) {
        efree(buffer);
    }

    return ret;
}

/**
 * Write to the file cache.
 *
 * @param request
 * cURL request.
 * @warning
 * This function expects the cURL request mutex to be locked.
 */
static void
curl_write_cache (curl_request_t *request)
{
    HARD_ASSERT(request != NULL);

    /* If we have a cache path, and we managed to retrieve some data, update
     * the cached file. */
    if (request->path == NULL || request->header == NULL ||
        request->body == NULL) {
        return;
    }

    char *etag = NULL;

    char header[HUGE_BUF];
    size_t pos = 0;
    while (string_get_word(request->header,
                           &pos,
                           '\n',
                           header,
                           sizeof(header), 0)) {
        char *cps[2];
        if (string_split(header, cps, arraysize(cps), ':') != arraysize(cps)) {
            continue;
        }

        if (strcmp(cps[0], "ETag") == 0) {
            string_whitespace_trim(cps[1]);
            etag = cps[1];
            break;
        }
    }

    FILE *fp = path_fopen(request->path, "wb");
    if (fp != NULL) {
        if (fwrite(request->body,
                   1,
                   request->body_size,
                   fp) != request->body_size) {
            LOG(ERROR, "Failed to save %s: %d (%s)",
                request->path, errno, strerror(errno));
            etag = NULL;
        }

        fclose(fp);
    } else {
        LOG(ERROR, "Failed to open %s for saving: %d (%s)",
            request->path, errno, strerror(errno));
        etag = NULL;
    }

    if (etag != NULL) {
        char path[HUGE_BUF];
        snprintf(VS(path), "%s.etag", request->path);
        fp = path_fopen(path, "w");
        if (fp != NULL) {
            if (fputs(etag, fp) == EOF) {
                LOG(ERROR, "Failed to save %s: %d (%s)",
                    path, errno, strerror(errno));
            }

            fclose(fp);
        } else {
            LOG(ERROR, "Failed to open %s for saving: %d (%s)",
                path, errno, strerror(errno));
        }
    }
}

/**
 * Create a new ::curl_request_t structure.
 *
 * @param url
 * URL to connect to.
 * @param trust
 * Trust store to use.
 * @return
 * The new structure.
 */
curl_request_t *
curl_request_create (const char *url, curl_pkey_trust_t trust)
{
    HARD_ASSERT(url != NULL);

    curl_request_t *request = ecalloc(1, sizeof(*request));
    request->url = estrdup(url);
    request->http_code = -1;
    request->state = CURL_STATE_INPROGRESS;
    request->trust = trust;

    /* Create a mutex to protect the structure. */
    pthread_mutex_init(&request->mutex, NULL);

    return request;
}

/**
 * Adds form data (for POST requests).
 *
 * @param request
 * cURL request.
 * @param key
 * Form key.
 * @param value
 * Form value.
 */
void
curl_request_form_add (curl_request_t *request,
                       const char     *key,
                       const char     *value)
{
    HARD_ASSERT(request != NULL);
    HARD_ASSERT(key != NULL);
    HARD_ASSERT(value != NULL);

    curl_formadd(&request->form_post,
                 &request->form_post_last,
                 CURLFORM_COPYNAME,
                 key,
                 CURLFORM_COPYCONTENTS,
                 value,
                 CURLFORM_END);
}

/**
 * Specify path of the file that will be used for ETag and local cache.
 *
 * @param request
 * cURL request to modify.
 * @param path
 * Path.
 */
void
curl_request_set_path (curl_request_t *request, const char *path)
{
    HARD_ASSERT(path != NULL);

    if (request->path != NULL) {
        efree(request->path);
    }

    request->path = estrdup(path);
}

/**
 * Acquire state of the cURL download.
 *
 * @param request
 * cURL request to get the state of.
 * @return
 * State of the request.
 */
curl_state_t
curl_request_get_state (curl_request_t *request)
{
    HARD_ASSERT(request != NULL);

    pthread_mutex_lock(&request->mutex);
    curl_state_t state = request->state;
    pthread_mutex_unlock(&request->mutex);

    return state;
}

/**
 * Acquire the response body of the specified cURL request.
 *
 * @param request
 * cURL request.
 * @param[out] body_size
 * Size of the body. Can be NULL.
 * @return
 * Response body. Always NUL-terminated. NULL in case the download isn't
 * finished yet (or has failed).
 */
char *
curl_request_get_body (curl_request_t *request, size_t *body_size)
{
    HARD_ASSERT(request != NULL);

    if (curl_request_get_state(request) == CURL_STATE_INPROGRESS) {
        return NULL;
    }

    if (body_size) {
        *body_size = request->body_size;
    }

    return request->body;
}

/**
 * Acquire the response header of the specified cURL request.
 *
 * @param request
 * cURL request.
 * @param[out] header_size
 * Size of the header. Can be NULL.
 * @return
 * Response header. Always NUL-terminated. NULL in case the download isn't
 * finished yet (or has failed).
 */
char *
curl_request_get_header (curl_request_t *request, size_t *header_size)
{
    HARD_ASSERT(request != NULL);

    if (curl_request_get_state(request) == CURL_STATE_INPROGRESS) {
        return NULL;
    }

    if (header_size) {
        *header_size = request->header_size;
    }

    return request->header;
}

/**
 * Acquire HTTP status code of the specified cURL request.
 *
 * @param request
 * cURL request.
 * @return
 * HTTP code, -1 if the request isn't complete yet.
 */
int
curl_request_get_http_code (curl_request_t *request)
{
    HARD_ASSERT(request != NULL);

    if (curl_request_get_state(request) == CURL_STATE_INPROGRESS) {
        return -1;
    }

    return request->http_code;
}

/**
 * Acquire size information about the specified cURL request instance.
 *
 * @param request
 * cURL request.
 * @param info
 * Type of information to acquire.
 * @return
 * The returned size information.
 */
int64_t
curl_request_sizeinfo (curl_request_t *request, curl_info_t info)
{
    HARD_ASSERT(request != NULL);

    if (curl_request_get_state(request) != CURL_STATE_INPROGRESS ||
        request->handle == NULL) {
        return 0;
    }

    CURLINFO info_code;
    switch (info) {
    case CURL_INFO_DL_LENGTH:
        info_code = CURLINFO_CONTENT_LENGTH_DOWNLOAD;
        break;

    case CURL_INFO_DL_SPEED:
        info_code = CURLINFO_SPEED_DOWNLOAD;
        break;

    case CURL_INFO_DL_SIZE:
        info_code = CURLINFO_SIZE_DOWNLOAD;
        break;

    default:
        LOG(ERROR, "Invalid info ID: %d", info);
        return 0;
    }

    double val;
    CURLcode res = curl_easy_getinfo(request->handle, info_code, &val);

    if (res == CURLE_OK) {
        /* cURL uses doubles, but all the info values we use this for are
         * in bytes, so there's no reason for a double. */
        return (int64_t) val;
    }

    return 0;
}

/**
 * Construct speed information string.
 *
 * @param request
 * cURL request.
 * @param buf
 * Where to store the information.
 * @param bufsize
 * Size of 'buf'.
 * @return
 * 'buf'.
 */
char *
curl_request_speedinfo (curl_request_t *request, char *buf, size_t bufsize)
{
    HARD_ASSERT(request != NULL);
    HARD_ASSERT(buf != NULL);

    int64_t speed = curl_request_sizeinfo(request, CURL_INFO_DL_SPEED);
    int64_t received = curl_request_sizeinfo(request, CURL_INFO_DL_SIZE);
    int64_t size = curl_request_sizeinfo(request, CURL_INFO_DL_LENGTH);

    if (speed == 0 && received == 0 && size == 0) {
        *buf = '\0';
    } else if (size == -1) {
        snprintf(buf, bufsize, "%0.3f MB/s",
                 speed / 1000.0 / 1000.0);
    } else {
        received = MAX(1, received);
        size = MAX(1, size);
        snprintf(buf, bufsize, "%.0f%% @ %0.3f MB/s",
                 received * 100.0 / size,
                 speed / 1000.0 / 1000.0);
    }

    return buf;
}

/**
 * Frees previously created ::curl_request_t structure.
 *
 * Note that this might wait up to a second in order to exit the thread,
 * if any.
 *
 * @param request
 * What to free.
 */
void
curl_request_free (curl_request_t *request)
{
    HARD_ASSERT(request != NULL);

    /* Still downloading? Kill the thread. */
    if (curl_request_get_state(request) == CURL_STATE_INPROGRESS) {
        pthread_mutex_lock(&request->mutex);
        request->finished = true;
        pthread_mutex_unlock(&request->mutex);
    }

    pthread_join(request->thread_id, NULL);
    pthread_detach(request->thread_id);

    if (request->body != NULL) {
        efree(request->body);
    }

    if (request->header != NULL) {
        efree(request->header);
    }

    if (request->path != NULL) {
        efree(request->path);
    }

    if (request->cert_cn != NULL) {
        efree(request->cert_cn);
    }

    pthread_mutex_destroy(&request->mutex);
    curl_formfree(request->form_post);

    efree(request->url);
    efree(request);
}

/**
 * Performs additional verification of the server's X509 certificate.
 *
 * @param preverify_ok
 * Pre-verification result.
 * @param ctx
 * SSL X509 certificate store context.
 * @return
 * 1 if the verification succeeded, 0 otherwise.
 */
static int
curl_ssl_verify (int preverify_ok, X509_STORE_CTX *ctx)
{
    if (!preverify_ok) {
        return 0;
    }

    SSL *ssl = X509_STORE_CTX_get_ex_data(ctx,
                                          SSL_get_ex_data_X509_STORE_CTX_idx());
    SOFT_ASSERT_RC(ssl != NULL, 0, "Failed to get SSL pointer");
    SSL_CTX *ssl_ctx = SSL_get_SSL_CTX(ssl);
    SOFT_ASSERT_RC(ssl_ctx != NULL, 0, "Failed to get SSL_CTX pointer");
    curl_request_t *request = SSL_CTX_get_app_data(ssl_ctx);
    SOFT_ASSERT_RC(request != NULL, 0, "Failed to get curl_request_t pointer");

    pthread_mutex_lock(&request->mutex);

    if (curl_trust_pkeys[request->trust] == NULL) {
        SOFT_ASSERT_RC(request->trust == CURL_PKEY_TRUST_ULTIMATE, 0,
                       "Ultimate trust check requested but store is NULL");
        request->untrusted = true;
        pthread_mutex_unlock(&request->mutex);
        return 1;
    }

    pthread_mutex_unlock(&request->mutex);

    X509 *cert = X509_STORE_CTX_get_current_cert(ctx);
    SOFT_ASSERT_RC(cert != NULL, 0, "Failed to get X509 pointer");
    EVP_PKEY *pubkey = X509_get_pubkey(cert);;
    SOFT_ASSERT_RC(pubkey != NULL, 0, "Failed to get EVP_PKEY pointer");

    /* Acquire the certificate's common name and store it. */
    X509_NAME *subject_name = X509_get_subject_name(cert);
    SOFT_ASSERT_RC(subject_name != NULL, 0, "Failed to get X509_NAME pointer");
    char cn[256];
    X509_NAME_get_text_by_NID(subject_name, NID_commonName, VS(cn));

    pthread_mutex_lock(&request->mutex);

    if (request->cert_cn != NULL) {
        efree(request->cert_cn);
    }

    request->cert_cn = estrdup(cn);

    if (request->cert_id == sizeof(request->cert_chain) * CHAR_BIT) {
        LOG(ERROR, "Certificate chain too long for URL: %s", request->url);
        preverify_ok = 0;
        goto out;
    }

    curl_trust_store_t *store;
    LL_FOREACH(curl_trust_pkeys[request->trust], store) {
        int res = EVP_PKEY_cmp(pubkey, store->key);
        if (res == 1) {
            BIT_SET(request->cert_chain, request->cert_id);
        }
    }

    request->cert_id++;

out:
    EVP_PKEY_free(pubkey);
    pthread_mutex_unlock(&request->mutex);

    return preverify_ok;
}

/**
 * Verifies the public key certificate chain that was built in
 * curl_ssl_verify().
 *
 * @param request
 * cURL request.
 * @return
 * True on verification success, false on failure.
 * @warning
 * This function expects the cURL request mutex to be locked.
 */
static bool
curl_verify_cert_chain (curl_request_t *request)
{
    HARD_ASSERT(request != NULL);

    /* No certificate verified yet. */
    if (request->cert_id == 0) {
        return true;
    }

    SOFT_ASSERT_RC(request->cert_cn != NULL, false, "cert_cn is NULL");

    if (request->cert_chain == 0) {
        LOG(SYSTEM, "!!! UNTRUSTED CERTIFICATE !!!");
        LOG(SYSTEM, "Aborting connection to URL: %s, CN: %s",
            request->url, request->cert_cn);
        return false;
    }

    unsigned char sha1_output[20];
    sha1((unsigned char *) request->cert_cn,
         strlen(request->cert_cn),
         sha1_output);

    char sha1_output_ascii[sizeof(sha1_output) * 2 + 1];
    for (size_t i = 0; i < sizeof(sha1_output); i++) {
        sprintf(sha1_output_ascii + i * 2, "%02x", sha1_output[i]);
    }

    char path[HUGE_BUF];
    snprintf(VS(path), "%s/certchains/%s", curl_data_dir, sha1_output_ascii);

    FILE *fp = path_fopen(path, "rb");
    if (fp != NULL) {
        uint32_t cert_chain;
        if (fread(&cert_chain, sizeof(cert_chain), 1, fp) != 1) {
            LOG(ERROR, "Certificate chain file is corrupt: %s", path);
        } else if (cert_chain != request->cert_chain) {
            LOG(SYSTEM, "!!! CERTIFICATE CHAIN CHANGED !!!");
            LOG(SYSTEM, "Old chain: %x, new chain: %x",
                cert_chain, request->cert_chain);
            LOG(SYSTEM, "Aborting connection to URL: %s, CN: %s",
                request->url, request->cert_cn);
            fclose(fp);
            return false;
        }

        fclose(fp);
    }

    fp = path_fopen(path, "wb");
    if (fp != NULL) {
        if (fwrite(&request->cert_chain,
                   sizeof(request->cert_chain),
                   1,
                   fp) != 1) {
            LOG(ERROR, "Failed to write data to file: %s", path);
        }

        fclose(fp);
    } else {
        LOG(ERROR, "Failed to open file for saving: %s", path);
    }


    return true;
}

/**
 * Custom SSL context function.
 *
 * @param curl
 * cURL handler.
 * @param ssl_ctx
 * SSL context.
 * @param user_data
 * User-supplied pointer.
 * @return
 * CURLE_OK on success, anything else on failure.
 */
static CURLcode
curl_ssl_ctx (CURL *curl, void *ssl_ctx, void *user_data)
{
    curl_request_t *request = user_data;
    pthread_mutex_lock(&request->mutex);

    if (!curl_verify_cert_chain(request)) {
        pthread_mutex_unlock(&request->mutex);
        return CURLE_SSL_CONNECT_ERROR;
    }

    request->cert_id = 0;
    request->cert_chain = 0;
    pthread_mutex_unlock(&request->mutex);

    SSL_CTX *ctx = (SSL_CTX *) ssl_ctx;
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, curl_ssl_verify);
    SSL_CTX_set_app_data(ctx, user_data);
    return CURLE_OK;
}

/**
 * Function to call when receiving HTTP body data from cURL.
 *
 * @param buffer
 * Pointer to data to process.
 * @param size
 * The size of each piece of data.
 * @param nitems
 * Number of data elements.
 * @param userdata
 * User supplied data pointer - points to ::curl_request_t.
 * @return
 * Number of bytes processed.
 */
static size_t
curl_callback (char *buffer, size_t size, size_t nitems, void *userdata)
{
    curl_request_t *request = userdata;
    pthread_mutex_lock(&request->mutex);

    if (!curl_verify_cert_chain(request)) {
        pthread_mutex_unlock(&request->mutex);
        return 0;
    }

    size_t realsize = size * nitems;

    if (process_cb != NULL) {
        process_cb(CURL_REQUEST_PROCESS_RX, realsize);
    }

    request->body = erealloc(request->body,
                             request->body_size + realsize + 1);
    if (request->body != NULL) {
        memcpy(request->body + request->body_size, buffer, realsize);
        request->body_size += realsize;
        request->body[request->body_size] = '\0';
    }

    pthread_mutex_unlock(&request->mutex);

    return realsize;
}

/**
 * Function to call when receiving HTTP header data from cURL.
 *
 * @param buffer
 * Pointer to data to process.
 * @param size
 * The size of each piece of data.
 * @param nitems
 * Number of data elements.
 * @param userdata
 * User supplied data pointer - points to ::curl_request_t.
 * @return
 * Number of bytes processed.
 */
static size_t
curl_header_callback (char *buffer, size_t size, size_t nitems, void *userdata)
{
    curl_request_t *request = userdata;
    pthread_mutex_lock(&request->mutex);

    if (!curl_verify_cert_chain(request)) {
        pthread_mutex_unlock(&request->mutex);
        return 0;
    }

    size_t realsize = size * nitems;

    if (process_cb != NULL) {
        process_cb(CURL_REQUEST_PROCESS_RX, realsize);
    }

    request->header = erealloc(request->header,
                               request->header_size + realsize + 1);
    if (request->header != NULL) {
        memcpy(request->header + request->header_size, buffer, realsize);
        request->header_size += realsize;
        request->header[request->header_size] = '\0';
    }

    pthread_mutex_unlock(&request->mutex);

    return realsize;
}

/**
 * Progress callback function for cURL.
 *
 * @param userdata
 * User supplied pointer.
 * @param dltotal
 * Total bytes to download.
 * @param dlnow
 * Bytes downloaded.
 * @param ultotal
 * Total bytes to upload.
 * @param ulnow
 * Bytes uploaded.
 * @return
 * 1 to continue downloading, 0 otherwise.
 */
static int
curl_progress (void  *userdata,
               double dltotal,
               double dlnow,
               double ultotal,
               double ulnow)
{
    curl_request_t *request = userdata;

    pthread_mutex_lock(&request->mutex);

    if (!curl_verify_cert_chain(request)) {
        pthread_mutex_unlock(&request->mutex);
        return 0;
    }

    bool finished = request->finished;
    pthread_mutex_unlock(&request->mutex);

    if (finished) {
        return 0;
    }

    return 1;
}

/**
 * Set up common cURL handle parameters.
 *
 * @param request
 * cURL request.
 */
static curl_state_t
curl_request_setup (curl_request_t *request)
{
    HARD_ASSERT(request != NULL);
    HARD_ASSERT(request->handle != NULL);

    /* Set connection timeout. */
    CURL_SETOPT(request->handle, CURLOPT_CONNECTTIMEOUT, CURL_TIMEOUT);

    /* Disable signals since we are in a thread. See
     * http://curl.haxx.se/libcurl/c/curl_easy_setopt.html#CURLOPTNOSIGNAL
     * for details. */
    CURL_SETOPT(request->handle, CURLOPT_NOSIGNAL, 1);
    CURL_SETOPT(request->handle, CURLOPT_FOLLOWLOCATION, 1);

    /* Register a progress function so that we can quit the thread if
     * we need to. */
    CURL_SETOPT(request->handle, CURLOPT_PROGRESSFUNCTION, curl_progress);
    CURL_SETOPT(request->handle, CURLOPT_PROGRESSDATA, request);

    pthread_mutex_lock(&request->mutex);
    CURL_SETOPT(request->handle, CURLOPT_URL, request->url);
    CURL_SETOPT(request->handle, CURLOPT_REFERER, request->url);
    CURL_SETOPT(request->handle, CURLOPT_SHARE, handle_share);
    pthread_mutex_unlock(&request->mutex);

    /* The callback function. */
    CURL_SETOPT(request->handle, CURLOPT_WRITEFUNCTION, curl_callback);
    CURL_SETOPT(request->handle, CURLOPT_WRITEDATA, (void *) request);

    /* The header callback function. */
    CURL_SETOPT(request->handle, CURLOPT_HEADERFUNCTION, curl_header_callback);
    CURL_SETOPT(request->handle, CURLOPT_HEADERDATA, (void *) request);

    /* Set user agent. */
    CURL_SETOPT(request->handle, CURLOPT_USERAGENT, curl_user_agent);

    CURL_SETOPT(request->handle, CURLOPT_SSL_CTX_FUNCTION, curl_ssl_ctx);
    CURL_SETOPT(request->handle, CURLOPT_SSL_CTX_DATA, (void *) request);

#ifdef WIN32
    curl_easy_setopt(request->handle, CURLOPT_CAINFO, "ca-bundle.crt");
#endif

    return CURL_STATE_OK;
}

/**
 * Complete a previously setup cURL request.
 *
 * @param request
 * cURL request.
 * @return
 * State code.
 */
static curl_state_t
curl_request_complete (curl_request_t *request)
{
    HARD_ASSERT(request != NULL);
    HARD_ASSERT(request->handle != NULL);

    /* Get the data. */
    CURLcode res = curl_easy_perform(request->handle);

    long request_size;
    if (process_cb != NULL &&
        curl_easy_getinfo(request->handle,
                          CURLINFO_REQUEST_SIZE,
                          &request_size) == CURLE_OK) {
        process_cb(CURL_REQUEST_PROCESS_TX, request_size);
    }

    if (res != CURLE_OK) {
        LOG(ERROR, "curl_easy_perform() got error %d (%s).",
            res, curl_easy_strerror(res));
        return CURL_STATE_ERROR;
    }

    long http_code;
    curl_easy_getinfo(request->handle, CURLINFO_HTTP_CODE, &http_code);
    pthread_mutex_lock(&request->mutex);

    if (request->untrusted) {
        LOG(SYSTEM, "!!! CONNECTED TO UNTRUSTED HOST !!! ");
        LOG(SYSTEM, "Request URL: %s", request->url);
    }

    request->http_code = http_code;
    pthread_mutex_unlock(&request->mutex);

    if (http_code != 200 && http_code != 304) {
        return CURL_STATE_ERROR;
    }

    if (http_code == 200) {
        pthread_mutex_lock(&request->mutex);
        curl_write_cache(request);
        pthread_mutex_unlock(&request->mutex);
    } else if (http_code == 304) {
        pthread_mutex_lock(&request->mutex);
        bool cache_res = curl_load_cache(request);
        pthread_mutex_unlock(&request->mutex);

        if (!cache_res) {
            return CURL_STATE_ERROR;
        }
    }

    return CURL_STATE_OK;
}

/**
 * Use cURL to send a GET request to the URL specified in ::curl_request_t
 * structure (user_data).
 *
 * @param user_data
 * ::curl_request_t structure that will receive the data.
 */
void *
curl_request_do_get (void *user_data)
{
    curl_request_t *request = user_data;
    HARD_ASSERT(request != NULL);

    struct curl_slist *chunk = NULL;
    curl_state_t state;

    /* Init "easy" cURL */
    request->handle = curl_easy_init();
    if (request->handle == NULL) {
        state = CURL_STATE_ERROR;
        goto done;
    }

    char *etag = curl_load_etag(request);
    if (etag != NULL) {
        char header[MAX_BUF];
        snprintf(VS(header), "If-None-Match: %s", etag);
        efree(etag);
        chunk = curl_slist_append(chunk, header);
        curl_easy_setopt(request->handle, CURLOPT_HTTPHEADER, chunk);
    }

    state = curl_request_setup(request);
    if (state != CURL_STATE_OK) {
        goto done;
    }

    state = curl_request_complete(request);

done:
    pthread_mutex_lock(&request->mutex);
    request->state = state;
    pthread_mutex_unlock(&request->mutex);

    if (request->handle != NULL) {
        curl_easy_cleanup(request->handle);
    }

    if (chunk != NULL) {
        curl_slist_free_all(chunk);
    }

    return NULL;
}

/**
 * Begin an HTTP GET request inside of a thread.
 *
 * @param request
 * Previously created cURL request.
 */
void
curl_request_start_get (curl_request_t *request)
{
    HARD_ASSERT(request != NULL);

    int rc = pthread_create(&request->thread_id,
                            NULL,
                            curl_request_do_get,
                            request);
    if (rc != 0) {
        LOG(ERROR, "Failed to create thread: %s (%d)", strerror(rc), rc);
        request->state = CURL_STATE_ERROR;
    }
}

/**
 * Use cURL to send a POST request to the URL specified in ::curl_request_t
 * structure (user_data).
 *
 * @param user_data
 * ::curl_request_t structure that will receive the data.
 */
void *
curl_request_do_post (void *user_data)
{
    curl_request_t *request = user_data;
    HARD_ASSERT(request != NULL);

    curl_state_t state;

    /* Init "easy" cURL */
    request->handle = curl_easy_init();
    if (request->handle == NULL) {
        state = CURL_STATE_ERROR;
        goto done;
    }

    state = curl_request_setup(request);
    if (state != CURL_STATE_OK) {
        goto done;
    }

    curl_easy_setopt(request->handle, CURLOPT_HTTPPOST, request->form_post);

    state = curl_request_complete(request);

done:
    pthread_mutex_lock(&request->mutex);
    request->state = state;
    pthread_mutex_unlock(&request->mutex);

    if (request->handle != NULL) {
        curl_easy_cleanup(request->handle);
    }

    return NULL;
}

/**
 * Begin an HTTP POST request inside of a thread.
 *
 * @param request
 * Previously created cURL request.
 */
void
curl_request_start_post (curl_request_t *request)
{
    HARD_ASSERT(request != NULL);

    int rc = pthread_create(&request->thread_id,
                            NULL,
                            curl_request_do_post,
                            request);
    if (rc != 0) {
        LOG(ERROR, "Failed to create thread: %s (%d)", strerror(rc), rc);
        request->state = CURL_STATE_ERROR;
    }
}
