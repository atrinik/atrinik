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
 * Header file for progress indicators. */

#ifndef PROGRESS_H
#define PROGRESS_H

/**
 * Dots progress indicator. */
typedef struct progress_dots
{
    /** Last time when one of the dots was lit. */
    uint32 ticks;

    /** Which dot is currently lit. */
    uint8 dot;

    /** Whether the progress is done. */
    uint8 done;
} progress_dots;

/** Number of progress dots shown. */
#define PROGRESS_DOTS_NUM (5)
/** Spacing between the dots. */
#define PROGRESS_DOTS_SPACING (2)
/** How often to advance the progress dots, in ticks. */
#define PROGRESS_DOTS_TICKS (275)

#endif
