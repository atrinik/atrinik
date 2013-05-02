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
 * Implements active effects type widgets.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Active effects widget data. */
typedef struct widget_active_effects_struct
{
	list_struct *list;
} widget_active_effects_struct;

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{

}

void widget_active_effects_add(widgetdata *widget, object *op, sint32 sec, const char *msg)
{
	if (!(op->flags & CS_FLAG_APPLIED))
	{
		return;
	}
	
}

void widget_active_effects_remove(widgetdata *widget, object *op)
{
}

/**
 * Initialize one active effects widget. */
void widget_active_effects_init(widgetdata *widget)
{
	widget_active_effects_struct *tmp;
	uint32 i;

	tmp = calloc(1, sizeof(*tmp));
	tmp->list = list_create(1, 8, 0);
	tmp->list->row_selected_func = NULL;
	tmp->list->row_highlight_func = NULL;
	tmp->list->row_height_adjust = INVENTORY_ICON_SIZE;
	tmp->list->header_height = tmp->list->frame_offset = 0;
	list_set_font(tmp->list, -1);
	list_scrollbar_enable(tmp->list);

	for (i = 0; i < tmp->list->cols; i++)
	{
		list_set_column(tmp->list, i, INVENTORY_ICON_SIZE, 0, NULL, -1);
	}

	widget->draw_func = widget_draw;
	widget->subwidget = tmp;
}
