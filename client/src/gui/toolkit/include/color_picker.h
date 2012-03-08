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
 * Color picker API header file.
 *
 * @author Alex Tokar */

#ifndef COLOR_PICKER_H
#define COLOR_PICKER_H

enum
{
	/**
	 * The color chooser. */
	COLOR_PICKER_ELEM_COLOR,
	/**
	 * Hue chooser. */
	COLOR_PICKER_ELEM_HUE,

	/**
	 * Number of elements. */
	COLOR_PICKER_ELEM_NUM
};

/**
 * One color picker element. */
typedef struct color_picker_element_struct
{
	/**
	 * Dimensions. */
	SDL_Rect coords;

	/**
	 * If 1, this element is being dragged. */
	uint8 dragging;
} color_picker_element_struct;

/**
 * Color picker structure. */
typedef struct color_picker_struct
{
	/**
	 * X position of the color picker. */
	int x;

	/**
	 * Y position of the color picker. */
	int y;

	/**
	 * X position of color picker's parent. */
	int px;

	/**
	 * Y position of color picker's parent. */
	int py;

	/**
	 * The elements. */
	color_picker_element_struct elements[COLOR_PICKER_ELEM_NUM];

	/**
	 * Thickness of the border; 0 for none. */
	uint8 border_thickness;

	/**
	 * Which color is currently selected, in HSV (hue,saturation,value)
	 * colorspace. */
	double hsv[3];
} color_picker_struct;

#endif
