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
#include <socket_security.h>
#include <clioptions.h>

TOOLKIT_API(DEPENDS(clioptions), DEPENDS(logger), DEPENDS(memory));

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
    return true;
}

/**
 * Initialize the socket security API.
 */
TOOLKIT_INIT_FUNC(socket_security)
{
    clioption_t *cli;
    CLIOPTIONS_CREATE_ARGUMENT(cli, crypto_curves, "Select crypto curves");
#if 0
    if (!true) { // TODO: check if security is enabled
        return;
    }
#endif

    const char *security_curves_str = "prime256v1,curve25519"; // TODO: options

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

    if (security_curves == NULL) {
        LOG(ERROR, "Your system does not support any crypto curves. Aborting.");
        exit(1);
    }

    StringBuffer *sb = stringbuffer_new();
    security_curve_t *curve;
    LL_FOREACH(security_curves, curve) {
        stringbuffer_append_printf(sb, "%s%s",
                                   stringbuffer_length(sb) != 0 ? ", " : "",
                                   curve->name);
    }

    char *cp = stringbuffer_finish(sb);
    LOG(INFO, "Determined supported crypto curves: %s", cp);
    efree(cp);
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
