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
 * Controls map swap functions. */

#include <global.h>

/**
 * Write maps log. */
void write_map_log(void)
{
    FILE *fp;
    mapstruct *map;
    char buf[MAX_BUF];
    long current_time = time(NULL);

    snprintf(buf, sizeof(buf), "%s/temp.maps", settings.datapath);

    if (!(fp = fopen(buf, "w"))) {
        logger_print(LOG(BUG), "Could not open %s for writing", buf);
        return;
    }

    for (map = first_map; map; map = map->next) {
        /* If tmpname is null, it is probably a unique player map,
         * so don't save information on it. */
        if (map->in_memory != MAP_IN_MEMORY && map->tmpname && strncmp(map->path, "/random", 7)) {
            fprintf(fp, "%s:%s:%ld:%d:%d\n", map->path, map->tmpname, (map->reset_time - current_time), map->difficulty, map->darkness);
        }
    }

    fclose(fp);
}

/**
 * Read map log. */
void read_map_log(void)
{
    FILE *fp;
    mapstruct *map;
    char buf[MAX_BUF];
    int darkness;

    snprintf(buf, sizeof(buf), "%s/temp.maps", settings.datapath);

    if (!(fp = fopen(buf, "r"))) {
        return;
    }

    while (fgets(buf, sizeof(buf), fp)) {
        char *tmp[3];

        map = get_linked_map();

        if (string_split(buf, tmp, sizeof(tmp) / sizeof(*tmp), ':') != 3) {
            logger_print(LOG(DEBUG), "%s/temp.maps: ignoring invalid line: %s", settings.datapath, buf);
            continue;
        }

        FREE_AND_COPY_HASH(map->path, tmp[0]);
        map->tmpname = estrdup(tmp[1]);

        sscanf(tmp[2], "%ud:%d:%d\n", &map->reset_time, &map->difficulty, &darkness);

        map->in_memory = MAP_SWAPPED;
        map->darkness = darkness;

        if (darkness == -1) {
            darkness = MAX_DARKNESS;
        }

        map->light_value = global_darkness_table[MAX_DARKNESS];
    }

    fclose(fp);
}

/**
 * Swaps a map to disk.
 * @param map Map to swap.
 * @param force_flag Force flag. If set, will not check for players. */
void swap_map(mapstruct *map, int force_flag)
{
    int i;

    if (map->in_memory != MAP_IN_MEMORY) {
        logger_print(LOG(BUG), "Tried to swap out map which was not in memory (%s).", map->path);
        return;
    }

    /* Test for players. */
    if (!force_flag) {
        if (map->player_first) {
            return;
        }

        for (i = 0; i < TILED_NUM; i++) {
            /* If there is a map, is loaded and in memory, has players, then no
             * swap */
            if (map->tile_map[i] && map->tile_map[i]->in_memory == MAP_IN_MEMORY && map->tile_map[i]->player_first) {
                return;
            }
        }
    }

    /* Update the reset time. */
    if (!MAP_FIXED_RESETTIME(map)) {
        set_map_reset_time(map);
    }

    /* If it is immediate reset time, don't bother saving it - just get
     * rid of it right away. */
    if (map->reset_time <= (uint32) seconds()) {
        mapstruct *oldmap = map;

        if (map->events) {
            /* Trigger the map reset event */
            trigger_map_event(MEVENT_RESET, map, NULL, NULL, NULL, map->path, 0);
        }

        map = map->next;
        delete_map(oldmap);
        return;
    }

    if (new_save_map(map, 0) == -1) {
        logger_print(LOG(BUG), "Failed to swap map %s.", map->path);
        /* Need to reset the in_memory flag so that delete map will also
         * free the objects with it. */
        map->in_memory = MAP_IN_MEMORY;
        delete_map(map);
    }
    else {
        free_map(map, 1);
    }
}

/**
 * Check active maps and swap them out. */
void check_active_maps(void)
{
    mapstruct *map, *next;

    for (map = first_map; map != NULL; map = next) {
        next = map->next;

        if (map->in_memory != MAP_IN_MEMORY) {
            continue;
        }

        if (!map->timeout) {
            if (!map->player_first) {
                set_map_timeout(map);
            }

            continue;
        }

        if (--(map->timeout) > 0) {
            continue;
        }

        swap_map(map, 0);
    }
}

/**
 * Removes temporary files of maps which are going to be reset next time
 * they are visited.
 *
 * This is very useful if the tmp-disk is very full. */
void flush_old_maps(void)
{
    mapstruct *m = first_map, *oldmap;
    long sec = seconds();

    while (m) {
        /* There can be cases (ie death) where a player leaves a map and
         * the timeout is not set so it isn't swapped out. */
        if ((m->in_memory == MAP_IN_MEMORY) && (m->timeout == 0) && !m->player_first) {
            set_map_timeout(m);
        }

        /* Per player unique maps are never really reset. */
        if (MAP_UNIQUE(m) && m->in_memory == MAP_SWAPPED) {
            oldmap = m;
            m = m->next;
            delete_map(oldmap);
        }
        /* No need to flush them if there are no resets */
        else if (m->in_memory != MAP_SWAPPED || m->tmpname == NULL || (uint32) sec < m->reset_time) {
            m = m->next;
        }
        else {
            if (m->events) {
                /* Trigger the map reset event */
                trigger_map_event(MEVENT_RESET, m, NULL, NULL, NULL, m->path, 0);
            }

            clean_tmp_map(m);
            oldmap = m;
            m = m->next;
            delete_map(oldmap);
        }
    }
}
