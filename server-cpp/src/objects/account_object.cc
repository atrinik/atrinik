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

#include <account_object.h>

using namespace atrinik;
using namespace boost;
using namespace std;

namespace atrinik {

static void validate_name(const std::string& s)
{
    if (s.empty()) {
        throw AccountError("name cannot be empty");
    }

    // TODO: add allowed characters check

    if (s.length() < AccountObject::name_min() ||
            s.length() > AccountObject::name_max()) {
        throw AccountError("name has invalid length");
    }
}

static void validate_password(const std::string& s)
{
    if (s.empty()) {
        throw AccountError("password cannot be empty");
    }

    // TODO: add allowed characters check

    if (s.length() < AccountObject::password_min() ||
            s.length() > AccountObject::password_max()) {
        throw AccountError("password has invalid length");
    }
}

bool AccountObject::load(const std::string& key, const std::string& val)
{

}

std::string AccountObject::dump()
{
    stringstream ss;

    ss << "pswd ";

    for (auto c : password) {
        ss << hex << setfill('0') << setw(2) << (int) c;
    }

    ss << endl;
    ss << "salt ";

    for (auto c : salt) {
        ss << hex << setfill('0') << setw(2) << (int) c;
    }

    ss << endl;

    return ss.str();
}

void AccountObject::action_register(const std::string& name,
        const std::string& pswd, const std::string& pswd2)
{
    validate_name(name);
    validate_password(pswd);
    validate_password(pswd2);

    if (pswd != pswd2) {
        throw AccountError("passwords do not match");
    }

    encrypt_password(pswd);

    for (int i = 0; i < 32; i++) {
        printf("%02x", 255 & password[i]);
    }

    printf("\n");
}

void AccountObject::action_login(const std::string& name,
        const std::string& pswd)
{

}

void AccountObject::action_logout(const GameObject& obj)
{

}

void AccountObject::action_char_login(const std::string& name)
{

}

void AccountObject::action_char_new(const std::string& name,
        const std::string& archname)
{

}

void AccountObject::action_change_pswd(const std::string& pswd,
        const std::string& pswd_new, const std::string& pswd_new2)
{

}

GameMessage* AccountObject::construct_packet()
{

}

void AccountObject::encrypt_password(const std::string& s)
{
    if (!RAND_bytes(&salt[0], salt.size())) {
        throw AccountError("OpenSSL PRNG is not seeded");
    }

    PKCS5_PBKDF2_HMAC(s.c_str(), s.length(), &salt[0], salt.size(),
            password_hash_iterations(), EVP_sha256(), password.size(),
            &password[0]);
}

};
