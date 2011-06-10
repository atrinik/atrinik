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
 * Handles inventory related functions. */

#include "include.h"

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

	/* Always show open container. */
	if (cpl.container && cpl.container->tag == op->tag)
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
	cpl.win_inv_slot = 0;
	cpl.win_inv_start = 0;
	cpl.win_inv_tag = get_inventory_data(cpl.ob, &cpl.win_inv_ctag, &cpl.win_inv_slot, &cpl.win_inv_start, &cpl.win_inv_count, INVITEMXLEN, INVITEMYLEN);
	draw_info("Inventory filter changed.", COLOR_GREEN);
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

	cpl.win_inv_slot = 0;
	cpl.win_inv_start = 0;
	cpl.win_inv_tag = get_inventory_data(cpl.ob, &cpl.win_inv_ctag, &cpl.win_inv_slot, &cpl.win_inv_start, &cpl.win_inv_count, INVITEMXLEN, INVITEMYLEN);
	draw_info("Inventory filter changed.", COLOR_GREEN);
}

/* This function returns number of items and adjusts the inventory window data */
int get_inventory_data(object *op, int *ctag, int *slot, int *start, int *count, int wxlen, int wylen)
{
	object *tmp, *tmpc;
	int i = 0, ret = -1;

	cpl.window_weight = 0.0f;
	*ctag = -1;
	*count = 0;

	if (!op)
	{
		*slot = *start = 0;
		return -1;
	}

	if (!op->inv)
	{
		*slot = *start = 0;
		return -1;
	}

	if (*slot < 0)
		*slot = 0;

	/* Pre count items, and adjust slot cursor */
	for (tmp = op->inv; tmp; tmp = tmp->next)
	{
		if (inventory_matches_filter(tmp))
		{
			(*count)++;
			cpl.window_weight += tmp->weight * (float)tmp->nrof;
		}

		if (tmp->tag == cpl.container_tag)
			cpl.container = tmp;

		if (cpl.container && cpl.container->tag == tmp->tag)
		{
			tmpc = cpl.sack->inv;

			for (; tmpc; tmpc = tmpc->next)
				(*count)++;
		}
	}

	if (!*count)
		*slot = 0;
	else if (*slot >= *count)
		*slot = *count - 1;

	/* Now find tag */
	for (tmp = op->inv; tmp; tmp = tmp->next)
	{
		if (inventory_matches_filter(tmp))
		{
			if (*slot == i)
				ret = tmp->tag;

			i++;
		}

		if (cpl.container && cpl.container->tag == tmp->tag)
		{
			tmpc = cpl.sack->inv;

			for (; tmpc; tmpc = tmpc->next)
			{
				if (*slot == i)
				{
					*ctag = cpl.container->tag;
					ret = tmpc->tag;
				}

				i++;
			}
		}
	}

	/* And adjust the slot/start position of the window */
	if (*slot < *start)
		*start = *slot - (*slot % wxlen);
	else if (*slot > *start + (wxlen * wylen) - 1)
		*start = ((int)(*slot / wxlen)) * wxlen - (wxlen * (wylen - 1));

	return ret;
}

static void show_inventory_item_stats(object *tmp, widgetdata *widget)
{
	char buf[MAX_BUF];
	SDL_Rect tmp_rect;
	tmp_rect.w = 222;

	if (tmp->nrof > 1)
		snprintf(buf, sizeof(buf), "%d %s", tmp->nrof, tmp->s_name);
	else
		snprintf(buf, sizeof(buf), "%s", tmp->s_name);

	StringBlt(ScreenSurface, &SystemFont, buf, widget->x1 + 26, widget->y1 + 3, COLOR_HGOLD, &tmp_rect, NULL);

	snprintf(buf, sizeof(buf), "weight: %4.3f", tmp->weight * (float)tmp->nrof);
	StringBlt(ScreenSurface, &SystemFont, buf, widget->x1 + 173, widget->y1 + 15, COLOR_HGOLD, NULL, NULL);

	/* This comes from server when not identified */
	if (tmp->item_qua == 255)
		StringBlt(ScreenSurface, &SystemFont, "not identified", widget->x1 + 26, widget->y1 + 15, COLOR_RED, NULL, NULL);
	else
	{
		StringBlt(ScreenSurface, &SystemFont, "con: ", widget->x1 + 26, widget->y1 + 15, COLOR_HGOLD, NULL, NULL);

		snprintf(buf, sizeof(buf), "%d", tmp->item_qua);
		StringBlt(ScreenSurface, &SystemFont, buf, widget->x1 + 60 - get_string_pixel_length(buf, &SystemFont), widget->y1 + 15, COLOR_HGOLD, NULL, NULL);

		snprintf(buf, sizeof(buf), "(%d)", tmp->item_con);
		StringBlt(ScreenSurface, &SystemFont, buf, widget->x1 + 63, widget->y1 + 15, COLOR_HGOLD, NULL, NULL);

		if (tmp->item_level)
		{
			snprintf(buf, sizeof(buf), "allowed: lvl %d %s", tmp->item_level, skill_level_name[tmp->item_skill]);

			if ((!tmp->item_skill && tmp->item_level <= cpl.stats.level) || (tmp->item_skill && tmp->item_level <= cpl.stats.skill_level[tmp->item_skill - 1]))
				StringBlt(ScreenSurface, &SystemFont, buf, widget->x1 + 103 , widget->y1 + 15, COLOR_HGOLD, NULL, NULL);
			else
				StringBlt(ScreenSurface, &SystemFont, buf, widget->x1 + 103, widget->y1 + 15, COLOR_RED, NULL, NULL);
		}
		else
			StringBlt(ScreenSurface, &SystemFont, "allowed: all", widget->x1 + 103, widget->y1 + 15, COLOR_HGOLD, NULL, NULL);
	}
}

void widget_inventory_event(widgetdata *widget, int x, int y, SDL_Event event)
{
	int mx = 0, my = 0;
	mx = x - widget->x1;
	my = y - widget->y1;

	switch (event.type)
	{
		case SDL_MOUSEBUTTONUP:
			if (draggingInvItem(DRAG_GET_STATUS) > DRAG_IWIN_BELOW)
			{
				/* KEYFUNC_APPLY and KEYFUNC_DROP works only if cpl.inventory_win = IWIN_INV. The tag must
				 * be placed in cpl.win_inv_tag. So we do this and after DnD we restore the old values. */
				int old_inv_win = cpl.inventory_win;
				int old_inv_tag = cpl.win_inv_tag;
				cpl.inventory_win = IWIN_INV;

				if (draggingInvItem(DRAG_GET_STATUS) == DRAG_PDOLL)
				{
					cpl.win_inv_tag = cpl.win_pdoll_tag;
					/* We don't have to check for the coordinates, if we are here we are in the widget */

					/* Drop to inventory */
					process_macro_keys(KEYFUNC_APPLY, 0);
				}

				cpl.inventory_win = old_inv_win;
				cpl.win_inv_tag = old_inv_tag;
			}
			else if (draggingInvItem(DRAG_GET_STATUS) == DRAG_IWIN_BELOW)
			{
				sound_play_effect("get.ogg", 100);
				process_macro_keys(KEYFUNC_GET, 0);
			}

			draggingInvItem(DRAG_NONE);
			break;

		case SDL_MOUSEBUTTONDOWN:
			/* Inventory (open / close) */
			if (mx >= 4 && mx <= 22 && my >= 4 && my <= 26)
			{
				if (cpl.inventory_win == IWIN_INV)
					cpl.inventory_win = IWIN_BELOW;
				else
					cpl.inventory_win = IWIN_INV;

				break;
			}

			/* scrollbar */
			if (mx > 226 && mx < 236)
			{
				if (my <= 39 && my >= 30 && cpl.win_inv_slot >= INVITEMXLEN)
					cpl.win_inv_slot -= INVITEMXLEN;
				else if (my >= 116 && my <= 125)
				{
					cpl.win_inv_slot += INVITEMXLEN;

					if (cpl.win_inv_slot > cpl.win_inv_count)
						cpl.win_inv_slot = cpl.win_inv_count;
				}
			}
			else if (mx > 3)
			{
				/* Stuff */

				/* Mousewheel */
				if (event.button.button == 4 && cpl.win_inv_slot >= INVITEMXLEN)
					cpl.win_inv_slot -= INVITEMXLEN;
				/* Mousewheel */
				else if (event.button.button == 5)
				{
					cpl.win_inv_slot += INVITEMXLEN;

					if (cpl.win_inv_slot > cpl.win_inv_count)
						cpl.win_inv_slot = cpl.win_inv_count;
				}
				else if ((event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT || event.button.button == SDL_BUTTON_MIDDLE) && my > 29 && my < 125)
				{
					cpl.win_inv_slot = (my - 30) / 32 * INVITEMXLEN + (mx - 3) / 32 + cpl.win_inv_start;
					cpl.win_inv_tag = get_inventory_data(cpl.ob, &cpl.win_inv_ctag, &cpl.win_inv_slot, &cpl.win_inv_start, &cpl.win_inv_count, INVITEMXLEN, INVITEMYLEN);

					if (event.button.button == SDL_BUTTON_RIGHT || event.button.button == SDL_BUTTON_MIDDLE)
						process_macro_keys(KEYFUNC_MARK, 0);
					else
					{
						if (cpl.inventory_win == IWIN_INV)
							draggingInvItem(DRAG_IWIN_INV);
					}
				}
			}

			break;

		case SDL_MOUSEMOTION:
			/* Scrollbar sliders */
			if (event.button.button == SDL_BUTTON_LEFT && !draggingInvItem(DRAG_GET_STATUS))
			{
				/* IWIN_INV Slider */
				if (cpl.inventory_win == IWIN_INV && my + 38 && my + 116 && mx + 227 && mx + 236)
				{
					if (old_mouse_y - y > 0)
						cpl.win_inv_slot -= INVITEMXLEN;
					else if (old_mouse_y - y < 0)
						cpl.win_inv_slot += INVITEMXLEN;

					if (cpl.win_inv_slot > cpl.win_inv_count)
						cpl.win_inv_slot = cpl.win_inv_count;

					break;
				}
			}
	}
}

void widget_show_inventory_window(widgetdata *widget)
{
	int i, invxlen, invylen;
	object *op, *tmp, *tmpx = NULL;
	object *tmpc;
	char buf[256];
	widgetdata *tmp_widget;

	if (cpl.inventory_win != IWIN_INV)
	{
		if (!options.playerdoll)
		{
			/* do this for all player doll widgets, even though there shouldn't be more than one */
			for (tmp_widget = cur_widget[PDOLL_ID]; tmp_widget; tmp_widget = tmp_widget->type_next)
			{
				tmp_widget->show = 0;
			}
		}

		if (widget->ht != 32)
		{
			resize_widget(widget, RESIZE_BOTTOM, 32);
		}

		sprite_blt(Bitmaps[BITMAP_INV_BG], widget->x1, widget->y1, NULL, NULL);

		StringBlt(ScreenSurface, &SystemFont, "Carry", widget->x1 + 140, widget->y1 + 4, COLOR_HGOLD, NULL, NULL);

		snprintf(buf, sizeof(buf), "%4.3f kg", cpl.real_weight);
		StringBlt(ScreenSurface, &SystemFont, buf, widget->x1 + 140 + 35, widget->y1 + 4, COLOR_DEFAULT, NULL, NULL);

		StringBlt(ScreenSurface, &SystemFont, "Limit", widget->x1 + 140, widget->y1 + 15, COLOR_HGOLD, NULL, NULL);

		snprintf(buf, sizeof(buf), "%4.3f kg", (float) cpl.weight_limit / 1000.0);
		StringBlt(ScreenSurface, &SystemFont, buf, widget->x1 + 140 + 35, widget->y1 + 15, COLOR_DEFAULT, NULL, NULL);

		StringBlt(ScreenSurface, &Font6x3Out, "(SHIFT for inventory)", widget->x1 + 32, widget->y1 + 9, COLOR_DEFAULT, NULL, NULL);

		return;
	}

	if (!options.playerdoll)
	{
		/* do this for all player doll widgets, even though there shouldn't be more than one */
		for (tmp_widget = cur_widget[PDOLL_ID]; tmp_widget; tmp_widget = tmp_widget->type_next)
		{
			tmp_widget->show = 1;
		}
	}

	if (widget->ht != 129)
	{
		resize_widget(widget, RESIZE_BOTTOM, 129);
	}

	invxlen = INVITEMXLEN;
	invylen = INVITEMYLEN;

	sprite_blt(Bitmaps[BITMAP_INVENTORY], widget->x1, widget->y1, NULL, NULL);

	blt_window_slider(Bitmaps[BITMAP_INV_SCROLL], ((cpl.win_inv_count - 1) / invxlen) + 1, invylen, cpl.win_inv_start / invxlen, -1, widget->x1 + 229, widget->y1 + 42);

	if (!cpl.ob)
	{
		return;
	}

	op = cpl.ob;

	for (tmpc = NULL, i = 0, tmp = op->inv; tmp && i < cpl.win_inv_start; tmp = tmp->next)
	{
		if (inventory_matches_filter(tmp))
		{
			i++;
		}

		if (cpl.container && cpl.container->tag == tmp->tag)
		{
			tmpx = tmp;
			tmpc = cpl.sack->inv;

			for (; tmpc && i < cpl.win_inv_start; tmpc = tmpc->next,i++);

			if (tmpc)
			{
				break;
			}
		}
	}

	i = 0;

	if (tmpc)
	{
		tmp = tmpx;
		goto jump_in_container1;
	}

	for (; tmp && i < invxlen * invylen; tmp = tmp->next)
	{
		if (inventory_matches_filter(tmp))
		{
			if (tmp->tag == cpl.mark_count)
			{
				sprite_blt(Bitmaps[BITMAP_INVSLOT_MARKED], widget->x1 + (i % invxlen) * 32 + 3, widget->y1 + (i / invxlen) * 32 + 31, NULL, NULL);
			}

			blt_inv_item(tmp, widget->x1 + (i % invxlen) * 32 + 4, widget->y1 + (i / invxlen) * 32 + 32);

			if (cpl.inventory_win != IWIN_BELOW && i + cpl.win_inv_start == cpl.win_inv_slot)
			{
				sprite_blt(Bitmaps[BITMAP_INVSLOT], widget->x1 + (i % invxlen) * 32 + 3, widget->y1 + (i / invxlen) * 32 + 31, NULL, NULL);
				show_inventory_item_stats(tmp, widget);
			}

			i++;
		}

		/* We have an open container - 'insert' the items inside in the panel */
		if (cpl.container && cpl.container->tag == tmp->tag)
		{
			sprite_blt(Bitmaps[BITMAP_CMARK_START], widget->x1 + ((i - 1) % invxlen) * 32 + 4, widget->y1 + ((i - 1) / invxlen) * 32 + 32, NULL, NULL);
			tmpc = cpl.sack->inv;

jump_in_container1:
			for (; tmpc && i < invxlen * invylen; tmpc = tmpc->next)
			{
				if (tmpc->tag == cpl.mark_count)
				{
					sprite_blt(Bitmaps[BITMAP_INVSLOT_MARKED], widget->x1 + (i % invxlen) * 32 + 3, widget->y1 + (i / invxlen) * 32 + 31, NULL, NULL);
				}

				blt_inv_item(tmpc, widget->x1 + (i % invxlen) * 32 + 4, widget->y1 + (i / invxlen) * 32 + 32);

				if (cpl.inventory_win != IWIN_BELOW && i + cpl.win_inv_start == cpl.win_inv_slot)
				{
					sprite_blt(Bitmaps[BITMAP_INVSLOT], widget->x1 + (i % invxlen) * 32 + 3, widget->y1 + (i / invxlen) * 32 + 31, NULL, NULL);

					show_inventory_item_stats(tmpc, widget);
				}

				sprite_blt(Bitmaps[BITMAP_CMARK_MIDDLE], widget->x1 + (i % invxlen) * 32 + 4, widget->y1 + (i / invxlen) * 32 + 32, NULL, NULL);
				i++;
			}

			if (!tmpc)
			{
				sprite_blt(Bitmaps[BITMAP_CMARK_END], widget->x1 + ((i - 1) % invxlen) * 32 + 4, widget->y1 + ((i - 1) / invxlen) * 32 + 32, NULL, NULL);
			}
		}
	}
}

void widget_below_window_event(widgetdata *widget, int x, int y, int MEvent)
{
	/* ground ( IWIN_BELOW )  */
	if (y >= widget->y1 + 19 && y <= widget->y1 + widget->ht - 4 && x > widget->x1 + 4 && x < widget->x1 + widget->wd - 12)
	{
		if (cpl.inventory_win == IWIN_INV)
		{
			cpl.inventory_win = IWIN_BELOW;
		}

		cpl.win_below_slot = (x - widget->x1 - 5) / 32;

		cpl.win_below_tag = get_inventory_data(cpl.below, &cpl.win_below_ctag, &cpl.win_below_slot, &cpl.win_below_start, &cpl.win_below_count, INVITEMBELOWXLEN, INVITEMBELOWYLEN);

		if ((SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)))
		{
			if (cpl.below->inv)
			{
				draggingInvItem(DRAG_IWIN_BELOW);
			}
		}
		else
		{
			process_macro_keys(KEYFUNC_APPLY, 0);
		}
	}
	else if (y >= widget->y1 + 20 && y <= widget->y1 + 29 && x > widget->x1 + 262 && x < widget->x1 + 269 && MEvent == MOUSE_DOWN)
	{
		if (cpl.inventory_win == IWIN_INV)
		{
			cpl.inventory_win = IWIN_BELOW;
		}

		cpl.win_below_slot = cpl.win_below_slot - INVITEMBELOWXLEN;

		if (cpl.win_below_slot < 0)
		{
			cpl.win_below_slot = 0;
		}

		cpl.win_below_tag = get_inventory_data(cpl.below, &cpl.win_below_ctag, &cpl.win_below_slot, &cpl.win_below_start, &cpl.win_below_count, INVITEMBELOWXLEN, INVITEMBELOWYLEN);
	}
	else if (y >= widget->y1 + 42 && y <= widget->y1 + 51 && x > widget->x1 + 262 && x < widget->x1 + 269 && MEvent == MOUSE_DOWN)
	{
		if (cpl.inventory_win == IWIN_INV)
		{
			cpl.inventory_win = IWIN_BELOW;
		}

		cpl.win_below_slot = cpl.win_below_slot + INVITEMBELOWXLEN;

		if (cpl.win_below_slot > cpl.win_below_count -1)
		{
			cpl.win_below_slot = cpl.win_below_count -1;
		}

		cpl.win_below_tag = get_inventory_data(cpl.below, &cpl.win_below_ctag, &cpl.win_below_slot, &cpl.win_below_start, &cpl.win_below_count, INVITEMBELOWXLEN, INVITEMBELOWYLEN);
	}
}

void widget_show_below_window(widgetdata *widget)
{
	int i, slot,at;
	object *tmp, *tmpc, *tmpx = NULL;
	char buf[256];
	SDL_Rect tmp_rect;
	tmp_rect.w = 265;

	sprite_blt(Bitmaps[BITMAP_BELOW], widget->x1, widget->y1, NULL, NULL);

	blt_window_slider(Bitmaps[BITMAP_BELOW_SCROLL], ((cpl.win_below_count - 1) / INVITEMBELOWXLEN) + 1, INVITEMBELOWYLEN, cpl.win_below_start / INVITEMBELOWXLEN, -1, widget->x1 + 263, widget->y1 + 30);

	if (!cpl.below)
	{
		return;
	}

	for (i = 0, tmpc = NULL, tmp = cpl.below->inv; tmp && i < cpl.win_below_start; tmp = tmp->next)
	{
		i++;
		tmpx = tmp;

		if (cpl.container && cpl.container->tag == tmp->tag)
		{
			tmpc = cpl.sack->inv;

			for (; tmpc && i < cpl.win_below_start; tmpc = tmpc->next, i++);

			if (tmpc)
			{
				break;
			}
		}
	}

	i = 0;

	if (tmpc)
	{
		tmp = tmpx;
		goto jump_in_container2;
	}

	for (; tmp && i < INVITEMBELOWXLEN * INVITEMBELOWYLEN; tmp = tmp->next)
	{
		at = tmp->flags & F_APPLIED;

		if (tmp->tag != cpl.container_tag)
			tmp->flags &= ~F_APPLIED;

		blt_inv_item(tmp, widget->x1 + (i % INVITEMBELOWXLEN) * 32 + 5, widget->y1 + (i / INVITEMBELOWXLEN) * 32 + 19);

		if (at)
		{
			tmp->flags |= F_APPLIED;
		}

		if (i + cpl.win_below_start == cpl.win_below_slot)
		{
			if (cpl.inventory_win == IWIN_BELOW)
				slot = BITMAP_INVSLOT;
			else
				slot = BITMAP_INVSLOT_U;

			sprite_blt(Bitmaps[slot], widget->x1 + (i % INVITEMBELOWXLEN) * 32 + 5, widget->y1 + (i / INVITEMBELOWXLEN) * 32 + 19, NULL, NULL);

			if (tmp->nrof > 1)
				snprintf(buf, sizeof(buf), "%d %s", tmp->nrof, tmp->s_name);
			else
				snprintf(buf, sizeof(buf), "%s", tmp->s_name);

			StringBlt(ScreenSurface, &SystemFont, buf, widget->x1 + 6, widget->y1 + 5, COLOR_HGOLD, &tmp_rect, NULL);
		}

		i++;

		/* We have an open container - 'insert' the items inside in the panel */
		if (cpl.container && cpl.container->tag == tmp->tag)
		{
			sprite_blt(Bitmaps[BITMAP_CMARK_START], widget->x1 + ((i - 1) % INVITEMBELOWXLEN) * 32 + 5, widget->y1 + ((i - 1) / INVITEMBELOWXLEN) * 32 + 19, NULL, NULL);
			tmpc = cpl.sack->inv;

jump_in_container2:
			for (; tmpc && i < INVITEMBELOWXLEN * INVITEMBELOWYLEN; tmpc = tmpc->next)
			{
				blt_inv_item(tmpc, widget->x1 + (i % INVITEMBELOWXLEN) * 32 + 5, widget->y1 + (i / INVITEMBELOWXLEN) * 32 + 19);

				if (i + cpl.win_below_start == cpl.win_below_slot)
				{
					if (cpl.inventory_win == IWIN_BELOW)
						slot = BITMAP_INVSLOT;
					else
						slot = BITMAP_INVSLOT_U;

					sprite_blt(Bitmaps[slot], widget->x1 + (i % INVITEMBELOWXLEN) * 32 + 5, widget->y1 + (i / INVITEMBELOWXLEN) * 32 + 19, NULL, NULL);

					if (tmpc->nrof > 1)
						snprintf(buf, sizeof(buf), "%d %s", tmpc->nrof, tmpc->s_name);
					else
						snprintf(buf, sizeof(buf), "%s", tmpc->s_name);

					StringBlt(ScreenSurface, &SystemFont, buf, widget->x1 + 6, widget->y1 + 4, COLOR_HGOLD, &tmp_rect, NULL);
				}

				sprite_blt(Bitmaps[BITMAP_CMARK_MIDDLE], widget->x1 + (i % INVITEMBELOWXLEN) * 32 + 5, widget->y1 + (i / INVITEMBELOWXLEN) * 32 + 19, NULL, NULL);
				i++;
			}

			if (!tmpc)
				sprite_blt(Bitmaps[BITMAP_CMARK_END], widget->x1 + ((i - 1) % INVITEMBELOWXLEN) * 32 + 5, widget->y1 + ((i - 1) / INVITEMBELOWXLEN) * 32 + 19, NULL, NULL);
		}
	}
}

#define ICONDEFLEN 32
int blt_inv_item_centered(object *tmp, int x, int y)
{
	int temp, xstart, xlen, ystart, ylen;
	sint16 anim1;
	SDL_Rect box;
	_BLTFX bltfx;
	bltfx.flags = 0;
	bltfx.dark_level = 0;
	bltfx.surface = NULL;
	bltfx.alpha = 128;

#if 0
	bltfx.flags = 1;
	bltfx.dark_level = 0;
#endif

	if (!FaceList[tmp->face].sprite)
		return 0;

	anim1 = tmp->face;

	/* This is part of animation... Because ISO items have different offsets and sizes,
	 * we must use ONE sprite base offset to center an animation over an animation.
	 * we use the first frame of an animation for it.*/
	if (tmp->animation_id > 0)
	{
		check_animation_status(tmp->animation_id);

		/* First bitmap of this animation */
		if (animations[tmp->animation_id].num_animations && animations[tmp->animation_id].facings <= 1)
			anim1 = animations[tmp->animation_id].faces[0];
	}

	/* Fallback: first animation bitmap not loaded */
	if (!FaceList[anim1].sprite)
		anim1 = tmp->face;

	xstart = FaceList[anim1].sprite->border_left;
	xlen = FaceList[anim1].sprite->bitmap->w - xstart-FaceList[anim1].sprite->border_right;
	ystart = FaceList[anim1].sprite->border_up;
	ylen = FaceList[anim1].sprite->bitmap->h - ystart-FaceList[anim1].sprite->border_down;

	if (xlen > 32)
	{
		box.w = 32;
		temp = (xlen - 32) / 2;
		box.x = xstart + temp;
		xstart = 0;
	}
	else
	{
		box.w = xlen;
		box.x = xstart;
		xstart = (32 - xlen) / 2;
	}

	if (ylen > 32)
	{
		box.h = 32;
		temp = (ylen - 32) / 2;
		box.y = ystart + temp;
		ystart = 0;
	}
	else
	{
		box.h = ylen;
		box.y = ystart;
		ystart = (32 - ylen) / 2;
	}

	/* Now we have a perfect centered sprite.
	 * But: If this is the start pos of our
	 * first animation and not of our sprite,
	 * we must shift it a bit to insert our
	 * face exactly. */
	if (anim1 != tmp->face)
	{
		temp = xstart-box.x;

		box.x = 0;
		box.w = FaceList[tmp->face].sprite->bitmap->w;
		xstart = temp;

		temp = ystart - box.y + (FaceList[anim1].sprite->bitmap->h - FaceList[tmp->face].sprite->bitmap->h);
		box.y = 0;
		box.h = FaceList[tmp->face].sprite->bitmap->h;
		ystart = temp;

		if (xstart < 0)
		{
			box.x = -xstart;
			box.w = FaceList[tmp->face].sprite->bitmap->w + xstart;

			if (box.w > 32)
				box.w = 32;

			xstart = 0;
		}
		else
		{
			if (box.w + xstart > 32)
				box.w -= ((box.w + xstart) - 32);
		}

		if (ystart < 0)
		{
			box.y = -ystart;
			box.h = FaceList[tmp->face].sprite->bitmap->h + ystart;

			if (box.h > 32)
				box.h = 32;

			ystart = 0;
		}
		else
		{
			if (box.h + ystart > 32)
				box.h -= ((box.h + ystart) - 32);
		}
	}

	if (tmp->flags & F_INVISIBLE)
		bltfx.flags = BLTFX_FLAG_SRCALPHA | BLTFX_FLAG_GREY;

	if (tmp->flags & F_ETHEREAL)
		bltfx.flags = BLTFX_FLAG_SRCALPHA;

	sprite_blt(FaceList[tmp->face].sprite, x + xstart, y + ystart, &box, &bltfx);

	return 1;
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
void blt_inv_item(object *tmp, int x, int y)
{
	int fire_ready;

	blt_inv_item_centered(tmp, x, y);

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

		StringBlt(ScreenSurface, &Font6x3Out, buf, x + (ICONDEFLEN / 2) - (get_string_pixel_length(buf, &Font6x3Out) / 2), y + 18, COLOR_WHITE, NULL, NULL);
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
		sprite_blt(Bitmaps[BITMAP_LOCK], x, y + ICONDEFLEN - Bitmaps[BITMAP_LOCK]->bitmap->w - 2, NULL, NULL);
	}

	if (tmp->flags & F_MAGIC)
	{
		sprite_blt(Bitmaps[BITMAP_MAGIC], x + ICONDEFLEN - Bitmaps[BITMAP_MAGIC]->bitmap->w - 2, y + ICONDEFLEN - Bitmaps[BITMAP_MAGIC]->bitmap->h - 2, NULL, NULL);
	}

	if (tmp->flags & F_DAMNED)
	{
		sprite_blt(Bitmaps[BITMAP_DAMNED], x + ICONDEFLEN - Bitmaps[BITMAP_DAMNED]->bitmap->w - 2, y, NULL, NULL);
	}
	else if (tmp->flags & F_CURSED)
	{
		sprite_blt(Bitmaps[BITMAP_CURSED], x + ICONDEFLEN - Bitmaps[BITMAP_CURSED]->bitmap->w - 2, y, NULL, NULL);
	}

	if (tmp->flags & F_TRAPPED)
	{
		sprite_blt(Bitmaps[BITMAP_TRAPPED], x + ICONDEFLEN / 2 - Bitmaps[BITMAP_TRAPPED]->bitmap->w / 2, y + ICONDEFLEN / 2 - Bitmaps[BITMAP_TRAPPED]->bitmap->h / 2, NULL, NULL);
	}
}
