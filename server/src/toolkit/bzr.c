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
 * Bzr related API.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Name of the API. */
#define API_NAME bzr

/**
 * If 1, the API has been initialized. */
static uint8_t did_init = 0;

/**
 * Where to search for .bzr directory. */
static const char *const branch_paths[] = {
    ".", ".."
};

/**
 * Revision number of the branch, if any. */
static int branch_revision;

/**
 * Initialize the bzr API.
 * @internal */
void toolkit_bzr_init(void)
{

    TOOLKIT_INIT_FUNC_START(bzr)
    {
        toolkit_import(path);

        branch_revision = -1;
    }
    TOOLKIT_INIT_FUNC_END()
}

/**
 * Deinitialize the bzr API.
 * @internal */
void toolkit_bzr_deinit(void)
{

    TOOLKIT_DEINIT_FUNC_START(bzr)
    {
    }
    TOOLKIT_DEINIT_FUNC_END()
}

/**
 * Acquire the revision number of the branch the executable is located
 * in (if any). */
int bzr_get_revision(void)
{
    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    if (branch_revision == -1) {
        size_t i;
        char buf[MAX_BUF], *contents, *cps[2];

        branch_revision = 0;

        /* Try to find branch revision. */
        for (i = 0; i < arraysize(branch_paths) && branch_revision == 0; i++) {
            snprintf(buf, sizeof(buf), "%s/.bzr/branch/last-revision", branch_paths[i]);
            contents = path_file_contents(buf);

            if (!contents) {
                continue;
            }

            if (string_split(contents, cps, arraysize(cps), ' ') == 2) {
                branch_revision = atoi(cps[0]);
            }

            efree(contents);
        }
    }

    return branch_revision;
}
