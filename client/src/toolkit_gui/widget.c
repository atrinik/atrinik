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
 * This file controls all the widget related functions, movement of the
 * widgets, initialization, etc.
 *
 * To add a new widget:
 * -# Add an entry (same index in both cases) to ::con_widget and ::WidgetID.
 * -# If applicable, add extended attributes in its own struct, and add handler code for its initialization in create_widget_object().
 * -# If applicable, add handler code for widget movement in widget_event_mousedn().
 * -# If applicable, add handler code to get_widget_owner().
 * -# Add handler function to process_widget().
 *
 * @author Dantee
 * @author Alex Tokar
 * @author Daniel Liptrot */

#include <global.h>

static int load_interface_file(char *filename);
static void process_widget(widgetdata *widget);

/** Current (default) data list of all widgets. */
static widgetdata def_widget[TOTAL_SUBWIDGETS];

/** Default data list of all widgets. */
/* {name, x1, y1, wd, ht, moveable?, show?, redraw?, unique?, no_kill?, visible?, delete_inv?, save?, save_width_height?
 * * the next members are used internally *
 * next(NULL), prev(NULL), inv(NULL), inv_rev(NULL), env(NULL), type_next(NULL), type_prev(NULL),
 * subwidget(NULL), widgetSF(NULL), WidgetTypeID(0), WidgetSubtypeID(0), WidgetObjID(0)} */
static const widgetdata con_widget[TOTAL_SUBWIDGETS] =
{
	/* base widgets */
	{"MAP", 0, 10, 850, 600, 1, 1, 1, 1, 1, 1, 1, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0},
	{"STATS",           227,   0, 172, 102, 1, 1, 1, 1, 1, 1, 1, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"RESIST",          497,   0, 198,  79, 1, 1, 1, 1, 1, 1, 1, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"MAIN_LVL",        399,  39,  98,  62, 1, 1, 1, 1, 1, 1, 1, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"SKILL_EXP",       497,  79, 198,  22, 1, 1, 1, 1, 1, 1, 1, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"REGEN",           399,   0,  98,  39, 1, 1, 1, 1, 1, 1, 1, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"SKILL_LVL",       695,   0,  52, 101, 1, 1, 1, 1, 1, 1, 1, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"MENUBUTTONS",     747,   0,  47, 101, 1, 1, 1, 1, 1, 1, 1, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"QUICKSLOTS",      735, 489, 282,  34, 1, 1, 1, 1, 1, 1, 1, 1, 1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"CHATWIN",         631, 540, 392, 226, 1, 1, 1, 0, 1, 1, 1, 1, 1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 1, 80, 80, 0, 0, 0},
	{"MSGWIN",            1, 540, 308, 226, 1, 1, 1, 0, 1, 1, 1, 1, 1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 1, 80, 80, 0, 0, 0},
	{"PLAYERDOLL",        0,  41, 219, 243, 1, 1, 1, 1, 1, 1, 1, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"BELOWINV",        331, 713, 274,  55, 1, 1, 1, 1, 1, 1, 1, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"PLAYERINFO",        0,   0, 219,  41, 1, 1, 1, 1, 1, 1, 1, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"RANGEBOX",          6,  51,  94,  60, 1, 1, 1, 1, 1, 1, 1, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"MAININV",           1, 508, 271,  32, 1, 1, 1, 1, 1, 1, 1, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"MAPNAME",         228, 106,  36,  16, 1, 1, 1, 1, 1, 1, 1, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"CONSOLE",         339, 655, 256,  25, 1, 0, 1, 1, 1, 1, 1, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"NUMBER",          340, 637, 256,  43, 1, 0, 1, 1, 1, 1, 1, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"FPS",             123,  47,  70,  22, 1, 1, 1, 1, 1, 1, 1, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"MPLAYER", 474, 101, 320, 190, 1, 0, 1, 1, 1, 1, 1, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"SPELLS", 474, 101, 320, 190, 1, 0, 1, 1, 1, 1, 1, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"SKILLS", 474, 101, 320, 190, 1, 0, 1, 1, 1, 1, 1, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"PARTY", 474, 101, 320, 190, 1, 0, 1, 1, 1, 1, 1, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"NOTIFICATION_ID", 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"CONTAINER",         0,   0, 128, 128, 1, 0, 1, 0, 1, 1, 0, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"LABEL",             0,   0,   5,   5, 1, 1, 1, 0, 0, 1, 1, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"TEXTURE",           0,   0,   5,   5, 1, 1, 1, 0, 0, 1, 1, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	/* subwidgets */
	{"CONTAINER_STRIP",   0,   0, 128, 128, 1, 0, 1, 0, 1, 1, 0, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"MENU",              0,   0,   5,   5, 0, 1, 1, 0, 0, 1, 1, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{"MENUITEM",          0,   0,   5,   5, 0, 1, 1, 0, 0, 0, 1, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};


/* Default overall priority tree. Will change during runtime.
 * Widget at the top (head) of the tree has highest priority.
 * Events go to the top (head) of the tree first.
 * Displaying goes to the right (foot) of the tree first. */
/** The root node at the top of the tree. */
static widgetdata *widget_list_head;
/** The last sibling on the top level of the tree (the far right). */
static widgetdata *widget_list_foot;

/**
 * The head and foot node for each widget type.
 * The nodes in this linked list do not change until a node is deleted. */
/* TODO: change cur_widget to type_list_head */
widgetdata *cur_widget[TOTAL_SUBWIDGETS];
static widgetdata *type_list_foot[TOTAL_SUBWIDGETS];

/**
 * Determines which widget has mouse focus
 * This value is determined in the mouse routines for the widgets */
widgetevent widget_mouse_event =
{
	NULL, 0, 0
};

/** This is used when moving a widget with the mouse. */
static widgetmove widget_event_move =
{
	0, NULL, 0, 0
};

/** This is used when resizing a widget with the mouse. */
static widgetresize widget_event_resize =
{
	0, NULL
};

/**
 * A way to steal the mouse, and to prevent widgets from using mouse events
 * Example: Prevents widgets from using mouse events during dragging procedure */
static int IsMouseExclusive = 0;

/**
 * Load the defaults and initialize the priority list.
 * Create the interface file, if it doesn't exist */
static void init_widgets_fromDefault(void)
{
	int lp;

	/* Exit, if there are no widget IDs */
	if (!TOTAL_SUBWIDGETS)
	{
		return;
	}

	/* Store the constant default widget lookup in the current lookup(s) */
	for (lp = 0; lp < TOTAL_SUBWIDGETS; ++lp)
	{
		def_widget[lp] = con_widget[lp];
	}

	/* Initiate the linked lists for the widgets. */
	init_widgets();
}

/**
 * Try to load the main interface file and initialize the priority list
 * On failure, initialize the widgets with init_widgets_fromDefault() */
void init_widgets_fromCurrent(void)
{
	/* Exit, if there are no widgets */
	if (!TOTAL_SUBWIDGETS)
	{
		return;
	}

	/* If can't open/load the interface file load defaults and create file */
	if (!load_interface_file(INTERFACE_FILE))
	{
		/* Load the defaults - this also allocates priority list */
		init_widgets_fromDefault();

		/* Create the interface file */
		save_interface_file();
	}

	if (!setting_get_int(OPT_CAT_CLIENT, OPT_OFFSCREEN_WIDGETS))
	{
		widgets_ensure_onscreen();
	}
}

/** Wrapper function to handle the creation of a widget. */
widgetdata *create_widget_object(int widget_subtype_id)
{
	widgetdata *widget;
	textwin_struct *textwin;
	_widget_container *container;
	_widget_container_strip *container_strip;
	_menu *menu;
	_menuitem *menuitem;
	_widget_label *label;
	_widget_texture *texture;
	int widget_type_id = widget_subtype_id;

	/* map the widget subtype to widget type */
	if (widget_subtype_id >= TOTAL_WIDGETS)
	{
		switch (widget_subtype_id)
		{
			case CONTAINER_STRIP_ID:
			case MENU_ID:
			case MENUITEM_ID:
				widget_type_id = CONTAINER_ID;
				break;

			/* no subtype was found, so get out of here */
			default:
				return NULL;
		}
	}

	/* sanity check */
	if (widget_subtype_id < 0 || widget_subtype_id >= TOTAL_SUBWIDGETS)
	{
		return NULL;
	}

	/* don't create more than one widget if it is a unique widget */
	if (con_widget[widget_subtype_id].unique && cur_widget[widget_subtype_id])
	{
		return NULL;
	}

	/* allocate the widget node, this should always be the first function called in here */
	widget = create_widget(widget_subtype_id);
	widget->WidgetTypeID = widget_type_id;

	/* allocate the custom attributes for the widget if applicable */
	switch (widget->WidgetTypeID)
	{
		case CHATWIN_ID:
		case MSGWIN_ID:
			textwin = calloc(1, sizeof(textwin_struct));

			if (!textwin)
			{
				exit(0);
			}

			textwin->font = FONT_ARIAL11;
			textwin->selection_start = -1;
			textwin->selection_end = -1;
			widget->subwidget = (textwin_struct *) textwin;
			textwin_create_scrollbar(widget);
			break;

		case MAPNAME_ID:
			/* set the bounding box to another one that exists, otherwise it can be wrong initially */
			if (cur_widget[MAPNAME_ID])
			{
				widget->wd = cur_widget[MAPNAME_ID]->wd;
				widget->ht = cur_widget[MAPNAME_ID]->ht;
			}

			break;

		case CONTAINER_ID:
			container = malloc(sizeof(_widget_container));

			if (!container)
			{
				exit(0);
			}

			/* begin initializing the members */
			container->widget_type = -1;
			container->outer_padding_top = 10;
			container->outer_padding_bottom = 10;
			container->outer_padding_left = 10;
			container->outer_padding_right = 10;
			container->x_left_buf1 = 0;
			container->x_left_buf2 = 0;
			container->x_right_buf1 = 0;
			container->x_right_buf2 = 0;
			container->y_top_buf1 = 0;
			container->y_top_buf2 = 0;
			container->y_bottom_buf1 = 0;
			container->y_bottom_buf2 = 0;
			/* have the subwidget point to it */
			widget->subwidget = (_widget_container *) container;

			/* allocate the custom attributes for the container if applicable */
			switch (widget->WidgetSubtypeID)
			{
				case CONTAINER_STRIP_ID:
				case MENU_ID:
				case MENUITEM_ID:
					container_strip = malloc(sizeof(_widget_container_strip));

					if (!container_strip)
					{
						exit(0);
					}

					/* Begin initializing the members. */
					container_strip->inner_padding = 10;
					container_strip->horizontal = 0;
					container_strip->size = 0;
					/* Have the subcontainer point to it. */
					container->subcontainer = (_widget_container_strip *) container_strip;

					/* Allocate the custom attributes for the strip container if applicable. */
					switch (widget->WidgetSubtypeID)
					{
						case MENU_ID:
							menu = malloc(sizeof(_menu));

							if (!menu)
							{
								exit(0);
							}

							/* Begin initializing the members. */
							menu->submenu = NULL;
							menu->owner = NULL;
							/* Have the sub strip container point to it. */
							container_strip->subcontainer_strip = (_menu *) menu;
							break;

						case MENUITEM_ID:
							menuitem = malloc(sizeof(_menuitem));

							if (!menuitem)
							{
								exit(0);
							}

							/* Begin initializing the members. */
							menuitem->menu_func_ptr = NULL;
							menuitem->menu_type = MENU_NORMAL;
							/* Have the sub strip container point to it. */
							container_strip->subcontainer_strip = (_menuitem *) menuitem;
							break;
					}

					break;
			}

			break;

		case LABEL_ID:
			label = malloc(sizeof(_widget_label));

			if (!label)
			{
				exit(0);
			}

			/* begin initializing the members */
			label->text = "";
			label->font = FONT_ARIAL10;
			label->color = COLOR_WHITE;
			/* have the subwidget point to it */
			widget->subwidget = (_widget_label *) label;
			break;

		case TEXTURE_ID:
			texture = malloc(sizeof(*texture));

			if (!texture)
			{
				exit(0);
			}

			/* begin initializing the members */
			texture->texture = NULL;
			/* have the subwidget point to it */
			widget->subwidget = (_widget_texture *) texture;
			break;

		case MAIN_INV_ID:
		case BELOW_INV_ID:
			widget->subwidget = calloc(1, sizeof(inventory_struct));
			widget_inventory_init(widget);
			break;

		case IN_CONSOLE_ID:
		case IN_NUMBER_ID:
			widget->subwidget = calloc(1, sizeof(widget_input_struct));
			text_input_create(&WIDGET_INPUT(widget)->text_input);
			WIDGET_INPUT(widget)->text_input.w = widget->wd - 16;

			if (widget->WidgetTypeID == IN_NUMBER_ID)
			{
				WIDGET_INPUT(widget)->text_input.max = 20;
			}
			else
			{
				WIDGET_INPUT(widget)->text_input.max = 250;
				WIDGET_INPUT(widget)->text_input_history = text_input_history_create();
				text_input_set_history(&WIDGET_INPUT(widget)->text_input, WIDGET_INPUT(widget)->text_input_history);
			}

			break;
	}

	return widget;
}

/** Wrapper function to handle the obliteration of a widget. */
void remove_widget_object(widgetdata *widget)
{
	/* don't delete the last widget if there needs to be at least one of this widget type */
	if (widget->no_kill && cur_widget[widget->WidgetSubtypeID] == type_list_foot[widget->WidgetSubtypeID])
	{
		return;
	}

	remove_widget_object_intern(widget);
}

/**
 * Wrapper function to handle the annihilation of a widget, including possibly killing the linked list altogether.
 * Please do not use, this should only be explicitly called by kill_widget_tree() and remove_widget_object().
 * Use remove_widget_object() for everything else. */
void remove_widget_object_intern(widgetdata *widget)
{
	widgetdata *tmp;
	_widget_container *container;
	_widget_container_strip *container_strip;
	int widget_subtype_id = widget->WidgetSubtypeID;

	/* If this flag is enabled, we need to delete all contents of the widget too, which calls for some recursion. */
	if (widget->delete_inv)
	{
		remove_widget_inv(widget);
	}

	/* If this widget happens to be the owner of an event, keeping them pointed to it is a bad idea. */
	if (widget_mouse_event.owner == widget)
	{
		widget_mouse_event.owner = NULL;
	}

	if (widget_event_move.owner == widget)
	{
		widget_event_move.owner = NULL;
	}

	if (widget_event_resize.owner == widget)
	{
		widget_event_resize.owner = NULL;
	}

	/* If any menu is open and this widget is the owner, bad things could happen here too. Clear the pointers. */
	if (cur_widget[MENU_ID] && (MENU(cur_widget[MENU_ID]))->owner == widget)
	{
		for (tmp = cur_widget[MENU_ID]; tmp; tmp = tmp->type_next)
		{
			(MENU(cur_widget[MENU_ID]))->owner = NULL;
		}
	}

	/* Get the environment if it exists, this is used to make containers auto-resize when the widget is deleted. */
	tmp = widget->env;

	/* remove the custom attribute nodes if they exist */
	if (widget->subwidget)
	{
		switch (widget_subtype_id)
		{
			case CONTAINER_STRIP_ID:
			case MENU_ID:
			case MENUITEM_ID:
				if (widget_subtype_id == MENUITEM_ID)
				{
					container_strip = CONTAINER_STRIP(widget);

					if (container_strip->subcontainer_strip)
					{
						free(container_strip->subcontainer_strip);
						container_strip->subcontainer_strip = NULL;
					}
				}

				container = CONTAINER(widget);

				if (container->subcontainer)
				{
					free(container->subcontainer);
					container->subcontainer = NULL;
				}

				break;

			case CHATWIN_ID:
			case MSGWIN_ID:
			{
				textwin_struct *textwin = TEXTWIN(widget);

				if (textwin->entries)
				{
					free(textwin->entries);
				}

				break;
			}

			case IN_NUMBER_ID:
			case IN_CONSOLE_ID:
				if (WIDGET_INPUT(widget)->text_input_history)
				{
					text_input_history_free(WIDGET_INPUT(widget)->text_input_history);
				}
		}

		free(widget->subwidget);
		widget->subwidget = NULL;
	}

	switch (widget_subtype_id)
	{
		case MPLAYER_ID:
			widget_mplayer_deinit(widget);
			break;
	}

	/* finally de-allocate the widget node, this should always be the last node removed in here */
	remove_widget(widget);

	/* resize the container that used to contain this widget, if it exists */
	if (tmp)
	{
		/* if something else exists in its inventory, make it auto-resize to fit the widgets inside */
		if (tmp->inv)
		{
			resize_widget(tmp->inv, RESIZE_RIGHT, tmp->inv->wd);
			resize_widget(tmp->inv, RESIZE_BOTTOM, tmp->inv->ht);
		}
		/* otherwise if its inventory is empty, resize it to its default size */
		else
		{
			resize_widget(tmp, RESIZE_RIGHT, con_widget[tmp->WidgetSubtypeID].wd);
			resize_widget(tmp, RESIZE_BOTTOM, con_widget[tmp->WidgetSubtypeID].ht);
		}
	}
}

/**
 * Deletes the entire inventory of a widget, child nodes first. This should be the fastest way.
 * Any widgets that can't be deleted should end up on the top level.
 * This function is already automatically handled with the delete_inv flag,
 * so it shouldn't be called explicitly apart from in remove_widget_object_intern(). */
void remove_widget_inv(widgetdata *widget)
{
	widgetdata *tmp;

	for (widget = widget->inv; widget; widget = tmp)
	{
		/* call this function recursively to get to the first child node deep down inside the widget */
		remove_widget_inv(widget);
		/* we need a temp pointer for the next node, as the current node is about to be no more */
		tmp = widget->next;
		/* then remove the widget, and slowly work our way up the tree deleting widgets until we get to the original widget again */
		remove_widget_object(widget);
	}
}

/** Wrapper function to initiate one of each widget. */
/* TODO: this is looking more and more like a function to simply initiate all the widgets with their default attributes,
 * as loading from a file now creates nodes dynamically instead, so this function is now doomed to that role */
void init_widgets(void)
{
	int i;

	/* exit, if there're no widgets */
	if (!TOTAL_SUBWIDGETS)
	{
	    return;
	}

	/* in all cases should reset */
	kill_widgets();

	/* initiate the widget tree and everything else that links to it. */
	for (i = 0; i < TOTAL_SUBWIDGETS; ++i)
	{
		if (!con_widget[i].no_kill)
		{
			continue;
		}

	    create_widget_object(i);
	}
}

/**
 * Deinitialize all widgets, and free their SDL surfaces. */
void kill_widgets(void)
{
	/* get rid of the pointer to the widgets first */
	widget_mouse_event.owner = NULL;
	widget_event_move.owner = NULL;
	widget_event_resize.owner = NULL;

	/* kick off the chain reaction, there's no turning back now :) */
	if (widget_list_head)
	{
		kill_widget_tree(widget_list_head);
	}
}

/**
 * Resets widget's coordinates from default.
 * @param name Widget name to reset. If NULL, will reset all. */
void reset_widget(const char *name)
{
	widgetdata *tmp;

	for (tmp = widget_list_head; tmp; tmp = tmp->next)
	{
		if (!tmp->moveable)
		{
			continue;
		}

		if (!name || !strcasecmp(tmp->name, name))
		{
			tmp->x1 = con_widget[tmp->WidgetTypeID].x1;
			tmp->y1 = con_widget[tmp->WidgetTypeID].y1;
			tmp->wd = con_widget[tmp->WidgetTypeID].wd;
			tmp->ht = con_widget[tmp->WidgetTypeID].ht;
			tmp->show = con_widget[tmp->WidgetTypeID].show;
			WIDGET_REDRAW(tmp);
		}
	}
}

/**
 * Ensures a single widget is on-screen.
 * @param widget The widget. */
static void widget_ensure_onscreen(widgetdata *widget)
{
	int dx = 0, dy = 0;

	if (widget->x1 < 0)
	{
		dx = -widget->x1;
	}
	else if (widget->x1 + widget->wd > setting_get_int(OPT_CAT_CLIENT, OPT_RESOLUTION_X))
	{
		dx = setting_get_int(OPT_CAT_CLIENT, OPT_RESOLUTION_X) - widget->wd - widget->x1;
	}

	if (widget->y1 < 0)
	{
		dy = -widget->y1;
	}
	else if (widget->y1 + widget->ht > setting_get_int(OPT_CAT_CLIENT, OPT_RESOLUTION_Y))
	{
		dy = setting_get_int(OPT_CAT_CLIENT, OPT_RESOLUTION_Y) - widget->ht - widget->y1;
	}

	move_widget_rec(widget, dx, dy);
}

/**
 * Ensures all widgets are on-screen. */
void widgets_ensure_onscreen(void)
{
	widgetdata *tmp;

	for (tmp = widget_list_head; tmp; tmp = tmp->next)
	{
		widget_ensure_onscreen(tmp);
	}
}

/** Recursive function to nuke the entire widget tree. */
void kill_widget_tree(widgetdata *widget)
{
	widgetdata *tmp;

	do
	{
		/* we want to process the widgets starting from the left hand side of the tree first */
		if (widget->inv)
		{
			kill_widget_tree(widget->inv);
		}

		/* store the next sibling in a tmp variable, as widget is about to be zapped from existence */
		tmp = widget->next;

		/* here we call our widget kill function, and force removal by using the internal one */
		remove_widget_object_intern(widget);

		/* get the next sibling for our next loop */
		widget = tmp;
	}
	while (widget);
}

/**
 * Creates a new widget object with a unique ID and inserts it at the root of the widget tree.
 * This should always be the first function called by create_widget_object() in order to get the pointer
 * to the new node so we can link it to other new nodes that depend on it. */
widgetdata *create_widget(int widget_id)
{
	widgetdata *node;
	/* our unique widget count variable */
	static int widget_uid = 0;

#ifdef DEBUG_WIDGET
	logger_print(LOG(INFO), "Entering create_widget()..");
#endif

	/* allocate it */
	node = malloc(sizeof(widgetdata));

	if (!node)
	{
		exit(0);
	}

	/* set the members */
	/* this also sets all pointers in the struct to NULL */
	*node = con_widget[widget_id];
	node->WidgetSubtypeID = widget_id;
	/* give it a unique ID */
	node->WidgetObjID = widget_uid;

	/* link it up to the tree if the root exists */
	if (widget_list_head)
	{
		node->next = widget_list_head;
		widget_list_head->prev = node;
	}

	/* set the foot if it doesn't exist */
	if (!widget_list_foot)
	{
		widget_list_foot = node;
	}

	/* the new node becomes the new root node, which also automatically brings it to the front */
	widget_list_head = node;

	/* if head of widget type linked list doesn't exist, set the head and foot */
	if (!cur_widget[widget_id])
	{
		cur_widget[widget_id] = type_list_foot[widget_id] = node;
	}
	/* otherwise, link the node in to the existing type list */
	else
	{
		type_list_foot[widget_id]->type_next = node;
		node->type_prev = type_list_foot[widget_id];
		type_list_foot[widget_id] = node;
	}

	/* increment the unique ID counter */
	++widget_uid;

#ifdef DEBUG_WIDGET
	logger_print(LOG(DEBUG), "..ALLOCATED: %s, WidgetObjID: %d", node->name, node->WidgetObjID);
	debug_count_nodes(1);

	logger_print(LOG(INFO), "..create_widget(): Done.");
#endif

	return node;
}

/** Removes the pointer passed to it from anywhere in the linked list and reconnects the adjacent nodes to each other. */
void remove_widget(widgetdata *widget)
{
	widgetdata *tmp = NULL;

#ifdef DEBUG_WIDGET
	logger_print(LOG(INFO), "Entering remove_widget()..");
#endif

	/* node to delete is the only node in the tree, bye-bye binary tree :) */
	if (!widget_list_head->next && !widget_list_head->inv)
	{
		widget_list_head = NULL;
		widget_list_foot = NULL;
		cur_widget[widget->WidgetSubtypeID] = NULL;
		type_list_foot[widget->WidgetSubtypeID] = NULL;
	}
	else
	{
		/* node to delete is the head, move the pointer to next node */
		if (widget == widget_list_head)
		{
			widget_list_head = widget_list_head->next;
			widget_list_head->prev = NULL;
		}
		/* node to delete is the foot, move the pointer to the previous node */
		else if (widget == widget_list_foot)
		{
			widget_list_foot = widget_list_foot->prev;
			widget_list_foot->next = NULL;
		}
		/* node is first sibling, and should have a parent since it is not the root node */
		else if (!widget->prev)
		{
			/* node is also the last sibling, so NULL the parent's inventory */
			if (!widget->next)
			{
				widget->env->inv = NULL;
				widget->env->inv_rev = NULL;
			}
			/* or else make it the parent's first child */
			else
			{
				widget->env->inv = widget->next;
				widget->next->prev = NULL;
			}
		}
		/* node is last sibling and should have a parent, move the inv_rev pointer to the previous sibling */
		else if (!widget->next)
		{
			widget->env->inv_rev = widget->prev;
			widget->prev->next = NULL;
		}
		/* node to delete is in the middle of the tree somewhere */
		else
		{
			widget->next->prev = widget->prev;
			widget->prev->next = widget->next;
		}

		/* move the children to the top level of the list, starting from the end child */
		for (tmp = widget->inv_rev; tmp; tmp = tmp->prev)
		{
			/* tmp is no longer in a container */
			tmp->env = NULL;
			widget_list_head->prev = tmp;
			tmp->next = widget_list_head;
			widget_list_head = tmp;
		}

		/* if widget type list has only one node, kill it */
		if (cur_widget[widget->WidgetSubtypeID] == type_list_foot[widget->WidgetSubtypeID])
		{
			cur_widget[widget->WidgetSubtypeID] = type_list_foot[widget->WidgetSubtypeID] = NULL;
		}
		/* widget is head node */
		else if (widget == cur_widget[widget->WidgetSubtypeID])
		{
			cur_widget[widget->WidgetSubtypeID] = cur_widget[widget->WidgetSubtypeID]->type_next;
			cur_widget[widget->WidgetSubtypeID]->type_prev = NULL;
		}
		/* widget is foot node */
		else if (widget == type_list_foot[widget->WidgetSubtypeID])
		{
			type_list_foot[widget->WidgetSubtypeID] = type_list_foot[widget->WidgetSubtypeID]->type_prev;
			type_list_foot[widget->WidgetSubtypeID]->type_next = NULL;
		}
		/* widget is in middle of type list */
		else
		{
			widget->type_prev->type_next = widget->type_next;
			widget->type_next->type_prev = widget->type_prev;
		}
	}

#ifdef DEBUG_WIDGET
	logger_print(LOG(DEBUG), "..REMOVED: %s, WidgetObjID: %d", widget->name, widget->WidgetObjID);
#endif

	/* free the surface */
	if (widget->widgetSF)
	{
		SDL_FreeSurface(widget->widgetSF);
		widget->widgetSF = NULL;
	}

	free(widget);

#ifdef DEBUG_WIDGET
	debug_count_nodes(1);
	logger_print(LOG(INFO), "..remove_widget(): Done.");
#endif
}

/** Removes the widget from the container it is inside and moves it to the top of the priority tree. */
void detach_widget(widgetdata *widget)
{
	/* sanity check */
	if (!widget->env)
	{
		return;
	}

	/* first unlink the widget from the container and siblings */

	/* if widget is only one in the container's inventory, clear both pointers */
	if (!widget->prev && !widget->next)
	{
		widget->env->inv = NULL;
		widget->env->inv_rev = NULL;
	}
	/* widget is first sibling */
	else if (!widget->prev)
	{
		widget->env->inv = widget->next;
		widget->next->prev = NULL;
	}
	/* widget is last sibling */
	else if (!widget->next)
	{
		widget->env->inv_rev = widget->prev;
		widget->prev->next = NULL;
	}
	/* widget is a middle sibling */
	else
	{
		widget->prev->next = widget->next;
		widget->next->prev = widget->prev;
	}

	/* if something else exists in the container's inventory, make it auto-resize to fit the widgets inside */
	if (widget->env->inv)
	{
		resize_widget(widget->env->inv, RESIZE_RIGHT, widget->env->inv->wd);
		resize_widget(widget->env->inv, RESIZE_BOTTOM, widget->env->inv->ht);
	}
	/* otherwise if its inventory is empty, resize it to its default size */
	else
	{
		resize_widget(widget->env, RESIZE_RIGHT, con_widget[widget->env->WidgetSubtypeID].wd);
		resize_widget(widget->env, RESIZE_BOTTOM, con_widget[widget->env->WidgetSubtypeID].ht);
	}

	/* widget is no longer in a container */
	widget->env = NULL;
	/* move the widget to the top of the priority tree */
	widget->prev = NULL;
	widget_list_head->prev = widget;
	widget->next = widget_list_head;
	widget_list_head = widget;
}

#ifdef DEBUG_WIDGET
/** A debug function to count the number of widget nodes that exist. */
int debug_count_nodes_rec(widgetdata *widget, int i, int j, int output)
{
	int tmp = 0;

	do
	{
		/* we print out the top node, and then go down a level, rather than go down first */
		if (output)
		{
			/* a way of representing graphically how many levels down we are */
			for (tmp = 0; tmp < j; ++ tmp)
			{
				printf("..");
			}

			logger_print(LOG(INFO), "..%s, WidgetObjID: %d", widget->name, widget->WidgetObjID);
		}

		i++;

		/* we want to process the widgets starting from the left hand side of the tree first */
		if (widget->inv)
		{
			i = debug_count_nodes_rec(widget->inv, i, j + 1, output);
		}

		/* get the next sibling for our next loop */
		widget = widget->next;
	}
	while (widget);

	return i;
}

void debug_count_nodes(int output)
{
	int i = 0;

	logger_print(LOG(INFO), "Output of widget nodes:");
	logger_print(LOG(INFO), "========================================");

	if (widget_list_head)
	{
		i = debug_count_nodes_rec(widget_list_head, 0, 0, output);
	}

	logger_print(LOG(INFO), "========================================");
	logger_print(LOG(INFO), "..Total widget nodes: %d", i);
}
#endif

/**
 * Load the widgets interface from a file.
 * @param filename The interface filename.
 * @return 1 on success, 0 on failure. */
static int load_interface_file(char *filename)
{
	int i = -1, pos;
	FILE *stream;
	widgetdata *widget = NULL;
	char line[256], keyword[256], parameter[256];
	int found_widget[TOTAL_SUBWIDGETS] = {0};

#ifdef DEBUG_WIDGET
	logger_print(LOG(DEBUG), "Entering load_interface_file()..");
#endif

	/* Sanity check - if the file doesn't exist, exit with error */
	if (!(stream = fopen_wrapper(filename, "r")))
	{
		logger_print(LOG(INFO), "Can't find file %s.", filename);
		return 0;
	}

	/* Read the settings from the file */
	while (fgets(line, 255, stream))
	{
		if (line[0] == '#' || line[0] == '\n')
		{
			continue;
		}

		i = 0;

		while (line[i] && line[i] != ':')
		{
			i++;
		}

		line[++i] = '\0';

		strncpy(keyword, line, sizeof(keyword));
		strncpy(parameter, line + i + 1, sizeof(parameter));

		/* Remove the newline character */
		parameter[strcspn(line + i + 1, "\n")] = 0;

		/* Beginning */
		if (strncmp(keyword, "Widget:", 7) == 0)
		{
#ifdef DEBUG_WIDGET
			logger_print(LOG(DEBUG), "..Trying to find 'Widget: %s'", parameter);
#endif

			pos = 0;

			/* Find the index of the widget for reference */
			while (pos < TOTAL_SUBWIDGETS && (strcmp(con_widget[pos].name, parameter) != 0))
			{
				++pos;
			}

			/* The widget name couldn't be found? */
			if (pos >= TOTAL_SUBWIDGETS)
			{
				continue;
			}
			/* Get the block */
			else
			{
				if (!con_widget[pos].no_kill)
				{
					continue;
				}

				/* If we haven't found this widget, mark it */
				if (!found_widget[pos])
				{
#ifdef DEBUG_WIDGET
					logger_print(LOG(INFO), "Found! (Index = %d) (%d widgets total)", pos, TOTAL_SUBWIDGETS);
#endif
					found_widget[pos] = 1;
				}

				/* create the widget with that ID, it is already fully initialized to the defaults */
				widget = create_widget_object(pos);

				/* in case something went wrong */
				if (!widget)
				{
#ifdef DEBUG_WIDGET
					logger_print(LOG(DEBUG), ".. Failed to create widget!");
#endif
					continue;
				}

				while (fgets(line, 255, stream))
				{
					if (line[0] == '#' || line[0] == '\n')
					{
						continue;
					}

					/* End marker */
					if (!strncmp(line, "end", 3))
					{
						break;
					}

					i = 0;

					while (line[i] && line[i] != ':')
					{
						i++;
					}

					line[++i] = '\0';
					strcpy(keyword, line);
					strcpy(parameter, line + i + 1);

					if (strncmp(keyword, "x:", 2) == 0)
					{
						widget->x1 = atoi(parameter);
#ifdef DEBUG_WIDGET
						logger_print(LOG(DEBUG), "..Loading: (%s %d)", keyword, widget->x1);
#endif
					}
					else if (strncmp(keyword, "y:", 2) == 0)
					{
						widget->y1 = atoi(parameter);
#ifdef DEBUG_WIDGET
						logger_print(LOG(DEBUG), "..Loading: (%s %d)", keyword, widget->y1);
#endif
					}
					else if (strncmp(keyword, "moveable:", 9) == 0)
					{
						widget->moveable = atoi(parameter);
#ifdef DEBUG_WIDGET
						logger_print(LOG(DEBUG), "..Loading: (%s %d)", keyword, widget->moveable);
#endif
					}
					else if (strncmp(keyword, "active:", 7) == 0)
					{
						widget->show = atoi(parameter);
#ifdef DEBUG_WIDGET
						logger_print(LOG(DEBUG), "..Loading: (%s %d)", keyword, widget->show);
#endif
					}
					else if (strncmp(keyword, "width:", 6) == 0)
					{
						widget->wd = atoi(parameter);
#ifdef DEBUG_WIDGET
						logger_print(LOG(DEBUG), "..Loading: (%s %d)", keyword, widget->wd);
#endif
					}
					else if (strncmp(keyword, "height:", 7) == 0)
					{
						widget->ht = atoi(parameter);
#ifdef DEBUG_WIDGET
						logger_print(LOG(DEBUG), "..Loading: (%s %d)", keyword, widget->ht);
#endif
					}
					else if (!strncmp(keyword, "font:", 5))
					{
						textwin_struct *textwin = TEXTWIN(widget);
						char font_name[MAX_BUF];
						int font_size, font_id;

						if (textwin && sscanf(parameter, "%s %d", font_name, &font_size) == 2 && (font_id = get_font_id(font_name, font_size)) != -1)
						{
							textwin->font = font_id;
						}
					}
				}
			}
		}
	}

	fclose(stream);

	/* Go through the widgets */
	for (pos = 0; pos < TOTAL_SUBWIDGETS; ++pos)
	{
		/* If a required widget was not found, load the default data for it. */
		if (!found_widget[pos] && con_widget[pos].no_kill)
		{
			/* A newly created widget is loaded with the default values. */
			create_widget_object(pos);
		}
	}

#ifdef DEBUG_WIDGET
	logger_print(LOG(DEBUG), "..load_interface_file(): Done.");
#endif

	return 1;
}

/**
 * Save the widgets interface to a file. */
void save_interface_file(void)
{
	FILE *stream;

	/* Leave, if there's an error opening or creating */
	if (!(stream = fopen_wrapper(INTERFACE_FILE, "w")))
	{
		return;
	}

	fputs("#############################################\n", stream);
	fputs("# This is the Atrinik client interface file #\n", stream);
	fputs("#############################################\n", stream);

	/* start walking through the widgets */
	save_interface_file_rec(widget_list_foot, stream);

	fclose(stream);
}

/**
 * The recursive part of save_interface_file().
 * NEVER call this explicitly, use save_interface_file() in order to use this safely. */
void save_interface_file_rec(widgetdata *widget, FILE *stream)
{
	do
	{
		/* skip the widget if it shouldn't be saved */
		if (!widget->save)
		{
			widget = widget->prev;
			continue;
		}

		/* we want to process the widgets starting from the left hand side of the tree first */
		if (widget->inv_rev)
		{
			save_interface_file_rec(widget->inv_rev, stream);
		}

		fprintf(stream, "\nWidget: %s\n", widget->name);
		fprintf(stream, "moveable: %d\n", widget->moveable);
		fprintf(stream, "active: %d\n", widget->show);
		fprintf(stream, "x: %d\n", widget->x1);
		fprintf(stream, "y: %d\n", widget->y1);

		if (widget->save_width_height)
		{
			fprintf(stream, "width: %d\n", widget->wd);
			fprintf(stream, "height: %d\n", widget->ht);
		}

		switch (widget->WidgetTypeID)
		{
			case CHATWIN_ID:
			case MSGWIN_ID:
			{
				textwin_struct *textwin = TEXTWIN(widget);

				fprintf(stream, "font: %s %"FMT64U"\n", get_font_filename(textwin->font), (uint64) fonts[textwin->font].size);
				break;
			}
		}

		/* End of block */
		fputs("end\n", stream);

		/* get the next sibling for our next loop */
		widget = widget->prev;
	}
	while (widget);
}

/**
 * Mouse is down. Check for owner of the mouse focus.
 * Setup widget dragging, if enabled
 * @param x Mouse X position.
 * @param y Mouse Y position.
 * @param event SDL event type.
 * @return 1 if this is a widget and we're handling the mouse, 0 otherwise. */
int widget_event_mousedn(int x, int y, SDL_Event *event)
{
	widgetdata *widget;

	/* Try to put down a widget that is being moved. */
	if (widget_event_move.active)
	{
		return widget_event_move_stop(x, y);
	}

	/* Update the widget event struct if the mouse is in a widget, or
	 * else get out of here for sanity reasons. */
	if (!widget_event_respond(x, y))
	{
		return 0;
	}

	widget = widget_mouse_event.owner;

	/* sanity check */
	if (!widget)
	{
		return 0;
	}

	/* Set the priority to this widget */
	SetPriorityWidget(widget);

	/* Right mouse button was clicked */
	if (event->button.button == SDL_BUTTON_RIGHT && widget->WidgetTypeID != MAP_ID && !cur_widget[MENU_ID])
	{
		widgetdata *menu;

		/* Create a context menu for the widget clicked on. */
		menu = create_menu(x, y, widget);

		if ((widget->WidgetSubtypeID == MAIN_INV_ID || widget->WidgetSubtypeID == BELOW_INV_ID) && INVENTORY_MOUSE_INSIDE(widget, x, y))
		{
			if (widget->WidgetSubtypeID == MAIN_INV_ID)
			{
				add_menuitem(menu, "Drop", &menu_inventory_drop, MENU_NORMAL, 0);
			}

			add_menuitem(menu, "Get", &menu_inventory_get, MENU_NORMAL, 0);

			if (widget->WidgetSubtypeID == BELOW_INV_ID)
			{
				add_menuitem(menu, "Get all", &menu_inventory_getall, MENU_NORMAL, 0);
			}

			add_menuitem(menu, "Examine", &menu_inventory_examine, MENU_NORMAL, 0);

			if (setting_get_int(OPT_CAT_DEVEL, OPT_OPERATOR))
			{
				add_menuitem(menu, "Load to console", &menu_inventory_loadtoconsole, MENU_NORMAL, 0);
			}

			if (widget->WidgetSubtypeID == MAIN_INV_ID)
			{
				add_menuitem(menu, "More  >", &menu_inventory_submenu_more, MENU_SUBMENU, 0);
			}

			/* Process the right click event so the correct item is
			 * selected. */
			widget_inventory_event(widget, event);
		}
		else
		{
			add_menuitem(menu, "Move Widget", &menu_move_widget, MENU_NORMAL, 0);

			if (widget->WidgetSubtypeID == MAIN_INV_ID)
			{
				add_menuitem(menu, "Inventory Filters  >", &menu_inv_filter_submenu, MENU_SUBMENU, 0);
			}
			else if (widget->WidgetSubtypeID == MSGWIN_ID || widget->WidgetSubtypeID == CHATWIN_ID)
			{
				add_menuitem(menu, "Clear", &menu_textwin_clear, MENU_NORMAL, 0);
				add_menuitem(menu, "Copy", &menu_textwin_copy, MENU_NORMAL, 0);
				add_menuitem(menu, "Increase Font Size", &menu_textwin_font_inc, MENU_NORMAL, 0);
				add_menuitem(menu, "Decrease Font Size", &menu_textwin_font_dec, MENU_NORMAL, 0);
			}
		}

		menu_finalize(menu);

		return 1;
	}
	/* Start resizing. */
	else if (widget->resizeable && widget->resize_flags && event->button.button == SDL_BUTTON_LEFT)
	{
		widget_event_resize.active = 1;
		widget_event_resize.owner = widget;
	}
	/* Normal condition - respond to mouse down event */
	else
	{
		/* Handler(s) for miscellaneous mouse movement(s) go here. */

		/* Special case for menuitems, if menuitem or a widget inside is clicked on, calls the function tied to the menuitem. */
		widget_menu_event(widget, x, y);

		/* Place here all the mousedown handlers. */
		switch (widget->WidgetTypeID)
		{
			case MENU_B_ID:
				widget_menubuttons_event(widget, event);
				break;

			case QUICKSLOT_ID:
				widget_quickslots_mouse_event(widget, event);
				break;

			case CHATWIN_ID:
			case MSGWIN_ID:
				textwin_event(widget, event);
				break;

			case RANGE_ID:
				widget_range_event(widget, x, y, *event, MOUSE_DOWN);
				break;

			case MAIN_INV_ID:
			case BELOW_INV_ID:
				widget_inventory_event(widget, event);
				break;

			case PLAYER_INFO_ID:
				widget_player_data_event(widget, x, y);
				break;

			case MPLAYER_ID:
				widget_mplayer_mevent(widget, event);
				break;

			case SPELLS_ID:
				widget_spells_mevent(widget, event);
				break;

			case MAP_ID:
				widget_map_mevent(widget, event);
				break;

			case SKILLS_ID:
				widget_skills_mevent(widget, event);
				break;

			case PARTY_ID:
				widget_party_mevent(widget, event);
				break;

			case NOTIFICATION_ID:
				widget_notification_event(widget, event);
				break;
		}
	}

	/* User didn't click on a menu, so remove any menus that exist. */
	if (widget->WidgetSubtypeID != MENU_ID)
	{
		widgetdata *menu, *tmp;

		for (menu = cur_widget[MENU_ID]; menu; menu = tmp)
		{
			tmp = menu->type_next;
			remove_widget_object(menu);
		}
	}

	return 1;
}

/**
 * Mouse is up. Check for owner of mouse focus.
 * Stop dragging the widget, if active.
 * @param x Mouse X position.
 * @param y Mouse Y position.
 * @param event SDL event type.
 * @return 1 if this is a widget and we're handling the mouse, 0 otherwise. */
int widget_event_mouseup(int x, int y, SDL_Event *event)
{
	widgetdata *widget;

	/* Widget is now being moved, don't do anything. */
	if (widget_event_move.active)
	{
		return 1;
	}
	/* End resizing. */
	else if (widget_event_resize.active)
	{
		widget_event_resize.active = 0;
		widget_event_resize.owner = NULL;
		return 1;
	}
	/* Normal condition - respond to mouse up event */
	else
	{
		/* update the widget event struct if the mouse is in a widget, or else get out of here for sanity reasons */
		if (!widget_event_respond(x, y))
		{
			return 0;
		}

		widget = widget_mouse_event.owner;

		/* sanity check */
		if (!widget)
		{
			return 0;
		}

		/* Handler for the widgets go here */
		switch (widget->WidgetTypeID)
		{
			case QUICKSLOT_ID:
				widget_quickslots_mouse_event(widget, event);
				break;

			case CHATWIN_ID:
			case MSGWIN_ID:
				textwin_event(widget, event);
				break;

			case RANGE_ID:
				widget_range_event(widget, x, y, *event, MOUSE_UP);
				break;

			case MAIN_INV_ID:
			case BELOW_INV_ID:
				widget_inventory_event(widget, event);
				break;

			case MPLAYER_ID:
				widget_mplayer_mevent(widget, event);
				break;

			case SPELLS_ID:
				widget_spells_mevent(widget, event);
				break;

			case MENU_B_ID:
				widget_menubuttons_event(widget, event);
				break;

			case MAP_ID:
				widget_map_mevent(widget, event);
				break;

			case SKILLS_ID:
				widget_skills_mevent(widget, event);
				break;

			case PARTY_ID:
				widget_party_mevent(widget, event);
				break;
		}

		return 1;
	}
}

/**
 * Mouse was moved. Check for owner of mouse focus.
 * Drag the widget, if active.
 * @param x Mouse X position.
 * @param y Mouse Y position.
 * @param event SDL event type.
 * @return 1 if this is a widget and we're handling the mouse, 0 otherwise. */
int widget_event_mousemv(int x, int y, SDL_Event *event)
{
	widgetdata *widget;

	/* Widget moving condition */
	if (widget_event_move.active)
	{
		int nx, ny;

		widget = widget_event_move.owner;

		/* The widget being moved doesn't exist. Sanity check in case something mad like this happens. */
		if (!widget)
		{
			return 0;
		}

		x -= widget_event_move.xOffset;
		y -= widget_event_move.yOffset;
		nx = x;
		ny = y;

		/* Widget snapping logic courtesy of OpenTTD (GPLv2). */
		if (setting_get_int(OPT_CAT_GENERAL, OPT_SNAP_RADIUS))
		{
			widgetdata *tmp;
			int delta, hsnap, vsnap;

			delta = 0;
			hsnap = vsnap = setting_get_int(OPT_CAT_GENERAL, OPT_SNAP_RADIUS);

			for (tmp = widget_list_head; tmp; tmp = tmp->next)
			{
				if (tmp == widget || tmp->disable_snapping || !tmp->show || !tmp->visible)
				{
					continue;
				}

				if (y + widget->ht > tmp->y1 && y < tmp->y1 + tmp->ht)
				{
					/* Your left border <-> other right border */
					delta = abs(tmp->x1 + tmp->wd - x);

					if (delta <= hsnap)
					{
						nx = tmp->x1 + tmp->wd;
						hsnap = delta;
					}

					/* Your right border <-> other left border */
					delta = abs(tmp->x1 - x - widget->wd);

					if (delta <= hsnap)
					{
						nx = tmp->x1 - widget->wd;
						hsnap = delta;
					}
				}

				if (widget->y1 + widget->ht >= tmp->y1 && widget->y1 <= tmp->y1 + tmp->ht)
				{
					/* Your left border <-> other left border */
					delta = abs(tmp->x1 - x);

					if (delta <= hsnap)
					{
						nx = tmp->x1;
						hsnap = delta;
					}

					/* Your right border <-> other right border */
					delta = abs(tmp->x1 + tmp->wd - x - widget->wd);

					if (delta <= hsnap)
					{
						nx = tmp->x1 + tmp->wd - widget->wd;
						hsnap = delta;
					}
				}

				if (x + widget->wd > tmp->x1 && x < tmp->x1 + tmp->wd)
				{
					/* Your top border <-> other bottom border */
					delta = abs(tmp->y1 + tmp->ht - y);

					if (delta <= vsnap)
					{
						ny = tmp->y1 + tmp->ht;
						vsnap = delta;
					}

					/* Your bottom border <-> other top border */
					delta = abs(tmp->y1 - y - widget->ht);

					if (delta <= vsnap)
					{
						ny = tmp->y1 - widget->ht;
						vsnap = delta;
					}
				}

				if (widget->x1 + widget->wd >= tmp->x1 && widget->x1 <= tmp->x1 + tmp->wd)
				{
					/* Your top border <-> other top border */
					delta = abs(tmp->y1 - y);

					if (delta <= vsnap)
					{
						ny = tmp->y1;
						vsnap = delta;
					}

					/* Your bottom border <-> other bottom border */
					delta = abs(tmp->y1 + tmp->ht - y - widget->ht);

					if (delta <= vsnap)
					{
						ny = tmp->y1 + tmp->ht - widget->ht;
						vsnap = delta;
					}
				}
			}
		}

		/* we move the widget here, as well as all the widgets inside it if they exist */
		/* we use the recursive version since we already have the outermost container */
		move_widget_rec(widget, nx - widget->x1, ny - widget->y1);

		/* Ensure widget is on-screen. */
		if (!setting_get_int(OPT_CAT_CLIENT, OPT_OFFSCREEN_WIDGETS))
		{
			widget_ensure_onscreen(widget);
		}

		map_udate_flag = 2;

		return 1;
	}
	else if (widget_event_resize.active)
	{
		widget = widget_event_resize.owner;

		if (!widget)
		{
			return 0;
		}

		if (widget->resize_flags & (RESIZE_LEFT | RESIZE_RIGHT))
		{
			resize_widget(widget, widget->resize_flags & ~(RESIZE_TOP | RESIZE_BOTTOM), MAX(widget->min_w, widget->resize_flags & RESIZE_LEFT ? widget->x1 - x + widget->wd : x - widget->x1));
		}

		if (widget->resize_flags & (RESIZE_TOP | RESIZE_BOTTOM))
		{
			resize_widget(widget, widget->resize_flags & ~(RESIZE_LEFT | RESIZE_RIGHT), MAX(widget->min_h, widget->resize_flags & RESIZE_TOP ? widget->y1 - y + widget->ht : y - widget->y1));
		}

		return 1;
	}
	/* Normal condition - respond to mouse move event */
	else
	{
		/* update the widget event struct if the mouse is in a widget, or else get out of here for sanity reasons */
		if (!widget_event_respond(x, y))
		{
			return 0;
		}

		widget = widget_mouse_event.owner;

		/* sanity check */
		if (!widget)
		{
			return 0;
		}

		if (widget->resizeable)
		{
			widget->resizeable = 1;
			widget->resize_flags = 0;

			if (y >= widget->y1 && y <= widget->y1 + 2)
			{
				widget->resize_flags |= RESIZE_TOP;
			}
			else if (y >= widget->y1 + widget->ht - 2 && y <= widget->y1 + widget->ht)
			{
				widget->resize_flags |= RESIZE_BOTTOM;
			}

			if (x >= widget->x1 && x <= widget->x1 + 2)
			{
				widget->resize_flags |= RESIZE_LEFT;
			}
			else if (x >= widget->x1 + widget->wd - 2 && x <= widget->x1 + widget->wd)
			{
				widget->resize_flags |= RESIZE_RIGHT;
			}
		}

		/* Handlers for miscellaneous mouse movements go here */

		/* Handlers for the widgets mouse move */
		switch (widget->WidgetTypeID)
		{
			case CHATWIN_ID:
			case MSGWIN_ID:
				textwin_event(widget, event);
				break;

			case MAIN_INV_ID:
			case BELOW_INV_ID:
				widget_inventory_event(widget, event);
				break;

			case MPLAYER_ID:
				widget_mplayer_mevent(widget, event);
				break;

			case SPELLS_ID:
				widget_spells_mevent(widget, event);
				break;

			case MENU_B_ID:
				widget_menubuttons_event(widget, event);
				break;

			case MAP_ID:
				widget_map_mevent(widget, event);
				break;

			case SKILLS_ID:
				widget_skills_mevent(widget, event);
				break;

			case PARTY_ID:
				widget_party_mevent(widget, event);
				break;

			case QUICKSLOT_ID:
				widget_quickslots_mouse_event(widget, event);
				break;
		}

		return 1;
	}
}

/** Handles the initiation of widget dragging. */
int widget_event_start_move(widgetdata *widget, int x, int y)
{
	/* get the outermost container so we can move the container with everything in it */
	widget = get_outermost_container(widget);

	/* if its moveable, start moving it when the conditions warrant it, or else run away */
	if (!widget->moveable)
	{
		return 0;
	}

	/* we know this widget owns the mouse.. */
	widget_event_move.active = 1;

	/* start the movement procedures */
	widget_event_move.owner = widget;
	widget_event_move.xOffset = x - widget->x1;
	widget_event_move.yOffset = y - widget->y1;

	/* enable the custom cursor */
	f_custom_cursor = MSCURSOR_MOVE;
	/* hide the system cursor */
	SDL_ShowCursor(0);

#ifdef WIN32
	/* Workaround another bug with SDL 1.2.x on Windows. Make sure the cursor
	 * is in the center of the screen if we are in fullscreen mode. */
	if (ScreenSurface->flags & SDL_FULLSCREEN)
	{
		SDL_WarpMouse(ScreenSurface->w / 2, ScreenSurface->h / 2);
	}
#endif

	return 1;
}

int widget_event_move_stop(int x, int y)
{
	widgetdata *widget, *widget_container;

	if (!widget_event_move.active)
	{
		return 0;
	}

	widget = widget_mouse_event.owner;

	widget_event_move.active = 0;
	widget_mouse_event.x = x;
	widget_mouse_event.y = y;
	/* No widgets are being moved now. */
	widget_event_move.owner = NULL;

	/* Disable the custom cursor. */
	f_custom_cursor = 0;

	/* Show the system cursor. */
	SDL_ShowCursor(1);

	/* Due to a bug in SDL 1.2.x, the mouse X/Y position is not updated
	 * while in fullscreen with the cursor hidden, so we must take care
	 * of it ourselves. Apparently SDL 1.3 should fix it.
	 * See http://old.nabble.com/Mouse-movement-problems-in-fullscreen-mode-td20890669.html
	 * for details. */
	SDL_WarpMouse(x, y);

	/* Somehow the owner before the widget dragging is gone now. Not a
	 * good idea to carry on... */
	if (!widget)
	{
		return 0;
	}

	/* Check to see if it's on top of a widget container. */
	widget_container = get_widget_owner(x, y, widget->next, NULL);

	/* Attempt to insert it into the widget container if it exists. */
	insert_widget_in_container(widget_container, widget);

	return 1;
}

/** Updates the widget mouse event struct in order to respond to an event. */
int widget_event_respond(int x, int y)
{
	/* only update the owner if there is no event override taking place */
	if (!widget_event_override())
	{
		widget_mouse_event.owner = get_widget_owner(x, y, NULL, NULL);
	}

	/* sanity check.. return if mouse is not in a widget */
	if (!widget_mouse_event.owner)
	{
		return 0;
	}

	/* setup the event structure in response */
	widget_mouse_event.x = x;
	widget_mouse_event.y = y;

	return 1;
}

/** Priority override function, we have to have that here for resizing... */
int widget_event_override(void)
{
	return 0;
}

/** Find the widget with mouse focus on a mouse-hit-test basis. */
widgetdata *get_widget_owner(int x, int y, widgetdata *start, widgetdata *end)
{
	widgetdata *success;

	/* mouse cannot be used by widgets */
	if (IsMouseExclusive)
	{
		return NULL;
	}

	/* no widgets exist */
	if (!widget_list_head)
	{
		return NULL;
	}

	/* sometimes we want a fast way to get the widget behind the widget at the front.
	 * this is what start is for, and we will only start walking the list beginning with start.
	 * if start is NULL, we just do a regular search */
	if (!start)
	{
		start = widget_list_head;
	}

	/* ok, let's kick off the recursion. if we find our widget, we get a widget back. if not, we get a big fat NULL */
	success = get_widget_owner_rec(x, y, start, end);

	/*logger_print(LOG(DEBUG), "WIDGET OWNER: %s, WidgetObjID: %d", success? success->name: "NULL", success? success->WidgetObjID: -1);*/

	return success;
}

/* traverse through the tree & perform custom or default hit-test */
widgetdata *get_widget_owner_rec(int x, int y, widgetdata *widget, widgetdata *end)
{
	widgetdata *success = NULL;

	do
	{
		/* we want to get the first widget starting from the left hand side of the tree first */
		if (widget->inv)
		{
			success = get_widget_owner_rec(x, y, widget->inv, end);

			/* we found a widget in the hit test? if so, get out of this recursive mess with our prize */
			if (success)
			{
				return success;
			}
		}

		/* skip if widget is hidden */
		if (!widget->show)
		{
			widget = widget->next;
			continue;
		}

		switch (widget->WidgetTypeID)
		{
			default:
				if (x >= widget->x1 && x <= (widget->x1 + widget->wd) && y >= widget->y1 && y <= (widget->y1 + widget->ht))
				{
					return widget;
				}
		}

		/* get the next sibling for our next loop */
		widget = widget->next;
	}
	while (widget || widget != end);

	return NULL;
}

/**
 * Function list for each widget. Calls the widget with the process type.
 * @param nID The widget ID. */
static void process_widget(widgetdata *widget)
{
	switch (widget->WidgetTypeID)
	{
		case STATS_ID:
			widget_player_stats(widget);
			break;

		case RESIST_ID:
			widget_show_resist(widget);
			break;

		case MAIN_LVL_ID:
			widget_show_main_lvl(widget);
			break;

		case SKILL_EXP_ID:
			widget_show_skill_exp(widget);
			break;

		case REGEN_ID:
			widget_show_regeneration(widget);
			break;

		case SKILL_LVL_ID:
			widget_skillgroups(widget);
			break;

		case MENU_B_ID:
			widget_menubuttons(widget);
			break;

		case QUICKSLOT_ID:
			widget_quickslots(widget);
			break;

		case CHATWIN_ID:
		case MSGWIN_ID:
			widget_textwin_show(widget);
			break;

		case PDOLL_ID:
			widget_show_player_doll(widget);
			break;

		case PLAYER_INFO_ID:
			widget_show_player_data(widget);
			break;

		case RANGE_ID:
			widget_show_range(widget);
			break;

		case MAIN_INV_ID:
		case BELOW_INV_ID:
			widget_inventory_render(widget);
			break;

		case MAPNAME_ID:
			widget_show_mapname(widget);
			break;

		case IN_CONSOLE_ID:
			widget_show_console(widget);
			break;

		case IN_NUMBER_ID:
			widget_show_number(widget);
			break;

		case FPS_ID:
			widget_show_fps(widget);
			break;

		case CONTAINER_ID:
			widget_show_container(widget);
			break;

		case LABEL_ID:
			widget_show_label(widget);
			break;

		case TEXTURE_ID:
			widget_show_texture(widget);
			break;

		case MPLAYER_ID:
			widget_show_mplayer(widget);
			break;

		case SPELLS_ID:
			widget_spells_render(widget);
			break;

		case SKILLS_ID:
			widget_skills_render(widget);
			break;

		case PARTY_ID:
			widget_party_render(widget);
			break;

		case NOTIFICATION_ID:
			widget_notification_render(widget);
			break;
	}
}

/**
 * Process background tasks of a widget; called even if the widget is
 * not currently visible.
 * @param widget The widget. */
static void process_widget_background(widgetdata *widget)
{
	switch (widget->WidgetTypeID)
	{
		case MPLAYER_ID:
			widget_mplayer_background(widget);
			break;

		case PARTY_ID:
			widget_party_background(widget);
			break;
	}
}

/**
 * Traverse through all the widgets and call the corresponding handlers.
 * This is now a wrapper function just to make the sanity checks before continuing with the actual handling. */
void process_widgets(void)
{
	/* sanity check */
	if (!widget_list_foot)
	{
		return;
	}

	process_widgets_rec(widget_list_foot);
}

/**
 * The priority list is a binary tree, so we walk the tree by using loops and recursions.
 * We actually only need to recurse for every child node. When we traverse the siblings, we can just do a simple loop.
 * This makes it as fast as a linear linked list if there are no child nodes. */
void process_widgets_rec(widgetdata *widget)
{
	do
	{
		process_widget_background(widget);

		/* if widget isn't hidden, process it. this is mostly to do with rendering them */
		if (widget->show && widget->visible)
		{
			process_widget(widget);
		}

		/* we want to process the widgets starting from the right hand side of the tree first */
		if (widget->inv_rev)
		{
			process_widgets_rec(widget->inv_rev);
		}

		/* get the previous sibling for our next loop */
		widget = widget->prev;
	}
	while (widget);
}

/**
 * A recursive function to bring a widget to the front of the priority list.
 * This makes the widget get displayed last so that they appear on top, and handle events first.
 * In order to do this, we need to recurse backwards up the tree to the top node,
 * and then work our way back down again, bringing each node in front of its siblings. */
void SetPriorityWidget(widgetdata *node)
{
#ifdef DEBUG_WIDGET
	logger_print(LOG(DEBUG), "Entering SetPriorityWidget(WidgetObjID=%d)..", node->WidgetObjID);
#endif

	/* widget doesn't exist, means parent node has no children, so nothing to do here */
	if (!node)
	{
#ifdef DEBUG_WIDGET
		logger_print(LOG(DEBUG), "..SetPriorityWidget(): Done (Node does not exist).");
#endif
		return;
	}

	if (node->WidgetTypeID == MAP_ID)
	{
		return;
	}

#ifdef DEBUG_WIDGET
	logger_print(LOG(DEBUG), "..BEFORE:");
	logger_print(LOG(DEBUG), "....node: %p - %s", node, node->name);
	logger_print(LOG(DEBUG), "....node->env: %p - %s", node->env, node->env? node->env->name: "NULL");
	logger_print(LOG(DEBUG), "....node->prev: %p - %s, node->next: %p - %s", node->prev, node->prev? node->prev->name: "NULL", node->next, node->next? node->next->name: "NULL");
	logger_print(LOG(DEBUG), "....node->inv: %p - %s, node->inv_rev: %p - %s", node->inv, node->inv? node->inv->name: "NULL", node->inv_rev, node->inv_rev? node->inv_rev->name: "NULL");
#endif

	/* see if the node has a parent before continuing */
	if (node->env)
	{
		SetPriorityWidget(node->env);

		/* Strip containers are sorted in a fixed order, and no part of any widget inside should be covered by a sibling.
		 * This means we don't need to bother moving the node to the front inside the container. */
		switch (node->env->WidgetSubtypeID)
		{
			case CONTAINER_STRIP_ID:
			case MENU_ID:
			case MENUITEM_ID:
				return;
		}
	}

	/* now we need to move our other node in front of the first sibling */
	if (!node->prev)
	{
#ifdef DEBUG_WIDGET
		logger_print(LOG(DEBUG), "..SetPriorityWidget(): Done (Node already at front).");
#endif
		/* no point continuing, node is already at the front */
		return;
	}

	/* Unlink node from its current position in the priority tree. */

	/* node is last sibling, clear the pointer of the previous sibling */
	if (!node->next)
	{
		/* node also has a parent pointing to it, hand the inv_rev pointer to the previous sibling */
		if (node->env)
		{
			node->env->inv_rev = node->prev;
		}
		/* no parent, this must be the foot then, so move it to the previous node */
		else
		{
			widget_list_foot = node->prev;
		}

		node->prev->next = NULL;
	}
	else
	{
		/* link up the adjacent nodes */
		node->prev->next = node->next;
		node->next->prev = node->prev;
	}

	/* Insert node at the head of its container, or make it the root node if it is not in a container. */

	/* Node is now the first sibling so the parent should point to it. */
	if (node->env)
	{
		node->next = node->env->inv;
		node->env->inv = node;
	}
	/* We are out of containers and this node is about to become the first sibling, which means it's taking the place of the root node. */
	else
	{
		node->next = widget_list_head;
		widget_list_head = node;
	}

	/* Point the former head node to this node. */
	node->next->prev = node;
	/* There's no siblings in front of node now. */
	node->prev = NULL;

#ifdef DEBUG_WIDGET
	logger_print(LOG(DEBUG), "..AFTER:");
	logger_print(LOG(DEBUG), "....node: %p - %s", node, node->name);
	logger_print(LOG(DEBUG), "....node->env: %p - %s", node->env, node->env? node->env->name: "NULL");
	logger_print(LOG(DEBUG), "....node->prev: %p - %s, node->next: %p - %s", node->prev, node->prev? node->prev->name: "NULL", node->next, node->next? node->next->name: "NULL");
	logger_print(LOG(DEBUG), "....node->inv: %p - %s, node->inv_rev: %p - %s", node->inv, node->inv? node->inv->name: "NULL", node->inv_rev, node->inv_rev? node->inv_rev->name: "NULL");

	logger_print(LOG(DEBUG), "..SetPriorityWidget(): Done.");
#endif
}

/**
 * Like SetPriorityWidget(), but in reverse.
 * @param node The widget. */
void SetPriorityWidget_reverse(widgetdata *node)
{
	if (!node)
	{
		return;
	}

	if (!node->next)
	{
		return;
	}

	if (!node->prev)
	{
		if (node->env)
		{
			node->env->inv_rev = node->next;
		}
		else
		{
			widget_list_head = node->next;
		}

		node->next->prev = NULL;
	}
	else
	{
		node->next->prev = node->prev;
		node->prev->next = node->next;
	}

	if (node->env)
	{
		node->prev = node->env->inv;
		node->env->inv = node;
	}
	else
	{
		node->prev = widget_list_foot;
		widget_list_foot = node;
	}

	node->prev->next = node;
	node->next = NULL;
}

void insert_widget_in_container(widgetdata *widget_container, widgetdata *widget)
{
	_widget_container *container;
	_widget_container_strip *container_strip;

	/* sanity checks */
	if (!widget_container || !widget)
	{
		return;
	}

	/* no, we don't want to end the universe just yet... */
	if (widget_container == widget)
	{
		return;
	}

	/* is the widget already in a container? */
	if (widget->env)
	{
		return;
	}

	/* if the widget isn't a container, get out of here */
	if (widget_container->WidgetTypeID != CONTAINER_ID)
	{
		return;
	}

	/* we have our container, now we attempt to place the widget inside it */
	container = CONTAINER(widget_container);

	/* check to see if the widget is compatible with it */
	if (container->widget_type != -1 && container->widget_type != widget->WidgetTypeID)
	{
		return;
	}

	/* if we get here, we now proceed to insert the widget into the container */

	/* snap the widget into the widget container if it is a strip container */
	if (widget_container->inv)
	{
		switch (widget_container->WidgetSubtypeID)
		{
			case CONTAINER_STRIP_ID:
			case MENU_ID:
			case MENUITEM_ID:
				container_strip = CONTAINER_STRIP(widget_container);

				/* container is horizontal, insert the widget to the right of the first widget in its inventory */
				if (container_strip->horizontal)
				{
					move_widget_rec(widget, widget_container->inv->x1 + widget_container->inv->wd + container_strip->inner_padding - widget->x1, widget_container->y1 + container->outer_padding_top - widget->y1);
				}
				/* otherwise the container is vertical, so insert the widget below the first child widget */
				else
				{
					move_widget_rec(widget, widget_container->x1 + container->outer_padding_left - widget->x1, widget_container->inv->y1 + widget_container->inv->ht + container_strip->inner_padding - widget->y1);
				}

				break;
		}
	}
	/* no widgets inside it yet, so snap it to the bounds of the container */
	else
	{
		move_widget(widget, widget_container->x1 + container->outer_padding_left - widget->x1, widget_container->y1 + container->outer_padding_top - widget->y1);
	}

	/* link up the adjacent nodes, there *should* be at least two nodes next to each other here so no sanity checks should be required */
	if (!widget->prev)
	{
		/* widget is no longer the root now, pass it on to the next widget */
		if (widget == widget_list_head)
		{
			widget_list_head = widget->next;
		}

		widget->next->prev = NULL;
	}
	else if (!widget->next)
	{
		/* widget is no longer the foot, move it to the previous widget */
		if (widget == widget_list_foot)
		{
			widget_list_foot = widget->prev;
		}

		widget->prev->next = NULL;
	}
	else
	{
		widget->prev->next = widget->next;
		widget->next->prev = widget->prev;
	}

	/* the widget to be placed inside will have a new sibling next to it, or NULL if it doesn't exist */
	widget->next = widget_container->inv;
	/* it's also going to be the first child node, so it has no siblings on the other side */
	widget->prev = NULL;

	/* if inventory doesn't exist, set the end child pointer too */
	if (!widget_container->inv)
	{
		widget_container->inv_rev = widget;
	}
	/* otherwise, link the first child in the inventory to the widget about to be inserted */
	else
	{
		widget_container->inv->prev = widget;
	}

	/* this new widget becomes the first widget in the container */
	widget_container->inv = widget;
	/* set the environment of the widget inside */
	widget->env = widget_container;

	/* resize the container to fit the new widget. a little dirty trick here,
	 * we just resize the widget inside by nothing and it will trigger the auto-resize */
	resize_widget(widget, RESIZE_RIGHT, widget->wd);
	resize_widget(widget, RESIZE_BOTTOM, widget->ht);
}

/** Get the outermost container the widget is inside. */
widgetdata *get_outermost_container(widgetdata *widget)
{
	widgetdata *tmp = widget;

	/* Sanity check. */
	if (!widget)
	{
		return NULL;
	}

	/* Get the outsidemost container if the widget is inside one. */
	while (tmp->env)
	{
		tmp = tmp->env;
		widget = tmp;
	}

	return widget;
}

/**
 * Find a widget by its surface.
 * @param surface The surface to look for.
 * @return First widget with the passed surface, NULL if there isn't any
 * such widget. */
widgetdata *widget_find_by_surface(SDL_Surface *surface)
{
	widgetdata *tmp;

	for (tmp = widget_list_head; tmp; tmp = tmp->next)
	{
		if (tmp->widgetSF == surface)
		{
			return tmp;
		}
	}

	return NULL;
}

/**
 * Find the first widget to do a copy operation from, depending on the
 * priority list.
 * @return Widget to copy from, can be NULL. */
widgetdata *widget_find_copy_from(void)
{
	widgetdata *tmp;

	for (tmp = widget_list_head; tmp; tmp = tmp->next)
	{
		if (tmp->WidgetTypeID == CHATWIN_ID || tmp->WidgetTypeID == MSGWIN_ID)
		{
			return tmp;
		}
	}

	return NULL;
}

/* wrapper function to get the outermost container the widget is inside before moving it */
void move_widget(widgetdata *widget, int x, int y)
{
	widget = get_outermost_container(widget);

	move_widget_rec(widget, x, y);
}

/* move all widgets inside the container with the container at the same time */
void move_widget_rec(widgetdata *widget, int x, int y)
{
	/* widget doesn't exist, means the parent node has no children */
	if (!widget)
	{
		return;
	}

	/* no movement needed */
	if (x == 0 && y == 0)
	{
		return;
	}

	/* move the widget */
	widget->x1 += x;
	widget->y1 += y;

	/* here, we want to walk through the inventory of the widget, if it exists.
	 * when we come across a widget, we move it like we did with the container.
	 * we loop until we reach the last sibling, but we also need to go recursive if we find a child node */
	for (widget = widget->inv; widget; widget = widget->next)
	{
		move_widget_rec(widget, x, y);
	}
}

void resize_widget(widgetdata *widget, int side, int offset)
{
	int x = widget->x1;
	int y = widget->y1;
	int width = widget->wd;
	int height = widget->ht;

	if (side & RESIZE_LEFT)
	{
		x = widget->x1 + widget->wd - offset;
		width = offset;
	}
	else if (side & RESIZE_RIGHT)
	{
		width = offset;
	}

	if (side & RESIZE_TOP)
	{
		y = widget->y1 + widget->ht - offset;
		height = offset;
	}
	else if (side & RESIZE_BOTTOM)
	{
		height = offset;
	}

	resize_widget_rec(widget, x, width, y, height);
}

void resize_widget_rec(widgetdata *widget, int x, int width, int y, int height)
{
	widgetdata *widget_container, *tmp, *cmp1, *cmp2, *cmp3, *cmp4;
	_widget_container_strip *container_strip = NULL;
	_widget_container *container = NULL;

	/* move the widget. this is the easy bit, watch as your eyes bleed when you see the next thing we have to do */
	widget->x1 = x;
	widget->y1 = y;
	widget->wd = width;
	widget->ht = height;

	WIDGET_REDRAW(widget);

	/* now we get our parent node if it exists */

	/* loop until we hit the first sibling */
	for (widget_container = widget; widget_container->prev; widget_container = widget_container->prev)
	{
	}

	/* does the first sibling have a parent node? */
	if (widget_container->env)
	{
		/* ok, now we need to resize the parent too. but before we do this, we need to see if other widgets inside should prevent it from
		 * being resized. the code below is ugly, but necessary in order to calculate the new size of the container. and one more thing...
		 * MY EYES! THE GOGGLES DO NOTHING! */

		widget_container = widget_container->env;
		container = CONTAINER(widget_container);

		/* special case for strip containers */
		switch (widget_container->WidgetSubtypeID)
		{
			case CONTAINER_STRIP_ID:
			case MENU_ID:
			case MENUITEM_ID:
				container_strip = CONTAINER_STRIP(widget_container);

				/* we move all the widgets before or after the widget that got resized, depending on which side got the resize */
				if (container_strip->horizontal)
				{
					/* now move everything we come across */
					move_widget_rec(widget, 0, widget_container->y1 + container->outer_padding_top - widget->y1);

					/* every node before the widget we push right */
					for (tmp = widget->prev; tmp; tmp = tmp->prev)
					{
						move_widget_rec(tmp, tmp->next->x1 + tmp->next->wd - tmp->x1 + container_strip->inner_padding, widget_container->y1 + container->outer_padding_top - tmp->y1);
					}

					/* while every node after the widget we push left */
					for (tmp = widget->next; tmp; tmp = tmp->next)
					{
						move_widget_rec(tmp, tmp->prev->x1 - tmp->x1 - tmp->wd - container_strip->inner_padding, widget_container->y1 + container->outer_padding_top - tmp->y1);
					}

					/* we have to set this, otherwise stupid things happen */
					x = widget_container->inv_rev->x1;
					/* we don't want the container moving up or down in this case */
					y = widget_container->y1 + container->outer_padding_top;
				}
				else
				{
					/* now move everything we come across */
					move_widget_rec(widget, widget_container->x1 + container->outer_padding_left - widget->x1, 0);

					/* every node before the widget we push downwards */
					for (tmp = widget->prev; tmp; tmp = tmp->prev)
					{
						move_widget_rec(tmp, widget_container->x1 + container->outer_padding_left - tmp->x1, tmp->next->y1 + tmp->next->ht - tmp->y1 + container_strip->inner_padding);
					}

					/* while every node after the widget we push upwards */
					for (tmp = widget->next; tmp; tmp = tmp->next)
					{
						move_widget_rec(tmp, widget_container->x1 + container->outer_padding_left - tmp->x1, tmp->prev->y1 - tmp->y1 - tmp->ht - container_strip->inner_padding);
					}

					/* we don't want the container moving sideways in this case */
					x = widget_container->x1 + container->outer_padding_left;
					/* we have to set this, otherwise stupid things happen */
					y = widget_container->inv_rev->y1;
				}
				break;
		}

		/* TODO: add the buffer system so that this mess of code will only need to be executed after the user stops resizing the widget */
		cmp1 = cmp2 = cmp3 = cmp4 = widget;

		for (tmp = widget_container->inv; tmp; tmp = tmp->next)
		{
			/* widget's left x co-ordinate becomes greater than tmp's left x coordinate */
			if (cmp1->x1 > tmp->x1)
			{
				x = tmp->x1;
				width += cmp1->x1 - tmp->x1;
				cmp1 = tmp;
			}

			/* widget's top y co-ordinate becomes greater than tmp's top y coordinate */
			if (cmp2->y1 > tmp->y1)
			{
				y = tmp->y1;
				height += cmp2->y1 - tmp->y1;
				cmp2 = tmp;
			}

			/* widget's right x co-ordinate becomes less than tmp's right x coordinate */
			if (cmp3->x1 + cmp3->wd < tmp->x1 + tmp->wd)
			{
				width += tmp->x1 + tmp->wd - cmp3->x1 - cmp3->wd;
				cmp3 = tmp;
			}

			/* widget's bottom y co-ordinate becomes less than tmp's bottom y coordinate */
			if (cmp4->y1 + cmp4->ht < tmp->y1 + tmp->ht)
			{
				height += tmp->y1 + tmp->ht - cmp4->y1 - cmp4->ht;
				cmp4 = tmp;
			}
		}

		x -= container->outer_padding_left;
		y -= container->outer_padding_top;
		width += container->outer_padding_left + container->outer_padding_right;
		height += container->outer_padding_top + container->outer_padding_bottom;

		/* after all that, we now check to see if the parent needs to be resized before we waste even more resources going recursive */
		if (x != widget_container->x1 || y != widget_container->y1 || width != widget_container->wd || height != widget_container->ht)
		{
			resize_widget_rec(widget_container, x, width, y, height);
		}
	}
}

/** Creates a label with the given text, font and colour, and sets the size of the widget to the correct boundaries. */
widgetdata *add_label(char *text, int font, const char *color)
{
	widgetdata *widget;
	_widget_label *label;

	widget = create_widget_object(LABEL_ID);
	label = LABEL(widget);

	label->text = text;

	label->font = font;
	label->color = color;

	resize_widget(widget, RESIZE_RIGHT, string_get_width(font, text, 0));
	resize_widget(widget, RESIZE_BOTTOM, string_get_height(font, text, 0) + 3);

	return widget;
}

/** Creates a texture. */
widgetdata *add_texture(const char *texture)
{
	widgetdata *widget;
	_widget_texture *widget_texture;

	widget = create_widget_object(TEXTURE_ID);
	widget_texture = WIDGET_TEXTURE(widget);

	widget_texture->texture = texture_get(TEXTURE_TYPE_CLIENT, texture);

	resize_widget(widget, RESIZE_RIGHT, TEXTURE_SURFACE(widget_texture->texture)->w);
	resize_widget(widget, RESIZE_BOTTOM, TEXTURE_SURFACE(widget_texture->texture)->h);

	return widget;
}

/** Initializes a menu widget. */
widgetdata *create_menu(int x, int y, widgetdata *owner)
{
	widgetdata *widget_menu = create_widget_object(MENU_ID);
	_widget_container *container_menu = CONTAINER(widget_menu);
	_widget_container_strip *container_strip_menu = CONTAINER_STRIP(widget_menu);

	/* Place the menu at these co-ordinates. */
	widget_menu->x1 = x;
	widget_menu->y1 = y;
	/* Point the menu to the owner. */
	(MENU(widget_menu))->owner = owner;
	/* Magic numbers for now, maybe it will be possible in future to customize this in files. */
	container_menu->outer_padding_left = 2;
	container_menu->outer_padding_right = 2;
	container_menu->outer_padding_top = 2;
	container_menu->outer_padding_bottom = 2;
	container_strip_menu->inner_padding = 0;

	return widget_menu;
}

/** Adds a menuitem to a menu. */
void add_menuitem(widgetdata *menu, char *text, void (*menu_func_ptr)(widgetdata *, int, int), int menu_type, int val)
{
	widgetdata *widget_menuitem, *widget_label, *widget_texture, *tmp;
	_widget_container *container_menuitem, *container_menu;
	_widget_container_strip *container_strip_menuitem;
	_menuitem *menuitem;

	widget_menuitem = create_widget_object(MENUITEM_ID);

	container_menuitem = CONTAINER(widget_menuitem);
	container_strip_menuitem = CONTAINER_STRIP(widget_menuitem);

	/* Initialize attributes. */
	container_menuitem->outer_padding_left = 4;
	container_menuitem->outer_padding_right = 2;
	container_menuitem->outer_padding_top = 2;
	container_menuitem->outer_padding_bottom = 0;
	container_strip_menuitem->inner_padding = 4;
	container_strip_menuitem->horizontal = 1;

	widget_label = add_label(text, FONT_ARIAL10, COLOR_WHITE);

	if (menu_type == MENU_CHECKBOX)
	{
		widget_texture = add_texture(val ? "checkbox_on" : "checkbox_off");
		insert_widget_in_container(widget_menuitem, widget_texture);
	}

	insert_widget_in_container(widget_menuitem, widget_label);
	insert_widget_in_container(menu, widget_menuitem);

	/* Add the pointer to the function to the menuitem. */
	menuitem = MENUITEM(widget_menuitem);
	menuitem->menu_func_ptr = menu_func_ptr;
	menuitem->menu_type = menu_type;

	/* Sanity check. Menuitems should always exist inside a menu. */
	if (widget_menuitem->env && widget_menuitem->env->WidgetSubtypeID == MENU_ID)
	{
		container_menu = CONTAINER(widget_menuitem->env);

		/* Resize labels in each menuitem to the width of the menu. */
		for (tmp = widget_menuitem; tmp; tmp = tmp->next)
		{
			if (tmp->inv)
			{
				container_menuitem = CONTAINER(tmp);

				if (menu_type == MENU_CHECKBOX)
				{
					resize_widget(tmp->inv, RESIZE_RIGHT, menu->wd - tmp->inv_rev->wd - container_strip_menuitem->inner_padding - container_menu->outer_padding_left - container_menu->outer_padding_right - container_menuitem->outer_padding_left - container_menuitem->outer_padding_right);
				}
				else
				{
					resize_widget(tmp->inv, RESIZE_RIGHT, menu->wd - container_menu->outer_padding_left - container_menu->outer_padding_right - container_menuitem->outer_padding_left - container_menuitem->outer_padding_right);
				}
			}
		}
	}
}

/** Placeholder for menu separators. */
void add_separator(widgetdata *widget)
{
	(void) widget;
}

/**
 * Finalizes menu creation.
 *
 * Makes sure the menu does not go over the screen size by adding x/y,
 * using standard GUI behavior.
 * @param widget The menu to finalize. */
void menu_finalize(widgetdata *widget)
{
	int xoff = 0, yoff = 0;

	/* Would the menu go over the maximum screen width? */
	if (widget->x1 + widget->wd > ScreenSurface->w)
	{
		/* Will appear to the left of the cursor instead of right of it. */
		xoff = -widget->wd;

		/* Take submenus into account, and shift them depending on the
		 * parent menu's width. */
		if (widget->type_prev && widget->type_prev->WidgetSubtypeID == MENU_ID)
		{
			xoff += -widget->type_prev->wd + 4;
		}
	}

	/* Similar checks for screen height. */
	if (widget->y1 + widget->ht > ScreenSurface->h)
	{
		/* Submenu, shift it up, so all of it can appear. */
		if (widget->type_prev && widget->type_prev->WidgetSubtypeID == MENU_ID)
		{
			yoff = ScreenSurface->h - widget->ht - widget->y1 - 1;
		}
		/* Will appear above the cursor. */
		else
		{
			yoff = -widget->ht;
		}
	}

	move_widget(widget, xoff, yoff);
}

/** Redraws all widgets of a particular type. */
void widget_redraw_all(int widget_type_id)
{
	widgetdata *widget = cur_widget[widget_type_id];

	for (; widget; widget = widget->type_next)
	{
		widget->redraw = 1;
	}
}

void menu_move_widget(widgetdata *widget, int x, int y)
{
	widget_event_start_move(widget, x, y);
}

void menu_create_widget(widgetdata *widget, int x, int y)
{
	(void) x;
	(void) y;
	create_widget_object(widget->WidgetSubtypeID);
}

void menu_remove_widget(widgetdata *widget, int x, int y)
{
	(void) x;
	(void) y;
	remove_widget_object(widget);
}

void menu_detach_widget(widgetdata *widget, int x, int y)
{
	(void) x;
	(void) y;
	detach_widget(widget);
}

void menu_set_say_filter(widgetdata *widget, int x, int y)
{
	(void) widget;
	(void) x;
	(void) y;
}

void menu_set_shout_filter(widgetdata *widget, int x, int y)
{
	(void) widget;
	(void) x;
	(void) y;
}

void menu_set_gsay_filter(widgetdata *widget, int x, int y)
{
	(void) widget;
	(void) x;
	(void) y;
}

void menu_set_tell_filter(widgetdata *widget, int x, int y)
{
	(void) widget;
	(void) x;
	(void) y;
}

void menu_set_channel_filter(widgetdata *widget, int x, int y)
{
	(void) widget;
	(void) x;
	(void) y;
}

void submenu_chatwindow_filters(widgetdata *widget, int x, int y)
{
	(void) x;
	(void) y;
	add_menuitem(widget, "Say", &menu_set_say_filter, MENU_CHECKBOX, 0);
	add_menuitem(widget, "Shout", &menu_set_shout_filter, MENU_CHECKBOX, 0);
	add_menuitem(widget, "Group", &menu_set_gsay_filter, MENU_CHECKBOX, 0);
	add_menuitem(widget, "Tells", &menu_set_tell_filter, MENU_CHECKBOX, 0);
	add_menuitem(widget, "Channels", &menu_set_channel_filter, MENU_CHECKBOX, 0);
}

void menu_inv_filter_all(widgetdata *widget, int x, int y)
{
	(void) widget;
	(void) x;
	(void) y;
	inventory_filter_set(INVENTORY_FILTER_ALL);
}

void menu_inv_filter_applied(widgetdata *widget, int x, int y)
{
	(void) widget;
	(void) x;
	(void) y;
	inventory_filter_toggle(INVENTORY_FILTER_APPLIED);
}

void menu_inv_filter_containers(widgetdata *widget, int x, int y)
{
	(void) widget;
	(void) x;
	(void) y;
	inventory_filter_toggle(INVENTORY_FILTER_CONTAINER);
}

void menu_inv_filter_magical(widgetdata *widget, int x, int y)
{
	(void) widget;
	(void) x;
	(void) y;
	inventory_filter_toggle(INVENTORY_FILTER_MAGICAL);
}

void menu_inv_filter_cursed(widgetdata *widget, int x, int y)
{
	(void) widget;
	(void) x;
	(void) y;
	inventory_filter_toggle(INVENTORY_FILTER_CURSED);
}

void menu_inv_filter_unidentified(widgetdata *widget, int x, int y)
{
	(void) widget;
	(void) x;
	(void) y;
	inventory_filter_toggle(INVENTORY_FILTER_UNIDENTIFIED);
}

void menu_inv_filter_locked(widgetdata *widget, int x, int y)
{
	(void) widget;
	(void) x;
	(void) y;
	inventory_filter_toggle(INVENTORY_FILTER_LOCKED);
}

void menu_inv_filter_unapplied(widgetdata *widget, int x, int y)
{
	(void) widget;
	(void) x;
	(void) y;
	inventory_filter_toggle(INVENTORY_FILTER_UNAPPLIED);
}

void menu_inv_filter_submenu(widgetdata *widget, int x, int y)
{
	(void) widget;
	(void) x;
	(void) y;
}

void menu_inventory_submenu_more(widgetdata *widget, int x, int y)
{
	(void) widget;
	(void) x;
	(void) y;
}

void menu_inventory_submenu_quickslots(widgetdata *widget, int x, int y)
{
	(void) widget;
	(void) x;
	(void) y;
}
