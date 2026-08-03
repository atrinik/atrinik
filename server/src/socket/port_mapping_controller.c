/**
 * @file
 *
 * Backend-neutral UDP port-mapping controller.
 */

#include <port_mapping.h>

bool
socket_port_mapping_controller_open (
    socket_port_mapping_controller_t    *controller,
    const socket_port_mapping_backend_t *backends,
    size_t                               backend_count,
    uint16_t                             port,
    char                                *host,
    size_t                               host_size,
    uint16_t                            *external_port)
{
    HARD_ASSERT(controller != NULL);
    HARD_ASSERT(backends != NULL || backend_count == 0);
    HARD_ASSERT(host != NULL);
    HARD_ASSERT(external_port != NULL);

    socket_port_mapping_controller_close(controller);
    for (size_t i = 0; i < backend_count; i++) {
        const socket_port_mapping_backend_t *backend = &backends[i];
        HARD_ASSERT(backend->name != NULL);
        HARD_ASSERT(backend->open != NULL);

        if (backend->open(backend->data,
                          port,
                          host,
                          host_size,
                          external_port)) {
            controller->active = backend;
            return true;
        }
        if (backend->close != NULL) {
            backend->close(backend->data);
        }
    }

    return false;
}

void
socket_port_mapping_controller_process (
    socket_port_mapping_controller_t *controller)
{
    HARD_ASSERT(controller != NULL);

    if (controller->active != NULL &&
        controller->active->process != NULL) {
        controller->active->process(controller->active->data);
    }
}

void
socket_port_mapping_controller_close (
    socket_port_mapping_controller_t *controller)
{
    HARD_ASSERT(controller != NULL);

    if (controller->active != NULL && controller->active->close != NULL) {
        controller->active->close(controller->active->data);
    }
    controller->active = NULL;
}

const char *
socket_port_mapping_controller_name (
    const socket_port_mapping_controller_t *controller)
{
    HARD_ASSERT(controller != NULL);

    return controller->active != NULL ? controller->active->name : NULL;
}
