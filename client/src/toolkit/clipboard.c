/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
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
 * Implements clipboard handling.
 *
 * Based on the works of Sam Lantinga (SDL_scrap) and Eric Wing (SDL_Clipboard).
 *
 * @author Alex Tokar */

#include <global.h>
#include <SDL_syswm.h>

#if defined(HAVE_X11)
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#ifdef HAVE_X11_XMU
#	include <X11/Xmu/Atoms.h>
#endif

static Display *SDL_Display = NULL;
static Window SDL_Window;
#elif defined(WIN32)
static HWND SDL_Window = NULL;
#endif

#if defined(HAVE_X11)
static int clipboard_filter(const SDL_Event *event)
{
	/* Post all non-window manager specific events */
	if (event->type != SDL_SYSWMEVENT)
	{
		return 1;
	}

	/* Handle window-manager specific clipboard events. */
	switch (event->syswm.msg->event.xevent.type)
	{
		/* Copy the selection from XA_CUT_BUFFER0 to the requested property. */
		case SelectionRequest:
		{
			XSelectionRequestEvent *req;
			XEvent sevent;
			int seln_format;
			unsigned long nbytes, overflow;
			unsigned char *seln_data;

			req = &event->syswm.msg->event.xevent.xselectionrequest;
			sevent.xselection.type = SelectionNotify;
			sevent.xselection.display = req->display;
			sevent.xselection.selection = req->selection;
			sevent.xselection.target = None;
			sevent.xselection.property = None;
			sevent.xselection.requestor = req->requestor;
			sevent.xselection.time = req->time;

			if (XGetWindowProperty(SDL_Display, DefaultRootWindow(SDL_Display), XA_CUT_BUFFER0, 0, INT_MAX / 4, False, req->target, &sevent.xselection.target, &seln_format, &nbytes, &overflow, &seln_data) == Success)
			{
				if (sevent.xselection.target == req->target)
				{
					if (sevent.xselection.target == XA_STRING)
					{
						if (seln_data[nbytes - 1] == '\0')
						{
							nbytes--;
						}
					}

					XChangeProperty(SDL_Display, req->requestor, req->property, sevent.xselection.target, seln_format, PropModeReplace, seln_data, nbytes);
					sevent.xselection.property = req->property;
				}

				XFree(seln_data);
			}

			XSendEvent(SDL_Display, req->requestor, False, 0, &sevent);
			XSync(SDL_Display, False);
		}

		break;
	}

	/* Post the event for X11 clipboard reading above. */
	return 1;
}
#endif

/**
 * Initializes the clipboard
 * @return 1 on success, 0 on failure. */
int clipboard_init(void)
{
	SDL_SysWMinfo info;

	/* Grab the window manager specific information. */
	SDL_VERSION(&info.version);

	if (SDL_GetWMInfo(&info))
	{
		/* Save the information for later use. */
#if defined(HAVE_X11)
		if (info.subsystem == SDL_SYSWM_X11)
		{
			SDL_Display = info.info.x11.display;
			SDL_Window = info.info.x11.window;

			/* Enable the special window hook events. */
			SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
			SDL_SetEventFilter(clipboard_filter);

			return 1;
		}
		else
		{
			LOG(llevBug, "SDL is not running on X11 display.\n");
			return 0;
		}

#elif defined(WIN32)
		SDL_Window = info.window;
		return 1;

#else
		return 0;

#endif
	}

	return 0;
}

/**
 * Set the contents of the clipboard.
 * @param str String to set contents of the clipboard to.
 * @return 1 on success, 0 on failure. */
int clipboard_set(const char *str)
{
#if defined(HAVE_X11)
	if (!SDL_Display)
	{
		return 0;
	}

	XChangeProperty(SDL_Display, DefaultRootWindow(SDL_Display), XA_CUT_BUFFER0, XA_STRING, 8, PropModeReplace, (unsigned const char *) str, strlen(str));

	if (XGetSelectionOwner(SDL_Display, XA_PRIMARY) != SDL_Window)
	{
		XSetSelectionOwner(SDL_Display, XA_PRIMARY, SDL_Window, CurrentTime);
	}

#ifdef HAVE_X11_XMU
	if (XGetSelectionOwner(SDL_Display, XA_CLIPBOARD(SDL_Display)) != SDL_Window)
	{
		XSetSelectionOwner(SDL_Display, XA_CLIPBOARD(SDL_Display), SDL_Window, CurrentTime);
	}
#endif

	if (getenv("KDE_FULL_SESSION"))
	{
		char buf[4096 * 4];

		snprintf(buf, sizeof(buf), "dbus-send --type=method_call --dest=org.kde.klipper /klipper org.kde.klipper.klipper.setClipboardContents string:\"%s\"", str);

		if (system(buf) != 0)
		{
			return 0;
		}
	}

	return 1;
#elif defined(WIN32)
	if (!SDL_Window)
	{
		return 0;
	}

	if (OpenClipboard(SDL_Window))
	{
		SIZE_T i, size;
		HANDLE hMem;

		/* Find out the size of the data. */
		for (size = 0, i = 0; str[i] != '\0'; i++, size++)
		{
			if (str[i] == '\n' && (i == 0 || str[i - 1] != '\r'))
			{
				/* We're going to insert a carriage return. */
				size++;
			}
		}

		size += 1;

		hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, size);

		if (hMem)
		{
			LPTSTR dst = (LPTSTR) GlobalLock(hMem);

			/* Copy the text over, adding carriage returns as necessary. */
			for (i = 0; str[i] != '\0'; i++)
			{
				if (str[i] == '\n' && (i == 0 || str[i - 1] != '\r'))
				{
					*dst++ = '\r';
				}

				*dst++ = str[i];
			}

			*dst = '\0';
			GlobalUnlock(hMem);

			EmptyClipboard();

			if (!SetClipboardData(CF_TEXT, hMem))
			{
				CloseClipboard();
				return 0;
			}
		}

		CloseClipboard();
	}

	return 1;
#else
	(void) str;
	return 0;
#endif
}

/**
 * Get the contents of the clipboard.
 * @return Clipboard contents, must be freed. May be NULL in case of
 * failure to acquire the clipboard contents. */
char *clipboard_get(void)
{
	char *result;
#if defined(HAVE_X11)
	Window owner;
	Atom selection, seln_type;
	int seln_format;
	unsigned long nbytes, overflow;
	char *src;

	if (!SDL_Display)
	{
		return NULL;
	}

	owner = XGetSelectionOwner(SDL_Display, XA_PRIMARY);
	result = NULL;

	if (owner == None || owner == SDL_Window)
	{
		owner = DefaultRootWindow(SDL_Display);
		selection = XA_CUT_BUFFER0;
	}
	else
	{
		int selection_response = 0;
		SDL_Event event;

		owner = SDL_Window;
		selection = XInternAtom(SDL_Display, "SDL_SELECTION", False);
		XConvertSelection(SDL_Display, XA_PRIMARY, XA_STRING, selection, owner, CurrentTime);

		while (!selection_response)
		{
			SDL_WaitEvent(&event);

			if (event.type == SDL_SYSWMEVENT)
			{
				XEvent xevent = event.syswm.msg->event.xevent;

				if ((xevent.type == SelectionNotify) && (xevent.xselection.requestor == owner))
				{
					selection_response = 1;
				}
			}
		}
	}

	if (XGetWindowProperty(SDL_Display, owner, selection, 0, INT_MAX / 4, False, XA_STRING, &seln_type, &seln_format, &nbytes, &overflow, (unsigned char **) &src) == Success)
	{
		if (seln_type == XA_STRING)
		{
			result = strdup(src);
		}

		XFree(src);
	}
#elif defined(WIN32)
	if (!SDL_Window)
	{
		return NULL;
	}

	if (IsClipboardFormatAvailable(CF_TEXT) && OpenClipboard(SDL_Window))
	{
		HANDLE hMem;
		char *src;

		hMem = GetClipboardData(CF_TEXT);

		if (hMem)
		{
			src = (char *) GlobalLock(hMem);
			result = strdup(src);
			GlobalUnlock(hMem);
		}

		CloseClipboard();
	}
#else
	result = NULL;
#endif

	return result;
}
