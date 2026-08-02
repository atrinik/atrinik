/**
 * @file
 *
 * OpenSSL 3.5 QUIC transport support.
 */

#include "socket_private.h"
#include "socket_crypto.h"
#include "string.h"

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/sha.h>

#if OPENSSL_VERSION_NUMBER >= 0x30500000L

static const unsigned char socket_quic_alpn[] = {
    9, 'a', 't', 'r', 'i', 'n', 'i', 'k', '/', '1'
};

static int
socket_quic_select_alpn (SSL                *ssl,
                         const unsigned char **out,
                         unsigned char       *out_len,
                         const unsigned char *in,
                         unsigned int         in_len,
                         void                *arg)
{
    if (SSL_select_next_proto((unsigned char **) out,
                              out_len,
                              socket_quic_alpn,
                              sizeof(socket_quic_alpn),
                              in,
                              in_len) == OPENSSL_NPN_NEGOTIATED) {
        return SSL_TLSEXT_ERR_OK;
    }

    return SSL_TLSEXT_ERR_ALERT_FATAL;
}

static void
socket_quic_log_error (const char *operation)
{
    unsigned long error = ERR_get_error();
    LOG(ERROR,
        "QUIC %s failed: %s",
        operation,
        error != 0 ? ERR_error_string(error, NULL) : "unknown error");
}

static bool
socket_quic_generate_identity (SSL_CTX *ctx)
{
    EVP_PKEY *key = EVP_PKEY_Q_keygen(NULL, NULL, "EC", "prime256v1");
    X509 *cert = X509_new();
    if (key == NULL || cert == NULL ||
        X509_set_version(cert, 2) != 1 ||
        ASN1_INTEGER_set(X509_get_serialNumber(cert), (long) time(NULL)) != 1 ||
        X509_gmtime_adj(X509_getm_notBefore(cert), 0) == NULL ||
        X509_gmtime_adj(X509_getm_notAfter(cert), 31536000L) == NULL ||
        X509_set_pubkey(cert, key) != 1) {
        EVP_PKEY_free(key);
        X509_free(cert);
        return false;
    }

    X509_NAME *name = X509_get_subject_name(cert);
    bool ok = name != NULL &&
              X509_NAME_add_entry_by_txt(
                  name,
                  "CN",
                  MBSTRING_ASC,
                  (const unsigned char *) "Atrinik private server",
                  -1,
                  -1,
                  0) == 1 &&
              X509_set_issuer_name(cert, name) == 1 &&
              X509_sign(cert, key, EVP_sha256()) > 0 &&
              SSL_CTX_use_certificate(ctx, cert) == 1 &&
              SSL_CTX_use_PrivateKey(ctx, key) == 1 &&
              SSL_CTX_check_private_key(ctx) == 1;

    EVP_PKEY_free(key);
    X509_free(cert);
    return ok;
}

static SSL_CTX *
socket_quic_server_ctx (void)
{
    SSL_CTX *ctx = SSL_CTX_new(OSSL_QUIC_server_method());
    if (ctx == NULL) {
        socket_quic_log_error("server context creation");
        return NULL;
    }

    const char *cert_pem = socket_crypto_get_cert();
    const char *key_pem = socket_crypto_get_cert_key();
    bool ok;
    if (cert_pem == NULL || key_pem == NULL) {
        ok = socket_quic_generate_identity(ctx);
        if (ok) {
            LOG(INFO,
                "Using an automatically generated QUIC identity; configure "
                "crypto_cert and crypto_cert_key for a stable server ID");
        }
    } else {
        BIO *cert_bio = BIO_new_mem_buf(cert_pem, -1);
        BIO *key_bio = BIO_new_mem_buf(key_pem, -1);
        X509 *cert = cert_bio != NULL
            ? PEM_read_bio_X509(cert_bio, NULL, NULL, NULL)
            : NULL;
        EVP_PKEY *key = key_bio != NULL
            ? PEM_read_bio_PrivateKey(key_bio, NULL, NULL, NULL)
            : NULL;

        ok = cert != NULL &&
             key != NULL &&
             SSL_CTX_use_certificate(ctx, cert) == 1 &&
             SSL_CTX_use_PrivateKey(ctx, key) == 1 &&
             SSL_CTX_check_private_key(ctx) == 1;

        X509_free(cert);
        EVP_PKEY_free(key);
        BIO_free(cert_bio);
        BIO_free(key_bio);
    }

    if (!ok) {
        socket_quic_log_error("certificate loading");
        SSL_CTX_free(ctx);
        return NULL;
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
    SSL_CTX_set_alpn_select_cb(ctx, socket_quic_select_alpn, NULL);
    return ctx;
}

static bool
socket_quic_peer_addr (const struct sockaddr_storage *addr, BIO_ADDR *peer)
{
    const struct sockaddr *generic = (const struct sockaddr *) addr;
    if (generic->sa_family == AF_INET) {
        const struct sockaddr_in *v4 = (const struct sockaddr_in *) addr;
        return BIO_ADDR_rawmake(peer,
                                AF_INET,
                                &v4->sin_addr,
                                sizeof(v4->sin_addr),
                                v4->sin_port) == 1;
    }
#ifdef HAVE_IPV6
    if (generic->sa_family == AF_INET6) {
        const struct sockaddr_in6 *v6 = (const struct sockaddr_in6 *) addr;
        return BIO_ADDR_rawmake(peer,
                                AF_INET6,
                                &v6->sin6_addr,
                                sizeof(v6->sin6_addr),
                                v6->sin6_port) == 1;
    }
#endif
    return false;
}

static bool
socket_quic_wait (SSL *ssl)
{
    int fd = SSL_get_fd(ssl);
    if (fd == -1) {
        return false;
    }

    fd_set read_fds, write_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    if (SSL_net_read_desired(ssl)) {
        FD_SET(fd, &read_fds);
    }
    if (SSL_net_write_desired(ssl)) {
        FD_SET(fd, &write_fds);
    }

    struct timeval timeout = {0, 100000};
    struct timeval event_timeout;
    int infinite = 1;
    if (SSL_get_event_timeout(ssl, &event_timeout, &infinite) &&
        !infinite &&
        timercmp(&event_timeout, &timeout, <)) {
        timeout = event_timeout;
    }

    int rc = select(fd + 1,
                    &read_fds,
                    &write_fds,
                    NULL,
                    &timeout);
    return rc >= 0 || errno == EINTR;
}

static bool
socket_quic_check_fingerprint (SSL *ssl, const char *expected)
{
    if (expected == NULL || strlen(expected) != SHA256_DIGEST_LENGTH * 2) {
        LOG(ERROR, "Invalid QUIC certificate fingerprint");
        return false;
    }

    X509 *cert = SSL_get1_peer_certificate(ssl);
    if (cert == NULL) {
        LOG(ERROR, "QUIC peer did not provide a certificate");
        return false;
    }

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    bool ok = X509_digest(cert, EVP_sha256(), digest, &digest_len) == 1 &&
              digest_len == SHA256_DIGEST_LENGTH;
    X509_free(cert);
    if (!ok) {
        socket_quic_log_error("certificate fingerprint");
        return false;
    }

    char actual[SHA256_DIGEST_LENGTH * 2 + 1];
    string_tohex(digest, digest_len, VS(actual), false);
    if (strcasecmp(actual, expected) != 0) {
        LOG(ERROR,
            "QUIC certificate fingerprint mismatch (expected %s, got %s)",
            expected,
            actual);
        return false;
    }

    return true;
}

socket_t *
socket_quic_server_create (const char *host,
                           uint16_t    port,
                           bool        dual_stack)
{
    SSL_CTX *ctx = socket_quic_server_ctx();
    if (ctx == NULL) {
        return NULL;
    }

    char port_str[6];
    snprintf(VS(port_str), "%" PRIu16, port);
    struct addrinfo hints, *addresses = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;

    int rc = getaddrinfo(host, port_str, &hints, &addresses);
    if (rc != 0) {
        LOG(ERROR, "Cannot resolve QUIC bind address: %s", gai_strerror(rc));
        SSL_CTX_free(ctx);
        return NULL;
    }

    socket_t *sc = ecalloc(1, sizeof(*sc));
    sc->handle = -1;
    sc->owns_handle = true;
    sc->transport = SOCKET_TRANSPORT_QUIC_LISTENER;
    sc->role = SOCKET_ROLE_SERVER;
    sc->port = port;
    sc->host = host != NULL ? estrdup(host) : NULL;
    sc->quic_ctx = ctx;

    for (struct addrinfo *ai = addresses; ai != NULL; ai = ai->ai_next) {
#ifdef HAVE_IPV6
        if (host == NULL && ai->ai_family != AF_INET6) {
            continue;
        }
#endif
        sc->handle = socket(ai->ai_family, SOCK_DGRAM, IPPROTO_UDP);
        if (sc->handle == -1) {
            continue;
        }

#ifdef HAVE_IPV6
        if (ai->ai_family == AF_INET6) {
            int v6only = !dual_stack;
            if (setsockopt(sc->handle,
                           IPPROTO_IPV6,
                           IPV6_V6ONLY,
                           (const char *) &v6only,
                           sizeof(v6only)) != 0) {
                socket_close(sc);
                sc->handle = -1;
                continue;
            }
        }
#endif

        int reuse = 1;
        setsockopt(sc->handle,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   (const char *) &reuse,
                   sizeof(reuse));
        if (bind(sc->handle, ai->ai_addr, ai->ai_addrlen) == 0) {
            memcpy(&sc->addr, ai->ai_addr, ai->ai_addrlen);
            break;
        }

        socket_close(sc);
        sc->handle = -1;
    }

    freeaddrinfo(addresses);
    if (sc->handle == -1 || !socket_opt_non_blocking(sc, true)) {
        socket_destroy(sc);
        return NULL;
    }

    sc->quic = SSL_new_listener(ctx, 0);
    BIO *bio = sc->handle != -1
        ? BIO_new_dgram(sc->handle, BIO_NOCLOSE)
        : NULL;
    bool configured = sc->quic != NULL && bio != NULL;
    if (configured) {
        SSL_set_bio(sc->quic, bio, bio);
        bio = NULL;
        configured = SSL_set_blocking_mode(sc->quic, 0) == 1 &&
                     SSL_listen(sc->quic) == 1;
    }
    BIO_free(bio);
    if (!configured) {
        socket_quic_log_error("listener creation");
        socket_destroy(sc);
        return NULL;
    }

    return sc;
}

socket_t *
socket_quic_client_create (const char *host,
                           uint16_t    port,
                           const char *certificate_sha256,
                           const char *rendezvous_url,
                           const char *stun_endpoint)
{
    HARD_ASSERT(host != NULL);

    char port_str[6];
    snprintf(VS(port_str), "%" PRIu16, port);
    struct addrinfo hints, *addresses = NULL, *selected = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_NUMERICSERV;

    int rc = getaddrinfo(host, port_str, &hints, &addresses);
    if (rc != 0) {
        LOG(ERROR, "Cannot resolve QUIC peer %s: %s", host, gai_strerror(rc));
        return NULL;
    }

    socket_t *sc = ecalloc(1, sizeof(*sc));
    sc->handle = -1;
    sc->owns_handle = true;
    sc->transport = SOCKET_TRANSPORT_QUIC_CONNECTION;
    sc->role = SOCKET_ROLE_CLIENT;
    sc->port = port;
    sc->host = estrdup(host);

    for (struct addrinfo *ai = addresses; ai != NULL; ai = ai->ai_next) {
        sc->handle = socket(ai->ai_family, SOCK_DGRAM, IPPROTO_UDP);
        if (sc->handle == -1) {
            continue;
        }
        memcpy(&sc->addr, ai->ai_addr, ai->ai_addrlen);
        selected = ai;
        break;
    }

    if (selected == NULL) {
        freeaddrinfo(addresses);
        socket_destroy(sc);
        return NULL;
    }

    if (rendezvous_url != NULL &&
        !socket_rendezvous_client(sc, rendezvous_url, stun_endpoint)) {
        LOG(ERROR,
            "Rendezvous signaling failed; attempting the direct candidate");
    }

    if (connect(sc->handle,
                (struct sockaddr *) &sc->addr,
                selected->ai_addrlen) != 0 ||
        !socket_opt_non_blocking(sc, true)) {
        freeaddrinfo(addresses);
        socket_destroy(sc);
        return NULL;
    }

    sc->quic_ctx = SSL_CTX_new(OSSL_QUIC_client_method());
    sc->quic = sc->quic_ctx != NULL ? SSL_new(sc->quic_ctx) : NULL;
    BIO_ADDR *peer = BIO_ADDR_new();
    BIO *bio = sc->handle != -1 ? BIO_new_dgram(sc->handle, BIO_NOCLOSE) : NULL;
    bool configured = sc->quic_ctx != NULL &&
                      sc->quic != NULL &&
                      peer != NULL &&
                      bio != NULL &&
                      socket_quic_peer_addr(&sc->addr, peer);

    if (configured) {
        SSL_CTX_set_verify(sc->quic_ctx, SSL_VERIFY_NONE, NULL);
        SSL_set_bio(sc->quic, bio, bio);
        bio = NULL;
        configured = SSL_set_blocking_mode(sc->quic, 0) == 1 &&
                     SSL_set_alpn_protos(sc->quic,
                                         socket_quic_alpn,
                                         sizeof(socket_quic_alpn)) == 0 &&
                     SSL_set1_initial_peer_addr(sc->quic, peer) == 1;
    }

    BIO_free(bio);
    BIO_ADDR_free(peer);
    freeaddrinfo(addresses);

    if (!configured) {
        socket_quic_log_error("client setup");
        socket_destroy(sc);
        return NULL;
    }

    TIMER_START(connect_timer);
    for (;;) {
        int result = SSL_connect(sc->quic);
        if (result == 1) {
            break;
        }

        int ssl_error = SSL_get_error(sc->quic, result);
        if (ssl_error != SSL_ERROR_WANT_READ &&
            ssl_error != SSL_ERROR_WANT_WRITE) {
            socket_quic_log_error("client handshake");
            socket_destroy(sc);
            return NULL;
        }

        TIMER_UPDATE(connect_timer);
        if (TIMER_GET(connect_timer) > SOCKET_TIMEOUT_MS ||
            !socket_quic_wait(sc->quic)) {
            LOG(ERROR, "QUIC connection to %s timed out", host);
            socket_destroy(sc);
            return NULL;
        }
    }

    if (!socket_quic_check_fingerprint(sc->quic, certificate_sha256)) {
        socket_destroy(sc);
        return NULL;
    }

    return sc;
}

bool
socket_certificate_sha256 (socket_t *sc, char fingerprint[65])
{
    HARD_ASSERT(sc != NULL);
    HARD_ASSERT(fingerprint != NULL);

    X509 *cert = NULL;
    if (sc->transport == SOCKET_TRANSPORT_QUIC_LISTENER) {
        cert = SSL_CTX_get0_certificate(sc->quic_ctx);
    } else if (sc->transport == SOCKET_TRANSPORT_QUIC_CONNECTION) {
        cert = SSL_get0_peer_certificate(sc->quic);
    }

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    if (cert == NULL ||
        X509_digest(cert, EVP_sha256(), digest, &digest_len) != 1 ||
        digest_len != SHA256_DIGEST_LENGTH) {
        return false;
    }

    return string_tohex(digest, digest_len, fingerprint, 65, false) == 64;
}

#else

socket_t *
socket_quic_server_create (const char *host,
                           uint16_t    port,
                           bool        dual_stack)
{
    LOG(ERROR, "QUIC requires OpenSSL 3.5 or newer");
    return NULL;
}

socket_t *
socket_quic_client_create (const char *host,
                           uint16_t    port,
                           const char *certificate_sha256,
                           const char *rendezvous_url,
                           const char *stun_endpoint)
{
    LOG(ERROR, "QUIC requires OpenSSL 3.5 or newer");
    return NULL;
}

bool
socket_certificate_sha256 (socket_t *sc, char fingerprint[65])
{
    return false;
}

#endif

bool
socket_is_quic (socket_t *sc)
{
    HARD_ASSERT(sc != NULL);
    return sc->transport != SOCKET_TRANSPORT_TCP;
}
