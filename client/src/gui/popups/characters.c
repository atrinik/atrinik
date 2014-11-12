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
 * Implements the characters chooser.
 *
 * @author Alex Tokar */

#include <global.h>

enum
{
    TEXT_INPUT_CHARNAME,
    TEXT_INPUT_PASSWORD,
    TEXT_INPUT_PASSWORD_NEW,
    TEXT_INPUT_PASSWORD_NEW2,

    TEXT_INPUT_NUM
};

/**
 * Progress dots buffer. */
static progress_dots progress;
/**
 * Button buffer. */
static button_struct button_tab_characters, button_tab_new, button_tab_password, button_character_male, button_character_female, button_character_left, button_character_right, button_login, button_done;
/**
 * Text input buffers. */
static text_input_struct text_inputs[TEXT_INPUT_NUM];
/**
 * Characters list. */
static list_struct *list_characters;
/**
 * Currently selected text input. */
static size_t text_input_current;
/**
 * Which character race is selected in the character creation tab. */
static size_t character_race;
/**
 * Which gender is selected in the character creation tab. */
static size_t character_gender;

/**
 * Switch to the specified tab in the characters GUI.
 * @param button The button tab to switch to. */
static void button_tab_switch(button_struct *button)
{
    if (button == &button_tab_characters) {
        button_tab_new.pressed_forced = button_tab_password.pressed_forced = 0;
        button_tab_characters.pressed_forced = 1;
    }
    else if (button == &button_tab_new) {
        button_tab_characters.pressed_forced = button_tab_password.pressed_forced = 0;
        button_tab_new.pressed_forced = 1;

        text_input_reset(&text_inputs[TEXT_INPUT_CHARNAME]);
        text_inputs[TEXT_INPUT_CHARNAME].focus = 1;

        character_race = 0;
        character_gender = GENDER_MALE;
    }
    else if (button == &button_tab_password) {
        size_t i;

        button_tab_characters.pressed_forced = button_tab_new.pressed_forced = 0;
        button_tab_password.pressed_forced = 1;

        for (i = TEXT_INPUT_PASSWORD; i < TEXT_INPUT_NUM; i++) {
            text_input_reset(&text_inputs[i]);
        }

        text_input_current = TEXT_INPUT_PASSWORD;
        text_inputs[text_input_current].focus = 1;
    }
}

/** @copydoc text_input_struct::character_check_func */
static int text_input_character_check(text_input_struct *text_input, char c)
{
    if (text_input == &text_inputs[TEXT_INPUT_CHARNAME] && !char_contains(c, s_settings->text[SERVER_TEXT_ALLOWED_CHARS_CHARNAME])) {
        return 0;
    }
    else if (!char_contains(c, s_settings->text[SERVER_TEXT_ALLOWED_CHARS_PASSWORD])) {
        return 0;
    }

    return 1;
}

/** @copydoc list_struct::text_color_hook */
static void list_text_color(list_struct *list, uint32 row, uint32 col, const char **color, const char **color_shadow)
{
    if (col == 0) {
        *color_shadow = NULL;
        *color = NULL;
    }
}

/** @copydoc list_struct::post_column_func */
static void list_post_column(list_struct *list, uint32 row, uint32 col)
{
    if (col == 0) {
        static uint32 ticks[2] = {0, 0};
        static uint8 state[2] = {0, 0};
        uint16 anim_id, face;
        size_t idx;

        anim_id = atoi(list->text[row][col]);
        check_animation_status(anim_id);
        idx = list->row_selected - 1 == row ? 1 : 0;

        if (SDL_GetTicks() - ticks[idx] > 500) {
            ticks[idx] = SDL_GetTicks();
            state[idx]++;
        }

        if (state[idx] >= (animations[anim_id].num_animations / animations[anim_id].facings)) {
            state[idx] = 0;
        }

        if (list->row_selected - 1 == row) {
            face = animations[anim_id].faces[(animations[anim_id].num_animations / animations[anim_id].facings) * (5 + 8) + state[idx]];
        }
        else {
            face = animations[anim_id].faces[(animations[anim_id].num_animations / animations[anim_id].facings) * 5 + state[idx]];
        }

        request_face(face);

        if (FaceList[face].name) {
            char *facename;

            facename = string_sub(FaceList[face].name, 0, -4);
            text_show_format(list->surface, FONT_ARIAL10, list->x, LIST_ROWS_START(list) + (LIST_ROW_OFFSET(row, list) * LIST_ROW_HEIGHT(list)), COLOR_WHITE, TEXT_MARKUP, NULL, "[img=%s 0 10 0 0 0 0 0 0 0 0 0 50 45]", facename);
            efree(facename);
        }
    }
}

/** @copydoc text_anchor_handle_func */
static int text_anchor_handle(const char *anchor_action, const char *buf, size_t len, void *custom_data)
{
    if (strcmp(anchor_action, "charname") == 0) {
        packet_struct *packet;

        packet = packet_new(SERVER_CMD_ACCOUNT, 64, 64);
        packet_append_uint8(packet, CMD_ACCOUNT_LOGIN_CHAR);
        packet_append_string_terminated(packet, buf);
        socket_send_packet(packet);

        cpl.state = ST_WAITFORPLAY;

        return 1;
    }

    return 0;
}

/** @copydoc list_struct::handle_enter_func */
static void list_handle_enter(list_struct *list, SDL_Event *event)
{
    if (list->row_selected) {
        text_info_struct info;

        text_anchor_parse(&info, list->text[list->row_selected - 1][1]);
        text_set_anchor_handle(text_anchor_handle);
        text_anchor_execute(&info, NULL);
        text_set_anchor_handle(NULL);
    }
}

/** @copydoc popup_struct::popup_draw_func */
static int popup_draw(popup_struct *popup)
{
    SDL_Rect box;
    char timebuf[MAX_BUF];
    size_t i;

    box.w = popup->surface->w;
    box.h = 38;
    text_show_shadow_format(popup->surface, FONT_SERIF14, 0, 0, COLOR_HGOLD, COLOR_BLACK, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box, "Welcome, %s", cpl.account);

    /* Waiting to log in. */
    if (cpl.state == ST_WAITFORPLAY) {
        box.w = popup->surface->w;
        box.h = popup->surface->h;
        text_show_shadow(popup->surface, FONT_SERIF12, "Logging in, please wait...", 0, 0, COLOR_HGOLD, COLOR_BLACK, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);
        return 1;
    }
    else if (cpl.state == ST_PLAY || cpl.state < ST_STARTCONNECT) {
        return 0;
    }

    textwin_show(popup->surface, 265, 45, 220, 132);

    box.w = 220;
    box.h = 80;
    strftime(timebuf, sizeof(timebuf), "%a %b %d %H:%M:%S %Y", localtime(&cpl.last_time));
    text_show_shadow_format(popup->surface, FONT_ARIAL11, 265, 190, COLOR_WHITE, COLOR_BLACK, TEXT_MARKUP | TEXT_WORD_WRAP, &box, "Your IP: [b]%s[/b]\nYou last logged in from [b]%s[/b] at %s.", cpl.host, cpl.last_host, timebuf);

    button_set_parent(&button_tab_characters, popup->x, popup->y);
    button_set_parent(&button_tab_new, popup->x, popup->y);
    button_set_parent(&button_tab_password, popup->x, popup->y);
    button_set_parent(&button_character_male, popup->x, popup->y);
    button_set_parent(&button_character_female, popup->x, popup->y);
    button_set_parent(&button_character_left, popup->x, popup->y);
    button_set_parent(&button_character_right, popup->x, popup->y);
    button_set_parent(&button_login, popup->x, popup->y);
    button_set_parent(&button_done, popup->x, popup->y);

    button_tab_characters.x = 38;
    button_tab_characters.y = 38;
    button_show(&button_tab_characters, "Characters");
    button_tab_new.x = button_tab_characters.x + texture_surface(button_tab_characters.texture)->w + 1;
    button_tab_new.y = button_tab_characters.y;
    button_show(&button_tab_new, "New");
    button_tab_password.x = button_tab_new.x + texture_surface(button_tab_new.texture)->w + 1;
    button_tab_password.y = button_tab_new.y;
    button_show(&button_tab_password, "Password");

    for (i = 0; i < TEXT_INPUT_NUM; i++) {
        text_input_set_parent(&text_inputs[i], popup->x, popup->y);
    }

    if (button_tab_characters.pressed_forced) {
        list_set_parent(list_characters, popup->x, popup->y);
        list_show(list_characters, 36, 50);

        button_login.x = list_characters->x + LIST_WIDTH_FULL(list_characters) / 2 - texture_surface(button_login.texture)->w / 2;
        button_login.y = list_characters->y + LIST_HEIGHT_FULL(list_characters) + 8;
        button_show(&button_login, "[b]Login[/b]");
    }
    else if (button_tab_new.pressed_forced) {
        int max_width, width;

        max_width = 0;

        for (i = 0; i < s_settings->num_characters; i++) {
            width = text_get_width(FONT_SERIF12, s_settings->characters[i].name, 0);

            if (width > max_width) {
                max_width = width;
            }
        }

        text_show_format(popup->surface, FONT_ARIAL10, 38, 90, COLOR_WHITE, TEXT_MARKUP, NULL, "[bar=#202020 50 60][icon=%s 50 60][border=#909090 50 60]", s_settings->characters[character_race].gender_faces[character_gender]);

        button_character_left.x = 100;
        button_character_left.y = 90;
        button_show(&button_character_left, "<");

        box.w = max_width;
        box.h = 0;
        text_show_shadow(popup->surface, FONT_SERIF12, s_settings->characters[character_race].name, button_character_left.x + texture_surface(button_character_left.texture)->w + 5, button_character_left.y, COLOR_HGOLD, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);

        button_character_right.x = button_character_left.x + texture_surface(button_character_left.texture)->w + 5 + max_width + 5;
        button_character_right.y = 90;
        button_show(&button_character_right, ">");

        button_character_male.x = button_character_left.x;
        button_character_male.y = button_character_left.y + 30;
        button_show(&button_character_male, "Male");

        button_character_female.x = button_character_male.x + texture_surface(button_character_female.texture)->w + 5;
        button_character_female.y = button_character_male.y;
        button_show(&button_character_female, "Female");

        box.w = text_inputs[TEXT_INPUT_CHARNAME].coords.w;
        text_show(popup->surface, FONT_ARIAL12, "Character name &lsqb;[tooltip=Enter your character's name.][h=#"COLOR_HGOLD "]?[/h][/tooltip]&rsqb;", 50, 172, COLOR_WHITE, TEXT_MARKUP | TEXT_ALIGN_CENTER, &box);
        text_input_show(&text_inputs[TEXT_INPUT_CHARNAME], popup->surface, 50, 190);

        button_done.x = text_inputs[TEXT_INPUT_CHARNAME].coords.x + text_inputs[TEXT_INPUT_CHARNAME].coords.w - texture_surface(button_done.texture)->w;
        button_done.y = 210;
        button_show(&button_done, "Done");
    }
    else if (button_tab_password.pressed_forced) {
        box.w = text_inputs[TEXT_INPUT_PASSWORD].coords.w;
        text_show(popup->surface, FONT_ARIAL12, "Current password &lsqb;[tooltip=Enter your current password.][h=#"COLOR_HGOLD "]?[/h][/tooltip]&rsqb;", 50, 92, COLOR_WHITE, TEXT_MARKUP | TEXT_ALIGN_CENTER, &box);
        text_show(popup->surface, FONT_ARIAL12, "New password &lsqb;[tooltip=Enter your new password.][h=#"COLOR_HGOLD "]?[/h][/tooltip]&rsqb;", 50, 132, COLOR_WHITE, TEXT_MARKUP | TEXT_ALIGN_CENTER, &box);
        text_show(popup->surface, FONT_ARIAL12, "Verify new password &lsqb;[tooltip=Enter your new password again.][h=#"COLOR_HGOLD "]?[/h][/tooltip]&rsqb;", 50, 172, COLOR_WHITE, TEXT_MARKUP | TEXT_ALIGN_CENTER, &box);

        text_input_show(&text_inputs[TEXT_INPUT_PASSWORD], popup->surface, 50, 110);
        text_input_show(&text_inputs[TEXT_INPUT_PASSWORD_NEW], popup->surface, 50, 150);
        text_input_show(&text_inputs[TEXT_INPUT_PASSWORD_NEW2], popup->surface, 50, 190);

        button_done.x = text_inputs[TEXT_INPUT_PASSWORD_NEW2].coords.x + text_inputs[TEXT_INPUT_CHARNAME].coords.w - texture_surface(button_done.texture)->w;
        button_done.y = 210;
        button_show(&button_done, "Done");
    }

    return 1;
}

/** @copydoc popup_struct::popup_event_func */
static int popup_event(popup_struct *popup, SDL_Event *event)
{
    size_t i;

    if (event->type == SDL_KEYDOWN) {
        if (event->key.keysym.sym == SDLK_ESCAPE) {
            popup_destroy(popup);
            return 1;
        }
    }

    if (button_event(&button_tab_characters, event)) {
        button_tab_switch(&button_tab_characters);
        return 1;
    }
    else if (button_event(&button_tab_new, event)) {
        button_tab_switch(&button_tab_new);
        return 1;
    }
    else if (button_event(&button_tab_password, event)) {
        button_tab_switch(&button_tab_password);
        return 1;
    }
    else if (button_event(button_tab_characters.pressed_forced ? &button_login : &button_done, event)) {
        event_push_key_once(SDLK_RETURN, 0);
        return 1;
    }

    if (button_tab_characters.pressed_forced) {
        if (list_handle_keyboard(list_characters, event) || list_handle_mouse(list_characters, event)) {
            return 1;
        }
    }
    else if (button_tab_new.pressed_forced) {
        if (event->type == SDL_KEYDOWN) {
            if (IS_ENTER(event->key.keysym.sym)) {
                uint32 lower, upper;
                packet_struct *packet;

                if (*text_inputs[TEXT_INPUT_CHARNAME].str == '\0') {
                    draw_info(COLOR_RED, "You must enter a character name.");
                    return 1;
                }
                else if (sscanf(s_settings->text[SERVER_TEXT_ALLOWED_CHARS_CHARNAME_MAX], "%u-%u", &lower, &upper) == 2 && (text_inputs[TEXT_INPUT_CHARNAME].num < lower || text_inputs[TEXT_INPUT_CHARNAME].num > upper)) {
                    draw_info_format(COLOR_RED, "Character name must be between %d and %d characters long.", lower, upper);
                    return 1;
                }

                packet = packet_new(SERVER_CMD_ACCOUNT, 64, 64);
                packet_append_uint8(packet, CMD_ACCOUNT_NEW_CHAR);
                packet_append_string_terminated(packet, text_inputs[TEXT_INPUT_CHARNAME].str);
                packet_append_string_terminated(packet, s_settings->characters[character_race].gender_archetypes[character_gender]);
                socket_send_packet(packet);

                text_input_reset(&text_inputs[TEXT_INPUT_CHARNAME]);

                return 1;
            }
        }

        if (text_input_event(&text_inputs[TEXT_INPUT_CHARNAME], event)) {
            return 1;
        }
        else if (button_event(&button_character_male, event)) {
            character_gender = GENDER_MALE;
            return 1;
        }
        else if (button_event(&button_character_female, event)) {
            character_gender = GENDER_FEMALE;
            return 1;
        }
        else if (button_event(&button_character_left, event)) {
            if (character_race == 0) {
                character_race = s_settings->num_characters - 1;
            }
            else {
                character_race--;
            }
        }
        else if (button_event(&button_character_right, event)) {
            if (character_race == s_settings->num_characters - 1) {
                character_race = 0;
            }
            else {
                character_race++;
            }
        }
    }
    else if (button_tab_password.pressed_forced) {
        if (event->type == SDL_KEYDOWN) {
            if (IS_NEXT(event->key.keysym.sym)) {
                if (IS_ENTER(event->key.keysym.sym) && text_input_current == TEXT_INPUT_PASSWORD_NEW2) {
                    packet_struct *packet;
                    uint32 lower, upper;

                    if (strcmp(text_inputs[TEXT_INPUT_PASSWORD_NEW].str, text_inputs[TEXT_INPUT_PASSWORD_NEW2].str) != 0) {
                        draw_info(COLOR_RED, "The new passwords do not match.");
                        return 1;
                    }

                    packet = packet_new(SERVER_CMD_ACCOUNT, 64, 64);
                    packet_append_uint8(packet, CMD_ACCOUNT_PSWD);

                    for (i = TEXT_INPUT_PASSWORD; i < TEXT_INPUT_NUM; i++) {
                        if (*text_inputs[i].str == '\0') {
                            draw_info(COLOR_RED, "You must enter a valid value for all text inputs.");
                            packet_free(packet);
                            return 1;
                        }
                        else if (sscanf(s_settings->text[SERVER_TEXT_ALLOWED_CHARS_PASSWORD_MAX], "%u-%u", &lower, &upper) == 2 && (text_inputs[i].num < lower || text_inputs[i].num > upper)) {
                            draw_info_format(COLOR_RED, "Password must be between %d and %d characters long.", lower, upper);
                            packet_free(packet);
                            return 1;
                        }

                        packet_append_string_terminated(packet, text_inputs[i].str);
                    }

                    socket_send_packet(packet);

                    for (i = TEXT_INPUT_PASSWORD; i < TEXT_INPUT_NUM; i++) {
                        text_input_reset(&text_inputs[i]);
                        text_inputs[i].focus = 0;
                    }

                    text_input_current = TEXT_INPUT_PASSWORD;
                    text_inputs[text_input_current].focus = 1;

                    return 1;
                }

                text_inputs[text_input_current].focus = 0;
                text_input_current++;

                if (text_input_current > TEXT_INPUT_PASSWORD_NEW2) {
                    text_input_current = TEXT_INPUT_PASSWORD;
                }

                text_inputs[text_input_current].focus = 1;

                return 1;
            }
        }
        else if (event->type == SDL_MOUSEBUTTONDOWN) {
            if (event->button.button == SDL_BUTTON_LEFT) {
                for (i = TEXT_INPUT_PASSWORD; i < TEXT_INPUT_NUM; i++) {
                    if (text_input_mouse_over(&text_inputs[i], event->motion.x, event->motion.y)) {
                        text_inputs[text_input_current].focus = 0;
                        text_input_current = i;
                        text_inputs[text_input_current].focus = 1;
                        return 1;
                    }
                }
            }
        }

        if (text_input_event(&text_inputs[text_input_current], event)) {
            return 1;
        }
    }

    return -1;
}

/** @copydoc popup_struct::destroy_callback_func */
static int popup_destroy_callback(popup_struct *popup)
{
    list_remove(list_characters);

    if (cpl.state != ST_PLAY) {
        cpl.state = ST_START;
    }

    button_destroy(&button_login);

    return 1;
}

/**
 * Open the characters chooser popup. */
void characters_open(void)
{
    popup_struct *popup;
    size_t i;

    progress_dots_create(&progress);

    popup = popup_create(texture_get(TEXTURE_TYPE_CLIENT, "popup"));
    popup->draw_func = popup_draw;
    popup->event_func = popup_event;
    popup->destroy_callback_func = popup_destroy_callback;

    button_create(&button_tab_characters);
    button_create(&button_tab_new);
    button_create(&button_tab_password);
    button_create(&button_character_male);
    button_create(&button_character_female);
    button_create(&button_character_left);
    button_create(&button_character_right);
    button_create(&button_done);
    button_create(&button_login);
    button_tab_characters.pressed_forced = 1;
    button_tab_characters.surface = button_tab_new.surface = button_tab_password.surface = button_character_male.surface = button_character_female.surface = button_character_left.surface = button_character_right.surface = button_login.surface = button_done.surface = popup->surface;
    button_tab_characters.texture = button_tab_new.texture = button_tab_password.texture = texture_get(TEXTURE_TYPE_CLIENT, "button_tab");
    button_tab_characters.texture_over = button_tab_new.texture_over = button_tab_password.texture_over = texture_get(TEXTURE_TYPE_CLIENT, "button_tab_over");
    button_tab_characters.texture_pressed = button_tab_new.texture_pressed = button_tab_password.texture_pressed = texture_get(TEXTURE_TYPE_CLIENT, "button_tab_down");
    button_character_left.texture = button_character_right.texture = texture_get(TEXTURE_TYPE_CLIENT, "button_round");
    button_character_left.texture_over = button_character_right.texture_over = texture_get(TEXTURE_TYPE_CLIENT, "button_round_over");
    button_character_left.texture_pressed = button_character_right.texture_pressed = texture_get(TEXTURE_TYPE_CLIENT, "button_round_down");
    button_login.texture = texture_get(TEXTURE_TYPE_CLIENT, "button_large");
    button_login.texture_over = texture_get(TEXTURE_TYPE_CLIENT, "button_large_over");
    button_login.texture_pressed = texture_get(TEXTURE_TYPE_CLIENT, "button_large_down");
    button_login.flags = TEXT_MARKUP;
    button_set_font(&button_login, FONT_SERIF14);

    for (i = 0; i < TEXT_INPUT_NUM; i++) {
        text_input_create(&text_inputs[i]);
        text_inputs[i].character_check_func = text_input_character_check;
        text_inputs[i].coords.w = 150;
        text_inputs[i].focus = 0;

        if (i != TEXT_INPUT_CHARNAME) {
            text_inputs[i].show_edit_func = text_input_show_edit_password;
        }
    }

    list_characters = list_create(3, 2, 8);
    list_characters->handle_enter_func = list_handle_enter;
    list_characters->text_color_hook = list_text_color;
    list_characters->post_column_func = list_post_column;
    list_characters->surface = popup->surface;
    list_characters->row_height_adjust = 45;
    list_characters->text_flags = TEXT_MARKUP;
    list_set_column(list_characters, 0, 55, 0, NULL, -1);
    list_set_column(list_characters, 1, 150, 0, NULL, -1);
    list_scrollbar_enable(list_characters);
}

/**
 * Resolves an archname to race ID and gender ID, using the information
 * in ::s_settings.
 * @param archname Archname.
 * @param[out] race Will contain race ID.
 * @param[out] gender Will contain gender ID.
 * @return 1 on success, 0 on failure. */
static int archname_to_character(const char *archname, size_t *race, size_t *gender)
{
    for (*race = 0; *race < s_settings->num_characters; (*race)++) {
        for (*gender = 0; *gender < GENDER_MAX; (*gender)++) {
            if (s_settings->characters[*race].gender_archetypes[*gender] && strcmp(s_settings->characters[*race].gender_archetypes[*gender], archname) == 0) {
                return 1;
            }
        }
    }

    return 0;
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_characters(uint8 *data, size_t len, size_t pos)
{
    char archname[MAX_BUF], name[MAX_BUF], region_name[MAX_BUF], buf[MAX_BUF], race_gender[MAX_BUF];
    uint16 anim_id;
    uint8 level;
    size_t race, gender;

    if (len == 0) {
        cpl.state = ST_LOGIN;
        clioption_settings.reconnect = 0;
        return;
    }

    if (cpl.state != ST_CHARACTERS) {
        characters_open();
        cpl.state = ST_CHARACTERS;
    }

    list_clear(list_characters);

    packet_to_string(data, len, &pos, cpl.account, sizeof(cpl.account));
    packet_to_string(data, len, &pos, cpl.host, sizeof(cpl.host));
    packet_to_string(data, len, &pos, cpl.last_host, sizeof(cpl.last_host));
    cpl.last_time = datetime_utctolocal(packet_to_uint64(data, len, &pos));

    while (pos < len) {
        packet_to_string(data, len, &pos, archname, sizeof(archname));
        packet_to_string(data, len, &pos, name, sizeof(name));
        packet_to_string(data, len, &pos, region_name, sizeof(region_name));
        anim_id = packet_to_uint16(data, len, &pos);
        level = packet_to_uint8(data, len, &pos);

        /* If it's a valid player arch, add race and gender information. */
        if (archname_to_character(archname, &race, &gender)) {
            snprintf(race_gender, sizeof(race_gender), "%s %s\n", s_settings->characters[race].name, gender_noun[gender]);
        }
        else {
            *race_gender = '\0';
        }

        /* If we have specified a character in '--connect' command line
         * option, update the selected row and create an enter event. */
        if ((string_isempty(clioption_settings.connect[0]) || strcasecmp(selected_server->name, clioption_settings.connect[0]) == 0) &&
            clioption_settings.connect[3] && (strcasecmp(clioption_settings.connect[3], name) == 0 ||
                (string_isdigit(clioption_settings.connect[3]) && (uint32) atoi(clioption_settings.connect[3]) == list_characters->rows + 1))) {
            list_characters->row_selected = list_characters->rows + 1;

            if (!clioption_settings.reconnect) {
                efree(clioption_settings.connect[3]);
                clioption_settings.connect[3] = NULL;
            }

            event_push_key_once(SDLK_RETURN, 0);
        }

        snprintf(buf, sizeof(buf), "%d", anim_id);
        list_add(list_characters, list_characters->rows, 0, buf);
        snprintf(buf, sizeof(buf), "[y=3][a=charname][c=#"COLOR_HGOLD "][font=serif 12]%s[/font][/c][/a]\n[y=2]%sLevel: %d\n%s", name, race_gender, level, region_name);
        list_add(list_characters, list_characters->rows - 1, 1, buf);
    }

    /* No characters yet, so switch to the character creation tab. */
    if (list_characters->rows == 0) {
        button_tab_switch(&button_tab_new);
    }
    /* Characters tab otherwise. */
    else {
        button_tab_switch(&button_tab_characters);
    }
}
