/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
 *                                                                       *
 * Fork from Crossfire (Multiplayer game for X-windows).                 *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the Free Software           *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
 *                                                                       *
 * The author can be reached at admin@atrinik.org                        *
 ************************************************************************/

/**
 * @file
 * Socket crypto implementation.
 *
 * @author
 * Alex Tokar
 */

#include <global.h>
#include <toolkit_string.h>
#include <socket.h>
#include <socket_crypto.h>
#include <clioptions.h>

#include <openssl/err.h>

TOOLKIT_API(DEPENDS(clioptions), DEPENDS(logger), DEPENDS(memory));

/**
 * Structure that contains all the necessary information for the  crypto
 * extension of sockets.
 */
struct socket_crypto {
    EVP_PKEY *pkey; ///< Public key.
    int nid; ///< NID to use.
    uint8_t last_cmd; ///< Last received crypto sub-command.
    X509 *cert; ///< X509 certificate.
};

/**
 * Structure representing a single supported crypto curve.
 */
typedef struct crypto_curve {
    struct crypto_curve *next; ///< Next crypto curve.
    char *name; ///< Curve name.
    int nid; ///< OpenSSL NID.
} crypto_curve_t;

/** All the supported crypto curves. */
static crypto_curve_t *crypto_curves = NULL;
/** Whether socket cryptography is enabled. */
static bool crypto_enabled = false;
/** Currently used certificate in PEM format. */
static char *crypto_cert = NULL;
/** Currently used certificate chain in PEM format. */
static char *crypto_cert_chain = NULL;
/** Private key of the certificate. */
static EVP_PKEY *crypto_cert_key = NULL;
/** Certificate store. */
static X509_STORE *crypto_store = NULL;

/**
 * Frees the crypto curves.
 */
static void
socket_crypto_curves_free (void)
{
    crypto_curve_t *curve, *tmp;
    LL_FOREACH_SAFE(crypto_curves, curve, tmp) {
        efree(curve->name);
        efree(curve);
    }

    crypto_curves = NULL;
}

/**
 * Description of the --crypto command.
 */
static const char *clioptions_option_crypto_desc =
"Enables/disables socket cryptography.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_crypto (const char *arg,
                          char      **errmsg)
{
    if (KEYWORD_IS_TRUE(arg)) {
        crypto_enabled = true;
    } else if (KEYWORD_IS_FALSE(arg)) {
        crypto_enabled = false;
    } else {
        *errmsg = estrdup("Invalid value supplied");
        return false;
    }

    return true;
}

/**
 * Description of the --crypto_curves command.
 */
static const char *clioptions_option_crypto_curves_desc =
"Select crypto curves to support in the crypto exchange. You can acquire a "
"list of supported curves on your system by running "
"'openssl ecparam -list_curves'.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_crypto_curves (const char *arg,
                                 char      **errmsg)
{
    socket_crypto_curves_free();

    char name[MAX_BUF];
    size_t pos = 0;
    while (string_get_word(arg, &pos, ',', VS(name), 0) != NULL) {
        string_whitespace_trim(name);
        int nid = OBJ_txt2nid(name);
        if (nid == NID_undef) {
            continue;
        }

        crypto_curve_t *curve = ecalloc(1, sizeof(*curve));
        curve->name = estrdup(name);
        curve->nid = nid;
        LL_APPEND(crypto_curves, curve);
    }

    StringBuffer *sb = stringbuffer_new();
    crypto_curve_t *curve;
    LL_FOREACH(crypto_curves, curve) {
        stringbuffer_append_printf(sb, "%s%s",
                                   stringbuffer_length(sb) != 0 ? ", " : "",
                                   curve->name);
    }

    char *cp = stringbuffer_finish(sb);
    LOG(INFO, "Determined supported crypto curves: %s", cp);
    efree(cp);

    return true;
}

/**
 * Description of the --crypto_cert command.
 */
static const char *clioptions_option_crypto_cert_desc =
"Specify the certificate to use when acting as a server.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_crypto_cert (const char *arg,
                               char      **errmsg)
{
    char *cp = estrdup(arg);
    BIO *bio = BIO_new_mem_buf(cp, -1);
    if (bio == NULL) {
        *errmsg = estrdup("BIO_new_mem_buf() failed");
        efree(cp);
        return false;
    }

    X509 *cert = NULL;
    if (PEM_read_bio_X509(bio, &cert, 0, NULL) == NULL) {
        string_fmt(*errmsg,
                   "Failed to read certificate; ensure it's in PEM format: %s",
                   ERR_error_string(ERR_get_error(), NULL));
        BIO_free(bio);
        efree(cp);
        return false;
    }

    if (crypto_cert != NULL) {
        efree(crypto_cert);
    }

    crypto_cert = cp;
    BIO_free(bio);

    return true;
}

/**
 * Description of the --crypto_cert_chain command.
 */
static const char *clioptions_option_crypto_cert_chain_desc =
"Specify the certificate chain to use when acting as a server.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_crypto_cert_chain (const char *arg,
                                     char      **errmsg)
{
    char *cp = estrdup(arg);
    BIO *bio = BIO_new_mem_buf(cp, -1);
    if (bio == NULL) {
        *errmsg = estrdup("BIO_new_mem_buf() failed");
        efree(cp);
        return false;
    }

    X509 *cert = NULL;
    if (PEM_read_bio_X509(bio, &cert, 0, NULL) == NULL) {
        string_fmt(*errmsg,
                   "Failed to read certificate chain; ensure it's in PEM "
                   "format: %s",
                   ERR_error_string(ERR_get_error(), NULL));
        BIO_free(bio);
        efree(cp);
        return false;
    }

    if (crypto_cert_chain != NULL) {
        efree(crypto_cert_chain);
    }

    crypto_cert_chain = cp;
    X509_free(cert);
    BIO_free(bio);

    return true;
}

/**
 * Description of the --crypto_cert_key command.
 */
static const char *clioptions_option_crypto_cert_key_desc =
"Specify the private key that was used to generate the certificate.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_crypto_cert_key (const char *arg,
                                   char      **errmsg)
{
    char *cp = estrdup(arg);
    BIO *bio = BIO_new_mem_buf(cp, -1);
    if (bio == NULL) {
        *errmsg = estrdup("BIO_new_mem_buf() failed");
        efree(cp);
        return false;
    }

    EVP_PKEY *cert_key = NULL;
    if (PEM_read_bio_PrivateKey(bio,
                                &cert_key,
                                NULL,
                                NULL) == NULL) {
        string_fmt(*errmsg,
                   "Failed to read private key; ensure it's in PEM format: %s",
                   ERR_error_string(ERR_get_error(), NULL));
        BIO_free(bio);
        efree(cp);
        return false;
    }

    if (crypto_cert_key != NULL) {
        EVP_PKEY_free(crypto_cert_key);
    }

    crypto_cert_key = cert_key;
    BIO_free(bio);
    efree(cp);

    return true;
}

/**
 * Description of the --crypto_cert_bundle command.
 */
static const char *clioptions_option_crypto_cert_bundle_desc =
"Specify location of the ca-bundle.crt file.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_crypto_cert_bundle (const char *arg,
                                      char      **errmsg)
{
    BIO *bio = BIO_new(BIO_s_file());
    if (bio == NULL) {
        string_fmt(*errmsg, "BIO_new() failed: %s",
                   ERR_error_string(ERR_get_error(), NULL));
        return false;
    }

    if (BIO_read_filename(bio, arg) != 1) {
        string_fmt(*errmsg, "BIO_new() failed: %s",
                   ERR_error_string(ERR_get_error(), NULL));
        BIO_free(bio);
        return false;
    }

    X509_STORE *store = X509_STORE_new();
    if (store == NULL) {
        string_fmt(*errmsg, "X509_STORE_new() failed: %s",
                   ERR_error_string(ERR_get_error(), NULL));
        BIO_free(bio);
        return false;
    }

    if (X509_STORE_load_locations(store, arg, NULL) != 1) {
        string_fmt(*errmsg, "X509_STORE_load_locations() failed: %s",
                   ERR_error_string(ERR_get_error(), NULL));
        X509_STORE_free(store);
        BIO_free(bio);
        return false;
    }

    if (crypto_store != NULL) {
        X509_STORE_free(crypto_store);
    }

    crypto_store = store;
    BIO_free(bio);

    return true;
}

/**
 * Initialize the socket crypto API.
 */
TOOLKIT_INIT_FUNC(socket_crypto)
{
    clioption_t *cli;
    CLIOPTIONS_CREATE_ARGUMENT(cli, crypto, "Enable/disable socket crypto");
    CLIOPTIONS_CREATE_ARGUMENT(cli, crypto_curves, "Select crypto curves");
    CLIOPTIONS_CREATE_ARGUMENT(cli, crypto_cert, "Crypto certificate");
    CLIOPTIONS_CREATE_ARGUMENT(cli, crypto_cert_chain, "Certificate chain");
    CLIOPTIONS_CREATE_ARGUMENT(cli, crypto_cert_key, "Certificate key");
    CLIOPTIONS_CREATE_ARGUMENT(cli, crypto_cert_bundle, "Certificate bundle");
}
TOOLKIT_INIT_FUNC_FINISH

/**
 * Deinitialize the socket crypto API.
 */
TOOLKIT_DEINIT_FUNC(socket_crypto)
{
    socket_crypto_curves_free();

    if (crypto_cert != NULL) {
        efree(crypto_cert);
    }

    if (crypto_cert_chain != NULL) {
        efree(crypto_cert_chain);
    }

    if (crypto_cert_key != NULL) {
        EVP_PKEY_free(crypto_cert_key);
    }

    if (crypto_store != NULL) {
        X509_STORE_free(crypto_store);
    }
}
TOOLKIT_DEINIT_FUNC_FINISH

/**
 * Check whether the socket crypto sub-system is enabled.
 *
 * @return
 * Whether the socket crypto sub-system is enabled.
 */
bool
socket_crypto_enabled (void)
{
    TOOLKIT_PROTECT();
    return crypto_enabled;
}

/**
 * Check whether the socket crypto sub-system is aware of any supported
 * crypto curves.
 *
 * @return
 * Whether any crypto curves are available.
 */
bool
socket_crypto_has_curves (void)
{
    TOOLKIT_PROTECT();
    return crypto_curves != NULL;
}

/**
 * Figure out whether the specified curve is supported.
 *
 * @param name
 * Name of the curve to check.
 * @param[out] nid
 * Will contain NID of the curve on success. Can be NULL.
 * @return
 * True if the specified curve is supported, false otherwise.
 */
bool
socket_crypto_curve_supported (const char *name, int *nid)
{
    TOOLKIT_PROTECT();
    HARD_ASSERT(name != NULL);

    crypto_curve_t *curve;
    LL_FOREACH(crypto_curves, curve) {
        if (strcmp(curve->name, name) == 0) {
            if (nid != NULL) {
                *nid = curve->nid;
            }

            return true;
        }
    }

    return false;
}

/**
 * Acquires the currently used certificate in PEM format.
 *
 * @return
 * Certificate.
 */
const char *
socket_crypto_get_cert (void)
{
    TOOLKIT_PROTECT();
    return crypto_cert;
}

/**
 * Acquires the currently used certificate chain in PEM format.
 *
 * @return
 * Certificate chain.
 */
const char *
socket_crypto_get_cert_chain (void)
{
    TOOLKIT_PROTECT();
    return crypto_cert_chain;
}

/**
 * Determines if the specified crypto sub-command type is legal depending
 * on the state of the crypto exchange.
 *
 * This is a fairly crucial aspect of the exchange; we don't want a MITM
 * possibly changing any of parameters already set in the exchange.
 *
 * @param type
 * Type of the crypto sub-command.
 * @param crypto
 * Pointer to a socket crypto. Can be NULL if the exchange hasn't started yet.
 * @return
 * True if the sub-command is legal, false otherwise.
 * @note
 * If this function returns true, the command MUST be processed, otherwise
 * the internal state will get out-of-sync and it will not be possible to
 * continue the exchange setup.
 */
bool
socket_crypto_check_cmd (uint8_t type, socket_crypto_t *crypto)
{
    TOOLKIT_PROTECT();

    if (crypto == NULL) {
        /* The hello sub-command is legal only when the exchange hasn't
         * begun yet, anything else is invalid. */
        return type == CMD_CRYPTO_HELLO;
    }

    if (type != crypto->last_cmd + 1) {
        return false;
    }

    crypto->last_cmd = type;
    return true;
}

/**
 * Sets up crypto on the specified socket.
 *
 * @param sc
 * Socket.
 */
void
socket_crypto_create (socket_t *sc)
{
    TOOLKIT_PROTECT();
    HARD_ASSERT(sc != NULL);

    socket_crypto_t *crypto = ecalloc(1, sizeof(*crypto));
    crypto->nid = NID_undef;
    socket_set_crypto(sc, crypto);
}

/**
 * Changes NID of the specified crypto socket. Can only be done ONCE.
 *
 * @param crypto
 * Crypto socket.
 * @param nid
 * NID of the elliptic curve to use. Can be obtained from
 * socket_crypto_curve_supported().
 */
void
socket_crypto_set_nid (socket_crypto_t *crypto, int nid)
{
    TOOLKIT_PROTECT();
    HARD_ASSERT(crypto != NULL);
    SOFT_ASSERT(nid != NID_undef, "Undefined NID");
    SOFT_ASSERT(crypto->nid == NID_undef, "Attempt to change NID");
    crypto->nid = nid;
}

/**
 * Destroys the specified socket crypto.
 *
 * @param crypto
 * Socket crypto to destroy.
 */
void
socket_crypto_destroy (socket_crypto_t *crypto)
{
    TOOLKIT_PROTECT();
    HARD_ASSERT(crypto != NULL);

    if (crypto->pkey != NULL) {
        EVP_PKEY_free(crypto->pkey);
    }

    if (crypto->cert != NULL) {
        X509_free(crypto->cert);
    }

    efree(crypto);
}

/**
 * Load an X509 certificate, validating it and extracting the public key
 * from it.
 *
 * @param crypto
 * Crypto socket.
 * @param cert_str
 * Certificate in PEM format. Will be freed on success.
 * @param chain_str
 * Certificate chain in PEM format. Will be freed on success.
 * @return
 * True on success, false on failure.
 */
bool
socket_crypto_load_cert (socket_crypto_t *crypto,
                         char            *cert_str,
                         char            *chain_str)
{
    TOOLKIT_PROTECT();
    HARD_ASSERT(crypto != NULL);
    HARD_ASSERT(cert_str != NULL);
    HARD_ASSERT(chain_str != NULL);
    SOFT_ASSERT_RC(crypto->cert == NULL, false, "Cert already loaded");

    BIO *bio = BIO_new_mem_buf(cert_str, -1);
    if (bio == NULL) {
        LOG(ERROR, "BIO_new_mem_buf() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        return false;
    }

    X509 *cert = NULL;
    if (PEM_read_bio_X509(bio, &cert, 0, NULL) == NULL) {
        LOG(ERROR, "PEM_read_bio_X509() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        BIO_free(bio);
        return false;
    }

    BIO_free(bio);

    STACK_OF(X509) *chains = sk_X509_new(NULL);
    if (chains == NULL) {
        LOG(ERROR, "sk_X509_new() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        X509_free(cert);
        return false;
    }

    if (*chain_str != '\0') {
        bio = BIO_new_mem_buf(chain_str, -1);
        if (bio == NULL) {
            LOG(ERROR, "BIO_new_mem_buf() failed: %s",
                ERR_error_string(ERR_get_error(), NULL));
            X509_free(cert);
            sk_X509_free(chains);
            return false;
        }

        X509 *chain = NULL;
        if (PEM_read_bio_X509(bio, &chain, 0, NULL) == NULL) {
            LOG(ERROR, "PEM_read_bio_X509() failed: %s",
                ERR_error_string(ERR_get_error(), NULL));
            X509_free(cert);
            BIO_free(bio);
            sk_X509_free(chains);
            return false;
        }

        BIO_free(bio);

        if (sk_X509_push(chains, chain) != 1) {
            LOG(ERROR, "sk_X509_push() failed: %s",
                ERR_error_string(ERR_get_error(), NULL));
            X509_free(cert);
            X509_free(chain);
            sk_X509_free(chains);
            return false;
        }
    }

    X509_STORE_CTX *store_ctx = X509_STORE_CTX_new();
    if (store_ctx == NULL) {
        LOG(ERROR, "X509_STORE_CTX_new() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        X509_free(cert);
        sk_X509_free(chains);
        return false;
    }

    if (X509_STORE_CTX_init(store_ctx, crypto_store, cert, chains) != 1) {
        LOG(ERROR, "X509_STORE_CTX_init() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        X509_free(cert);
        X509_STORE_CTX_free(store_ctx);
        sk_X509_free(chains);
        return false;
    }

    if (X509_verify_cert(store_ctx) != 1) {
        LOG(ERROR, "X509_verify_cert() failed: %s",
            X509_verify_cert_error_string(store_ctx->error));
        X509_free(cert);
        X509_STORE_CTX_free(store_ctx);
        sk_X509_free(chains);
        return false;
    }

    X509_STORE_CTX_free(store_ctx);
    sk_X509_free(chains);

    crypto->cert = cert;

    efree(cert_str);
    efree(chain_str);
    return true;
}

/**
 * Load a public key in PEM format.
 *
 * @param crypto
 * Crypto socket.
 * @param buf
 * Public key data. Will be freed on success.
 * @param len
 * Length of the data.
 * @return
 * True on success, false on failure.
 */
bool
socket_crypto_load_pub_key (socket_crypto_t *crypto, char *buf, size_t len)
{
    TOOLKIT_PROTECT();
    HARD_ASSERT(crypto != NULL);
    HARD_ASSERT(buf != NULL);
    SOFT_ASSERT_RC(crypto->pkey == NULL, false,
                   "Crypto socket already has a public key");

    BIO *bio = BIO_new_mem_buf(buf, len);
    if (bio == NULL) {
        LOG(ERROR, "BIO_new_mem_buf() failed");
        return false;
    }

    EVP_PKEY *pkey = NULL;
    if (PEM_read_bio_PUBKEY(bio, &pkey, NULL, NULL) == NULL) {
        LOG(ERROR, "PEM_read_bio_PUBKEY() failed");
        BIO_free(bio);
        return false;
    }

    crypto->pkey = pkey;

    BIO_free(bio);
    efree(buf);
    return true;
}

/**
 * Generate a new public key.
 *
 * @param crypto
 * Crypto socket.
 * @param[out] len
 * Will contain the public key's length on success, undefined on failure.
 * @return
 * Public key in PEM format on success, NULL on failure.
 */
char *
socket_crypto_gen_pub_key (socket_crypto_t *crypto, size_t *len)
{
    TOOLKIT_PROTECT();
    HARD_ASSERT(crypto != NULL);
    HARD_ASSERT(len != NULL);
    SOFT_ASSERT_RC(crypto->pkey == NULL, NULL,
                   "Crypto socket already has a public key");
    SOFT_ASSERT_RC(crypto->nid != NID_undef, NULL,
                   "Undefined NID");

    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
    if (pctx == NULL) {
        LOG(ERROR, "EVP_PKEY_CTX_new_id() failed");
        return NULL;
    }

    if (EVP_PKEY_paramgen_init(pctx) != 1) {
        LOG(ERROR, "EVP_PKEY_paramgen_init() failed");
        EVP_PKEY_CTX_free(pctx);
        return NULL;
    }

    if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pctx, crypto->nid) != 1) {
        LOG(ERROR, "EVP_PKEY_CTX_set_ec_paramgen_curve_nid() failed");
        EVP_PKEY_CTX_free(pctx);
        return NULL;
    }

    EVP_PKEY *params = NULL;
    if (EVP_PKEY_paramgen(pctx, &params) != 1) {
        LOG(ERROR, "EVP_PKEY_paramgen() failed");
        EVP_PKEY_CTX_free(pctx);
        return NULL;
    }

    EVP_PKEY_CTX *kctx = EVP_PKEY_CTX_new(params, NULL);
    if (kctx == NULL) {
        LOG(ERROR, "EVP_PKEY_CTX_new() failed");
        EVP_PKEY_free(params);
        EVP_PKEY_CTX_free(pctx);
        return NULL;
    }

    if (EVP_PKEY_keygen_init(kctx) != 1) {
        LOG(ERROR, "EVP_PKEY_keygen_init() failed");
        EVP_PKEY_CTX_free(kctx);
        EVP_PKEY_free(params);
        EVP_PKEY_CTX_free(pctx);
        return NULL;
    }

    EVP_PKEY *pkey = NULL;
    if (EVP_PKEY_keygen(kctx, &pkey) != 1) {
        LOG(ERROR, "EVP_PKEY_keygen() failed");
        EVP_PKEY_CTX_free(kctx);
        EVP_PKEY_free(params);
        EVP_PKEY_CTX_free(pctx);
        return NULL;
    }

    BIO *bio = BIO_new(BIO_s_mem());
    if (bio == NULL) {
        LOG(ERROR, "BIO_new() failed");
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(kctx);
        EVP_PKEY_free(params);
        EVP_PKEY_CTX_free(pctx);
        return NULL;
    }

    if (PEM_write_bio_PUBKEY(bio, pkey) != 1){
        LOG(ERROR, "PEM_write_bio_PUBKEY() failed");
        BIO_free(bio);
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(kctx);
        EVP_PKEY_free(params);
        EVP_PKEY_CTX_free(pctx);
        return NULL;
    }

    crypto->pkey = pkey;

    BUF_MEM *buf;
    BIO_get_mem_ptr(bio, &buf);

    char *cp = ecalloc(1, buf->length);
    memcpy(cp, buf->data, buf->length);
    *len = buf->length;

    BIO_free(bio);
    EVP_PKEY_CTX_free(kctx);
    EVP_PKEY_free(params);
    EVP_PKEY_CTX_free(pctx);

    return cp;
}

/**
 * Derive a secret key from the specified crypto socket.
 *
 * @param crypto
 * Crypto socket.
 * @return
 * True on success, false on failure.
 */
bool
socket_crypto_derive (socket_crypto_t *crypto)
{
    TOOLKIT_PROTECT();
    return true;
}
