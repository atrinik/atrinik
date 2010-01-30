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

#ifndef WIDGET_H
#define WIDGET_H

/* If you want (a LOT) of debug info about widgets, uncomment this */
/*#define DEBUG_WIDGET*/

/** Used in the priority list (to order widgets) */
struct _widget_node
{
	/** Next */
	struct _widget_node *next;

	/** Previous */
	struct _widget_node *prev;

	/** Widget ID */
	int WidgetID;
};

typedef struct _widget_node widget_node;

/** Information about a widget - used for current/default list */
typedef struct _widgetdata
{
	/** Widget name */
	char *name;

	/** Widget priority */
	widget_node *priority_index;

	/** Position X */
	int x1;

	/** Position Y */
	int y1;

	/** Width */
	int wd;

	/** Height */
	int ht;

	/** Moveable */
	int moveable;

	/** Hidden and inactive or shown */
	int show;

	/** Widget must be redrawn */
	int redraw;
}_widgetdata;

/** Events that are passed to the widget handler */
typedef enum _proc_type
{
	PROCESS,
	EVENT,
	KILL
} _proc_type;

enum _MEvent
{
	MOUSE_UP = 1,
	MOUSE_DOWN,
	MOUSE_MOVE
};

/* add the widget id here */
typedef enum _WidgetID
{
	STATS_ID,
	RESIST_ID,
	MAIN_LVL_ID,
	SKILL_EXP_ID,
	REGEN_ID,
	SKILL_LVL_ID,
	MENU_B_ID,
	QUICKSLOT_ID,
	CHATWIN_ID,
	MSGWIN_ID,
	MIXWIN_ID,
	PDOLL_ID,
	BELOW_INV_ID,
	PLAYER_INFO_ID,
	RANGE_ID,
	TARGET_ID,
	MAIN_INV_ID,
	MAPNAME_ID,
	IN_CONSOLE_ID,
	IN_NUMBER_ID,
	SHOP_ID,
	FPS_ID,

	/* Must be last element */
	TOTAL_WIDGETS
}_WidgetID;

/** Used for mouse button/move events */
typedef struct _widgetevent
{
	/** Widget owner */
	int owner;

	/** Widget X */
	int x;

	/** Widget Y */
	int y;
}_widgetevent;

/** This is used when moving a widget with the mouse */
typedef struct _widgetmove
{
	/** Is the widget active? */
	int active;

	/** Widget ID */
	int id;

	/** X offset */
	int xOffset;

	/** Y offset */
	int yOffset;
}_widgetmove;

SDL_Surface *widgetSF[TOTAL_WIDGETS];

extern _widgetdata cur_widget[TOTAL_WIDGETS];
extern _widgetevent widget_mouse_event;
extern int IsMouseExclusive;
extern int old_alpha_option;

/** Macro to redraw widget */
#define WIDGET_REDRAW(__a) cur_widget[__a].redraw = 1;

#ifdef WIDGET_SNAP
/** Left position */
#define LEFT(ID) (cur_widget[(ID)].x1)

/** Right position*/
#define RIGHT(ID) (cur_widget[(ID)].x1 + cur_widget[(ID)].wd)

/** Top position */
#define TOP(ID) (cur_widget[(ID)].y1)

/** Bottom position */
#define BOTTOM(ID) (cur_widget[(ID)].y1 + cur_widget[(ID)].ht)
#endif

#endif
