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
 * Lighting system. */

#include <global.h>

#define MAX_MASK_SIZE 81
#define NR_LIGHT_MASK 10
#define MAX_LIGHT_SOURCE 13

static int lmask_x[MAX_MASK_SIZE] = {
    0, 0, 1, 1, 1, 0, -1, -1, -1, 0, 1, 2, 2, 2, 2, 2, 1, 0, -1, -2, -2, -2, -2, -2, -1,
    0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 2, 1, 0, -1, -2, -3, -3, -3, -3, -3, -3, -3, -2, -1,
    0, 1, 2, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 2, 1, 0, -1, -2, -3, -4, -4, -4, -4, -4, -4, -4, -4, -4, -3, -2, -1
};

static int lmask_y[MAX_MASK_SIZE] = {
    0, -1, -1, 0, 1, 1, 1, 0, -1, -2, -2, -2, -1, 0, 1, 2, 2, 2, 2, 2, 1, 0, -1, -2, -2,
    -3, -3, -3, -3, -2, -1, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 2, 1, 0, -1, -2, -3, -3, -3,
    4, 4, 4, 4, 4, 3, 2, 1, 0, -1, -2, -3, -4, -4, -4, -4, -4, -4, -4, -4, -4, -3, -2, -1, 0, 1, 2, 3, 4, 4, 4, 4
};

static int light_mask[MAX_LIGHT_SOURCE + 1] = {
    0,
    1,
    2, 3,
    4, 5, 6, 6,
    7, 7, 8, 8,
    8, 9
};

static int light_mask_width[NR_LIGHT_MASK] = {
    0, 1, 2, 2, 3,
    3, 3, 4, 4, 4
};

static int light_mask_size[NR_LIGHT_MASK] = {
    0, 9, 25, 25, 49,
    49, 49, 81, 81, 81
};

static int light_masks[NR_LIGHT_MASK][MAX_MASK_SIZE] = {
    {0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, },
    {40,
        20, 20, 20, 20, 20, 20, 20, 20,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, },
    {80,
        40, 40, 40, 40, 40, 40, 40, 40,
        20, 20, 20, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, },
    {160,
        80, 80, 80, 80, 80, 80, 80, 80,
        40, 40, 40, 40, 40, 40, 40, 40,
        40, 40, 40, 40, 40, 40, 40, 40,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, },
    {160,
        80, 80, 80, 80, 80, 80, 80, 80,
        40, 40, 40, 40, 40, 40, 40, 40,
        40, 40, 40, 40, 40, 40, 40, 40,
        20, 20, 20, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, },
    {320,
        160, 160, 160, 160, 160, 160, 160, 160,
        80, 80, 80, 80, 80, 80, 80, 80,
        80, 80, 80, 80, 80, 80, 80, 80,
        40, 40, 40, 40, 40, 40, 40, 40,
        40, 40, 40, 40, 40, 40, 40, 40,
        40, 40, 40, 40, 40, 40, 40, 40,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, },
    {320,
        160, 160, 160, 160, 160, 160, 160, 160,
        80, 80, 80, 80, 80, 80, 80, 80,
        80, 80, 80, 80, 80, 80, 80, 80,
        40, 40, 40, 40, 40, 40, 40, 40,
        40, 40, 40, 40, 40, 40, 40, 40,
        40, 40, 40, 40, 40, 40, 40, 40,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, },
    {320,
        160, 160, 160, 160, 160, 160, 160, 160,
        80, 80, 80, 80, 80, 80, 80, 80,
        80, 80, 80, 80, 80, 80, 80, 80,
        40, 40, 40, 40, 40, 40, 40, 40,
        40, 40, 40, 40, 40, 40, 40, 40,
        40, 40, 40, 40, 40, 40, 40, 40,
        20, 20, 20, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20, },
    {640,
        320, 320, 320, 320, 320, 320, 320, 320,
        160, 160, 160, 160, 160, 160, 160, 160,
        160, 160, 160, 160, 160, 160, 160, 160,
        80, 80, 80, 80, 80, 80, 80, 80,
        80, 80, 80, 80, 80, 80, 80, 80,
        80, 80, 80, 80, 80, 80, 80, 80,
        40, 40, 40, 40, 40, 40, 40, 40,
        40, 40, 40, 40, 40, 40, 40, 40,
        40, 40, 40, 40, 40, 40, 40, 40,
        40, 40, 40, 40, 40, 40, 40, 40, },
    {1280,
        640, 640, 640, 640, 640, 640, 640, 640,
        160, 160, 160, 160, 160, 160, 160, 160,
        160, 160, 160, 160, 160, 160, 160, 160,
        80, 80, 80, 80, 80, 80, 80, 80,
        80, 80, 80, 80, 80, 80, 80, 80,
        80, 80, 80, 80, 80, 80, 80, 80,
        40, 40, 40, 40, 40, 40, 40, 40,
        40, 40, 40, 40, 40, 40, 40, 40,
        40, 40, 40, 40, 40, 40, 40, 40,
        40, 40, 40, 40, 40, 40, 40, 40, }

};

static int get_real_light_source_value(int l)
{
    if (l > MAX_LIGHT_SOURCE) {
        return light_mask[MAX_LIGHT_SOURCE];
    }

    if (l < -MAX_LIGHT_SOURCE) {
        return -light_mask[MAX_LIGHT_SOURCE];
    }

    if (l < 0) {
        return -light_mask[-l];
    }

    return light_mask[l];
}

static int light_mask_adjust(mapstruct *map, int x, int y, int intensity, int mod, mapstruct *restore_map, int other_only)
{
    MapSpace *msp;
    mapstruct *m;
    int xt, yt, i, mlen, map_flag = 0;

    if (intensity < 0) {
        mod = -mod;
    }

    intensity = abs(intensity);
    mlen = light_mask_size[intensity];

    for (i = 0; i < mlen; i++) {
        xt = x + lmask_x[i];
        yt = y + lmask_y[i];

        if (!(m = get_map_from_coord2(map, &xt, &yt))) {
            if (xt) {
                map_flag = 1;
            }

            continue;
        }

        if (restore_map && m != restore_map) {
            continue;
        }

        if (other_only && m == map) {
            continue;
        }

        /* This light mask crosses some tiled map borders */
        if (m != map) {
            map_flag = 1;
        }

        msp = GET_MAP_SPACE_PTR(m, xt, yt);
        msp->light_value += light_masks[intensity][i] * mod;
    }

    return map_flag;
}

/**
 * Add or remove a light source to a map space.
 * Adjust the light source map counter and apply
 * the area of light it invokes around it.
 * @param map The map of this light
 * @param x X position of light
 * @param y Y position of light
 * @param light Glow radius of the light */
void adjust_light_source(mapstruct *map, int x, int y, int light)
{
    int nlm, olm;
    MapSpace *msp1 = GET_MAP_SPACE_PTR(map, x, y);

    /* this happens, we don't change the intense of the old light mask */
    /* old mask */
    olm = get_real_light_source_value(msp1->light_source);
    msp1->light_source += light;
    /* new mask */
    nlm = get_real_light_source_value(msp1->light_source);

    /* Old mask same as new one? not much to do */
    if (nlm == olm) {
        return;
    }

    if (olm) {
        /* Remove the old light mask */
        light_mask_adjust(map, x, y, olm, -1, NULL, 0);

        /* Perhaps we are in this list - perhaps we are not */
        if (msp1->prev_light) {
            msp1->prev_light->next_light = msp1->next_light;
        } else {
            /* We are the list head */
            if (map->first_light == msp1) {
                map->first_light = msp1->next_light;
            }
        }

        /* Handle next link */
        if (msp1->next_light) {
            msp1->next_light->prev_light = msp1->prev_light;
        }

        msp1->prev_light = NULL;
        msp1->next_light = NULL;
    }

    if (nlm) {
        /* Add new light mask */
        if (light_mask_adjust(map, x, y, nlm, 1, NULL, 0)) {
            /* Don't chain if we are chained previous */
            if (msp1->next_light || msp1->prev_light || map->first_light == msp1) {
                return;
            }

            /* We should be always unlinked here - so link it now */
            msp1->next_light = map->first_light;

            if (map->first_light) {
                msp1->next_light->prev_light = msp1;
            }

            map->first_light = msp1;
        }
    }
}

/**
 * Check light source list of specified map.
 * This will also check all tiled maps attached
 * to the map.
 * @param map The map to check. */
void check_light_source_list(mapstruct *map)
{
    int i, intensity, x, y, reaching;
    mapstruct *m;
    MapSpace *tmp;

    for (i = 0; i < TILED_NUM_DIR; i++) {
        m = map->tile_map[i];

        if (m && (m->in_memory == MAP_IN_MEMORY || m->in_memory == MAP_LOADING) && m->first_light) {
            for (tmp = m->first_light; tmp; tmp = tmp->next_light) {
                if (!tmp->first) {
                    continue;
                }

                intensity = get_real_light_source_value(tmp->light_source);

                x = tmp->first->x;
                y = tmp->first->y;
                reaching = 1;

                switch (i) {
                case TILED_NORTH:

                    if (y + light_mask_width[abs(intensity)] < MAP_HEIGHT(m)) {
                        reaching = 0;
                    }

                    break;

                case TILED_EAST:

                    if (x - light_mask_width[abs(intensity)] >= 0) {
                        reaching = 0;
                    }

                    break;

                case TILED_SOUTH:

                    if (y - light_mask_width[abs(intensity)] >= 0) {
                        reaching = 0;
                    }

                    break;

                case TILED_WEST:

                    if (x + light_mask_width[abs(intensity)] < MAP_WIDTH(m)) {
                        reaching = 0;
                    }

                    break;

                case TILED_NORTHEAST:

                    if ((y + light_mask_width[abs(intensity)]) < MAP_HEIGHT(m) || (x - light_mask_width[abs(intensity)]) >= 0) {
                        reaching = 0;
                    }

                    break;

                case TILED_SOUTHEAST:

                    if ((x - light_mask_width[abs(intensity)]) >= 0 || (y - light_mask_width[abs(intensity)]) >= 0) {
                        reaching = 0;
                    }

                    break;

                case TILED_SOUTHWEST:

                    if ((y - light_mask_width[abs(intensity)]) >= 0 || (x + light_mask_width[abs(intensity)]) < MAP_WIDTH(m)) {
                        reaching = 0;
                    }

                    break;

                case TILED_NORTHWEST:

                    if ((y + light_mask_width[abs(intensity)]) < MAP_HEIGHT(m) || (x + light_mask_width[abs(intensity)]) < MAP_WIDTH(m)) {
                        reaching = 0;
                    }

                    break;
                }

                if (reaching) {
                    light_mask_adjust(m, x, y, intensity, 1, map, 0);
                }
            }
        }
    }
}

/**
 * Remove light sources list from a map.
 * @param map The map to remove from. */
void remove_light_source_list(mapstruct *map)
{
    MapSpace *tmp;

    for (tmp = map->first_light; tmp; tmp = tmp->next_light) {
        if (!tmp->first) {
            continue;
        }

        light_mask_adjust(map, tmp->first->x, tmp->first->y, get_real_light_source_value(tmp->light_source), -1, NULL, 1);
    }

    map->first_light = NULL;
}
