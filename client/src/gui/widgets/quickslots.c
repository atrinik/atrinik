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
 * Implements quickslots type widgets.
 *
 * @author Alex Tokar */

#include <global.h>

typedef struct widget_quickslots_struct
{
    list_struct *list;
} widget_quickslots_struct;

void quickslots_init(void)
{
    widgetdata *widget;
    widget_quickslots_struct *tmp;
    uint32 i;

    for (widget = cur_widget[QUICKSLOT_ID]; widget; widget = widget->type_next) {
        tmp = widget->subwidget;
        list_clear(tmp->list);

        for (i = 0; i < MAX_QUICK_SLOTS * MAX_QUICKSLOT_GROUPS; i++) {
            list_add(tmp->list, (uint32) ((double) i / (double) MAX_QUICK_SLOTS - 0.5), i % tmp->list->cols, NULL);
        }
    }
}

/**
 * Tell the server to set quickslot with ID 'slot' to the item with ID
 * 'tag'.
 * @param slot Quickslot ID.
 * @param tag ID of the item to set. */
static void quickslots_set(widgetdata *widget, uint32 row, uint32 col, sint32 tag)
{
    widget_quickslots_struct *tmp;
    uint32 slot;
    packet_struct *packet;
    char buf[MAX_BUF];

    tmp = widget->subwidget;
    slot = (row * MAX_QUICK_SLOTS) + (col + 1);

    packet = packet_new(SERVER_CMD_QUICKSLOT, 32, 0);
    packet_append_uint8(packet, slot);
    packet_append_sint32(packet, tag);
    socket_send_packet(packet);

    snprintf(buf, sizeof(buf), "%d", tag);
    tmp->list->text[row][col] = estrdup(buf);
}

/**
 * Scroll the quickslots list.
 * @param widget Quickslots widget.
 * @param up If 1, scroll upwards, otherwise scroll downwards.
 * @param scroll Scroll amount. */
void quickslots_scroll(widgetdata *widget, int up, int scroll)
{
    widget_quickslots_struct *tmp;

    tmp = widget->subwidget;
    list_scroll(tmp->list, up, scroll);
}

/**
 * Cycle the quickslots list.
 * @param widget Quickslots widget. */
void quickslots_cycle(widgetdata *widget)
{
    widget_quickslots_struct *tmp;

    tmp = widget->subwidget;

    if (tmp->list->row_selected == tmp->list->rows) {
        list_scroll(tmp->list, 1, tmp->list->rows);
    } else {
        list_scroll(tmp->list, 0, 1);
    }
}

/**
 * Remove item from the quickslots by tag.
 * @param tag Item tag to remove from quickslots. */
static void quickslots_remove(widgetdata *widget, tag_t tag)
{
    widget_quickslots_struct *tmp;
    uint32 row, col;

    tmp = widget->subwidget;

    for (row = 0; row < tmp->list->rows; row++) {
        for (col = 0; col < tmp->list->cols; col++) {
            if (tmp->list->text[row][col] && (tag_t) atoi(tmp->list->text[row][col]) == tag) {
                efree(tmp->list->text[row][col]);
                tmp->list->text[row][col] = NULL;
                break;
            }
        }
    }
}

/**
 * Trigger the specified quickslot.
 * @param widget Quickslots widget.
 * @param row Row.
 * @param col Column.
 * @return 1 if the trigger was handled, 0 otherwise.
 */
static int quickslots_trigger(widgetdata *widget, uint32 row, uint32 col)
{
    widget_quickslots_struct *tmp;
    tag_t tag;
    object *ob;
    size_t spell_path, spell_id;
    spell_entry_struct *spell;

    assert(widget != NULL);

    tmp = widget->subwidget;
    tag = tmp->list->text[row][col] ? atoi(tmp->list->text[row][col]) : 0;

    if (tag == 0) {
        return 0;
    }

    ob = object_find(tag);

    if (ob->itype == TYPE_SPELL &&
            spell_find(ob->s_name, &spell_path, &spell_id) &&
            (spell = spell_get(spell_path, spell_id)) &&
            spell->flags & SPELL_DESC_SELF) {
        char buf[MAX_BUF];

        snprintf(buf, sizeof(buf), "/cast %s", ob->s_name);
        send_command_check(buf);
    } else {
        client_send_apply(tag);
    }

    return 1;
}

/**
 * Changes the specified quickslot.
 * @param widget Quickslots widget.
 * @param row Row.
 * @param col Column.
 * @param tag Tag to change to.
 * @return 1 if the change was handled, 0 otherwise.
 */
static int quickslots_change(widgetdata *widget, uint32 row, uint32 col)
{
    object *ob;
    widget_quickslots_struct *tmp;
    tag_t tag;

    assert(widget != NULL);

    if (!cur_widget[MAIN_INV_ID]->show) {
        return 0;
    }

    ob = widget_inventory_get_selected(cur_widget[MAIN_INV_ID]);

    if (ob == NULL) {
        return 0;
    }

    tmp = widget->subwidget;
    tag = tmp->list->text[row][col] ? atoi(tmp->list->text[row][col]) : 0;

    if (tag == (tag_t) ob->tag) {
        efree(tmp->list->text[row][col]);
        tmp->list->text[row][col] = NULL;
        quickslots_set(widget, row, col, -1);
    } else {
        char buf[MAX_BUF];

        quickslots_remove(widget, ob->tag);

        snprintf(buf, sizeof(buf), "%d", ob->tag);
        tmp->list->text[row][col] = estrdup(buf);
        quickslots_set(widget, row, col, ob->tag);
    }

    return 1;
}

/* Handle quickslot key event. */
void quickslots_handle_key(int slot)
{
    widgetdata *widget;
    widget_quickslots_struct *tmp;

    for (widget = cur_widget[QUICKSLOT_ID]; widget != NULL;
            widget = widget->type_next) {
        tmp = widget->subwidget;

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
static void list_post_column(list_struct *list, uint32 row, uint32 col)
{
    object *tmp;
    int x, y;
    char buf[MAX_BUF];
    keybind_struct *keybind;

    if (!list->text[row][col]) {
        return;
    }

    tmp = object_find_object(cpl.ob, atoi(list->text[row][col]));

    if (tmp == NULL) {
        return;
    }

    x = list->x + list->frame_offset + (INVENTORY_ICON_SIZE + 1) * col;
    y = LIST_ROWS_START(list) + (LIST_ROW_OFFSET(row, list) *
            LIST_ROW_HEIGHT(list));
    object_show_inventory(list->surface, tmp, x, y);

    snprintf(buf, sizeof(buf), "?QUICKSLOT_%d", col + 1);
    keybind = keybind_find_by_command(buf);

    if (keybind) {
        SDL_Rect box;

        box.w = INVENTORY_ICON_SIZE;
        box.h = INVENTORY_ICON_SIZE;

        keybind_get_key_shortcut(keybind->key, keybind->mod, buf, sizeof(buf));
        text_show_format(list->surface, FONT("sans", 8), x, y,
                COLOR_HGOLD, TEXT_MARKUP, &box,
                "[right][alpha=220][o=#000000]%s[/o][/alpha][/right]", buf);
    }

}

/** @copydoc list_struct::row_color_func */
static void list_row_color(list_struct *list, int row, SDL_Rect box)
{
    int i;

    SDL_FillRect(list->surface, &box, SDL_MapRGB(list->surface->format, 25, 25, 25));
    box.w = 1;

    for (i = 0; i < MAX_QUICK_SLOTS; i++) {
        box.x = (INVENTORY_ICON_SIZE + 1) * i + 1;
        SDL_FillRect(list->surface, &box, SDL_MapRGB(list->surface->format, 130, 130, 130));
    }
}

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
    widget_quickslots_struct *tmp;

    if (!widget->redraw) {
        return;
    }

    widget->redraw++;

    tmp = widget->subwidget;
    tmp->list->surface = widget->surface;
    list_set_parent(tmp->list, widget->x, widget->y);
    list_show(tmp->list, 2, 2);
}

/** @copydoc widgetdata::event_func */
static int widget_event(widgetdata *widget, SDL_Event *event)
{
    widget_quickslots_struct *tmp;
    uint32 row, col;

    tmp = widget->subwidget;

    if (EVENT_IS_MOUSE(event) && list_mouse_get_pos(tmp->list, event->motion.x, event->motion.y, &row, &col)) {
        if (event->button.button == SDL_BUTTON_LEFT) {
            if (event->type == SDL_MOUSEBUTTONUP) {
                if (event_dragging_check()) {
                    if (!object_find_object_inv(cpl.ob, cpl.dragging_tag)) {
                        draw_info(COLOR_RED, "Only items from main inventory are allowed in quickslots.");
                    }
                    else {
                        quickslots_remove(widget, cpl.dragging_tag);
                        quickslots_set(widget, row, col, cpl.dragging_tag);
                    }

                    event_dragging_stop();
                }
                else {
                    quickslots_trigger(widget, row, col);
                }

                return 1;
            }
            else if (event->type == SDL_MOUSEBUTTONDOWN && tmp->list->text[row][col]) {
                event_dragging_start(atoi(tmp->list->text[row][col]), event->motion.x, event->motion.y);
                return 1;
            }
        }
        else if (event->type == SDL_MOUSEMOTION) {
            if (tmp->list->text[row][col]) {
                object *ob;

                ob = object_find_object(cpl.ob, atoi(tmp->list->text[row][col]));

                if (ob) {
                    tooltip_create(event->motion.x, event->motion.y, FONT_ARIAL10, ob->s_name);
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

/**
 * Initialize one quickslots widget. */
void widget_quickslots_init(widgetdata *widget)
{
    widget_quickslots_struct *tmp;
    uint32 i;

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
    widget->event_func = widget_event;
    widget->subwidget = tmp;
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_quickslots(uint8 *data, size_t len, size_t pos)
{
    widgetdata *widget;
    widget_quickslots_struct *tmp;
    uint8 slot;
    tag_t tag;
    char buf[MAX_BUF];

    quickslots_init();

    while (pos < len) {
        slot = packet_to_uint8(data, len, &pos);
        tag = packet_to_uint32(data, len, &pos);
        snprintf(buf, sizeof(buf), "%d", tag);

        for (widget = cur_widget[QUICKSLOT_ID]; widget; widget = widget->type_next) {
            tmp = widget->subwidget;
            list_add(tmp->list, (uint32) ((double) slot / (double) MAX_QUICK_SLOTS - 0.5), slot % tmp->list->cols, buf);
        }
    }
}
