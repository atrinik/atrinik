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

/**
 * @file
 * This file controls all the widget related functions,
 * movement of the widgets, initialization, etc. */

/* TO add a new widget:
 *  1) Add an entry (same index in both cases) to "_widgetdata con_widget[]" (widget.c), and "enum _WidgetID" (widget.h)
 *  2) If applicable, add handler code for widget movement in widget_event_mousedn()
 *  3) If applicable, add handler code to get_widget_owner()
 *  4) Add handler/function to 'process_widget(int nID)' */

#include <include.h>

/* File scope routines */
static void  process_widget(int nID);
static int load_interface_file(char *filename);
static void init_priority_list();
static void kill_priority_list();

/** Current (working) data list of all widgets */
_widgetdata cur_widget[TOTAL_WIDGETS];

/** Current (default) data list of all widgets */
_widgetdata def_widget[TOTAL_WIDGETS];

/** Default data list of all widgets */
static const _widgetdata con_widget[TOTAL_WIDGETS] =
{
	{"STATS",		NULL,	227,	0,		172,	102,	TRUE, 	TRUE, 	TRUE},
	{"RESIST",		NULL,	497,	0,		198,	79,		TRUE, 	TRUE, 	TRUE},
	{"MAIN_LVL",	NULL,	399,	39,		98,		62,		TRUE, 	TRUE, 	TRUE},
	{"SKILL_EXP",	NULL,	497,	79,		198,	22,		TRUE, 	TRUE, 	TRUE},
	{"REGEN",		NULL,	399,	0,		98,		39,		TRUE, 	TRUE, 	TRUE},
	{"SKILL_LVL",	NULL,	695,	0,		52,		101,	TRUE, 	TRUE, 	TRUE},
	{"MENUBUTTONS",	NULL,	747,	0,		47,		101,	TRUE, 	TRUE, 	TRUE},
	{"QUICKSLOTS",	NULL,	513,	107,	282,	34,		TRUE, 	TRUE, 	TRUE},
	{"CHATWIN",		NULL,	0,		366,	261,	233,	TRUE, 	TRUE, 	TRUE},
	{"MSGWIN",		NULL,	537,	366,	261,	233,	TRUE, 	TRUE, 	TRUE},
	{"MIXWIN",		NULL,	539,	420,	261,	233,	TRUE, 	FALSE, 	TRUE},
	{"PLAYERDOLL",	NULL,	0,		41,		221,	224,	TRUE, 	TRUE, 	TRUE},
	{"BELOWINV",	NULL,	262,	545,	274,	55,		TRUE, 	TRUE, 	TRUE},
	{"PLAYERINFO",	NULL,	0,		0,		219,	41,		TRUE, 	TRUE, 	TRUE},
	{"RANGEBOX",	NULL,	6,		100,	94,		60,		TRUE, 	TRUE, 	TRUE},
	{"TARGET",		NULL,	267,	514,	264,	31,		TRUE, 	TRUE, 	TRUE},
	{"MAININV",		NULL,	539,	147,	239,	32,		TRUE, 	TRUE, 	TRUE},
	{"MAPNAME",		NULL,	228,	106,	36,		12,		TRUE, 	TRUE, 	TRUE},
	{"CONSOLE",		NULL,	271,	489,	256,	25,		TRUE, 	FALSE, 	TRUE},
	{"NUMBER",		NULL,	271,	465,	256,	43,		TRUE, 	FALSE, 	TRUE},
};

/** Default overall priority list.. Will change during runtime.
 * Widget at the head has highest priority.
 * Events go to the head first
 * Displaying goes to the foot first */
static widget_node *priority_list_head, *priority_list_foot;

/** Determines which widget has mouse focus
 * This value is determined in the mouse routines for the widgets */
_widgetevent widget_mouse_event =
{
	0,
	0,
	0
};

/** This is used when moving a widget with the mouse. */
static _widgetmove widget_event_move =
{
	0,
	0,
	0,
	0
};

/** SDL surface for the widgets */
SDL_Surface *widgetSF[TOTAL_WIDGETS] = {NULL};

/** A way to steal the mouse, and to prevent widgets from using mouse events
 * Example: Prevents widgets from using mouse events during dragging procedure */
int IsMouseExclusive = 0;

/**
 * Load the defaults and initialize the priority list.
 * Create the interface file, if it doesn't exist */
void init_widgets_fromDefault()
{
	int lp;

#ifdef DEBUG_WIDGET
	LOG(LOG_MSG, "Entering init_widgets_fromDefault()\n");
#endif

	/* In all cases should reset */
	kill_widgets();

	/* Exit, if there are no widget ID's */
	if (!TOTAL_WIDGETS)
		return;

	/* Store the constant default widget lookup in the current lookup(s) */
	for (lp = 0; lp < TOTAL_WIDGETS; ++lp)
		cur_widget[lp] = def_widget[lp] = con_widget[lp];

	/* Allocate the priority list now */
	init_priority_list();

#ifdef DEBUG_WIDGET
	LOG(LOG_MSG, "init_widgets_fromDefault(): Done.\n");
#endif
}

/**
 * Try to load the main interface file and initialize the priority list
 * On failure, initialize the widgets with init_widgets_fromDefault() */
void init_widgets_fromCurrent()
{
#ifdef DEBUG_WIDGET
	LOG(LOG_MSG, "Entering init_widgets_fromCurrent()..\n");
#endif

	/* An all cases should reset */
	kill_widgets();

	/* Exit, if there are no widgets */
	if (!TOTAL_WIDGETS)
		return;

	/* If can't open/load the interface file load defaults and create file */
	if (!load_interface_file(INTERFACE_FILE))
	{
		/* Inform user */
		LOG(LOG_MSG, "Can't open/load the interface file - %s. Resetting\n", INTERFACE_FILE);

		/* Load the defaults - this also allocates priority list */
		init_widgets_fromDefault();

		/* Create the interface file */
		save_interface_file();
	}
	/* Was able to load the interface file */
	else
	{
		/* Clear the priority list if it already exists */
		if (priority_list_head)
			kill_priority_list();

		/* Allocate the priority list now */
		init_priority_list();
	}

#ifdef DEBUG_WIDGET
	LOG(LOG_MSG, "init_widgets_fromCurrent(): Done.\n");
#endif
}

/**
 * Initializes the widget priority list.
 * Used in two places at the moment, so it's in a static scope function */
void init_priority_list()
{
	widget_node *node;
	int lp;

	/* If it's already allocated, leave */
	if (priority_list_head)
		return;

#ifdef DEBUG_WIDGET
	LOG(LOG_MSG, "Entering init_priority_list()\n");
#endif

	/* Allocate the head of the list */
	priority_list_head = node = _malloc(sizeof(widget_node), "init_priority_list: widget_node");

	if (!node)
	{
		LOG(LOG_ERROR, "ERROR: Out of memory.\n");
		exit(0);
	}

	/* Set the members and store a link to this pointer */
	priority_list_head->next = NULL;
	priority_list_head->prev = NULL;
	priority_list_head->WidgetID = 0;
	cur_widget[0].priority_index = priority_list_head;

	for (lp = 1; lp < TOTAL_WIDGETS; ++lp)
	{
		/* Allocate it */
		node->next = _malloc(sizeof(widget_node), "init_priority_list: widget_node");

		if (!node->next)
		{
			LOG(LOG_ERROR, "ERROR: Out of memory.\n");
			exit(0);
		}

		node->next->prev = node;
		/* Set the members and store a link to this pointer */
		node = node->next;
		node->next = NULL;
		node->WidgetID = lp;
		cur_widget[lp].priority_index = node;
	}

	/* Set the foot of the priority list */
	priority_list_foot = node;

#ifdef DEBUG_WIDGET
	LOG(LOG_MSG, "Output of node list:\n");

	for (lp = 0, node = priority_list_head; node; node = node->next, ++lp)
	{
		LOG(LOG_MSG, "Node #%d: %d\n", lp,node->WidgetID);
	}

	LOG(LOG_MSG, "Allocated %d/%d nodes!\n", lp, TOTAL_WIDGETS);

	LOG(LOG_MSG, "init_priority_list(): Done.\n");
#endif
}

/**
 * Kill widget priority list. */
void kill_priority_list()
{
	widget_node *tmp_node;
	int lp;

	/* Leave if it's clear already */
	if (!priority_list_head)
		return;

#ifdef DEBUG_WIDGET
	LOG(LOG_MSG, "Entering kill_priority_list()\n");

	LOG(LOG_MSG, "Output of deleted node(s):\n");
#endif

	/* Walk down the list and free it */
	for (lp = 0; priority_list_head; ++lp)
	{
#ifdef DEBUG_WIDGET
		LOG(LOG_MSG, "Node #%d: %d\n", lp, priority_list_head->WidgetID);
#endif

		tmp_node = priority_list_head->next;
		free(priority_list_head);
		priority_list_head = tmp_node;
	}

#ifdef DEBUG_WIDGET
	LOG(LOG_MSG, "De-Allocated %d/%d nodes!\n", lp, TOTAL_WIDGETS);
#endif

	priority_list_head = NULL;
	priority_list_foot = NULL;

#ifdef DEBUG_WIDGET
	LOG(LOG_MSG, "kill_priority_list(): Done.\n");
#endif
}

/**
 * Perform deinitialization of the widgets */
void kill_widgets()
{
	int pos;

	for (pos = 0; pos < TOTAL_WIDGETS; ++pos)
	{
		if (widgetSF[pos])
		{
			SDL_FreeSurface(widgetSF[pos]);
			widgetSF[pos] = NULL;
		}
	}

	kill_priority_list();
}

/**
 * Load the widgets interface from a file.
 * Do not perform any dynamic allocation.
 * @param filename The interface filename
 * @return 1 on success, 0 on failure */
int load_interface_file(char *filename)
{
	int i = -1, pos;
	FILE *stream;
	_widgetdata tmp_widget[TOTAL_WIDGETS];
	char line[256], keyword[256], parameter[256];
	int found_widget[TOTAL_WIDGETS] = {0};

#ifdef DEBUG_WIDGET
	LOG(LOG_MSG, "Entering load_interface_file()\n");
#endif

	/* Transfer the constant lookup to a temp lookup.
	 * We'll use it here to load the file */
	for (pos = 0; pos < TOTAL_WIDGETS; ++pos)
		tmp_widget[pos] = con_widget[pos];

	/* Sanity check - if the file doesn't exist, exit with error */
	if (!(stream = fopen(filename, "r")))
	{
		/* Inform user */
		LOG(LOG_MSG, "load_interface_file(): Can't find file %s.\n", filename);

		/* Done */
		return 0;
	}

	/* Read the settings from the file */
	while (fgets(line, 255, stream))
	{
		if (line[0] == '#' || line[0] == '\n')
			continue;

		i = 0;

		while (line[i] && line[i] !=  ':')
			i++;

		line[++i] = 0;

		strncpy(keyword, line, sizeof(keyword));
		strncpy(parameter, line + i + 1, sizeof(parameter));

		/* Remove the newline character */
		parameter[strcspn(line + i + 1, "\n")] = 0;

		/* If we find a "Widget " (beginning of block) read until "end" (end of block) */

		/* Beginning */
		if (strncmp(keyword, "Widget:", 7) == 0)
		{
#ifdef DEBUG_WIDGET
			LOG(LOG_MSG, "Trying to find \"Widget: %s\"", parameter);
#endif

			pos = 0;

			/* Find the index of the widget for reference */
			while (pos < TOTAL_WIDGETS && (strcmp(tmp_widget[pos].name, parameter) != 0))
				++pos;

			/* The widget name couldn't be found? */
			if (pos >= TOTAL_WIDGETS)
			{
#ifdef DEBUG_WIDGET
				LOG(LOG_MSG, "Widget not found!\n");
#endif

				continue;
			}
			/* Get the block */
			else
			{
				/* If we haven't found this widget, mark it */
				if (!found_widget[pos])
				{
#ifdef DEBUG_WIDGET
					LOG(LOG_MSG, "Found! (Index = %d) (%d widgets total)\n", pos, TOTAL_WIDGETS);
#endif
					found_widget[pos] = 1;
				}
				/* If we have found it, skip this block .. */
				else
				{
#ifdef DEBUG_WIDGET
					LOG(LOG_MSG, "Widget already found! Please remove duplicate(s)!\n");
#endif
					continue;
				}

				while (fgets(line, 255, stream))
				{
					if (line[0] == '#' || line[0] == '\n')
						continue;

					/* End marker */
					if (strncmp(line, "end", 3) == 0)
						break;

					i = 0;

					while (line[i] && line[i] !=  ':')
						i++;

					line[++i] = 0;
					strcpy(keyword, line);
					strcpy(parameter, line + i + 1);

					if (strncmp(keyword, "x:", 2) == 0)
					{
						tmp_widget[pos].x1 = atoi(parameter);
#ifdef DEBUG_WIDGET
						LOG(LOG_MSG, "Loading: (%s)\n", keyword);
#endif
					}
					else if (strncmp(keyword, "y:", 2) == 0)
					{
						tmp_widget[pos].y1 = atoi(parameter);
#ifdef DEBUG_WIDGET
						LOG(LOG_MSG, "Loading: (%s)\n", keyword);
#endif
					}
					else if (strncmp(keyword, "moveable:", 9) == 0)
					{
						tmp_widget[pos].moveable = atoi(parameter);
#ifdef DEBUG_WIDGET
						LOG(LOG_MSG, "Loading: (%s)\n", keyword);
#endif
					}
					else if (strncmp(keyword, "active:", 7) == 0)
					{
						tmp_widget[pos].show = atoi(parameter);
#ifdef DEBUG_WIDGET
						LOG(LOG_MSG, "Loading: (%s)\n", keyword);
#endif
					}
					else if (strncmp(keyword, "width:", 6) == 0)
					{
						tmp_widget[pos].wd = atoi(parameter);
#ifdef DEBUG_WIDGET
						LOG(LOG_MSG, "Loading: (%s)\n", keyword);
#endif
					}
					else if (strncmp(keyword, "height:", 7) == 0)
					{
						tmp_widget[pos].ht = atoi(parameter);
#ifdef DEBUG_WIDGET
						LOG(LOG_MSG, "Loading: (%s)\n", keyword);
#endif
					}
				}
			}
		}
	}

	fclose(stream);

	/* Test to see if all widgets were found */
	for (pos = 0; pos < TOTAL_WIDGETS && found_widget[pos]; ++pos);

	/* If all were loaded/found, transfer */
	if (pos >= TOTAL_WIDGETS)
	{
		for (pos = 0; pos < TOTAL_WIDGETS; ++pos)
			cur_widget[pos] = def_widget[pos] = tmp_widget[pos];
	}
	/* Some are missing, don't transfer */
	else
	{
		LOG(LOG_MSG, "load_interface_file(): Error! Not all widgets included in interface file: %s (%d, %d)\n", filename, pos, TOTAL_WIDGETS);

		return 0;
	}

#ifdef DEBUG_WIDGET
	LOG(LOG_MSG, "load_interface_file(): Done.\n");
#endif

	return 1;
}

/**
 * Save the widgets interface to a file */
void save_interface_file()
{
	char txtBuffer[20];
	int i = -1;
	FILE *stream;

	/* Leave, if there's an error opening or creating */
	if (!(stream = fopen(INTERFACE_FILE, "w")))
		return;

	fputs("#############################################\n", stream);
	fputs("# This is the Atrinik client interface file #\n", stream);
	fputs("#############################################\n", stream);

	while (++i < TOTAL_WIDGETS)
	{
		/* Beginning of block */
		fputs("\nWidget: ", stream);
		fputs(cur_widget[i].name, stream);
		fputs("\n", stream);

		fputs("moveable:", stream);
		snprintf(txtBuffer, sizeof(txtBuffer), " %d", cur_widget[i].moveable);
		fputs(txtBuffer, stream);
		fputs("\n", stream);

		fputs("active:", stream);
		snprintf(txtBuffer, sizeof(txtBuffer), " %d", cur_widget[i].show);
		fputs(txtBuffer, stream);
		fputs("\n", stream);

		fputs("x:", stream);
		snprintf(txtBuffer, sizeof(txtBuffer), " %d", cur_widget[i].x1);
		fputs(txtBuffer, stream);
		fputs("\n", stream);

		fputs("y:", stream);
		snprintf(txtBuffer, sizeof(txtBuffer), " %d", cur_widget[i].y1);
		fputs(txtBuffer, stream);
		fputs("\n", stream);

		fputs("width:", stream);
		snprintf(txtBuffer, sizeof(txtBuffer), " %d", cur_widget[i].wd);
		fputs(txtBuffer, stream);
		fputs("\n", stream);

		fputs("height:", stream);
		snprintf(txtBuffer, sizeof(txtBuffer), " %d", cur_widget[i].ht);
		fputs(txtBuffer, stream);
		fputs("\n", stream);

		/* End of block */
		fputs("end\n", stream);
	}

	fclose(stream);
}

/**
 * Mouse is down. Check for owner of the mouse focus.
 * Setup widget dragging, if enabled
 * @param x Mouse X position
 * @param y Mouse Y position
 * @param event SDL event type
 * @return 1 if this is a widget and we're handling the mouse, 0 otherwise
 * @todo Right click, select 'move' to move a widget */
int widget_event_mousedn(int x, int y, SDL_Event *event)
{
	int nID = get_widget_owner(x, y);

	/* Setup the event structure in response */
	widget_mouse_event.owner = nID;

	/* Sanity check */
	if (nID < 0)
		return 0;

	/* Setup the event structure in response */
	widget_mouse_event.x = x;
	widget_mouse_event.y = y;

	/* Set the priority to this widget */
	SetPriorityWidget(nID);

	/* If it's moveable, start moving it when the conditions warrant it */
	if (cur_widget[nID].moveable && MouseEvent == RB_DN)
	{
		/* If widget is moveable, this defines the hotspot areas for activating */
		switch (nID)
		{
			default:
				/* We know this widget owns the mouse */
				widget_event_move.active = 1;
				break;
		}

		/* Start the movement procedures */
		if (widget_event_move.active)
		{
			widget_event_move.id = nID;
			widget_event_move.xOffset = x - cur_widget[nID].x1;
			widget_event_move.yOffset = y - cur_widget[nID].y1;

			/* Nothing owns the mouse right now */
			widget_mouse_event.owner = -1;

			/* Enable the custom cursor */
			f_custom_cursor = MSCURSOR_MOVE;

			/* Hide the system cursor */
			SDL_ShowCursor(0);
		}

		return 1;
	}
	/* Normal condition - respond to mouse down event */
	else
	{
		/* Place here all the mousedown Handlers */
		switch (nID)
		{
			/* Handle the mouse down on the exp area */
			case SKILL_EXP_ID:
				widget_skill_exp_event();
				break;

			/* Handle mousedown on the menu buttons */
			case MENU_B_ID:
				widget_menubuttons_event(x, y);
				break;

			/* Handle mouse down on quickslots */
			case QUICKSLOT_ID:
				widget_quickslots_mouse_event(x, y, MOUSE_DOWN);
				break;

			/* Handle the mouse down on chat windows */
			case CHATWIN_ID:
			case MSGWIN_ID:
			case MIXWIN_ID:
				textwin_event(TW_CHECK_BUT_DOWN, event, nID);
				break;

			/* Handle the mouse down on range area */
			case RANGE_ID:
				widget_range_event(x, y, *event, MOUSE_DOWN);
				break;

			/* Handle the mouse down in below window */
			case BELOW_INV_ID:
				widget_below_window_event(x, y, MOUSE_DOWN);
				break;

			/* Handle the mouse down in target widget */
			case TARGET_ID:
				widget_event_target(x, y);
				break;

			/* Handle the mouse down in inventory */
			case MAIN_INV_ID:
				widget_inventory_event(x, y, *event);
				break;

			/* Handle the mouse down in player info */
			case PLAYER_INFO_ID:
				widget_player_data_event(x, y);
				break;

			/* Handle the mouse down in input number */
			case IN_NUMBER_ID:
				widget_number_event(x, y);
				break;
		}

		return 1;
	}
}


/* Mouse is up.
 * Check for owner of mouse focus
 * Stop dragging the widget, if active */
/**
 * Mouse is up. Check for owner of mouse focus.
 * Stop dragging the widget, if active.
 * @param x Mouse X position
 * @param y Mouse Y position
 * @param event SDL event type
 * @return 1 if this is a widget and we're handling the mouse, 0 otherwise */
int widget_event_mouseup(int x, int y, SDL_Event *event)
{
	/* Widget moving condition */
	if (widget_event_move.active)
	{
		widget_event_move.active = 0;
		widget_mouse_event.owner = widget_event_move.id;
		widget_mouse_event.x = x;
		widget_mouse_event.y = y;

		/* Disable the custom cursor */
		f_custom_cursor = 0;

		/* Show the system cursor */
		SDL_ShowCursor(1);

		/* The interface has changed, save it!
		 * TODO: Implement this */
#if 0
		if (options.auto_save_interface)
			save_interface_file();
#endif

		return 1;
	}
	/* Normal condition - respond to mouse up event */
	else
	{
		int nID = get_widget_owner(x, y);

		/* Setup the event structure in response */
		widget_mouse_event.owner = nID;

		/* Handler for miscellanous mouse movement(s) go here */

		/* Sanity check... Return if mouse is not in a widget */
		if (nID < 0)
			return 0;
		else
		{
			/* Setup the event structure in response */
			widget_mouse_event.x = x;
			widget_mouse_event.y = y;
		}

		/* Handler for the widgets go here */
		switch (nID)
		{
			/* Drop to quickslots */
			case QUICKSLOT_ID:
				widget_quickslots_mouse_event(x, y, MOUSE_UP);
				break;

			/* Chat windows */
			case CHATWIN_ID:
			case MSGWIN_ID:
			case MIXWIN_ID:
				textwin_event(TW_CHECK_BUT_UP, event, nID);
				break;

			/* Player doll */
			case PDOLL_ID:
				widget_show_player_doll_event();
				break;

			/* Range widget */
			case RANGE_ID:
				widget_range_event(x, y, *event, MOUSE_UP);
				break;

			/* Inventory window */
			case MAIN_INV_ID:
				widget_inventory_event(x, y, *event);
				break;
		}

		return 1;
	}
}

/**
 * Mouse was moved. Check for owner of mouse focus.
 * Drag the widget, if active.
 * @param x Mouse X position
 * @param y Mouse Y position
 * @param event SDL event type
 * @return 1 if this is a widget and we're handling the mouse, 0 otherwise */
int widget_event_mousemv(int x, int y, SDL_Event *event)
{
	/* With widgets we have to clear every loop the txtwin cursor */
	cursor_type = 0;

	/* Widget moving condition */
	if (widget_event_move.active)
	{
#if 0
		int adjx = x - widget_event_move.xOffset, adjy = y - widget_event_move.yOffset;
#endif

#ifdef WIDGET_SNAP
		if (options.widget_snap > 0)
		{
			if (event->motion.xrel != 0 && event->motion.yrel != 0)
			{
				int mID  = widget_event_move.id;
				widget_node *node;

				for (node = priority_list_head; node; node = node->next)
				{
					int nID = node->WidgetID;
					int done = 0;

					if (nID == mID || !cur_widget[nID].show)
						continue;

					if ((TOP(mID) >= TOP(nID) && TOP(mID) <= BOTTOM (nID)) || (BOTTOM(mID) >= TOP(nID) && BOTTOM(mID) <= BOTTOM(nID)))
					{
						if (event->motion.xrel < 0 && LEFT(mID) <= RIGHT(nID) + options.widget_snap && LEFT(mID) > RIGHT(nID))
						{
#if 0
							adjx = RIGHT(nID);
#endif
							event->motion.x = RIGHT(nID) + widget_event_move.xOffset;
							done = 1;
						}
						else if (event->motion.xrel > 0 && RIGHT(mID) >= LEFT(nID) - options.widget_snap && RIGHT(mID) < LEFT(nID))
						{
#if 0
							adjx = LEFT(nID) - cur_widget[mID].wd;
#endif
							event->motion.x = LEFT(nID) - cur_widget[mID].wd + widget_event_move.xOffset;
							done = 1;
						}
					}

					if ((LEFT(mID) >= LEFT(nID) && LEFT(mID) <= RIGHT(nID)) || (RIGHT(mID) >= LEFT(nID) && RIGHT(mID) <= RIGHT(nID)))
					{
						if (event->motion.yrel < 0 && TOP(mID) <= BOTTOM(nID) + options.widget_snap && TOP(mID) > BOTTOM(nID))
						{
#if 0
							adjy = BOTTOM(nID);
#endif
							event->motion.y = BOTTOM(nID) + widget_event_move.yOffset;
							done = 1;
						}
						else if (event->motion.yrel > 0 && BOTTOM(mID) >= TOP(nID) - options.widget_snap && BOTTOM(mID) < TOP(nID))
						{
#if 0
							adjy = TOP(nID) - cur_widget[mID].ht;
#endif
							event->motion.y = TOP(nID) - cur_widget[mID].ht + widget_event_move.yOffset;
							done = 1;
						}
					}

					if (done)
					{
#if 0
						draw_info_format(COLOR_RED, "%s l=%d r=%d t=%d b=%d", cur_widget[nID].name, LEFT(nID), RIGHT(nID), TOP(nID), BOTTOM(nID));
#endif
						sound_play_effect(SOUND_SCROLL, 0, 10);

						/* Acts as a brake, preventing mID from 'skipping' through a stack of nodes */
						event->motion.xrel = event->motion.yrel = 0;
						SDL_PushEvent(event);
						break;
					}
				}
			}
		}
#endif

#if 0
		cur_widget[widget_event_move.id].x1 = adjx;
		cur_widget[widget_event_move.id].y1 = adjy;
#endif

		cur_widget[widget_event_move.id].x1 = x - widget_event_move.xOffset;
		cur_widget[widget_event_move.id].y1 = y - widget_event_move.yOffset;
		map_udate_flag = 2;
		return 1;
	}
	/* Normal condition - respond to mouse move event */
	else
	{
		int nID = get_widget_owner(x, y);

		/* Setup the event structure in response */
		widget_mouse_event.owner = nID;

		/* Handlers for miscellanous mouse movements go here */

		/* Text window special handling */
		if (txtwin[TW_CHAT].highlight != TW_HL_NONE)
		{
			txtwin[TW_CHAT].highlight = TW_HL_NONE;
			WIDGET_REDRAW(CHATWIN_ID);
		}

		if (txtwin[TW_MSG].highlight != TW_HL_NONE)
		{
			txtwin[TW_MSG].highlight = TW_HL_NONE;
			WIDGET_REDRAW(MSGWIN_ID);
		}

		/* Sanity check.. Return if mouse is not in a widget */
		if (nID < 0)
			return 0;
		else
		{
			/* Setup the event structure in response */
			widget_mouse_event.x = x;
			widget_mouse_event.y = y;
		}

		/* Handlers for the widgets mouse move */
		switch (nID)
		{
			/* Text windows */
			case CHATWIN_ID:
			case MSGWIN_ID:
			case MIXWIN_ID:
				textwin_event(TW_CHECK_MOVE, event, nID);
				break;

			/* Inventory */
			case MAIN_INV_ID:
				widget_inventory_event(x, y, *event);
				break;
		}

		return 1;
	}
}

/**
 * Find the widget with mouse focus on a mouse-hit-test basis.
 * @param x Mouse X position
 * @param y Mouse Y position
 * @return -1 if no widget, otherwise the widget's ID */
int get_widget_owner(int x, int y)
{
	widget_node *node;
	int nID;

	/* Priority overide function, we have to have that here for resizing */
	if (textwin_flags & TW_RESIZE)
	{
		if (textwin_flags & TW_CHAT)
			return CHATWIN_ID;
		else if (textwin_flags & TW_MSG)
			return MSGWIN_ID;
		else if (textwin_flags & TW_MIX)
			return MIXWIN_ID;
	}

	/* Mouse cannot be used by widgets */
	if (IsMouseExclusive)
		return -1;

	/* Priority list doesn't exist */
	if (!priority_list_head)
		return -1;

	/* Loop through the list and perform custom or default hit-test */
	for (node = priority_list_head; node; node = node->next)
	{
		nID = node->WidgetID;

		if (!cur_widget[nID].show)
			continue;

		switch (nID)
		{
			/* Playerdoll widget is NOT a rectangle, handle special */
			case PDOLL_ID:
				if (x > cur_widget[nID].x1 + 111)
				{
					if (x <= cur_widget[nID].x1 + cur_widget[nID].wd && y >= cur_widget[nID].y1 && y <= ((x - (cur_widget[nID].x1 + 111)) / -2) + 215 + cur_widget[nID].y1)
						return nID;
				}
				else
				{
					if (x >= cur_widget[nID].x1 && y >= cur_widget[nID].y1 && y <= ((x - cur_widget[nID].x1) / 2) + 160 + cur_widget[nID].y1)
						return nID;
				}

				break;

			default:
				if (x >= cur_widget[nID].x1 && x <= (cur_widget[nID].x1 + cur_widget[nID].wd) && y >= cur_widget[nID].y1 && y <= (cur_widget[nID].y1 + cur_widget[nID].ht))
					return nID;

				break;
		}
	}

	return -1;
}

/**
 * Function list for each widget. Calls the widget with the process type.
 * @param nID The widget ID */
void process_widget(int nID)
{
	/* Doesn't matter which order the case statements follow */
	switch (nID)
	{
		case STATS_ID:
			widget_player_stats(cur_widget[nID].x1, cur_widget[nID].y1);
			break;

		case RESIST_ID:
			widget_show_resist(cur_widget[nID].x1, cur_widget[nID].y1);
			break;

		case MAIN_LVL_ID:
			widget_show_main_lvl(cur_widget[nID].x1, cur_widget[nID].y1);
			break;

		case SKILL_EXP_ID:
			widget_show_skill_exp(cur_widget[nID].x1, cur_widget[nID].y1);
			break;

		case REGEN_ID:
			widget_show_regeneration(cur_widget[nID].x1, cur_widget[nID].y1);
			break;

		case SKILL_LVL_ID:
			widget_skillgroups(cur_widget[nID].x1, cur_widget[nID].y1);
			break;

		case MENU_B_ID:
			widget_menubuttons(cur_widget[nID].x1, cur_widget[nID].y1);
			break;

		case QUICKSLOT_ID:
			widget_quickslots(cur_widget[nID].x1, cur_widget[nID].y1);
			break;

		case CHATWIN_ID:
			widget_textwin_show(cur_widget[nID].x1, cur_widget[nID].y1, TW_CHAT);
			break;

		case MSGWIN_ID:
			widget_textwin_show(cur_widget[nID].x1, cur_widget[nID].y1, TW_MSG);
			break;

		case MIXWIN_ID:
			widget_textwin_show(cur_widget[nID].x1, cur_widget[nID].y1, TW_MIX);
			break;

		case PDOLL_ID:
			widget_show_player_doll(cur_widget[nID].x1, cur_widget[nID].y1);
			break;

		case BELOW_INV_ID:
			widget_show_below_window(cpl.below, cur_widget[nID].x1, cur_widget[nID].y1);
			break;

		case PLAYER_INFO_ID:
			widget_show_player_data(cur_widget[nID].x1, cur_widget[nID].y1);
			break;

		case RANGE_ID:
			widget_show_range(cur_widget[nID].x1, cur_widget[nID].y1);
			break;

		case TARGET_ID:
			widget_show_target(cur_widget[nID].x1, cur_widget[nID].y1);
			break;

		case MAIN_INV_ID:
			widget_show_inventory_window(cur_widget[nID].x1, cur_widget[nID].y1);
			break;

		case MAPNAME_ID:
			widget_show_mapname(cur_widget[nID].x1, cur_widget[nID].y1);
			break;

		case IN_CONSOLE_ID:
			widget_show_console(cur_widget[nID].x1, cur_widget[nID].y1);
			break;

		case IN_NUMBER_ID:
			widget_show_number(cur_widget[nID].x1, cur_widget[nID].y1);
			break;
	}
}

/**
 * Loop through all the widgets and call the corresponding handlers.
 * This is called everytime in main.c, in the main loop. */
void process_widgets()
{
	widget_node *node;
	int nID;

	/* Sanity checks */
	if (!priority_list_head || !priority_list_foot)
		return;

	for (node = priority_list_foot; node; node = node->prev)
	{
		nID = node->WidgetID;

		if (cur_widget[nID].show)
			process_widget(nID);
	}
}

/**
 * This is used by the widgets when they use the mouse.
 * @param mx Mouse X position
 * @param my Mouse Y position
 * @param widget_id Widget ID
 * @return 0 to not use the mouse, otherwise values such as IDLE, LB_DN, LB_UP, RB_DN, RB_UP, MB_UP, MB_DN */
uint32 GetMouseState(int *mx, int *my, int widget_id)
{
	if (widget_mouse_event.owner == widget_id && !IsMouseExclusive)
	{
		/* Continue only when no menu is active. */
		if (cpl.menustatus != MENU_NO || esc_menu_flag)
			return 0;

		*mx = widget_mouse_event.x;
		*my = widget_mouse_event.y;
		return MouseState;
	}

	/* Don't use the mouse in the calling widget */
	return 0;
}

/**
 * Sets this widget to have the highest priority.
 * 1) Transfer head to a new node below head
 * 2) Transfer this widget to the head
 * 3) Remove this widget from its previous priority
 * @param nWidgetID The widget ID */
void SetPriorityWidget(int nWidgetID)
{
	widget_node *node;

#ifdef DEBUG_WIDGET
	LOG(LOG_MSG, "Entering SetPriorityWidget(nWidgetID=%d)\n", nWidgetID);
#endif

	/* Sanity check */
	if (nWidgetID < 0 || nWidgetID >= TOTAL_WIDGETS)
		return;

	/* Exit, if already highest priority */
	if (priority_list_head->WidgetID == nWidgetID)
		return;

	/* Move the current highest to second highest priority */
	node = (widget_node *) _malloc (sizeof(widget_node), "SetPriorityWidget: widget_node");

	if (!node)
	{
		LOG(LOG_ERROR, "ERROR: Out of memory.");
		exit(0);
	}

	*node = *priority_list_head;
	node->prev = priority_list_head;
	node->next->prev = node;
	priority_list_head->next = node;
	cur_widget[node->WidgetID].priority_index = node;

	/* Make this widget have highest priority */
	priority_list_head->WidgetID = nWidgetID;

	/* Remove it from its previous priority */
	node = cur_widget[nWidgetID].priority_index;

#ifdef DEBUG_WIDGET
	LOG(LOG_MSG, "node: %d\n", node);
	LOG(LOG_MSG, "cur_widget[nWidgetID].priority_index: %d\n", cur_widget[nWidgetID].priority_index);
	LOG(LOG_MSG, "node->prev: %d, node->next: %d\n", node->prev, node->next);
#endif

	if (node->next)
	{
		node->next->prev = node->prev;
		node->prev->next = node->next;
	}
	/* Foot of list */
	else
	{
		/* Update the foot of priority list */
		priority_list_foot = node->prev;
		node->prev->next = NULL;
	}

	free(node);

	/* Re-link the widget lookup */
	cur_widget[nWidgetID].priority_index = priority_list_head;

#ifdef DEBUG_WIDGET
	LOG(LOG_MSG, "SetPriorityWidget(): Done.\n");
#endif
}
