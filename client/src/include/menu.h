/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
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
 * Menus header file.
 */

#ifndef MENU_H
#define MENU_H

/** Maximum quickslots in a single group. */
#define MAX_QUICK_SLOTS 8
/** Maximum quickslot groups. */
#define MAX_QUICKSLOT_GROUPS 4

/** Public API implemented in src/client/menu.c. */

extern int client_command_check(const char *cmd);

extern int send_command_check(const char *cmd);

/** Public API implemented in src/gui/widgets/menu.c. */

extern void widget_highlight_menu(widgetdata *widget);

/** Public API implemented in src/gui/widgets/menu_buttons.c. */

extern void widget_menu_buttons_init(widgetdata *widget);

/** Public API implemented in src/gui/widgets/quickslots.c. */

extern void quickslots_init(void);

extern void quickslots_scroll(widgetdata *widget, int up, int scroll);

extern void quickslots_cycle(widgetdata *widget);

extern void quickslots_handle_key(int slot);

extern void widget_quickslots_init(widgetdata *widget);

extern void socket_command_quickslots(uint8_t *data, size_t len, size_t pos);

#endif
