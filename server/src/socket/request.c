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
 * This file implements all of the goo on the server side for handling
 * clients.  It's got a bunch of global variables for keeping track of
 * each of the clients.
 *
 * @note All functions that are used to process data from the client
 * have the prototype of (char *data, int datalen, int client_num).  This
 * way, we can use one dispatch table. */

#include <global.h>

#define GET_CLIENT_FLAGS(_O_)   ((_O_)->flags[0] & 0x7f)
#define NO_FACE_SEND (-1)

void socket_command_setup(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos)
{
    packet_struct *packet;
    uint8 type;

    packet = packet_new(CLIENT_CMD_SETUP, 256, 256);

    while (pos < len) {
        type = packet_to_uint8(data, len, &pos);
        packet_append_uint8(packet, type);

        if (type == CMD_SETUP_SOUND) {
            ns->sound = packet_to_uint8(data, len, &pos);
            packet_append_uint8(packet, ns->sound);
        } else if (type == CMD_SETUP_MAPSIZE) {
            int x, y;

            x = packet_to_uint8(data, len, &pos);
            y = packet_to_uint8(data, len, &pos);

            if (x < 9 || y < 9 || x > MAP_CLIENT_X || y > MAP_CLIENT_Y) {
                x = MAP_CLIENT_X;
                y = MAP_CLIENT_Y;
            }

            ns->mapx = x;
            ns->mapy = y;
            ns->mapx_2 = x / 2;
            ns->mapy_2 = y / 2;

            packet_append_uint8(packet, x);
            packet_append_uint8(packet, y);
        } else if (type == CMD_SETUP_BOT) {
            ns->is_bot = packet_to_uint8(data, len, &pos);

            if (ns->is_bot != 0 && ns->is_bot != 1) {
                ns->is_bot = 0;
            }

            packet_append_uint8(packet, ns->is_bot);
        } else if (type == CMD_SETUP_DATA_URL) {
            char url[MAX_BUF];

            packet_to_string(data, len, &pos, url, sizeof(url));

            if (!string_isempty(url)) {
                packet_append_string_terminated(packet, url);
            } else {
                packet_append_string_terminated(packet, settings.http_url);
            }
        }
    }

    socket_send_packet(ns, packet);
}

void socket_command_player_cmd(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos)
{
    char command[MAX_BUF];

    if (pl->socket.state != ST_PLAYING) {
        return;
    }

    packet_to_string(data, len, &pos, command, sizeof(command));
    commands_handle(pl->ob, command);
}

void socket_command_version(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos)
{
    uint32 ver;
    packet_struct *packet;

    /* Ignore multiple version commands. */
    if (ns->socket_version != 0) {
        return;
    }

    ver = packet_to_uint32(data, len, &pos);

    if (ver == 0 || ver == 991017 || ver == 1055) {
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Your client is "
                "outdated!\nGo to http://www.atrinik.org/ and download the latest "
                "Atrinik client.");
        ns->state = ST_ZOMBIE;
        return;
    }

    ns->socket_version = ver;

    packet = packet_new(CLIENT_CMD_VERSION, 4, 4);
    packet_append_uint32(packet, SOCKET_VERSION);
    socket_send_packet(ns, packet);
}

void socket_command_item_move(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos)
{
    tag_t to, tag;
    uint32 nrof;

    to = packet_to_uint32(data, len, &pos);
    tag = packet_to_uint32(data, len, &pos);
    nrof = packet_to_uint32(data, len, &pos);

    if (!tag) {
        return;
    }

    esrv_move_object(pl->ob, to, tag, nrof);
}

#define AddIfInt(_old, _new, _type, _bitsize) \
    if ((_old) != (_new)) \
    { \
        (_old) = (_new); \
        packet_append_uint8(packet, (_type)); \
        packet_append_ ## _bitsize(packet, (_new)); \
    }

#define AddIfFloat(_old, _new, _type) \
    if ((_old) != (_new)) \
    { \
        (_old) = (_new); \
        packet_append_uint8(packet, (_type)); \
        packet_append_uint32(packet, (_new) * FLOAT_MULTI); \
    }

/**
 * Sends player statistics update.
 *
 * We look at the old values, and only send what has changed.
 *
 * Stat mapping values are in socket.h */
void esrv_update_stats(player *pl)
{
    packet_struct *packet;
    int i;
    uint16 flags;

    packet = packet_new(CLIENT_CMD_STATS, 32, 256);

    if (pl->target_object && pl->target_object != pl->ob) {
        char hp;

        hp = MAX(1, (((float) pl->target_object->stats.hp / (float) pl->target_object->stats.maxhp) * 100.0f));

        AddIfInt(pl->target_hp, hp, CS_STAT_TARGET_HP, uint8);
    }

    AddIfInt(pl->last_gen_hp, pl->gen_client_hp, CS_STAT_REG_HP, uint16);
    AddIfInt(pl->last_gen_sp, pl->gen_client_sp, CS_STAT_REG_MANA, uint16);
    AddIfInt(pl->last_action_timer, pl->action_timer, CS_STAT_ACTION_TIME, uint32);

    if (pl->ob) {
        object *arrow;

        AddIfInt(pl->last_level, pl->ob->level, CS_STAT_LEVEL, uint8);
        AddIfFloat(pl->last_speed, pl->ob->speed, CS_STAT_SPEED);
        AddIfFloat(pl->last_weapon_speed, pl->ob->weapon_speed / MAX_TICKS, CS_STAT_WEAPON_SPEED);
        AddIfInt(pl->last_weight_limit, weight_limit[pl->ob->stats.Str], CS_STAT_WEIGHT_LIM, uint32);
        AddIfInt(pl->last_stats.hp, pl->ob->stats.hp, CS_STAT_HP, sint32);
        AddIfInt(pl->last_stats.maxhp, pl->ob->stats.maxhp, CS_STAT_MAXHP, sint32);
        AddIfInt(pl->last_stats.sp, pl->ob->stats.sp, CS_STAT_SP, sint16);
        AddIfInt(pl->last_stats.maxsp, pl->ob->stats.maxsp, CS_STAT_MAXSP, sint16);
        AddIfInt(pl->last_stats.Str, pl->ob->stats.Str, CS_STAT_STR, uint8);
        AddIfInt(pl->last_stats.Dex, pl->ob->stats.Dex, CS_STAT_DEX, uint8);
        AddIfInt(pl->last_stats.Con, pl->ob->stats.Con, CS_STAT_CON, uint8);
        AddIfInt(pl->last_stats.Int, pl->ob->stats.Int, CS_STAT_INT, uint8);
        AddIfInt(pl->last_stats.Pow, pl->ob->stats.Pow, CS_STAT_POW, uint8);
        AddIfInt(pl->last_stats.exp, pl->ob->stats.exp, CS_STAT_EXP, uint64);
        AddIfInt(pl->last_stats.wc, pl->ob->stats.wc, CS_STAT_WC, uint16);
        AddIfInt(pl->last_stats.ac, pl->ob->stats.ac, CS_STAT_AC, uint16);
        AddIfInt(pl->last_stats.dam, pl->ob->stats.dam, CS_STAT_DAM, uint16);
        AddIfInt(pl->last_stats.food, pl->ob->stats.food, CS_STAT_FOOD, uint16);
        AddIfInt(pl->last_path_attuned, pl->ob->path_attuned, CS_STAT_PATH_ATTUNED, uint32);
        AddIfInt(pl->last_path_repelled, pl->ob->path_repelled, CS_STAT_PATH_REPELLED, uint32);
        AddIfInt(pl->last_path_denied, pl->ob->path_denied, CS_STAT_PATH_DENIED, uint32);

        if (pl->equipment[PLAYER_EQUIP_WEAPON_RANGED] && pl->equipment[PLAYER_EQUIP_WEAPON_RANGED]->type == BOW && (arrow = arrow_find(pl->ob, pl->equipment[PLAYER_EQUIP_WEAPON_RANGED]->race))) {
            AddIfInt(pl->last_ranged_dam, arrow_get_damage(pl->ob, pl->equipment[PLAYER_EQUIP_WEAPON_RANGED], arrow), CS_STAT_RANGED_DAM, uint16);
            AddIfInt(pl->last_ranged_wc, arrow_get_wc(pl->ob, pl->equipment[PLAYER_EQUIP_WEAPON_RANGED], arrow), CS_STAT_RANGED_WC, uint16);
            AddIfInt(pl->last_ranged_ws, bow_get_ws(pl->equipment[PLAYER_EQUIP_WEAPON_RANGED], arrow), CS_STAT_RANGED_WS, uint32);
        } else {
            AddIfInt(pl->last_ranged_dam, 0, CS_STAT_RANGED_DAM, uint16);
            AddIfInt(pl->last_ranged_wc, 0, CS_STAT_RANGED_WC, uint16);
            AddIfInt(pl->last_ranged_ws, 0, CS_STAT_RANGED_WS, uint32);
        }
    }

    flags = 0;

    if (pl->run_on) {
        flags |= SF_RUNON;
    }

    /* we add additional player status flags - in old style, you got a msg
     * in the text windows when you get xray of get blinded - we will skip
     * this and add the info here, so the client can track it down and make
     * it the user visible in it own, server independent way. */

    /* player is blind */
    if (QUERY_FLAG(pl->ob, FLAG_BLIND)) {
        flags |= SF_BLIND;
    }

    /* player has xray */
    if (QUERY_FLAG(pl->ob, FLAG_XRAYS)) {
        flags |= SF_XRAYS;
    }

    /* player has infravision */
    if (QUERY_FLAG(pl->ob, FLAG_SEE_IN_DARK)) {
        flags |= SF_INFRAVISION;
    }

    AddIfInt(pl->last_flags, flags, CS_STAT_FLAGS, uint16);

    for (i = 0; i < NROFATTACKS; i++) {
        /* If there are more attacks, but we reached CS_STAT_PROT_END,
         * we stop now. */
        if (CS_STAT_PROT_START + i > CS_STAT_PROT_END) {
            break;
        }

        AddIfInt(pl->last_protection[i], pl->ob->protection[i],
                CS_STAT_PROT_START + i, sint8);
    }

    for (i = 0; i < PLAYER_EQUIP_MAX; i++) {
        AddIfInt(pl->last_equipment[i], pl->equipment[i] ? pl->equipment[i]->count : 0, CS_STAT_EQUIP_START + i, uint32);
    }

    if (pl->socket.ext_title_flag) {
        generate_quick_name(pl);
        pl->socket.ext_title_flag = 0;
    }

    AddIfInt(pl->last_gender, object_get_gender(pl->ob), CS_STAT_GENDER, uint8);

    if (packet->len >= 1) {
        socket_send_packet(&pl->socket, packet);
    } else {
        packet_free(packet);
    }
}

/**
 * Tells the client that here is a player it should start using. */
void esrv_new_player(player *pl, uint32 weight)
{
    packet_struct *packet;

    packet = packet_new(CLIENT_CMD_PLAYER, 12, 0);
    packet_append_uint32(packet, pl->ob->count);
    packet_append_uint32(packet, weight);
    packet_append_uint32(packet, pl->ob->face->number);
    packet_append_string_terminated(packet, pl->ob->name);
    socket_send_packet(&pl->socket, packet);
}

/**
 * Get ID of a tiled map by checking player::last_update.
 * @param pl Player.
 * @param map Tiled map.
 * @return ID of the tiled map, 0 if there is no match. */
static inline int get_tiled_map_id(player *pl, struct mapdef *map)
{
    int i;

    if (!pl->last_update) {
        return 0;
    }

    for (i = 0; i < TILED_NUM; i++) {
        if (pl->last_update->tile_path[i] == map->path) {
            return i + 1;
        }
    }

    return 0;
}

/**
 * Copy socket's last map according to new coordinates.
 * @param ns Socket.
 * @param dx X.
 * @param dy Y. */
static inline void copy_lastmap(socket_struct *ns, int dx, int dy)
{
    struct Map newmap;
    int x, y;

    for (x = 0; x < ns->mapx; x++) {
        for (y = 0; y < ns->mapy; y++) {
            if (x + dx < 0 || x + dx >= ns->mapx || y + dy < 0 || y + dy >= ns->mapy) {
                memset(&(newmap.cells[x][y]), 0, sizeof(MapCell));
                continue;
            }

            memcpy(&(newmap.cells[x][y]), &(ns->lastmap.cells[x + dx][y + dy]), sizeof(MapCell));
        }
    }

    memcpy(&(ns->lastmap), &newmap, sizeof(struct Map));
}

void draw_map_text_anim(object *pl, const char *color, const char *text)
{
    packet_struct *packet;

    packet = packet_new(CLIENT_CMD_MAPSTATS, 64, 64);
    packet_append_uint8(packet, CMD_MAPSTATS_TEXT_ANIM);
    packet_append_string_terminated(packet, color);
    packet_append_string_terminated(packet, text);
    socket_send_packet(&CONTR(pl)->socket, packet);
}

/**
 * Do some checks, map name and LOS and then draw the map.
 * @param pl Whom to draw map for. */
void draw_client_map(object *pl)
{
    int redraw_below = 0;

    if (pl->type != PLAYER) {
        logger_print(LOG(BUG), "Called with non-player: %s", pl->name);
        return;
    }

    /* IF player is just joining the game, he isn't on a map,
     * If so, don't try to send them a map.  All will
     * be OK once they really log in. */
    if (!pl->map || pl->map->in_memory != MAP_IN_MEMORY) {
        return;
    }

    CONTR(pl)->map_update_cmd = MAP_UPDATE_CMD_SAME;

    /* If we changed somewhere the map, prepare map data */
    if (CONTR(pl)->last_update != pl->map) {
        int tile_map = get_tiled_map_id(CONTR(pl), pl->map);

        /* Are we on a new map? */
        if (!CONTR(pl)->last_update || !tile_map) {
            CONTR(pl)->map_update_cmd = MAP_UPDATE_CMD_NEW;
            memset(&(CONTR(pl)->socket.lastmap), 0, sizeof(struct Map));
            CONTR(pl)->last_update = pl->map;
            redraw_below = 1;
        } else {
            CONTR(pl)->map_update_cmd = MAP_UPDATE_CMD_CONNECTED;
            CONTR(pl)->map_update_tile = tile_map;
            redraw_below = 1;

            /* We have moved to a tiled map. Let's calculate the offsets. */
            switch (tile_map - 1) {
            case 0:
                CONTR(pl)->map_off_x = pl->x - CONTR(pl)->map_tile_x;
                CONTR(pl)->map_off_y = -(CONTR(pl)->map_tile_y + (MAP_HEIGHT(pl->map) - pl->y));
                break;

            case 1:
                CONTR(pl)->map_off_y = pl->y - CONTR(pl)->map_tile_y;
                CONTR(pl)->map_off_x = (MAP_WIDTH(pl->map) - CONTR(pl)->map_tile_x) + pl->x;
                break;

            case 2:
                CONTR(pl)->map_off_x = pl->x - CONTR(pl)->map_tile_x;
                CONTR(pl)->map_off_y = (MAP_HEIGHT(pl->map) - CONTR(pl)->map_tile_y) + pl->y;
                break;

            case 3:
                CONTR(pl)->map_off_y = pl->y - CONTR(pl)->map_tile_y;
                CONTR(pl)->map_off_x = -(CONTR(pl)->map_tile_x + (MAP_WIDTH(pl->map) - pl->x));
                break;

            case 4:
                CONTR(pl)->map_off_y = -(CONTR(pl)->map_tile_y + (MAP_HEIGHT(pl->map) - pl->y));
                CONTR(pl)->map_off_x = (MAP_WIDTH(pl->map) - CONTR(pl)->map_tile_x) + pl->x;
                break;

            case 5:
                CONTR(pl)->map_off_x = (MAP_WIDTH(pl->map) - CONTR(pl)->map_tile_x) + pl->x;
                CONTR(pl)->map_off_y = (MAP_HEIGHT(pl->map) - CONTR(pl)->map_tile_y) + pl->y;
                break;

            case 6:
                CONTR(pl)->map_off_y = (MAP_HEIGHT(pl->map) - CONTR(pl)->map_tile_y) + pl->y;
                CONTR(pl)->map_off_x = -(CONTR(pl)->map_tile_x + (MAP_WIDTH(pl->map) - pl->x));
                break;

            case 7:
                CONTR(pl)->map_off_x = -(CONTR(pl)->map_tile_x + (MAP_WIDTH(pl->map) - pl->x));
                CONTR(pl)->map_off_y = -(CONTR(pl)->map_tile_y + (MAP_HEIGHT(pl->map) - pl->y));
                break;
            }

            copy_lastmap(&CONTR(pl)->socket, CONTR(pl)->map_off_x, CONTR(pl)->map_off_y);
            CONTR(pl)->last_update = pl->map;
        }
    } else {
        if (CONTR(pl)->map_tile_x != pl->x || CONTR(pl)->map_tile_y != pl->y) {
            copy_lastmap(&CONTR(pl)->socket, pl->x - CONTR(pl)->map_tile_x, pl->y - CONTR(pl)->map_tile_y);
            redraw_below = 1;
        }
    }

    /* Redraw below window and backbuffer new positions? */
    if (redraw_below) {
        /* Backbuffer position so we can determine whether we have moved or not
         * */
        CONTR(pl)->map_tile_x = pl->x;
        CONTR(pl)->map_tile_y = pl->y;
        CONTR(pl)->socket.below_clear = 1;
        /* Redraw it */
        CONTR(pl)->socket.update_tile = 0;
        CONTR(pl)->socket.look_position = 0;
    }

    /* Do LOS after calls to update_position */
    if (CONTR(pl)->update_los) {
        update_los(pl);
        CONTR(pl)->update_los = 0;
    }

    draw_client_map2(pl);

    /* If we moved on the same map, check for map name/music to update. */
    if (redraw_below && CONTR(pl)->map_update_cmd == MAP_UPDATE_CMD_SAME) {
        MapSpace *msp;
        packet_struct *packet;
        object *map_info;

        msp = GET_MAP_SPACE_PTR(pl->map, pl->x, pl->y);
        map_info = msp->map_info && OBJECT_VALID(msp->map_info, msp->map_info_count) ? msp->map_info : NULL;

        packet = packet_new(CLIENT_CMD_MAPSTATS, 256, 256);

        if ((map_info && map_info->race && strcmp(map_info->race, CONTR(pl)->map_info_name) != 0) || (!map_info && CONTR(pl)->map_info_name[0] != '\0')) {
            packet_append_uint8(packet, CMD_MAPSTATS_NAME);
            packet_append_map_name(packet, pl, map_info);

            if (map_info) {
                strncpy(CONTR(pl)->map_info_name, map_info->race, sizeof(CONTR(pl)->map_info_name) - 1);
                CONTR(pl)->map_info_name[sizeof(CONTR(pl)->map_info_name) - 1] = '\0';
            } else {
                CONTR(pl)->map_info_name[0] = '\0';
            }
        }

        if ((map_info && map_info->slaying && strcmp(map_info->slaying, CONTR(pl)->map_info_music) != 0) || (!map_info && CONTR(pl)->map_info_music[0] != '\0')) {
            packet_append_uint8(packet, CMD_MAPSTATS_MUSIC);
            packet_append_map_music(packet, pl, map_info);

            if (map_info) {
                strncpy(CONTR(pl)->map_info_music, map_info->slaying, sizeof(CONTR(pl)->map_info_music) - 1);
                CONTR(pl)->map_info_music[sizeof(CONTR(pl)->map_info_music) - 1] = '\0';
            } else {
                CONTR(pl)->map_info_music[0] = '\0';
            }
        }

        if ((map_info && map_info->title && strcmp(map_info->title, CONTR(pl)->map_info_weather) != 0) || (!map_info && CONTR(pl)->map_info_weather[0] != '\0')) {
            packet_append_uint8(packet, CMD_MAPSTATS_WEATHER);
            packet_append_map_weather(packet, pl, map_info);

            if (map_info) {
                strncpy(CONTR(pl)->map_info_weather, map_info->title, sizeof(CONTR(pl)->map_info_weather) - 1);
                CONTR(pl)->map_info_weather[sizeof(CONTR(pl)->map_info_weather) - 1] = '\0';
            } else {
                CONTR(pl)->map_info_weather[0] = '\0';
            }
        }

        /* Anything to send? */
        if (packet->len >= 1) {
            socket_send_packet(&CONTR(pl)->socket, packet);
        } else {
            packet_free(packet);
        }
    }
}

/**
 * Figure out player name color for the client to show, in HTML notation.
 *
 * As you can see in this function, it is easy to add new player name
 * colors, just check for the match and make it return the correct color.
 * @param pl Player object that will get the map data sent to.
 * @param op Player object on the map, to get the name from.
 * @return The color. */
static const char *get_playername_color(object *pl, object *op)
{
    if (CONTR(pl)->party != NULL && CONTR(op)->party != NULL && CONTR(pl)->party == CONTR(op)->party) {
        return COLOR_GREEN;
    } else if (pl != op && pvp_area(pl, op)) {
        return COLOR_RED;
    }

    return COLOR_WHITE;
}

void packet_append_map_name(packet_struct *packet, object *op, object *map_info)
{
    packet_append_string(packet, "[b][o=#000000]");
    packet_append_string(packet, map_info && map_info->race ? map_info->race : op->map->name);
    packet_append_string_terminated(packet, "[/o][/b]");
}

void packet_append_map_music(packet_struct *packet, object *op, object *map_info)
{
    packet_append_string_terminated(packet, map_info && map_info->slaying ? map_info->slaying : (op->map->bg_music ? op->map->bg_music : "no_music"));
}

void packet_append_map_weather(packet_struct *packet, object *op, object *map_info)
{
    packet_append_string_terminated(packet, map_info && map_info->title ? map_info->title : (op->map->weather ? op->map->weather : "none"));
}

/** Clear a map cell. */
#define map_clearcell(_cell_) \
    { \
        memset((void *) ((char *) (_cell_) + offsetof(MapCell, count)), 0, sizeof(MapCell) - offsetof(MapCell, count)); \
        (_cell_)->count = -1; \
    }

/** Clear a map cell, but only if it has not been cleared before. */
#define map_if_clearcell() \
    { \
        if (CONTR(pl)->socket.lastmap.cells[ax][ay].count != -1) \
        { \
            packet_append_uint16(packet, mask | MAP2_MASK_CLEAR); \
            map_clearcell(&CONTR(pl)->socket.lastmap.cells[ax][ay]); \
        } \
    }

/** Draw the client map. */
void draw_client_map2(object *pl)
{
    static uint32 map2_count = 0;
    MapCell *mp;
    MapSpace *msp;
    mapstruct *m;
    int x, y, ax, ay, d, nx, ny;
    int x_start;
    int special_vision;
    uint16 mask;
    int layer, dark;
    int anim_value, anim_type, ext_flags;
    int num_layers;
    object *mirror = NULL;
    uint8 have_sound_ambient;
    packet_struct *packet, *packet_layer, *packet_sound;
    size_t oldpos;

    /* Any kind of special vision? */
    special_vision = (QUERY_FLAG(pl, FLAG_XRAYS) ? 1 : 0) | (QUERY_FLAG(pl, FLAG_SEE_IN_DARK) ? 2 : 0);
    map2_count++;

    packet = packet_new(CLIENT_CMD_MAP, 0, 512);
    packet_sound = packet_new(CLIENT_CMD_SOUND_AMBIENT, 0, 256);

    packet_enable_ndelay(packet);
    packet_append_uint8(packet, CONTR(pl)->map_update_cmd);

    if (CONTR(pl)->map_update_cmd != MAP_UPDATE_CMD_SAME) {
        object *map_info;

        msp = GET_MAP_SPACE_PTR(pl->map, pl->x, pl->y);
        map_info = msp->map_info && OBJECT_VALID(msp->map_info, msp->map_info_count) ? msp->map_info : NULL;

        packet_append_map_name(packet, pl, map_info);
        packet_append_map_music(packet, pl, map_info);
        packet_append_map_weather(packet, pl, map_info);
        packet_append_uint8(packet, MAP_HEIGHT_DIFF(pl->map) ? 1 : 0);

        if (map_info) {
            if (map_info->race) {
                strncpy(CONTR(pl)->map_info_name, map_info->race, sizeof(CONTR(pl)->map_info_name) - 1);
                CONTR(pl)->map_info_name[sizeof(CONTR(pl)->map_info_name) - 1] = '\0';
            }

            if (map_info->slaying) {
                strncpy(CONTR(pl)->map_info_music, map_info->slaying, sizeof(CONTR(pl)->map_info_music) - 1);
                CONTR(pl)->map_info_music[sizeof(CONTR(pl)->map_info_music) - 1] = '\0';
            }

            if (map_info->title) {
                strncpy(CONTR(pl)->map_info_weather, map_info->title, sizeof(CONTR(pl)->map_info_weather) - 1);
                CONTR(pl)->map_info_weather[sizeof(CONTR(pl)->map_info_weather) - 1] = '\0';
            }
        }

        if (CONTR(pl)->map_update_cmd == MAP_UPDATE_CMD_CONNECTED) {
            packet_append_uint8(packet, CONTR(pl)->map_update_tile);
            packet_append_sint8(packet, CONTR(pl)->map_off_x);
            packet_append_sint8(packet, CONTR(pl)->map_off_y);
        } else {
            packet_append_uint8(packet, pl->map->width);
            packet_append_uint8(packet, pl->map->height);
        }
    }

    packet_append_uint8(packet, pl->x);
    packet_append_uint8(packet, pl->y);

    x_start = (pl->x + (CONTR(pl)->socket.mapx + 1) / 2) - 1;

    for (ay = CONTR(pl)->socket.mapy - 1, y = (pl->y + (CONTR(pl)->socket.mapy + 1) / 2) - 1; y >= pl->y - CONTR(pl)->socket.mapy_2; y--, ay--) {
        ax = CONTR(pl)->socket.mapx - 1;

        for (x = x_start; x >= pl->x - CONTR(pl)->socket.mapx_2; x--, ax--) {
            d = CONTR(pl)->blocked_los[ax][ay];
            /* Form the data packet for x and y positions. */
            mask = (ax & 0x1f) << 11 | (ay & 0x1f) << 6;
            mp = &(CONTR(pl)->socket.lastmap.cells[ax][ay]);

            /* Space is out of map or blocked. Update space and clear values if
             * needed. */
            if (d & BLOCKED_LOS_OUT_OF_MAP) {
                map_if_clearcell();
                continue;
            }

            nx = x;
            ny = y;

            if (!(m = get_map_from_coord(pl->map, &nx, &ny))) {
                map_if_clearcell();
                continue;
            }

            msp = GET_MAP_SPACE_PTR(m, nx, ny);
            /* Check whether there is ambient sound effect on this tile. */
            have_sound_ambient = msp->sound_ambient && OBJECT_VALID(msp->sound_ambient, msp->sound_ambient_count);

            /* If there is an ambient sound effect but it cannot be heard
             * through walls due to its configuration, we will pretend
             * there is no sound effect here. */
            if (have_sound_ambient && !QUERY_FLAG(msp->sound_ambient, FLAG_XRAYS) && d & BLOCKED_LOS_BLOCKED) {
                have_sound_ambient = 0;
            }

            /* If there is an ambient sound effect and we haven't sent it
             * before, or there isn't one but it was sent before, send an
             * update. */
            if ((have_sound_ambient && mp->sound_ambient_count != msp->sound_ambient->count) || (!have_sound_ambient && mp->sound_ambient_count)) {
                packet_append_uint8(packet_sound, ax);
                packet_append_uint8(packet_sound, ay);
                packet_append_uint32(packet_sound, mp->sound_ambient_count);

                if (have_sound_ambient) {
                    packet_append_uint32(packet_sound, msp->sound_ambient->count);
                    packet_append_string_terminated(packet_sound, msp->sound_ambient->race);
                    packet_append_uint8(packet_sound, msp->sound_ambient->item_condition);
                    packet_append_uint8(packet_sound, msp->sound_ambient->item_level);

                    mp->sound_ambient_count = msp->sound_ambient->count;
                } else {
                    packet_append_uint32(packet_sound, 0);

                    mp->sound_ambient_count = 0;
                }
            }

            if (d & BLOCKED_LOS_BLOCKED) {
                map_if_clearcell();
                continue;
            }

            /* Border tile, we can ignore every LOS change */
            if (!(d & BLOCKED_LOS_IGNORE)) {
                /* Tile has blocksview set? */
                if (msp->flags & P_BLOCKSVIEW) {
                    if (!d) {
                        CONTR(pl)->update_los = 1;
                    }
                } else {
                    if (d & BLOCKED_LOS_BLOCKSVIEW) {
                        CONTR(pl)->update_los = 1;
                    }
                }
            }

            d = map_get_darkness(m, nx, ny, &mirror);

            if (CONTR(pl)->tli) {
                d += global_darkness_table[MAX_DARKNESS];
            }

            /* Tile is not normally visible */
            if (d <= 0) {
                /* Xray or infravision? */
                if (special_vision & 1 || (special_vision & 2 && msp->flags & (P_IS_PLAYER | P_IS_MONSTER))) {
                    d = 100;
                } else {
                    map_if_clearcell();
                    continue;
                }
            }

            if (d > 640) {
                d = 210;
            } else if (d > 320) {
                d = 180;
            } else if (d > 160) {
                d = 150;
            } else if (d > 80) {
                d = 120;
            } else if (d > 40) {
                d = 90;
            } else if (d > 20) {
                d = 60;
            } else {
                d = 30;
            }

            /* Initialize default values for some variables. */
            dark = NO_FACE_SEND;
            ext_flags = 0;
            oldpos = packet_get_pos(packet);
            anim_type = 0;
            anim_value = 0;

            /* Do we need to send the darkness? */
            if (mp->count != d) {
                mask |= MAP2_MASK_DARKNESS;
                dark = d;
                mp->count = d;
            }

            /* Add the mask. Any mask changes should go above this line. */
            packet_append_uint16(packet, mask);

            /* If we have darkness to send, send it. */
            if (dark != NO_FACE_SEND) {
                packet_append_uint8(packet, dark);
            }

            packet_layer = packet_new(0, 0, 128);
            num_layers = 0;

            /* Go through the visible layers. */
            for (layer = LAYER_FLOOR; layer <= NUM_LAYERS; layer++) {
                int sub_layer, socket_layer;

                for (sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
                    object *tmp = GET_MAP_SPACE_LAYER(msp, layer, sub_layer);

                    /* Double check that we can actually see this object. */
                    if (tmp && QUERY_FLAG(tmp, FLAG_HIDDEN)) {
                        tmp = NULL;
                    }

                    /* This is done so that the player image is always shown
                     * to the player, even if they are standing on top of
                     * another
                     * player or monster. */
                    if (tmp && tmp->layer == pl->layer && tmp->sub_layer == pl->sub_layer && pl->x == nx && pl->y == ny) {
                        tmp = pl;
                    }

                    /* Still nothing, but there's a magic mirror on this tile?
                     * */
                    if (!tmp && mirror) {
                        magic_mirror_struct *m_data = MMIRROR(mirror);
                        mapstruct *mirror_map;

                        if (m_data && (mirror_map = magic_mirror_get_map(mirror)) && !OUT_OF_MAP(mirror_map, m_data->x, m_data->y)) {
                            tmp = GET_MAP_SPACE_LAYER(GET_MAP_SPACE_PTR(mirror_map, m_data->x, m_data->y), layer, sub_layer);
                        }
                    }

                    /* Handle objects that are shown based on their direction
                     * and the player's position. */
                    if (tmp && QUERY_FLAG(tmp, FLAG_DRAW_DIRECTION)) {
                        /* If the object is dir [0124568] and not in the top
                         * or right quadrant or on the central square, do not
                         * show it. */
                        if ((!tmp->direction || tmp->direction == NORTH || tmp->direction == NORTHEAST || tmp->direction == SOUTHEAST || tmp->direction == SOUTH || tmp->direction == SOUTHWEST || tmp->direction == NORTHWEST) && !((ax <= CONTR(pl)->socket.mapx_2) && (ay <= CONTR(pl)->socket.mapy_2)) && !((ax > CONTR(pl)->socket.mapx_2) && (ay < CONTR(pl)->socket.mapy_2))) {
                            tmp = NULL;
                        } else if ((!tmp->direction || tmp->direction == NORTHEAST || tmp->direction == EAST || tmp->direction == SOUTHEAST || tmp->direction == SOUTHWEST || tmp->direction == WEST || tmp->direction == NORTHWEST) && !((ax <= CONTR(pl)->socket.mapx_2) && (ay <= CONTR(pl)->socket.mapy_2)) && !((ax < CONTR(pl)->socket.mapx_2) && (ay > CONTR(pl)->socket.mapy_2))) {
                            /* If the object is dir [0234768] and not in the top
                             * or left quadrant or on the central square, do not
                             * show it. */
                            tmp = NULL;
                        }
                    }

                    socket_layer = NUM_LAYERS * sub_layer + layer - 1;

                    /* Found something. */
                    if (tmp) {
                        sint16 face;
                        uint8 quick_pos = tmp->quick_pos;
                        uint8 flags = 0, probe_val = 0;
                        uint32 flags2 = 0;
                        object *head = tmp->head ? tmp->head : tmp, *face_obj;
                        tag_t target_object_count = 0;
                        uint8 anim_speed, anim_facing, anim_flags;

                        face_obj = NULL;
                        anim_speed = anim_facing = anim_flags = 0;

                        /* If we have a multi-arch object. */
                        if (quick_pos) {
                            flags |= MAP2_FLAG_MULTI;

                            /* Tail? */
                            if (tmp->head) {
                                /* If true, we have sent a part of this in this
                                 * map
                                 * update before, so skip it. */
                                if (head->update_tag == map2_count) {
                                    face = 0;
                                } else {
                                    /* Mark this object as sent. */
                                    head->update_tag = map2_count;
                                    face_obj = head;
                                }
                            } else {
                                /* Head. */

                                if (tmp->update_tag == map2_count) {
                                    face = 0;
                                } else {
                                    tmp->update_tag = map2_count;
                                    face_obj = tmp;
                                }
                            }
                        } else {
                            face_obj = tmp;
                        }

                        if (face_obj != NULL) {
                            if (QUERY_FLAG(face_obj, FLAG_ANIMATE)) {
                                flags |= MAP2_FLAG_ANIMATION;
                                face = face_obj->animation_id;
                                anim_speed = face_obj->anim_speed;
                                anim_facing = face_obj->direction + 1;
                                anim_flags = face_obj->anim_flags & ~ANIM_FLAG_STOP_MOVING;
                            } else {
                                face = face_obj->face->number;
                            }
                        }

                        /* Player? So we want to send their name. */
                        if (tmp->type == PLAYER) {
                            flags |= MAP2_FLAG_NAME;
                        }

                        /* If our player has this object as their target, we
                         * want to
                         * know its HP percent. */
                        if (head->count == CONTR(pl)->target_object_count) {
                            flags2 |= MAP2_FLAG2_PROBE;
                            probe_val = MAX(1, ((double) head->stats.hp / ((double) head->stats.maxhp / 100.0)));
                        }

                        /* Z position set? */
                        if (head->z) {
                            flags |= MAP2_FLAG_HEIGHT;
                        }

                        /* Check if the object has zoom, or check if the magic
                         * mirror
                         * should affect the zoom value of this layer. */
                        if ((head->zoom_x && head->zoom_x != 100) || (head->zoom_y && head->zoom_y != 100) || (mirror && mirror->last_heal && mirror->last_heal != 100 && mirror->path_attuned & (1U << (layer - 1)))) {
                            flags |= MAP2_FLAG_ZOOM;
                        }

                        if (head->align || (mirror && mirror->align)) {
                            flags |= MAP2_FLAG_ALIGN;
                        }

                        /* Draw the object twice if set, but only if it's not
                         * in the bottom quadrant of the map. */
                        if ((QUERY_FLAG(tmp, FLAG_DRAW_DOUBLE) && (ax < CONTR(pl)->socket.mapx_2 || ay < CONTR(pl)->socket.mapy_2)) || QUERY_FLAG(tmp, FLAG_DRAW_DOUBLE_ALWAYS)) {
                            flags |= MAP2_FLAG_DOUBLE;
                        }

                        if (head->alpha) {
                            flags2 |= MAP2_FLAG2_ALPHA;
                        }

                        if (head->rotate) {
                            flags2 |= MAP2_FLAG2_ROTATE;
                        }

                        if (QUERY_FLAG(pl, FLAG_SEE_IN_DARK) && ((head->layer == LAYER_LIVING && d < 150) || (head->type == CONTAINER && (head->sub_type & 1) == ST1_CONTAINER_CORPSE && QUERY_FLAG(head, FLAG_IS_USED_UP) && (float) head->stats.food / head->last_eat >= CORPSE_INFRAVISION_PERCENT / 100.0))) {
                            flags2 |= MAP2_FLAG2_INFRAVISION;
                        }

                        if (head != pl && layer == LAYER_LIVING && IS_LIVE(head)) {
                            flags2 |= MAP2_FLAG2_TARGET;
                            target_object_count = head->count;
                        }

                        if (flags2) {
                            flags |= MAP2_FLAG_MORE;
                        }

                        /* Damage animation? Store it for later. */
                        if (tmp->last_damage && tmp->damage_round_tag == global_round_tag) {
                            ext_flags |= MAP2_FLAG_EXT_ANIM;
                            anim_type = ANIM_DAMAGE;
                            anim_value = tmp->last_damage;
                        }

                        /* Now, check if we have cached this. */
                        if (mp->faces[socket_layer] == face && mp->quick_pos[socket_layer] == quick_pos && mp->flags[socket_layer] == flags && (layer != LAYER_LIVING || !IS_LIVE(head) || (mp->probe == probe_val && mp->target_object_count == target_object_count)) && mp->anim_speed[socket_layer] == anim_speed && mp->anim_facing[socket_layer] == anim_facing && (layer != LAYER_LIVING || mp->anim_flags[sub_layer] == anim_flags)) {
                            continue;
                        }

                        /* Different from cache, add it to the cache now. */
                        mp->faces[socket_layer] = face;
                        mp->quick_pos[socket_layer] = quick_pos;
                        mp->flags[socket_layer] = flags;
                        mp->anim_speed[socket_layer] = anim_speed;
                        mp->anim_facing[socket_layer] = anim_facing;

                        if (layer == LAYER_LIVING) {
                            mp->anim_flags[sub_layer] = anim_flags;
                        }

                        if (layer == LAYER_LIVING) {
                            mp->probe = probe_val;
                            mp->target_object_count = target_object_count;
                        }

                        if (OBJECT_IS_HIDDEN(pl, head)) {
                            /* Update target if applicable. */
                            if (flags2 & MAP2_FLAG2_PROBE) {
                                CONTR(pl)->target_object = NULL;
                                CONTR(pl)->target_object_count = 0;
                                send_target_command(CONTR(pl));
                            }

                            if (mp->faces[socket_layer]) {
                                packet_append_uint8(packet_layer, MAP2_LAYER_CLEAR);
                                packet_append_uint8(packet_layer, socket_layer);
                                num_layers++;
                            }

                            continue;
                        }

                        num_layers++;

                        packet_append_uint8(packet_layer, socket_layer);
                        packet_append_uint16(packet_layer, face);
                        packet_append_uint8(packet_layer, GET_CLIENT_FLAGS(head));
                        packet_append_uint8(packet_layer, flags);

                        /* Multi-arch? Add it's quick pos. */
                        if (flags & MAP2_FLAG_MULTI) {
                            packet_append_uint8(packet_layer, quick_pos);
                        }

                        /* Player name? Add the player's name, and their player
                         * name color. */
                        if (flags & MAP2_FLAG_NAME) {
                            packet_append_string_terminated(packet_layer, CONTR(tmp)->quick_name);
                            packet_append_string_terminated(packet_layer, get_playername_color(pl, tmp));
                        }

                        if (flags & MAP2_FLAG_ANIMATION) {
                            packet_append_uint8(packet_layer, anim_speed);
                            packet_append_uint8(packet_layer, anim_facing);
                            packet_append_uint8(packet_layer, anim_flags);

                            if (anim_flags & ANIM_FLAG_MOVING) {
                                packet_append_uint8(packet_layer,
                                        face_obj->state);
                            }
                        }

                        /* Z position. */
                        if (flags & MAP2_FLAG_HEIGHT) {
                            if (mirror && mirror->last_eat) {
                                packet_append_sint16(packet_layer, head->z + mirror->last_eat);
                            } else {
                                packet_append_sint16(packet_layer, head->z);
                            }
                        }

                        if (flags & MAP2_FLAG_ZOOM) {
                            /* First check mirror, even if the object *does*
                             * have custom zoom. */
                            if (mirror && mirror->last_heal) {
                                packet_append_uint16(packet_layer, mirror->last_heal);
                                packet_append_uint16(packet_layer, mirror->last_heal);
                            } else {
                                packet_append_uint16(packet_layer, head->zoom_x);
                                packet_append_uint16(packet_layer, head->zoom_y);
                            }
                        }

                        if (flags & MAP2_FLAG_ALIGN) {
                            if (mirror && mirror->align) {
                                packet_append_sint16(packet_layer, head->align + mirror->align);
                            } else {
                                packet_append_sint16(packet_layer, head->align);
                            }
                        }

                        if (flags & MAP2_FLAG_MORE) {
                            packet_append_uint32(packet_layer, flags2);

                            if (flags2 & MAP2_FLAG2_ALPHA) {
                                packet_append_uint8(packet_layer, head->alpha);
                            }

                            if (flags2 & MAP2_FLAG2_ROTATE) {
                                packet_append_sint16(packet_layer, head->rotate);
                            }

                            if (flags2 & MAP2_FLAG2_TARGET) {
                                packet_append_uint32(packet_layer, target_object_count);
                                packet_append_uint8(packet_layer, is_friend_of(pl, head));
                            }

                            /* Target's HP bar. */
                            if (flags2 & MAP2_FLAG2_PROBE) {
                                packet_append_uint8(packet_layer, probe_val);
                            }
                        }
                    } else if (mp->faces[socket_layer]) {
                        /* Didn't find anything. Now, if we have previously seen a
                         * face
                         * on this layer, we will want the client to clear it. */
                        mp->faces[socket_layer] = 0;
                        mp->quick_pos[socket_layer] = 0;
                        mp->anim_speed[socket_layer] = 0;
                        mp->anim_facing[socket_layer] = 0;

                        if (layer == LAYER_LIVING) {
                            mp->anim_flags[sub_layer] = 0;
                        }

                        packet_append_uint8(packet_layer, MAP2_LAYER_CLEAR);
                        packet_append_uint8(packet_layer, socket_layer);
                        num_layers++;
                    }
                }
            }

            packet_append_uint8(packet, num_layers);

            packet_merge(packet_layer, packet);
            packet_free(packet_layer);

            /* Kill animation? */
            if (GET_MAP_RTAG(m, nx, ny) == global_round_tag) {
                ext_flags |= MAP2_FLAG_EXT_ANIM;
                anim_type = ANIM_KILL;
                anim_value = GET_MAP_DAMAGE(m, nx, ny);
            }

            if (ext_flags == mp->ext_flags) {
                ext_flags = 0;
            } else {
                mp->ext_flags = ext_flags;
            }

            /* Add flags for this tile. */
            packet_append_uint8(packet, ext_flags);

            /* Animation? Add its type and value. */
            if (ext_flags & MAP2_FLAG_EXT_ANIM) {
                packet_append_uint8(packet, anim_type);
                packet_append_uint16(packet, anim_value);
            }

            /* If nothing has really changed, go back to the old position
             * in the packet. */
            if (!(mask & 0x3f) && !num_layers && !ext_flags) {
                packet_set_pos(packet, oldpos);
            }

            /* Set 'mirror' back to NULL, so we'll try to re-find it on another
             * tile. */
            mirror = NULL;
        }
    }

    /* Verify that we in fact do need to send this. */
    if (packet->len >= 4) {
        socket_send_packet(&CONTR(pl)->socket, packet);
    } else {
        packet_free(packet);
    }

    if (packet_sound->len >= 1) {
        socket_send_packet(&CONTR(pl)->socket, packet_sound);
    } else {
        packet_free(packet_sound);
    }
}

void socket_command_quest_list(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos)
{
    object *quest_container, *tmp, *tmp2, *last;
    StringBuffer *sb;
    packet_struct *packet;
    char *cp;
    size_t cp_len;

    quest_container = pl->quest_container;

    if (!quest_container || !quest_container->inv) {
        packet = packet_new(CLIENT_CMD_BOOK, 0, 0);
        packet_append_string_terminated(packet, "[title]No quests to speak of.[/title]");
        socket_send_packet(&pl->socket, packet);
        return;
    }

    sb = stringbuffer_new();
    stringbuffer_append_string(sb, "[book]Quest List[/book][title]Incomplete quests:[/title]\n");

    /* First show incomplete quests */
    for (tmp = quest_container->inv; tmp; tmp = tmp->below) {
        if (tmp->type != QUEST_CONTAINER || tmp->magic == QUEST_STATUS_COMPLETED) {
            continue;
        }

        stringbuffer_append_printf(sb, "\n[title]%s[/title]", tmp->race);

        /* Find the last entry. */
        for (last = tmp->inv; last && last->below; last = last->below) {
        }

        /* Show the quest parts. */
        for (tmp2 = last; tmp2; tmp2 = tmp2->above) {
            stringbuffer_append_printf(sb, "\n[b]%s[/b]", tmp2->race);

            if (tmp2->msg) {
                stringbuffer_append_printf(sb, ": %s", tmp2->msg);

                if (tmp2->magic == QUEST_STATUS_COMPLETED) {
                    stringbuffer_append_string(sb, " []done]");
                }
            }

            switch (tmp2->sub_type) {
            case QUEST_TYPE_KILL:
                stringbuffer_append_printf(sb, "\n[x=10]Status: %d/%d", MIN(tmp2->last_sp, tmp2->last_grace), tmp2->last_grace);
                break;
            }
        }

        stringbuffer_append_string(sb, "\n");
    }

    stringbuffer_append_string(sb, "[p]\n[title]Completed quests:[/title]\n");

    /* Now show completed quests */
    for (tmp = quest_container->inv; tmp; tmp = tmp->below) {
        if (tmp->type != QUEST_CONTAINER || tmp->magic != QUEST_STATUS_COMPLETED) {
            continue;
        }

        stringbuffer_append_printf(sb, "\n[title]%s[/title]", tmp->race);
    }

    cp_len = stringbuffer_length(sb);
    cp = stringbuffer_finish(sb);

    packet = packet_new(CLIENT_CMD_BOOK, 0, 0);
    packet_append_data_len(packet, (uint8 *) cp, cp_len);
    socket_send_packet(&pl->socket, packet);
    efree(cp);
}

void socket_command_clear(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos)
{
    ns->packet_recv_cmd->len = 0;
}

void socket_command_move_path(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos)
{
    uint8 x, y;
    mapstruct *m;
    int xt, yt;
    path_node *node, *tmp;

    x = packet_to_uint8(data, len, &pos);
    y = packet_to_uint8(data, len, &pos);

    /* Validate the passed x/y. */
    if (x >= pl->socket.mapx || y >= pl->socket.mapy) {
        return;
    }

    /* If this is the middle of the screen where the player is already,
     * there isn't much to do. */
    if (x == pl->socket.mapx_2 && y == pl->socket.mapy_2) {
        return;
    }

    /* The x/y we got above is from the client's map, so 0,0 is
     * actually topmost (northwest) corner of the map in the client,
     * and not 0,0 of the actual map, so we need to transform it to
     * actual map coordinates. */
    xt = pl->ob->x + (x - pl->socket.mapx / 2);
    yt = pl->ob->y + (y - pl->socket.mapy / 2);
    m = get_map_from_coord(pl->ob->map, &xt, &yt);

    /* Invalid x/y. */
    if (!m) {
        return;
    }

    /* Find and compress the path to the destination. */
    node = compress_path(find_path(pl->ob, pl->ob->map, pl->ob->x, pl->ob->y, m, xt, yt));

    /* No path available. */
    if (!node) {
        return;
    }

    /* Clear any previously queued paths. */
    player_path_clear(pl);

    /* 'node' now actually points to where the player is standing, so
     * skip that. */
    if (node->next) {
        for (tmp = node->next; tmp; tmp = tmp->next) {
            player_path_add(pl, tmp->map, tmp->x, tmp->y);
        }
    }

    /* The last x,y where we wanted to move is not included in the
     * above paths finding, so we have to add it manually. */
    player_path_add(pl, m, xt, yt);
}

void socket_command_fire(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos)
{
    int dir;
    tag_t tag;
    object *tmp;
    double skill_time, delay;

    dir = packet_to_uint8(data, len, &pos);
    dir = MAX(0, MIN(dir, 8));
    tag = packet_to_uint32(data, len, &pos);

    if (tag) {
        if (pl->equipment[PLAYER_EQUIP_WEAPON_RANGED] && pl->equipment[PLAYER_EQUIP_WEAPON_RANGED]->count == tag) {
            tmp = pl->equipment[PLAYER_EQUIP_WEAPON_RANGED];
        } else {
            for (tmp = pl->ob->inv; tmp; tmp = tmp->below) {
                if (tmp->count == tag && (tmp->type == SPELL || tmp->type == SKILL)) {
                    break;
                }
            }
        }
    } else {
        tmp = pl->equipment[PLAYER_EQUIP_WEAPON_RANGED];

        if (!tmp && pl->equipment[PLAYER_EQUIP_AMMO] && QUERY_FLAG(pl->equipment[PLAYER_EQUIP_AMMO], FLAG_IS_THROWN)) {
            tmp = pl->equipment[PLAYER_EQUIP_WEAPON];
        }
    }

    if (!tmp) {
        return;
    }

    if (!check_skill_to_fire(pl->ob, tmp)) {
        return;
    }

    if (pl->action_attack > global_round_tag) {
        return;
    }

    skill_time = skills[pl->ob->chosen_skill->stats.sp].time;
    delay = 0;

    object_ranged_fire(tmp, pl->ob, dir, &delay);

    if (skill_time > 1.0f) {
        skill_time -= (SK_level(pl->ob) / 10 / 3) * 0.1f;

        if (skill_time < 1.0f) {
            skill_time = 1.0f;
        }
    }

    pl->action_attack = global_round_tag + skill_time + delay;

    pl->action_timer = (float) (pl->action_attack - global_round_tag) / MAX_TICKS * 1000.0;
    pl->last_action_timer = 0;
}

void socket_command_keepalive(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos)
{
    uint32 id;
    packet_struct *packet;

    ns->keepalive = 0;

    id = packet_to_uint32(data, len, &pos);

    packet = packet_new(CLIENT_CMD_KEEPALIVE, 20, 0);
    packet_enable_ndelay(packet);
    packet_append_uint32(packet, id);
    socket_send_packet(ns, packet);
}

void socket_command_move(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos)
{
    uint8 dir, run_on;

    dir = packet_to_uint8(data, len, &pos);
    dir = MIN(dir, 8);
    run_on = packet_to_uint8(data, len, &pos);

    pl->run_on = MIN(1, run_on);

    if (dir != 0) {
        pl->ob->speed_left -= 1.0;
        move_object(pl->ob, dir);
    }
}

/**
 * Send target command, calculate the target's color level, etc.
 * @param pl Player requesting this. */
void send_target_command(player *pl)
{
    packet_struct *packet;

    if (!pl->ob->map) {
        return;
    }

    packet = packet_new(CLIENT_CMD_TARGET, 64, 64);

    pl->ob->enemy = NULL;
    pl->ob->enemy_count = 0;

    if (!pl->target_object || pl->target_object == pl->ob || !OBJECT_VALID(pl->target_object, pl->target_object_count) || IS_INVISIBLE(pl->target_object, pl->ob)) {
        packet_append_uint8(packet, CMD_TARGET_SELF);
        packet_append_string_terminated(packet, COLOR_YELLOW);
        packet_append_string_terminated(packet, pl->ob->name);

        pl->target_object = pl->ob;
        pl->target_object_count = 0;
    } else {
        if (is_friend_of(pl->ob, pl->target_object)) {
            packet_append_uint8(packet, CMD_TARGET_FRIEND);
        } else {
            packet_append_uint8(packet, CMD_TARGET_ENEMY);

            pl->ob->enemy = pl->target_object;
            pl->ob->enemy_count = pl->target_object_count;
        }

        if (pl->target_object->level < level_color[pl->ob->level].yellow) {
            if (pl->target_object->level < level_color[pl->ob->level].green) {
                packet_append_string_terminated(packet, COLOR_GRAY);
            } else {
                if (pl->target_object->level < level_color[pl->ob->level].blue) {
                    packet_append_string_terminated(packet, COLOR_GREEN);
                } else {
                    packet_append_string_terminated(packet, COLOR_BLUE);
                }
            }
        } else {
            if (pl->target_object->level >= level_color[pl->ob->level].purple) {
                packet_append_string_terminated(packet, COLOR_PURPLE);
            } else if (pl->target_object->level >= level_color[pl->ob->level].red) {
                packet_append_string_terminated(packet, COLOR_RED);
            } else if (pl->target_object->level >= level_color[pl->ob->level].orange) {
                packet_append_string_terminated(packet, COLOR_ORANGE);
            } else {
                packet_append_string_terminated(packet, COLOR_YELLOW);
            }
        }

        if (pl->tgm) {
            char buf[MAX_BUF];

            snprintf(buf, sizeof(buf), "%s (lvl %d)", pl->target_object->name, pl->target_object->level);
            packet_append_string_terminated(packet, buf);
        } else {
            packet_append_string_terminated(packet, pl->target_object->name);
        }
    }

    socket_send_packet(&pl->socket, packet);
}

void socket_command_account(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos)
{
    uint8 type;

    type = packet_to_uint8(data, len, &pos);

    if (type == CMD_ACCOUNT_LOGIN) {
        char name[MAX_BUF], password[MAX_BUF];

        packet_to_string(data, len, &pos, name, sizeof(name));
        packet_to_string(data, len, &pos, password, sizeof(password));

        if (*name == '\0' || *password == '\0' || string_contains_other(name, settings.allowed_chars[ALLOWED_CHARS_ACCOUNT]) || string_contains_other(password, settings.allowed_chars[ALLOWED_CHARS_PASSWORD])) {
            return;
        }

        account_login(ns, name, password);
    } else if (type == CMD_ACCOUNT_REGISTER) {
        char name[MAX_BUF], password[MAX_BUF], password2[MAX_BUF];

        packet_to_string(data, len, &pos, name, sizeof(name));
        packet_to_string(data, len, &pos, password, sizeof(password));
        packet_to_string(data, len, &pos, password2, sizeof(password2));

        account_register(ns, name, password, password2);
    } else if (type == CMD_ACCOUNT_LOGIN_CHAR) {
        char name[MAX_BUF];

        packet_to_string(data, len, &pos, name, sizeof(name));

        account_login_char(ns, name);
    } else if (type == CMD_ACCOUNT_NEW_CHAR) {
        char name[MAX_BUF], archname[MAX_BUF];

        packet_to_string(data, len, &pos, name, sizeof(name));
        packet_to_string(data, len, &pos, archname, sizeof(archname));

        account_new_char(ns, name, archname);
    } else if (type == CMD_ACCOUNT_PSWD) {
        char password[MAX_BUF], password_new[MAX_BUF], password_new2[MAX_BUF];

        packet_to_string(data, len, &pos, password, sizeof(password));
        packet_to_string(data, len, &pos, password_new, sizeof(password_new));
        packet_to_string(data, len, &pos, password_new2, sizeof(password_new2));

        account_password_change(ns, password, password_new, password_new2);
    }
}

/**
 * Generate player's name, as visible on the map.
 * @param pl The player. */
void generate_quick_name(player *pl)
{
    int i;

    snprintf(pl->quick_name, sizeof(pl->quick_name), "%s", pl->ob->name);

    for (i = 0; i < pl->num_cmd_permissions; i++) {
        if (pl->cmd_permissions[i] && string_startswith(pl->cmd_permissions[i], "[") && string_endswith(pl->cmd_permissions[i], "]")) {
            snprintfcat(pl->quick_name, sizeof(pl->quick_name), " %s",
                    pl->cmd_permissions[i]);
        }
    }

    if (pl->afk) {
        snprintfcat(pl->quick_name, sizeof(pl->quick_name), " [AFK]");
    }
}

void socket_command_target(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos)
{
    uint8 type;

    type = packet_to_uint8(data, len, &pos);

    if (type == CMD_TARGET_MAPXY) {
        uint8 x, y;
        uint32 count, target_object_count;
        int i, xt, yt;
        mapstruct *m;
        object *tmp;

        x = packet_to_uint8(data, len, &pos);
        y = packet_to_uint8(data, len, &pos);
        count = packet_to_uint32(data, len, &pos);

        /* Validate the passed x/y. */
        if (x >= pl->socket.mapx || y >= pl->socket.mapy) {
            return;
        }

        target_object_count = pl->target_object_count;
        pl->target_object = NULL;
        pl->target_object_count = 0;

        for (i = 0; i <= SIZEOFFREE1 && !pl->target_object_count; i++) {
            /* Check whether we are still in range of the player's
             * viewport, and whether the player can see the square. */
            if (x + freearr_x[i] < 0 || x + freearr_x[i] >= pl->socket.mapx || y + freearr_y[i] < 0 || y + freearr_y[i] >= pl->socket.mapy || pl->blocked_los[x + freearr_x[i]][y + freearr_y[i]] > BLOCKED_LOS_BLOCKSVIEW) {
                continue;
            }

            /* The x/y we got above is from the client's map, so 0,0 is
             * actually topmost (northwest) corner of the map in the client,
             * and not 0,0 of the actual map, so we need to transform it to
             * actual map coordinates. */
            xt = pl->ob->x + (x - pl->socket.mapx_2) + freearr_x[i];
            yt = pl->ob->y + (y - pl->socket.mapy_2) + freearr_y[i];
            m = get_map_from_coord(pl->ob->map, &xt, &yt);

            /* Invalid x/y. */
            if (!m) {
                continue;
            }

            /* Nothing alive on this spot. */
            if (!(GET_MAP_FLAGS(m, xt, yt) & (P_IS_MONSTER | P_IS_PLAYER))) {
                continue;
            }

            FOR_MAP_LAYER_BEGIN(m, xt, yt, LAYER_LIVING, -1, tmp)
            {
                tmp = HEAD(tmp);

                if ((!count || tmp->count == count) && IS_LIVE(tmp) && tmp != pl->ob && !IS_INVISIBLE(tmp, pl->ob) && !OBJECT_IS_HIDDEN(pl->ob, tmp)) {
                    pl->target_object = tmp;
                    pl->target_object_count = tmp->count;
                    FOR_MAP_LAYER_BREAK;
                }
            }
            FOR_MAP_LAYER_END
        }

        if (pl->target_object_count != target_object_count) {
            send_target_command(pl);
        }
    } else if (type == CMD_TARGET_CLEAR) {
        if (pl->target_object_count) {
            pl->target_object = NULL;
            pl->target_object_count = 0;
            send_target_command(pl);
        }
    }
}

void socket_command_talk(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos)
{
    uint8 type;
    char msg[HUGE_BUF];

    pl->ob->speed_left -= 1.0;

    type = packet_to_uint8(data, len, &pos);

    if (type == CMD_TALK_NPC || type == CMD_TALK_NPC_NAME) {
        char npc_name[MAX_BUF];
        int i, x, y;
        mapstruct *m;
        object *tmp, *npc;

        if (type == CMD_TALK_NPC_NAME) {
            packet_to_string(data, len, &pos, npc_name, sizeof(npc_name));

            if (string_isempty(npc_name)) {
                return;
            }
        }

        packet_to_string(data, len, &pos, msg, sizeof(msg));
        player_sanitize_input(msg);

        if (string_isempty(msg)) {
            return;
        }

        npc = NULL;

        if (type == CMD_TALK_NPC && OBJECT_VALID(pl->target_object, pl->target_object_count) && OBJECT_CAN_TALK(pl->target_object)) {
            for (i = 0; i <= SIZEOFFREE2; i++) {
                x = pl->ob->x + freearr_x[i];
                y = pl->ob->y + freearr_y[i];
                m = get_map_from_coord(pl->ob->map, &x, &y);

                if (!m) {
                    continue;
                }

                if (m == pl->target_object->map && x == pl->target_object->x && y == pl->target_object->y) {
                    npc = pl->target_object;
                    break;
                }
            }
        }

        /* Use larger search space when trying to talk to a specific NPC. */
        for (i = 0; i < (type == CMD_TALK_NPC ? SIZEOFFREE2 + 1 : SIZEOFFREE) && !npc; i++) {
            x = pl->ob->x + freearr_x[i];
            y = pl->ob->y + freearr_y[i];
            m = get_map_from_coord(pl->ob->map, &x, &y);

            if (!m) {
                continue;
            }

            FOR_MAP_LAYER_BEGIN(m, x, y, LAYER_LIVING, -1, tmp)
            {
                if (OBJECT_CAN_TALK(tmp) && (type == CMD_TALK_NPC || strcmp(tmp->name, npc_name) == 0)) {
                    npc = tmp;
                    FOR_MAP_LAYER_BREAK;
                }
            }
            FOR_MAP_LAYER_END
        }

        if (npc) {
            logger_print(LOG(CHAT), "[TALKTO] [%s] [%s] %s", pl->ob->name, npc->name, msg);

            if (talk_to_npc(pl->ob, npc, msg)) {
                if (pl->target_object != npc || pl->target_object_count != npc->count) {
                    pl->target_object = npc;
                    pl->target_object_count = npc->count;
                    send_target_command(pl);
                }
            }
        } else if (type == CMD_TALK_NPC && OBJECT_VALID(pl->target_object, pl->target_object_count) && OBJECT_CAN_TALK(pl->target_object)) {
            draw_info_format(COLOR_WHITE, pl->ob, "You are too far away from %s.", pl->target_object->name);
        } else {
            draw_info(COLOR_WHITE, pl->ob, "There are no NPCs that you can talk to nearby.");
        }
    } else if (type == CMD_TALK_INV || type == CMD_TALK_BELOW || type == CMD_TALK_CONTAINER) {
        tag_t tag;
        object *tmp;

        tag = packet_to_uint32(data, len, &pos);
        packet_to_string(data, len, &pos, msg, sizeof(msg));
        player_sanitize_input(msg);

        if (string_isempty(msg)) {
            return;
        }

        if (type == CMD_TALK_INV) {
            tmp = pl->ob->inv;
        } else if (type == CMD_TALK_BELOW) {
            tmp = GET_MAP_OB_LAST(pl->ob->map, pl->ob->x, pl->ob->y);
        } else if (type == CMD_TALK_CONTAINER && pl->container) {
            tmp = pl->container->inv;
        } else {
            return;
        }

        for (; tmp; tmp = tmp->below) {
            if (tmp->count == tag && HAS_EVENT(tmp, EVENT_SAY)) {
                trigger_event(EVENT_SAY, pl->ob, tmp, NULL, msg, 0, 0, 0, 0);
                break;
            }
        }
    }
}

void socket_command_control(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos)
{
    size_t pos2;
    char word[MAX_BUF], app_name[MAX_BUF];
    uint8 ip_match, type, sub_type;
    packet_struct *packet;

    if (strcasecmp(settings.control_allowed_ips, "none") == 0) {
        return;
    }

    pos2 = 0;
    ip_match = 0;

    while (string_get_word(settings.control_allowed_ips, &pos2, ',', word, sizeof(word), 0)) {
        if (strcmp(ns->host, word) == 0) {
            ip_match = 1;
            break;
        }
    }

    if (!ip_match) {
        return;
    }

    packet_to_string(data, len, &pos, app_name, sizeof(app_name));

    if (string_isempty(app_name)) {
        return;
    }

    type = packet_to_uint8(data, len, &pos);
    sub_type = packet_to_uint8(data, len, &pos);

    switch (type) {
    case CMD_CONTROL_MAP:
    {
        char mappath[HUGE_BUF];
        shstr *mappath_sh;
        mapstruct *control_map;

        packet_to_string(data, len, &pos, mappath, sizeof(mappath));

        mappath_sh = add_string(mappath);
        control_map = has_been_loaded_sh(mappath_sh);
        free_string_shared(mappath_sh);

        /* No such map has been loaded, nothing to do. */
        if (control_map == NULL) {
            return;
        }

        switch (sub_type) {
        case CMD_CONTROL_MAP_RESET:
        {
            map_force_reset(control_map);
            return;
        }
        }

        break;
    }

    case CMD_CONTROL_PLAYER:
    {
        char playername[MAX_BUF];
        player *control_player;
        int ret;

        packet_to_string(data, len, &pos, playername, sizeof(playername));

        /* Attempt to find a suitable player as the controller. */
        if (!string_isempty(playername)) {
            control_player = find_player(playername);
        } else if (!string_isempty(settings.control_player)) {
            control_player = find_player(settings.control_player);
        } else {
            control_player = first_player;
        }

        /* No player has been found, return immediately. This is not an
         * error; no player is logged in, for example. */
        if (control_player == NULL) {
            return;
        }

        ret = 0;

        switch (sub_type) {
        case CMD_CONTROL_PLAYER_TELEPORT:
        {
            char mappath[HUGE_BUF];
            sint16 x, y;
            mapstruct *m;

            packet_to_string(data, len, &pos, mappath, sizeof(mappath));
            x = packet_to_sint16(data, len, &pos);
            y = packet_to_sint16(data, len, &pos);

            m = ready_map_name(mappath, 0);

            if (m == NULL) {
                log(LOG(DEBUG), "Could not teleport player to '%s' "
                        "(%d,%d): map could not be loaded.",
                        mappath, x, y);
                return;
            }

            ret = object_enter_map(control_player->ob, NULL, m, x, y, 1);
            break;
        }
        }

        if (ret == 1) {
            packet = packet_new(CLIENT_CMD_CONTROL, 256, 256);
            packet_append_data_len(packet, data, len);
            socket_send_packet(&control_player->socket, packet);

            return;
        }

        break;
    }
    }

    log(LOG(DEBUG), "Unrecognised control command type: %d, sub-type: %d, "
            "by application: '%s'", type, sub_type, app_name);
}
