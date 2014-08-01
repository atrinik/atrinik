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
#include <boost/shared_ptr.hpp>
#include <boost/filesystem.hpp>
#include <tbb/concurrent_hash_map.h>

#include <game_object.h>
#include <game_message.h>
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
 * Implements the Account class, which is used to hold data about a particular
 * player account.
 */
class Account {
public:

    static const int hashSize = 32; // Password hash and salt size in bytes

    Account()
    {
        std::cout << "allocating account" << std::endl;
    }

    ~Account()
    {
        std::cout << "deallocating account" << std::endl;
    }

    /**
     * Acquire the minimum number of characters an account name must have.
     * @return Number of characters.
     */
    static inline int name_min()
    {
        return 4;
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

    /**
     * Attempt to load the specified key/value pair into the account.
     * @param key Key.
     * @param val Value.
     * @return Whether the key/value was loaded into the account.
     * @throws AccountError on invalid data in the value.
     */
    bool load(const std::string& key, const std::string& val);

    /**
     * Dump the specified account as string.
     * @return Dumped account data.
     */
    std::string dump();

    void login(const std::string& host);

    void set_password(const std::string& s);

    bool check_password(const std::string& s);

    void character_login(const std::string& name);

    void character_create(const std::string& name, const std::string& archname);

    void character_logout(const GameObject& obj);

    void change_password(const std::string& pswd,
            const std::string& pswd_new, const std::string& pswd_new2);

    bool has_old_password();

    GameMessage* construct_packet();

private:
    std::array<uint8_t, hashSize> password; ///< Hashed account password.

    std::array<uint8_t, hashSize> salt; ///< Account password salt.

    std::string password_old; ///< Old-style crypt() password.

    AccountCharacterList characters; ///< Account's characters.

    std::string host; ///< Hostname the account last logged in from.

    time_t timestamp = 0; ///< UTC timestamp of when the account last logged in.
};

typedef boost::shared_ptr<Account> AccountPtr; ///< Account pointer.
typedef tbb::concurrent_hash_map<std::string, AccountPtr>
AccountMap; ///< Map of accounts.

class AccountManager {
public:

    static inline std::string accounts_path()
    {
        return "data/accounts"; // TODO: configuration
    }

    static inline int gc_frequency()
    {
        return 10; // TODO: configuration
    }

    void gc();

    /**
     * Register a new account.
     * @param name Account name to register.
     * @param pswd Password.
     * @param pswd2 Password verification.
     * @return Account.
     * @throws AccountError if the account cannot be registered; account with
     * the same name exists, the password wasn't long enough, the two passwords
     * didn't match, etc.
     */
    AccountPtr account_register(const std::string& name,
            const std::string& pswd, const std::string& pswd2);

    /**
     * Log into an account.
     * @param name Account name.
     * @param pswd Account password.
     * @return Account.
     */
    AccountPtr account_login(const std::string& name, const std::string& pswd);

    void account_save(const std::string& name, AccountPtr account);
private:
    boost::filesystem::path account_make_path(const std::string& name);
    AccountPtr account_load(const std::string& name);
    static void validate_name(const std::string& s);
    static void validate_password(const std::string& s);

    AccountMap accounts;
};

class AccountError : public Error {
    using Error::Error;
};

};
