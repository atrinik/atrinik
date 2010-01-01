/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
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
 * Handles the player shop GUI code. */

#include <include.h>

/** The shop GUI */
_shop_gui_struct *shop_gui = NULL;

/**
 * Array of all possible coin types, including the coin value and
 * name. Used by shop_int2price() and shop_price2int(). */
coins_struct coins[] =
{
	{"m",	10000000},
	{"g",	10000},
	{"s",	100},
	{"c",	1},
};

/** Size of the coin types array */
#define COINS_ARRAY_SIZE (int) (sizeof(coins) / sizeof(coins_struct))

/**
 * Show the shop GUI widget.
 * @param x X position of the widget
 * @param y Y position of the widget */
void widget_show_shop(int x, int y)
{
	/* Show the main shop bitmap */
	sprite_blt(Bitmaps[BITMAP_SHOP], x, y, NULL, NULL);

	/* Add a close button at top right */
	shop_add_close_button(x + Bitmaps[BITMAP_SHOP]->bitmap->w - 10, y + 2);

	/* Sanity check, because the close button can close the GUI */
	if (!shop_gui)
	{
		return;
	}

	/* Show a title for the shop widget */
	if (shop_gui->shop_state == SHOP_STATE_BUYING)
	{
		char buf[MAX_BUF];

		snprintf(buf, sizeof(buf), "Shop: %s", shop_gui->shop_owner);

		StringBlt(ScreenSurface, &SystemFont, buf, x + 4, y + 1, COLOR_BLACK, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, buf, x + 5, y + 2, COLOR_WHITE, NULL, NULL);
	}
	else
	{
		StringBlt(ScreenSurface, &SystemFont, "Shop", x + 4, y + 1, COLOR_BLACK, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, "Shop", x + 5, y + 2, COLOR_WHITE, NULL, NULL);
	}

	/* Determine the right kind of buttons and inputs to show */
	switch (shop_gui->shop_state)
	{
		/* Show "Open" button and Price/Number inputs */
		case SHOP_STATE_NONE:
			if (shop_gui->selected_tag)
			{
				shop_add_button(x + 4, y + Bitmaps[BITMAP_SHOP]->bitmap->h - Bitmaps[BITMAP_DIALOG_BUTTON_UP]->bitmap->h - 3, "Remove");
			}

			shop_add_button(x + Bitmaps[BITMAP_SHOP]->bitmap->w - Bitmaps[BITMAP_DIALOG_BUTTON_UP]->bitmap->w - 4, y + Bitmaps[BITMAP_SHOP]->bitmap->h - Bitmaps[BITMAP_DIALOG_BUTTON_UP]->bitmap->h - 3, "Open");

			if (!shop_gui)
			{
				return;
			}

			StringBlt(ScreenSurface, &SystemFont, "Price:", x + 5, y + Bitmaps[BITMAP_SHOP]->bitmap->h - 56, COLOR_BLACK, NULL, NULL);

			if (shop_gui->input_type == SHOP_INPUT_TYPE_PRICE)
			{
				StringBlt(ScreenSurface, &SystemFont, "Price:", x + 6, y + Bitmaps[BITMAP_SHOP]->bitmap->h - 55, COLOR_HGOLD, NULL, NULL);
			}
			else
			{
				StringBlt(ScreenSurface, &SystemFont, "Price:", x + 6, y + Bitmaps[BITMAP_SHOP]->bitmap->h - 55, COLOR_WHITE, NULL, NULL);
			}

			sprite_blt(Bitmaps[BITMAP_SHOP_INPUT], x + Bitmaps[BITMAP_SHOP]->bitmap->w - Bitmaps[BITMAP_SHOP_INPUT]->bitmap->w - 4, y + Bitmaps[BITMAP_SHOP]->bitmap->h - 57, NULL, NULL);

			StringBlt(ScreenSurface, &SystemFont, "Number:", x + 5, y + Bitmaps[BITMAP_SHOP]->bitmap->h - 39, COLOR_BLACK, NULL, NULL);

			if (shop_gui->input_type == SHOP_INPUT_TYPE_NROF)
			{
				StringBlt(ScreenSurface, &SystemFont, "Number:", x + 6, y + Bitmaps[BITMAP_SHOP]->bitmap->h - 38, COLOR_HGOLD, NULL, NULL);
			}
			else
			{
				StringBlt(ScreenSurface, &SystemFont, "Number:", x + 6, y + Bitmaps[BITMAP_SHOP]->bitmap->h - 38, COLOR_WHITE, NULL, NULL);
			}

			sprite_blt(Bitmaps[BITMAP_SHOP_INPUT], x + Bitmaps[BITMAP_SHOP]->bitmap->w - Bitmaps[BITMAP_SHOP_INPUT]->bitmap->w - 4, y + Bitmaps[BITMAP_SHOP]->bitmap->h - 40, NULL, NULL);

			break;

		/* Shop "Buy" and "Examine" buttons and Number input */
		case SHOP_STATE_BUYING:
			if (shop_gui->selected_tag)
			{
				shop_add_button(x + 4, y + Bitmaps[BITMAP_SHOP]->bitmap->h - Bitmaps[BITMAP_DIALOG_BUTTON_UP]->bitmap->h - 3, "Buy");

				shop_add_button(x + Bitmaps[BITMAP_SHOP]->bitmap->w - Bitmaps[BITMAP_DIALOG_BUTTON_UP]->bitmap->w - 4, y + Bitmaps[BITMAP_SHOP]->bitmap->h - Bitmaps[BITMAP_DIALOG_BUTTON_UP]->bitmap->h - 3, "Examine");
			}

			StringBlt(ScreenSurface, &SystemFont, "Number:", x + 5, y + Bitmaps[BITMAP_SHOP]->bitmap->h - 56, COLOR_BLACK, NULL, NULL);

			if (shop_gui->input_type == SHOP_INPUT_TYPE_NROF)
			{
				StringBlt(ScreenSurface, &SystemFont, "Number:", x + 6, y + Bitmaps[BITMAP_SHOP]->bitmap->h - 55, COLOR_HGOLD, NULL, NULL);
			}
			else
			{
				StringBlt(ScreenSurface, &SystemFont, "Number:", x + 6, y + Bitmaps[BITMAP_SHOP]->bitmap->h - 55, COLOR_WHITE, NULL, NULL);
			}

			sprite_blt(Bitmaps[BITMAP_SHOP_INPUT], x + Bitmaps[BITMAP_SHOP]->bitmap->w - Bitmaps[BITMAP_SHOP_INPUT]->bitmap->w - 4, y + Bitmaps[BITMAP_SHOP]->bitmap->h - 57, NULL, NULL);

			break;
	}

	/* If there are items to show */
	if (shop_gui->shop_items)
	{
		_shop_struct *shop_item_tmp;
		int item_x = 36, item_y = 18, i = 1, found_selected = 0;
		int mx, my, mb;
		char buf[MAX_BUF];

		mb = SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT);

		/* Loop through the items */
		for (shop_item_tmp = shop_gui->shop_items; shop_item_tmp; shop_item_tmp = shop_item_tmp->next)
		{
			item *shop_item;

			/* Item is gone */
			if (shop_item_tmp->tag == -1)
			{
				continue;
			}

			shop_item = locate_item(shop_item_tmp->tag);

			if (!shop_item)
			{
				continue;
			}

			/* If we clicked on the item, set the selected tag accordingly */
			if (mx > x + item_x && mx < x + item_x + 32 && my > y + item_y && my < y + item_y + 32 && mb && mb_clicked)
			{
				shop_gui->selected_tag = shop_item_tmp->tag;

				switch (shop_gui->shop_state)
				{
					case SHOP_STATE_NONE:
						shop_gui->current_cursor_pos = strlen(shop_item_tmp->price_buf);

						break;

					case SHOP_STATE_BUYING:
						snprintf(buf, sizeof(buf), "%d", shop_item_tmp->nrof);

						shop_gui->current_cursor_pos = strlen(buf);

						break;
				}
			}

			/* If this is the selected item, show a marker */
			if (shop_item_tmp->tag == shop_gui->selected_tag)
			{
				found_selected = 1;

				sprite_blt(Bitmaps[BITMAP_INVSLOT], x + item_x, y + item_y, NULL, NULL);

				switch (shop_gui->shop_state)
				{
					/* Show "Open" button and Price/Number inputs */
					case SHOP_STATE_NONE:
						snprintf(buf, sizeof(buf), "%s", shop_item_tmp->price_buf);

						strcpy(buf, shop_show_input(buf, &SystemFont, Bitmaps[BITMAP_SHOP_INPUT]->bitmap->w - 5, shop_gui->input_type == SHOP_INPUT_TYPE_PRICE ? 1 : 0));

						StringBlt(ScreenSurface, &SystemFont, buf, x + Bitmaps[BITMAP_SHOP]->bitmap->w - Bitmaps[BITMAP_SHOP_INPUT]->bitmap->w, y + Bitmaps[BITMAP_SHOP]->bitmap->h - 55, COLOR_BLACK, NULL, NULL);
						StringBlt(ScreenSurface, &SystemFont, buf, x + Bitmaps[BITMAP_SHOP]->bitmap->w - Bitmaps[BITMAP_SHOP_INPUT]->bitmap->w - 1, y + Bitmaps[BITMAP_SHOP]->bitmap->h - 56, COLOR_WHITE, NULL, NULL);

						snprintf(buf, sizeof(buf), "%d", shop_item_tmp->nrof);

						strcpy(buf, shop_show_input(buf, &SystemFont, Bitmaps[BITMAP_SHOP_INPUT]->bitmap->w - 5, shop_gui->input_type == SHOP_INPUT_TYPE_NROF ? 1 : 0));

						StringBlt(ScreenSurface, &SystemFont, buf, x + Bitmaps[BITMAP_SHOP]->bitmap->w - Bitmaps[BITMAP_SHOP_INPUT]->bitmap->w, y + Bitmaps[BITMAP_SHOP]->bitmap->h - 38, COLOR_BLACK, NULL, NULL);
						StringBlt(ScreenSurface, &SystemFont, buf, x + Bitmaps[BITMAP_SHOP]->bitmap->w - Bitmaps[BITMAP_SHOP_INPUT]->bitmap->w - 1, y + Bitmaps[BITMAP_SHOP]->bitmap->h - 39, COLOR_WHITE, NULL, NULL);

						break;

					case SHOP_STATE_BUYING:
						snprintf(buf, sizeof(buf), "%d", shop_item_tmp->nrof);

						strcpy(buf, shop_show_input(buf, &SystemFont, Bitmaps[BITMAP_SHOP_INPUT]->bitmap->w - 5, shop_gui->input_type == SHOP_INPUT_TYPE_NROF ? 1 : 0));

						StringBlt(ScreenSurface, &SystemFont, buf, x + Bitmaps[BITMAP_SHOP]->bitmap->w - Bitmaps[BITMAP_SHOP_INPUT]->bitmap->w, y + Bitmaps[BITMAP_SHOP]->bitmap->h - 55, COLOR_BLACK, NULL, NULL);
						StringBlt(ScreenSurface, &SystemFont, buf, x + Bitmaps[BITMAP_SHOP]->bitmap->w - Bitmaps[BITMAP_SHOP_INPUT]->bitmap->w - 1, y + Bitmaps[BITMAP_SHOP]->bitmap->h - 56, COLOR_WHITE, NULL, NULL);

						break;
				}

				if (shop_gui->shop_state != SHOP_STATE_OPEN)
				{
					snprintf(buf, sizeof(buf), "Price:%s", shop_int2price(shop_item_tmp->price));

					StringBlt(ScreenSurface, &SystemFont, buf, x + Bitmaps[BITMAP_SHOP]->bitmap->w - Bitmaps[BITMAP_SHOP_INPUT]->bitmap->w, y + Bitmaps[BITMAP_SHOP]->bitmap->h - 75, COLOR_WHITE, NULL, NULL);
				}
			}

			blt_inv_item(shop_item, x + item_x, y + item_y, shop_item_tmp->nrof);

			/* Adjust the item X position */
			item_x += 32;

			/* If we hit fourth item, adjust the positions */
			if (i == 4)
			{
				item_x = 36;
				item_y += 32;
				i = 0;
			}

			i++;
		}

		if (shop_gui->selected_tag && !found_selected)
		{
			shop_gui->selected_tag = 0;
		}
	}
}

/**
 * Open the player shop. */
void shop_open()
{
	_shop_struct *shop_item_tmp;
	char buf[HUGE_BUF];

	/* Need at least one item */
	if (shop_gui->shop_items_count < 1)
	{
		draw_info("You need to have at least one item to sell in order to open a shop.", COLOR_RED);
		return;
	}

	strcpy(buf, "shop open");

	/* Loop through the shop items */
	for (shop_item_tmp = shop_gui->shop_items; shop_item_tmp; shop_item_tmp = shop_item_tmp->next)
	{
		char tmp_buf[MAX_BUF];
		item *shop_item = locate_item(shop_item_tmp->tag);

		/* If the item could not be found, something is wrong */
		if (!shop_item)
		{
			draw_info("Unable to locate shop item from tag.", COLOR_RED);
			clear_shop(0);
			return;
		}

		/* Some checking */
		if (shop_item_tmp->price < 1 || shop_item_tmp->price > MAX_PRICE_VALUE)
		{
			draw_info_format(COLOR_RED, "The item %s doesn't have a valid price.", shop_item->s_name);
			return;
		}
		else if (shop_item_tmp->nrof < 1 || shop_item_tmp->nrof > MAX_PRICE_VALUE)
		{
			draw_info_format(COLOR_RED, "The item %s doesn't have a valid nrof.", shop_item->s_name);
			return;
		}

		/* Store the item tag, nrof and price */
		snprintf(tmp_buf, sizeof(tmp_buf), "|%d:%d:%d", shop_item_tmp->tag, shop_item_tmp->nrof, shop_item_tmp->price);

		/* Append it to the socket string */
		strncat(buf, tmp_buf, sizeof(buf) - strlen(buf) - 1);
	}

	/* Write the socket string to server */
	cs_write_string(csocket.fd, buf, strlen(buf));

	/* The shop is now open */
	shop_gui->shop_state = SHOP_STATE_OPEN;
}

/**
 * Buy a shop item. */
void shop_buy_item()
{
	char buf[MAX_BUF];
	item *shop_item = locate_item(shop_gui->selected_tag);
	_shop_struct *shop_item_tmp;

	/* Loop through the shop items */
	for (shop_item_tmp = shop_gui->shop_items; shop_item_tmp; shop_item_tmp = shop_item_tmp->next)
	{
		/* If the tag matches */
		if (shop_item_tmp->tag == shop_item->tag)
		{
			/* If the nrof is the same, we will want to remove the item from the list */
			if (shop_item_tmp->nrof == shop_item->nrof)
			{
				shop_remove_item(shop_item->tag);
			}

			break;
		}
	}

	/* Sanity check */
	if (!shop_item_tmp)
	{
		return;
	}

	/* If the nrof is not the same, decrease the shop inventory item's nrof */
	if (shop_item_tmp->nrof != shop_item->nrof)
	{
		shop_item->nrof -= shop_item_tmp->nrof;
	}

	/* Store the information in a buffer */
	snprintf(buf, sizeof(buf), "shop buy %s %d %d", shop_gui->shop_owner, shop_item->tag, shop_item_tmp->nrof);

	/* Send the buffer */
	cs_write_string(csocket.fd, buf, strlen(buf));
}

/**
 * Initialize the shop GUI.
 * @param shop_state Shop state. See
 * {@link shop_states shop state declarations} for possible values. */
void initialize_shop(int shop_state)
{
	shop_gui = (_shop_gui_struct *) malloc(sizeof(_shop_gui_struct));

	/* Initialize defaults */
	shop_gui->shop_state = shop_state;
	shop_gui->shop_items_count = 0;
	shop_gui->shop_items = NULL;
	shop_gui->selected_tag = 0;

	shop_gui->current_cursor_pos = 0;
	shop_gui->input_count = 0;

	/* Determine correct input type */
	switch (shop_state)
	{
		case SHOP_STATE_NONE:
			shop_gui->input_type = SHOP_INPUT_TYPE_PRICE;

			break;

		case SHOP_STATE_BUYING:
			shop_gui->input_type = SHOP_INPUT_TYPE_NROF;

			break;
	}

	shop_gui->shop_owner[0] = '\0';

	map_udate_flag = 2;

	/* Show the shop widget now */
	cur_widget[SHOP_ID].show = 1;
}

/**
 * Clear and deinitialize shop GUI.
 * @param send_to_server Whether to send a command to the server to free
 * and close the shop. */
void clear_shop(int send_to_server)
{
	_shop_struct *shop_item_tmp, *shop_item;

	/* Hide the shop widget */
	cur_widget[SHOP_ID].show = 0;

	/* Sanity check */
	if (!shop_gui)
	{
		return;
	}

	if (send_to_server)
	{
		cs_write_string(csocket.fd, "shop close", 10);
	}

	shop_item = shop_gui->shop_items;

	/* Loop through the shop items */
	while (shop_item)
	{
		shop_item_tmp = shop_item->next;

		free(shop_item);

		shop_item = shop_item_tmp;
	}

	/* Free the shop GUI */
	free(shop_gui);

	shop_gui = NULL;
}

/**
 * Add a close button.
 * @param x X position of the button
 * @param y Y position of the button */
void shop_add_close_button(int x, int y)
{
	int mx, my, mb;

	mb = SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT);

	/* Show X as background */
	StringBlt(ScreenSurface, &SystemFont, "X", x + 1, y + 1, COLOR_BLACK, NULL, NULL);

	/* If mouse is over it */
	if (mx > x - 3 && mx < x + 7 && my > y && my < y + 12)
	{
		/* Show the X in gold */
		StringBlt(ScreenSurface, &SystemFont, "X", x, y, COLOR_HGOLD, NULL, NULL);

		/* If it was clicked, close the shop */
		if (mb && mb_clicked)
		{
			clear_shop(1);
		}
		/* Otherwise display a tooltip */
		else
		{
			if (shop_gui->shop_state == SHOP_STATE_NONE)
			{
				show_tooltip(mx, my, "Click to cancel opening the shop");
			}
			else
			{
				show_tooltip(mx, my, "Close the shop window");
			}
		}
	}
	/* Mouse is not over it, show in white */
	else
	{
		StringBlt(ScreenSurface, &SystemFont, "X", x, y, COLOR_WHITE, NULL, NULL);
	}
}

/**
 * Add a button in the shop GUI.
 * @param x X position of the button
 * @param y Y position of the button
 * @param text Text of the button */
void shop_add_button(int x, int y, char *text)
{
	int mx, my, mb, text_x;

	mb = SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT);

	sprite_blt(Bitmaps[BITMAP_DIALOG_BUTTON_UP], x, y, NULL, NULL);

	/* Calculate text X position */
	text_x = x + Bitmaps[BITMAP_DIALOG_BUTTON_UP]->bitmap->w / 2 - strlen(text) * 2 - 1;

	/* Display the button text background in black */
	StringBlt(ScreenSurface, &SystemFont, text, text_x + 1, y + 2, COLOR_BLACK, NULL, NULL);

	/* If the mouse is over it */
	if (mx > x && my > y && mx < x + Bitmaps[BITMAP_DIALOG_BUTTON_UP]->bitmap->w && my < y + Bitmaps[BITMAP_DIALOG_BUTTON_UP]->bitmap->h)
	{
		static int delta = 0;

		/* If it was clicked */
		if (shop_gui && mb && mb_clicked && !(delta++ & 7))
		{
			/* If we are buying */
			if (shop_gui->shop_state == SHOP_STATE_BUYING)
			{
				/* For Examine button, do an examine */
				if (strcmp(text, "Examine") == 0)
				{
					char buf[MAX_BUF];

					snprintf(buf, sizeof(buf), "shop examine %s %d", shop_gui->shop_owner, shop_gui->selected_tag);

					cs_write_string(csocket.fd, buf, strlen(buf));
				}
				/* Otherwise buy the item */
				else if (strcmp(text, "Buy") == 0)
				{
					shop_buy_item();
				}
			}
			else
			{
				if (strcmp(text, "Open") == 0)
				{
					shop_open();
				}
				else if (strcmp(text, "Remove") == 0)
				{
					shop_remove_item(shop_gui->selected_tag);
				}
			}
		}

		/* Show the text in gold */
		StringBlt(ScreenSurface, &SystemFont, text, text_x, y + 1, COLOR_HGOLD, NULL, NULL);
	}
	/* Mouse is not over the button, show the text in white */
	else
	{
		StringBlt(ScreenSurface, &SystemFont, text, text_x, y + 1, COLOR_WHITE, NULL, NULL);
	}
}

/**
 * Put an item to the shop GUI. Called when dropping a dragged item.
 * @param x X position of the item
 * @param y Y position of the item
 * @return 1 if the XY positions were within the shop GUI, 0 otherwise */
int shop_put_item(int x, int y)
{
	/* If we are in the shop GUI */
	if (x > cur_widget[SHOP_ID].x1 && x < cur_widget[SHOP_ID].x1 + Bitmaps[BITMAP_SHOP]->bitmap->w && y > cur_widget[SHOP_ID].y1 && y < cur_widget[SHOP_ID].y1 + Bitmaps[BITMAP_SHOP]->bitmap->h)
	{
		_shop_struct *shop_item_tmp;
		item *item_object;

		/* If we're not opening or the item limit was reached, return */
		if (shop_gui->shop_state != SHOP_STATE_NONE || shop_gui->shop_items_count >= SHOP_MAX_ITEMS)
		{
			draggingInvItem(DRAG_NONE);
			itemExamined = 0;

			return 1;
		}

		/* Loop through shop items */
		for (shop_item_tmp = shop_gui->shop_items; shop_item_tmp; shop_item_tmp = shop_item_tmp->next)
		{
			/* If the item is already in the GUI, return */
			if (shop_item_tmp->tag == cpl.win_inv_tag)
			{
				draggingInvItem(DRAG_NONE);
				itemExamined = 0;

				return 1;
			}
		}

		/* Locate the item */
		item_object = locate_item(cpl.win_inv_tag);

		/* Sanity check, shouldn't happen */
		if (!item_object)
		{
			draggingInvItem(DRAG_NONE);
			itemExamined = 0;

			return 1;
		}

		if (item_object->itype == TYPE_MONEY)
		{
			draw_info_format(COLOR_WHITE, "The %s is not allowed to be sold in a player shop.", item_object->s_name);

			draggingInvItem(DRAG_NONE);
			itemExamined = 0;

			return 1;
		}

		/* Allocate a new item */
		shop_item_tmp = (_shop_struct *) malloc(sizeof(_shop_struct));

		/* Initialize values for the object */
		shop_item_tmp->nrof = item_object->nrof;
		shop_item_tmp->price = 0;
		shop_item_tmp->price_buf[0] = '\0';
		shop_item_tmp->tag = cpl.win_inv_tag;
		shop_item_tmp->next = NULL;

		/* One more shop item */
		shop_gui->shop_items_count++;

		/* Set the selected tag to the object's tag */
		shop_gui->selected_tag = shop_item_tmp->tag;

		/* Easier if there are no shop items yet */
		if (!shop_gui->shop_items)
		{
			shop_gui->shop_items = shop_item_tmp;
		}
		/* Otherwise we want to append the item to the end of the list */
		else
		{
			_shop_struct *shop_item_next = shop_gui->shop_items;
			int i;

			/* Loop until we find the end of the list */
			for (i = 1; i < shop_gui->shop_items_count - 1 && shop_item_next; i++, shop_item_next = shop_item_next->next)
			{
			}

			/* Append it to the end of the list */
			shop_item_next->next = shop_item_tmp;
		}

		draggingInvItem(DRAG_NONE);
		itemExamined = 0;

		return 1;
	}

	return 0;
}

/**
 * Remove an item from the linked list of shop items.
 * @param tag Tag of the item to remove. */
void shop_remove_item(sint32 tag)
{
	_shop_struct *currP, *prevP = NULL;

	/* Visit each node, maintaining a pointer to the previous node we
	 * just visited. */
	for (currP = shop_gui->shop_items; currP != NULL; prevP = currP, currP = currP->next)
	{
		/* Found it. */
		if (currP->tag == tag)
		{
			if (prevP == NULL)
			{
				/* Fix beginning pointer. */
				shop_gui->shop_items = currP->next;
			}
			else
			{
				/* Fix previous node's next to skip over the removed
				 * node. */
				prevP->next = currP->next;

				/* If we had a selected tag, and the current pointer's
				 * tag matches, change the selected tag to previous
				 * pointer's tag. */
				if (shop_gui->selected_tag && currP->tag == shop_gui->selected_tag)
				{
					shop_gui->selected_tag = currP->tag;
				}
			}

			/* Deallocate the node. */
			free(currP);

			/* Decrease the count of shop items */
			shop_gui->shop_items_count--;

			/* Done searching. */
			break;
		}
	}
}

/**
 * Handle key presses if shop widget is open.
 * @param key SDL Keyboard event
 * @return 1 if we did something with the keypress and do not want the
 * caller function to do any other handling for the event, 0 otherwise */
int check_shop_keys(SDL_KeyboardEvent *key)
{
	char c;
	char buf[MAX_BUF];
	_shop_struct *shop_item_tmp;
	int i, rtn = 0;

	/* If no shop gui or no selected tag, we won't do anything */
	if (!shop_gui || !shop_gui->selected_tag || shop_gui->shop_state == SHOP_STATE_OPEN)
	{
		return 0;
	}

	/* Loop through the items */
	for (shop_item_tmp = shop_gui->shop_items; shop_item_tmp; shop_item_tmp = shop_item_tmp->next)
	{
		/* Found the one we're looking for? */
		if (shop_item_tmp->tag == shop_gui->selected_tag)
		{
			break;
		}
	}

	/* Sanity check */
	if (!shop_item_tmp)
	{
		return 0;
	}

	/* Determine which value to store in the buffer */
	if (shop_gui->input_type == SHOP_INPUT_TYPE_NROF)
	{
		snprintf(buf, sizeof(buf), "%d", shop_item_tmp->nrof);
	}
	else
	{
		snprintf(buf, sizeof(buf), "%s", shop_item_tmp->price_buf);
	}

	/* Calculate the input count */
	shop_gui->input_count = strlen(buf);

	if (key->type == SDL_KEYDOWN)
	{
		int changed = 0;

		switch (key->keysym.sym)
		{
			/* Handle backspace key */
			case SDLK_BACKSPACE:
				if (shop_gui->input_count && shop_gui->current_cursor_pos)
				{
					int ii;

					/* Actual position of the cursor */
					ii = shop_gui->current_cursor_pos;

					/* Where we will end up, by default one character back */
					i = ii - 1;

					if (key->keysym.mod & KMOD_CTRL)
					{
						/* Jumps eventual whitespaces */
						while (buf[i] == ' ' && i >= 0)
						{
							i--;
						}

						/* Jump a word */
						while (buf[i] != ' ' && i >= 0)
						{
							i--;
						}

						/* We end up at the beginning of the current word */
						i++;
					}

					/* This loop copies even the terminating \0 of the buffer */
					while (ii <= shop_gui->input_count)
					{
						buf[i++] = buf[ii++];
					}

					shop_gui->current_cursor_pos -= (ii - i);
					shop_gui->input_count -= (ii - i);

					/* There was a change in the buffer */
					changed = 1;
				}

				rtn = 1;

				break;

			/* Handle tab key, to switch between inputs */
			case SDLK_TAB:
				shop_gui->input_type++;

				/* Sanity checks */
				if (shop_gui->input_type > SHOP_INPUT_TYPES || (shop_gui->shop_state == SHOP_STATE_BUYING && shop_gui->input_type == SHOP_INPUT_TYPE_PRICE))
				{
					shop_gui->input_type = SHOP_INPUT_TYPE_NROF;
				}

				/* Again determine which value to store in the buffer, since the input type changed */
				if (shop_gui->input_type == SHOP_INPUT_TYPE_NROF)
				{
					snprintf(buf, sizeof(buf), "%d", shop_item_tmp->nrof);
				}
				else
				{
					snprintf(buf, sizeof(buf), "%s", shop_item_tmp->price_buf);
				}

				/* Recalculate input count and cursor position */
				shop_gui->input_count = shop_gui->current_cursor_pos = strlen(buf);

				return 1;

				break;

			/* Move the cursor position to the left */
			case SDLK_LEFT:
				shop_gui->current_cursor_pos--;

				break;

			/* Move the cursor position to the right */
			case SDLK_RIGHT:
				shop_gui->current_cursor_pos++;

				break;

			/* Handle anything else */
			default:
				if ((key->keysym.unicode & 0xFF80) == 0)
				{
					c = key->keysym.unicode & 0x7F;
				}

				c = key->keysym.unicode & 0xff;

				/* If the character is a number, or the input type is price then accept " ", "c", "s", "g" and "m" as well */
				if ((c >= 48 && c <= 57) || (shop_gui->input_type == SHOP_INPUT_TYPE_PRICE && (c == ' ' || c == 'c' || c == 's' || c == 'g' || c == 'm')))
				{
					i = shop_gui->input_count;

					while (i >= shop_gui->current_cursor_pos)
					{
						buf[i + 1] = buf[i];
						i--;
					}

					buf[shop_gui->current_cursor_pos] = c;
					shop_gui->current_cursor_pos++;
					shop_gui->input_count++;
					buf[shop_gui->input_count] = 0;

					shop_gui->current_cursor_pos++;

					rtn = 1;

					changed = 1;
				}

				break;
		}

		/* If there was a change in the buffer */
		if (changed)
		{
			int buf_int = atoi(buf);

			/* Only allow numbers in certain range, price input is an exception */
			if (shop_gui->input_type == SHOP_INPUT_TYPE_PRICE || (buf_int >= 0 && buf_int <= MAX_PRICE_VALUE))
			{
				if (shop_gui->input_type == SHOP_INPUT_TYPE_NROF)
				{
					shop_item_tmp->nrof = buf_int;

					/* Store the new integer in the buffer */
					snprintf(buf, sizeof(buf), "%d", buf_int);

					/* Recalculate input count and cursor position */
					shop_gui->input_count = shop_gui->current_cursor_pos = strlen(buf);
				}
				else
				{
					/* Store the price buffer */
					snprintf(shop_item_tmp->price_buf, sizeof(shop_item_tmp->price_buf), "%s", buf);

					/* Adjust the value of the item */
					shop_item_tmp->price = shop_price2int(buf);
				}
			}
		}
	}

	/* Sanity checks so the cursor position doesn't go over boundaries */
	if (shop_gui->current_cursor_pos < 0)
	{
		shop_gui->current_cursor_pos = 0;
	}
	else if (shop_gui->current_cursor_pos > shop_gui->input_count)
	{
		shop_gui->current_cursor_pos = shop_gui->input_count;
	}

	return rtn;
}

/**
 * Show shop input text and calculate the maximum width the string can
 * have so the text doesn't overflow the input area.
 * @param text The input text
 * @param font Font used to display the text
 * @param wlen Maximum width of the text
 * @param append_underscore Whether to append underscore at the end of
 * the input text.
 * @return Char pointer to the string to display */
char *shop_show_input(char *text, struct _Font *font, int wlen, int append_underscore)
{
	int i, j, len;
	char buf[MAX_INPUT_STR];
	static char buf_text[MAX_INPUT_STR];

	strcpy(buf, text);

	len = strlen(buf);

	/* Are we going to append an underscore to the text? */
	if (append_underscore)
	{
		while (len >= shop_gui->current_cursor_pos)
		{
			buf[len + 1] = buf[len];
			len--;
		}

		buf[shop_gui->current_cursor_pos] = '_';
	}

	for (len = 25, i = shop_gui->current_cursor_pos; i >= 0; i--)
	{
		if (!buf[i])
			continue;

		if (len + font->c[(int) (buf[i])].w + font->char_offset >= wlen)
		{
			i--;

			break;
		}

		len += font->c[(int) (buf[i])].w + font->char_offset;
	}

	len -= 25;

	for (j = shop_gui->current_cursor_pos; j <= (int) strlen(buf); j++)
	{
		if (len + font->c[(int) (buf[j])].w + font->char_offset >= wlen)
		{
			break;
		}

		len += font->c[(int) (buf[j])].w + font->char_offset;
	}

	buf[j] = 0;

	strcpy(buf_text, &buf[++i]);

	return buf_text;
}

/**
 * Parse a price string from the shop interface to an integer.
 *
 * For example, the string <pre>10 m 15 g 90 s 40 c</pre> would become
 * <pre>100159040</pre>
 * @param text The string to parse
 * @return An integer representation of the price string. */
int shop_price2int(char *text)
{
	int pos = 0, total_value = 0;
	char *word, value_buf[MAX_BUF];

	while ((word = get_word_from_string(text, &pos)))
	{
		int i = 0, flag = *word;

		while (*(word + i) != '\0')
		{
			if (*(word + i) < '0' || *(word + i) > '9')
			{
				flag = 0;
			}

			i++;
		}

		/* If still set, we have a valid number in the word string */
		if (flag)
		{
			int value = atoi(word);

			/* A valid number - now let's look if we have a valid money keyword */
			if (value > 0 && value < 100000)
			{
				if ((word = get_word_from_string(text, &pos)) && *word != '\0')
				{
					int len = strlen(word);

					for (i = 0; i < COINS_ARRAY_SIZE; i++)
					{
						if (!strncasecmp(coins[i].name, word, len))
						{
							total_value += value * coins[i].value;

							break;
						}
					}
				}
			}
		}
	}

	snprintf(value_buf, sizeof(value_buf), "%d", total_value);

	if (total_value > MAX_PRICE_VALUE || atoi(value_buf) < 1)
	{
		return 0;
	}

	return total_value;
}

/**
 * Does the reverse of shop_price2int().
 * @param value Integer value to parse
 * @return String representation of the given integer value */
char *shop_int2price(int value)
{
	static char value_buf[MAX_BUF];
	char buf[MAX_BUF];
	int i;

	value_buf[0] = '\0';

	/* Sanity check */
	if (value == 0)
	{
		return value_buf;
	}

	/* Loop through the money types */
	for (i = 0; i < COINS_ARRAY_SIZE; i++)
	{
		int num = (value / coins[i].value);

		/* If we got a value */
		if (num)
		{
			/* Store it in a temporary buffer */
			snprintf(buf, sizeof(buf), " %d %s", num, coins[i].name);

			/* And append it to the value buffer */
			strcat(value_buf, buf);

			/* Decrease the value */
			value -= num * coins[i].value;
		}
	}

	return value_buf;
}
