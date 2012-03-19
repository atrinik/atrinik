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
 * Implements buddy type widgets.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Buddy data structure. */
typedef struct buddy_struct
{
	/**
	 * The character names. */
	UT_array *names;

	/**
	 * Path where to load/save the character names. */
	char *path;
} buddy_struct;

#define WIDGET_BUDDY(_widget) ((buddy_struct *) (_widget)->subwidget)

/**
 * Add a buddy.
 * @param widget Widget to add to.
 * @param name Buddy's name. */
void widget_buddy_add(widgetdata *widget, const char *name)
{
	utarray_push_back(WIDGET_BUDDY(widget)->names, &name);
}

/**
 * Remove a buddy.
 * @param widget Widget to remove from.
 * @param name Buddy's name. */
void widget_buddy_remove(widgetdata *widget, const char *name)
{
	ssize_t idx;

	idx = widget_buddy_check(widget, name);

	if (idx != -1)
	{
		utarray_erase(WIDGET_BUDDY(widget)->names, idx, 1);
	}
}

/**
 * Check if the specified character name is a buddy.
 * @param widget Widget to check.
 * @param name Character name to check.
 * @return -1 if the character name is not a buddy, index in the character
 * names array otherwise. */
ssize_t widget_buddy_check(widgetdata *widget, const char *name)
{
	char **p;

	p = NULL;

	while ((p = (char **) utarray_next(WIDGET_BUDDY(widget)->names, p)))
	{
		if (strcmp(*p, name) == 0)
		{
			return utarray_eltidx(WIDGET_BUDDY(widget)->names, p);
		}
	}

	return -1;
}

/**
 * Load the buddy data file.
 * @param widget The buddy widget. */
static void widget_buddy_load(widgetdata *widget)
{
	char buf[MAX_BUF], *end;
	buddy_struct *tmp;
	FILE *fp;

	tmp = WIDGET_BUDDY(widget);

	if (tmp->path)
	{
		return;
	}

	snprintf(buf, sizeof(buf), "%s.dat", widget->id);
	tmp->path = player_make_path(buf);

	fp = fopen_wrapper(tmp->path, "r");

	if (!fp)
	{
		return;
	}

	while (fgets(buf, sizeof(buf), fp))
	{
		end = strchr(buf, '\n');

		if (end)
		{
			*end = '\0';
		}

		widget_buddy_add(widget, buf);
	}

	fclose(fp);
}

/**
 * Save the buddy data file.
 * @param widget The buddy widget. */
static void widget_buddy_save(widgetdata *widget)
{
	buddy_struct *tmp;
	FILE *fp;

	tmp = WIDGET_BUDDY(widget);

	if (!tmp->path)
	{
		return;
	}

	fp = fopen_wrapper(tmp->path, "w");

	if (!fp)
	{
		logger_print(LOG(BUG), "Could not open file for writing: %s", tmp->path);
	}
	else
	{
		char **p;

		p = NULL;

		while ((p = (char **) utarray_next(tmp->names, p)))
		{
			fprintf(fp, "%s\n", *p);
		}

		fclose(fp);
	}

	free(tmp->path);
	tmp->path = NULL;
}

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
	SDL_Rect dstrect;

	if (!widget->surface)
	{
		SDL_Surface *texture;

		texture = TEXTURE_CLIENT("content");
		widget->surface = SDL_ConvertSurface(texture, texture->format, texture->flags);
	}

	if (widget->redraw)
	{
		surface_show(widget->surface, 0, 0, NULL, TEXTURE_CLIENT("content"));
	}

	dstrect.x = widget->x;
	dstrect.y = widget->y;
	SDL_BlitSurface(widget->surface, NULL, ScreenSurface, &dstrect);
}

/** @copydoc widgetdata::background_func */
static void widget_background(widgetdata *widget)
{
	if (cpl.state == ST_PLAY)
	{
		widget_buddy_load(widget);
	}
	else
	{
		widget_buddy_save(widget);
	}
}

/** @copydoc widgetdata::deinit_func */
static void widget_deinit(widgetdata *widget)
{
	widget_buddy_save(widget);
	utarray_free(WIDGET_BUDDY(widget)->names);
}

/**
 * Initialize one buddy widget. */
void widget_buddy_init(widgetdata *widget)
{
	widget->draw_func = widget_draw;
	widget->background_func = widget_background;
	widget->deinit_func = widget_deinit;

	widget->subwidget = calloc(1, sizeof(buddy_struct));
	utarray_new(WIDGET_BUDDY(widget)->names, &ut_str_icd);
}
