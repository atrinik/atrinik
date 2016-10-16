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
 * Colorspace API header file.
 *
 * @author Alex Tokar
 */

#ifndef TOOLKIT_COLORSPACE_H
#define TOOLKIT_COLORSPACE_H

#include "toolkit.h"

/* Prototypes */

TOOLKIT_FUNCS_DECLARE(colorspace);

/**
 * Return the biggest color value in the specified RGB array.
 *
 * @param rgb
 * RGB array.
 * @return
 * Biggest color value.
 */
extern double
colorspace_rgb_max(const double rgb[3]);

/**
 * Return the smallest color value in the specified RGB array.
 *
 * @param rgb
 * RGB array.
 * @return
 * Smallest color value.
 */
extern double
colorspace_rgb_min(const double rgb[3]);

/**
 * Converts RGB (red,green,blue) colorspace to HSV (hue,saturation,value).
 *
 * @param rgb
 * RGB array.
 * @param[out] hsv
 * HSV array.
 */
extern void
colorspace_rgb2hsv(const double rgb[3], double hsv[3]);

/**
 * Converts HSV (hue,saturation,value) colorspace to RGB (red,green,blue).
 *
 * @param hsv
 * HSV array.
 * @param[out] rgb
 * RGB array.
 */
extern void
colorspace_hsv2rgb(const double hsv[3], double rgb[3]);

#endif
