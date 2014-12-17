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
 * Sound related functions. */

#include <global.h>

/**
 * Maximum distance a player may hear a sound from.
 *
 * This is only used for new client/server sound.  If the sound source
 * on the map is farther away than this, we don't sent it to the client. */
#define MAX_SOUND_DISTANCE 12
/** Squared maximum sound distance, using POW2. */
#define MAX_SOUND_DISTANCE_SQUARED POW2(MAX_SOUND_DISTANCE)
/**
 * Volume adjustment for each map level.
 */
#define MAP_LEVEL_VOLUME_ADJUST 5

/**
 * Plays a sound for specified player only.
 * @param pl Player to play sound to.
 * @param type Type of the sound being played, one of @ref CMD_SOUND_xxx.
 * @param filename Name of the sound to play.
 * @param x X position where the sound is playing from.
 * @param y Y position where the sound is playing from.
 * @param loop How many times to loop the sound, -1 for infinite number.
 * @param volume Volume adjustment. */
void play_sound_player_only(player *pl, int type, const char *filename, int x, int y, int loop, int volume)
{
    packet_struct *packet;

    /* Player has disabled sound */
    if (!pl->socket.sound) {
        return;
    }

    packet = packet_new(CLIENT_CMD_SOUND, 64, 64);
    packet_append_uint8(packet, type);
    packet_append_string_terminated(packet, filename);
    packet_append_sint8(packet, loop);
    packet_append_sint8(packet, volume);

    /* Add X/Y offset for sound effects. */
    if (type == CMD_SOUND_EFFECT) {
        packet_append_uint8(packet, x);
        packet_append_uint8(packet, y);
    }

    socket_send_packet(&pl->socket, packet);
}

/**
 * Internal function used by play_sound_map().
 * @param map Map to play the sound on.
 * @param type Type of the sound being played, one of @ref CMD_SOUND_xxx.
 * @param filename Name of the sound to play.
 * @param x X position where the sound is playing from.
 * @param y Y position where the sound is playing from.
 * @param loop How many times to loop the sound, -1 for infinite number.
 * @param volume Volume adjustment.
 */
static void play_sound_map_tiled(mapstruct *map, int type, const char *filename,
        int x, int y, int loop, int volume)
{
    object *pl;
    int i;
    rv_vector rv;

    /* Check the map for players. */
    for (pl = map->player_first; pl != NULL; pl = CONTR(pl)->map_above) {
        if ((POW2(pl->x - x) + POW2(pl->y - y)) <= MAX_SOUND_DISTANCE_SQUARED) {
            play_sound_player_only(CONTR(pl), type, filename,
                    x - pl->x, y - pl->y, loop, volume);
        }
    }

    /* Check tiled maps for players. */
    for (i = 0; i < TILED_NUM_DIR; i++) {
        if (map->tile_map[i] != NULL &&
                map->tile_map[i]->in_memory == MAP_IN_MEMORY) {
            for (pl = map->tile_map[i]->player_first; pl != NULL;
                    pl = CONTR(pl)->map_above) {
                if (get_rangevector_from_mapcoords(map, x, y, pl->map, pl->x,
                        pl->y, &rv, RV_NO_DISTANCE) &&
                        POW2(rv.distance_x) + POW2(rv.distance_y) <=
                        MAX_SOUND_DISTANCE_SQUARED) {
                    play_sound_player_only(CONTR(pl), type, filename,
                            rv.distance_x, rv.distance_y, loop, volume);
                }
            }
        }
    }
}

/**
 * Plays a sound on a map.
 *
 * Considers tiled maps.
 * @param map Map to play the sound on.
 * @param type Type of the sound being played, one of @ref CMD_SOUND_xxx.
 * @param filename Name of the sound to play.
 * @param x X position where the sound is playing from.
 * @param y Y position where the sound is playing from.
 * @param loop How many times to loop the sound, -1 for infinite number.
 * @param volume Volume adjustment.
 */
void play_sound_map(mapstruct *map, int type, const char *filename, int x, int y, int loop, int volume)
{
    mapstruct *tiled;
    int dir, i;

    if (map == NULL || map->in_memory != MAP_IN_MEMORY) {
        return;
    }

    play_sound_map_tiled(map, type, filename, x, y, loop, volume);

    for (dir = TILED_UP; dir <= TILED_DOWN; dir++) {
        for (tiled = map->tile_map[dir], i = 1;
                tiled != NULL && tiled->in_memory == MAP_IN_MEMORY;
                tiled = tiled->tile_map[dir], i++) {
            play_sound_map_tiled(tiled, type, filename, x, y, loop,
                    volume + -(MAP_LEVEL_VOLUME_ADJUST * i));
        }
    }
}
