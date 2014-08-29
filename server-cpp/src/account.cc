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
 * Account system implementation.
 */

#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <boost/locale.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <unistd.h>
#include <fstream>

#include <account.h>
#include <account_parser.h>
#include <base_object_type.h>
#include <player_object_type.h>
#include <anim_object_type.h>
#include <server.h>
#include <logger.h>

using namespace atrinik;
using namespace boost;
using namespace std;

namespace atrinik {

bool Account::load(const std::string& key, const std::string& val)
{
    BOOST_LOG_FUNCTION();

    if (key == "pswd") {
        // Parse old-style passwords (UNIX crypt and Windows SHA1)
        if (val.length() == 13 || val.length() == 40) {
            password_old = val;
            return true;
        }

        // Must be exactly twice that of the password hash size
        if (val.length() != hashSize * 2) {
            throw LOG_EXCEPTION(AccountError("invalid password"));
        }

        algorithm::unhex(val.begin(), val.end(), password.data());
        return true;
    } else if (key == "salt") {
        // Must be exactly twice that of the salt size
        if (val.length() != hashSize * 2) {
            throw LOG_EXCEPTION(AccountError("invalid password"));
        }

        algorithm::unhex(val.begin(), val.end(), salt.data());
        return true;
    } else if (key == "host") {
        host = val;
        return true;
    } else if (key == "time") {
        try {
            timestamp = lexical_cast<time_t>(val);
        } catch (const bad_cast& e) {
            throw LOG_EXCEPTION(AccountError("invalid timestamp"));
        }

        return true;
    } else if (key == "char") {
        std::vector<string> strs;

        // Must have correct amount of colon-separated strings
        if (split(strs, val, is_any_of(":")).size() < 4) {
            throw LOG_EXCEPTION(AccountError("invalid character data"));
        }

        AccountCharacter character;

        character.archetype = GameObjectManager::get(strs[0]);
        character.name = strs[1];
        character.region_name = strs[2];

        try {
            character.level = numeric_cast<uint8_t>(lexical_cast<int>(strs[3]));
        } catch (const bad_cast& e) {
            throw LOG_EXCEPTION(AccountError("invalid character level"));
        }

        characters.push_back(character);

        return true;
    }

    return false;
}

std::string Account::dump()
{
    stringstream ss;

    // Write out old-style password if it exists, new style password hash and
    // salt otherwise.
    if (!password_old.empty()) {
        ss << "pswd " << password_old << endl;
    } else {
        ss << hex << setfill('0');

        ss << "pswd ";

        for (auto c : password) {
            ss << setw(2) << (int) c;
        }

        ss << endl;
        ss << "salt ";

        for (auto c : salt) {
            ss << setw(2) << (int) c;
        }

        ss << dec;
        ss << endl;
    }

    ss << "host " << host << endl;
    ss << "time " << timestamp << endl;

    // Write out the characters.
    for (auto character : characters) {
        ss << "char " << boost::apply_visitor(GameObjectArchVisitor(),
                character.archetype->arch) <<
                ":" << character.name <<
                ":" << character.region_name <<
                ":" << (int) character.level << endl;
    }

    return ss.str();
}

void Account::update_last_login(const std::string& host)
{
    this->host = host;

    time_t t;
    time(&t);
    timestamp = mktime(gmtime(&t));
}

void Account::set_password(const std::string& s)
{
    if (!RAND_bytes(salt.data(), salt.size())) {
        throw LOG_EXCEPTION(AccountError("OpenSSL PRNG is not seeded"));
    }

    PKCS5_PBKDF2_HMAC(s.c_str(), s.length(), salt.data(), salt.size(),
            password_hash_iterations(), EVP_sha256(), password.size(),
            password.data());
    password_old.clear();
}

bool Account::check_password(const std::string& s)
{
    std::array<uint8_t, hashSize> output;

    if (!password_old.empty()) {
        return crypt(s.c_str(), password_old.c_str()) == password_old;
    }

    PKCS5_PBKDF2_HMAC(s.c_str(), s.length(), salt.data(), salt.size(),
            password_hash_iterations(), EVP_sha256(), output.size(),
            output.data());

    return output == password;
}

void Account::character_login(const std::string& name)
{

}

void Account::character_create(const std::string& name,
        const std::string& archname)
{
    if (name.empty()) {
        throw LOG_EXCEPTION(AccountError("name cannot be empty"));
    }

    // TODO: invalid characters in name check
    // TODO: player exists check

    if (characters.size() >= Account::characters_max()) {
        throw LOG_EXCEPTION(AccountError(
                "reached the maximum number of allowed characters"));
    }

    auto archetype = GameObjectManager::get(archname);

    if (!archetype->isinstance<PlayerObjectType>()) {
        throw LOG_EXCEPTION(AccountError("invalid archname"));
    }

    AccountCharacter character;

    character.name = locale::to_title(name);
    character.archetype = archetype;
    character.region_name = "";
    character.level = 1;

    characters.push_back(character);

    // TODO: make player file
}

void Account::character_logout(const GameObject& obj)
{

}

void Account::change_password(const std::string& pswd,
        const std::string& pswd_new, const std::string& pswd_new2)
{
    validate_password(pswd);
    validate_password(pswd_new);
    validate_password(pswd_new2);

    if (pswd_new != pswd_new2) {
        throw LOG_EXCEPTION(AccountError("passwords do not match"));
    }

    if (!check_password(pswd)) {
        throw LOG_EXCEPTION(AccountError("invalid password"));
    }

    set_password(pswd_new);
}

bool Account::has_old_password()
{
    return !password_old.empty();
}

void Account::construct_message(GameMessage* msg)
{
    msg->string(host);
    msg->int64(static_cast<uint64_t>(timestamp));

    for (auto character : characters) {
        msg->string(apply_visitor(GameObjectArchVisitor(),
                character.archetype->arch));
        msg->string(character.name);
        msg->string(character.region_name);

        auto anim_object = character.archetype->getinstance<AnimObjectType>();
        msg->int16(anim_object ? anim_object->animation() : 0);
        msg->int8(character.level);
    }
}

void Account::validate_name(const std::string& s)
{
    if (s.empty()) {
        throw LOG_EXCEPTION(AccountError("name cannot be empty"));
    }

    // TODO: add allowed characters check

    if (s.length() < name_min() ||
            s.length() > name_max()) {
        throw LOG_EXCEPTION(AccountError("name has invalid length"));
    }
}

void Account::validate_password(const std::string& s)
{
    if (s.empty()) {
        throw LOG_EXCEPTION(AccountError("password cannot be empty"));
    }

    // TODO: add allowed characters check

    if (s.length() < password_min() ||
            s.length() > password_max()) {
        throw LOG_EXCEPTION(AccountError("password has invalid length"));
    }
}

template<> bool Manager<AccountManager>::use_secondary = true;

void AccountManager::gc()
{
    BOOST_LOG_FUNCTION();

    while (true) {
        for (auto &it : accounts) {
            // Skip accounts that are still being used
            if (!it.second.unique()) {
                continue;
            }

            BOOST_LOG_NAMED_SCOPE("for accounts");
            LOG(Development) << "GCing account: " << it.first;

            try {
                account_save(it.first, it.second);
            } catch (const AccountError& e) {
                LOG(Error) << "Could not save account (" << it.first << "): " <<
                        e.what() << LOG_STACK(e);
                continue;
            }

            accounts.erase(it.first);
        }

        usleep(Server::ticks_duration() * gc_frequency());
    }
}

AccountPtr AccountManager::account_register(const std::string& name,
        const std::string& pswd, const std::string& pswd2)
{
    Account::validate_name(name);
    Account::validate_password(pswd);
    Account::validate_password(pswd2);

    if (pswd != pswd2) {
        throw LOG_EXCEPTION(AccountError("passwords do not match"));
    }

    filesystem::path p = manager().account_make_path(name);

    if (filesystem::exists(p)) {
        throw LOG_EXCEPTION(AccountError("account is already registered"));
    }

    AccountPtr account(new Account());
    account->set_password(pswd);

    manager().accounts.insert(make_pair(name, account));

    return account;
}

AccountPtr AccountManager::account_login(const std::string& name,
        const std::string& pswd)
{
    Account::validate_name(name);
    Account::validate_password(pswd);

    AccountPtr account = manager().account_load(name);

    if (!account->check_password(pswd)) {
        throw LOG_EXCEPTION(AccountError("invalid password"));
    }

    // If the account uses old-style password, update it
    if (account->has_old_password()) {
        account->set_password(pswd);
    }

    return account;
}

void AccountManager::account_save(const std::string& name, AccountPtr account)
{
    BOOST_LOG_FUNCTION();

    filesystem::path p = manager().account_make_path(name);
    filesystem::create_directories(p.parent_path());

    // TODO: write to temp file, if successful, rename

    ofstream file(p.string());

    if (!file.is_open()) {
        throw LOG_EXCEPTION(AccountError("failed to open file for writing"));
    }

    file << account->dump();
}

boost::filesystem::path AccountManager::account_make_path(
        const std::string& name)
{
    filesystem::path p(accounts_path());
    string name_lower = locale::to_lower(name);

    for (int i = 0; i < Account::name_min(); i++) {
        p /= name_lower.substr(0, i + 1);
    }

    p /= name_lower;
    p += ".dat";

    return p;
}

AccountPtr AccountManager::account_load(const std::string& name)
{
    BOOST_LOG_FUNCTION();

    AccountMap::accessor a;

    if (accounts.find(a, name)) {
        return a->second;
    }

    ifstream file(account_make_path(name).string());

    if (!file) {
        throw LOG_EXCEPTION(AccountError("no such account"));
    }

    // Load up the account and cache it
    AccountPtr account(new Account());
    AccountParser::load(file, account);
    accounts.insert(make_pair(name, account));

    return account;
}

};
