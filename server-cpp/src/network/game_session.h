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
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/thread.hpp>
#include <boost/thread/lockable_adapter.hpp>
#include <boost/thread/strict_lock.hpp>

#include <game_message.h>

namespace atrinik {

class game_sessions;

class game_session : public boost::enable_shared_from_this<game_session> {
public:

    game_session(boost::asio::io_service& io_service, game_sessions& sessions)
    : socket_(io_service), sessions_(sessions)
    {
    }

    boost::asio::ip::tcp::socket& socket()
    {
        return socket_;
    }

    virtual void write(const game_message& msg);

    void start();
    void handle_read_header(const boost::system::error_code& error,
            std::size_t bytes_transferred);
    void handle_read_body(const boost::system::error_code& error,
            std::size_t bytes_transferred);
    void handle_write(const boost::system::error_code& error,
            std::size_t bytes_transferred);

    game_message_queue read_queue;
    game_message_queue write_queue;
private:
    boost::asio::ip::tcp::socket socket_;
    game_sessions& sessions_;
    game_message read_msg_;
};

typedef boost::shared_ptr<game_session> game_session_ptr;

class game_sessions
: public boost::basic_lockable_adapter<boost::mutex>
{
public:
    void add(game_session_ptr session)
    {
        boost::strict_lock<boost::mutex> guard(this->lockable());
        sessions_.insert(session);
    }

    void remove(game_session_ptr session)
    {
        boost::strict_lock<boost::mutex> guard(this->lockable());
        sessions_.erase(session);
    }

    void process()
    {
        boost::strict_lock<boost::mutex> guard(this->lockable());
        for (auto session : sessions_) {
            while (!session->read_queue.empty()) {
                game_message msg;

                if (!session->read_queue.try_pop(msg)) {
                    break;
                }

                printf("RECEIVED %d bytes:\n", msg.length());
                for (size_t i = 0; i < msg.length(); i++)
                {
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

                    game_message write_msg;

                    write_msg.int8(15);
                    write_msg.int32(1058);
                    session->write(write_msg);
                }
            }
        }
    }

private:
    std::set<game_session_ptr> sessions_;
};

}
