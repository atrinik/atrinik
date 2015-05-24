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
 * Handles commands received by the server. This does not necessarily
 * handle all the commands - some might be in other files. */

#include <global.h>
#include <region_map.h>
#include <packet.h>

/** @copydoc socket_command_struct::handle_func */
void socket_command_book(uint8_t *data, size_t len, size_t pos)
{
    sound_play_effect("book.ogg", 100);
    book_load((char *) data, len);
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_setup(uint8_t *data, size_t len, size_t pos)
{
    uint8_t type;

    while (pos < len) {
        type = packet_to_uint8(data, len, &pos);

        if (type == CMD_SETUP_SOUND) {
            packet_to_uint8(data, len, &pos);
        } else if (type == CMD_SETUP_MAPSIZE) {
            int x, y;

            x = packet_to_uint8(data, len, &pos);
            y = packet_to_uint8(data, len, &pos);

            setting_set_int(OPT_CAT_MAP, OPT_MAP_WIDTH, x);
            setting_set_int(OPT_CAT_MAP, OPT_MAP_HEIGHT, y);
        } else if (type == CMD_SETUP_DATA_URL) {
            packet_to_string(data, len, &pos, cpl.http_url,
                    sizeof(cpl.http_url));
        }
    }

    if (cpl.state != ST_PLAY) {
        cpl.state = ST_REQUEST_FILES_LISTING;
    }
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_anim(uint8_t *data, size_t len, size_t pos)
{
    uint16_t anim_id;
    int i;

    anim_id = packet_to_uint16(data, len, &pos);
    animations[anim_id].flags = packet_to_uint8(data, len, &pos);
    animations[anim_id].facings = packet_to_uint8(data, len, &pos);
    animations[anim_id].num_animations = (len - pos) / 2;

    if (animations[anim_id].facings > 1) {
        animations[anim_id].frame = animations[anim_id].num_animations / animations[anim_id].facings;
    } else {
        animations[anim_id].frame = animations[anim_id].num_animations;
    }

    animations[anim_id].faces = emalloc(sizeof(uint16_t) * animations[anim_id].num_animations);

    for (i = 0; pos < len; i++) {
        animations[anim_id].faces[i] = packet_to_uint16(data, len, &pos);
        request_face(animations[anim_id].faces[i]);
    }
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_image(uint8_t *data, size_t len, size_t pos)
{
    uint32_t facenum, filesize;
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
    map_redraw_flag = minimap_redraw_flag = 1;

    book_redraw();
    interface_redraw();

    /* TODO: this could be a bit more intelligent to detect whether any of
     * these widgets actually contain an object with the updated face. */
    WIDGET_REDRAW_ALL(PDOLL_ID);
    WIDGET_REDRAW_ALL(QUICKSLOT_ID);
    WIDGET_REDRAW_ALL(INVENTORY_ID);
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_drawinfo(uint8_t *data, size_t len, size_t pos)
{
    uint8_t type;
    char color[COLOR_BUF], *str;
    StringBuffer *sb;

    type = packet_to_uint8(data, len, &pos);
    packet_to_string(data, len, &pos, color, sizeof(color));

    sb = stringbuffer_new();
    packet_to_stringbuffer(data, len, &pos, sb);
    str = stringbuffer_finish(sb);

    draw_info_tab(type, color, str);

    efree(str);
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_target(uint8_t *data, size_t len, size_t pos)
{
    cpl.target_code = packet_to_uint8(data, len, &pos);
    packet_to_string(data, len, &pos, cpl.target_color, sizeof(cpl.target_color));
    packet_to_string(data, len, &pos, cpl.target_name, sizeof(cpl.target_name));
    cpl.combat = packet_to_uint8(data, len, &pos);
    cpl.combat_force = packet_to_uint8(data, len, &pos);
    WIDGET_REDRAW_ALL(TARGET_ID);

    map_redraw_flag = 1;
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_stats(uint8_t *data, size_t len, size_t pos)
{
    uint8_t type;
    int temp;

    while (pos < len) {
        type = packet_to_uint8(data, len, &pos);

        if (type >= CS_STAT_EQUIP_START && type <= CS_STAT_EQUIP_END) {
            cpl.equipment[type - CS_STAT_EQUIP_START] = packet_to_uint32(data, len, &pos);
            WIDGET_REDRAW_ALL(PDOLL_ID);
        } else if (type >= CS_STAT_PROT_START && type <= CS_STAT_PROT_END) {
            cpl.stats.protection[type - CS_STAT_PROT_START] =
                    packet_to_int8(data, len, &pos);
            WIDGET_REDRAW_ALL(PROTECTIONS_ID);
        } else {
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
                temp = packet_to_int32(data, len, &pos);

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
                cpl.stats.maxhp = packet_to_int32(data, len, &pos);
                widget_redraw_type_id(STAT_ID, "health");
                break;

            case CS_STAT_SP:
                cpl.stats.sp = packet_to_int16(data, len, &pos);
                widget_redraw_type_id(STAT_ID, "mana");
                break;

            case CS_STAT_MAXSP:
                cpl.stats.maxsp = packet_to_int16(data, len, &pos);
                widget_redraw_type_id(STAT_ID, "mana");
                break;

            case CS_STAT_STR:
            case CS_STAT_INT:
            case CS_STAT_POW:
            case CS_STAT_DEX:
            case CS_STAT_CON:
            {
                int8_t *stat_curr;
                uint8_t stat_new;

                stat_curr = &(cpl.stats.Str) + (sizeof(cpl.stats.Str) * (type - CS_STAT_STR));
                stat_new = packet_to_uint8(data, len, &pos);

                if (*stat_curr != -1) {
                    if (stat_new > *stat_curr) {
                        cpl.warn_statup = 1;
                    } else if (stat_new < *stat_curr) {
                        cpl.warn_statdown = 1;
                    }
                }

                *stat_curr = stat_new;
                WIDGET_REDRAW_ALL(PDOLL_ID);
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
                widget_redraw_type_id(STAT_ID, "exp");
                break;

            case CS_STAT_LEVEL:
                cpl.stats.level = packet_to_uint8(data, len, &pos);
                WIDGET_REDRAW_ALL(PLAYER_INFO_ID);
                break;

            case CS_STAT_WC:
                cpl.stats.wc = packet_to_uint16(data, len, &pos);
                WIDGET_REDRAW_ALL(PDOLL_ID);
                break;

            case CS_STAT_AC:
                cpl.stats.ac = packet_to_uint16(data, len, &pos);
                WIDGET_REDRAW_ALL(PDOLL_ID);
                break;

            case CS_STAT_DAM:
                cpl.stats.dam = packet_to_uint16(data, len, &pos);
                WIDGET_REDRAW_ALL(PDOLL_ID);
                break;

            case CS_STAT_SPEED:
                cpl.stats.speed = packet_to_double(data, len, &pos);
                WIDGET_REDRAW_ALL(PDOLL_ID);
                break;

            case CS_STAT_FOOD:
                cpl.stats.food = packet_to_uint16(data, len, &pos);
                widget_redraw_type_id(STAT_ID, "food");
                break;

            case CS_STAT_WEAPON_SPEED:
                cpl.stats.weapon_speed = packet_to_double(data, len, &pos);
                WIDGET_REDRAW_ALL(PDOLL_ID);
                break;

            case CS_STAT_FLAGS:
                cpl.stats.flags = packet_to_uint16(data, len, &pos);
                break;

            case CS_STAT_WEIGHT_LIM:
                cpl.weight_limit = abs(packet_to_uint32(data, len, &pos)) / 1000.0;
                break;

            case CS_STAT_ACTION_TIME:
                cpl.action_timer = packet_to_float(data, len, &pos);
                WIDGET_REDRAW_ALL(PLAYER_INFO_ID);
                break;

            case CS_STAT_GENDER:
                cpl.gender = packet_to_uint8(data, len, &pos);
                WIDGET_REDRAW_ALL(PDOLL_ID);
                break;

            case CS_STAT_RANGED_DAM:
                cpl.stats.ranged_dam = packet_to_uint16(data, len, &pos);
                WIDGET_REDRAW_ALL(PDOLL_ID);
                break;

            case CS_STAT_RANGED_WC:
                cpl.stats.ranged_wc = packet_to_uint16(data, len, &pos);
                WIDGET_REDRAW_ALL(PDOLL_ID);
                break;

            case CS_STAT_RANGED_WS:
                cpl.stats.ranged_ws = packet_to_float(data, len, &pos);
                WIDGET_REDRAW_ALL(PDOLL_ID);
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
void socket_command_player(uint8_t *data, size_t len, size_t pos)
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

    if (cur_widget[PARTY_ID]->show) {
        send_command("/party list");
    }

    cpl.state = ST_PLAY;
}

static void command_item_update(uint8_t *data, size_t len, size_t *pos, uint32_t flags, object *tmp)
{
    bool force_anim;

    force_anim = false;

    if (flags & UPD_LOCATION) {
        /* Currently unused. */
        packet_to_uint32(data, len, pos);
    }

    if (flags & UPD_FLAGS) {
        tmp->flags = packet_to_uint32(data, len, pos);
    }

    if (flags & UPD_WEIGHT) {
        tmp->weight = packet_to_uint32(data, len, pos) / 1000.0;
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
        uint16_t animation_id;

        animation_id = packet_to_uint16(data, len, pos);

        /* Changing animation ID, force animation. */
        if (tmp->animation_id != animation_id) {
            force_anim = true;
            tmp->anim_state = 0;
        }

        tmp->animation_id = animation_id;
    }

    if (flags & UPD_ANIMSPEED) {
        uint8_t anim_speed;

        anim_speed = packet_to_uint8(data, len, pos);

        /* Animation was disabled and we're enabling it, force animation. */
        if (tmp->anim_speed == 0 && anim_speed != 0) {
            force_anim = true;
        }

        tmp->anim_speed = anim_speed;
    }

    if (flags & UPD_NROF) {
        tmp->nrof = packet_to_uint32(data, len, pos);

        if (tmp->nrof == 0) {
            tmp->nrof = 1;
        }
    }

    if (flags & UPD_EXTRA) {
        if (tmp->itype == TYPE_SPELL) {
            uint16_t spell_cost;
            uint32_t spell_path, spell_flags;
            char spell_msg[MAX_BUF];

            spell_cost = packet_to_uint16(data, len, pos);
            spell_path = packet_to_uint32(data, len, pos);
            spell_flags = packet_to_uint32(data, len, pos);
            packet_to_string(data, len, pos, spell_msg, sizeof(spell_msg));

            spells_update(tmp, spell_cost, spell_path, spell_flags, spell_msg);
        } else if (tmp->itype == TYPE_SKILL) {
            uint8_t skill_level;
            int64_t skill_exp;

            skill_level = packet_to_uint8(data, len, pos);
            skill_exp = packet_to_int64(data, len, pos);

            skills_update(tmp, skill_level, skill_exp);
        } else if (tmp->itype == TYPE_FORCE || tmp->itype == TYPE_POISONING) {
            int32_t sec;
            char msg[HUGE_BUF];

            sec = packet_to_int32(data, len, pos);
            packet_to_string(data, len, pos, msg, sizeof(msg));

            widget_active_effects_update(cur_widget[ACTIVE_EFFECTS_ID], tmp, sec, msg);
        }
    }

    if (tmp->itype == TYPE_REGION_MAP) {
        region_map_fow_update(MapData.region_map);
        minimap_redraw_flag = 1;
    }

    if (force_anim) {
        tmp->last_anim = tmp->anim_speed;
        object_animate(tmp);
    }

    object_redraw(tmp);
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_item(uint8_t *data, size_t len, size_t pos)
{
    int32_t dmode, loc;
    uint32_t tag, flags;
    object *env, *tmp;
    uint8_t bflag;

    dmode = packet_to_int32(data, len, &pos);
    loc = packet_to_int32(data, len, &pos);

    if (dmode >= 0) {
        object_remove_inventory(object_find(loc));
    }

    if (dmode == -4) { /* send item flag */
        /* and redirect it to our invisible sack */
        if (loc == cpl.container_tag) {
            loc = -1;
        }
    } else if (dmode == -1) { /* container flag! */
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
void socket_command_item_update(uint8_t *data, size_t len, size_t pos)
{
    uint32_t flags, tag;
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
void socket_command_item_delete(uint8_t *data, size_t len, size_t pos)
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
    static uint32_t tick = 0;

    if (LastTick - tick > 125) {
        step++;

        if (step % 2) {
            sound_play_effect("step1.ogg", 100);
        } else {
            step = 0;
            sound_play_effect("step2.ogg", 100);
        }

        tick = LastTick;
    }
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_mapstats(uint8_t *data, size_t len, size_t pos)
{
    uint8_t type;
    char buf[HUGE_BUF];

    while (pos < len) {
        /* Get the type of this command... */
        type = packet_to_uint8(data, len, &pos);

        if (type == CMD_MAPSTATS_NAME) {
            /* Change map name. */
            packet_to_string(data, len, &pos, buf, sizeof(buf));
            update_map_name(buf);
        } else if (type == CMD_MAPSTATS_MUSIC) {
            /* Change map music. */
            packet_to_string(data, len, &pos, buf, sizeof(buf));
            update_map_bg_music(buf);
        } else if (type == CMD_MAPSTATS_WEATHER) {
            /* Change map weather. */
            packet_to_string(data, len, &pos, buf, sizeof(buf));
            update_map_weather(buf);
        } else if (type == CMD_MAPSTATS_TEXT_ANIM) {
            packet_to_string(data, len, &pos, msg_anim.color, sizeof(msg_anim.color));
            packet_to_string(data, len, &pos, msg_anim.message, sizeof(msg_anim.message));
            msg_anim.tick = SDL_GetTicks();
        }
    }
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_map(uint8_t *data, size_t len, size_t pos)
{
    static int mx = 0, my = 0;
    int mask, x, y, rx, ry;
    int mapstat;
    int xpos, ypos;
    int layer, ext_flags;
    uint8_t num_layers, in_building;
    region_map_def_map_t *def_map;
    bool region_map_fow_need_update;

    mapstat = packet_to_uint8(data, len, &pos);

    if (mapstat != MAP_UPDATE_CMD_SAME) {
        char mapname[HUGE_BUF], bg_music[HUGE_BUF], weather[MAX_BUF],
                region_name[MAX_BUF], region_longname[MAX_BUF],
                mappath[HUGE_BUF];
        uint8_t height_diff;

        packet_to_string(data, len, &pos, mapname, sizeof(mapname));
        packet_to_string(data, len, &pos, bg_music, sizeof(bg_music));
        packet_to_string(data, len, &pos, weather, sizeof(weather));
        height_diff = packet_to_uint8(data, len, &pos);
        MapData.region_has_map = packet_to_uint8(data, len, &pos);
        packet_to_string(data, len, &pos, VS(region_name));
        packet_to_string(data, len, &pos, VS(region_longname));
        packet_to_string(data, len, &pos, VS(mappath));

        if (mapstat == MAP_UPDATE_CMD_NEW) {
            int map_w, map_h;

            map_w = packet_to_uint8(data, len, &pos);
            map_h = packet_to_uint8(data, len, &pos);
            xpos = packet_to_uint8(data, len, &pos);
            ypos = packet_to_uint8(data, len, &pos);
            mx = xpos;
            my = ypos;
            init_map_data(map_w, map_h, xpos, ypos);
        } else {
            int xoff, yoff;

            mapstat = packet_to_uint8(data, len, &pos);
            xoff = packet_to_int8(data, len, &pos);
            yoff = packet_to_int8(data, len, &pos);
            xpos = packet_to_uint8(data, len, &pos);
            ypos = packet_to_uint8(data, len, &pos);
            mx = xpos;
            my = ypos;
            display_mapscroll(xoff, yoff, 0, 0);

            map_play_footstep();
        }

        update_map_name(mapname);
        update_map_bg_music(bg_music);
        update_map_weather(weather);
        update_map_height_diff(height_diff);
        update_map_region_name(region_name);
        update_map_region_longname(region_longname);
        update_map_path(mappath);
    } else {
        xpos = packet_to_uint8(data, len, &pos);
        ypos = packet_to_uint8(data, len, &pos);

        /* Have we moved? */
        if ((xpos - mx || ypos - my)) {
            display_mapscroll(xpos - mx, ypos - my, 0, 0);
            map_play_footstep();
        }

        mx = xpos;
        my = ypos;
    }

    MapData.posx = xpos;
    MapData.posy = ypos;
    MapData.player_sub_layer = packet_to_uint8(data, len, &pos);
    def_map = region_map_find_map(MapData.region_map, MapData.map_path);

    in_building = packet_to_uint8(data, len, &pos);

    map_get_real_coords(&rx, &ry);
    region_map_fow_need_update = false;

    while (pos < len) {
        mask = packet_to_uint16(data, len, &pos);
        x = (mask >> 11) & 0x1f;
        y = (mask >> 6) & 0x1f;

        /* Clear the whole cell? */
        if (mask & MAP2_MASK_CLEAR) {
            map_clear_cell(x, y);
            continue;
        }

        if (MapData.region_name[0] != '\0') {
            if (region_map_fow_set_visited(MapData.region_map, def_map,
                    MapData.map_path, rx + x, ry + y)) {
                region_map_fow_need_update = true;
            }
        }

        /* Do we have darkness information? */
        if (mask & MAP2_MASK_DARKNESS) {
            map_set_darkness(x, y, 0, packet_to_uint8(data, len, &pos));
        }

        if (mask & MAP2_MASK_DARKNESS_MORE) {
            int sub_layer;

            for (sub_layer = 1; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
                map_set_darkness(x, y, sub_layer, packet_to_uint8(data, len,
                        &pos));
            }
        }

        num_layers = packet_to_uint8(data, len, &pos);

        /* Go through all the layers on this tile. */
        for (layer = 0; layer < num_layers; layer++) {
            uint8_t type;

            type = packet_to_uint8(data, len, &pos);

            /* Clear this layer. */
            if (type == MAP2_LAYER_CLEAR) {
                map_set_data(x, y, packet_to_uint8(data, len, &pos), 0, 0, 0, "", "", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            } else { /* We have some data. */
                int16_t face, height = 0, zoom_x = 0, zoom_y = 0, align = 0, rotate = 0;
                uint8_t flags, obj_flags, quick_pos = 0, probe = 0, draw_double = 0, alpha = 0, infravision = 0, target_is_friend = 0;
                uint8_t anim_speed, anim_facing, anim_flags, anim_state, priority, secondpass;
                char player_name[64], player_color[COLOR_BUF];
                uint32_t target_object_count = 0;

                anim_speed = anim_facing = anim_flags = anim_state = 0;
                priority = secondpass = 0;

                player_name[0] = '\0';
                player_color[0] = '\0';

                face = packet_to_uint16(data, len, &pos);
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

                /* Animation? */
                if (flags & MAP2_FLAG_ANIMATION) {
                    anim_speed = packet_to_uint8(data, len, &pos);
                    anim_facing = packet_to_uint8(data, len, &pos);
                    anim_flags = packet_to_uint8(data, len, &pos);

                    if (anim_flags & ANIM_FLAG_MOVING) {
                        anim_state = packet_to_uint8(data, len, &pos);
                    }
                }

                /* Z position? */
                if (flags & MAP2_FLAG_HEIGHT) {
                    height = packet_to_int16(data, len, &pos);
                }

                /* Align? */
                if (flags & MAP2_FLAG_ALIGN) {
                    align = packet_to_int16(data, len, &pos);
                }

                if (flags & MAP2_FLAG_INFRAVISION) {
                    infravision = 1;
                }

                /* Double? */
                if (flags & MAP2_FLAG_DOUBLE) {
                    draw_double = 1;
                }

                if (flags & MAP2_FLAG_MORE) {
                    uint32_t flags2;

                    flags2 = packet_to_uint32(data, len, &pos);

                    if (flags2 & MAP2_FLAG2_ALPHA) {
                        alpha = packet_to_uint8(data, len, &pos);
                    }

                    if (flags2 & MAP2_FLAG2_ROTATE) {
                        rotate = packet_to_int16(data, len, &pos);
                    }

                    /* Zoom? */
                    if (flags2 & MAP2_FLAG2_ZOOM) {
                        zoom_x = packet_to_uint16(data, len, &pos);
                        zoom_y = packet_to_uint16(data, len, &pos);
                    }

                    if (flags2 & MAP2_FLAG2_TARGET) {
                        target_object_count = packet_to_uint32(data, len, &pos);
                        target_is_friend = packet_to_uint8(data, len, &pos);
                    }

                    /* Target's HP? */
                    if (flags2 & MAP2_FLAG2_PROBE) {
                        probe = packet_to_uint8(data, len, &pos);
                    }

                    if (flags2 & MAP2_FLAG2_PRIORITY) {
                        priority = 1;
                    }

                    if (flags2 & MAP2_FLAG2_SECONDPASS) {
                        secondpass = 1;
                    }
                }

                /* Set the data we figured out. */
                map_set_data(x, y, type, face, quick_pos, obj_flags,
                        player_name, player_color, height, probe, zoom_x,
                        zoom_y, align, draw_double, alpha, rotate, infravision,
                        target_object_count, target_is_friend, anim_speed,
                        anim_facing, anim_flags, anim_state, priority,
                        secondpass);
            }
        }

        /* Get tile flags. */
        ext_flags = packet_to_uint8(data, len, &pos);

        /* Animation? */
        if (ext_flags & MAP2_FLAG_EXT_ANIM) {
            uint8_t anim_num = packet_to_uint8(data, len, &pos);

            for (uint8_t i = 0; i < anim_num; i++) {
                uint8_t sub_layer = packet_to_uint8(data, len, &pos);
                uint8_t anim_type = packet_to_uint8(data, len, &pos);
                int16_t anim_value = packet_to_int16(data, len, &pos);

                map_anims_add(anim_type, x, y, sub_layer, anim_value);
            }
        }
    }

    adjust_tile_stretch();
    map_update_in_building(in_building);
    map_redraw_flag = minimap_redraw_flag = 1;

    if (region_map_fow_need_update) {
        region_map_fow_update(MapData.region_map);
    }
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_version(uint8_t *data, size_t len, size_t pos)
{
    if (cpl.state != ST_WAITVERSION) {
        LOG(BUG, "Received version command when not in proper "
                "state: %d, should be: %d.", cpl.state, ST_WAITVERSION);
        return;
    }

    cpl.server_socket_version = packet_to_uint32(data, len, &pos);
    cpl.state = ST_VERSION;
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_compressed(uint8_t *data, size_t len, size_t pos)
{
    unsigned long ucomp_len;
    uint8_t type, *dest;
    size_t dest_size;

    type = packet_to_uint8(data, len, &pos);
    ucomp_len = packet_to_uint32(data, len, &pos);

    dest_size = ucomp_len + 1;
    dest = emalloc(dest_size);
    dest[0] = type;

    if (uncompress((Bytef *) dest + 1, (uLongf *) & ucomp_len,
            (const Bytef *) data + pos, (uLong) len - pos) == Z_OK) {
        command_buffer *buf;

        buf = command_buffer_new(ucomp_len + 1, dest);
        add_input_command(buf);
    }

    efree(dest);
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_control(uint8_t *data, size_t len, size_t pos)
{
    char app_name[MAX_BUF];
    uint8_t type, sub_type;

    packet_to_string(data, len, &pos, app_name, sizeof(app_name));
    type = packet_to_uint8(data, len, &pos);
    sub_type = packet_to_uint8(data, len, &pos);

    if (type == CMD_CONTROL_PLAYER && sub_type == CMD_CONTROL_PLAYER_TELEPORT) {
        x11_window_activate(SDL_display, x11_window_get_parent(SDL_display, SDL_window), 1);
    }
}
