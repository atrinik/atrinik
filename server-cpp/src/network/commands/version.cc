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
 * Version game command implementation.
 */

#include <game_command.h>
#include <game_message.h>
#include <game_session.h>
#include <draw_info_message.h>
#include <version_message.h>

using namespace atrinik;
using namespace std;

namespace atrinik {

void GameCommand::cmd_version(const GameMessage& msg)
{
    // Ignore multiple version commands
    if (session_.version() != 0) {
        return;
    }

    uint32_t version = msg.int32();

    if (version == 0 || version == 991017 || version == 1055) {
        DrawInfoMessage write_msg(ClientCommands::DrawInfoCommandColors::red,
                "Your client is outdated!\nGo to http://www.atrinik.org/ and "
                "download the latest Atrinik client.");
        session_.write(write_msg);
        // TODO: dc/zombie
        return;
    }

    session_.version(version);

    VersionMessage write_msg(Server::server.socket_version());
    session_.write(write_msg);
}

};


