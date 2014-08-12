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
 * Account game command implementation.
 */

#include <game_command.h>
#include <game_message.h>
#include <game_session.h>
#include <draw_info_message.h>
#include <account.h>

using namespace atrinik;
using namespace boost;
using namespace std;

namespace atrinik {

void GameCommand::cmd_account(const GameMessage& msg)
{
    ServerCommands::AccountCommand type =
            static_cast<ServerCommands::AccountCommand> (msg.int8());

    if (!session_.account) {
        GameMessage write_msg;

        write_msg.int8(ClientCommands::Characters);

        string name = msg.string();
        string password = msg.string();

        switch (type) {
        case ServerCommands::AccountCommand::Login:
            try {
                AccountPtr account = AccountManager::manager.
                        account_login(name, password);
                session_.account = account;
            } catch (const AccountError& e) {
                DrawInfoMessage info_msg(
                        ClientCommands::DrawInfoCommandColors::red,
                        format("Error: %s") % e.what());
                session_.write(info_msg);
                // TODO: syslog
            }

            break;

        case ServerCommands::AccountCommand::Register:
            string password2 = msg.string();

            try {
                AccountPtr account = AccountManager::manager.
                        account_register(name, password, password2);
                session_.account = account;
                session_.account->update_last_login(session_.ip());
            } catch (const AccountError& e) {
                DrawInfoMessage info_msg(
                        ClientCommands::DrawInfoCommandColors::red,
                        format("Error: %s") % e.what());
                session_.write(info_msg);
                // TODO: syslog
            }

            break;
        }

        if (session_.account) {
            write_msg.string(name);
            write_msg.string(session_.ip());
            session_.account->construct_message(&write_msg);
            session_.account->update_last_login(session_.ip());
        }

        session_.write(write_msg);
    } else {
        switch (type) {
        case ServerCommands::AccountCommand::Pswd:
        {
            string password = msg.string();
            string password_new = msg.string();
            string password_new2 = msg.string();

            try {
                session_.account->change_password(password, password_new,
                        password_new2);
            } catch (const AccountError& e) {
                DrawInfoMessage info_msg(
                        ClientCommands::DrawInfoCommandColors::red,
                        format("Error: %s") % e.what());
                session_.write(info_msg);
                // TODO: syslog
            }

            break;
        }
        }
    }
}

};


