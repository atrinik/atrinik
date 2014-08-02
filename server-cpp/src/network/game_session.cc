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
 * Game connection implementation.
 */

#include <boost/bind.hpp>
#include <game_session.h>

using namespace atrinik;
using namespace ClientCommands;
using namespace ServerCommands;
using namespace boost;
using namespace std;

namespace atrinik {

void GameSession::start()
{
    sessions_.add(shared_from_this());
    asio::async_read(socket_,
            asio::buffer(read_msg_.header(), GameMessage::header_length),
            bind(&GameSession::handle_read_header, shared_from_this(),
            _1, _2));
}

void GameSession::write(const GameMessage& msg)
{
    bool write_in_progress = !write_queue.empty();
    GameMessage write_msg;

    printf("SENDING %d bytes (header not encoded):\n", (int) msg.length());
    for (size_t i = 0; i < msg.length(); i++) {
        printf("0x%02x ", msg.header()[i] & 0xff);

        if (((i + 1) % 8) == 0) {
            printf("\n");
        }
    }
    printf("\n");

    write_queue.push(msg);

    if (!write_in_progress && write_queue.try_pop(write_msg)) {
        write_msg.encode_header();
        asio::async_write(socket_,
                asio::buffer(write_msg.header(), write_msg.length()),
                bind(&GameSession::handle_write, shared_from_this(),
                _1, _2));
    }
}

void GameSession::handle_read_header(const boost::system::error_code& error,
        std::size_t bytes_transferred)
{
    if (!error && read_msg_.decode_header()) {
        asio::async_read(socket_,
                asio::buffer(read_msg_.body(), read_msg_.body_length()),
                bind(&GameSession::handle_read_body, shared_from_this(),
                _1, _2));
    } else {
        sessions_.remove(shared_from_this());
    }
}

void GameSession::handle_read_body(const boost::system::error_code& error,
        std::size_t bytes_transferred)
{
    if (!error) {
        shared_from_this()->read_queue.push(read_msg_);

        asio::async_read(socket_,
                asio::buffer(read_msg_.header(), GameMessage::header_length),
                bind(&GameSession::handle_read_header, shared_from_this(),
                _1, _2));
    } else {
        sessions_.remove(shared_from_this());
    }
}

void GameSession::handle_write(const boost::system::error_code& error,
        std::size_t bytes_transferred)
{
    if (!error) {
        GameMessage write_msg;

        if (!write_queue.empty() && write_queue.try_pop(write_msg)) {
            write_msg.encode_header();
            asio::async_write(socket_,
                    asio::buffer(write_msg.header(), write_msg.length()),
                    bind(&GameSession::handle_write, shared_from_this(),
                    _1, _2));
        }
    } else {
        sessions_.remove(shared_from_this());
    }
}

bool GameSession::process_message(const GameMessage& msg)
{
    printf("RECEIVED %d bytes:\n", (int) msg.length());
    for (size_t i = 0; i < msg.length(); i++) {
        printf("0x%02x ", msg.header()[i] & 0xff);

        if (((i + 1) % 8) == 0) {
            printf("\n");
        }
    }
    printf("\n");

    uint8_t type = msg.int8();

    switch (type) {
    case ServerCommands::Setup:
        command_.cmd_setup(msg);
        break;

    case ServerCommands::Version:
        command_.cmd_version(msg);
        break;

    case ServerCommands::Account:
        command_.cmd_account(msg);
        break;
    }
}

void GameSession::process()
{
    while (!read_queue.empty()) {
        GameMessage msg;

        if (!read_queue.try_pop(msg)) {
            break;
        }

        process_message(msg);
    }
}

void GameSessions::add(GameSessionPtr session)
{
    strict_lock<mutex> guard(this->lockable());
    sessions_.insert(session);
}

void GameSessions::remove(GameSessionPtr session)
{
    strict_lock<mutex> guard(this->lockable());
    sessions_.erase(session);
}

void GameSessions::process()
{
    strict_lock<mutex> guard(this->lockable());

    for (auto session : sessions_) {
        session->process();
    }
}

}

