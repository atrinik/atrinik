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
 * Inventory header file. */

#ifndef INVENTORY_H
#define INVENTORY_H

#define INVITEMBELOWXLEN 8
#define INVITEMBELOWYLEN 1

#define INVITEMXLEN 7
#define INVITEMYLEN 3

/**
 * @defgroup INVENTORY_FILTER_xxx Inventory filters
 * Various inventory filters.
 *@{*/
/** All objects. */
#define INVENTORY_FILTER_ALL 0
/** Applied objects. */
#define INVENTORY_FILTER_APPLIED 1
/** Containers. */
#define INVENTORY_FILTER_CONTAINER 2
/** Magical objects. */
#define INVENTORY_FILTER_MAGICAL 4
/** Cursed objects. */
#define INVENTORY_FILTER_CURSED 8
/** Unidentified objects. */
#define INVENTORY_FILTER_UNIDENTIFIED 16
/** Unapplied objects. */
#define INVENTORY_FILTER_UNAPPLIED 32
/** Locked objects. */
#define INVENTORY_FILTER_LOCKED 64
/*@}*/

#endif
