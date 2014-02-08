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
 * Video-related code. */

#include <global.h>

/**
 * The display. */
x11_display_type SDL_display;
/**
 * The window. */
x11_window_type SDL_window;

SDL_Window *window;
SDL_Renderer *renderer;

/**
 * Initialize the video system. */
void video_init(void)
{
    SDL_SysWMinfo info;

    window = SDL_CreateWindow(PACKAGE_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, setting_get_int(OPT_CAT_CLIENT, OPT_RESOLUTION_X), setting_get_int(OPT_CAT_CLIENT, OPT_RESOLUTION_Y), setting_get_int(OPT_CAT_CLIENT, OPT_FULLSCREEN) ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    renderer = SDL_CreateRenderer(window, -1, 0);

    if (!video_set_size()) {
        logger_print(LOG(ERROR), "Couldn't set video size: %s", SDL_GetError());
        exit(1);
    }

    SDL_display = NULL;

    /* Grab the window manager specific information. */
    SDL_VERSION(&info.version);

    if (SDL_GetWindowWMInfo(window, &info)) {
#if defined(HAVE_X11)

        if (info.subsystem == SDL_SYSWM_X11) {
            SDL_display = info.info.x11.display;
            SDL_window = info.info.x11.window;
        }
        else {
            logger_print(LOG(BUG), "SDL is not running on X11 display.");
        }

#elif defined(WIN32)
        SDL_window = SDL_display = info.window;
#endif
    }
}

/**
 * Sets the screen surface to a new size, after updating ::Screensize.
 * @return 1 on success, 0 on failure. */
int video_set_size(void)
{
    SDL_RenderSetLogicalSize(renderer, setting_get_int(OPT_CAT_CLIENT, OPT_RESOLUTION_X), setting_get_int(OPT_CAT_CLIENT, OPT_RESOLUTION_Y));
    return 1;
}

/**
 * Check if the client window is in fullscreen mode.
 * @return 1 if the window is fullscreen, 0 otherwise. */
int video_is_fullscreen(void)
{
    return SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN;
}

/**
 * Attempt to flip the video surface to fullscreen or windowed mode.
 *  @return Non-zero on success, zero on failure. */
int video_fullscreen_toggle(void)
{
    if (video_is_fullscreen()) {
        if (SDL_SetWindowFullscreen(window, SDL_FALSE) < 0) {
            return 0;
        }

        return 1;
    }

    if (SDL_SetWindowFullscreen(window, SDL_TRUE) < 0) {
        return 0;
    }

    return 1;
}
