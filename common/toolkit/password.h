/*************************************************************************
 * Atrinik password-record API.                                         *
 ************************************************************************/

#ifndef TOOLKIT_PASSWORD_H
#define TOOLKIT_PASSWORD_H

#include "toolkit.h"

#define PASSWORD_RECORD_SIZE 256

typedef enum password_verify_result {
    PASSWORD_VERIFY_ERROR = -1,
    PASSWORD_VERIFY_MISMATCH = 0,
    PASSWORD_VERIFY_MATCH = 1,
} password_verify_result_t;

TOOLKIT_FUNCS_DECLARE(password);

bool password_record_create(const char *password, char record[PASSWORD_RECORD_SIZE]);
password_verify_result_t password_record_verify(const char *password, const char *record);
bool password_record_is_valid(const char *record);
bool password_record_needs_rehash(const char *record);

/* Verification-only support for repository account records written before
 * Argon2id. Successful logins are immediately rehashed by the account layer. */
password_verify_result_t password_legacy_pbkdf2_verify(const char *password,
                                                       const unsigned char salt[32],
                                                       const unsigned char expected[32]);

#endif
