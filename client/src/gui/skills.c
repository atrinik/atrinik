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
 * Implements the skills widget.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * The skills list. */
static skill_entry_struct **skill_list;
/**
 * Number of skills contained in ::skill_list. */
static size_t skill_list_num;
/**
 * Button buffer. */
static button_struct button_close, button_help;
/**
 * The skills list. */
static list_struct *list_skills = NULL;
/**
 * Currently selected skill in the skills list. */
static size_t selected_skill;

/**
 * Initialize skills system. */
void skills_init(void)
{
	skill_list = NULL;
	skill_list_num = 0;
	selected_skill = 0;
}

/** @copydoc list_struct::post_column_func */
static void list_post_column(list_struct *list, uint32 row, uint32 col)
{
	size_t skill_id;
	SDL_Rect box;

	skill_id = row * list->cols + col;

	if (skill_id >= skill_list_num)
	{
		return;
	}

	if (!FaceList[skill_list[skill_id]->skill->face].sprite)
	{
		return;
	}

	box.x = list->x + list->frame_offset + INVENTORY_ICON_SIZE * col;
	box.y = LIST_ROWS_START(list) + (LIST_ROW_OFFSET(row, list) * LIST_ROW_HEIGHT(list));
	box.w = INVENTORY_ICON_SIZE;
	box.h = INVENTORY_ICON_SIZE;

	surface_show(list->surface, box.x, box.y, NULL, FaceList[skill_list[skill_id]->skill->face].sprite->bitmap);

	if (selected_skill == skill_id)
	{
		char buf[MAX_BUF];

		border_create_color(list->surface, &box, "ff0000");

		strncpy(buf, skill_list[skill_id]->skill->s_name, sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = '\0';
		string_title(buf);

		box.w = 170;
		string_show(list->surface, FONT_SERIF12, buf, 147, 25, COLOR_HGOLD, TEXT_ALIGN_CENTER, &box);

		string_show_format(list->surface, FONT_ARIAL11, 165, 45, COLOR_WHITE, TEXT_MARKUP, NULL, "<b>Current level</b>: %d", skill_list[skill_id]->level);
		string_show(list->surface, FONT_ARIAL11, "<b>Skill progress</b>:", 165, 60, COLOR_WHITE, TEXT_MARKUP, NULL);

		player_draw_exp_progress(list->surface, 165, 80, skill_list[skill_id]->exp, skill_list[skill_id]->level);
	}
}

/** @copydoc list_struct::row_color_func */
static void list_row_color(list_struct *list, int row, SDL_Rect box)
{
	SDL_FillRect(list->surface, &box, SDL_MapRGB(list->surface->format, 25, 25, 25));
}

/**
 * Reload the skills list, due to a change of the skill type, for example. */
static void skill_list_reload(void)
{
	size_t i;
	uint32 offset, rows, selected;

	if (!list_skills)
	{
		return;
	}

	offset = list_skills->row_offset;
	selected = list_skills->row_selected;
	rows = list_skills->rows;
	list_clear(list_skills);

	for (i = 0; i < skill_list_num; i++)
	{
		list_add(list_skills, list_skills->rows - (i % list_skills->cols == 0 ? 0 : 1), i % list_skills->cols, NULL);
	}

	if (list_skills->rows == rows)
	{
		list_skills->row_offset = offset;
		list_skills->row_selected = selected;
	}

	cur_widget[SKILLS_ID]->redraw = 1;
}

/**
 * Render the skill list widget.
 * @param widget The widget to render. */
void widget_skills_render(widgetdata *widget)
{
	SDL_Rect box, box2;

	/* Create the surface. */
	if (!widget->widgetSF)
	{
		SDL_Surface *texture;

		texture = TEXTURE_CLIENT("content");
		widget->widgetSF = SDL_ConvertSurface(texture, texture->format, texture->flags);
	}

	/* Create the skill list. */
	if (!list_skills)
	{
		list_skills = list_create(5, 4, 8);
		list_skills->post_column_func = list_post_column;
		list_skills->row_color_func = list_row_color;
		list_skills->row_selected_func = NULL;
		list_skills->row_highlight_func = NULL;
		list_skills->surface = widget->widgetSF;
		list_skills->row_height_adjust = INVENTORY_ICON_SIZE;
		list_set_font(list_skills, -1);
		list_scrollbar_enable(list_skills);
		list_set_column(list_skills, 0, INVENTORY_ICON_SIZE, 0, NULL, -1);
		list_set_column(list_skills, 1, INVENTORY_ICON_SIZE, 0, NULL, -1);
		list_set_column(list_skills, 2, INVENTORY_ICON_SIZE, 0, NULL, -1);
		list_set_column(list_skills, 3, INVENTORY_ICON_SIZE, 0, NULL, -1);
		skill_list_reload();

		/* Create various buttons... */
		button_create(&button_close);
		button_create(&button_help);
		button_close.texture = button_help.texture = texture_get(TEXTURE_TYPE_CLIENT, "button_round");
		button_close.texture_pressed = button_help.texture_pressed = texture_get(TEXTURE_TYPE_CLIENT, "button_round_down");
		button_close.texture_over = button_help.texture_over = texture_get(TEXTURE_TYPE_CLIENT, "button_round_over");
	}

	if (widget->redraw)
	{
		surface_show(widget->widgetSF, 0, 0, NULL, TEXTURE_CLIENT("content"));

		box.h = 0;
		box.w = widget->wd;
		string_show(widget->widgetSF, FONT_SERIF12, "Skills", 0, 3, COLOR_HGOLD, TEXT_ALIGN_CENTER, &box);
		list_set_parent(list_skills, widget->x1, widget->y1);
		list_show(list_skills, 10, 2);

		widget->redraw = list_need_redraw(list_skills);
	}

	box2.x = widget->x1;
	box2.y = widget->y1;
	SDL_BlitSurface(widget->widgetSF, NULL, ScreenSurface, &box2);

	/* Render the various buttons. */
	button_close.x = widget->x1 + widget->wd - TEXTURE_SURFACE(button_close.texture)->w - 4;
	button_close.y = widget->y1 + 4;
	button_show(&button_close, "X");

	button_help.x = widget->x1 + widget->wd - TEXTURE_SURFACE(button_close.texture)->w * 2 - 4;
	button_help.y = widget->y1 + 4;
	button_show(&button_help, "?");
}

/**
 * Handle mouse events inside the skills widget.
 * @param widget The skills widget.
 * @param event The event to handle. */
void widget_skills_mevent(widgetdata *widget, SDL_Event *event)
{
	uint32 row, col;

	/* If the list has handled the mouse event, we need to redraw the
	 * widget. */
	if (list_skills && list_handle_mouse(list_skills, event))
	{
		widget->redraw = 1;
	}

	if (event->button.button == SDL_BUTTON_LEFT && list_mouse_get_pos(list_skills, event->motion.x, event->motion.y, &row, &col))
	{
		size_t skill_id;

		skill_id = row * list_skills->cols + col;

		if (skill_id < skill_list_num)
		{
			if (event->type == SDL_MOUSEBUTTONUP)
			{
				if (selected_skill != skill_id)
				{
					selected_skill = skill_id;
					widget->redraw = 1;
				}
			}
			else if (event->type == SDL_MOUSEBUTTONDOWN)
			{
				event_dragging_start(skill_list[skill_id]->skill->tag, event->motion.x, event->motion.y);
			}
		}
	}

	if (button_event(&button_close, event))
	{
		widget->show = 0;
	}
	else if (button_event(&button_help, event))
	{
		help_show("skill list");
	}
}

/**
 * Find a skill in the ::skill_list based on its name.
 *
 * Partial skill names will be matched.
 * @param name Skill name to find.
 * @param[out] id Will contain the skill's ID.
 * @return 1 if the skill was found, 0 otherwise. */
int skill_find(const char *name, size_t *id)
{
	for (*id = 0; *id < skill_list_num; *id += 1)
	{
		if (!strncasecmp(skill_list[*id]->skill->s_name, name, strlen(name)))
		{
			return 1;
		}
	}

	return 0;
}

int skill_find_object(object *op, size_t *id)
{
	for (*id = 0; *id < skill_list_num; *id += 1)
	{
		if (skill_list[*id]->skill == op)
		{
			return 1;
		}
	}

	return 0;
}

/**
 * Get skill from the ::skill_list structure.
 * @param id Skill ID.
 * @return The skill. */
skill_entry_struct *skill_get(size_t id)
{
	return skill_list[id];
}

void skills_update(object *op, uint8 level, sint64 exp)
{
	size_t skill_id;
	skill_entry_struct *skill;

	if (skill_find_object(op, &skill_id))
	{
		skill = skill_get(skill_id);
	}
	else
	{
		skill = calloc(1, sizeof(*skill));
		skill->skill = op;

		skill_list = realloc(skill_list, sizeof(*skill_list) * (skill_list_num + 1));
		skill_list[skill_list_num] = skill;
		skill_list_num++;
	}

	skill->level = level;
	skill->exp = exp;

	skill_list_reload();
}

void skills_remove(object *op)
{
	size_t skill_id, i;

	if (!skill_find_object(op, &skill_id))
	{
		logger_print(LOG(BUG), "Tried to remove skill '%s', but it was not in skill list.", op->s_name);
		return;
	}

	free(skill_list[skill_id]);

	for (i = skill_id + 1; i < skill_list_num; i++)
	{
		skill_list[i - 1] = skill_list[i];
	}

	skill_list = realloc(skill_list, sizeof(*skill_list) * (skill_list_num - 1));
	skill_list_num--;

	skill_list_reload();
}
