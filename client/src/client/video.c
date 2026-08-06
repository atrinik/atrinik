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

/**
 * @file
 * SDL3 window and software-surface presentation.
 */

#include <global.h>
#include <video.h>

void video_init(void) {
    list_vid_modes();

    if (!video_set_size()) {
        LOG(ERROR, "Couldn't create the game window: %s", SDL_GetError());
        exit(1);
    }
}

void video_set_icon(SDL_Surface *icon) {
    HARD_ASSERT(icon != NULL);

    if (!SDL_SetWindowIcon(ScreenWindow, icon)) {
        LOG(BUG, "Could not set the window icon: %s", SDL_GetError());
    }

    SDL_DestroySurface(icon);
}

int video_get_bpp(void) {
    if (ScreenSurface == NULL) {
        return 32;
    }

    const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(ScreenSurface->format);
    return format != NULL ? format->bits_per_pixel : 32;
}

int video_set_size(void) {
    int width = setting_get_int(OPT_CAT_CLIENT, OPT_RESOLUTION_X);
    int height = setting_get_int(OPT_CAT_CLIENT, OPT_RESOLUTION_Y);

    if (ScreenWindow == NULL) {
        ScreenWindow = SDL_CreateWindow(PACKAGE_NAME, width, height, get_video_flags());
        if (ScreenWindow == NULL) {
            return 0;
        }
    } else {
        if (!SDL_SetWindowSize(ScreenWindow, width, height) ||
            !SDL_SetWindowFullscreen(ScreenWindow,
                                     setting_get_int(OPT_CAT_CLIENT, OPT_FULLSCREEN) != 0)) {
            return 0;
        }
    }

    ScreenSurface = SDL_GetWindowSurface(ScreenWindow);
    return ScreenSurface != NULL;
}

uint32_t get_video_flags(void) {
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;

    if (setting_get_int(OPT_CAT_CLIENT, OPT_FULLSCREEN)) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }

    return (uint32_t)flags;
}

int video_fullscreen_toggle(SDL_Surface **surface, uint32_t *flags) {
    HARD_ASSERT(surface != NULL);

    bool fullscreen = (SDL_GetWindowFlags(ScreenWindow) & SDL_WINDOW_FULLSCREEN) != 0;
    if (!SDL_SetWindowFullscreen(ScreenWindow, !fullscreen)) {
        return 0;
    }

    *surface = SDL_GetWindowSurface(ScreenWindow);
    if (*surface == NULL) {
        return 0;
    }

    if (flags != NULL) {
        *flags = get_video_flags();
    }

    return 1;
}
