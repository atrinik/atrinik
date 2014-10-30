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
 * -# If applicable, add extended attributes in its own struct, and add handler
 * code for its initialization in create_widget_object().
 * -# If applicable, add handler code for widget movement in
 * widget_event_mousedn().
 * -# If applicable, add handler code to get_widget_owner().
 * -# Add handler function to process_widget().
 *
 * @author Alex Tokar
 * @author Daniel Liptrot
 * @author Dantee */

#include <global.h>

static widgetdata def_widget[TOTAL_SUBWIDGETS];
static const char *const widget_names[TOTAL_SUBWIDGETS] =
{
    "map", "stat", "menu_buttons", "quickslots", "textwin", "playerdoll",
    "belowinv", "playerinfo", "maininv", "mapname",
    "input", "fps", "mplayer", "spells", "skills", "party", "notification",
    "container", "label", "texture", "buddy", "active_effects",

    "container_strip", "menu", "menuitem"
};
static void (*widget_initializers[TOTAL_SUBWIDGETS]) (widgetdata *);

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
    0, NULL, 0, 0
};

/**
 * A way to steal the mouse, and to prevent widgets from using mouse events
 * Example: Prevents widgets from using mouse events during dragging procedure
 * */
static int IsMouseExclusive = 0;

static int widget_id_from_name(const char *name)
{
    int i;

    for (i = 0; i < TOTAL_SUBWIDGETS; i++) {
        if (strcmp(widget_names[i], name) == 0) {
            return i;
        }
    }

    return -1;
}

static int widget_load(const char *path, uint8 defaults, widgetdata *widgets[])
{
    FILE *fp;
    char buf[HUGE_BUF], *end, *line, *cp;
    widgetdata *widget;
    int depth, old_depth;

    fp = fopen_wrapper(path, "r");

    if (!fp) {
        return 0;
    }

    widget = NULL;
    depth = old_depth = 0;

    while (fgets(buf, sizeof(buf), fp)) {
        if (*buf == '#' || *buf == '\n') {
            continue;
        }

        end = strchr(buf, '\n');

        if (end) {
            *end = '\0';
        }

        old_depth = depth;
        depth = 0;
        line = buf;

        while (*line == '\t') {
            depth++;
            line++;
        }

        if (string_startswith(line, "[") && string_endswith(line, "]")) {
            int id;

            if (old_depth != 0) {
                insert_widget_in_container(widgets[old_depth - 1], widget, 1);
            }

            cp = string_sub(line, 1, -1);
            id = widget_id_from_name(cp);
            efree(cp);

            if (id == -1) {
                /* Reset to NULL in case there was a valid widget previously,
                 * so that we don't load into it from this invalid one. */
                widget = NULL;
                logger_print(LOG(DEBUG), "Invalid widget: %s", line);
                continue;
            }

            widget = defaults ? &def_widget[id] : create_widget_object(id);
            widgets[depth] = widget;

            if (defaults) {
                widget->required = 1;
                widget->save = 1;
            }
        }
        else if (widget) {
            char *cps[2];

            if (string_split(line, cps, arraysize(cps), '=') != arraysize(cps)) {
                logger_print(LOG(BUG), "Invalid line: %s", line);
                continue;
            }

            string_whitespace_trim(cps[0]);
            string_whitespace_trim(cps[1]);

            if (strcmp(cps[0], "id") == 0) {
                widget->id = estrdup(cps[1]);
            }
            else if (strcmp(cps[0], "texture_type") == 0) {
                widget->texture_type = atoi(cps[1]);
            }
            else if (strcmp(cps[0], "bg") == 0) {
                strncpy(widget->bg, cps[1], sizeof(widget->bg) - 1);
                widget->bg[sizeof(widget->bg) - 1] = '\0';
            }
            else if (strcmp(cps[0], "moveable") == 0) {
                KEYWORD_TO_BOOLEAN(cps[1], widget->moveable);
            }
            else if (strcmp(cps[0], "shown") == 0) {
                KEYWORD_TO_BOOLEAN(cps[1], widget->show);
            }
            else if (strcmp(cps[0], "resizeable") == 0) {
                KEYWORD_TO_BOOLEAN(cps[1], widget->resizeable);
            }
            else if (strcmp(cps[0], "required") == 0) {
                KEYWORD_TO_BOOLEAN(cps[1], widget->required);
            }
            else if (strcmp(cps[0], "save") == 0) {
                KEYWORD_TO_BOOLEAN(cps[1], widget->save);
            }
            else if (strcmp(cps[0], "x") == 0) {
                widget->x = atoi(cps[1]);
            }
            else if (strcmp(cps[0], "y") == 0) {
                widget->y = atoi(cps[1]);
            }
            else if (strcmp(cps[0], "w") == 0) {
                resize_widget(widget, RESIZE_RIGHT, atoi(cps[1]));
            }
            else if (strcmp(cps[0], "h") == 0) {
                resize_widget(widget, RESIZE_BOTTOM, atoi(cps[1]));
            }
            else if (strcmp(cps[0], "min_w") == 0) {
                widget->min_w = atoi(cps[1]);
            }
            else if (strcmp(cps[0], "min_h") == 0) {
                widget->min_h = atoi(cps[1]);
            }
            else if (widget->load_func && widget->load_func(widget, cps[0], cps[1])) {
            }
            else {
                logger_print(LOG(BUG), "Invalid line: %s = %s", cps[0], cps[1]);
            }
        }
    }

    fclose(fp);

    if (old_depth != 0) {
        insert_widget_in_container(widgets[old_depth - 1], widget, 1);
    }

    return 1;
}

/**
 * Try to load the main interface file and initialize the priority list
 * On failure, initialize the widgets with init_widgets_fromDefault() */
void toolkit_widget_init(void)
{
    widgetdata *widgets[100];

    widget_initializers[ACTIVE_EFFECTS_ID] = widget_active_effects_init;
    widget_initializers[BUDDY_ID] = widget_buddy_init;
    widget_initializers[CONTAINER_ID] = widget_container_init;
    widget_initializers[FPS_ID] = widget_fps_init;
    widget_initializers[INPUT_ID] = widget_input_init;
    widget_initializers[MAIN_INV_ID] = widget_inventory_init;
    widget_initializers[BELOW_INV_ID] = widget_inventory_init;
    widget_initializers[LABEL_ID] = widget_label_init;
    widget_initializers[MAP_ID] = widget_map_init;
    widget_initializers[MAPNAME_ID] = widget_mapname_init;
    widget_initializers[MENU_B_ID] = widget_menu_buttons_init;
    widget_initializers[MPLAYER_ID] = widget_mplayer_init;
    widget_initializers[NOTIFICATION_ID] = widget_notification_init;
    widget_initializers[PARTY_ID] = widget_party_init;
    widget_initializers[PDOLL_ID] = widget_playerdoll_init;
    widget_initializers[PLAYER_INFO_ID] = widget_playerinfo_init;
    widget_initializers[QUICKSLOT_ID] = widget_quickslots_init;
    widget_initializers[SKILLS_ID] = widget_skills_init;
    widget_initializers[SPELLS_ID] = widget_spells_init;
    widget_initializers[STAT_ID] = widget_stat_init;
    widget_initializers[TEXTURE_ID] = widget_texture_init;
    widget_initializers[CHATWIN_ID] = widget_textwin_init;

    if (!widget_load("data/interface.cfg", 1, widgets)) {
        logger_print(LOG(ERROR), "Could not load widget defaults from data/interface.cfg.");
        exit(1);
    }

    widget_load("settings/interface.cfg", 0, widgets);
    widgets_ensure_onscreen();
}

/** @copydoc widgetdata::menu_handle_func */
static int widget_menu_handle(widgetdata *widget, SDL_Event *event)
{
    widgetdata *menu;

    /* Create a context menu for the widget clicked on. */
    menu = create_menu(event->motion.x, event->motion.y, widget);

    if ((widget->sub_type == MAIN_INV_ID || widget->sub_type == BELOW_INV_ID) && INVENTORY_MOUSE_INSIDE(widget, event->motion.x, event->motion.y)) {
        if (widget->sub_type == MAIN_INV_ID) {
            add_menuitem(menu, "Drop", &menu_inventory_drop, MENU_NORMAL, 0);
        }

        add_menuitem(menu, "Get", &menu_inventory_get, MENU_NORMAL, 0);

        if (widget->sub_type == BELOW_INV_ID) {
            add_menuitem(menu, "Get all", &menu_inventory_getall, MENU_NORMAL, 0);
        }

        add_menuitem(menu, "Examine", &menu_inventory_examine, MENU_NORMAL, 0);

        if (setting_get_int(OPT_CAT_DEVEL, OPT_OPERATOR)) {
            add_menuitem(menu, "Patch", &menu_inventory_patch, MENU_NORMAL, 0);
            add_menuitem(menu, "Load to console", &menu_inventory_loadtoconsole, MENU_NORMAL, 0);
        }

        if (widget->sub_type == MAIN_INV_ID) {
            add_menuitem(menu, "More  >", &menu_inventory_submenu_more, MENU_SUBMENU, 0);
        }

        /* Process the right click event so the correct item is
         * selected. */
        widget->event_func(widget, event);
    }
    else {
        widget_menu_standard_items(widget, menu);

        if (widget->sub_type == MAIN_INV_ID) {
            add_menuitem(menu, "Inventory Filters  >", &menu_inv_filter_submenu, MENU_SUBMENU, 0);
        }
    }

    menu_finalize(menu);

    return 1;
}

void menu_container_move(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    widget_event_start_move(widget->env ? widget->env : widget);
}

void menu_container_detach(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    widgetdata *widget_container;

    widget_container = widget->env;
    detach_widget(widget);

    if (!widget_container->inv) {
        remove_widget_object(widget_container);
    }
}

void menu_container_attach(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    widgetdata *widget_container;

    widget_container = create_widget_object(CONTAINER_ID);
    widget_container->x = widget->x;
    widget_container->y = widget->y;
    insert_widget_in_container(widget_container, widget, 0);
}

void menu_container_background_change(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    widgetdata *tmp, *container;
    _widget_label *label;

    container = get_innermost_container(widget);

    for (tmp = menuitem->inv; tmp; tmp = tmp->next) {
        if (tmp->type == LABEL_ID) {
            label = LABEL(tmp);
            logger_print(LOG(INFO), "%s, %s", label->text, container->texture ? container->texture->name : "NONE");

            SDL_FreeSurface(container->surface);
            container->surface = NULL;

            container->texture_type = WIDGET_TEXTURE_TYPE_NORMAL;
            container->texture = NULL;

            if (strcmp(label->text, "Blank") == 0) {
                strncpy(container->bg, "#000000", sizeof(container->bg) - 1);
                container->bg[sizeof(container->bg) - 1] = '\0';
            }
            else if (strcmp(label->text, "Texturised") == 0) {
                strncpy(container->bg, "widget_bg", sizeof(container->bg) - 1);
                container->bg[sizeof(container->bg) - 1] = '\0';
            }
            else if (strcmp(label->text, "Transparent") == 0) {
                container->texture_type = WIDGET_TEXTURE_TYPE_NONE;
            }

            WIDGET_REDRAW(container);

            break;
        }
    }
}

void menu_container_background(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    widgetdata *submenu, *container;
    int is_widget_bg;

    submenu = MENU(menuitem->env)->submenu;
    container = get_innermost_container(widget);
    is_widget_bg = container->texture != NULL && strstr(container->texture->name, "widget_bg") != NULL;

    add_menuitem(submenu, "Blank", &menu_container_background_change, MENU_RADIO, container->texture != NULL && !is_widget_bg);
    add_menuitem(submenu, "Texturised", &menu_container_background_change, MENU_RADIO, container->texture != NULL && is_widget_bg);
    add_menuitem(submenu, "Transparent", &menu_container_background_change, MENU_RADIO, container->texture == NULL);
}

static void menu_container(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    widgetdata *submenu, *outermost;

    submenu = MENU(menuitem->env)->submenu;
    outermost = get_outermost_container(widget);

    if (outermost->type == CONTAINER_ID) {
        add_menuitem(submenu, "Move", &menu_container_move, MENU_NORMAL, 0);
        add_menuitem(submenu, "Background  >", &menu_container_background, MENU_SUBMENU, 0);
    }

    if (widget != outermost) {
        add_menuitem(submenu, "Detach", &menu_container_detach, MENU_NORMAL, 0);
    }
    else if (widget->type != CONTAINER_ID) {
        add_menuitem(submenu, "Attach", &menu_container_attach, MENU_NORMAL, 0);
    }
}

void widget_menu_standard_items(widgetdata *widget, widgetdata *menu)
{
    if (widget->type != CONTAINER_ID) {
        add_menuitem(menu, "Move Widget", &menu_move_widget, MENU_NORMAL, 0);
    }

    add_menuitem(menu, "Container  >", &menu_container, MENU_SUBMENU, 0);
}

static void widget_texture_create(widgetdata *widget)
{
    char buf[MAX_BUF];

    if (widget->texture_type == WIDGET_TEXTURE_TYPE_NORMAL) {
        snprintf(buf, sizeof(buf), "rectangle:%d,%d;[bar=%s]", widget->w, widget->h, widget->bg[0] != '\0' ? widget->bg : "widget_bg");
    }

    texture_delete(widget->texture);
    widget->texture = texture_get(TEXTURE_TYPE_SOFTWARE, buf);
}

/** Wrapper function to handle the creation of a widget. */
widgetdata *create_widget_object(int widget_subtype_id)
{
    widgetdata *widget;
    int widget_type_id = widget_subtype_id;

    /* map the widget subtype to widget type */
    if (widget_subtype_id >= TOTAL_WIDGETS) {
        switch (widget_subtype_id) {
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
    if (widget_subtype_id < 0 || widget_subtype_id >= TOTAL_SUBWIDGETS) {
        return NULL;
    }

    /* don't create more than one widget if it is a unique widget */
    if (cur_widget[widget_subtype_id] && cur_widget[widget_subtype_id]->unique) {
        return NULL;
    }

    /* allocate the widget node, this should always be the first function called
     * in here */
    widget = create_widget(widget_subtype_id);
    widget->type = widget_type_id;
    widget->name = estrdup(widget_names[widget_subtype_id]);
    widget->redraw = 1;
    widget->menu_handle_func = widget_menu_handle;

    if (widget_initializers[widget->type]) {
        widget_initializers[widget->type](widget);
    }

    return widget;
}

/** Wrapper function to handle the obliteration of a widget. */
void remove_widget_object(widgetdata *widget)
{
    /* don't delete the last widget if there needs to be at least one of this
     * widget type */
    if (widget->required && cur_widget[widget->sub_type] == type_list_foot[widget->sub_type]) {
        return;
    }

    remove_widget_object_intern(widget);
}

/**
 * Wrapper function to handle the annihilation of a widget, including possibly
 * killing the linked list altogether.
 * Please do not use, this should only be explicitly called by
 * kill_widget_tree() and remove_widget_object().
 * Use remove_widget_object() for everything else. */
void remove_widget_object_intern(widgetdata *widget)
{
    widgetdata *tmp;

    remove_widget_inv(widget);

    /* If this widget happens to be the owner of an event, keeping them pointed
     * to it is a bad idea. */
    if (widget_mouse_event.owner == widget) {
        widget_mouse_event.owner = NULL;
    }

    if (widget_event_move.owner == widget) {
        widget_event_move.owner = NULL;
    }

    if (widget_event_resize.owner == widget) {
        widget_event_resize.owner = NULL;
    }

    /* If any menu is open and this widget is the owner, bad things could happen
     * here too. Clear the pointers. */
    if (cur_widget[MENU_ID] && (MENU(cur_widget[MENU_ID]))->owner == widget) {
        for (tmp = cur_widget[MENU_ID]; tmp; tmp = tmp->type_next) {
            (MENU(cur_widget[MENU_ID]))->owner = NULL;
        }
    }

    /* Get the environment if it exists, this is used to make containers
     * auto-resize when the widget is deleted. */
    tmp = widget->env;

    if (widget->deinit_func) {
        widget->deinit_func(widget);
    }

    efree(widget->name);

    if (widget->id) {
        efree(widget->id);
    }

    /* remove the custom attribute nodes if they exist */
    if (widget->subwidget) {
        efree(widget->subwidget);
        widget->subwidget = NULL;
    }

    /* finally de-allocate the widget node, this should always be the last node
     * removed in here */
    remove_widget(widget);

    /* resize the container that used to contain this widget, if it exists */
    if (tmp) {
        /* if something else exists in its inventory, make it auto-resize to fit
         * the widgets inside */
        if (tmp->inv) {
            resize_widget(tmp->inv, RESIZE_RIGHT, tmp->inv->w);
            resize_widget(tmp->inv, RESIZE_BOTTOM, tmp->inv->h);
        }
        /* otherwise if its inventory is empty, resize it to its default size */
        else {
            resize_widget(tmp, RESIZE_RIGHT, cur_widget[tmp->sub_type]->w);
            resize_widget(tmp, RESIZE_BOTTOM, cur_widget[tmp->sub_type]->h);
        }
    }
}

/**
 * Deletes the entire inventory of a widget, child nodes first. This should be
 * the fastest way.
 * Any widgets that can't be deleted should end up on the top level.
 * This function is already automatically handled with the delete_inv flag,
 * so it shouldn't be called explicitly apart from in
 * remove_widget_object_intern(). */
void remove_widget_inv(widgetdata *widget)
{
    widgetdata *tmp;

    for (widget = widget->inv; widget; widget = tmp) {
        /* call this function recursively to get to the first child node deep
         * down inside the widget */
        remove_widget_inv(widget);
        /* we need a temp pointer for the next node, as the current node is
         * about to be no more */
        tmp = widget->next;
        /* then remove the widget, and slowly work our way up the tree deleting
         * widgets until we get to the original widget again */
        remove_widget_object(widget);
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
    if (widget_list_head) {
        kill_widget_tree(widget_list_head);
    }
}

/**
 * Resets widget's coordinates from default.
 * @param name Widget name to reset. If NULL, will reset all. */
void reset_widget(const char *name)
{
    widgetdata *tmp;

    for (tmp = widget_list_head; tmp; tmp = tmp->next) {
        if (!tmp->moveable) {
            continue;
        }

        if (!name || !strcasecmp(tmp->name, name)) {
            tmp->x = def_widget[tmp->type].x;
            tmp->y = def_widget[tmp->type].y;
            tmp->w = def_widget[tmp->type].w;
            tmp->h = def_widget[tmp->type].h;
            tmp->show = def_widget[tmp->type].show;
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

    if (!setting_get_int(OPT_CAT_CLIENT, OPT_OFFSCREEN_WIDGETS)) {
        if (widget->x < 0) {
            dx = -widget->x;
        }
        else if (widget->x + widget->w > setting_get_int(OPT_CAT_CLIENT, OPT_RESOLUTION_X)) {
            dx = setting_get_int(OPT_CAT_CLIENT, OPT_RESOLUTION_X) - widget->w - widget->x;
        }

        if (widget->y < 0) {
            dy = -widget->y;
        }
        else if (widget->y + widget->h > setting_get_int(OPT_CAT_CLIENT, OPT_RESOLUTION_Y)) {
            dy = setting_get_int(OPT_CAT_CLIENT, OPT_RESOLUTION_Y) - widget->h - widget->y;
        }
    }

    if (widget->env) {
        if (widget->x < widget->env->x) {
            dx = widget->env->x - widget->x;
        }
        else if (widget->x + widget->w > widget->env->x + widget->env->w) {
            dx = (widget->env->x + widget->env->w) - (widget->w + widget->x);
        }

        if (widget->y < widget->env->y) {
            dy = widget->env->y - widget->y;
        }
        else if (widget->y + widget->h > widget->env->y + widget->env->h) {
            dy = (widget->env->y + widget->env->h) - (widget->h + widget->y);
        }
    }

    move_widget_rec(widget, dx, dy);
}

/**
 * Ensures all widgets are on-screen. */
void widgets_ensure_onscreen(void)
{
    widgetdata *tmp;

    for (tmp = widget_list_head; tmp; tmp = tmp->next) {
        widget_ensure_onscreen(tmp);
    }
}

/** Recursive function to nuke the entire widget tree. */
void kill_widget_tree(widgetdata *widget)
{
    widgetdata *tmp;

    do
    {
        /* we want to process the widgets starting from the left hand side of
         * the tree first */
        if (widget->inv) {
            kill_widget_tree(widget->inv);
        }

        /* store the next sibling in a tmp variable, as widget is about to be
         * zapped from existence */
        tmp = widget->next;

        /* here we call our widget kill function, and force removal by using the
         * internal one */
        remove_widget_object_intern(widget);

        /* get the next sibling for our next loop */
        widget = tmp;
    }
    while (widget);
}

/**
 * Creates a new widget object with a unique ID and inserts it at the root of
 * the widget tree.
 * This should always be the first function called by create_widget_object() in
 * order to get the pointer
 * to the new node so we can link it to other new nodes that depend on it. */
widgetdata *create_widget(int widget_id)
{
    widgetdata *node;

#ifdef DEBUG_WIDGET
    logger_print(LOG(INFO), "Entering create_widget()..");
#endif

    /* allocate it */
    node = ecalloc(1, sizeof(widgetdata));

    /* set the members */
    memcpy(node, &def_widget[widget_id], sizeof(*node));
    node->sub_type = widget_id;

    /* link it up to the tree if the root exists */
    if (widget_list_head) {
        node->next = widget_list_head;
        widget_list_head->prev = node;
    }

    /* set the foot if it doesn't exist */
    if (!widget_list_foot) {
        widget_list_foot = node;
    }

    /* the new node becomes the new root node, which also automatically brings
     * it to the front */
    widget_list_head = node;

    /* if head of widget type linked list doesn't exist, set the head and foot
     * */
    if (!cur_widget[widget_id]) {
        cur_widget[widget_id] = type_list_foot[widget_id] = node;
    }
    /* otherwise, link the node in to the existing type list */
    else {
        type_list_foot[widget_id]->type_next = node;
        node->type_prev = type_list_foot[widget_id];
        type_list_foot[widget_id] = node;
    }

#ifdef DEBUG_WIDGET
    logger_print(LOG(DEBUG), "..ALLOCATED: %s", node->name);
    debug_count_nodes(1);

    logger_print(LOG(INFO), "..create_widget(): Done.");
#endif

    return node;
}

/** Removes the pointer passed to it from anywhere in the linked list and
 * reconnects the adjacent nodes to each other. */
void remove_widget(widgetdata *widget)
{
    widgetdata *tmp = NULL;

#ifdef DEBUG_WIDGET
    logger_print(LOG(INFO), "Entering remove_widget()..");
#endif

    /* node to delete is the only node in the tree, bye-bye binary tree :) */
    if (!widget_list_head->next && !widget_list_head->inv) {
        widget_list_head = NULL;
        widget_list_foot = NULL;
        cur_widget[widget->sub_type] = NULL;
        type_list_foot[widget->sub_type] = NULL;
    }
    else {
        /* node to delete is the head, move the pointer to next node */
        if (widget == widget_list_head) {
            widget_list_head = widget_list_head->next;
            widget_list_head->prev = NULL;
        }
        /* node to delete is the foot, move the pointer to the previous node */
        else if (widget == widget_list_foot) {
            widget_list_foot = widget_list_foot->prev;
            widget_list_foot->next = NULL;
        }
        /* node is first sibling, and should have a parent since it is not the
         * root node */
        else if (!widget->prev) {
            /* node is also the last sibling, so NULL the parent's inventory */
            if (!widget->next) {
                widget->env->inv = NULL;
                widget->env->inv_rev = NULL;
            }
            /* or else make it the parent's first child */
            else {
                widget->env->inv = widget->next;
                widget->next->prev = NULL;
            }
        }
        /* node is last sibling and should have a parent, move the inv_rev
         * pointer to the previous sibling */
        else if (!widget->next) {
            widget->env->inv_rev = widget->prev;
            widget->prev->next = NULL;
        }
        /* node to delete is in the middle of the tree somewhere */
        else {
            widget->next->prev = widget->prev;
            widget->prev->next = widget->next;
        }

        /* move the children to the top level of the list, starting from the end
         * child */
        for (tmp = widget->inv_rev; tmp; tmp = tmp->prev) {
            /* tmp is no longer in a container */
            tmp->env = NULL;
            widget_list_head->prev = tmp;
            tmp->next = widget_list_head;
            widget_list_head = tmp;
        }

        /* if widget type list has only one node, kill it */
        if (cur_widget[widget->sub_type] == type_list_foot[widget->sub_type]) {
            cur_widget[widget->sub_type] = type_list_foot[widget->sub_type] = NULL;
        }
        /* widget is head node */
        else if (widget == cur_widget[widget->sub_type]) {
            cur_widget[widget->sub_type] = cur_widget[widget->sub_type]->type_next;
            cur_widget[widget->sub_type]->type_prev = NULL;
        }
        /* widget is foot node */
        else if (widget == type_list_foot[widget->sub_type]) {
            type_list_foot[widget->sub_type] = type_list_foot[widget->sub_type]->type_prev;
            type_list_foot[widget->sub_type]->type_next = NULL;
        }
        /* widget is in middle of type list */
        else {
            widget->type_prev->type_next = widget->type_next;
            widget->type_next->type_prev = widget->type_prev;
        }
    }

#ifdef DEBUG_WIDGET
    logger_print(LOG(DEBUG), "..REMOVED: %s", widget->name);
#endif

    /* free the surface */
    if (widget->surface) {
        SDL_FreeSurface(widget->surface);
        widget->surface = NULL;
    }

    efree(widget);

#ifdef DEBUG_WIDGET
    debug_count_nodes(1);
    logger_print(LOG(INFO), "..remove_widget(): Done.");
#endif
}

/** Removes the widget from the container it is inside and moves it to the top
 * of the priority tree. */
void detach_widget(widgetdata *widget)
{
    /* sanity check */
    if (!widget->env) {
        return;
    }

    /* first unlink the widget from the container and siblings */

    /* if widget is only one in the container's inventory, clear both pointers
     * */
    if (!widget->prev && !widget->next) {
        widget->env->inv = NULL;
        widget->env->inv_rev = NULL;
    }
    /* widget is first sibling */
    else if (!widget->prev) {
        widget->env->inv = widget->next;
        widget->next->prev = NULL;
    }
    /* widget is last sibling */
    else if (!widget->next) {
        widget->env->inv_rev = widget->prev;
        widget->prev->next = NULL;
    }
    /* widget is a middle sibling */
    else {
        widget->prev->next = widget->next;
        widget->next->prev = widget->prev;
    }

    /* if something else exists in the container's inventory, make it
     * auto-resize to fit the widgets inside */
    if (widget->env->inv) {
        resize_widget(widget->env->inv, RESIZE_RIGHT, widget->env->inv->w);
        resize_widget(widget->env->inv, RESIZE_BOTTOM, widget->env->inv->h);
    }
    /* otherwise if its inventory is empty, resize it to its default size */
    else {
        resize_widget(widget->env, RESIZE_RIGHT, cur_widget[widget->env->sub_type]->w);
        resize_widget(widget->env, RESIZE_BOTTOM, cur_widget[widget->env->sub_type]->h);
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
        /* we print out the top node, and then go down a level, rather than go
         * down first */
        if (output) {
            /* a way of representing graphically how many levels down we are */
            for (tmp = 0; tmp < j; ++tmp) {
                printf("..");
            }

            logger_print(LOG(INFO), "..%s", widget->name);
        }

        i++;

        /* we want to process the widgets starting from the left hand side of
         * the tree first */
        if (widget->inv) {
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

    if (widget_list_head) {
        i = debug_count_nodes_rec(widget_list_head, 0, 0, output);
    }

    logger_print(LOG(INFO), "========================================");
    logger_print(LOG(INFO), "..Total widget nodes: %d", i);
}
#endif

static void widget_save_rec(FILE *fp, widgetdata *widget, int depth)
{
    char *padding;

    for (; widget; widget = widget->prev) {
        if (!widget->save) {
            continue;
        }

        padding = string_repeat("\t", depth);

        fprintf(fp, "%s[%s]\n", padding, widget->name);

        if (widget->id) {
            fprintf(fp, "%sid = %s\n", padding, widget->id);
        }

        if (widget->moveable != def_widget[widget->sub_type].moveable) {
            fprintf(fp, "%smoveable = %s\n", padding, widget->moveable ? "yes" : "no");
        }

        if (widget->show != def_widget[widget->sub_type].show) {
            fprintf(fp, "%sshown = %s\n", padding, widget->show ? "yes" : "no");
        }

        if (widget->resizeable != def_widget[widget->sub_type].resizeable) {
            fprintf(fp, "%sresizeable = %s\n", padding, widget->resizeable ? "yes" : "no");
        }

        if (widget->required != def_widget[widget->sub_type].required) {
            fprintf(fp, "%srequired = %s\n", padding, widget->required ? "yes" : "no");
        }

        if (widget->save != def_widget[widget->sub_type].save) {
            fprintf(fp, "%ssave = %s\n", padding, widget->save ? "yes" : "no");
        }

        if (widget->x != def_widget[widget->sub_type].x) {
            fprintf(fp, "%sx = %d\n", padding, widget->x);
        }

        if (widget->y != def_widget[widget->sub_type].y) {
            fprintf(fp, "%sy = %d\n", padding, widget->y);
        }

        if (widget->w != def_widget[widget->sub_type].w) {
            fprintf(fp, "%sw = %d\n", padding, widget->w);
        }

        if (widget->h != def_widget[widget->sub_type].h) {
            fprintf(fp, "%sh = %d\n", padding, widget->h);
        }

        if (widget->min_w != def_widget[widget->sub_type].min_w) {
            fprintf(fp, "%smin_w = %d\n", padding, widget->min_w);
        }

        if (widget->min_h != def_widget[widget->sub_type].min_h) {
            fprintf(fp, "%smin_h = %d\n", padding, widget->min_h);
        }

        if (strcmp(widget->bg, def_widget[widget->sub_type].bg) != 0) {
            fprintf(fp, "%sbg = %s\n", padding, widget->bg);
        }

        if (widget->texture_type != def_widget[widget->sub_type].texture_type) {
            fprintf(fp, "%stexture_type = %d\n", padding, widget->texture_type);
        }

        if (widget->save_func) {
            widget->save_func(widget, fp, padding);
        }

        if (widget->inv_rev) {
            widget_save_rec(fp, widget->inv_rev, depth + 1);
        }

        if (depth == 0) {
            fprintf(fp, "\n");
        }

        efree(padding);
    }
}

static void widget_save(void)
{
    FILE *fp;

    fp = fopen_wrapper("settings/interface.cfg", "w");

    if (!fp) {
        return;
    }

    widget_save_rec(fp, widget_list_foot, 0);
    fclose(fp);
}

void toolkit_widget_deinit(void)
{
    widget_save();
    kill_widgets();
}

/**
 * Make widgets try to handle an event.
 * @param event Event to handle.
 * @return 1 if the event was handled, 0 otherwise. */
int widgets_event(SDL_Event *event)
{
    widgetdata *widget;
    int ret;

    /* Widget is being moved. */
    if (widget_event_move.active) {
        widget = widget_event_move.owner;

        if (event->type == SDL_MOUSEMOTION) {
            int x, y, nx, ny;

            x = event->motion.x - widget_event_move.xOffset;
            y = event->motion.y - widget_event_move.yOffset;
            nx = x;
            ny = y;

            /* Widget snapping logic courtesy of OpenTTD (GPLv2). */
            if (setting_get_int(OPT_CAT_GENERAL, OPT_SNAP_RADIUS)) {
                widgetdata *tmp;
                int delta, hsnap, vsnap;

                delta = 0;
                hsnap = vsnap = setting_get_int(OPT_CAT_GENERAL, OPT_SNAP_RADIUS);

                for (tmp = widget_list_head; tmp; tmp = tmp->next) {
                    if (tmp == widget || tmp->disable_snapping || !tmp->show) {
                        continue;
                    }

                    if (y + widget->h > tmp->y && y < tmp->y + tmp->h) {
                        /* Your left border <-> other right border */
                        delta = abs(tmp->x + tmp->w - x);

                        if (delta <= hsnap) {
                            nx = tmp->x + tmp->w;
                            hsnap = delta;
                        }

                        /* Your right border <-> other left border */
                        delta = abs(tmp->x - x - widget->w);

                        if (delta <= hsnap) {
                            nx = tmp->x - widget->w;
                            hsnap = delta;
                        }
                    }

                    if (widget->y + widget->h >= tmp->y && widget->y <= tmp->y + tmp->h) {
                        /* Your left border <-> other left border */
                        delta = abs(tmp->x - x);

                        if (delta <= hsnap) {
                            nx = tmp->x;
                            hsnap = delta;
                        }

                        /* Your right border <-> other right border */
                        delta = abs(tmp->x + tmp->w - x - widget->w);

                        if (delta <= hsnap) {
                            nx = tmp->x + tmp->w - widget->w;
                            hsnap = delta;
                        }
                    }

                    if (x + widget->w > tmp->x && x < tmp->x + tmp->w) {
                        /* Your top border <-> other bottom border */
                        delta = abs(tmp->y + tmp->h - y);

                        if (delta <= vsnap) {
                            ny = tmp->y + tmp->h;
                            vsnap = delta;
                        }

                        /* Your bottom border <-> other top border */
                        delta = abs(tmp->y - y - widget->h);

                        if (delta <= vsnap) {
                            ny = tmp->y - widget->h;
                            vsnap = delta;
                        }
                    }

                    if (widget->x + widget->w >= tmp->x && widget->x <= tmp->x + tmp->w) {
                        /* Your top border <-> other top border */
                        delta = abs(tmp->y - y);

                        if (delta <= vsnap) {
                            ny = tmp->y;
                            vsnap = delta;
                        }

                        /* Your bottom border <-> other bottom border */
                        delta = abs(tmp->y + tmp->h - y - widget->h);

                        if (delta <= vsnap) {
                            ny = tmp->y + tmp->h - widget->h;
                            vsnap = delta;
                        }
                    }
                }
            }

            move_widget_rec(widget, nx - widget->x, ny - widget->y);
            widget_ensure_onscreen(widget);
        }
        else if (event->type == SDL_MOUSEBUTTONDOWN) {
            return widget_event_move_stop(event->motion.x, event->motion.y);
        }

        return 1;
    }
    /* Widget is being resized. */
    else if (widget_event_resize.active) {
        widget = widget_event_resize.owner;

        if (event->type == SDL_MOUSEBUTTONUP) {
            widget_event_resize.active = 0;
            widget_event_resize.owner = NULL;
        }
        else if (event->type == SDL_MOUSEMOTION) {
            if (widget->resize_flags & (RESIZE_LEFT | RESIZE_RIGHT)) {
                resize_widget(widget, widget->resize_flags & (RESIZE_LEFT | RESIZE_RIGHT), MAX(MAX(5, widget->min_w), widget->w + (event->motion.x - widget_event_resize.xoff) * (widget->resize_flags & RESIZE_LEFT ? -1 : 1)));
            }

            if (widget->resize_flags & (RESIZE_TOP | RESIZE_BOTTOM)) {
                resize_widget(widget, widget->resize_flags & (RESIZE_TOP | RESIZE_BOTTOM), MAX(MAX(5, widget->min_h), widget->h + (event->motion.y - widget_event_resize.yoff) * (widget->resize_flags & RESIZE_TOP ? -1 : 1)));
            }

            widget_event_resize.xoff = event->motion.x;
            widget_event_resize.yoff = event->motion.y;
        }

        return 1;
    }

    if (EVENT_IS_MOUSE(event)) {
        if (!widget_event_respond(event->motion.x, event->motion.y)) {
            return 0;
        }

        widget = widget_mouse_event.owner;

        if (event->type == SDL_MOUSEMOTION) {
            if (widget->resizeable) {
                widget->resize_flags = 0;

#               define WIDGET_RESIZE_CHECK(coord, upper_adj, lower_adj) (event->motion.coord >= widget->coord + (upper_adj) && event->motion.coord <= widget->coord + (lower_adj))

                if (WIDGET_RESIZE_CHECK(y, 0, 2)) {
                    widget->resize_flags = RESIZE_TOP;
                }
                else if (WIDGET_RESIZE_CHECK(y, widget->h - 2, widget->h)) {
                    widget->resize_flags = RESIZE_BOTTOM;
                }
                else if (WIDGET_RESIZE_CHECK(x, 0, 2)) {
                    widget->resize_flags = RESIZE_LEFT;
                }
                else if (WIDGET_RESIZE_CHECK(x, widget->w - 2, widget->w)) {
                    widget->resize_flags = RESIZE_RIGHT;
                }

                if (widget->resize_flags & (RESIZE_TOP | RESIZE_BOTTOM)) {
                    if (WIDGET_RESIZE_CHECK(x, 0, widget->w * 0.05)) {
                        widget->resize_flags |= RESIZE_LEFT;
                    }
                    else if (WIDGET_RESIZE_CHECK(x, widget->w - widget->w * 0.05, widget->w)) {
                        widget->resize_flags |= RESIZE_RIGHT;
                    }
                }
                else if (widget->resize_flags & (RESIZE_LEFT | RESIZE_RIGHT)) {
                    if (WIDGET_RESIZE_CHECK(y, 0, widget->h * 0.05)) {
                        widget->resize_flags |= RESIZE_TOP;
                    }
                    else if (WIDGET_RESIZE_CHECK(y, widget->h - widget->h * 0.05, widget->h)) {
                        widget->resize_flags |= RESIZE_BOTTOM;
                    }
                }

                if (widget->resize_flags) {
                    return 1;
                }
            }
        }
        else if (event->type == SDL_MOUSEBUTTONDOWN) {
            /* Set the priority to this widget. */
            SetPriorityWidget(widget);

            /* Right mouse button was clicked, try to create menu. */
            if (event->button.button == SDL_BUTTON_RIGHT && !cur_widget[MENU_ID] && widget->menu_handle_func && widget->menu_handle_func(widget, event)) {
                return 1;
            }
            /* Start resizing. */
            else if (widget->resize_flags && event->button.button == SDL_BUTTON_LEFT) {
                widget_event_resize.active = 1;
                widget_event_resize.owner = widget;
                widget_event_resize.xoff = event->motion.x;
                widget_event_resize.yoff = event->motion.y;
                return 1;
            }

            if (cur_widget[MENU_ID] && widget->sub_type != MENUITEM_ID && widget->env && widget->env->sub_type == MENUITEM_ID) {
                widget = widget->env;
            }
        }

        ret = 0;

        if (widget->event_func) {
            ret = widget->event_func(widget, event);
        }

        if (event->type == SDL_MOUSEBUTTONDOWN) {
            widgetdata *tmp, *next;

            for (tmp = cur_widget[MENU_ID]; tmp; tmp = next) {
                next = tmp->type_next;
                remove_widget_object(tmp);
            }
        }

        return ret;
    }
    else {
        for (widget = widget_list_head; widget; widget = widget->next) {
            if (widget->event_func && widget->event_func(widget, event)) {
                return 1;
            }
        }
    }

    return 0;
}

/** Handles the initiation of widget dragging. */
int widget_event_start_move(widgetdata *widget)
{
    int x, y;

    /* if its moveable, start moving it when the conditions warrant it, or else
     * run away */
    if (!widget->moveable) {
        return 0;
    }

    x = widget->x + widget->w / 2;
    y = widget->y + widget->h / 2;
    SDL_WarpMouse(x, y);

    /* we know this widget owns the mouse.. */
    widget_event_move.active = 1;

    /* start the movement procedures */
    widget_event_move.owner = widget;
    widget_event_move.xOffset = x - widget->x;
    widget_event_move.yOffset = y - widget->y;

    return 1;
}

int widget_event_move_stop(int x, int y)
{
    widgetdata *widget, *widget_container;

    if (!widget_event_move.active) {
        return 0;
    }

    widget = widget_mouse_event.owner;

    widget_event_move.active = 0;
    widget_mouse_event.x = x;
    widget_mouse_event.y = y;
    /* No widgets are being moved now. */
    widget_event_move.owner = NULL;

    cursor_texture = texture_get(TEXTURE_TYPE_CLIENT, "cursor_default");

    /* Somehow the owner before the widget dragging is gone now. Not a
     * good idea to carry on... */
    if (!widget) {
        return 0;
    }

    /* Check to see if it's on top of a widget container. */
    widget_container = get_outermost_container(get_widget_owner(x, y, get_outermost_container(widget)->next, NULL));

    /* Attempt to insert it into the widget container if it exists. */
    insert_widget_in_container(widget_container, get_outermost_container(widget), 0);

    return 1;
}

/** Updates the widget mouse event struct in order to respond to an event. */
int widget_event_respond(int x, int y)
{
    widget_mouse_event.owner = get_widget_owner(x, y, NULL, NULL);

    /* sanity check.. return if mouse is not in a widget */
    if (!widget_mouse_event.owner) {
        return 0;
    }

    /* setup the event structure in response */
    widget_mouse_event.x = x;
    widget_mouse_event.y = y;

    return 1;
}

/** Find the widget with mouse focus on a mouse-hit-test basis. */
widgetdata *get_widget_owner(int x, int y, widgetdata *start, widgetdata *end)
{
    widgetdata *success;

    /* mouse cannot be used by widgets */
    if (IsMouseExclusive) {
        return NULL;
    }

    /* no widgets exist */
    if (!widget_list_head) {
        return NULL;
    }

    /* sometimes we want a fast way to get the widget behind the widget at the
     * front.
     * this is what start is for, and we will only start walking the list
     * beginning with start.
     * if start is NULL, we just do a regular search */
    if (!start) {
        start = widget_list_head;
    }

    /* ok, let's kick off the recursion. if we find our widget, we get a widget
     * back. if not, we get a big fat NULL */
    success = get_widget_owner_rec(x, y, start, end);

    /*logger_print(LOG(DEBUG), "WIDGET OWNER: %s", success? success->name:
     * "NULL");*/

    return success;
}

/* traverse through the tree & perform custom or default hit-test */
widgetdata *get_widget_owner_rec(int x, int y, widgetdata *widget, widgetdata *end)
{
    widgetdata *success = NULL;

    do
    {
        /* skip if widget is hidden */
        if (!widget->show) {
            widget = widget->next;
            continue;
        }

        /* we want to get the first widget starting from the left hand side of
         * the tree first */
        if (widget->inv) {
            success = get_widget_owner_rec(x, y, widget->inv, end);

            /* we found a widget in the hit test? if so, get out of this
             * recursive mess with our prize */
            if (success) {
                return success;
            }
        }

        switch (widget->type) {
            default:

                if (x >= widget->x && x <= (widget->x + widget->w) && y >= widget->y && y <= (widget->y + widget->h)) {
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
 * The priority list is a binary tree, so we walk the tree by using loops and
 * recursions.
 * We actually only need to recurse for every child node. When we traverse the
 * siblings, we can just do a simple loop.
 * This makes it as fast as a linear linked list if there are no child nodes. */
static void process_widgets_rec(int draw, widgetdata *widget)
{
    uint8 redraw;

    for (; widget; widget = widget->prev) {
        if (widget->background_func) {
            widget->background_func(widget);
        }

        if (draw && widget->show && !widget->hidden && widget->draw_func) {
            if (widget->resize_flags) {
                if (cursor_x < widget->x || cursor_x > widget->x + widget->w || cursor_y < widget->y || cursor_y > widget->y + widget->h) {
                    widget->resize_flags = 0;
                }

                if (widget->resize_flags == (RESIZE_TOP | RESIZE_LEFT) || widget->resize_flags == (RESIZE_BOTTOM | RESIZE_RIGHT)) {
                    cursor_texture = texture_get(TEXTURE_TYPE_CLIENT, "cursor_resize_tl2br");
                }
                else if (widget->resize_flags == (RESIZE_TOP | RESIZE_RIGHT) || widget->resize_flags == (RESIZE_BOTTOM | RESIZE_LEFT)) {
                    cursor_texture = texture_get(TEXTURE_TYPE_CLIENT, "cursor_resize_tr2bl");
                }
                else if (widget->resize_flags & (RESIZE_LEFT | RESIZE_RIGHT)) {
                    cursor_texture = texture_get(TEXTURE_TYPE_CLIENT, "cursor_resize_hor");
                }
                else if (widget->resize_flags & (RESIZE_TOP | RESIZE_BOTTOM)) {
                    cursor_texture = texture_get(TEXTURE_TYPE_CLIENT, "cursor_resize_ver");
                }
            }

            if (!widget->texture && widget->texture_type != WIDGET_TEXTURE_TYPE_NONE) {
                widget_texture_create(widget);
            }

            if (widget->texture) {
                if (!widget->surface || widget->surface->w != widget->w || widget->surface->h != widget->h) {
                    SDL_Surface *texture;

                    if (widget->surface) {
                        widget_texture_create(widget);
                        SDL_FreeSurface(widget->surface);
                    }

                    texture = texture_surface(widget->texture);
                    widget->surface = SDL_ConvertSurface(texture, texture->format, texture->flags);
                }

                if (widget->redraw) {
                    surface_show(widget->surface, 0, 0, NULL, texture_surface(widget->texture));
                }
            }

            redraw = widget->redraw;
            widget->draw_func(widget);

            if (widget->texture) {
                SDL_Rect box;

                if (redraw && widget->texture_type == WIDGET_TEXTURE_TYPE_NORMAL) {
                    box.w = widget->w;
                    box.h = widget->h;
                    text_show_format(widget->surface, FONT_ARIAL10, 0, 0, COLOR_BLACK, TEXT_MARKUP, &box, "[border=widget_border -1 -1 %d]", WIDGET_BORDER_SIZE);
                }

                box.x = widget->x;
                box.y = widget->y;
                box.w = 0;
                box.h = 0;
                SDL_BlitSurface(widget->surface, NULL, ScreenSurface, &box);
            }

            widget->redraw -= redraw;
        }

        /* we want to process the widgets starting from the right hand side of
         * the tree first */
        if (widget->inv_rev) {
            process_widgets_rec(widget->show ? draw : 0, widget->inv_rev);
        }
    }
}

/**
 * Traverse through all the widgets and call the corresponding handlers.
 * This is now a wrapper function just to make the sanity checks before
 * continuing with the actual handling. */
void process_widgets(int draw)
{
    if (draw && widget_event_move.active) {
        cursor_texture = texture_get(TEXTURE_TYPE_CLIENT, "cursor_move");
    }

    process_widgets_rec(draw, widget_list_foot);
}

/**
 * A recursive function to bring a widget to the front of the priority list.
 * This makes the widget get displayed last so that they appear on top, and
 * handle events first.
 * In order to do this, we need to recurse backwards up the tree to the top
 * node,
 * and then work our way back down again, bringing each node in front of its
 * siblings. */
void SetPriorityWidget(widgetdata *node)
{
#ifdef DEBUG_WIDGET
    logger_print(LOG(DEBUG), "Entering SetPriorityWidget..");
#endif

    /* widget doesn't exist, means parent node has no children, so nothing to do
     * here */
    if (!node) {
#ifdef DEBUG_WIDGET
        logger_print(LOG(DEBUG), "..SetPriorityWidget(): Done (Node does not exist).");
#endif
        return;
    }

    if (node->type == MAP_ID) {
        return;
    }

#ifdef DEBUG_WIDGET
    logger_print(LOG(DEBUG), "..BEFORE:");
    logger_print(LOG(DEBUG), "....node: %p - %s", node, node->name);
    logger_print(LOG(DEBUG), "....node->env: %p - %s", node->env, node->env ? node->env->name : "NULL");
    logger_print(LOG(DEBUG), "....node->prev: %p - %s, node->next: %p - %s", node->prev, node->prev ? node->prev->name : "NULL", node->next, node->next ? node->next->name : "NULL");
    logger_print(LOG(DEBUG), "....node->inv: %p - %s, node->inv_rev: %p - %s", node->inv, node->inv ? node->inv->name : "NULL", node->inv_rev, node->inv_rev ? node->inv_rev->name : "NULL");
#endif

    /* see if the node has a parent before continuing */
    if (node->env) {
        SetPriorityWidget(node->env);

        /* Strip containers are sorted in a fixed order, and no part of any
         * widget inside should be covered by a sibling.
         * This means we don't need to bother moving the node to the front
         * inside the container. */
        switch (node->env->sub_type) {
            case CONTAINER_STRIP_ID:
            case MENU_ID:
            case MENUITEM_ID:
                return;
        }
    }

    /* now we need to move our other node in front of the first sibling */
    if (!node->prev) {
#ifdef DEBUG_WIDGET
        logger_print(LOG(DEBUG), "..SetPriorityWidget(): Done (Node already at front).");
#endif
        /* no point continuing, node is already at the front */
        return;
    }

    /* Unlink node from its current position in the priority tree. */

    /* node is last sibling, clear the pointer of the previous sibling */
    if (!node->next) {
        /* node also has a parent pointing to it, hand the inv_rev pointer to
         * the previous sibling */
        if (node->env) {
            node->env->inv_rev = node->prev;
        }
        /* no parent, this must be the foot then, so move it to the previous
         * node */
        else {
            widget_list_foot = node->prev;
        }

        node->prev->next = NULL;
    }
    else {
        /* link up the adjacent nodes */
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

    /* Insert node at the head of its container, or make it the root node if it
     * is not in a container. */

    /* Node is now the first sibling so the parent should point to it. */
    if (node->env) {
        node->next = node->env->inv;
        node->env->inv = node;
    }
    /* We are out of containers and this node is about to become the first
     * sibling, which means it's taking the place of the root node. */
    else {
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
    logger_print(LOG(DEBUG), "....node->env: %p - %s", node->env, node->env ? node->env->name : "NULL");
    logger_print(LOG(DEBUG), "....node->prev: %p - %s, node->next: %p - %s", node->prev, node->prev ? node->prev->name : "NULL", node->next, node->next ? node->next->name : "NULL");
    logger_print(LOG(DEBUG), "....node->inv: %p - %s, node->inv_rev: %p - %s", node->inv, node->inv ? node->inv->name : "NULL", node->inv_rev, node->inv_rev ? node->inv_rev->name : "NULL");

    logger_print(LOG(DEBUG), "..SetPriorityWidget(): Done.");
#endif
}

/**
 * Like SetPriorityWidget(), but in reverse.
 * @param node The widget. */
void SetPriorityWidget_reverse(widgetdata *node)
{
    if (!node) {
        return;
    }

    if (!node->next) {
        return;
    }

    if (!node->prev) {
        if (node->env) {
            node->env->inv_rev = node->next;
        }
        else {
            widget_list_head = node->next;
        }

        node->next->prev = NULL;
    }
    else {
        node->next->prev = node->prev;
        node->prev->next = node->next;
    }

    if (node->env) {
        node->prev = node->env->inv;
        node->env->inv = node;
    }
    else {
        node->prev = widget_list_foot;
        widget_list_foot = node;
    }

    node->prev->next = node;
    node->next = NULL;
}

void insert_widget_in_container(widgetdata *widget_container, widgetdata *widget, int absolute)
{
    _widget_container *container;
    _widget_container_strip *container_strip;

    /* sanity checks */
    if (!widget_container || !widget) {
        return;
    }

    /* no, we don't want to end the universe just yet... */
    if (widget_container == widget) {
        return;
    }

    /* is the widget already in a container? */
    if (widget->env) {
        return;
    }

    /* if the widget isn't a container, get out of here */
    if (widget_container->type != CONTAINER_ID) {
        return;
    }

    /* we have our container, now we attempt to place the widget inside it */
    container = CONTAINER(widget_container);

    /* check to see if the widget is compatible with it */
    if (container->widget_type != -1 && container->widget_type != widget->type) {
        return;
    }

    /* if we get here, we now proceed to insert the widget into the container */

    /* snap the widget into the widget container if it is a strip container */
    if (widget_container->inv) {
        switch (widget_container->sub_type) {
            case CONTAINER_STRIP_ID:
            case MENU_ID:
            case MENUITEM_ID:
                container_strip = CONTAINER_STRIP(widget_container);

                /* container is horizontal, insert the widget to the right of
                 * the first widget in its inventory */
                if (container_strip->horizontal) {
                    move_widget_rec(widget, widget_container->inv->x + widget_container->inv->w + container_strip->inner_padding - widget->x, widget_container->y + container->outer_padding_top - widget->y);
                }
                /* otherwise the container is vertical, so insert the widget
                 * below the first child widget */
                else {
                    move_widget_rec(widget, widget_container->x + container->outer_padding_left - widget->x, widget_container->inv->y + widget_container->inv->h + container_strip->inner_padding - widget->y);
                }

                break;
        }
    }
    /* no widgets inside it yet, so snap it to the bounds of the container */
    else if (!absolute) {
        move_widget(widget, widget_container->x + container->outer_padding_left - widget->x, widget_container->y + container->outer_padding_top - widget->y);
    }

    /* link up the adjacent nodes, there *should* be at least two nodes next to
     * each other here so no sanity checks should be required */
    if (!widget->prev) {
        /* widget is no longer the root now, pass it on to the next widget */
        if (widget == widget_list_head) {
            widget_list_head = widget->next;
        }

        widget->next->prev = NULL;
    }
    else if (!widget->next) {
        /* widget is no longer the foot, move it to the previous widget */
        if (widget == widget_list_foot) {
            widget_list_foot = widget->prev;
        }

        widget->prev->next = NULL;
    }
    else {
        widget->prev->next = widget->next;
        widget->next->prev = widget->prev;
    }

    /* the widget to be placed inside will have a new sibling next to it, or
     * NULL if it doesn't exist */
    widget->next = widget_container->inv;
    /* it's also going to be the first child node, so it has no siblings on the
     * other side */
    widget->prev = NULL;

    /* if inventory doesn't exist, set the end child pointer too */
    if (!widget_container->inv) {
        widget_container->inv_rev = widget;
    }
    /* otherwise, link the first child in the inventory to the widget about to
     * be inserted */
    else {
        widget_container->inv->prev = widget;
    }

    /* this new widget becomes the first widget in the container */
    widget_container->inv = widget;
    /* set the environment of the widget inside */
    widget->env = widget_container;

    /* resize the container to fit the new widget. a little dirty trick here,
     * we just resize the widget inside by nothing and it will trigger the
     * auto-resize */
    resize_widget(widget, RESIZE_RIGHT, widget->w);
    resize_widget(widget, RESIZE_BOTTOM, widget->h);
}

/** Get the outermost container the widget is inside. */
widgetdata *get_outermost_container(widgetdata *widget)
{
    widgetdata *tmp = widget;

    /* Sanity check. */
    if (!widget) {
        return NULL;
    }

    /* Get the outsidemost container if the widget is inside one. */
    while (tmp->env) {
        tmp = tmp->env;
        widget = tmp;
    }

    return widget;
}

widgetdata *get_innermost_container(widgetdata *widget)
{
    widgetdata *tmp;

    for (tmp = widget; tmp; tmp = tmp->env) {
        if (tmp->type == CONTAINER_ID) {
            return tmp;
        }
    }

    return widget;
}

/**
 * Find the first widget in the priority list.
 * @param where Where to look for.
 * @param type Widget type to look for. -1 for any type.
 * @param id Identifier to look for. NULL for any identifier.
 * @param surface Surface to look for. NULL for any surface.
 * @return Widget if found, NULL otherwise. */
widgetdata *widget_find(widgetdata *where, int type, const char *id, SDL_Surface *surface)
{
    widgetdata *tmp, *tmp2;

    if (!where) {
        where = widget_list_head;
    }

    for (tmp = where; tmp; tmp = tmp->next) {
        if ((type == -1 || tmp->type == type) && (id == NULL || strcmp(tmp->id, id) == 0) && (surface == NULL || tmp->surface == surface)) {
            return tmp;
        }

        if (tmp->inv) {
            tmp2 = widget_find(tmp->inv, type, id, surface);

            if (tmp2) {
                return tmp2;
            }
        }
    }

    return NULL;
}

widgetdata *widget_find_create_id(int type, const char *id)
{
    widgetdata *tmp;

    tmp = widget_find(NULL, type, id, NULL);

    if (!tmp) {
        tmp = create_widget_object(type);
        tmp->id = estrdup(id);
    }

    return tmp;
}

/* wrapper function to get the outermost container the widget is inside before
 * moving it */
void move_widget(widgetdata *widget, int x, int y)
{
    widget = get_outermost_container(widget);

    move_widget_rec(widget, x, y);
}

/* move all widgets inside the container with the container at the same time */
void move_widget_rec(widgetdata *widget, int x, int y)
{
    /* widget doesn't exist, means the parent node has no children */
    if (!widget) {
        return;
    }

    /* no movement needed */
    if (x == 0 && y == 0) {
        return;
    }

    /* move the widget */
    widget->x += x;
    widget->y += y;

    /* here, we want to walk through the inventory of the widget, if it exists.
     * when we come across a widget, we move it like we did with the container.
     * we loop until we reach the last sibling, but we also need to go recursive
     * if we find a child node */
    for (widget = widget->inv; widget; widget = widget->next) {
        move_widget_rec(widget, x, y);
    }
}

void resize_widget(widgetdata *widget, int side, int offset)
{
    int x = widget->x;
    int y = widget->y;
    int width = widget->w;
    int height = widget->h;

    if (side & RESIZE_LEFT) {
        x = widget->x + widget->w - offset;
        width = offset;
    }
    else if (side & RESIZE_RIGHT) {
        width = offset;
    }

    if (side & RESIZE_TOP) {
        y = widget->y + widget->h - offset;
        height = offset;
    }
    else if (side & RESIZE_BOTTOM) {
        height = offset;
    }

    resize_widget_rec(widget, x, width, y, height);
}

void resize_widget_rec(widgetdata *widget, int x, int width, int y, int height)
{
    widgetdata *widget_container, *tmp, *cmp1, *cmp2, *cmp3, *cmp4;
    _widget_container_strip *container_strip = NULL;
    _widget_container *container = NULL;

    /* move the widget. this is the easy bit, watch as your eyes bleed when you
     * see the next thing we have to do */
    widget->x = x;
    widget->y = y;
    widget->w = width;
    widget->h = height;

    WIDGET_REDRAW(widget);

    /* now we get our parent node if it exists */

    /* loop until we hit the first sibling */
    for (widget_container = widget; widget_container->prev; widget_container = widget_container->prev) {
    }

    /* does the first sibling have a parent node? */
    if (widget_container->env) {
        /* ok, now we need to resize the parent too. but before we do this, we
         * need to see if other widgets inside should prevent it from
         * being resized. the code below is ugly, but necessary in order to
         * calculate the new size of the container. and one more thing...
         * MY EYES! THE GOGGLES DO NOTHING! */

        widget_container = widget_container->env;
        container = CONTAINER(widget_container);

        /* special case for strip containers */
        switch (widget_container->sub_type) {
            case CONTAINER_STRIP_ID:
            case MENU_ID:
            case MENUITEM_ID:
                container_strip = CONTAINER_STRIP(widget_container);

                /* we move all the widgets before or after the widget that got
                 * resized, depending on which side got the resize */
                if (container_strip->horizontal) {
                    /* now move everything we come across */
                    move_widget_rec(widget, 0, widget_container->y + container->outer_padding_top - widget->y);

                    /* every node before the widget we push right */
                    for (tmp = widget->prev; tmp; tmp = tmp->prev) {
                        move_widget_rec(tmp, tmp->next->x + tmp->next->w - tmp->x + container_strip->inner_padding, widget_container->y + container->outer_padding_top - tmp->y);
                    }

                    /* while every node after the widget we push left */
                    for (tmp = widget->next; tmp; tmp = tmp->next) {
                        move_widget_rec(tmp, tmp->prev->x - tmp->x - tmp->w - container_strip->inner_padding, widget_container->y + container->outer_padding_top - tmp->y);
                    }

                    /* we have to set this, otherwise stupid things happen */
                    x = widget_container->inv_rev->x;
                    /* we don't want the container moving up or down in this
                     * case */
                    y = widget_container->y + container->outer_padding_top;
                }
                else {
                    /* now move everything we come across */
                    move_widget_rec(widget, widget_container->x + container->outer_padding_left - widget->x, 0);

                    /* every node before the widget we push downwards */
                    for (tmp = widget->prev; tmp; tmp = tmp->prev) {
                        move_widget_rec(tmp, widget_container->x + container->outer_padding_left - tmp->x, tmp->next->y + tmp->next->h - tmp->y + container_strip->inner_padding);
                    }

                    /* while every node after the widget we push upwards */
                    for (tmp = widget->next; tmp; tmp = tmp->next) {
                        move_widget_rec(tmp, widget_container->x + container->outer_padding_left - tmp->x, tmp->prev->y - tmp->y - tmp->h - container_strip->inner_padding);
                    }

                    /* we don't want the container moving sideways in this case
                     * */
                    x = widget_container->x + container->outer_padding_left;
                    /* we have to set this, otherwise stupid things happen */
                    y = widget_container->inv_rev->y;
                }

                break;
        }

        /* TODO: add the buffer system so that this mess of code will only need
         * to be executed after the user stops resizing the widget */
        cmp1 = cmp2 = cmp3 = cmp4 = widget;

        for (tmp = widget_container->inv; tmp; tmp = tmp->next) {
            /* widget's left x co-ordinate becomes greater than tmp's left x
             * coordinate */
            if (cmp1->x > tmp->x) {
                x = tmp->x;
                width += cmp1->x - tmp->x;
                cmp1 = tmp;
            }

            /* widget's top y co-ordinate becomes greater than tmp's top y
             * coordinate */
            if (cmp2->y > tmp->y) {
                y = tmp->y;
                height += cmp2->y - tmp->y;
                cmp2 = tmp;
            }

            /* widget's right x co-ordinate becomes less than tmp's right x
             * coordinate */
            if (cmp3->x + cmp3->w < tmp->x + tmp->w) {
                width += tmp->x + tmp->w - cmp3->x - cmp3->w;
                cmp3 = tmp;
            }

            /* widget's bottom y co-ordinate becomes less than tmp's bottom y
             * coordinate */
            if (cmp4->y + cmp4->h < tmp->y + tmp->h) {
                height += tmp->y + tmp->h - cmp4->y - cmp4->h;
                cmp4 = tmp;
            }
        }

        x -= container->outer_padding_left;
        y -= container->outer_padding_top;
        width += container->outer_padding_left + container->outer_padding_right;
        height += container->outer_padding_top + container->outer_padding_bottom;

        /* after all that, we now check to see if the parent needs to be resized
         * before we waste even more resources going recursive */
        if (x != widget_container->x || y != widget_container->y || width != widget_container->w || height != widget_container->h) {
            resize_widget_rec(widget_container, x, width, y, height);
        }
    }
}

/** Creates a label with the given text, font and colour, and sets the size of
 * the widget to the correct boundaries. */
widgetdata *add_label(const char *text, font_struct *font, const char *color)
{
    widgetdata *widget;
    _widget_label *label;

    widget = create_widget_object(LABEL_ID);
    label = LABEL(widget);

    label->text = estrdup(text);

    FONT_INCREF(font);
    label->font = font;
    label->color = color;

    resize_widget(widget, RESIZE_RIGHT, text_get_width(font, text, TEXT_MARKUP));
    resize_widget(widget, RESIZE_BOTTOM, text_get_height(font, text, TEXT_MARKUP) + 3);

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

    resize_widget(widget, RESIZE_RIGHT, texture_surface(widget_texture->texture)->w);
    resize_widget(widget, RESIZE_BOTTOM, texture_surface(widget_texture->texture)->h);

    return widget;
}

/** Initializes a menu widget. */
widgetdata *create_menu(int x, int y, widgetdata *owner)
{
    widgetdata *widget_menu = create_widget_object(MENU_ID);
    _widget_container *container_menu = CONTAINER(widget_menu);
    _widget_container_strip *container_strip_menu = CONTAINER_STRIP(widget_menu);

    /* Place the menu at these co-ordinates. */
    widget_menu->x = x;
    widget_menu->y = y;
    /* Point the menu to the owner. */
    (MENU(widget_menu))->owner = owner;
    /* Magic numbers for now, maybe it will be possible in future to customize
     * this in files. */
    container_menu->outer_padding_left = 2;
    container_menu->outer_padding_right = 2;
    container_menu->outer_padding_top = 2;
    container_menu->outer_padding_bottom = 2;
    container_strip_menu->inner_padding = 0;

    return widget_menu;
}

/** Adds a menuitem to a menu. */
void add_menuitem(widgetdata *menu, const char *text, void (*menu_func_ptr)(widgetdata *, widgetdata *, SDL_Event *event), int menu_type, int val)
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

    if (menu_type == MENU_CHECKBOX) {
        widget_texture = add_texture(val ? "checkbox_on" : "checkbox_off");
        insert_widget_in_container(widget_menuitem, widget_texture, 0);
    }
    else if (menu_type == MENU_RADIO) {
        widget_texture = add_texture(val ? "radio_on" : "radio_off");
        insert_widget_in_container(widget_menuitem, widget_texture, 0);
    }

    insert_widget_in_container(widget_menuitem, widget_label, 0);
    insert_widget_in_container(menu, widget_menuitem, 0);

    /* Add the pointer to the function to the menuitem. */
    menuitem = MENUITEM(widget_menuitem);
    menuitem->menu_func_ptr = menu_func_ptr;
    menuitem->menu_type = menu_type;
    menuitem->val = val;

    /* Sanity check. Menuitems should always exist inside a menu. */
    if (widget_menuitem->env && widget_menuitem->env->sub_type == MENU_ID) {
        container_menu = CONTAINER(widget_menuitem->env);

        /* Resize labels in each menuitem to the width of the menu. */
        for (tmp = widget_menuitem; tmp; tmp = tmp->next) {
            if (tmp->inv) {
                container_menuitem = CONTAINER(tmp);

                if (menu_type == MENU_CHECKBOX || menu_type == MENU_RADIO) {
                    resize_widget(tmp->inv, RESIZE_RIGHT, menu->w - tmp->inv_rev->w - container_strip_menuitem->inner_padding - container_menu->outer_padding_left - container_menu->outer_padding_right - container_menuitem->outer_padding_left - container_menuitem->outer_padding_right);
                }
                else {
                    resize_widget(tmp->inv, RESIZE_RIGHT, menu->w - container_menu->outer_padding_left - container_menu->outer_padding_right - container_menuitem->outer_padding_left - container_menuitem->outer_padding_right);
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
    if (widget->x + widget->w > ScreenSurface->w) {
        /* Will appear to the left of the cursor instead of right of it. */
        xoff = -widget->w;

        /* Take submenus into account, and shift them depending on the
         * parent menu's width. */
        if (widget->type_prev && widget->type_prev->sub_type == MENU_ID) {
            xoff += -widget->type_prev->w + 4;
        }
    }

    /* Similar checks for screen height. */
    if (widget->y + widget->h > ScreenSurface->h) {
        /* Submenu, shift it up, so all of it can appear. */
        if (widget->type_prev && widget->type_prev->sub_type == MENU_ID) {
            yoff = ScreenSurface->h - widget->h - widget->y - 1;
        }
        /* Will appear above the cursor. */
        else {
            yoff = -widget->h;
        }
    }

    move_widget(widget, xoff, yoff);
}

/** Redraws all widgets of a particular type. */
void widget_redraw_all(int widget_type_id)
{
    widgetdata *widget;

    for (widget = cur_widget[widget_type_id]; widget; widget = widget->type_next) {
        widget->redraw = 1;
    }
}

void widget_redraw_type_id(int type, const char *id)
{
    widgetdata *widget;

    for (widget = cur_widget[type]; widget; widget = widget->type_next) {
        if (widget->id && strcmp(widget->id, id) == 0) {
            widget->redraw = 1;
        }
    }
}

void menu_move_widget(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    widget_event_start_move(widget);
}

void menu_create_widget(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    widgetdata *tmp;

    tmp = create_widget_object(widget->sub_type);
    tmp->x = menuitem->env->x;
    tmp->y = menuitem->env->y;
    widget_ensure_onscreen(tmp);
}

void menu_remove_widget(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    remove_widget_object(widget);
}

void menu_detach_widget(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    detach_widget(widget);
}

void menu_inv_filter_all(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    inventory_filter_set(INVENTORY_FILTER_ALL);
}

void menu_inv_filter_applied(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    inventory_filter_toggle(INVENTORY_FILTER_APPLIED);
}

void menu_inv_filter_containers(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    inventory_filter_toggle(INVENTORY_FILTER_CONTAINER);
}

void menu_inv_filter_magical(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    inventory_filter_toggle(INVENTORY_FILTER_MAGICAL);
}

void menu_inv_filter_cursed(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    inventory_filter_toggle(INVENTORY_FILTER_CURSED);
}

void menu_inv_filter_unidentified(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    inventory_filter_toggle(INVENTORY_FILTER_UNIDENTIFIED);
}

void menu_inv_filter_locked(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    inventory_filter_toggle(INVENTORY_FILTER_LOCKED);
}

void menu_inv_filter_unapplied(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    inventory_filter_toggle(INVENTORY_FILTER_UNAPPLIED);
}

void menu_inv_filter_submenu(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
}

void menu_inventory_submenu_more(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
}

void menu_inventory_submenu_quickslots(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
}
