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
 * Implements player doll type widgets.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Player doll item positions.
 *
 * Used to determine where to put item sprites on the player doll. */
static int player_doll_positions[PLAYER_EQUIP_MAX][2] =
{
    {22, 44},
    {22, 6},
    {22, 82},
    {102, 82},
    {102, 120},
    {22, 158},
    {62, 6},
    {62, 44},
    {62, 82},
    {62, 120},
    {62, 158},
    {102, 6},
    {102, 44},
    {22, 82},
    {22, 120},
    {102, 158}
};

object *playerdoll_get_equipment(int i, int *xpos, int *ypos)
{
    object *obj;

    if (cpl.equipment[i] == 0) {
        return NULL;
    }

    obj = object_find(cpl.equipment[i]);

    if (obj == NULL) {
        return NULL;
    }

    if (i == PLAYER_EQUIP_SHIELD) {
        object *obj2 = NULL;

        if (cpl.equipment[PLAYER_EQUIP_WEAPON_RANGED] != 0) {
            obj2 = object_find(cpl.equipment[PLAYER_EQUIP_WEAPON_RANGED]);
        } else if (cpl.equipment[PLAYER_EQUIP_WEAPON] != 0) {
            obj2 = object_find(cpl.equipment[PLAYER_EQUIP_WEAPON]);
        }

        if (obj2 != NULL && obj2->flags & CS_FLAG_WEAPON_2H) {
            obj = obj2;
        }
    } else if (i == PLAYER_EQUIP_WEAPON_RANGED) {
        if (obj->flags & CS_FLAG_WEAPON_2H) {
            return NULL;
        }
    }

    *xpos = player_doll_positions[i][0] + 2;
    *ypos = player_doll_positions[i][1] + 2;

    return obj;
}

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
    int i, xpos, ypos;
    SDL_Surface *texture_slot_border;
    object *obj;

    if (!widget->redraw) {
        return;
    }

    text_show(widget->surface, FONT_SANS12, "[b]Ranged[/b]", 20, 188, COLOR_HGOLD, TEXT_MARKUP, NULL);
    text_show(widget->surface, FONT_ARIAL10, "DMG", 9, 205, COLOR_HGOLD, 0, NULL);
    text_show_format(widget->surface, FONT_MONO10, 40, 205, COLOR_WHITE, 0, NULL, "%02d", cpl.stats.ranged_dam);
    text_show(widget->surface, FONT_ARIAL10, "WC", 10, 215, COLOR_HGOLD, 0, NULL);
    text_show_format(widget->surface, FONT_MONO10, 40, 215, COLOR_WHITE, 0, NULL, "%02d", cpl.stats.ranged_wc);
    text_show(widget->surface, FONT_ARIAL10, "WS", 10, 225, COLOR_HGOLD, 0, NULL);
    text_show_format(widget->surface, FONT_MONO10, 40, 225, COLOR_WHITE, 0, NULL, "%3.2fs", cpl.stats.ranged_ws / 1000.0);

    text_show(widget->surface, FONT_SANS12, "[b]Melee[/b]", 155, 188, COLOR_HGOLD, TEXT_MARKUP, NULL);
    text_show(widget->surface, FONT_ARIAL10, "DMG", 139, 205, COLOR_HGOLD, 0, NULL);
    text_show_format(widget->surface, FONT_MONO10, 170, 205, COLOR_WHITE, 0, NULL, "%02d", cpl.stats.dam);
    text_show(widget->surface, FONT_ARIAL10, "WC", 140, 215, COLOR_HGOLD, 0, NULL);
    text_show_format(widget->surface, FONT_MONO10, 170, 215, COLOR_WHITE, 0, NULL, "%02d", cpl.stats.wc);
    text_show(widget->surface, FONT_ARIAL10, "WS", 140, 225, COLOR_HGOLD, 0, NULL);
    text_show_format(widget->surface, FONT_MONO10, 170, 225, COLOR_WHITE, 0, NULL, "%3.2fs", cpl.stats.weapon_speed);

    text_show(widget->surface, FONT_ARIAL10, "Speed", 92, 193, COLOR_HGOLD, 0, NULL);
    text_show_format(widget->surface, FONT_MONO10, 93, 205, COLOR_WHITE, 0, NULL, "%3.2f", (float) cpl.stats.speed / FLOAT_MULTF);
    text_show(widget->surface, FONT_ARIAL10, "AC", 92, 215, COLOR_HGOLD, 0, NULL);
    text_show_format(widget->surface, FONT_MONO10, 92, 225, COLOR_WHITE, 0, NULL, "%02d", cpl.stats.ac);

    texture_slot_border = TEXTURE_CLIENT("player_doll_slot_border");

    for (i = 0; i < PLAYER_EQUIP_MAX; i++) {
        rectangle_create(widget->surface, player_doll_positions[i][0], player_doll_positions[i][1], texture_slot_border->w, texture_slot_border->h, PLAYER_DOLL_SLOT_COLOR);
    }

    surface_show(widget->surface, 0, 0, NULL, TEXTURE_CLIENT(cpl.gender == GENDER_FEMALE ? "player_doll_f" : "player_doll"));

    for (i = 0; i < PLAYER_EQUIP_MAX; i++) {
        surface_show(widget->surface, player_doll_positions[i][0], player_doll_positions[i][1], NULL, texture_slot_border);

        obj = playerdoll_get_equipment(i, &xpos, &ypos);

        if (!obj) {
            continue;
        }

        object_show_centered(widget->surface, obj, xpos, ypos);
    }
}

/** @copydoc widgetdata::event_func */
static int widget_event(widgetdata *widget, SDL_Event *event)
{
    if (event->type == SDL_MOUSEMOTION) {
        char buf[HUGE_BUF];
        object *obj;
        int i, xpos, ypos;

        buf[0] = '\0';

        for (i = 0; i < PLAYER_EQUIP_MAX; i++) {
            obj = playerdoll_get_equipment(i, &xpos, &ypos);

            if (!obj) {
                continue;
            }

            if (event->motion.x - widget->x > xpos &&
                    event->motion.x - widget->x <= xpos + INVENTORY_ICON_SIZE &&
                    event->motion.y - widget->y > ypos &&
                    event->motion.y - widget->y <= ypos + INVENTORY_ICON_SIZE) {
                if (buf[0] != '\0') {
                    strncat(buf, "\n", sizeof(buf) - strlen(buf) - 1);
                }

                if (obj->nrof > 1) {
                    snprintfcat(buf, sizeof(buf), "%d %s", obj->nrof,
                            obj->s_name);
                } else {
                    strncat(buf, obj->s_name, sizeof(buf) - strlen(buf) - 1);
                }
            }
        }

        if (buf[0] != '\0') {
            tooltip_create(event->motion.x, event->motion.y, FONT_ARIAL11, buf);
            tooltip_enable_delay(300);
            tooltip_multiline(200);
        }

        return 1;
    }

    return 0;
}

void widget_playerdoll_init(widgetdata *widget)
{
    widget->draw_func = widget_draw;
    widget->event_func = widget_event;
}
