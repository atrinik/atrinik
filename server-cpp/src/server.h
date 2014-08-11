/*******************************************************************************
 *               Atrinik, a Multiplayer Online Role Playing Game               *
 *                                                                             *
 *       Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team       *
 *                                                                             *
 * This program is free software; you can redistribute it and/or modify it     *
 * under the terms of the GNU General Public License as published by the Free  *
 * Software Foundation; either version 2 of the License, or (at your option)   *
 * any later version.                                                          *
 *                                                                             *
 * This program is distributed in the hope that it will be useful, but WITHOUT *
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       *
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for    *
 * more details.                                                               *
 *                                                                             *
 * You should have received a copy of the GNU General Public License along     *
 * with this program; if not, write to the Free Software Foundation, Inc.,     *
 * 675 Mass Ave, Cambridge, MA 02139, USA.                                     *
 *                                                                             *
 * The author can be reached at admin@atrinik.org                              *
 ******************************************************************************/

/**
 * @file
 * Atrinik server.
 */

#pragma once

#include <atomic>

#include <account.h>
#include <animation.h>
#include <face.h>

namespace atrinik {

class Server {
public:

    static Server server;

    static inline int ticks_duration()
    {
        return 125000; // TODO: config
    }

    static inline int socket_version()
    {
        return 1058;
    }

    static inline std::string http_url()
    {
        return "http://localhost:13326"; // TODO: config
    }

    Server() : account_manager()
    {
    }

    ~Server()
    {
    }

    uint64_t get_ticks();

    AccountManager account_manager;
    
    AnimationManager animation;
    
    FaceManager face;

private:
    std::atomic<uint64_t> ticks;
};

};
