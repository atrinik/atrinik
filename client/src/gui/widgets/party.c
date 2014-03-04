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
 * Implements party type widgets.
 *
 * @author Alex Tokar */

#include <global.h>

/** Width of the hp/sp stat bar. */
#define STAT_BAR_WIDTH 60

/** Macro to create the stat bar markup. */
#define PARTY_STAT_BAR() \
    snprintf(bars, sizeof(bars), "[x=5][bar=#000000 %d 6][bar=#cb0202 %d 6][border=#909090 60 6][y=6][bar=#000000 %d 6][bar=#1818a4 %d 6][y=-1][border=#909090 60 7]", STAT_BAR_WIDTH, (int) (STAT_BAR_WIDTH * (hp / 100.0)), STAT_BAR_WIDTH, (int) (STAT_BAR_WIDTH * (sp / 100.0)));

enum
{
    BUTTON_PARTIES,
    BUTTON_MEMBERS,
    BUTTON_FORM,
    BUTTON_LEAVE,
    BUTTON_PASSWORD,
    BUTTON_CHAT,
    BUTTON_CLOSE,
    BUTTON_HELP,

    BUTTON_NUM
};

/**
 * Button buffer. */
static button_struct buttons[BUTTON_NUM];
/**
 * The party list. */
static list_struct *list_party = NULL;
/**
 * What type of data is currently in the list; -1 means no data,
 * otherwise one of @ref CMD_PARTY_xxx. */
static sint8 list_contents = -1;

/**
 * Handle enter/double click for the party list.
 * @param list List. */
static void list_handle_enter(list_struct *list, SDL_Event *event)
{
    if (list_contents == CMD_PARTY_LIST && list->text) {
        char buf[MAX_BUF];

        snprintf(buf, sizeof(buf), "/party join %s", list->text[list->row_selected - 1][0]);
        send_command(buf);
    }
}

/**
 * Highlight a row in the party list.
 * @param list List.
 * @param box Dimensions for the row. */
static void list_row_highlight(list_struct *list, SDL_Rect box)
{
    SDL_FillRect(list->surface, &box, SDL_MapRGB(list->surface->format, 0x00, 0x80, 0x00));
}

/**
 * Highlight selected row in the party list.
 * @param list List.
 * @param box Dimensions for the row. */
static void list_row_selected(list_struct *list, SDL_Rect box)
{
    SDL_FillRect(list->surface, &box, SDL_MapRGB(list->surface->format, 0x00, 0x00, 0xef));
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_party(uint8 *data, size_t len, size_t pos)
{
    uint8 type;

    type = packet_to_uint8(data, len, &pos);

    /* List of parties, or list of party members. */
    if (type == CMD_PARTY_LIST || type == CMD_PARTY_WHO) {
        list_clear(list_party);

        while (pos < len) {
            if (type == CMD_PARTY_LIST) {
                char party_name[MAX_BUF], party_leader[MAX_BUF];

                packet_to_string(data, len, &pos, party_name, sizeof(party_name));
                packet_to_string(data, len, &pos, party_leader, sizeof(party_leader));
                list_add(list_party, list_party->rows, 0, party_name);
                list_add(list_party, list_party->rows - 1, 1, party_leader);
            }
            else if (type == CMD_PARTY_WHO) {
                char name[MAX_BUF], bars[MAX_BUF];
                uint8 hp, sp;

                packet_to_string(data, len, &pos, name, sizeof(name));
                hp = packet_to_uint8(data, len, &pos);
                sp = packet_to_uint8(data, len, &pos);
                list_add(list_party, list_party->rows, 0, name);
                PARTY_STAT_BAR();
                list_add(list_party, list_party->rows - 1, 1, bars);
            }
        }

        /* Sort the list of party members alphabetically. */
        if (type == CMD_PARTY_WHO) {
            list_sort(list_party, LIST_SORT_ALPHA);
        }

        /* Update column names, depending on the list contents. */
        list_set_column(list_party, 0, -1, -1, type == CMD_PARTY_LIST ? "Party name" : "Player", -1);
        list_set_column(list_party, 1, -1, -1, type == CMD_PARTY_LIST ? "Leader" : "Stats", -1);

        list_contents = type;
        cur_widget[PARTY_ID]->redraw = 1;
        cur_widget[PARTY_ID]->show = 1;
        SetPriorityWidget(cur_widget[PARTY_ID]);
    }
    /* Join command; store the party name we're member of, and show the
     * list of party members, if the party widget is not hidden. */
    else if (type == CMD_PARTY_JOIN) {
        packet_to_string(data, len, &pos, cpl.partyname, sizeof(cpl.partyname));

        if (cur_widget[PARTY_ID]->show) {
            send_command("/party who");
        }
    }
    /* Leave; clear the party name and switch to list of parties (unless
     * the party widget is hidden). */
    else if (type == CMD_PARTY_LEAVE) {
        cpl.partyname[0] = '\0';

        if (cur_widget[PARTY_ID]->show) {
            send_command("/party list");
        }
    }
    /* Party requires password, bring up the console for the player to
     * enter the password. */
    else if (type == CMD_PARTY_PASSWORD) {
        char buf[MAX_BUF];

        packet_to_string(data, len, &pos, cpl.partyjoin, sizeof(cpl.partyjoin));
        snprintf(buf, sizeof(buf), "?MCON /joinpassword ");
        keybind_process_command(buf);
    }
    /* Update list of party members. */
    else if (type == CMD_PARTY_UPDATE) {
        char name[MAX_BUF], bars[MAX_BUF];
        uint8 hp, sp;
        uint32 row;

        if (list_contents != CMD_PARTY_WHO) {
            return;
        }

        packet_to_string(data, len, &pos, name, sizeof(name));
        hp = packet_to_uint8(data, len, &pos);
        sp = packet_to_uint8(data, len, &pos);

        PARTY_STAT_BAR();
        cur_widget[PARTY_ID]->redraw = 1;

        for (row = 0; row < list_party->rows; row++) {
            if (!strcmp(list_party->text[row][0], name)) {
                free(list_party->text[row][1]);
                list_party->text[row][1] = strdup(bars);
                return;
            }
        }

        list_add(list_party, list_party->rows, 0, name);
        list_add(list_party, list_party->rows - 1, 1, bars);
        list_sort(list_party, LIST_SORT_ALPHA);
    }
    /* Remove member from the list of party members. */
    else if (type == CMD_PARTY_REMOVE_MEMBER) {
        char name[MAX_BUF];
        uint32 row;

        if (list_contents != CMD_PARTY_WHO) {
            return;
        }

        packet_to_string(data, len, &pos, name, sizeof(name));
        cur_widget[PARTY_ID]->redraw = 1;

        for (row = 0; row < list_party->rows; row++) {
            if (!strcmp(list_party->text[row][0], name)) {
                list_remove_row(list_party, row);
                return;
            }
        }
    }
}

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
    SDL_Rect box;
    size_t i;

    if (widget->redraw) {
        box.h = 0;
        box.w = widget->w;
        text_show(widget->surface, FONT_SERIF12, "Party", 0, 3, COLOR_HGOLD, TEXT_ALIGN_CENTER, &box);

        if (list_party) {
            list_party->surface = widget->surface;
            list_set_parent(list_party, widget->x, widget->y);
            list_show(list_party, 10, 23);
        }

        for (i = 0; i < BUTTON_NUM; i++) {
            buttons[i].surface = widget->surface;
            button_set_parent(&buttons[i], widget->x, widget->y);
        }

        /* Render the various buttons. */
        buttons[BUTTON_CLOSE].x = widget->w - TEXTURE_CLIENT("button_round")->w - 4;
        buttons[BUTTON_CLOSE].y = 4;
        button_show(&buttons[BUTTON_CLOSE], "X");

        buttons[BUTTON_HELP].x = widget->w - TEXTURE_CLIENT("button_round")->w * 2 - 4;
        buttons[BUTTON_HELP].y = 4;
        button_show(&buttons[BUTTON_HELP], "?");

        buttons[BUTTON_PARTIES].x = 244;
        buttons[BUTTON_PARTIES].y = 38;
        button_show(&buttons[BUTTON_PARTIES], list_contents == CMD_PARTY_LIST ? "[u]Parties[/u]" : "Parties");

        buttons[BUTTON_MEMBERS].x = buttons[BUTTON_FORM].x = 244;
        buttons[BUTTON_MEMBERS].y = buttons[BUTTON_FORM].y = 60;

        if (cpl.partyname[0] == '\0') {
            button_show(&buttons[BUTTON_FORM], "Form");
        }
        else {
            button_show(&buttons[BUTTON_MEMBERS], list_contents == CMD_PARTY_WHO ? "[u]Members[/u]" : "Members");
            buttons[BUTTON_LEAVE].x = buttons[BUTTON_PASSWORD].x = buttons[BUTTON_CHAT].x = 244;
            buttons[BUTTON_LEAVE].y = 82;
            buttons[BUTTON_PASSWORD].y = 104;
            buttons[BUTTON_CHAT].y = 126;
            button_show(&buttons[BUTTON_LEAVE], "Leave");
            button_show(&buttons[BUTTON_PASSWORD], "Password");
            button_show(&buttons[BUTTON_CHAT], "Chat");
        }
    }
}

/** @copydoc widgetdata::background_func */
static void widget_background(widgetdata *widget)
{
    size_t i;

    /* Create the party list. */
    if (!list_party) {
        list_party = list_create(12, 2, 8);
        list_party->handle_enter_func = list_handle_enter;
        list_party->text_flags = TEXT_MARKUP;
        list_party->row_highlight_func = list_row_highlight;
        list_party->row_selected_func = list_row_selected;
        list_scrollbar_enable(list_party);
        list_set_column(list_party, 0, 130, 7, NULL, -1);
        list_set_column(list_party, 1, 60, 7, NULL, -1);
        list_party->header_height = 6;

        for (i = 0; i < BUTTON_NUM; i++) {
            button_create(&buttons[i]);

            if (i == BUTTON_CLOSE || i == BUTTON_HELP) {
                buttons[i].texture = texture_get(TEXTURE_TYPE_CLIENT, "button_round");
                buttons[i].texture_pressed = texture_get(TEXTURE_TYPE_CLIENT, "button_round_down");
                buttons[i].texture_over = texture_get(TEXTURE_TYPE_CLIENT, "button_round_over");
            }
            else if (i == BUTTON_PARTIES || i == BUTTON_MEMBERS) {
                buttons[i].flags = TEXT_MARKUP;
            }
        }

        widget->redraw = 1;
        list_contents = -1;
    }

    if (!widget->redraw) {
        widget->redraw = list_need_redraw(list_party);
    }

    if (!widget->redraw) {
        for (i = 0; i < BUTTON_NUM; i++) {
            if (button_need_redraw(&buttons[i])) {
                widget->redraw = 1;
                break;
            }
        }
    }
}

/** @copydoc widgetdata::event_func */
static int widget_event(widgetdata *widget, SDL_Event *event)
{
    char buf[MAX_BUF];
    size_t i;

    /* If the list has handled the mouse event, we need to redraw the
     * widget. */
    if (list_party && list_handle_mouse(list_party, event)) {
        widget->redraw = 1;
        return 1;
    }

    for (i = 0; i < BUTTON_NUM; i++) {
        if ((cpl.partyname[0] == '\0' && (i == BUTTON_PASSWORD || i == BUTTON_LEAVE || i == BUTTON_CHAT || i == BUTTON_MEMBERS)) || (cpl.partyname[0] != '\0' && (i == BUTTON_FORM))) {
            continue;
        }

        if (button_event(&buttons[i], event)) {
            switch (i) {
                case BUTTON_PARTIES:
                    send_command("/party list");
                    break;

                case BUTTON_MEMBERS:
                    send_command("/party who");
                    break;

                case BUTTON_FORM:
                    snprintf(buf, sizeof(buf), "?MCON /party form ");
                    keybind_process_command(buf);
                    break;

                case BUTTON_PASSWORD:
                    snprintf(buf, sizeof(buf), "?MCON /party password ");
                    keybind_process_command(buf);
                    break;

                case BUTTON_LEAVE:
                    send_command("/party leave");
                    break;

                case BUTTON_CHAT:
                    snprintf(buf, sizeof(buf), "?MCON /gsay ");
                    keybind_process_command(buf);
                    break;

                case BUTTON_CLOSE:
                    widget->show = 0;
                    break;

                case BUTTON_HELP:
                    help_show("spell list");
                    break;
            }

            widget->redraw = 1;
            return 1;
        }

        if (buttons[i].redraw) {
            widget->redraw = 1;
        }
    }

    return 0;
}

/**
 * Initialize one party widget. */
void widget_party_init(widgetdata *widget)
{
    widget->draw_func = widget_draw;
    widget->background_func = widget_background;
    widget->event_func = widget_event;
}
