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
 * Text input API.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <toolkit/string.h>
#include <toolkit/x11.h>

text_input_history_struct *text_input_history_create(void)
{
    text_input_history_struct *tmp;

    tmp = ecalloc(1, sizeof(*tmp));
    utarray_new(tmp->history, &ut_str_icd);

    return tmp;
}

void text_input_history_free(text_input_history_struct *history)
{
    utarray_free(history->history);
    efree(history);
}

/**
 * Add string to text input history.
 * @param history
 * The history to add to.
 * @param text
 * The text to add to the history.
 */
static void text_input_history_add(text_input_history_struct *history, const char *text)
{
    char **p;

    if (!history) {
        return;
    }

    p = (char **) utarray_back(history->history);

    if (p && !strcmp(*p, text)) {
        return;
    }

    utarray_push_back(history->history, &text);

    if (utarray_len(history->history) > (size_t) setting_get_int(OPT_CAT_GENERAL, OPT_MAX_INPUT_HISTORY_LINES)) {
        utarray_erase(history->history, 0, utarray_len(history->history) - (size_t) setting_get_int(OPT_CAT_GENERAL, OPT_MAX_INPUT_HISTORY_LINES));
    }
}

void text_input_create(text_input_struct *text_input)
{
    memset(text_input, 0, sizeof(*text_input));
    text_input->focus = 1;
    text_input->coords.w = 200;
    text_input->max = MIN(sizeof(text_input->str), 256);

    text_input_set_font(text_input, FONT_ARIAL11);
}

/**
 * Destroy data associated with the specified text input. The text input
 * structure itself is not freed.
 * @param text_input
 * Text input to destroy.
 */
void text_input_destroy(text_input_struct *text_input)
{
    if (text_input->font != NULL) {
        font_free(text_input->font);
    }
}

void text_input_set_font(text_input_struct *text_input, font_struct *font)
{
    if (text_input->font != NULL) {
        font_free(text_input->font);
    }

    FONT_INCREF(font);
    text_input->font = font;
    text_input->coords.h = FONT_HEIGHT(font) + TEXT_INPUT_PADDING * 2;
}

void text_input_reset(text_input_struct *text_input)
{
    if (text_input->history) {
        text_input->history->pos = 0;
    }

    text_input_set(text_input, NULL);
}

void text_input_set_history(text_input_struct *text_input, text_input_history_struct *history)
{
    text_input->history = history;
}

void text_input_set(text_input_struct *text_input, const char *str)
{
    strncpy(text_input->str, str ? str : "", text_input->max - 1);
    text_input->str[text_input->max - 1] = '\0';
    text_input->pos = text_input->num = strlen(text_input->str);
}

void text_input_set_parent(text_input_struct *text_input, int px, int py)
{
    text_input->px = px;
    text_input->py = py;
}

int text_input_mouse_over(text_input_struct *text_input, int mx, int my)
{
    mx -= text_input->px;
    my -= text_input->py;

    if (mx >= text_input->coords.x &&
            my >= text_input->coords.y &&
            mx < text_input->coords.x + text_input->coords.w &&
            my < text_input->coords.y + text_input->coords.h) {
        return 1;
    }

    return 0;
}

void text_input_show_edit_password(text_input_struct *text_input)
{
    string_replace_char(text_input->str, NULL, '*');
}

int text_input_number_character_check(text_input_struct *text_input, char c)
{
    return isdigit(c);
}

void text_input_show(text_input_struct *text_input, SDL_Surface *surface,
        int x, int y)
{
    text_info_struct info;
    int underscore_width;
    size_t pos;
    char *cp, *cp2;
    SDL_Rect box;
    StringBuffer *sb;

    text_input->coords.x = x;
    text_input->coords.y = y;

    rectangle_create(surface, text_input->coords.x, text_input->coords.y,
            text_input->coords.w, text_input->coords.h, "000000");
    border_create_color(surface, &text_input->coords, 1, "303030");

    cp = NULL;
    box.w = 0;

    if (text_input->show_edit_func) {
        cp = estrdup(text_input->str);
        text_input->show_edit_func(text_input);
    }

    text_show_character_init(&info);
    underscore_width = glyph_get_width(text_input->font, '_');

    /* Figure out the width by going backwards. */
    for (pos = text_input->pos; pos; pos--) {
        /* Reached the maximum yet? */
        if (box.w + glyph_get_width(text_input->font,
                *(text_input->str + pos)) +
                underscore_width > text_input->coords.w - TEXT_INPUT_PADDING * 2) {
            break;
        }

        text_show_character(&text_input->font, text_input->font, NULL, &box,
                text_input->str + pos, NULL, NULL, 0, NULL, NULL,
                &info);
    }

    sb = stringbuffer_new();
    stringbuffer_append_string_len(sb, text_input->str + pos,
            text_input->pos - pos);

    if (text_input->focus) {
        stringbuffer_append_char(sb, '_');
    }

    if ((text_input->str + pos) + (text_input->pos - pos)) {
        stringbuffer_append_string(sb,
                (text_input->str + pos) + (text_input->pos - pos));
    }

    box.w = text_input->coords.w - TEXT_INPUT_PADDING * 2;
    box.h = text_input->coords.h - TEXT_INPUT_PADDING * 2;

    cp2 = stringbuffer_finish(sb);
    text_show(surface, text_input->font, cp2,
            text_input->coords.x + TEXT_INPUT_PADDING,
            text_input->coords.y + TEXT_INPUT_PADDING,
            COLOR_WHITE, text_input->text_flags | TEXT_WIDTH, &box);
    efree(cp2);

    if (cp) {
        text_input_set(text_input, cp);
        efree(cp);
    }
}

void text_input_add_char(text_input_struct *text_input, char c)
{
    size_t i;

    if (text_input->num >= text_input->max) {
        return;
    }

    i = text_input->num;

    for (i = text_input->num; i >= text_input->pos; i--) {
        text_input->str[i + 1] = text_input->str[i];

        if (i == 0) {
            break;
        }
    }

    text_input->str[text_input->pos] = c;
    text_input->pos++;
    text_input->num++;
    text_input->str[text_input->num] = '\0';
}

int text_input_event(text_input_struct *text_input, SDL_Event *event)
{
    if (!text_input->focus) {
        return 0;
    }

    if (event->type == SDL_KEYDOWN) {
        if (keybind_command_matches_event("?PASTE", &event->key)) {
            char *clipboard_contents;

            clipboard_contents = x11_clipboard_get(SDL_display, SDL_window);

            if (clipboard_contents) {
                strncat(text_input->str, clipboard_contents, text_input->max - text_input->num - 1);
                text_input->str[text_input->max - 1] = '\0';
                text_input->pos = text_input->num = strlen(text_input->str);
                string_replace_unprintable_chars(text_input->str);

                efree(clipboard_contents);
            }

            return 1;
        } else if (IS_ENTER(event->key.keysym.sym)) {
            if (*text_input->str != '\0') {
                text_input_history_add(text_input->history, text_input->str);
            }

            return 1;
        } else if (event->key.keysym.sym == SDLK_BACKSPACE) {
            if (text_input->num && text_input->pos) {
                size_t i, j;

                i = j = text_input->pos;

                if (event->key.keysym.mod & KMOD_CTRL) {
                    string_skip_word(text_input->str, &i, -1);
                } else {
                    i--;
                }

                while (j <= text_input->num) {
                    text_input->str[i++] = text_input->str[j++];
                }

                text_input->pos -= (j - i);
                text_input->num -= (j - i);
            }

            return 1;
        } else if (event->key.keysym.sym == SDLK_DELETE) {
            if (text_input->pos != text_input->num) {
                size_t i, j;

                i = j = text_input->pos;

                if (event->key.keysym.mod & KMOD_CTRL) {
                    string_skip_word(text_input->str, &i, 1);
                } else {
                    i++;
                }

                while (i <= text_input->num) {
                    text_input->str[j++] = text_input->str[i++];
                }

                text_input->num -= (i - j);
            }

            return 1;
        } else if (event->key.keysym.sym == SDLK_LEFT) {
            if (event->key.keysym.mod & KMOD_CTRL) {
                size_t i;

                i = text_input->pos;
                string_skip_word(text_input->str, &i, -1);
                text_input->pos = i;
            } else if (text_input->pos != 0) {
                text_input->pos--;
            }

            return 1;
        } else if (event->key.keysym.sym == SDLK_RIGHT) {
            if (event->key.keysym.mod & KMOD_CTRL) {
                size_t i;

                i = text_input->pos;
                string_skip_word(text_input->str, &i, 1);
                text_input->pos = i;
            } else if (text_input->pos < text_input->num) {
                text_input->pos++;
            }

            return 1;
        } else if (event->key.keysym.sym == SDLK_UP) {
            if (text_input->history) {
                char **p;

                p = (char **) utarray_eltptr(text_input->history->history, utarray_len(text_input->history->history) - 1 - text_input->history->pos);

                if (p) {
                    if (text_input->history->pos == 0) {
                        strncpy(text_input->str_editing, text_input->str, sizeof(text_input->str_editing) - 1);
                        text_input->str_editing[sizeof(text_input->str_editing) - 1] = '\0';
                    }

                    text_input->history->pos++;
                    text_input_set(text_input, *p);
                }
            }

            return 1;
        } else if (event->key.keysym.sym == SDLK_DOWN) {
            if (text_input->history) {
                if (text_input->history->pos > 0) {
                    text_input->history->pos--;

                    if (text_input->history->pos == 0) {
                        text_input_set(text_input, text_input->str_editing);
                        text_input->str_editing[0] = '\0';
                    } else {
                        char **p;

                        p = (char **) utarray_eltptr(text_input->history->history, utarray_len(text_input->history->history) - text_input->history->pos);

                        if (p) {
                            text_input_set(text_input, *p);
                        }
                    }
                } else if (*text_input->str != '\0') {
                    text_input_history_add(text_input->history, text_input->str);
                    text_input_set(text_input, NULL);
                }
            }

            return 1;
        } else if (event->key.keysym.sym == SDLK_HOME) {
            text_input->pos = 0;
            return 1;
        } else if (event->key.keysym.sym == SDLK_END) {
            text_input->pos = text_input->num;
            return 1;
        } else if (event->key.keysym.sym == SDLK_RSHIFT || event->key.keysym.sym == SDLK_LSHIFT) {
            return 1;
        } else {
            char c;

            c = event->key.keysym.unicode & 0xff;

            if (isprint(c) && (!text_input->character_check_func || text_input->character_check_func(text_input, c))) {
                if (event->key.keysym.mod & KMOD_SHIFT) {
                    c = toupper(c);
                }

                text_input_add_char(text_input, c);
                return 1;
            }
        }
    }

    return 0;
}
