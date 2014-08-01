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
 * Draw info message.
 */

#pragma once

#include <boost/format.hpp>

#include <game_message.h>

namespace atrinik {

class DrawInfoMessage : public GameMessage {
public:
    using GameMessage::GameMessage;

    DrawInfoMessage(ClientCommands::DrawInfoCommand type,
            const std::string& color, const std::string& msg,
            const std::string& name = "")
    {
        int8(ClientCommands::DrawInfo);
        int8((uint8_t) type);
        string(color);

        if (!name.empty()) {
            string("[a=#charname]" + name + "[/a]: ", false);
        }

        string(msg);
    }

    DrawInfoMessage(ClientCommands::DrawInfoCommand type,
            const std::string& color, const boost::format fmter,
            const std::string& name = "") :
    DrawInfoMessage(type, color, fmter.str(), name)
    {
    }

    DrawInfoMessage(const std::string& color, const std::string& msg,
            const std::string& name = "") :
    DrawInfoMessage(ClientCommands::DrawInfoCommand::Game, color, msg, name)
    {
    }

    DrawInfoMessage(const std::string& color, const boost::format fmter,
            const std::string& name = "") :
    DrawInfoMessage(ClientCommands::DrawInfoCommand::Game, color, fmter.str(),
    name)
    {
    }
};

};
