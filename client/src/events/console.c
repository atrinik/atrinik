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
 *  */

#include <global.h>

/**
 * Handle mouse hold events in number input widget. */
void mouse_InputNumber(void)
{
	static int timeVal = 1;
	int x, y;

	if (!(SDL_GetMouseState(&x, &y) & SDL_BUTTON(SDL_BUTTON_LEFT)))
	{
		timeVal = 1;
		return;
	}

	x = x - cur_widget[IN_NUMBER_ID]->x1;
	y = y - cur_widget[IN_NUMBER_ID]->y1;

	if (x < 230 || x > 237 || y < 5)
	{
		return;
	}

	/* Plus */
	if (y > 13)
	{
		x = atoi(text_input_string) + timeVal;

		if (x > cpl.nrof)
		{
			x = cpl.nrof;
		}
	}
	/* Minus */
	else
	{
		x = atoi(text_input_string) - timeVal;

		if (x < 1)
		{
			x = 1;
		}
	}

	snprintf(text_input_string, sizeof(text_input_string), "%d", x);
	text_input_count = (int) strlen(text_input_string);
	timeVal += (timeVal / 8) + 1;
}
