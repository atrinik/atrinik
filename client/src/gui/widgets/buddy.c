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
 * Implements buddy type widgets.
 *
 * @author Alex Tokar */

#include <global.h>
#include <toolkit_string.h>

enum {
    BUTTON_ADD,
    BUTTON_REMOVE,
    BUTTON_CLOSE,
    BUTTON_HELP,

    BUTTON_NUM
};

/**
 * Buddy data structure. */
typedef struct buddy_struct {
    /**
     * The character names. */
    UT_array *names;

    /**
     * Path where to load/save the character names. */
    char *path;

    /**
     * Button buffer. */
    button_struct buttons[BUTTON_NUM];

    /**
     * The list. */
    list_struct *list;

    /**
     * Text input buffer. */
    text_input_struct text_input;
} buddy_struct;

#define WIDGET_BUDDY(_widget) ((buddy_struct *) (_widget)->subwidget)

/** @copydoc list_struct::handle_enter_func */
static void list_handle_enter(list_struct *list, SDL_Event *event)
{
    widgetdata *widget;

    widget = widget_find(NULL, -1, NULL, list->surface);

    if (strcmp(widget->id, "buddy") == 0) {
        textwin_tab_open(widget_find(NULL, CHATWIN_ID, NULL, NULL),
                list->text[list->row_selected - 1][0]);
    }
}

/**
 * Add a buddy.
 * @param widget Widget to add to.
 * @param name Buddy's name. Can be NULL, in which case nothing is added.
 * @param sort If 1, sort the buddy list.
 */
void widget_buddy_add(widgetdata *widget, const char *name, uint8_t sort)
{
    buddy_struct *tmp;

    tmp = WIDGET_BUDDY(widget);

    if (name != NULL) {
        if (widget_buddy_check(widget, name) == -1) {
            utarray_push_back(tmp->names, &name);
            list_add(tmp->list, tmp->list->rows, 0, name);
        }
    }

    if (sort) {
        list_sort(tmp->list, LIST_SORT_ALPHA);
    }

    widget->redraw = 1;
}

/**
 * Remove a buddy.
 * @param widget Widget to remove from.
 * @param name Buddy's name.
 */
void widget_buddy_remove(widgetdata *widget, const char *name)
{
    ssize_t idx;

    idx = widget_buddy_check(widget, name);

    if (idx != -1) {
        buddy_struct *tmp;
        uint32_t row;

        tmp = WIDGET_BUDDY(widget);
        utarray_erase(tmp->names, (size_t) idx, 1);

        for (row = 0; row < tmp->list->rows; row++) {
            if (strcmp(tmp->list->text[row][0], name) == 0) {
                list_remove_row(tmp->list, row);
                break;
            }
        }

        widget->redraw = 1;
    }
}

/**
 * Check if the specified character name is a buddy.
 * @param widget Widget to check. Can be NULL, in which case -1 is returned.
 * @param name Character name to check.
 * @return -1 if the character name is not a buddy, index in the character
 * names array otherwise.
 */
ssize_t widget_buddy_check(widgetdata *widget, const char *name)
{
    char **p;

    if (!widget) {
        return -1;
    }

    p = NULL;

    while ((p = (char **) utarray_next(WIDGET_BUDDY(widget)->names, p))) {
        if (strcasecmp(*p, name) == 0) {
            return utarray_eltidx(WIDGET_BUDDY(widget)->names, p);
        }
    }

    return -1;
}

/**
 * Load the buddy data file.
 * @param widget The buddy widget.
 */
static void widget_buddy_load(widgetdata *widget)
{
    char buf[MAX_BUF], *end;
    buddy_struct *tmp;
    FILE *fp;

    tmp = WIDGET_BUDDY(widget);

    if (tmp->path) {
        return;
    }

    snprintf(buf, sizeof(buf), "%s.dat", widget->id);
    tmp->path = file_path_player(buf);

    fp = fopen_wrapper(tmp->path, "r");

    if (!fp) {
        return;
    }

    while (fgets(buf, sizeof(buf), fp)) {
        end = strchr(buf, '\n');

        if (end) {
            *end = '\0';
        }

        widget_buddy_add(widget, buf, 0);
    }

    fclose(fp);
    widget_buddy_add(widget, NULL, 1);
}

/**
 * Save the buddy data file.
 * @param widget The buddy widget.
 */
static void widget_buddy_save(widgetdata *widget)
{
    buddy_struct *tmp;
    FILE *fp;

    tmp = WIDGET_BUDDY(widget);

    if (!tmp->path) {
        return;
    }

    fp = fopen_wrapper(tmp->path, "w");

    if (!fp) {
        logger_print(LOG(BUG), "Could not open file for writing: %s",
                tmp->path);
    } else {
        char **p;

        p = NULL;

        while ((p = (char **) utarray_next(tmp->names, p))) {
            fprintf(fp, "%s\n", *p);
        }

        fclose(fp);
    }

    efree(tmp->path);
    tmp->path = NULL;
}

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
    buddy_struct *tmp;

    tmp = WIDGET_BUDDY(widget);

    if (widget->redraw) {
        SDL_Rect box;
        char buf[MAX_BUF];
        size_t i;

        box.w = widget->w;
        box.h = 0;
        snprintf(buf, sizeof(buf), "%s list", widget->id);
        string_title(buf);
        text_show(widget->surface, FONT_SERIF12, buf, 0, 3, COLOR_HGOLD,
                TEXT_ALIGN_CENTER, &box);

        tmp->list->surface = widget->surface;
        list_set_parent(tmp->list, widget->x, widget->y);
        list_show(tmp->list, 10, 2);

        for (i = 0; i < BUTTON_NUM; i++) {
            tmp->buttons[i].surface = widget->surface;
            button_set_parent(&tmp->buttons[i], widget->x, widget->y);
        }

        tmp->buttons[BUTTON_CLOSE].x = widget->w -
                texture_surface(tmp->buttons[BUTTON_CLOSE].texture)->w - 4;
        tmp->buttons[BUTTON_CLOSE].y = 4;
        button_show(&tmp->buttons[BUTTON_CLOSE], "X");

        tmp->buttons[BUTTON_HELP].x = widget->w -
                texture_surface(tmp->buttons[BUTTON_HELP].texture)->w -
                texture_surface(tmp->buttons[BUTTON_CLOSE].texture)->w - 4;
        tmp->buttons[BUTTON_HELP].y = 4;
        button_show(&tmp->buttons[BUTTON_HELP], "?");

        tmp->buttons[BUTTON_REMOVE].x = 10;
        tmp->buttons[BUTTON_REMOVE].y = 2 + LIST_HEIGHT_FULL(tmp->list) + 3;
        button_show(&tmp->buttons[BUTTON_REMOVE], "Remove");

        tmp->buttons[BUTTON_ADD].x = tmp->buttons[BUTTON_REMOVE].x +
                texture_surface(tmp->buttons[BUTTON_REMOVE].texture)->w + 5;
        tmp->buttons[BUTTON_ADD].y = tmp->buttons[BUTTON_REMOVE].y;
        button_show(&tmp->buttons[BUTTON_ADD], "Add");

        text_input_set_parent(&tmp->text_input, widget->x, widget->y);
        text_input_show(&tmp->text_input, widget->surface,
                tmp->buttons[BUTTON_ADD].x +
                texture_surface(tmp->buttons[BUTTON_REMOVE].texture)->w + 5,
                tmp->buttons[BUTTON_ADD].y);
    }
}

/** @copydoc widgetdata::background_func */
static void widget_background(widgetdata *widget, int draw)
{
    buddy_struct *tmp;

    tmp = WIDGET_BUDDY(widget);

    if (!widget->redraw) {
        widget->redraw = list_need_redraw(tmp->list);
    }

    if (!widget->redraw) {
        size_t i;

        for (i = 0; i < BUTTON_NUM; i++) {
            if (button_need_redraw(&tmp->buttons[i])) {
                widget->redraw = 1;
                break;
            }
        }
    }

    if (cpl.state == ST_PLAY) {
        widget_buddy_load(widget);
    } else {
        widget_buddy_save(widget);
    }
}

/**
 * Handles event for adding of a buddy to the specified buddy widget.
 * @param widget The buddy widget.
 */
static void widget_event_buddy_add(widgetdata *widget)
{
    buddy_struct *tmp;

    tmp = WIDGET_BUDDY(widget);

    if (*tmp->text_input.str != '\0') {
        widget_buddy_add(widget, tmp->text_input.str, 1);
        text_input_set(&tmp->text_input, NULL);
    }

    tmp->text_input.focus = 0;
}

/** @copydoc widgetdata::event_func */
static int widget_event(widgetdata *widget, SDL_Event *event)
{
    buddy_struct *tmp;
    size_t i;

    tmp = WIDGET_BUDDY(widget);

    if (list_handle_mouse(tmp->list, event)) {
        widget->redraw = 1;
        return 1;
    }

    for (i = 0; i < BUTTON_NUM; i++) {
        if (button_event(&tmp->buttons[i], event)) {
            switch (i) {
            case BUTTON_ADD:
                if (tmp->text_input.focus) {
                    widget_event_buddy_add(widget);
                } else {
                    tmp->text_input.focus = 1;
                }

                break;

            case BUTTON_REMOVE:
            {
                const char *selected;

                selected = list_get_selected(tmp->list, 0);

                if (selected != NULL) {
                    widget_buddy_remove(widget, selected);
                }

                break;
            }

            case BUTTON_CLOSE:
                widget->show = 0;
                break;

            case BUTTON_HELP:
            {
                char buf[MAX_BUF];

                snprintf(buf, sizeof(buf), "%s list", widget->id);
                help_show(buf);
                break;
            }
            }

            widget->redraw = 1;
            return 1;
        }

        if (tmp->buttons[i].redraw) {
            widget->redraw = 1;
        }
    }

    if (event->type == SDL_KEYDOWN && tmp->text_input.focus) {
        if (event->key.keysym.sym == SDLK_ESCAPE) {
            tmp->text_input.focus = 0;
            widget->redraw = 1;
            return 1;
        } else if (IS_ENTER(event->key.keysym.sym)) {
            widget_event_buddy_add(widget);
            widget->redraw = 1;
            return 1;
        }
    }

    if (text_input_event(&tmp->text_input, event)) {
        widget->redraw = 1;
        return 1;
    }

    if (event->type == SDL_MOUSEBUTTONDOWN) {
        if (event->button.button == SDL_BUTTON_LEFT &&
                text_input_mouse_over(&tmp->text_input, event->motion.x,
                event->motion.y)) {
            tmp->text_input.focus = 1;
            widget->redraw = 1;
            return 1;
        }

        if (tmp->text_input.focus) {
            tmp->text_input.focus = 0;
            widget->redraw = 1;
        }
    }

    return 0;
}

/** @copydoc widgetdata::deinit_func */
static void widget_deinit(widgetdata *widget)
{
    buddy_struct *tmp;

    widget_buddy_save(widget);

    tmp = WIDGET_BUDDY(widget);
    utarray_free(tmp->names);
    list_remove(tmp->list);
}

/**
 * Initialize one buddy widget. */
void widget_buddy_init(widgetdata *widget)
{
    buddy_struct *tmp;
    size_t i;

    widget->draw_func = widget_draw;
    widget->event_func = widget_event;
    widget->background_func = widget_background;
    widget->deinit_func = widget_deinit;

    widget->subwidget = tmp = ecalloc(1, sizeof(*tmp));
    utarray_new(tmp->names, &ut_str_icd);

    /* Create the list and set up settings. */
    tmp->list = list_create(12, 1, 8);
    tmp->list->handle_enter_func = list_handle_enter;
    list_scrollbar_enable(tmp->list);
    list_set_column(tmp->list, 0, widget->w - 10 * 2 -
            LIST_WIDTH_FULL(tmp->list) - tmp->list->frame_offset, 0, NULL, -1);
    list_set_font(tmp->list, FONT_ARIAL10);

    for (i = 0; i < BUTTON_NUM; i++) {
        button_create(&tmp->buttons[i]);

        if (i == BUTTON_CLOSE || i == BUTTON_HELP) {
            tmp->buttons[i].texture = texture_get(TEXTURE_TYPE_CLIENT,
                    "button_round");
            tmp->buttons[i].texture_pressed = texture_get(TEXTURE_TYPE_CLIENT,
                    "button_round_down");
            tmp->buttons[i].texture_over = texture_get(TEXTURE_TYPE_CLIENT,
                    "button_round_over");
        }
    }

    text_input_create(&tmp->text_input);
    tmp->text_input.focus = 0;
    tmp->text_input.coords.w = LIST_WIDTH_FULL(tmp->list) -
            texture_surface(tmp->buttons[BUTTON_ADD].texture)->w * 2 - 5 * 2;
}
