/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
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
#include <video.h>
#include <client_socket.h>
#include <openssl/crypto.h>
#include <packet_payload.h>
#include <region_map.h>
#include <wrapper.h>
#include <toolkit/map_protocol.h>
#include <toolkit/packet.h>
#include <toolkit/path.h>
#include <toolkit/string.h>

/** @copydoc socket_command_struct::handle_func */
void socket_command_book(uint8_t *data, size_t len, size_t pos) {
    sound_play_effect("book.ogg", 100);
    book_load((char *)data + pos, len);
}
/** @copydoc socket_command_struct::handle_func */
void socket_command_setup(uint8_t *data, size_t len, size_t pos) {
    packet_reader_t reader;
    packet_reader_init_cursor(&reader, data, len, &pos);
    uint8_t type;

    while (pos < len) {
        type = packet_reader_read_uint8(&reader);

        if (type == CMD_SETUP_SOUND) {
            packet_reader_read_uint8(&reader);
        } else if (type == CMD_SETUP_MAPSIZE) {
            int x, y;

            x = packet_reader_read_uint8(&reader);
            y = packet_reader_read_uint8(&reader);

            setting_set_int(OPT_CAT_MAP, OPT_MAP_WIDTH, MAP_WIRE_TO_LOOK_SIZE(x));
            setting_set_int(OPT_CAT_MAP, OPT_MAP_HEIGHT, MAP_WIRE_TO_LOOK_SIZE(y));
        } else if (type == CMD_SETUP_DATA_URL) {
            packet_reader_read_string(&reader, cpl.http_url, sizeof(cpl.http_url));
        } else if (type == CMD_SETUP_ASSET_TRANSPORT) {
            cpl.asset_transport = packet_reader_read_uint8(&reader) != 0;
        } else if (type == CMD_SETUP_CONNECTION_MODE) {
            packet_reader_read_uint8(&reader);
        } else if (type == CMD_SETUP_JOIN_PASSWORD) {
            if (packet_reader_read_uint8(&reader) == 0) {
                if (selected_server != NULL && selected_server->join_password != NULL) {
                    OPENSSL_cleanse(selected_server->join_password,
                                    strlen(selected_server->join_password));
                    free(selected_server->join_password);
                    selected_server->join_password = NULL;
                }
                if (clioption_settings.join_password != NULL) {
                    OPENSSL_cleanse(clioption_settings.join_password,
                                    strlen(clioption_settings.join_password));
                    free(clioption_settings.join_password);
                    clioption_settings.join_password = NULL;
                }
                draw_info(COLOR_RED, "The server rejected the join password.");
                cpl.state = ST_START;
                return;
            }
        }
    }

    if (cpl.state != ST_PLAY) {
        cpl.state = ST_REQUEST_FILES_LISTING;
    }
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_anim(uint8_t *data, size_t len, size_t pos) {
    packet_reader_t reader;
    packet_reader_init_cursor(&reader, data, len, &pos);
    if (pos > len || len - pos < 4) {
        LOG(ERROR, "Ignoring truncated animation packet");
        return;
    }

    uint16_t anim_id = packet_reader_read_uint16(&reader);
    uint8_t flags = packet_reader_read_uint8(&reader);
    uint8_t facings = packet_reader_read_uint8(&reader);

    if (anim_id >= animations_num || animations == NULL) {
        LOG(ERROR,
            "Ignoring invalid animation ID %u (count: %" PRIu64 ")",
            anim_id,
            (uint64_t)animations_num);
        return;
    }

    size_t num_animations = (len - pos) / 2;
    if ((len - pos) % 2 != 0 || num_animations == 0 || facings == 0 ||
        num_animations % facings != 0) {
        LOG(ERROR,
            "Ignoring malformed animation %u (%" PRIu64 " faces, %u facings)",
            anim_id,
            (uint64_t)num_animations,
            facings);
        return;
    }

    Animations *animation = &animations[anim_id];
    free(animation->faces);
    animation->faces = xmallocarray(num_animations, sizeof(*animation->faces));
    animation->flags = flags;
    animation->facings = facings;
    animation->num_animations = num_animations;
    animation->frame = num_animations / facings;

    for (size_t i = 0; i < num_animations; i++) {
        uint16_t face = packet_reader_read_uint16(&reader) & FACE_ID_MASK;
        if (!image_face_valid(face)) {
            LOG(ERROR, "Animation %u contains invalid face ID %u", anim_id, face);
            face = 0;
        }

        animation->faces[i] = face;
        image_request_face(face);
    }

    animation->loaded = 1;
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_image(uint8_t *data, size_t len, size_t pos) {
    uint32_t facenum;
    packet_view_t image;
    char buf[HUGE_BUF];

    if (!client_packet_parse_image(data, len, pos, &facenum, &image)) {
        return;
    }
    if (!image_face_valid(facenum) || image_get_face_name(facenum) == NULL) {
        LOG(ERROR, "Ignoring image packet with invalid face ID %" PRIu32, facenum);
        return;
    }

    /* Save picture to cache and load it to FaceList. */
    snprintf(buf, sizeof(buf), DIRECTORY_CACHE "/%s", image_get_face_name(facenum));
    char *path = file_path(buf, "wb");
    bool saved = path_write_atomic(path, image.data, image.len, 0600);
    free(path);
    if (!saved) {
        LOG(ERROR, "Could not atomically write image cache file '%s'.", buf);
        return;
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
void socket_command_drawinfo(uint8_t *data, size_t len, size_t pos) {
    packet_reader_t reader;
    packet_reader_init_cursor(&reader, data, len, &pos);
    uint8_t type;
    char color[COLOR_BUF], *str;
    StringBuffer *sb;

    type = packet_reader_read_uint8(&reader);
    packet_reader_read_string(&reader, color, sizeof(color));

    sb = stringbuffer_new();
    packet_reader_read_stringbuffer(&reader, sb);
    str = stringbuffer_finish(sb);

    draw_info_tab(type, color, str);

    free(str);
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_target(uint8_t *data, size_t len, size_t pos) {
    packet_reader_t reader;
    packet_reader_init_cursor(&reader, data, len, &pos);
    cpl.target_code = packet_reader_read_uint8(&reader);
    packet_reader_read_string(&reader, cpl.target_color, sizeof(cpl.target_color));
    packet_reader_read_string(&reader, cpl.target_name, sizeof(cpl.target_name));
    cpl.target_level = packet_reader_read_uint8(&reader);
    cpl.combat = packet_reader_read_uint8(&reader);
    cpl.combat_force = packet_reader_read_uint8(&reader);
    WIDGET_REDRAW_ALL(TARGET_ID);

    map_redraw_flag = 1;
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_stats(uint8_t *data, size_t len, size_t pos) {
    packet_reader_t reader;
    packet_reader_init_cursor(&reader, data, len, &pos);
    uint8_t type;
    int temp;

    while (pos < len) {
        type = packet_reader_read_uint8(&reader);

        if (type >= CS_STAT_EQUIP_START && type <= CS_STAT_EQUIP_END) {
            cpl.equipment[type - CS_STAT_EQUIP_START] = packet_reader_read_uint32(&reader);
            WIDGET_REDRAW_ALL(PDOLL_ID);
        } else if (type >= CS_STAT_PROT_START && type <= CS_STAT_PROT_END) {
            cpl.stats.protection[type - CS_STAT_PROT_START] = packet_reader_read_int8(&reader);
            WIDGET_REDRAW_ALL(PROTECTIONS_ID);
        } else {
            switch (type) {
                case CS_STAT_TARGET_HP:
                    cpl.target_hp = packet_reader_read_uint8(&reader);
                    WIDGET_REDRAW_ALL(TARGET_ID);
                    break;

                case CS_STAT_REG_HP:
                    cpl.gen_hp = packet_reader_read_uint16(&reader) / 10.0f;
                    widget_redraw_type_id(STAT_ID, "health");
                    break;

                case CS_STAT_REG_MANA:
                    cpl.gen_sp = packet_reader_read_uint16(&reader) / 10.0f;
                    widget_redraw_type_id(STAT_ID, "mana");
                    break;

                case CS_STAT_HP:
                    temp = packet_reader_read_int32(&reader);

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
                    cpl.stats.maxhp = packet_reader_read_int32(&reader);
                    widget_redraw_type_id(STAT_ID, "health");
                    break;

                case CS_STAT_SP:
                    cpl.stats.sp = packet_reader_read_int16(&reader);
                    widget_redraw_type_id(STAT_ID, "mana");
                    break;

                case CS_STAT_MAXSP:
                    cpl.stats.maxsp = packet_reader_read_int16(&reader);
                    widget_redraw_type_id(STAT_ID, "mana");
                    break;

                case CS_STAT_STR:
                case CS_STAT_INT:
                case CS_STAT_POW:
                case CS_STAT_DEX:
                case CS_STAT_CON: {
                    int8_t *stat_curr;
                    uint8_t stat_new;

                    stat_curr = &(cpl.stats.Str) + (sizeof(cpl.stats.Str) * (type - CS_STAT_STR));
                    stat_new = packet_reader_read_uint8(&reader);

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
                    cpl.path_attuned = packet_reader_read_uint32(&reader);
                    WIDGET_REDRAW_ALL(SPELLS_ID);
                    break;

                case CS_STAT_PATH_REPELLED:
                    cpl.path_repelled = packet_reader_read_uint32(&reader);
                    WIDGET_REDRAW_ALL(SPELLS_ID);
                    break;

                case CS_STAT_PATH_DENIED:
                    cpl.path_denied = packet_reader_read_uint32(&reader);
                    WIDGET_REDRAW_ALL(SPELLS_ID);
                    break;

                case CS_STAT_EXP:
                    cpl.stats.exp = packet_reader_read_uint64(&reader);
                    telemetry_exp_update(cpl.stats.exp);
                    widget_redraw_type_id(STAT_ID, "exp");
                    break;

                case CS_STAT_LEVEL:
                    cpl.stats.level = packet_reader_read_uint8(&reader);
                    WIDGET_REDRAW_ALL(PLAYER_INFO_ID);
                    break;

                case CS_STAT_WC:
                    cpl.stats.wc = packet_reader_read_uint16(&reader);
                    WIDGET_REDRAW_ALL(PDOLL_ID);
                    break;

                case CS_STAT_AC:
                    cpl.stats.ac = packet_reader_read_uint16(&reader);
                    WIDGET_REDRAW_ALL(PDOLL_ID);
                    break;

                case CS_STAT_DAM:
                    cpl.stats.dam = packet_reader_read_uint16(&reader);
                    WIDGET_REDRAW_ALL(PDOLL_ID);
                    break;

                case CS_STAT_SPEED:
                    cpl.stats.speed = packet_reader_read_double(&reader);
                    WIDGET_REDRAW_ALL(PDOLL_ID);
                    break;

                case CS_STAT_FOOD:
                    cpl.stats.food = packet_reader_read_uint16(&reader);
                    widget_redraw_type_id(STAT_ID, "food");
                    break;

                case CS_STAT_WEAPON_SPEED:
                    cpl.stats.weapon_speed = packet_reader_read_double(&reader);
                    WIDGET_REDRAW_ALL(PDOLL_ID);
                    break;

                case CS_STAT_FLAGS:
                    cpl.stats.flags = packet_reader_read_uint16(&reader);
                    break;

                case CS_STAT_WEIGHT_LIM:
                    cpl.weight_limit = packet_reader_read_uint32(&reader) / 1000.0;
                    break;

                case CS_STAT_ACTION_TIME:
                    cpl.action_timer = packet_reader_read_float(&reader);
                    WIDGET_REDRAW_ALL(PLAYER_INFO_ID);
                    break;

                case CS_STAT_GENDER:
                    cpl.gender = packet_reader_read_uint8(&reader);
                    WIDGET_REDRAW_ALL(PDOLL_ID);
                    break;

                case CS_STAT_RANGED_DAM:
                    cpl.stats.ranged_dam = packet_reader_read_uint16(&reader);
                    WIDGET_REDRAW_ALL(PDOLL_ID);
                    break;

                case CS_STAT_RANGED_WC:
                    cpl.stats.ranged_wc = packet_reader_read_uint16(&reader);
                    WIDGET_REDRAW_ALL(PDOLL_ID);
                    break;

                case CS_STAT_RANGED_WS:
                    cpl.stats.ranged_ws = packet_reader_read_float(&reader);
                    WIDGET_REDRAW_ALL(PDOLL_ID);
                    break;
            }
        }
    }
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_player(uint8_t *data, size_t len, size_t pos) {
    packet_reader_t reader;
    packet_reader_init_cursor(&reader, data, len, &pos);
    int tag, weight;

    tag = packet_reader_read_uint32(&reader);
    weight = packet_reader_read_uint32(&reader);
    uint32_t raw_face = packet_reader_read_uint32(&reader);
    uint16_t face = raw_face & FACE_ID_MASK;
    if (!image_face_valid(face)) {
        LOG(ERROR, "Player %d received invalid face ID %" PRIu32, tag, raw_face);
        face = 0;
    }

    image_request_face(face);
    packet_reader_read_string(&reader, cpl.name, sizeof(cpl.name));

    new_player(tag, weight, face);
    map_redraw_flag = 1;

    cur_widget[INPUT_ID]->show = 0;

    if (cur_widget[PARTY_ID]->show) {
        send_command("/party list");
    }

    cpl.state = ST_PLAY;
}

void command_item_update(packet_reader_t *reader, uint32_t flags, object *tmp) {
    bool force_anim = false;

    if (flags & UPD_LOCATION) {
        /* Currently unused. */
        packet_reader_read_uint32(reader);
    }

    if (flags & UPD_FLAGS) {
        tmp->flags = packet_reader_read_uint32(reader);
    }

    if (flags & UPD_WEIGHT) {
        tmp->weight = packet_reader_read_uint32(reader) / 1000.0;
    }

    if (flags & UPD_FACE) {
        uint16_t raw_face = packet_reader_read_uint16(reader);
        uint16_t face = raw_face & FACE_ID_MASK;
        if (!image_face_valid(face)) {
            LOG(ERROR,
                "Object %" PRIu32 " received invalid face ID %u "
                "(animation: %u, direction: %u)",
                tmp->tag,
                raw_face,
                tmp->animation_id,
                tmp->direction);
            face = 0;
        }

        tmp->face = face;
        image_request_face(face);
    }

    if (flags & UPD_DIRECTION) {
        tmp->direction = packet_reader_read_uint8(reader);
    }

    if (flags & UPD_TYPE) {
        tmp->itype = packet_reader_read_uint8(reader);
        tmp->stype = packet_reader_read_uint8(reader);
        tmp->item_qua = packet_reader_read_uint8(reader);

        if (tmp->item_qua != 255) {
            tmp->item_con = packet_reader_read_uint8(reader);
            tmp->item_level = packet_reader_read_uint8(reader);
            tmp->item_skill_tag = packet_reader_read_uint32(reader);
        }
    }

    if (flags & UPD_NAME) {
        packet_reader_read_string(reader, tmp->s_name, sizeof(tmp->s_name));
    }

    if (flags & UPD_ANIM) {
        uint16_t animation_id = packet_reader_read_uint16(reader);

        if (animation_id >= animations_num) {
            LOG(ERROR,
                "Object %" PRIu32 " received invalid animation ID %u "
                "(face: %u, direction: %u)",
                tmp->tag,
                animation_id,
                tmp->face,
                tmp->direction);
            animation_id = 0;
        }

        /* Changing animation ID, force animation. */
        if (tmp->animation_id != animation_id) {
            force_anim = true;
            tmp->anim_state = 0;
        }

        tmp->animation_id = animation_id;
    }

    if (flags & UPD_ANIMSPEED) {
        uint8_t anim_speed;

        anim_speed = packet_reader_read_uint8(reader);

        /* Animation was disabled and we're enabling it, force animation. */
        if (tmp->anim_speed == 0 && anim_speed != 0) {
            force_anim = true;
        }

        tmp->anim_speed = anim_speed;
    }

    if (flags & UPD_NROF) {
        tmp->nrof = packet_reader_read_uint32(reader);

        if (tmp->nrof == 0) {
            tmp->nrof = 1;
        }
    }

    if (flags & UPD_EXTRA) {
        if (tmp->itype == TYPE_SPELL) {
            uint16_t spell_cost;
            uint32_t spell_path, spell_flags;
            char spell_msg[MAX_BUF];

            spell_cost = packet_reader_read_uint16(reader);
            spell_path = packet_reader_read_uint32(reader);
            spell_flags = packet_reader_read_uint32(reader);
            packet_reader_read_string(reader, spell_msg, sizeof(spell_msg));

            spells_update(tmp, spell_cost, spell_path, spell_flags, spell_msg);
        } else if (tmp->itype == TYPE_SKILL) {
            uint8_t skill_level = packet_reader_read_uint8(reader);
            int64_t skill_exp = packet_reader_read_int64(reader);
            char skill_msg[MAX_BUF];
            packet_reader_read_string(reader, VS(skill_msg));

            skills_update(tmp, skill_level, skill_exp, skill_msg);
        } else if (tmp->itype == TYPE_FORCE || tmp->itype == TYPE_POISONING) {
            int32_t sec;
            char msg[HUGE_BUF];

            sec = packet_reader_read_int32(reader);
            packet_reader_read_string(reader, msg, sizeof(msg));

            widget_active_effects_update(cur_widget[ACTIVE_EFFECTS_ID], tmp, sec, msg);
        }
    }

    if (flags & UPD_GLOW) {
        packet_reader_read_string(reader, VS(tmp->glow));
        tmp->glow_speed = packet_reader_read_uint8(reader);
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
void socket_command_item(uint8_t *data, size_t len, size_t pos) {
    packet_reader_t reader;
    packet_reader_init_cursor(&reader, data, len, &pos);
    bool delete_env = packet_reader_read_uint8(&reader) == 1;
    if (delete_env) {
        tag_t loc_delete = packet_reader_read_uint32(&reader);
        object *env = object_find(loc_delete);
        if (env == NULL) {
            return;
        }

        object_remove_inventory(env);

        if (pos == len) {
            return;
        }
    }

    tag_t loc = packet_reader_read_uint32(&reader);
    object *env = object_find(loc);
    if (env == NULL) {
        LOG(ERROR, "Server sent invalid location: %" PRIu32, loc);
        return;
    }

    if (env != cpl.below && env != cpl.ob) {
        cpl.sack = env;
    }

    uint8_t bflag = packet_reader_read_uint8(&reader);

    while (pos < len) {
        tag_t tag = packet_reader_read_uint32(&reader);
        uint8_t apply_action = CMD_APPLY_ACTION_NORMAL;

        object *tmp = NULL;
        if (tag != 0) {
            tmp = object_find(tag);
        } else {
            apply_action = packet_reader_read_uint8(&reader);
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

        uint32_t flags = UPD_FLAGS | UPD_WEIGHT | UPD_FACE | UPD_DIRECTION | UPD_NAME | UPD_ANIM |
                         UPD_ANIMSPEED | UPD_NROF | UPD_GLOW;

        if (loc > 0) {
            flags |= UPD_TYPE | UPD_EXTRA;
        }

        command_item_update(&reader, flags, tmp);
    }
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_item_update(uint8_t *data, size_t len, size_t pos) {
    packet_reader_t reader;
    packet_reader_init_cursor(&reader, data, len, &pos);
    uint32_t flags, tag;
    object *tmp;

    flags = packet_reader_read_uint16(&reader);
    tag = packet_reader_read_uint32(&reader);

    tmp = object_find(tag);

    if (!tmp) {
        return;
    }

    command_item_update(&reader, flags, tmp);
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_item_delete(uint8_t *data, size_t len, size_t pos) {
    packet_reader_t reader;
    packet_reader_init_cursor(&reader, data, len, &pos);
    tag_t tag;

    while (pos < len) {
        tag = packet_reader_read_uint32(&reader);
        delete_object(tag);
    }
}

/**
 * Plays the footstep sounds when moving on the map.
 */
static void map_play_footstep(void) {
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
void socket_command_mapstats(uint8_t *data, size_t len, size_t pos) {
    packet_reader_t reader;
    packet_reader_init_cursor(&reader, data, len, &pos);
    uint8_t type;
    char buf[HUGE_BUF];

    while (pos < len) {
        /* Get the type of this command... */
        type = packet_reader_read_uint8(&reader);

        if (type == CMD_MAPSTATS_NAME) {
            /* Change map name. */
            packet_reader_read_string(&reader, buf, sizeof(buf));
            update_map_name(buf);
        } else if (type == CMD_MAPSTATS_MUSIC) {
            /* Change map music. */
            packet_reader_read_string(&reader, buf, sizeof(buf));
            update_map_bg_music(buf);
        } else if (type == CMD_MAPSTATS_WEATHER) {
            /* Change map weather. */
            packet_reader_read_string(&reader, buf, sizeof(buf));
            update_map_weather(buf);
        } else if (type == CMD_MAPSTATS_TEXT_ANIM) {
            packet_reader_read_string(&reader, msg_anim.color, sizeof(msg_anim.color));
            packet_reader_read_string(&reader, msg_anim.message, sizeof(msg_anim.message));
            msg_anim.tick = LastTick;
        } else if (type == CMD_MAPSTATS_TIME) {
            uint64_t game_seconds = packet_reader_read_uint64(&reader);
            uint32_t millis_per_game_minute = packet_reader_read_uint32(&reader);
            telemetry_game_time_sync(game_seconds, millis_per_game_minute);
            WIDGET_REDRAW_ALL(GAME_TIME_ID);
        }
    }
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_map(uint8_t *data, size_t len, size_t pos) {
    packet_reader_t reader;
    packet_reader_init_cursor(&reader, data, len, &pos);
    static int mx = 0, my = 0;
    int mask, x, y, rx, ry;
    int mapstat;
    int xpos, ypos;
    int layer, ext_flags;
    uint8_t num_layers;
    region_map_def_map_t *def_map;
    bool region_map_fow_need_update;

    if (!map_protocol_validate(
            data,
            len,
            pos,
            MAP_LOOK_TO_WIRE_SIZE(setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH)),
            MAP_LOOK_TO_WIRE_SIZE(setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT)))) {
        LOG(PACKET, "Rejected malformed map packet.");
        return;
    }

    mapstat = packet_reader_read_uint8(&reader);

    if (mapstat != MAP_UPDATE_CMD_SAME) {
        char mapname[HUGE_BUF], bg_music[HUGE_BUF], weather[MAX_BUF], region_name[MAX_BUF],
            region_longname[MAX_BUF], mappath[HUGE_BUF];
        uint8_t height_diff;

        packet_reader_read_string(&reader, mapname, sizeof(mapname));
        packet_reader_read_string(&reader, bg_music, sizeof(bg_music));
        packet_reader_read_string(&reader, weather, sizeof(weather));
        height_diff = packet_reader_read_uint8(&reader);
        MapData.region_has_map = packet_reader_read_uint8(&reader);
        packet_reader_read_string(&reader, VS(region_name));
        packet_reader_read_string(&reader, VS(region_longname));
        packet_reader_read_string(&reader, VS(mappath));

        if (mapstat == MAP_UPDATE_CMD_NEW) {
            int map_w, map_h;

            map_w = packet_reader_read_uint8(&reader);
            map_h = packet_reader_read_uint8(&reader);
            xpos = packet_reader_read_uint8(&reader);
            ypos = packet_reader_read_uint8(&reader);
            mx = xpos;
            my = ypos;
            init_map_data(map_w, map_h, xpos, ypos);
        } else {
            int xoff, yoff, zoff;

            packet_reader_read_uint8(&reader);
            xoff = packet_reader_read_int8(&reader);
            yoff = packet_reader_read_int8(&reader);
            zoff = packet_reader_read_int8(&reader);
            xpos = packet_reader_read_uint8(&reader);
            ypos = packet_reader_read_uint8(&reader);
            mx = xpos;
            my = ypos;
            display_mapscroll(xoff, yoff, 0, 0);
            map_level_scroll(zoff);

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
        xpos = packet_reader_read_uint8(&reader);
        ypos = packet_reader_read_uint8(&reader);

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
    MapData.player_sub_layer = packet_reader_read_uint8(&reader);
    def_map = region_map_find_map(MapData.region_map, MapData.map_path);

    map_get_real_coords(&rx, &ry);
    region_map_fow_need_update = false;

    if (pos >= len) {
        LOG(PACKET, "Map packet has no level count.");
        return;
    }

    uint8_t level_count = packet_reader_read_uint8(&reader);
    if (level_count > MAP2_LEVELS) {
        LOG(PACKET, "Map packet contains too many levels: %" PRIu8 ".", level_count);
        return;
    }

    uint16_t level_mask = 0;
    size_t packet_end = len;

    for (uint8_t level_num = 0; level_num < level_count; level_num++) {
        if (len - pos < sizeof(int8_t) + sizeof(uint32_t)) {
            LOG(PACKET, "Truncated map level header.");
            return;
        }

        int depth = packet_reader_read_int8(&reader);
        uint32_t level_size = packet_reader_read_uint32(&reader);
        if (depth < -MAP2_MAX_DEPTH || depth > MAP2_MAX_DEPTH || level_size > len - pos) {
            LOG(PACKET, "Invalid map level depth or payload size.");
            return;
        }

        size_t level_end = pos + level_size;
        len = level_end;
        if (!map_select_level(depth, true)) {
            LOG(PACKET, "Could not select map level %d.", depth);
            return;
        }
        uint16_t level_bit = UINT16_C(1) << MAP2_DEPTH_INDEX(depth);
        if (level_mask & level_bit) {
            LOG(PACKET, "Map packet contains duplicate depth %d.", depth);
            return;
        }
        level_mask |= level_bit;

        while (pos < level_end) {
            if (len - pos < sizeof(uint16_t)) {
                LOG(PACKET, "Truncated map tile mask.");
                return;
            }

            mask = packet_reader_read_uint16(&reader);
            x = (mask >> 11) & 0x1f;
            y = (mask >> 6) & 0x1f;

            /* Clear the whole cell? */
            if (mask & MAP2_MASK_CLEAR) {
                map_clear_cell(x, y, (mask & MAP2_MASK_HARD_CLEAR) != 0);
                continue;
            }

            size_t tile_values = 0;
            if (mask & MAP2_MASK_SUPPORT_HEIGHT) {
                tile_values += sizeof(int16_t);
            }
            if (mask & MAP2_MASK_FOW) {
                tile_values++;
            }
            if (mask & MAP2_MASK_LIGHT_LEVEL) {
                tile_values++;
            }
            if (mask & MAP2_MASK_LIGHT_LEVEL_MORE) {
                tile_values += NUM_SUB_LAYERS - 1;
            }
            if (len - pos < tile_values + sizeof(num_layers)) {
                LOG(PACKET, "Truncated map tile metadata.");
                return;
            }

            if (mask & MAP2_MASK_SUPPORT_HEIGHT) {
                map_set_structural_support_height(x, y, packet_reader_read_int16(&reader));
            }

            bool fow_updated = (mask & MAP2_MASK_FOW) != 0;
            bool tile_fow =
                fow_updated ? packet_reader_read_uint8(&reader) != 0 : map_get_fow(x, y);

            /* Do we have light-level information? */
            if (mask & MAP2_MASK_LIGHT_LEVEL) {
                map_set_light_level(x, y, 0, packet_reader_read_uint8(&reader));
            }

            if (mask & MAP2_MASK_LIGHT_LEVEL_MORE) {
                int sub_layer;

                for (sub_layer = 1; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
                    map_set_light_level(x, y, sub_layer, packet_reader_read_uint8(&reader));
                }
            }

            num_layers = packet_reader_read_uint8(&reader);

            /* Go through all the layers on this tile. */
            for (layer = 0; layer < num_layers; layer++) {
                uint8_t type;

                type = packet_reader_read_uint8(&reader);

                /* Clear this layer. */
                if (type == MAP2_LAYER_CLEAR) {
                    map_set_data(x,
                                 y,
                                 packet_reader_read_uint8(&reader),
                                 0,
                                 0,
                                 0,
                                 "",
                                 "",
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 "",
                                 0);
                } else { /* We have some data. */
                    int16_t face, height = 0, zoom_x = 0, zoom_y = 0, align = 0, rotate = 0;
                    uint8_t flags, obj_flags, quick_pos = 0, probe = 0, draw_double = 0, alpha = 0,
                                              infravision = 0, target_is_friend = 0;
                    uint8_t anim_speed, anim_facing, anim_flags, anim_state, priority, secondpass,
                        roof, door, glow_speed;
                    char player_name[64], player_color[COLOR_BUF], glow[COLOR_BUF];
                    uint32_t target_object_count = 0;

                    anim_speed = anim_facing = anim_flags = anim_state = 0;
                    priority = secondpass = roof = door = glow_speed = 0;

                    player_name[0] = '\0';
                    player_color[0] = '\0';
                    glow[0] = '\0';

                    face = packet_reader_read_uint16(&reader);
                    /* Object flags. */
                    obj_flags = packet_reader_read_uint8(&reader);
                    /* Flags of this layer. */
                    flags = packet_reader_read_uint8(&reader);

                    /* Multi-arch? */
                    if (flags & MAP2_FLAG_MULTI) {
                        quick_pos = packet_reader_read_uint8(&reader);
                    }

                    /* Player name? */
                    if (flags & MAP2_FLAG_NAME) {
                        packet_reader_read_string(&reader, VS(player_name));
                        packet_reader_read_string(&reader, VS(player_color));
                    }

                    /* Animation? */
                    if (flags & MAP2_FLAG_ANIMATION) {
                        anim_speed = packet_reader_read_uint8(&reader);
                        anim_facing = packet_reader_read_uint8(&reader);
                        anim_flags = packet_reader_read_uint8(&reader);

                        if (anim_flags & ANIM_FLAG_MOVING) {
                            anim_state = packet_reader_read_uint8(&reader);
                        }
                    }

                    /* Z position? */
                    if (flags & MAP2_FLAG_HEIGHT) {
                        height = packet_reader_read_int16(&reader);
                    }

                    /* Align? */
                    if (flags & MAP2_FLAG_ALIGN) {
                        align = packet_reader_read_int16(&reader);
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

                        flags2 = packet_reader_read_uint32(&reader);

                        if (flags2 & MAP2_FLAG2_ALPHA) {
                            alpha = packet_reader_read_uint8(&reader);
                        }

                        if (flags2 & MAP2_FLAG2_ROTATE) {
                            rotate = packet_reader_read_int16(&reader);
                        }

                        /* Zoom? */
                        if (flags2 & MAP2_FLAG2_ZOOM) {
                            zoom_x = packet_reader_read_uint16(&reader);
                            zoom_y = packet_reader_read_uint16(&reader);
                        }

                        if (flags2 & MAP2_FLAG2_TARGET) {
                            target_object_count = packet_reader_read_uint32(&reader);
                            target_is_friend = packet_reader_read_uint8(&reader);
                        }

                        /* Target's HP? */
                        if (flags2 & MAP2_FLAG2_PROBE) {
                            probe = packet_reader_read_uint8(&reader);
                        }

                        if (flags2 & MAP2_FLAG2_PRIORITY) {
                            priority = 1;
                        }

                        if (flags2 & MAP2_FLAG2_SECONDPASS) {
                            secondpass = 1;
                        }

                        if (flags2 & MAP2_FLAG2_GLOW) {
                            packet_reader_read_string(&reader, VS(glow));
                            glow_speed = packet_reader_read_uint8(&reader);
                        }

                        if (flags2 & MAP2_FLAG2_ROOF) {
                            roof = 1;
                        }

                        if (flags2 & MAP2_FLAG2_DOOR) {
                            door = 1;
                        }
                    }

                    /* Set the data we figured out. */
                    map_set_data(x,
                                 y,
                                 type,
                                 face,
                                 quick_pos,
                                 obj_flags,
                                 player_name,
                                 player_color,
                                 height,
                                 probe,
                                 zoom_x,
                                 zoom_y,
                                 align,
                                 draw_double,
                                 alpha,
                                 rotate,
                                 infravision,
                                 target_object_count,
                                 target_is_friend,
                                 anim_speed,
                                 anim_facing,
                                 anim_flags,
                                 anim_state,
                                 priority,
                                 secondpass,
                                 roof,
                                 door,
                                 glow,
                                 glow_speed);
                }
            }

            /* Get tile flags. */
            ext_flags = packet_reader_read_uint8(&reader);

            /* Animation? */
            if (ext_flags & MAP2_FLAG_EXT_ANIM) {
                uint8_t anim_num = packet_reader_read_uint8(&reader);

                for (uint8_t i = 0; i < anim_num; i++) {
                    uint8_t sub_layer = packet_reader_read_uint8(&reader);
                    uint8_t anim_type = packet_reader_read_uint8(&reader);
                    int16_t anim_value = packet_reader_read_int16(&reader);

                    map_anims_add(anim_type, x, y, sub_layer, depth, anim_value);
                }
            }

            if (fow_updated) {
                map_set_fow(x, y, tile_fow);
            }

            if (depth == 0 && !tile_fow && MapData.region_name[0] != '\0') {
                if (region_map_fow_set_visited(MapData.region_map,
                                               def_map,
                                               MapData.map_path,
                                               rx + x,
                                               ry + y)) {
                    region_map_fow_need_update = true;
                }
            }
        }

        if (pos != level_end) {
            LOG(PACKET, "Map level payload was not consumed exactly.");
            return;
        }

        len = packet_end;
    }

    if (pos != packet_end) {
        LOG(PACKET, "Map packet has trailing data after its level blocks.");
        return;
    }

    map_set_level_mask(level_mask);

    for (int depth = -MAP2_MAX_DEPTH; depth <= MAP2_MAX_DEPTH; depth++) {
        if ((level_mask & (UINT16_C(1) << MAP2_DEPTH_INDEX(depth))) &&
            map_select_level(depth, false)) {
            adjust_tile_stretch();
        }
    }
    map_select_level(0, true);
    map_redraw_flag = minimap_redraw_flag = 1;

    if (region_map_fow_need_update) {
        region_map_fow_update(MapData.region_map);
    }
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_version(uint8_t *data, size_t len, size_t pos) {
    packet_reader_t reader;
    packet_reader_init_cursor(&reader, data, len, &pos);
    if (cpl.state != ST_WAITVERSION) {
        LOG(BUG,
            "Received version command when not in proper "
            "state: %d, should be: %d.",
            cpl.state,
            ST_WAITVERSION);
        return;
    }

    cpl.server_socket_version = packet_reader_read_uint32(&reader);
    if (cpl.server_socket_version != SOCKET_VERSION) {
        draw_info(COLOR_RED, "The client and server use incompatible gameplay protocol versions.");
        cpl.state = ST_START;
        return;
    }

    cpl.state = ST_VERSION;
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_compressed(uint8_t *data, size_t len, size_t pos) {
    packet_reader_t reader;
    packet_reader_init_cursor(&reader, data, len, &pos);
    uint32_t declared_len;
    uint8_t type, *dest;
    size_t dest_size;

    type = packet_reader_read_uint8(&reader);
    declared_len = packet_reader_read_uint32(&reader);
    packet_view_t compressed = packet_reader_read_view(&reader, packet_reader_remaining(&reader));
    if (packet_reader_error(&reader) != PACKET_ERROR_NONE ||
        declared_len > PACKET_PAYLOAD_MAX - 1 || type >= CLIENT_CMD_NROF ||
        type == CLIENT_CMD_REGION_MAP) {
        packet_reader_set_error(&reader,
                                declared_len > PACKET_PAYLOAD_MAX - 1 ? PACKET_ERROR_LIMIT_EXCEEDED
                                                                      : PACKET_ERROR_UNSUPPORTED);
        return;
    }

    dest_size = (size_t)declared_len + 1;
    dest = xmalloc(dest_size);
    dest[0] = type;

    uLongf actual_len = declared_len;
    if (uncompress((Bytef *)dest + 1, &actual_len, compressed.data, compressed.len) == Z_OK &&
        actual_len == declared_len) {
        command_buffer *buf;

        buf = command_buffer_new((size_t)actual_len + 1, dest);
        add_input_command(buf);
    } else {
        packet_reader_set_error(&reader, PACKET_ERROR_INVALID_ENCODING);
    }

    free(dest);
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_control(uint8_t *data, size_t len, size_t pos) {
    packet_reader_t reader;
    packet_reader_init_cursor(&reader, data, len, &pos);
    char app_name[MAX_BUF];
    uint8_t type, sub_type;

    packet_reader_read_string(&reader, app_name, sizeof(app_name));
    type = packet_reader_read_uint8(&reader);
    sub_type = packet_reader_read_uint8(&reader);

    if (type == CMD_CONTROL_PLAYER && sub_type == CMD_CONTROL_PLAYER_TELEPORT) {
        SDL_RaiseWindow(ScreenWindow);
    }
}
