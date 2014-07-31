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
 * Account system.
 *
 * The account system is used to store various data about individual player
 * accounts, including their last login time, IP used to log in, etc.
 */

#pragma once

#include <string.h>

#include <object.h>
#include <game_object.h>
#include <game_session.h>
#include <error.h>

namespace atrinik {

/**
 * Structure for holding account character related data.
 */
struct AccountCharacter {
    const GameObject* archetype; ///< Pointer to player's base archetype object.

    std::string name; ///< Character name.

    std::string region_name; ///< Region where the character logged out.

    uint8_t level; ///< Character's level.
};

typedef std::list<AccountCharacter>
AccountCharacterList; ///< List of characters.

/**
 * Implements the Account object class, which is used to hold data about a
 * particular player account.
 */
class AccountObject {
public:

    static const int hashSize = 32;

    /**
     * Acquire the minimum number of characters an account name must have.
     * @return Number of characters.
     */
    static inline int name_min()
    {
        return 3;
    }

    /**
     * Acquire the maximum number of characters an account name can have.
     * @return Number of characters.
     */
    static inline int name_max()
    {
        return 16;
    }

    /**
     * Acquire the minimum number of characters an account password must have.
     * @return Number of characters.
     */
    static inline int password_min()
    {
        return 6;
    }

    /**
     * Acquire the maximum number of characters a password can have.
     * @return Number of characters.
     */
    static inline int password_max()
    {
        return 32;
    }

    /**
     * Acquire the maximum number of player characters the account can have.
     * @return Number of characters.
     */
    static inline int characters_max()
    {
        return 16;
    }

    /**
     * Acquire the number of iterations to perform when hashing a password.
     * @return Number of iterations.
     */
    static inline int password_hash_iterations()
    {
        return 4096;
    }

    bool load(const std::string& key, const std::string& val);
    std::string dump();

    void action_register(const std::string& name, const std::string& pswd,
            const std::string& pswd2);

    void action_login(const std::string& name, const std::string& pswd);

    void action_logout(const GameObject& obj);

    void action_char_login(const std::string& name);

    void action_char_new(const std::string& name, const std::string& archname);

    void action_change_pswd(const std::string& pswd,
            const std::string& pswd_new, const std::string& pswd_new2);

    GameMessage* construct_packet();

private:
    std::array<uint8_t, hashSize> password; ///< Hashed account password.

    std::array<uint8_t, hashSize> salt; ///< Account password salt.

    std::string password_old; ///< Old-style crypt() password.

    AccountCharacterList characters; ///< Account's characters.

    std::string host; ///< Hostname the account last logged in from.

    time_t timestamp = 0; ///< UTC timestamp of when the account last logged in.

    void encrypt_password(const std::string& s);
    bool check_password(const std::string& s);
};

class AccountError : public Error {
    using Error::Error;
};

};
