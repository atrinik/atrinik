/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
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

#ifndef SHOP_H
#define SHOP_H

#include <decls.h>

/**
 * @file
 * Public declarations for the corresponding server module.
 */

/** Public API implemented in src/server/shop.c. */

extern int64_t shop_get_cost(object *op, int mode);

extern const char *shop_get_cost_string(int64_t cost);

extern const char *shop_get_cost_string_item(object *op, int flag);

extern int64_t shop_get_money(object *op);

extern bool shop_pay(object *op, int64_t to_pay);

extern bool shop_pay_item(object *op, object *item);

extern bool shop_pay_items(object *op);

extern void shop_sell_item(object *op, object *item);

extern int64_t bank_get_balance(object *op);

extern object *bank_find_info(object *op);

extern int bank_deposit(object *op, const char *text, int64_t *value);

extern int bank_withdraw(object *op, const char *text, int64_t *value);

extern void shop_insert_coins(object *op, int64_t value);

#endif
