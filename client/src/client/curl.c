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
 * cURL module for downloading data from URLs.
 *
 * The module uses SDL threads to download data in the background. This
 * makes the API slightly more complicated, but does not freeze the
 * client GUI while the download is in progress.
 *
 * Common API usage is something like this:
 *
 * @code
 * static curl_data *dl_data = NULL;
 *
 * // This would be your function where the user triggers something, for
 * // example, clicks "Download" button or similar.
 * void action_do(void)
 * {
 *     // This is only necessary if you want to give the user a chance to retry,
 *     // using for example, a "Retry" button. It may also be necessary if you
 *     // do not properly cleanup in your GUI exiting code.
 *     if (dl_data != NULL) {
 *         curl_data_free(dl_data);
 *     }
 *
 *     // Start downloading; now dl_data will not be NULL anymore, and a new
 *     // thread will be created, which will start downloading the data from
 *     // the provided URL.
 *     dl_data = curl_download_start("http://www.atrinik.org/", NULL, false);
 * }
 *
 * // Here would be your GUI drawing code, as an example.
 * void draw_gui(void)
 * {
 *     // The trigger action to start download could be something like:
 *     // if button_pressed(xxx) then action_do()
 *
 *     // Here you check for the dl_data; if non-NULL, there is something
 *     // being downloaded (or perhaps an error occurred that you can check
 *     // for.
 *     if (dl_data != NULL) {
 *         sint8 ret;
 *
 *         // This checks the state of the download.
 *         ret = curl_download_finished(dl_data);
 *
 *         // -1 return value means that some kind of an error occurred;
 *         // 404 error, connection timed out, etc.
 *         // 0 means the download is still in progress.
 *         // 1 means the download finished.
 *         if (ret == -1) {
 *             // Here you can either cleanup using curl_data_free(dl_data);
 *             // dl_data = NULL; and exit, or show the user that something has
 *             // gone wrong, and (optionally) show them a button to retry.
 *         } else if (ret == 0) {
 *             // Here you can show the user that download is still in progress
 *             // with some text, for example.
 *         } else if (ret == 1) {
 *             // What you do here depends on what type of GUI you are creating.
 *             // For example, in the case of the metaserver, the downloaded
 *             // data is parsed and added to the servers list, and then dl_data
 *             // is freed and NULLed. However, cleaning up the dl_data pointer
 *             // can be left up to exit_gui(), and here you can just show the
 *             // raw data to the user, by accessing dl_data->memory (this can
 *             // also be used to split the data or whatever). dl_data->size
 *             // will contain the number of bytes in dl_data->memory. Note that
 *             // dl_data->memory may be NULL, in case no data was downloaded
 *             // (empty page). dl_data->memory is always NUL-terminated
 *             // (unless, of course, there is no data, as previously
 *             // mentioned).
 *         }
 *     }
 * }
 *
 * // Cleaning up should be done after exiting the GUI, to make sure
 * // downloading process is stopped (if it's running) and cleanup the
 * // structure.
 * void exit_gui(void)
 * {
 *     if (dl_data != NULL) {
 *         curl_data_free(dl_data);
 *         dl_data = NULL;
 *     }
 * }
 * @endcode
 *
 * @author Alex Tokar */

#include <global.h>

/** Shared handle. */
static CURLSH *handle_share = NULL;
/** Mutex to protect the shared handle. */
static SDL_mutex *handle_share_mutex = NULL;

/**
 * Function to call when receiving data from cURL.
 * @param ptr Pointer to data to process.
 * @param size The size of each piece of data.
 * @param nmemb Number of data elements.
 * @param data User supplied data pointer - points to ::curl_data that
 * holds the data returned from the url.
 * @return Number of bytes processed. */
static size_t curl_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
    size_t realsize = size * nmemb;
    curl_data *mem = data;

    SDL_LockMutex(mem->mutex);
    mem->memory = realloc(mem->memory, mem->size + realsize + 1);

    if (mem->memory) {
        memcpy(&(mem->memory[mem->size]), ptr, realsize);
        mem->size += realsize;
        mem->memory[mem->size] = '\0';
    }

    SDL_UnlockMutex(mem->mutex);

    return realsize;
}

/**
 * Function to call when receiving header data from cURL.
 * @param ptr Pointer to data to process.
 * @param size The size of each piece of data.
 * @param nmemb Number of data elements.
 * @param data User supplied data pointer - points to ::curl_data that
 * holds the data returned from the url.
 * @return Number of bytes processed. */
static size_t curl_header_callback(void *ptr, size_t size, size_t nmemb,
        void *data)
{
    size_t realsize = size * nmemb;
    curl_data *mem = data;

    SDL_LockMutex(mem->mutex);
    mem->header = realloc(mem->header, mem->header_size + realsize + 1);

    if (mem->header) {
        memcpy(&(mem->header[mem->header_size]), ptr, realsize);
        mem->header_size += realsize;
        mem->header[mem->header_size] = '\0';
    }

    SDL_UnlockMutex(mem->mutex);

    return realsize;
}

/**
 * Attempts to load an ETag for the requested file.
 * @param data cURL data structure.
 * @return ETag on success, NULL on failure.
 */
static const char *curl_load_etag(curl_data *data)
{
    char path[HUGE_BUF];
    static char etag[MAX_BUF];
    FILE *fp;

    HARD_ASSERT(data != NULL);

    if (data->path == NULL) {
        /* No file cache location specified */
        return NULL;
    }

    snprintf(VS(path), "%s.etag", data->path);
    fp = fopen_wrapper(path, "r");

    if (fp == NULL) {
        /* File doesn't exist. */
        return NULL;
    }

    if (!fgets(etag, sizeof(etag), fp)) {
        log(LOG(BUG), "Could not read %s: %d (%s)", path, errno,
                strerror(errno));
        fclose(fp);
        return NULL;
    }

    fclose(fp);

    return etag;
}

/**
 * Fills up the cURL data structure from cached data.
 * @param data cURL data structure.
 * @return True on success, false on failure.
 */
static bool curl_load_cache(curl_data *data)
{
    FILE *fp;
    struct stat statbuf;
    char *memory;
    size_t size;

    HARD_ASSERT(data != NULL);

    if (data->path == NULL) {
        log(LOG(BUG), "No cache location specified for %s", data->url);
        return false;
    }

    fp = fopen_wrapper(data->path, "rb");

    if (fp == NULL) {
        log(LOG(BUG), "Could not open %s: %d (%s)", data->path, errno,
                strerror(errno));
        return false;
    }

    if (fstat(fileno(fp), &statbuf) == -1) {
        log(LOG(BUG), "Could not stat %s: %d (%s)", data->path, errno,
                strerror(errno));
        fclose(fp);
        return false;
    }

    size = statbuf.st_size;
    memory = emalloc(size);

    if (!fread(memory, 1, size, fp)) {
        log(LOG(BUG), "Could not read %s: %d (%s)", data->path, errno,
                strerror(errno));
        fclose(fp);
        efree(memory);
        return false;
    }

    data->size = size;
    data->memory = memory;

    fclose(fp);

    return true;
}

/**
 * Use cURL to download the url we specified in curl_data structure
 * (c_data).
 * @param c_data ::curl_data structure that will receive the data (and
 * has url of what to download).
 * @return -1 on failure, 1 on success. */
int curl_connect(void *c_data)
{
    curl_data *data = c_data;
    char user_agent[MAX_BUF], version[MAX_BUF];
    const char *etag;
    CURLcode res;
    long http_code;
    struct curl_slist *chunk;
    sint8 status;

    HARD_ASSERT(c_data != NULL);

    package_get_version_full(version, sizeof(version));

    /* Store user agent for cURL, including if this is GNU/Linux build of client
     * or Windows one. */
#if defined(WIN32)
    snprintf(user_agent, sizeof(user_agent), "Atrinik Client (Win32)/%s (%d)",
            version, SOCKET_VERSION);
#elif defined(__GNUC__)
    snprintf(user_agent, sizeof(user_agent), "Atrinik Client (GNU/Linux)/%s "
            "(%d)", version, SOCKET_VERSION);
#else
    snprintf(user_agent, sizeof(user_agent), "Atrinik Client (Unknown)/%s (%d)",
            version, SOCKET_VERSION);
#endif

    /* Init "easy" cURL */
    data->handle = curl_easy_init();

    if (data->handle == NULL) {
        status = -1;
        goto done;
    }

    chunk = NULL;
    etag = curl_load_etag(data);

    if (etag != NULL) {
        char header[MAX_BUF];

        snprintf(VS(header), "If-None-Match: %s", etag);
        chunk = curl_slist_append(chunk, header);
        curl_easy_setopt(data->handle, CURLOPT_HTTPHEADER, chunk);
    }

    /* Set connection timeout. */
    curl_easy_setopt(data->handle, CURLOPT_CONNECTTIMEOUT, CURL_TIMEOUT);
    /* Disable signals since we are in a thread. See
     * http://curl.haxx.se/libcurl/c/curl_easy_setopt.html#CURLOPTNOSIGNAL
     * for details. */
    curl_easy_setopt(data->handle, CURLOPT_NOSIGNAL, 1);

    SDL_LockMutex(data->mutex);
    curl_easy_setopt(data->handle, CURLOPT_URL, data->url);
    curl_easy_setopt(data->handle, CURLOPT_REFERER, data->url);
    curl_easy_setopt(data->handle, CURLOPT_SHARE, handle_share);
    SDL_UnlockMutex(data->mutex);

    /* The callback function. */
    curl_easy_setopt(data->handle, CURLOPT_WRITEFUNCTION, curl_callback);
    curl_easy_setopt(data->handle, CURLOPT_WRITEDATA, (void *) data);

    /* The header callback function. */
    curl_easy_setopt(data->handle, CURLOPT_HEADERFUNCTION,
            curl_header_callback);
    curl_easy_setopt(data->handle, CURLOPT_HEADERDATA, (void *) data);

    /* Set user agent. */
    curl_easy_setopt(data->handle, CURLOPT_USERAGENT, user_agent);

#ifdef WIN32
    curl_easy_setopt(data->handle, CURLOPT_CAINFO, "ca-bundle.crt");
#endif

    /* Get the data. */
    res = curl_easy_perform(data->handle);

    if (res) {
        logger_print(LOG(BUG), "curl_easy_perform() got error %d (%s).", res,
                curl_easy_strerror(res));
        status = -1;
        goto done;
    }

    curl_easy_getinfo(data->handle, CURLINFO_HTTP_CODE, &http_code);
    data->http_code = http_code;

    if (http_code != 200 && http_code != 304) {
        status = -1;
        goto done;
    }

    if (http_code == 200) {
        /* If we have a cache path, and we managed to retrieve some data, update
         * the cached file. */
        if (data->path != NULL && data->header != NULL &&
                data->memory != NULL) {
            FILE *fp;
            char header[HUGE_BUF], *cps[2];
            size_t pos;

            pos = 0;
            etag = NULL;

            while (string_get_word(data->header, &pos, '\n', header,
                    sizeof(header), 0)) {
                if (string_split(header, cps, arraysize(cps), ':') !=
                        arraysize(cps)) {
                    continue;
                }

                if (strcmp(cps[0], "ETag") == 0) {
                    string_whitespace_trim(cps[1]);
                    etag = cps[1];
                    break;
                }
            }

            fp = fopen_wrapper(data->path, "wb");

            if (fp != NULL) {
                if (!fwrite(data->memory, 1, data->size, fp)) {
                    log(LOG(BUG), "Failed to save %s: %d (%s)", data->path,
                            errno, strerror(errno));
                }

                fclose(fp);
            } else {
                log(LOG(BUG), "Failed to open %s: %d (%s)", data->path, errno,
                        strerror(errno));
            }

            if (etag != NULL) {
                char path[HUGE_BUF];

                snprintf(VS(path), "%s.etag", data->path);
                fp = fopen_wrapper(path, "w");

                if (fp != NULL) {
                    if (!fputs(etag, fp)) {
                        log(LOG(BUG), "Failed to save %s: %d (%s)", data->path,
                                errno, strerror(errno));
                    }

                    fclose(fp);
                } else {
                    log(LOG(BUG), "Failed to open %s: %d (%s)", data->path,
                            errno, strerror(errno));
                }
            }
        }
    } else if (http_code == 304) {
        if (!curl_load_cache(data)) {
            status = -1;
            goto done;
        }
    }

    status = 1;

done:
    SDL_LockMutex(data->mutex);
    data->status = status;
    SDL_UnlockMutex(data->mutex);

    if (data->handle != NULL) {
        curl_easy_cleanup(data->handle);
    }

    if (chunk != NULL) {
        curl_slist_free_all(chunk);
    }

    return status;
}

/**
 * Initialize new ::curl_data structure.
 * @param url Url to connect to.
 * @param path Path to cached file. Can be NULL.
 * @return The new structure. */
curl_data *curl_data_new(const char *url, const char *path)
{
    curl_data *data;

    HARD_ASSERT(url != NULL);

    data = ecalloc(1, sizeof(curl_data));
    data->url = estrdup(url);
    data->http_code = -1;
    /* Create a mutex to protect the structure. */
    data->mutex = SDL_CreateMutex();

    if (path != NULL) {
        data->path = estrdup(path);
    }

    return data;
}

/**
 * Start downloading an url.
 * @param url What to download.
 * @param path Path to cached file. Can be NULL.
 * @return cURL data structure. You should store this somehow, and the
 * next tick see if it has finished by using curl_download_finished(). If
 * so, you can access its members such as curl_data::memory. Do not
 * forget to clean up with curl_data_free() at some point (even if an
 * error occurred). */
curl_data *curl_download_start(const char *url, const char *path)
{
    curl_data *data;

    HARD_ASSERT(url != NULL);

    data = curl_data_new(url, path);

    /* Create a new thread. */
    data->thread = SDL_CreateThread(curl_connect, data);

    if (!data->thread) {
        logger_print(LOG(ERROR), "Thread creation failed.");
        exit(1);
    }

    return data;
}

/**
 * Check if cURL has finished downloading the previously supplied url.
 * @param data cURL data structure that was returned by a previous
 * curl_download_start() call.
 * @return @copydoc curl_data::status */
sint8 curl_download_finished(curl_data *data)
{
    sint8 status;

    HARD_ASSERT(data != NULL);

    SDL_LockMutex(data->mutex);
    status = data->status;
    SDL_UnlockMutex(data->mutex);

    return status;
}

/**
 * @param data cURL data structure.
 * @param info Type of information to acquire. One of:
 * - CURLINFO_CONTENT_LENGTH_DOWNLOAD; size of the download in bytes, can
 *   be -1 if the size is not known.
 * - CURLINFO_SPEED_DOWNLOAD; speed in bytes that the file is being
 *   downloaded at.
 * - CURLINFO_SIZE_DOWNLOAD; how many bytes have been downloaded so far. */
double curl_download_sizeinfo(curl_data *data, CURLINFO info)
{
    CURLcode res;
    double val;

    HARD_ASSERT(data != NULL);

    if (curl_download_finished(data) != 0 || !data->handle) {
        return 0;
    }

    res = curl_easy_getinfo(data->handle, info, &val);

    if (res == CURLE_OK) {
        return val;
    }

    return 0;
}

/**
 * Construct speed information string.
 * @param data cURL data structure.
 * @param buf Where to store the information.
 * @param bufsize Size of 'buf'.
 * @return 'buf'. */
char *curl_download_speedinfo(curl_data *data, char *buf, size_t bufsize)
{
    double speed, received, size;

    HARD_ASSERT(data != NULL);
    HARD_ASSERT(buf != NULL);

    speed = curl_download_sizeinfo(data, CURLINFO_SPEED_DOWNLOAD);
    received = curl_download_sizeinfo(data, CURLINFO_SIZE_DOWNLOAD);
    size = curl_download_sizeinfo(data, CURLINFO_CONTENT_LENGTH_DOWNLOAD);

    if (!speed && !received && !size) {
        *buf = '\0';
    } else if (size == -1) {
        snprintf(buf, bufsize, "%0.3f MB/s", speed / 1024.0 / 1024.0);
    } else {
        snprintf(buf, bufsize, "%.0f%% @ %0.3f MB/s", received * 100.0 / size,
                speed / 1024.0 / 1024.0);
    }

    return buf;
}

/**
 * Frees previously created ::curl_data structure.
 * @param data What to free. */
void curl_data_free(curl_data *data)
{
    HARD_ASSERT(data != NULL);

    /* Still downloading? Kill the thread. */
    if (curl_download_finished(data) == 0) {
        SDL_KillThread(data->thread);
    }

    if (data->memory != NULL) {
        efree(data->memory);
    }

    if (data->path != NULL) {
        efree(data->path);
    }

    if (data->header != NULL) {
        efree(data->header);
    }

    SDL_DestroyMutex(data->mutex);
    efree(data->url);
    efree(data);
}

/**
 * Lock the share handle. */
static void curl_share_lock(CURL *handle, curl_lock_data data,
        curl_lock_access lock_access, void *userptr)
{
    (void) handle;
    (void) data;
    (void) lock_access;
    SDL_LockMutex(userptr);
}

/**
 * Unlock the share handle. */
static void curl_share_unlock(CURL *handle, curl_lock_data data, void *userptr)
{
    (void) handle;
    (void) data;
    SDL_UnlockMutex(userptr);
}

/**
 * Initialize cURL module. */
void curl_init(void)
{
    curl_global_init(CURL_GLOBAL_ALL);
    handle_share_mutex = SDL_CreateMutex();

    handle_share = curl_share_init();
    curl_share_setopt(handle_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
    curl_share_setopt(handle_share, CURLSHOPT_USERDATA, handle_share_mutex);
    curl_share_setopt(handle_share, CURLSHOPT_LOCKFUNC, curl_share_lock);
    curl_share_setopt(handle_share, CURLSHOPT_UNLOCKFUNC, curl_share_unlock);
}

/**
 * Deinitialize cURL module. */
void curl_deinit(void)
{
    curl_share_cleanup(handle_share);
    SDL_DestroyMutex(handle_share_mutex);
    curl_global_cleanup();
}
