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
 * Video-related code.
 */

#include <global.h>

/**
 * The display.
 */
x11_display_type SDL_display;
/**
 * The window.
 */
x11_window_type SDL_window;

/**
 * Initialize the video system.
 */
void video_init(void)
{
    SDL_SysWMinfo info;

    list_vid_modes();

    if (!video_set_size()) {
        LOG(ERROR, "Couldn't set video size: %s", SDL_GetError());
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
            LOG(BUG, "SDL is not running on X11 display.");
        }

#elif defined(WIN32)
        SDL_window = SDL_display = info.window;
#endif
    }
}

#if defined(HAVE_X11)
/**
 * Creates a mask from the specified icon and flags.
 *
 * @param icon
 * Icon.
 * @param[out] mask
 * Mask to write to.
 * @param flags
 * Flags bitmask, 0x1 for color key, 0x2 for alpha channel.
 * @author
 * SDL - Simple DirectMedia Layer
 * Copyright (C) 1997-2012 Sam Lantinga
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Sam Lantinga
 * slouken@libsdl.org
 */
static void
video_mask_from_icon (SDL_Surface *icon,
                      Uint8       *mask,
                      int          flags)
{
    HARD_ASSERT(icon != NULL);
    HARD_ASSERT(mask != NULL);

#define SET_MASKBIT(icon, x, y, mask) \
    mask[(y * ((icon->w + 7) / 8)) + (x / 8)] &= ~(0x1 << (7 - (x % 8)))

    Uint32 colorkey = icon->format->colorkey;
    switch (icon->format->BytesPerPixel) {
        case 1:
            for (int y = 0; y < icon->h; ++y) {
                Uint8 *pixels = (Uint8 *) icon->pixels + y * icon->pitch;
                for (int x = 0; x < icon->w; ++x) {
                    if (*pixels++ == colorkey) {
                        SET_MASKBIT(icon, x, y, mask);
                    }
                }
            }

            break;

        case 2:
            for (int y = 0; y < icon->h; ++y) {
                Uint16 *pixels = (Uint16 *) icon->pixels + y * icon->pitch / 2;
                for (int x = 0; x < icon->w; ++x) {
                    if ((flags & 0x1) && *pixels == colorkey) {
                        SET_MASKBIT(icon, x, y, mask);
                    } else if ((flags & 0x2) &&
                               (*pixels & icon->format->Amask) == 0) {
                        SET_MASKBIT(icon, x, y, mask);
                    }

                    pixels++;
                }
            }

            break;

        case 4:
            for (int y = 0; y < icon->h; ++y) {
                Uint32 *pixels = (Uint32 *) icon->pixels + y * icon->pitch / 4;
                for (int x = 0; x < icon->w; ++x) {
                    if ((flags & 0x1) && *pixels == colorkey) {
                        SET_MASKBIT(icon, x, y, mask);
                    } else if ((flags & 0x2) &&
                               (*pixels & icon->format->Amask) == 0) {
                        SET_MASKBIT(icon, x, y, mask);
                    }

                    pixels++;
                }
            }

            break;
    }

#undef SET_MASKBIT
}

/**
 * Sets an icon for the specified X11 window.
 *
 * @param icon
 * Icon to set.
 * @param display
 * X11 display.
 * @param win
 * X11 window.
 * @param net_wm_icon
 * Atom _NET_WM_ICON variable.
 * @author
 * SDL - Simple DirectMedia Layer
 * Copyright (C) 1997-2012 Sam Lantinga
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Sam Lantinga
 * slouken@libsdl.org
 */
static void
video_set_icon_x11 (SDL_Surface     *icon,
                    x11_display_type display,
                    x11_window_type  win,
                    Atom             net_wm_icon)
{
    HARD_ASSERT(icon != NULL);

    size_t mask_len = icon->h * (icon->w + 7) / 8;
    int flags = 0;
    Uint8 *mask = emalloc(mask_len);
    memset(mask, ~0, mask_len);

    if (icon->flags & SDL_SRCCOLORKEY) {
        flags |= 0x1;
    }

    if (icon->flags & SDL_SRCALPHA) {
        flags |= 0x2;
    }

    if (flags != 0x0) {
        video_mask_from_icon(icon, mask, flags);
    }

    /* Convert the icon to ARGB for modern window managers */
    SDL_PixelFormat format;
    SDL_memset(&format, 0, sizeof(format));
    format.BitsPerPixel = 32;
    format.BytesPerPixel = 4;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    format.Rshift = 8;
    format.Gshift = 16;
    format.Bshift = 24;
    format.Ashift = 0;
#else
    format.Rshift = 16;
    format.Gshift = 8;
    format.Bshift = 0;
    format.Ashift = 24;
#endif
    format.Rmask = 0xFF << format.Rshift;
    format.Gmask = 0xFF << format.Gshift;
    format.Bmask = 0xFF << format.Bshift;
    format.Amask = 0xFF << format.Ashift;
    format.alpha = SDL_ALPHA_OPAQUE;

    SDL_Surface *surface = SDL_ConvertSurface(icon, &format, 0);
    if (surface == NULL) {
        goto out;
    }

    size_t prop_size = 2 + (icon->w * icon->h);
    long *prop_data = emalloc(prop_size * sizeof(*prop_data));

    prop_data[0] = icon->w;
    prop_data[1] = icon->h;

    long *dst = &prop_data[2];
    size_t maskidx = 0;
    for (int y = 0; y < icon->h; ++y) {
        Uint32 *src = (Uint32 *) ((Uint8 *) surface->pixels +
                                  y * surface->pitch);

        for (int x = 0; x < icon->w; ++x) {
            const Uint32 pixel = *(src++);

            if (mask[maskidx / 8] & (1 << (7 - (maskidx % 8)))) {
                *dst++ = pixel | format.Amask;
            } else {
                *dst++ = pixel & ~format.Amask;
            }

            maskidx++;
        }
    }

    XChangeProperty(display,
                    win,
                    net_wm_icon,
                    XA_CARDINAL,
                    32,
                    PropModeReplace,
                    (unsigned char *) prop_data,
                    prop_size);
    efree(prop_data);

out:
    if (surface != NULL) {
        SDL_FreeSurface(surface);
    }

    efree(mask);
}
#endif

/**
 * Sets the application icon.
 *
 * @param icon
 * Icon to set.
 */
void
video_set_icon (SDL_Surface *icon)
{
    HARD_ASSERT(icon != NULL);

#if defined(HAVE_X11)
    Atom net_wm_icon = XInternAtom(SDL_display, "_NET_WM_ICON", False);
    if (net_wm_icon) {
        video_set_icon_x11(icon,
                           SDL_display,
                           x11_window_get_parent(SDL_display, SDL_window),
                           net_wm_icon);
        goto out;
    }
#endif

    SDL_WM_SetIcon(icon, NULL);

out:
    SDL_FreeSurface(icon);
}

/**
 * Get the bits per pixel value to use
 * @return
 * Bits per pixel.
 */
int video_get_bpp(void)
{
    return SDL_GetVideoInfo()->vfmt->BitsPerPixel;
}

/**
 * Sets the screen surface to a new size, after updating ::Screensize.
 * @return
 * 1 on success, 0 on failure.
 */
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
 * @return
 * The flags
 */
uint32_t get_video_flags(void)
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
 * @param surface
 * Pointer to surface ptr to toggle. May be different
 * pointer on return. May be NULL on return due to failure.
 * @param flags
 * Pointer to flags to set on surface. The value pointed
 * to will be XOR'd with SDL_FULLSCREEN before use. Actual flags set
 * will be filled into pointer. Contents are undefined on failure. Can
 * be NULL, in which case the surface's current flags are used.
 *  @return
 * Non-zero on success, zero on failure.
 */
int video_fullscreen_toggle(SDL_Surface **surface, uint32_t *flags)
{
    size_t framesize = 0;
    void *pixels = NULL;
    SDL_Rect clip;
    uint32_t tmpflags = 0;
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
        framesize = (w * h) * (size_t) (*surface)->format->BytesPerPixel;
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
