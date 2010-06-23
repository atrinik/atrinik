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
 * Controls player shop related functions. */

#include <global.h>

static void player_shop_send_items(player *pl, player *seller);
static void player_shop_close_interface(player *pl);
static void player_shop_free_structure(player *pl, int send_close);
static int shop_player_in_range(object *op, object *seller);

/**
 * Loop through seller's items on sale, and send the item tags to buyer's
 * shop inventory so they can be displayed and animated properly.
 *
 * Will also send all the data about the shop items to the buyer.
 * @param pl The player to send the items to
 * @param seller The selling player */
static void player_shop_send_items(player *pl, player *seller)
{
	player_shop *shop_item_tmp;
	int flags, anim_speed;
	size_t len;
	SockList sl;
	char item_n[MAX_BUF];
	char shop_buf[HUGE_BUF];

	/* Sanity check */
	if (!seller->shop_items)
	{
		return;
	}

	snprintf(shop_buf, sizeof(shop_buf), "Xload|%s", seller->ob->name);

	/* Allocate large enough buffer */
	sl.buf = malloc(MAXSOCKBUF);

	/* Set binary command to shop */
	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_SHOP);

	/* Loop through the seller's items on sale */
	for (shop_item_tmp = seller->shop_items; shop_item_tmp; shop_item_tmp = shop_item_tmp->next)
	{
		object *tmp = shop_item_tmp->item_object;
		char tmp_buf[MAX_BUF];

		if (!tmp)
		{
			continue;
		}

		/* Store the object's count, nrof and price */
		snprintf(tmp_buf, sizeof(tmp_buf), "|%d:%d:%d", shop_item_tmp->item_object->count, shop_item_tmp->nrof, shop_item_tmp->price);

		/* Append it to the shop buf */
		strncat(shop_buf, tmp_buf, sizeof(tmp_buf) - strlen(tmp_buf) - 1);

		flags = query_flags(tmp);

		/* Add the object's count */
		SockList_AddInt(&sl, tmp->count);

		/* Add the flags */
		SockList_AddInt(&sl, flags);

		/* Add face number */
		if (tmp->inv_face && QUERY_FLAG(tmp, FLAG_IDENTIFIED))
		{
			SockList_AddInt(&sl, tmp->inv_face->number);
		}
		else
		{
			SockList_AddInt(&sl, tmp->face->number);
		}

		/* Add the direction the object is facing */
		SockList_AddChar(&sl, tmp->facing);

		/* Add the object's base name */
		strncpy(item_n, query_base_name(tmp, seller->ob), 127);
		item_n[127] = 0;
		len = strlen(item_n) + 1;
		SockList_AddChar(&sl, (char) len);
		memcpy(sl.buf + sl.len, item_n, len);
		sl.len += len;

		/* Add animation ID */
		if (tmp->inv_animation_id)
		{
			SockList_AddShort(&sl, tmp->inv_animation_id);
		}
		else
		{
			SockList_AddShort(&sl, tmp->animation_id);
		}

		anim_speed = 0;

		/* If this is animated item, calculate animation speed for it */
		if (QUERY_FLAG(tmp, FLAG_ANIMATE))
		{
			if (tmp->anim_speed)
			{
				anim_speed = tmp->anim_speed;
			}
			else
			{
				if (FABS(tmp->speed) < 0.001)
				{
					anim_speed = 255;
				}
				else if (FABS(tmp->speed) >= 1.0)
				{
					anim_speed = 1;
				}
				else
				{
					anim_speed = (int) (1.0 / FABS(tmp->speed));
				}
			}

			if (anim_speed > 255)
			{
				anim_speed = 255;
			}
		}

		/* Add the animation speed */
		SockList_AddChar(&sl, (char) anim_speed);

		/* Add the nrof of it */
		SockList_AddInt(&sl, shop_item_tmp->nrof);
	}

	/* Send it to the player's socket */
	Send_With_Handling(&pl->socket, &sl);

	/* Send the shop items data to the player's socket */
	Write_String_To_Socket(&pl->socket, BINARY_CMD_SHOP, shop_buf, strlen(shop_buf));

	/* Free the buf we previously allocated */
	free(sl.buf);
}

/**
 * Send a player shop socket command to the player's client to close a
 * shop interface.
 * @param pl The player to send the command to */
static void player_shop_close_interface(player *pl)
{
	char sock_buf[MAX_BUF];

	strncpy(sock_buf, "Xclose", sizeof(sock_buf) - 1);

	Write_String_To_Socket(&pl->socket, BINARY_CMD_SHOP, sock_buf, strlen(sock_buf));
}

/**
 * Free a shop structure from player.
 * @param pl The player
 * @param send_close If nonzero, send a close command to the client. */
static void player_shop_free_structure(player *pl, int send_close)
{
	player_shop *shop_item_tmp, *shop_item;

	shop_item = pl->shop_items;

	/* Loop through the shop items */
	while (shop_item)
	{
		shop_item_tmp = shop_item->next;

		free(shop_item);

		shop_item = shop_item_tmp;
	}

	/* Assign the shop items pointer to NULL */
	pl->shop_items = NULL;

	/* Count of items will be 0 now */
	pl->shop_items_count = 0;

	if (send_close)
	{
		player_shop_close_interface(pl);
	}
}

/**
 * Check if player that wants to buy from someone's player shop is in
 * range to actually initiate the shop interface.
 *
 * Uses get_rangevector() to calculate the distance.
 * @param op Pointer to the buying player's object
 * @param seller Pointer to the selling player's object
 * @return 1 if the buyer is in range, 0 otherwise
 * @see PLAYER_SHOP_MAX_DISTANCE */
static int shop_player_in_range(object *op, object *seller)
{
	rv_vector rv;

	/* Check the distance */
	if (!get_rangevector(op, seller, &rv, RV_DIAGONAL_DISTANCE) || rv.distance > PLAYER_SHOP_MAX_DISTANCE)
	{
		return 0;
	}

	return 1;
}

/**
 * Open a player shop.
 *
 * The data passed includes the items, ie, their tags, nrof, price, etc.
 *
 * <b>Format:</b>
 * <pre>item_tag:nrof:price|item_tag:nrof:price|...</pre>
 *
 * <b>Example:</b>
 * <pre>25984:10:10000|28549:1:20200|27859:3:10010</pre>
 * @param data Data, with the shop items in it separated by | characters.
 * @param pl The player opening the shop */
void player_shop_open(char *data, player *pl)
{
	char *p;

	/* Sanity check */
	if (!data || QUERY_FLAG(pl->ob, FLAG_PLAYER_SHOP))
	{
		return;
	}

	p = strtok(data, "|");

	/* We must get all the items for the shop */
	while (p)
	{
		sint32 tag, price;
		int nrof;
		player_shop *shop_item_tmp;
		object *item_object;

		/* Get the tag, nrof and price */
		if (sscanf(p, "%d:%d:%d", &tag, &nrof, &price) != 3)
		{
			player_shop_free_structure(pl, 1);
			return;
		}

		/* If we hit a limit, just return. Client should prevent this */
		if (pl->shop_items_count >= PLAYER_SHOP_MAX_ITEMS)
		{
			player_shop_free_structure(pl, 1);
			return;
		}

		/* Some checking */
		if ((price < 1 || price > PLAYER_SHOP_MAX_INT_VALUE) || (nrof < 1 || nrof > PLAYER_SHOP_MAX_INT_VALUE))
		{
			return;
		}

		/* Find the object pointer from the player's inventory */
		item_object = esrv_get_ob_from_count(pl->ob, tag);

		/* No item object? Just return */
		if (!item_object)
		{
			player_shop_free_structure(pl, 1);
			return;
		}

		/* Some items cannot be sold in shops */
		if (IS_SYS_INVISIBLE(item_object) || QUERY_FLAG(item_object, FLAG_UNPAID) || QUERY_FLAG(item_object, FLAG_STARTEQUIP) || item_object->type == MONEY || item_object->quickslot || QUERY_FLAG(item_object, FLAG_INV_LOCKED))
		{
			new_draw_info_format(NDI_UNIQUE, pl->ob, "The %s is not allowed to be sold in a player shop.", query_name(item_object, NULL));
			player_shop_free_structure(pl, 1);
			return;
		}

		/* Loop through the items the player has already set up */
		for (shop_item_tmp = pl->shop_items; shop_item_tmp; shop_item_tmp = shop_item_tmp->next)
		{
			/* If we got a match, this is a duplicate, so return */
			if (shop_item_tmp->item_object == item_object)
			{
				player_shop_free_structure(pl, 1);
				return;
			}
		}

		/* Allocate a new item */
		shop_item_tmp = (player_shop *) malloc(sizeof(player_shop));

		/* Set the nrof, price and object pointer */
		shop_item_tmp->nrof = nrof;
		shop_item_tmp->price = price;
		shop_item_tmp->item_object = item_object;
		shop_item_tmp->next = NULL;

		/* One more item... */
		pl->shop_items_count++;

		/* If no shop items yet, this is easy */
		if (!pl->shop_items)
		{
			pl->shop_items = shop_item_tmp;
		}
		/* Otherwise we want to add it to the end of the list */
		else
		{
			player_shop *shop_item_next = pl->shop_items;
			int i;

			/* Loop until the last entry */
			for (i = 1; i < pl->shop_items_count - 1 && shop_item_next; i++, shop_item_next = shop_item_next->next)
			{
			}

			/* Add it after the last entry */
			shop_item_next->next = shop_item_tmp;
		}

		p = strtok(NULL, "|");
	}

	/* Mark this player as having an open shop interface */
	SET_FLAG(pl->ob, FLAG_PLAYER_SHOP);
	generate_ext_title(pl);
	/* Ensure we're not running or firing. */
	pl->run_on = pl->fire_on = 0;
}

/**
 * Close a player shop. If the player has shop items (is selling), free
 * them, but do not send close command to the client, as the client
 * should take care of that.
 * @param pl The player to close the shop for */
void player_shop_close(player *pl)
{
	/* So the player can move again */
	CLEAR_FLAG(pl->ob, FLAG_PLAYER_SHOP);

	/* Clear the shop items structure */
	if (pl->shop_items)
	{
		player_shop_free_structure(pl, 0);
		generate_ext_title(pl);
	}
}

/**
 * Load a player shop of other player.
 * @param data The player name of the shop owner to open
 * @param pl The player to load the shop for */
void player_shop_load(char *data, player *pl)
{
	player *seller;

	/* Sanity check */
	if (!data)
	{
		return;
	}

	/* First find the seller */
	seller = find_player(data);

	/* No valid seller? */
	if (!seller || !seller->shop_items || seller == pl)
	{
		return;
	}

	/* Too far away? */
	if (!shop_player_in_range(pl->ob, seller->ob))
	{
		new_draw_info_format(NDI_UNIQUE, pl->ob, "You are too far away from %s.", seller->ob->name);
		return;
	}

	/* Send shop items */
	player_shop_send_items(pl, seller);

	/* Mark this player as having an open shop interface */
	SET_FLAG(pl->ob, FLAG_PLAYER_SHOP);
	/* Ensure we're not running or firing. */
	pl->run_on = pl->fire_on = 0;
}

/**
 * Examine a player shop item.
 * @param data The data buffer sent from the client. Contains the seller
 * name and the object's tag ID.
 * @param pl The player examining the item */
void player_shop_examine(char *data, player *pl)
{
	char seller_name[MAX_BUF];
	tag_t count;
	player *seller;
	player_shop *shop_item_tmp;

	/* Sanity check */
	if (!data)
	{
		return;
	}

	/* The player MUST have the player shop flag in order to do this */
	if (!QUERY_FLAG(pl->ob, FLAG_PLAYER_SHOP))
	{
		player_shop_close_interface(pl);
		return;
	}

	if (!sscanf(data, "%s %d", seller_name, &count))
	{
		return;
	}

	seller = find_player(seller_name);

	/* Sanity checks */
	if (!seller || !seller->shop_items || seller == pl)
	{
		return;
	}

	/* Too far away? */
	if (!shop_player_in_range(pl->ob, seller->ob))
	{
		return;
	}

	/* Loop through the shop items */
	for (shop_item_tmp = seller->shop_items; shop_item_tmp; shop_item_tmp = shop_item_tmp->next)
	{
		if (!shop_item_tmp->item_object)
		{
			continue;
		}

		/* Found it */
		if (shop_item_tmp->item_object->count == count)
		{
			examine(pl->ob, shop_item_tmp->item_object);

			break;
		}
	}
}

/**
 * Buy a player shop item.
 * @param data The data buffer. Includes seller's name, the object's tag
 * ID we're buying, and the nrof of objects to buy.
 * @param pl The player buying the item */
void player_shop_buy(char *data, player *pl)
{
	char seller_name[MAX_BUF];
	tag_t count;
	uint32 nrof;
	player *seller;
	player_shop *shop_item_tmp;

	if (!data)
	{
		return;
	}

	/* The player MUST have the player shop flag in order to do this */
	if (!QUERY_FLAG(pl->ob, FLAG_PLAYER_SHOP))
	{
		player_shop_close_interface(pl);
		return;
	}

	if (!sscanf(data, "%s %d %d", seller_name, &count, &nrof))
	{
		return;
	}

	if (nrof < 1 || count < 1)
	{
		return;
	}

	seller = find_player(seller_name);

	/* Sanity checks */
	if (!seller || !seller->shop_items || seller == pl)
	{
		return;
	}

	/* Too far away? */
	if (!shop_player_in_range(pl->ob, seller->ob))
	{
		new_draw_info_format(NDI_UNIQUE, pl->ob, "You are too far away from %s.", seller->ob->name);
		player_shop_close_interface(pl);
		return;
	}

	/* Loop through the shop items */
	for (shop_item_tmp = seller->shop_items; shop_item_tmp; shop_item_tmp = shop_item_tmp->next)
	{
		if (!shop_item_tmp->item_object)
		{
			continue;
		}

		/* Found it */
		if (shop_item_tmp->item_object->count == count)
		{
			sint64 to_pay;
			object *tmp = shop_item_tmp->item_object;
			uint32 tmp_nrof = tmp->nrof ? tmp->nrof : 1;

			if (nrof > tmp_nrof || nrof == 0)
			{
				nrof = tmp_nrof;
			}

			to_pay = nrof * shop_item_tmp->price;

			if (!pay_for_amount(to_pay, pl->ob))
			{
				new_draw_info_format(NDI_UNIQUE, pl->ob, "You don't have enough money to buy %s.", shop_item_tmp->item_object->name);
				return;
			}

			if (nrof != tmp_nrof)
			{
				char err[MAX_BUF];

				tmp = get_split_ob(tmp, nrof, err, sizeof(err));

				if (!tmp)
				{
					new_draw_info(NDI_UNIQUE, pl->ob, err);
					return;
				}
			}
			else
			{
				remove_ob(tmp);
			}

			insert_coins(seller->ob, to_pay);
			insert_ob_in_ob(tmp, pl->ob);
			new_draw_info_format(NDI_UNIQUE | NDI_BLUE, seller->ob, "%s bought %s.", pl->ob->name, query_name(tmp, NULL));

			esrv_send_inventory(pl->ob, pl->ob);
			esrv_send_inventory(seller->ob, seller->ob);

			if (nrof == shop_item_tmp->nrof)
			{
				shop_item_tmp->item_object = NULL;
			}
			else
			{
				shop_item_tmp->nrof -= nrof;
			}

			break;
		}
	}
}
