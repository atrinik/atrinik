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

#include <openssl/aes.h>
#include <openssl/engine.h>
#include <openssl/err.h>

TOOLKIT_API(DEPENDS(clioptions), DEPENDS(logger), DEPENDS(memory));

/**
 * Structure that contains all the necessary information for the  crypto
 * extension of sockets.
 */
struct socket_crypto {
    EVP_PKEY *pubkey; ///< Public key.
    EVP_PKEY *privkey; ///< Private key.
    EVP_PKEY_CTX *pkey_ctx; ///< Public key encryption context.
    int nid; ///< NID to use.
    uint8_t last_cmd; ///< Last received crypto sub-command.
    socket_t *sc; ///< Socket this was created for.
    unsigned char *key; ///< The secret key.
    uint8_t key_len; ///< Length of the secret key.
    EVP_CIPHER_CTX *cipher_ctx; ///< Cipher context used for AES.
    unsigned char iv[AES_BLOCK_SIZE]; ///< AES IV buffer.
    unsigned char iv2[AES_BLOCK_SIZE]; ///< AES IV buffer.
    unsigned char secret[SHA512_DIGEST_LENGTH]; ///< Secret for checksums.
    unsigned char secret2[SHA512_DIGEST_LENGTH]; ///< Secret for checksums.
    bool done:1; ///< Whether the handshake has been completed.
};

/**
 * Structure representing a single supported crypto curve.
 */
typedef struct crypto_curve {
    struct crypto_curve *next; ///< Next crypto curve.
    char *name; ///< Curve name.
    int nid; ///< OpenSSL NID.
} crypto_curve_t;

/**
 * Possible encrypted packet command types.
 */
enum {
    CRYPTO_CMD_ENCRYPTED = 1, ///< Encrypted data.
    CRYPTO_CMD_CHECKSUM, ///< Checksummed data.

    CRYPTO_CMD_MAX ///< Total number of known crypto commands.
};

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
 * Reads an X509 certificate in PEM format.
 *
 * @param pem
 * Certificate PEM data. Must be NUL-terminated.
 * @return
 * Certificate on success, NULL on failure.
 */
static X509 *
crypto_read_pem_x509 (const char *pem)
{
    char *cp = estrdup(pem);
    BIO *bio = BIO_new_mem_buf(cp, -1);
    if (bio == NULL) {
        LOG(ERROR, "BIO_new_mem_buf() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        efree(cp);
        return false;
    }

    X509 *cert = NULL;
    if (PEM_read_bio_X509(bio, &cert, 0, NULL) == NULL) {
        cert = NULL;
        LOG(ERROR, "PEM_read_bio_X509() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
    }

    BIO_free(bio);
    efree(cp);
    return cert;
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
    X509 *cert = crypto_read_pem_x509(arg);
    if (cert == NULL) {
        *errmsg = estrdup("Failed to read certificate; ensure it's in "
                          "PEM format");
        return false;
    }

    /* We don't actually need the cert; we just needed to validate the
     * format. */
    X509_free(cert);

    if (crypto_cert != NULL) {
        efree(crypto_cert);
    }

    crypto_cert = estrdup(arg);
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

    OPENSSL_init();
    OpenSSL_add_all_ciphers();
    ERR_load_crypto_strings();
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
 * Figure out whether the specified elliptic curve is supported.
 *
 * @param name
 * Name of the elliptic curve to check.
 * @param[out] nid
 * Will contain NID of the elliptic curve on success. Can be NULL.
 * @return
 * True if the specified elliptic curve is supported, false otherwise.
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
 * Append supported elliptic curves to the specified packet.
 *
 * @param packet
 * Packet to append to.
 */
void
socket_crypto_packet_append_curves (packet_struct *packet)
{
    TOOLKIT_PROTECT();
    HARD_ASSERT(packet != NULL);

    crypto_curve_t *curve;
    LL_FOREACH(crypto_curves, curve) {
        packet_append_string_terminated(packet, curve->name);
    }
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

    /* No more crypto commands if the handshake is done. */
    if (socket_crypto_is_done(crypto)) {
        return false;
    }

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
 * @return
 * Created crypto socket.
 */
socket_crypto_t *
socket_crypto_create (socket_t *sc)
{
    TOOLKIT_PROTECT();
    HARD_ASSERT(sc != NULL);

    socket_crypto_t *crypto = ecalloc(1, sizeof(*crypto));
    crypto->nid = NID_undef;
    crypto->sc = sc;
    crypto->last_cmd = CMD_CRYPTO_HELLO;
    socket_set_crypto(sc, crypto);

    return crypto;
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
 * Frees the specified socket crypto.
 *
 * @param crypto
 * Socket crypto to free.
 */
void
socket_crypto_free (socket_crypto_t *crypto)
{
    TOOLKIT_PROTECT();
    HARD_ASSERT(crypto != NULL);

    if (crypto->pubkey != NULL) {
        EVP_PKEY_free(crypto->pubkey);
    }

    if (crypto->privkey != NULL) {
        EVP_PKEY_free(crypto->privkey);
    }

    if (crypto->pkey_ctx != NULL) {
        EVP_PKEY_CTX_free(crypto->pkey_ctx);
    }

    if (crypto->key != NULL) {
        efree(crypto->key);
    }

    efree(crypto);
}

/**
 * Certificate verify callback.
 *
 * @param ok
 * What to return on verification success.
 * @param ctx
 * X509 store context that is being verified.
 * @return
 * Verification state.
 */
static int
crypto_cert_verify_callback (int ok, X509_STORE_CTX *ctx)
{
    // TODO: only accept previously trusted self-signed certificates
    if (X509_STORE_CTX_get_error(ctx) ==
        X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT) {
        return 1;
    }

    return ok;
}

/**
 * Load an X509 certificate, validating it and extracting the public key
 * from it.
 *
 * @param crypto
 * Crypto socket.
 * @param cert_str
 * Certificate in PEM format. Must be NUL-terminated.
 * @param chain_str
 * Certificate chain in PEM format. Must be NUL-terminated.
 * @return
 * True on success, false on failure.
 */
bool
socket_crypto_load_cert (socket_crypto_t *crypto,
                         const char      *cert_str,
                         const char      *chain_str)
{
    TOOLKIT_PROTECT();
    HARD_ASSERT(crypto != NULL);
    HARD_ASSERT(cert_str != NULL);
    HARD_ASSERT(chain_str != NULL);

    X509 *cert = NULL;
    STACK_OF(X509) *chains = NULL;
    X509_STORE_CTX *store_ctx = NULL;

    cert = crypto_read_pem_x509(cert_str);
    if (cert == NULL) {
        /* Logging already done */
        goto error;
    }

    store_ctx = X509_STORE_CTX_new();
    if (store_ctx == NULL) {
        LOG(ERROR, "X509_STORE_CTX_new() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    chains = sk_X509_new(NULL);
    if (chains == NULL) {
        LOG(ERROR, "sk_X509_new() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    if (*chain_str != '\0') {
        X509 *chain = crypto_read_pem_x509(chain_str);
        if (chain == NULL) {
            /* Logging already done */
            goto error;
        }

        if (sk_X509_push(chains, chain) != 1) {
            LOG(ERROR, "sk_X509_push() failed: %s",
                ERR_error_string(ERR_get_error(), NULL));
            X509_free(chain);
            goto error;
        }
    }

    if (X509_STORE_CTX_init(store_ctx, crypto_store, cert, chains) != 1) {
        LOG(ERROR, "X509_STORE_CTX_init() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    X509_STORE_CTX_set_verify_cb(store_ctx, crypto_cert_verify_callback);

    /* Perform the verification. */
    if (X509_verify_cert(store_ctx) != 1) {
        LOG(ERROR, "X509_verify_cert() failed: %s",
            X509_verify_cert_error_string(store_ctx->error));
        goto error;
    }

    /* Acquire the certificate's common name. */
    X509_NAME *subject_name = X509_get_subject_name(cert);
    SOFT_ASSERT_LABEL(subject_name != NULL, error,
                      "Failed to get X509_NAME pointer");
    char cn[256];
    X509_NAME_get_text_by_NID(subject_name, NID_commonName, VS(cn));

    const char *host = socket_get_host(crypto->sc);
    SOFT_ASSERT_LABEL(host != NULL, error,
                      "Failed to get host from socket");

    if (strcmp(host, cn) != 0) {
        LOG(SYSTEM, "!!! CERTIFICATE ERROR !!!");
        LOG(SYSTEM, "Certificate CN (%s) doesn't match host (%s): %s",
            cn, host, socket_get_str(crypto->sc));
        goto error;
    }

    if (crypto->pubkey != NULL) {
        EVP_PKEY *pubkey = X509_get_pubkey(cert);
        SOFT_ASSERT_LABEL(pubkey != NULL, error,
                          "Failed to get EVP_PKEY pointer");
        int res = EVP_PKEY_cmp(pubkey, crypto->pubkey);
        EVP_PKEY_free(pubkey);

        if (res != 1) {
            LOG(SYSTEM, "!!! CERTIFICATE ERROR !!!");
            LOG(SYSTEM,
                "Certificate's public key doesn't match public "
                "metaserver record: %s",
                socket_get_str(crypto->sc));
            goto error;
        }
    }

    bool ret = true;
    goto out;

error:
    ret = false;

out:
    if (cert != NULL) {
        X509_free(cert);
    }

    if (store_ctx != NULL) {
        X509_STORE_CTX_free(store_ctx);
    }

    /* Free certificates in the chain. */
    if (chains != NULL) {
        while ((cert = sk_X509_pop(chains)) != NULL) {
            X509_free(cert);
        }

        /* Free the stack of chains. */
        sk_X509_free(chains);
    }

    if (crypto->pubkey != NULL) {
        EVP_PKEY_free(crypto->pubkey);
        crypto->pubkey = NULL;
    }

    return ret;
}

/**
 * Load a public key in PEM format.
 *
 * @param crypto
 * Crypto socket.
 * @param buf
 * Public key data. Must be NUL-terminated.
 * @return
 * True on success, false on failure.
 */
bool
socket_crypto_load_pubkey (socket_crypto_t *crypto, const char *buf)
{
    TOOLKIT_PROTECT();
    HARD_ASSERT(crypto != NULL);
    HARD_ASSERT(buf != NULL);
    SOFT_ASSERT_RC(crypto->pubkey == NULL, false,
                   "Crypto socket already has a public key");
    SOFT_ASSERT_RC(crypto->pkey_ctx == NULL, false,
                   "Crypto socket already has a public key context");

    char *cp = estrdup(buf);
    BIO *bio = BIO_new_mem_buf(cp, -1);
    if (bio == NULL) {
        LOG(ERROR, "BIO_new_mem_buf() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        efree(cp);
        return false;
    }

    EVP_PKEY *pkey = NULL;
    if (PEM_read_bio_PUBKEY(bio, &pkey, NULL, NULL) == NULL) {
        LOG(ERROR, "PEM_read_bio_PUBKEY() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        BIO_free(bio);
        efree(cp);
        return false;
    }

    EVP_PKEY_CTX *pkey_ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (pkey_ctx == NULL) {
        LOG(ERROR, "EVP_PKEY_CTX_new() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        EVP_PKEY_free(pkey);
        return false;
    }

    if (EVP_PKEY_encrypt_init(pkey_ctx) != 1) {
        LOG(ERROR, "EVP_PKEY_encrypt_init() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        EVP_PKEY_CTX_free(pkey_ctx);
        return false;
    }

    if (EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_OAEP_PADDING) != 1) {
        LOG(ERROR, "EVP_PKEY_CTX_set_rsa_padding() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        EVP_PKEY_CTX_free(pkey_ctx);
        return false;
    }

    crypto->pubkey = pkey;
    crypto->pkey_ctx = pkey_ctx;

    BIO_free(bio);
    efree(cp);
    return true;
}

/**
 * Generate a new public key.
 *
 * @param crypto
 * Crypto socket.
 * @param[out] pubkey_len
 * Will contain the public key's length on success.
 * @return
 * Public key in EC format on success, NULL on failure.
 */
unsigned char *
socket_crypto_gen_pubkey (socket_crypto_t *crypto,
                          size_t          *pubkey_len)
{
    TOOLKIT_PROTECT();
    HARD_ASSERT(crypto != NULL);
    SOFT_ASSERT_RC(crypto->pubkey == NULL, NULL,
                   "Crypto socket already has a public key");
    SOFT_ASSERT_RC(crypto->privkey == NULL, NULL,
                   "Crypto socket already has a private key");
    SOFT_ASSERT_RC(crypto->nid != NID_undef, NULL,
                   "Undefined NID");

    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
    if (pctx == NULL) {
        LOG(ERROR, "EVP_PKEY_CTX_new_id() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        return NULL;
    }

    if (EVP_PKEY_paramgen_init(pctx) != 1) {
        LOG(ERROR, "EVP_PKEY_paramgen_init() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        EVP_PKEY_CTX_free(pctx);
        return NULL;
    }

    if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pctx, crypto->nid) != 1) {
        LOG(ERROR, "EVP_PKEY_CTX_set_ec_paramgen_curve_nid() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        EVP_PKEY_CTX_free(pctx);
        return NULL;
    }

    EVP_PKEY *params = NULL;
    if (EVP_PKEY_paramgen(pctx, &params) != 1) {
        LOG(ERROR, "EVP_PKEY_paramgen() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        EVP_PKEY_CTX_free(pctx);
        return NULL;
    }

    EVP_PKEY_CTX *kctx = EVP_PKEY_CTX_new(params, NULL);
    if (kctx == NULL) {
        LOG(ERROR, "EVP_PKEY_CTX_new() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        EVP_PKEY_free(params);
        EVP_PKEY_CTX_free(pctx);
        return NULL;
    }

    if (EVP_PKEY_keygen_init(kctx) != 1) {
        LOG(ERROR, "EVP_PKEY_keygen_init() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        EVP_PKEY_CTX_free(kctx);
        EVP_PKEY_free(params);
        EVP_PKEY_CTX_free(pctx);
        return NULL;
    }

    EVP_PKEY *pkey = NULL;
    if (EVP_PKEY_keygen(kctx, &pkey) != 1) {
        LOG(ERROR, "EVP_PKEY_keygen() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        EVP_PKEY_CTX_free(kctx);
        EVP_PKEY_free(params);
        EVP_PKEY_CTX_free(pctx);
        return NULL;
    }

    EC_KEY *eckey = EVP_PKEY_get1_EC_KEY(pkey);
    if (eckey == NULL) {
        LOG(ERROR, "EVP_PKEY_get1_EC_KEY() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        EVP_PKEY_CTX_free(kctx);
        EVP_PKEY_free(params);
        EVP_PKEY_CTX_free(pctx);
        EVP_PKEY_free(pkey);
        return NULL;
    }

    const EC_GROUP *ecgroup = EC_KEY_get0_group(eckey);
    if (ecgroup == NULL) {
        LOG(ERROR, "EC_KEY_get0_group() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        EVP_PKEY_CTX_free(kctx);
        EVP_PKEY_free(params);
        EVP_PKEY_CTX_free(pctx);
        EVP_PKEY_free(pkey);
        EC_KEY_free(eckey);
        return NULL;
    }

    const EC_POINT *ecpoint = EC_KEY_get0_public_key(eckey);
    if (ecpoint == NULL) {
        LOG(ERROR, "EC_KEY_get0_public_key() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        EVP_PKEY_CTX_free(kctx);
        EVP_PKEY_free(params);
        EVP_PKEY_CTX_free(pctx);
        EVP_PKEY_free(pkey);
        EC_KEY_free(eckey);
        return NULL;
    }

    *pubkey_len = EC_POINT_point2oct(ecgroup,
                                     ecpoint,
                                     POINT_CONVERSION_COMPRESSED,
                                     NULL,
                                     0,
                                     NULL);
    if (*pubkey_len == 0) {
        LOG(ERROR, "EC_POINT_point2oct() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        EVP_PKEY_CTX_free(kctx);
        EVP_PKEY_free(params);
        EVP_PKEY_CTX_free(pctx);
        EVP_PKEY_free(pkey);
        EC_KEY_free(eckey);
        return NULL;
    }

    unsigned char *pubkey = emalloc(*pubkey_len);
    if (EC_POINT_point2oct(ecgroup,
                           ecpoint,
                           POINT_CONVERSION_COMPRESSED,
                           pubkey,
                           *pubkey_len,
                           NULL) != *pubkey_len) {
        LOG(ERROR, "EC_POINT_point2oct() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        EVP_PKEY_CTX_free(kctx);
        EVP_PKEY_free(params);
        EVP_PKEY_CTX_free(pctx);
        EVP_PKEY_free(pkey);
        EC_KEY_free(eckey);
        return NULL;
    }

    crypto->privkey = pkey;

    EVP_PKEY_CTX_free(kctx);
    EVP_PKEY_free(params);
    EVP_PKEY_CTX_free(pctx);
    EC_KEY_free(eckey);

    return pubkey;
}

/**
 * Generate an IV buffer.
 *
 * @param crypto
 * Crypto socket.
 * @param[out] iv_size
 * Will contain the IV buffer size.
 * @return
 * IV buffer on success, NULL on failure.
 */
const unsigned char *
socket_crypto_gen_iv (socket_crypto_t *crypto,
                      uint8_t         *iv_size)
{
    HARD_ASSERT(crypto != NULL);
    HARD_ASSERT(iv_size != NULL);

    *iv_size = sizeof(crypto->iv2);

    if (RAND_bytes(crypto->iv2, *iv_size) != 1) {
        LOG(ERROR, "RAND_bytes() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        return NULL;
    }

    return crypto->iv2;
}

/**
 * Creates a key to use for AES encryption which will be used until ECDH
 * secret keys are derived.
 *
 * Also generates the IV buffer.
 *
 * @param crypto
 * Crypto socket.
 * @param[out] len
 * Will contain the key's length on success.
 * @return
 * Created key on success, NULL on failure.
 */
const unsigned char *
socket_crypto_create_key (socket_crypto_t *crypto, uint8_t *len)
{
    HARD_ASSERT(crypto != NULL);
    HARD_ASSERT(len != NULL);
    SOFT_ASSERT_RC(crypto->key == NULL, NULL,
                   "Crypto socket already has a key: %s",
                   socket_get_str(crypto->sc));

    unsigned char buf[128];
    if (RAND_bytes(VS(buf)) != 1) {
        LOG(ERROR, "RAND_bytes() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        return NULL;
    }

    *len = SHA256_DIGEST_LENGTH;
    unsigned char digest[SHA256_DIGEST_LENGTH];
    if (SHA256(VS(buf), digest) == NULL) {
        LOG(ERROR, "SHA256() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        return NULL;
    }

    if (!socket_crypto_set_key(crypto, VS(digest), true)) {
        return NULL;
    }

    return crypto->key;
}

/**
 * Stores a crypto key to use for AES encryption.
 *
 * @param crypto
 * Crypto socket.
 * @param key
 * The key to set.
 * @param key_len
 * Length of the key.
 * @param reset_iv
 * If true, will reset the IV buffer.
 * @return
 * True on success, false on failure.
 */
bool
socket_crypto_set_key (socket_crypto_t *crypto,
                       const uint8_t   *key,
                       uint8_t          key_len,
                       bool             reset_iv)
{
    HARD_ASSERT(crypto != NULL);
    SOFT_ASSERT_RC(crypto->key == NULL, false,
                   "Crypto socket already has a key: %s",
                   socket_get_str(crypto->sc));

    crypto->key = emalloc(key_len);
    memcpy(crypto->key, key, key_len);

    if (crypto->cipher_ctx != NULL) {
        EVP_CIPHER_CTX_free(crypto->cipher_ctx);
    }

    crypto->cipher_ctx = EVP_CIPHER_CTX_new();
    if (crypto->cipher_ctx == NULL) {
        LOG(ERROR, "EVP_CIPHER_CTX_new() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    if (reset_iv) {
        if (RAND_bytes(crypto->iv, AES_BLOCK_SIZE) != 1) {
            LOG(ERROR, "RAND_bytes() failed: %s",
                ERR_error_string(ERR_get_error(), NULL));
            goto error;
        }
    }

    return true;

error:
    if (crypto->key != NULL) {
        efree(crypto->key);
        crypto->key = NULL;
    }

    if (crypto->cipher_ctx != NULL) {
        EVP_CIPHER_CTX_free(crypto->cipher_ctx);
        crypto->cipher_ctx = NULL;
    }

    return false;
}

/**
 * Acquire the IV buffer.
 *
 * @param crypto
 * Crypto socket.
 * @param[out] len
 * Will contain the length of the IV buffer.
 * @return
 * IV buffer on success, NULL on failure.
 */
const unsigned char *
socket_crypto_get_iv (socket_crypto_t *crypto, uint8_t *len)
{
    HARD_ASSERT(crypto != NULL);
    HARD_ASSERT(len != NULL);
    SOFT_ASSERT_RC(crypto->key != NULL, NULL,
                   "Crypto socket doesn't have a key: %s",
                   socket_get_str(crypto->sc));

    *len = sizeof(crypto->iv);
    return crypto->iv;
}

/**
 * Stores an IV buffer to use for AES encryption.
 *
 * @param crypto
 * Crypto socket.
 * @param iv
 * The IV buffer to set.
 * @param iv_len
 * Length of the IV buffer.
 * @return
 * True on success, false on failure.
 */
bool
socket_crypto_set_iv (socket_crypto_t *crypto,
                      const uint8_t   *iv,
                      uint8_t          iv_len)
{
    HARD_ASSERT(crypto != NULL);
    HARD_ASSERT(iv != NULL);
    SOFT_ASSERT_RC(crypto->key != NULL, false,
                   "Crypto socket doesn't have a key: %s",
                   socket_get_str(crypto->sc));

    if (sizeof(crypto->iv) != (size_t) iv_len) {
        LOG(ERROR, "Mismatched IV buffer sizes");
        return false;
    }

    memcpy(crypto->iv, iv, sizeof(crypto->iv));
    return true;
}

/**
 * Creates a new secret to use for checksums.
 *
 * @param crypto
 * Crypto socket.
 * @param[out] secret_len
 * Will contain the length of the created secret on success.
 * @return
 * Created secret, NULL on failure.
 */
const unsigned char *
socket_crypto_create_secret (socket_crypto_t *crypto,
                             uint8_t         *secret_len)
{
    HARD_ASSERT(crypto != NULL);
    HARD_ASSERT(secret_len != NULL);

    unsigned char buf[128];
    if (RAND_bytes(VS(buf)) != 1) {
        LOG(ERROR, "RAND_bytes() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        return NULL;
    }

    *secret_len = SHA512_DIGEST_LENGTH;
    if (SHA512(VS(buf), crypto->secret) == NULL) {
        LOG(ERROR, "SHA512() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        return NULL;
    }

    return crypto->secret;
}

/**
 * Sets the secret to use for checksums to the specified value.
 *
 * @param crypto
 * Crypto socket.
 * @param secret
 * Secret to use.
 * @param secret_len
 * Length of the secret.
 * @return
 * True on success, false on failure.
 */
bool
socket_crypto_set_secret (socket_crypto_t *crypto,
                          uint8_t         *secret,
                          uint8_t          secret_len)
{
    HARD_ASSERT(crypto != NULL);
    HARD_ASSERT(secret != NULL);

    if (sizeof(crypto->secret2) != (size_t) secret_len) {
        LOG(ERROR, "Secret size mismatch");
        return false;
    }

    memcpy(crypto->secret2, secret, sizeof(crypto->secret2));

    return true;
}

/**
 * Marks the crypto handshake as completed.
 *
 * @param crypto
 * Crypto socket.
 * @return
 * True on success, false on failure.
 */
bool
socket_crypto_set_done (socket_crypto_t *crypto)
{
    HARD_ASSERT(crypto != NULL);
    SOFT_ASSERT_RC(!crypto->done, false, "Crypto is already done!");
    crypto->done = true;

    /* Extend the received secret with our own, and then re-hash
     * our own secret. */
    CASSERT(sizeof(crypto->secret) == sizeof(crypto->secret2));
    for (size_t i = 0; i < sizeof(crypto->secret2); i++) {
        crypto->secret2[i] += crypto->secret[i];
    }

    /* Re-hash the secret. */
    if (SHA512(VS(crypto->secret2), crypto->secret) == NULL) {
        LOG(ERROR, "SHA512() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        return false;
    }

    /* Clear the received secret. Just to be safe. */
    memset(crypto->secret2, 0, sizeof(crypto->secret2));
    return true;
}

/**
 * Checks if the crypto handshake has been completed.
 *
 * @param crypto
 * Crypto socket. Can be NULL.
 * @return
 * Whether the handshake is completed.
 */
bool
socket_crypto_is_done (socket_crypto_t *crypto)
{
    if (crypto == NULL) {
        return false;
    }

    return crypto->done;
}

/**
 * Derive a secret key from the specified crypto socket.
 *
 * @param crypto
 * Crypto socket.
 * @param pubkey
 * Public key to derive from.
 * @param pubkey_len
 * Length of the public key.
 * @param iv
 * IV buffer.
 * @param iv_size
 * IV buffer size.
 * @return
 * True on success, false on failure.
 */
bool
socket_crypto_derive (socket_crypto_t     *crypto,
                      const unsigned char *pubkey,
                      size_t               pubkey_len,
                      const unsigned char *iv,
                      size_t               iv_size)
{
    TOOLKIT_PROTECT();
    HARD_ASSERT(crypto != NULL);
    SOFT_ASSERT_RC(crypto->key != NULL, false,
                   "Crypto socket doesn't have an AES key: %s",
                   socket_get_str(crypto->sc));
    SOFT_ASSERT_RC(crypto->privkey != NULL, false,
                   "Crypto socket doesn't have private key: %s",
                   socket_get_str(crypto->sc));

    EC_KEY *ecprivkey = NULL;
    const EC_GROUP *ecgroup = NULL;
    EC_POINT *ecpoint = NULL;
    EC_KEY *eckey = NULL;
    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *ctx = NULL;

    if (pubkey_len == 0 || iv_size != sizeof(crypto->iv2)) {
        LOG(DEVEL, "Mismatched lengths");
        goto error;
    }

    ecprivkey = EVP_PKEY_get1_EC_KEY(crypto->privkey);
    if (ecprivkey == NULL) {
        LOG(ERROR, "EVP_PKEY_get1_EC_KEY() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    ecgroup = EC_KEY_get0_group(ecprivkey);
    if (ecgroup == NULL) {
        LOG(ERROR, "EC_KEY_get0_group() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    ecpoint = EC_POINT_new(ecgroup);
    if (ecpoint == NULL) {
        LOG(ERROR, "EC_POINT_new() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    if (EC_POINT_oct2point(ecgroup, ecpoint, pubkey, pubkey_len, NULL) != 1) {
        LOG(ERROR, "EC_POINT_oct2point() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    eckey = EC_KEY_new();
    if (eckey == NULL) {
        LOG(ERROR, "EC_KEY_new() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    EC_KEY_set_group(eckey, ecgroup);

    if (EC_KEY_set_public_key(eckey, ecpoint) != 1) {
        LOG(ERROR, "EC_KEY_set_public_key() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    pkey = EVP_PKEY_new();
    if (pkey == NULL) {
        LOG(ERROR, "EVP_PKEY_new() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    if (EVP_PKEY_set1_EC_KEY(pkey, eckey) != 1) {
        LOG(ERROR, "EVP_PKEY_set1_EC_KEY() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    ctx = EVP_PKEY_CTX_new(crypto->privkey, NULL);
    if (ctx == NULL) {
        LOG(ERROR, "EVP_PKEY_CTX_new() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    if (EVP_PKEY_derive_init(ctx) != 1) {
        LOG(ERROR, "EVP_PKEY_derive_init() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    if (EVP_PKEY_derive_set_peer(ctx, pkey) != 1) {
        LOG(ERROR, "EVP_PKEY_derive_set_peer() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    size_t key_len;
    if (EVP_PKEY_derive(ctx, NULL, &key_len) != 1){
        LOG(ERROR, "EVP_PKEY_derive() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    unsigned char *key = emalloc(key_len);
    if (EVP_PKEY_derive(ctx, key, &key_len) != 1){
        LOG(ERROR, "EVP_PKEY_derive() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        efree(key);
        goto error;
    }

    unsigned char digest[SHA256_DIGEST_LENGTH];
    if (PKCS5_PBKDF2_HMAC((const char *) key,
                          key_len,
                          crypto->key,
                          crypto->key_len,
                          1000,
                          EVP_sha256(),
                          sizeof(digest),
                          digest) != 1) {
        LOG(ERROR, "PKCS5_PBKDF2_HMAC() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        efree(key);
        goto error;
    }

    /* Extend our own IV buffer with the incoming one. */
    for (size_t i = 0; i < iv_size; i++) {
        crypto->iv2[i] += iv[i];
    }

    CASSERT(sizeof(crypto->iv) == sizeof(crypto->iv2));
    memcpy(crypto->iv, crypto->iv2, sizeof(crypto->iv));
    memset(crypto->iv2, 0, sizeof(crypto->iv2));

    efree(key);
    efree(crypto->key);
    crypto->key = NULL;

    if (!socket_crypto_set_key(crypto, VS(digest), false)) {
        goto error;
    }

    bool ret = true;
    goto out;
error:
    ret = false;

out:
    if (ecprivkey != NULL) {
        EC_KEY_free(ecprivkey);
    }

    if (ecpoint != NULL) {
        EC_POINT_free(ecpoint);
    }

    if (eckey != NULL) {
        EC_KEY_free(eckey);
    }

    if (pkey != NULL) {
        EVP_PKEY_free(pkey);
    }

    if (ctx != NULL) {
        EVP_PKEY_CTX_free(ctx);
    }

    return ret;

}

/**
 * Encrypts the specified packet.
 *
 * @param sc
 * Socket.
 * @param packet_orig
 * Packet to encrypt. Will be freed (even in error cases); use the returned
 * packet.
 * @param packet_meta
 * Meta-data packet (length and command type).
 * @param checksum_only
 * If true, only generate checksums and do not encrypt the packet.
 * @return
 * Encrypted packet, NULL on failure.
 */
packet_struct *
socket_crypto_encrypt (socket_t      *sc,
                       packet_struct *packet_orig,
                       packet_struct *packet_meta,
                       bool           checksum_only)
{
    TOOLKIT_PROTECT();
    HARD_ASSERT(sc != NULL);
    HARD_ASSERT(packet_orig != NULL);
    HARD_ASSERT(packet_meta != NULL);

    socket_crypto_t *crypto = socket_get_crypto(sc);
    /* Force checksumming until we're past the hello exchange. */
    if (crypto == NULL || crypto->last_cmd == CMD_CRYPTO_HELLO) {
        checksum_only = true;
    }

    /* SHA256 digest length + 1 byte for crypto command type */
    size_t packet_len = SHA256_DIGEST_LENGTH + 1;
    size_t enc_len;
    packet_struct *packet = NULL;
    uint8_t packet_orig_type = packet_orig->type;
    uint16_t packet_orig_len = packet_orig->len;

    if (checksum_only) {
        /* Original packet + 1 byte for original packet type */
        packet_len += packet_orig->len + 1;
        packet = packet_orig;
        packet_orig = NULL;
    } else if (crypto->key != NULL) {
        /* We need to include the Atrinik command type as well. */
        packet_orig_len += 1;
        enc_len = (((packet_orig_len + AES_BLOCK_SIZE) / AES_BLOCK_SIZE) *
                   AES_BLOCK_SIZE);
        packet_len += enc_len;
        packet = packet_new(0, packet_len, 0);
        packet_len += 2 + 128 / CHAR_BIT;
    } else if (crypto->pkey_ctx != NULL) {
        if (EVP_PKEY_encrypt(crypto->pkey_ctx,
                             NULL,
                             &enc_len,
                             packet->data,
                             packet->len) != 1) {
            LOG(ERROR, "EVP_PKEY_encrypt() failed: %s, for %s",
                ERR_error_string(ERR_get_error(), NULL),
                socket_get_str(crypto->sc));
            goto error;
        }

        packet_len += enc_len;
        packet = packet_new(0, packet_len, 0);
    } else {
        LOG(ERROR, "Cannot encrypt packet!");
        goto error;
    }

    if (unlikely(packet_len > UINT16_MAX)) {
        LOG(ERROR, "Crypto packet is too large: %" PRIu64,
            (uint64_t) packet_len);
        goto error;
    }

    /* Construct the crypto packet metadata header */
    packet_debug_data(packet_meta, 0, "Crypto packet length");
    packet_append_uint16(packet_meta, (uint16_t) packet_len);
    packet_debug_data(packet_meta, 0, "Crypto packet type");
    if (checksum_only) {
        packet_append_uint8(packet_meta, CRYPTO_CMD_CHECKSUM);
    } else {
        packet_append_uint8(packet_meta, CRYPTO_CMD_ENCRYPTED);
    }

    if (checksum_only || crypto->key == NULL) {
        packet_debug_data(packet_meta, 0, "Atrinik packet type");
        packet_append_uint8(packet_meta, packet_orig_type);
    } else {
        packet_debug_data(packet_meta, 0, "Decrypted length");
        packet_append_uint16(packet_meta, packet_orig_len);
    }

    if (checksum_only) {
    } else if (crypto->key != NULL) {
        if (EVP_EncryptInit_ex(crypto->cipher_ctx,
                               EVP_aes_256_gcm(),
                               NULL,
                               crypto->key,
                               crypto->iv) != 1) {
            LOG(ERROR, "EVP_EncryptInit_ex() failed: %s",
                ERR_error_string(ERR_get_error(), NULL));
            goto error;
        }

        packet->len += enc_len;
        enc_len = 0;

        int new_len = 0;
        if (EVP_EncryptUpdate(crypto->cipher_ctx,
                              packet->data,
                              &new_len,
                              &packet_orig_type,
                              sizeof(packet_orig_type)) != 1) {
            LOG(ERROR, "EVP_EncryptUpdate() failed: %s",
                ERR_error_string(ERR_get_error(), NULL));
            goto error;
        }
        enc_len += new_len;

        if (packet_orig->len != 0) {
            if (EVP_EncryptUpdate(crypto->cipher_ctx,
                                  packet->data + enc_len,
                                  &new_len,
                                  packet_orig->data,
                                  packet_orig->len) != 1) {
                LOG(ERROR, "EVP_EncryptUpdate() failed: %s",
                    ERR_error_string(ERR_get_error(), NULL));
                goto error;
            }
            enc_len += new_len;
        }

        if (EVP_EncryptFinal_ex(crypto->cipher_ctx,
                                packet->data + enc_len,
                                &new_len) != 1) {
            LOG(ERROR, "EVP_EncryptFinal_ex() failed: %s",
                ERR_error_string(ERR_get_error(), NULL));
            goto error;
        }
        enc_len += new_len;

        /* Zero out the rest of the packet. */
        memset(packet->data + enc_len, 0, packet->len - enc_len);

        /* Get the tag */
        unsigned char tag[128 / CHAR_BIT];
	if (EVP_CIPHER_CTX_ctrl(crypto->cipher_ctx,
                                EVP_CTRL_GCM_GET_TAG,
                                sizeof(tag),
                                tag) != 1) {
            LOG(ERROR, "EVP_CIPHER_CTX_ctrl() failed: %s",
                ERR_error_string(ERR_get_error(), NULL));
            goto error;
        }

        packet_append_data_len(packet, tag, sizeof(tag));
    } else if (crypto->pkey_ctx != NULL) {
        size_t new_len;
        if (EVP_PKEY_encrypt(crypto->pkey_ctx,
                             packet->data,
                             &new_len,
                             packet_orig->data,
                             packet_orig->len) != 1 ||
            enc_len != new_len) {
            LOG(ERROR, "EVP_PKEY_encrypt() failed: %s, for %s",
                ERR_error_string(ERR_get_error(), NULL),
                socket_get_str(crypto->sc));
            goto error;
        }

        packet->len += new_len;
    } else {
        LOG(ERROR, "Cannot encrypt packet!");
        goto error;
    }

    SHA256_CTX ctx;
    if (SHA256_Init(&ctx) != 1) {
        LOG(ERROR, "SHA256_Init() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    if (checksum_only || crypto->key == NULL) {
        /* Checksum the Atrinik packet type */
        if (SHA256_Update(&ctx,
                          &packet_orig_type,
                          sizeof(packet_orig_type)) != 1) {
            LOG(ERROR, "SHA256_Update() failed: %s",
                ERR_error_string(ERR_get_error(), NULL));
            goto error;
        }
    } else {
        /* Checksum the original packet length */
        if (SHA256_Update(&ctx,
                          &packet_orig_len,
                          sizeof(packet_orig_len)) != 1) {
            LOG(ERROR, "SHA256_Update() failed: %s",
                ERR_error_string(ERR_get_error(), NULL));
            goto error;
        }
    }

    /* Checksum the payload */
    if (SHA256_Update(&ctx, packet->data, packet->len) != 1) {
        LOG(ERROR, "SHA256_Update() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    /* Checksum the secret */
    if (crypto != NULL &&
        crypto->done &&
        SHA256_Update(&ctx,
                      crypto->secret,
                      sizeof(crypto->secret)) != 1) {
        LOG(ERROR, "SHA256_Update() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    unsigned char digest[SHA256_DIGEST_LENGTH];
    if (SHA256_Final(digest, &ctx) != 1) {
        LOG(ERROR, "SHA256_Final() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    packet_debug_data(packet, 0, "SHA256 checksum");
    packet_append_data_len(packet, digest, sizeof(digest));

    goto out;

error:
    if (packet != NULL) {
        packet_free(packet);
        packet = NULL;
    }

out:
    if (packet_orig != NULL) {
        packet_free(packet_orig);
    }

    return packet;
}

/**
 * Decrypts the specified packet data.
 *
 * @param sc
 * Socket.
 * @param data
 * Buffer with the packet data.
 * @param len
 * Length of the encrypted packet.
 * @param[out] data_out
 * Will contain decrypted packet on success. Must be freed.
 * @param[out] len_out
 * Length of the decrypted packet.
 * @return
 * True on success, false on failure.
 */
bool
socket_crypto_decrypt (socket_t *sc,
                       uint8_t  *data,
                       size_t    len,
                       uint8_t **data_out,
                       size_t   *len_out)
{
    HARD_ASSERT(sc != NULL);
    HARD_ASSERT(data != NULL);
    HARD_ASSERT(data_out != NULL);
    HARD_ASSERT(len_out != NULL);

    socket_crypto_t *crypto = socket_get_crypto(sc);

    unsigned char *decrypted = NULL;
    *data_out = NULL;
    *len_out = 0;

    if (len < SHA256_DIGEST_LENGTH + 1) {
        LOG(ERROR,
            "Crypto packet length is too short, %" PRIu64 " bytes from %s",
            (uint64_t) len, socket_get_str(sc));
        goto error;
    }

    size_t pos = 0;
    uint8_t type = packet_to_uint8(data, len, &pos);
    if (type == 0 || type >= CRYPTO_CMD_MAX) {
        LOG(ERROR, "Invalid crypto packet %" PRIu8 " from %s",
            type, socket_get_str(sc));
        goto error;
    }

    uint16_t decrypted_len;
    if (crypto != NULL && type == CRYPTO_CMD_ENCRYPTED && crypto->key != NULL) {
        decrypted_len = packet_to_uint16(data, len, &pos);
    }

    *len_out = len - pos - SHA256_DIGEST_LENGTH;
    *data_out = emalloc(*len_out);
    memcpy(*data_out, data + pos, *len_out);
    pos += *len_out;

    SHA256_CTX ctx;
    if (SHA256_Init(&ctx) != 1) {
        LOG(ERROR, "SHA256_Init() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    if (crypto != NULL && type == CRYPTO_CMD_ENCRYPTED && crypto->key != NULL) {
        if (SHA256_Update(&ctx, &decrypted_len, sizeof(decrypted_len)) != 1) {
            LOG(ERROR, "SHA256_Update() failed: %s",
                ERR_error_string(ERR_get_error(), NULL));
            goto error;
        }
    }

    /* Checksum the payload */
    if (SHA256_Update(&ctx, *data_out, *len_out) != 1) {
        LOG(ERROR, "SHA256_Update() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    /* Checksum the secret */
    if (crypto != NULL &&
        crypto->done &&
        SHA256_Update(&ctx,
                      crypto->secret,
                      sizeof(crypto->secret)) != 1) {
        LOG(ERROR, "SHA256_Update() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    unsigned char digest[SHA256_DIGEST_LENGTH];
    if (SHA256_Final(digest, &ctx) != 1) {
        LOG(ERROR, "SHA256_Final() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    if (memcmp(digest, data + pos, sizeof(digest)) != 0) {
        LOG(SYSTEM, "!!! SHA256 DIGEST OF PACKET IS INVALID !!!");
        LOG(SYSTEM, "It is highly probable someone is hijacking your "
                    "connection (MITM attack).");
        LOG(SYSTEM, "Packet of size %" PRIu64 " from %s",
            (uint64_t) len, socket_get_str(sc));

        char digest_ascii[SHA256_DIGEST_LENGTH * 3 + 1];
        string_tohex(data + pos,
                     sizeof(digest),
                     digest_ascii,
                     sizeof(digest_ascii),
                     true);
        LOG(SYSTEM, "SHA256 digest received: %s", digest_ascii);
        string_tohex(digest,
                     sizeof(digest),
                     digest_ascii,
                     sizeof(digest_ascii),
                     true);
        LOG(SYSTEM, "SHA256 digest computed: %s", digest_ascii);
        goto error;
    }

    /* Only wanted to verify the checksum, so stop right here. */
    if (crypto == NULL || type == CRYPTO_CMD_CHECKSUM) {
        return true;
    }

    if (crypto->key == NULL) {
        LOG(ERROR, "Crypto key hasn't been generated!");
        goto error;
    }

    size_t tag_len = 128 / CHAR_BIT;
    pos = 0;
    if (*len_out < tag_len) {
        LOG(PACKET, "Malformed packet detected: %s",
            socket_get_str(sc));
        goto error;
    }

    unsigned char *tag = *data_out + (*len_out - tag_len);
    *len_out -= tag_len;

    if (EVP_DecryptInit_ex(crypto->cipher_ctx,
                           EVP_aes_256_gcm(),
                           NULL,
                           crypto->key,
                           crypto->iv) != 1) {
        LOG(ERROR, "EVP_DecryptInit_ex() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    size_t enc_len = (((decrypted_len + AES_BLOCK_SIZE) / AES_BLOCK_SIZE) *
                      AES_BLOCK_SIZE);
    decrypted = emalloc(enc_len);

    int new_len = 0;
    size_t dec_len = 0;
    if (EVP_DecryptUpdate(crypto->cipher_ctx,
                          decrypted,
                          &new_len,
                          *data_out,
                          *len_out) != 1) {
        LOG(ERROR, "EVP_DecryptUpdate() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }
    dec_len += new_len;

    /* Set expected tag value. Works in OpenSSL 1.0.1d and later */
    if (EVP_CIPHER_CTX_ctrl(crypto->cipher_ctx,
                            EVP_CTRL_GCM_SET_TAG,
                            tag_len,
                            tag) != 1) {
        LOG(ERROR, "EVP_CIPHER_CTX_ctrl() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    new_len = 0;
    if (EVP_DecryptFinal_ex(crypto->cipher_ctx,
                            decrypted + dec_len,
                            &new_len) > 0) {
        LOG(ERROR, "EVP_DecryptFinal_ex() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }
    dec_len += new_len;

    efree(*data_out);
    *data_out = decrypted;
    *len_out = decrypted_len;

    return true;

error:
    if (*data_out != NULL) {
        efree(*data_out);
    }

    if (decrypted != NULL) {
        efree(decrypted);
    }

    *len_out = 0;

    return false;
}
