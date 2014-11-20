/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
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

/**
 * Initialize the video system. */
void video_init(void)
{
    SDL_SysWMinfo info;

    list_vid_modes();

    if (!video_set_size()) {
        logger_print(LOG(ERROR), "Couldn't set video size: %s", SDL_GetError());
        exit(1);
    }

    SDL_display = NULL;

    /* Grab the window manager specific information. */
    SDL_VERSION(&info.version);

    if (SDL_GetWMInfo(&info)) {
#if defined(HAVE_X11)

        if (info.subsystem == SDL_SYSWM_X11) {
            SDL_display = info.info.x11.display;
            SDL_window = info.info.x11.window;
        } else {
            logger_print(LOG(BUG), "SDL is not running on X11 display.");
        }

#elif defined(WIN32)
        SDL_window = SDL_display = info.window;
#endif
    }
}

/**
 * Get the bits per pixel value to use
 * @return Bits per pixel. */
int video_get_bpp(void)
{
    return SDL_GetVideoInfo()->vfmt->BitsPerPixel;
}

/**
 * Sets the screen surface to a new size, after updating ::Screensize.
 * @return 1 on success, 0 on failure. */
int video_set_size(void)
{
    SDL_Surface *new;

    /* Try to set the video mode. */
    new = SDL_SetVideoMode(setting_get_int(OPT_CAT_CLIENT, OPT_RESOLUTION_X), setting_get_int(OPT_CAT_CLIENT, OPT_RESOLUTION_Y), video_get_bpp(), get_video_flags());

    if (new) {
        ScreenSurface = new;
        return 1;
    }

    return 0;
}

/**
 * Calculate the video flags from the settings.
 * When settings are changed at runtime, this MUST be called again.
 * @return The flags */
uint32 get_video_flags(void)
{
    if (setting_get_int(OPT_CAT_CLIENT, OPT_FULLSCREEN)) {
        return SDL_FULLSCREEN | SDL_SWSURFACE | SDL_HWACCEL | SDL_HWPALETTE | SDL_DOUBLEBUF | SDL_ANYFORMAT;
    } else {
        return SDL_SWSURFACE | SDL_SWSURFACE | SDL_HWACCEL | SDL_HWPALETTE | SDL_ANYFORMAT | SDL_RESIZABLE;
    }
}

/**
 * Attempt to flip the video surface to fullscreen or windowed mode.
 *
 * Attempts to maintain the surface's state, but makes no guarantee
 * that pointers (i.e., the surface's pixels field) will be the same
 * after this call.
 *
 * @param surface Pointer to surface ptr to toggle. May be different
 * pointer on return. May be NULL on return due to failure.
 * @param flags Pointer to flags to set on surface. The value pointed
 * to will be XOR'd with SDL_FULLSCREEN before use. Actual flags set
 * will be filled into pointer. Contents are undefined on failure. Can
 * be NULL, in which case the surface's current flags are used.
 *  @return Non-zero on success, zero on failure. */
int video_fullscreen_toggle(SDL_Surface **surface, uint32 *flags)
{
    long framesize = 0;
    void *pixels = NULL;
    SDL_Rect clip;
    uint32 tmpflags = 0;
    int w = 0;
    int h = 0;
    int bpp = 0;
    int grabmouse, showmouse;

    grabmouse = SDL_WM_GrabInput(SDL_GRAB_QUERY) == SDL_GRAB_ON;
    showmouse = SDL_ShowCursor(-1);

    if ((!surface) || (!(*surface))) {
        return 0;
    }

    if (SDL_WM_ToggleFullScreen(*surface)) {
        if (flags) {
            *flags ^= SDL_FULLSCREEN;
        }

        return 1;
    }

    if (!(SDL_GetVideoInfo()->wm_available)) {
        return 0;
    }

    tmpflags = (*surface)->flags;
    w = (*surface)->w;
    h = (*surface)->h;
    bpp = (*surface)->format->BitsPerPixel;

    if (flags == NULL) {
        flags = &tmpflags;
    }

    if ((*surface = SDL_SetVideoMode(w, h, bpp, *flags)) == NULL) {
        *surface = SDL_SetVideoMode(w, h, bpp, tmpflags);
    } else {
        return 1;
    }

    SDL_GetClipRect(*surface, &clip);

    /* Save the contents of the screen. */
    if ((!(tmpflags & SDL_OPENGL)) && (!(tmpflags & SDL_OPENGLBLIT))) {
        framesize = (w * h) * ((*surface)->format->BytesPerPixel);
        pixels = emalloc(framesize);

        if (pixels == NULL) {
            return 0;
        }

        memcpy(pixels, (*surface)->pixels, framesize);
    }

    if (grabmouse) {
        SDL_WM_GrabInput(SDL_GRAB_OFF);
    }

    SDL_ShowCursor(1);

    *surface = SDL_SetVideoMode(w, h, bpp, (*flags) ^ SDL_FULLSCREEN);

    if (*surface != NULL) {
        *flags ^= SDL_FULLSCREEN;
    } else {
        *surface = SDL_SetVideoMode(w, h, bpp, tmpflags);

        if (*surface == NULL) {
            if (pixels) {
                efree(pixels);
            }

            return 0;
        }
    }

    /* Unfortunately, you lose your OpenGL image until the next frame... */
    if (pixels) {
        memcpy((*surface)->pixels, pixels, framesize);
        efree(pixels);
    }

    SDL_SetClipRect(*surface, &clip);

    if (grabmouse) {
        SDL_WM_GrabInput(SDL_GRAB_ON);
    }

    SDL_ShowCursor(showmouse);

    return 1;
}
