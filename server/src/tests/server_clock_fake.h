/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 ************************************************************************/

/** @file Test-only control surface for the server clock. */

#ifndef SERVER_CLOCK_FAKE_H
#define SERVER_CLOCK_FAKE_H

#include <server_clock.h>

void server_clock_fake_install(uint64_t tick_period_us,
                               server_tick_t tick,
                               server_monotonic_t monotonic,
                               server_wall_utc_t wall);
void server_clock_fake_advance_ticks(server_tick_duration_t duration);
void server_clock_fake_advance_monotonic(server_duration_t duration);
void server_clock_fake_set_wall(server_wall_utc_t wall);
void server_clock_fake_uninstall(void);

#endif
