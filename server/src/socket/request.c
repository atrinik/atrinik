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
 * This file implements all of the goo on the server side for handling
 * clients.  It's got a bunch of global variables for keeping track of
 * each of the clients.
 *
 * @note All functions that are used to process data from the client
 * have the prototype of (char *data, int datalen, int client_num).  This
 * way, we can use one dispatch table.
 */

#include <global.h>
#include <toolkit/packet.h>
#include <toolkit/string.h>
#include <monster_data.h>
#include <plugin.h>
#include <monster_guard.h>
#include <player.h>
#include <object.h>
#include <exp.h>
#include <arrow.h>
#include <bow.h>
#include <sound_ambient.h>
#include <object_methods.h>
#include <resources.h>

#include <openssl/crypto.h>
#define GET_CLIENT_FLAGS(_O_) ((_O_)->flags[0] & 0x7f)
#define NO_FACE_SEND (-1)
#define JOIN_FAILURES_PER_MINUTE 5U
#define JOIN_FAILURES_UNKNOWN_PER_MINUTE 64U
#define JOIN_FAILURES_GLOBAL_PER_MINUTE 256U
#define JOIN_FAILURE_ENTRY_MAX 1024U

typedef struct join_failure_entry {
    UT_hash_handle hh;
    char address[MAX_BUF];
    time_t window_started;
    unsigned int failures;
} join_failure_entry_t;

static join_failure_entry_t *join_failures;
static time_t join_failure_global_window;
static unsigned int join_failure_global_count;

static bool join_password_allowed(socket_struct *ns) {
    time_t now = time(NULL);
    if (now - join_failure_global_window >= 60) {
        join_failure_global_window = now;
        join_failure_global_count = 0;
    }
    if (join_failure_global_count >= JOIN_FAILURES_GLOBAL_PER_MINUTE) {
        return false;
    }

    const char *address = socket_get_addr(ns->sc);
    join_failure_entry_t *entry;
    HASH_FIND_STR(join_failures, address, entry);
    if (entry == NULL) {
        join_failure_entry_t *old, *next;
        HASH_ITER(hh, join_failures, old, next) {
            if (now - old->window_started >= 120) {
                HASH_DEL(join_failures, old);
                free(old);
            }
        }
        if (HASH_COUNT(join_failures) >= JOIN_FAILURE_ENTRY_MAX) {
            join_failure_entry_t *oldest = NULL;
            HASH_ITER(hh, join_failures, old, next) {
                if (oldest == NULL || old->window_started < oldest->window_started) {
                    oldest = old;
                }
            }
            if (oldest != NULL) {
                HASH_DEL(join_failures, oldest);
                free(oldest);
            }
        }
        entry = xcalloc(1, sizeof(*entry));
        snprintf(VS(entry->address), "%s", address);
        entry->window_started = now;
        HASH_ADD_STR(join_failures, address, entry);
    }

    if (now - entry->window_started >= 60) {
        entry->window_started = now;
        entry->failures = 0;
    }
    unsigned int limit = strcmp(address, "<no address>") == 0 ? JOIN_FAILURES_UNKNOWN_PER_MINUTE
                                                              : JOIN_FAILURES_PER_MINUTE;
    return entry->failures < limit;
}

static void join_password_failed(socket_struct *ns) {
    if (join_failure_global_count < UINT_MAX) {
        join_failure_global_count++;
    }
    const char *address = socket_get_addr(ns->sc);
    join_failure_entry_t *entry;
    HASH_FIND_STR(join_failures, address, entry);
    if (entry != NULL && entry->failures < UINT_MAX) {
        entry->failures++;
    }
}

void socket_command_setup(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos) {
    packet_struct *packet;
    uint8_t type;

    packet = packet_new(CLIENT_CMD_SETUP, 256, 256);

    while (pos < len) {
        type = packet_to_uint8(data, len, &pos);
        packet_debug_data(packet, 0, "Setup type");
        packet_append_uint8(packet, type);

        if (type == CMD_SETUP_SOUND) {
            ns->sound = packet_to_uint8(data, len, &pos);
            packet_debug_data(packet, 0, "Sound");
            packet_append_uint8(packet, ns->sound);
        } else if (type == CMD_SETUP_MAPSIZE) {
            int x, y;

            x = packet_to_uint8(data, len, &pos);
            y = packet_to_uint8(data, len, &pos);

            if (x < 13 || y < 13 || x > MAP_CLIENT_X || y > MAP_CLIENT_Y) {
                LOG(PACKET, "X/Y not in range: %d, %d", x, y);
                x = MAP_CLIENT_X;
                y = MAP_CLIENT_Y;
            }

            ns->mapx = x;
            ns->mapy = y;
            ns->mapx_2 = x / 2;
            ns->mapy_2 = y / 2;

            packet_debug_data(packet, 0, "Map width");
            packet_append_uint8(packet, x);
            packet_debug_data(packet, 0, "Map height");
            packet_append_uint8(packet, y);
        } else if (type == CMD_SETUP_DATA_URL) {
            char url[MAX_BUF];

            packet_to_string(data, len, &pos, url, sizeof(url));
            packet_debug_data(packet, 0, "Data URL");

            if (!string_isempty(url)) {
                packet_append_string_terminated(packet, url);
            } else {
                packet_append_string_terminated(packet, settings.http_url);
            }
        } else if (type == CMD_SETUP_ASSET_TRANSPORT) {
            packet_append_uint8(packet, socket_is_quic(ns->sc) ? 1 : 0);
        } else if (type == CMD_SETUP_CONNECTION_MODE) {
            socket_connection_mode_t mode = packet_to_uint8(data, len, &pos);
            if (socket_is_quic(ns->sc) && mode >= SOCKET_CONNECTION_MODE_QUIC_LAN &&
                mode <= SOCKET_CONNECTION_MODE_QUIC_DIRECTORY) {
                ns->connection_mode = mode;
            }
            packet_append_uint8(packet, ns->connection_mode);
        } else if (type == CMD_SETUP_JOIN_PASSWORD) {
            char password[MAX_BUF] = {0};
            packet_to_string(data, len, &pos, VS(password));

            bool allowed = join_password_allowed(ns);
            ns->join_authenticated =
                allowed && CRYPTO_memcmp(settings.join_password, password, sizeof(password)) == 0;
            if (!ns->join_authenticated) {
                join_password_failed(ns);
            }
            OPENSSL_cleanse(password, sizeof(password));
            packet_append_uint8(packet, ns->join_authenticated ? 1 : 0);
        } else {
            LOG(PACKET, "Unknown type: %d", type);
        }
    }

    if (*settings.join_password != '\0' && !ns->join_authenticated) {
        LOG(SYSTEM,
            "Connection %s rejected: incorrect or missing join password",
            socket_get_id(ns->sc));
        draw_info_send(CHAT_TYPE_GAME,
                       NULL,
                       COLOR_RED,
                       ns,
                       "Incorrect or missing server join password.");
        /* JOIN_PASSWORD promises an explicit acceptance result. Send the
         * SETUP response before entering the short zombie grace period so the
         * client can report the rejection instead of appearing to hang. */
        socket_send_packet(ns, packet);
        ns->state = ST_ZOMBIE;
        return;
    }

    socket_send_packet(ns, packet);
}

void socket_command_player_cmd(socket_struct *ns,
                               player *pl,
                               uint8_t *data,
                               size_t len,
                               size_t pos) {
    char command[MAX_BUF];

    if (pl->cs->state != ST_PLAYING) {
        LOG(PACKET, "Received player command while not playing.");
        return;
    }

    packet_to_string(data, len, &pos, command, sizeof(command));
    commands_handle(pl->ob, command);
}

void socket_command_version(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos) {
    uint32_t ver;
    packet_struct *packet;

    /* Ignore multiple version commands. */
    if (ns->socket_version != 0) {
        LOG(PACKET, "Received extraneous version command.");
        return;
    }

    ver = packet_to_uint32(data, len, &pos);

    if (ver < SOCKET_VERSION) {
        draw_info_send(CHAT_TYPE_GAME,
                       NULL,
                       COLOR_RED,
                       ns,
                       "Your client uses an incompatible gameplay protocol.\n"
                       "Please update to the latest Atrinik client.");
        ns->state = ST_ZOMBIE;
        return;
    }

    ns->socket_version = ver;

    packet = packet_new(CLIENT_CMD_VERSION, 4, 4);
    packet_debug_data(packet, 0, "Socket version");
    packet_append_uint32(packet, SOCKET_VERSION);
    socket_send_packet(ns, packet);
}

void socket_command_item_move(socket_struct *ns,
                              player *pl,
                              uint8_t *data,
                              size_t len,
                              size_t pos) {
    tag_t to, tag;
    uint32_t nrof;

    to = packet_to_uint32(data, len, &pos);
    tag = packet_to_uint32(data, len, &pos);
    nrof = packet_to_uint32(data, len, &pos);

    if (tag == 0) {
        LOG(PACKET, "Tag is zero.");
        return;
    }

    esrv_move_object(pl->ob, to, tag, nrof);
}

/**
 * Sends player statistics update.
 *
 * We look at the old values, and only send what has changed.
 *
 * Stat mapping values are in socket.h
 */
void esrv_update_stats(player *pl) {
    HARD_ASSERT(pl != NULL);

    SOFT_ASSERT(pl->ob != NULL, "Player has no object!");

#define _Add(_old, _new, _type, _bitsize)               \
    if (packet == NULL) {                               \
        packet = packet_new(CLIENT_CMD_STATS, 32, 128); \
    }                                                   \
    (_old) = (_new);                                    \
    packet_debug_data(packet, 0, "Stats command type"); \
    packet_append_uint8(packet, (_type));               \
    packet_debug_data(packet, 0, "%s", #_new);          \
    packet_append_##_bitsize(packet, (_new));
#define AddIf(_old, _new, _type, _bitsize) \
    if ((_old) != (_new)) {                \
        _Add(_old, _new, _type, _bitsize); \
    }
#define AddIfFloat(_old, _new, _type)   \
    if (!FLT_EQUAL(_old, _new)) {       \
        _Add(_old, _new, _type, float); \
    }
#define AddIfDouble(_old, _new, _type)   \
    if (!DBL_EQUAL(_old, _new)) {        \
        _Add(_old, _new, _type, double); \
    }

    packet_struct *packet = NULL;

    int spell_cost_level = 0;
    if (pl->skill_ptr[SK_WIZARDRY_SPELLS] != NULL) {
        spell_cost_level = pl->skill_ptr[SK_WIZARDRY_SPELLS]->level;
    }

    if (pl->last_spell_cost_level != spell_cost_level) {
        pl->last_spell_cost_level = spell_cost_level;

        FOR_INV_PREPARE(pl->ob, tmp) {
            if (tmp->type == SPELL) {
                esrv_update_item(UPD_EXTRA, tmp);
            }
        }
        FOR_INV_FINISH();
    }

    int target_level = pl->ob->level;
    if (pl->target_object != NULL &&
        (pl->target_object == pl->ob || OBJECT_VALID(pl->target_object, pl->target_object_count))) {
        target_level = pl->target_object->level;
    }

    if (pl->last_target_level != target_level || pl->last_target_viewer_level != pl->ob->level) {
        send_target_command(pl);
    }

    if (pl->target_object && pl->target_object != pl->ob) {
        uint8_t hp =
            MAX(1,
                (((float)pl->target_object->stats.hp / (float)pl->target_object->stats.maxhp) *
                 100.0f));
        AddIf(pl->target_hp, hp, CS_STAT_TARGET_HP, uint8);
    }

    AddIf(pl->last_gen_hp, pl->gen_client_hp, CS_STAT_REG_HP, uint16);
    AddIf(pl->last_gen_sp, pl->gen_client_sp, CS_STAT_REG_MANA, uint16);
    AddIfFloat(pl->last_action_timer, pl->action_timer, CS_STAT_ACTION_TIME);
    AddIf(pl->last_level, pl->ob->level, CS_STAT_LEVEL, uint8);
    AddIfDouble(pl->last_speed, pl->ob->speed, CS_STAT_SPEED);
    AddIfDouble(pl->last_weapon_speed, pl->ob->weapon_speed / MAX_TICKS, CS_STAT_WEAPON_SPEED);
    AddIf(pl->last_weight_limit, weight_limit[pl->ob->stats.Str], CS_STAT_WEIGHT_LIM, uint32);
    AddIf(pl->last_stats.hp, pl->ob->stats.hp, CS_STAT_HP, int32);
    AddIf(pl->last_stats.maxhp, pl->ob->stats.maxhp, CS_STAT_MAXHP, int32);
    AddIf(pl->last_stats.sp, pl->ob->stats.sp, CS_STAT_SP, int16);
    AddIf(pl->last_stats.maxsp, pl->ob->stats.maxsp, CS_STAT_MAXSP, int16);
    AddIf(pl->last_stats.Str, pl->ob->stats.Str, CS_STAT_STR, uint8);
    AddIf(pl->last_stats.Dex, pl->ob->stats.Dex, CS_STAT_DEX, uint8);
    AddIf(pl->last_stats.Con, pl->ob->stats.Con, CS_STAT_CON, uint8);
    AddIf(pl->last_stats.Int, pl->ob->stats.Int, CS_STAT_INT, uint8);
    AddIf(pl->last_stats.Pow, pl->ob->stats.Pow, CS_STAT_POW, uint8);
    AddIf(pl->last_stats.exp, pl->ob->stats.exp, CS_STAT_EXP, uint64);
    AddIf(pl->last_stats.wc, pl->ob->stats.wc, CS_STAT_WC, uint16);
    AddIf(pl->last_stats.ac, pl->ob->stats.ac, CS_STAT_AC, uint16);
    AddIf(pl->last_stats.dam, pl->ob->stats.dam, CS_STAT_DAM, uint16);
    AddIf(pl->last_stats.food, pl->ob->stats.food, CS_STAT_FOOD, uint16);
    AddIf(pl->last_path_attuned, pl->ob->path_attuned, CS_STAT_PATH_ATTUNED, uint32);
    AddIf(pl->last_path_repelled, pl->ob->path_repelled, CS_STAT_PATH_REPELLED, uint32);
    AddIf(pl->last_path_denied, pl->ob->path_denied, CS_STAT_PATH_DENIED, uint32);

    object *ranged_weapon = pl->equipment[PLAYER_EQUIP_WEAPON_RANGED];
    object *arrow = NULL;
    if (ranged_weapon != NULL && ranged_weapon->type == BOW) {
        arrow = arrow_find(pl->ob, ranged_weapon->race);
    }

    int16_t ranged_dam, ranged_wc;
    float ranged_ws;
    if (arrow != NULL) {
        HARD_ASSERT(ranged_weapon != NULL);
        ranged_dam = arrow_get_damage(pl->ob, ranged_weapon, arrow);
        ranged_wc = arrow_get_wc(pl->ob, ranged_weapon, arrow);
        ranged_ws = bow_get_ws(ranged_weapon, arrow);
    } else if (ranged_weapon != NULL && ranged_weapon->type == SPELL) {
        ranged_dam = SP_level_dam_adjust(pl->ob, ranged_weapon->stats.sp, true);
        ranged_wc = 200;
        ranged_ws = spells[ranged_weapon->stats.sp].time / MAX_TICKS;
    } else {
        ranged_dam = ranged_wc = ranged_ws = 0;
    }

    AddIf(pl->last_ranged_dam, ranged_dam, CS_STAT_RANGED_DAM, uint16);
    AddIf(pl->last_ranged_wc, ranged_wc, CS_STAT_RANGED_WC, uint16);
    AddIfFloat(pl->last_ranged_ws, ranged_ws, CS_STAT_RANGED_WS);

    uint16_t flags = 0;

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

    AddIf(pl->last_flags, flags, CS_STAT_FLAGS, uint16);

    for (int i = 0; i < NROFATTACKS; i++) {
        /* If there are more attacks, but we reached CS_STAT_PROT_END,
         * we stop now. */
        if (CS_STAT_PROT_START + i > CS_STAT_PROT_END) {
            break;
        }

        AddIf(pl->last_protection[i], pl->ob->protection[i], CS_STAT_PROT_START + i, int8);
    }

    for (int i = 0; i < PLAYER_EQUIP_MAX; i++) {
        AddIf(pl->last_equipment[i],
              pl->equipment[i] != NULL ? pl->equipment[i]->count : 0,
              CS_STAT_EQUIP_START + i,
              uint32);
    }

    AddIf(pl->last_gender, object_get_gender(pl->ob), CS_STAT_GENDER, uint8);

    if (packet != NULL) {
        socket_send_packet(pl->cs, packet);
    }

#undef AddIf
#undef _Add
#undef AddIfFloat
#undef AddIfDouble
}

/**
 * Tells the client that here is a player it should start using.
 */
void esrv_new_player(player *pl, uint32_t weight) {
    packet_struct *packet;

    packet = packet_new(CLIENT_CMD_PLAYER, 12, 0);
    packet_debug_data(packet, 0, "Player object ID");
    packet_append_uint32(packet, pl->ob->count);
    packet_debug_data(packet, 0, "Weight");
    packet_append_uint32(packet, weight);
    packet_debug_data(packet, 0, "Face number");
    packet_append_uint32(packet, pl->ob->face->number);
    packet_debug_data(packet, 0, "Name");
    packet_append_string_terminated(packet, pl->ob->name);
    socket_send_packet(pl->cs, packet);
}

/**
 * Get ID of a tiled map by checking player::last_update.
 * @param pl
 * Player.
 * @param map
 * Tiled map.
 * @return
 * ID of the tiled map, 0 if there is no match.
 */
static inline int get_tiled_map_id(player *pl, struct mapdef *map) {
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
 * @param ns
 * Socket.
 * @param dx
 * X.
 * @param dy
 * Y.
 */
static inline void copy_lastmap(socket_struct *ns, int dx, int dy, int dz) {
    struct Map newmap = {0};
    int depth, x, y;

    for (depth = -MAP2_MAX_DEPTH; depth <= MAP2_MAX_DEPTH; depth++) {
        int source_depth = depth + dz;

        if (source_depth < -MAP2_MAX_DEPTH || source_depth > MAP2_MAX_DEPTH) {
            continue;
        }

        MapCell *source = ns->lastmap.levels[MAP2_DEPTH_INDEX(source_depth)];
        if (source == NULL) {
            continue;
        }

        for (x = 0; x < ns->mapx; x++) {
            for (y = 0; y < ns->mapy; y++) {
                if (x + dx < 0 || x + dx >= ns->mapx || y + dy < 0 || y + dy >= ns->mapy) {
                    continue;
                }

                MapCell *destination = map_client_cache_cell(&newmap, depth, x, y, true);
                memcpy(destination,
                       &source[(size_t)(x + dx) * MAP_CLIENT_Y + y + dy],
                       sizeof(*destination));
            }
        }
    }

    map_client_cache_free(&ns->lastmap);
    ns->lastmap = newmap;
}

void draw_map_text_anim(object *pl, const char *color, const char *text) {
    packet_struct *packet;

    packet = packet_new(CLIENT_CMD_MAPSTATS, 64, 64);
    packet_debug_data(packet, 0, "Mapstats command type");
    packet_append_uint8(packet, CMD_MAPSTATS_TEXT_ANIM);
    packet_debug_data(packet, 0, "Color");
    packet_append_string_terminated(packet, color);
    packet_debug_data(packet, 0, "Text");
    packet_append_string_terminated(packet, text);
    socket_send_packet(CONTR(pl)->cs, packet);
}

/**
 * Do some checks, map name and LOS and then draw the map.
 * @param pl
 * Whom to draw map for.
 */
void draw_client_map(object *pl) {
    int redraw_below = 0;

    if (pl->type != PLAYER) {
        LOG(BUG, "Called with non-player: %s", pl->name);
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
            map_client_cache_clear(&CONTR(pl)->cs->lastmap);
            CONTR(pl)->last_update = pl->map;
            redraw_below = 1;
        } else {
            CONTR(pl)->map_update_cmd = MAP_UPDATE_CMD_CONNECTED;
            CONTR(pl)->map_update_tile = tile_map;
            CONTR(pl)->map_off_z = 0;
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

                case TILED_UP:
                    CONTR(pl)->map_off_x = pl->x - CONTR(pl)->map_tile_x;
                    CONTR(pl)->map_off_y = pl->y - CONTR(pl)->map_tile_y;
                    CONTR(pl)->map_off_z = 1;
                    break;

                case TILED_DOWN:
                    CONTR(pl)->map_off_x = pl->x - CONTR(pl)->map_tile_x;
                    CONTR(pl)->map_off_y = pl->y - CONTR(pl)->map_tile_y;
                    CONTR(pl)->map_off_z = -1;
                    break;
            }

            copy_lastmap(CONTR(pl)->cs,
                         CONTR(pl)->map_off_x,
                         CONTR(pl)->map_off_y,
                         CONTR(pl)->map_off_z);
            CONTR(pl)->last_update = pl->map;
        }
    } else {
        if (CONTR(pl)->map_tile_x != pl->x || CONTR(pl)->map_tile_y != pl->y) {
            copy_lastmap(CONTR(pl)->cs,
                         pl->x - CONTR(pl)->map_tile_x,
                         pl->y - CONTR(pl)->map_tile_y,
                         0);
            redraw_below = 1;
        }
    }

    /* Redraw below window and backbuffer new positions? */
    if (redraw_below) {
        /* Backbuffer position so we can determine whether we have moved or not
         * */
        CONTR(pl)->map_tile_x = pl->x;
        CONTR(pl)->map_tile_y = pl->y;
        CONTR(pl)->cs->below_clear = 1;
        /* Redraw it */
        CONTR(pl)->cs->update_tile = 0;
        CONTR(pl)->cs->look_position = 0;
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
        map_info = msp->map_info && OBJECT_VALID(msp->map_info, msp->map_info_count) ? msp->map_info
                                                                                     : NULL;

        packet = packet_new(CLIENT_CMD_MAPSTATS, 256, 256);

        if ((map_info && map_info->race && strcmp(map_info->race, CONTR(pl)->map_info_name) != 0) ||
            (!map_info && CONTR(pl)->map_info_name[0] != '\0')) {
            packet_debug_data(packet, 0, "\nMapstats command type");
            packet_append_uint8(packet, CMD_MAPSTATS_NAME);
            packet_append_map_name(packet, pl, map_info);

            if (map_info) {
                strncpy(CONTR(pl)->map_info_name,
                        map_info->race,
                        sizeof(CONTR(pl)->map_info_name) - 1);
                CONTR(pl)->map_info_name[sizeof(CONTR(pl)->map_info_name) - 1] = '\0';
            } else {
                CONTR(pl)->map_info_name[0] = '\0';
            }
        }

        if ((map_info && map_info->slaying &&
             strcmp(map_info->slaying, CONTR(pl)->map_info_music) != 0) ||
            (!map_info && CONTR(pl)->map_info_music[0] != '\0')) {
            packet_debug_data(packet, 0, "\nMapstats command type");
            packet_append_uint8(packet, CMD_MAPSTATS_MUSIC);
            packet_append_map_music(packet, pl, map_info);

            if (map_info) {
                strncpy(CONTR(pl)->map_info_music,
                        map_info->slaying,
                        sizeof(CONTR(pl)->map_info_music) - 1);
                CONTR(pl)->map_info_music[sizeof(CONTR(pl)->map_info_music) - 1] = '\0';
            } else {
                CONTR(pl)->map_info_music[0] = '\0';
            }
        }

        if ((map_info && map_info->title &&
             strcmp(map_info->title, CONTR(pl)->map_info_weather) != 0) ||
            (!map_info && CONTR(pl)->map_info_weather[0] != '\0')) {
            packet_debug_data(packet, 0, "\nMapstats command type");
            packet_append_uint8(packet, CMD_MAPSTATS_WEATHER);
            packet_append_map_weather(packet, pl, map_info);

            if (map_info) {
                strncpy(CONTR(pl)->map_info_weather,
                        map_info->title,
                        sizeof(CONTR(pl)->map_info_weather) - 1);
                CONTR(pl)->map_info_weather[sizeof(CONTR(pl)->map_info_weather) - 1] = '\0';
            } else {
                CONTR(pl)->map_info_weather[0] = '\0';
            }
        }

        /* Anything to send? */
        if (packet->len >= 1) {
            socket_send_packet(CONTR(pl)->cs, packet);
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
 * @param pl
 * Player object that will get the map data sent to.
 * @param op
 * Player object on the map, to get the name from.
 * @return
 * The color.
 */
static const char *get_playername_color(object *pl, object *op) {
    if (CONTR(pl)->party != NULL && CONTR(op)->party != NULL &&
        CONTR(pl)->party == CONTR(op)->party) {
        return COLOR_GREEN;
    } else if (pl != op && pvp_area(pl, op)) {
        return COLOR_RED;
    }

    return COLOR_WHITE;
}

/** Return the target-difficulty color used for non-player living names. */
static const char *get_living_level_color(const object *pl, const object *op) {
    if (op->level < level_color[pl->level].yellow) {
        if (op->level < level_color[pl->level].green) {
            return COLOR_GRAY;
        }
        if (op->level < level_color[pl->level].blue) {
            return COLOR_GREEN;
        }
        return COLOR_BLUE;
    }
    if (op->level >= level_color[pl->level].purple) {
        return COLOR_PURPLE;
    }
    if (op->level >= level_color[pl->level].red) {
        return COLOR_RED;
    }
    if (op->level >= level_color[pl->level].orange) {
        return COLOR_ORANGE;
    }
    return COLOR_YELLOW;
}

void packet_append_map_name(struct packet_struct *packet, object *op, object *map_info) {
    packet_debug_data(packet, 0, "Map name");
    packet_append_string(packet, "[b][o=#000000]");
    packet_append_string(packet, map_info && map_info->race ? map_info->race : op->map->name);
    packet_append_string_terminated(packet, "[/o][/b]");
}

void packet_append_map_music(struct packet_struct *packet, object *op, object *map_info) {
    packet_debug_data(packet, 0, "Map music");
    packet_append_string_terminated(packet,
                                    map_info && map_info->slaying
                                        ? map_info->slaying
                                        : (op->map->bg_music ? op->map->bg_music : "no_music"));
}

void packet_append_map_weather(struct packet_struct *packet, object *op, object *map_info) {
    packet_debug_data(packet, 0, "Map weather");
    packet_append_string_terminated(packet,
                                    map_info && map_info->title
                                        ? map_info->title
                                        : (op->map->weather ? op->map->weather : "none"));
}

/**
 * Synchronize the in-game clock with one client or every connected player.
 *
 * The client receives an absolute game-second value and the number of real
 * milliseconds per game minute, allowing it to advance the display without
 * receiving a packet every minute. Seconds preserve the partial minute at
 * login so the first locally advanced minute does not start late.
 *
 * @param recipient
 * Player to synchronize, or NULL to synchronize every player.
 */
void send_game_time(player *recipient) {
    uint64_t game_seconds = (uint64_t)todtick * 60 * 60 +
                            (uint64_t)(pticks % PTICKS_PER_CLOCK) * 60 * 60 / PTICKS_PER_CLOCK;
    uint64_t rate = max_time > 0 && max_time_multiplier > 0
                        ? (uint64_t)PTICKS_PER_CLOCK * (uint64_t)max_time /
                              (UINT64_C(60) * 1000 * (uint64_t)max_time_multiplier)
                        : 1;
    rate = MAX(rate, UINT64_C(1));
    uint32_t millis_per_game_minute = (uint32_t)MIN(UINT32_MAX, MAX(1, rate));

    for (player *pl = recipient != NULL ? recipient : first_player; pl != NULL;
         pl = recipient != NULL ? NULL : pl->next) {
        if (pl->ob == NULL || pl->ob->map == NULL) {
            continue;
        }

        packet_struct *packet = packet_new(CLIENT_CMD_MAPSTATS, 16, 0);
        packet_append_uint8(packet, CMD_MAPSTATS_TIME);
        packet_append_uint64(packet, game_seconds);
        packet_append_uint32(packet, millis_per_game_minute);
        socket_send_packet(pl->cs, packet);
    }
}

/** Clear a map cell. */
#define map_clearcell(_cell_)                                           \
    {                                                                   \
        memset((void *)((char *)(_cell_) + offsetof(MapCell, cleared)), \
               0,                                                       \
               sizeof(MapCell) - offsetof(MapCell, cleared));           \
        (_cell_)->cleared = 1;                                          \
    }

typedef enum map_level_visibility {
    MAP_LEVEL_HIDDEN,
    MAP_LEVEL_WALL_BOUNDARY,
    MAP_LEVEL_BOUNDARY,
    MAP_LEVEL_VISIBLE,
} map_level_visibility;

static bool map_space_blocks_vertical_view(mapstruct *map, int x, int y) {
    if (GET_MAP_FLAGS(map, x, y) & P_BLOCKSVIEW) {
        return true;
    }

    for (object *tmp = GET_MAP_OB(map, x, y); tmp != NULL; tmp = tmp->above) {
        if (QUERY_FLAG(tmp, FLAG_IS_FLOOR)) {
            return true;
        }
    }

    return false;
}

/** Whether one map space contains an explicit roof/camera surface. */
static bool map_space_has_roof(mapstruct *map, int x, int y) {
    for (object *tmp = GET_MAP_OB(map, x, y); tmp != NULL; tmp = tmp->above) {
        /* Roof archetypes deliberately use the hidden wall layer rather than
         * blocksview: they are camera occluders, not gameplay LOS blockers. */
        if (tmp->layer == LAYER_WALL && QUERY_FLAG(tmp, FLAG_HIDDEN)) {
            return true;
        }
    }

    return false;
}

static bool map_space_blocks_camera_view(mapstruct *map, int x, int y) {
    return map_space_blocks_vertical_view(map, x, y) || map_space_has_roof(map, x, y);
}

static bool map_level_layer_visible(map_level_visibility visibility, int layer) {
    if (visibility == MAP_LEVEL_WALL_BOUNDARY) {
        return layer == LAYER_WALL;
    }

    if (visibility == MAP_LEVEL_BOUNDARY) {
        return layer == LAYER_FLOOR || layer == LAYER_FMASK || layer == LAYER_WALL;
    }

    return visibility == MAP_LEVEL_VISIBLE;
}

static bool map_space_has_client_content(MapSpace *msp, map_level_visibility visibility) {
    for (int layer = LAYER_FLOOR; layer <= NUM_LAYERS; layer++) {
        if (!map_level_layer_visible(visibility, layer)) {
            continue;
        }

        for (int sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
            if (GET_MAP_SPACE_LAYER(msp, layer, sub_layer) != NULL) {
                return true;
            }
        }
    }

    return false;
}

/** Whether a map space contains structural wall content. */
static bool map_space_has_wall(MapSpace *msp) {
    for (int sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
        if (GET_MAP_SPACE_LAYER(msp, LAYER_WALL, sub_layer) != NULL) {
            return true;
        }
    }

    return false;
}

/** Return the nonnegative ground elevation supporting linked upper levels. */
static int16_t map_space_support_height(MapSpace *msp) {
    int16_t support_height = 0;

    for (int sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
        object *floor = GET_MAP_SPACE_LAYER(msp, LAYER_FLOOR, sub_layer);
        if (floor == NULL) {
            continue;
        }

        object *head = floor->head != NULL ? floor->head : floor;
        support_height = MAX(support_height, head->z);
    }

    return support_height;
}

/** Resolve one linked map depth without applying camera visibility. */
static mapstruct *map_resolve_raw_level(mapstruct *base, int depth) {
    mapstruct *level = base;
    int direction = depth > 0 ? TILED_UP : TILED_DOWN;

    for (int current_depth = depth > 0 ? 1 : -1; current_depth != depth + (depth > 0 ? 1 : -1);
         current_depth += depth > 0 ? 1 : -1) {
        level = get_map_from_tiled(level, direction);
        if (level == NULL || !MAP_TILE_IS_SAME_LEVEL(base, current_depth)) {
            return NULL;
        }
    }

    return level;
}

/** Resolve one physical linked level and enforce vertical information occlusion. */
static map_level_visibility
map_resolve_level(mapstruct *base, int x, int y, int depth, mapstruct **resolved) {
    mapstruct *level = base;

    if (depth > 0) {
        level = map_resolve_raw_level(base, depth);
        if (level == NULL) {
            return MAP_LEVEL_HIDDEN;
        }

        *resolved = level;
        map_level_visibility visibility =
            map_space_blocks_camera_view(level, x, y) ? MAP_LEVEL_BOUNDARY : MAP_LEVEL_VISIBLE;

        /* The camera looks down through the stack. A surface above the target
         * hides the target; a floor or wall below it must never hide a roof or
         * higher storey. */
        for (int current_depth = depth + 1; current_depth <= MAP2_MAX_DEPTH; current_depth++) {
            level = get_map_from_tiled(level, TILED_UP);
            if (level == NULL || !MAP_TILE_IS_SAME_LEVEL(base, current_depth)) {
                break;
            }

            if (map_space_blocks_camera_view(level, x, y)) {
                /* Keep only the target storey's exterior wall. The covering
                 * roof hides its floor and floor mask as well as enclosed
                 * objects; sending those horizontal surfaces makes them cut
                 * holes through the roof in projected painter order. */
                return MAP_LEVEL_WALL_BOUNDARY;
            }
        }

        return visibility;
    } else if (depth < 0) {
        for (int current_depth = -1; current_depth >= depth; current_depth--) {
            if (map_space_blocks_vertical_view(level, x, y)) {
                return MAP_LEVEL_HIDDEN;
            }

            level = get_map_from_tiled(level, TILED_DOWN);
            if (level == NULL || !MAP_TILE_IS_SAME_LEVEL(base, current_depth)) {
                return MAP_LEVEL_HIDDEN;
            }
        }
    }

    *resolved = level;
    return MAP_LEVEL_VISIBLE;
}

/**
 * Find connected overhead components covering the player.
 *
 * These components are omitted as a camera cutaway while the player is inside
 * them. The relationship comes entirely from solid floors, opaque cells, and
 * roof objects on the linked maps; no authored building flags are required.
 */
static void map_build_level_cutaway(object *pl,
                                    bool cutaway[MAP2_LEVELS][MAP_CLIENT_X][MAP_CLIENT_Y]) {
    bool blockers[MAP_CLIENT_X][MAP_CLIENT_Y];
    bool walls[MAP_CLIENT_X][MAP_CLIENT_Y];
    bool boundary[MAP_CLIENT_X][MAP_CLIENT_Y];
    int queue_x[MAP_CLIENT_X * MAP_CLIENT_Y];
    int queue_y[MAP_CLIENT_X * MAP_CLIENT_Y];

    memset(cutaway, 0, sizeof(bool) * MAP2_LEVELS * MAP_CLIENT_X * MAP_CLIENT_Y);

    for (int depth = 1; depth <= MAP2_MAX_DEPTH; depth++) {
        mapstruct *center_level = map_resolve_raw_level(pl->map, depth);
        if (center_level == NULL || !map_space_blocks_camera_view(center_level, pl->x, pl->y)) {
            continue;
        }

        memset(blockers, 0, sizeof(blockers));
        memset(walls, 0, sizeof(walls));

        for (int ay = CONTR(pl)->cs->mapy - 1, y = (pl->y + (CONTR(pl)->cs->mapy + 1) / 2) - 1;
             y >= pl->y - CONTR(pl)->cs->mapy_2;
             y--, ay--) {
            int ax = CONTR(pl)->cs->mapx - 1;

            for (int x = (pl->x + (CONTR(pl)->cs->mapx + 1) / 2) - 1;
                 x >= pl->x - CONTR(pl)->cs->mapx_2;
                 x--, ax--) {
                int nx = x;
                int ny = y;
                mapstruct *base = get_map_from_coord(pl->map, &nx, &ny);
                if (base == NULL) {
                    continue;
                }

                mapstruct *level = map_resolve_raw_level(base, depth);
                if (level != NULL) {
                    blockers[ax][ay] = map_space_blocks_camera_view(level, nx, ny);
                    walls[ax][ay] = map_space_has_wall(GET_MAP_SPACE_PTR(level, nx, ny));
                }
            }
        }

        int center_x = CONTR(pl)->cs->mapx_2;
        int center_y = CONTR(pl)->cs->mapy_2;

        size_t queue_first = 0;
        size_t queue_last = 0;
        size_t depth_index = MAP2_DEPTH_INDEX(depth);
        queue_x[queue_last] = center_x;
        queue_y[queue_last++] = center_y;
        cutaway[depth_index][center_x][center_y] = true;

        static const int neighbors_x[] = {-1, 1, 0, 0};
        static const int neighbors_y[] = {0, 0, -1, 1};
        while (queue_first < queue_last) {
            int current_x = queue_x[queue_first];
            int current_y = queue_y[queue_first++];

            for (size_t neighbor = 0; neighbor < arraysize(neighbors_x); neighbor++) {
                int next_x = current_x + neighbors_x[neighbor];
                int next_y = current_y + neighbors_y[neighbor];
                if (next_x < 0 || next_x >= CONTR(pl)->cs->mapx || next_y < 0 ||
                    next_y >= CONTR(pl)->cs->mapy || !blockers[next_x][next_y] ||
                    cutaway[depth_index][next_x][next_y]) {
                    continue;
                }

                cutaway[depth_index][next_x][next_y] = true;
                queue_x[queue_last] = next_x;
                queue_y[queue_last++] = next_y;
            }
        }

        /* Perimeter walls commonly occupy a neighboring cell without a floor
         * of their own. Include the one-cell structural boundary around the
         * connected overhead component; otherwise those upper-storey walls
         * remain as a floating lattice after the roof is cut away. Keep the
         * expansion non-recursive so it cannot consume a separate building
         * through a chain of nearby wall cells. */
        memset(boundary, 0, sizeof(boundary));
        for (int ay = 0; ay < CONTR(pl)->cs->mapy; ay++) {
            for (int ax = 0; ax < CONTR(pl)->cs->mapx; ax++) {
                if (!walls[ax][ay] || cutaway[depth_index][ax][ay]) {
                    continue;
                }

                for (int offset_y = -1; offset_y <= 1 && !boundary[ax][ay]; offset_y++) {
                    for (int offset_x = -1; offset_x <= 1; offset_x++) {
                        int neighbor_x = ax + offset_x;
                        int neighbor_y = ay + offset_y;
                        if (neighbor_x >= 0 && neighbor_x < CONTR(pl)->cs->mapx &&
                            neighbor_y >= 0 && neighbor_y < CONTR(pl)->cs->mapy &&
                            cutaway[depth_index][neighbor_x][neighbor_y]) {
                            boundary[ax][ay] = true;
                            break;
                        }
                    }
                }
            }
        }

        for (int ay = 0; ay < CONTR(pl)->cs->mapy; ay++) {
            for (int ax = 0; ax < CONTR(pl)->cs->mapx; ax++) {
                cutaway[depth_index][ax][ay] |= boundary[ax][ay];
            }
        }
    }
}

/**
 * Whether a structural column has a higher roof visible to the camera.
 *
 * Roof components can be disconnected even when their supporting walls and
 * interior floors form one connected component. Cutting the latter as a whole
 * must not leave a retained roof floating without its wall layer.
 */
static bool
map_column_has_visible_roof(mapstruct *base,
                            int x,
                            int y,
                            int depth,
                            int ax,
                            int ay,
                            const bool cutaway[MAP2_LEVELS][MAP_CLIENT_X][MAP_CLIENT_Y]) {
    for (int roof_depth = depth + 1; roof_depth <= MAP2_MAX_DEPTH; roof_depth++) {
        mapstruct *roof_map = map_resolve_raw_level(base, roof_depth);
        if (roof_map == NULL) {
            break;
        }

        if (!cutaway[MAP2_DEPTH_INDEX(roof_depth)][ax][ay] && map_space_has_roof(roof_map, x, y)) {
            return true;
        }
    }

    return false;
}

/** Clear a map cell, but only if it has not been cleared before. */
#define map_if_clearcell(_hard_)                                                                  \
    {                                                                                             \
        MapCell *cached = map_client_cache_cell(&CONTR(pl)->cs->lastmap, depth, ax, ay, false);   \
        if (cached != NULL && cached->cleared != 1) {                                             \
            packet_debug_data(packet, 0, "Clearing tile %d,%d, mask", ax, ay);                    \
            packet_append_uint16(packet,                                                          \
                                 mask | MAP2_MASK_CLEAR | ((_hard_) ? MAP2_MASK_HARD_CLEAR : 0)); \
            map_clearcell(cached);                                                                \
            level_present = true;                                                                 \
        }                                                                                         \
    }

/** Send changed base geometry without disclosing any drawable object layer. */
static bool map_append_support_height(packet_struct *packet,
                                      MapCell *cached,
                                      uint16_t mask,
                                      int16_t support_height) {
    bool support_changed =
        !cached->support_height_known || cached->support_height != support_height;
    if (!support_changed) {
        return false;
    }

    packet_debug_data(packet, 0, "Tile structural support height, mask");
    packet_append_uint16(packet, mask | MAP2_MASK_SUPPORT_HEIGHT);
    packet_debug_data(packet, 1, "Structural support height");
    packet_append_int16(packet, support_height);
    packet_debug_data(packet, 1, "Number of layers");
    packet_append_uint8(packet, 0);
    packet_debug_data(packet, 1, "Extended tile flags");
    packet_append_uint8(packet, 0);

    cached->support_height = support_height;
    cached->support_height_known = 1;
    return true;
}

/** Draw the client map. */
void draw_client_map2(object *pl) {
    static uint32_t map2_count = 0;
    MapCell *mp;
    MapSpace *msp;
    mapstruct *m;
    int x, y, ax, ay, d, nx, ny;
    int blocksview;
    int special_vision;
    uint16_t mask;
    int layer, raw_light[NUM_SUB_LAYERS], light_set[NUM_SUB_LAYERS];
    uint8_t light_level[NUM_SUB_LAYERS];
    int ext_flags, anim_num;
    int num_layers;
    object *tmp, *tmp2;
    uint8_t have_sound_ambient;
    packet_struct *packet, *packet_header, *packet_levels, *packet_layer, *packet_sound;
    int sub_layer, socket_layer;
    packet_save_t packet_save_buf;

    /* Any kind of special vision? */
    special_vision =
        (QUERY_FLAG(pl, FLAG_XRAYS) ? 1 : 0) | (QUERY_FLAG(pl, FLAG_SEE_IN_DARK) ? 2 : 0);
    map2_count++;

    /* Non-player name colors are relative to the viewer's level. Invalidate
     * the delta cache when that level changes so every visible label is sent
     * again with its newly authoritative color. */
    if (!CONTR(pl)->cs->lastmap_player_level_known ||
        CONTR(pl)->cs->lastmap_player_level != pl->level) {
        map_client_cache_clear(&CONTR(pl)->cs->lastmap);
        CONTR(pl)->cs->lastmap_player_level = pl->level;
        CONTR(pl)->cs->lastmap_player_level_known = true;
    }

    packet = packet_new(CLIENT_CMD_MAP, 0, 512);
    packet_sound = packet_new(CLIENT_CMD_SOUND_AMBIENT, 0, 256);

    packet_enable_ndelay(packet);
    packet_debug_data(packet, 0, "Map update command type");
    packet_append_uint8(packet, CONTR(pl)->map_update_cmd);

    if (CONTR(pl)->map_update_cmd != MAP_UPDATE_CMD_SAME) {
        object *map_info;
        const region_struct *region;

        msp = GET_MAP_SPACE_PTR(pl->map, pl->x, pl->y);
        map_info = msp->map_info && OBJECT_VALID(msp->map_info, msp->map_info_count) ? msp->map_info
                                                                                     : NULL;

        packet_append_map_name(packet, pl, map_info);
        packet_append_map_music(packet, pl, map_info);
        packet_append_map_weather(packet, pl, map_info);
        packet_debug_data(packet, 0, "Map height diff");
        packet_append_uint8(packet, MAP_HEIGHT_DIFF(pl->map) != 0);

        if (map_info) {
            if (map_info->race) {
                strncpy(CONTR(pl)->map_info_name,
                        map_info->race,
                        sizeof(CONTR(pl)->map_info_name) - 1);
                CONTR(pl)->map_info_name[sizeof(CONTR(pl)->map_info_name) - 1] = '\0';
            }

            if (map_info->slaying) {
                strncpy(CONTR(pl)->map_info_music,
                        map_info->slaying,
                        sizeof(CONTR(pl)->map_info_music) - 1);
                CONTR(pl)->map_info_music[sizeof(CONTR(pl)->map_info_music) - 1] = '\0';
            }

            if (map_info->title) {
                strncpy(CONTR(pl)->map_info_weather,
                        map_info->title,
                        sizeof(CONTR(pl)->map_info_weather) - 1);
                CONTR(pl)->map_info_weather[sizeof(CONTR(pl)->map_info_weather) - 1] = '\0';
            }
        }

        region = pl->map->region != NULL ? region_find_with_map(pl->map->region) : NULL;

        if (CONTR(pl)->cs->socket_version >= 1059) {
            bool has_map;

            has_map = false;

            /* TODO: This should be improved once maps have a real basename */
            if (region != NULL && (pl->map->region->map_first != NULL || pl->map->coords[2] >= 0)) {
                const char *basename, *underscore, *basename_region;

                basename = strrchr(pl->map->path, '/') + 1;
                underscore = basename ? strchr(basename, '_') : NULL;
                basename_region = strrchr(region->map_first, '/') + 1;

                if (basename != NULL && basename_region != NULL && underscore != NULL &&
                    strncmp(basename, basename_region, underscore - basename) == 0) {
                    has_map = true;
                }
            }

            packet_debug_data(packet, 0, "Display region map");
            packet_append_uint8(packet, has_map);
            packet_debug_data(packet, 0, "Region name");
            packet_append_string_terminated(packet, region != NULL ? region->name : "");
            packet_debug_data(packet, 0, "Region long name");
            packet_append_string_terminated(packet,
                                            region != NULL ? region_get_longname(region) : "");
            packet_debug_data(packet, 0, "Map path");
            packet_append_string_terminated(packet, pl->map->path);
        }

        if (CONTR(pl)->map_update_cmd == MAP_UPDATE_CMD_CONNECTED) {
            packet_debug_data(packet, 0, "Map update tile");
            packet_append_uint8(packet, CONTR(pl)->map_update_tile);
            packet_debug_data(packet, 0, "Map X offset");
            packet_append_int8(packet, CONTR(pl)->map_off_x);
            packet_debug_data(packet, 0, "Map Y offset");
            packet_append_int8(packet, CONTR(pl)->map_off_y);
            packet_debug_data(packet, 0, "Map depth offset");
            packet_append_int8(packet, CONTR(pl)->map_off_z);
        } else {
            packet_debug_data(packet, 0, "Map width");
            packet_append_uint8(packet, pl->map->width);
            packet_debug_data(packet, 0, "Map height");
            packet_append_uint8(packet, pl->map->height);
        }
    }

    packet_debug_data(packet, 0, "Player's X coordinate");
    packet_append_uint8(packet, pl->x);
    packet_debug_data(packet, 0, "Player's Y coordinate");
    packet_append_uint8(packet, pl->y);
    packet_debug_data(packet, 0, "Player's sub-layer");
    packet_append_uint8(packet, pl->sub_layer);

    packet_header = packet;
    packet_levels = packet_new(0, 0, 512);
    uint8_t level_count = 0;
    bool level_cutaway[MAP2_LEVELS][MAP_CLIENT_X][MAP_CLIENT_Y];
    map_build_level_cutaway(pl, level_cutaway);

    for (int depth = -MAP2_MAX_DEPTH; depth <= MAP2_MAX_DEPTH; depth++) {
        bool level_present = false;
        packet = packet_new(0, 0, 512);

        for (ay = CONTR(pl)->cs->mapy - 1, y = (pl->y + (CONTR(pl)->cs->mapy + 1) / 2) - 1;
             y >= pl->y - CONTR(pl)->cs->mapy_2;
             y--, ay--) {
            ax = CONTR(pl)->cs->mapx - 1;

            for (x = (pl->x + (CONTR(pl)->cs->mapx + 1) / 2) - 1;
                 x >= pl->x - CONTR(pl)->cs->mapx_2;
                 x--, ax--) {
                d = CONTR(pl)->blocked_los[ax][ay];
                /* Form the data packet for x and y positions. */
                mask = (ax & 0x1f) << 11 | (ay & 0x1f) << 6;
                /* Space is out of map or blocked. Update space and clear values if
                 * needed. */
                if (d & BLOCKED_LOS_OUT_OF_MAP) {
                    map_if_clearcell(false);
                    continue;
                }

                nx = x;
                ny = y;

                if (!(m = get_map_from_coord(pl->map, &nx, &ny))) {
                    map_if_clearcell(false);
                    continue;
                }

                mapstruct *base_map = m;
                mapstruct *level_map = NULL;
                map_level_visibility level_visibility =
                    map_resolve_level(m, nx, ny, depth, &level_map);
                if (level_visibility == MAP_LEVEL_HIDDEN) {
                    map_if_clearcell(depth != 0);
                    continue;
                }

                m = level_map;
                msp = GET_MAP_SPACE_PTR(m, nx, ny);
                bool visible_roof_above =
                    depth >= 0 &&
                    map_column_has_visible_roof(base_map, nx, ny, depth, ax, ay, level_cutaway);

                if (depth > 0 && level_cutaway[MAP2_DEPTH_INDEX(depth)][ax][ay]) {
                    if (visible_roof_above &&
                        map_space_has_client_content(msp, MAP_LEVEL_BOUNDARY)) {
                        level_visibility = MAP_LEVEL_BOUNDARY;
                    } else {
                        map_if_clearcell(true);
                        continue;
                    }
                }

                blocksview = d & BLOCKED_LOS_BLOCKED;
                bool tile_fow = false;
                /* A lower level behind gameplay LOS cannot disclose content.
                 * It has no structural shell that the top-down camera needs to
                 * preserve, so discard the complete cached cell. */
                if (depth < 0 && blocksview) {
                    map_if_clearcell(true);
                    continue;
                }

                /* Base-level LOS uses the normal explored-map cache: a cell
                 * that has never been sent remains empty, while previously
                 * seen contents are retained and rendered as grayscale FOW. */
                if (depth == 0 && blocksview) {
                    if (visible_roof_above) {
                        level_visibility = MAP_LEVEL_BOUNDARY;
                        tile_fow = true;
                    } else {
                        map_if_clearcell(false);
                        mp = map_client_cache_cell(&CONTR(pl)->cs->lastmap, depth, ax, ay, true);
                        if (map_append_support_height(packet,
                                                      mp,
                                                      mask,
                                                      map_space_support_height(msp))) {
                            level_present = true;
                        }
                        continue;
                    }
                }

                /* Gameplay LOS protects actors, items, and effects at and
                 * above the player's level. Positive depths retain floors,
                 * masks, and walls so clearing their complete cells does not
                 * slice roofs into the base level's LOS silhouette. */
                if (depth > 0 && blocksview) {
                    if (level_visibility == MAP_LEVEL_VISIBLE || visible_roof_above) {
                        level_visibility = MAP_LEVEL_BOUNDARY;
                    }
                    tile_fow = true;
                }

                if (!map_space_has_client_content(msp, level_visibility)) {
                    map_if_clearcell(false);
                    if (depth == 0) {
                        mp = map_client_cache_cell(&CONTR(pl)->cs->lastmap, depth, ax, ay, true);
                        if (map_append_support_height(packet,
                                                      mp,
                                                      mask,
                                                      map_space_support_height(msp))) {
                            level_present = true;
                        }
                    }
                    continue;
                }

                level_present = true;
                mp = map_client_cache_cell(&CONTR(pl)->cs->lastmap, depth, ax, ay, true);

                /* Check whether there is ambient sound effect on this tile. */
                have_sound_ambient = msp->sound_ambient &&
                                     OBJECT_VALID(msp->sound_ambient, msp->sound_ambient_count);

                /* If there is an ambient sound effect but it cannot be heard
                 * through walls due to its configuration, we will pretend
                 * there is no sound effect here. */
                if (have_sound_ambient &&
                    ((!QUERY_FLAG(msp->sound_ambient, FLAG_XRAYS) && d & BLOCKED_LOS_BLOCKED) ||
                     !sound_ambient_match(msp->sound_ambient))) {
                    have_sound_ambient = 0;
                }

                /* If there is an ambient sound effect and we haven't sent it
                 * before, or there isn't one but it was sent before, send an
                 * update. */
                if (depth == 0 &&
                    ((have_sound_ambient && mp->sound_ambient_count != msp->sound_ambient->count) ||
                     (!have_sound_ambient && mp->sound_ambient_count))) {
                    packet_debug(packet_sound, 0, "\nSound tile data:");
                    packet_debug_data(packet_sound, 1, "X coordinate");
                    packet_append_uint8(packet_sound, ax);
                    packet_debug_data(packet_sound, 1, "Y coordinate");
                    packet_append_uint8(packet_sound, ay);
                    packet_debug_data(packet_sound, 1, "Last sound object ID");
                    packet_append_uint32(packet_sound, mp->sound_ambient_count);
                    packet_debug_data(packet_sound, 1, "Sound object ID");

                    if (have_sound_ambient) {
                        packet_append_uint32(packet_sound, msp->sound_ambient->count);
                        packet_debug_data(packet_sound, 1, "Sound filename");
                        packet_append_string_terminated(packet_sound, msp->sound_ambient->race);
                        packet_debug_data(packet_sound, 1, "Volume");
                        packet_append_uint8(packet_sound, msp->sound_ambient->item_condition);
                        packet_debug_data(packet_sound, 1, "Max range");
                        packet_append_uint8(packet_sound, msp->sound_ambient->item_level);

                        mp->sound_ambient_count = msp->sound_ambient->count;
                    } else {
                        packet_append_uint32(packet_sound, 0);

                        mp->sound_ambient_count = 0;
                    }
                }

                /* Any map_if_clearcell() calls should go above this line. */
                mp->cleared = 0;

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

                for (sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
                    light_set[sub_layer] = 0;
                    light_level[sub_layer] = 0;
                }

                /* Initialize default values for some variables. */
                ext_flags = 0;
                packet_save(packet, &packet_save_buf);
                anim_num = 0;
                uint8_t anim_type[NUM_SUB_LAYERS] = {0};
                int16_t anim_value[NUM_SUB_LAYERS] = {0};

                packet_layer = packet_new(0, 0, 128);
                num_layers = 0;

                if (depth == 0) {
                    int16_t support_height = map_space_support_height(msp);
                    if (!mp->support_height_known || mp->support_height != support_height) {
                        mask |= MAP2_MASK_SUPPORT_HEIGHT;
                    }
                }
                if (!mp->fow_known || (mp->fow != 0) != tile_fow) {
                    mask |= MAP2_MASK_FOW;
                }

                /* Go through the visible layers. */
                for (layer = LAYER_FLOOR; layer <= NUM_LAYERS; layer++) {
                    for (sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
                        tmp = GET_MAP_SPACE_LAYER(msp, layer, sub_layer);

                        if (!map_level_layer_visible(level_visibility, layer)) {
                            tmp = NULL;
                        }

                        /* This is done so that the player image is always shown
                         * to the player, even if they are standing on top of
                         * another
                         * player or monster. */
                        if (depth == 0 && tmp != NULL && layer == pl->layer &&
                            sub_layer == pl->sub_layer && pl->x == nx && pl->y == ny) {
                            tmp = pl;
                        }

                        /* If the object is invisible but the player cannot see
                         * invisible tiles, attempt to find a different object
                         * that is not invisible on the same layer and sub-layer. */
                        if (tmp != NULL && QUERY_FLAG(tmp, FLAG_IS_INVISIBLE) &&
                            !QUERY_FLAG(pl, FLAG_SEE_INVISIBLE)) {
                            for (tmp2 = tmp, tmp = NULL; tmp2 != NULL && tmp2->layer == layer &&
                                                         tmp2->sub_layer == sub_layer;
                                 tmp2 = tmp2->above) {
                                if (!QUERY_FLAG(tmp2, FLAG_IS_INVISIBLE)) {
                                    tmp = tmp2;
                                    break;
                                }
                            }
                        }

                        if (tmp != NULL && tmp->layer != LAYER_WALL &&
                            QUERY_FLAG(tmp, FLAG_HIDDEN)) {
                            tmp = NULL;
                        }

                        /* Handle objects that are shown based on their direction
                         * and the player's position. */
                        if (tmp && QUERY_FLAG(tmp, FLAG_DRAW_DIRECTION)) {
                            /* If the object is dir [0124568] and not in the top
                             * or right quadrant or on the central square, do not
                             * show it. */
                            if ((!tmp->direction || tmp->direction == NORTH ||
                                 tmp->direction == NORTHEAST || tmp->direction == SOUTHEAST ||
                                 tmp->direction == SOUTH || tmp->direction == SOUTHWEST ||
                                 tmp->direction == NORTHWEST) &&
                                !((ax <= CONTR(pl)->cs->mapx_2) && (ay <= CONTR(pl)->cs->mapy_2)) &&
                                !((ax > CONTR(pl)->cs->mapx_2) && (ay < CONTR(pl)->cs->mapy_2))) {
                                tmp = NULL;
                            } else if ((!tmp->direction || tmp->direction == NORTHEAST ||
                                        tmp->direction == EAST || tmp->direction == SOUTHEAST ||
                                        tmp->direction == SOUTHWEST || tmp->direction == WEST ||
                                        tmp->direction == NORTHWEST) &&
                                       !((ax <= CONTR(pl)->cs->mapx_2) &&
                                         (ay <= CONTR(pl)->cs->mapy_2)) &&
                                       !((ax < CONTR(pl)->cs->mapx_2) &&
                                         (ay > CONTR(pl)->cs->mapy_2))) {
                                /* If the object is dir [0234768] and not in the top
                                 * or left quadrant or on the central square, do not
                                 * show it. */
                                tmp = NULL;
                            }
                        }

                        if (tmp != NULL &&
                            (!light_set[sub_layer] || (layer == LAYER_EFFECT && sub_layer > 0))) {
                            light_set[sub_layer] = 1;
                            raw_light[sub_layer] = map_get_darkness(tmp->map, tmp->x, tmp->y, NULL);

                            if (CONTR(pl)->tli) {
                                raw_light[sub_layer] += global_darkness_table[MAX_DARKNESS];
                            }

                            if (raw_light[sub_layer] < 100) {
                                if (QUERY_FLAG(tmp, FLAG_HIDDEN) || special_vision & 1) {
                                    raw_light[sub_layer] = 100;
                                }
                            }

                            light_level[sub_layer] = light_level_from_raw(raw_light[sub_layer]);
                        }

                        if (tmp != NULL && raw_light[sub_layer] <= 0) {
                            tmp = NULL;
                        }

                        if (tmp != NULL && tmp->map != m && anim_type[sub_layer] == 0 &&
                            GET_MAP_RTAG(tmp->map, tmp->x, tmp->y, sub_layer) == global_round_tag) {
                            anim_type[sub_layer] = ANIM_KILL;
                            anim_value[sub_layer] =
                                GET_MAP_DAMAGE(tmp->map, tmp->x, tmp->y, sub_layer);
                            anim_num++;
                        }

                        socket_layer = NUM_LAYERS * sub_layer + layer - 1;

                        /* Found something. */
                        if (tmp) {
                            int16_t face;
                            uint8_t quick_pos = tmp->quick_pos;
                            uint8_t flags = 0, probe_val = 0;
                            uint32_t flags2 = 0;
                            object *head = tmp->head ? tmp->head : tmp, *face_obj;
                            tag_t target_object_count = 0;
                            uint8_t anim_speed, anim_facing, anim_flags;
                            uint8_t client_flags;
                            int is_friend = 0;
                            uint8_t is_roof = 0;
                            uint8_t is_door = 0;

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

                            client_flags = GET_CLIENT_FLAGS(head);

                            /* Keep every visible living creature identified. */
                            if (head->type == PLAYER || (layer == LAYER_LIVING && IS_LIVE(head))) {
                                flags |= MAP2_FLAG_NAME;
                            }

                            /* If our player has this object as their target, we
                             * want to
                             * know its HP percent. */
                            if (head->count == CONTR(pl)->target_object_count) {
                                flags2 |= MAP2_FLAG2_PROBE;
                                probe_val = MAX(
                                    1,
                                    ((double)head->stats.hp / ((double)head->stats.maxhp / 100.0)));
                            }

                            /* Z position set? */
                            if (head->z != 0) {
                                flags |= MAP2_FLAG_HEIGHT;
                            }

                            if (QUERY_FLAG(pl, FLAG_SEE_IN_DARK) &&
                                ((head->layer == LAYER_LIVING && raw_light[sub_layer] < 150) ||
                                 (head->type == CONTAINER &&
                                  head->sub_type == ST1_CONTAINER_CORPSE &&
                                  QUERY_FLAG(head, FLAG_IS_USED_UP) &&
                                  (float)head->stats.food / head->last_eat >=
                                      CORPSE_INFRAVISION_PERCENT / 100.0))) {
                                flags |= MAP2_FLAG_INFRAVISION;
                            }

                            if (head->align) {
                                flags |= MAP2_FLAG_ALIGN;
                            }

                            /* The unified stacked-map painter keeps tall wall
                             * faces in the correct global order. Omitting the
                             * upper copy in the player's lower quadrant now
                             * creates holes in building facades instead of
                             * providing the old directional camera cutaway. */
                            if (QUERY_FLAG(tmp, FLAG_DRAW_DOUBLE) ||
                                QUERY_FLAG(tmp, FLAG_DRAW_DOUBLE_ALWAYS)) {
                                flags |= MAP2_FLAG_DOUBLE;
                            }

                            if (head->alpha) {
                                flags2 |= MAP2_FLAG2_ALPHA;
                            }

                            if (head->rotate) {
                                flags2 |= MAP2_FLAG2_ROTATE;
                            }

                            /* Check if the object has zoom. */
                            if ((head->zoom_x && head->zoom_x != 100) ||
                                (head->zoom_y && head->zoom_y != 100)) {
                                flags2 |= MAP2_FLAG2_ZOOM;
                            }

                            if (head != pl && layer == LAYER_LIVING && IS_LIVE(head)) {
                                flags2 |= MAP2_FLAG2_TARGET;
                                target_object_count = head->count;
                                is_friend = is_friend_of(pl, head);
                            }

                            if (head->type == DOOR || layer == LAYER_LIVING) {
                                flags2 |= MAP2_FLAG2_SECONDPASS;
                            }

                            if (head->type == DOOR) {
                                flags2 |= MAP2_FLAG2_DOOR;
                                is_door = 1;
                            }

                            if (head->glow != NULL && CONTR(pl)->cs->socket_version >= 1060) {
                                flags2 |= MAP2_FLAG2_GLOW;
                            }

                            if (layer == LAYER_WALL && QUERY_FLAG(head, FLAG_HIDDEN)) {
                                flags2 |= MAP2_FLAG2_ROOF;
                                is_roof = 1;
                            }

                            if (flags2) {
                                flags |= MAP2_FLAG_MORE;
                            }

                            /* Damage animation? Store it for later. */
                            if (tmp->last_damage && tmp->damage_round_tag == global_round_tag) {
                                if (anim_type[sub_layer] == 0) {
                                    anim_num++;
                                }

                                anim_type[sub_layer] = ANIM_DAMAGE;
                                anim_value[sub_layer] = tmp->last_damage;
                            }

                            /* Now, check if we have cached this. */
                            if (mp->faces[socket_layer] == face &&
                                mp->quick_pos[socket_layer] == quick_pos &&
                                mp->flags[socket_layer] == flags &&
                                mp->roof[socket_layer] == is_roof &&
                                mp->door[socket_layer] == is_door &&
                                (layer != LAYER_LIVING || !IS_LIVE(head) ||
                                 (mp->probe == probe_val &&
                                  mp->target_object_count == target_object_count)) &&
                                mp->anim_speed[socket_layer] == anim_speed &&
                                mp->anim_facing[socket_layer] == anim_facing &&
                                (layer != LAYER_LIVING ||
                                 (mp->anim_flags[sub_layer] == anim_flags &&
                                  mp->client_flags[sub_layer] == client_flags)) &&
                                (!(flags & MAP2_FLAG_NAME) || head->type != PLAYER ||
                                 !CONTR(head)->cs->ext_title_flag) &&
                                (!(flags2 & MAP2_FLAG2_TARGET) ||
                                 ((mp->is_friend & (1 << sub_layer)) != 0) == is_friend)) {
                                continue;
                            }

                            /* Different from cache, add it to the cache now. */
                            mp->faces[socket_layer] = face;
                            mp->quick_pos[socket_layer] = quick_pos;
                            mp->flags[socket_layer] = flags;
                            mp->roof[socket_layer] = is_roof;
                            mp->door[socket_layer] = is_door;
                            mp->anim_speed[socket_layer] = anim_speed;
                            mp->anim_facing[socket_layer] = anim_facing;

                            if (layer == LAYER_LIVING) {
                                mp->anim_flags[sub_layer] = anim_flags;
                                mp->client_flags[sub_layer] = client_flags;
                            }

                            if (layer == LAYER_LIVING) {
                                mp->probe = probe_val;
                                mp->target_object_count = target_object_count;

                                if (flags2 & MAP2_FLAG2_TARGET) {
                                    if (is_friend) {
                                        mp->is_friend |= 1 << sub_layer;
                                    } else {
                                        mp->is_friend &= ~(1 << sub_layer);
                                    }
                                }
                            }

                            if (OBJECT_IS_HIDDEN(pl, head)) {
                                /* Update target if applicable. */
                                if (flags2 & MAP2_FLAG2_PROBE) {
                                    CONTR(pl)->target_object = NULL;
                                    CONTR(pl)->target_object_count = 0;
                                    send_target_command(CONTR(pl));
                                }

                                if (mp->faces[socket_layer]) {
                                    packet_debug_data(packet_layer, 1, "Socket layer ID (clear)");
                                    packet_append_uint8(packet_layer, MAP2_LAYER_CLEAR);
                                    packet_debug_data(packet_layer, 1, "Actual socket layer");
                                    packet_append_uint8(packet_layer, socket_layer);
                                    num_layers++;
                                }

                                continue;
                            }

                            num_layers++;

                            packet_debug_data(packet_layer,
                                              1,
                                              "Socket layer (layer: %d, sub-layer: %d)",
                                              layer,
                                              sub_layer);
                            packet_append_uint8(packet_layer, socket_layer);
                            packet_debug_data(packet_layer, 2, "Face ID");
                            packet_append_uint16(packet_layer, face);
                            packet_debug_data(packet_layer, 2, "Client flags");
                            packet_append_uint8(packet_layer, client_flags);
                            packet_debug_data(packet_layer, 2, "Socket flags");
                            packet_append_uint8(packet_layer, flags);

                            /* Multi-arch? Add it's quick pos. */
                            if (flags & MAP2_FLAG_MULTI) {
                                packet_debug_data(packet_layer, 2, "Quick pos");
                                packet_append_uint8(packet_layer, quick_pos);
                            }

                            /* Add a visible living object's name and display color. */
                            if (flags & MAP2_FLAG_NAME) {
                                const char *name_color;
                                char *living_name = NULL;
                                const char *name;

                                if (head->type == PLAYER) {
                                    name = CONTR(head)->quick_name;
                                    name_color = get_playername_color(pl, head);
                                } else {
                                    living_name = object_get_short_name_s(head, pl);
                                    name = living_name;
                                    name_color = get_living_level_color(pl, head);
                                }

                                packet_debug_data(packet_layer, 2, "Living object name");
                                packet_append_string_terminated(packet_layer, name);
                                packet_debug_data(packet_layer, 2, "Living object name color");
                                packet_append_string_terminated(packet_layer, name_color);
                                free(living_name);
                            }

                            if (flags & MAP2_FLAG_ANIMATION) {
                                packet_debug(packet_layer, 2, "Animation\n");
                                packet_debug_data(packet_layer, 3, "Speed");
                                packet_append_uint8(packet_layer, anim_speed);
                                packet_debug_data(packet_layer, 3, "Facing");
                                packet_append_uint8(packet_layer, anim_facing);
                                packet_debug_data(packet_layer, 3, "Flags");
                                packet_append_uint8(packet_layer, anim_flags);

                                if (anim_flags & ANIM_FLAG_MOVING) {
                                    packet_debug_data(packet_layer, 3, "State");
                                    packet_append_uint8(packet_layer, face_obj->state);
                                }
                            }

                            /* Z position. */
                            if (flags & MAP2_FLAG_HEIGHT) {
                                int16_t z;

                                z = head->z;

                                packet_debug_data(packet_layer, 2, "Z");
                                packet_append_int16(packet_layer, z);
                            }

                            if (flags & MAP2_FLAG_ALIGN) {
                                packet_debug_data(packet_layer, 2, "Align");

                                packet_append_int16(packet_layer, head->align);
                            }

                            if (flags & MAP2_FLAG_MORE) {
                                packet_debug(packet_layer, 2, "Extended info:\n");
                                packet_debug_data(packet_layer, 3, "Flags");
                                packet_append_uint32(packet_layer, flags2);

                                if (flags2 & MAP2_FLAG2_ALPHA) {
                                    packet_debug_data(packet_layer, 3, "Alpha");
                                    packet_append_uint8(packet_layer, head->alpha);
                                }

                                if (flags2 & MAP2_FLAG2_ROTATE) {
                                    packet_debug_data(packet_layer, 3, "Rotate");
                                    packet_append_int16(packet_layer, head->rotate);
                                }

                                if (flags2 & MAP2_FLAG2_ZOOM) {
                                    packet_debug_data(packet_layer, 3, "X zoom");
                                    packet_append_uint16(packet_layer, head->zoom_x);
                                    packet_debug_data(packet_layer, 3, "Y zoom");
                                    packet_append_uint16(packet_layer, head->zoom_y);
                                }

                                if (flags2 & MAP2_FLAG2_TARGET) {
                                    packet_debug_data(packet_layer, 3, "Target object ID");
                                    packet_append_uint32(packet_layer, target_object_count);
                                    packet_debug_data(packet_layer, 3, "Target is friend");
                                    packet_append_uint8(packet_layer, is_friend);
                                }

                                /* Target's HP bar. */
                                if (flags2 & MAP2_FLAG2_PROBE) {
                                    packet_debug_data(packet_layer, 3, "HP percentage");
                                    packet_append_uint8(packet_layer, probe_val);
                                }

                                /* Target's HP bar. */
                                if (flags2 & MAP2_FLAG2_GLOW) {
                                    packet_debug_data(packet_layer, 3, "Glow color");
                                    packet_append_string_terminated(packet_layer, head->glow);
                                    packet_debug_data(packet_layer, 3, "Glow speed");
                                    packet_append_uint8(packet_layer, head->glow_speed);
                                }
                            }
                        } else if (mp->faces[socket_layer]) {
                            /* Didn't find anything. Now, if we have previously seen
                             * a face on this layer, we will want the client to
                             * clear it. */
                            mp->faces[socket_layer] = 0;
                            mp->quick_pos[socket_layer] = 0;
                            mp->flags[socket_layer] = 0;
                            mp->roof[socket_layer] = 0;
                            mp->door[socket_layer] = 0;
                            mp->anim_speed[socket_layer] = 0;
                            mp->anim_facing[socket_layer] = 0;

                            if (layer == LAYER_LIVING) {
                                mp->anim_flags[sub_layer] = 0;
                            }

                            packet_debug_data(packet_layer, 1, "Socket layer ID (clear)");
                            packet_append_uint8(packet_layer, MAP2_LAYER_CLEAR);
                            packet_debug_data(packet_layer, 1, "Actual socket layer");
                            packet_append_uint8(packet_layer, socket_layer);
                            num_layers++;
                        }
                    }
                }

                /* A zero-valued sample is meaningful too. Compare the completed
                 * light field independently of whether a dark tile had a drawable
                 * object, and send every visible sub-layer at least once. */
                for (sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
                    uint8_t resolved_light = light_set[sub_layer] ? light_level[sub_layer] : 0;

                    if (!mp->light_known[sub_layer] ||
                        resolved_light != mp->light_level[sub_layer]) {
                        if (sub_layer == 0) {
                            mask |= MAP2_MASK_LIGHT_LEVEL;
                        } else {
                            mask |= MAP2_MASK_LIGHT_LEVEL_MORE;
                        }
                    }
                }

                /* Add the mask. Any mask changes should go above this line. */
                packet_debug_data(packet, 0, "Tile %d,%d data, mask", ax, ay);
                packet_append_uint16(packet, mask);

                if (mask & MAP2_MASK_SUPPORT_HEIGHT) {
                    mp->support_height = map_space_support_height(msp);
                    mp->support_height_known = 1;
                    packet_debug_data(packet, 1, "Structural support height");
                    packet_append_int16(packet, mp->support_height);
                }

                if (mask & MAP2_MASK_FOW) {
                    mp->fow = tile_fow;
                    mp->fow_known = 1;
                    packet_debug_data(packet, 1, "Fog-of-war state");
                    packet_append_uint8(packet, mp->fow);
                }

                for (sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
                    if ((sub_layer == 0 && !(mask & MAP2_MASK_LIGHT_LEVEL)) ||
                        (sub_layer != 0 && !(mask & MAP2_MASK_LIGHT_LEVEL_MORE))) {
                        if (!light_set[sub_layer] && mp->light_level[sub_layer] != 0) {
                            mp->light_level[sub_layer] = 0;
                        }

                        continue;
                    }

                    packet_debug_data(packet, 1, "Light level (sub-layer: %d)", sub_layer);
                    mp->light_level[sub_layer] = light_set[sub_layer] ? light_level[sub_layer] : 0;
                    mp->light_known[sub_layer] = 1;
                    packet_append_uint8(packet, mp->light_level[sub_layer]);
                }

                packet_debug_data(packet, 1, "Number of layers");
                packet_append_uint8(packet, num_layers);

                packet_append_packet(packet, packet_layer);
                packet_free(packet_layer);

                /* Kill animations? */
                for (sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
                    if (GET_MAP_RTAG(m, nx, ny, sub_layer) == global_round_tag) {
                        if (anim_type[sub_layer] == 0) {
                            anim_num++;
                        }

                        anim_type[sub_layer] = ANIM_KILL;
                        anim_value[sub_layer] = GET_MAP_DAMAGE(m, nx, ny, sub_layer);
                    }
                }

                if (anim_num != 0) {
                    ext_flags |= MAP2_FLAG_EXT_ANIM;
                }

                if (ext_flags == mp->ext_flags && anim_num == mp->anim_num && process_delay != 0) {
                    ext_flags = 0;
                } else {
                    mp->ext_flags = ext_flags;
                    mp->anim_num = anim_num;
                }

                /* Add flags for this tile. */
                packet_debug_data(packet, 1, "Extended tile flags");
                packet_append_uint8(packet, ext_flags);

                /* Animation? Add its type and value. */
                if (ext_flags & MAP2_FLAG_EXT_ANIM) {
                    packet_debug_data(packet, 1, "Number of animations");
                    packet_append_uint8(packet, anim_num);

                    for (sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
                        if (anim_type[sub_layer] == 0) {
                            continue;
                        }

                        packet_debug_data(packet, 1, "Animation sub-layer");
                        packet_append_uint8(packet, sub_layer);
                        packet_debug_data(packet, 1, "Animation type");
                        packet_append_uint8(packet, anim_type[sub_layer]);
                        packet_debug_data(packet, 1, "Animation value");
                        packet_append_int16(packet, anim_value[sub_layer]);
                    }
                }

                /* If nothing has really changed, go back to the old position
                 * in the packet. */
                if (!(mask & 0x3f) && !num_layers && !ext_flags) {
                    packet_load(packet, &packet_save_buf);
                }
            }
        }

        if (level_present) {
            packet_debug_data(packet_levels, 0, "Map level depth");
            packet_append_int8(packet_levels, depth);
            packet_debug_data(packet_levels, 0, "Map level payload size");
            packet_append_uint32(packet_levels, packet->len);
            packet_append_packet(packet_levels, packet);
            level_count++;
        }

        packet_free(packet);
    }

    packet_debug_data(packet_header, 0, "Number of map levels");
    packet_append_uint8(packet_header, level_count);
    packet_append_packet(packet_header, packet_levels);
    packet_free(packet_levels);
    bool connected = CONTR(pl)->map_update_cmd == MAP_UPDATE_CMD_CONNECTED;
    socket_send_packet(CONTR(pl)->cs, packet_header);

    if (connected) {
        send_game_time(CONTR(pl));
    }

    if (packet_sound->len >= 1) {
        socket_send_packet(CONTR(pl)->cs, packet_sound);
    } else {
        packet_free(packet_sound);
    }
}

void socket_command_quest_list(socket_struct *ns,
                               player *pl,
                               uint8_t *data,
                               size_t len,
                               size_t pos) {
    object *quest_container, *tmp, *tmp2, *last;
    StringBuffer *sb;
    packet_struct *packet;
    char *cp;
    size_t cp_len;

    quest_container = pl->quest_container;

    if (!quest_container || !quest_container->inv) {
        packet = packet_new(CLIENT_CMD_BOOK, 0, 0);
        packet_debug_data(packet, 0, "Quest list message");
        packet_append_string_terminated(packet, "[title]No quests to speak of.[/title]");
        socket_send_packet(pl->cs, packet);
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
        for (last = tmp->inv; last && last->below; last = last->below) {}

        /* Show the quest parts. */
        for (tmp2 = last; tmp2; tmp2 = tmp2->above) {
            stringbuffer_append_printf(sb, "\n[b]%s[/b]", tmp2->race);

            if (tmp2->msg) {
                stringbuffer_append_printf(sb, ": %s", tmp2->msg);

                if (tmp2->magic == QUEST_STATUS_COMPLETED) {
                    stringbuffer_append_string(sb, " []done]");
                } else if (tmp2->magic == QUEST_STATUS_FAILED) {
                    stringbuffer_append_string(sb, " []failed]");
                }
            }

            switch (tmp2->sub_type) {
                case QUEST_TYPE_KILL:
                    stringbuffer_append_printf(sb,
                                               "\n[x=10]Status: %d/%d",
                                               MIN(tmp2->last_sp, tmp2->last_grace),
                                               tmp2->last_grace);
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
    packet_debug_data(packet, 0, "Quest list message");
    packet_append_string_len(packet, cp, cp_len);
    socket_send_packet(pl->cs, packet);
    free(cp);
}

void socket_command_clear(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos) {
    /* The graphical client sends CLEAR for its Stay action. Besides dropping
     * commands which have not been dispatched yet, cancel movement already
     * expanded into the player's persistent server-side path queue and stop
     * directional run mode. This lets combat or other urgent input reliably
     * interrupt click-to-move. */
    ns->packet_recv_cmd->len = 0;
    if (pl != NULL) {
        player_path_clear(pl);
        pl->run_on = 0;
    }
}

void socket_command_move_path(socket_struct *ns,
                              player *pl,
                              uint8_t *data,
                              size_t len,
                              size_t pos) {
    uint8_t x, y;
    mapstruct *m;
    int xt, yt;
    path_node_t *node, *tmp;

    x = packet_to_uint8(data, len, &pos);
    y = packet_to_uint8(data, len, &pos);

    /* Validate the passed x/y. */
    if (x >= pl->cs->mapx || y >= pl->cs->mapy) {
        LOG(PACKET, "X/Y not in range: %d, %d", x, y);
        return;
    }

    /* If this is the middle of the screen where the player is already,
     * there isn't much to do. */
    if (x == pl->cs->mapx_2 && y == pl->cs->mapy_2) {
        return;
    }

    /* The x/y we got above is from the client's map, so 0,0 is
     * actually topmost (northwest) corner of the map in the client,
     * and not 0,0 of the actual map, so we need to transform it to
     * actual map coordinates. */
    xt = pl->ob->x + (x - pl->cs->mapx / 2);
    yt = pl->ob->y + (y - pl->cs->mapy / 2);
    m = get_map_from_coord(pl->ob->map, &xt, &yt);

    /* Invalid x/y. */
    if (!m) {
        return;
    }

    /* Find and compress the path to the destination. */
    node = path_compress(path_find(pl->ob, pl->ob->map, pl->ob->x, pl->ob->y, m, xt, yt, NULL));

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

void socket_command_fire(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos) {
    int dir;
    tag_t tag;
    object *tmp;
    double skill_time, delay;

    dir = packet_to_uint8(data, len, &pos);
    dir = MAX(0, MIN(dir, 8));
    tag = packet_to_uint32(data, len, &pos);

    if (tag) {
        if (pl->equipment[PLAYER_EQUIP_WEAPON_RANGED] &&
            pl->equipment[PLAYER_EQUIP_WEAPON_RANGED]->count == tag) {
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

        if (!tmp && pl->equipment[PLAYER_EQUIP_AMMO] &&
            QUERY_FLAG(pl->equipment[PLAYER_EQUIP_AMMO], FLAG_IS_THROWN)) {
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

    if (skill_time > 1.0) {
        skill_time -= (SK_level(pl->ob) / 10.0 / 3.0) * 0.1;

        if (skill_time < 1.0) {
            skill_time = 1.0;
        }
    }

    pl->action_attack = global_round_tag + skill_time + delay;

    pl->action_timer = (float)(pl->action_attack - global_round_tag) / MAX_TICKS;
    pl->last_action_timer = 0;
}

void socket_command_keepalive(socket_struct *ns,
                              player *pl,
                              uint8_t *data,
                              size_t len,
                              size_t pos) {
    ns->keepalive = 0;

    if (len == pos) {
        return;
    }

    uint32_t id = packet_to_uint32(data, len, &pos);

    packet_struct *packet = packet_new(CLIENT_CMD_KEEPALIVE, 20, 0);
    packet_enable_ndelay(packet);
    packet_debug_data(packet, 0, "Keepalive ID");
    packet_append_uint32(packet, id);
    socket_send_packet(ns, packet);
}

void socket_command_move(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos) {
    uint8_t dir, run_on;

    dir = packet_to_uint8(data, len, &pos);
    run_on = packet_to_uint8(data, len, &pos);

    if (dir > 8) {
        LOG(PACKET, "%s: Invalid dir: %d", socket_get_id(ns->sc), dir);
        return;
    }

    if (run_on > 1) {
        LOG(PACKET, "%s: Invalid run_on: %d", socket_get_id(ns->sc), run_on);
        return;
    }

    if (run_on == 1 && dir == 0) {
        LOG(PACKET, "%s: run_on is 1 but dir is 0", socket_get_id(ns->sc));
        return;
    }

    pl->run_on = run_on;

    if (dir != 0) {
        pl->run_on_dir = dir - 1;
        pl->ob->speed_left -= 1.0;
        move_object(pl->ob, dir);
    }
}

/**
 * Send target command, calculate the target's color level, etc.
 * @param pl
 * Player requesting this.
 */
void send_target_command(player *pl) {
    packet_struct *packet;

    if (!pl->ob->map) {
        LOG(PACKET, "Received target command while not playing.");
        return;
    }

    packet = packet_new(CLIENT_CMD_TARGET, 64, 64);
    packet_enable_ndelay(packet);

    pl->ob->enemy = NULL;
    pl->ob->enemy_count = 0;

    if (!pl->target_object || pl->target_object == pl->ob ||
        !OBJECT_VALID(pl->target_object, pl->target_object_count) ||
        IS_INVISIBLE(pl->target_object, pl->ob)) {
        packet_debug_data(packet, 0, "Target command type");
        packet_append_uint8(packet, CMD_TARGET_SELF);
        packet_debug_data(packet, 0, "Color");
        packet_append_string_terminated(packet, COLOR_YELLOW);
        packet_debug_data(packet, 0, "Target name");
        packet_append_string_terminated(packet, pl->ob->name);
        packet_debug_data(packet, 0, "Target level");
        packet_append_uint8(packet, pl->ob->level);

        pl->target_object = pl->ob;
        pl->target_object_count = 0;
    } else {
        packet_debug_data(packet, 0, "Target command type");

        if (is_friend_of(pl->target_object, pl->ob)) {
            if (pl->target_object->type == PLAYER) {
                packet_append_uint8(packet, CMD_TARGET_FRIEND);
            } else {
                packet_append_uint8(packet, CMD_TARGET_NEUTRAL);
            }
        } else {
            packet_append_uint8(packet, CMD_TARGET_ENEMY);

            pl->ob->enemy = pl->target_object;
            pl->ob->enemy_count = pl->target_object_count;
        }

        packet_debug_data(packet, 0, "Color");

        packet_append_string_terminated(packet, get_living_level_color(pl->ob, pl->target_object));

        packet_debug_data(packet, 0, "Target name");

        packet_append_string_terminated(packet, pl->target_object->name);
        packet_debug_data(packet, 0, "Target level");
        packet_append_uint8(packet, pl->target_object->level);
    }

    packet_debug_data(packet, 0, "Combat mode");
    packet_append_uint8(packet, pl->combat);
    packet_debug_data(packet, 0, "Combat force mode");
    packet_append_uint8(packet, pl->combat_force);

    pl->last_target_level = pl->target_object->level;
    pl->last_target_viewer_level = pl->ob->level;

    socket_send_packet(pl->cs, packet);
}

void socket_command_account(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos) {
    uint8_t type;

    type = packet_to_uint8(data, len, &pos);

    if (type == CMD_ACCOUNT_LOGIN) {
        char name[MAX_BUF], password[MAX_BUF];

        packet_to_string(data, len, &pos, name, sizeof(name));
        packet_to_string(data, len, &pos, password, sizeof(password));

        if (*name == '\0' || *password == '\0' ||
            string_contains_other(name, settings.allowed_chars[ALLOWED_CHARS_ACCOUNT]) ||
            string_contains_other(password, settings.allowed_chars[ALLOWED_CHARS_PASSWORD])) {
            LOG(PACKET, "Received invalid data in account login command.");
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
    } else {
        LOG(PACKET, "Invalid type: %d", type);
    }
}

/**
 * Generate player's name, as visible on the map.
 * @param pl
 * The player.
 */
void generate_quick_name(player *pl) {
    int i;

    snprintf(pl->quick_name, sizeof(pl->quick_name), "%s", pl->ob->name);

    for (i = 0; i < pl->num_cmd_permissions; i++) {
        if (pl->cmd_permissions[i] && string_startswith(pl->cmd_permissions[i], "[") &&
            string_endswith(pl->cmd_permissions[i], "]")) {
            snprintfcat(pl->quick_name, sizeof(pl->quick_name), " %s", pl->cmd_permissions[i]);
        }
    }

    if (pl->afk) {
        snprintfcat(pl->quick_name, sizeof(pl->quick_name), " [AFK]");
    }
}

void socket_command_target(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos) {
    uint8_t type;

    type = packet_to_uint8(data, len, &pos);

    if (type == CMD_TARGET_MAPXY) {
        uint8_t x, y;
        uint32_t count, target_object_count;
        int i, xt, yt;
        mapstruct *m;
        object *tmp;

        x = packet_to_uint8(data, len, &pos);
        y = packet_to_uint8(data, len, &pos);
        count = packet_to_uint32(data, len, &pos);

        /* Validate the passed x/y. */
        if (x >= pl->cs->mapx || y >= pl->cs->mapy) {
            LOG(PACKET, "Invalid X/Y: %d, %d", x, y);
            return;
        }

        target_object_count = pl->target_object_count;
        pl->target_object = NULL;
        pl->target_object_count = 0;

        for (i = 0; i <= SIZEOFFREE1 && !pl->target_object_count; i++) {
            /* Check whether we are still in range of the player's
             * viewport, and whether the player can see the square. */
            if (x + freearr_x[i] < 0 || x + freearr_x[i] >= pl->cs->mapx || y + freearr_y[i] < 0 ||
                y + freearr_y[i] >= pl->cs->mapy ||
                pl->blocked_los[x + freearr_x[i]][y + freearr_y[i]] > BLOCKED_LOS_BLOCKSVIEW) {
                continue;
            }

            /* The x/y we got above is from the client's map, so 0,0 is
             * actually topmost (northwest) corner of the map in the client,
             * and not 0,0 of the actual map, so we need to transform it to
             * actual map coordinates. */
            xt = pl->ob->x + (x - pl->cs->mapx_2) + freearr_x[i];
            yt = pl->ob->y + (y - pl->cs->mapy_2) + freearr_y[i];
            m = get_map_from_coord(pl->ob->map, &xt, &yt);

            /* Invalid x/y. */
            if (!m) {
                continue;
            }

            /* Nothing alive on this spot. */
            if (!(GET_MAP_FLAGS(m, xt, yt) & (P_IS_MONSTER | P_IS_PLAYER))) {
                continue;
            }

            FOR_MAP_LAYER_BEGIN(m, xt, yt, LAYER_LIVING, -1, tmp) {
                tmp = HEAD(tmp);

                if ((!count || tmp->count == count) && IS_LIVE(tmp) && tmp != pl->ob &&
                    !IS_INVISIBLE(tmp, pl->ob) && !OBJECT_IS_HIDDEN(pl->ob, tmp)) {
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
    } else {
        LOG(PACKET, "Invalid type: %d", type);
    }
}

void socket_command_talk(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos) {
    uint8_t type;
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
                LOG(PACKET, "Empty NPC name.");
                return;
            }
        }

        packet_to_string(data, len, &pos, msg, sizeof(msg));
        player_sanitize_input(msg);

        if (string_isempty(msg)) {
            LOG(PACKET, "Empty message.");
            return;
        }

        npc = NULL;

        if (type == CMD_TALK_NPC && OBJECT_VALID(pl->target_object, pl->target_object_count) &&
            OBJECT_CAN_TALK(pl->target_object)) {
            for (i = 0; i <= SIZEOFFREE2; i++) {
                x = pl->ob->x + freearr_x[i];
                y = pl->ob->y + freearr_y[i];
                m = get_map_from_coord(pl->ob->map, &x, &y);

                if (!m) {
                    continue;
                }

                if (m == pl->target_object->map && x == pl->target_object->x &&
                    y == pl->target_object->y) {
                    npc = pl->target_object;
                    break;
                }
            }
        }

        /* Use larger search space when trying to talk to a specific NPC. */
        for (i = 0; i <= (type == CMD_TALK_NPC ? SIZEOFFREE2 : SIZEOFFREE3) && !npc; i++) {
            x = pl->ob->x + freearr_x[i];
            y = pl->ob->y + freearr_y[i];
            m = get_map_from_coord(pl->ob->map, &x, &y);

            if (!m) {
                continue;
            }

            FOR_MAP_LAYER_BEGIN(m, x, y, LAYER_LIVING, -1, tmp) {
                if (OBJECT_CAN_TALK(tmp) &&
                    (type == CMD_TALK_NPC || strcmp(tmp->name, npc_name) == 0)) {
                    npc = tmp;
                    FOR_MAP_LAYER_BREAK;
                }
            }
            FOR_MAP_LAYER_END
        }

        if (npc) {
            LOG(CHAT, "[TALKTO] [%s] [%s] %s", pl->ob->name, npc->name, msg);

            if (!monster_guard_check(npc, pl->ob, msg, 0)) {
                talk_to_npc(pl->ob, npc, msg);
            }
        } else if (type == CMD_TALK_NPC &&
                   OBJECT_VALID(pl->target_object, pl->target_object_count) &&
                   OBJECT_CAN_TALK(pl->target_object)) {
            draw_info_format(COLOR_WHITE,
                             pl->ob,
                             "You are too far away from %s.",
                             pl->target_object->name);
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
            LOG(PACKET, "Empty message.");
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
    } else if (type == CMD_TALK_CLOSE) {
        if (OBJECT_VALID(pl->talking_to, pl->talking_to_count)) {
            monster_data_dialogs_remove(pl->talking_to, pl->ob);
            pl->talking_to = NULL;
            pl->talking_to_count = 0;
        }
    } else {
        LOG(PACKET, "Invalid type: %d", type);
    }
}

void socket_command_control(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos) {
    char word[MAX_BUF], app_name[MAX_BUF];
    uint8_t type, sub_type;
    packet_struct *packet;

    if (strcasecmp(settings.control_allowed_ips, "none") == 0) {
        LOG(PACKET, "Control command received but no IPs are allowed.");
        return;
    }

    bool ip_match = false;

    size_t pos2 = 0;
    while (string_get_word(settings.control_allowed_ips, &pos2, ',', VS(word), 0)) {
        char *split[2];
        if (string_split(word, split, arraysize(split), '/') < 1) {
            continue;
        }

        struct sockaddr_storage addr;
        if (!socket_host2addr(split[0], &addr)) {
            continue;
        }

        unsigned short plen = socket_addr_plen(&addr);
        if (split[1] != NULL) {
            uint64_t value;
            if (!string_parse_uint64(split[1], 10, 0, plen, &value)) {
                LOG(ERROR, "Ignoring invalid control CIDR prefix: %s", word);
                continue;
            }
            plen = (unsigned short)value;
        }

        if (socket_cmp_addr(ns->sc, &addr, plen) == 0) {
            ip_match = true;
            break;
        }
    }

    if (!ip_match) {
        LOG(PACKET, "Received control command from unauthorized IP: %s", socket_get_id(ns->sc));
        return;
    }

    packet_to_string(data, len, &pos, app_name, sizeof(app_name));

    if (string_isempty(app_name)) {
        LOG(PACKET, "Received empty app_name.");
        return;
    }

    type = packet_to_uint8(data, len, &pos);
    sub_type = packet_to_uint8(data, len, &pos);

    switch (type) {
        case CMD_CONTROL_MAP: {
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
                case CMD_CONTROL_MAP_RESET: {
                    map_force_reset(control_map);
                    return;
                }
            }

            break;
        }

        case CMD_CONTROL_PLAYER: {
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
                case CMD_CONTROL_PLAYER_TELEPORT: {
                    char mappath[HUGE_BUF];
                    int16_t x, y;
                    mapstruct *m;

                    packet_to_string(data, len, &pos, mappath, sizeof(mappath));
                    x = packet_to_int16(data, len, &pos);
                    y = packet_to_int16(data, len, &pos);

                    m = ready_map_name(mappath, NULL, 0);

                    if (m == NULL) {
                        LOG(ERROR,
                            "Could not teleport player to '%s' (%d,%d): "
                            "map could not be loaded.",
                            mappath,
                            x,
                            y);
                        return;
                    }

                    ret = object_enter_map(control_player->ob, NULL, m, x, y, 1);
                    break;
                }
            }

            if (ret == 1) {
                packet = packet_new(CLIENT_CMD_CONTROL, 256, 256);
                packet_enable_ndelay(packet);
                packet_debug_data(packet, 0, "Forwarded data");
                packet_append_data_len(packet, data, len);
                socket_send_packet(control_player->cs, packet);

                return;
            }

            break;
        }
    }

    LOG(PACKET,
        "Unrecognised control command type: %d, sub-type: %d, "
        "by application: '%s'",
        type,
        sub_type,
        app_name);
}

void socket_command_combat(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos) {
    uint8_t combat = packet_to_uint8(data, len, &pos);
    uint8_t combat_force = packet_to_uint8(data, len, &pos);

    if (combat_force && !pl->combat_force) {
        combat = true;
    } else if (!combat && pl->combat) {
        combat_force = false;
    }

    pl->combat = combat;
    pl->combat_force = combat_force;

    send_target_command(pl);
}

/**
 * Handler for the ask resource command.
 *
 * @copydoc socket_command_func
 */
void socket_command_ask_resource(socket_struct *ns,
                                 player *pl,
                                 uint8_t *data,
                                 size_t len,
                                 size_t pos) {
    HARD_ASSERT(ns != NULL);
    HARD_ASSERT(data != NULL);

    char resource_name[HUGE_BUF];
    packet_to_string(data, len, &pos, VS(resource_name));

    if (string_isempty(resource_name)) {
        LOG(PACKET, "Empty resource name from client %s", socket_get_id(ns->sc));
        return;
    }

    resource_t *resource = resources_find(resource_name);
    if (resource == NULL) {
        LOG(DEVEL, "Invalid resource '%s' from client %s", resource_name, socket_get_id(ns->sc));
        return;
    }

    resources_send(resource, ns);
}
