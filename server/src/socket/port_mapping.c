/**
 * @file
 *
 * Automatic UDP port mapping using PCP/NAT-PMP with a UPnP IGD fallback.
 */

#include <global.h>
#include <server.h>

#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>
#include <pcpnatpmp.h>

#define PORT_MAPPING_LIFETIME 7200U

static pcp_ctx_t *mapping_pcp_context;
static pcp_flow_t *mapping_pcp_flow;
static bool mapping_upnp_active;
static struct UPNPUrls mapping_upnp_urls;
static struct IGDdatas mapping_upnp_data;
static char mapping_upnp_port[6];
static char mapping_upnp_lan_address[65];
static time_t mapping_upnp_renew_at;

static bool
socket_port_mapping_pcp (uint16_t port,
                         char    *host,
                         size_t   host_size,
                         uint16_t *external_port)
{
    struct sockaddr_in source;
    memset(&source, 0, sizeof(source));
    source.sin_family = AF_INET;
    source.sin_port = htons(port);

    mapping_pcp_context = pcp_init(ENABLE_AUTODISCOVERY, NULL);
    if (mapping_pcp_context == NULL) {
        return false;
    }
    mapping_pcp_flow = pcp_new_flow(mapping_pcp_context,
                                    (struct sockaddr *) &source,
                                    NULL,
                                    NULL,
                                    IPPROTO_UDP,
                                    PORT_MAPPING_LIFETIME,
                                    NULL);
    pcp_fstate_e state = mapping_pcp_flow != NULL
        ? pcp_wait(mapping_pcp_flow, 5000, 1) : pcp_state_failed;
    if (state != pcp_state_succeeded &&
        state != pcp_state_partial_result) {
        pcp_terminate(mapping_pcp_context, 0);
        mapping_pcp_context = NULL;
        mapping_pcp_flow = NULL;
        return false;
    }

    size_t info_count = 0;
    pcp_flow_info_t *info = pcp_flow_get_info(mapping_pcp_flow, &info_count);
    bool ok = false;
    for (size_t i = 0; i < info_count; i++) {
        if (info[i].result != pcp_state_succeeded) {
            continue;
        }
        const unsigned char *bytes = info[i].ext_ip.s6_addr;
        const void *address = &info[i].ext_ip;
        int family = AF_INET6;
        if (memcmp(bytes, "\0\0\0\0\0\0\0\0\0\0\xff\xff", 12) == 0) {
            family = AF_INET;
            address = bytes + 12;
        }
        if (inet_ntop(family, address, host, host_size) != NULL) {
            *external_port = ntohs(info[i].ext_port);
            ok = *external_port != 0;
            if (ok) {
                break;
            }
        }
    }
    free(info);
    if (!ok) {
        pcp_terminate(mapping_pcp_context, 0);
        mapping_pcp_context = NULL;
        mapping_pcp_flow = NULL;
    }
    return ok;
}

static bool
socket_port_mapping_upnp (uint16_t port,
                          char    *host,
                          size_t   host_size,
                          uint16_t *external_port)
{
    int error = 0;
    struct UPNPDev *devices = upnpDiscover(2000,
                                           NULL,
                                           NULL,
                                           UPNP_LOCAL_PORT_ANY,
                                           0,
                                           2,
                                           &error);
    if (devices == NULL) {
        return false;
    }

    char lan_address[65];
#if MINIUPNPC_API_VERSION >= 21
    char wan_address[65];
    int status = UPNP_GetValidIGD(devices,
                                  &mapping_upnp_urls,
                                  &mapping_upnp_data,
                                  VS(lan_address),
                                  VS(wan_address));
#else
    int status = UPNP_GetValidIGD(devices,
                                  &mapping_upnp_urls,
                                  &mapping_upnp_data,
                                  VS(lan_address));
#endif
    freeUPNPDevlist(devices);
    if (status == 0) {
        return false;
    }

    snprintf(VS(mapping_upnp_port), "%" PRIu16, port);
    char external_address[65];
    int address_result = UPNP_GetExternalIPAddress(
        mapping_upnp_urls.controlURL,
        mapping_upnp_data.first.servicetype,
        external_address);
    int mapping_result = UPNP_AddPortMapping(
        mapping_upnp_urls.controlURL,
        mapping_upnp_data.first.servicetype,
        mapping_upnp_port,
        mapping_upnp_port,
        lan_address,
        "Atrinik QUIC server",
        "UDP",
        NULL,
        "7200");
    struct in_addr parsed_address;
    if (address_result != UPNPCOMMAND_SUCCESS ||
        mapping_result != UPNPCOMMAND_SUCCESS ||
        inet_pton(AF_INET, external_address, &parsed_address) != 1) {
        LOG(DEBUG,
            "UPnP UDP mapping failed: %s",
            strupnperror(mapping_result));
        FreeUPNPUrls(&mapping_upnp_urls);
        memset(&mapping_upnp_urls, 0, sizeof(mapping_upnp_urls));
        return false;
    }

    snprintf(host, host_size, "%s", external_address);
    *external_port = port;
    snprintf(VS(mapping_upnp_lan_address), "%s", lan_address);
    mapping_upnp_renew_at = time(NULL) + PORT_MAPPING_LIFETIME / 2;
    mapping_upnp_active = true;
    return true;
}

bool
socket_port_mapping_init (uint16_t port,
                          char    *host,
                          size_t   host_size,
                          uint16_t *external_port)
{
    if (strcmp(settings.port_mapping, "off") == 0) {
        return false;
    }
    if (socket_port_mapping_pcp(port, host, host_size, external_port)) {
        LOG(INFO,
            "Created PCP/NAT-PMP UDP mapping %s:%" PRIu16,
            host,
            *external_port);
        return true;
    }
    if (socket_port_mapping_upnp(port, host, host_size, external_port)) {
        LOG(INFO,
            "Created UPnP UDP mapping %s:%" PRIu16,
            host,
            *external_port);
        return true;
    }

    LOG(INFO, "No PCP, NAT-PMP, or UPnP IGD mapping was available");
    return false;
}

void
socket_port_mapping_process (void)
{
    if (mapping_pcp_context != NULL) {
        struct timeval timeout = {0, 0};
        pcp_pulse(mapping_pcp_context, &timeout);
    }
    if (mapping_upnp_active && time(NULL) >= mapping_upnp_renew_at) {
        int result = UPNP_AddPortMapping(
            mapping_upnp_urls.controlURL,
            mapping_upnp_data.first.servicetype,
            mapping_upnp_port,
            mapping_upnp_port,
            mapping_upnp_lan_address,
            "Atrinik QUIC server",
            "UDP",
            NULL,
            "7200");
        if (result != UPNPCOMMAND_SUCCESS) {
            LOG(ERROR,
                "Could not renew UPnP UDP mapping: %s",
                strupnperror(result));
        }
        mapping_upnp_renew_at = time(NULL) + PORT_MAPPING_LIFETIME / 2;
    }
}

void
socket_port_mapping_deinit (void)
{
    if (mapping_pcp_context != NULL) {
        pcp_terminate(mapping_pcp_context, 1);
        mapping_pcp_context = NULL;
        mapping_pcp_flow = NULL;
    }
    if (mapping_upnp_active) {
        UPNP_DeletePortMapping(mapping_upnp_urls.controlURL,
                               mapping_upnp_data.first.servicetype,
                               mapping_upnp_port,
                               "UDP",
                               NULL);
        FreeUPNPUrls(&mapping_upnp_urls);
        memset(&mapping_upnp_urls, 0, sizeof(mapping_upnp_urls));
        mapping_upnp_lan_address[0] = '\0';
        mapping_upnp_renew_at = 0;
        mapping_upnp_active = false;
    }
}
