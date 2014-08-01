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
 * Game server.
 */

#pragma once

#include <memory>
#include <game_session.h>

namespace atrinik {

class GameServer {
public:

    GameServer(boost::asio::io_service& io_service,
            const boost::asio::ip::tcp::endpoint& endpoint)
    : io_service_(io_service), acceptor_(io_service, endpoint)
    {
        start_accept();
    }

    void start_accept();
    void handle_accept(GameSessionPtr session,
            const boost::system::error_code& error);

    void process();

private:
    boost::asio::io_service& io_service_;
    boost::asio::ip::tcp::acceptor acceptor_;
    GameSessions sessions_;
};

typedef std::shared_ptr<GameServer> game_server_ptr;

}
