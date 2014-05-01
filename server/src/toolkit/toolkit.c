/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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
 * Toolkit system.
 *
 * @author Alex Tokar */

#include <global.h>

static toolkit_func *deinit_funcs = NULL;
static size_t deinit_funcs_num = 0;

void toolkit_import_register(toolkit_func func)
{
    deinit_funcs = erealloc(deinit_funcs, sizeof(*deinit_funcs) * (deinit_funcs_num + 1));
    deinit_funcs[deinit_funcs_num] = func;
    deinit_funcs_num++;
}

int toolkit_check_imported(toolkit_func func)
{
    size_t i;

    for (i = 0; i < deinit_funcs_num; i++) {
        if (deinit_funcs[i] == func) {
            return 1;
        }
    }

    return 0;
}

void toolkit_deinit(void)
{
    size_t i;

    for (i = deinit_funcs_num; i > 0; i--) {
        deinit_funcs[i - 1]();
    }

    if (deinit_funcs) {
        free(deinit_funcs);
        deinit_funcs = NULL;
    }

    deinit_funcs_num = 0;
}
