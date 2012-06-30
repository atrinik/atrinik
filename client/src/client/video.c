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
SDL_display_type SDL_display;
/**
 * The window. */
SDL_window_type SDL_window;

/**
 * Initialize the video system. */
void video_init(void)
{
	SDL_SysWMinfo info;

	list_vid_modes();

	if (!video_set_size())
	{
		logger_print(LOG(ERROR), "Couldn't set video size: %s", SDL_GetError());
		exit(1);
	}

	SDL_display = NULL;

	/* Grab the window manager specific information. */
	SDL_VERSION(&info.version);

	if (SDL_GetWMInfo(&info))
	{
#if defined(HAVE_X11)
		if (info.subsystem == SDL_SYSWM_X11)
		{
			SDL_display = info.info.x11.display;
			SDL_window = info.info.x11.window;
		}
		else
		{
			logger_print(LOG(BUG), "SDL is not running on X11 display.");
		}

#elif defined(WIN32)
		SDL_window = SDL_display = info.window;
#endif
	}
}

#if defined(HAVE_X11)
/**
 * Sends event to X11.
 *
 * @author Tomas Styblo (wmctrl - GPLv2) */
static int x11_send_event(Display *disp, Window win, char *msg, unsigned long data0, unsigned long data1, unsigned long data2, unsigned long data3, unsigned long data4)
{
	XEvent event;
	long mask;

	mask = SubstructureRedirectMask | SubstructureNotifyMask;
	event.xclient.type = ClientMessage;
	event.xclient.serial = 0;
	event.xclient.send_event = True;
	event.xclient.message_type = XInternAtom(disp, msg, False);
	event.xclient.window = win;
	event.xclient.format = 32;
	event.xclient.data.l[0] = data0;
	event.xclient.data.l[1] = data1;
	event.xclient.data.l[2] = data2;
	event.xclient.data.l[3] = data3;
	event.xclient.data.l[4] = data4;

	if (!XSendEvent(disp, DefaultRootWindow(disp), False, mask, &event))
	{
		logger_print(LOG(BUG), "Cannot send event: %s", msg);
		return 0;
	}

	return 1;
}

/**
 * Acquires X11 window's property.
 *
 * @author Tomas Styblo (wmctrl - GPLv2) */
static char *x11_get_property(Display *disp, Window win, Atom xa_prop_type, char *prop_name, unsigned long *size)
{
	Atom xa_prop_name;
	Atom xa_ret_type;
	int ret_format;
	unsigned long ret_nitems;
	unsigned long ret_bytes_after;
	unsigned long tmp_size;
	unsigned char *ret_prop;
	char *ret;

	xa_prop_name = XInternAtom(disp, prop_name, False);

	/* MAX_PROPERTY_VALUE_LEN / 4 explanation (XGetWindowProperty manpage):
	 *
	 * long_length = Specifies the length in 32-bit multiples of the
	 *               data to be retrieved.
	 */
	if (XGetWindowProperty(disp, win, xa_prop_name, 0, 1024, False, xa_prop_type, &xa_ret_type, &ret_format, &ret_nitems, &ret_bytes_after, &ret_prop) != Success)
	{
		logger_print(LOG(BUG), "Cannot get property: %s", prop_name);
		return NULL;
	}

	if (xa_ret_type != xa_prop_type)
	{
		logger_print(LOG(BUG), "Invalid type of property: %s", prop_name);
		XFree(ret_prop);
		return NULL;
	}

	/* Terminate the result to make string handling easier. */
	tmp_size = (ret_format / 8) * ret_nitems;
	ret = malloc(tmp_size + 1);
	memcpy(ret, ret_prop, tmp_size);
	ret[tmp_size] = '\0';

	if (size)
	{
		*size = tmp_size;
	}

	XFree(ret_prop);
	return ret;
}

/**
 * Get the actual client window. */
static Window x11_get_window(void)
{
	Window root, parent, *children;
	unsigned int children_count;

	XQueryTree(SDL_display, SDL_window, &root, &parent, &children, &children_count);

	return parent;
}

#endif

/**
 * Raises the client's window.
 * @param switch_desktop If 1, will also switch the desktop to that of
 * the client's desktop.
 * @author Tomas Styblo (wmctrl - GPLv2) */
void video_window_activate(uint8 switch_desktop)
{
#if defined(HAVE_X11)
	Window win;
#endif

	if (!SDL_display)
	{
		return;
	}

#if defined(HAVE_X11)
	win = x11_get_window();

	if (switch_desktop)
	{
		unsigned long *desktop;

		if (!(desktop = (unsigned long *) x11_get_property(SDL_display, win, XA_CARDINAL, "_NET_WM_DESKTOP", NULL)))
		{
			if (!(desktop = (unsigned long *) x11_get_property(SDL_display, win, XA_CARDINAL, "_WIN_WORKSPACE", NULL)))
			{
				logger_print(LOG(BUG), "Cannot find desktop ID of the window.");
			}
		}

		if (desktop)
		{
			if (!x11_send_event(SDL_display, DefaultRootWindow(SDL_display), "_NET_CURRENT_DESKTOP", *desktop, 0, 0, 0, 0))
			{
				logger_print(LOG(BUG), "Cannot switch desktop.");
			}

			free(desktop);
		}
	}

	x11_send_event(SDL_display, win, "_NET_ACTIVE_WINDOW", 0, 0, 0, 0, 0);
	XMapRaised(SDL_display, win);
#endif
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

	if (new)
	{
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
	if (setting_get_int(OPT_CAT_CLIENT, OPT_FULLSCREEN))
	{
		return SDL_FULLSCREEN | SDL_SWSURFACE | SDL_HWACCEL | SDL_HWPALETTE | SDL_DOUBLEBUF | SDL_ANYFORMAT;
	}
	else
	{
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

	if ((!surface) || (!(*surface)))
	{
		return 0;
	}

	if (SDL_WM_ToggleFullScreen(*surface))
	{
		if (flags)
		{
			*flags ^= SDL_FULLSCREEN;
		}

		return 1;
	}

	if (!(SDL_GetVideoInfo()->wm_available))
	{
		return 0;
	}

	tmpflags = (*surface)->flags;
	w = (*surface)->w;
	h = (*surface)->h;
	bpp = (*surface)->format->BitsPerPixel;

	if (flags == NULL)
	{
		flags = &tmpflags;
	}

	if ((*surface = SDL_SetVideoMode(w, h, bpp, *flags)) == NULL)
	{
		*surface = SDL_SetVideoMode(w, h, bpp, tmpflags);
	}
	else
	{
		return 1;
	}

	if (flags == NULL)
	{
		flags = &tmpflags;
	}

	SDL_GetClipRect(*surface, &clip);

	/* Save the contents of the screen. */
	if ((!(tmpflags & SDL_OPENGL)) && (!(tmpflags & SDL_OPENGLBLIT)))
	{
		framesize = (w * h) * ((*surface)->format->BytesPerPixel);
		pixels = malloc(framesize);

		if (pixels == NULL)
		{
			return 0;
		}

		memcpy(pixels, (*surface)->pixels, framesize);
	}

	if (grabmouse)
	{
		SDL_WM_GrabInput(SDL_GRAB_OFF);
	}

	SDL_ShowCursor(1);

	*surface = SDL_SetVideoMode(w, h, bpp, (*flags) ^ SDL_FULLSCREEN);

	if (*surface != NULL)
	{
		*flags ^= SDL_FULLSCREEN;
	}
	else
	{
		*surface = SDL_SetVideoMode(w, h, bpp, tmpflags);

		if (*surface == NULL)
		{
			if (pixels)
			{
				free(pixels);
			}

			return 0;
		}
	}

	/* Unfortunately, you lose your OpenGL image until the next frame... */
	if (pixels)
	{
		memcpy((*surface)->pixels, pixels, framesize);
		free(pixels);
	}

	SDL_SetClipRect(*surface, &clip);

	if (grabmouse)
	{
		SDL_WM_GrabInput(SDL_GRAB_ON);
	}

	SDL_ShowCursor(showmouse);

	return 1;
}
