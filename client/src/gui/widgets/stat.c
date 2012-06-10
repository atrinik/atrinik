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
 * Implements stat type widgets.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
	widget_stat_struct *tmp;

	tmp = (widget_stat_struct *) widget->subwidget;

	if (widget->redraw)
	{
		int curr, max;
		char buf[MAX_BUF];
		SDL_Rect box;

		if (strcmp(widget->id, "health") == 0)
		{
			curr = cpl.stats.hp;
			max = cpl.stats.maxhp;
		}
		else if (strcmp(widget->id, "mana") == 0)
		{
			curr = cpl.stats.sp;
			max = cpl.stats.maxsp;
		}
		else if (strcmp(widget->id, "food") == 0)
		{
			curr = cpl.stats.food;
			max = 999;
		}
		else
		{
			curr = max = 1;
		}

		if (strcmp(tmp->texture, "text") == 0)
		{
			snprintf(buf, sizeof(buf), "%s: %d/%d", widget->id, curr, max);
			string_title(buf);

			box.w = widget->surface->w;
			box.h = widget->surface->h;
			text_show(widget->surface, FONT_ARIAL11, buf, 0, 0, COLOR_WHITE, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);
		}
		else
		{
			SDL_Surface *texture;

			snprintf(buf, sizeof(buf), "stat_%s_back", tmp->texture);
			texture = TEXTURE_CLIENT(buf);
			surface_show(widget->surface, widget->surface->w / 2 - texture->w / 2, widget->surface->h / 2 - texture->h / 2, NULL, texture);

			snprintf(buf, sizeof(buf), "stat_%s_%s", tmp->texture, widget->id);
			texture = TEXTURE_CLIENT(buf);

			box.x = 0;
			box.y = texture->h - texture->h * ((double) curr / (double) max);
			box.w = texture->w;
			box.h = texture->h;

			surface_show(widget->surface, widget->surface->w / 2 - texture->w / 2, widget->surface->h / 2 - texture->h / 2 + box.y, &box, texture);

			snprintf(buf, sizeof(buf), "stat_%s", tmp->texture);
			texture = TEXTURE_CLIENT(buf);
			surface_show(widget->surface, widget->surface->w / 2 - texture->w / 2, widget->surface->h / 2 - texture->h / 2, NULL, texture);
		}
	}
}

/** @copydoc widgetdata::deinit_func */
static void widget_deinit(widgetdata *widget)
{
	widget_stat_struct *tmp;

	tmp = (widget_stat_struct *) widget->subwidget;

	free(tmp->texture);
}

/** @copydoc widgetdata::load_func */
static int widget_load(widgetdata *widget, const char *keyword, const char *parameter)
{
	widget_stat_struct *tmp;

	tmp = (widget_stat_struct *) widget->subwidget;

	if (strcmp(keyword, "texture") == 0)
	{
		tmp->texture = strdup(parameter);
		return 1;
	}

	return 0;
}

/** @copydoc widgetdata::save_func */
static void widget_save(widgetdata *widget, FILE *fp, const char *padding)
{
	widget_stat_struct *tmp;

	tmp = (widget_stat_struct *) widget->subwidget;

	fprintf(fp, "%stexture = %s\n", padding, tmp->texture);
}

/**
 * Initialize one stat widget. */
void widget_stat_init(widgetdata *widget)
{
	widget_stat_struct *tmp;

	tmp = calloc(1, sizeof(*tmp));

	widget->draw_func = widget_draw;
	widget->deinit_func = widget_deinit;
	widget->load_func = widget_load;
	widget->save_func = widget_save;
	widget->subwidget = tmp;
}
