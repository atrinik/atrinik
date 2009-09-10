/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
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
 * Handles player shop structures and function prototypes. */

#include <include.h>

#ifndef SHOP_H
#define SHOP_H

/** Shop item structure. */
typedef struct shop_struct
{
	/** The item tag */
	sint32 tag;

	/** The item's price */
	int price;

	/** Character representation of the price */
	char price_buf[MAX_INPUT_STR];

	/** Number of items to sell */
	int nrof;

	/** Next item in this linked list */
	struct shop_struct *next;
} _shop_struct;

/** Shop GUI structure */
typedef struct shop_gui_struct
{
	/** The items in this GUI */
	_shop_struct *shop_items;

	/**
	 * State the shop is in, used for various things including
	 * buttons to display.
	 * @see shop_states */
	int shop_state;

	/** Number of items in the GUI */
	int shop_items_count;

	/** Object tag of the selected object */
	sint32 selected_tag;

	/**
	 * Input type being used, to determine what kind of input we are
	 * receiving.
	 * @see shop_input_types */
	int input_type;

	/** Count of characters in input */
	int input_count;

	/** Current position of cursor */
	int current_cursor_pos;

	/** Name of the player that owns this shop */
	char shop_owner[MAX_BUF];
} _shop_gui_struct;

/** Structure for money types */
typedef struct coins_struct
{
	/** Small buffer for the money name, only uses one letter */
	char name[2];

	/** The value of the money type */
	sint32 value;
} coins_struct;

extern _shop_gui_struct *shop_gui;

/**
 * @defgroup shop_input_types Shop Input Types
 * Used to determine which input type is being used, in example, whether
 * we're entering number of items to sell/buy or the price for the item.
 *@{*/
/** Entering a number */
#define SHOP_INPUT_TYPE_NROF	1
/** Entering price */
#define SHOP_INPUT_TYPE_PRICE	2
/** Count of all the above. Should be equal to the last input type. */
#define SHOP_INPUT_TYPES		2
/*@}*/

/**
 * @defgroup shop_states Shop States
 * Defines various shop states used when dealing with things like
 * the shop GUI buttons, ie, only display Open button when opening
 * the shop, Buy/Examine button when buying, etc.
 *@{*/
/** No state set. This means we are opening the shop */
#define SHOP_STATE_NONE		0
/** The shop is open */
#define SHOP_STATE_OPEN		1
/** We are buying from this shop */
#define SHOP_STATE_BUYING	2
/*@}*/

/**
 * Maximum number of items in a shop. This number is validated by the
 * server, so there's no point in increasing it in order to sell more
 * items. */
#define SHOP_MAX_ITEMS 28

/** Maximum price value the server will accept */
#define MAX_PRICE_VALUE 100000000

extern void widget_show_shop(int x, int y);
extern void shop_open();
extern void shop_buy_item();
extern void initialize_shop(int shop_state);
extern void clear_shop(int send_to_server);
extern void shop_add_close_button(int x, int y);
extern void shop_add_button(int x, int y, char *text);
extern int shop_put_item(int x, int y);
extern void shop_remove_item(sint32 tag);
extern int check_shop_keys(SDL_KeyboardEvent *key);
extern char *shop_show_input(char *text, struct _Font *font, int wlen, int append_underscore);
extern int shop_price2int(char *text);
extern char *shop_int2price(int value);

#endif
