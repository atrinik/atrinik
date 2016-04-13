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
#include <socket_crypto.h>
#include <clioptions.h>

TOOLKIT_API(DEPENDS(clioptions), DEPENDS(logger), DEPENDS(memory));

/**
 * Structure representing a single supported crypto curve.
 */
typedef struct crypto_curve {
    struct crypto_curve *next; ///< Next crypto curve.
    char *name; ///< Curve name.
    int nid; ///< OpenSSL NID.
} crypto_curve_t;

/** All the supported crypto curves. */
static crypto_curve_t *crypto_curves = NULL;

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
 * Initialize the socket crypto API.
 */
TOOLKIT_INIT_FUNC(socket_crypto)
{
    clioption_t *cli;
    CLIOPTIONS_CREATE_ARGUMENT(cli, crypto_curves, "Select crypto curves");
#if 0
    if (!true) { // TODO: check if crypto is enabled
        return;
    }
#endif

    const char *crypto_curves_str = "prime256v1,curve25519"; // TODO: options

    char name[MAX_BUF];
    size_t pos = 0;
    while (string_get_word(crypto_curves_str,
                           &pos,
                           ',',
                           VS(name),
                           0) != NULL) {
        int nid = OBJ_txt2nid(name);
        if (nid == NID_undef) {
            continue;
        }

        crypto_curve_t *curve = ecalloc(1, sizeof(*curve));
        curve->name = estrdup(name);
        curve->nid = nid;
        LL_APPEND(crypto_curves, curve);
    }

    if (crypto_curves == NULL) {
        LOG(ERROR, "Your system does not support any crypto curves. Aborting.");
        exit(1);
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
}
TOOLKIT_INIT_FUNC_FINISH

/**
 * Deinitialize the socket crypto API.
 */
TOOLKIT_DEINIT_FUNC(socket_crypto)
{
    crypto_curve_t *curve, *tmp;
    LL_FOREACH_SAFE(crypto_curves, curve, tmp) {
        efree(curve->name);
        efree(curve);
    }

    crypto_curves = NULL;
}
TOOLKIT_DEINIT_FUNC_FINISH
