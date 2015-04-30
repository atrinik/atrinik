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
 * Socket related functions.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <toolkit_string.h>

/**
 * The OpenSSL context.
 */
static SSL_CTX *ssl_context;

/**
 * Whitelist of candidate ciphers.
 */
static const char *const ciphers_candidate[] = {
    "AES128-GCM-SHA256", "AES128-SHA256", "AES256-SHA256", /* strong ciphers */
    "AES128-SHA", "AES256-SHA", /* strong ciphers, also in older versions */
    "RC4-SHA", "RC4-MD5", /* backwards compatibility, supposed to be weak */
    "DES-CBC3-SHA", "DES-CBC3-MD5", /* more backwards compatibility */
    NULL
};

static SSL_CTX *socket_ssl_ctx_create(void);
static void socket_ssl_ctx_destroy(SSL_CTX *ctx);

TOOLKIT_API();

TOOLKIT_INIT_FUNC(socket)
{
    OPENSSL_config(NULL);
    SSL_load_error_strings();
    SSL_library_init();

    ssl_context = socket_ssl_ctx_create();
}
TOOLKIT_INIT_FUNC_FINISH

TOOLKIT_DEINIT_FUNC(socket)
{
    socket_ssl_ctx_destroy(ssl_context);
}
TOOLKIT_DEINIT_FUNC_FINISH

socket_t *socket_create(const char *host, uint16_t port)
{
    int handle;
    socket_t *sc;
#ifndef WIN32
    struct protoent *protox;
#endif

#ifndef WIN32
    protox = getprotobyname("tcp");

    if (protox == NULL) {
        return NULL;
    }

    handle = socket(PF_INET, SOCK_STREAM, protox->p_proto);

#else
    handle = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif

    if (handle == -1) {
        return NULL;
    }

    sc = calloc(1, sizeof(*sc));
    sc->handle = handle;
    sc->host = strdup(host);
    sc->port = port;

    return sc;
}

int socket_connect(socket_t *sc)
{
    struct hostent *host;
    struct sockaddr_in server;

    if (sc == NULL) {
        return 0;
    }

    host = gethostbyname(sc->host);

    if (host == NULL) {
        return 0;
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(sc->port);
    server.sin_addr = *((struct in_addr *) host->h_addr);
    memset(&server.sin_zero, 0, sizeof(server.sin_zero));

    if (connect(sc->handle, (struct sockaddr *) &server,
            sizeof(struct sockaddr)) == -1) {
        return 0;
    }

    return 1;
}

void socket_destroy(socket_t *sc)
{
    close(sc->handle);
}

/**
 * Selects the best cipher from the list of available ciphers, which is
 * obtained by creating a dummy SSL session.
 * @param ctx Context to select the best cipher for.
 * @return 1 on success, 0 on failure.
 */
static int socket_ssl_ctx_select_cipher(SSL_CTX *ctx)
{
    SSL *ssl;
    STACK_OF(SSL_CIPHER) *active_ciphers;
    char ciphers[300];
    const char *const *c;
    int i;

    ssl = SSL_new(ctx);

    if (ssl == NULL) {
        return 0;
    }

    active_ciphers = SSL_get_ciphers(ssl);

    if (active_ciphers == NULL) {
        return 0;
    }

    ciphers[0] = '\0';

    for (c = ciphers_candidate; *c; c++) {
        for (i = 0; i < sk_SSL_CIPHER_num(active_ciphers); i++) {
            if (strcmp(SSL_CIPHER_get_name(
                    sk_SSL_CIPHER_value(active_ciphers, i)), *c) == 0) {
                if (*ciphers != '\0') {
                    snprintfcat(VS(ciphers), ":");
                }

                snprintfcat(VS(ciphers), "%s", *c);
                break;
            }
        }
    }

    SSL_free(ssl);

    /* Apply final cipher list. */
    if (SSL_CTX_set_cipher_list(ctx, ciphers) != 1) {
        return 0;
    }

    return 1;
}

static SSL_CTX *socket_ssl_ctx_create(void)
{
    const SSL_METHOD *req_method;
    SSL_CTX *ctx;

    req_method = TLSv1_client_method();
    ctx = SSL_CTX_new(req_method);

    if (ctx == NULL) {
        return NULL;
    }

    /* Configure a client connection context. */
    SSL_CTX_set_options(ctx, SSL_OP_NO_COMPRESSION);

    /* Adjust the ciphers list based on a whitelist. First enable all
     * ciphers of at least medium strength, to get the list which is
     * compiled into OpenSSL. */
    if (SSL_CTX_set_cipher_list(ctx, "HIGH:MEDIUM") != 1) {
        return NULL;
    }

    /* Select best cipher. */
    if (socket_ssl_ctx_select_cipher(ctx) != 1) {
        return NULL;
    }

    /* Load the set of trusted root certificates. */
    if (!SSL_CTX_set_default_verify_paths(ctx)) {
        return NULL;
    }

    return ctx;
}

static void socket_ssl_ctx_destroy(SSL_CTX *ctx)
{
    SSL_CTX_free(ctx);
}

SSL *socket_ssl_create(socket_t *sc, SSL_CTX *ctx)
{
    SSL *ssl;
    int ret;
    X509 *peercert;
    char peer_CN[256];

    ssl = SSL_new(ctx);

    if (ssl == NULL) {
        return NULL;
    }

    SSL_set_fd(ssl, sc->handle);

    /* Enable the ServerNameIndication extension */
    if (!SSL_set_tlsext_host_name(ssl, sc->host)) {
        return NULL;
    }

    /* Perform the TLS handshake with the server. */
    ret = SSL_connect(ssl);

    /* Error status can be 0 or negative. */
    if (ret != 1) {
        return NULL;
    }

    /* Obtain the server certificate. */
    peercert = SSL_get_peer_certificate(ssl);

    if (peercert == NULL) {
        LOG(SYSTEM, "Server's peer certificate is missing.");
        return NULL;
    }

    ret = SSL_get_verify_result(ssl);

    /* Check the certificate verification result. */
    if (ret != X509_V_OK) {
        LOG(SYSTEM, "Verify result: %s",
                X509_verify_cert_error_string(ret));
        X509_free(peercert);
        return NULL;
    }

    /* Check if the server certificate matches the host name used to
     * establish the connection. */
    X509_NAME_get_text_by_NID(X509_get_subject_name(peercert), NID_commonName,
            peer_CN, sizeof(peer_CN));

    if (strcasecmp(peer_CN, sc->host) != 0) {
        LOG(SYSTEM, "Peer name %s doesn't match host name %s\n", peer_CN,
                sc->host);
        X509_free(peercert);
        return NULL;
    }

    X509_free(peercert);

    return ssl;
}

void socket_ssl_destroy(SSL *ssl)
{
    int ret;

    /* Send the close_notify alert. */
    ret = SSL_shutdown(ssl);

    switch (ret) {
        /* A close_notify alert has already been received. */
    case 1:
        break;

        /* Wait for the close_notify alert from the peer. */
    case 0:
        ret = SSL_shutdown(ssl);

        switch (ret) {
        case 0:
            LOG(SYSTEM,  "second SSL_shutdown returned zero");
            break;

        case 1:
            break;

        default:
            LOG(SYSTEM,  "SSL_shutdown 2 %d", ret);
        }
        break;

    default:
        LOG(SYSTEM,  "SSL_shutdown 1 %d", ret);
    }

    SSL_free(ssl);
}
