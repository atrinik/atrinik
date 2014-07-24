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

#include <account.h>

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
    
    if (s.length() < Account::name_min() || s.length() > Account::name_max()) {
        throw AccountError("name has invalid length");
    }
}

static void validate_password(const std::string& s)
{
    if (s.empty()) {
        throw AccountError("password cannot be empty");
    }
    
    // TODO: add allowed characters check
    
    if (s.length() < Account::password_min() ||
            s.length() > Account::password_max()) {
        throw AccountError("password has invalid length");
    }
}
    
void Account::action_register(const std::string& name, const std::string& pswd,
            const std::string& pswd2)
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

void Account::action_login(const std::string& name, const std::string& pswd)
{
    
}
    
void Account::action_logout(const GameObject& obj)
{
    
}
    
void Account::action_char_login(const std::string& name)
{
    
}
    
void Account::action_char_new(const std::string& name,
        const std::string& archname)
{
    
}
    
void Account::action_change_pswd(const std::string& pswd,
        const std::string& pswd_new, const std::string& pswd_new2)
{
    
}

GameMessage* Account::construct_packet()
{
    
}

void Account::encrypt_password(const std::string& s)
{
    if (!RAND_bytes(salt, 32)) {
        throw AccountError("OpenSSL PRNG is not seeded");
    }
    
    PKCS5_PBKDF2_HMAC(s.c_str(), s.length(), salt, 32,
            password_hash_iterations(), EVP_sha256(), 32, password);
}

};
