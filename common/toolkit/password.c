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
        OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_MEMCOST, &memory_kib),
        OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_LANES, &lanes),
        OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_THREADS, &threads),
        OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_VERSION, &version),
        OSSL_PARAM_construct_end(),
    };
    bool ok = context != NULL && EVP_KDF_derive(context, output, PASSWORD_HASH_SIZE, params) == 1;

    EVP_KDF_CTX_free(context);
    EVP_KDF_free(kdf);
    return ok;
}

static bool password_parse(const char *record, password_parameters_t *parameters) {
    char salt_hex[PASSWORD_SALT_SIZE * 2 + 1];
    char hash_hex[PASSWORD_HASH_SIZE * 2 + 1];
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
    return string_fromhex(salt_hex, strlen(salt_hex), parameters->salt, sizeof(parameters->salt)) ==
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
    char salt_hex[PASSWORD_SALT_SIZE * 2 + 1];
    char hash_hex[PASSWORD_HASH_SIZE * 2 + 1];

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
    OPENSSL_cleanse(&parameters, sizeof(parameters));
    return length > 0 && length < PASSWORD_RECORD_SIZE;
}

password_verify_result_t password_record_verify(const char *password, const char *record) {
    password_parameters_t parameters;
    unsigned char output[PASSWORD_HASH_SIZE];

    if (password == NULL || !password_parse(record, &parameters)) {
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
    password_parameters_t parameters;
    bool valid = password_parse(record, &parameters);
    OPENSSL_cleanse(&parameters, sizeof(parameters));
    return valid;
}

password_verify_result_t password_legacy_pbkdf2_verify(const char *password,
                                                       const unsigned char salt[32],
                                                       const unsigned char expected[32]) {
    unsigned char output[32];
    int ok = password != NULL && PKCS5_PBKDF2_HMAC(password,
                                                   (int)strlen(password),
                                                   salt,
                                                   32,
                                                   4096,
                                                   EVP_sha256(),
                                                   sizeof(output),
                                                   output) == 1;
    password_verify_result_t result = PASSWORD_VERIFY_ERROR;
    if (ok) {
        result = CRYPTO_memcmp(output, expected, sizeof(output)) == 0 ? PASSWORD_VERIFY_MATCH
                                                                      : PASSWORD_VERIFY_MISMATCH;
    }
    OPENSSL_cleanse(output, sizeof(output));
    return result;
}
