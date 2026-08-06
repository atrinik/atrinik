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

/** @file Typed server clock implementation. */

#include <global.h>
#include <server_main.h>
#include <server_clock.h>
#include <toolkit/datetime.h>
#include "server_clock_internal.h"

typedef struct server_clock_state {
    uint64_t production_tick_period_us;
    server_tick_t production_tick;
    bool fake;
    uint64_t fake_tick_period_us;
    server_tick_t fake_tick;
    server_monotonic_t fake_monotonic;
    server_wall_utc_t fake_wall;
} server_clock_state_t;

static server_clock_state_t clock_state;

static uint64_t saturating_add(uint64_t lhs, uint64_t rhs) {
    return UINT64_MAX - lhs < rhs ? UINT64_MAX : lhs + rhs;
}

static uint64_t saturating_multiply(uint64_t lhs, uint64_t rhs) {
    return rhs != 0 && lhs > UINT64_MAX / rhs ? UINT64_MAX : lhs * rhs;
}

void server_clock_init(uint64_t tick_period_us, server_tick_t initial_tick) {
    HARD_ASSERT(tick_period_us != 0);

    clock_state.production_tick_period_us = tick_period_us;
    clock_state.production_tick = initial_tick;
    clock_state.fake = false;
}

void server_clock_set_tick_period(uint64_t tick_period_us) {
    HARD_ASSERT(tick_period_us != 0);
    clock_state.production_tick_period_us = tick_period_us;
}

void server_clock_advance_tick(void) {
    server_tick_t *tick = clock_state.fake ? &clock_state.fake_tick : &clock_state.production_tick;
    tick->value = saturating_add(tick->value, 1);
}

server_tick_t server_tick_now(void) {
    return clock_state.fake ? clock_state.fake_tick : clock_state.production_tick;
}

server_tick_t server_tick_deadline_after(server_tick_duration_t duration) {
    server_tick_t deadline = {saturating_add(server_tick_now().value, duration.value)};
    return deadline;
}

bool server_tick_expired(server_tick_t deadline) {
    return server_tick_now().value >= deadline.value;
}

server_tick_duration_t server_tick_difference(server_tick_t later, server_tick_t earlier) {
    server_tick_duration_t elapsed = {later.value >= earlier.value ? later.value - earlier.value
                                                                   : 0};
    return elapsed;
}

server_tick_duration_t server_tick_elapsed(server_tick_t since) {
    return server_tick_difference(server_tick_now(), since);
}

server_monotonic_t server_monotonic_now(void) {
    if (clock_state.fake) {
        return clock_state.fake_monotonic;
    }

    server_monotonic_t now = {datetime_monotonic_us()};
    return now;
}

server_monotonic_t server_monotonic_deadline_after(server_duration_t duration) {
    server_monotonic_t deadline = {
        saturating_add(server_monotonic_now().microseconds, duration.microseconds)};
    return deadline;
}

bool server_monotonic_reached(server_monotonic_t now, server_monotonic_t deadline) {
    return now.microseconds >= deadline.microseconds;
}

bool server_monotonic_expired(server_monotonic_t deadline) {
    return server_monotonic_reached(server_monotonic_now(), deadline);
}

bool server_monotonic_before(server_monotonic_t lhs, server_monotonic_t rhs) {
    return lhs.microseconds < rhs.microseconds;
}

bool server_monotonic_is_set(server_monotonic_t timestamp) {
    return timestamp.microseconds != 0;
}

server_duration_t server_monotonic_difference(server_monotonic_t later,
                                              server_monotonic_t earlier) {
    server_duration_t elapsed = {
        later.microseconds >= earlier.microseconds ? later.microseconds - earlier.microseconds : 0};
    return elapsed;
}

server_duration_t server_monotonic_elapsed(server_monotonic_t since) {
    return server_monotonic_difference(server_monotonic_now(), since);
}

bool server_monotonic_elapsed_at_least(server_monotonic_t now,
                                       server_monotonic_t since,
                                       server_duration_t duration) {
    return server_monotonic_difference(now, since).microseconds >= duration.microseconds;
}

server_wall_utc_t server_wall_utc_now(void) {
    if (clock_state.fake) {
        return clock_state.fake_wall;
    }

    server_wall_utc_t now = {(int64_t)time(NULL)};
    return now;
}

bool server_wall_utc_remaining(server_wall_utc_t deadline,
                               server_wall_utc_t now,
                               server_duration_t maximum,
                               server_duration_t *remaining) {
    HARD_ASSERT(remaining != NULL);

    if (deadline.seconds <= now.seconds) {
        remaining->microseconds = 0;
        return false;
    }

    /* Unsigned subtraction represents the complete ordered int64_t distance,
     * including INT64_MIN to INT64_MAX, without signed overflow. */
    uint64_t seconds = (uint64_t)deadline.seconds - (uint64_t)now.seconds;
    *remaining = server_duration_from_seconds(seconds);
    if (remaining->microseconds > maximum.microseconds) {
        *remaining = maximum;
    }
    return true;
}

server_duration_t server_duration_from_milliseconds(uint64_t milliseconds) {
    server_duration_t duration = {saturating_multiply(milliseconds, UINT64_C(1000))};
    return duration;
}

server_duration_t server_duration_from_seconds(uint64_t seconds) {
    server_duration_t duration = {saturating_multiply(seconds, UINT64_C(1000000))};
    return duration;
}

bool server_duration_to_ticks(server_duration_t duration, server_tick_duration_t *ticks) {
    HARD_ASSERT(ticks != NULL);

    uint64_t tick_period_us =
        clock_state.fake ? clock_state.fake_tick_period_us : clock_state.production_tick_period_us;
    if (tick_period_us == 0) {
        return false;
    }

    ticks->value = duration.microseconds / tick_period_us;
    if (duration.microseconds % tick_period_us != 0) {
        ticks->value++;
    }
    return true;
}

bool server_ticks_to_duration(server_tick_duration_t ticks, server_duration_t *duration) {
    HARD_ASSERT(duration != NULL);

    uint64_t tick_period_us =
        clock_state.fake ? clock_state.fake_tick_period_us : clock_state.production_tick_period_us;
    if (tick_period_us == 0 || (ticks.value != 0 && tick_period_us > UINT64_MAX / ticks.value)) {
        return false;
    }

    duration->microseconds = ticks.value * tick_period_us;
    return true;
}

void server_clock_test_install(uint64_t tick_period_us,
                               server_tick_t tick,
                               server_monotonic_t monotonic,
                               server_wall_utc_t wall) {
    HARD_ASSERT(tick_period_us != 0);

    clock_state.fake_tick_period_us = tick_period_us;
    clock_state.fake_tick = tick;
    clock_state.fake_monotonic = monotonic;
    clock_state.fake_wall = wall;
    clock_state.fake = true;
}

void server_clock_test_advance_ticks(server_tick_duration_t duration) {
    HARD_ASSERT(clock_state.fake);
    clock_state.fake_tick.value = saturating_add(clock_state.fake_tick.value, duration.value);
}

void server_clock_test_advance_monotonic(server_duration_t duration) {
    HARD_ASSERT(clock_state.fake);
    clock_state.fake_monotonic.microseconds =
        saturating_add(clock_state.fake_monotonic.microseconds, duration.microseconds);
}

void server_clock_test_set_wall(server_wall_utc_t wall) {
    HARD_ASSERT(clock_state.fake);
    clock_state.fake_wall = wall;
}

void server_clock_test_uninstall(void) {
    clock_state.fake = false;
}
