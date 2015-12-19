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
 * Implements target type widgets.
 *
 * @author Alex Tokar
 */

#include <global.h>

/**
 * Target widget data structure.
 */
typedef struct target_widget {
    button_struct button_talk; ///< The 'hello' button.
    button_struct button_combat; ///< The combat toggle button.
} target_widget_t;

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
    if (!widget->redraw) {
        return;
    }

    const char *str;

    if (cpl.target_code == CMD_TARGET_SELF) {
        if (cpl.combat) {
            str = "target self (hold attack)";
        } else {
            str = "target self";
        }
    } else if (cpl.target_code == CMD_TARGET_ENEMY) {
        if (cpl.combat) {
            str = "target and attack enemy";
        } else {
            str = "target enemy";
        }
    } else if (cpl.target_code == CMD_TARGET_NEUTRAL ||
            cpl.target_code == CMD_TARGET_FRIEND) {
        if (cpl.combat) {
            if (cpl.combat_force && cpl.target_code != CMD_TARGET_FRIEND) {
                str = "target and attack friend";
            } else {
                str = "target friend (hold attack)";
            }
        } else {
            str = "target friend";
        }
    } else {
        LOG(ERROR, "Invalid target code: %d", cpl.target_code);
        str = "invalid target";
    }

    target_widget_t *target_widget = widget->subwidget;

    target_widget->button_combat.texture =
            target_widget->button_combat.texture_over =
            target_widget->button_combat.texture_pressed =
            texture_get(TEXTURE_TYPE_CLIENT,
                        cpl.combat ? "target_attack" : "target_normal");

    target_widget->button_combat.surface = widget->surface;
    button_set_parent(&target_widget->button_combat, widget->x, widget->y);
    target_widget->button_combat.x = 5;
    target_widget->button_combat.y = widget->h / 2 -
            texture_surface(target_widget->button_combat.texture)->h / 2;
    button_show(&target_widget->button_combat, "");

    int x = target_widget->button_combat.x;
    int y = target_widget->button_combat.y +
            texture_surface(target_widget->button_combat.texture)->h - 3;
    surface_show(widget->surface, x, y, NULL, TEXTURE_CLIENT("target_hp_b"));

    if (cpl.target_code != CMD_TARGET_SELF) {
        target_widget->button_talk.surface = widget->surface;
        target_widget->button_talk.x = widget->w -
                texture_surface(target_widget->button_talk.texture)->w - 5;
        target_widget->button_talk.y = widget->h / 2 -
                texture_surface(target_widget->button_talk.texture)->h / 2;
        button_set_parent(&target_widget->button_talk, widget->x, widget->y);
        button_show(&target_widget->button_talk, "");
    }

    if (setting_get_int(OPT_CAT_GENERAL, OPT_TARGET_SELF) ||
            cpl.target_code != 0) {
        int hp = cpl.target_hp;

        if (cpl.target_code == CMD_TARGET_SELF) {
            hp = (int) (((double) cpl.stats.hp / (double) cpl.stats.maxhp) *
                    100.0);
        } else {
            hp = MIN(100, MAX(0, hp));
        }

        SDL_Surface *target_hp = TEXTURE_CLIENT("target_hp");

        SDL_Rect box;
        box.x = 0;
        box.y = 0;
        box.h = target_hp->h;
        box.w = (Uint16) (target_hp->w * ((double) hp * 0.01));
        surface_show(widget->surface, x + 1, y + 1, &box, target_hp);

        box.x = texture_surface(target_widget->button_combat.texture)->w +
                5 * 2;
        box.y = 1;
        box.h = widget->h - 1 * 2;
        box.w = widget->w - box.x - 5 * 2 -
                texture_surface(target_widget->button_talk.texture)->w;

        const char *hp_color;

        if (hp > 90) {
            hp_color = COLOR_GREEN;
        } else if (hp > 75) {
            hp_color = COLOR_DGOLD;
        } else if (hp > 50) {
            hp_color = COLOR_HGOLD;
        } else if (hp > 25) {
            hp_color = COLOR_ORANGE;
        } else if (hp > 10) {
            hp_color = COLOR_YELLOW;
        } else {
            hp_color = COLOR_RED;
        }

        text_show_format(widget->surface, FONT_ARIAL11, box.x, box.y,
                         cpl.target_color, TEXT_MARKUP | TEXT_VALIGN_CENTER,
                         &box, "%s\n[c=#%s]HP: %d%%[/c] %s", cpl.target_name,
                         hp_color, hp, str);
    }
}

/** @copydoc widgetdata::event_func */
static int widget_event(widgetdata *widget, SDL_Event *event)
{
    target_widget_t *target_widget = widget->subwidget;

    if (button_event(&target_widget->button_combat, event)) {
        keybind_process_command("?COMBAT");
        return 1;
    }

    if (cpl.target_code != CMD_TARGET_SELF) {
        if (button_event(&target_widget->button_talk, event)) {
            keybind_process_command("?HELLO");
            return 1;
        }
    }

    return 0;
}

/** @copydoc widgetdata::deinit_func */
static void widget_deinit(widgetdata *widget)
{
    target_widget_t *target_widget = widget->subwidget;
    button_destroy(&target_widget->button_talk);
    button_destroy(&target_widget->button_combat);
}

/**
 * Initialize the target type widget.
 * @param widget
 * Widget.
 */
void widget_target_init(widgetdata *widget)
{
    target_widget_t *target_widget;

    widget->draw_func = widget_draw;
    widget->event_func = widget_event;
    widget->deinit_func = widget_deinit;
    target_widget = widget->subwidget = ecalloc(1, sizeof(*target_widget));
    button_create(&target_widget->button_talk);
    button_create(&target_widget->button_combat);
    target_widget->button_talk.texture =
            target_widget->button_talk.texture_over =
            target_widget->button_talk.texture_pressed =
            texture_get(TEXTURE_TYPE_CLIENT, "target_talk");
}