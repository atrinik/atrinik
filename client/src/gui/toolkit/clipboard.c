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
 * Implements clipboard handling.
 *
 * Based on the works of Sam Lantinga (SDL_scrap) and Eric Wing (SDL_Clipboard).
 *
 * @author Alex Tokar */

#include <global.h>

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

			if (XGetWindowProperty(SDL_display, DefaultRootWindow(SDL_display), XA_CUT_BUFFER0, 0, INT_MAX / 4, False, req->target, &sevent.xselection.target, &seln_format, &nbytes, &overflow, &seln_data) == Success)
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

					XChangeProperty(SDL_display, req->requestor, req->property, sevent.xselection.target, seln_format, PropModeReplace, seln_data, nbytes);
					sevent.xselection.property = req->property;
				}

				XFree(seln_data);
			}

			XSendEvent(SDL_display, req->requestor, False, 0, &sevent);
			XSync(SDL_display, False);
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
	if (!SDL_display)
	{
		return 0;
	}

#if defined(HAVE_X11)
	/* Enable the special window hook events. */
	SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
	SDL_SetEventFilter(clipboard_filter);
#endif

	return 1;
}

/**
 * Set the contents of the clipboard.
 * @param str String to set contents of the clipboard to.
 * @return 1 on success, 0 on failure. */
int clipboard_set(const char *str)
{
	if (!SDL_display)
	{
		return 0;
	}

#if defined(HAVE_X11)
	XChangeProperty(SDL_display, DefaultRootWindow(SDL_display), XA_CUT_BUFFER0, XA_STRING, 8, PropModeReplace, (unsigned const char *) str, strlen(str));

	if (XGetSelectionOwner(SDL_display, XA_PRIMARY) != SDL_window)
	{
		XSetSelectionOwner(SDL_display, XA_PRIMARY, SDL_window, CurrentTime);
	}

#ifdef HAVE_X11_XMU
	if (XGetSelectionOwner(SDL_display, XA_CLIPBOARD(SDL_display)) != SDL_window)
	{
		XSetSelectionOwner(SDL_display, XA_CLIPBOARD(SDL_display), SDL_window, CurrentTime);
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
	if (OpenClipboard(SDL_window))
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
#endif

	if (!SDL_display)
	{
		return 0;
	}

	result = NULL;

#if defined(HAVE_X11)
	owner = XGetSelectionOwner(SDL_display, XA_PRIMARY);

	if (owner == None || owner == SDL_window)
	{
		owner = DefaultRootWindow(SDL_display);
		selection = XA_CUT_BUFFER0;
	}
	else
	{
		int selection_response = 0;
		SDL_Event event;

		owner = SDL_window;
		selection = XInternAtom(SDL_display, "SDL_SELECTION", False);
		XConvertSelection(SDL_display, XA_PRIMARY, XA_STRING, selection, owner, CurrentTime);

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

	if (XGetWindowProperty(SDL_display, owner, selection, 0, INT_MAX / 4, False, XA_STRING, &seln_type, &seln_format, &nbytes, &overflow, (unsigned char **) &src) == Success)
	{
		if (seln_type == XA_STRING)
		{
			result = strdup(src);
		}

		XFree(src);
	}
#elif defined(WIN32)
	if (IsClipboardFormatAvailable(CF_TEXT) && OpenClipboard(SDL_window))
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
#endif

	return result;
}
