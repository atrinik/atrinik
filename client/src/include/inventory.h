/*
    Daimonin SDL client, a client program for the Daimonin MMORPG.


  Copyright (C) 2003 Michael Toennies

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    The author can be reached via e-mail to daimonin@nord-com.net
*/

#if !defined(__INVENTORY_H)
#define __INVENTORY_H


#define INVITEMBELOWXLEN 8
#define INVITEMBELOWYLEN 1

#define INVITEMXLEN 7
#define INVITEMYLEN 3

extern void show_inventory_window(int x, int y);
extern void show_below_window(item *op, int x, int y);
extern void blt_inv_item(item *tmp, int x, int y);
extern int get_inventory_data(item *op, int *cflag, int *slot, int *start, int *count, int wxlen, int wylen);
extern void examine_range_inv(void);
extern void examine_range_marks(int tag);
extern Boolean blt_inv_item_centered(item *tmp, int x, int y);

#endif
