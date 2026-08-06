/*************************************************************************
 * Atrinik password-record API.                                         *
 ************************************************************************/

#include "password.h"
#include "string.h"

#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>

#define PASSWORD_ALGORITHM "argon2id"
#define PASSWORD_VERSION 19U
#define PASSWORD_MEMORY_KIB 65536U
#define PASSWORD_ITERATIONS 3U
#define PASSWORD_LANES 1U
#define PASSWORD_THREADS 1U
#define PASSWORD_SALT_SIZE 16
#define PASSWORD_HASH_SIZE 32

/* These are part of the OpenSSL 3.5 Argon2 provider contract. Keep the stable
 * string names local so packaging-only native tools can compile with older
 * headers; account authentication still fails closed when ARGON2ID is absent. */
#define PASSWORD_PARAM_MEMORY "memcost"
#define PASSWORD_PARAM_LANES "lanes"
#define PASSWORD_PARAM_THREADS "threads"
#define PASSWORD_PARAM_VERSION "version"

#define PASSWORD_MIN_MEMORY_KIB 8192U
#define PASSWORD_MAX_MEMORY_KIB 262144U
#define PASSWORD_MIN_ITERATIONS 1U
#define PASSWORD_MAX_ITERATIONS 10U
#define PASSWORD_MIN_LANES 1U
#define PASSWORD_MAX_LANES 4U

TOOLKIT_API();

TOOLKIT_INIT_FUNC(password) {}
TOOLKIT_INIT_FUNC_FINISH

TOOLKIT_DEINIT_FUNC(password) {}
TOOLKIT_DEINIT_FUNC_FINISH

typedef struct password_parameters {
    uint32_t memory_kib;
    uint32_t iterations;
    uint32_t lanes;
    unsigned char salt[PASSWORD_SALT_SIZE];
    unsigned char hash[PASSWORD_HASH_SIZE];
} password_parameters_t;

static bool password_derive(const char *password,
                            const password_parameters_t *parameters,
                            unsigned char output[PASSWORD_HASH_SIZE]) {
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, "ARGON2ID", NULL);
    EVP_KDF_CTX *context = kdf != NULL ? EVP_KDF_CTX_new(kdf) : NULL;
    uint32_t version = PASSWORD_VERSION;
    uint32_t iterations = parameters->iterations;
    uint32_t memory_kib = parameters->memory_kib;
    uint32_t lanes = parameters->lanes;
    uint32_t threads = parameters->lanes;
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD,
                                          (void *)password,
                                          strlen(password)),
        OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                          (void *)parameters->salt,
                                          sizeof(parameters->salt)),
        OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ITER, &iterations),
        OSSL_PARAM_construct_uint32(PASSWORD_PARAM_MEMORY, &memory_kib),
        OSSL_PARAM_construct_uint32(PASSWORD_PARAM_LANES, &lanes),
        OSSL_PARAM_construct_uint32(PASSWORD_PARAM_THREADS, &threads),
        OSSL_PARAM_construct_uint32(PASSWORD_PARAM_VERSION, &version),
        OSSL_PARAM_construct_end(),
    };
    bool ok = context != NULL && EVP_KDF_derive(context, output, PASSWORD_HASH_SIZE, params) == 1;

    EVP_KDF_CTX_free(context);
    EVP_KDF_free(kdf);
    return ok;
}

static bool password_parse(const char *record, password_parameters_t *parameters) {
    char salt_hex[PASSWORD_SALT_SIZE * 2 + 1] = {0};
    char hash_hex[PASSWORD_HASH_SIZE * 2 + 1] = {0};
    char canonical[PASSWORD_RECORD_SIZE];
    unsigned int version, memory_kib, iterations, lanes;
    int consumed = 0;

    if (record == NULL || strlen(record) >= PASSWORD_RECORD_SIZE ||
        sscanf(record,
               "$argon2id$v=%u$m=%u,t=%u,p=%u$%32[0-9A-F]$%64[0-9A-F]%n",
               &version,
               &memory_kib,
               &iterations,
               &lanes,
               salt_hex,
               hash_hex,
               &consumed) != 6 ||
        record[consumed] != '\0' || version != PASSWORD_VERSION ||
        memory_kib < PASSWORD_MIN_MEMORY_KIB || memory_kib > PASSWORD_MAX_MEMORY_KIB ||
        iterations < PASSWORD_MIN_ITERATIONS || iterations > PASSWORD_MAX_ITERATIONS ||
        lanes < PASSWORD_MIN_LANES || lanes > PASSWORD_MAX_LANES ||
        strlen(salt_hex) != PASSWORD_SALT_SIZE * 2 || strlen(hash_hex) != PASSWORD_HASH_SIZE * 2) {
        return false;
    }

    parameters->memory_kib = memory_kib;
    parameters->iterations = iterations;
    parameters->lanes = lanes;
    int length = snprintf(canonical,
                          sizeof(canonical),
                          "$%s$v=%u$m=%u,t=%u,p=%u$%s$%s",
                          PASSWORD_ALGORITHM,
                          PASSWORD_VERSION,
                          parameters->memory_kib,
                          parameters->iterations,
                          parameters->lanes,
                          salt_hex,
                          hash_hex);
    return length > 0 && length < (int)sizeof(canonical) && strcmp(record, canonical) == 0 &&
           string_fromhex(salt_hex, strlen(salt_hex), parameters->salt, sizeof(parameters->salt)) ==
               sizeof(parameters->salt) &&
           string_fromhex(hash_hex, strlen(hash_hex), parameters->hash, sizeof(parameters->hash)) ==
               sizeof(parameters->hash);
}

bool password_record_create(const char *password, char record[PASSWORD_RECORD_SIZE]) {
    password_parameters_t parameters = {
        .memory_kib = PASSWORD_MEMORY_KIB,
        .iterations = PASSWORD_ITERATIONS,
        .lanes = PASSWORD_LANES,
    };
    char salt_hex[PASSWORD_SALT_SIZE * 2 + 1] = {0};
    char hash_hex[PASSWORD_HASH_SIZE * 2 + 1] = {0};

    if (record != NULL) {
        record[0] = '\0';
    }
    if (password == NULL || record == NULL ||
        RAND_bytes(parameters.salt, sizeof(parameters.salt)) != 1 ||
        !password_derive(password, &parameters, parameters.hash) ||
        string_tohex(parameters.salt, sizeof(parameters.salt), salt_hex, sizeof(salt_hex), false) !=
            sizeof(salt_hex) - 1 ||
        string_tohex(parameters.hash, sizeof(parameters.hash), hash_hex, sizeof(hash_hex), false) !=
            sizeof(hash_hex) - 1) {
        OPENSSL_cleanse(&parameters, sizeof(parameters));
        OPENSSL_cleanse(salt_hex, sizeof(salt_hex));
        OPENSSL_cleanse(hash_hex, sizeof(hash_hex));
        return false;
    }

    int length = snprintf(record,
                          PASSWORD_RECORD_SIZE,
                          "$%s$v=%u$m=%u,t=%u,p=%u$%s$%s",
                          PASSWORD_ALGORITHM,
                          PASSWORD_VERSION,
                          parameters.memory_kib,
                          parameters.iterations,
                          parameters.lanes,
                          salt_hex,
                          hash_hex);
    bool ok = length > 0 && length < PASSWORD_RECORD_SIZE;
    OPENSSL_cleanse(&parameters, sizeof(parameters));
    OPENSSL_cleanse(salt_hex, sizeof(salt_hex));
    OPENSSL_cleanse(hash_hex, sizeof(hash_hex));
    if (!ok) {
        record[0] = '\0';
    }
    return ok;
}

password_verify_result_t password_record_verify(const char *password, const char *record) {
    password_parameters_t parameters = {0};
    unsigned char output[PASSWORD_HASH_SIZE];

    if (password == NULL || !password_parse(record, &parameters)) {
        OPENSSL_cleanse(&parameters, sizeof(parameters));
        return PASSWORD_VERIFY_ERROR;
    }

    if (!password_derive(password, &parameters, output)) {
        OPENSSL_cleanse(&parameters, sizeof(parameters));
        OPENSSL_cleanse(output, sizeof(output));
        return PASSWORD_VERIFY_ERROR;
    }

    password_verify_result_t result = CRYPTO_memcmp(output, parameters.hash, sizeof(output)) == 0
                                          ? PASSWORD_VERIFY_MATCH
                                          : PASSWORD_VERIFY_MISMATCH;
    OPENSSL_cleanse(&parameters, sizeof(parameters));
    OPENSSL_cleanse(output, sizeof(output));
    return result;
}

bool password_record_is_valid(const char *record) {
    password_parameters_t parameters = {0};
    bool valid = password_parse(record, &parameters);
    OPENSSL_cleanse(&parameters, sizeof(parameters));
    return valid;
}

bool password_record_needs_rehash(const char *record) {
    password_parameters_t parameters = {0};
    if (!password_parse(record, &parameters)) {
        OPENSSL_cleanse(&parameters, sizeof(parameters));
        return false;
    }

    bool needs_rehash = parameters.memory_kib != PASSWORD_MEMORY_KIB ||
                        parameters.iterations != PASSWORD_ITERATIONS ||
                        parameters.lanes != PASSWORD_LANES;
    OPENSSL_cleanse(&parameters, sizeof(parameters));
    return needs_rehash;
}

password_verify_result_t password_legacy_pbkdf2_verify(const char *password,
                                                       const unsigned char salt[32],
                                                       const unsigned char expected[32]) {
    if (password == NULL || salt == NULL || expected == NULL) {
        return PASSWORD_VERIFY_ERROR;
    }

    unsigned char output[32];
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, "PBKDF2", NULL);
    EVP_KDF_CTX *context = kdf != NULL ? EVP_KDF_CTX_new(kdf) : NULL;
    uint64_t iterations = 4096;
    char digest[] = "SHA256";
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD,
                                          (void *)password,
                                          strlen(password)),
        OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, (void *)salt, 32),
        OSSL_PARAM_construct_uint64(OSSL_KDF_PARAM_ITER, &iterations),
        OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, digest, 0),
        OSSL_PARAM_construct_end(),
    };
    int ok = context != NULL && EVP_KDF_derive(context, output, sizeof(output), params) == 1;
    password_verify_result_t result = PASSWORD_VERIFY_ERROR;
    if (ok) {
        result = CRYPTO_memcmp(output, expected, sizeof(output)) == 0 ? PASSWORD_VERIFY_MATCH
                                                                      : PASSWORD_VERIFY_MISMATCH;
    }
    OPENSSL_cleanse(output, sizeof(output));
    EVP_KDF_CTX_free(context);
    EVP_KDF_free(kdf);
    return result;
}
