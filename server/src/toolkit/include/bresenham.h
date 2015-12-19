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
 * Bresenham algorithm functions.
 */

#ifndef BRESENHAM_H
#define	BRESENHAM_H

/**
 * @defgroup bresenham_algorithm Bresenham algorithm
 * Bresenham line drawing algorithm. Implemented as macros for
 * flexibility and speed.
 *@{
 */

/**
 * Bresenham init.
 * dx & dy are input only and will not be changed.
 * All other parameters are the outputs which will be initialized
 */
#define BRESENHAM_INIT(dx, dy, fraction, stepx, stepy, dx2, dy2)      \
    {                                                                 \
        (dx2) = (dx) << 1;                                            \
        (dy2) = (dy) << 1;                                            \
                                                                      \
        if ((dy) < 0) {                                               \
            (dy2) = -(dy2);                                           \
            (stepy) = -1;                                             \
        } else {                                                      \
            (stepy) = 1;                                              \
        }                                                             \
                                                                      \
        if ((dx) < 0) {                                               \
            (dx2) = -(dx2);                                           \
            (stepx) = -1;                                             \
        } else {                                                      \
            (stepx) = 1;                                              \
        }                                                             \
                                                                      \
        if ((dx2) > (dy2)) {                                          \
            (fraction) = (dy2) - (dx) * (stepx);                      \
        } else {                                                      \
            (fraction) = (dx2) - (dy) * (stepy);                      \
        }                                                             \
    }

/**
 * Bresenham line stepping macro.
 * x, y are input-output and will be always be changed
 * fraction is also input-output, but should be initialized with
 * BRESENHAM_INIT.
 * stepx, stepy, dx2 and dy2 are input only and should also
 * be initialized by BRESENHAM_INIT
 */
#define BRESENHAM_STEP(x, y, fraction, stepx, stepy, dx2, dy2)        \
    if ((dx2) > (dy2)) {                                              \
        if ((fraction) >= 0) {                                        \
            (y) += (stepy);                                           \
            (fraction) -= (dx2);                                      \
        }                                                             \
                                                                      \
        (x) += (stepx);                                               \
        (fraction) += (dy2);                                          \
    } else {                                                          \
        if ((fraction) >= 0) {                                        \
            (x) += (stepx);                                           \
            (fraction) -= (dy2);                                      \
        }                                                             \
                                                                      \
        (y) += (stepy);                                               \
        (fraction) += (dx2);                                          \
    }
/*@}*/

#endif
