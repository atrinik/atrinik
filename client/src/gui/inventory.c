/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
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
 * Handles inventory related functions.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Skill category names. */
char *skill_level_name[] =
{
	"",
	"Ag",
	"Pe",
	"Me",
	"Ph",
	"Ma",
	"Wi"
};

/** Active inventory filter, one of @ref INVENTORY_FILTER_xxx. */
uint64 inventory_filter = INVENTORY_FILTER_ALL;

/**
 * Check if an object matches one of the active inventory filters.
 * @param op Object to check.
 * @return 1 if there is a match, 0 otherwise. */
static int inventory_matches_filter(object *op)
{
	/* No filtering of objects in the below inventory. */
	if (op->env == cpl.below)
	{
		return 1;
	}

	/* Always show open container, and the items inside. */
	if (cpl.container_tag == op->tag || (op->env && op->env->env))
	{
		return 1;
	}

	if (inventory_filter == INVENTORY_FILTER_ALL)
	{
		return 1;
	}

	if (inventory_filter & INVENTORY_FILTER_APPLIED && op->flags & F_APPLIED)
	{
		return 1;
	}

	if (inventory_filter & INVENTORY_FILTER_CONTAINER && op->itype == TYPE_CONTAINER)
	{
		return 1;
	}

	if (inventory_filter & INVENTORY_FILTER_MAGICAL && op->flags & F_MAGIC)
	{
		return 1;
	}

	if (inventory_filter & INVENTORY_FILTER_CURSED && op->flags & (F_CURSED | F_DAMNED))
	{
		return 1;
	}

	if (inventory_filter & INVENTORY_FILTER_UNIDENTIFIED && op->item_qua == 255)
	{
		return 1;
	}

	if (inventory_filter & INVENTORY_FILTER_UNAPPLIED && !(op->flags & F_APPLIED))
	{
		return 1;
	}

	if (inventory_filter & INVENTORY_FILTER_LOCKED && op->flags & F_LOCKED)
	{
		return 1;
	}

	return 0;
}

/**
 * Set an inventory filter to the passed value.
 * @param filter The value to set. */
void inventory_filter_set(uint64 filter)
{
	inventory_filter = filter;
	widget_inventory_handle_arrow_key(cur_widget[MAIN_INV_ID], SDLK_UNKNOWN);
	draw_info(COLOR_GREEN, "Inventory filter changed.");
}

/**
 * Toggle one inventory filter.
 * @param filter Filter to toggle. */
void inventory_filter_toggle(uint64 filter)
{
	if (inventory_filter & filter)
	{
		inventory_filter &= ~filter;
	}
	else
	{
		inventory_filter |= filter;
	}

	widget_inventory_handle_arrow_key(cur_widget[MAIN_INV_ID], SDLK_UNKNOWN);
	draw_info(COLOR_GREEN, "Inventory filter changed.");
}

/**
 * Initialize the inventory widget.
 * @param widget Widget to initialize. */
void widget_inventory_init(widgetdata *widget)
{
	inventory_struct *inventory = INVENTORY(widget);

	if (widget->WidgetTypeID == MAIN_INV_ID)
	{
		inventory->x = 3;
		inventory->y = 31;
		inventory->w = 256;
		inventory->h = 96;
	}
	else if (widget->WidgetTypeID == BELOW_INV_ID)
	{
		inventory->x = 5;
		inventory->y = 19;
		inventory->w = 256;
		inventory->h = 32;
	}

	scrollbar_info_create(&inventory->scrollbar_info);
	scrollbar_create(&inventory->scrollbar, 9, inventory->h, &inventory->scrollbar_info.scroll_offset, &inventory->scrollbar_info.num_lines, INVENTORY_ROWS(inventory));
}

/**
 * Render a single object in the inventory widget.
 *
 * If 'mx' and 'my' are not -1, no rendering is done and instead the
 * return value indicates whether the mx/my coordinates are over the
 * object.
 * @param widget The widget.
 * @param ob Object to render.
 * @param i Integer index of the object in the linked list.
 * @param[out] r Rendering index of the object.
 * @param mx Mouse X. Can be -1.
 * @param my Mouse Y. Can be -1.
 * @return 1 if the object was rendered, 0 otherwise. */
static int inventory_render_object(widgetdata *widget, object *ob, uint32 i, uint32 *r, int mx, int my)
{
	inventory_struct *inventory;
	uint32 row;

	inventory = INVENTORY(widget);
	row = i / INVENTORY_COLS(inventory);

	/* Check if this object should be visible. */
	if (row >= inventory->scrollbar_info.scroll_offset && row < inventory->scrollbar_info.scroll_offset + INVENTORY_ROWS(inventory))
	{
		uint32 r_row, r_col;
		int x, y;

		/* Calculate the row and column to render on. */
		r_row = *r / INVENTORY_COLS(inventory);
		r_col = *r % INVENTORY_COLS(inventory);

		/* Calculate the X/Y positions. */
		x = widget->x1 + inventory->x + r_col * INVENTORY_ICON_SIZE;
		y = widget->y1 + inventory->y + r_row * INVENTORY_ICON_SIZE;

		/* Increase the rendering index. */
		*r += 1;

		/* If 'mx' and 'my' are not -1, do not render, just check if the
		 * provided coordinates are over the object. */
		if (mx != -1 && my != -1)
		{
			if (mx >= x && mx < x + INVENTORY_ICON_SIZE && my >= y && my < y + INVENTORY_ICON_SIZE)
			{
				return 1;
			}
			else
			{
				return 0;
			}
		}

		/* Blit the object. */
		object_blit_inventory(ob, x, y);

		/* If this object is selected, show the selected graphic and
		 * show some extra information in the widget. */
		if (i == inventory->selected)
		{
			char buf[MAX_BUF];

			sprite_blt(Bitmaps[BITMAP_INVSLOT], x, y, NULL, NULL);

			if (ob->nrof > 1)
			{
				snprintf(buf, sizeof(buf), "%d %s", ob->nrof, ob->s_name);
			}
			else
			{
				snprintf(buf, sizeof(buf), "%s", ob->s_name);
			}

			if (widget->WidgetTypeID == MAIN_INV_ID)
			{
				string_truncate_overflow(FONT_ARIAL10, buf, widget->wd - 26 - 4);
				string_blt(ScreenSurface, FONT_ARIAL10, buf, widget->x1 + 26, widget->y1 + 2, COLOR_HGOLD, 0, NULL);

				snprintf(buf, sizeof(buf), "%4.3f kg", ob->weight * (double) ob->nrof);
				string_blt(ScreenSurface, FONT_ARIAL10, buf, widget->x1 + widget->wd - 4 - string_get_width(FONT_ARIAL10, buf, 0), widget->y1 + 15, COLOR_HGOLD, 0, NULL);

				/* 255 item quality marks the item as unidentified. */
				if (ob->item_qua == 255)
				{
					string_blt(ScreenSurface, FONT_ARIAL10, "not identified", widget->x1 + 26, widget->y1 + 15, COLOR_RED, 0, NULL);
				}
				else
				{
					string_blt(ScreenSurface, FONT_ARIAL10, "con: ", widget->x1 + 26, widget->y1 + 15, COLOR_HGOLD, 0, NULL);
					string_blt_format(ScreenSurface, FONT_ARIAL10, widget->x1 + 53, widget->y1 + 15, COLOR_HGOLD, 0, NULL, "%d/%d", ob->item_qua, ob->item_con);

					if (ob->item_level)
					{
						snprintf(buf, sizeof(buf), "allowed: lvl %d %s", ob->item_level, skill_level_name[ob->item_skill]);

						if ((!ob->item_skill && ob->item_level <= cpl.stats.level) || (ob->item_skill && ob->item_level <= cpl.stats.skill_level[ob->item_skill - 1]))
						{
							string_blt(ScreenSurface, FONT_ARIAL10, buf, widget->x1 + 101, widget->y1 + 15, COLOR_HGOLD, 0, NULL);
						}
						else
						{
							string_blt(ScreenSurface, FONT_ARIAL10, buf, widget->x1 + 101, widget->y1 + 15, COLOR_RED, 0, NULL);
						}
					}
					else
					{
						string_blt(ScreenSurface, FONT_ARIAL10, "allowed: all", widget->x1 + 101, widget->y1 + 15, COLOR_HGOLD, 0, NULL);
					}
				}
			}
			else if (widget->WidgetTypeID == BELOW_INV_ID)
			{
				string_truncate_overflow(FONT_ARIAL10, buf, 250);
				string_blt(ScreenSurface, FONT_ARIAL10, buf, widget->x1 + 6, widget->y1 + 3, COLOR_HGOLD, 0, NULL);
			}
		}

		/* If the object is marked, show that. */
		if (ob->tag == cpl.mark_count)
		{
			sprite_blt(Bitmaps[BITMAP_INVSLOT_MARKED], x, y, NULL, NULL);
		}

		/* If it's the currently open container, add the 'container
		 * start' graphic. */
		if (ob->tag == cpl.container_tag)
		{
			sprite_blt(Bitmaps[BITMAP_CMARK_START], x, y, NULL, NULL);
		}
		/* Object inside the open container... */
		else if (ob->env == cpl.sack)
		{
			/* If there is still something more in the container, show the
			 * 'object in the middle of container' graphic. */
			if (ob->next)
			{
				sprite_blt(Bitmaps[BITMAP_CMARK_MIDDLE], x, y, NULL, NULL);
			}
			/* The end, show the 'end of container' graphic instead. */
			else
			{
				sprite_blt(Bitmaps[BITMAP_CMARK_END], x, y, NULL, NULL);
			}
		}

		return 1;
	}

	return 0;
}

/**
 * Render the inventory widget.
 * @param widget Widget to render. */
void widget_inventory_render(widgetdata *widget)
{
	inventory_struct *inventory;
	object *tmp, *tmp2;
	uint32 i, r;

	inventory = INVENTORY(widget);

	if (widget->WidgetTypeID == MAIN_INV_ID)
	{
		/* Recalculate the weight, as it may have changed. */
		cpl.real_weight = 0.0;

		for (tmp = INVENTORY_WHERE(widget); tmp; tmp = tmp->next)
		{
			if (!inventory_matches_filter(tmp))
			{
				continue;
			}

			cpl.real_weight += tmp->weight * (float) tmp->nrof;
		}

		if (cpl.inventory_focus != widget->WidgetTypeID)
		{
			if (!setting_get_int(OPT_CAT_GENERAL, OPT_PLAYERDOLL))
			{
				cur_widget[PDOLL_ID]->show = 0;
			}

			if (widget->ht != 32)
			{
				resize_widget(widget, RESIZE_BOTTOM, 32);
			}

			sprite_blt(Bitmaps[BITMAP_INV_BG], widget->x1, widget->y1, NULL, NULL);

			string_blt(ScreenSurface, FONT_ARIAL10, "Carrying", widget->x1 + 162, widget->y1 + 4, COLOR_HGOLD, 0, NULL);
			string_blt_format(ScreenSurface, FONT_ARIAL10, widget->x1 + 207, widget->y1 + 4, COLOR_WHITE, 0, NULL, "%4.3f kg", cpl.real_weight);

			string_blt(ScreenSurface, FONT_ARIAL10, "Limit", widget->x1 + 162, widget->y1 + 15, COLOR_HGOLD, 0, NULL);
			string_blt_format(ScreenSurface, FONT_ARIAL10, widget->x1 + 207, widget->y1 + 15, COLOR_WHITE, 0, NULL, "%4.3f kg", (float) cpl.weight_limit / 1000.0);

			if (inventory_filter == INVENTORY_FILTER_ALL)
			{
				string_blt(ScreenSurface, FONT_ARIAL10, "(SHIFT for inventory)", widget->x1 + 35, widget->y1 + 9, COLOR_WHITE, TEXT_OUTLINE, NULL);
			}
			else
			{
				string_blt(ScreenSurface, FONT_ARIAL10, "(SHIFT for inventory)", widget->x1 + 35, widget->y1 + 4, COLOR_WHITE, TEXT_OUTLINE, NULL);
				string_blt(ScreenSurface, FONT_ARIAL10, "filter(s) active", widget->x1 + 54, widget->y1 + 15, COLOR_WHITE, TEXT_OUTLINE, NULL);
			}

			return;
		}

		if (!setting_get_int(OPT_CAT_GENERAL, OPT_PLAYERDOLL))
		{
			cur_widget[PDOLL_ID]->show = 1;
		}

		if (widget->ht != 129)
		{
			resize_widget(widget, RESIZE_BOTTOM, 129);
		}

		sprite_blt(Bitmaps[BITMAP_INVENTORY], widget->x1, widget->y1, NULL, NULL);
	}
	else if (widget->WidgetTypeID == BELOW_INV_ID)
	{
		sprite_blt(Bitmaps[BITMAP_BELOW], widget->x1, widget->y1, NULL, NULL);
	}

	/* Make sure the scroll offset and the selected object ID are valid. */
	widget_inventory_handle_arrow_key(widget, SDLK_UNKNOWN);

	for (i = 0, r = 0, tmp = INVENTORY_WHERE(widget)->inv; tmp; tmp = tmp->next)
	{
		if (!inventory_matches_filter(tmp))
		{
			continue;
		}

		inventory_render_object(widget, tmp, i, &r, -1, -1);
		i++;

		if (cpl.container_tag == tmp->tag)
		{
			for (tmp2 = cpl.sack->inv; tmp2; tmp2 = tmp2->next)
			{
				if (!inventory_matches_filter(tmp2))
				{
					continue;
				}

				inventory_render_object(widget, tmp2, i, &r, -1, -1);
				i++;
			}
		}
	}

	inventory->scrollbar_info.num_lines = ceil((double) i / INVENTORY_COLS(inventory));
	scrollbar_render(&inventory->scrollbar, ScreenSurface, widget->x1 + inventory->x + inventory->w, widget->y1 + inventory->y);
}

/**
 * Handle events in inventory widget.
 * @param widget The widget.
 * @param event Event to handle. */
void widget_inventory_event(widgetdata *widget, SDL_Event *event)
{
	inventory_struct *inventory;

	inventory = INVENTORY(widget);

	if (scrollbar_event(&inventory->scrollbar, event))
	{
		return;
	}

	if (event->type == SDL_MOUSEBUTTONDOWN)
	{
		if (event->button.button == SDL_BUTTON_WHEELUP)
		{
			widget_inventory_handle_arrow_key(widget, SDLK_UP);
			return;
		}
		else if (event->button.button == SDL_BUTTON_WHEELDOWN)
		{
			widget_inventory_handle_arrow_key(widget, SDLK_DOWN);
			return;
		}
		else if (event->button.button == SDL_BUTTON_LEFT || event->button.button == SDL_BUTTON_RIGHT)
		{
			uint32 i, r;
			object *tmp, *tmp2;
			uint8 found = 0;

			for (i = 0, r = 0, tmp = INVENTORY_WHERE(widget)->inv; tmp && !found; tmp = tmp->next)
			{
				if (!inventory_matches_filter(tmp))
				{
					continue;
				}

				if (inventory_render_object(widget, tmp, i, &r, event->motion.x, event->motion.y))
				{
					found = 1;
					break;
				}

				i++;

				if (cpl.container_tag == tmp->tag)
				{
					for (tmp2 = cpl.sack->inv; tmp2; tmp2 = tmp2->next)
					{
						if (!inventory_matches_filter(tmp2))
						{
							continue;
						}

						if (inventory_render_object(widget, tmp2, i, &r, event->motion.x, event->motion.y))
						{
							found = 1;
							break;
						}

						i++;
					}
				}
			}

			if (found)
			{
				inventory->selected = i;

				if (event->button.button == SDL_BUTTON_LEFT)
				{
					keybind_process_command("?APPLY");
				}
			}

			return;
		}
	}
}

/**
 * Calculate number of items in the inventory widget.
 * @param widget The widget.
 * @return Number of items in the inventory widget. */
uint32 widget_inventory_num_items(widgetdata *widget)
{
	uint32 i;
	object *tmp, *tmp2;

	for (i = 0, tmp = INVENTORY_WHERE(widget)->inv; tmp; tmp = tmp->next)
	{
		if (!inventory_matches_filter(tmp))
		{
			continue;
		}

		i++;

		if (cpl.container_tag == tmp->tag)
		{
			for (tmp2 = cpl.sack->inv; tmp2; tmp2 = tmp2->next)
			{
				if (!inventory_matches_filter(tmp2))
				{
					continue;
				}

				i++;
			}
		}
	}

	return i;
}

/**
 * Get the selected object from the inventory widget.
 * @param widget The inventory object.
 * @return The selected object, if any. */
object *widget_inventory_get_selected(widgetdata *widget)
{
	inventory_struct *inventory;
	uint32 i;
	object *tmp, *tmp2;

	inventory = INVENTORY(widget);

	for (i = 0, tmp = INVENTORY_WHERE(widget)->inv; tmp; tmp = tmp->next)
	{
		if (!inventory_matches_filter(tmp))
		{
			continue;
		}

		if (i == inventory->selected)
		{
			return tmp;
		}

		i++;

		if (cpl.container_tag == tmp->tag)
		{
			for (tmp2 = cpl.sack->inv; tmp2; tmp2 = tmp2->next)
			{
				if (!inventory_matches_filter(tmp2))
				{
					continue;
				}

				if (i == inventory->selected)
				{
					return tmp2;
				}

				i++;
			}
		}
	}

	return NULL;
}

/**
 * Handle the arrow keys in the inventory widget.
 * @param widget The inventory widget.
 * @param key The key. */
void widget_inventory_handle_arrow_key(widgetdata *widget, SDLKey key)
{
	inventory_struct *inventory;
	int selected, max;

	inventory = INVENTORY(widget);
	selected = inventory->selected;

	switch (key)
	{
		case SDLK_UP:
			selected -= INVENTORY_COLS(inventory);
			break;

		case SDLK_DOWN:
			selected += INVENTORY_COLS(inventory);
			break;

		case SDLK_LEFT:
			selected -= 1;
			break;

		case SDLK_RIGHT:
			selected += 1;
			break;

		default:
			break;
	}

	/* Calculate maximum number of inventory items. */
	max = widget_inventory_num_items(widget);

	/* Make sure the selected value does not overflow. */
	if (selected < 0)
	{
		selected = 0;
	}
	else if (selected > max - 1)
	{
		selected = max - 1;
	}

	inventory->selected = selected;
	/* Scroll the scrollbar as necessary. */
	inventory->scrollbar_info.scroll_offset = MAX(0, selected / (int) INVENTORY_COLS(inventory) - (int) INVENTORY_ROWS(inventory) + 1);
}

/**
 * Blit an inventory item to the screen surface.
 *
 * Uses blt_inv_item_centered() to draw the item's face and center it.
 * Draws any additional flags (like magical, cursed, damned) as icons
 * and draws nrof (if higher than 1) of items near the bottom.
 * @param tmp Pointer to the inventory item
 * @param x X position of the item
 * @param y Y position of the item */
void object_blit_inventory(object *tmp, int x, int y)
{
	int fire_ready;

	object_blit_centered(tmp, x, y);

	if (tmp->nrof > 1)
	{
		char buf[64];

		if (tmp->nrof > 9999)
		{
			snprintf(buf, sizeof(buf), "many");
		}
		else
		{
			snprintf(buf, sizeof(buf), "%d", tmp->nrof);
		}

		string_blt(ScreenSurface, FONT_ARIAL10, buf, x + INVENTORY_ICON_SIZE / 2 - string_get_width(FONT_ARIAL10, buf, 0) / 2, y + 18, COLOR_WHITE, TEXT_OUTLINE, NULL);
	}

	/* Determine whether there is a readied object for firing or not. */
	fire_ready = (fire_mode_tab[FIRE_MODE_THROW].item == tmp->tag || fire_mode_tab[FIRE_MODE_BOW].amun == tmp->tag) && tmp->env == cpl.ob;

	if (tmp->flags & F_APPLIED)
	{
		sprite_blt(Bitmaps[BITMAP_APPLY], x, y, NULL, NULL);

		if (fire_ready)
		{
			sprite_blt(Bitmaps[BITMAP_FIRE_READY], x, y + 8, NULL, NULL);
		}
	}
	else if (tmp->flags & F_UNPAID)
	{
		sprite_blt(Bitmaps[BITMAP_UNPAID], x, y, NULL, NULL);
	}
	else if (fire_ready)
	{
		sprite_blt(Bitmaps[BITMAP_FIRE_READY], x, y, NULL, NULL);
	}

	if (tmp->flags & F_LOCKED)
	{
		sprite_blt(Bitmaps[BITMAP_LOCK], x, y + INVENTORY_ICON_SIZE - Bitmaps[BITMAP_LOCK]->bitmap->w - 2, NULL, NULL);
	}

	if (tmp->flags & F_MAGIC)
	{
		sprite_blt(Bitmaps[BITMAP_MAGIC], x + INVENTORY_ICON_SIZE - Bitmaps[BITMAP_MAGIC]->bitmap->w - 2, y + INVENTORY_ICON_SIZE - Bitmaps[BITMAP_MAGIC]->bitmap->h - 2, NULL, NULL);
	}

	if (tmp->flags & F_DAMNED)
	{
		sprite_blt(Bitmaps[BITMAP_DAMNED], x + INVENTORY_ICON_SIZE - Bitmaps[BITMAP_DAMNED]->bitmap->w - 2, y, NULL, NULL);
	}
	else if (tmp->flags & F_CURSED)
	{
		sprite_blt(Bitmaps[BITMAP_CURSED], x + INVENTORY_ICON_SIZE - Bitmaps[BITMAP_CURSED]->bitmap->w - 2, y, NULL, NULL);
	}

	if (tmp->flags & F_TRAPPED)
	{
		sprite_blt(Bitmaps[BITMAP_TRAPPED], x + INVENTORY_ICON_SIZE / 2 - Bitmaps[BITMAP_TRAPPED]->bitmap->w / 2, y + INVENTORY_ICON_SIZE / 2 - Bitmaps[BITMAP_TRAPPED]->bitmap->h / 2, NULL, NULL);
	}
}

/**
 * The 'Drop' menu action for inventory windows.
 * @param widget The widget.
 * @param x X.
 * @param y Y. */
void menu_inventory_drop(widgetdata *widget, int x, int y)
{
	(void) widget;
	(void) x;
	(void) y;
	keybind_process_command("?DROP");
}

/**
 * The 'Drop all' menu action for inventory windows.
 * @param widget The widget.
 * @param x X.
 * @param y Y. */
void menu_inventory_dropall(widgetdata *widget, int x, int y)
{
	(void) widget;
	(void) x;
	(void) y;
	send_command_check("/drop all");
}

/**
 * The 'Get' menu action for inventory windows.
 * @param widget The widget.
 * @param x X.
 * @param y Y. */
void menu_inventory_get(widgetdata *widget, int x, int y)
{
	(void) widget;
	(void) x;
	(void) y;
	keybind_process_command("?GET");
}

/**
 * The 'Get all' menu action for inventory windows.
 * @param widget The widget.
 * @param x X.
 * @param y Y. */
void menu_inventory_getall(widgetdata *widget, int x, int y)
{
	(void) widget;
	(void) x;
	(void) y;
	send_command_check("/take all");
}

/**
 * The 'Examine' menu action for inventory windows.
 * @param widget The widget.
 * @param x X.
 * @param y Y. */
void menu_inventory_examine(widgetdata *widget, int x, int y)
{
	(void) widget;
	(void) x;
	(void) y;
	keybind_process_command("?EXAMINE");
}

/**
 * The 'Mark' menu action for inventory windows.
 * @param widget The widget.
 * @param x X.
 * @param y Y. */
void menu_inventory_mark(widgetdata *widget, int x, int y)
{
	(void) widget;
	(void) x;
	(void) y;
	keybind_process_command("?MARK");
}

/**
 * The 'Lock' menu action for inventory windows.
 * @param widget The widget.
 * @param x X.
 * @param y Y. */
void menu_inventory_lock(widgetdata *widget, int x, int y)
{
	(void) widget;
	(void) x;
	(void) y;
	keybind_process_command("?LOCK");
}

/**
 * The 'Ready' menu action for inventory windows.
 * @param widget The widget.
 * @param x X.
 * @param y Y. */
void menu_inventory_ready(widgetdata *widget, int x, int y)
{
	(void) widget;
	(void) x;
	(void) y;
	keybind_process_command("?FIRE_READY");
}

/**
 * The 'Drag' menu action for inventory windows.
 * @param widget The widget.
 * @param x X.
 * @param y Y. */
void menu_inventory_drag(widgetdata *widget, int x, int y)
{
	object *ob;

	(void) widget;
	(void) x;
	(void) y;

	ob = widget_inventory_get_selected(widget);

	if (!ob)
	{
		return;
	}

	cpl.dragging.tag = ob->tag;
	draggingInvItem(DRAG_QUICKSLOT);
}

/**
 * Handle the 'apply' operation for objects inside inventory widget.
 * @param widget The widget. */
void widget_inventory_handle_apply(widgetdata *widget)
{
	object *ob;

	ob = widget_inventory_get_selected(widget);

	if (!ob)
	{
		return;
	}

	draw_info_format(COLOR_DGOLD, "apply %s", ob->s_name);
	client_send_apply(ob->tag);
}

/**
 * Handle the 'examine' operation for objects inside inventory widget.
 * @param widget The widget. */
void widget_inventory_handle_examine(widgetdata *widget)
{
	object *ob;

	ob = widget_inventory_get_selected(widget);

	if (!ob)
	{
		return;
	}

	draw_info_format(COLOR_DGOLD, "examine %s", ob->s_name);
	client_send_examine(ob->tag);
}

/**
 * Handle the 'mark' operation for objects inside inventory widget.
 * @param widget The widget. */
void widget_inventory_handle_mark(widgetdata *widget)
{
	object *ob;

	ob = widget_inventory_get_selected(widget);

	if (!ob)
	{
		return;
	}

	if (ob->tag == cpl.mark_count)
	{
		draw_info_format(COLOR_DGOLD, "unmark %s", ob->s_name);
	}
	else
	{
		draw_info_format(COLOR_DGOLD, "mark %s", ob->s_name);
	}

	object_send_mark(ob);
}

/**
 * Handle the 'lock' operation for objects inside inventory widget.
 * @param widget The widget. */
void widget_inventory_handle_lock(widgetdata *widget)
{
	object *ob;

	ob = widget_inventory_get_selected(widget);

	if (!ob)
	{
		return;
	}

	if (ob->flags & F_LOCKED)
	{
		draw_info_format(COLOR_DGOLD, "unlock %s", ob->s_name);
	}
	else
	{
		draw_info_format(COLOR_DGOLD, "lock %s", ob->s_name);
	}

	toggle_locked(ob);
}

/**
 * Handle the 'get' operation for objects inside inventory widget.
 * @param widget The widget. */
void widget_inventory_handle_get(widgetdata *widget)
{
	object *ob, *container;
	int nrof;
	sint32 loc;

	ob = widget_inventory_get_selected(widget);
	container = object_find(cpl.container_tag);

	if (!ob)
	{
		return;
	}

	/* 'G' in main inventory. */
	if (widget->WidgetTypeID == MAIN_INV_ID)
	{
		/* Need to have an open container to do 'get' in main inventory... */
		if (!container)
		{
			draw_info(COLOR_DGOLD, "You have no open container to put it in.");
			return;
		}
		else
		{
			/* Open container not in main inventory... */
			if (container->env != cpl.ob)
			{
				draw_info(COLOR_DGOLD, "You already have it.");
				return;
			}
			/* If the object is already in the open container, take it out. */
			else if (ob->env == cpl.sack)
			{
				loc = cpl.ob->tag;
			}
			/* Put the object into the open container. */
			else
			{
				loc = container->tag;
			}
		}
	}
	/* 'G' in below inventory. */
	else if (widget->WidgetTypeID == BELOW_INV_ID)
	{
		/* If there is an open container on the ground and the item to
		 * 'get' is not the container and it's not inside the container,
		 * put it into the container. */
		if (container && container->env == cpl.below && container->tag != ob->tag && ob->env != cpl.sack)
		{
			loc = container->tag;
		}
		/* Otherwise pick it up into the player's inventory. */
		else
		{
			loc = cpl.ob->tag;
		}
	}
	else
	{
		return;
	}

	nrof = ob->nrof;

	if (nrof == 1)
	{
		nrof = 0;
	}
	else if (!(setting_get_int(OPT_CAT_GENERAL, OPT_COLLECT_MODE) & 1))
	{
		char buf[MAX_BUF];

		cpl.input_mode = INPUT_MODE_NUMBER;
		text_input_open(22);
		cpl.loc = loc;
		cpl.tag = ob->tag;
		cpl.nrof = nrof;
		cpl.nummode = NUM_MODE_GET;
		snprintf(buf, sizeof(buf), "%d", nrof);
		text_input_set_string(buf);
		strncpy(cpl.num_text, ob->s_name, sizeof(cpl.num_text) - 1);
		cpl.num_text[sizeof(cpl.num_text) - 1] = '\0';
		return;
	}

	draw_info_format(COLOR_DGOLD, "get %s", ob->s_name);
	client_send_move(loc, ob->tag, nrof);
	sound_play_effect("get.ogg", 100);
}

/**
 * Handle the 'drop' operation for objects inside inventory widget.
 * @param widget The widget. */
void widget_inventory_handle_drop(widgetdata *widget)
{
	object *ob, *container;
	int nrof;
	sint32 loc;

	if (widget->WidgetTypeID != MAIN_INV_ID)
	{
		return;
	}

	ob = widget_inventory_get_selected(widget);
	container = object_find(cpl.container_tag);

	if (!ob)
	{
		return;
	}

	if (ob->flags & F_LOCKED)
	{
		draw_info(COLOR_DGOLD, "That item is locked.");
		return;
	}

	if (container && container->env == cpl.below)
	{
		loc = container->tag;
	}
	else
	{
		loc = cpl.below->tag;
	}

	nrof = ob->nrof;

	if (nrof == 1)
	{
		nrof = 0;
	}
	else if (!(setting_get_int(OPT_CAT_GENERAL, OPT_COLLECT_MODE) & 2))
	{
		char buf[MAX_BUF];

		cpl.input_mode = INPUT_MODE_NUMBER;
		text_input_open(22);
		cpl.loc = loc;
		cpl.tag = ob->tag;
		cpl.nrof = nrof;
		cpl.nummode = NUM_MODE_DROP;
		snprintf(buf, sizeof(buf), "%d", nrof);
		text_input_set_string(buf);
		strncpy(cpl.num_text, ob->s_name, sizeof(cpl.num_text) - 1);
		cpl.num_text[sizeof(cpl.num_text) - 1] = '\0';
		return;
	}

	draw_info_format(COLOR_DGOLD, "drop %s", ob->s_name);
	client_send_move(loc, ob->tag, nrof);
	sound_play_effect("drop.ogg", 100);
}

/**
 * Handle the 'ready' operation for objects inside inventory widget.
 * @param widget The widget. */
void widget_inventory_handle_ready(widgetdata *widget)
{
	object *ob;

	if (widget->WidgetTypeID != MAIN_INV_ID)
	{
		return;
	}

	ob = widget_inventory_get_selected(widget);

	if (!ob)
	{
		return;
	}

	ready_object(ob);
}
