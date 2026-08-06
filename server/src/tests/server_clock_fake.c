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

/** @file Test-only fake-clock adapter. */

#include <server_clock_fake.h>
#include <server_main.h>
#include "server/server_clock_internal.h"

void server_clock_fake_install(uint64_t tick_period_us,
                               server_tick_t tick,
                               server_monotonic_t monotonic,
                               server_wall_utc_t wall) {
    server_clock_test_install(tick_period_us, tick, monotonic, wall);
}

void server_clock_fake_advance_ticks(server_tick_duration_t duration) {
    server_clock_test_advance_ticks(duration);
}

void server_clock_fake_advance_monotonic(server_duration_t duration) {
    server_clock_test_advance_monotonic(duration);
}

void server_clock_fake_set_wall(server_wall_utc_t wall) {
    server_clock_test_set_wall(wall);
}

void server_clock_fake_uninstall(void) {
    server_clock_test_uninstall();
}
