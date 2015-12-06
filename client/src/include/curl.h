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
 * Header file for the cURL module. */

#ifndef CURL_H
#define CURL_H

/**
 * Possible cURL data states.
 */
typedef enum curl_state {
    CURL_STATE_NONE, ///< No state.
    CURL_STATE_DOWNLOAD, ///< Downloading the data.
    CURL_STATE_OK, ///< cURL thread finished and the data is ready to be used.
    CURL_STATE_ERROR, ///< An error occurred trying to get the data
} curl_state_t;

/** cURL data. */
typedef struct curl_data {
    /** The data. Can be NULL in case we got no data from the url. */
    char *memory;

    /** Size of the data. */
    size_t size;

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
     * threads. */
    SDL_mutex *mutex;

    /** The thread. */
    SDL_Thread *thread;

    /**
     * State of the data.
     */
    curl_state_t state;

    /**
     * Will contain HTTP code. */
    int http_code;

    /**
     * cURL handle being used. */
    CURL *handle;
} curl_data;

#define CURL_TIMEOUT 15

#endif
