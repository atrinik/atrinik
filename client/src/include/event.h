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
 * Event related header file. */

#ifndef EVENT_H
#define EVENT_H

enum
{
    DRAG_GET_STATUS = -1,
    DRAG_NONE,
    DRAG_QUICKSLOT,
    DRAG_QUICKSLOT_SPELL
};

/**
 * Key information. */
typedef struct key_struct
{
    /** If 1, the key is pressed. */
    uint8 pressed;

    /** Last repeat time. */
    uint32 time;

    /** Whether the key is being repeated. */
    uint8 repeated;
} key_struct;

#define EVENT_IS_MOUSE(_event) ((_event)->type == SDL_MOUSEBUTTONDOWN || (_event)->type == SDL_MOUSEBUTTONUP || (_event)->type == SDL_MOUSEMOTION)
#define EVENT_IS_KEY(_event) ((_event)->type == SDL_KEYDOWN || (_event)->type == SDL_KEYUP)

#endif
