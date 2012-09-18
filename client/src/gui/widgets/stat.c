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

/**
 * Possible display modes of the stat widget. */
static const char *const display_modes[] = {
	"Sphere", "Bar", "Text"
};

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
		else if (strcmp(widget->id, "exp") == 0)
		{
			curr = cpl.stats.exp - s_settings->level_exp[cpl.stats.level];
			max = s_settings->level_exp[cpl.stats.level + 1] - s_settings->level_exp[cpl.stats.level];
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
		else if (strcmp(tmp->texture, "sphere") == 0)
		{
			box.x = 0;
			box.y = 0;
			box.w = widget->w - WIDGET_BORDER_SIZE * 2;
			box.h = widget->h - WIDGET_BORDER_SIZE * 2;
			text_show_format(widget->surface, FONT_ARIAL11, WIDGET_BORDER_SIZE, WIDGET_BORDER_SIZE, COLOR_WHITE, TEXT_MARKUP, NULL, "<icon=stat_sphere_back %d %d 1>", widget->w - WIDGET_BORDER_SIZE * 2, widget->h - WIDGET_BORDER_SIZE * 2);
			text_show_format(widget->surface, FONT_ARIAL11, WIDGET_BORDER_SIZE + 2, WIDGET_BORDER_SIZE + 2, COLOR_WHITE, TEXT_MARKUP, NULL, "<icon=stat_sphere_%s %d %d 1 0 %f>", widget->id, widget->w - WIDGET_BORDER_SIZE * 2 - 2 * 2, widget->h - WIDGET_BORDER_SIZE * 2 - 2 * 2, 4.0 + ((double) curr / (double) max));
			text_show_format(widget->surface, FONT_ARIAL11, WIDGET_BORDER_SIZE, WIDGET_BORDER_SIZE, COLOR_WHITE, TEXT_MARKUP, &box, "<icon=stat_sphere %d %d 1>", widget->w - WIDGET_BORDER_SIZE * 2, widget->h - WIDGET_BORDER_SIZE * 2);
		}
		else
		{
			int thickness;

			thickness = (double) MIN(widget->w, widget->h) * 0.15;
			box.x = WIDGET_BORDER_SIZE;
			box.y = WIDGET_BORDER_SIZE;
			box.w = widget->w - WIDGET_BORDER_SIZE * 2;
			box.h = widget->h - WIDGET_BORDER_SIZE * 2;
			SDL_FillRect(widget->surface, &box, SDL_MapRGB(widget->surface->format, 0, 0, 0));
			border_create_texture(widget->surface, &box, thickness, TEXTURE_CLIENT("stat_border"));

			box.x += thickness;
			box.y += thickness;
			box.w = MAX(0, box.w - thickness * 2);
			box.h = MAX(0, box.h - thickness * 2);

			if (widget->w > widget->h)
			{
				box.w *= ((double) curr / (double) max);
			}
			else
			{
				int h;

				h = box.h * ((double) curr / (double) max);
				box.y += box.h - h;
				box.h = h;
			}

			snprintf(buf, sizeof(buf), "stat_bar_%s", widget->id);
			surface_show_fill(widget->surface, box.x, box.y, NULL, TEXTURE_CLIENT(buf), &box);
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

static void menu_stat_display_change(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
	widget_stat_struct *tmp;
	widgetdata *tmp2;
	_widget_label *label;

	tmp = (widget_stat_struct *) widget->subwidget;

	for (tmp2 = menuitem->inv; tmp2; tmp2 = tmp2->next)
	{
		if (tmp2->type == LABEL_ID)
		{
			label = LABEL(tmp2);
			free(tmp->texture);
			tmp->texture = strdup(label->text);
			string_tolower(tmp->texture);
			WIDGET_REDRAW(widget);

			break;
		}
	}
}

static void menu_stat_display(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
	widget_stat_struct *tmp;
	widgetdata *submenu;
	size_t i;

	tmp = (widget_stat_struct *) widget->subwidget;
	submenu = MENU(menuitem->env)->submenu;

	for (i = 0; i < arraysize(display_modes); i++)
	{
		add_menuitem(submenu, display_modes[i], &menu_stat_display_change, MENU_CHECKBOX, strcasecmp(tmp->texture, display_modes[i]) == 0);
	}
}

/** @copydoc widgetdata::menu_handle_func */
static int widget_menu_handle(widgetdata *widget, SDL_Event *event)
{
	widgetdata *menu;

	menu = create_menu(event->motion.x, event->motion.y, widget);

	add_menuitem(menu, "Move Widget", &menu_move_widget, MENU_NORMAL, 0);
	add_menuitem(menu, "Display  >", &menu_stat_display, MENU_SUBMENU, 0);

	menu_finalize(menu);

	return 1;
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
	widget->menu_handle_func = widget_menu_handle;
	widget->subwidget = tmp;
}
