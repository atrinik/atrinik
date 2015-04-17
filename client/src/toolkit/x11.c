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
 * X11 API.
 *
 * Clipboard support is based on the works of Sam Lantinga (SDL_scrap) and
 * Eric Wing (SDL_Clipboard). Some X11 logic courtesy of Tomas Styblo (wmctrl).
 *
 * @author Alex Tokar */

#include <global.h>
#include <toolkit_string.h>

TOOLKIT_API(DEPENDS(string));

TOOLKIT_INIT_FUNC(x11)
{
}
TOOLKIT_INIT_FUNC_FINISH

TOOLKIT_DEINIT_FUNC(x11)
{
}
TOOLKIT_DEINIT_FUNC_FINISH

/**
 * Get the parent window.
 * @param display Display.
 * @param win Window.
 * @return Parent window. */
x11_window_type x11_window_get_parent(x11_display_type display, x11_window_type win)
{
#if defined(HAVE_X11)
    Window root, parent, *children;
    unsigned int children_count;

    XQueryTree(display, win, &root, &parent, &children, &children_count);

    return parent;
#else
    return win;
#endif
}

#if defined(HAVE_X11)

/**
 * Sends event to X11.
 * @param display Display to send event to.
 * @param win Window to sent event to.
 * @param msg Event name.
 * @param data0 Data.
 * @param data1 Data.
 * @param data2 Data.
 * @param data3 Data.
 * @param data4 Data.
 * @return 1 on success, 0 on failure.
 * @author Tomas Styblo (wmctrl - GPLv2) */
static int x11_send_event(Display *display, Window win, char *msg, unsigned long data0, unsigned long data1, unsigned long data2, unsigned long data3, unsigned long data4)
{
    XEvent event;
    long mask;
    mask = SubstructureRedirectMask | SubstructureNotifyMask;
    event.xclient.type = ClientMessage;
    event.xclient.serial = 0;
    event.xclient.send_event = True;
    event.xclient.message_type = XInternAtom(display, msg, False);
    event.xclient.window = win;
    event.xclient.format = 32;
    event.xclient.data.l[0] = data0;
    event.xclient.data.l[1] = data1;
    event.xclient.data.l[2] = data2;
    event.xclient.data.l[3] = data3;
    event.xclient.data.l[4] = data4;

    if (!XSendEvent(display, DefaultRootWindow(display), False, mask, &event)) {
        logger_print(LOG(BUG), "Cannot send event: %s", msg);
        return 0;
    }

    return 1;
}

/**
 * Acquires X11 window's property.
 * @param display Display.
 * @param win Window to get the property of.
 * @param xa_prop_type Property type.
 * @param prop_name Property name.
 * @param size If not NULL, will contain the size of the returned property's
 * value.
 * @return The property value.
 * @author Tomas Styblo (wmctrl - GPLv2) */
static char *x11_get_property(Display *display, Window win, Atom xa_prop_type, char *prop_name, unsigned long *size)
{
    Atom xa_prop_name;
    Atom xa_ret_type;
    int ret_format;
    unsigned long ret_nitems;
    unsigned long ret_bytes_after;
    unsigned long tmp_size;
    unsigned char *ret_prop;
    char *ret;

    xa_prop_name = XInternAtom(display, prop_name, False);

    if (XGetWindowProperty(display, win, xa_prop_name, 0, 1024, False, xa_prop_type, &xa_ret_type, &ret_format, &ret_nitems, &ret_bytes_after, &ret_prop) != Success) {
        logger_print(LOG(BUG), "Cannot get property: %s", prop_name);
        return NULL;
    }

    if (xa_ret_type != xa_prop_type) {
        logger_print(LOG(BUG), "Invalid type of property: %s", prop_name);
        XFree(ret_prop);
        return NULL;
    }

    /* Terminate the result to make string handling easier. */
    tmp_size = (ret_format / 8) * ret_nitems;
    ret = emalloc(tmp_size + 1);
    memcpy(ret, ret_prop, tmp_size);
    ret[tmp_size] = '\0';

    if (size) {
        *size = tmp_size;
    }

    XFree(ret_prop);
    return ret;
}

#endif

/**
 * Raises the specified window.
 * @param display Display.
 * @param win Window to raise.
 * @param switch_desktop If 1, will also switch the desktop to that of
 * the window's desktop.
 * @author Tomas Styblo (wmctrl - GPLv2) */
void x11_window_activate(x11_display_type display, x11_window_type win, uint8_t switch_desktop)
{
    TOOLKIT_PROTECT();

#if defined(HAVE_X11)

    if (switch_desktop) {
        char *desktop;

        if (!(desktop = x11_get_property(display, win, XA_CARDINAL, "_NET_WM_DESKTOP", NULL))) {
            if (!(desktop = x11_get_property(display, win, XA_CARDINAL, "_WIN_WORKSPACE", NULL))) {
                logger_print(LOG(BUG), "Cannot find desktop ID of the window.");
            }
        }

        if (desktop) {
            if (!x11_send_event(display, DefaultRootWindow(display), "_NET_CURRENT_DESKTOP", *desktop, 0, 0, 0, 0)) {
                logger_print(LOG(BUG), "Cannot switch desktop.");
            }

            efree(desktop);
        }
    }

    x11_send_event(display, win, "_NET_ACTIVE_WINDOW", 0, 0, 0, 0, 0);
    x11_send_event(display, win, "_NET_WM_STATE_DEMANDS_ATTENTION", 0, 0, 0, 0, 0);
    XMapRaised(display, win);
#elif defined(WIN32)
    SetForegroundWindow(win);
#endif
}

#if defined(HAVE_X11) && defined(HAVE_SDL)

static int x11_clipboard_filter(const SDL_Event *event)
{
    /* Post all non-window manager specific events */
    if (event->type != SDL_SYSWMEVENT) {
        return 1;
    }

    /* Handle window-manager specific clipboard events. */
    switch (event->syswm.msg->event.xevent.type) {
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

        if (XGetWindowProperty(SDL_display, DefaultRootWindow(SDL_display), XA_CUT_BUFFER0, 0, INT_MAX / 4, False, req->target, &sevent.xselection.target, &seln_format, &nbytes, &overflow, &seln_data) == Success) {
            if (sevent.xselection.target == req->target) {
                if (sevent.xselection.target == XA_STRING) {
                    if (seln_data[nbytes - 1] == '\0') {
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
 * Register clipboard events.
 * @return 1 on success, 0 on failure. */
int x11_clipboard_register_events(void)
{
    TOOLKIT_PROTECT();

#if defined(HAVE_X11) && defined(HAVE_SDL)

    if (!SDL_display) {
        return 0;
    }

    /* Enable the special window hook events. */
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
    SDL_SetEventFilter(x11_clipboard_filter);
#endif

    return 1;
}

/**
 * Set the contents of the clipboard.
 * @param display Display.
 * @param win Window.
 * @param str String to set contents of the clipboard to.
 * @return 1 on success, 0 on failure. */
int x11_clipboard_set(x11_display_type display, x11_window_type win, const char *str)
{
    TOOLKIT_PROTECT();

    if (!display) {
        return 0;
    }

#if defined(HAVE_X11)
    XChangeProperty(display, DefaultRootWindow(display), XA_CUT_BUFFER0, XA_STRING, 8, PropModeReplace, (unsigned const char *) str, strlen(str));

    if (XGetSelectionOwner(display, XA_PRIMARY) != win) {
        XSetSelectionOwner(display, XA_PRIMARY, win, CurrentTime);
    }

#ifdef HAVE_X11_XMU

    if (XGetSelectionOwner(display, XA_CLIPBOARD(display)) != win) {
        XSetSelectionOwner(display, XA_CLIPBOARD(display), win, CurrentTime);
    }
#endif

    if (getenv("KDE_FULL_SESSION")) {
        char strbuf[HUGE_BUF * 4], buf[HUGE_BUF * 4];

        string_replace(str, "'", "'\\''", strbuf, sizeof(strbuf));
        snprintf(buf, sizeof(buf), "dbus-send --type=method_call --dest=org.kde.klipper /klipper org.kde.klipper.klipper.setClipboardContents string:'%s'", strbuf);

        if (system(buf) != 0) {
            return 0;
        }
    }

    return 1;
#elif defined(WIN32)

    if (OpenClipboard(win)) {
        SIZE_T i, size;
        HANDLE hMem;

        /* Find out the size of the data. */
        for (size = 0, i = 0; str[i] != '\0'; i++, size++) {
            if (str[i] == '\n' && (i == 0 || str[i - 1] != '\r')) {
                /* We're going to insert a carriage return. */
                size++;
            }
        }

        size += 1;

        hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, size);

        if (hMem) {
            LPTSTR dst = (LPTSTR) GlobalLock(hMem);

            /* Copy the text over, adding carriage returns as necessary. */
            for (i = 0; str[i] != '\0'; i++) {
                if (str[i] == '\n' && (i == 0 || str[i - 1] != '\r')) {
                    *dst++ = '\r';
                }

                *dst++ = str[i];
            }

            *dst = '\0';
            GlobalUnlock(hMem);

            EmptyClipboard();

            if (!SetClipboardData(CF_TEXT, hMem)) {
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
 * @param display Display.
 * @param window Window.
 * @return Clipboard contents, must be freed. May be NULL in case of
 * failure to acquire the clipboard contents. */
char *x11_clipboard_get(x11_display_type display, x11_window_type win)
{
    char *result;
#if defined(HAVE_X11)
    Window owner;
    Atom selection, seln_type;
    int seln_format;
    unsigned long nbytes, overflow;
    char *src;
#endif

    TOOLKIT_PROTECT();

    if (!display) {
        return 0;
    }

    result = NULL;

#if defined(HAVE_X11)
    owner = XGetSelectionOwner(display, XA_PRIMARY);

    if (owner == None || owner == win) {
        owner = DefaultRootWindow(display);
        selection = XA_CUT_BUFFER0;
    } else {
#if defined(HAVE_SDL)
        int selection_response = 0;
        SDL_Event event;
#endif

        owner = win;
        selection = XInternAtom(display, "SDL_SELECTION", False);
        XConvertSelection(display, XA_PRIMARY, XA_STRING, selection, owner, CurrentTime);

#if defined(HAVE_SDL)
        while (!selection_response) {
            SDL_WaitEvent(&event);

            if (event.type == SDL_SYSWMEVENT) {
                XEvent xevent = event.syswm.msg->event.xevent;

                if ((xevent.type == SelectionNotify) && (xevent.xselection.requestor == owner)) {
                    selection_response = 1;
                }
            }
        }
#endif
    }

    if (XGetWindowProperty(display, owner, selection, 0, INT_MAX / 4, False, XA_STRING, &seln_type, &seln_format, &nbytes, &overflow, (unsigned char **) &src) == Success) {
        if (seln_type == XA_STRING) {
            result = estrdup(src);
        }

        XFree(src);
    }
#elif defined(WIN32)

    if (IsClipboardFormatAvailable(CF_TEXT) && OpenClipboard(win)) {
        HANDLE hMem;
        char *src;

        hMem = GetClipboardData(CF_TEXT);

        if (hMem) {
            src = (char *) GlobalLock(hMem);
            result = estrdup(src);
            GlobalUnlock(hMem);
        }

        CloseClipboard();
    }
#endif

    return result;
}
