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
#include <map_tile_object.h>
#include <logger.h>

#include "map_object.h"

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
    BOOST_LOG_FUNCTION();

    stringstream strm;
    strm << "Sending " << msg.length() << " bytes:\n\t";
    strm << hex << setfill('0');
    char c;

    for (size_t i = 0; i < msg.length(); i++) {
        if (i < 2) {
            c = msg.length() >> (16 - 8 * (i + 1));
        } else {
            c = msg.header()[i] & 0xff;
        }

        strm << "0x" << setw(2) << (c & 0xff) << " ";

        if (((i + 1) % 8) == 0 && i < msg.length() - 1) {
            strm << "\n\t";
        }
    }

    LOG(Development) << strm.str();

    bool write_in_progress = !write_queue.empty();
    GameMessage write_msg;

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
    BOOST_LOG_FUNCTION();

    stringstream strm;
    strm << "Received " << msg.length() << " bytes:\n\t";
    strm << hex << setfill('0');

    for (size_t i = 0; i < msg.length(); i++) {
        strm << "0x" << setw(2) << (msg.header()[i] & 0xff) << " ";

        if (((i + 1) % 8) == 0 && i < msg.length() - 1) {
            strm << "\n\t";
        }
    }

    LOG(Development) << strm.str();

    uint8_t type = msg.int8();

    switch (type) {
    case ServerCommands::Setup:
    {
        BOOST_LOG_NAMED_SCOPE("case server command setup");
        command_.cmd_setup(msg);
        break;
    }

    case ServerCommands::Version:
    {
        BOOST_LOG_NAMED_SCOPE("case server command version");
        command_.cmd_version(msg);
        break;
    }

    case ServerCommands::Account:
    {
        BOOST_LOG_NAMED_SCOPE("case server command account");
        command_.cmd_account(msg);
        break;
    }
    }
}

void GameSession::process()
{
    BOOST_LOG_FUNCTION();

    while (!read_queue.empty()) {
        BOOST_LOG_NAMED_SCOPE("while read_queue");

        GameMessage msg;

        if (!read_queue.try_pop(msg)) {
            break;
        }

        process_message(msg);
    }
}

void GameSession::draw_map()
{
    GameMessage map_msg, layer_msg, sound_msg;
    auto map_ = obj_->map();
    auto tile_ = obj_->map_tile();

    if (!map_ || !tile_) {
        return;
    }

    auto map = *map_;
    auto tile_pl = *tile_;

    map_msg.int8(static_cast<int> (map_cache.update_cmd));

    // TODO: update_cmd handling

    map_msg.int8(tile_pl->x());
    map_msg.int8(tile_pl->y());

    for (int ay = mapy_ - 1, y = tile_pl->y() + (mapy_ + 1) / 2 - 1;
            y >= tile_pl->y() - mapy_ / 2; y--, ay--) {
        for (int ax = mapx_ - 1, x = tile_pl->x() + (mapx_ + 1) / 2 - 1;
                x >= tile_pl->x() - mapx_ / 2; x--, ax--) {
            // line of sight checks
            uint16_t mask = (ax & 0x1f) << 11 | (ay & 0x1f) << 6;

            map_msg.int16(mask);

            auto tile = map->tile_get(x, y);

            int num_layers = 0;

            for (int layer = static_cast<int> (MapTileObject::Layer::Floor);
                    layer <= MapTileObject::NumLayers(); layer++) {
                for (int sub_layer = 0; sub_layer <
                        MapTileObject::NumSubLayers(); sub_layer++) {
                    auto p = tile->get_obj_layer(MapTileObject::NumLayers() *
                            sub_layer + layer);

                    if (p.first == p.second) {
                        continue;
                    }

                    auto obj = *p.first;
                }
            }
        }
    }
}

void GameSessions::add(GameSessionPtr session)
{
    BOOST_LOG_FUNCTION();

    strict_lock<mutex> guard(this->lockable());
    sessions_.insert(session);
}

void GameSessions::remove(GameSessionPtr session)
{
    BOOST_LOG_FUNCTION();

    strict_lock<mutex> guard(this->lockable());
    sessions_.erase(session);
}

void GameSessions::process()
{
    BOOST_LOG_FUNCTION();

    strict_lock<mutex> guard(this->lockable());

    for (auto session : sessions_) {
        BOOST_LOG_NAMED_SCOPE("for sessions_");
        session->process();
    }
}

}

