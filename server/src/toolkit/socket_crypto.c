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
#include <socket.h>
#include <socket_crypto.h>
#include <clioptions.h>

TOOLKIT_API(DEPENDS(clioptions), DEPENDS(logger), DEPENDS(memory));

/**
 * Structure that contains all the necessary information for the  crypto
 * extension of sockets.
 */
struct socket_crypto {
    int foo;
};

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
/** Whether socket cryptography is enabled. */
static bool crypto_enabled = false;

/**
 * Frees the crypto curves.
 */
static void
socket_crypto_curves_free (void)
{
    crypto_curve_t *curve, *tmp;
    LL_FOREACH_SAFE(crypto_curves, curve, tmp) {
        efree(curve->name);
        efree(curve);
    }

    crypto_curves = NULL;
}

/**
 * Description of the --crypto command.
 */
static const char *clioptions_option_crypto_desc =
"Enables/disables socket cryptography.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_crypto (const char *arg,
                          char      **errmsg)
{
    if (KEYWORD_IS_TRUE(arg)) {
        crypto_enabled = true;
    } else if (KEYWORD_IS_FALSE(arg)) {
        crypto_enabled = false;
    } else {
        *errmsg = estrdup("Invalid value supplied");
        return false;
    }

    return true;
}

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
    socket_crypto_curves_free();

    char name[MAX_BUF];
    size_t pos = 0;
    while (string_get_word(arg, &pos, ',', VS(name), 0) != NULL) {
        string_whitespace_trim(name);
        int nid = OBJ_txt2nid(name);
        if (nid == NID_undef) {
            continue;
        }

        crypto_curve_t *curve = ecalloc(1, sizeof(*curve));
        curve->name = estrdup(name);
        curve->nid = nid;
        LL_APPEND(crypto_curves, curve);
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

    return true;
}

/**
 * Initialize the socket crypto API.
 */
TOOLKIT_INIT_FUNC(socket_crypto)
{
    clioption_t *cli;
    CLIOPTIONS_CREATE_ARGUMENT(cli, crypto, "Enable/disable socket crypto");
    CLIOPTIONS_CREATE_ARGUMENT(cli, crypto_curves, "Select crypto curves");
}
TOOLKIT_INIT_FUNC_FINISH

/**
 * Deinitialize the socket crypto API.
 */
TOOLKIT_DEINIT_FUNC(socket_crypto)
{
    socket_crypto_curves_free();
}
TOOLKIT_DEINIT_FUNC_FINISH

/**
 * Check whether the socket crypto sub-system is enabled.
 *
 * @return
 * Whether the socket crypto sub-system is enabled.
 */
bool
socket_crypto_enabled (void)
{
    return crypto_enabled;
}

/**
 * Check whether the socket crypto sub-system is aware of any supported
 * crypto curves.
 *
 * @return
 * Whether any crypto curves are available.
 */
bool
socket_crypto_has_curves (void)
{
    return crypto_curves != NULL;
}

/**
 * Figure out whether the specified curve is supported.
 *
 * @param name
 * Name of the curve to check.
 * @param[out] nid
 * Will contain NID of the curve on success. Can be NULL.
 * @return
 * True if the specified curve is supported, false otherwise.
 */
bool
socket_crypto_curve_supported (const char *name, int *nid)
{
    HARD_ASSERT(name != NULL);

    crypto_curve_t *curve;
    LL_FOREACH(crypto_curves, curve) {
        if (strcmp(curve->name, name) == 0) {
            if (nid != NULL) {
                *nid = curve->nid;
            }

            return true;
        }
    }

    return false;
}

/**
 * Creates crypto on the specified socket.
 *
 * @param sc
 * Socket.
 * @param nid
 * NID of the crypto curve to set up. Can be obtained from
 * socket_crypto_curve_supported().
 * @return
 * True on success, false on failure.
 */
bool
socket_crypto_create (socket_t *sc, int nid)
{
    HARD_ASSERT(sc != NULL);
    socket_crypto_t *crypto = ecalloc(1, sizeof(*crypto));
    socket_set_crypto(sc, crypto);
    return true;
}

/**
 * Destroys the specified socket crypto.
 *
 * @param crypto
 * Socket crypto to destroy.
 */
void
socket_crypto_destroy (socket_crypto_t *crypto)
{
    HARD_ASSERT(crypto != NULL);
    efree(crypto);
}
