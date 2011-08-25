/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
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
 * Implements the interface used by NPCs and the like. */

#include <global.h>

void cmd_interface(uint8 *data, int len)
{
	int pos = 0;
	interface_struct *interface;
	uint8 text_input = 0;
	char type, text_input_content[HUGE_BUF];

	interface = calloc(1, sizeof(*interface));
	utarray_new(interface->links, &ut_str_icd);

	while (pos < len)
	{
		type = data[pos++];

		switch (type)
		{
			case CMD_INTERFACE_TEXT:
			{
				char message[HUGE_BUF * 8];

				GetString_String(data, &pos, message, sizeof(message));
				interface->message = strdup(message);
				break;
			}

			case CMD_INTERFACE_LINK:
			{
				char interface_link[HUGE_BUF];

				GetString_String(data, &pos, interface_link, sizeof(interface_link));
				utarray_push_back(interface->links, &interface_link);
				break;
			}

			case CMD_INTERFACE_ICON:
			{
				char icon[MAX_BUF];

				GetString_String(data, &pos, icon, sizeof(icon));
				interface->icon = get_bmap_id(icon);
				break;
			}

			case CMD_INTERFACE_TITLE:
			{
				char title[HUGE_BUF];

				GetString_String(data, &pos, title, sizeof(title));
				interface->title = strdup(title);
				break;
			}

			case CMD_INTERFACE_INPUT:
				text_input = 1;
				GetString_String(data, &pos, text_input_content, sizeof(text_input_content));
				break;

			default:
				break;
		}
	}

	if (text_input)
	{
	}
}
