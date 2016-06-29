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
 * Handles connection to the metaserver and receiving data from it.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <toolkit_string.h>
#include <curl.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/valid.h>
#include <libxml/xmlschemas.h>

#include <openssl/sha.h>
#include <openssl/err.h>

/**
 * Macro to check for XML string equality without the annoying xmlChar
 * pointer casts.
 *
 * @param s1
 * First string to compare.
 * @param s2
 * Second string to compare.
 * @return
 * 1 if both strings are equal, 0 otherwise.
 */
#define XML_STR_EQUAL(s1, s2) \
    xmlStrEqual((const xmlChar *) s1, (const xmlChar *) s2)

#ifdef __MINGW32__
#   define xmlFree free
#endif

/** Are we connecting to the metaserver? */
static int metaserver_connecting = 1;
/** Mutex to protect ::metaserver_connecting. */
static SDL_mutex *metaserver_connecting_mutex;
/** The list of the servers. */
static server_struct *server_head;
/** Number of the servers. */
static size_t server_count;
/** Mutex to protect ::server_head and ::server_count. */
static SDL_mutex *server_head_mutex;
/** Is metaserver enabled? */
static uint8_t enabled = 1;
/** The string that begins certificate information. */
static const char *cert_begin_str = "=========================="
                                    "     BEGIN INFORMATION     "
                                    "==========================";
/** The string that ends certificate information. */
static const char *cert_end_str = "=========================="
                                  "      END INFORMATION      "
                                  "==========================";

/**
 * Initialize the metaserver data.
 */
void metaserver_init(void)
{
    /* Initialize the data. */
    server_head = NULL;
    server_count = 0;

    /* Initialize mutexes. */
    metaserver_connecting_mutex = SDL_CreateMutex();
    server_head_mutex = SDL_CreateMutex();

    /* Initialize libxml2 */
    LIBXML_TEST_VERSION
}

/**
 * Disable the metaserver.
 */
void metaserver_disable(void)
{
    enabled = 0;
    metaserver_connecting = 0;
}

/**
 * Free the specified server certificate information structure.
 *
 * @param info
 * What to free.
 */
static void
metaserver_cert_free (server_cert_info_t *info)
{
    HARD_ASSERT(info != NULL);

    if (info->name != NULL) {
        efree(info->name);
    }

    if (info->hostname != NULL) {
        efree(info->hostname);
    }

    if (info->ipv4_address != NULL) {
        efree(info->ipv4_address);
    }

    if (info->ipv6_address != NULL) {
        efree(info->ipv6_address);
    }

    if (info->pubkey != NULL) {
        efree(info->pubkey);
    }

    efree(info);
}

/**
 * Free the specified metaserver server node.
 *
 * @param server
 * Node to free.
 */
static void
metaserver_free (server_struct *server)
{
    HARD_ASSERT(server != NULL);

    if (server->hostname != NULL) {
        efree(server->hostname);
    }

    if (server->name != NULL) {
        efree(server->name);
    }

    if (server->version != NULL) {
        efree(server->version);
    }

    if (server->desc != NULL) {
        efree(server->desc);
    }

    if (server->cert_pubkey != NULL) {
        efree(server->cert_pubkey);
    }

    if (server->cert != NULL) {
        efree(server->cert);
    }

    if (server->cert_sig != NULL) {
        efree(server->cert_sig);
    }

    if (server->cert_info != NULL) {
        metaserver_cert_free(server->cert_info);
    }

    efree(server);
}

/**
 * Parse the metaserver certificate information.
 *
 * @param server
 * Metaserver entry.
 * @return
 * True on success, false on failure.
 */
static bool
parse_metaserver_cert (server_struct *server)
{
    HARD_ASSERT(server != NULL);

    if (server->cert == NULL || server->cert_sig == NULL) {
        /* No certificate. */
        return true;
    }

    /* Generate a SHA512 hash of the certificate's contents. */
    unsigned char cert_digest[SHA512_DIGEST_LENGTH];
    if (SHA512((unsigned char *) server->cert,
               strlen(server->cert),
               cert_digest) == NULL) {
        LOG(ERROR, "SHA512() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        return false;
    }

    char cert_hash[SHA512_DIGEST_LENGTH * 2 + 1];
    SOFT_ASSERT_RC(string_tohex(VS(cert_digest),
                                VS(cert_hash),
                                false) == sizeof(cert_hash) - 1,
                   false,
                   "string_tohex failed");
    string_tolower(cert_hash);

    /* Verify the signature. */
    if (!curl_verify(CURL_PKEY_TRUST_ULTIMATE,
                     cert_hash,
                     strlen(cert_hash),
                     server->cert_sig,
                     server->cert_sig_len)) {
        LOG(ERROR, "Failed to verify signature");
        return false;
    }

    server_cert_info_t *info = ecalloc(1, sizeof(*info));

    char buf[MAX_BUF];
    size_t pos = 0;
    bool in_info = false;
    while (string_get_word(server->cert, &pos, '\n', VS(buf), 0)) {
        char *cp = buf;
        string_skip_whitespace(cp);
        string_strip_newline(cp);

        if (*cp == '\0') {
            continue;
        }

        if (strcmp(cp, cert_begin_str) == 0) {
            in_info = true;
            continue;
        } else if (!in_info) {
            continue;
        } else if (strcmp(cp, cert_end_str) == 0) {
            break;
        }

        char *cps[2];
        if (string_split(cp, cps, arraysize(cps), ':') != arraysize(cps)) {
            LOG(ERROR, "Parsing error");
            continue;
        }

        string_tolower(cps[0]);
        string_skip_whitespace(cps[1]);
        const char *key = cps[0];
        const char *value = cps[1];
        char **content = NULL;
        if (strcmp(key, "name") == 0) {
            content = &info->name;
        } else if (strcmp(key, "hostname") == 0) {
            content = &info->hostname;
        } else if (strcmp(key, "ipv4 address") == 0) {
            content = &info->ipv4_address;
        } else if (strcmp(key, "ipv6 address") == 0) {
            content = &info->ipv6_address;
        } else if (strcmp(key, "public key") == 0) {
            content = &info->pubkey;
        } else if (strcmp(key, "port") == 0) {
            info->port = atoi(value);
        } else if (strcmp(key, "crypto port") == 0) {
            info->port_crypto = atoi(value);
        } else {
            LOG(DEVEL, "Unrecognized key: %s", key);
            continue;
        }

        if (content != NULL) {
            StringBuffer *sb = stringbuffer_new();

            if (*content != NULL) {
                stringbuffer_append_string(sb, *content);
                stringbuffer_append_char(sb, '\n');
                efree(*content);
            }

            stringbuffer_append_string(sb, value);
            *content = stringbuffer_finish(sb);
        }
    }

    /* Ensure we got the data we need. */
    if (info->name == NULL ||
        info->hostname == NULL ||
        info->pubkey == NULL ||
        info->port_crypto <= 0 ||
        (info->ipv4_address == NULL) != (info->ipv6_address == NULL)) {
        LOG(ERROR,
            "Certificate is missing required data.");
        goto error;
    }

    /* Ensure certificate attributes match the advertised ones. */
    if (strcmp(info->hostname, server->hostname) != 0) {
        LOG(ERROR,
            "Certificate hostname does not match advertised hostname.");
        goto error;
    }

    if (strcmp(info->name, server->name) != 0) {
        LOG(ERROR,
            "Certificate name does not match advertised name.");
        goto error;
    }

    if (info->port != server->port) {
        LOG(ERROR,
            "Certificate port does not match advertised port.");
        goto error;
    }

    if (info->port_crypto != server->port_crypto) {
        LOG(ERROR,
            "Certificate crypto port does not match advertised crypto port.");
        goto error;
    }

    server->cert_info = info;
    return true;

error:
    metaserver_cert_free(info);
    return false;
}

/**
 * Parse a single metaserver data node within a 'server' node.
 *
 * @param node
 * The data node.
 * @param server
 * Allocated server structure.
 * @return
 * True on success, false on failure.
 */
static bool
parse_metaserver_data_node (xmlNodePtr node, server_struct *server)
{
    HARD_ASSERT(node != NULL);
    HARD_ASSERT(server != NULL);

    xmlChar *content = xmlNodeGetContent(node);
    SOFT_ASSERT_LABEL(content != NULL && *content != '\0',
                      error,
                      "Parsing error");

    if (XML_STR_EQUAL(node->name, "Hostname")) {
        SOFT_ASSERT_LABEL(server->hostname == NULL, error, "Parsing error");
        server->hostname = estrdup((const char *) content);
    } else if (XML_STR_EQUAL(node->name, "Port")) {
        SOFT_ASSERT_LABEL(server->port == 0, error, "Parsing error");
        server->port = atoi((const char *) content);
    } else if (XML_STR_EQUAL(node->name, "PortCrypto")) {
        SOFT_ASSERT_LABEL(server->port_crypto == -1, error, "Parsing error");
        server->port_crypto = atoi((const char *) content);
    } else if (XML_STR_EQUAL(node->name, "Name")) {
        SOFT_ASSERT_LABEL(server->name == NULL, error, "Parsing error");
        server->name = estrdup((const char *) content);
    } else if (XML_STR_EQUAL(node->name, "PlayersCount")) {
        SOFT_ASSERT_LABEL(server->player == 0, error, "Parsing error");
        server->player = atoi((const char *) content);
    } else if (XML_STR_EQUAL(node->name, "Version")) {
        SOFT_ASSERT_LABEL(server->version == NULL, error, "Parsing error");
        server->version = estrdup((const char *) content);
    } else if (XML_STR_EQUAL(node->name, "TextComment")) {
        SOFT_ASSERT_LABEL(server->desc == NULL, error, "Parsing error");
        server->desc = estrdup((const char *) content);
    } else if (XML_STR_EQUAL(node->name, "CertificatePublicKey")) {
        SOFT_ASSERT_LABEL(server->cert_pubkey == NULL, error, "Parsing error");
        server->cert_pubkey = estrdup((const char *) content);
    } else if (XML_STR_EQUAL(node->name, "Certificate")) {
        SOFT_ASSERT_LABEL(server->cert == NULL, error, "Parsing error");
        server->cert = estrdup((const char *) content);
    } else if (XML_STR_EQUAL(node->name, "CertificateSignature")) {
        SOFT_ASSERT_LABEL(server->cert_sig == NULL, error, "Parsing error");
        unsigned char *sig;
        size_t sig_len;
        if (!math_base64_decode((const char *) content, &sig, &sig_len)) {
            LOG(ERROR, "Error decoding BASE64 certificate signature");
            goto error;
        }

        server->cert_sig = sig;
        server->cert_sig_len = sig_len;
    } else {
        LOG(DEVEL, "Unrecognized node: %s", (const char *) node->name);
    }

    bool ret = true;
    goto out;

error:
    ret = false;

out:
    if (content != NULL) {
        xmlFree(content);
    }

    return ret;
}

/**
 * Parse metaserver 'server' node.
 *
 * @param node
 * Node to parse.
 */
static void
parse_metaserver_node (xmlNodePtr node)
{
    HARD_ASSERT(node != NULL);

    server_struct *server = ecalloc(1, sizeof(*server));
    server->port_crypto = -1;
    server->is_meta = true;

    for (xmlNodePtr tmp = node->children; tmp != NULL; tmp = tmp->next) {
        if (!parse_metaserver_data_node(tmp, server)) {
            goto error;
        }
    }

    if (server->hostname == NULL ||
        server->port == 0 ||
        server->name == NULL ||
        server->version == NULL ||
        server->desc == NULL) {
        LOG(ERROR, "Incomplete data from metaserver");
        goto error;
    }

    if (!parse_metaserver_cert(server)) {
        /* Logging already done */
        goto error;
    }

    SDL_LockMutex(server_head_mutex);
    DL_PREPEND(server_head, server);
    server_count++;
    SDL_UnlockMutex(server_head_mutex);
    return;

error:
    metaserver_free(server);
}

/**
 * Callback to display an error message when XML validation fails.
 *
 * @param ctx
 * XML context.
 * @param format
 * Format specifier.
 * @param ...
 * Variable arguments.
 */
static void
parse_metaserver_data_error (void *ctx, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    char formatted[HUGE_BUF * 32];
    vsnprintf(VS(formatted), format, args);
    string_strip_newline(formatted);
    va_end(args);
    LOG(ERROR, "%s", formatted);
}

/**
 * Parse data returned from HTTP metaserver and add it to the list of servers.
 *
 * @param body
 * The data to parse.
 * @param body_size
 * Length of the body.
 */
static void
parse_metaserver_data (const char *body, size_t body_size)
{
    HARD_ASSERT(body != NULL);

    xmlSchemaParserCtxtPtr parser_ctx = NULL;
    xmlSchemaPtr schema = NULL;
    xmlSchemaValidCtxtPtr valid_ctx = NULL;

    xmlDocPtr doc = xmlReadMemory(body, body_size, "noname.xml", NULL, 0);
    if (doc == NULL) {
        LOG(ERROR, "Failed to parse data from metaserver");
        goto out;
    }

    parser_ctx = xmlSchemaNewParserCtxt("schemas/Atrinik-ADS-7.xsd");
    if (parser_ctx == NULL) {
        LOG(ERROR, "Failed to create a schema parser context");
        goto out;
    }

    schema = xmlSchemaParse(parser_ctx);
    if (schema == NULL) {
        LOG(ERROR, "Failed to parse schema file");
        goto out;
    }

    valid_ctx = xmlSchemaNewValidCtxt(schema);
    if (valid_ctx == NULL) {
        LOG(ERROR, "Failed to create a validation context");
        goto out;
    }

    xmlSetStructuredErrorFunc(NULL, NULL);
    xmlSetGenericErrorFunc(NULL, parse_metaserver_data_error);
    xmlThrDefSetStructuredErrorFunc(NULL, NULL);
    xmlThrDefSetGenericErrorFunc(NULL, parse_metaserver_data_error);

    if (xmlSchemaValidateDoc(valid_ctx, doc) != 0) {
        LOG(ERROR, "XML verification failed.");
        goto out;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (root == NULL || !XML_STR_EQUAL(root->name, "Servers")) {
        LOG(ERROR, "No servers element found in metaserver XML");
        goto out;
    }

    xmlNodePtr last = NULL;
    for (xmlNodePtr node = root->children; node != NULL; node = node->next) {
        last = node;
    }

    for (xmlNodePtr node = last; node != NULL; node = node->prev) {
        if (!XML_STR_EQUAL(node->name, "Server")) {
            continue;
        }

        parse_metaserver_node(node);
    }

out:
    if (doc != NULL) {
            xmlFreeDoc(doc);
    }

    if (parser_ctx != NULL) {
        xmlSchemaFreeParserCtxt(parser_ctx);
    }

    if (schema != NULL) {
        xmlSchemaFree(schema);
    }

    if (valid_ctx != NULL) {
        xmlSchemaFreeValidCtxt(valid_ctx);
    }
}

/**
 * Verify resolved address of a server against the server's metaserver
 * certificate.
 *
 * @param server
 * Server to verify.
 * @param host
 * Host address.
 * @return
 * True if the resolved address is equal to the one in the certificate,
 * false otherwise.
 */
bool
metaserver_cert_verify_host (server_struct *server, const char *host)
{
    HARD_ASSERT(server != NULL);
    HARD_ASSERT(host != NULL);

    /* No certificate, nothing to verify. */
    if (server->cert_info == NULL) {
        return true;
    }

    struct sockaddr_storage addr;
    SOFT_ASSERT_RC(socket_host2addr(host, &addr), false,
                   "Failed to convert host to IP address");

    switch (addr.ss_family) {
    case AF_INET:
        if (strcmp(host, server->cert_info->ipv4_address) != 0) {
            LOG(ERROR, "!!! Certificate IPv4 address error: %s != %s !!!",
                host, server->cert_info->ipv4_address);
            return false;
        }

        break;

    case AF_INET6:
        if (strcmp(host, server->cert_info->ipv6_address) != 0) {
            LOG(ERROR, "!!! Certificate IPv6 address error: %s != %s !!!",
                host, server->cert_info->ipv6_address);
            return false;
        }

        break;

    default:
        LOG(ERROR, "!!! Unknown address family %u !!!", addr.ss_family);
        return false;
    }

    return true;
}

/**
 * Get server from the servers list by its ID.
 * @param num
 * ID of the server to find.
 * @return
 * The server if found, NULL otherwise.
 */
server_struct *server_get_id(size_t num)
{
    server_struct *node;
    size_t i;

    SDL_LockMutex(server_head_mutex);

    for (node = server_head, i = 0; node; node = node->next, i++) {
        if (i == num) {
            break;
        }
    }

    SDL_UnlockMutex(server_head_mutex);
    return node;
}

/**
 * Get number of the servers in the list.
 * @return
 * The number.
 */
size_t server_get_count(void)
{
    size_t count;

    SDL_LockMutex(server_head_mutex);
    count = server_count;
    SDL_UnlockMutex(server_head_mutex);
    return count;
}

/**
 * Check if we're connecting to the metaserver.
 * @param val
 * If not -1, set the metaserver connecting value to this.
 * @return
 * 1 if we're connecting to the metaserver, 0 otherwise.
 */
int ms_connecting(int val)
{
    int connecting;

    SDL_LockMutex(metaserver_connecting_mutex);
    connecting = metaserver_connecting;

    /* More useful to return the old value than the one we're setting. */
    if (val != -1) {
        metaserver_connecting = val;
    }

    SDL_UnlockMutex(metaserver_connecting_mutex);
    return connecting;
}

/**
 * Clear all data in the linked list of servers reported by metaserver.
 */
void metaserver_clear_data(void)
{
    server_struct *node, *tmp;

    SDL_LockMutex(server_head_mutex);

    DL_FOREACH_SAFE(server_head, node, tmp)
    {
        DL_DELETE(server_head, node);
        metaserver_free(node);
    }

    server_count = 0;
    SDL_UnlockMutex(server_head_mutex);
}

/**
 * Add a server entry to the linked list of available servers reported by
 * metaserver.
 *
 * @param hostname
 * Server's hostname.
 * @param port
 * Server port.
 * @param port_crypto
 * Secure port to use.
 * @param name
 * Server's name.
 * @param version
 * Server version.
 * @param desc
 * Description of the server.
 */
server_struct *
metaserver_add (const char *hostname,
                int         port,
                int         port_crypto,
                const char *name,
                const char *version,
                const char *desc)
{
    server_struct *node = ecalloc(1, sizeof(*node));
    node->player = -1;
    node->port = port;
    node->port_crypto = port_crypto;
    node->hostname = estrdup(hostname);
    node->name = estrdup(name);
    node->version = estrdup(version);
    node->desc = estrdup(desc);

    SDL_LockMutex(server_head_mutex);
    DL_PREPEND(server_head, node);
    server_count++;
    SDL_UnlockMutex(server_head_mutex);

    return node;
}

/**
 * Threaded function to connect to metaserver.
 *
 * Goes through the list of metaservers and calls metaserver_connect()
 * until it gets a return value of 1. If if goes through all the
 * metaservers and still fails, show an info to the user.
 * @param dummy
 * Unused.
 * @return
 * Always returns 0.
 */
int metaserver_thread(void *dummy)
{
    /* Go through all the metaservers in the list */
    for (size_t i = clioption_settings.metaservers_num; i > 0; i--) {
        /* Send a GET request to the metaserver */
        curl_request_t *request =
            curl_request_create(clioption_settings.metaservers[i - 1],
                                CURL_PKEY_TRUST_ULTIMATE);
        curl_request_do_get(request);

        /* If the request succeeded, parse the metaserver data and break out. */
        int http_code = curl_request_get_http_code(request);
        size_t body_size;
        char *body = curl_request_get_body(request, &body_size);
        if (http_code == 200 && body != NULL) {
            parse_metaserver_data(body, body_size);
            curl_request_free(request);
            break;
        }

        curl_request_free(request);
    }

    SDL_LockMutex(metaserver_connecting_mutex);
    /* We're not connecting anymore. */
    metaserver_connecting = 0;
    SDL_UnlockMutex(metaserver_connecting_mutex);
    return 0;
}

/**
 * Connect to metaserver and get the available servers.
 *
 * Works in a thread using SDL_CreateThread().
 */
void metaserver_get_servers(void)
{
    SDL_Thread *thread;

    if (!enabled) {
        return;
    }

    SDL_LockMutex(metaserver_connecting_mutex);
    metaserver_connecting = 1;
    SDL_UnlockMutex(metaserver_connecting_mutex);

    thread = SDL_CreateThread(metaserver_thread, NULL);

    if (!thread) {
        LOG(ERROR, "Thread creation failed.");
        exit(1);
    }
}
