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
 * Implements menu buttons type widgets.
 *
 * @author Alex Tokar
 */

#include <global.h>

/**
 * The different buttons inside the widget.
 */
enum {
    BUTTON_SPELLS, ///< Spells.
    BUTTON_SKILLS, ///< Skills.
    BUTTON_PROTECTIONS, ///< Protections.
    BUTTON_PARTY, ///< Party.
    BUTTON_MPLAYER, ///< Music player.
    BUTTON_BUDDY, ///< Buddy list.
    BUTTON_IGNORE, ///< Ignore list.
    BUTTON_MINIMAP, ///< Minimap.
    BUTTON_MAP, ///< Region map.
    BUTTON_QUEST, ///< Quest list.
    BUTTON_HELP, ///< Help.
    BUTTON_SETTINGS, ///< Esc menu.

    NUM_BUTTONS ///< Total number of the buttons.
} ;

/**
 * Button buffers.
 */
static button_struct buttons[NUM_BUTTONS];
/**
 * Images to render on top of the buttons, NULL for none.
 */
static const char *button_images[NUM_BUTTONS] = {
    "magic", "skill", "protections", "party", "music", "buddy", "ignore",
    "minimap", "map", "quest", NULL, "cogs"
};
/**
 * Tooltip texts for the buttons.
 */
static const char *const button_tooltips[NUM_BUTTONS] = {
    "Spells", "Skills", "Protections", "Party", "Music player", "Buddy List",
    "Ignore List", "Minimap", "Region map", "Quest list", "Help", "Settings"
};
/**
 * Widgets associated with the buttons, -1 for none.
 */
static int button_widgets[NUM_BUTTONS] = {
    SPELLS_ID, SKILLS_ID, PROTECTIONS_ID, PARTY_ID, MPLAYER_ID, BUDDY_ID,
    BUDDY_ID, MINIMAP_ID, -1, -1, -1, -1
};

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
    int i, x, y;
    const char *text;

    if (!widget->redraw) {
        return;
    }

    x = 4;
    y = 3;

    /* Render the buttons. */
    for (i = 0; i < NUM_BUTTONS; i++) {
        if (x > widget->w - 4) {
            x = 4;
            y += texture_surface(buttons[i].texture)->h + 1;
        }

        text = NULL;

        if (button_widgets[i] != -1) {
            widgetdata *tmp;

            if (button_widgets[i] == BUDDY_ID) {
                tmp = widget_find(NULL, button_widgets[i], button_images[i],
                        NULL);
            } else {
                tmp = cur_widget[button_widgets[i]];
            }

            SOFT_ASSERT(tmp != NULL, "Could not find widget type: %d",
                    button_widgets[i]);
            buttons[i].pressed_forced = tmp->show;
        } else if (i == BUTTON_HELP) {
            text = "[y=2]?";
        }

        buttons[i].x = x;
        buttons[i].y = y;
        buttons[i].surface = widget->surface;
        button_set_parent(&buttons[i], widget->x, widget->y);
        button_show(&buttons[i], text);

        if (button_images[i]) {
            char buf[MAX_BUF];

            snprintf(buf, sizeof(buf), "icon_%s", button_images[i]);
            surface_show(widget->surface, x, y, NULL, TEXTURE_CLIENT(buf));
        }

        x += texture_surface(buttons[i].texture)->w + 3;
    }
}

/** @copydoc widgetdata::background_func */
static void widget_background(widgetdata *widget, int draw)
{
    int i;

    /* Figure out whether we need to redraw the widget due to change in
     * the buttons.*/
    for (i = 0; i < NUM_BUTTONS; i++) {
        if (button_need_redraw(&buttons[i])) {
            widget->redraw = 1;
            break;
        }
    }
}

/** @copydoc widgetdata::event_func */
static int widget_event(widgetdata *widget, SDL_Event *event)
{
    int i;

    for (i = 0; i < NUM_BUTTONS; i++) {
        if (!button_event(&buttons[i], event)) {
            if (buttons[i].redraw) {
                widget->redraw = 1;
            }

            if (BUTTON_CHECK_TOOLTIP(&buttons[i])) {
                tooltip_create(event->motion.x, event->motion.y, FONT_ARIAL11,
                        button_tooltips[i]);
                tooltip_enable_delay(300);
            }

            continue;
        }

        widget->redraw = 1;

        /* Toggle visibility of a widget. */
        if (button_widgets[i] != -1) {
            widgetdata *tmp;

            if (button_widgets[i] == BUDDY_ID) {
                tmp = widget_find(NULL, button_widgets[i], button_images[i],
                        NULL);
            } else {
                tmp = cur_widget[button_widgets[i]];
            }

            SOFT_ASSERT_RC(tmp != NULL, 1, "Could not find widget type: %d",
                    button_widgets[i]);
            WIDGET_SHOW_TOGGLE(tmp);
            SetPriorityWidget(tmp);

            if (button_widgets[i] == PARTY_ID && tmp->show) {
                send_command("/party list");
            }

            return 1;
        }

        /* Decide how to handle the button. */
        switch (i) {
        case BUTTON_SETTINGS:
            settings_open();
            break;

        case BUTTON_MAP:
            send_command_check("/region_map");
            break;

        case BUTTON_QUEST:
            keybind_process_command("?QLIST");
            break;

        case BUTTON_HELP:
            help_show("main");
            break;

        default:
            LOG(BUG, "Cannot handle button ID: %d", i);
            break;
        }

        return 1;
    }

    return 0;
}

/** @copydoc widgetdata::deinit_func */
static void widget_deinit(widgetdata *widget)
{
    for (int i = 0; i < NUM_BUTTONS; i++) {
        button_destroy(&buttons[i]);
    }
}

/**
 * Initialize one menu buttons widget.
 */
void widget_menu_buttons_init(widgetdata *widget)
{
    int i;

    for (i = 0; i < NUM_BUTTONS; i++) {
        button_create(&buttons[i]);

        buttons[i].texture = texture_get(TEXTURE_TYPE_CLIENT,
                "button_rect");
        buttons[i].texture_over = texture_get(TEXTURE_TYPE_CLIENT,
                "button_rect_over");
        buttons[i].texture_pressed = texture_get(TEXTURE_TYPE_CLIENT,
                "button_rect_down");
    }

    buttons[BUTTON_HELP].flags |= TEXT_MARKUP;
    button_set_font(&buttons[BUTTON_HELP], FONT_SANS16);

    widget->draw_func = widget_draw;
    widget->background_func = widget_background;
    widget->event_func = widget_event;
    widget->deinit_func = widget_deinit;
}
