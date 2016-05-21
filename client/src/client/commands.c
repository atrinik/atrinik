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
 * handle all the commands - some might be in other files.
 */

#include <global.h>
#include <region_map.h>
#include <packet.h>
#include <path.h>
#include <toolkit_string.h>

/** @copydoc socket_command_struct::handle_func */
void socket_command_book(uint8_t *data, size_t len, size_t pos)
{
    sound_play_effect("book.ogg", 100);
    book_load((char *) data + pos, len);
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
        image_request_face(animations[anim_id].faces[i]);
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

    fp = path_fopen(buf, "wb+");

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
                WIDGET_REDRAW_ALL(TARGET_ID);
                break;

            case CS_STAT_REG_HP:
                cpl.gen_hp = packet_to_uint16(data, len, &pos) / 10.0f;
                widget_redraw_type_id(STAT_ID, "health");
                break;

            case CS_STAT_REG_MANA:
                cpl.gen_sp = packet_to_uint16(data, len, &pos) / 10.0f;
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
                cpl.weight_limit = packet_to_uint32(data, len, &pos) / 1000.0;
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
 * @param text
 * Null terminated string of text to send.
 */
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
    image_request_face(face);
    packet_to_string(data, len, &pos, cpl.name, sizeof(cpl.name));

    new_player(tag, weight, face);
    map_redraw_flag = 1;

    cur_widget[INPUT_ID]->show = 0;

    if (cur_widget[PARTY_ID]->show) {
        send_command("/party list");
    }

    cpl.state = ST_PLAY;
}

void command_item_update(uint8_t *data, size_t len, size_t *pos, uint32_t flags, object *tmp)
{
    bool force_anim = false;

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
        image_request_face(tmp->face);
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
            uint8_t skill_level = packet_to_uint8(data, len, pos);
            int64_t skill_exp = packet_to_int64(data, len, pos);
            char skill_msg[MAX_BUF];
            packet_to_string(data, len, pos, VS(skill_msg));

            skills_update(tmp, skill_level, skill_exp, skill_msg);
        } else if (tmp->itype == TYPE_FORCE || tmp->itype == TYPE_POISONING) {
            int32_t sec;
            char msg[HUGE_BUF];

            sec = packet_to_int32(data, len, pos);
            packet_to_string(data, len, pos, msg, sizeof(msg));

            widget_active_effects_update(cur_widget[ACTIVE_EFFECTS_ID], tmp, sec, msg);
        }
    }

    if (flags & UPD_GLOW) {
        packet_to_string(data, len, pos, VS(tmp->glow));
        tmp->glow_speed = packet_to_uint8(data, len, pos);
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
    bool delete_env = packet_to_uint8(data, len, &pos) == 1;
    if (delete_env) {
        tag_t loc_delete = packet_to_uint32(data, len, &pos);
        object *env = object_find(loc_delete);
        if (env == NULL) {
            return;
        }

        object_remove_inventory(env);

        if (pos == len) {
            return;
        }
    }

    tag_t loc = packet_to_uint32(data, len, &pos);
    object *env = object_find(loc);
    if (env == NULL) {
        LOG(ERROR, "Server sent invalid location: %" PRIu32, loc);
        return;
    }

    if (env != cpl.below && env != cpl.ob) {
        cpl.sack = env;
    }

    uint8_t bflag = packet_to_uint8(data, len, &pos);

    while (pos < len) {
        tag_t tag = packet_to_uint32(data, len, &pos);
        uint8_t apply_action = CMD_APPLY_ACTION_NORMAL;

        object *tmp = NULL;
        if (tag != 0) {
            tmp = object_find(tag);
        } else {
            apply_action = packet_to_uint8(data, len, &pos);
        }

        if (tmp != NULL && tmp->env != env) {
            object_remove(tmp);
            tmp = NULL;
        }

        if (tmp == NULL || delete_env) {
            object *old_tmp = tmp;
            tmp = object_create(env, tag, bflag);
            tmp->apply_action = apply_action;

            if (old_tmp != NULL) {
                if (old_tmp == cpl.sack) {
                    cpl.sack = tmp;
                }

                object_transfer_inventory(old_tmp, tmp);
                object_remove(old_tmp);
            }
        }

        uint32_t flags = UPD_FLAGS | UPD_WEIGHT | UPD_FACE | UPD_DIRECTION |
                UPD_NAME | UPD_ANIM | UPD_ANIMSPEED | UPD_NROF | UPD_GLOW;

        if (loc > 0) {
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
 * Plays the footstep sounds when moving on the map.
 */
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
            msg_anim.tick = LastTick;
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
                map_set_data(x, y, packet_to_uint8(data, len, &pos), 0, 0, 0,
                        "", "", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, "", 0);
            } else { /* We have some data. */
                int16_t face, height = 0, zoom_x = 0, zoom_y = 0, align = 0, rotate = 0;
                uint8_t flags, obj_flags, quick_pos = 0, probe = 0, draw_double = 0, alpha = 0, infravision = 0, target_is_friend = 0;
                uint8_t anim_speed, anim_facing, anim_flags, anim_state, priority, secondpass, glow_speed;
                char player_name[64], player_color[COLOR_BUF], glow[COLOR_BUF];
                uint32_t target_object_count = 0;

                anim_speed = anim_facing = anim_flags = anim_state = 0;
                priority = secondpass = glow_speed = 0;

                player_name[0] = '\0';
                player_color[0] = '\0';
                glow[0] = '\0';

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
                    packet_to_string(data, len, &pos, VS(player_name));
                    packet_to_string(data, len, &pos, VS(player_color));
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

                    if (flags2 & MAP2_FLAG2_GLOW) {
                        packet_to_string(data, len, &pos, VS(glow));
                        glow_speed = packet_to_uint8(data, len, &pos);
                    }
                }

                /* Set the data we figured out. */
                map_set_data(x, y, type, face, quick_pos, obj_flags,
                        player_name, player_color, height, probe, zoom_x,
                        zoom_y, align, draw_double, alpha, rotate, infravision,
                        target_object_count, target_is_friend, anim_speed,
                        anim_facing, anim_flags, anim_state, priority,
                        secondpass, glow, glow_speed);
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

/**
 * Data associated with a single crypto warning popup.
 */
typedef struct socket_crypto_popup {
    socket_crypto_cb_ctx_t ctx; ///< Context from the socket crypto API.
    button_struct button_ok; ///< OK button.
    button_struct button_close; ///< Close button.
    uint32_t ticks; ///< When the popup appeared.
    int seconds; ///< Seconds since the popup appeared.
} socket_crypto_popup_t;

/**
 * Text used in the crypto warning popup.
 */
static const char *const socket_crypto_popup_texts[] = {
    "The selected server uses a self-signed certificate. This means that it's "
    "impossible to verify the authenticity of the server, and someone could be "
    "spying on your connection via a forged certificate.\n\n"
    "It is strongly recommended NOT to proceed (especially if you've connected "
    "to this server before and this warning did not appear then).\n\n"
    "Do you want to proceed (and remember this choice)?",
    "!!! THE PUBLIC KEY OF THE SERVER HAS CHANGED !!!\n\n"
    "This is most likely an indication of an MITM (Man In The Middle) attack. "
    "It is also possible the server's public key has just changed.\n\n"
    "It is STRONGLY RECOMMENDED [b]NOT[/b] to connect unless you have solid "
    "evidence that this is correct (eg, an announcement from the server "
    "officials about the change, ideally if this announcement was at least a "
    "month in advance).\n\n"
    "Do you want to proceed (and remember this choice)?",
};
CASSERT_ARRAY(socket_crypto_popup_texts, SOCKET_CRYPTO_CB_MAX);

/**
 * Number of seconds that must pass before the user can click 'OK' in
 * the crypto warning popup.
 */
static const int socket_crypto_popup_delays[] = {
    20, /* Self-signed certificate */
    60, /* Public key has changed */
};
CASSERT_ARRAY(socket_crypto_popup_delays, SOCKET_CRYPTO_CB_MAX);

/** @copydoc popup_struct::draw_func */
static int
popup_draw (popup_struct *popup)
{
    socket_crypto_popup_t *data = popup->custom_data;

    int delay = socket_crypto_popup_delays[data->ctx.id];
    if (data->seconds != delay) {
        uint32_t ticks_diff = SDL_GetTicks() - data->ticks;
        int seconds = ticks_diff / 1000;
        if (data->seconds != seconds) {
            data->seconds = seconds;
            popup->redraw = 1;

            if (seconds == delay) {
                data->button_ok.disabled = false;
            }
        }
    }

    if (!popup->redraw) {
        return 1;
    }

    popup->redraw = 0;
    surface_show(popup->surface, 0, 0, NULL, texture_surface(popup->texture));

    SDL_Rect box;
    box.w = popup->surface->w;
    box.h = 38;
    text_show(popup->surface,
              FONT_SERIF16,
              "Suspicious connection!",
              0,
              0,
              COLOR_HGOLD,
              TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER,
              &box);

    box.w = popup->surface->w - 10 * 2;
    box.h = popup->surface->h - box.h;
    text_show(popup->surface,
              FONT_ARIAL12,
              socket_crypto_popup_texts[data->ctx.id],
              10,
              50,
              COLOR_WHITE,
              TEXT_ALIGN_CENTER | TEXT_WORD_WRAP | TEXT_MARKUP,
              &box);

    if (data->ctx.fingerprint != NULL) {
        text_show(popup->surface,
                  FONT_ARIAL12,
                  "Certificate fingerprint:",
                  0,
                  90,
                  COLOR_WHITE,
                  TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER,
                  &box);
        text_show(popup->surface,
                  FONT_ARIAL12,
                  data->ctx.fingerprint,
                  0,
                  110,
                  COLOR_WHITE,
                  TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER,
                  &box);
    }

    char buf[MAX_BUF];
    snprintf(VS(buf), "%s", "OK");
    if (data->seconds != delay) {
        snprintfcat(VS(buf), " (%d)", delay - data->seconds);
    }

    data->button_ok.x = popup->surface->w / 4 -
                        data->button_ok.texture->surface->w;
    data->button_ok.y = popup->surface->h -
                        data->button_ok.texture->surface->h * 2;
    data->button_ok.surface = popup->surface;
    data->button_ok.px = popup->x;
    data->button_ok.py = popup->y;
    button_show(&data->button_ok, buf);

    data->button_close.x = popup->surface->w - popup->surface->w / 4;
    data->button_close.y = popup->surface->h -
                           data->button_close.texture->surface->h * 2;
    data->button_close.surface = popup->surface;
    data->button_close.px = popup->x;
    data->button_close.py = popup->y;
    button_show(&data->button_close, "Close");

    return 1;
}

/** @copydoc popup_struct::event_func */
static int
popup_event (popup_struct *popup, SDL_Event *event)
{
    socket_crypto_popup_t *data = popup->custom_data;

    if (button_event(&data->button_ok, event) ||
        (event->type == SDL_KEYDOWN &&
         (event->key.keysym.sym == SDLK_RETURN ||
          event->key.keysym.sym == SDLK_KP_ENTER))) {
        char *errmsg;
        if (!socket_crypto_handle_cb(&data->ctx, &errmsg)) {
            draw_info(COLOR_RED, errmsg);
            efree(errmsg);
        }

        popup_destroy(popup);
        login_start();
        return 1;
    } else if (button_event(&data->button_close, event)) {
        popup_destroy(popup);
        return 1;
    }

    if (data->button_ok.redraw || data->button_close.redraw) {
        popup->redraw = true;
    }

    return -1;
}

/** @copydoc popup_struct::destroy_callback_func */
static int
popup_destroy_callback (popup_struct *popup)
{
    if (cpl.state != ST_WAITLOOP) {
        return 0;
    }

    socket_crypto_popup_t *data = popup->custom_data;
    socket_crypto_free_cb(&data->ctx);
    efree(data);
    popup->custom_data = NULL;
    return 1;
}

/** @copydoc popup_struct::clipboard_copy_func */
static const char *
popup_clipboard_copy_func (popup_struct *popup)
{
    socket_crypto_popup_t *data = popup->custom_data;
    return data->ctx.fingerprint != NULL ? data->ctx.fingerprint : "";
}

/** @copydoc socket_crypto_cb_t */
static void
socket_crypto_cb (socket_crypto_t *crypto, const socket_crypto_cb_ctx_t *ctx)
{
    socket_crypto_popup_t *data = ecalloc(1, sizeof(*data));
    data->ctx = *ctx;
    data->ticks = SDL_GetTicks();

    button_create(&data->button_ok);
    data->button_ok.disabled = true;
    button_create(&data->button_close);

    popup_struct *popup = popup_create(texture_get(TEXTURE_TYPE_CLIENT,
                                                   "popup"));
    popup->draw_func = popup_draw;
    popup->event_func = popup_event;
    popup->destroy_callback_func = popup_destroy_callback;
    popup->clipboard_copy_func = popup_clipboard_copy_func;
    popup->disable_texture_drawing = 1;
    popup->custom_data = data;
}

/**
 * Aborts the connection due to a crypto error.
 */
static void
socket_command_crypto_abort (void)
{
    cpl.state = ST_START;
    draw_info_format(COLOR_RED,
                     "Failed to establish a secure connection with %s; see the "
                     "client log for details.",
                     selected_server->hostname);
}

/**
 * Handler for the crypto hello sub-command.
 *
 * @copydoc socket_command_struct::handle_func
 */
static void
socket_command_crypto_hello (uint8_t *data, size_t len, size_t pos)
{
    StringBuffer *sb = stringbuffer_new();
    packet_to_stringbuffer(data, len, &pos, sb);
    char *cert = stringbuffer_finish(sb);
    sb = stringbuffer_new();
    packet_to_stringbuffer(data, len, &pos, sb);
    char *chain = stringbuffer_finish(sb);

    if (*cert == '\0') {
        LOG(ERROR, "Received empty certificate from server");
        socket_command_crypto_abort();
        return;
    }

    socket_crypto_t *crypto = socket_crypto_create(csocket.sc);
    socket_crypto_set_cb(crypto, socket_crypto_cb);

    /* Meta-server servers must have a certificate public key record
     * associated with them.*/
    if (selected_server->is_meta) {
        const char *pubkey = METASERVER_GET_PUBKEY(selected_server);
        if (pubkey == NULL) {
            LOG(ERROR, " !!! Server has no public key record! !!!");
            socket_command_crypto_abort();
            return;
        }

        socket_crypto_load_pubkey(crypto, pubkey);
    }

    if (!socket_crypto_load_cert(crypto, cert, chain)) {
        efree(cert);
        efree(chain);
        socket_command_crypto_abort();
        return;
    }

    efree(cert);
    efree(chain);

    uint8_t key_len;
    const unsigned char *key = socket_crypto_create_key(crypto, &key_len);
    if (key == NULL) {
        socket_command_crypto_abort();
        return;
    }

    /* Send the generated key to the server. */
    packet_struct *packet = packet_new(SERVER_CMD_CRYPTO, 128, 128);
    packet_append_uint8(packet, CMD_CRYPTO_KEY);
    packet_append_uint8(packet, key_len);
    packet_append_data_len(packet, key, key_len);

    uint8_t iv_len;
    const unsigned char *iv = socket_crypto_get_iv(crypto, &iv_len);
    if (iv == NULL) {
        socket_command_crypto_abort();
        return;
    }

    packet_append_uint8(packet, iv_len);
    packet_append_data_len(packet, iv, iv_len);
    socket_send_packet(packet);

    cpl.state = ST_WAITCRYPTO;
}

/**
 * Handler for the crypto key sub-command.
 *
 * @copydoc socket_command_struct::handle_func
 */
static void
socket_command_crypto_key (uint8_t *data, size_t len, size_t pos)
{
    socket_crypto_t *crypto = socket_get_crypto(csocket.sc);
    SOFT_ASSERT(crypto != NULL, "crypto is NULL");

    if (len == pos) {
        LOG(PACKET, "Server sent malformed crypto key command: %s",
            socket_get_str(csocket.sc));
        socket_command_crypto_abort();
        return;
    }

    uint8_t iv_len;
    const unsigned char *iv = socket_crypto_get_iv(crypto, &iv_len);
    if (iv == NULL) {
        socket_command_crypto_abort();
        return;
    }

    uint8_t recv_iv_len = packet_to_uint8(data, len, &pos);
    recv_iv_len = MIN(recv_iv_len, len - pos);
    const unsigned char *recv_iv = data + pos;
    pos += recv_iv_len;

    if (len != pos) {
        LOG(PACKET, "Server sent malformed crypto key command: %s",
            socket_get_str(csocket.sc));
        socket_command_crypto_abort();
        return;
    }

    if (iv_len != recv_iv_len) {
        LOG(PACKET, "Server sent mismatching IV buffer length");
        socket_command_crypto_abort();
        return;
    }

    if (memcmp(iv, recv_iv, iv_len) != 0) {
        LOG(PACKET, "Server sent mismatching IV buffer");
        socket_command_crypto_abort();
        return;
    }

    packet_struct *packet = packet_new(SERVER_CMD_CRYPTO, 128, 128);
    packet_append_uint8(packet, CMD_CRYPTO_CURVES);
    socket_crypto_packet_append_curves(packet);
    socket_send_packet(packet);
}

/**
 * Handler for the crypto curves sub-command.
 *
 * @copydoc socket_command_struct::handle_func
 */
static void
socket_command_crypto_curves (uint8_t *data, size_t len, size_t pos)
{
    socket_crypto_t *crypto = socket_get_crypto(csocket.sc);
    SOFT_ASSERT(crypto != NULL, "crypto is NULL");

    char name[MAX_BUF];
    while (packet_to_string(data, len, &pos, VS(name)) != NULL) {
        int nid;
        if (socket_crypto_curve_supported(name, &nid)) {
            socket_crypto_set_nid(crypto, nid);

            uint8_t iv_size;
            const unsigned char *iv = socket_crypto_gen_iv(crypto, &iv_size);
            if (iv == NULL) {
                LOG(SYSTEM, "Failed to generate IV buffer: %s",
                    socket_get_str(csocket.sc));
                socket_command_crypto_abort();
                return;
            }

            size_t pubkey_len;
            unsigned char *pubkey = socket_crypto_gen_pubkey(crypto,
                                                             &pubkey_len);
            if (pubkey == NULL) {
                LOG(SYSTEM, "Failed to generate a public key: %s",
                    socket_get_str(csocket.sc));
                socket_command_crypto_abort();
                return;
            }

            if (pubkey_len > INT16_MAX) {
                LOG(SYSTEM, "Public key too long: %s",
                    socket_get_str(csocket.sc));
                socket_command_crypto_abort();
                efree(pubkey);
                return;
            }

            packet_struct *packet = packet_new(SERVER_CMD_CRYPTO, 512, 0);
            packet_append_uint8(packet, CMD_CRYPTO_PUBKEY);
            packet_append_uint16(packet, (uint16_t) pubkey_len);
            packet_append_data_len(packet, pubkey, pubkey_len);
            packet_append_uint8(packet, iv_size);
            packet_append_data_len(packet, iv, iv_size);
            socket_send_packet(packet);
            efree(pubkey);
            return;
        }
    }

    LOG(SYSTEM,
        "Server requested crypto but failed to provide a compatible "
        "crypto elliptic curve: %s",
        socket_get_str(csocket.sc));
    socket_command_crypto_abort();
}

/**
 * Handler for the crypto pubkey sub-command.
 *
 * @copydoc socket_command_struct::handle_func
 */
static void
socket_command_crypto_pubkey (uint8_t *data, size_t len, size_t pos)
{
    socket_crypto_t *crypto = socket_get_crypto(csocket.sc);
    SOFT_ASSERT(crypto != NULL, "crypto is NULL");

    if (len == pos) {
        LOG(PACKET, "Server sent malformed crypto pubkey command: %s",
            socket_get_str(csocket.sc));
        socket_command_crypto_abort();
        return;
    }

    uint16_t pubkey_len = packet_to_uint16(data, len, &pos);
    unsigned char *pubkey = data + pos;
    pubkey_len = MIN(pubkey_len, len - pos);
    pos += pubkey_len;
    uint8_t iv_len = packet_to_uint8(data, len, &pos);
    unsigned char *iv = data + pos;
    iv_len = MIN(iv_len, len - pos);
    pos += iv_len;

    if (!socket_crypto_derive(crypto, pubkey, pubkey_len, iv, iv_len)) {
        LOG(SYSTEM, "Couldn't derive shared secret key: %s",
            socket_get_str(csocket.sc));
        socket_command_crypto_abort();
        return;
    }

    if (len != pos) {
        LOG(PACKET, "Server sent malformed crypto pubkey command: %s",
            socket_get_str(csocket.sc));
        socket_command_crypto_abort();
        return;
    }

    uint8_t secret_len;
    const unsigned char *secret = socket_crypto_create_secret(crypto,
                                                              &secret_len);
    if (secret == NULL) {
        LOG(ERROR, "Failed to generate a secret");
        socket_command_crypto_abort();
        return;
    }

    packet_struct *packet = packet_new(SERVER_CMD_CRYPTO, 64, 0);
    packet_append_uint8(packet, CMD_CRYPTO_SECRET);
    packet_append_uint8(packet, secret_len);
    packet_append_data_len(packet, secret, secret_len);
    socket_send_packet(packet);
}

/**
 * Handler for the crypto secret sub-command.
 *
 * @copydoc socket_command_struct::handle_func
 */
static void
socket_command_crypto_secret (uint8_t *data, size_t len, size_t pos)
{
    socket_crypto_t *crypto = socket_get_crypto(csocket.sc);
    SOFT_ASSERT(crypto != NULL, "crypto is NULL");

    if (len == pos) {
        LOG(PACKET, "Server sent malformed crypto secret command: %s",
            socket_get_str(csocket.sc));
        socket_command_crypto_abort();
        return;
    }

    uint8_t secret_len = packet_to_uint8(data, len, &pos);
    secret_len = MIN(secret_len, len - pos);

    if (!socket_crypto_set_secret(crypto, data + pos, secret_len)) {
        LOG(PACKET, "Server sent malformed crypto secret command: %s",
            socket_get_str(csocket.sc));
        socket_command_crypto_abort();
        return;
    }

    pos += secret_len;

    if (len != pos) {
        LOG(PACKET, "Server sent malformed crypto secret command: %s",
            socket_get_str(csocket.sc));
        socket_command_crypto_abort();
        return;
    }

    packet_struct *packet = packet_new(SERVER_CMD_CRYPTO, 8, 0);
    packet_append_uint8(packet, CMD_CRYPTO_DONE);
    socket_send_packet(packet);
}

/**
 * Handler for the crypto done sub-command.
 *
 * @copydoc socket_command_struct::handle_func
 */
static void
socket_command_crypto_done (uint8_t *data, size_t len, size_t pos)
{
    socket_crypto_t *crypto = socket_get_crypto(csocket.sc);
    SOFT_ASSERT(crypto != NULL, "crypto is NULL");

    if (len != pos) {
        LOG(PACKET, "Server sent malformed crypto secret command: %s",
            socket_get_str(csocket.sc));
        socket_command_crypto_abort();
        return;
    }

    if (!socket_crypto_set_done(crypto)) {
        /* Logging already done */
        socket_command_crypto_abort();
        return;
    }

    /* Begin game data communications */
    cpl.state = ST_START_DATA;
    LOG(SYSTEM, "Connection: established a secure channel with %s",
        socket_get_str(csocket.sc));
}

/**
 * Handler for the crypto client command.
 *
 * @copydoc socket_command_struct::handle_func
 */
void
socket_command_crypto (uint8_t *data, size_t len, size_t pos)
{
    uint8_t type = packet_to_uint8(data, len, &pos);
    if (!socket_crypto_check_cmd(type, socket_get_crypto(csocket.sc))) {
        LOG(PACKET, "Received crypto command in invalid state: %u", type);
        return;
    }

    switch (type) {
    case CMD_CRYPTO_HELLO:
        socket_command_crypto_hello(data, len, pos);
        break;

    case CMD_CRYPTO_KEY:
        socket_command_crypto_key(data, len, pos);
        break;

    case CMD_CRYPTO_CURVES:
        socket_command_crypto_curves(data, len, pos);
        break;

    case CMD_CRYPTO_PUBKEY:
        socket_command_crypto_pubkey(data, len, pos);
        break;

    case CMD_CRYPTO_SECRET:
        socket_command_crypto_secret(data, len, pos);
        break;

    case CMD_CRYPTO_DONE:
        socket_command_crypto_done(data, len, pos);
        break;

    default:
        LOG(PACKET, "Received unknown security sub-command: %" PRIu8, type);
        break;
    }
}
