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
 * Implements skills type widgets.
 *
 * @author Alex Tokar */

#include <global.h>

enum {
    BUTTON_CLOSE,
    BUTTON_HELP,

    BUTTON_NUM
} ;

/**
 * Button buffer. */
static button_struct buttons[BUTTON_NUM];
/**
 * The skills list. */
static skill_entry_struct **skill_list;
/**
 * Number of skills contained in ::skill_list. */
static size_t skill_list_num;
/**
 * The skills list. */
static list_struct *list_skills = NULL;
/**
 * Currently selected skill in the skills list. */
static size_t selected_skill;

/**
 * Initialize skills system. */
void skills_init(void)
{
    skill_list = NULL;
    skill_list_num = 0;
    selected_skill = 0;
}

/** @copydoc list_struct::post_column_func */
static void list_post_column(list_struct *list, uint32 row, uint32 col)
{
    size_t skill_id;
    SDL_Rect box;

    skill_id = row * list->cols + col;

    if (skill_id >= skill_list_num) {
        return;
    }

    if (!FaceList[skill_list[skill_id]->skill->face].sprite) {
        return;
    }

    box.x = list->x + list->frame_offset + INVENTORY_ICON_SIZE * col;
    box.y = LIST_ROWS_START(list) + (LIST_ROW_OFFSET(row, list) * LIST_ROW_HEIGHT(list));
    box.w = INVENTORY_ICON_SIZE;
    box.h = INVENTORY_ICON_SIZE;

    surface_show(list->surface, box.x, box.y, NULL, FaceList[skill_list[skill_id]->skill->face].sprite->bitmap);

    if (selected_skill == skill_id) {
        char buf[MAX_BUF];

        border_create_color(list->surface, &box, 1, "ff0000");

        strncpy(buf, skill_list[skill_id]->skill->s_name, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        string_title(buf);

        box.w = 170;
        text_show(list->surface, FONT_SERIF12, buf, 147, 25, COLOR_HGOLD, TEXT_ALIGN_CENTER, &box);

        text_show_format(list->surface, FONT_ARIAL11, 165, 45, COLOR_WHITE, TEXT_MARKUP, NULL, "[b]Current level[/b]: %d", skill_list[skill_id]->level);
        text_show(list->surface, FONT_ARIAL11, "[b]Skill progress[/b]:", 165, 60, COLOR_WHITE, TEXT_MARKUP, NULL);

        player_draw_exp_progress(list->surface, 165, 80, skill_list[skill_id]->exp, skill_list[skill_id]->level);
    }
}

/** @copydoc list_struct::row_color_func */
static void list_row_color(list_struct *list, int row, SDL_Rect box)
{
    SDL_FillRect(list->surface, &box, SDL_MapRGB(list->surface->format, 25, 25, 25));
}

/**
 * Reload the skills list, due to a change of the skill type, for example. */
static void skill_list_reload(void)
{
    size_t i;
    uint32 offset, rows, selected;

    if (!list_skills) {
        return;
    }

    offset = list_skills->row_offset;
    selected = list_skills->row_selected;
    rows = list_skills->rows;
    list_clear(list_skills);

    for (i = 0; i < skill_list_num; i++) {
        list_add(list_skills, list_skills->rows - (i % list_skills->cols == 0 ? 0 : 1), i % list_skills->cols, NULL);
    }

    if (list_skills->rows == rows) {
        list_skills->row_offset = offset;
        list_skills->row_selected = selected;
    }

    cur_widget[SKILLS_ID]->redraw = 1;
}

/**
 * Find a skill in the ::skill_list based on its name.
 *
 * Partial skill names will be matched.
 * @param name Skill name to find.
 * @param[out] id Will contain the skill's ID.
 * @return 1 if the skill was found, 0 otherwise. */
int skill_find(const char *name, size_t *id)
{
    for (*id = 0; *id < skill_list_num; *id += 1) {
        if (!strncasecmp(skill_list[*id]->skill->s_name, name, strlen(name))) {
            return 1;
        }
    }

    return 0;
}

int skill_find_object(object *op, size_t *id)
{
    for (*id = 0; *id < skill_list_num; *id += 1) {
        if (skill_list[*id]->skill == op) {
            return 1;
        }
    }

    return 0;
}

/**
 * Get skill from the ::skill_list structure.
 * @param id Skill ID.
 * @return The skill. */
skill_entry_struct *skill_get(size_t id)
{
    return skill_list[id];
}

void skills_update(object *op, uint8 level, sint64 xp)
{
    size_t skill_id;
    skill_entry_struct *skill;

    if (skill_find_object(op, &skill_id)) {
        skill = skill_get(skill_id);
    } else {
        skill = ecalloc(1, sizeof(*skill));
        skill->skill = op;

        skill_list = realloc(skill_list, sizeof(*skill_list) * (skill_list_num + 1));
        skill_list[skill_list_num] = skill;
        skill_list_num++;
    }

    skill->level = level;
    skill->exp = xp;

    skill_list_reload();
}

void skills_remove(object *op)
{
    size_t skill_id, i;

    if (!skill_find_object(op, &skill_id)) {
        logger_print(LOG(BUG), "Tried to remove skill '%s', but it was not in skill list.", op->s_name);
        return;
    }

    efree(skill_list[skill_id]);

    for (i = skill_id + 1; i < skill_list_num; i++) {
        skill_list[i - 1] = skill_list[i];
    }

    skill_list = realloc(skill_list, sizeof(*skill_list) * (skill_list_num - 1));
    skill_list_num--;

    skill_list_reload();
}

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
    SDL_Rect box;
    size_t i;

    /* Create the skill list. */
    if (!list_skills) {
        list_skills = list_create(5, 4, 8);
        list_skills->post_column_func = list_post_column;
        list_skills->row_color_func = list_row_color;
        list_skills->row_selected_func = NULL;
        list_skills->row_highlight_func = NULL;
        list_skills->surface = widget->surface;
        list_skills->row_height_adjust = INVENTORY_ICON_SIZE;
        list_set_font(list_skills, NULL);
        list_scrollbar_enable(list_skills);
        list_set_column(list_skills, 0, INVENTORY_ICON_SIZE, 0, NULL, -1);
        list_set_column(list_skills, 1, INVENTORY_ICON_SIZE, 0, NULL, -1);
        list_set_column(list_skills, 2, INVENTORY_ICON_SIZE, 0, NULL, -1);
        list_set_column(list_skills, 3, INVENTORY_ICON_SIZE, 0, NULL, -1);
        skill_list_reload();

        for (i = 0; i < BUTTON_NUM; i++) {
            button_create(&buttons[i]);
            buttons[i].texture = texture_get(TEXTURE_TYPE_CLIENT, "button_round");
            buttons[i].texture_pressed = texture_get(TEXTURE_TYPE_CLIENT, "button_round_down");
            buttons[i].texture_over = texture_get(TEXTURE_TYPE_CLIENT, "button_round_over");
        }
    }

    if (widget->redraw) {
        box.h = 0;
        box.w = widget->w;
        text_show(widget->surface, FONT_SERIF12, "Skills", 0, 3, COLOR_HGOLD, TEXT_ALIGN_CENTER, &box);
        list_set_parent(list_skills, widget->x, widget->y);
        list_show(list_skills, 10, 2);

        for (i = 0; i < BUTTON_NUM; i++) {
            buttons[i].surface = widget->surface;
            button_set_parent(&buttons[i], widget->x, widget->y);
        }

        buttons[BUTTON_CLOSE].x = widget->w - texture_surface(buttons[BUTTON_CLOSE].texture)->w - 4;
        buttons[BUTTON_CLOSE].y = 4;
        button_show(&buttons[BUTTON_CLOSE], "X");

        buttons[BUTTON_HELP].x = widget->w - texture_surface(buttons[BUTTON_HELP].texture)->w * 2 - 4;
        buttons[BUTTON_HELP].y = 4;
        button_show(&buttons[BUTTON_HELP], "?");
    }
}

/** @copydoc widgetdata::background_func */
static void widget_background(widgetdata *widget)
{
    if (!widget->redraw) {
        widget->redraw = list_need_redraw(list_skills);
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
    uint32 row, col;
    size_t i;

    if (EVENT_IS_MOUSE(event) && event->button.button == SDL_BUTTON_LEFT && list_mouse_get_pos(list_skills, event->motion.x, event->motion.y, &row, &col)) {
        size_t skill_id;

        skill_id = row * list_skills->cols + col;

        if (skill_id < skill_list_num) {
            if (event->type == SDL_MOUSEBUTTONUP) {
                if (selected_skill != skill_id) {
                    selected_skill = skill_id;
                    widget->redraw = 1;
                    return 1;
                }
            } else if (event->type == SDL_MOUSEBUTTONDOWN) {
                event_dragging_start(skill_list[skill_id]->skill->tag, event->motion.x, event->motion.y);
                return 1;
            }
        }
    }

    /* If the list has handled the mouse event, we need to redraw the
     * widget. */
    if (list_skills && list_handle_mouse(list_skills, event)) {
        widget->redraw = 1;
        return 1;
    }


    for (i = 0; i < BUTTON_NUM; i++) {
        if (button_event(&buttons[i], event)) {
            switch (i) {
            case BUTTON_CLOSE:
                widget->show = 0;
                break;

            case BUTTON_HELP:
                help_show("skill list");
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
 * Initialize one skills widget. */
void widget_skills_init(widgetdata *widget)
{
    widget->draw_func = widget_draw;
    widget->background_func = widget_background;
    widget->event_func = widget_event;
}
