/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2026 Atrinik Development Team                    *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 ************************************************************************/

/** @file Client-side session telemetry. */

#include <global.h>
#include <toolkit/datetime.h>

typedef struct telemetry_state {
    bool exp_initialized;
    uint64_t exp_last;
    uint64_t exp_gained;
    uint64_t exp_started_us;
    bool game_time_valid;
    uint64_t game_seconds;
    uint64_t game_time_synced_us;
    uint32_t millis_per_game_minute;
} telemetry_state_t;

static telemetry_state_t telemetry;

void telemetry_reset(void) {
    telemetry = (telemetry_state_t){0};
    telemetry.exp_started_us = datetime_monotonic_us();
    WIDGET_REDRAW_ALL(XP_TRACKER_ID);
    WIDGET_REDRAW_ALL(GAME_TIME_ID);
}

void telemetry_exp_update(uint64_t exp) {
    if (!telemetry.exp_initialized) {
        telemetry.exp_initialized = true;
        telemetry.exp_last = exp;
        telemetry.exp_started_us = datetime_monotonic_us();
        return;
    }

    if (exp > telemetry.exp_last) {
        uint64_t gained = exp - telemetry.exp_last;
        if (UINT64_MAX - telemetry.exp_gained < gained) {
            telemetry.exp_gained = UINT64_MAX;
        } else {
            telemetry.exp_gained += gained;
        }
    }

    telemetry.exp_last = exp;
    WIDGET_REDRAW_ALL(XP_TRACKER_ID);
}

void telemetry_exp_tracker_reset(void) {
    telemetry.exp_initialized = true;
    telemetry.exp_last = cpl.stats.exp;
    telemetry.exp_gained = 0;
    telemetry.exp_started_us = datetime_monotonic_us();
    WIDGET_REDRAW_ALL(XP_TRACKER_ID);
    draw_info(COLOR_GREEN, "Experience tracker reset.");
}

uint64_t telemetry_exp_gained(void) {
    return telemetry.exp_gained;
}

uint64_t telemetry_exp_elapsed_seconds(void) {
    uint64_t elapsed_us = datetime_monotonic_us() - telemetry.exp_started_us;
    return elapsed_us / 1000000;
}

uint64_t telemetry_exp_per_hour(void) {
    uint64_t elapsed_us = datetime_monotonic_us() - telemetry.exp_started_us;
    if (elapsed_us == 0 || telemetry.exp_gained == 0) {
        return 0;
    }

    long double hourly = (long double)telemetry.exp_gained * 3600000000.0L / elapsed_us;
    return hourly >= UINT64_MAX ? UINT64_MAX : (uint64_t)hourly;
}

void telemetry_game_time_sync(uint64_t game_seconds, uint32_t millis_per_game_minute) {
    if (millis_per_game_minute == 0) {
        LOG(PACKET, "Ignoring game-time synchronization with a zero rate");
        return;
    }

    telemetry.game_time_valid = true;
    telemetry.game_seconds = game_seconds;
    telemetry.game_time_synced_us = datetime_monotonic_us();
    telemetry.millis_per_game_minute = millis_per_game_minute;
}

bool telemetry_game_time_get(uint64_t *game_minutes, uint32_t *millis_per_game_minute) {
    if (!telemetry.game_time_valid) {
        return false;
    }

    uint64_t elapsed_us = datetime_monotonic_us() - telemetry.game_time_synced_us;
    uint64_t minute_us = (uint64_t)telemetry.millis_per_game_minute * 1000;
    uint64_t elapsed_minutes = elapsed_us / minute_us;
    uint64_t elapsed_seconds = elapsed_minutes * 60 + elapsed_us % minute_us * 60 / minute_us;
    uint64_t current_seconds = telemetry.game_seconds;
    if (UINT64_MAX - current_seconds < elapsed_seconds) {
        current_seconds = UINT64_MAX;
    } else {
        current_seconds += elapsed_seconds;
    }

    *game_minutes = current_seconds / 60;
    *millis_per_game_minute = telemetry.millis_per_game_minute;
    return true;
}
