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
 * Setup game command implementation.
 */

#include <game_command.h>
#include <game_message.h>
#include <game_session.h>

using namespace atrinik;
using namespace std;

namespace atrinik {

void GameCommand::cmd_setup(const GameMessage& msg)
{
    GameMessage write_msg;

    write_msg.int8(ClientCommands::Setup);

    while (!msg.end()) {
        uint8_t type = msg.int8();
        write_msg.int8(type);

        switch (static_cast<ServerCommands::SetupCommand>(type)) {
        case ServerCommands::SetupCommand::Sound:
            session_.sound((bool) msg.int8());
            write_msg.int8((uint8_t) session_.sound());
            break;

        case ServerCommands::SetupCommand::MapSize:
            session_.mapx(msg.int8());
            write_msg.int8(session_.mapx());
            session_.mapy(msg.int8());
            write_msg.int8(session_.mapy());
            break;

        case ServerCommands::SetupCommand::Bot:
            session_.bot((bool) msg.int8());
            write_msg.int8((uint8_t) session_.bot());
            break;

        case ServerCommands::SetupCommand::DataURL:
            string url;

            url = msg.string();

            if (!url.empty()) {
                write_msg.string(url);
            } else {
                write_msg.string(Server::server.http_url());
            }
        }
    }

    session_.write(write_msg);
}

};


