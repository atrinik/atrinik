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
 * Implements container type widgets.
 *
 * @author Alex Tokar
 * @author Daniel Liptrot */

#include <global.h>

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
	/* Special case, menuitem is highlighted when mouse is moved over it. */
	if (widget->sub_type == MENU_ID)
	{
		widget_highlight_menu(widget);
	}

}

/** @copydoc widgetdata::event_func */
static int widget_event(widgetdata *widget, SDL_Event *event)
{
	if (widget->sub_type == MENUITEM_ID && event->type == SDL_MOUSEBUTTONDOWN)
	{
		_menuitem *menuitem;

		if (widget->env->sub_type != MENU_ID)
		{
			return 0;
		}

		menuitem = MENUITEM(widget);
		widget_mouse_event.owner = (MENU(widget->env))->owner;

		if (widget_mouse_event.owner && menuitem->menu_type != MENU_SUBMENU && menuitem->menu_func_ptr)
		{
			if (menuitem->menu_type == MENU_RADIO && menuitem->val != 0)
			{
				return 1;
			}
			
			menuitem->menu_func_ptr(widget_mouse_event.owner, widget, event);
		}

		return 1;
	}

	return 0;
}

/** @copydoc widgetdata::load_func */
static int widget_load(widgetdata *widget, const char *keyword, const char *parameter)
{
	_widget_container *container;

	container = CONTAINER(widget);

	if (strcmp(keyword, "padding") == 0)
	{
		int padding[5];

		if (sscanf(parameter, "%d %d %d %d %d", &padding[0], &padding[1], &padding[2], &padding[3], &padding[4]) >= 4)
		{
			if (widget->sub_type == CONTAINER_STRIP_ID || widget->sub_type == MENU_ID || widget->sub_type == MENUITEM_ID)
			{
				_widget_container_strip *container_strip;

				container_strip = CONTAINER_STRIP(widget);
				container_strip->inner_padding = padding[4];
			}

			container->outer_padding_top = padding[0];
			container->outer_padding_right = padding[1];
			container->outer_padding_bottom = padding[2];
			container->outer_padding_left = padding[3];

			return 1;
		}
	}

	return 0;
}

/** @copydoc widgetdata::save_func */
static void widget_save(widgetdata *widget, FILE *fp, const char *padding)
{
	_widget_container *container;

	container = CONTAINER(widget);

	fprintf(fp, "%spadding = %d %d %d %d", padding, container->outer_padding_top, container->outer_padding_right, container->outer_padding_bottom, container->outer_padding_left);

	if (widget->sub_type == CONTAINER_STRIP_ID || widget->sub_type == MENU_ID || widget->sub_type == MENUITEM_ID)
	{
		_widget_container_strip *container_strip;

		container_strip = CONTAINER_STRIP(widget);
		fprintf(fp, " %d", container_strip->inner_padding);
	}

	fprintf(fp, "\n");
}

/**
 * Initialize one container widget.
 * @param widget Widget to initialize. */
void widget_container_init(widgetdata *widget)
{
	_widget_container *container;

	container = calloc(1, sizeof(_widget_container));

	if (!container)
	{
		logger_print(LOG(ERROR), "OOM.");
		exit(-1);
	}

	container->widget_type = -1;

	widget->draw_func = widget_draw;
	widget->event_func = widget_event;
	widget->load_func = widget_load;
	widget->save_func = widget_save;
	widget->subwidget = container;

	if (widget->sub_type == CONTAINER_STRIP_ID || widget->sub_type == MENU_ID || widget->sub_type == MENUITEM_ID)
	{
		_widget_container_strip *container_strip;

		container_strip = calloc(1, sizeof(_widget_container_strip));

		if (!container_strip)
		{
			logger_print(LOG(ERROR), "OOM.");
			exit(-1);
		}

		container_strip->inner_padding = 10;

		container->subcontainer = container_strip;

		if (widget->sub_type == MENU_ID)
		{
			_menu *menu;

			menu = calloc(1, sizeof(_menu));

			if (!menu)
			{
				logger_print(LOG(ERROR), "OOM.");
				exit(-1);
			}

			container_strip->subcontainer_strip = menu;
		}
		else if (widget->sub_type == MENUITEM_ID)
		{
			_menuitem *menuitem;

			menuitem = malloc(sizeof(_menuitem));

			if (!menuitem)
			{
				logger_print(LOG(ERROR), "OOM.");
				exit(-1);
			}

			menuitem->menu_type = MENU_NORMAL;

			container_strip->subcontainer_strip = menuitem;
		}
	}
}
