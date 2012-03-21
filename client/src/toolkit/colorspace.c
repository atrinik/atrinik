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
 * Colorspace related API.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Name of the API. */
#define API_NAME colorspace

/**
 * If 1, the API has been initialized. */
static uint8 init = 0;

/**
 * Initialize the colorspace API.
 * @internal */
void toolkit_colorspace_init(void)
{
	TOOLKIT_INIT_FUNC_START(colorspace)
	{
	}
	TOOLKIT_INIT_FUNC_END()
}

/**
 * Deinitialize the colorspace API.
 * @internal */
void toolkit_colorspace_deinit(void)
{
	init = 0;
}

/**
 * @author LIBGIMP (GNU LGPL 3.0) */
double colorspace_rgb_max(const double rgb[3])
{
	TOOLKIT_FUNC_PROTECTOR(API_NAME);

	if (rgb[0] > rgb[1])
	{
		return (rgb[0] > rgb[2]) ? rgb[0] : rgb[2];
	}
	else
	{
		return (rgb[1] > rgb[2]) ? rgb[1] : rgb[2];
	}
}

/**
 * @author LIBGIMP (GNU LGPL 3.0) */
double colorspace_rgb_min(const double rgb[3])
{
	TOOLKIT_FUNC_PROTECTOR(API_NAME);

	if (rgb[0] < rgb[1])
	{
		return (rgb[0] < rgb[2]) ? rgb[0] : rgb[2];
	}
	else
	{
		return (rgb[1] < rgb[2]) ? rgb[1] : rgb[2];
	}
}

/**
 * Converts RGB (red,green,blue) colorspace to HSV (hue,saturation,value).
 * @author LIBGIMP (GNU LGPL 3.0) */
void colorspace_rgb2hsv(const double rgb[3], double hsv[3])
{
	double max, min, delta;

	TOOLKIT_FUNC_PROTECTOR(API_NAME);

	max = colorspace_rgb_max(rgb);
	min = colorspace_rgb_min(rgb);

	hsv[2] = max;
	delta = max - min;

	if (delta > 0.0001)
	{
		hsv[1] = delta / max;

		if (rgb[0] == max)
		{
			hsv[0] = (rgb[1] - rgb[2]) / delta;

			if (hsv[0] < 0.0)
			{
				hsv[0] += 6.0;
			}
		}
		else if (rgb[1] == max)
		{
			hsv[0] = 2.0 + (rgb[2] - rgb[0]) / delta;
		}
		else if (rgb[2] == max)
		{
			hsv[0] = 4.0 + (rgb[0] - rgb[1]) / delta;
		}

		hsv[0] /= 6.0;
	}
	else
	{
		hsv[0] = hsv[1] = 0.0;
	}
}

/**
 * Converts HSV (hue,saturation,value) colorspace to RGB (red,green,blue).
 * @author LIBGIMP (GNU LGPL 3.0) */
void colorspace_hsv2rgb(const double hsv[3], double rgb[3])
{
	int i;
	double f, w, q, t;
	double hue;

	TOOLKIT_FUNC_PROTECTOR(API_NAME);

	if (hsv[1] == 0.0)
	{
		rgb[0] = hsv[2];
		rgb[1] = hsv[2];
		rgb[2] = hsv[2];
	}
	else
	{
		hue = hsv[0];

		if (hue == 1.0)
		{
			hue = 0.0;
		}

		hue *= 6.0;

		i = (int) hue;
		f = hue - i;
		w = hsv[2] * (1.0 - hsv[1]);
		q = hsv[2] * (1.0 - (hsv[1] * f));
		t = hsv[2] * (1.0 - (hsv[1] * (1.0 - f)));

		switch (i)
		{
			case 0:
				rgb[0] = hsv[2];
				rgb[1] = t;
				rgb[2] = w;
				break;

			case 1:
				rgb[0] = q;
				rgb[1] = hsv[2];
				rgb[2] = w;
				break;

			case 2:
				rgb[0] = w;
				rgb[1] = hsv[2];
				rgb[2] = t;
				break;

			case 3:
				rgb[0] = w;
				rgb[1] = q;
				rgb[2] = hsv[2];
				break;

			case 4:
				rgb[0] = t;
				rgb[1] = w;
				rgb[2] = hsv[2];
				break;

			case 5:
				rgb[0] = hsv[2];
				rgb[1] = w;
				rgb[2] = q;
				break;
		}
	}
}
