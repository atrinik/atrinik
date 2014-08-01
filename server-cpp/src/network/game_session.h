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
 * Game TCP connection.
 */

#pragma once

#include <set>
#include <memory>
#include <boost/asio.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/thread.hpp>
#include <boost/thread/lockable_adapter.hpp>
#include <boost/thread/strict_lock.hpp>

#include <game_message.h>
#include <draw_info_message.h>
#include <account.h>
#include <server.h>

namespace atrinik {

class GameSessions;

class GameSession : public std::enable_shared_from_this<GameSession> {
public:

    GameSession(boost::asio::io_service& io_service, GameSessions& sessions)
    : socket_(io_service), sessions_(sessions)
    {
    }

    boost::asio::ip::tcp::socket& socket()
    {
        return socket_;
    }

    virtual void write(const GameMessage& msg);

    void start();
    void handle_read_header(const boost::system::error_code& error,
            std::size_t bytes_transferred);
    void handle_read_body(const boost::system::error_code& error,
            std::size_t bytes_transferred);
    void handle_write(const boost::system::error_code& error,
            std::size_t bytes_transferred);

    GameMessageQueue read_queue;
    GameMessageQueue write_queue;
    AccountPtr account;
private:
    boost::asio::ip::tcp::socket socket_;
    GameSessions& sessions_;
    GameMessage read_msg_;
};

typedef std::shared_ptr<GameSession> GameSessionPtr;

class GameSessions
: public boost::basic_lockable_adapter<boost::mutex> {
public:

    void add(GameSessionPtr session)
    {
        boost::strict_lock<boost::mutex> guard(this->lockable());
        sessions_.insert(session);
    }

    void remove(GameSessionPtr session)
    {
        boost::strict_lock<boost::mutex> guard(this->lockable());
        sessions_.erase(session);
    }

    void process()
    {
        boost::strict_lock<boost::mutex> guard(this->lockable());
        for (auto session : sessions_) {
            while (!session->read_queue.empty()) {
                GameMessage msg;

                if (!session->read_queue.try_pop(msg)) {
                    break;
                }

                printf("RECEIVED %d bytes:\n", (int) msg.length());
                for (size_t i = 0; i < msg.length(); i++) {
                    printf("0x%02x ", msg.header()[i] & 0xff);

                    if (((i + 1) % 8) == 0) {
                        printf("\n");
                    }
                }
                printf("\n");

                uint8_t type = msg.int8();

                if (type == 3) {
                    uint32_t version = msg.int32();
                    printf("DETERMINED version: %d\n", version);

                    GameMessage write_msg;

                    write_msg.int8(15);
                    write_msg.int32(1058);
                    session->write(write_msg);

                    DrawInfoMessage write_msg2(
                            ClientCommands::DrawInfoCommand::Game,
                            ClientCommands::DrawInfoCommandColors::white,
                            boost::format("%s") % "hello");

                    session->write(write_msg2);
                }
            }
        }
    }

private:
    std::set<GameSessionPtr> sessions_;
};

}
