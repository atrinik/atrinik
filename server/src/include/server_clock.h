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

/**
 * @file
 * Typed server clock domains and overflow-safe deadline helpers.
 */

#ifndef SERVER_CLOCK_H
#define SERVER_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

/** Authoritative simulation time. It advances only from the game loop. */
typedef struct server_tick {
    uint64_t value;
} server_tick_t;

/** Number of authoritative simulation ticks. */
typedef struct server_tick_duration {
    uint64_t value;
} server_tick_duration_t;

/** Monotonic process timestamp. It has no wall-clock or persistent meaning. */
typedef struct server_monotonic {
    uint64_t microseconds;
} server_monotonic_t;

/** Elapsed real time, represented in microseconds. */
typedef struct server_duration {
    uint64_t microseconds;
} server_duration_t;

/** UTC Unix timestamp, used only for durable calendar facts and operator output. */
typedef struct server_wall_utc {
    int64_t seconds;
} server_wall_utc_t;

/**
 * Initialize the production clock.
 *
 * @param tick_period_us
 * Duration of one authoritative simulation tick. Must be non-zero.
 * @param initial_tick
 * Initial simulation tick.
 */
void server_clock_init(uint64_t tick_period_us, server_tick_t initial_tick);

/** Update the configured simulation tick period after an intentional speed change. */
void server_clock_set_tick_period(uint64_t tick_period_us);

/** Advance simulation time by one tick. This is owned by the authoritative main loop. */
void server_clock_advance_tick(void);

server_tick_t server_tick_now(void);
server_tick_t server_tick_deadline_after(server_tick_duration_t duration);
bool server_tick_expired(server_tick_t deadline);
server_tick_duration_t server_tick_elapsed(server_tick_t since);
server_tick_duration_t server_tick_difference(server_tick_t later, server_tick_t earlier);

server_monotonic_t server_monotonic_now(void);
server_monotonic_t server_monotonic_deadline_after(server_duration_t duration);
bool server_monotonic_expired(server_monotonic_t deadline);
bool server_monotonic_reached(server_monotonic_t now, server_monotonic_t deadline);
bool server_monotonic_before(server_monotonic_t lhs, server_monotonic_t rhs);
bool server_monotonic_is_set(server_monotonic_t timestamp);
server_duration_t server_monotonic_elapsed(server_monotonic_t since);
server_duration_t server_monotonic_difference(server_monotonic_t later, server_monotonic_t earlier);

server_wall_utc_t server_wall_utc_now(void);

/**
 * Convert a persisted UTC deadline to a bounded in-process remaining duration.
 *
 * Expired deadlines produce zero and false. Future deadlines are clamped to
 * maximum so corrupt or unreasonable persisted values cannot create an
 * unbounded deadline.
 */
bool server_wall_utc_remaining(server_wall_utc_t deadline,
                               server_wall_utc_t now,
                               server_duration_t maximum,
                               server_duration_t *remaining);

server_duration_t server_duration_from_milliseconds(uint64_t milliseconds);
server_duration_t server_duration_from_seconds(uint64_t seconds);

/** Convert a duration to ticks, rounding up so deadlines never expire early. */
bool server_duration_to_ticks(server_duration_t duration, server_tick_duration_t *ticks);

/** Convert ticks to a duration, failing if the exact result is not representable. */
bool server_ticks_to_duration(server_tick_duration_t ticks, server_duration_t *duration);

#endif
