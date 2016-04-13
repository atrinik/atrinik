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
 * Socket security implementation.
 *
 * @author
 * Alex Tokar
 */

#include <global.h>
#include <toolkit_string.h>
#include <packet.h>
#include <security.h>
#include <player.h>
#include <object.h>

TOOLKIT_API(DEPENDS(logger), DEPENDS(memory));

/**
 * Structure representing a single supported security curve.
 */
typedef struct security_curve {
    struct security_curve *next; ///< Next security curve.
    char *name; ///< Curve name.
    int nid; ///< OpenSSL NID.
} security_curve_t;

/** All the supported security curves. */
static security_curve_t *security_curves = NULL;

/**
 * Initialize the socket security API.
 */
TOOLKIT_INIT_FUNC(socket_security)
{
#if 0
    if (!true) { // TODO: check if security is enabled
        return;
    }
#endif

    const char *security_curves_str = "prime256v1"; // TODO: server options

    char name[MAX_BUF];
    size_t pos = 0;
    while (string_get_word(security_curves_str,
                           &pos,
                           ',',
                           VS(name),
                           0) != NULL) {
        int nid = OBJ_txt2nid(name);
        if (nid == NID_undef) {
            continue;
        }

        security_curve_t *curve = ecalloc(1, sizeof(*curve));
        curve->name = estrdup(name);
        curve->nid = nid;
        LL_APPEND(security_curves, curve);
    }
}
TOOLKIT_INIT_FUNC_FINISH

/**
 * Deinitialize the socket security API.
 */
TOOLKIT_DEINIT_FUNC(socket_security)
{
    security_curve_t *curve, *tmp;
    LL_FOREACH_SAFE(security_curves, curve, tmp) {
        efree(curve->name);
        efree(curve);
    }

    security_curves = NULL;
}
TOOLKIT_DEINIT_FUNC_FINISH

/**
 * Handler for the security hello sub-command.
 *
 * @copydoc socket_command_func
 */
static void
socket_security_hello (socket_struct *ns,
                       player        *pl,
                       uint8_t       *data,
                       size_t         len,
                       size_t         pos)
{
    HARD_ASSERT(ns != NULL);
    HARD_ASSERT(pl == NULL);
    HARD_ASSERT(data != NULL);

    char curve[MAX_BUF];
    while (packet_to_string(data, len, &pos, VS(curve)) != NULL) {

    }
}

/**
 * Handler for the security socket command.
 *
 * @copydoc socket_command_func
 */
void
socket_command_security (socket_struct *ns,
                         player        *pl,
                         uint8_t       *data,
                         size_t         len,
                         size_t         pos)
{
    HARD_ASSERT(ns != NULL);
    HARD_ASSERT(data != NULL);
    /* TODO: Check if received before on the socket */

    /* Don't let clients initiate a handshake when they're already logged in.
     * In practice, this should never happen. */
    if (pl != NULL) {
        LOG(PACKET, "Received while logged in: %s", pl->ob->name);
        return;
    }

    uint8_t type = packet_to_uint8(data, len, &pos);
    switch (type) {
    case CMD_SECURITY_HELLO:
        socket_security_hello(ns, pl, data, len, pos);
        break;

    default:
        LOG(PACKET, "Received unknown security sub-command: %" PRIu8, type);
        break;
    }
}
