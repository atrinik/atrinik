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
 * Initialize skills system. */
void skills_init(void)
{
	skill_list = NULL;
	skill_list_num = 0;
}

/**
 * Handle double click inside the skills list.
 * @param list The skills list. */
static void list_handle_enter(list_struct *list)
{
	/* Ready the selected skill, if any. */
	if (list->text)
	{
		char buf[MAX_BUF];

		snprintf(buf, sizeof(buf), "/ready_skill %s", list->text[list->row_selected - 1][0]);
		client_command_check(buf);
	}
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
		list_add(list_skills, list_skills->rows, 0, skill_list[i]->skill->s_name);
	}

	list_sort(list_skills, LIST_SORT_ALPHA);

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
		widget->widgetSF = SDL_ConvertSurface(Bitmaps[BITMAP_CONTENT]->bitmap, Bitmaps[BITMAP_CONTENT]->bitmap->format, Bitmaps[BITMAP_CONTENT]->bitmap->flags);
	}

	/* Create the skill list. */
	if (!list_skills)
	{
		list_skills = list_create(12, 1, 8);
		list_skills->handle_enter_func = list_handle_enter;
		list_skills->surface = widget->widgetSF;
		list_scrollbar_enable(list_skills);
		list_set_column(list_skills, 0, 130, 7, NULL, -1);
		list_set_font(list_skills, FONT_ARIAL10);
		skill_list_reload();

		/* Create various buttons... */
		button_create(&button_close);
		button_create(&button_help);
		button_close.bitmap = button_help.bitmap = BITMAP_BUTTON_ROUND;
		button_close.bitmap_pressed = button_help.bitmap_pressed = BITMAP_BUTTON_ROUND_DOWN;
		button_close.bitmap_over = button_help.bitmap_over = BITMAP_BUTTON_ROUND_HOVER;
	}

	if (widget->redraw)
	{
		_BLTFX bltfx;

		bltfx.surface = widget->widgetSF;
		bltfx.flags = 0;
		bltfx.alpha = 0;
		sprite_blt(Bitmaps[BITMAP_CONTENT], 0, 0, NULL, &bltfx);

		box.h = 0;
		box.w = widget->wd;
		string_blt(widget->widgetSF, FONT_SERIF12, "Skills", 0, 3, COLOR_HGOLD, TEXT_ALIGN_CENTER, &box);
		list_set_parent(list_skills, widget->x1, widget->y1);
		list_show(list_skills, 10, 2);

#if 0
		/* Show the skill's icon, if it's known. */
		if (list_skills->text && skill_list[skill_list_type][skill_id]->known)
		{
			_Sprite *icon = FaceList[skill_list[skill_list_type][skill_id]->icon].sprite;
			char level_buf[MAX_BUF], exp_buf[MAX_BUF];

			if (skill_list[skill_list_type][skill_id]->exp == -1)
			{
				strncpy(level_buf, "<b>Level</b>: n/a", sizeof(level_buf) - 1);
				level_buf[sizeof(level_buf) - 1] = '\0';
			}
			else
			{
				snprintf(level_buf, sizeof(level_buf), "<b>Level</b>: %d", skill_list[skill_list_type][skill_id]->level);
			}

			if (skill_list[skill_list_type][skill_id]->exp >= 0)
			{
				snprintf(exp_buf, sizeof(exp_buf), "<b>Exp</b>: %"FMT64, skill_list[skill_list_type][skill_id]->exp);
			}
			else
			{
				strncpy(exp_buf, "<b>Exp</b>: n/a", sizeof(exp_buf) - 1);
				exp_buf[sizeof(exp_buf) - 1] = '\0';
			}

			string_blt(widget->widgetSF, FONT_ARIAL10, level_buf, 160, widget->ht - 30, COLOR_WHITE, TEXT_MARKUP, NULL);
			string_blt(widget->widgetSF, FONT_ARIAL10, exp_buf, 160, widget->ht - 18, COLOR_WHITE, TEXT_MARKUP, NULL);

			draw_frame(widget->widgetSF, widget->wd - 6 - icon->bitmap->w, widget->ht - 6 - icon->bitmap->h, icon->bitmap->w + 1, icon->bitmap->h + 1);
			sprite_blt(icon, widget->wd - 5 - icon->bitmap->w, widget->ht - 5 - icon->bitmap->h, NULL, &bltfx);
		}
#endif

		widget->redraw = list_need_redraw(list_skills);
	}

	box2.x = widget->x1;
	box2.y = widget->y1;
	SDL_BlitSurface(widget->widgetSF, NULL, ScreenSurface, &box2);

	/* Render the various buttons. */
	button_close.x = widget->x1 + widget->wd - Bitmaps[BITMAP_BUTTON_ROUND]->bitmap->w - 4;
	button_close.y = widget->y1 + 4;
	button_render(&button_close, "X");

	button_help.x = widget->x1 + widget->wd - Bitmaps[BITMAP_BUTTON_ROUND]->bitmap->w * 2 - 4;
	button_help.y = widget->y1 + 4;
	button_render(&button_help, "?");
}

/**
 * Handle mouse events inside the skills widget.
 * @param widget The skills widget.
 * @param event The event to handle. */
void widget_skills_mevent(widgetdata *widget, SDL_Event *event)
{
	/* If the list has handled the mouse event, we need to redraw the
	 * widget. */
	if (list_skills && list_handle_mouse(list_skills, event))
	{
		widget->redraw = 1;
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
