/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
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

/**
 * @file
 * Inventory header file. */

#ifndef INVENTORY_H
#define INVENTORY_H

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
/**
 * Number of the inventory filters.
 */
#define INVENTORY_FILTER_MAX 7
/*@}*/

/** Size of the image icons in inventory. */
#define INVENTORY_ICON_SIZE 32

/** Calculate number of columns in the inventory. */
#define INVENTORY_COLS(_inventory) ((_inventory)->w / INVENTORY_ICON_SIZE)
/** Calculate number of rows in the inventory. */
#define INVENTORY_ROWS(_inventory) ((_inventory)->h / INVENTORY_ICON_SIZE)
/**
 * Decide where to look for objects, depending on the inventory's display type.
 */
#define INVENTORY_WHERE(_inventory) \
    ((_inventory)->display == INVENTORY_DISPLAY_MAIN ? cpl.ob : cpl.below)
/** Check whether the mouse is inside the inventory area. */
#define INVENTORY_MOUSE_INSIDE(_widget, _mx, _my) ((_mx) >= (_widget)->x + INVENTORY((_widget))->x && (_mx) < (_widget)->x + INVENTORY((_widget))->x + INVENTORY((_widget))->w && (_my) >= (_widget)->y + INVENTORY((_widget))->y && (_my) < (_widget)->y + INVENTORY((_widget))->y + INVENTORY((_widget))->h)

/**
 * Possible values for inventory_struct::display.
 */
typedef enum inventory_display {
    INVENTORY_DISPLAY_NONE, ///< None.
    INVENTORY_DISPLAY_MAIN, ///< Display player's inventory.
    INVENTORY_DISPLAY_BELOW, ///< Display below inventory.
} inventory_display_t;

/**
 * The inventory data. */
typedef struct inventory_struct {
    /** Index of the selected object. */
    uint32_t selected;

    /** X position of the inventory area. */
    int x;

    /** Y position of the inventory area. */
    int y;

    /** Width of the inventory area. */
    int w;

    /** Height of the inventory area. */
    int h;

    /** Which inventory to display. */
    inventory_display_t display;

    /** The inventory scrollbar. */
    scrollbar_struct scrollbar;

    /** Holds scrollbar information. */
    scrollbar_info_struct scrollbar_info;

    /** Texture to use for the background. */
    texture_struct *texture;
} inventory_struct;

#endif
