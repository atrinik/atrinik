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
 * Available filter types.
 * @anchor SPELLS_FILTER_xxx */
enum
{
	/** Only spells. */
	SPELLS_FILTER_SPELL,
	/** Only prayers. */
	SPELLS_FILTER_PRAYER,
	/** Both spells and prayers. */
	SPELLS_FILTER_ALL
};

/**
 * The spell list. This is a multi-dimensional array, containing
 * dynamically resized spell path arrays, which actually hold the spells
 * for each spell path. For example (pseudo array structure):
 *
 * - spell_list:
 *  - fire
 *   - firestorm, firebolt */
static spell_entry_struct **spell_list[SPELL_PATH_NUM];
/** Number of spells contained in each spell path array in ::spell_list. */
static size_t spell_list_num[SPELL_PATH_NUM];
/** Currently selected spell path ID. */
static size_t spell_list_path = 0;
/** If 1, only show known spells in the list of spells. */
static uint8 spell_list_filter_known;
/** One of @ref SPELLS_FILTER_xxx "filter types". */
static uint8 spell_list_filter_type;

/** Names for @ref SPELLS_FILTER_xxx "filter types". */
static const char *const filter_names[SPELLS_FILTER_ALL + 1] =
{
	"Spells", "Prayers", "All"
};

/** Button buffer. */
static button_struct button_path_left, button_path_right, button_close, button_filter_left, button_filter_right, button_filter_known, button_help;
/** The spells list. */
static list_struct *list_spells = NULL;

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
 * Handle list::text_color_hook in the spells list. */
static const char *list_text_color_hook(list_struct *list, const char *default_color, uint32 row, uint32 col)
{
	size_t spell_id;
	spell_entry_struct *spell;

	/* If the spell is not known, use gray instead of the default color. */
	if (spell_find_path_selected(list->text[row][col], &spell_id))
	{
		spell = spell_get(spell_list_path, spell_id);

		if (!spell->known)
		{
			return COLOR_GRAY;
		}
	}

	return default_color;
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
		if (spell_list_filter_known && !spell_list[spell_list_path][i]->known)
		{
			continue;
		}

		if (spell_list_filter_type != SPELLS_FILTER_ALL)
		{
			if (spell_list_filter_type == SPELLS_FILTER_SPELL && spell_list[spell_list_path][i]->type != SPELLS_FILTER_SPELL)
			{
				continue;
			}

			if (spell_list_filter_type == SPELLS_FILTER_PRAYER && spell_list[spell_list_path][i]->type != SPELLS_FILTER_PRAYER)
			{
				continue;
			}
		}

		list_add(list_spells, list_spells->rows, 0, spell_list[spell_list_path][i]->name);
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
 * Click filter button.
 * @param adj 1 if the right button was clicked, -1 if the left one was
 * clicked instead. */
static void button_filter_adjust(int adj)
{
	int type = spell_list_filter_type + adj;

	if (type < 0)
	{
		type = SPELLS_FILTER_ALL;
	}
	else if (type > SPELLS_FILTER_ALL)
	{
		type = 0;
	}

	spell_list_filter_type = type;
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
		spell_list_filter_known = 0;
		spell_list_filter_type = SPELLS_FILTER_ALL;

		list_spells = list_create(12, 1, 8);
		list_spells->handle_enter_func = list_handle_enter;
		list_spells->text_color_hook = list_text_color_hook;
		list_spells->surface = widget->widgetSF;
		list_scrollbar_enable(list_spells);
		list_set_column(list_spells, 0, 130, 7, NULL, -1);
		list_set_font(list_spells, FONT_ARIAL10);
		spell_list_reload();

		/* Create various buttons... */
		button_create(&button_path_left);
		button_create(&button_path_right);
		button_create(&button_close);
		button_create(&button_filter_left);
		button_create(&button_filter_right);
		button_create(&button_filter_known);
		button_create(&button_help);
		button_path_left.repeat_func = button_path_right.repeat_func = button_repeat_func;
		button_close.bitmap = button_path_left.bitmap = button_path_right.bitmap = button_filter_left.bitmap = button_filter_right.bitmap = button_help.bitmap = BITMAP_BUTTON_ROUND;
		button_close.bitmap_pressed = button_path_left.bitmap_pressed = button_path_right.bitmap_pressed = button_filter_left.bitmap_pressed = button_filter_right.bitmap_pressed = button_help.bitmap_pressed = BITMAP_BUTTON_ROUND_DOWN;
		button_close.bitmap_over = button_path_left.bitmap_over = button_path_right.bitmap_over = button_filter_left.bitmap_over = button_filter_right.bitmap_over = button_help.bitmap_over = BITMAP_BUTTON_ROUND_HOVER;
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

		box.w = 80;
		string_blt(widget->widgetSF, FONT_ARIAL10, filter_names[spell_list_filter_type], 160, 24, COLOR_WHITE, TEXT_ALIGN_CENTER, &box);

		/* Show the spell's description. */
		if (list_spells->text && spell_find_path_selected(list_spells->text[list_spells->row_selected - 1][0], &spell_id))
		{
			box.h = 120;
			box.w = 150;
			string_blt(widget->widgetSF, FONT_ARIAL10, spell_list[spell_list_path][spell_id]->desc, 160, 40, COLOR_WHITE, TEXT_WORD_WRAP, &box);
		}

		/* Show info such as the spell cost, path status, etc if there is
		 * a selected spell and it's a known one. */
		if (list_spells->text && spell_list[spell_list_path][spell_id]->known)
		{
			_Sprite *icon = FaceList[spell_list[spell_list_path][spell_id]->icon].sprite;
			const char *status;

			string_blt_format(widget->widgetSF, FONT_ARIAL10, 160, widget->ht - 30, COLOR_WHITE, TEXT_MARKUP, NULL, "<b>Cost</b>: %d", spell_list[spell_list_path][spell_id]->cost);

			switch (spell_list[spell_list_path][spell_id]->path)
			{
				case 'a':
					status = "Attuned";
					break;

				case 'r':
					status = "Repelled";
					break;

				case 'd':
					status = "Denied";
					break;

				default:
					status = "Normal";
					break;
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

	button_filter_left.x = widget->x1 + 160;
	button_filter_left.y = widget->y1 + 24;
	button_render(&button_filter_left, "<");

	button_filter_right.x = widget->x1 + 220;
	button_filter_right.y = widget->y1 + 24;
	button_render(&button_filter_right, ">");

	button_filter_known.pressed_forced = spell_list_filter_known;
	button_filter_known.x = widget->x1 + 243;
	button_filter_known.y = widget->y1 + 22;
	button_render(&button_filter_known, "Known");
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
	else if (button_event(&button_filter_left, event))
	{
		button_filter_adjust(-1);
	}
	else if (button_event(&button_filter_right, event))
	{
		button_filter_adjust(1);
	}
	else if (button_event(&button_filter_known, event))
	{
		spell_list_filter_known = !spell_list_filter_known;
		spell_list_reload();
	}
	else if (button_event(&button_help, event))
	{
		help_show("spell list");
	}
	else if (list_spells->text && event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT && !draggingInvItem(DRAG_GET_STATUS))
	{
		size_t spell_id;
		_Sprite *icon;

		if (!spell_find_path_selected(list_spells->text[list_spells->row_selected - 1][0], &spell_id) || !spell_list[spell_list_path][spell_id]->known)
		{
			return;
		}

		icon = FaceList[spell_list[spell_list_path][spell_id]->icon].sprite;

		if (event->motion.x >= widget->x1 + widget->wd - 5 - icon->bitmap->w && event->motion.x <= widget->x1 + widget->wd - 5 && event->motion.y >= widget->y1 + widget->ht - 5 - icon->bitmap->h && event->motion.y <= widget->y1 + widget->ht - 5)
		{
			cpl.dragging.spell = spell_get(spell_list_path, spell_id);
			draggingInvItem(DRAG_QUICKSLOT_SPELL);
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
			if (!strncasecmp(spell_list[*spell_path][*spell_id]->name, name, strlen(name)))
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
		if (!strncasecmp(spell_list[spell_list_path][*spell_id]->name, name, strlen(name)))
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

/**
 * Initialize the spell list from file. */
void spells_init(void)
{
	FILE *fp;
	char line[HUGE_BUF];
	size_t i, j, num_spells;

	/* Free previously allocated spells. */
	for (i = 0; i < SPELL_PATH_NUM; i++)
	{
		if (spell_list[i])
		{
			if (i != SPELL_PATH_NUM - 1)
			{
				for (j = 0; j < spell_list_num[i]; j++)
				{
					free(spell_list[i][j]);
				}
			}

			free(spell_list[i]);
		}
	}

	memset(&spell_list, 0, sizeof(*spell_list) * SPELL_PATH_NUM);
	memset(&spell_list_num, 0, sizeof(*spell_list_num) * SPELL_PATH_NUM);
	spell_list_path = 0;
	num_spells = 0;

	fp = server_file_open(SERVER_FILE_SPELLS);

	if (!fp)
	{
		return;
	}

	while (fgets(line, sizeof(line) - 1, fp))
	{
		char *spell_name, *icon, desc[HUGE_BUF];
		int spell_type, spell_path;

		line[strlen(line) - 1] = '\0';
		spell_name = strdup(line);

		if (!fgets(line, sizeof(line) - 1, fp))
		{
			logger_print(LOG(BUG), "Got unexpected EOF reading spells file.");
			break;
		}

		spell_type = atoi(line);

		if (!fgets(line, sizeof(line) - 1, fp))
		{
			logger_print(LOG(BUG), "Got unexpected EOF reading spells file.");
			break;
		}

		spell_path = atoi(line);

		if (!fgets(line, sizeof(line) - 1, fp))
		{
			logger_print(LOG(BUG), "Got unexpected EOF reading spells file.");
			break;
		}

		line[strlen(line) - 1] = '\0';
		icon = strdup(line);
		desc[0] = '\0';

		while (fgets(line, sizeof(line) - 1, fp))
		{
			if (!strcmp(line, "end\n"))
			{
				spell_entry_struct *entry;

				/* Resize the spell list array for this spell path. */
				spell_list[spell_path] = realloc(spell_list[spell_path], sizeof(*spell_list[spell_path]) * (spell_list_num[spell_path] + 1));
				entry = spell_list[spell_path][spell_list_num[spell_path]] = malloc(sizeof(**spell_list[spell_path]));
				spell_list_num[spell_path]++;

				/* Store the data. */
				strncpy(entry->name, spell_name, sizeof(entry->name) - 1);
				entry->name[sizeof(entry->name) - 1] = '\0';
				strncpy(entry->desc, desc, sizeof(entry->desc) - 1);
				entry->desc[sizeof(entry->desc) - 1] = '\0';
				strncpy(entry->icon_name, icon, sizeof(entry->icon_name) - 1);
				entry->icon_name[sizeof(entry->icon_name) - 1] = '\0';
				entry->icon = get_bmap_id(entry->icon_name);
				entry->known = '\0';
				entry->path = 0;
				entry->type = spell_type - 1;

				free(icon);
				free(spell_name);
				num_spells++;
				break;
			}

			strncat(desc, line, sizeof(desc) - strlen(desc) - 1);
		}
	}

	fclose(fp);

	if (num_spells)
	{
		spell_list[SPELL_PATH_NUM - 1] = malloc(sizeof(*spell_list[SPELL_PATH_NUM - 1]) * num_spells);

		for (i = 0; i < SPELL_PATH_NUM - 1; i++)
		{
			for (j = 0; j < spell_list_num[i]; j++)
			{
				spell_list[SPELL_PATH_NUM - 1][spell_list_num[SPELL_PATH_NUM - 1]] = spell_list[i][j];
				spell_list_num[SPELL_PATH_NUM - 1]++;
			}
		}
	}
}

/**
 * Reload the icon IDs, as they may have changed due to an update. */
void spells_reload(void)
{
	size_t spell_path, spell_id;

	for (spell_path = 0; spell_path < SPELL_PATH_NUM - 1; spell_path++)
	{
		for (spell_id = 0; spell_id < spell_list_num[spell_path]; spell_id++)
		{
			spell_list[spell_path][spell_id]->icon = get_bmap_id(spell_list[spell_path][spell_id]->icon_name);
		}
	}
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_spell_list(uint8 *data, size_t len, size_t pos)
{
	char name[MAX_BUF];
	uint8 type, path;
	uint32 cost;
	size_t spell_path, spell_id;

	type = packet_to_uint8(data, len, &pos);

	while (pos < len)
	{
		packet_to_string(data, len, &pos, name, sizeof(name));
		cost = packet_to_uint16(data, len, &pos);
		path = packet_to_uint8(data, len, &pos);

		/* If the spell exists, mark it as known, and store the path
		 * status and casting cost. */
		if (spell_find(name, &spell_path, &spell_id))
		{
			spell_entry_struct *spell;

			spell = spell_get(spell_path, spell_id);

			if (type == SPLIST_MODE_REMOVE)
			{
				spell->known = 0;
			}
			else
			{
				spell->known = 1;
				spell->path = path;
				spell->cost = cost;
			}
		}
	}

	spell_list_reload();
}
