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
 * Text drawing API. Uses SDL_ttf for rendering.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * If 1, all text shown using 'box' parameter of text_show() for max
 * width/height will have a frame around it. */
static uint8 text_debug = 0;

/**
 * If not 0, ::text_anchor_help is used to open the help GUI after
 * finishing drawing.
 *
 * This is used so the text being drawn is not changed in the middle of
 * the drawing by clicking on a link. */
static uint8 text_anchor_help_clicked = 0;

/** Help GUI to open if ::text_anchor_help_clicked is 1. */
static char text_anchor_help[HUGE_BUF];

/** Mouse X offset. */
static int text_offset_mx = -1;
/** Mouse Y offset. */
static int text_offset_my = -1;

/** Pointer to integer holding the selection start. */
static sint64 *selection_start = NULL;
/** Pointer to integer holding the selection end. */
static sint64 *selection_end = NULL;
/** If 1, selection start has been set, and the end should be updated next. */
static uint8 *selection_started = NULL;

/** Default link color. */
static SDL_Color text_link_color_default = {96, 160, 255, 0};
/** Current text link color. */
static SDL_Color text_link_color = {0, 0, 0, 0};

/** @copydoc text_anchor_handle_func */
static text_anchor_handle_func text_anchor_handle = NULL;

/**
 * If non-NULL, when blitting text inside an anchor tag, anchor execution
 * function will be called with this pointer as the optional custom data
 * argument. */
static void *text_anchor_info_ptr = NULL;

/** All the usable fonts. */
font_struct fonts[FONTS_MAX] =
{
    {"fonts/vera/sans.ttf", 7, NULL, 0},
    {"fonts/vera/sans.ttf", 8, NULL, 0},
    {"fonts/vera/sans.ttf", 9, NULL, 0},
    {"fonts/vera/sans.ttf", 10, NULL, 0},
    {"fonts/vera/sans.ttf", 11, NULL, 0},
    {"fonts/vera/sans.ttf", 12, NULL, 0},
    {"fonts/vera/sans.ttf", 13, NULL, 0},
    {"fonts/vera/sans.ttf", 14, NULL, 0},
    {"fonts/vera/sans.ttf", 15, NULL, 0},
    {"fonts/vera/sans.ttf", 16, NULL, 0},
    {"fonts/vera/sans.ttf", 18, NULL, 0},
    {"fonts/vera/sans.ttf", 20, NULL, 0},
    {"fonts/vera/serif.ttf", 8, NULL, 0},
    {"fonts/vera/serif.ttf", 10, NULL, 0},
    {"fonts/vera/serif.ttf", 12, NULL, 0},
    {"fonts/vera/serif.ttf", 14, NULL, 0},
    {"fonts/vera/serif.ttf", 16, NULL, 0},
    {"fonts/vera/serif.ttf", 18, NULL, 0},
    {"fonts/vera/serif.ttf", 20, NULL, 0},
    {"fonts/vera/serif.ttf", 22, NULL, 0},
    {"fonts/vera/serif.ttf", 24, NULL, 0},
    {"fonts/vera/serif.ttf", 26, NULL, 0},
    {"fonts/vera/serif.ttf", 28, NULL, 0},
    {"fonts/vera/serif.ttf", 30, NULL, 0},
    {"fonts/vera/serif.ttf", 32, NULL, 0},
    {"fonts/vera/serif.ttf", 34, NULL, 0},
    {"fonts/vera/serif.ttf", 36, NULL, 0},
    {"fonts/vera/serif.ttf", 38, NULL, 0},
    {"fonts/vera/serif.ttf", 40, NULL, 0},
    {"fonts/vera/mono.ttf", 8, NULL, 0},
    {"fonts/vera/mono.ttf", 9, NULL, 0},
    {"fonts/vera/mono.ttf", 10, NULL, 0},
    {"fonts/vera/mono.ttf", 12, NULL, 0},
    {"fonts/vera/mono.ttf", 14, NULL, 0},
    {"fonts/vera/mono.ttf", 16, NULL, 0},
    {"fonts/vera/mono.ttf", 18, NULL, 0},
    {"fonts/vera/mono.ttf", 20, NULL, 0},
    {"fonts/arial.ttf", 8, NULL, 0},
    {"fonts/arial.ttf", 10, NULL, 0},
    {"fonts/arial.ttf", 11, NULL, 0},
    {"fonts/arial.ttf", 12, NULL, 0},
    {"fonts/arial.ttf", 13, NULL, 0},
    {"fonts/arial.ttf", 14, NULL, 0},
    {"fonts/arial.ttf", 15, NULL, 0},
    {"fonts/arial.ttf", 16, NULL, 0},
    {"fonts/arial.ttf", 18, NULL, 0},
    {"fonts/arial.ttf", 20, NULL, 0},
    {"fonts/logisoso.ttf", 8, NULL, 0},
    {"fonts/logisoso.ttf", 10, NULL, 0},
    {"fonts/logisoso.ttf", 12, NULL, 0},
    {"fonts/logisoso.ttf", 14, NULL, 0},
    {"fonts/logisoso.ttf", 16, NULL, 0},
    {"fonts/logisoso.ttf", 18, NULL, 0},
    {"fonts/logisoso.ttf", 20, NULL, 0},
    {"fonts/fanwood.otf", 8, NULL, 0},
    {"fonts/fanwood.otf", 10, NULL, 0},
    {"fonts/fanwood.otf", 12, NULL, 0},
    {"fonts/fanwood.otf", 14, NULL, 0},
    {"fonts/fanwood.otf", 16, NULL, 0},
    {"fonts/fanwood.otf", 18, NULL, 0},
    {"fonts/fanwood.otf", 20, NULL, 0},
    {"fonts/courier.otf", 8, NULL, 0},
    {"fonts/courier.otf", 10, NULL, 0},
    {"fonts/courier.otf", 12, NULL, 0},
    {"fonts/courier.otf", 14, NULL, 0},
    {"fonts/courier.otf", 16, NULL, 0},
    {"fonts/courier.otf", 18, NULL, 0},
    {"fonts/courier.otf", 20, NULL, 0},
    {"fonts/pecita.otf", 8, NULL, 0},
    {"fonts/pecita.otf", 10, NULL, 0},
    {"fonts/pecita.otf", 12, NULL, 0},
    {"fonts/pecita.otf", 14, NULL, 0},
    {"fonts/pecita.otf", 16, NULL, 0},
    {"fonts/pecita.otf", 18, NULL, 0},
    {"fonts/pecita.otf", 20, NULL, 0}
};

/**
 * Initialize the text API. Should only be done once. */
void text_init(void)
{
    size_t i;
    TTF_Font *font;

    TTF_Init();

    for (i = 0; i < FONTS_MAX; i++) {
        font = TTF_OpenFont_wrapper(fonts[i].path, fonts[i].size);

        if (!font) {
            logger_print(LOG(ERROR), "Unable to load font (%s): %s", fonts[i].path, TTF_GetError());
            exit(1);
        }

        fonts[i].font = font;
        fonts[i].height = TTF_FontLineSkip(font);
    }

    text_link_color = text_link_color_default;
}

/**
 * Deinitializes the text API. */
void text_deinit(void)
{
    size_t i;

    for (i = 0; i < FONTS_MAX; i++) {
        TTF_CloseFont(fonts[i].font);
    }

    TTF_Quit();
}

/**
 * If text_show() is called on surface that is not ScreenSurface, you
 * must use this to set mouse X/Y detection offset, so things like links
 * will work correctly.
 *
 * Note that this is not required for widget surfaces, as it's done
 * automatically by searching the widgets for the surface that is being
 * used.
 * @param x X position of the surface.
 * @param y Y position of the surface. */
void text_offset_set(int x, int y)
{
    text_offset_mx = x;
    text_offset_my = y;
}

/**
 * Reset the text offset. This must be done after text_offset_set() and
 * text_show() calls eventually, */
void text_offset_reset(void)
{
    text_offset_mx = text_offset_my = -1;
}

/**
 * Set color to use for links. Will be reset to default color after the
 * next call to string rendering is done.
 * @param r Red.
 * @param g Green.
 * @param b Blue. */
void text_color_set(int r, int g, int b)
{
    text_link_color.r = r;
    text_link_color.g = g;
    text_link_color.b = b;
}

/**
 * Allow grabbing a text selection.
 * @param start Pointer that will be used to store start of selection.
 * @param end Pointer that will be used to store start of selection.
 * @param started Pointer that is used to determine whether to store
 * selection data in start or end.
 * @note You must call this with all arguments set to NULL after your
 * call to string drawing routines. */
void text_set_selection(sint64 *start, sint64 *end, uint8 *started)
{
    selection_start = start;
    selection_end = end;
    selection_started = started;
}

/**
 * Set the anchor handler function.
 * @param func The function. */
void text_set_anchor_handle(text_anchor_handle_func func)
{
    text_anchor_handle = func;
}

/**
 * Set the value of ::text_anchor_info_ptr.
 * @param ptr Value to set. */
void text_set_anchor_info(void *ptr)
{
    text_anchor_info_ptr = ptr;
}

/**
 * Get font's filename; removes the path/to/fontdir part from the font's
 * path and returns it.
 * @param font Font ID.
 * @return The filename. */
const char *get_font_filename(int font)
{
    const char *cp;

    cp = strrchr(fonts[font].path, '/');

    if (!cp) {
        cp = fonts[font].path;
    }
    else {
        cp++;
    }

    return cp;
}

/**
 * Get font's ID from its xxx.ttf name (not including path) and the pixel
 * size.
 * @param name The font name.
 * @param size The size.
 * @return The font ID, -1 if there is no such font. */
int get_font_id(const char *name, size_t size)
{
    size_t i;
    const char *cp;
    uint8 ext = strstr(name, ".ttf") || strstr(name, ".otf") ? 1 : 0;

    for (i = 0; i < FONTS_MAX; i++) {
        cp = strrchr(fonts[i].path, '/');

        if (!cp) {
            cp = fonts[i].path;
        }
        else {
            cp++;
        }

        if ((!strcmp(cp, name) || (!ext && !strncmp(cp, name, strlen(cp) - 4))) && fonts[i].size == size) {
            return i;
        }
    }

    return -1;
}

/**
 * Remove all markup tags, including their contents.
 *
 * Entities will also be replaced with their proper replacements.
 * @param buf Buffer containing the text with markup, from which to
 * remove tags.
 * @param[out] len Length of 'buf'. This will contain the length of the
 * new string, without markup tags. Can be NULL, in which case length of
 * the original string will be calculated automatically.
 * @param do_free If 1, will automatically free 'buf'.
 * @return Newly allocated string with markup removed, and entities
 * replaced. */
char *text_strip_markup(char *buf, size_t *buf_len, uint8 do_free)
{
    char *cp;
    size_t pos = 0, cp_pos = 0, len;
    uint8 in_tag = 0;

    if (buf_len) {
        len = *buf_len;
    }
    else {
        len = strlen(buf);
    }

    cp = malloc(sizeof(char) * (len + 1));

    while (pos < len) {
        if (buf[pos] == '[') {
            in_tag = 1;
        }
        else if (buf[pos] == ']') {
            in_tag = 0;
        }
        else if (buf[pos] == '<') {
            in_tag = 1;
        }
        else if (buf[pos] == '>') {
            in_tag = 0;
        }
        else if (!in_tag) {
            if (!strncmp(buf + pos, "&lt;", 4)) {
                cp[cp_pos++] = '<';
                pos += 3;
            }
            else if (!strncmp(buf + pos, "&gt;", 4)) {
                cp[cp_pos++] = '>';
                pos += 3;
            }
            else if (!strncmp(buf + pos, "&lsqb;", 6)) {
                cp[cp_pos++] = '[';
                pos += 5;
            }
            else if (!strncmp(buf + pos, "&rsqb;", 6)) {
                cp[cp_pos++] = ']';
                pos += 5;
            }
            else if (!strncmp(buf + pos, "&lbrack;", 7)) {
                cp[cp_pos++] = '[';
                pos += 6;
            }
            else if (!strncmp(buf + pos, "&rbrack;", 7)) {
                cp[cp_pos++] = ']';
                pos += 6;
            }
            else {
                cp[cp_pos++] = buf[pos];
            }
        }

        pos++;
    }

    cp[cp_pos] = '\0';

    if (do_free) {
        free(buf);
    }

    if (buf_len) {
        *buf_len = strlen(cp);
    }

    return cp;
}

/**
 * Escapes markup in 'buf'.
 * @param buf String to escape markup in.
 * @return New string with markup escaped; never NULL. Must be freed. */
char *text_escape_markup(const char *buf)
{
    StringBuffer *sb;

    sb = stringbuffer_new();

    while (*buf != '\0') {
        if (*buf == '[') {
            stringbuffer_append_string(sb, "&lsqb;");
        }
        else if (*buf == ']') {
            stringbuffer_append_string(sb, "&rsqb;");
        }
        else if (*buf == '<') {
            stringbuffer_append_string(sb, "&lt;");
        }
        else if (*buf == '>') {
            stringbuffer_append_string(sb, "&gt;");
        }
        else {
            stringbuffer_append_char(sb, *buf);
        }

        buf++;
    }

    return stringbuffer_finish(sb);
}

/**
 * Adjust mouse X/Y coordinates for mouse-related checks based on which
 * surface we're using.
 * @param surface The surface.
 * @param[out] mx Mouse X, may be modified.
 * @param[out] my Mouse Y, may be modified.
 * @return 1 if mouse-related checks can happen, 0 otherwise. */
static int text_adjust_coords(SDL_Surface *surface, int *mx, int *my)
{
    if (popup_get_head() && surface != popup_get_head()->surface) {
        return 0;
    }

    if (surface == ScreenSurface) {
        return 1;
    }

    if (text_offset_mx != -1 || text_offset_my != -1) {
        if (text_offset_mx != -1) {
            *mx -= text_offset_mx;
        }

        if (text_offset_my != -1) {
            *my -= text_offset_my;
        }
    }
    else {
        widgetdata *widget = widget_find(NULL, -1, NULL, surface);

        if (widget) {
            *mx -= widget->x;
            *my -= widget->y;
        }
    }

    return 1;
}

/**
 * Parse the given string as a HTML notation color, and store the RGB
 * values in 'color'.
 * @param color_notation The HTML notation to parse.
 * @param color Where the RGB values will be stored.
 * @return 1 if the notation was parsed successfully, 0 otherwise. */
int text_color_parse(const char *color_notation, SDL_Color *color)
{
    uint32 r, g, b;

    if (*color_notation == '#') {
        color_notation++;
    }

    if (sscanf(color_notation, "%2X%2X%2X", &r, &g, &b) == 3) {
        if (color) {
            color->r = r;
            color->g = g;
            color->b = b;
        }

        return 1;
    }

    return 0;
}

/**
 * Execute anchor.
 * @param info Text info, should contain the anchor action and tag
 * position.
 * @param custom_data User-supplied data. Can be NULL. */
void text_anchor_execute(text_info_struct *info, void *custom_data)
{
    size_t len;
    char *buf, *pos;

    /* Sanity check. */
    if (!info->anchor_action || !info->anchor_tag) {
        return;
    }

    pos = strchr(info->anchor_action, ':');

    if (pos && pos + 1) {
        buf = strdup(pos + 1);
        len = strlen(buf);
        info->anchor_action[pos - info->anchor_action] = '\0';
    }
    else {
        char *tag;

        tag = strstr(info->anchor_tag, "[/a]");

        if (tag == NULL) {
            tag = strstr(info->anchor_tag, "</a>");

            if (tag == NULL) {
                return;
            }
        }

        /* Get the length of the text until the ending </a>. */
        len = tag - info->anchor_tag;
        /* Allocate a temporary buffer and copy the text until the
         * ending </a>, so we have the text between the anchor tags. */
        buf = malloc(len + 1);
        memcpy(buf, info->anchor_tag, len);
        buf[len] = '\0';
    }

    buf = text_strip_markup(buf, &len, 1);

    if (text_anchor_handle && text_anchor_handle(info->anchor_action, buf, len, custom_data)) {
    }
    /* No action specified. */
    else if (info->anchor_action[0] == '\0') {
        if (cpl.state == ST_PLAY) {
            /* It's not a command, so prepend "/say " to it. */
            if (buf[0] != '/') {
                /* Resize the buffer so it can hold 5 more bytes. */
                buf = realloc(buf, len + 5 + 1);
                /* Copy the existing bytes to the end, so we have 5
                 * we can use in the front. */
                memmove(buf + 5, buf, len + 1);
                /* Prepend "/say ". */
                memcpy(buf, "/say ", 5);
            }

            send_command_check(buf);
        }
    }
    /* Help GUI. */
    else if (!strcmp(info->anchor_action, "help")) {
        strncpy(text_anchor_help, buf, sizeof(text_anchor_help) - 1);
        text_anchor_help[sizeof(text_anchor_help) - 1] = '\0';
        text_anchor_help_clicked = 1;
    }
    else if (!strcmp(info->anchor_action, "url")) {
        browser_open(buf);
    }

    free(buf);
}

/**
 * Initialize the 'info' argument of text_show_character(). Should only be
 * called once.
 * @param info The text information to initialize. */
void text_show_character_init(text_info_struct *info)
{
    info->anchor_tag = NULL;
    info->anchor_action[0] = '\0';
    info->outline_color.r = info->outline_color.g = info->outline_color.b = 0;
    info->outline_show = 0;
    info->in_book_title = 0;
    info->used_alpha = 255;
    info->in_bold = info->in_italic = info->in_underline = info->in_strikethrough = 0;
    info->obscured = 0;
    info->calc_bold = 0;
    info->calc_font = -1;
    info->hcenter_y = 0;
    info->height = 0;
    info->start_x = 0;
    info->start_y = 0;
    info->highlight = 0;
    info->highlight_color.r = info->highlight_color.g = info->highlight_color.b = 0;
    info->tooltip_text[0] = '\0';
    info->flip = 0;
}

/**
 * Draw one character on the screen or parse markup (if applicable).
 * @param[out] font Font to use. One of @ref FONT_xxx.
 * @param orig_font Original font, used for the font tag.
 * @param surface Surface to draw on. If NULL, there is no drawing done,
 * but the return value is still calculated along with dest->w.
 * @param dest Destination, will have width (and x, if surface wasn't
 * NULL) updated.
 * @param cp String we are working on, cp[0] is the character to draw.
 * @param color Color to use.
 * @param orig_color Original color.
 * @param flags Flags as passed to text_show().
 * @return How many characters to jump. Usually 1, but can be more in
 * case of markup tags that need to be jumped over, since they are not
 * actually drawn. */
int text_show_character(int *font, int orig_font, SDL_Surface *surface, SDL_Rect *dest, const char *cp, SDL_Color *color, SDL_Color *orig_color, uint64 flags, SDL_Rect *box, int *x_adjust, text_info_struct *info)
{
    int width, minx, ret = 1, restore_font = -1, new_style;
    char c = *cp;
    uint8 remove_bold = 0;

    /* Doing markup? */
    if (flags & TEXT_MARKUP && (c == '[' || c == '<')) {
        const char *pos, *tag, *tag2;
        size_t tag_len;

        /* Get the position of the ending ']'. */
        pos = strchr(cp, c == '[' ? ']' : '>');

        if (pos == NULL) {
            return ret;
        }

        ret = 1 + (pos - cp);
        tag = cp + 1;
        tag_len = MAX(0, ret - 2);

        /* Color tag: [c=#RRGGBB] */
        if (tag_len >= 3 && strncmp(tag, "c=#", 3) == 0) {
            if (color && (surface || info->obscured) && !(flags & TEXT_NO_COLOR_CHANGE)) {
                uint32 r, g, b;
                int change_orig = 0;

                /* Parse the r,g,b colors. */
                if (sscanf(tag + 3, "%2X%2X%2X %d", &r, &g, &b, &change_orig) >= 3) {
                    color->r = r;
                    color->g = g;
                    color->b = b;

                    if (change_orig) {
                        SDL_color_copy(orig_color, color);
                    }
                }
            }
        }
        /* End of color tag. */
        else if (tag_len == 2 && strncmp(tag, "/c", tag_len) == 0) {
            if (color && (surface || info->obscured)) {
                SDL_color_copy(color, orig_color);
            }
        }
        /* Convenience tag to make string green. */
        else if (tag_len == 5 && strncmp(tag, "green", tag_len) == 0) {
            if (color && (surface || info->obscured) && !(flags & TEXT_NO_COLOR_CHANGE)) {
                color->r = 0;
                color->g = 255;
                color->b = 0;
            }
        }
        else if (tag_len == 6 && strncmp(tag, "/green", tag_len) == 0) {
            if (color && (surface || info->obscured)) {
                SDL_color_copy(color, orig_color);
            }
        }
        /* Convenience tag to make string yellow. */
        else if (tag_len == 6 && strncmp(tag, "yellow", tag_len) == 0) {
            if (color && (surface || info->obscured) && !(flags & TEXT_NO_COLOR_CHANGE)) {
                color->r = 255;
                color->g = 255;
                color->b = 0;
            }
        }
        else if (tag_len == 7 && strncmp(tag, "/yellow", tag_len) == 0) {
            if (color && (surface || info->obscured)) {
                SDL_color_copy(color, orig_color);
            }
        }
        /* Convenience tag to make string red. */
        else if (tag_len == 3 && strncmp(tag, "red", tag_len) == 0) {
            if (color && (surface || info->obscured) && !(flags & TEXT_NO_COLOR_CHANGE)) {
                color->r = 255;
                color->g = 0;
                color->b = 0;
            }
        }
        else if (tag_len == 4 && strncmp(tag, "/red", tag_len) == 0) {
            if (color && (surface || info->obscured)) {
                SDL_color_copy(color, orig_color);
            }
        }
        /* Convenience tag to make string blue. */
        else if (tag_len == 4 && strncmp(tag, "blue", tag_len) == 0) {
            if (color && (surface || info->obscured) && !(flags & TEXT_NO_COLOR_CHANGE)) {
                color->r = 0;
                color->g = 0;
                color->b = 255;
            }
        }
        else if (tag_len == 5 && strncmp(tag, "/blue", tag_len) == 0) {
            if (color && (surface || info->obscured)) {
                SDL_color_copy(color, orig_color);
            }
        }
        /* Bold. */
        else if (tag_len == 1 && strncmp(tag, "b", tag_len) == 0) {
            if (surface || info->obscured) {
                info->in_bold = 1;
            }
            else {
                info->calc_bold = 1;
            }
        }
        else if (tag_len == 2 && strncmp(tag, "/b", tag_len) == 0) {
            if (surface || info->obscured) {
                info->in_bold = 0;
            }
            else {
                info->calc_bold = 0;
            }
        }
        /* Italic. */
        else if (tag_len == 1 && strncmp(tag, "i", tag_len) == 0) {
            if (surface || info->obscured) {
                info->in_italic = 1;
            }
        }
        else if (tag_len == 2 && strncmp(tag, "/i", tag_len) == 0) {
            if (surface || info->obscured) {
                info->in_italic = 0;
            }
        }
        /* Underscore. */
        else if (tag_len == 1 && strncmp(tag, "u", tag_len) == 0) {
            if (surface || info->obscured) {
                info->in_underline = 1;
            }
        }
        else if (tag_len == 2 && strncmp(tag, "/u", tag_len) == 0) {
            if (surface || info->obscured) {
                info->in_underline = 0;
            }
        }
        /* Strikethrough. */
        else if (tag_len == 1 && strncmp(tag, "s", tag_len) == 0) {
            if (surface || info->obscured) {
                info->in_strikethrough = 1;
            }
        }
        else if (tag_len == 2 && strncmp(tag, "/s", tag_len) == 0) {
            if (surface || info->obscured) {
                info->in_strikethrough = 0;
            }
        }
        /* Font change. */
        else if (tag_len >= 5 && strncmp(tag, "font=", 5) == 0) {
            int font_size = 10;
            char font_name[MAX_BUF];

            if (!(flags & TEXT_NO_FONT_CHANGE) && sscanf(tag + 5, "%64[^] >] %d", font_name, &font_size) >= 1) {
                int font_id = get_font_id(font_name, font_size);

                if (font_id != -1) {
                    if (surface || info->obscured) {
                        *font = font_id;
                    }
                    else {
                        info->calc_font = font_id;
                    }
                }
            }
        }
        else if (tag_len >= 5 && !strncmp(tag, "size=", 5)) {
            int font_size;

            if (!(flags & TEXT_NO_FONT_CHANGE) && sscanf(tag + 5, "%d", &font_size) == 1) {
                const char *tmp = strrchr(fonts[*font].path, '/');
                int font_id;

                if (!tmp) {
                    tmp = fonts[*font].path;
                }
                else {
                    tmp++;
                }

                font_id = get_font_id(tmp, font_size);

                if (font_id != -1) {
                    if (surface || info->obscured) {
                        *font = font_id;
                    }
                    else {
                        info->calc_font = font_id;
                    }
                }
            }
        }
        else if (tag_len == 5 && (strncmp(tag, "/font", tag_len) == 0 || strncmp(tag, "/size", tag_len) == 0)) {
            if (surface || info->obscured) {
                *font = orig_font;
            }
            else {
                info->calc_font = -1;
            }
        }
        /* Make text centered. */
        else if (tag_len == 6 && strncmp(tag, "center", tag_len) == 0) {
            /* Find the ending tag. */
            if (c == '[') {
                tag2 = strstr(tag + tag_len, "[/center]");
            }
            else {
                tag2 = strstr(tag + tag_len, "</center>");
            }

            if (tag2 && box && box->w) {
                char *buf = malloc(tag2 - cp - 8 + 1);
                int w;

                /* Copy the string between [center] and [/center] to a
                 * temporary buffer so we can calculate its width. */
                memcpy(buf, cp + 8, tag2 - cp - 8);
                buf[tag2 - cp - 8] = '\0';
                w = dest->x + box->w / 2 - text_get_width(*font, buf, flags) / 2;
                free(buf);

                if (surface) {
                    dest->x = w;
                }
            }
        }
        else if (tag_len == 7 && strncmp(tag, "/center", tag_len) == 0) {
        }
        /* Anchor tag. */
        else if ((tag_len == 1 && strncmp(tag, "a", tag_len) == 0) || (tag_len >= 2 && strncmp(tag, "a=", 2) == 0)) {
            /* Scan for action other than the default. */
            if (sscanf(tag, "a=%1024[^]>]", info->anchor_action) != 1) {
                info->anchor_action[0] = '\0';
            }

            if (surface || info->obscured) {
                /* Change to light blue only if no custom color was
                 * specified. */
                if (color && color->r == orig_color->r && color->g == orig_color->g && color->b == orig_color->b && !(flags & TEXT_NO_COLOR_CHANGE) && !string_startswith(info->anchor_action, "#")) {
                    color->r = text_link_color.r;
                    color->g = text_link_color.g;
                    color->b = text_link_color.b;
                }

                info->anchor_tag = pos + 1;
            }
        }
        else if (tag_len == 2 && strncmp(tag, "/a", tag_len) == 0) {
            if (surface || info->obscured) {
                if (color) {
                    SDL_color_copy(color, orig_color);
                }

                info->anchor_tag = NULL;
            }
        }
        else if (tag_len >= 2 && strncmp(tag, "y=", 2) == 0) {
            if (surface) {
                int height;

                if (sscanf(tag, "y=%d", &height) == 1) {
                    dest->y += height;
                }
            }
        }
        else if (tag_len >= 2 && !strncmp(tag, "x=", 2)) {
            if (surface) {
                int w;

                if (sscanf(tag, "x=%d", &w) == 1) {
                    dest->x += w;
                }
            }
        }
        else if (tag_len >= 4 && strncmp(tag, "img=", 4) == 0) {
            if (surface) {
                char face[MAX_BUF];
                int x = 0, y = 0, alpha = 0, align = 0, zoom_x = 0, zoom_y = 0, rotate = 0, wd = 0, ht = 0, sprite_flags = 0, dark_level = 0, quick_pos = 0;
                uint32 stretch = 0;

                if (sscanf(tag + 4, "%128[^] >] %d %d %d %d %d %d %d %d %d %d %u %d %d", face, &x, &y, &align, &sprite_flags, &dark_level, &quick_pos, &alpha, &zoom_x, &zoom_y, &rotate, &stretch, &wd, &ht) >= 1) {
                    int id;

                    id = get_bmap_id(face);

                    if (id != -1 && FaceList[id].sprite) {
                        int w, h;
                        SDL_Rect srcrect;

                        w = FaceList[id].sprite->bitmap->w;
                        h = FaceList[id].sprite->bitmap->h;

                        if (rotate) {
                            rotozoomSurfaceSizeXY(w, h, rotate, zoom_x ? zoom_x / 100.0 : 1.0, zoom_y ? zoom_y / 100.0 : 1.0, &w, &h);
                        }
                        else if ((zoom_x && zoom_x != 100) || (zoom_y && zoom_y != 100)) {
                            zoomSurfaceSize(w, h, zoom_x ? zoom_x / 100.0 : 1.0, zoom_y ? zoom_y / 100.0 : 1.0, &w, &h);
                        }

                        if (align & 1) {
                            x += box->w - w - dest->x;
                        }

                        if (align & 2) {
                            y += -dest->y;
                        }

                        if (align & 4) {
                            if (quick_pos) {
                                int mnr, mid;

                                mnr = quick_pos;
                                mid = mnr >> 4;
                                mnr &= 0x0f;
                                y = y - MultiArchs[mid].part[mnr].yoff + MultiArchs[mid].ylen - h;
                                x -= MultiArchs[mid].part[mnr].xoff;

                                if (w > MultiArchs[mid].xlen) {
                                    x += (MultiArchs[mid].xlen - w) >> 1;
                                }
                            }
                            else {
                                y = (y + MAP_TILE_POS_YOFF) - h;

                                if (w > MAP_TILE_POS_XOFF) {
                                    x -= (w - MAP_TILE_POS_XOFF) / 2;
                                }
                            }
                        }

                        if (wd || ht) {
                            srcrect.x = 0;
                            srcrect.y = 0;
                            srcrect.w = wd;
                            srcrect.h = ht;
                        }

                        map_sprite_show(surface, dest->x + x, dest->y + y, wd || ht ? &srcrect : NULL, FaceList[id].sprite, sprite_flags, dark_level, alpha, stretch, zoom_x, zoom_y, rotate);
                    }
                }
            }
        }
        else if (tag_len >= 3 && strncmp(tag, "o=#", 3) == 0) {
            if (surface || info->obscured) {
                uint32 r, g, b;

                /* Parse the r,g,b colors. */
                if (sscanf(tag + 3, "%2X%2X%2X", &r, &g, &b) == 3) {
                    info->outline_color.r = r;
                    info->outline_color.g = g;
                    info->outline_color.b = b;
                    info->outline_show = 1;
                }
            }
        }
        else if (tag_len == 2 && strncmp(tag, "/o", tag_len) == 0) {
            if (surface || info->obscured) {
                info->outline_color.r = info->outline_color.g = info->outline_color.b = 0;
                info->outline_show = 0;
            }
        }
        else if (tag_len >= 6 && strncmp(tag, "alpha=", 6) == 0) {
            if (surface || info->obscured) {
                int alpha;

                if (sscanf(tag, "alpha=%d", &alpha) == 1) {
                    info->used_alpha = alpha;
                }
            }
        }
        else if (tag_len == 6 && strncmp(tag, "/alpha", tag_len) == 0) {
            info->used_alpha = 255;
        }
        else if (tag_len >= 5 && strncmp(tag, "book=", 5) == 0) {
            if (flags & TEXT_LINES_CALC) {
                book_name_change(tag + 5, pos - cp - 6);
            }
        }
        else if (tag_len == 4 && strncmp(tag, "book", tag_len) == 0) {
            if (c == '[') {
                tag2 = strstr(tag + tag_len, "[/book]");
            }
            else {
                tag2 = strstr(tag + tag_len, "</book>");
            }

            if (tag2 && flags & TEXT_LINES_CALC) {
                book_name_change(cp + tag_len, tag2 - cp - 7);
            }
        }
        else if (tag_len == 1 && strncmp(tag, "p", tag_len) == 0) {
            if (surface && box && box->w) {
                SDL_Rect rect;

                rect.y = dest->y + FONT_HEIGHT(*font) / 2;
                rect.w = 1;
                rect.h = 3;
                rect.x = dest->x;
                SDL_FillRect(surface, &rect, SDL_MapRGB(surface->format, 96, 96, 96));
                rect.x += box->w;
                SDL_FillRect(surface, &rect, SDL_MapRGB(surface->format, 96, 96, 96));

                rect.x = dest->x + 1;
                rect.w = box->w - 1;
                rect.h = 1;
                SDL_FillRect(surface, &rect, SDL_MapRGB(surface->format, 96, 96, 96));
                rect.y++;
                SDL_FillRect(surface, &rect, SDL_MapRGB(surface->format, 110, 110, 110));
                rect.y++;
                SDL_FillRect(surface, &rect, SDL_MapRGB(surface->format, 96, 96, 96));
            }
        }
        else if (tag_len == 1 && strncmp(tag, "title", tag_len) == 0) {
            if (!(flags & TEXT_NO_FONT_CHANGE)) {
                if (surface || info->obscured) {
                    *font = FONT_SERIF14;
                }
                else {
                    info->calc_font = FONT_SERIF14;
                }
            }

            if (surface || info->obscured) {
                info->in_underline = 1;
            }
        }
        else if (tag_len == 6 && strncmp(tag, "/title", tag_len) == 0) {
            if (surface || info->obscured) {
                info->in_underline = 0;
                *font = orig_font;
            }
            else {
                info->calc_font = -1;
            }
        }
        else if (tag_len >= 8 && strncmp(tag, "padding=", 8) == 0) {
            int val;

            if (x_adjust && sscanf(tag + 8, "%d", &val) == 1) {
                if (surface) {
                    dest->x += val;
                }

                dest->w += val;
                *x_adjust = val;
            }
        }
        else if (tag_len == 8 && strncmp(tag, "/padding", tag_len) == 0) {
            if (x_adjust) {
                *x_adjust = 0;
            }
        }
        else if (tag_len >= 4 && strncmp(tag, "bar=", 4) == 0) {
            if (surface && !(flags & TEXT_NO_COLOR_CHANGE)) {
                char texture[MAX_BUF];
                int bar_w, bar_h;

                bar_w = -1;
                bar_h = -1;

                if (sscanf(tag + 4, "%128[^] >] %d %d", texture, &bar_w, &bar_h) >= 1) {
                    SDL_Rect bar_dst;
                    SDL_Color bar_color;

                    bar_dst.x = dest->x;
                    bar_dst.y = dest->y;
                    bar_dst.w = box && bar_w == -1 ? box->w : bar_w;
                    bar_dst.h = box && bar_h == -1 ? box->h : bar_h;

                    if (*texture == '#' && text_color_parse(texture, &bar_color)) {
                        SDL_FillRect(surface, &bar_dst, SDL_MapRGB(surface->format, bar_color.r, bar_color.g, bar_color.b));
                    }
                    else {
                        surface_show_fill(surface, bar_dst.x, bar_dst.y, NULL, TEXTURE_CLIENT(texture), &bar_dst);
                    }
                }
            }
        }
        else if (tag_len >= 7 && strncmp(tag, "border=", 7) == 0) {
            if (surface && !(flags & TEXT_NO_COLOR_CHANGE)) {
                char texture[MAX_BUF];
                int wd, ht, thickness = 1;

                wd = -1;
                ht = -1;

                if (sscanf(tag + 7, "%128[^] >] %d %d %d", texture, &wd, &ht, &thickness) >= 1) {
                    SDL_Rect border_dst;
                    SDL_Color border_color;

                    border_dst.x = dest->x;
                    border_dst.y = dest->y;
                    border_dst.w = box && wd == -1 ? box->w : wd;
                    border_dst.h = box && ht == -1 ? box->h : ht;

                    if (*texture == '#' && text_color_parse(texture, &border_color)) {
                        border_create(surface, border_dst.x, border_dst.y, border_dst.w, border_dst.h, SDL_MapRGB(surface->format, border_color.r, border_color.g, border_color.b), thickness);
                    }
                    else {
                        border_create_texture(surface, &border_dst, thickness, TEXTURE_CLIENT(texture));
                    }
                }
            }
        }
        else if (tag_len >= 8 && strncmp(tag, "hcenter=", 8) == 0) {
            if (surface) {
                int ht;

                if (c == '[') {
                    tag2 = strstr(tag + tag_len, "[/hcenter]");
                }
                else {
                    tag2 = strstr(tag + tag_len, "</hcenter>");
                }

                if (tag2 && sscanf(tag + 8, "%d", &ht) == 1) {
                    size_t len;
                    char *tmpbuf;
                    SDL_Rect hcenter_box;

                    len = tag2 - (tag + tag_len);
                    tmpbuf = malloc(len + 1);
                    memcpy(tmpbuf, tag + tag_len, len);
                    tmpbuf[len] = '\0';

                    hcenter_box.w = box ? (box->w - (dest->w - box->w)) : 0;
                    hcenter_box.h = 0;
                    hcenter_box.y = 0;
                    text_show(NULL, *font, tmpbuf, 0, 0, "000000", flags | TEXT_HEIGHT, &hcenter_box);
                    dest->y += ht / 2 - hcenter_box.h / 2;
                    info->hcenter_y = MAX(0, ht / 2 - hcenter_box.h / 2);
                    free(tmpbuf);
                }
            }
        }
        else if (tag_len == 8 && strncmp(tag, "/hcenter", tag_len) == 0) {
            if (surface) {
                dest->y += info->hcenter_y;
            }
        }
        else if (tag_len >= 5 && strncmp(tag, "icon=", 5) == 0) {
            if (surface) {
                char face[MAX_BUF];
                int wd, ht, fit_to_size, flip;
                float show_percentage;

                wd = ht = -1;
                fit_to_size = flip = 0;
                show_percentage = 1.0;

                if (sscanf(tag + 5, "%255[^] >] %d %d %d %d %f", face, &wd, &ht, &fit_to_size, &flip, &show_percentage) >= 1) {
                    int id;
                    sprite_struct *icon_sprite;
                    SDL_Surface *icon_surface;

                    id = get_bmap_id(face);
                    icon_sprite = NULL;
                    icon_surface = NULL;

                    if (id != -1 && FaceList[id].sprite) {
                        icon_sprite = FaceList[id].sprite;
                        icon_surface = icon_sprite->bitmap;
                    }

                    if (!icon_sprite) {
                        uint8 is_software;

                        is_software = strncmp(face, "soft:", 5) == 0;
                        icon_surface = texture_surface(texture_get(is_software ? TEXTURE_TYPE_SOFTWARE : TEXTURE_TYPE_CLIENT, is_software ? face + 5 : face));
                    }

                    if (icon_surface) {
                        int icon_w, icon_h, icon_orig_w, icon_orig_h;
                        int border_up, border_down, border_left, border_right;
                        SDL_Rect icon_box, icon_dst;
                        double zoom_factor;
                        double zoom_x, zoom_y;

                        if (icon_sprite) {
                            border_up = icon_sprite->border_up;
                            border_down = icon_sprite->border_down;
                            border_left = icon_sprite->border_left;
                            border_right = icon_sprite->border_right;
                        }
                        else {
                            surface_borders_get(icon_surface, &border_up, &border_down, &border_left, &border_right, icon_surface->format->colorkey);
                        }

                        icon_w = icon_orig_w = icon_surface->w - border_left - border_right;
                        icon_h = icon_orig_h = icon_surface->h - border_up - border_down;

                        if (wd == -1) {
                            wd = icon_w;
                        }

                        if (ht == -1) {
                            ht = icon_h;
                        }

                        if (fit_to_size) {
                            if (icon_w < wd) {
                                zoom_factor = (double) wd / icon_w;
                                icon_w *= zoom_factor;
                                icon_h *= zoom_factor;
                            }

                            if (icon_h < ht) {
                                zoom_factor = (double) ht / icon_h;
                                icon_w *= zoom_factor;
                                icon_h *= zoom_factor;
                            }
                        }

                        if (icon_w > wd) {
                            zoom_factor = (double) wd / icon_w;
                            icon_w *= zoom_factor;
                            icon_h *= zoom_factor;
                        }

                        if (icon_h > ht) {
                            zoom_factor = (double) ht / icon_h;
                            icon_w *= zoom_factor;
                            icon_h *= zoom_factor;
                        }

                        icon_box.x = border_left;
                        icon_box.y = border_up;
                        icon_box.w = icon_w;
                        icon_box.h = icon_h;

                        icon_dst.x = dest->x + wd / 2 - icon_w / 2;
                        icon_dst.y = dest->y + ht / 2 - icon_h / 2;

                        zoom_x = (double) icon_w / icon_orig_w;
                        zoom_y = (double) icon_h / icon_orig_h;

                        icon_box.x *= zoom_x;
                        icon_box.y *= zoom_y;

                        if (flip & TEXT_FLIP_HORIZONTAL) {
                            zoom_x *= -1;
                        }

                        if (flip & TEXT_FLIP_VERTICAL) {
                            zoom_y *= -1;
                        }

                        if (show_percentage != 1.0) {
                            // Left to right
                            if (show_percentage >= 6.0) {
                                icon_box.w *= show_percentage - 6.0;
                            }
                            // Bottom to top
                            else if (show_percentage >= 4.0) {
                                int offset;

                                offset = icon_box.h - ((float) icon_box.h * (show_percentage - 4.0));
                                icon_box.y += offset;
                                icon_dst.y += offset;
                            }
                            // Right to left
                            else if (show_percentage >= 2.0) {
                                int offset;

                                offset = icon_box.w - ((float) icon_box.w * (show_percentage - 2.0));
                                icon_box.x += offset;
                                icon_dst.x += offset;
                            }
                            // Top to bottom
                            else {
                                icon_box.h *= show_percentage;
                            }
                        }

                        if (icon_w != icon_orig_w || icon_h != icon_orig_h || flip != 0) {
                            SDL_Surface *tmp_icon;

                            tmp_icon = zoomSurface(icon_surface, zoom_x, zoom_y, icon_w != icon_orig_w || icon_h != icon_orig_h ? setting_get_int(OPT_CAT_CLIENT, OPT_ZOOM_SMOOTH) : 0);

                            SDL_BlitSurface(tmp_icon, &icon_box, surface, &icon_dst);
                            SDL_FreeSurface(tmp_icon);
                        }
                        else {
                            SDL_BlitSurface(icon_surface, &icon_box, surface, &icon_dst);
                        }
                    }
                }
            }
        }
        else if (tag_len >= 9 && strncmp(tag, "ctooltip=", 9) == 0) {
            if (surface || info->obscured) {
                int mx, my;
                char tooltip_width[MAX_BUF], tooltip_height[MAX_BUF], tooltip_text[HUGE_BUF];
                int tooltip_max_width;

                SDL_GetMouseState(&mx, &my);

                if (text_adjust_coords(surface, &mx, &my) && mx >= dest->x && my >= dest->y && sscanf(tag + 9, "%64s %64s %d %512[^]>]", tooltip_width, tooltip_height, &tooltip_max_width, tooltip_text) == 4) {
                    int wd, ht;

                    if (string_startswith(tooltip_width, "w:")) {
                        wd = text_get_width(*font, tooltip_width + 2, 0);
                    }
                    else {
                        wd = atoi(tooltip_width);
                    }

                    if (string_startswith(tooltip_height, "h:")) {
                        ht = text_get_height(*font, tooltip_height + 2, 0);
                    }
                    else {
                        ht = atoi(tooltip_height);
                    }

                    if (box) {
                        if (box->w) {
                            wd = MIN(box->w - (dest->x - info->start_x), wd);
                        }

                        if (box->h) {
                            ht = MIN(box->h - (dest->y - info->start_y), ht);
                        }
                    }

                    if (info->obscured) {
                        ht -= info->height;
                    }

                    if (mx < dest->x + wd && my < dest->y + ht) {
                        SDL_GetMouseState(&mx, &my);
                        tooltip_create(mx, my, *font, tooltip_text);

                        if (tooltip_max_width >= 0) {
                            tooltip_multiline(tooltip_max_width);
                        }
                    }
                }
            }
        }
        else if (tag_len >= 8 && strncmp(tag, "tooltip=", 8) == 0) {
            if (surface || info->obscured) {
                if (sscanf(tag + 8, "%512[^]>]", info->tooltip_text) == 1) {
                }
            }
        }
        else if (tag_len == 8 && strncmp(tag, "/tooltip", tag_len) == 0) {
            if (surface || info->obscured) {
                info->tooltip_text[0] = '\0';
            }
        }
        else if (tag_len >= 3 && strncmp(tag, "h=#", 3) == 0) {
            int r, g, b;

            if ((surface || info->obscured) && sscanf(tag + 3, "%2X%2X%2X", &r, &g, &b) == 3) {
                /* Find the ending tag. */
                if (c == '[') {
                    tag2 = strstr(tag + tag_len, "[/h]");
                }
                else {
                    tag2 = strstr(tag + tag_len, "</h>");
                }

                if (tag2) {
                    char *buf;

                    buf = malloc(tag2 - cp - 11 + 1);
                    memcpy(buf, cp + 11, tag2 - cp - 11);
                    buf[tag2 - cp - 11] = '\0';
                    info->highlight_rect.x = dest->x;
                    info->highlight_rect.y = dest->y;
                    info->highlight_rect.w = text_get_width(*font, buf, flags);
                    info->highlight_rect.h = text_get_height(*font, buf, flags);
                    free(buf);

                    info->highlight = 1;
                    info->highlight_color.r = r;
                    info->highlight_color.g = g;
                    info->highlight_color.b = b;
                }
            }
        }
        else if (tag_len == 2 && strncmp(tag, "/h", tag_len) == 0) {
            if (surface || info->obscured) {
                info->highlight = 0;
            }
        }
        else if (tag_len >= 5 && strncmp(tag, "line=", 5) == 0) {
            int x, y, x2, y2;

            if ((surface || info->obscured) && sscanf(tag + 5, "%d,%d,%d,%d", &x, &y, &x2, &y2) == 4) {
                if (info->obscured) {
                    y = MAX(0, y - info->height);
                    y2 = MAX(1, y2 - info->height);
                }

                lineRGBA(surface, dest->x + x, dest->y + y, dest->x + x2, dest->y + y2, color->r, color->g, color->b, info->used_alpha);
            }
        }
        else if (tag_len >= 5 && strncmp(tag, "flip=", 5) == 0) {
            if (surface || info->obscured) {
                char flip_type[MAX_BUF];

                if (sscanf(tag + 5, "%64[^]>]", flip_type) == 1) {
                    if (strcmp(flip_type, "horizontal") == 0) {
                        info->flip = TEXT_FLIP_HORIZONTAL;
                    }
                    else if (strcmp(flip_type, "vertical") == 0) {
                        info->flip = TEXT_FLIP_VERTICAL;
                    }
                    else if (strcmp(flip_type, "both") == 0) {
                        info->flip = TEXT_FLIP_BOTH;
                    }
                }
            }
        }
        else if (tag_len == 4 && strncmp(tag, "/flip", tag_len) == 0) {
            if (surface || info->obscured) {
                info->flip = 0;
            }
        }

        return ret;
    }

    /* Parse entities. */
    if (flags & TEXT_MARKUP && c == '&') {
        if (!strncmp(cp, "&lt;", 4)) {
            c = '<';
            ret = 4;
        }
        else if (!strncmp(cp, "&gt;", 4)) {
            c = '>';
            ret = 4;
        }
        else if (!strncmp(cp, "&lsqb;", 6)) {
            c = '[';
            ret = 6;
        }
        else if (!strncmp(cp, "&rsqb;", 6)) {
            c = ']';
            ret = 6;
        }
        else if (!strncmp(cp, "&lbrack;", 7)) {
            c = '[';
            ret = 7;
        }
        else if (!strncmp(cp, "&rbrack;", 7)) {
            c = ']';
            ret = 7;
        }
    }

    new_style = 0;

    /* Try to set applicable font style. */
    if (surface || info->obscured) {
        if (info->in_bold) {
            new_style |= TTF_STYLE_BOLD;
        }

        if (info->in_italic) {
            new_style |= TTF_STYLE_ITALIC;
        }

        if (info->in_underline) {
            new_style |= TTF_STYLE_UNDERLINE;
        }

        if (TTF_GetFontStyle(fonts[*font].font) != new_style) {
            TTF_SetFontStyle(fonts[*font].font, new_style);
        }
    }
    /* Deals with the case when calculating width. */
    else {
        uint8 is_bold;

        /* Different font sizes affect the width, so we need to
         * temporarily change the current font. */
        if (info->calc_font != -1) {
            restore_font = *font;
            *font = info->calc_font;
        }

        is_bold = TTF_GetFontStyle(fonts[*font].font) & TTF_STYLE_BOLD;

        /* Bold style also slightly affects the width. */
        if (info->calc_bold && !is_bold) {
            TTF_SetFontStyle(fonts[*font].font, TTF_GetFontStyle(fonts[*font].font) | TTF_STYLE_BOLD);
            remove_bold = 1;
        }
        else if (!info->calc_bold && is_bold) {
            TTF_SetFontStyle(fonts[*font].font, TTF_GetFontStyle(fonts[*font].font) & ~TTF_STYLE_BOLD);
        }
    }

    /* Get the glyph's metrics. */
    if (TTF_GlyphMetrics(fonts[*font].font, c == '\t' ? ' ' : c, &minx, NULL, NULL, NULL, &width) == -1) {
        return ret;
    }

    /* Remove temporary bold style. */
    if (remove_bold) {
        TTF_SetFontStyle(fonts[*font].font, TTF_GetFontStyle(fonts[*font].font) & ~TTF_STYLE_BOLD);
    }

    /* Restore font. */
    if (restore_font != -1) {
        *font = restore_font;
        restore_font = -1;
    }

    if (minx < 0) {
        width -= minx;
    }

    /* Outline, add a little bit to the width. */
    if (info->outline_show) {
        width += 2;
    }

    if (c == '\t') {
        width *= 4;
    }

    /* Draw the character (unless it's a space, since there's no point in
     * drawing whitespace [but only if underline style is not active,
     * since we do want the underline below the space]). */
    if (surface && ((c != ' ' && c != '\t') || info->in_underline || info->anchor_tag || info->highlight || *info->tooltip_text != '\0')) {
        SDL_Surface *ttf_surface;
        char buf[2];
        SDL_Color *use_color;
        SDL_Rect dstrect, srcrect;

        buf[0] = c;
        buf[1] = '\0';
        use_color = color;

        if (info->anchor_tag || info->highlight || *info->tooltip_text != '\0') {
            int state, mx, my, orig_mx, orig_my;

            if (info->anchor_tag && cp == info->anchor_tag && text_anchor_info_ptr) {
                text_anchor_execute(info, text_anchor_info_ptr);
            }

            state = SDL_GetMouseState(&mx, &my);
            orig_mx = mx;
            orig_my = my;

            if (text_adjust_coords(surface, &mx, &my)) {
                if (mx >= dest->x && mx < dest->x + width && my >= dest->y && my < dest->y + FONT_HEIGHT(*font)) {
                    static uint32 ticks = 0;

                    if (info->anchor_tag) {
                        if (state == SDL_BUTTON_LEFT && (!selection_start || !selection_end || *selection_start == -1 || *selection_end == -1) && (!ticks || SDL_GetTicks() - ticks > 125)) {
                            ticks = SDL_GetTicks();
                            text_anchor_execute(info, NULL);
                        }
                        else {
                            cursor_texture = texture_get(TEXTURE_TYPE_CLIENT, "cursor_pointer");
                        }
                    }

                    if (*info->tooltip_text != '\0') {
                        tooltip_create(orig_mx, orig_my, *font, info->tooltip_text);
                        tooltip_multiline(0);
                    }
                }

                if (info->highlight && mx >= info->highlight_rect.x && mx < info->highlight_rect.x + info->highlight_rect.w && my >= info->highlight_rect.y && my < info->highlight_rect.y + info->highlight_rect.h) {
                    use_color = &info->highlight_color;
                }
            }
        }

        if (info->outline_show || flags & TEXT_OUTLINE) {
            int outline_x, outline_y;
            SDL_Rect outline_box;

            for (outline_x = -1; outline_x < 2; outline_x++) {
                for (outline_y = -1; outline_y < 2; outline_y++) {
                    outline_box.x = dest->x + outline_x;
                    outline_box.y = dest->y + outline_y + MAX(info->start_y - dest->y + outline_y, 0);

                    if (flags & TEXT_SOLID) {
                        ttf_surface = TTF_RenderText_Solid(fonts[*font].font, buf, info->outline_color);
                    }
                    else {
                        ttf_surface = TTF_RenderText_Blended(fonts[*font].font, buf, info->outline_color);
                    }

                    srcrect.x = 0;
                    srcrect.y = MAX(info->start_y - dest->y + outline_y, 0);
                    srcrect.w = ttf_surface->w;
                    srcrect.h = box && box->h ? MAX(MIN(box->h - (outline_box.y - info->start_y), ttf_surface->h), 0) : ttf_surface->h;

                    SDL_BlitSurface(ttf_surface, &srcrect, surface, &outline_box);
                    SDL_FreeSurface(ttf_surface);
                }
            }
        }

        /* Render the character. */
        if (flags & TEXT_SOLID) {
            ttf_surface = TTF_RenderText_Solid(fonts[*font].font, buf, *use_color);

            /* Opacity. */
            if (info->used_alpha != 255) {
                SDL_Surface *new_ttf_surface;

                /* Remove black border. */
                SDL_SetColorKey(ttf_surface, SDL_SRCCOLORKEY | SDL_ANYFORMAT, 0);
                /* Set the opacity. */
                SDL_SetAlpha(ttf_surface, SDL_SRCALPHA | SDL_RLEACCEL, info->used_alpha);
                /* Create new surface to blit. */
                new_ttf_surface = SDL_DisplayFormatAlpha(ttf_surface);
                /* Free the old one. */
                SDL_FreeSurface(ttf_surface);
                ttf_surface = new_ttf_surface;
            }
        }
        else {
            ttf_surface = TTF_RenderText_Blended(fonts[*font].font, buf, *use_color);

            if (info->used_alpha != 255) {
                surface_set_alpha(ttf_surface, info->used_alpha);
            }
        }

        if (info->in_strikethrough) {
            int font_height;

            font_height = TTF_FontHeight(fonts[*font].font);
            lineRGBA(ttf_surface, 0, font_height / 2, ttf_surface->w - 1, font_height / 2, use_color->r, use_color->g, use_color->b, 255);
        }

        if (info->flip) {
            SDL_Surface *ttf_surface_orig;

            ttf_surface_orig = ttf_surface;
            ttf_surface = zoomSurface(ttf_surface_orig, info->flip & TEXT_FLIP_HORIZONTAL ? -1.0 : 1.0, info->flip & TEXT_FLIP_VERTICAL ? -1.0 : 1.0, 0);
            SDL_FreeSurface(ttf_surface_orig);
        }

        /* Output the rendered character to the screen and free the
         * used surface. */
        dstrect.x = dest->x;
        dstrect.y = dest->y + MAX(info->start_y - dest->y, 0);
        srcrect.x = 0;
        srcrect.y = MAX(info->start_y - dest->y, 0);
        srcrect.w = ttf_surface->w;
        srcrect.h = box && box->h ? MAX(MIN(box->h - (dstrect.y - info->start_y), ttf_surface->h), 0) : ttf_surface->h;

        SDL_BlitSurface(ttf_surface, &srcrect, surface, &dstrect);
        SDL_FreeSurface(ttf_surface);
    }

    /* Update the x/w of the destination with the character's width. */
    if (surface) {
        dest->x += width;
    }

    dest->w += width;

    return ret;
}

/**
 * Get glyph's width.
 * @param font Font of the glyph.
 * @param c The glyph.
 * @return The width. */
int glyph_get_width(int font, char c)
{
    int minx, width;

    if (TTF_GlyphMetrics(fonts[font].font, c == '\t' ? ' ' : c, &minx, NULL, NULL, NULL, &width) != -1) {
        if (minx < 0) {
            width -= minx;
        }

        if (c == '\t') {
            return width * 4;
        }

        return width;
    }

    return 0;
}

/**
 * Get glyph's height.
 * @param font Font of the glyph.
 * @param c The glyph.
 * @return The height. */
int glyph_get_height(int font, char c)
{
    int miny, maxy;

    if (TTF_GlyphMetrics(fonts[font].font, c, NULL, NULL, &miny, &maxy, NULL) != -1) {
        if (miny) {
            maxy -= miny;
        }

        return maxy;
    }

    return 0;
}

/**
 * Begin handling text mouse-based selection. */
#define TEXT_SHOW_SELECT_BEGIN() \
    { \
        if (!skip && selection_start && selection_end) \
        { \
            select_start = *selection_start; \
            select_end = *selection_end; \
\
            if (select_end < select_start) \
            { \
                select_start = *selection_end; \
                select_end = *selection_start; \
            } \
\
            if (select_start >= 0 && select_end >= 0 && cp - text >= select_start && cp - text <= select_end) \
            { \
                SDL_Rect selection_box; \
\
                selection_box.x = dest.x; \
                selection_box.y = dest.y; \
                selection_box.w = 0; \
                selection_box.h = FONT_HEIGHT(FONT_TRY_INFO(font, info, surface)); \
\
                if (text_show_character(&font, orig_font, NULL, &selection_box, cp, &color, &orig_color, flags, box, &x_adjust, &info) == 1) \
                { \
                    SDL_FillRect(surface, &selection_box, -1); \
\
                    select_color_orig = color; \
                    color.r = color.g = color.b = 0; \
                    select_color_changed = 1; \
                } \
            } \
        } \
    }

/**
 * End handling text mouse-based selection. */
#define TEXT_SHOW_SELECT_END() \
    { \
        if (select_color_changed) \
        { \
            color = select_color_orig; \
            select_color_changed = 0; \
        } \
        if (selection_start && selection_end && mstate == SDL_BUTTON_LEFT) \
        { \
            if (my >= dest.y && my <= dest.y + FONT_HEIGHT(FONT_TRY_INFO(font, info, surface)) && mx >= old_x && mx <= old_x + glyph_get_width(FONT_TRY_INFO(font, info, surface), *cp)) \
            { \
                if (*selection_started) \
                { \
                    *selection_end = cp - text; \
                } \
                else \
                { \
                    *selection_start = cp - text; \
                } \
            } \
        } \
    }

/**
 * Draw a string on the specified surface.
 * @param surface Surface to draw on.
 * @param font Font to use. One of @ref FONT_xxx.
 * @param text The string to draw.
 * @param x X position.
 * @param y Y position.
 * @param color_notation Color to use.
 * @param flags One or a combination of @ref TEXT_xxx.
 * @param box If word wrap was enabled by passing @ref TEXT_WORD_WRAP as
 * one of the 'flags', this is used to get the max width from. Also even
 * if word wrap is disabled, this is used to get the max height from, if
 * set (both box->w and box->h can be 0 to indicate unlimited). */
void text_show(SDL_Surface *surface, int font, const char *text, int x, int y, const char *color_notation, uint64 flags, SDL_Rect *box)
{
    const char *cp = text;
    SDL_Rect dest;
    int pos = 0, last_space = 0, is_lf, ret, skip, max_height, max_width, height = 0;
    SDL_Color color, orig_color, select_color_orig;
    int orig_font = font, lines = 1, width = 0;
    uint16 *heights = NULL;
    size_t num_heights = 0;
    int x_adjust = 0;
    int mx, my, mstate = 0, old_x;
    sint64 select_start = 0, select_end = 0;
    uint8 select_color_changed = 0;
    text_info_struct info;

    if (text_color_parse(color_notation, &color)) {
        orig_color = color;
    }
    else {
        logger_print(LOG(BUG), "Invalid color: %s, text: %s", color_notation, text);
        return;
    }

    text_show_character_init(&info);
    info.start_x = x;
    info.start_y = y;

    if (text_debug && box && surface) {
        draw_frame(surface, x, y, box->w, box->h);
    }

    /* Align to the center. */
    if (box && flags & (TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER)) {
        int w, h;

        text_get_width_height(font, text, flags & ~(TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER), box, flags & TEXT_ALIGN_CENTER ? &w : NULL, flags & TEXT_VALIGN_CENTER ? &h : NULL);

        if (flags & TEXT_ALIGN_CENTER) {
            x += box->w / 2 - w / 2;
        }

        if (flags & TEXT_VALIGN_CENTER) {
            y += box->h / 2 - h / 2;
        }
    }

    if (selection_start && selection_end) {
        mstate = SDL_GetMouseState(&mx, &my);

        if (!text_adjust_coords(surface, &mx, &my)) {
            mstate = 0;
        }

        if (box && mx >= x && mx <= x + box->w && my >= y && my <= y + box->h) {
            cursor_texture = texture_get(TEXTURE_TYPE_CLIENT, "cursor_text");
        }
    }

    /* Store the x/y. */
    dest.x = x;
    dest.y = y;
    dest.w = 0;
    height = 0;
    max_height = 0;
    max_width = 0;

    if (flags & TEXT_HEIGHT) {
        dest.y -= box->y;
    }

    while (cp[pos] != '\0') {
        /* Have we gone over the height limit yet? */
        if (box && box->h && dest.y + (flags & TEXT_LINES_CALC ? FONT_HEIGHT(FONT_TRY_INFO(font, info, surface)) : 0) - y > box->h) {
            /* We are calculating height/lines, keep going on but without
             * any more drawing. */
            if ((flags & TEXT_LINES_CALC) || (flags & TEXT_HEIGHT && box->y == 0)) {
                surface = NULL;
            }
            else {
                return;
            }
        }

        is_lf = cp[pos] == '\n';

        /* Is this a newline, or word wrap was set and we are over
         * maximum width? */
        if (is_lf || (flags & TEXT_WORD_WRAP && box && box->w && dest.w + (flags & TEXT_MARKUP && cp[pos] == '[' ? 0 : glyph_get_width(FONT_TRY_INFO(font, info, surface), cp[pos])) > box->w)) {
            /* Store the last space. */
            if (is_lf || last_space == 0) {
                last_space = pos;

                if (last_space == 0 && !is_lf) {
                    last_space = 1;
                }
            }

            skip = 0;
            max_height = 0;

            /* See if we should skip drawing. */
            if (flags & TEXT_MAX_WIDTH) {
                skip = 1;
            }
            else if (flags & TEXT_LINES_SKIP) {
                skip = box->y && lines - 1 < box->y;
            }

            info.height = height;

            /* Draw characters until we have reached the cut point (last_space).
             * */
            while (*cp != '\0' && last_space > 0) {
                old_x = dest.x;

                TEXT_SHOW_SELECT_BEGIN();

                info.obscured = skip;
                ret = text_show_character(&font, orig_font, skip ? NULL : surface, &dest, cp, &color, &orig_color, flags, box, &x_adjust, &info);
                info.obscured = 0;

                TEXT_SHOW_SELECT_END();

                cp += ret;
                last_space -= ret;

                if (FONT_HEIGHT(FONT_TRY_INFO(font, info, surface)) > max_height) {
                    max_height = FONT_HEIGHT(FONT_TRY_INFO(font, info, surface));
                }
            }

            if (selection_start && selection_end && mstate == SDL_BUTTON_LEFT && box && my >= dest.y && my <= dest.y + FONT_HEIGHT(FONT_TRY_INFO(font, info, surface)) && mx >= dest.x && mx <= dest.x + (box->w - (dest.x - info.start_x))) {
                if (*selection_started) {
                    *selection_end = cp - text;
                }
                else {
                    *selection_start = cp - text;
                }
            }

            if (!max_height) {
                max_height = FONT_HEIGHT(FONT_TRY_INFO(font, info, surface));
            }

            /* Update the Y position. */
            if (!skip) {
                dest.y += max_height;
            }

            height += max_height;
            lines++;

            /* Calculating lines, store the height of this line. */
            if (flags & TEXT_LINES_CALC) {
                heights = realloc(heights, sizeof(*heights) * (num_heights + 1));
                heights[num_heights] = max_height;
                num_heights++;
            }

            if (flags & TEXT_MAX_WIDTH && dest.w / 2 > max_width) {
                max_width = dest.w / 2;
            }

            /* Jump over the newline, if any. */
            if (is_lf) {
                cp++;
            }
            else {
                /* Strip leading spaces. */
                while (*cp != '\0' && *cp == ' ') {
                    cp++;
                }
            }

            /* Update the coordinates. */
            last_space = pos = 0;
            dest.w = x_adjust;
            dest.x = x + x_adjust;
            max_height = 0;
        }
        else {
            /* Store last space position. */
            if (cp[pos] == ' ') {
                last_space = pos;
            }

            /* Do not do any drawing, just calculate how many characters
             * to jump and the width. */
            pos += text_show_character(&font, orig_font, NULL, &dest, cp + pos, &color, &orig_color, flags, box, &x_adjust, &info);
        }
    }

    max_height = 0;

    /* Draw leftover characters. */
    while (*cp != '\0') {
        if (flags & TEXT_WIDTH && box) {
            int w = glyph_get_width(font, *cp);

            if (box->w && width + w > box->w) {
                break;
            }

            width += w;
        }

        old_x = dest.x;
        skip = 0;
        TEXT_SHOW_SELECT_BEGIN();

        cp += text_show_character(&font, orig_font, surface, &dest, cp, &color, &orig_color, flags, box, &x_adjust, &info);

        TEXT_SHOW_SELECT_END();

        if (FONT_HEIGHT(FONT_TRY_INFO(font, info, surface)) > max_height) {
            max_height = FONT_HEIGHT(FONT_TRY_INFO(font, info, surface));
        }
    }

    if (selection_start && selection_end && mstate == SDL_BUTTON_LEFT && box && my >= dest.y + max_height && my <= dest.y + max_height + (box->h - ((dest.y + max_height) - info.start_y))) {
        if (*selection_started) {
            *selection_end = cp - text;
        }
        else {
            *selection_start = cp - text;
        }
    }

    if (!max_height) {
        max_height = FONT_HEIGHT(FONT_TRY_INFO(font, info, surface));
    }

    if (flags & TEXT_MAX_WIDTH && dest.w / 2 > max_width) {
        max_width = dest.w / 2;
    }

    if (box && flags & TEXT_MAX_WIDTH) {
        box->w = max_width;
    }

    /* Give caller access to the calculated height. */
    if (box && flags & TEXT_HEIGHT) {
        box->h = height + max_height;
    }

    /* Calculating lines? */
    if (box && flags & TEXT_LINES_CALC) {
        int total_height = 0, i, last_lines = 0;

        heights = realloc(heights, sizeof(*heights) * (num_heights + 1));
        heights[num_heights] = max_height;
        num_heights++;

        /* Go backwards to figure out the maximum number of lines shown
         * at the end of the string. */
        for (i = num_heights - 1; i >= 0; i--) {
            if (total_height + heights[i] > box->h) {
                break;
            }

            total_height += heights[i];
            last_lines++;
        }

        free(heights);
        box->y = last_lines;
        box->h = lines;
    }

    if (text_anchor_help_clicked) {
        text_anchor_help_clicked = 0;
        help_show(text_anchor_help);
    }

    text_link_color = text_link_color_default;
}

/**
 * Draw a string with a shadow.
 * @param surface Surface to draw on.
 * @param font Font to use. One of @ref FONT_xxx.
 * @param text The string to draw.
 * @param x X position.
 * @param y Y position.
 * @param color_notation Color to use.
 * @param color_shadow_notation Color to use for the shadow.
 * @param flags One or a combination of @ref TEXT_xxx.
 * @param box If word wrap was enabled by passing @ref TEXT_WORD_WRAP as
 * one of the 'flags', this is used to get the max width from. Also even
 * if word wrap is disabled, this is used to get the max height from, if
 * set (both box->w and box->h can be 0 to indicate unlimited). */
void text_show_shadow(SDL_Surface *surface, int font, const char *text, int x, int y, const char *color_notation, const char *color_shadow_notation, uint64 flags, SDL_Rect *box)
{
    text_show(surface, font, text, x + 1, y + 1, color_shadow_notation, flags | TEXT_NO_COLOR_CHANGE, box);
    text_show(surface, font, text, x, y, color_notation, flags, box);
}

/**
 * Like text_show(), but allows using printf-like format specifiers.
 *
 * @copydoc text_show() */
void text_show_format(SDL_Surface *surface, int font, int x, int y, const char *color_notation, uint64 flags, SDL_Rect *box, const char *format, ...)
{
    char buf[HUGE_BUF * 4];
    va_list ap;

    va_start(ap, format);
    vsnprintf(buf, sizeof(buf), format, ap);
    text_show(surface, font, buf, x, y, color_notation, flags, box);
    va_end(ap);
}

/**
 * Like text_show_shadow(), but allows using printf-like format specifiers.
 *
 * @copydoc text_show_shadow() */
void text_show_shadow_format(SDL_Surface *surface, int font, int x, int y, const char *color_notation, const char *color_shadow_notation, uint64 flags, SDL_Rect *box, const char *format, ...)
{
    char buf[HUGE_BUF * 4];
    va_list ap;

    va_start(ap, format);
    vsnprintf(buf, sizeof(buf), format, ap);
    text_show_shadow(surface, font, buf, x, y, color_notation, color_shadow_notation, flags, box);
    va_end(ap);
}

/**
 * Calculate string's pixel width, taking into account markup, if
 * applicable.
 * @param font Font. One of @ref FONT_xxx.
 * @param text String to get width of.
 * @param flags One or a combination of @ref TEXT_xxx.
 * @return The string's width. */
int text_get_width(int font, const char *text, uint64 flags)
{
    SDL_Rect dest;
    const char *cp = text;
    text_info_struct info;

    text_show_character_init(&info);
    TTF_SetFontStyle(fonts[font].font, TTF_STYLE_NORMAL);

    dest.w = 0;

    while (*cp != '\0') {
        cp += text_show_character(&font, font, NULL, &dest, cp, NULL, NULL, flags, NULL, NULL, &info);
    }

    return dest.w;
}

/**
 * Calculate string's pixel height, taking into account markup, if
 * applicable.
 *
 * It is usually enough to use FONT_HEIGHT() to get the string's
 * font height, unless markup is allowed, in which case the maximum used
 * height might be different.
 * @param font Font. One of @ref FONT_xxx.
 * @param text String to get height of.
 * @param flags One or a combination of @ref TEXT_xxx.
 * @return The string's height. */
int text_get_height(int font, const char *text, uint64 flags)
{
    SDL_Rect dest;
    const char *cp;
    int max_height;
    text_info_struct info;

    max_height = FONT_HEIGHT(font);

    /* No markup, the text cannot become different size. */
    if (!(flags & TEXT_MARKUP)) {
        return max_height;
    }

    text_show_character_init(&info);

    cp = text;
    dest.w = 0;

    while (*cp != '\0') {
        cp += text_show_character(&font, font, NULL, &dest, cp, NULL, NULL, flags, NULL, NULL, &info);

        if (FONT_HEIGHT(font) > max_height) {
            max_height = FONT_HEIGHT(font);
        }
    }

    return max_height;
}

/**
 * Calculate text's pixel height and width.
 * @param font Font. One of @ref FONT_xxx.
 * @param text Text to get width and height of.
 * @param flags Combination of @ref TEXT_xxx flags.
 * @param box Optional, may contain maximum width/height (for word wrap,
 * for example).
 * @param[out] w Will contain the calculated width.
 * @param[out] h Will contain the calculated height. */
void text_get_width_height(int font, const char *text, uint64 flags, SDL_Rect *box, int *w, int *h)
{
    SDL_Rect box2;

    box2.w = box ? box->w : 0;
    box2.h = box ? box->h : 0;
    box2.y = 0;

    if (w) {
        flags |= TEXT_MAX_WIDTH;
    }

    if (h) {
        flags |= TEXT_HEIGHT;
    }

    text_show(NULL, font, text, 0, 0, COLOR_WHITE, flags, &box2);

    if (w) {
        *w = box2.w;
    }

    if (h) {
        *h = box2.h;
    }
}

/**
 * Truncate a text string if overflow would occur when rendering it.
 * @param font Font used for the text.
 * @param text The text.
 * @param max_width Maximum possible width. */
void text_truncate_overflow(int font, char *text, int max_width)
{
    size_t pos = 0;
    int width = 0;

    while (text[pos] != '\0') {
        width += glyph_get_width(font, text[pos]);

        if (width > max_width) {
            text[pos] = '\0';
            break;
        }

        pos++;
    }
}

/**
 * Utility function to parse text and store information about anchor tag,
 * if any.
 * @param info Where to store the information.
 * @param text The text to parse. */
void text_anchor_parse(text_info_struct *info, const char *text)
{
    const char *cp = text;
    SDL_Rect dest;
    int font = FONT_ARIAL10;

    text_show_character_init(info);
    info->obscured = 1;

    dest.w = 0;

    while (*cp != '\0') {
        cp += text_show_character(&font, font, NULL, &dest, cp, NULL, NULL, TEXT_MARKUP, NULL, NULL, info);

        if (info->anchor_tag) {
            return;
        }
    }
}

/**
 * Enable text debugging. */
void text_enable_debug(void)
{
    text_debug = 1;
}
