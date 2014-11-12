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
 * Header file for the region map code. */

#ifndef REGION_MAP_H
#define REGION_MAP_H

/** Size of the book GUI borders. */
#define RM_BORDER_SIZE 25

/** Default zoom level. */
#define RM_ZOOM_DEFAULT 100
/** Minimum zoom level. */
#define RM_ZOOM_MIN 50
/** Maximum zoom level. */
#define RM_ZOOM_MAX 200
/** How much to progress the zoom level with a single mouse wheel event. */
#define RM_ZOOM_PROGRESS 10

/** Number of pixels to scroll using the keyboard arrows. */
#define RM_SCROLL 10
/** Number of pixels to scroll using the keyboard arrows when shift is held. */
#define RM_SCROLL_SHIFT 50

/** Single map. */
typedef struct region_map_struct {
    /** The map path. */
    char *path;

    /** X position. */
    int xpos;

    /** Y position. */
    int ypos;
} region_map_struct;

/** Single map label. */
typedef struct region_label_struct {
    /** X position. */
    int x;

    /** Y position. */
    int y;

    /** Unique name of the label. */
    char *name;

    /** Text of the label (markup allowed). */
    char *text;

    /**
     * The 'hidden' status of this label:
     *
     * <b>-1</b>: Shown by default.
     * <b>0</b>: Was hidden using label_hide but server told us to show it.
     * <b>1</b>: Hidden by label_hide command. */
    int hidden;
} region_label_struct;

/** Map tooltips. */
typedef struct region_map_tooltip {
    /** X position. */
    int x;

    /** Y position. */
    int y;

    /** Width. */
    int w;

    /** Height. */
    int h;

    /** Unique name of this tooltip. */
    char *name;

    /** Tooltip text. */
    char *text;

    /** Same as region_label_struct::hidden. */
    int hidden;

    /** Show an outline? */
    uint8 outline;

    /** Outline's color. */
    SDL_Color outline_color;

    /** Size of the outline. */
    uint8 outline_size;
} region_map_tooltip;

/** Map region definitions. */
typedef struct region_map_def {
    /** The maps. */
    region_map_struct *maps;

    /** Number of maps. */
    size_t num_maps;

    /** The tooltips. */
    region_map_tooltip *tooltips;

    /** Number of tooltips. */
    size_t num_tooltips;

    /** The map labels. */
    region_label_struct *labels;

    /** Number of labels. */
    size_t num_labels;

    /** Pixel size of one map tile. */
    int pixel_size;

    /** X Size of the map. */
    int map_size_x;

    /** Y Size of the map. */
    int map_size_y;
} region_map_def;

/**
 * @defgroup RM_TYPE_xxx Region map command types
 * Leftover text data types in region map command.
 *@{*/
/** Show previously hidden label. */
#define RM_TYPE_LABEL 1
/** Show previously hidden tooltip. */
#define RM_TYPE_TOOLTIP 2
/*@}*/

/**
 * @defgroup RM_MAP_xxx Region map content coords
 * Region map content coordinates.
 *@{*/
/** The map X position. */
#define RM_MAP_STARTX 25
/** The map Y position. */
#define RM_MAP_STARTY 60
/** Maximum width of the map. */
#define RM_MAP_WIDTH 630
/** Maximum height of the map. */
#define RM_MAP_HEIGHT 345
/*@}*/

/**
 * @defgroup RM_BUTTON_LEFT_xxx Region map left button coords
 * Region map left button coordinates.
 *@{*/
/** X position of the left button. */
#define RM_BUTTON_LEFT_STARTX 25
/** Y position of the left button. */
#define RM_BUTTON_LEFT_STARTY 25
/*@}*/

/**
 * @defgroup RM_BUTTON_RIGHT_xxx Region map right button coords
 * Region map right button coordinates.
 *@{*/
/** X position of the right button. */
#define RM_BUTTON_RIGHT_STARTX 649
/** Y position of the right button. */
#define RM_BUTTON_RIGHT_STARTY 25
/*@}*/

/**
 * @defgroup RM_TITLE_xxx Region map title coords
 * Region map title coordinates.
 *@{*/
/** X position of the title text. */
#define RM_TITLE_STARTX 60
/** Y position of the title text. */
#define RM_TITLE_STARTY 27
/** Maximum width of the title text. */
#define RM_TITLE_WIDTH 580
/** Maximum height of the title text. */
#define RM_TITLE_HEIGHT 22
/*@}*/

/**
 * @defgroup RM_SCROLLBAR_xxx Region map scrollbar coords
 * Region map scrollbar coordinates.
 *@{*/
/** X position of the vertical scrollbar. */
#define RM_SCROLLBAR_STARTX 662
/** Y position of the vertical scrollbar. */
#define RM_SCROLLBAR_STARTY 60
/** Width of the vertical scrollbar. */
#define RM_SCROLLBAR_WIDTH 13
/** Height of the vertical scrollbar. */
#define RM_SCROLLBAR_HEIGHT 345
/*@}*/

/**
 * @defgroup RM_SCROLLBARH_xxx Region map horizontal scrollbar coords
 * Region map horizontal scrollbar coordinates.
 *@{*/
/** X position of the horizontal scrollbar. */
#define RM_SCROLLBARH_STARTX 25
/** Y position of the horizontal scrollbar. */
#define RM_SCROLLBARH_STARTY 412
/** Width of the horizontal scrollbar. */
#define RM_SCROLLBARH_WIDTH 630
/** Height of the horizontal scrollbar. */
#define RM_SCROLLBARH_HEIGHT 13
/*@}*/

/**
 * Check whether the mouse is inside the region map. */
#define RM_IN_MAP(_popup, _mx, _my) ((_mx) >= (_popup)->x + RM_MAP_STARTX && (_mx) < (_popup)->x + RM_MAP_STARTX + RM_MAP_WIDTH && (_my) >= (_popup)->y + RM_MAP_STARTY && (_my) < (_popup)->y + RM_MAP_STARTY + RM_MAP_HEIGHT)

#endif
