/**
 * Direct-connect metaserver directory parser (protocol 2).
 */

#include <global.h>
#include "metaserver_private.h"
#include <toolkit/string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <curl/curl.h>

#define XML_STR_EQUAL(s1, s2) \
    xmlStrEqual((const xmlChar *) s1, (const xmlChar *) s2)

#ifdef __MINGW32__
#   define xmlFree free
#endif

static bool
metaserver_direct_hex64 (const char *value)
{
    if (value == NULL || strlen(value) != 64) {
        return false;
    }
    for (const unsigned char *cp = (const unsigned char *) value;
         *cp != '\0';
         cp++) {
        if (!isxdigit(*cp)) {
            return false;
        }
    }
    return true;
}

static bool
parse_direct_server_field (xmlNodePtr node, server_struct *server)
{
    if (node->type != XML_ELEMENT_NODE) {
        return true;
    }

    xmlChar *content = xmlNodeGetContent(node);
    if (content == NULL || *content == '\0') {
        xmlFree(content);
        return false;
    }

    const char *value = (const char *) content;
    bool ok = true;
    if (XML_STR_EQUAL(node->name, "Id")) {
        if (server->server_id != NULL) {
            ok = false;
        } else {
            server->server_id = estrdup(value);
        }
    } else if (XML_STR_EQUAL(node->name, "Address") ||
               XML_STR_EQUAL(node->name, "Hostname")) {
        if (server->hostname != NULL) {
            ok = false;
        } else {
            server->hostname = estrdup(value);
        }
    } else if (XML_STR_EQUAL(node->name, "Port")) {
        uint64_t port;
        if (!string_parse_uint64(value, 10, 1, UINT16_MAX, &port) ||
                server->port != 0) {
            ok = false;
        } else {
            server->port = (int) port;
        }
    } else if (XML_STR_EQUAL(node->name, "Name")) {
        if (server->name != NULL) {
            ok = false;
        } else {
            server->name = estrdup(value);
        }
    } else if (XML_STR_EQUAL(node->name, "PlayersCount")) {
        uint64_t players;
        if (!string_parse_uint64(value, 10, 0, INT_MAX, &players)) {
            ok = false;
        } else {
            server->player = (int) players;
        }
    } else if (XML_STR_EQUAL(node->name, "Version")) {
        if (server->version != NULL) {
            ok = false;
        } else {
            server->version = estrdup(value);
        }
    } else if (XML_STR_EQUAL(node->name, "TextComment")) {
        if (server->desc != NULL) {
            ok = false;
        } else {
            server->desc = estrdup(value);
        }
    } else if (XML_STR_EQUAL(node->name, "ConnectivityMode")) {
        ok = strcmp(value, "direct_only") == 0 ||
             strcmp(value, "direct_preferred") == 0;
    } else if (XML_STR_EQUAL(node->name, "CertificateSha256")) {
        if (server->quic_certificate_sha256 != NULL) {
            ok = false;
        } else {
            server->quic_certificate_sha256 = estrdup(value);
        }
    } else if (XML_STR_EQUAL(node->name, "PasswordRequired")) {
        if (strcmp(value, "true") == 0) {
            server->password_required = true;
        } else if (strcmp(value, "false") != 0) {
            ok = false;
        }
    }

    xmlFree(content);
    return ok;
}

static void
parse_direct_server (xmlNodePtr node, const char *origin)
{
    server_struct *server = ecalloc(1, sizeof(*server));
    server->port_crypto = -1;
    server->is_meta = true;
    server->direct = true;
    server->rendezvous_origin = estrdup(origin);

    for (xmlNodePtr field = node->children;
         field != NULL;
         field = field->next) {
        if (!parse_direct_server_field(field, server)) {
            goto error;
        }
    }

    if (!metaserver_direct_hex64(server->server_id) ||
        !metaserver_direct_hex64(server->quic_certificate_sha256) ||
        server->hostname == NULL ||
        server->port <= 0 ||
        server->port > UINT16_MAX ||
        server->name == NULL ||
        server->version == NULL ||
        server->desc == NULL) {
        LOG(ERROR, "Incomplete or invalid direct server entry");
        goto error;
    }

    string_tolower(server->server_id);
    string_tolower(server->quic_certificate_sha256);
    metaserver_server_add(server);
    return;

error:
    metaserver_server_free(server);
}

bool
metaserver_direct_parse (const char *body,
                         size_t      body_size,
                         const char *origin)
{
    xmlDocPtr doc = xmlReadMemory(body,
                                  body_size,
                                  "direct-servers.xml",
                                  NULL,
                                  XML_PARSE_NONET);
    if (doc == NULL) {
        LOG(ERROR, "Failed to parse direct metaserver directory");
        return false;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    xmlChar *protocol = root != NULL
        ? xmlGetProp(root, (const xmlChar *) "protocol")
        : NULL;
    bool valid = root != NULL &&
                 XML_STR_EQUAL(root->name, "Servers") &&
                 protocol != NULL &&
                 XML_STR_EQUAL(protocol, "2");
    xmlFree(protocol);
    if (!valid) {
        LOG(ERROR, "Invalid direct metaserver directory root");
        xmlFreeDoc(doc);
        return false;
    }

    for (xmlNodePtr node = root->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE &&
            XML_STR_EQUAL(node->name, "Server")) {
            parse_direct_server(node, origin);
        }
    }

    xmlFreeDoc(doc);
    return true;
}

void
metaserver_direct_url (const char *legacy_url, char *url, size_t url_size)
{
    CURLU *parsed = curl_url();
    char *rendered = NULL;
    bool ok = parsed != NULL &&
              curl_url_set(parsed, CURLUPART_URL, legacy_url, 0) == CURLUE_OK &&
              curl_url_set(parsed, CURLUPART_PATH, "/v2/servers", 0) == CURLUE_OK &&
              curl_url_set(parsed, CURLUPART_QUERY, NULL, 0) == CURLUE_OK &&
              curl_url_set(parsed, CURLUPART_FRAGMENT, NULL, 0) == CURLUE_OK &&
              curl_url_get(parsed, CURLUPART_URL, &rendered, 0) == CURLUE_OK;
    snprintf(url, url_size, "%s", ok ? rendered : "");
    curl_free(rendered);
    if (parsed != NULL) {
        curl_url_cleanup(parsed);
    }
}
