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
 * Sound related functions.
 */

#include <global.h>
#include <toolkit/packet.h>
#include <player.h>
#include <object.h>

/**
 * Maximum distance a player may hear a sound from.
 *
 * This is only used for new client/server sound.  If the sound source
 * on the map is farther away than this, we don't sent it to the client.
 */
#define MAX_SOUND_DISTANCE 12
/** Squared maximum sound distance, using POW2. */
#define MAX_SOUND_DISTANCE_SQUARED POW2(MAX_SOUND_DISTANCE)
/**
 * Volume adjustment for each map level.
 */
#define MAP_LEVEL_VOLUME_ADJUST 10

/**
 * Plays a sound for specified player only.
 * @param pl
 * Player to play sound to.
 * @param type
 * Type of the sound being played, one of @ref CMD_SOUND_xxx.
 * @param filename
 * Name of the sound to play.
 * @param x
 * X position where the sound is playing from.
 * @param y
 * Y position where the sound is playing from.
 * @param loop
 * How many times to loop the sound, -1 for infinite number.
 * @param volume
 * Volume adjustment.
 */
void play_sound_player_only(player *pl, int type, const char *filename, int x, int y, int loop, int volume)
{
    packet_struct *packet;

    /* Player has disabled sound */
    if (!pl->cs->sound) {
        return;
    }

    packet = packet_new(CLIENT_CMD_SOUND, 64, 64);
    packet_debug_data(packet, 0, "Sound command type");
    packet_append_uint8(packet, type);
    packet_debug_data(packet, 0, "Filename");
    packet_append_string_terminated(packet, filename);
    packet_debug_data(packet, 0, "Loop");
    packet_append_int8(packet, loop);
    packet_debug_data(packet, 0, "Volume");
    packet_append_int8(packet, volume);

    /* Add X/Y offset for sound effects. */
    if (type == CMD_SOUND_EFFECT) {
        packet_debug_data(packet, 0, "X coordinate");
        packet_append_uint8(packet, x);
        packet_debug_data(packet, 0, "Y coordinate");
        packet_append_uint8(packet, y);
    }

    socket_send_packet(pl->cs, packet);
}

/**
 * Internal function used by play_sound_map().
 * @param tiled
 * Tiled map that is being checked.
 * @param map
 * Map to play the sound on.
 * @param type
 * Type of the sound being played, one of @ref CMD_SOUND_xxx.
 * @param filename
 * Name of the sound to play.
 * @param x
 * X position where the sound is playing from.
 * @param y
 * Y position where the sound is playing from.
 * @param loop
 * How many times to loop the sound, -1 for infinite number.
 * @param volume
 * Volume adjustment.
 * @return
 * 0.
 */
static int play_sound_map_internal(mapstruct *tiled, mapstruct *map,
        mapstruct *orig, int type, const char *filename, int x, int y, int loop,
        int volume)
{
    object *pl;
    rv_vector rv;
    int volume_adj;

    volume_adj = abs(orig->coords[2] - map->coords[2]) *
            MAP_LEVEL_VOLUME_ADJUST;

    for (pl = tiled->player_first; pl != NULL; pl = CONTR(pl)->map_above) {
        if (get_rangevector_from_mapcoords(map, x, y, pl->map, pl->x, pl->y,
                &rv, RV_NO_DISTANCE) && POW2(rv.distance_x) +
                POW2(rv.distance_y) <= MAX_SOUND_DISTANCE_SQUARED) {
            play_sound_player_only(CONTR(pl), type, filename, rv.distance_x,
                    rv.distance_y, loop, volume - volume_adj);
        }
    }

    return 0;
}

/**
 * Plays a sound on a map.
 *
 * Considers tiled maps.
 * @param map
 * Map to play the sound on.
 * @param type
 * Type of the sound being played, one of @ref CMD_SOUND_xxx.
 * @param filename
 * Name of the sound to play.
 * @param x
 * X position where the sound is playing from.
 * @param y
 * Y position where the sound is playing from.
 * @param loop
 * How many times to loop the sound, -1 for infinite number.
 * @param volume
 * Volume adjustment.
 */
void play_sound_map(mapstruct *map, int type, const char *filename, int x, int y, int loop, int volume)
{
    if (map == NULL) {
        return;
    }

    MAP_TILES_WALK_START(map, play_sound_map_internal, map, type, filename, x,
            y, loop, volume)
    {
    }
    MAP_TILES_WALK_END
}
