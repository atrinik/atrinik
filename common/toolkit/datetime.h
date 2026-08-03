/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
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
 * Date and time API header file.
 *
 * @author Zoey Rose
 */

#ifndef TOOLKIT_DATETIME_H
#define TOOLKIT_DATETIME_H

#include "toolkit.h"

/* Prototypes */

TOOLKIT_FUNCS_DECLARE(datetime);

/**
 * Get the current UTC time as UNIX timestamp.
 *
 * @return
 * UTC time as UNIX timestamp.
 */
extern time_t
datetime_getutc(void);

/**
 * Converts UTC time to local time.
 *
 * @param t
 * UTC time.
 * @return
 * Converted local time.
 */
extern time_t
datetime_utctolocal(time_t t);

/**
 * Return a monotonic timestamp in milliseconds.
 *
 * The value has no wall-clock meaning and is intended exclusively for
 * elapsed-time measurements, deadlines and rate windows.
 */
extern uint64_t
datetime_monotonic_ms(void);
extern uint64_t
datetime_monotonic_us(void);

#endif
