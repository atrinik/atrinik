/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team     *
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
 * Implements inventory type widgets.
 *
 * @author Alex Tokar
 */

#include <global.h>

/**
 * Active inventory filter, one of @ref INVENTORY_FILTER_xxx.
 */
uint64 inventory_filter = INVENTORY_FILTER_ALL;

/**
 * String representations of the possible inventory filters.
 */
const char *inventory_filter_names[INVENTORY_FILTER_MAX] = {
    "applied", "container", "magical", "cursed", "unidentified", "unapplied",
    "locked"
};

/**
 * Check if an object matches one of the active inventory filters.
 * @param op Object to check.
 * @return 1 if there is a match, 0 otherwise.
 */
static int inventory_matches_filter(object *op)
{
    /* No filtering of objects in the below inventory. */
    if (op->env == cpl.below) {
        return 1;
    }

    /* Never show spell/skill/force objects in the inventory. */
    if (op->itype == TYPE_SPELL || op->itype == TYPE_SKILL ||
            op->itype == TYPE_FORCE || op->itype == TYPE_POISONING) {
        return 0;
    }

    /* Always show open container, and the items inside. */
    if (cpl.container_tag == op->tag || (op->env && op->env->env)) {
        return 1;
    }

    if (inventory_filter == INVENTORY_FILTER_ALL) {
        return 1;
    }

    if (inventory_filter & INVENTORY_FILTER_APPLIED &&
            op->flags & CS_FLAG_APPLIED) {
        return 1;
    }

    if (inventory_filter & INVENTORY_FILTER_CONTAINER &&
            op->itype == TYPE_CONTAINER) {
        return 1;
    }

    if (inventory_filter & INVENTORY_FILTER_MAGICAL &&
            op->flags & CS_FLAG_IS_MAGICAL) {
        return 1;
    }

    if (inventory_filter & INVENTORY_FILTER_CURSED &&
            op->flags & (CS_FLAG_CURSED | CS_FLAG_DAMNED)) {
        return 1;
    }

    if (inventory_filter & INVENTORY_FILTER_UNIDENTIFIED &&
            op->item_qua == 255) {
        return 1;
    }

    if (inventory_filter & INVENTORY_FILTER_UNAPPLIED &&
            !(op->flags & CS_FLAG_APPLIED)) {
        return 1;
    }

    if (inventory_filter & INVENTORY_FILTER_LOCKED &&
            op->flags & CS_FLAG_LOCKED) {
        return 1;
    }

    return 0;
}

/**
 * Set an inventory filter to the passed value.
 * @param filter The value to set.
 */
void inventory_filter_set(uint64 filter)
{
    inventory_filter = filter;
    widget_inventory_handle_arrow_key(cur_widget[MAIN_INV_ID], SDLK_UNKNOWN);
    cur_widget[MAIN_INV_ID]->redraw = 1;
    draw_info(COLOR_GREEN, "Inventory filter changed.");
}

/**
 * Toggle one inventory filter.
 * @param filter Filter to toggle.
 */
void inventory_filter_toggle(uint64 filter)
{
    if (inventory_filter & filter) {
        inventory_filter &= ~filter;
    }
    else {
        inventory_filter |= filter;
    }

    widget_inventory_handle_arrow_key(cur_widget[MAIN_INV_ID], SDLK_UNKNOWN);
    cur_widget[MAIN_INV_ID]->redraw = 1;
    draw_info(COLOR_GREEN, "Inventory filter changed.");
}

/**
 * Set one or more filters.
 * @param filter Filter(s) to toggle.
 */
void inventory_filter_set_names(const char *filter)
{
    char word[MAX_BUF];
    size_t i, pos;

    inventory_filter = INVENTORY_FILTER_ALL;
    pos = 0;

    while (string_get_word(filter, &pos, ' ', word, sizeof(word), 0)) {
        for (i = 0; i < INVENTORY_FILTER_MAX; i++) {
            if (strcmp(inventory_filter_names[i], word) == 0) {
                inventory_filter |= 1 << i;
                break;
            }
        }
    }

    widget_inventory_handle_arrow_key(cur_widget[MAIN_INV_ID], SDLK_UNKNOWN);
    cur_widget[MAIN_INV_ID]->redraw = 1;
    draw_info(COLOR_GREEN, "Inventory filter changed.");
}

/**
 * Render a single object in the inventory widget.
 *
 * If 'mx' and 'my' are not -1, no rendering is done and instead the
 * return value indicates whether the mx/my coordinates are over the
 * object.
 * @param widget The widget.
 * @param ob Object to render.
 * @param i Integer index of the object in the linked list.
 * @param[out] r Rendering index of the object.
 * @param mx Mouse X. Can be -1.
 * @param my Mouse Y. Can be -1.
 * @return 1 if the object was rendered, 0 otherwise.
 */
static int inventory_render_object(widgetdata *widget, object *ob, uint32 i,
        uint32 *r, int mx, int my)
{
    inventory_struct *inventory;
    uint32 row, r_row, r_col;
    int x, y;
    char buf[HUGE_BUF];
    SDL_Rect box;

    inventory = INVENTORY(widget);
    row = i / INVENTORY_COLS(inventory);

    if (mx != -1 && my != -1) {
        mx -= widget->x;
        my -= widget->y;
    }

    /* Check if this object should be visible. */
    if (row < inventory->scrollbar_info.scroll_offset ||
            row >= inventory->scrollbar_info.scroll_offset +
            INVENTORY_ROWS(inventory)) {
        return 0;
    }

    /* Calculate the row and column to render on. */
    r_row = *r / INVENTORY_COLS(inventory);
    r_col = *r % INVENTORY_COLS(inventory);

    /* Calculate the X/Y positions. */
    x = inventory->x + r_col * INVENTORY_ICON_SIZE;
    y = inventory->y + r_row * INVENTORY_ICON_SIZE;

    /* Increase the rendering index. */
    *r += 1;

    /* If 'mx' and 'my' are not -1, do not render, just check if the
     * provided coordinates are over the object. */
    if (mx != -1 && my != -1) {
        if (mx >= x && mx < x + INVENTORY_ICON_SIZE &&
                my >= y && my < y + INVENTORY_ICON_SIZE) {
            return 1;
        } else {
            return 0;
        }
    }

    object_show_inventory(widget->surface, ob, x, y);

    /* If this object is selected, show the selected graphic. */
    if (i == inventory->selected) {
        surface_show(widget->surface, x, y, NULL, TEXTURE_CLIENT(
                cpl.inventory_focus == widget->type ? "invslot" : "invslot_u"));
    }

    /* If the object is marked, show that. */
    if (ob->tag == cpl.mark_count) {
        surface_show(widget->surface, x, y, NULL,
                TEXTURE_CLIENT("invslot_marked"));
    }

    /* If it's the currently open container, add the 'container
     * start' graphic. */
    if (ob->tag == cpl.container_tag) {
        surface_show(widget->surface, x, y, NULL,
                TEXTURE_CLIENT("cmark_start"));
    }
    /* Object inside the open container... */
    else if (ob->env == cpl.sack) {
        /* If there is still something more in the container, show the
         * 'object in the middle of container' graphic. */
        if (ob->next) {
            surface_show(widget->surface, x, y, NULL,
                    TEXTURE_CLIENT("cmark_middle"));
        }
        /* The end, show the 'end of container' graphic instead. */
        else {
            surface_show(widget->surface, x, y, NULL,
                    TEXTURE_CLIENT("cmark_end"));
        }
    }

    /* Only show extra information for the selected object. */
    if (i != inventory->selected) {
        return 1;
    }

    snprintf(buf, sizeof(buf), "[center][b]");

    /* Construct the name */
    if (ob->nrof > 1) {
        snprintfcat(buf, sizeof(buf), "%d %s", ob->nrof, ob->s_name);
    } else {
        snprintfcat(buf, sizeof(buf), "%s", ob->s_name);
    }

    snprintfcat(buf, sizeof(buf), "[/b][/center]\n");

    /* Extra information for items in the player's inventory */
    if (widget->type == MAIN_INV_ID) {
        char filter[MAX_BUF];

        /* Item quality of 255 marks unidentified items */
        if (ob->item_qua == 255) {
            snprintfcat(buf, sizeof(buf), "[red]not identified[/red]");
        } else {
            snprintfcat(buf, sizeof(buf), "Con: %d/%d", ob->item_con,
                    ob->item_qua);

            /* Show item's level and required skill */
            if (ob->item_level) {
                object *skill;
                size_t skill_id;
                int level;
                char buf2[MAX_BUF];

                if (ob->item_skill_tag &&
                        (skill = object_find(ob->item_skill_tag)) &&
                        skill_find_object(skill, &skill_id)) {
                    level = skill_get(skill_id)->level;
                    snprintf(buf2, sizeof(buf2), "level %d %s",
                            ob->item_level, skill->s_name);
                }
                else {
                    level = cpl.stats.level;
                    snprintf(buf2, sizeof(buf2), "level %d",
                            ob->item_level);
                }

                /* If the player or the player's skill level is too low,
                 * show the required level in red to indicate that. */
                if (ob->item_level > level) {
                    snprintfcat(buf, sizeof(buf), " [red]%s[/red]",
                            buf2);
                } else {
                    snprintfcat(buf, sizeof(buf), " %s", buf2);
                }
            }
        }

        /* Item's weight */
        snprintfcat(buf, sizeof(buf), " [right]%4.3f kg[/right]\n",
                ob->weight * (double) ob->nrof);

        /* No active filter, show "all". */
        if (inventory_filter == INVENTORY_FILTER_ALL) {
            snprintf(filter, sizeof(filter), "all");
        } else {
            size_t filter_num;

            *filter = '\0';

            /* Construct a string of active filters. Only the first active
             * filter will be shown, and if there are any more active filters,
             * ellipsis will be appended. */
            for (filter_num = 0; filter_num < INVENTORY_FILTER_MAX;
                    filter_num++) {
                if (inventory_filter & (1 << filter_num)) {
                    if (*filter == '\0') {
                        snprintf(filter, sizeof(filter), "%s",
                                inventory_filter_names[filter_num]);
                    } else {
                        snprintfcat(filter, sizeof(filter), ", ...");
                        break;
                    }
                }
            }
        }

        /* Append the active filter(s) and carrying capacity of the player */
        snprintfcat(buf, sizeof(buf),
                "Showing: %s [right]Carrying: %4.3f/%4.3f kg[/right]", filter,
                cpl.real_weight, cpl.weight_limit);
    }

    box.w = widget->w - 4 * 2;
    box.h = widget->h - inventory->h - 2 * 2;

    text_show(widget->surface, FONT_ARIAL11, buf, 4, 2, COLOR_HGOLD,
            TEXT_MARKUP, &box);

    return 1;
}

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
    inventory_struct *inventory;
    int w, h;
    object *tmp, *tmp2;
    uint32 i, r;

    if (!widget->redraw) {
        return;
    }

    inventory = INVENTORY(widget);

    w = MAX(widget->w - inventory->x * 2 - 9, INVENTORY_ICON_SIZE);
    h = MAX(widget->h - inventory->y - inventory->x, INVENTORY_ICON_SIZE);

    if (inventory->w != w || inventory->h != h) {
        char buf[MAX_BUF];

        inventory->w = w;
        inventory->h = h;

        scrollbar_create(&inventory->scrollbar, 9, inventory->h,
                &inventory->scrollbar_info.scroll_offset,
                &inventory->scrollbar_info.num_lines,
                INVENTORY_ROWS(inventory));
        inventory->scrollbar.redraw = &inventory->scrollbar_info.redraw;

        texture_delete(inventory->texture);
        snprintf(buf, sizeof(buf),
                "rectangle:%d,%d;[bar=inventory_bg][border=widget_border]",
                inventory->w + 1 * 2 + inventory->scrollbar.background.w,
                inventory->h + 1 * 2);
        inventory->texture = texture_get(TEXTURE_TYPE_SOFTWARE, buf);
    }

    if (widget->type == MAIN_INV_ID) {
        /* Recalculate the weight, as it may have changed. */
        cpl.real_weight = 0.0;

        for (tmp = INVENTORY_WHERE(widget)->inv; tmp; tmp = tmp->next) {
            if (!inventory_matches_filter(tmp)) {
                continue;
            }

            cpl.real_weight += tmp->weight * (double) tmp->nrof;
        }

        surface_show(widget->surface, inventory->x - 1, inventory->y - 1, NULL,
                texture_surface(inventory->texture));
    }
    else if (widget->type == BELOW_INV_ID) {
        surface_show(widget->surface, inventory->x - 1, inventory->y - 1, NULL,
                texture_surface(inventory->texture));
    }

    widget_inventory_handle_arrow_key(widget, SDLK_UNKNOWN);

    for (i = 0, r = 0, tmp = INVENTORY_WHERE(widget)->inv; tmp;
            tmp = tmp->next) {
        if (!inventory_matches_filter(tmp)) {
            continue;
        }

        inventory_render_object(widget, tmp, i, &r, -1, -1);
        i++;

        if (cpl.container_tag == tmp->tag) {
            for (tmp2 = cpl.sack->inv; tmp2; tmp2 = tmp2->next) {
                if (!inventory_matches_filter(tmp2)) {
                    continue;
                }

                inventory_render_object(widget, tmp2, i, &r, -1, -1);
                i++;
            }
        }
    }

    inventory->scrollbar.px = widget->x;
    inventory->scrollbar.py = widget->y;
    scrollbar_show(&inventory->scrollbar, widget->surface,
            inventory->x + inventory->w, inventory->y);
}

/** @copydoc widgetdata::event_func */
static int widget_event(widgetdata *widget, SDL_Event *event)
{
    inventory_struct *inventory;

    inventory = INVENTORY(widget);

    if (scrollbar_event(&inventory->scrollbar, event)) {
        widget->redraw = 1;

        if (inventory->scrollbar_info.redraw) {
            inventory->selected = *inventory->scrollbar.scroll_offset *
                    INVENTORY_COLS(inventory);
            inventory->scrollbar_info.redraw = 0;
        }

        return 1;
    }

    if (event->type == SDL_MOUSEBUTTONDOWN) {
        if (event->button.button == SDL_BUTTON_WHEELUP) {
            widget_inventory_handle_arrow_key(widget, SDLK_LEFT);
            return 1;
        } else if (event->button.button == SDL_BUTTON_WHEELDOWN) {
            widget_inventory_handle_arrow_key(widget, SDLK_RIGHT);
            return 1;
        } else if (event->button.button == SDL_BUTTON_LEFT ||
                event->button.button == SDL_BUTTON_RIGHT) {
            uint32 i, r;
            object *tmp, *tmp2;
            uint8 found = 0;

            for (i = 0, r = 0, tmp = INVENTORY_WHERE(widget)->inv;
                    tmp && !found; tmp = tmp->next) {
                if (!inventory_matches_filter(tmp)) {
                    continue;
                }

                if (inventory_render_object(widget, tmp, i, &r,
                        event->motion.x, event->motion.y)) {
                    found = 1;
                    break;
                }

                i++;

                if (cpl.container_tag == tmp->tag) {
                    for (tmp2 = cpl.sack->inv; tmp2; tmp2 = tmp2->next) {
                        if (!inventory_matches_filter(tmp2)) {
                            continue;
                        }

                        if (inventory_render_object(widget, tmp2, i, &r,
                                event->motion.x, event->motion.y)) {
                            found = 1;
                            break;
                        }

                        i++;
                    }
                }
            }

            if (found) {
                inventory->selected = i;
                widget->redraw = 1;

                if (event->button.button == SDL_BUTTON_LEFT) {
                    keybind_process_command("?APPLY");
                }

                return 1;
            }
        }
    }

    return 0;
}

/**
 * Initialize one inventory widget.
 */
void widget_inventory_init(widgetdata *widget)
{
    inventory_struct *inventory;

    inventory = ecalloc(1, sizeof(*inventory));
    scrollbar_info_create(&inventory->scrollbar_info);

    if (widget->type == MAIN_INV_ID) {
        inventory->x = 3;
        inventory->y = 44;
    } else if (widget->type == BELOW_INV_ID) {
        inventory->x = 5;
        inventory->y = 19;
    }

    widget->draw_func = widget_draw;
    widget->event_func = widget_event;
    widget->subwidget = inventory;
}

/**
 * Calculate number of items in the inventory widget.
 * @param widget The widget.
 * @return Number of items in the inventory widget.
 */
uint32 widget_inventory_num_items(widgetdata *widget)
{
    uint32 i;
    object *tmp, *tmp2;

    for (i = 0, tmp = INVENTORY_WHERE(widget)->inv; tmp; tmp = tmp->next) {
        if (!inventory_matches_filter(tmp)) {
            continue;
        }

        i++;

        if (cpl.container_tag == tmp->tag) {
            for (tmp2 = cpl.sack->inv; tmp2; tmp2 = tmp2->next) {
                if (!inventory_matches_filter(tmp2)) {
                    continue;
                }

                i++;
            }
        }
    }

    return i;
}

/**
 * Get the selected object from the inventory widget.
 * @param widget The inventory object.
 * @return The selected object, if any.
 */
object *widget_inventory_get_selected(widgetdata *widget)
{
    inventory_struct *inventory;
    uint32 i;
    object *tmp, *tmp2;

    inventory = INVENTORY(widget);

    for (i = 0, tmp = INVENTORY_WHERE(widget)->inv; tmp; tmp = tmp->next) {
        if (!inventory_matches_filter(tmp)) {
            continue;
        }

        if (i == inventory->selected) {
            return tmp;
        }

        i++;

        if (cpl.container_tag == tmp->tag) {
            for (tmp2 = cpl.sack->inv; tmp2; tmp2 = tmp2->next) {
                if (!inventory_matches_filter(tmp2)) {
                    continue;
                }

                if (i == inventory->selected) {
                    return tmp2;
                }

                i++;
            }
        }
    }

    return NULL;
}

/**
 * Handle the arrow keys in the inventory widget.
 * @param widget The inventory widget.
 * @param key The key.
 */
void widget_inventory_handle_arrow_key(widgetdata *widget, SDLKey key)
{
    inventory_struct *inventory;
    int selected, max;
    uint32 offset;

    inventory = INVENTORY(widget);

    if (INVENTORY_COLS(inventory) == 0) {
        return;
    }

    selected = inventory->selected;

    switch (key) {
        case SDLK_UP:
            selected -= INVENTORY_COLS(inventory);
            break;

        case SDLK_DOWN:
            selected += INVENTORY_COLS(inventory);
            break;

        case SDLK_LEFT:
            selected -= 1;
            break;

        case SDLK_RIGHT:
            selected += 1;
            break;

        default:
            break;
    }

    /* Calculate maximum number of inventory items. */
    max = widget_inventory_num_items(widget);

    /* Make sure the selected value does not overflow. */
    if (selected < 0) {
        selected = 0;
    }
    else if (selected > max - 1) {
        selected = max - 1;
    }

    /* Selection is the same, nothing to do. */
    if (inventory->selected == (uint32) selected) {
        return;
    }

    inventory->selected = selected;
    widget->redraw = 1;

    offset = MAX(0, selected / (int) INVENTORY_COLS(inventory));

    if (inventory->scrollbar_info.scroll_offset > offset) {
        inventory->scrollbar_info.scroll_offset = offset;
    }
    else if (offset >= inventory->scrollbar.max_lines +
            inventory->scrollbar_info.scroll_offset) {
        inventory->scrollbar_info.scroll_offset = offset -
                inventory->scrollbar.max_lines + 1;
    }

    inventory->scrollbar_info.num_lines = ceil(max /
            (double) INVENTORY_COLS(inventory));
    /* Makes sure the scroll offset doesn't overflow. */
    scrollbar_scroll_adjust(&inventory->scrollbar, 0);
    inventory->scrollbar_info.redraw = 0;
}

/**
 * Draw an inventory item on the screen surface.
 *
 * Uses object_show_centered() to draw the item's face and center it.
 * Draws any additional flags (like magical, cursed, damned) as icons
 * and draws nrof (if higher than 1) of items near the bottom.
 * @param tmp Pointer to the inventory item
 * @param x X position of the item
 * @param y Y position of the item
 */
void object_show_inventory(SDL_Surface *surface, object *tmp, int x, int y)
{
    SDL_Surface *icon;

    object_show_centered(surface, tmp, x, y);

    if (tmp->nrof > 1) {
        char buf[64];
        SDL_Rect box;

        if (tmp->nrof > 9999) {
            snprintf(buf, sizeof(buf), "many");
        } else {
            snprintf(buf, sizeof(buf), "%d", tmp->nrof);
        }

        box.w = INVENTORY_ICON_SIZE;
        text_show(surface, FONT_ARIAL10, buf, x, y + 18, COLOR_WHITE,
                TEXT_OUTLINE | TEXT_ALIGN_CENTER, &box);
    }

    if (tmp->flags & CS_FLAG_APPLIED) {
        surface_show(surface, x, y, NULL, TEXTURE_CLIENT("apply"));
    } else if (tmp->flags & CS_FLAG_UNPAID) {
        surface_show(surface, x, y, NULL, TEXTURE_CLIENT("unpaid"));
    }

    if (tmp->flags & CS_FLAG_LOCKED) {
        icon = TEXTURE_CLIENT("lock");
        surface_show(surface, x, y + INVENTORY_ICON_SIZE - icon->w - 2, NULL,
                icon);
    }

    if (tmp->flags & CS_FLAG_IS_MAGICAL) {
        icon = TEXTURE_CLIENT("magic");
        surface_show(surface, x + INVENTORY_ICON_SIZE - icon->w - 2,
                y + INVENTORY_ICON_SIZE - icon->h - 2, NULL, icon);
    }

    if (tmp->flags & (CS_FLAG_CURSED | CS_FLAG_DAMNED)) {
        if (tmp->flags & CS_FLAG_DAMNED) {
            icon = TEXTURE_CLIENT("damned");
        } else {
            icon = TEXTURE_CLIENT("cursed");
        }

        surface_show(surface, x + INVENTORY_ICON_SIZE - icon->w - 2, y, NULL,
                icon);
    }

    if (tmp->flags & CS_FLAG_IS_TRAPPED) {
        icon = TEXTURE_CLIENT("trapped");
        surface_show(surface, x + INVENTORY_ICON_SIZE / 2 - icon->w / 2,
                y + INVENTORY_ICON_SIZE / 2 - icon->h / 2, NULL, icon);
    }
}

/**
 * The 'Drop' menu action for inventory windows.
 * @param widget The widget.
 * @param menuitem The menu item.
 * @param event Event.
 */
void menu_inventory_drop(widgetdata *widget, widgetdata *menuitem,
        SDL_Event *event)
{
    object *ob, *container;
    int nrof;
    sint32 loc;

    if (widget->type != MAIN_INV_ID) {
        return;
    }

    ob = widget_inventory_get_selected(widget);
    container = object_find(cpl.container_tag);

    if (ob == NULL) {
        return;
    }

    if (ob->flags & CS_FLAG_LOCKED) {
        draw_info(COLOR_DGOLD, "That item is locked.");
        return;
    }

    if (container && container->env == cpl.below) {
        loc = container->tag;
    } else {
        loc = cpl.below->tag;
    }

    nrof = ob->nrof;

    if (nrof == 1) {
        nrof = 0;
    } else if (!(setting_get_int(OPT_CAT_GENERAL, OPT_COLLECT_MODE) & 2)) {
        widget_input_struct *input;
        char buf[MAX_BUF];

        WIDGET_SHOW(cur_widget[INPUT_ID]);
        SetPriorityWidget(cur_widget[INPUT_ID]);
        input = cur_widget[INPUT_ID]->subwidget;

        snprintf(input->title_text, sizeof(input->title_text),
                "Drop how many from %d %s?", nrof, ob->s_name);
        snprintf(input->prepend_text, sizeof(input->prepend_text),
                "/droptag %d %d ", loc, ob->tag);
        snprintf(buf, sizeof(buf), "%d", nrof);
        text_input_set(&input->text_input, buf);
        input->text_input.character_check_func =
                text_input_number_character_check;
        text_input_set_history(&input->text_input, NULL);
        return;
    }

    draw_info_format(COLOR_DGOLD, "drop %s", ob->s_name);
    client_send_move(loc, ob->tag, nrof);
    sound_play_effect("drop.ogg", 100);
}

/**
 * The 'Drop all' menu action for inventory windows.
 * @param widget The widget.
 * @param menuitem The menu item.
 * @param event Event.
 */
void menu_inventory_dropall(widgetdata *widget, widgetdata *menuitem,
        SDL_Event *event)
{
    send_command_check("/drop all");
}

/**
 * The 'Get' menu action for inventory windows.
 * @param widget The widget.
 * @param menuitem The menu item.
 * @param event Event.
 */
void menu_inventory_get(widgetdata *widget, widgetdata *menuitem,
        SDL_Event *event)
{
    object *ob, *container;
    int nrof;
    sint32 loc;

    ob = widget_inventory_get_selected(widget);
    container = object_find(cpl.container_tag);

    if (ob == NULL) {
        return;
    }

    assert(widget->type == MAIN_INV_ID || widget->type == BELOW_INV_ID);

    if (widget->type == MAIN_INV_ID) { /* 'G' in main inventory. */
        /* Need to have an open container to do 'get' in main inventory... */
        if (container == NULL) {
            draw_info(COLOR_DGOLD, "You have no open container to put it in.");
            return;
        } else {
            if (container->env != cpl.ob) {
                /* Open container not in main inventory... */
                draw_info(COLOR_DGOLD, "You already have it.");
                return;
            } else if (ob->env == cpl.sack) {
                /* If the object is already in the open container, take it
                 * out. */
                loc = cpl.ob->tag;
            } else {
                /* Put the object into the open container. */
                loc = container->tag;
            }
        }
    } else if (widget->type == BELOW_INV_ID) { /* 'G' in below inventory. */
        if (container != NULL && container->env == cpl.below &&
                container->tag != ob->tag && ob->env != cpl.sack) {
            /* If there is an open container on the ground and the item to
             * 'get' is not the container and it's not inside the container,
             * put it into the container. */
            loc = container->tag;
        } else {
            /* Otherwise pick it up into the player's inventory. */
            loc = cpl.ob->tag;
        }
    }

    nrof = ob->nrof;

    if (nrof == 1) {
        nrof = 0;
    } else if (!(setting_get_int(OPT_CAT_GENERAL, OPT_COLLECT_MODE) & 1)) {
        widget_input_struct *input;
        char buf[MAX_BUF];

        WIDGET_SHOW(cur_widget[INPUT_ID]);
        SetPriorityWidget(cur_widget[INPUT_ID]);
        input = cur_widget[INPUT_ID]->subwidget;

        snprintf(input->title_text, sizeof(input->title_text),
                "Take how many from %d %s?", nrof, ob->s_name);
        snprintf(input->prepend_text, sizeof(input->prepend_text),
                "/gettag %d %d ", loc, ob->tag);
        snprintf(buf, sizeof(buf), "%d", nrof);
        text_input_set(&input->text_input, buf);
        input->text_input.character_check_func =
                text_input_number_character_check;
        text_input_set_history(&input->text_input, NULL);
        return;
    }

    draw_info_format(COLOR_DGOLD, "get %s", ob->s_name);
    client_send_move(loc, ob->tag, nrof);
    sound_play_effect("get.ogg", 100);
}

/**
 * The 'Get all' menu action for inventory windows.
 * @param widget The widget.
 * @param menuitem The menu item.
 * @param event Event.
 */
void menu_inventory_getall(widgetdata *widget, widgetdata *menuitem,
        SDL_Event *event)
{
    send_command_check("/take all");
}

/**
 * The 'Examine' menu action for inventory windows.
 * @param widget The widget.
 * @param menuitem The menu item.
 * @param event Event.
 */
void menu_inventory_examine(widgetdata *widget, widgetdata *menuitem,
        SDL_Event *event)
{
    object *ob;

    ob = widget_inventory_get_selected(widget);

    if (ob == NULL) {
        return;
    }

    draw_info_format(COLOR_DGOLD, "examine %s", ob->s_name);
    client_send_examine(ob->tag);
}

/**
 * The 'Load to console' menu action for inventory windows.
 * @param widget The widget.
 * @param menuitem The menu item.
 * @param event Event.
 */
void menu_inventory_loadtoconsole(widgetdata *widget, widgetdata *menuitem,
        SDL_Event *event)
{
    object *ob;
    char buf[HUGE_BUF];

    ob = widget_inventory_get_selected(widget);

    if (ob == NULL) {
        return;
    }

    snprintf(buf, sizeof(buf), "/console noinf::obj = find_obj(me, count = %d)",
            ob->tag);
    send_command(buf);
}

/**
 * The 'Patch' menu action for inventory windows.
 * @param widget The widget.
 * @param menuitem The menu item.
 * @param event Event.
 */
void menu_inventory_patch(widgetdata *widget, widgetdata *menuitem,
        SDL_Event *event)
{
    object *ob;
    char buf[HUGE_BUF];

    ob = widget_inventory_get_selected(widget);

    if (ob == NULL) {
        return;
    }

    snprintf(buf, sizeof(buf), "/patch #%d ", ob->tag);
    widget_textwin_handle_console(buf);
}

/**
 * The 'Mark' menu action for inventory windows.
 * @param widget The widget.
 * @param menuitem The menu item.
 * @param event Event.
 */
void menu_inventory_mark(widgetdata *widget, widgetdata *menuitem,
        SDL_Event *event)
{
    object *ob;

    ob = widget_inventory_get_selected(widget);

    if (ob == NULL) {
        return;
    }

    if (ob->tag == cpl.mark_count) {
        draw_info_format(COLOR_DGOLD, "unmark %s", ob->s_name);
    } else {
        draw_info_format(COLOR_DGOLD, "mark %s", ob->s_name);
    }

    object_send_mark(ob);
}

/**
 * The 'Lock' menu action for inventory windows.
 * @param widget The widget.
 * @param menuitem The menu item.
 * @param event Event.
 */
void menu_inventory_lock(widgetdata *widget, widgetdata *menuitem,
        SDL_Event *event)
{
    object *ob;

    ob = widget_inventory_get_selected(widget);

    if (ob == NULL) {
        return;
    }

    if (ob->flags & CS_FLAG_LOCKED) {
        draw_info_format(COLOR_DGOLD, "unlock %s", ob->s_name);
    } else {
        draw_info_format(COLOR_DGOLD, "lock %s", ob->s_name);
    }

    toggle_locked(ob);
}

/**
 * The 'Drag' menu action for inventory windows.
 * @param widget The widget.
 * @param menuitem The menu item.
 * @param event Event.
 */
void menu_inventory_drag(widgetdata *widget, widgetdata *menuitem,
        SDL_Event *event)
{
    object *ob;

    ob = widget_inventory_get_selected(widget);

    if (ob == NULL) {
        return;
    }

    cpl.dragging_tag = ob->tag;
    cpl.dragging_startx = event->motion.x;
    cpl.dragging_starty = event->motion.y;
}

/**
 * Handle the 'apply' operation for objects inside inventory widget.
 * @param widget The widget.
 */
void widget_inventory_handle_apply(widgetdata *widget)
{
    object *ob;

    ob = widget_inventory_get_selected(widget);

    if (!ob) {
        return;
    }

    draw_info_format(COLOR_DGOLD, "apply %s", ob->s_name);
    client_send_apply(ob->tag);
}

/**
 * Handle clicking a specific inventory filter.
 * @param widget The widget.
 * @param menuitem The menu item.
 * @param event Event.
 */
void menu_inv_filter(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    widgetdata *tmp;
    _widget_label *label;
    size_t i;

    for (tmp = menuitem->inv; tmp; tmp = tmp->next) {
        if (tmp->type == LABEL_ID) {
            label = LABEL(menuitem->inv);

            if (strcasecmp(label->text, "all") == 0) {
                inventory_filter_set(INVENTORY_FILTER_ALL);
                return;
            }

            for (i = 0; i < INVENTORY_FILTER_MAX; i++) {
                if (strcasecmp(label->text, inventory_filter_names[i]) == 0) {
                    inventory_filter_toggle(1 << i);
                    return;
                }
            }

            break;
        }
    }
}

/**
 * Construct the inventory filters submenu.
 * @param widget The widget.
 * @param menuitem The menu item.
 * @param event Event.
 */
void menu_inv_filter_submenu(widgetdata *widget, widgetdata *menuitem,
        SDL_Event *event)
{
    widgetdata *submenu;
    size_t i;
    char buf[MAX_BUF];

    submenu = MENU(menuitem->env)->submenu;

    add_menuitem(submenu, "All", &menu_inv_filter, MENU_CHECKBOX,
            inventory_filter == INVENTORY_FILTER_ALL);

    for (i = 0; i < INVENTORY_FILTER_MAX; i++) {
        snprintf(buf, sizeof(buf), "%s", inventory_filter_names[i]);
        string_capitalize(buf);

        add_menuitem(submenu, buf, &menu_inv_filter, MENU_CHECKBOX,
                inventory_filter & (1 << i));
    }
}

/**
 * Construct the "More" inventory submenu.
 * @param widget The widget.
 * @param menuitem The menu item.
 * @param event Event.
 */
void menu_inventory_submenu_more(widgetdata *widget, widgetdata *menuitem,
        SDL_Event *event)
{
    widgetdata *submenu;

    submenu = MENU(menuitem->env)->submenu;
    add_menuitem(submenu, "Drop all", &menu_inventory_dropall, MENU_NORMAL, 0);
    add_menuitem(submenu, "Mark", &menu_inventory_mark, MENU_NORMAL, 0);
    add_menuitem(submenu, "Lock", &menu_inventory_lock, MENU_NORMAL, 0);
    add_menuitem(submenu, "Drag", &menu_inventory_drag, MENU_NORMAL, 0);
}
