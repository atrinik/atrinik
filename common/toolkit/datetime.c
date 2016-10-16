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
 * Date and time API.
 *
 * @author Alex Tokar
 */

#include "datetime.h"

TOOLKIT_API();

TOOLKIT_INIT_FUNC(datetime)
{
}
TOOLKIT_INIT_FUNC_FINISH

TOOLKIT_DEINIT_FUNC(datetime)
{
}
TOOLKIT_DEINIT_FUNC_FINISH

time_t
datetime_getutc (void)
{
    TOOLKIT_PROTECT();

    time_t t;
    time(&t);
    struct tm *tm = gmtime(&t);

    return mktime(tm);
}

time_t
datetime_utctolocal (time_t t)
{
    TOOLKIT_PROTECT();
    return t - (datetime_getutc() - time(NULL));
}
