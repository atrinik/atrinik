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

	/**
	 * Button buffer. */
	button_struct button_close, button_help, button_add;

	/**
	 * The list. */
	list_struct *list;

	/**
	 * Text input buffer. */
	text_input_struct text_input;
} buddy_struct;

#define WIDGET_BUDDY(_widget) ((buddy_struct *) (_widget)->subwidget)

/** @copydoc list_struct::handle_enter_func */
static void list_handle_enter(list_struct *list)
{
	widgetdata *widget;

	widget = widget_find_by_surface(list->surface);

	if (strcmp(widget->id, "buddy") == 0)
	{
		textwin_tab_open(widget_find_by_type(CHATWIN_ID), list->text[list->row_selected - 1][0]);
	}
}

/**
 * Add a buddy.
 * @param widget Widget to add to.
 * @param name Buddy's name. Can be NULL, in which case nothing is added.
 * @param sort If 1, sort the buddy list. */
void widget_buddy_add(widgetdata *widget, const char *name, uint8 sort)
{
	buddy_struct *tmp;

	tmp = WIDGET_BUDDY(widget);

	if (name)
	{
		utarray_push_back(tmp->names, &name);
		list_add(tmp->list, tmp->list->rows, 0, name);
	}

	if (sort)
	{
		list_sort(tmp->list, LIST_SORT_ALPHA);
	}

	widget->redraw = 1;
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
		buddy_struct *tmp;
		uint32 row;

		tmp = WIDGET_BUDDY(widget);
		utarray_erase(tmp->names, idx, 1);

		for (row = 0; row < tmp->list->rows; row++)
		{
			if (strcmp(tmp->list->text[row][0], name) == 0)
			{
				list_remove_row(tmp->list, row);
				break;
			}
		}

		widget->redraw = 1;
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

		widget_buddy_add(widget, buf, 0);
	}

	fclose(fp);
	widget_buddy_add(widget, NULL, 1);
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
	buddy_struct *tmp;
	SDL_Rect dstrect;

	tmp = WIDGET_BUDDY(widget);

	if (!widget->surface)
	{
		SDL_Surface *texture;

		texture = TEXTURE_CLIENT("content");
		widget->surface = SDL_ConvertSurface(texture, texture->format, texture->flags);
	}

	if (widget->redraw)
	{
		SDL_Rect box;
		char buf[MAX_BUF];

		surface_show(widget->surface, 0, 0, NULL, TEXTURE_CLIENT("content"));

		box.w = widget->w;
		box.h = 0;
		snprintf(buf, sizeof(buf), "%s list", widget->id);
		string_title(buf);
		text_show(widget->surface, FONT_SERIF12, buf, 0, 3, COLOR_HGOLD, TEXT_ALIGN_CENTER, &box);

		tmp->list->surface = widget->surface;
		list_set_parent(tmp->list, widget->x, widget->y);
		list_show(tmp->list, 10, 2);

		widget->redraw = list_need_redraw(tmp->list);
	}

	dstrect.x = widget->x;
	dstrect.y = widget->y;
	SDL_BlitSurface(widget->surface, NULL, ScreenSurface, &dstrect);

	/* Show close button. */
	tmp->button_close.x = widget->x + widget->w - TEXTURE_SURFACE(tmp->button_close.texture)->w - 4;
	tmp->button_close.y = widget->y + 4;
	button_show(&tmp->button_close, "X");

	/* Show help button. */
	tmp->button_help.x = widget->x + widget->w - TEXTURE_SURFACE(tmp->button_close.texture)->w - TEXTURE_SURFACE(tmp->button_help.texture)->w - 4;
	tmp->button_help.y = widget->y + 4;
	button_show(&tmp->button_help, "?");

	text_input_show(&tmp->text_input, ScreenSurface, widget->x + 160, widget->y + 100);

	tmp->button_add.x = widget->x + 160;
	tmp->button_add.y = widget->y + 130;
	button_show(&tmp->button_add, "Add");
}

/** @copydoc widgetdata::event_func */
static int widget_event(widgetdata *widget, SDL_Event *event)
{
	buddy_struct *tmp;

	tmp = WIDGET_BUDDY(widget);

	if (list_handle_mouse(tmp->list, event))
	{
		widget->redraw = 1;
		return 1;
	}

	if (button_event(&tmp->button_close, event))
	{
		widget->show = 0;
		return 1;
	}
	else if (button_event(&tmp->button_help, event))
	{
		char buf[MAX_BUF];

		snprintf(buf, sizeof(buf), "%s list", widget->id);
		help_show(buf);
		return 1;
	}
	else if (button_event(&tmp->button_add, event) || (tmp->text_input.focus && event->type == SDL_KEYDOWN && IS_ENTER(event->key.keysym.sym)))
	{
		if (tmp->text_input.focus)
		{
			if (*tmp->text_input.str != '\0')
			{
				widget_buddy_add(widget, tmp->text_input.str, 1);
				text_input_set(&tmp->text_input, NULL);
			}

			tmp->text_input.focus = 0;
		}
		else
		{
			tmp->text_input.focus = 1;
		}

		return 1;
	}
	else if (text_input_event(&tmp->text_input, event))
	{
		return 1;
	}

	if (event->type == SDL_MOUSEBUTTONDOWN)
	{
		if (event->button.button == SDL_BUTTON_LEFT && text_input_mouse_over(&tmp->text_input, event->motion.x, event->motion.y))
		{
			tmp->text_input.focus = 1;
			return 1;
		}

		tmp->text_input.focus = 0;
	}

	if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE)
	{
		tmp->text_input.focus = 0;
		return 1;
	}

	return 0;
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
	buddy_struct *tmp;

	widget_buddy_save(widget);

	tmp = WIDGET_BUDDY(widget);
	utarray_free(tmp->names);
	list_remove(tmp->list);
}

/**
 * Initialize one buddy widget. */
void widget_buddy_init(widgetdata *widget)
{
	buddy_struct *tmp;

	widget->draw_func = widget_draw;
	widget->event_func = widget_event;
	widget->background_func = widget_background;
	widget->deinit_func = widget_deinit;

	widget->subwidget = tmp = calloc(1, sizeof(*tmp));
	utarray_new(tmp->names, &ut_str_icd);

	/* Create the list and set up settings. */
	tmp->list = list_create(12, 1, 8);
	tmp->list->handle_enter_func = list_handle_enter;
	list_scrollbar_enable(tmp->list);
	list_set_column(tmp->list, 0, 130, 7, NULL, -1);
	list_set_font(tmp->list, FONT_ARIAL10);

	button_create(&tmp->button_close);
	button_create(&tmp->button_help);
	button_create(&tmp->button_add);
	tmp->button_close.texture = tmp->button_help.texture = texture_get(TEXTURE_TYPE_CLIENT, "button_round");
	tmp->button_close.texture_pressed = tmp->button_help.texture_pressed = texture_get(TEXTURE_TYPE_CLIENT, "button_round_down");
	tmp->button_close.texture_over = tmp->button_help.texture_over = texture_get(TEXTURE_TYPE_CLIENT, "button_round_over");

	text_input_create(&tmp->text_input);
	tmp->text_input.focus = 0;
	tmp->text_input.w = 60;
}
