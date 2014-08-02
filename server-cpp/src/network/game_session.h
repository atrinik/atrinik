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
#include <algorithm>
#include <boost/asio.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/thread.hpp>
#include <boost/thread/lockable_adapter.hpp>
#include <boost/thread/strict_lock.hpp>

#include <game_message.h>
#include <draw_info_message.h>
#include <account.h>
#include <server.h>
#include <game_command.h>

namespace atrinik {

class GameSessions;
class GameCommand;

class GameSession : public std::enable_shared_from_this<GameSession> {
public:

    enum class State {
        Login,
        Playing,
        Dead,
        Zombie,
    };

    GameSession(boost::asio::io_service& io_service, GameSessions& sessions)
    : socket_(io_service), sessions_(sessions), command_(*this)
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
    void process();

    inline const uint32_t version() const
    {
        return version_;
    }

    inline void version(uint32_t version)
    {
        version_ = version;
    }

    inline const bool sound() const
    {
        return sound_;
    }

    inline void sound(bool sound)
    {
        sound_ = sound;
    }

    inline const bool bot() const
    {
        return bot_;
    }

    inline void bot(bool bot)
    {
        bot_ = bot;
    }

    inline const uint8_t mapx() const
    {
        return mapx_;
    }

    inline void mapx(uint8_t mapx)
    {
        mapx_ = std::min(std::max(mapx, (uint8_t) 9), (uint8_t) 17);
    }

    inline const uint8_t mapy() const
    {
        return mapy_;
    }

    inline void mapy(uint8_t mapy)
    {
        mapy_ = std::min(std::max(mapy, (uint8_t) 9), (uint8_t) 17);
    }

    std::string ip()
    {
        return socket_.remote_endpoint().address().to_string();
    }

    GameMessageQueue read_queue;
    GameMessageQueue write_queue;
    AccountPtr account;
private:

    bool process_message(const GameMessage& msg);

    boost::asio::ip::tcp::socket socket_;
    GameSessions& sessions_;
    GameMessage read_msg_;
    GameCommand command_;

    uint32_t version_ = 0;
    bool sound_ = false;
    bool bot_ = false;
    uint8_t mapx_;
    uint8_t mapy_;
};

typedef std::shared_ptr<GameSession> GameSessionPtr;

class GameSessions
: public boost::basic_lockable_adapter<boost::mutex> {
public:

    void add(GameSessionPtr session);
    void remove(GameSessionPtr session);
    void process();

private:
    std::set<GameSessionPtr> sessions_;
};

}
