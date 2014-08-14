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
 * Game command.
 */

#pragma once

#include <array>
#include <functional>

namespace atrinik {

class GameSession;
class GameMessage;

class GameCommand {
public:

    GameCommand(GameSession& session) : session_(session)
    {
    }

    ~GameCommand()
    {
    }

    void cmd_control(const GameMessage& msg);
    void cmd_ask_face(const GameMessage& msg);
    void cmd_setup(const GameMessage& msg);
    void cmd_version(const GameMessage& msg);
    void cmd_request_file(const GameMessage& msg); ///< @deprecated
    void cmd_clear(const GameMessage& msg);
    void cmd_request_update(const GameMessage& msg);
    void cmd_keepalive(const GameMessage& msg);
    void cmd_account(const GameMessage& msg);
    void cmd_item_examine(const GameMessage& msg);
    void cmd_item_apply(const GameMessage& msg);
    void cmd_item_move(const GameMessage& msg);
    void cmd_reply(const GameMessage& msg); ///< @deprecated
    void cmd_player_cmd(const GameMessage& msg);
    void cmd_item_lock(const GameMessage& msg);
    void cmd_item_mark(const GameMessage& msg);
    void cmd_fire(const GameMessage& msg);
    void cmd_quickslot(const GameMessage& msg);
    void cmd_quest_list(const GameMessage& msg);
    void cmd_move_path(const GameMessage& msg);
    void cmd_item_ready(const GameMessage& msg); ///< @deprecated
    void cmd_talk(const GameMessage& msg);
    void cmd_move(const GameMessage& msg);
    void cmd_target(const GameMessage& msg);

private:

    GameSession& session_;
};

};
