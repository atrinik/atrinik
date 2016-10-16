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
 * Implements quickslots type widgets.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <toolkit/packet.h>
#include <toolkit/string.h>

/**
 * Structure that holds custom data for quickslot type widgets.
 */
typedef struct widget_quickslots {
    list_struct *list; ///< The quickslots list.
} widget_quickslots_t;

/**
 * Initialize all quickslot widgets.
 */
void quickslots_init(void)
{
    for (widgetdata *widget = cur_widget[QUICKSLOT_ID]; widget != NULL;
            widget = widget->type_next) {
        widget_quickslots_t *tmp = widget->subwidget;
        list_clear(tmp->list);

        for (uint32_t i = 0; i < MAX_QUICK_SLOTS * MAX_QUICKSLOT_GROUPS; i++) {
            list_add(tmp->list, i / MAX_QUICK_SLOTS, i % tmp->list->cols, NULL);
        }
    }
}

/**
 * Tell the server to set quickslot with ID 'slot' to the item with ID
 * 'tag'.
 * @param slot
 * Quickslot ID.
 * @param tag
 * ID of the item to set. Zero will clear any item from the slot.
 */
static void quickslots_set(widgetdata *widget, uint32_t row, uint32_t col,
        tag_t tag)
{
    widget_quickslots_t *tmp = widget->subwidget;

    packet_struct *packet = packet_new(SERVER_CMD_QUICKSLOT, 32, 0);
    packet_append_uint8(packet, (row * MAX_QUICK_SLOTS) + col + 1);
    packet_append_uint32(packet, tag);
    socket_send_packet(packet);

    if (tmp->list->text[row][col] != NULL) {
        efree(tmp->list->text[row][col]);
        tmp->list->text[row][col] = NULL;
    }

    if (tag != 0) {
        char buf[MAX_BUF];
        snprintf(VS(buf), "%" PRIu32, tag);
        tmp->list->text[row][col] = estrdup(buf);
    }
}

/**
 * Scroll the quickslots list.
 * @param widget
 * Quickslots widget.
 * @param up
 * If 1, scroll upwards, otherwise scroll downwards.
 * @param scroll
 * Scroll amount.
 */
void quickslots_scroll(widgetdata *widget, int up, int scroll)
{
    widget_quickslots_t *tmp = widget->subwidget;
    list_scroll(tmp->list, up, scroll);

    widget->redraw = 1;
}

/**
 * Cycle the quickslots list.
 * @param widget
 * Quickslots widget.
 */
void quickslots_cycle(widgetdata *widget)
{
    widget_quickslots_t *tmp = widget->subwidget;

    if (tmp->list->row_selected == tmp->list->rows) {
        list_scroll(tmp->list, 1, tmp->list->rows);
    } else {
        list_scroll(tmp->list, 0, 1);
    }

    widget->redraw = 1;
}

/**
 * Remove item from the quickslots by tag.
 * @param tag
 * Item tag to remove from quickslots.
 */
static void quickslots_remove(widgetdata *widget, tag_t tag)
{
    widget_quickslots_t *tmp = widget->subwidget;

    for (uint32_t row = 0; row < tmp->list->rows; row++) {
        for (uint32_t col = 0; col < tmp->list->cols; col++) {
            if (tmp->list->text[row][col] == NULL) {
                continue;
            }

            if (tag == strtoul(tmp->list->text[row][col], NULL, 10)) {
                efree(tmp->list->text[row][col]);
                tmp->list->text[row][col] = NULL;
                break;;
            }
        }
    }
}

/**
 * Trigger the specified quickslot.
 * @param widget
 * Quickslots widget.
 * @param row
 * Row.
 * @param col
 * Column.
 * @return
 * 1 if the trigger was handled, 0 otherwise.
 */
static int quickslots_trigger(widgetdata *widget, uint32_t row, uint32_t col)
{
    HARD_ASSERT(widget != NULL);

    widget_quickslots_t *tmp = widget->subwidget;
    unsigned long int tag = tmp->list->text[row][col] != NULL ?
        strtoul(tmp->list->text[row][col], NULL, 10) : 0;
    if (tag == 0) {
        return 0;
    }

    object *ob = object_find(tag);
    SOFT_ASSERT_RC(ob != NULL, 0, "Could not find object by tag: %lu", tag);

    if (ob->itype == TYPE_SPELL) {
        size_t spell_path, spell_id;
        if (spell_find(ob->s_name, &spell_path, &spell_id)) {
            spell_entry_struct *spell = spell_get(spell_path, spell_id);
            if (spell != NULL && spell->flags & SPELL_DESC_SELF) {
                char buf[MAX_BUF];
                snprintf(VS(buf), "/cast %s", ob->s_name);
                send_command_check(buf);
                return 1;
            }
        }
    }

    client_send_apply(ob);
    return 1;
}

/**
 * Changes the specified quickslot.
 * @param widget
 * Quickslots widget.
 * @param row
 * Row.
 * @param col
 * Column.
 * @return
 * 1 if the change was handled, 0 otherwise.
 */
static int quickslots_change(widgetdata *widget, uint32_t row, uint32_t col)
{
    HARD_ASSERT(widget != NULL);

    object *ob = widget_inventory_get_selected(cpl.inventory_focus);
    if (ob == NULL || ob->env != cpl.ob) {
        return 0;
    }

    widget_quickslots_t *tmp = widget->subwidget;

    unsigned long int tag = tmp->list->text[row][col] != NULL ?
        strtoul(tmp->list->text[row][col], NULL, 10) : 0;
    if ((tag_t) ob->tag == tag) {
        quickslots_set(widget, row, col, 0);
    } else {
        quickslots_remove(widget, ob->tag);
        quickslots_set(widget, row, col, ob->tag);
    }

    widget->redraw = 1;
    return 1;
}

/**
 * Handle quickslot key event.
 * @param slot
 * The quickslot to handle.
 */
void quickslots_handle_key(int slot)
{
    for (widgetdata *widget = cur_widget[QUICKSLOT_ID]; widget != NULL;
            widget = widget->type_next) {
        widget_quickslots_t *tmp = widget->subwidget;

        if (keybind_command_matches_state("?QUICKSLOT_SET_KEY")) {
            if (quickslots_change(widget, tmp->list->row_offset, slot)) {
                break;
            }
        } else if (quickslots_trigger(widget, tmp->list->row_offset, slot)) {
            break;
        }
    }
}

/** @copydoc list_struct::post_column_func */
static void list_post_column(list_struct *list, uint32_t row, uint32_t col)
{
    if (list->text[row][col] == NULL) {
        return;
    }

    object *tmp = object_find_object(cpl.ob,
            strtoul(list->text[row][col], NULL, 10));
    if (tmp == NULL) {
        return;
    }

    int x = list->x + list->frame_offset + (INVENTORY_ICON_SIZE + 1) * col;
    int y = LIST_ROWS_START(list) + (LIST_ROW_OFFSET(row, list) *
            LIST_ROW_HEIGHT(list));
    object_show_inventory(list->surface, tmp, x, y);

    char buf[MAX_BUF];
    snprintf(VS(buf), "?QUICKSLOT_%" PRIu32, col + 1);
    keybind_struct *keybind = keybind_find_by_command(buf);

    if (keybind != NULL) {
        SDL_Rect box;
        box.w = INVENTORY_ICON_SIZE;
        box.h = INVENTORY_ICON_SIZE;

        keybind_get_key_shortcut(keybind->key, keybind->mod, VS(buf));
        text_show_format(list->surface, FONT("sans", 8), x, y,
                COLOR_HGOLD, TEXT_MARKUP, &box,
                "[right][alpha=220][o=#000000]%s[/o][/alpha][/right]", buf);
    }
}

/** @copydoc list_struct::row_color_func */
static void list_row_color(list_struct *list, int row, SDL_Rect box)
{
    Uint32 color = SDL_MapRGB(list->surface->format, 25, 25, 25);
    SDL_FillRect(list->surface, &box, color);
    box.w = 1;

    color = SDL_MapRGB(list->surface->format, 130, 130, 130);
    for (int i = 0; i < MAX_QUICK_SLOTS; i++) {
        box.x = (INVENTORY_ICON_SIZE + 1) * i + 1;
        SDL_FillRect(list->surface, &box, color);
    }
}

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
    if (!widget->redraw) {
        return;
    }

    widget_quickslots_t *tmp = widget->subwidget;
    tmp->list->surface = widget->surface;
    list_set_parent(tmp->list, widget->x, widget->y);
    list_show(tmp->list, 2, 2);
}

/** @copydoc widgetdata::event_func */
static int widget_event(widgetdata *widget, SDL_Event *event)
{
    if (!EVENT_IS_MOUSE(event)) {
        return 0;
    }

    widget_quickslots_t *tmp = widget->subwidget;

    uint32_t row, col;
    if (list_mouse_get_pos(tmp->list, event->motion.x, event->motion.y,
            &row, &col)) {
        if (event->button.button == SDL_BUTTON_LEFT) {
            if (event->type == SDL_MOUSEBUTTONUP) {
                if (event_dragging_check()) {
                    if (!object_find_object_inv(cpl.ob, cpl.dragging_tag)) {
                        draw_info(COLOR_RED, "Only items from main inventory "
                                "are allowed in quickslots.");
                    } else {
                        quickslots_remove(widget, cpl.dragging_tag);
                        quickslots_set(widget, row, col, cpl.dragging_tag);
                        widget->redraw = 1;
                    }

                    event_dragging_stop();
                } else {
                    quickslots_trigger(widget, row, col);
                }

                return 1;
            } else if (event->type == SDL_MOUSEBUTTONDOWN &&
                    tmp->list->text[row][col] != NULL) {
                event_dragging_start(strtoul(tmp->list->text[row][col], NULL,
                        10), event->motion.x, event->motion.y);
                return 1;
            }
        } else if (event->type == SDL_MOUSEMOTION) {
            if (tmp->list->text[row][col] != NULL) {
                object *ob = object_find_object(cpl.ob,
                        strtoul(tmp->list->text[row][col], NULL, 10));

                if (ob != NULL) {
                    tooltip_create(event->motion.x, event->motion.y,
                            FONT_ARIAL11, ob->s_name);
                }
            }
        }
    }

    if (list_handle_mouse(tmp->list, event)) {
        widget->redraw = 1;
        return 1;
    }

    return 0;
}

/** @copydoc widgetdata::deinit_func */
static void widget_deinit(widgetdata *widget)
{
    widget_quickslots_t *tmp = widget->subwidget;
    list_remove(tmp->list);
}

/**
 * Initialize one quickslots widget.
 * @param widget
 * The widget to initialize.
 */
void widget_quickslots_init(widgetdata *widget)
{
    widget_quickslots_t *tmp;
    uint32_t i;

    tmp = ecalloc(1, sizeof(*tmp));
    tmp->list = list_create(1, MAX_QUICK_SLOTS, 0);
    tmp->list->post_column_func = list_post_column;
    tmp->list->row_color_func = list_row_color;
    tmp->list->row_selected_func = NULL;
    tmp->list->row_highlight_func = NULL;
    tmp->list->row_height_adjust = INVENTORY_ICON_SIZE;
    tmp->list->header_height = tmp->list->frame_offset = 0;
    list_set_font(tmp->list, NULL);
    list_scrollbar_enable(tmp->list);

    for (i = 0; i < tmp->list->cols; i++) {
        list_set_column(tmp->list, i, INVENTORY_ICON_SIZE + 1, 0, NULL, -1);
    }

    widget->draw_func = widget_draw;
    widget->event_func = widget_event;;
    widget->deinit_func = widget_deinit;
    widget->subwidget = tmp;
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_quickslots(uint8_t *data, size_t len, size_t pos)
{
    quickslots_init();

    while (pos < len) {
        uint8_t slot = packet_to_uint8(data, len, &pos);
        tag_t tag = packet_to_uint32(data, len, &pos);
        char buf[MAX_BUF];
        snprintf(VS(buf), "%" PRIu32, tag);

        for (widgetdata *widget = cur_widget[QUICKSLOT_ID]; widget != NULL;
                widget = widget->type_next) {
            widget_quickslots_t *tmp = widget->subwidget;
            list_add(tmp->list, slot / MAX_QUICK_SLOTS, slot % tmp->list->cols,
                    buf);
        }
    }
}
