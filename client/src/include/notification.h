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
 * Notification header file. */

#ifndef NOTIFICATION_H
#define NOTIFICATION_H

/**
 * The notification data. */
typedef struct notification_struct {
    /** Current alpha value of the notification surface. */
    int alpha;

    /** Macro or command to execute when the notification is clicked. */
    char *action;

    /** Macro that is temporarily bound to the notification's action. */
    char *shortcut;

    /** When the notification was created. */
    uint32 start_ticks;

    /**
     * Milliseconds that must pass before the notification is
     * dismissed. */
    uint32 delay;
} notification_struct;

/**
 * @defgroup CMD_NOTIFICATION_xxx Notification command types
 * Notification command types.
 *@{*/
/** The notification contents. */
#define CMD_NOTIFICATION_TEXT 0
/** What macro or command to execute. */
#define CMD_NOTIFICATION_ACTION 1
/** Macro temporarily assigned to this notification. */
#define CMD_NOTIFICATION_SHORTCUT 2
/**
 * How many milliseconds must pass before the notification is
 * dismissed. */
#define CMD_NOTIFICATION_DELAY 3
/*@}*/

/**
 * @defgroup NOTIFICATION_DEFAULT_xxx Notification defaults
 * Default notification values.
 *@{*/
/** The maximum width of the notification. */
#define NOTIFICATION_DEFAULT_WIDTH 200
/** The font of the notification's message. */
#define NOTIFICATION_DEFAULT_FONT FONT_ARIAL11
/** Milliseconds before dismissal. */
#define NOTIFICATION_DEFAULT_DELAY (30 * 1000)
/** How many seconds before the dismissal to start the fadeout. */
#define NOTIFICATION_DEFAULT_FADEOUT (5 * 1000)
/*@}*/

#endif
