/**
 * @file
 *
 * Backend-neutral UDP port-mapping controller.
 */

#ifndef PORT_MAPPING_H
#define PORT_MAPPING_H

#include <toolkit/toolkit.h>

/** A PCP/NAT-PMP, UPnP, or test port-mapping backend. */
typedef struct socket_port_mapping_backend {
    const char *name;
    void *data;
    bool (*open)(void *data,
                 uint16_t port,
                 char *host,
                 size_t host_size,
                 uint16_t *external_port);
    void (*process)(void *data);
    void (*close)(void *data);
} socket_port_mapping_backend_t;

/**
 * Tracks the one backend that owns the active router mapping.
 *
 * Initialize this structure to zero before first use. A backend array selected
 * by open must remain valid until close is called.
 */
typedef struct socket_port_mapping_controller {
    const socket_port_mapping_backend_t *active;
} socket_port_mapping_controller_t;

bool
socket_port_mapping_controller_open(
    socket_port_mapping_controller_t     *controller,
    const socket_port_mapping_backend_t  *backends,
    size_t                                backend_count,
    uint16_t                              port,
    char                                 *host,
    size_t                                host_size,
    uint16_t                             *external_port);
void
socket_port_mapping_controller_process(
    socket_port_mapping_controller_t *controller);
void
socket_port_mapping_controller_close(
    socket_port_mapping_controller_t *controller);
const char *
socket_port_mapping_controller_name(
    const socket_port_mapping_controller_t *controller);

#endif
