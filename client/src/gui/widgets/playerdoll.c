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
 * @author Alex Tokar
 */

#include <global.h>

/**
 * Player doll item positions.
 *
 * Used to determine where to put item sprites on the player doll.
 */
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

/**
 * Text used in the player doll.
 */
static const char *player_doll_text =
        "[center][font=sans 12][b]Statistics[/b][/font][/center]\n"
        "Strength[c=#ffffff][right][font=mono]%02d[/font][/right][/c]\n"
        "Dexterity[c=#ffffff][right][font=mono]%02d[/font][/right][/c]\n"
        "Constitution[c=#ffffff][right][font=mono]%02d[/font][/right][/c]\n"
        "Intelligence[c=#ffffff][right][font=mono]%02d[/font][/right][/c]\n"
        "Power[c=#ffffff][right][font=mono]%02d[/font][/right][/c]\n"
        "Speed[c=#ffffff][right][font=mono]%3.2f[/font][/right][/c]\n"
        "Armour class[c=#ffffff][right][font=mono]%02d[/font][/right][/c]\n"
        "[y=4][center][font=sans 12][b]Melee[/b][/font][/center]\n"
        "Damage[c=#ffffff][right][font=mono]%02d[/font][/right][/c]\n"
        "Weapon class[c=#ffffff][right][font=mono]%02d[/font][/right][/c]\n"
        "Weapon speed[c=#ffffff][right][font=mono]%3.2fs[/font][/right][/c]\n"
        "[y=4][center][font=sans 12][b]Ranged[/b][/font][/center]\n"
        "Damage[c=#ffffff][right][font=mono]%02d[/font][/right][/c]\n"
        "Weapon class[c=#ffffff][right][font=mono]%02d[/font][/right][/c]\n"
        "Weapon speed[c=#ffffff][right][font=mono]%3.2fs[/font][/right][/c]\n";

/**
 * Same as above, except with abbreviations to conserve horizontal space.
 */
static const char *player_doll_text_abbr =
        "[center][font=sans 12][b]Stats[/b][/font][/center]\n"
        "Str[c=#ffffff][right][font=mono]%02d[/font][/right][/c]\n"
        "Dex[c=#ffffff][right][font=mono]%02d[/font][/right][/c]\n"
        "Con[c=#ffffff][right][font=mono]%02d[/font][/right][/c]\n"
        "Int[c=#ffffff][right][font=mono]%02d[/font][/right][/c]\n"
        "Pow[c=#ffffff][right][font=mono]%02d[/font][/right][/c]\n"
        "Speed[c=#ffffff][right][font=mono]%3.2f[/font][/right][/c]\n"
        "AC[c=#ffffff][right][font=mono]%02d[/font][/right][/c]\n"
        "[y=4][center][font=sans 12][b]Melee[/b][/font][/center]\n"
        "DMG[c=#ffffff][right][font=mono]%02d[/font][/right][/c]\n"
        "WC[c=#ffffff][right][font=mono]%02d[/font][/right][/c]\n"
        "WS[c=#ffffff][right][font=mono]%3.2fs[/font][/right][/c]\n"
        "[y=4][center][font=sans 12][b]Ranged[/b][/font][/center]\n"
        "DMG[c=#ffffff][right][font=mono]%02d[/font][/right][/c]\n"
        "WC[c=#ffffff][right][font=mono]%02d[/font][/right][/c]\n"
        "WS[c=#ffffff][right][font=mono]%3.2fs[/font][/right][/c]\n";

#define PLAYER_DOLL_TEXT_RENDER(flags, box) \
    text_show_format(widget->surface, FONT_ARIAL10, 10, 10, COLOR_HGOLD, \
            TEXT_MARKUP | flags, box, text, \
            cpl.stats.Str, cpl.stats.Dex, cpl.stats.Con, \
            cpl.stats.Int, cpl.stats.Pow, \
            (double) cpl.stats.speed / FLOAT_MULTF, cpl.stats.ac, \
            cpl.stats.dam, cpl.stats.wc, cpl.stats.weapon_speed, \
            cpl.stats.ranged_dam, cpl.stats.ranged_wc, \
            cpl.stats.ranged_ws / 1000.0);

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
    int i, xpos, ypos, xoff, yoff;
    SDL_Surface *texture_slot_border, *texture;
    object *obj;
    SDL_Rect box, box2;
    const char *text;

    if (!widget->redraw) {
        return;
    }

    if (cpl.gender == GENDER_FEMALE) {
        texture = TEXTURE_CLIENT("player_doll_f");
    } else {
        texture = TEXTURE_CLIENT("player_doll");
    }

    xoff = widget->w - texture->w + 10;
    yoff = widget->h / 2 - texture->h / 2;

    box.w = xoff - 10;
    box.h = widget->h - 10 * 2;

    text = player_doll_text;

    PLAYER_DOLL_TEXT_RENDER(TEXT_MAX_WIDTH, &box2);

    if (box2.w > box.w) {
        text = player_doll_text_abbr;
    }

    PLAYER_DOLL_TEXT_RENDER(0, &box);

    texture_slot_border = TEXTURE_CLIENT("player_doll_slot_border");

    for (i = 0; i < PLAYER_EQUIP_MAX; i++) {
        rectangle_create(widget->surface, player_doll_positions[i][0] + xoff,
                player_doll_positions[i][1] + yoff, texture_slot_border->w,
                texture_slot_border->h, PLAYER_DOLL_SLOT_COLOR);
    }

    surface_show(widget->surface, xoff, yoff, NULL, texture);

    for (i = 0; i < PLAYER_EQUIP_MAX; i++) {
        surface_show(widget->surface, player_doll_positions[i][0] + xoff,
                player_doll_positions[i][1] + yoff, NULL, texture_slot_border);

        obj = playerdoll_get_equipment(i, &xpos, &ypos);

        if (!obj) {
            continue;
        }

        object_show_centered(widget->surface, obj, xpos + xoff, ypos + yoff);
    }
}

/** @copydoc widgetdata::event_func */
static int widget_event(widgetdata *widget, SDL_Event *event)
{
    if (event->type == SDL_MOUSEMOTION) {
        char buf[HUGE_BUF];
        object *obj;
        int i, xpos, ypos, xoff, yoff;

        buf[0] = '\0';
        xoff = widget->w - TEXTURE_CLIENT("player_doll")->w + 10;
        yoff = widget->h / 2 - TEXTURE_CLIENT("player_doll")->h / 2;

        for (i = 0; i < PLAYER_EQUIP_MAX; i++) {
            obj = playerdoll_get_equipment(i, &xpos, &ypos);

            if (obj == NULL) {
                continue;
            }

            xpos += xoff;
            ypos += yoff;

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
