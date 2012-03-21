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
 * Date and time API.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Name of the API. */
#define API_NAME datetime

/**
 * Initialize the datetime API.
 * @internal */
void toolkit_datetime_init(void)
{
	TOOLKIT_INIT_FUNC_START(datetime)
	{
	}
	TOOLKIT_INIT_FUNC_END()
}

/**
 * Deinitialize the datetime API.
 * @internal */
void toolkit_datetime_deinit(void)
{
}

/**
 * Get the current UTC time as UNIX timestamp.
 * @return UTC time as UNIX timestamp. */
time_t datetime_getutc(void)
{
	time_t t;
	struct tm *tm;

	TOOLKIT_FUNC_PROTECTOR(API_NAME);

	time(&t);
	tm = gmtime(&t);

	return mktime(tm);
}

/**
 * Converts UTC time to local time.
 * @param t UTC time.
 * @return Converted local time. */
time_t datetime_utctolocal(time_t t)
{
	TOOLKIT_FUNC_PROTECTOR(API_NAME);
	return t - (datetime_getutc() - time(NULL));
}
