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
 * Handles the spells widget code.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * The spell list. This is a multi-dimensional array, containing
 * dynamically resized spell path arrays, which actually hold the spells
 * for each spell path. For example (pseudo array structure):
 *
 * - spell_list:
 *  - fire
 *   - firestorm, firebolt */
static spell_entry_struct **spell_list[SPELL_PATH_NUM - 1];
/** Number of spells contained in each spell path array in ::spell_list. */
static size_t spell_list_num[SPELL_PATH_NUM];
/** Currently selected spell path ID. */
static size_t spell_list_path = 0;

/** Button buffer. */
static button_struct button_path_left, button_path_right, button_close, button_help;
/** The spells list. */
static list_struct *list_spells = NULL;

/**
 * Initialize the spells system. */
void spells_init(void)
{
	memset(&spell_list, 0, sizeof(*spell_list) * SPELL_PATH_NUM);
	memset(&spell_list_num, 0, sizeof(*spell_list_num) * SPELL_PATH_NUM);
	spell_list_path = 0;
}

/**
 * Handle double click inside the spells list.
 * @param list The spells list. */
static void list_handle_enter(list_struct *list)
{
	/* Ready the selected spell, if any. */
	if (list->text)
	{
		char buf[MAX_BUF];

		snprintf(buf, sizeof(buf), "/ready_spell %s", list->text[list->row_selected - 1][0]);
		client_command_check(buf);
	}
}

/**
 * Reload the spells list, due to a change of the spell path, filtering
 * options, etc. */
static void spell_list_reload(void)
{
	size_t i;
	uint32 offset, rows, selected;

	if (!list_spells)
	{
		return;
	}

	offset = list_spells->row_offset;
	selected = list_spells->row_selected;
	rows = list_spells->rows;
	list_clear(list_spells);

	for (i = 0; i < spell_list_num[spell_list_path]; i++)
	{
		list_add(list_spells, list_spells->rows, 0, spell_list[spell_list_path][i]->spell->s_name);
	}

	list_sort(list_spells, LIST_SORT_ALPHA);

	if (list_spells->rows == rows)
	{
		list_spells->row_offset = offset;
		list_spells->row_selected = selected;
	}

	cur_widget[SPELLS_ID]->redraw = 1;
}

/**
 * Handle button repeating (and actual pressing).
 * @param button The button. */
static void button_repeat_func(button_struct *button)
{
	int path = spell_list_path;

	if (button == &button_path_left)
	{
		path--;
	}
	else
	{
		path++;
	}

	if (path < 0)
	{
		path = SPELL_PATH_NUM - 1;
	}
	else if (path > SPELL_PATH_NUM - 1)
	{
		path = 0;
	}

	spell_list_path = path;
	spell_list_reload();
}

/**
 * Render the spell list widget.
 * @param widget The widget to render. */
void widget_spells_render(widgetdata *widget)
{
	SDL_Rect box, box2;

	/* Create the surface. */
	if (!widget->widgetSF)
	{
		widget->widgetSF = SDL_ConvertSurface(Bitmaps[BITMAP_CONTENT]->bitmap, Bitmaps[BITMAP_CONTENT]->bitmap->format, Bitmaps[BITMAP_CONTENT]->bitmap->flags);
	}

	/* Create the spell list. */
	if (!list_spells)
	{
		list_spells = list_create(12, 1, 8);
		list_spells->handle_enter_func = list_handle_enter;
		list_spells->surface = widget->widgetSF;
		list_scrollbar_enable(list_spells);
		list_set_column(list_spells, 0, 130, 7, NULL, -1);
		list_set_font(list_spells, FONT_ARIAL10);
		spell_list_reload();

		/* Create various buttons... */
		button_create(&button_path_left);
		button_create(&button_path_right);
		button_create(&button_close);
		button_create(&button_help);
		button_path_left.repeat_func = button_path_right.repeat_func = button_repeat_func;
		button_close.bitmap = button_path_left.bitmap = button_path_right.bitmap = button_help.bitmap = BITMAP_BUTTON_ROUND;
		button_close.bitmap_pressed = button_path_left.bitmap_pressed = button_path_right.bitmap_pressed = button_help.bitmap_pressed = BITMAP_BUTTON_ROUND_DOWN;
		button_close.bitmap_over = button_path_left.bitmap_over = button_path_right.bitmap_over = button_help.bitmap_over = BITMAP_BUTTON_ROUND_HOVER;
	}

	if (widget->redraw)
	{
		_BLTFX bltfx;
		size_t spell_id;

		bltfx.surface = widget->widgetSF;
		bltfx.flags = 0;
		bltfx.alpha = 0;
		sprite_blt(Bitmaps[BITMAP_CONTENT], 0, 0, NULL, &bltfx);

		box.h = 0;
		box.w = widget->wd;
		string_blt(widget->widgetSF, FONT_SERIF12, "Spells", 0, 3, COLOR_HGOLD, TEXT_ALIGN_CENTER, &box);
		list_set_parent(list_spells, widget->x1, widget->y1);
		list_show(list_spells, 10, 2);

		box.w = 160;
		string_blt(widget->widgetSF, FONT_SERIF12, s_settings->spell_paths[spell_list_path], 0, widget->ht - FONT_HEIGHT(FONT_SERIF12) - 7, COLOR_HGOLD, TEXT_ALIGN_CENTER, &box);

		/* Show the spell's description. */
		if (list_spells->text && spell_find_path_selected(list_spells->text[list_spells->row_selected - 1][0], &spell_id))
		{
			box.h = 120;
			box.w = 150;
			string_blt(widget->widgetSF, FONT_ARIAL10, spell_list[spell_list_path][spell_id]->msg, 160, 40, COLOR_WHITE, TEXT_WORD_WRAP, &box);
		}

		/* Show info such as the spell cost, path status, etc if there is
		 * a selected spell and it's a known one. */
		if (list_spells->text)
		{
			_Sprite *icon = FaceList[spell_list[spell_list_path][spell_id]->spell->face].sprite;
			const char *status;

			string_blt_format(widget->widgetSF, FONT_ARIAL10, 160, widget->ht - 30, COLOR_WHITE, TEXT_MARKUP, NULL, "<b>Cost</b>: %d", spell_list[spell_list_path][spell_id]->cost);

			if (cpl.path_denied & spell_list[spell_list_path][spell_id]->path)
			{
				status = "Denied";
			}
			else if (cpl.path_attuned & spell_list[spell_list_path][spell_id]->path && !(cpl.path_repelled & spell_list[spell_list_path][spell_id]->path))
			{
				status = "Attuned";
			}
			else if (cpl.path_repelled & spell_list[spell_list_path][spell_id]->path && !(cpl.path_attuned & spell_list[spell_list_path][spell_id]->path))
			{
				status = "Repelled";
			}
			else
			{
				status = "Normal";
			}

			string_blt_format(widget->widgetSF, FONT_ARIAL10, 160, widget->ht - 18, COLOR_WHITE, TEXT_MARKUP, NULL, "<b>Status</b>: %s", status);
			draw_frame(widget->widgetSF, widget->wd - 6 - icon->bitmap->w, widget->ht - 6 - icon->bitmap->h, icon->bitmap->w + 1, icon->bitmap->h + 1);
			sprite_blt(icon, widget->wd - 5 - icon->bitmap->w, widget->ht - 5 - icon->bitmap->h, NULL, &bltfx);
		}

		widget->redraw = list_need_redraw(list_spells);
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

	button_path_left.x = widget->x1 + 6;
	button_path_left.y = widget->y1 + widget->ht - Bitmaps[BITMAP_BUTTON_ROUND]->bitmap->h - 5;
	button_render(&button_path_left, "<");

	button_path_right.x = widget->x1 + 6 + 130;
	button_path_right.y = widget->y1 + widget->ht - Bitmaps[BITMAP_BUTTON_ROUND]->bitmap->h - 5;
	button_render(&button_path_right, ">");
}

/**
 * Handle mouse events inside the spells widget.
 * @param widget The spells widget.
 * @param event The event to handle. */
void widget_spells_mevent(widgetdata *widget, SDL_Event *event)
{
	/* If the list has handled the mouse event, we need to redraw the
	 * widget. */
	if (list_spells && list_handle_mouse(list_spells, event))
	{
		widget->redraw = 1;
	}

	if (button_event(&button_path_left, event))
	{
		button_repeat_func(&button_path_left);
	}
	else if (button_event(&button_path_right, event))
	{
		button_repeat_func(&button_path_right);
	}
	else if (button_event(&button_close, event))
	{
		widget->show = 0;
	}
	else if (button_event(&button_help, event))
	{
		help_show("spell list");
	}
	else if (list_spells->text && event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT)
	{
		size_t spell_id;
		_Sprite *icon;

		if (!spell_find_path_selected(list_spells->text[list_spells->row_selected - 1][0], &spell_id))
		{
			return;
		}

		icon = FaceList[spell_list[spell_list_path][spell_id]->spell->face].sprite;

		if (event->motion.x >= widget->x1 + widget->wd - 5 - icon->bitmap->w && event->motion.x <= widget->x1 + widget->wd - 5 && event->motion.y >= widget->y1 + widget->ht - 5 - icon->bitmap->h && event->motion.y <= widget->y1 + widget->ht - 5)
		{
			cpl.dragging_tag = spell_list[spell_list_path][spell_id]->spell->tag;
		}
	}
}

/**
 * Find a spell in the ::spell_list based on its name.
 *
 * Partial spell names will be matched.
 * @param name Spell name to find.
 * @param[out] spell_path Will contain the spell's path.
 * @param[out] spell_id Will contain the spell's ID.
 * @return 1 if the spell was found, 0 otherwise. */
int spell_find(const char *name, size_t *spell_path, size_t *spell_id)
{
	for (*spell_path = 0; *spell_path < SPELL_PATH_NUM - 1; *spell_path += 1)
	{
		for (*spell_id = 0; *spell_id < spell_list_num[*spell_path]; *spell_id += 1)
		{
			if (!strncasecmp(spell_list[*spell_path][*spell_id]->spell->s_name, name, strlen(name)))
			{
				return 1;
			}
		}
	}

	return 0;
}

/**
 * Find a spell in the ::spell_list, based on a matching object.
 * @param op Object to find.
 * @param[out] spell_path Will contain the spell's path.
 * @param[out] spell_id Will contain the spell's ID.
 * @return 1 if the spell was found, 0 otherwise. */
int spell_find_object(object *op, size_t *spell_path, size_t *spell_id)
{
	for (*spell_path = 0; *spell_path < SPELL_PATH_NUM - 1; *spell_path += 1)
	{
		for (*spell_id = 0; *spell_id < spell_list_num[*spell_path]; *spell_id += 1)
		{
			if (spell_list[*spell_path][*spell_id]->spell == op)
			{
				return 1;
			}
		}
	}

	return 0;
}

/**
 * Find a spell in the ::spell_list based on its name, but only look
 * inside the currently selected spell path list.
 *
 * Partial spell names will be matched.
 * @param name Spell name to find.
 * @param[out] spell_id Will contain the spell's ID.
 * @return 1 if the spell was found, 0 otherwise. */
int spell_find_path_selected(const char *name, size_t *spell_id)
{
	for (*spell_id = 0; *spell_id < spell_list_num[spell_list_path]; *spell_id += 1)
	{
		if (!strncasecmp(spell_list[spell_list_path][*spell_id]->spell->s_name, name, strlen(name)))
		{
			return 1;
		}
	}

	return 0;
}

/**
 * Get spell from the ::spell_list structure.
 * @param spell_path Spell path.
 * @param spell_id Spell ID.
 * @return The spell. */
spell_entry_struct *spell_get(size_t spell_path, size_t spell_id)
{
	return spell_list[spell_path][spell_id];
}

void spells_update(object *op, uint16 cost, uint32 path, uint32 flags, const char *msg)
{
	size_t spell_path, spell_id, path_real;
	spell_entry_struct *spell;

	spell = NULL;

	for (spell_path = 0; spell_path < arraysize(spell_list); spell_path++)
	{
		if (path & (1 << spell_path))
		{
			path_real = spell_path;
			break;
		}
	}

	if (spell_path == arraysize(spell_list))
	{
		logger_print(LOG(BUG), "Invalid spell path for spell '%s'.", op->s_name);
		return;
	}

	if (spell_find_object(op, &spell_path, &spell_id))
	{
		if (spell_path != path)
		{
			spells_remove(op);
		}
		else
		{
			spell = spell_get(spell_path, spell_id);
		}
	}

	if (!spell)
	{
		spell = calloc(1, sizeof(*spell));
		spell->spell = op;

		spell_list[path_real] = realloc(spell_list[path_real], sizeof(*spell_list[path_real]) * (spell_list_num[path_real] + 1));
		spell_list[path_real][spell_list_num[path_real]] = spell;
		spell_list_num[path_real]++;
	}

	spell->cost = cost;
	spell->path = path;
	spell->flags = flags;
	strncpy(spell->msg, msg, sizeof(spell->msg) - 1);
	spell->msg[sizeof(spell->msg) - 1] = '\0';

	spell_list_reload();
}

void spells_remove(object *op)
{
	size_t spell_path, spell_id, i;

	if (!spell_find_object(op, &spell_path, &spell_id))
	{
		logger_print(LOG(BUG), "Tried to remove spell '%s', but it was not in spell list.", op->s_name);
		return;
	}

	free(spell_list[spell_path][spell_id]);

	for (i = spell_id + 1; i < spell_list_num[spell_path]; i++)
	{
		spell_list[spell_path][i - 1] = spell_list[spell_path][i];
	}

	spell_list[spell_path] = realloc(spell_list[spell_path], sizeof(*spell_list[spell_path]) * (spell_list_num[spell_path] - 1));
	spell_list_num[spell_path]--;

	spell_list_reload();
}
