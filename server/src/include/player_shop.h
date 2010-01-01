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
 * Handles player shop structures and defines. */

/**
 * Maximum number of items to allow in shop. The same limit is also set
 * for the client. */
#define PLAYER_SHOP_MAX_ITEMS 28

/**
 * Maximum integer value (nrof, price, etc) the server will accept for
 * shop items. */
#define PLAYER_SHOP_MAX_INT_VALUE 100000000

/**
 * Maximum distance a buyer can be from the seller in order for the
 * shop interface to work. */
#define PLAYER_SHOP_MAX_DISTANCE 3

/** The player shop item structure. */
typedef struct player_shop_struct
{
	/** Pointer to the object */
	object *item_object;

	/** Price of the object, set by the seller */
	sint32 price;

	/**
	 * Number of objects we're selling, so that you can sell only fifteen
	 * out of twenty bolts for example. */
	uint32 nrof;

	/** Pointer to the next object we're selling */
	struct player_shop_struct *next;
} player_shop;
