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
 * X11 API header file.
 *
 * @author Alex Tokar
 */

#ifndef TOOLKIT_X11_H
#define TOOLKIT_X11_H

#include "toolkit.h"

/* Prototypes */

TOOLKIT_FUNCS_DECLARE(x11);

/**
 * Get the parent window.
 *
 * @param display
 * Display.
 * @param win
 * Window.
 * @return
 * Parent window.
 */
extern x11_window_type
x11_window_get_parent(x11_display_type display,
                      x11_window_type  win);

/**
 * Raises the specified window.
 *
 * @param display
 * Display.
 * @param win
 * Window to raise.
 * @param switch_desktop
 * If 1, will also switch the desktop to that of the window's desktop.
 */
extern void
x11_window_activate(x11_display_type display,
                    x11_window_type  win,
                    uint8_t          switch_desktop);

/**
 * Register clipboard events.
 *
 * @return
 * 1 on success, 0 on failure.
 */
extern int
x11_clipboard_register_events(void);

/**
 * Set the contents of the clipboard.
 *
 * @param display
 * Display.
 * @param win
 * Window.
 * @param str
 * String to set contents of the clipboard to.
 * @return
 * 1 on success, 0 on failure.
 */
extern int
x11_clipboard_set(x11_display_type display,
                  x11_window_type  win,
                  const char      *str);

/**
 * Get the contents of the clipboard.
 *
 * @param display
 * Display.
 * @param window
 * Window.
 * @return
 * Clipboard contents, must be freed. May be NULL in case of
 * failure to acquire the clipboard contents.
 */
char *
x11_clipboard_get(x11_display_type display,
                  x11_window_type  win);

#endif
