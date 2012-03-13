/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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
 * Widget header file. */

#ifndef WIDGET_H
#define WIDGET_H

/* If you want (a LOT of) debug info about widgets, uncomment this */
/*#define DEBUG_WIDGET*/

/** Information about a widget. */
typedef struct widgetdata
{
	/** Widget name. */
	char *name;

	/** X position. */
	int x;

	/** Y position. */
	int y;

	/** Width. */
	int w;

	/** Height. */
	int h;

	/** Is the widget moveable? */
	uint8 moveable;

	/** Is the widget visible? */
	uint8 show;

	/** Widget must be redrawn. */
	uint8 redraw;

	/** Should this widget be the only one of its type? */
	uint8 unique;

	/** Must there be at least one of this widget type alive? */
	uint8 required;

	/**
	 * If 0, will not save this widget. */
	uint8 save;

	/** Next widget. */
	struct widgetdata *next;

	/** Previous widget. */
	struct widgetdata *prev;

	/** The first widget inside this widget. */
	struct widgetdata *inv;

	/** The last widget inside this widget, used for traversing in reverse. */
	struct widgetdata *inv_rev;

	/** The widget that contains this widget. */
	struct widgetdata *env;

	/** Next widget of the same type. */
	struct widgetdata *type_next;

	/** Previous widget of the same type. */
	struct widgetdata *type_prev;

	/** Used for custom attributes of a widget. */
	void *subwidget;

	/** Surface used to draw the widget. */
	SDL_Surface *surface;

	/** The ID for the type of the widget. */
	int type;

	/** The ID for the subtype of widget, used as a way of creating specific widgets. */
	int sub_type;

	uint8 resizeable;

	int min_w;

	int min_h;

	int resize_flags;

	int disable_snapping;

	uint32 showed_ticks;

	void (*draw_func)(struct widgetdata *widget);

	void (*background_func)(struct widgetdata *widget);

	int (*event_func)(struct widgetdata *widget, SDL_Event *event);

	void (*deinit_func)(struct widgetdata *widget);

	int (*load_func)(struct widgetdata *widget, const char *keyword, const char *parameter);

	void (*save_func)(struct widgetdata *widget, FILE *fp, const char *padding);

	int (*menu_handle_func)(struct widgetdata *widget, SDL_Event *event);
} widgetdata;

/** Information about a widget container. Containers can hold widgets inside them. */
typedef struct _widget_container
{
	/** What type of widget this widget can hold, set to -1 to hold all types of widgets. */
	int widget_type;

	/** The space between the widgets inside and the top outer border of the widget container. */
	int outer_padding_top;

	/** The space between the widgets inside and the bottom outer border of the widget container. */
	int outer_padding_bottom;

	/** The space between the widgets inside and the left outer border of the widget container. */
	int outer_padding_left;

	/** The space between the widgets inside and the right outer border of the widget container. */
	int outer_padding_right;

	/* these are the top two values of the widgets inside stored here for fast movement and resizing.
	 * the values are in relation to each border of the container. if a widget's relative co-ordinate
	 * was equal to one of these values before movement/resizing and then dips below the second top
	 * value, the client will scan the immediate children of the container to find out what the new
	 * second top value is after the movement/resizing is complete. this means it doesn't have to keep
	 * scanning the children for the new highest size during the movement */
	int x_left_buf1;
	int x_left_buf2;
	int x_right_buf1;
	int x_right_buf2;
	int y_top_buf1;
	int y_top_buf2;
	int y_bottom_buf1;
	int y_bottom_buf2;

	/* Used for custom attributes of a container. */
	void *subcontainer;
} _widget_container;

typedef struct _widget_label
{
	/** The string used in the label. */
	char *text;

	/** The font of the text. */
	int font;

	/** The color of the text. */
	const char *color;
} _widget_label;

typedef struct _widget_texture
{
	/** The texture. */
	texture_struct *texture;
} _widget_texture;

typedef struct widget_input_struct
{
	text_input_struct text_input;

	text_input_history_struct *text_input_history;

	char title_text[MAX_BUF];

	char prepend_text[MAX_BUF];
} widget_input_struct;

/** A more specialized kind of container, where widgets snap into it when inserted, and where widgets are sorted into rows and columns. */
typedef struct _widget_container_strip
{
	/** The space between the widgets inside in relation to each other. */
	int inner_padding;

	/** Have it as either a row or a column. 1 = row, 0 = column. */
	int horizontal;

	/** Height of row or width of column. */
	int size;

	/** Used for custom attributes of a strip container. */
	void *subcontainer_strip;
} _widget_container_strip;

/** A menu. This is a special strip container that contains the menuitems inside. */
typedef struct _menu
{
	/** Pointer to a submenu from one of the menuitems that is stored when the submenu is open. */
	widgetdata *submenu;

	/** The widget that was right clicked on in order to open the menu. */
	widgetdata *owner;
} _menu;

/**
 * A menuitem that holds the pointer to the function that is called when it is clicked on.
 * It is a special strip container that contains the string inside.
 * This allows the menu to detect how long the string is so that it can resize itself to fit it on the fly. */
typedef struct _menuitem
{
	/** Pointer to the function that performs the relevant operation when the menuitem is clicked. */
	void (*menu_func_ptr)(widgetdata *, widgetdata *, SDL_Event *event);

	/** The type of menuitem. */
	int menu_type;
} _menuitem;

/** A mouse event. */
enum _MEvent
{
	MOUSE_UP = 1,
	MOUSE_DOWN,
	MOUSE_MOVE
};

/** The widget type IDs. */
typedef enum WidgetID
{
	MAP_ID,
	STATS_ID,
	RESIST_ID,
	MAIN_LVL_ID,
	SKILL_EXP_ID,
	REGEN_ID,
	SKILL_LVL_ID,
	MENU_B_ID,
	QUICKSLOT_ID,
	CHATWIN_ID,
	PDOLL_ID,
	BELOW_INV_ID,
	PLAYER_INFO_ID,
	MAIN_INV_ID,
	MAPNAME_ID,
	INPUT_ID,
	FPS_ID,
	MPLAYER_ID,
	SPELLS_ID,
	SKILLS_ID,
	PARTY_ID,
	NOTIFICATION_ID,
	CONTAINER_ID,
	LABEL_ID,
	TEXTURE_ID,

	/** The total number of widgets. */
	TOTAL_WIDGETS
} WidgetID;

/** The widget subtype IDs. These are derived from base widgets. */
enum
{
	/** First element must be equal to TOTAL_WIDGETS. */
	CONTAINER_STRIP_ID = TOTAL_WIDGETS,
	MENU_ID,
	MENUITEM_ID,

	/** The total number of subwidgets. */
	TOTAL_SUBWIDGETS
};

/** Widget resize flags. */
enum
{
	RESIZE_LEFT = 1,
	RESIZE_TOP = 2,
	RESIZE_RIGHT = 4,
	RESIZE_BOTTOM = 8,
	RESIZE_TOPLEFT = RESIZE_TOP | RESIZE_LEFT,
	RESIZE_TOPRIGHT = RESIZE_TOP | RESIZE_RIGHT,
	RESIZE_BOTTOMRIGHT = RESIZE_BOTTOM | RESIZE_RIGHT,
	RESIZE_BOTTOMLEFT = RESIZE_BOTTOM | RESIZE_LEFT
};

/** Menu types. */
enum
{
	MENU_NORMAL,
	MENU_SUBMENU,
	MENU_CHECKBOX,
	MENU_RADIO
};

/** Used for mouse button/move events */
typedef struct widgetevent
{
	/** The widget involved in the mouse event. */
	widgetdata *owner;

	/** Widget X */
	int x;

	/** Widget Y */
	int y;
} widgetevent;

/** This is used when moving a widget with the mouse. */
typedef struct widgetmove
{
	/** Is the widget active? */
	int active;

	/** The widget involved in the move event. */
	widgetdata *owner;

	/** X offset. */
	int xOffset;

	/** Y offset. */
	int yOffset;
} widgetmove;

/** This is used when resizing a widget with the mouse. */
typedef struct widgetresize
{
	/** Is the widget active? */
	int active;

	/** The widget involved in the resize event. */
	widgetdata *owner;
} widgetresize;

/** Macro to redraw widget using the array. */
#define WIDGET_REDRAW(__tmp) __tmp->redraw = 1;

#define WIDGET_SHOW(_widget) \
{ \
	(_widget)->show = 1; \
	(_widget)->showed_ticks = SDL_GetTicks(); \
}

/* Macro to redraw all widgets of a particular type. Don't use this often. */
#define WIDGET_REDRAW_ALL(__id) widget_redraw_all(__id);

/** Macros to grab extended widget attributes. This works similar to inheritance. */
#define TEXTWIN(__textwin) ((textwin_struct *) ((__textwin)->subwidget))
#define WIDGET_INPUT(_widget) ((widget_input_struct *) (_widget)->subwidget)
#define CONTAINER(__widget_container) (_widget_container *) (__widget_container->subwidget)
#define LABEL(__widget_label) (_widget_label *) (__widget_label->subwidget)
#define WIDGET_TEXTURE(__widget_texture) (_widget_texture *) (__widget_texture->subwidget)
#define CONTAINER_STRIP(__widget_container_strip) \
	(_widget_container_strip *) ( ((_widget_container *) (__widget_container_strip->subwidget)) ->subcontainer)
#define MENU(__menu) \
	((_menu *) ( (( ((_widget_container_strip *) ((_widget_container *) (__menu->subwidget)) ->subcontainer)) ->subcontainer_strip)))
#define MENUITEM(__menuitem) \
	(_menuitem *) ( (( ((_widget_container_strip *) ((_widget_container *) (__menuitem->subwidget)) ->subcontainer)) ->subcontainer_strip))
#define INVENTORY(_widget) ((inventory_struct *) ((_widget)->subwidget))

#endif
