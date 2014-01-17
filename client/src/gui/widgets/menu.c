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
 * Implements menu type widgets.
 *
 * @author Alex Tokar
 * @author Daniel Liptrot */

#include <global.h>

/** Handles highlighting of menuitems when the cursor is hovering over them. */
void widget_highlight_menu(widgetdata *widget)
{
    widgetdata *tmp, *tmp2, *tmp3, *submenuwidget = NULL;
    _menu *menu = NULL, *tmp_menu = NULL;
    _menuitem *menuitem = NULL, *submenuitem = NULL;
    int visible, create_submenu = 0, x = 0, y = 0;

    /* Sanity check. Make sure widget is a menu. */
    if (!widget || widget->sub_type != MENU_ID) {
        return;
    }

    /* Check to see if the cursor is hovering over a menuitem or a widget inside
     * it.
     * We don't need to go recursive here, just scan the immediate children. */
    for (tmp = widget->inv; tmp; tmp = tmp->next) {
        visible = 0;

        /* Menuitem is being directly hovered over. Make the background visible
         * for visual feedback. */
        if (tmp == widget_mouse_event.owner) {
            visible = 1;
        }
        /* The cursor is hovering over something the menuitem contains. This
         * only needs to search the direct children,
         * as there should be nothing contained within the children. */
        else if (tmp->inv) {
            for (tmp2 = tmp->inv; tmp2; tmp2 = tmp2->next) {
                if (tmp2 == widget_mouse_event.owner) {
                    /* The cursor was hovering over something inside the
                     * menuitem. */
                    visible = 1;
                    break;
                }
            }
        }

        /* Only do any real working if the state of the menuitem changed. */
        if (tmp->hidden != visible) {
            continue;
        }

        menu = MENU(widget);
        menuitem = MENUITEM(tmp);

        /* Cursor has just started to hover over the menuitem. */
        if (visible) {
            tmp->hidden = 0;

            /* If the highlighted menuitem is a submenu, we need to create a
             * submenu next to the menuitem. */
            if (menuitem->menu_type == MENU_SUBMENU) {
                create_submenu = 1;
                submenuitem = menuitem;
                submenuwidget = tmp;
                x = tmp->x + widget->w - 4;
                y = tmp->y - (CONTAINER(widget))->outer_padding_top;
            }
        }
        /* Cursor no longer hovers over the menuitem. */
        else {
            tmp->hidden = 1;

            /* Let's check if we need to remove the submenu.
             * Don't remove it if the cursor is hovering over the submenu
             * itself,
             * or a submenu of the submenu, etc. */
            if (menuitem->menu_type == MENU_SUBMENU && menu->submenu) {
                /* This will for sure get the menu that the cursor is hovering
                 * over. */
                tmp2 = get_outermost_container(widget_mouse_event.owner);

                /* Just in case the 'for sure' part of the last comment turns
                 * out to be incorrect... */
                if (tmp2 && tmp2->sub_type == MENU_ID) {
                    /* Loop through the submenus to see if we find a match for
                     * the menu the cursor is hovering over. */
                    for (tmp_menu = menu; tmp_menu->submenu && tmp_menu->submenu != tmp2; tmp_menu = MENU(tmp_menu->submenu)) {
                    }

                    /* Remove any submenus related to menu if the menu
                     * underneath the cursor is not a submenu of menu. */
                    if (!tmp_menu->submenu) {
                        tmp2 = menu->submenu;

                        while (tmp2) {
                            tmp3 = (MENU(tmp2))->submenu;
                            remove_widget_object(tmp2);
                            tmp2 = tmp3;
                        }

                        menu->submenu = NULL;
                    }
                    else {
                        /* Cursor is hovering over the submenu, so leave this
                         * menuitem highlighted. */
                        tmp->hidden = 0;
                    }
                }
                /* Cursor is not over a menu, so leave the menuitem containing
                 * the submenu highlighted.
                 * We want to keep the submenu open, which should reduce
                 * annoyance if the user is not precise with the mouse. */
                else {
                    tmp->hidden = 0;
                }
            }
        }
    }

    /* If a submenu needs to be created, create it now. Make sure there can be
     * only one submenu open here. */
    if (create_submenu && !menu->submenu) {
        tmp_menu = MENU(widget);

        tmp_menu->submenu = create_menu(x, y, tmp_menu->owner);

        if (tmp_menu->owner->type == MAIN_INV_ID) {
            if (submenuitem->menu_func_ptr == menu_inv_filter_submenu) {
                add_menuitem(tmp_menu->submenu, "All", &menu_inv_filter_all, MENU_CHECKBOX, inventory_filter == INVENTORY_FILTER_ALL);
                add_menuitem(tmp_menu->submenu, "Applied", &menu_inv_filter_applied, MENU_CHECKBOX, inventory_filter & INVENTORY_FILTER_APPLIED);
                add_menuitem(tmp_menu->submenu, "Unapplied", &menu_inv_filter_unapplied, MENU_CHECKBOX, inventory_filter & INVENTORY_FILTER_UNAPPLIED);
                add_menuitem(tmp_menu->submenu, "Containers", &menu_inv_filter_containers, MENU_CHECKBOX, inventory_filter & INVENTORY_FILTER_CONTAINER);
                add_menuitem(tmp_menu->submenu, "Magical", &menu_inv_filter_magical, MENU_CHECKBOX, inventory_filter & INVENTORY_FILTER_MAGICAL);
                add_menuitem(tmp_menu->submenu, "Cursed", &menu_inv_filter_cursed, MENU_CHECKBOX, inventory_filter & INVENTORY_FILTER_CURSED);
                add_menuitem(tmp_menu->submenu, "Unidentified", &menu_inv_filter_unidentified, MENU_CHECKBOX, inventory_filter & INVENTORY_FILTER_UNIDENTIFIED);
                add_menuitem(tmp_menu->submenu, "Locked", &menu_inv_filter_locked, MENU_CHECKBOX, inventory_filter & INVENTORY_FILTER_LOCKED);
            }
            else if (submenuitem->menu_func_ptr == menu_inventory_submenu_more) {
                add_menuitem(tmp_menu->submenu, "Drop all", &menu_inventory_dropall, MENU_NORMAL, 0);
                add_menuitem(tmp_menu->submenu, "Ready", &menu_inventory_ready, MENU_NORMAL, 0);
                add_menuitem(tmp_menu->submenu, "Mark", &menu_inventory_mark, MENU_NORMAL, 0);
                add_menuitem(tmp_menu->submenu, "Lock", &menu_inventory_lock, MENU_NORMAL, 0);
                add_menuitem(tmp_menu->submenu, "Drag", &menu_inventory_drag, MENU_NORMAL, 0);
            }
        }

        if (submenuitem->menu_func_ptr) {
            submenuitem->menu_func_ptr(tmp_menu->owner, submenuwidget, NULL);
        }

        menu_finalize(tmp_menu->submenu);
    }
}
