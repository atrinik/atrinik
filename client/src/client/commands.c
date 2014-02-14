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
 * Handles commands received by the server. This does not necessarily
 * handle all the commands - some might be in other files. */

#include <global.h>

/** @copydoc socket_command_struct::handle_func */
void socket_command_book(uint8 *data, size_t len, size_t pos)
{
    sound_play_effect("book.ogg", 100);
    book_load((char *) data, len);
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_setup(uint8 *data, size_t len, size_t pos)
{
    uint8 type;

    server_files_clear_update();

    while (pos < len) {
        type = packet_to_uint8(data, len, &pos);

        if (type == CMD_SETUP_SOUND) {
            packet_to_uint8(data, len, &pos);
        }
        else if (type == CMD_SETUP_MAPSIZE) {
            int x, y;

            x = packet_to_uint8(data, len, &pos);
            y = packet_to_uint8(data, len, &pos);

            setting_set_int(OPT_CAT_MAP, OPT_MAP_WIDTH, x);
            setting_set_int(OPT_CAT_MAP, OPT_MAP_HEIGHT, y);
        }
        else if (type == CMD_SETUP_SERVER_FILE) {
            uint8 file_type, state;

            file_type = packet_to_uint8(data, len, &pos);
            state = packet_to_uint8(data, len, &pos);

            if (state) {
                server_files_mark_update(file_type);
            }
        }
    }

    if (cpl.state != ST_PLAY) {
        cpl.state = ST_REQUEST_FILES;
    }
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_anim(uint8 *data, size_t len, size_t pos)
{
    uint16 anim_id;
    int i;

    anim_id = packet_to_uint16(data, len, &pos);
    animations[anim_id].flags = packet_to_uint8(data, len, &pos);
    animations[anim_id].facings = packet_to_uint8(data, len, &pos);
    animations[anim_id].num_animations = (len - pos) / 2;

    if (animations[anim_id].facings > 1) {
        animations[anim_id].frame = animations[anim_id].num_animations / animations[anim_id].facings;
    }
    else {
        animations[anim_id].frame = animations[anim_id].num_animations;
    }

    animations[anim_id].faces = malloc(sizeof(uint16) * animations[anim_id].num_animations);

    for (i = 0; pos < len; i++) {
        animations[anim_id].faces[i] = packet_to_uint16(data, len, &pos);
        request_face(animations[anim_id].faces[i]);
    }
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_image(uint8 *data, size_t len, size_t pos)
{
    uint32 facenum, filesize;
    char buf[HUGE_BUF];
    FILE *fp;

    facenum = packet_to_uint32(data, len, &pos);
    filesize = packet_to_uint32(data, len, &pos);

    /* Save picture to cache and load it to FaceList. */
    snprintf(buf, sizeof(buf), DIRECTORY_CACHE "/%s", FaceList[facenum].name);

    fp = fopen_wrapper(buf, "wb+");

    if (fp) {
        fwrite(data + pos, 1, filesize, fp);
        fclose(fp);
    }

    FaceList[facenum].sprite = sprite_tryload_file(buf, 0, NULL);
    map_redraw_flag = 1;

    book_redraw();
    interface_redraw();
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_drawinfo(uint8 *data, size_t len, size_t pos)
{
    uint8 type;
    char color[COLOR_BUF], *str;
    StringBuffer *sb;

    type = packet_to_uint8(data, len, &pos);
    packet_to_string(data, len, &pos, color, sizeof(color));

    sb = stringbuffer_new();
    packet_to_stringbuffer(data, len, &pos, sb);
    str = stringbuffer_finish(sb);

    draw_info_tab(type, color, str);

    free(str);
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_target(uint8 *data, size_t len, size_t pos)
{
    cpl.target_code = packet_to_uint8(data, len, &pos);
    packet_to_string(data, len, &pos, cpl.target_color, sizeof(cpl.target_color));
    packet_to_string(data, len, &pos, cpl.target_name, sizeof(cpl.target_name));

    map_redraw_flag = 1;
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_stats(uint8 *data, size_t len, size_t pos)
{
    uint8 type;
    int temp;

    while (pos < len) {
        type = packet_to_uint8(data, len, &pos);

        if (type >= CS_STAT_EQUIP_START && type <= CS_STAT_EQUIP_END) {
            cpl.equipment[type - CS_STAT_EQUIP_START] = packet_to_uint32(data, len, &pos);
            WIDGET_REDRAW_ALL(PDOLL_ID);
        }
        else {
            switch (type) {
                case CS_STAT_TARGET_HP:
                    cpl.target_hp = packet_to_uint8(data, len, &pos);
                    break;

                case CS_STAT_REG_HP:
                    cpl.gen_hp = abs(packet_to_uint16(data, len, &pos)) / 10.0f;
                    widget_redraw_type_id(STAT_ID, "health");
                    break;

                case CS_STAT_REG_MANA:
                    cpl.gen_sp = abs(packet_to_uint16(data, len, &pos)) / 10.0f;
                    widget_redraw_type_id(STAT_ID, "mana");
                    break;

                case CS_STAT_HP:
                    temp = packet_to_sint32(data, len, &pos);

                    if (temp < cpl.stats.hp && cpl.stats.food) {
                        cpl.warn_hp = 1;

                        if (cpl.stats.maxhp / 12 <= cpl.stats.hp - temp) {
                            cpl.warn_hp = 2;
                        }
                    }

                    cpl.stats.hp = temp;
                    widget_redraw_type_id(STAT_ID, "health");
                    break;

                case CS_STAT_MAXHP:
                    cpl.stats.maxhp = packet_to_sint32(data, len, &pos);
                    widget_redraw_type_id(STAT_ID, "health");
                    break;

                case CS_STAT_SP:
                    cpl.stats.sp = packet_to_sint16(data, len, &pos);
                    widget_redraw_type_id(STAT_ID, "mana");
                    break;

                case CS_STAT_MAXSP:
                    cpl.stats.maxsp = packet_to_sint16(data, len, &pos);
                    widget_redraw_type_id(STAT_ID, "mana");
                    break;

                case CS_STAT_STR:
                case CS_STAT_INT:
                case CS_STAT_POW:
                case CS_STAT_DEX:
                case CS_STAT_CON:
                {
                    sint8 *stat;
                    uint8 stat_new;

                    stat = &(cpl.stats.Str) + (sizeof(cpl.stats.Str) * (type - CS_STAT_STR));
                    stat_new = packet_to_uint8(data, len, &pos);

                    if (*stat != -1) {
                        if (stat_new > *stat) {
                            cpl.warn_statup = 1;
                        }
                        else if (stat_new < *stat) {
                            cpl.warn_statdown = 1;
                        }
                    }

                    *stat = stat_new;
                    break;
                }

                case CS_STAT_PATH_ATTUNED:
                    cpl.path_attuned = packet_to_uint32(data, len, &pos);
                    WIDGET_REDRAW_ALL(SPELLS_ID);
                    break;

                case CS_STAT_PATH_REPELLED:
                    cpl.path_repelled = packet_to_uint32(data, len, &pos);
                    WIDGET_REDRAW_ALL(SPELLS_ID);
                    break;

                case CS_STAT_PATH_DENIED:
                    cpl.path_denied = packet_to_uint32(data, len, &pos);
                    WIDGET_REDRAW_ALL(SPELLS_ID);
                    break;

                case CS_STAT_EXP:
                    cpl.stats.exp = packet_to_uint64(data, len, &pos);
                    WIDGET_REDRAW_ALL(MAIN_LVL_ID);
                    widget_redraw_type_id(STAT_ID, "exp");
                    break;

                case CS_STAT_LEVEL:
                    cpl.stats.level = packet_to_uint8(data, len, &pos);
                    WIDGET_REDRAW_ALL(MAIN_LVL_ID);
                    break;

                case CS_STAT_WC:
                    cpl.stats.wc = packet_to_uint16(data, len, &pos);
                    break;

                case CS_STAT_AC:
                    cpl.stats.ac = packet_to_uint16(data, len, &pos);
                    break;

                case CS_STAT_DAM:
                    cpl.stats.dam = packet_to_uint16(data, len, &pos);
                    break;

                case CS_STAT_SPEED:
                    cpl.stats.speed = packet_to_uint32(data, len, &pos);
                    break;

                case CS_STAT_FOOD:
                    cpl.stats.food = packet_to_uint16(data, len, &pos);
                    widget_redraw_type_id(STAT_ID, "food");
                    break;

                case CS_STAT_WEAPON_SPEED:
                    cpl.stats.weapon_speed = abs(packet_to_uint32(data, len, &pos)) / FLOAT_MULTF;
                    break;

                case CS_STAT_FLAGS:
                    cpl.stats.flags = packet_to_uint16(data, len, &pos);
                    break;

                case CS_STAT_WEIGHT_LIM:
                    cpl.weight_limit = abs(packet_to_uint32(data, len, &pos)) / 1000.0;
                    break;

                case CS_STAT_ACTION_TIME:
                    cpl.action_timer = abs(packet_to_uint32(data, len, &pos)) / 1000.0;
                    WIDGET_REDRAW_ALL(SKILL_EXP_ID);
                    break;

                case CS_STAT_GENDER:
                    cpl.gender = packet_to_uint8(data, len, &pos);
                    break;

                case CS_STAT_EXT_TITLE:
                    packet_to_string(data, len, &pos, cpl.ext_title, sizeof(cpl.ext_title));
                    WIDGET_REDRAW_ALL(PLAYER_INFO_ID);
                    break;

                case CS_STAT_RANGED_DAM:
                    cpl.stats.ranged_dam = packet_to_uint16(data, len, &pos);
                    break;

                case CS_STAT_RANGED_WC:
                    cpl.stats.ranged_wc = packet_to_uint16(data, len, &pos);
                    break;

                case CS_STAT_RANGED_WS:
                    cpl.stats.ranged_ws = packet_to_uint32(data, len, &pos);
                    break;
            }
        }
    }
}

/**
 * Sends a reply to the server.
 * @param text Null terminated string of text to send. */
void send_reply(char *text)
{
    packet_struct *packet;

    packet = packet_new(SERVER_CMD_REPLY, 64, 0);
    packet_append_string_terminated(packet, text);
    socket_send_packet(packet);
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_player(uint8 *data, size_t len, size_t pos)
{
    int tag, weight, face;

    tag = packet_to_uint32(data, len, &pos);
    weight = packet_to_uint32(data, len, &pos);
    face = packet_to_uint32(data, len, &pos);
    request_face(face);
    packet_to_string(data, len, &pos, cpl.name, sizeof(cpl.name));

    new_player(tag, weight, face);
    map_redraw_flag = 1;

    cur_widget[INPUT_ID]->show = 0;

    cpl.state = ST_PLAY;
}

static void command_item_update(uint8 *data, size_t len, size_t *pos, uint32 flags, object *tmp)
{
    if (flags & UPD_LOCATION) {
        /* Currently unused. */
        packet_to_uint32(data, len, pos);
    }

    if (flags & UPD_FLAGS) {
        tmp->flags = packet_to_uint32(data, len, pos);
    }

    if (flags & UPD_WEIGHT) {
        tmp->weight = (double) packet_to_uint32(data, len, pos) / 1000.0;
    }

    if (flags & UPD_FACE) {
        tmp->face = packet_to_uint16(data, len, pos);
        request_face(tmp->face);
    }

    if (flags & UPD_DIRECTION) {
        tmp->direction = packet_to_uint8(data, len, pos);
    }

    if (flags & UPD_TYPE) {
        tmp->itype = packet_to_uint8(data, len, pos);
        tmp->stype = packet_to_uint8(data, len, pos);
        tmp->item_qua = packet_to_uint8(data, len, pos);

        if (tmp->item_qua != 255) {
            tmp->item_con = packet_to_uint8(data, len, pos);
            tmp->item_level = packet_to_uint8(data, len, pos);
            tmp->item_skill_tag = packet_to_uint32(data, len, pos);
        }
    }

    if (flags & UPD_NAME) {
        packet_to_string(data, len, pos, tmp->s_name, sizeof(tmp->s_name));
    }

    if (flags & UPD_ANIM) {
        tmp->animation_id = packet_to_uint16(data, len, pos);
    }

    if (flags & UPD_ANIMSPEED) {
        tmp->anim_speed = packet_to_uint8(data, len, pos);
    }

    if (flags & UPD_NROF) {
        tmp->nrof = packet_to_uint32(data, len, pos);

        if (tmp->nrof == 0) {
            tmp->nrof = 1;
        }
    }

    if (flags & UPD_EXTRA) {
        if (tmp->itype == TYPE_SPELL) {
            uint16 spell_cost;
            uint32 spell_path, spell_flags;
            char spell_msg[MAX_BUF];

            spell_cost = packet_to_uint16(data, len, pos);
            spell_path = packet_to_uint32(data, len, pos);
            spell_flags = packet_to_uint32(data, len, pos);
            packet_to_string(data, len, pos, spell_msg, sizeof(spell_msg));

            spells_update(tmp, spell_cost, spell_path, spell_flags, spell_msg);
        }
        else if (tmp->itype == TYPE_SKILL) {
            uint8 skill_level;
            sint64 skill_exp;

            skill_level = packet_to_uint8(data, len, pos);
            skill_exp = packet_to_sint64(data, len, pos);

            skills_update(tmp, skill_level, skill_exp);
        }
        else if (tmp->itype == TYPE_FORCE || tmp->itype == TYPE_POISONING) {
            sint32 sec;
            char msg[HUGE_BUF];

            sec = packet_to_sint32(data, len, pos);
            packet_to_string(data, len, pos, msg, sizeof(msg));

            widget_active_effects_update(cur_widget[ACTIVE_EFFECTS_ID], tmp, sec, msg);
        }
    }
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_item(uint8 *data, size_t len, size_t pos)
{
    sint32 dmode, loc;
    uint32 tag, flags;
    object *env, *tmp;
    uint8 bflag;

    dmode = packet_to_sint32(data, len, &pos);
    loc = packet_to_sint32(data, len, &pos);

    if (dmode >= 0) {
        object_remove_inventory(object_find(loc));
    }

    /* send item flag */
    if (dmode == -4) {
        /* and redirect it to our invisible sack */
        if (loc == cpl.container_tag) {
            loc = -1;
        }
    }
    /* container flag! */
    else if (dmode == -1) {
        /* we catch the REAL container tag */
        cpl.container_tag = loc;
        object_remove_inventory(object_find(-1));

        /* if this happens, we want to close the container */
        if (loc == -1) {
            cpl.container_tag = -998;
            return;
        }

        /* and redirect it to our invisible sack */
        loc = -1;
    }

    bflag = packet_to_uint8(data, len, &pos);

    env = object_find(loc);

    while (pos < len) {
        tag = packet_to_uint32(data, len, &pos);
        tmp = object_find(tag);

        if (tmp && tmp->env != env) {
            object_remove(tmp);
            tmp = NULL;
        }

        if (!tmp) {
            tmp = object_create(env, tag, bflag);
        }

        flags = UPD_FLAGS | UPD_WEIGHT | UPD_FACE | UPD_DIRECTION | UPD_NAME | UPD_ANIM | UPD_ANIMSPEED | UPD_NROF;

        if (loc) {
            flags |= UPD_TYPE | UPD_EXTRA;
        }

        command_item_update(data, len, &pos, flags, tmp);
    }
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_item_update(uint8 *data, size_t len, size_t pos)
{
    uint32 flags, tag;
    object *tmp;

    flags = packet_to_uint16(data, len, &pos);
    tag = packet_to_uint32(data, len, &pos);

    tmp = object_find(tag);

    if (!tmp) {
        return;
    }

    command_item_update(data, len, &pos, flags, tmp);
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_item_delete(uint8 *data, size_t len, size_t pos)
{
    tag_t tag;

    while (pos < len) {
        tag = packet_to_uint32(data, len, &pos);
        delete_object(tag);
    }
}

/**
 * Plays the footstep sounds when moving on the map. */
static void map_play_footstep(void)
{
    static int step = 0;
    static uint32 tick = 0;

    if (LastTick - tick > 125) {
        step++;

        if (step % 2) {
            sound_play_effect("step1.ogg", 100);
        }
        else {
            step = 0;
            sound_play_effect("step2.ogg", 100);
        }

        tick = LastTick;
    }
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_mapstats(uint8 *data, size_t len, size_t pos)
{
    uint8 type;
    char buf[HUGE_BUF];

    while (pos < len) {
        /* Get the type of this command... */
        type = packet_to_uint8(data, len, &pos);

        /* Change map name. */
        if (type == CMD_MAPSTATS_NAME) {
            packet_to_string(data, len, &pos, buf, sizeof(buf));
            update_map_name(buf);
        }
        /* Change map music. */
        else if (type == CMD_MAPSTATS_MUSIC) {
            packet_to_string(data, len, &pos, buf, sizeof(buf));
            update_map_bg_music(buf);
        }
        /* Change map weather. */
        else if (type == CMD_MAPSTATS_WEATHER) {
            packet_to_string(data, len, &pos, buf, sizeof(buf));
            update_map_weather(buf);
        }
        else if (type == CMD_MAPSTATS_TEXT_ANIM) {
            packet_to_string(data, len, &pos, msg_anim.color, sizeof(msg_anim.color));
            packet_to_string(data, len, &pos, msg_anim.message, sizeof(msg_anim.message));
            msg_anim.tick = SDL_GetTicks();
        }
    }
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_map(uint8 *data, size_t len, size_t pos)
{
    static int mx = 0, my = 0;
    int mask, x, y;
    int mapstat;
    int xpos, ypos;
    int layer, ext_flags;
    uint8 num_layers;

    mapstat = packet_to_uint8(data, len, &pos);

    if (mapstat != MAP_UPDATE_CMD_SAME) {
        char mapname[HUGE_BUF], bg_music[HUGE_BUF], weather[MAX_BUF];

        packet_to_string(data, len, &pos, mapname, sizeof(mapname));
        packet_to_string(data, len, &pos, bg_music, sizeof(bg_music));
        packet_to_string(data, len, &pos, weather, sizeof(weather));

        if (mapstat == MAP_UPDATE_CMD_NEW) {
            int map_w, map_h;

            map_w = packet_to_uint8(data, len, &pos);
            map_h = packet_to_uint8(data, len, &pos);
            xpos = packet_to_uint8(data, len, &pos);
            ypos = packet_to_uint8(data, len, &pos);
            mx = xpos;
            my = ypos;
            init_map_data(map_w, map_h, xpos, ypos);
        }
        else {
            int xoff, yoff;

            mapstat = packet_to_uint8(data, len, &pos);
            xoff = packet_to_sint8(data, len, &pos);
            yoff = packet_to_sint8(data, len, &pos);
            xpos = packet_to_uint8(data, len, &pos);
            ypos = packet_to_uint8(data, len, &pos);
            mx = xpos;
            my = ypos;
            display_mapscroll(xoff, yoff);

            map_play_footstep();
        }

        update_map_name(mapname);
        update_map_bg_music(bg_music);
        update_map_weather(weather);
    }
    else {
        xpos = packet_to_uint8(data, len, &pos);
        ypos = packet_to_uint8(data, len, &pos);

        /* Have we moved? */
        if ((xpos - mx || ypos - my)) {
            display_mapscroll(xpos - mx, ypos - my);
            map_play_footstep();
        }

        mx = xpos;
        my = ypos;
    }

    MapData.posx = xpos;
    MapData.posy = ypos;

    while (pos < len) {
        mask = packet_to_uint16(data, len, &pos);
        x = (mask >> 11) & 0x1f;
        y = (mask >> 6) & 0x1f;

        /* Clear the whole cell? */
        if (mask & MAP2_MASK_CLEAR) {
            map_clear_cell(x, y);
            continue;
        }

        /* Do we have darkness information? */
        if (mask & MAP2_MASK_DARKNESS) {
            map_set_darkness(x, y, packet_to_uint8(data, len, &pos));
        }

        num_layers = packet_to_uint8(data, len, &pos);

        /* Go through all the layers on this tile. */
        for (layer = 0; layer < num_layers; layer++) {
            uint8 type;

            type = packet_to_uint8(data, len, &pos);

            /* Clear this layer. */
            if (type == MAP2_LAYER_CLEAR) {
                map_set_data(x, y, packet_to_uint8(data, len, &pos), 0, 0, 0, "", "", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            }
            /* We have some data. */
            else {
                sint16 face, height = 0, zoom_x = 0, zoom_y = 0, align = 0, rotate = 0;
                uint8 flags, obj_flags, quick_pos = 0, probe = 0, draw_double = 0, alpha = 0, infravision = 0, target_is_friend = 0;
                char player_name[64], player_color[COLOR_BUF];
                uint32 target_object_count = 0;

                player_name[0] = '\0';
                player_color[0] = '\0';

                face = packet_to_uint16(data, len, &pos);
                /* Request the face. */
                request_face(face);
                /* Object flags. */
                obj_flags = packet_to_uint8(data, len, &pos);
                /* Flags of this layer. */
                flags = packet_to_uint8(data, len, &pos);

                /* Multi-arch? */
                if (flags & MAP2_FLAG_MULTI) {
                    quick_pos = packet_to_uint8(data, len, &pos);
                }

                /* Player name? */
                if (flags & MAP2_FLAG_NAME) {
                    packet_to_string(data, len, &pos, player_name, sizeof(player_name));
                    packet_to_string(data, len, &pos, player_color, sizeof(player_color));
                }

                /* Target's HP? */
                if (flags & MAP2_FLAG_PROBE) {
                    probe = packet_to_uint8(data, len, &pos);
                }

                /* Z position? */
                if (flags & MAP2_FLAG_HEIGHT) {
                    height = packet_to_sint16(data, len, &pos);
                }

                /* Zoom? */
                if (flags & MAP2_FLAG_ZOOM) {
                    zoom_x = packet_to_uint16(data, len, &pos);
                    zoom_y = packet_to_uint16(data, len, &pos);
                }

                /* Align? */
                if (flags & MAP2_FLAG_ALIGN) {
                    align = packet_to_sint16(data, len, &pos);
                }

                /* Double? */
                if (flags & MAP2_FLAG_DOUBLE) {
                    draw_double = 1;
                }

                if (flags & MAP2_FLAG_MORE) {
                    uint32 flags2;

                    flags2 = packet_to_uint32(data, len, &pos);

                    if (flags2 & MAP2_FLAG2_ALPHA) {
                        alpha = packet_to_uint8(data, len, &pos);
                    }

                    if (flags2 & MAP2_FLAG2_ROTATE) {
                        rotate = packet_to_sint16(data, len, &pos);
                    }

                    if (flags2 & MAP2_FLAG2_INFRAVISION) {
                        infravision = 1;
                    }

                    if (flags2 & MAP2_FLAG2_TARGET) {
                        target_object_count = packet_to_uint32(data, len, &pos);
                        target_is_friend = packet_to_uint8(data, len, &pos);
                    }
                }

                /* Set the data we figured out. */
                map_set_data(x, y, type, face, quick_pos, obj_flags, player_name, player_color, height, probe, zoom_x, zoom_y, align, draw_double, alpha, rotate, infravision, target_object_count, target_is_friend);
            }
        }

        /* Get tile flags. */
        ext_flags = packet_to_uint8(data, len, &pos);

        /* Animation? */
        if (ext_flags & MAP2_FLAG_EXT_ANIM) {
            uint8 anim_type;
            sint16 anim_value;

            anim_type = packet_to_uint8(data, len, &pos);
            anim_value = packet_to_uint16(data, len, &pos);

            add_anim(anim_type, xpos + x, ypos + y, anim_value);
        }
    }

    adjust_tile_stretch();
    map_redraw_flag = 1;
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_version(uint8 *data, size_t len, size_t pos)
{
    if (cpl.state != ST_WAITVERSION) {
        logger_print(LOG(BUG), "Received version command when not in proper "
            "state: %d, should be: %d.", cpl.state, ST_WAITVERSION);
        return;
    }

    cpl.server_socket_version = packet_to_uint32(data, len, &pos);
    cpl.state = ST_VERSION;
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_data(uint8 *data, size_t len, size_t pos)
{
    uint8 data_type;
    unsigned long len_ucomp;
    unsigned char *dest;

    data_type = packet_to_uint8(data, len, &pos);
    len_ucomp = packet_to_uint32(data, len, &pos);
    len -= pos;
    /* Allocate large enough buffer to hold the uncompressed file. */
    dest = malloc(len_ucomp);

    uncompress((Bytef *) dest, (uLongf *) &len_ucomp, (const Bytef *) data + pos, (uLong) len);
    server_file_save(data_type, dest, len_ucomp);
    free(dest);
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_compressed(uint8 *data, size_t len, size_t pos)
{
    unsigned long ucomp_len;
    uint8 type, *dest;
    size_t dest_size;
    command_buffer *buf;

    type = packet_to_uint8(data, len, &pos);
    ucomp_len = packet_to_uint32(data, len, &pos);

    dest_size = ucomp_len + 1;
    dest = malloc(dest_size);

    if (!dest) {
        logger_print(LOG(ERROR), "OOM.");
        exit(1);
    }

    dest[0] = type;
    uncompress((Bytef *) dest + 1, (uLongf *) &ucomp_len, (const Bytef *) data + pos, (uLong) len - pos);

    buf = command_buffer_new(ucomp_len + 1, dest);
    add_input_command(buf);

    free(dest);
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_control(uint8 *data, size_t len, size_t pos)
{
    char app_name[MAX_BUF];
    uint8 type;

    packet_to_string(data, len, &pos, app_name, sizeof(app_name));
    type = packet_to_uint8(data, len, &pos);

    if (type == CMD_CONTROL_UPDATE_MAP) {
        x11_window_activate(SDL_display, x11_window_get_parent(SDL_display, SDL_window), 1);
    }
}
