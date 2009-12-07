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

#ifndef INVENTORY_H
#define INVENTORY_H

#define INVITEMBELOWXLEN 8
#define INVITEMBELOWYLEN 1

#define INVITEMXLEN 7
#define INVITEMYLEN 3

extern void blt_inv_item(item *tmp, int x, int y, int nrof);
extern int get_inventory_data(item *op, int *cflag, int *slot, int *start, int *count, int wxlen, int wylen);
extern void examine_range_inv();
extern void examine_range_marks(int tag);
extern int blt_inv_item_centered(item *tmp, int x, int y);
extern void widget_below_window_event(int x, int y, int MEvent);
extern void widget_inventory_event(int x, int y, SDL_Event event);
extern void widget_show_below_window(item *op, int x, int y);
extern void widget_show_inventory_window(int x, int y);

#endif
