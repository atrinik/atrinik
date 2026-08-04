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

#ifndef RENDER_PROFILER_H
#define RENDER_PROFILER_H

/** Timed portions of the client frame and map renderer. */
typedef enum render_profile_stage {
    RENDER_PROFILE_FRAME,
    RENDER_PROFILE_EVENTS,
    RENDER_PROFILE_GAME,
    RENDER_PROFILE_WIDGETS,
    RENDER_PROFILE_OVERLAYS,
    RENDER_PROFILE_MAINTENANCE,
    RENDER_PROFILE_PRESENT,
    RENDER_PROFILE_WAIT,
    RENDER_PROFILE_MAP,
    RENDER_PROFILE_MAP_GROUND,
    RENDER_PROFILE_LIGHTING,
    RENDER_PROFILE_MAP_OBJECTS,
    RENDER_PROFILE_MAP_UI,

    RENDER_PROFILE_STAGE_NUM
} render_profile_stage_t;

/** One completed sampling interval displayed by the profiler widget. */
typedef struct render_profile_snapshot {
    uint64_t elapsed_us[RENDER_PROFILE_STAGE_NUM];
    uint32_t calls[RENDER_PROFILE_STAGE_NUM];
    uint64_t interval_us;
    uint32_t frames;
    uint32_t drawn_frames;
    uint32_t generation;
} render_profile_snapshot_t;

void render_profiler_set_enabled(bool enabled);
uint64_t render_profiler_begin(void);
void render_profiler_end(render_profile_stage_t stage, uint64_t started_us);
void render_profiler_frame_finished(bool drawn);
const render_profile_snapshot_t *render_profiler_snapshot(void);
void widget_render_profiler_init(widgetdata *widget);

#endif
