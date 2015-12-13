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
 * Implements spells type widgets.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * The spell list. This is a multi-dimensional array, containing
 * dynamically resized spell path arrays, which actually hold the spells
 * for each spell path. For example (pseudo array structure):
 *
 * - spell_list:
 *  - fire
 *   - firestorm, firebolt
 */
static spell_entry_struct **spell_list[SPELL_PATH_NUM - 1];
/**
 * Number of spells contained in each spell path array in ::spell_list
 */
static size_t spell_list_num[SPELL_PATH_NUM];
/**
 * Currently selected spell path ID.
 */
static size_t spell_list_path = 0;

enum {
    BUTTON_PATH_LEFT,
    BUTTON_PATH_RIGHT,
    BUTTON_CLOSE,
    BUTTON_HELP,

    BUTTON_NUM
} ;

/**
 * Button buffer.
 */
static button_struct buttons[BUTTON_NUM];
/**
 * The spells list.
 */
static list_struct *list_spells = NULL;

/**
 * Initialize the spells system.
 */
void spells_init(void)
{
    memset(&spell_list, 0, sizeof(*spell_list) * arraysize(spell_list));
    memset(&spell_list_num, 0,
            sizeof(*spell_list_num) * arraysize(spell_list_num));
    spell_list_path = SPELL_PATH_NUM - 1;
}

/**
 * Deinitialize the spells system.
 */
void spells_deinit(void)
{
    for (size_t spell_path = 0; spell_path < SPELL_PATH_NUM - 1; spell_path++) {
        for (size_t spell_id = 0; spell_id < spell_list_num[spell_path];
                spell_id++) {
            efree(spell_list[spell_path][spell_id]);
        }

        if (spell_list[spell_path] != NULL) {
            efree(spell_list[spell_path]);
        }
    }
}

/**
 * Handle double click inside the spells list.
 * @param list The spells list.
 */
static void list_handle_enter(list_struct *list, SDL_Event *event)
{
    const char *selected;
    char buf[MAX_BUF];

    selected = list_get_selected(list, 0);

    if (!selected) {
        return;
    }

    snprintf(buf, sizeof(buf), "/ready_spell %s", selected);
    client_command_check(buf);
}

/**
 * Reload the spells list, due to a change of the spell path, filtering
 * options, etc.
 */
static void spell_list_reload(void)
{
    size_t i, j;
    uint32_t offset, rows, selected;

    if (list_spells == NULL) {
        return;
    }

    offset = list_spells->row_offset;
    selected = list_spells->row_selected;
    rows = list_spells->rows;
    list_clear(list_spells);

    if (spell_list_path == SPELL_PATH_NUM - 1) {
        i = 0;
    } else {
        i = spell_list_path;
    }

    do {
        for (j = 0; j < spell_list_num[i]; j++) {
            list_add(list_spells, list_spells->rows, 0,
                    spell_list[i][j]->spell->s_name);
        }

        i++;
    } while (i < spell_list_path);

    list_sort(list_spells, LIST_SORT_ALPHA);

    if (list_spells->rows == rows) {
        list_spells->row_offset = offset;
        list_spells->row_selected = selected;
    }

    cur_widget[SPELLS_ID]->redraw = 1;
}

/**
 * Handle button repeating (and actual pressing).
 * @param button The button.
 */
static void button_repeat_func(button_struct *button)
{
    int path = spell_list_path;

    if (button == &buttons[BUTTON_PATH_LEFT]) {
        path--;
    } else {
        path++;
    }

    if (path < 0) {
        path = SPELL_PATH_NUM - 1;
    } else if (path > SPELL_PATH_NUM - 1) {
        path = 0;
    }

    spell_list_path = path;
    spell_list_reload();
}

/**
 * Find a spell in the ::spell_list based on its name.
 *
 * Partial spell names will be matched.
 * @param name Spell name to find.
 * @param[out] spell_path Will contain the spell's path.
 * @param[out] spell_id Will contain the spell's ID.
 * @return 1 if the spell was found, 0 otherwise.
 */
int spell_find(const char *name, size_t *spell_path, size_t *spell_id)
{
    size_t name_len;

    if (name == NULL) {
        return 0;
    }

    name_len = strlen(name);

    for (*spell_path = 0; *spell_path < SPELL_PATH_NUM - 1; *spell_path += 1) {
        for (*spell_id = 0; *spell_id < spell_list_num[*spell_path];
                *spell_id += 1) {
            if (strncasecmp(spell_list[*spell_path][*spell_id]->spell->s_name,
                    name, name_len) == 0) {
                return 1;
            }
        }
    }

    return 0;
}

/**
 * Find a spell in the ::spell_list, based on a matching object.
 * @param op Object to find.
 * @param[out] spell_path Will contain the spell's path.
 * @param[out] spell_id Will contain the spell's ID.
 * @return 1 if the spell was found, 0 otherwise.
 */
int spell_find_object(object *op, size_t *spell_path, size_t *spell_id)
{
    for (*spell_path = 0; *spell_path < SPELL_PATH_NUM - 1; *spell_path += 1) {
        for (*spell_id = 0; *spell_id < spell_list_num[*spell_path];
                *spell_id += 1) {
            if (spell_list[*spell_path][*spell_id]->spell == op) {
                return 1;
            }
        }
    }

    return 0;
}

/**
 * Find a spell in the ::spell_list based on its name, but only look
 * inside the currently selected spell path list.
 *
 * Partial spell names will be matched.
 * @param name Spell name to find.
 * @return The spell if found, NULL otherwise.
 */
static spell_entry_struct *spell_find_path_selected(const char *name)
{
    size_t spell_id, name_len;

    if (name == NULL) {
        return 0;
    }

    if (spell_list_path == SPELL_PATH_NUM - 1) {
        size_t spell_path;

        if (spell_find(name, &spell_path, &spell_id)) {
            return spell_get(spell_path, spell_id);
        }

        return NULL;
    }

    name_len = strlen(name);

    for (spell_id = 0; spell_id < spell_list_num[spell_list_path];
            spell_id += 1) {
        if (strncasecmp(spell_list[spell_list_path][spell_id]->spell->s_name,
                name, name_len) == 0) {
            return spell_get(spell_list_path, spell_id);
        }
    }

    return NULL;
}

/**
 * Get spell from the ::spell_list structure.
 * @param spell_path Spell path.
 * @param spell_id Spell ID.
 * @return The spell.
 */
spell_entry_struct *spell_get(size_t spell_path, size_t spell_id)
{
    return spell_list[spell_path][spell_id];
}

void spells_update(object *op, uint16_t cost, uint32_t path, uint32_t flags,
        const char *msg)
{
    size_t spell_path, spell_id, path_real;
    spell_entry_struct *spell;

    spell = NULL;

    for (spell_path = 0; spell_path < arraysize(spell_list); spell_path++) {
        if (path & (1 << spell_path)) {
            path_real = spell_path;
            break;
        }
    }

    if (spell_path == arraysize(spell_list)) {
        LOG(BUG, "Invalid spell path for spell '%s'.",
                op->s_name);
        return;
    }

    if (spell_find_object(op, &spell_path, &spell_id)) {
        if (spell_path != path) {
            spells_remove(op);
        } else {
            spell = spell_get(spell_path, spell_id);
        }
    }

    if (spell == NULL) {
        spell = ecalloc(1, sizeof(*spell));
        spell->spell = op;

        spell_list[path_real] = erealloc(spell_list[path_real],
                sizeof(*spell_list[path_real]) *
                (spell_list_num[path_real] + 1));
        spell_list[path_real][spell_list_num[path_real]] = spell;
        spell_list_num[path_real]++;
    }

    spell->cost = cost;
    spell->path = path;
    spell->flags = flags;
    snprintf(spell->msg, sizeof(spell->msg), "%s", msg);

    spell_list_reload();
}

void spells_remove(object *op)
{
    size_t spell_path, spell_id, i;

    if (!spell_find_object(op, &spell_path, &spell_id)) {
        LOG(BUG, "Tried to remove spell '%s', but it was not in spell list.",
            op->s_name);
        return;
    }

    efree(spell_list[spell_path][spell_id]);

    for (i = spell_id + 1; i < spell_list_num[spell_path]; i++) {
        spell_list[spell_path][i - 1] = spell_list[spell_path][i];
    }

    spell_list[spell_path] = erealloc(spell_list[spell_path],
            sizeof(*spell_list[spell_path]) * (spell_list_num[spell_path] - 1));
    spell_list_num[spell_path]--;

    spell_list_reload();
}

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
    SDL_Rect box;
    size_t i;
    spell_entry_struct *spell;

    /* Create the spell list. */
    if (list_spells == NULL) {
        list_spells = list_create(12, 1, 8);
        list_spells->handle_enter_func = list_handle_enter;
        list_spells->surface = widget->surface;
        list_scrollbar_enable(list_spells);
        list_set_column(list_spells, 0, 130, 7, NULL, -1);
        list_set_font(list_spells, FONT_ARIAL10);
        spell_list_reload();

        for (i = 0; i < BUTTON_NUM; i++) {
            button_create(&buttons[i]);
            buttons[i].texture = texture_get(TEXTURE_TYPE_CLIENT,
                    "button_round");
            buttons[i].texture_pressed = texture_get(TEXTURE_TYPE_CLIENT,
                    "button_round_down");
            buttons[i].texture_over = texture_get(TEXTURE_TYPE_CLIENT,
                    "button_round_over");

            if (i == BUTTON_PATH_LEFT || i == BUTTON_PATH_RIGHT) {
                buttons[i].repeat_func = button_repeat_func;
            }
        }
    }

    if (!widget->redraw) {
        return;
    }

    box.h = 0;
    box.w = widget->w;
    text_show(widget->surface, FONT_SERIF12, "Spells", 0, 3, COLOR_HGOLD,
            TEXT_ALIGN_CENTER, &box);
    list_set_parent(list_spells, widget->x, widget->y);
    list_show(list_spells, 10, 2);

    box.w = 160;
    text_show(widget->surface, FONT_SERIF12,
            s_settings->spell_paths[spell_list_path],
            0, widget->h - FONT_HEIGHT(FONT_SERIF12) - 7,
            COLOR_HGOLD, TEXT_ALIGN_CENTER, &box);

    spell = spell_find_path_selected(list_get_selected(list_spells, 0));

    /* If there is a currently selected spell, show its description,
     * icon, spell cost, etc. */
    if (spell != NULL) {
        SDL_Surface *icon;
        const char *status;

        box.h = 30;
        box.w = 150;
        text_show(widget->surface,
                  FONT_SERIF12,
                  spell->spell->s_name,
                  160,
                  20,
                  COLOR_HGOLD,
                  TEXT_OUTLINE | TEXT_WORD_WRAP | TEXT_ALIGN_CENTER,
                  &box);

        box.h = 120;
        box.w = 150;
        text_show(widget->surface, FONT_ARIAL10, spell->msg, 160, 40,
                COLOR_WHITE, TEXT_WORD_WRAP, &box);

        icon = FaceList[spell->spell->face].sprite->bitmap;

        text_show_format(widget->surface, FONT_ARIAL10, 160, widget->h - 30,
                COLOR_WHITE, TEXT_MARKUP, NULL, "[b]Cost[/b]: %d",
                (int) ((double) spell->cost *
                (double) PATH_SP_MULT(&cpl, spell)));

        if (cpl.path_denied & spell->path) {
            status = "Denied";
        } else if (cpl.path_attuned & spell->path &&
                !(cpl.path_repelled & spell->path)) {
            status = "Attuned";
        } else if (cpl.path_repelled & spell->path &&
                !(cpl.path_attuned & spell->path)) {
            status = "Repelled";
        } else {
            status = "Normal";
        }

        text_show_format(widget->surface, FONT_ARIAL10, 160, widget->h - 18,
                COLOR_WHITE, TEXT_MARKUP, NULL, "[b]Status[/b]: %s", status);
        draw_frame(widget->surface, widget->w - 6 - icon->w,
                widget->h - 6 - icon->h, icon->w + 1, icon->h + 1);
        surface_show(widget->surface, widget->w - 5 - icon->w,
                widget->h - 5 - icon->h, NULL, icon);
    }

    for (i = 0; i < BUTTON_NUM; i++) {
        buttons[i].surface = widget->surface;
        button_set_parent(&buttons[i], widget->x, widget->y);
    }

    buttons[BUTTON_PATH_LEFT].x = 6;
    buttons[BUTTON_PATH_LEFT].y = widget->h -
            texture_surface(buttons[BUTTON_PATH_LEFT].texture)->h - 5;
    button_show(&buttons[BUTTON_PATH_LEFT], "<");

    buttons[BUTTON_PATH_RIGHT].x = 6 + 130;
    buttons[BUTTON_PATH_RIGHT].y = widget->h -
            texture_surface(buttons[BUTTON_PATH_RIGHT].texture)->h - 5;
    button_show(&buttons[BUTTON_PATH_RIGHT], ">");

    /* Show close button. */
    buttons[BUTTON_CLOSE].x = widget->w -
            texture_surface(buttons[BUTTON_CLOSE].texture)->w - 4;
    buttons[BUTTON_CLOSE].y = 4;
    button_show(&buttons[BUTTON_CLOSE], "X");

    /* Show help button. */
    buttons[BUTTON_HELP].x = widget->w -
            texture_surface(buttons[BUTTON_HELP].texture)->w * 2 - 4;
    buttons[BUTTON_HELP].y = 4;
    button_show(&buttons[BUTTON_HELP], "?");
}

/** @copydoc widgetdata::background_func */
static void widget_background(widgetdata *widget, int draw)
{
    if (!widget->redraw) {
        widget->redraw = list_need_redraw(list_spells);
    }

    if (!widget->redraw) {
        size_t i;

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
    size_t i;

    /* If the list has handled the mouse event, we need to redraw the
     * widget. */
    if (list_spells != NULL && list_handle_mouse(list_spells, event)) {
        widget->redraw = 1;
        return 1;
    }

    for (i = 0; i < BUTTON_NUM; i++) {
        if (button_event(&buttons[i], event)) {
            switch (i) {
            case BUTTON_PATH_LEFT:
            case BUTTON_PATH_RIGHT:
                button_repeat_func(&buttons[i]);
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

    if (list_spells != NULL && event->type == SDL_MOUSEBUTTONDOWN &&
            event->button.button == SDL_BUTTON_LEFT) {
        spell_entry_struct *spell;
        sprite_struct *icon;
        int xpos, ypos;

        spell = spell_find_path_selected(list_get_selected(list_spells, 0));

        if (spell == NULL) {
            return 0;
        }

        icon = FaceList[spell->spell->face].sprite;
        xpos = widget->x + widget->w - 5;
        ypos = widget->y + widget->h - 5;

        if (event->motion.x >= xpos - icon->bitmap->w &&
                event->motion.y >= ypos - icon->bitmap->h &&
                event->motion.x < xpos && event->motion.y < ypos) {
            cpl.dragging_tag = spell->spell->tag;
            return 1;
        }
    }

    return 0;
}

/** @copydoc widgetdata::deinit_func */
static void widget_deinit(widgetdata *widget)
{
    if (list_spells != NULL) {
        list_remove(list_spells);
        list_spells = NULL;
    }

    for (size_t i = 0; i < BUTTON_NUM; i++) {
        button_destroy(&buttons[i]);
    }
}

/**
 * Initialize one spells widget.
 */
void widget_spells_init(widgetdata *widget)
{
    widget->draw_func = widget_draw;
    widget->background_func = widget_background;
    widget->event_func = widget_event;
    widget->deinit_func = widget_deinit;
}
