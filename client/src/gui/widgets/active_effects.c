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
 * Implements active effects type widgets.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * One active effect.
 */
typedef struct active_effect_struct {
    /**
     * Next active effect in a doubly-linked list.
     */
    struct active_effect_struct *next;

    /**
     * Previous active effect in a doubly-linked list.
     */
    struct active_effect_struct *prev;

    /**
     * Pointer to the actual active effect force object.
     */
    object *op;

    /**
     * Seconds remaining before the effect expires. -1 does not expire.
     */
    int32_t sec;

    /**
     * Explanation of the active effect.
     */
    char *msg;
} active_effect_struct;

/**
 * Active effects widget data. */
typedef struct widget_active_effects_struct {
    active_effect_struct *active_effects;

    uint32_t update_ticks;
} widget_active_effects_struct;

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
    widget_active_effects_struct *tmp;
    active_effect_struct *effect;
    SDL_Rect box;

    tmp = widget->subwidget;

    if (SDL_GetTicks() - tmp->update_ticks > 1000) {
        uint8_t redraw;
        int sec;

        redraw = 0;
        sec = (SDL_GetTicks() - tmp->update_ticks) / 1000;
        tmp->update_ticks = SDL_GetTicks();

        DL_FOREACH(tmp->active_effects, effect)
        {
            if (effect->sec > 0) {
                effect->sec -= sec;

                if (effect->sec < 0) {
                    effect->sec = 0;
                }

                redraw = 1;
            }
        }

        widget->redraw += redraw;
    }

    if (!widget->surface || widget->w != widget->surface->w || widget->h != widget->surface->h) {
        if (widget->surface) {
            SDL_FreeSurface(widget->surface);
        }

        widget->surface = SDL_CreateRGBSurface(get_video_flags(), widget->w, widget->h, video_get_bpp(), 0, 0, 0, 0);
        SDL_SetColorKey(widget->surface, SDL_SRCCOLORKEY | SDL_ANYFORMAT, 0);
    }

    if (widget->redraw) {
        int x, y;
        sprite_struct *sprite;

        x = y = 0;

        SDL_FillRect(widget->surface, NULL, 0);

        DL_FOREACH(tmp->active_effects, effect)
        {
            sprite = FaceList[effect->op->face].sprite;

            if (!sprite) {
                continue;
            }

            if (x + sprite->bitmap->w > widget->w) {
                x = 0;
                y += sprite->bitmap->h + 5;
            }

            if (y + sprite->bitmap->h > widget->h) {
                resize_widget(widget, RESIZE_BOTTOM, y + sprite->bitmap->h);
                widget->redraw++;
            }

            face_show(widget->surface, x, y, effect->op->face);

            if (effect->sec != -1) {
                SDL_Rect textbox;
                char buf[MAX_BUF];

                textbox.w = sprite->bitmap->w;

                if (effect->sec > 60) {
                    snprintf(buf, sizeof(buf), "%d:%02d", effect->sec / 60, effect->sec % 60);
                } else {
                    snprintf(buf, sizeof(buf), "%d", effect->sec);
                }

                text_show(widget->surface, FONT_MONO8, buf, x, y + sprite->bitmap->h - FONT_HEIGHT(FONT_MONO8), COLOR_WHITE, TEXT_OUTLINE | TEXT_ALIGN_CENTER, &textbox);
            }

            x += sprite->bitmap->w + 5;
        }
    }

    box.x = widget->x;
    box.y = widget->y;
    SDL_BlitSurface(widget->surface, NULL, ScreenSurface, &box);
}

/** @copydoc widgetdata::event_func */
static int widget_event(widgetdata *widget, SDL_Event *event)
{
    widget_active_effects_struct *tmp;

    tmp = widget->subwidget;

    if (event->type == SDL_MOUSEMOTION) {
        active_effect_struct *effect;
        int x, y;
        sprite_struct *sprite;

        x = y = 0;

        DL_FOREACH(tmp->active_effects, effect)
        {
            sprite = FaceList[effect->op->face].sprite;

            if (!sprite) {
                continue;
            }

            if (x + sprite->bitmap->w > widget->w) {
                x = 0;
                y += sprite->bitmap->h + 5;
            }

            if (event->motion.x >= widget->x + x && event->motion.x < widget->x + x + sprite->bitmap->w && event->motion.y >= widget->y + y && event->motion.y < widget->y + y + sprite->bitmap->h) {
                char buf[HUGE_BUF];

                snprintf(buf, sizeof(buf), "[b]%s[/b]%s%s", effect->op->s_name, effect->msg[0] != '\0' ? "\n" : "", effect->msg);
                tooltip_create(event->motion.x, event->motion.y, FONT_ARIAL11, buf);
                tooltip_multiline(200);
                break;
            }

            x += sprite->bitmap->w + 5;
        }
    }

    return 0;
}

void widget_active_effects_update(widgetdata *widget, object *op, int32_t sec, const char *msg)
{
    widget_active_effects_struct *tmp;
    active_effect_struct *effect;

    tmp = widget->subwidget;

    if (!(op->flags & CS_FLAG_APPLIED)) {
        return;
    }

    DL_FOREACH(tmp->active_effects, effect)
    {
        if (effect->op == op) {
            break;
        }
    }

    if (!effect) {
        effect = ecalloc(1, sizeof(*effect));
        DL_APPEND(tmp->active_effects, effect);
    } else {
        efree(effect->msg);
    }

    effect->op = op;
    effect->sec = sec;
    effect->msg = estrdup(msg);

    WIDGET_REDRAW(widget);
}

void widget_active_effects_remove(widgetdata *widget, object *op)
{
    widget_active_effects_struct *tmp;
    active_effect_struct *effect, *next;

    tmp = widget->subwidget;

    DL_FOREACH_SAFE(tmp->active_effects, effect, next)
    {
        if (effect->op == op) {
            DL_DELETE(tmp->active_effects, effect);
            efree(effect->msg);
            efree(effect);
            WIDGET_REDRAW(widget);
            break;
        }
    }
}

/**
 * Initialize one active effects widget. */
void widget_active_effects_init(widgetdata *widget)
{
    widget_active_effects_struct *tmp;

    tmp = ecalloc(1, sizeof(*tmp));

    widget->draw_func = widget_draw;
    widget->event_func = widget_event;
    widget->subwidget = tmp;
}
