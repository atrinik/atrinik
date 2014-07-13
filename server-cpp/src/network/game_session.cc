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
using namespace boost;
using namespace std;

namespace atrinik {

void game_session::start()
{
    sessions_.add(shared_from_this());
    asio::async_read(socket_,
            asio::buffer(read_msg_.header(), game_message::header_length),
            bind(&game_session::handle_read_header, shared_from_this(),
            _1, _2));
}

void game_session::write(const game_message& msg)
{
    bool write_in_progress = !write_queue.empty();
    game_message write_msg;

    write_queue.push(msg);

    if (!write_in_progress && write_queue.try_pop(write_msg)) {
        write_msg.encode_header();
        asio::async_write(socket_,
                asio::buffer(write_msg.header(), write_msg.length()),
                bind(&game_session::handle_write, shared_from_this(),
                _1, _2));
    }
}

void game_session::handle_read_header(const boost::system::error_code& error,
        std::size_t bytes_transferred)
{
    if (!error && read_msg_.decode_header()) {
        asio::async_read(socket_,
                asio::buffer(read_msg_.body(), read_msg_.body_length()),
                bind(&game_session::handle_read_body, shared_from_this(),
                _1, _2));
    } else {
        sessions_.remove(shared_from_this());
    }
}

void game_session::handle_read_body(const boost::system::error_code& error,
        std::size_t bytes_transferred)
{
    if (!error) {
        shared_from_this()->read_queue.push(read_msg_);

        asio::async_read(socket_,
                asio::buffer(read_msg_.header(), game_message::header_length),
                bind(&game_session::handle_read_header, shared_from_this(),
                _1, _2));
    } else {
        sessions_.remove(shared_from_this());
    }
}

void game_session::handle_write(const boost::system::error_code& error,
        std::size_t bytes_transferred)
{
    if (!error) {
        game_message write_msg;

        if (!write_queue.empty() && write_queue.try_pop(write_msg)) {
            write_msg.encode_header();
            asio::async_write(socket_,
                    asio::buffer(write_msg.header(), write_msg.length()),
                    bind(&game_session::handle_write, shared_from_this(),
                    _1, _2));
        }
    } else {
        sessions_.remove(shared_from_this());
    }
}

}

