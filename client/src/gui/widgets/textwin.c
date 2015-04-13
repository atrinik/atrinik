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
 * Implements text window type widgets.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Names of the text window tabs. */
const char *const textwin_tab_names[] = {
    "[ALL]", "[GAME]", "[CHAT]", "[LOCAL]", "[PRIVATE]", "[GUILD]", "[PARTY]", "[OPERATOR]"
};

const char *const textwin_tab_commands[] = {
    "say", NULL, "chat", "say", "reply", "guild", "party say", "opsay"
};

/**
 * Readjust text window's scroll/entries counts due to a font size
 * change.
 * @param widget Text window's widget. */
void textwin_readjust(widgetdata *widget)
{
    textwin_struct *textwin = TEXTWIN(widget);

    if (!textwin->tabs) {
        return;
    }

    textwin->tabs[textwin->tab_selected].unread = 0;

    if (textwin->tabs[textwin->tab_selected].entries) {
        SDL_Rect box;

        box.w = TEXTWIN_TEXT_WIDTH(widget);
        box.h = 0;
        box.x = 0;
        box.y = 0;
        text_show(NULL, textwin->font, textwin->tabs[textwin->tab_selected].entries, TEXTWIN_TEXT_STARTX(widget), 0, COLOR_BLACK, TEXTWIN_TEXT_FLAGS(widget) | TEXT_LINES_CALC, &box);

        /* Adjust the counts. */
        textwin->tabs[textwin->tab_selected].num_lines = box.h - 1;
    }

    textwin_create_scrollbar(widget);
    scrollbar_scroll_to(&textwin->scrollbar, SCROLL_BOTTOM(&textwin->scrollbar));
    WIDGET_REDRAW(widget);
}

/** @copydoc text_anchor_handle_func */
static int text_anchor_handle(const char *anchor_action, const char *buf, size_t len, void *custom_data)
{
    if (strcmp(anchor_action, "#charname") == 0) {
        StringBuffer *sb;

        if (custom_data != NULL) {
            sb = custom_data;

            if (sb->pos != 0) {
                stringbuffer_append_char(sb, ':');
            }

            stringbuffer_append_string(sb, buf);
        } else {
            char *cp;

            sb = stringbuffer_new();
            stringbuffer_append_printf(sb, "/tell \"%s\" ", buf);
            cp = stringbuffer_finish(sb);
            widget_textwin_handle_console(cp);
            efree(cp);
        }

        return 1;
    }

    return 0;
}

static void textwin_tab_append(widgetdata *widget, uint8_t id, uint8_t type, const char *color, const char *str)
{
    textwin_struct *textwin;
    SDL_Rect box;
    size_t len, scroll;
    char timebuf[MAX_BUF], tabname[MAX_BUF], *cp;

    textwin = TEXTWIN(widget);
    box.w = TEXTWIN_TEXT_WIDTH(widget);
    box.h = 0;

    timebuf[0] = tabname[0] = '\0';

    if (setting_get_int(OPT_CAT_GENERAL, OPT_CHAT_TIMESTAMPS) && textwin->timestamps) {
        time_t now = time(NULL);
        char tmptimebuf[MAX_BUF], *format;
        struct tm *tm = localtime(&now);
        size_t timelen;

        switch (setting_get_int(OPT_CAT_GENERAL, OPT_CHAT_TIMESTAMPS)) {
            /* HH:MM */
        case 1:
        default:
            format = "%H:%M";
            break;

            /* HH:MM:SS */
        case 2:
            format = "%H:%M:%S";
            break;

            /* H:MM AM/PM */
        case 3:
            format = "%I:%M %p";
            break;

            /* H:MM:SS AM/PM */
        case 4:
            format = "%I:%M:%S %p";
            break;
        }

        timelen = strftime(tmptimebuf, sizeof(tmptimebuf), format, tm);

        if (timelen != 0) {
            snprintf(timebuf, sizeof(timebuf), "&lsqb;%s&rsqb; ", tmptimebuf);
        }
    }

    if (textwin->tabs[id].type == CHAT_TYPE_ALL) {
        cp = text_escape_markup(textwin_tab_names[type - 1]);
        snprintf(tabname, sizeof(tabname), "%s ", cp);
        efree(cp);
    } else if (textwin->tabs[textwin->tab_selected].type != CHAT_TYPE_ALL) {
        textwin->tabs[id].unread = 1;
    }

    cp = string_join("", "[c=#", color, " 1]", timebuf, tabname, str, "\n", NULL);
    len = strlen(cp);
    /* Resize the characters array as needed. */
    textwin->tabs[id].entries = erealloc(textwin->tabs[id].entries, textwin->tabs[id].entries_size + len + 1);
    memcpy(textwin->tabs[id].entries + textwin->tabs[id].entries_size, cp, len);
    textwin->tabs[id].entries[textwin->tabs[id].entries_size + len] = '\0';
    textwin->tabs[id].entries_size += len;

    box.y = 0;
    /* Get the string's height. */
    text_show(NULL, textwin->font, cp, TEXTWIN_TEXT_STARTX(widget), 0, COLOR_BLACK, TEXTWIN_TEXT_FLAGS(widget) | TEXT_LINES_CALC, &box);
    scroll = box.h - 1;

    efree(cp);

    /* Adjust the counts. */
    textwin->tabs[id].num_lines += scroll;

    /* Have the entries gone over maximum allowed lines? */
    if (textwin->tabs[id].entries && textwin->tabs[id].num_lines >= (size_t) setting_get_int(OPT_CAT_GENERAL, OPT_MAX_CHAT_LINES)) {
        while (textwin->tabs[id].num_lines >= (size_t) setting_get_int(OPT_CAT_GENERAL, OPT_MAX_CHAT_LINES) && (cp = strchr(textwin->tabs[id].entries, '\n'))) {
            size_t pos = cp - textwin->tabs[id].entries + 1;
            char *buf = emalloc(pos + 1);

            /* Copy the string together with the newline to a temporary
             * buffer. */
            memcpy(buf, textwin->tabs[id].entries, pos);
            buf[pos] = '\0';

            /* Get the string's height. */
            box.h = 0;
            text_show(NULL, textwin->font, buf, TEXTWIN_TEXT_STARTX(widget), 0, COLOR_BLACK, TEXTWIN_TEXT_FLAGS(widget) | TEXT_LINES_CALC, &box);
            scroll = box.h - 1;

            efree(buf);

            /* Move the string after the found newline to the beginning,
             * effectively erasing the previous line. */
            textwin->tabs[id].entries_size -= pos;
            memmove(textwin->tabs[id].entries, textwin->tabs[id].entries + pos, textwin->tabs[id].entries_size);
            textwin->tabs[id].entries[textwin->tabs[id].entries_size] = '\0';

            /* Adjust the counts. */
            textwin->tabs[id].num_lines -= scroll;
        }
    }
}

static int textwin_tab_compare(const void *a, const void *b)
{
    const textwin_tab_struct *tab_one, *tab_two;

    tab_one = a;
    tab_two = b;

    if (tab_one->name && !tab_two->name) {
        return 1;
    } else if (!tab_one->name && tab_two->name) {
        return -1;
    } else if (tab_one->name && tab_two->name) {
        return strcmp(tab_one->name, tab_two->name);
    }

    return tab_one->type - tab_two->type;
}

size_t textwin_tab_name_to_id(const char *name)
{
    size_t i;

    for (i = 0; i < arraysize(textwin_tab_names); i++) {
        if (strcmp(textwin_tab_names[i], name) == 0) {
            return i + 1;

        }
    }

    return CHAT_TYPE_PRIVATE;
}

void textwin_tab_free(textwin_tab_struct *tab)
{
    if (tab->name) {
        efree(tab->name);
    }

    if (tab->entries) {
        efree(tab->entries);
    }

    if (tab->charnames) {
        efree(tab->charnames);
    }
}

void textwin_tab_remove(widgetdata *widget, const char *name)
{
    textwin_struct *textwin;
    size_t i;

    textwin = TEXTWIN(widget);

    for (i = 0; i < textwin->tabs_num; i++) {
        if (strcmp(TEXTWIN_TAB_NAME(&textwin->tabs[i]), name) != 0) {
            continue;
        }

        textwin_tab_free(&textwin->tabs[i]);

        for (i = i + 1; i < textwin->tabs_num; i++) {
            textwin->tabs[i - 1] = textwin->tabs[i];
        }

        textwin->tabs = erealloc(textwin->tabs, sizeof(*textwin->tabs) * (textwin->tabs_num - 1));
        textwin->tabs_num--;
        textwin->tab_selected = MIN(textwin->tab_selected, textwin->tabs_num - 1);
        textwin_readjust(widget);
        break;
    }
}

void textwin_tab_add(widgetdata *widget, const char *name)
{
    textwin_struct *textwin;
    int wd;
    char buf[MAX_BUF];

    textwin = TEXTWIN(widget);
    textwin->tabs = memory_reallocz(textwin->tabs, sizeof(*textwin->tabs) * textwin->tabs_num, sizeof(*textwin->tabs) * (textwin->tabs_num + 1));

    textwin->tabs[textwin->tabs_num].type = textwin_tab_name_to_id(name);

    if (!string_startswith(name, "[") && !string_endswith(name, "]")) {
        textwin->tabs[textwin->tabs_num].name = estrdup(name);
    }

    button_create(&textwin->tabs[textwin->tabs_num].button);
    wd = text_get_width(textwin->tabs[textwin->tabs_num].button.font, TEXTWIN_TAB_NAME(&textwin->tabs[textwin->tabs_num]), 0) + 10;
    snprintf(buf, sizeof(buf), "rectangle:%d,20,255;[border=widget_border %d 20]", wd, wd);
    textwin->tabs[textwin->tabs_num].button.texture = texture_get(TEXTURE_TYPE_SOFTWARE, buf);
    textwin->tabs[textwin->tabs_num].button.texture_over = textwin->tabs[textwin->tabs_num].button.texture_pressed = NULL;

    text_input_create(&textwin->tabs[textwin->tabs_num].text_input);
    text_input_set_font(&textwin->tabs[textwin->tabs_num].text_input, textwin->font);
    textwin->tabs[textwin->tabs_num].text_input.focus = 0;
    textwin->tabs[textwin->tabs_num].text_input.max = 250;
    textwin->tabs[textwin->tabs_num].text_input_history = text_input_history_create();
    text_input_set_history(&textwin->tabs[textwin->tabs_num].text_input, textwin->tabs[textwin->tabs_num].text_input_history);

    textwin->tabs_num++;

    qsort(textwin->tabs, textwin->tabs_num, sizeof(*textwin->tabs), textwin_tab_compare);
    textwin_readjust(widget);
}

int textwin_tab_find(widgetdata *widget, uint8_t type, const char *name, size_t *id)
{
    textwin_struct *textwin;

    textwin = TEXTWIN(widget);

    for (*id = 0; *id < textwin->tabs_num; (*id)++) {
        if (textwin->tabs[*id].type == type && (string_isempty(name) || (string_startswith(name, "[") && string_endswith(name, "]")) || (textwin->tabs[*id].name && strcmp(textwin->tabs[*id].name, name) == 0))) {
            return 1;
        }
    }

    return 0;
}

void textwin_tab_open(widgetdata *widget, const char *name)
{
    textwin_struct *textwin;
    size_t i;

    textwin = TEXTWIN(widget);

    if (textwin_tab_find(widget, CHAT_TYPE_PRIVATE, name, &i)) {
        textwin->tab_selected = i;
        textwin_readjust(widget);
    } else {
        textwin_tab_add(widget, name);
    }
}

void draw_info_tab(size_t type, const char *color, const char *str)
{
    text_info_struct info;
    StringBuffer *sb;
    char *name;
    widgetdata *widget;
    textwin_struct *textwin;
    uint32_t bottom;
    uint8_t found;
    size_t i;

    sb = stringbuffer_new();
    text_anchor_parse(&info, str);
    text_set_anchor_handle(text_anchor_handle);
    text_anchor_execute(&info, sb);
    text_set_anchor_handle(NULL);
    name = stringbuffer_finish(sb);

    if (!string_isempty(name)) {
        widgetdata *ignore_widget;

        ignore_widget = widget_find_create_id(BUDDY_ID, "ignore");

        if (widget_buddy_check(ignore_widget, name) != -1) {
            efree(name);
            return;
        }
    }

    for (widget = cur_widget[CHATWIN_ID]; widget; widget = widget->type_next) {
        textwin = TEXTWIN(widget);

        if (!textwin->tabs) {
            continue;
        }

        WIDGET_REDRAW(widget);
        bottom = SCROLL_BOTTOM(&textwin->scrollbar);
        found = 0;

        if (textwin_tab_find(widget, type, NULL, &i)) {
            textwin_tab_append(widget, i, type, color, str);
            found = 1;
        }

        if (!string_isempty(name) && textwin_tab_find(widget, type, name, &i)) {
            textwin_tab_append(widget, i, type, color, str);
            found = 1;
        }

        if (found) {
            if (textwin_tab_find(widget, CHAT_TYPE_ALL, NULL, &i)) {
                textwin_tab_append(widget, i, type, color, str);
            }
        }

        if (textwin->tabs[textwin->tab_selected].scroll_offset == bottom) {
            scrollbar_scroll_to(&textwin->scrollbar, SCROLL_BOTTOM(&textwin->scrollbar));
        }
    }

    efree(name);
}

/**
 * Draw info with format arguments.
 * @param flags Various flags, like color.
 * @param format Format arguments. */
void draw_info_format(const char *color, char *format, ...)
{
    char buf[HUGE_BUF * 2];
    va_list ap;

    va_start(ap, format);
    vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);

    draw_info_tab(CHAT_TYPE_GAME, color, buf);
}

/**
 * Add string to the text window.
 * @param flags Various flags, like color.
 * @param str The string. */
void draw_info(const char *color, const char *str)
{
    draw_info_tab(CHAT_TYPE_GAME, color, str);
}

/**
 * Handle ctrl+C for textwin widget
 * @param widget The textwin widget. If NULL, try to find the first one
 * in the priority list. */
void textwin_handle_copy(widgetdata *widget)
{
    int64_t start, end;
    textwin_struct *textwin;
    char *str, *cp;
    size_t i, pos;

    if (!widget) {
        widget = widget_find(NULL, CHATWIN_ID, NULL, NULL);

        if (!widget) {
            return;
        }
    }

    textwin = TEXTWIN(widget);

    start = textwin->selection_start;
    end = textwin->selection_end;

    if (end < start) {
        start = textwin->selection_end;
        end = textwin->selection_start;
    }

    if (end - start <= 0) {
        return;
    }

    /* Get the string to copy, depending on the start and end positions. */
    str = emalloc(sizeof(char) * (end - start + 1 + 1));
    memcpy(str, textwin->tabs[textwin->tab_selected].entries + start, end - start + 1);
    str[end - start + 1] = '\0';

    cp = emalloc(sizeof(char) * (end - start + 1 + 1));
    i = 0;

    /* Remove the special \r color changers. */
    for (pos = 0; pos < (size_t) (end - start + 1); pos++) {
        cp[i++] = str[pos];
    }

    cp[i] = '\0';
    cp = text_strip_markup(cp, NULL, 1);

    x11_clipboard_set(SDL_display, SDL_window, cp);
    efree(str);
    efree(cp);
}

/**
 * Display the message text window, without handling scrollbar/mouse
 * actions.
 * @param surface Surface to draw on.
 * @param x X position.
 * @param y Y position.
 * @param w Maximum width.
 * @param h Maximum height. */
void textwin_show(SDL_Surface *surface, int x, int y, int w, int h)
{
    widgetdata *widget;
    textwin_struct *textwin;
    size_t i;
    SDL_Rect box;
    int scroll;

    for (widget = cur_widget[CHATWIN_ID]; widget; widget = widget->type_next) {
        textwin = TEXTWIN(widget);

        for (i = 0; i < textwin->tabs_num; i++) {
            if (textwin->tabs[i].type == CHAT_TYPE_GAME &&
                    textwin->tabs[i].entries != NULL) {
                box.w = w - 3;
                box.h = 0;
                box.x = 0;
                box.y = 0;
                text_show(NULL, textwin->font, textwin->tabs[i].entries, 3, 0, COLOR_BLACK, TEXTWIN_TEXT_FLAGS(widget) | TEXT_LINES_CALC, &box);
                scroll = box.h;

                box.x = x;
                box.y = y;
                box.w = w;
                box.h = h;
                SDL_FillRect(surface, &box, SDL_MapRGB(surface->format, 0, 0, 0));
                draw_frame(surface, x, y, box.w, box.h);

                box.w = w - 3;
                box.h = h;

                box.y = MAX(0, scroll - (h / FONT_HEIGHT(textwin->font)));

                text_show(surface, textwin->font, textwin->tabs[i].entries, x + 3, y + 1, COLOR_BLACK, TEXTWIN_TEXT_FLAGS(widget) | TEXT_LINES_SKIP, &box);
                break;
            }
        }
    }
}

int textwin_tabs_height(widgetdata *widget)
{
    textwin_struct *textwin;
    int button_x, button_y;
    size_t i;

    textwin = TEXTWIN(widget);
    button_x = button_y = 0;

    if (textwin->tabs_num <= 1) {
        return 0;
    }

    for (i = 0; i < textwin->tabs_num; i++) {
        if (button_x + texture_surface(textwin->tabs[i].button.texture)->w > widget->w) {
            button_x = 0;
            button_y += TEXTWIN_TAB_HEIGHT - 1;
        }

        button_x += texture_surface(textwin->tabs[i].button.texture)->w - 1;
    }

    return button_y + TEXTWIN_TAB_HEIGHT - 1;
}

/**
 * Initialize text window scrollbar.
 * @param widget The text window. */
void textwin_create_scrollbar(widgetdata *widget)
{
    textwin_struct *textwin = TEXTWIN(widget);

    scrollbar_create(&textwin->scrollbar, TEXTWIN_SCROLLBAR_WIDTH(widget), TEXTWIN_SCROLLBAR_HEIGHT(widget), &textwin->tabs[textwin->tab_selected].scroll_offset, &textwin->tabs[textwin->tab_selected].num_lines, TEXTWIN_ROWS_VISIBLE(widget));
    textwin->scrollbar.redraw = &widget->redraw;
}

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
    SDL_Rect box;
    textwin_struct *textwin = TEXTWIN(widget);
    int alpha;

    /* Sanity check. */
    if (!textwin) {
        return;
    }

    /* If we don't have a backbuffer, create it */
    if (!widget->surface || widget->w != widget->surface->w || widget->h != widget->surface->h) {
        if (widget->surface) {
            SDL_FreeSurface(widget->surface);
        }

        widget->surface = SDL_CreateRGBSurface(get_video_flags(), widget->w, widget->h, video_get_bpp(), 0, 0, 0, 0);
        SDL_SetColorKey(widget->surface, SDL_SRCCOLORKEY | SDL_ANYFORMAT, 0);
        textwin_readjust(widget);
    }

    if ((alpha = setting_get_int(OPT_CAT_CLIENT, OPT_TEXT_WINDOW_TRANSPARENCY))) {
        SDL_Color bg_color;

        if (text_color_parse(setting_get_str(OPT_CAT_CLIENT, OPT_TEXT_WINDOW_BG_COLOR), &bg_color)) {
            filledRectAlpha(ScreenSurface, widget->x, widget->y, widget->x + widget->w - 1, widget->y + widget->h - 1, ((uint32_t) bg_color.r << 24) | ((uint32_t) bg_color.g << 16) | ((uint32_t) bg_color.b << 8) | (uint32_t) alpha);
        }
    }

    /* Let's draw the widgets in the backbuffer */
    if (widget->redraw) {
        SDL_FillRect(widget->surface, NULL, 0);

        if (textwin->tabs) {
            int yadjust;

            yadjust = 0;

            if (textwin->tabs_num > 1) {
                size_t i;
                int button_x, button_y;

                button_x = button_y = 0;

                for (i = 0; i < textwin->tabs_num; i++) {
                    if (button_x + texture_surface(textwin->tabs[i].button.texture)->w > widget->w) {
                        button_x = 0;
                        button_y += TEXTWIN_TAB_HEIGHT - 1;
                    }

                    if (textwin->tabs[i].unread) {
                        textwin->tabs[i].button.color = COLOR_NAVY;
                    } else {
                        textwin->tabs[i].button.color = COLOR_WHITE;
                    }

                    textwin->tabs[i].button.x = button_x;
                    textwin->tabs[i].button.y = button_y;
                    textwin->tabs[i].button.surface = widget->surface;
                    button_set_parent(&textwin->tabs[i].button, widget->x, widget->y);
                    button_show(&textwin->tabs[i].button, TEXTWIN_TAB_NAME(&textwin->tabs[i]));

                    button_x += texture_surface(textwin->tabs[i].button.texture)->w - 1;
                }

                yadjust = button_y + TEXTWIN_TAB_HEIGHT;
                box.w = widget->w;
                box.h = 1;
                surface_show_fill(widget->surface, 0, yadjust - box.h, NULL, TEXTURE_CLIENT("widget_border"), &box);
                yadjust -= 1;
            }

            /* Show the text entries, if any. */
            if (textwin->tabs[textwin->tab_selected].entries) {
                SDL_Rect box_text;
                StringBuffer *sb;
                char *charnames;

                sb = stringbuffer_new();

                box_text.w = TEXTWIN_TEXT_WIDTH(widget);
                box_text.h = TEXTWIN_TEXT_HEIGHT(widget);
                box_text.y = textwin->tabs[textwin->tab_selected].scroll_offset;
                text_set_selection(&textwin->selection_start, &textwin->selection_end, &textwin->selection_started);
                text_set_anchor_handle(text_anchor_handle);
                text_set_anchor_info(sb);
                text_show(widget->surface, textwin->font, textwin->tabs[textwin->tab_selected].entries, TEXTWIN_TEXT_STARTX(widget), TEXTWIN_TEXT_STARTY(widget) + yadjust, COLOR_BLACK, TEXTWIN_TEXT_FLAGS(widget) | TEXT_LINES_SKIP, &box_text);
                text_set_anchor_info(NULL);
                text_set_anchor_handle(NULL);
                text_set_selection(NULL, NULL, NULL);

                charnames = stringbuffer_finish(sb);

                if (textwin->tabs[textwin->tab_selected].charnames) {
                    efree(textwin->tabs[textwin->tab_selected].charnames);
                }

                textwin->tabs[textwin->tab_selected].charnames = charnames;
            }

            if (textwin_tab_commands[textwin->tabs[textwin->tab_selected].type - 1]) {
                text_input_set_parent(&textwin->tabs[textwin->tab_selected].text_input, widget->x, widget->y);
                textwin->tabs[textwin->tab_selected].text_input.coords.w = TEXTWIN_TEXT_INPUT_WIDTH(widget);
                text_input_show(&textwin->tabs[textwin->tab_selected].text_input, widget->surface, TEXTWIN_TEXT_INPUT_STARTX(widget), TEXTWIN_TEXT_INPUT_STARTY(widget) + yadjust);
            }

            textwin->scrollbar.max_lines = TEXTWIN_ROWS_VISIBLE(widget);
            textwin->scrollbar.px = widget->x;
            textwin->scrollbar.py = widget->y;
            scrollbar_show(&textwin->scrollbar, widget->surface, widget->w - 1 - textwin->scrollbar.background.w, TEXTWIN_TEXT_STARTY(widget) + yadjust);
        }
    }

    if (scrollbar_need_redraw(&textwin->scrollbar)) {
        widget->redraw++;
    }

    box.x = widget->x;
    box.y = widget->y;
    SDL_BlitSurface(widget->surface, NULL, ScreenSurface, &box);

    box.w = widget->w;
    box.h = widget->h;
    border_create_texture(ScreenSurface, &box, 1, TEXTURE_CLIENT("widget_border"));
}

/** @copydoc widgetdata::background_func */
static void widget_background(widgetdata *widget, int draw)
{
    textwin_struct *textwin;
    size_t i;

    textwin = TEXTWIN(widget);

    for (i = 0; i < textwin->tabs_num; i++) {
        if (button_need_redraw(&textwin->tabs[i].button)) {
            WIDGET_REDRAW(widget);
        }
    }
}

/** @copydoc widgetdata::event_func */
static int widget_event(widgetdata *widget, SDL_Event *event)
{
    textwin_struct *textwin = TEXTWIN(widget);

    if (event->type == SDL_MOUSEBUTTONDOWN) {
        widgetdata *tmp;
        text_input_struct *text_input;

        if (textwin->tabs != NULL) {
            text_input = &textwin->tabs[textwin->tab_selected].text_input;

            if (text_input->focus == 0 && *text_input->str != '\0') {
                text_input->focus = 1;
                WIDGET_REDRAW(widget);
            }
        }

        for (tmp = cur_widget[CHATWIN_ID]; tmp != NULL; tmp = tmp->type_next) {
            if (tmp != widget && TEXTWIN(tmp)->tabs != NULL) {
                text_input = &TEXTWIN(tmp)->tabs[TEXTWIN(tmp)->tab_selected].text_input;

                if (text_input->focus == 1) {
                    text_input->focus = 0;
                    WIDGET_REDRAW(tmp);
                }
            }
        }
    }

    if (textwin->tabs_num > 1 && (event->type == SDL_MOUSEMOTION || event->type == SDL_MOUSEBUTTONDOWN)) {
        size_t i;

        for (i = 0; i < textwin->tabs_num; i++) {
            if (button_event(&textwin->tabs[i].button, event)) {
                textwin->tab_selected = i;
                textwin_readjust(widget);
                return 1;
            }
        }
    }

    if (textwin->tabs != NULL && event->type == SDL_KEYDOWN && textwin->tabs[textwin->tab_selected].text_input.focus == 1 && widget == widget_find(NULL, CHATWIN_ID, NULL, NULL)) {
        if (IS_ENTER(event->key.keysym.sym) && *(textwin->tabs[textwin->tab_selected].text_input.str) != '\0') {
            StringBuffer *sb;
            char *cp, *str;

            sb = stringbuffer_new();

            if (*(textwin->tabs[textwin->tab_selected].text_input.str) != '/') {
                if (textwin->tabs[textwin->tab_selected].name != NULL) {
                    stringbuffer_append_printf(sb, "/tell \"%s\" ", textwin->tabs[textwin->tab_selected].name);
                } else {
                    stringbuffer_append_printf(sb, "/%s ", textwin_tab_commands[textwin->tabs[textwin->tab_selected].type - 1]);
                }
            }

            str = text_escape_markup(textwin->tabs[textwin->tab_selected].text_input.str);
            stringbuffer_append_string(sb, str);
            efree(str);

            cp = stringbuffer_finish(sb);
            send_command_check(cp);
            efree(cp);
        }

        if (event->key.keysym.sym == SDLK_ESCAPE) {
            textwin->tabs[textwin->tab_selected].text_input.focus = 0;
            text_input_reset(&textwin->tabs[textwin->tab_selected].text_input);
            WIDGET_REDRAW(widget);
            return 1;
        } else if (event->key.keysym.sym == SDLK_TAB) {
            help_handle_tabulator(&textwin->tabs[textwin->tab_selected].text_input);
            WIDGET_REDRAW(widget);
            return 1;
        } else if (text_input_event(&textwin->tabs[textwin->tab_selected].text_input, event)) {
            if (IS_ENTER(event->key.keysym.sym)) {
                text_input_reset(&textwin->tabs[textwin->tab_selected].text_input);
                textwin->tabs[textwin->tab_selected].text_input.focus = 0;
            }

            WIDGET_REDRAW(widget);
            return 1;
        }
    }

    if (scrollbar_event(&textwin->scrollbar, event)) {
        WIDGET_REDRAW(widget);
        return 1;
    }

    if (event->type == SDL_MOUSEMOTION) {
        WIDGET_REDRAW(widget);
    }

    if (event->button.button == SDL_BUTTON_LEFT) {
        if (event->type == SDL_MOUSEBUTTONUP) {
            return 1;
        } else if (event->type == SDL_MOUSEBUTTONDOWN) {
            textwin->selection_started = 0;
            textwin->selection_start = -1;
            textwin->selection_end = -1;
            WIDGET_REDRAW(widget);
            return 1;
        } else if (event->type == SDL_MOUSEMOTION) {
            textwin->selection_started = 1;
            return 1;
        }
    }

    if (event->type == SDL_MOUSEBUTTONDOWN) {
        if (event->button.button == SDL_BUTTON_WHEELUP) {
            scrollbar_scroll_adjust(&textwin->scrollbar, -1);
            return 1;
        } else if (event->button.button == SDL_BUTTON_WHEELDOWN) {
            scrollbar_scroll_adjust(&textwin->scrollbar, 1);
            return 1;
        }
    }

    if (event->type == SDL_KEYDOWN) {
        if (event->key.keysym.sym == SDLK_PAGEUP) {
            scrollbar_scroll_adjust(&textwin->scrollbar, -TEXTWIN_ROWS_VISIBLE(widget));
            return 1;
        } else if (event->key.keysym.sym == SDLK_PAGEDOWN) {
            scrollbar_scroll_adjust(&textwin->scrollbar, TEXTWIN_ROWS_VISIBLE(widget));
            return 1;
        }
    }

    return 0;
}

/** @copydoc widgetdata::deinit_func */
static void widget_deinit(widgetdata *widget)
{
    textwin_struct *textwin;
    size_t i;

    textwin = TEXTWIN(widget);

    for (i = 0; i < textwin->tabs_num; i++) {
        textwin_tab_free(&textwin->tabs[i]);
    }

    if (textwin->tabs) {
        efree(textwin->tabs);
    }

    font_free(textwin->font);
}

/** @copydoc widgetdata::load_func */
static int widget_load(widgetdata *widget, const char *keyword, const char *parameter)
{
    textwin_struct *textwin;

    textwin = TEXTWIN(widget);

    if (strcmp(keyword, "font") == 0) {
        char font_name[MAX_BUF];
        int font_size;

        if (sscanf(parameter, "%s %d", font_name, &font_size) == 2) {
            font_free(textwin->font);
            textwin->font = font_get(font_name, font_size);
            return 1;
        }
    } else if (strcmp(keyword, "tabs") == 0) {
        size_t pos;
        char word[MAX_BUF];

        pos = 0;

        while (string_get_word(parameter, &pos, ':', word, sizeof(word), 0)) {
            if (string_startswith(word, "/")) {
                textwin_tab_remove(widget, word + 1);
            } else {
                if (strcmp(word, "*") == 0) {
                    size_t i;

                    for (i = 0; i < arraysize(textwin_tab_names); i++) {
                        textwin_tab_add(widget, textwin_tab_names[i]);
                    }
                } else {
                    textwin_tab_add(widget, word);
                }
            }
        }

        return 1;
    } else if (strcmp(keyword, "timestamps") == 0) {
        KEYWORD_TO_BOOLEAN(parameter, textwin->timestamps);
        return 1;
    }

    return 0;
}

/** @copydoc widgetdata::save_func */
static void widget_save(widgetdata *widget, FILE *fp, const char *padding)
{
    textwin_struct *textwin;

    textwin = TEXTWIN(widget);

    fprintf(fp, "%sfont = %s %d\n", padding, textwin->font->name, textwin->font->size);
    fprintf(fp, "%stimestamps = %s\n", padding, textwin->timestamps ? "yes" : "no");

    if (textwin->tabs_num) {
        size_t i;

        fprintf(fp, "%stabs = ", padding);

        for (i = 0; i < textwin->tabs_num; i++) {
            fprintf(fp, "%s%s", i == 0 ? "" : ":", TEXTWIN_TAB_NAME(&textwin->tabs[i]));
        }

        fprintf(fp, "\n");
    }
}

static void menu_textwin_clear(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    textwin_struct *textwin;

    textwin = TEXTWIN(widget);
    efree(textwin->tabs[textwin->tab_selected].entries);
    textwin->tabs[textwin->tab_selected].entries = NULL;
    textwin->tabs[textwin->tab_selected].num_lines = textwin->tabs[textwin->tab_selected].entries_size = textwin->tabs[textwin->tab_selected].scroll_offset = 0;
    WIDGET_REDRAW(widget);
}

static void menu_textwin_copy(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    textwin_handle_copy(widget);
}

static void textwin_font_adjust(widgetdata *widget, int adjust)
{
    textwin_struct *textwin;
    font_struct *font;
    size_t i;

    textwin = TEXTWIN(widget);
    font = font_get_size(textwin->font, adjust);

    if (font == NULL) {
        return;
    }

    for (i = 0; i < textwin->tabs_num; i++) {
        text_input_set_font(&textwin->tabs[i].text_input, font);
    }

    font_free(textwin->font);
    FONT_INCREF(font);
    textwin->font = font;
    textwin_readjust(widget);
    WIDGET_REDRAW(widget);
}

static void menu_textwin_font_inc(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    textwin_font_adjust(widget, 1);
}

static void menu_textwin_font_dec(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    textwin_font_adjust(widget, -1);
}

static void menu_textwin_tabs_one(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    widgetdata *tmp;
    _widget_label *label;
    size_t id;
    char *cp;

    for (tmp = menuitem->inv; tmp; tmp = tmp->next) {
        if (tmp->type == LABEL_ID) {
            label = LABEL(menuitem->inv);
            cp = text_strip_markup(label->text, NULL, 0);

            if (textwin_tab_find(widget, textwin_tab_name_to_id(cp), cp, &id)) {
                textwin_tab_remove(widget, cp);
            } else {
                textwin_tab_add(widget, cp);
            }

            efree(cp);
            WIDGET_REDRAW(widget);
            break;
        }
    }
}

static void menu_textwin_tabs(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    widgetdata *tmp, *tmp2, *submenu;
    textwin_struct *textwin;
    size_t i, id;
    uint8_t found;
    _widget_label *label;
    char *cp;

    submenu = MENU(menuitem->env)->submenu;

    for (i = 0; i < arraysize(textwin_tab_names); i++) {
        if (i + 1 == CHAT_TYPE_OPERATOR && !setting_get_int(OPT_CAT_DEVEL, OPT_OPERATOR)) {
            continue;
        }

        cp = text_escape_markup(textwin_tab_names[i]);
        add_menuitem(submenu, cp, &menu_textwin_tabs_one, MENU_CHECKBOX, textwin_tab_find(widget, i + 1, NULL, &id));
        efree(cp);
    }

    for (tmp = cur_widget[CHATWIN_ID]; tmp; tmp = tmp->type_next) {
        textwin = TEXTWIN(tmp);

        for (i = 0; i < textwin->tabs_num; i++) {
            if (!textwin->tabs[i].name) {
                continue;
            }

            found = 0;

            for (tmp2 = submenu->inv; tmp2; tmp2 = tmp2->next) {
                if (tmp2->inv->type == LABEL_ID) {
                    label = LABEL(tmp2->inv);

                    if (strcmp(label->text, textwin->tabs[i].name) == 0) {
                        found = 1;
                        break;
                    }
                }
            }

            if (!found) {
                add_menuitem(submenu, textwin->tabs[i].name, &menu_textwin_tabs_one, MENU_CHECKBOX, tmp == widget);
            }
        }
    }
}

/** @copydoc text_anchor_handle_func */
static int text_anchor_handle_players_tab(const char *anchor_action, const char *buf, size_t len, void *custom_data)
{
    widgetdata *widget;

    if (strcmp(anchor_action, "#buddy") == 0 || strcmp(anchor_action, "#ignore") == 0) {
        widget = widget_find_create_id(BUDDY_ID, anchor_action + 1);

        if (widget_buddy_check(widget, buf) == -1) {
            widget_buddy_add(widget, buf, 1);
        } else {
            widget_buddy_remove(widget, buf);
        }

        return 1;
    } else if (strcmp(anchor_action, "#opentab") == 0) {
        widget = widget_find(NULL, CHATWIN_ID, NULL, NULL);
        textwin_tab_open(widget, buf);

        return 1;
    }

    return 0;
}

static void menu_textwin_players_one_tab(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    widgetdata *tmp;
    _widget_label *label;
    text_info_struct info;

    for (tmp = menuitem->inv; tmp; tmp = tmp->next) {
        if (tmp->type == LABEL_ID) {
            label = LABEL(menuitem->inv);

            text_anchor_parse(&info, label->text);
            text_set_anchor_handle(text_anchor_handle_players_tab);
            text_anchor_execute(&info, NULL);
            text_set_anchor_handle(NULL);
        }
    }
}

static void menu_textwin_players_one(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    widgetdata *submenu, *tmp;
    _widget_label *label;
    char *cp, buf[HUGE_BUF];

    submenu = MENU(menuitem->env)->submenu;

    for (tmp = menuitem->inv; tmp; tmp = tmp->next) {
        if (tmp->type == LABEL_ID) {
            label = LABEL(menuitem->inv);

            cp = string_sub(label->text, 0, -3);

            snprintf(buf, sizeof(buf), "[a=#buddy:%s]%s[/a]", cp, widget_buddy_check(widget_find(NULL, BUDDY_ID, "buddy", NULL), cp) == -1 ? "Add Buddy" : "Remove Buddy");
            add_menuitem(submenu, buf, &menu_textwin_players_one_tab, MENU_NORMAL, 0);

            snprintf(buf, sizeof(buf), "[a=#opentab:%s]Open Tab[/a]", cp);
            add_menuitem(submenu, buf, &menu_textwin_players_one_tab, MENU_NORMAL, 0);

            snprintf(buf, sizeof(buf), "[a=#ignore:%s]%s[/a]", cp, widget_buddy_check(widget_find(NULL, BUDDY_ID, "ignore", NULL), cp) == -1 ? "Ignore" : "Unignore");
            add_menuitem(submenu, buf, &menu_textwin_players_one_tab, MENU_NORMAL, 0);

            efree(cp);
            break;
        }
    }
}

static void menu_textwin_players(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
    textwin_struct *textwin;
    size_t pos;
    char charname[MAX_BUF], buf[HUGE_BUF];
    uint8_t found;
    widgetdata *tmp, *submenu;
    _widget_label *label;

    textwin = TEXTWIN(widget);
    submenu = MENU(menuitem->env)->submenu;
    pos = 0;

    if (textwin->tabs == NULL) {
        return;
    }

    while (string_get_word(textwin->tabs[textwin->tab_selected].charnames, &pos, ':', charname, sizeof(charname), 0)) {
        found = 0;

        snprintf(buf, sizeof(buf), "%s  >", charname);

        for (tmp = submenu->inv; tmp; tmp = tmp->next) {
            if (tmp->inv->type == LABEL_ID) {
                label = LABEL(tmp->inv);

                if (strcmp(label->text, buf) == 0) {
                    found = 1;
                    break;
                }
            }
        }

        if (!found) {
            add_menuitem(submenu, buf, &menu_textwin_players_one, MENU_SUBMENU, 0);
        }
    }
}

static int widget_menu_handle(widgetdata *widget, SDL_Event *event)
{
    widgetdata *menu;

    menu = create_menu(event->motion.x, event->motion.y, widget);

    widget_menu_standard_items(widget, menu);
    add_menuitem(menu, "New Window", &menu_create_widget, MENU_NORMAL, 0);
    add_menuitem(menu, "Remove Window", &menu_remove_widget, MENU_NORMAL, 0);

    add_menuitem(menu, "Clear", &menu_textwin_clear, MENU_NORMAL, 0);
    add_menuitem(menu, "Copy", &menu_textwin_copy, MENU_NORMAL, 0);
    add_menuitem(menu, "Increase Font Size", &menu_textwin_font_inc, MENU_NORMAL, 0);
    add_menuitem(menu, "Decrease Font Size", &menu_textwin_font_dec, MENU_NORMAL, 0);
    add_menuitem(menu, "Tabs  >", &menu_textwin_tabs, MENU_SUBMENU, 0);
    add_menuitem(menu, "Players  >", &menu_textwin_players, MENU_SUBMENU, 0);

    menu_finalize(menu);

    return 1;
}

void widget_textwin_init(widgetdata *widget)
{
    textwin_struct *textwin;

    textwin = ecalloc(1, sizeof(*textwin));
    textwin->font = font_get("arial", 11);
    textwin->selection_start = -1;
    textwin->selection_end = -1;

    widget->draw_func = widget_draw;
    widget->background_func = widget_background;
    widget->event_func = widget_event;
    widget->save_func = widget_save;
    widget->load_func = widget_load;
    widget->deinit_func = widget_deinit;
    widget->menu_handle_func = widget_menu_handle;
    widget->subwidget = textwin;

    textwin_create_scrollbar(widget);
}

void widget_textwin_handle_console(const char *text)
{
    widgetdata *widget;
    textwin_struct *textwin;

    widget = NULL;

    while (widget == NULL || widget->next != NULL) {
        widget = widget_find(widget != NULL ? widget->next : NULL, CHATWIN_ID, NULL, NULL);

        if (widget == NULL) {
            break;
        }

        textwin = TEXTWIN(widget);

        if (textwin->tabs != NULL && textwin_tab_commands[textwin->tabs[textwin->tab_selected].type - 1] != NULL) {
            break;
        }
    }

    if (widget == NULL) {
        logger_print(LOG(BUG), "Failed to find a text window.");
        return;
    }

    text_input_reset(&textwin->tabs[textwin->tab_selected].text_input);
    textwin->tabs[textwin->tab_selected].text_input.focus = 1;

    if (text != NULL) {
        text_input_set(&textwin->tabs[textwin->tab_selected].text_input, text);
    }

    SetPriorityWidget(widget);
    WIDGET_REDRAW(widget);
}
