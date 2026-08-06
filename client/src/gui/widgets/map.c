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
 * Implements map type widgets.
 *
 * @author Zoey Rose
 */

#include <global.h>
#include <video.h>
#include <surface_primitives.h>
#include <client_socket.h>
#include <animations.h>
#include <region_map.h>
#include <toolkit/packet.h>
#include <toolkit/string.h>
#include <toolkit/bresenham.h>
#include <toolkit/clioptions.h>
#include <toolkit/path.h>

/**
 * Map cells.
 */
static struct MapCell *cells;
static struct MapCell *level_cells[MAP2_LEVELS];
static uint64_t level_lighting_revision[MAP2_LEVELS];
static size_t current_level_index = MAP2_DEPTH_INDEX(0);
static uint16_t map_level_mask;
static int map_width;
static int map_height;
static int map_cache_origin_x;
static int map_cache_origin_y;

/** Return a logical cell from the circular fog-of-war cache. */
static struct MapCell *map_cache_cell_at(struct MapCell *level,
                                         int x,
                                         int y,
                                         int width,
                                         int height,
                                         int origin_x,
                                         int origin_y) {
    HARD_ASSERT(level != NULL);
    HARD_ASSERT(x >= 0 && x < width);
    HARD_ASSERT(y >= 0 && y < height);

    int physical_x = (x + origin_x) % width;
    int physical_y = (y + origin_y) % height;
    return &level[(size_t)physical_y * width + physical_x];
}

static struct MapCell *map_cache_cell(struct MapCell *level, int x, int y) {
    return map_cache_cell_at(level,
                             x,
                             y,
                             map_width * MAP_FOW_SIZE,
                             map_height * MAP_FOW_SIZE,
                             map_cache_origin_x,
                             map_cache_origin_y);
}

/** Mark the clipped logical rectangle as explored fog. */
static void map_cache_mark_fow(struct MapCell *level,
                               int x_start,
                               int x_end,
                               int y_start,
                               int y_end,
                               int width,
                               int height) {
    x_start = MAX(0, x_start);
    x_end = MIN(width, x_end);
    y_start = MAX(0, y_start);
    y_end = MIN(height, y_end);

    for (int x = x_start; x < x_end; x++) {
        for (int y = y_start; y < y_end; y++) {
            map_cache_cell(level, x, y)->fow = 1;
        }
    }
}

#define MAP_CELL_GET(_x, _y) map_cache_cell(cells, (_x), (_y))
#define MAP_CELL_GET_MIDDLE(_x, _y) MAP_CELL_GET((_x) + MAP_STARTX, (_y) + MAP_STARTY)

/** Vertical screen projection of one linked physical map level. */
#define MAP_LEVEL_PIXEL_HEIGHT 46

/** Nearby occluded doors receive a camera hint without exposing interiors. */
#define DOOR_HINT_RADIUS 3
#define DOOR_HINT_COLOR "ffc64a"

/** Select one protocol map depth, allocating its cache on demand. */
bool map_select_level(int depth, bool create) {
    if (depth < -MAP2_MAX_DEPTH || depth > MAP2_MAX_DEPTH) {
        return false;
    }

    size_t index = (size_t)MAP2_DEPTH_INDEX(depth);
    if (level_cells[index] == NULL && create) {
        size_t count = (size_t)map_width * MAP_FOW_SIZE * (size_t)map_height * MAP_FOW_SIZE;
        level_cells[index] = xcalloc(count, sizeof(*level_cells[index]));
    }

    current_level_index = index;
    cells = level_cells[index];
    return cells != NULL;
}

void map_set_level_mask(uint16_t mask) {
    map_level_mask = mask;

    for (int depth = -MAP2_MAX_DEPTH; depth <= MAP2_MAX_DEPTH; depth++) {
        size_t index = (size_t)MAP2_DEPTH_INDEX(depth);

        uint16_t bit = UINT16_C(1) << index;
        if (!(mask & bit) && level_cells[index] != NULL) {
            free(level_cells[index]);
            level_cells[index] = NULL;
            level_lighting_revision[index]++;
        }
    }

    lighting_set_level_mask(mask);
    map_select_level(0, true);
}
/**
 * Zoomed map.
 */
static SDL_Surface *zoomed = NULL;
static SDL_Surface *map_level_surfaces[2];
/**
 * Map animation queue.
 */
static map_anim_t *first_anim = NULL;

/**
 * Current shown map: mapname, length, etc
 */
_mapdata MapData;

/**
 * Multi-part object data.
 */
_multi_part_obj MultiArchs[16];

/**
 * Holds coordinates of the last map square the mouse was over.
 */
static int old_map_mouse_x = -1, old_map_mouse_y = -1;
/**
 * If true, show the mouse map square indicator.
 */
static bool map_show_mouse = false;
/**
 * When the right button was pressed on the map widget. -1 = not
 * pressed.
 */
static int right_click_ticks = -1;

/**
 * If true, will print tile coordinates.
 */
static bool tiles_debug = false;

static int get_top_floor_height(struct MapCell *cell, int sub_layer);

/**
 * Description of the --tiles_debug command.
 */
static const char *clioptions_option_tiles_debug_desc =
    "Enable map tiles debugging (shows tile coordinates).";
/** @copydoc clioptions_handler_func */
static bool clioptions_option_tiles_debug(const char *arg, char **errmsg) {
    tiles_debug = true;
    return true;
}

/**
 * Loads multi-arch object data offsets.
 */
void load_mapdef_dat(void) {
    FILE *stream;
    int i, ii, x, y, d[32];
    char line[MAX_BUF];

    clioption_t *cli;
    CLIOPTIONS_CREATE(cli, tiles_debug, "Enable map tiles debugging");

    stream = path_fopen(ARCHDEF_FILE, "r");

    if (stream == NULL) {
        LOG(BUG, "Can't open file %s", ARCHDEF_FILE);
        return;
    }

    for (i = 0; i < 16; i++) {
        if (!fgets(line, 255, stream)) {
            break;
        }

        sscanf(line,
               "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d "
               "%d %d %d %d %d %d %d %d %d %d %d %d %d %d",
               &x,
               &y,
               &d[0],
               &d[1],
               &d[2],
               &d[3],
               &d[4],
               &d[5],
               &d[6],
               &d[7],
               &d[8],
               &d[9],
               &d[10],
               &d[11],
               &d[12],
               &d[13],
               &d[14],
               &d[15],
               &d[16],
               &d[17],
               &d[18],
               &d[19],
               &d[20],
               &d[21],
               &d[22],
               &d[23],
               &d[24],
               &d[25],
               &d[26],
               &d[27],
               &d[28],
               &d[29],
               &d[30],
               &d[31]);
        MultiArchs[i].xlen = x;
        MultiArchs[i].ylen = y;

        for (ii = 0; ii < 16; ii++) {
            MultiArchs[i].part[ii].xoff = d[ii * 2];
            MultiArchs[i].part[ii].yoff = d[ii * 2 + 1];
        }
    }

    fclose(stream);
}

/**
 * Clear the map.
 * @param hard
 * Hard reset
 */
void clear_map(bool hard) {
    size_t cells_size;

    /* Cache the map width and height. */
    map_width = MAP_LOOK_TO_WIRE_SIZE(setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH));
    map_height = MAP_LOOK_TO_WIRE_SIZE(setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT));

    cells_size = sizeof(*cells) * map_width * MAP_FOW_SIZE * map_height * MAP_FOW_SIZE;

    for (size_t i = 0; i < arraysize(level_cells); i++) {
        if (level_cells[i] != NULL) {
            memset(level_cells[i], 0, cells_size);
            level_lighting_revision[i]++;
        }
    }

    map_level_mask = UINT16_C(1) << MAP2_DEPTH_INDEX(0);
    map_cache_origin_x = 0;
    map_cache_origin_y = 0;
    map_select_level(0, true);
    sound_ambient_clear();
    map_anims_clear();

    if (hard) {
        region_map_reset(MapData.region_map);
        MapData.region_name[0] = '\0';
        MapData.region_longname[0] = '\0';
        MapData.region_has_map = false;
    }
}

/**
 * Update map size.
 *
 * @param w
 * New width.
 * @param h
 * New height.
 */
void map_update_size(int w, int h) {
    int old_w = map_width;
    int old_h = map_height;

    if (w != 0) {
        map_width = w;
    }

    if (h != 0) {
        map_height = h;
    }

    display_mapscroll((old_w - map_width) * (MAP_FOW_SIZE / 2),
                      (old_h - map_height) * (MAP_FOW_SIZE / 2),
                      old_w * MAP_FOW_SIZE,
                      old_h * MAP_FOW_SIZE);
}

/**
 * Scroll the map.
 * @param dx
 * X offset.
 * @param dy
 * Y offset.
 * @param old_w
 * Old width. 0 if width hasn't changed.
 * @param old_h
 * Old height. 0 if height hasn't changed.
 */
void display_mapscroll(int dx, int dy, int old_w, int old_h) {
    int width = map_width * MAP_FOW_SIZE;
    int height = map_height * MAP_FOW_SIZE;

    if (old_w != 0 && old_h != 0 && (old_w != width || old_h != height)) {
        int old_origin_x = map_cache_origin_x;
        int old_origin_y = map_cache_origin_y;

        for (size_t level = 0; level < arraysize(level_cells); level++) {
            if (level_cells[level] == NULL) {
                continue;
            }

            struct MapCell *old_cells = level_cells[level];
            struct MapCell *new_cells = xcalloc((size_t)width * height, sizeof(*new_cells));
            for (int x = 0; x < width; x++) {
                for (int y = 0; y < height; y++) {
                    int source_x = x + dx;
                    int source_y = y + dy;
                    if (source_x < 0 || source_x >= old_w || source_y < 0 || source_y >= old_h) {
                        continue;
                    }

                    new_cells[(size_t)y * width + x] = *map_cache_cell_at(old_cells,
                                                                          source_x,
                                                                          source_y,
                                                                          old_w,
                                                                          old_h,
                                                                          old_origin_x,
                                                                          old_origin_y);
                }
            }

            free(old_cells);
            level_cells[level] = new_cells;
            level_lighting_revision[level]++;
        }

        map_cache_origin_x = 0;
        map_cache_origin_y = 0;
    } else {
        if (abs(dx) >= width || abs(dy) >= height) {
            for (size_t level = 0; level < arraysize(level_cells); level++) {
                if (level_cells[level] != NULL) {
                    memset(level_cells[level],
                           0,
                           (size_t)width * height * sizeof(*level_cells[level]));
                    level_lighting_revision[level]++;
                }
            }
            map_cache_origin_x = 0;
            map_cache_origin_y = 0;
        } else if (dx != 0 || dy != 0) {
            map_cache_origin_x = (map_cache_origin_x + dx + width) % width;
            map_cache_origin_y = (map_cache_origin_y + dy + height) % height;

            int view_x = map_width * (MAP_FOW_SIZE / 2);
            int view_y = map_height * (MAP_FOW_SIZE / 2);
            for (size_t level = 0; level < arraysize(level_cells); level++) {
                if (level_cells[level] == NULL) {
                    continue;
                }

                struct MapCell *level_cells_current = level_cells[level];

                /* Clear only the cache strips newly exposed by the scroll.
                 * The old implementation allocated and copied the complete
                 * five-window FOW cache for every map level on every step. */
                int clear_x_start = dx > 0 ? width - dx : 0;
                int clear_x_end = dx > 0 ? width : -dx;
                for (int x = clear_x_start; x < clear_x_end; x++) {
                    for (int y = 0; y < height; y++) {
                        memset(map_cache_cell(level_cells_current, x, y),
                               0,
                               sizeof(struct MapCell));
                    }
                }

                int clear_y_start = dy > 0 ? height - dy : 0;
                int clear_y_end = dy > 0 ? height : -dy;
                for (int y = clear_y_start; y < clear_y_end; y++) {
                    for (int x = 0; x < width; x++) {
                        memset(map_cache_cell(level_cells_current, x, y),
                               0,
                               sizeof(struct MapCell));
                    }
                }

                /* Cells leaving the visible window become explored FOW. The
                 * rest of the history already carries its prior FOW state. */
                int shifted_view_x = view_x - dx;
                int shifted_view_y = view_y - dy;
                int fow_x_start = dx > 0 ? shifted_view_x : view_x + map_width;
                int fow_x_end = dx > 0 ? view_x : shifted_view_x + map_width;
                map_cache_mark_fow(level_cells_current,
                                   fow_x_start,
                                   fow_x_end,
                                   shifted_view_y,
                                   shifted_view_y + map_height,
                                   width,
                                   height);

                int fow_y_start = dy > 0 ? shifted_view_y : view_y + map_height;
                int fow_y_end = dy > 0 ? view_y : shifted_view_y + map_height;
                map_cache_mark_fow(level_cells_current,
                                   shifted_view_x,
                                   shifted_view_x + map_width,
                                   fow_y_start,
                                   fow_y_end,
                                   width,
                                   height);

                level_lighting_revision[level]++;
            }
        }
    }

    map_select_level(0, true);

    sound_ambient_mapcroll(dx, dy);
    map_anims_mapscroll(dx, dy);
    cpl.target_object_index = 0;
}

/** Shift independently cached levels after moving through an up/down link. */
void map_level_scroll(int dz) {
    if (dz == 0) {
        return;
    }

    struct MapCell *shifted[MAP2_LEVELS] = {0};
    uint64_t shifted_revisions[MAP2_LEVELS] = {0};
    for (int depth = -MAP2_MAX_DEPTH; depth <= MAP2_MAX_DEPTH; depth++) {
        int source_depth = depth + dz;

        if (source_depth >= -MAP2_MAX_DEPTH && source_depth <= MAP2_MAX_DEPTH) {
            shifted[MAP2_DEPTH_INDEX(depth)] = level_cells[MAP2_DEPTH_INDEX(source_depth)];
            shifted_revisions[MAP2_DEPTH_INDEX(depth)] =
                level_lighting_revision[MAP2_DEPTH_INDEX(source_depth)];
            level_cells[MAP2_DEPTH_INDEX(source_depth)] = NULL;
        }
    }

    for (size_t i = 0; i < arraysize(level_cells); i++) {
        free(level_cells[i]);
        level_cells[i] = shifted[i];
        level_lighting_revision[i] = shifted_revisions[i];
    }

    if (dz > 0) {
        map_level_mask >>= dz;
    } else {
        map_level_mask <<= -dz;
    }
    map_level_mask &= (UINT16_C(1) << MAP2_LEVELS) - 1;
    lighting_level_scroll(dz);
    map_select_level(0, true);
    map_anims_clear();
}

/**
 * Update map's name.
 * @param name
 * New map name.
 */
void update_map_name(const char *name) {
    snprintf(MapData.name_new, sizeof(MapData.name_new), "%s", name);
}

/**
 * Update map's weather.
 * @param weather
 * New weather.
 */
void update_map_weather(const char *weather) {
    effect_start(weather);
}

/**
 * Update map's height difference rendering flag.
 */
void update_map_height_diff(uint8_t height_diff) {
    MapData.height_diff = height_diff;
}

/**
 * Update map's region name.
 * @param region_name
 * New region name.
 */
void update_map_region_name(const char *region_name) {
    if (strcmp(MapData.region_name, region_name) == 0) {
        return;
    }

    snprintf(VS(MapData.region_name), "%s", region_name);
    region_map_update(MapData.region_map, region_name);
}

/**
 * Update map's region long name.
 * @param region_longname
 * New region long name.
 */
void update_map_region_longname(const char *region_longname) {
    snprintf(VS(MapData.region_longname), "%s", region_longname);
}

/**
 * Update map's path.
 * @param map_path
 * New map path.
 */
void update_map_path(const char *map_path) {
    snprintf(VS(MapData.map_path), "%s", map_path);
}

/**
 * Get player's direction.
 * @return
 * Player's direction.
 */
int map_get_player_direction(void) {
    struct MapCell *cell;
    int direction;

    cell = MAP_CELL_GET_MIDDLE(map_width - (map_width / 2) - 1, map_height - (map_height / 2) - 1);

    direction = cell->anim_facing[GET_MAP_LAYER(LAYER_LIVING, MapData.player_sub_layer)];

    if (direction == 0) {
        return 1;
    }

    return direction - 1;
}

/**
 * Get real map X/Y coordinates adjusted for player's position.
 * @param[out] x Will contain X coordinate.
 * @param[out] y Will contain Y coordinate.
 */
void map_get_real_coords(int *x, int *y) {
    *x = MapData.posx - (map_width / 2);
    *y = MapData.posy - (map_height / 2);
}

/**
 * Initialize map's data.
 * @param xl
 * Map width.
 * @param yl
 * Map height.
 * @param px
 * Player's X position.
 * @param py
 * Player's Y position.
 */
void init_map_data(int xl, int yl, int px, int py) {
    if (xl != -1) {
        MapData.xlen = xl;
    }

    if (yl != -1) {
        MapData.ylen = yl;
    }

    if (px != -1) {
        MapData.posx = px;
    }

    if (py != -1) {
        MapData.posy = py;
    }

    if (xl > 0) {
        clear_map(false);
    }
}

#define MAX_STRETCH 8
#define MAX_STRETCH_DIAG 12

/** Return one floor height used to join a stretched tile to its neighbor. */
static int map_cell_stretch_height(int x, int y, int w, int h, int sub_layer, int my_height) {
    if (x < 0 || x >= w || y < 0 || y >= h) {
        return 0;
    }

    struct MapCell *cell = map_cache_cell(cells, x, y);

    /* A negative floor beside stacked terrain joins to that terrain's top
     * floor. This used to infer stacked terrain from LAYER_EFFECT objects;
     * floor geometry itself is the authoritative source. */
    if (my_height < 0) {
        if (cell->stretch_upper_height != 0) {
            return cell->stretch_upper_height;
        }

        if (sub_layer != 0) {
            return cell->stretch_top_height;
        }
    }

    return cell->height[GET_MAP_LAYER(LAYER_FLOOR, sub_layer)];
}

/**
 * Align tile stretch based on X/Y.
 * @param x
 * X position.
 * @param y
 * Y position.
 * @param w
 * Max width.
 * @param h
 * Max height.
 * @param sub_layer
 * Sub-layer.
 */
static void align_tile_stretch(int x, int y, int w, int h, int sub_layer) {
    int top, bottom, right, left, min_ht;
    int32_t stretch;
    int nw_height, n_height, ne_height, sw_height, s_height, se_height, w_height, e_height,
        my_height;

    if (x < 0 || y < 0 || x >= w || y >= h) {
        return;
    }

    my_height = map_cell_stretch_height(x, y, w, h, sub_layer, 0);
    nw_height = map_cell_stretch_height(x - 1, y - 1, w, h, sub_layer, my_height);
    n_height = map_cell_stretch_height(x, y - 1, w, h, sub_layer, my_height);
    ne_height = map_cell_stretch_height(x + 1, y - 1, w, h, sub_layer, my_height);
    sw_height = map_cell_stretch_height(x - 1, y + 1, w, h, sub_layer, my_height);
    s_height = map_cell_stretch_height(x, y + 1, w, h, sub_layer, my_height);
    se_height = map_cell_stretch_height(x + 1, y + 1, w, h, sub_layer, my_height);
    w_height = map_cell_stretch_height(x - 1, y, w, h, sub_layer, my_height);
    e_height = map_cell_stretch_height(x + 1, y, w, h, sub_layer, my_height);

    if (abs(my_height - e_height) > MAX_STRETCH) {
        e_height = my_height;
    }

    if (abs(my_height - se_height) > MAX_STRETCH_DIAG) {
        se_height = my_height;
    }

    if (abs(my_height - s_height) > MAX_STRETCH) {
        s_height = my_height;
    }

    if (abs(my_height - sw_height) > MAX_STRETCH_DIAG) {
        sw_height = my_height;
    }

    if (abs(my_height - w_height) > MAX_STRETCH) {
        w_height = my_height;
    }

    if (abs(my_height - nw_height) > MAX_STRETCH_DIAG) {
        nw_height = my_height;
    }

    if (abs(my_height - n_height) > MAX_STRETCH) {
        n_height = my_height;
    }

    if (abs(my_height - ne_height) > MAX_STRETCH_DIAG) {
        ne_height = my_height;
    }

    top = MAX(w_height, nw_height);
    top = MAX(top, n_height);
    top = MAX(top, my_height);

    bottom = MAX(s_height, se_height);
    bottom = MAX(bottom, e_height);
    bottom = MAX(bottom, my_height);

    right = MAX(n_height, ne_height);
    right = MAX(right, e_height);
    right = MAX(right, my_height);

    left = MAX(w_height, sw_height);
    left = MAX(left, s_height);
    left = MAX(left, my_height);

    min_ht = MIN(top, bottom);
    min_ht = MIN(min_ht, left);
    min_ht = MIN(min_ht, right);
    min_ht = MIN(min_ht, my_height);

    if (my_height < 0 && left == 0 && right == 0 && top == 0 && bottom == 0) {
        int top2 = MIN(w_height, nw_height);
        top2 = MIN(top2, n_height);
        top2 = MIN(top2, my_height);

        int bottom2 = MIN(s_height, se_height);
        bottom2 = MIN(bottom2, e_height);
        bottom2 = MIN(bottom2, my_height);

        int right2 = MIN(n_height, ne_height);
        right2 = MIN(right2, e_height);
        right2 = MIN(right2, my_height);

        int left2 = MIN(w_height, sw_height);
        left2 = MIN(left2, s_height);
        left2 = MIN(left2, my_height);

        top = top2 - top;
        bottom = bottom2 - bottom;
        right = right2 - right;
        left = left2 - left;

        min_ht = MIN(top, bottom);
        min_ht = MIN(min_ht, left);
        min_ht = MIN(min_ht, right);
        min_ht = MIN(min_ht, my_height);

        min_ht = abs(min_ht);
        top = abs(top);
        bottom = abs(bottom);
        left = abs(left);
        right = abs(right);
    }

    /* Normalize these... */
    top -= min_ht;
    bottom -= min_ht;
    left -= min_ht;
    right -= min_ht;

    stretch = abs(bottom) + (abs(left) << 8) + (abs(right) << 16) + (abs(top) << 24);
    map_cache_cell(cells, x, y)->stretch[sub_layer] = stretch;
}

/**
 * Adjust the tile stretch of a map.
 *
 * Scans the visible window and updates only cells marked dirty by incremental
 * map changes. A tile's stretch depends on its eight neighbors, so the setter
 * propagates dirtiness to that complete neighborhood.
 */
void adjust_tile_stretch(void) {
    int xoff, yoff, w, h, x, y, sub_layer;

    xoff = map_width * (MAP_FOW_SIZE / 2);
    yoff = map_height * (MAP_FOW_SIZE / 2);
    w = map_width * MAP_FOW_SIZE;
    h = map_height * MAP_FOW_SIZE;

    for (x = xoff; x < xoff + map_width; x++) {
        for (y = yoff; y < yoff + map_height; y++) {
            struct MapCell *cell = MAP_CELL_GET(x, y);
            if (!cell->stretch_dirty) {
                continue;
            }

            for (sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
                align_tile_stretch(x, y, w, h, sub_layer);
            }
            cell->stretch_dirty = 0;
        }
    }
}

/** Mark every stretch result affected by one changed map cell. */
static void map_mark_stretch_dirty(int x, int y) {
    int cache_width = map_width * MAP_FOW_SIZE;
    int cache_height = map_height * MAP_FOW_SIZE;
    int cache_x = x + map_width * (MAP_FOW_SIZE / 2);
    int cache_y = y + map_height * (MAP_FOW_SIZE / 2);

    for (int neighbor_x = cache_x - 1; neighbor_x <= cache_x + 1; neighbor_x++) {
        for (int neighbor_y = cache_y - 1; neighbor_y <= cache_y + 1; neighbor_y++) {
            if (neighbor_x >= 0 && neighbor_x < cache_width && neighbor_y >= 0 &&
                neighbor_y < cache_height) {
                MAP_CELL_GET(neighbor_x, neighbor_y)->stretch_dirty = 1;
            }
        }
    }
}

/** Refresh the floor-only geometry summary used by the tilestretcher. */
static void map_update_stretch_geometry(struct MapCell *cell) {
    cell->stretch_top_height = 0;
    cell->stretch_upper_height = 0;
    cell->level_support_height = 0;

    for (int sub_layer = NUM_SUB_LAYERS - 1; sub_layer >= 0; sub_layer--) {
        int16_t height = cell->height[GET_MAP_LAYER(LAYER_FLOOR, sub_layer)];
        cell->level_support_height = MAX(cell->level_support_height, height);
        if (height == 0) {
            continue;
        }

        if (cell->stretch_top_height == 0) {
            cell->stretch_top_height = height;
        }

        if (sub_layer != 0 && cell->stretch_upper_height == 0) {
            cell->stretch_upper_height = height;
        }
    }
}

/** Refresh the maximum elevation used for whole-cell screen rejection. */
static void map_update_render_height(struct MapCell *cell) {
    cell->render_max_height = 0;

    for (int sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
        cell->render_max_height =
            MAX(cell->render_max_height, cell->height[GET_MAP_LAYER(LAYER_FLOOR, sub_layer)]);
        cell->render_max_height =
            MAX(cell->render_max_height, cell->height[GET_MAP_LAYER(LAYER_EFFECT, sub_layer)]);
    }
}

/**
 * Set data for map cell.
 *
 * If FOW was previously set on this cell, cell data is cleared.
 * @param x
 * X of the cell.
 * @param y
 * Y of the cell.
 * @param layer
 * Layer we're doing this for.
 * @param face
 * Face to set.
 * @param quick_pos
 * Is this a multi-arch?
 * @param obj_flags
 * Flags.
 * @param name
 * Player's name.
 * @param name_color
 * Player's name color.
 * @param height
 * Z position of the tile.
 * @param probe
 * Target's HP bar.
 * @param zoom
 * How much to zoom the face by.
 * @param align
 * X align.
 * @param rotate
 * Rotation in degrees.
 * @param infravision
 * Whether to show the object in red.
 */
void map_set_data(int x,
                  int y,
                  int layer,
                  int16_t face,
                  uint8_t quick_pos,
                  uint8_t obj_flags,
                  const char *name,
                  const char *name_color,
                  int16_t height,
                  uint8_t probe,
                  int16_t zoom_x,
                  int16_t zoom_y,
                  int16_t align,
                  uint8_t draw_double,
                  uint8_t alpha,
                  int16_t rotate,
                  uint8_t infravision,
                  uint32_t target_object_count,
                  uint8_t target_is_friend,
                  uint8_t anim_speed,
                  uint8_t anim_facing,
                  uint8_t anim_flags,
                  uint8_t anim_state,
                  uint8_t priority,
                  uint8_t secondpass,
                  uint8_t roof,
                  uint8_t door,
                  const char *glow,
                  uint8_t glow_speed) {
    struct MapCell *cell;
    int sub_layer;

    cell = MAP_CELL_GET_MIDDLE(x, y);
    bool stretch_geometry_reset = cell->fow != 0 && cell->structural_fow == 0;
    sub_layer = layer / NUM_LAYERS;
    int object_layer = (layer % NUM_LAYERS) + 1;
    bool stretch_geometry_changed =
        stretch_geometry_reset || (object_layer == LAYER_FLOOR &&
                                   (cell->faces[layer] != face || cell->height[layer] != height));
    bool lighting_geometry_changed = object_layer == LAYER_FLOOR &&
                                     (cell->faces[layer] != face || cell->height[layer] != height);
    bool render_height_changed =
        stretch_geometry_reset || ((object_layer == LAYER_FLOOR || object_layer == LAYER_EFFECT) &&
                                   cell->height[layer] != height);

    if (cell->fow && !cell->structural_fow) {
        int i;

        cell->fow = 0;
        cell->structural_fow = 0;

        for (i = 0; i < NUM_REAL_LAYERS; i++) {
            cell->faces[i] = 0;
            cell->flags[i] = 0;
            cell->roof[i] = 0;
            cell->quick_pos[i] = 0;
            cell->height[i] = 0;
            cell->zoom_x[i] = 0;
            cell->zoom_y[i] = 0;
            cell->align[i] = 0;
            cell->rotate[i] = 0;
            cell->infravision[i] = 0;
            cell->anim_last[i] = 0;
            cell->anim_speed[i] = 0;
            cell->anim_facing[i] = 0;
            cell->anim_state[i] = 0;
        }

        for (i = 0; i < NUM_SUB_LAYERS; i++) {
            cell->anim_flags[i] = 0;
            cell->priority[i] = 0;
            cell->secondpass[i] = 0;
            cell->door[i] = 0;
            cell->probe[i] = 0;
            cell->target_object_count[i] = 0;
            cell->target_is_friend[i] = 0;
            cell->pname[i][0] = '\0';
            cell->pcolor[i][0] = '\0';
        }
    }

    if (anim_speed != 0 && cell->faces[layer] != face) {
        cell->anim_state[layer] = 0;
    }

    uint8_t object_layer_mask = UINT8_C(1) << (object_layer - 1);
    cell->priority[sub_layer] &= ~object_layer_mask;
    cell->secondpass[sub_layer] &= ~object_layer_mask;
    if (priority) {
        cell->priority[sub_layer] |= object_layer_mask;
    }
    if (secondpass) {
        cell->secondpass[sub_layer] |= object_layer_mask;
    }

    cell->faces[layer] = face;
    cell->flags[layer] = obj_flags;
    cell->roof[layer] = roof;
    cell->door[sub_layer] &= ~object_layer_mask;
    if (door) {
        cell->door[sub_layer] |= object_layer_mask;
    }

    cell->quick_pos[layer] = quick_pos;

    snprintf(VS(cell->glow[layer]), "%s", glow);

    cell->height[layer] = height;
    cell->zoom_x[layer] = zoom_x;
    cell->zoom_y[layer] = zoom_y;
    cell->align[layer] = align;
    cell->draw_double[layer] = draw_double;
    cell->alpha[layer] = alpha;
    cell->rotate[layer] = rotate;
    cell->infravision[layer] = infravision;
    cell->glow_speed[layer] = glow_speed;

    if (stretch_geometry_changed) {
        map_update_stretch_geometry(cell);
        map_mark_stretch_dirty(x, y);
    }

    if (render_height_changed) {
        map_update_render_height(cell);
    }

    cell->anim_speed[layer] = anim_speed;
    cell->anim_facing[layer] = anim_facing;

    if (object_layer == LAYER_LIVING) {
        if (cell->target_object_count[sub_layer] != target_object_count ||
            cell->target_is_friend[sub_layer] != target_is_friend) {
            cpl.target_object_index = 0;
        }

        cell->probe[sub_layer] = probe;
        cell->target_object_count[sub_layer] = target_object_count;
        cell->target_is_friend[sub_layer] = target_is_friend;
        snprintf(VS(cell->pcolor[sub_layer]), "%s", name_color);
        snprintf(VS(cell->pname[sub_layer]), "%s", name);

        if (anim_flags & ANIM_FLAG_ATTACKING &&
            !(cell->anim_flags[sub_layer] & ANIM_FLAG_ATTACKING)) {
            cell->anim_state[layer] = 0;
        } else if (anim_flags & ANIM_FLAG_MOVING &&
                   !(cell->anim_flags[sub_layer] & ANIM_FLAG_MOVING)) {
            cell->anim_state[layer] = anim_state;
        }

        cell->anim_flags[sub_layer] = anim_flags;
    }

    if (anim_speed != 0) {
        if (!check_animation_status(face)) {
            cell->faces[layer] = 0;
            cell->anim_speed[layer] = 0;
        }
    } else {
        image_request_face(face);
    }

    if (lighting_geometry_changed) {
        level_lighting_revision[current_level_index]++;
    }
}

/**
 * Clear map's cell.
 *
 * In reality, this only clears some data on the cell, and sets the FOW flag
 * to mark that the cell is actually FOW.
 * @param x X of the cell.
 * @param y Y of the cell.
 * @param hard Whether to discard cached geometry instead of retaining FOW.
 */
void map_clear_cell(int x, int y, bool hard) {
    struct MapCell *cell;
    cell = MAP_CELL_GET_MIDDLE(x, y);
    bool had_known_light = false;
    for (size_t sub_layer = 0; sub_layer < arraysize(cell->light_known); sub_layer++) {
        had_known_light |= cell->light_known[sub_layer] != 0;
    }

    if (hard) {
        bool had_floor_geometry = false;
        for (int sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
            had_floor_geometry |= cell->faces[GET_MAP_LAYER(LAYER_FLOOR, sub_layer)] != 0;
        }

        memset(cell, 0, sizeof(*cell));
        cell->fow = 1;

        if (had_floor_geometry) {
            map_mark_stretch_dirty(x, y);
        }

        if (had_known_light || had_floor_geometry) {
            level_lighting_revision[current_level_index]++;
        }

        return;
    }

    cell->fow = 1;
    cell->structural_fow = 0;
    memset(cell->light_known, 0, sizeof(cell->light_known));

    for (int sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
        cell->probe[sub_layer] = 0;
        cell->target_object_count[sub_layer] = 0;
        cell->target_is_friend[sub_layer] = 0;
        cell->pname[sub_layer][0] = '\0';
        cell->pcolor[sub_layer][0] = '\0';
    }

    if (had_known_light) {
        level_lighting_revision[current_level_index]++;
    }
}

/** Store base-map elevation needed to project independently cached upper levels. */
void map_set_structural_support_height(int x, int y, int16_t height) {
    struct MapCell *cell = MAP_CELL_GET_MIDDLE(x, y);

    if (cell->structural_support_height == height) {
        return;
    }

    cell->structural_support_height = height;
    level_lighting_revision[current_level_index]++;
}

/** Apply an explicit server visibility state after a tile's layer deltas. */
void map_set_fow(int x, int y, bool fow) {
    struct MapCell *cell = MAP_CELL_GET_MIDDLE(x, y);

    if ((cell->fow != 0) == fow && (cell->structural_fow != 0) == fow) {
        return;
    }

    cell->fow = fow;
    cell->structural_fow = fow;
    level_lighting_revision[current_level_index]++;
}

/** Return the currently cached visibility state for one map tile. */
bool map_get_fow(int x, int y) {
    return MAP_CELL_GET_MIDDLE(x, y)->fow != 0;
}

/**
 * Set normalized light level for a map cell.
 * @param x
 * X of the cell.
 * @param y
 * Y of the cell.
 * @param sub_layer
 * Sub-layer.
 * @param light_level
 * Light level to set: zero is unlit and 255 is fully lit.
 */
void map_set_light_level(int x, int y, int sub_layer, uint8_t light_level) {
    struct MapCell *cell;

    cell = MAP_CELL_GET_MIDDLE(x, y);
    bool changed = !cell->light_known[sub_layer] || cell->light_level[sub_layer] != light_level;
    cell->light_level[sub_layer] = light_level;
    cell->light_known[sub_layer] = 1;
    if (changed) {
        level_lighting_revision[current_level_index]++;
    }
}

/**
 * Get the height of the topmost floor on the specified square.
 * @param x
 * X position.
 * @param y
 * Y position.
 * @return
 * The height.
 */
static int get_top_floor_height(struct MapCell *cell, int sub_layer) {
    int16_t height;

    height = cell->height[GET_MAP_LAYER(LAYER_FLOOR, sub_layer)];

    return MAX(0, height);
}

static void map_animate_object(struct MapCell *cell, int layer) {
    Animations *animation;

    if (cell->faces[layer] == 0 || cell->anim_speed[layer] == 0 || cell->anim_facing[layer] == 0) {
        return;
    }

    animation = animation_get(cell->faces[layer]);
    if (animation == NULL) {
        cell->faces[layer] = 0;
        cell->anim_speed[layer] = 0;
        return;
    }

    if (!(cell->flags[layer] & FFLAG_SLEEP) && !(cell->flags[layer] & FFLAG_PARALYZED)) {
        cell->anim_state[layer]++;
        map_redraw_flag = 1;
    }

    /* If beyond drawable states, reset */
    if (cell->anim_state[layer] >= animation->frame) {
        cell->anim_state[layer] = 0;
    }
}

void map_animate(void) {
    int x, y, layer;
    struct MapCell *cell;

    for (int depth = -MAP2_MAX_DEPTH; depth <= MAP2_MAX_DEPTH; depth++) {
        if (!(map_level_mask & (UINT16_C(1) << MAP2_DEPTH_INDEX(depth))) ||
            !map_select_level(depth, false)) {
            continue;
        }

        for (x = 0; x < map_width; x++) {
            for (y = 0; y < map_height; y++) {
                cell = MAP_CELL_GET_MIDDLE(x, y);

                if (cell->fow) {
                    continue;
                }

                for (layer = 0; layer < NUM_REAL_LAYERS; layer++) {
                    if (cell->glow_speed[layer] > 1) {
                        cell->glow_state[layer]++;
                        map_redraw_flag = 1;

                        if (cell->glow_state[layer] > cell->glow_speed[layer]) {
                            cell->glow_state[layer] = 0;
                        }
                    }

                    if (cell->anim_speed[layer] == 0) {
                        continue;
                    }

                    if (cell->anim_last[layer] >= cell->anim_speed[layer]) {
                        map_animate_object(cell, layer);
                        cell->anim_last[layer] = 1;
                    } else {
                        cell->anim_last[layer]++;
                    }
                }
            }
        }
    }

    map_select_level(0, true);
}

static uint16_t map_object_get_face(struct MapCell *cell, int layer) {
    int sub_layer, dir;
    Animations *animation;
    uint16_t face;

    if (cell->anim_speed[layer] == 0) {
        return cell->faces[layer];
    }

    animation = animation_get(cell->faces[layer]);
    if (animation == NULL || cell->anim_facing[layer] == 0) {
        return 0;
    }

    sub_layer = layer / NUM_LAYERS;
    dir = cell->anim_facing[layer] - 1;

    if (animation->facings >= 25) {
        if (cell->anim_flags[sub_layer] & ANIM_FLAG_ATTACKING) {
            dir += 16;
        } else if (cell->anim_flags[sub_layer] & ANIM_FLAG_MOVING) {
            dir += 8;
        }
    }

    return animation_get_face(cell->faces[layer], dir, cell->anim_state[layer], &face) ? face : 0;
}

/** Deferred UI annotation associated with a rendered map object. */
typedef struct map_annotation {
    struct MapCell *cell;
    sprite_effects_t effects;
    int32_t xl;
    int32_t yl;
    int32_t xoff;
    int32_t xoff2;
    int32_t xlen;
    int32_t bitmap_w;
    uint8_t map_layer;
    uint8_t sub_layer;
} map_annotation_t;

/** One sprite deferred into the unified isometric painter order. */
typedef struct map_render_command {
    SDL_Surface *source;
    sprite_effects_t effects;
    int32_t x;
    int32_t y;
    int32_t bounds_x;
    int32_t bounds_y;
    int32_t bounds_w;
    int32_t bounds_h;
    int32_t sort_x;
    int32_t sort_y;
    int16_t tile_x;
    int16_t tile_y;
    size_t sequence;
    uint8_t object_layer;
    int8_t depth;
    bool draw_double;
    bool door;
    bool door_hint;
    bool transformed;
} map_render_command_t;

/** Output accumulated while traversing independently cached map levels. */
typedef struct map_render_context {
    map_render_command_t *commands;
    map_annotation_t *annotations;
    SDL_Rect *tiles;
    size_t commands_num;
    size_t commands_capacity;
    size_t annotations_num;
    size_t annotations_capacity;
    size_t tiles_num;
    size_t tiles_capacity;
    struct MapCell *target_cell;
    SDL_Rect target_rect;
    uint8_t target_sub_layer;
} map_render_context_t;

/**
 * Structure used to pass data between the rendering loops in map_draw_map()
 * and the actual rendering logic in draw_map_object().
 *
 * Try to keep this structure aligned whenever extending it.
 */
typedef struct map_render_data {
    int16_t x; ///< X index in the cells array.
    int16_t y; ///< Y index in the cells array.

    int16_t midx; ///< X index in the cells array of the middlemost cell.
    int16_t midy; ///< Y index in the cells array of the middlemost cell.

    int32_t xpos; ///< X coordinate where to render.
    int32_t ypos; ///< Y coordinate where to render.
    int32_t player_height_offset; ///< Player height offset.
    int32_t level_support_height; ///< Ground elevation supporting an upper level.

    struct MapCell *cell; ///< Cell that is being rendered.
    map_render_context_t *render_context; ///< Unified output for every physical level.

    uint8_t layer; ///< Layer to render on.
    uint8_t sub_layer; ///< Sub-layer to render on.
    uint8_t alpha_forced; ///< Force applying the specified alpha value.
    bool smooth_lighting; ///< Whether smooth world lighting is enabled.
    bool lightmap_pending; ///< Whether the ground lightmap has not been composited yet.
    bool defer_rendering; ///< Queue this sprite in the global painter order.
    bool world_surface; ///< Whether this is a world-rendering surface.
    bool primary_level; ///< Whether this is the player's physical level.
    int8_t depth; ///< Linked-map depth relative to the player.
} map_render_data_t;

/**
 * Draw a single object on the map.
 *
 * @param surface
 * Surface to render on.
 * @param data
 * Rendering data. May be modified.
 */
static void draw_map_object(SDL_Surface *surface, map_render_data_t *data) {
    HARD_ASSERT(surface != NULL);
    HARD_ASSERT(data != NULL);

    uint8_t map_layer = GET_MAP_LAYER(data->layer, data->sub_layer);
    uint16_t face = map_object_get_face(data->cell, map_layer);
    if (face == 0 || face >= MAX_FACE_TILES) {
        return;
    }

    sprite_struct *face_sprite = image_get_sprite(face);
    if (face_sprite == NULL || face_sprite->bitmap == NULL) {
        return;
    }

    /* When rendering on the map surface, avoid rendering the object
     * when it's too high up and either in the FoW or the map has the
     * "height difference" feature enabled. */
    if (data->world_surface && (data->cell->fow || MapData.height_diff) &&
        abs(get_top_floor_height(data->cell, data->sub_layer) - data->player_height_offset) >
            HEIGHT_MAX_RENDER) {
        return;
    }

    int bitmap_h = face_sprite->bitmap->h;
    int bitmap_w = face_sprite->bitmap->w;

    sprite_effects_t effects = {0};
    effects.rotate = data->cell->rotate[map_layer];
    effects.zoom_x = data->cell->zoom_x[map_layer];
    effects.zoom_y = data->cell->zoom_y[map_layer];

    if (effects.rotate != 0) {
        rotozoomSurfaceSizeXY(bitmap_w,
                              bitmap_h,
                              effects.rotate,
                              effects.zoom_x != 0 ? effects.zoom_x / 100.0 : 1.0,
                              effects.zoom_y != 0 ? effects.zoom_y / 100.0 : 1.0,
                              &bitmap_w,
                              &bitmap_h);
    } else if ((effects.zoom_x != 0 && effects.zoom_x != 100) ||
               (effects.zoom_y != 0 && effects.zoom_y != 100)) {
        zoomSurfaceSize(bitmap_w,
                        bitmap_h,
                        effects.zoom_x != 0 ? effects.zoom_x / 100.0 : 1.0,
                        effects.zoom_y != 0 ? effects.zoom_y / 100.0 : 1.0,
                        &bitmap_w,
                        &bitmap_h);
    }

    int xlen;
    int xoff;
    int yl;
    int xl;
    /* Multi-part object? */
    if (data->cell->quick_pos[map_layer]) {
        uint8_t mnr = data->cell->quick_pos[map_layer];
        uint8_t mid = mnr >> 4;
        mnr &= 0x0f;
        xlen = MultiArchs[mid].xlen;
        yl = data->ypos - MultiArchs[mid].part[mnr].yoff + MultiArchs[mid].ylen - bitmap_h;

        /* Center overlapping X borders */
        xl = 0;
        if (bitmap_w > MultiArchs[mid].xlen) {
            xl = (MultiArchs[mid].xlen - bitmap_w) >> 1;
        }

        xoff = data->xpos - MultiArchs[mid].part[mnr].xoff;
        xl += xoff;
    } else {
        /* Calculate offsets */
        xlen = MAP_TILE_POS_XOFF;
        yl = (data->ypos + MAP_TILE_POS_YOFF) - bitmap_h;
        xoff = xl = data->xpos;

        if (bitmap_w > MAP_TILE_POS_XOFF) {
            xl -= (bitmap_w - MAP_TILE_POS_XOFF) / 2;
        }
    }

    xl += data->cell->align[map_layer];

    snprintf(VS(effects.glow), "%s", data->cell->glow[map_layer]);
    effects.glow_speed = data->cell->glow_speed[map_layer];
    effects.glow_state = data->cell->glow_state[map_layer];

    if (effect_has_overlay()) {
        BIT_SET(effects.flags, SPRITE_FLAG_EFFECTS);
    }

    if (data->cell->fow && (!data->cell->structural_fow || data->layer <= LAYER_FMASK)) {
        BIT_SET(effects.flags, SPRITE_FLAG_FOW);
    } else if (data->cell->infravision[map_layer]) {
        BIT_SET(effects.flags, SPRITE_FLAG_RED);
    } else if (data->cell->flags[map_layer] & FFLAG_INVISIBLE) {
        BIT_SET(effects.flags, SPRITE_FLAG_GRAY);
    } else if (data->smooth_lighting && !data->lightmap_pending && data->layer == LAYER_WALL) {
        if (data->cell->roof[map_layer]) {
            BIT_SET(effects.flags, SPRITE_FLAG_SMOOTH_DARK_SURFACE);
        } else {
            BIT_SET(effects.flags, SPRITE_FLAG_SMOOTH_DARK);
            effects.smooth_dark_y =
                data->ypos + MAP_TILE_POS_YOFF -
                data->cell->height[GET_MAP_LAYER(LAYER_FLOOR, data->sub_layer)] +
                data->player_height_offset;
        }
    } else if (!data->lightmap_pending) {
        BIT_SET(effects.flags, SPRITE_FLAG_DARK);
    }

    if (!data->world_surface) {
        BITMASK_CLEAR(effects.flags, BIT_MASK(SPRITE_FLAG_RED) | BIT_MASK(SPRITE_FLAG_FOW));
        BIT_SET(effects.flags, SPRITE_FLAG_DARK);
    }

    if (BIT_QUERY(effects.flags, SPRITE_FLAG_DARK)) {
        effects.dark_level =
            (UINT8_MAX - data->cell->light_level[data->sub_layer]) * DARK_LEVELS / UINT8_MAX;
    }

    effects.alpha = data->cell->alpha[map_layer];

    if (data->alpha_forced != 0) {
        if (effects.alpha != 0) {
            effects.alpha = MIN(effects.alpha, data->alpha_forced);
        } else {
            effects.alpha = data->alpha_forced;
        }
    }

    /* Stretch floor and floor mask layers. */
    if (data->layer <= LAYER_FMASK) {
        effects.stretch = data->cell->stretch[data->sub_layer];
    }

    if (data->layer == LAYER_LIVING || data->layer == LAYER_EFFECT || data->layer == LAYER_ITEM ||
        data->layer == LAYER_ITEM2) {
        yl -= get_top_floor_height(data->cell, data->sub_layer);
    } else {
        yl -= data->cell->height[GET_MAP_LAYER(LAYER_FLOOR, data->sub_layer)];
    }

    yl += data->player_height_offset;

    /* Move the object up/down depending on its height, but only for
     * non-floor layers. */
    if (data->layer > LAYER_FLOOR) {
        yl -= data->cell->height[map_layer];
    }

    if (data->defer_rendering) {
        map_render_context_t *context = data->render_context;
        HARD_ASSERT(context != NULL);
        if (context->commands_num == context->commands_capacity) {
            context->commands_capacity =
                context->commands_capacity == 0 ? 256 : context->commands_capacity * 2;
            context->commands = xreallocarray(context->commands,
                                              context->commands_capacity,
                                              sizeof(*context->commands));
        }
        bool transformed = effects.rotate != 0 || (effects.zoom_x != 0 && effects.zoom_x != 100) ||
                           (effects.zoom_y != 0 && effects.zoom_y != 100);
        int bounds_x = xl;
        int bounds_y = yl;
        int bounds_w = bitmap_w;
        int bounds_h = bitmap_h;
        if (!transformed) {
            bounds_x += face_sprite->border_left;
            bounds_y += face_sprite->border_up;
            bounds_w -= face_sprite->border_left + face_sprite->border_right;
            bounds_h -= face_sprite->border_up + face_sprite->border_down;
        }
        if (data->cell->draw_double[map_layer]) {
            bounds_y -= 22;
            bounds_h += 22;
        }

        context->commands[context->commands_num] = (map_render_command_t){
            .source = face_sprite->bitmap,
            .effects = effects,
            .x = xl,
            .y = yl,
            .bounds_x = bounds_x,
            .bounds_y = bounds_y,
            .bounds_w = MAX(1, bounds_w),
            .bounds_h = MAX(1, bounds_h),
            .sort_x = data->xpos,
            /* Preserve the legacy world-tile traversal from the top corner
             * down. The physical level's 46-pixel display lift must not move
             * that tile earlier or later in painter order; levels sharing the
             * same world diagonal retain their low-to-high queue sequence. */
            .sort_y =
                data->ypos + data->depth * MAP_LEVEL_PIXEL_HEIGHT + data->level_support_height,
            .sequence = context->commands_num,
            .tile_x = data->x,
            .tile_y = data->y,
            .object_layer = data->layer,
            .depth = data->depth,
            .draw_double = data->cell->draw_double[map_layer],
            .door = (data->cell->door[data->sub_layer] & (UINT8_C(1) << (data->layer - 1))) != 0,
            .transformed = transformed,
        };
        context->commands_num++;
    } else {
        surface_show_effects(surface, xl, yl, NULL, face_sprite->bitmap, &effects);

        /* Double faces are shown twice, one above the other, when not lower
         * on the screen than the player. This simulates high walls without
         * obscuring the user's view. */
        if (data->cell->draw_double[map_layer]) {
            surface_show_effects(surface, xl, yl - 22, NULL, face_sprite->bitmap, &effects);
        }
    }

    /* Rest of the code deals with rendering on the map widget. */
    if (!data->world_surface) {
        return;
    }

    int xoff2;
    if (xlen == MAP_TILE_POS_XOFF) {
        xoff2 = (int)(((double)xlen / 100.0) * 25.0);
    } else {
        xoff2 = (int)(((double)xlen / 100.0) * 20.0);
    }

    if ((data->layer == LAYER_LIVING && data->cell->pname[data->sub_layer][0] != '\0') ||
        data->cell->flags[map_layer] != 0) {
        map_render_context_t *context = data->render_context;
        if (context->annotations_num == context->annotations_capacity) {
            context->annotations_capacity =
                context->annotations_capacity == 0 ? 64 : context->annotations_capacity * 2;
            context->annotations = xreallocarray(context->annotations,
                                                 context->annotations_capacity,
                                                 sizeof(*context->annotations));
        }
        map_annotation_t *annotation = &context->annotations[context->annotations_num++];
        *annotation = (map_annotation_t){
            .cell = data->cell,
            .xl = xl,
            .yl = yl,
            .xoff = xoff,
            .xoff2 = xoff2,
            .xlen = xlen,
            .bitmap_w = bitmap_w,
            .map_layer = map_layer,
            .sub_layer = data->sub_layer,
        };
        annotation->effects.alpha = effects.alpha;
        annotation->effects.stretch = effects.stretch;
        annotation->effects.zoom_x = effects.zoom_x;
        annotation->effects.zoom_y = effects.zoom_y;
        annotation->effects.rotate = effects.rotate;
    }

    if (data->layer == LAYER_FLOOR && tiles_debug) {
        map_render_context_t *context = data->render_context;
        if (context->tiles_num == context->tiles_capacity) {
            context->tiles_capacity =
                context->tiles_capacity == 0 ? 128 : context->tiles_capacity * 2;
            context->tiles =
                xreallocarray(context->tiles, context->tiles_capacity, sizeof(*context->tiles));
        }
        context->tiles[context->tiles_num].x = xl;
        context->tiles[context->tiles_num].y = yl;
        context->tiles[context->tiles_num].w = data->x;
        context->tiles[context->tiles_num].h = data->y;
        context->tiles_num++;
    }

    if (data->primary_level && data->layer == LAYER_LIVING && !data->cell->fow &&
        data->cell->probe[data->sub_layer] != 0) {
        map_render_context_t *context = data->render_context;
        context->target_cell = data->cell;
        context->target_sub_layer = data->sub_layer;
        context->target_rect.x = xoff + xoff2;
        context->target_rect.y = yl - 9;
        context->target_rect.w = (xlen - xoff2 * 2);
        context->target_rect.h = 1;
    }
}

/** Draw names and status icons after world lighting has been composited. */
static void map_draw_annotations(SDL_Surface *surface, map_render_context_t *context) {
    HARD_ASSERT(surface != NULL);
    HARD_ASSERT(context != NULL);

    for (size_t i = 0; i < context->annotations_num; i++) {
        map_annotation_t *annotation = &context->annotations[i];
        struct MapCell *cell = annotation->cell;
        uint8_t map_layer = annotation->map_layer;
        uint8_t sub_layer = annotation->sub_layer;

        if ((map_layer % NUM_LAYERS) + 1 == LAYER_LIVING && cell->pname[sub_layer][0] != '\0' &&
            setting_get_int(OPT_CAT_MAP, OPT_PLAYER_NAMES)) {
            bool draw_name = false;
            char *name = cell->pname[sub_layer];

            if (setting_get_int(OPT_CAT_MAP, OPT_PLAYER_NAMES) == 1) {
                draw_name = true;
            } else if (setting_get_int(OPT_CAT_MAP, OPT_PLAYER_NAMES) == 2) {
                draw_name = cell->target_object_count[sub_layer] != 0;
            } else if (setting_get_int(OPT_CAT_MAP, OPT_PLAYER_NAMES) == 3) {
                draw_name = cell->target_object_count[sub_layer] == 0;
            }

            if (draw_name) {
                text_show(surface,
                          FONT_SANS9,
                          name,
                          annotation->xoff + annotation->xoff2 +
                              (annotation->xlen - annotation->xoff2 * 2) / 2 -
                              text_get_width(FONT_SANS9, name, 0) / 2 - 2,
                          annotation->yl - 24,
                          cell->pcolor[sub_layer],
                          TEXT_OUTLINE,
                          NULL);
            }
        }

        if (cell->flags[map_layer] & FFLAG_SLEEP) {
            surface_show_effects(surface,
                                 annotation->xl + annotation->bitmap_w / 2,
                                 annotation->yl - 5,
                                 NULL,
                                 TEXTURE_CLIENT("sleep"),
                                 &annotation->effects);
        }

        if (cell->flags[map_layer] & FFLAG_CONFUSED) {
            surface_show_effects(surface,
                                 annotation->xl + annotation->bitmap_w / 2 - 1,
                                 annotation->yl - 4,
                                 NULL,
                                 TEXTURE_CLIENT("confused"),
                                 &annotation->effects);
        }

        if (cell->flags[map_layer] & FFLAG_SCARED) {
            surface_show_effects(surface,
                                 annotation->xl + annotation->bitmap_w / 2 + 10,
                                 annotation->yl - 4,
                                 NULL,
                                 TEXTURE_CLIENT("scared"),
                                 &annotation->effects);
        }

        if (cell->flags[map_layer] & FFLAG_BLINDED) {
            surface_show_effects(surface,
                                 annotation->xl + annotation->bitmap_w / 2 + 3,
                                 annotation->yl - 6,
                                 NULL,
                                 TEXTURE_CLIENT("blind"),
                                 &annotation->effects);
        }

        if (cell->flags[map_layer] & FFLAG_PARALYZED) {
            surface_show_effects(surface,
                                 annotation->xl + annotation->bitmap_w / 2 + 3,
                                 annotation->yl + 3,
                                 NULL,
                                 TEXTURE_CLIENT("paralyzed"),
                                 &annotation->effects);
        }
    }

    free(context->annotations);
    context->annotations = NULL;
    context->annotations_num = 0;
}

/**
 * Calculates whether the specified coordinates are behind a wall.
 *
 * @param dx
 * Start X.
 * @param dy
 * Start Y.
 * @param sx
 * End X.
 * @param sy
 * End Y.
 * @return
 * Whether the coordinates @p dx and @p dy are behind a wall or not.
 */
static bool obj_is_behind_wall(int dx, int dy, int sx, int sy) {
    int fraction, dx2, dy2, stepx, stepy;
    int x = sx, y = sy;
    int distance_x = dx - sx;
    int distance_y = dy - sy;

    BRESENHAM_INIT(distance_x, distance_y, fraction, stepx, stepy, dx2, dy2);

    while (1) {
        if (x == dx && y == dy) {
            return false;
        }

        if (x < 0 || x >= map_width || y < 0 || y >= map_height) {
            return false;
        }

        for (int sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
            MapCell *cell = MAP_CELL_GET_MIDDLE(x, y);

            if (cell->faces[GET_MAP_LAYER(LAYER_WALL, sub_layer)] != 0) {
                return true;
            }
        }

        BRESENHAM_STEP(x, y, fraction, stepx, stepy, dx2, dy2);
    }
}

/** Return the base-map elevation that supports a linked level at one tile. */
static int map_level_support_height(int x, int y, int depth) {
    if (depth <= 0) {
        return 0;
    }

    struct MapCell *base_cells = level_cells[MAP2_DEPTH_INDEX(0)];
    int cache_width = map_width * MAP_FOW_SIZE;
    int cache_height = map_height * MAP_FOW_SIZE;
    if (base_cells == NULL || x < 0 || x >= cache_width || y < 0 || y >= cache_height) {
        return 0;
    }

    return map_cache_cell(base_cells, x, y)->structural_support_height;
}

/**
 * Determine if an object being rendered should be culled.
 *
 * @param surface
 * Surface that rendering is being done for.
 * @param data
 * Rendering data.
 * @return
 * Whether the object should be culled.
 */
static bool map_should_cull(SDL_Surface *surface, map_render_data_t *data) {
    /* Determine the distance of the object relative to the PC. */
    int distance_x = data->x - map_width * MAP_FOW_SIZE / 2;
    int distance_y = data->y - map_height * MAP_FOW_SIZE / 2;
    int distance = isqrt(distance_x * distance_x + distance_y * distance_y);
    if (distance > 3) {
        /* Too far away, no culling. */
        return false;
    }

    /* Must be in the southern or eastern quadrant to be culled. */
    if (data->x < map_width * MAP_FOW_SIZE / 2 || data->y < map_height * MAP_FOW_SIZE / 2) {
        return false;
    }

    bool cull = false;
    int range = 2;

    for (int sub_layer2 = NUM_SUB_LAYERS - 1; sub_layer2 > 0; sub_layer2--) {
        int16_t height = data->cell->height[GET_MAP_LAYER(LAYER_EFFECT, sub_layer2)];
        if (height - data->player_height_offset > 50) {
            range = 0;
        }
    }

    if (range == 0) {
        cull = true;
    }

    for (int nx = data->x - range; nx <= data->x && !cull; nx++) {
        for (int ny = data->y - range; ny <= data->y && !cull; ny++) {
            MapCell *cell2 = MAP_CELL_GET(nx, ny);

            for (int sub_layer2 = 0; sub_layer2 < NUM_SUB_LAYERS; sub_layer2++) {
                if (cell2->secondpass[sub_layer2] & (1 << (LAYER_WALL - 1)) &&
                    !obj_is_behind_wall(nx,
                                        ny,
                                        map_width * MAP_FOW_SIZE / 2,
                                        map_height * MAP_FOW_SIZE / 2)) {
                    cull = true;
                    break;
                }
            }
        }
    }

    return cull && range != 0;
}

/**
 * Determine if a specified tile should be rendered.
 *
 * Assigns the cell to be rendered in the map render data structure on success.
 *
 * @param surface
 * Surface rendering is being done on.
 * @param data
 * Map rendering data.
 * @return
 * Whether the tile should be rendered.
 */
static bool map_should_draw(SDL_Surface *surface, map_render_data_t *data) {
    HARD_ASSERT(surface != NULL);
    HARD_ASSERT(data != NULL);

    data->xpos = surface->w / 2 - MAP_TILE_POS_XOFF / 2 + (data->x - data->midx) * MAP_TILE_YOFF -
                 (data->y - data->midy) * MAP_TILE_YOFF;
    data->ypos = surface->h / 2 - MAP_TILE_POS_YOFF / 2 + (data->x - data->midx) * MAP_TILE_XOFF +
                 (data->y - data->midy) * MAP_TILE_XOFF;
    data->level_support_height = map_level_support_height(data->x, data->y, data->depth);
    data->ypos -= data->depth * MAP_LEVEL_PIXEL_HEIGHT;
    data->ypos -= data->level_support_height;

    if (!data->world_surface) {
        data->ypos -= map_width * MAP_TILE_XOFF + map_height * MAP_TILE_XOFF * (MAP_FOW_SIZE / 2);
        data->ypos -= map_height * MAP_TILE_YOFF;
    }

    if (data->xpos > surface->w || data->xpos + MAP_TILE_POS_XOFF < 0 ||
        data->ypos + MAP_TILE_POS_YOFF < 0) {
        return false;
    }

    data->cell = MAP_CELL_GET(data->x, data->y);

    if (data->ypos - data->cell->render_max_height > surface->h) {
        return false;
    }

    return true;
}

/**
 * Setup the base information in a map render data structure and calculate
 * X/Y cell indexes and maximum dimensions.
 *
 * @param surface
 * Surface rendering is being done for.
 * @param[out] data Map rendering data.
 * @param[out] x Will contain X index of the cell to render. Can be NULL.
 * @param[out] y Will contain Y index of the cell to render. Can be NULL.
 * @param[out] w Maximum width. Can be NULL.
 * @param[out] h Maximum height. Can be NULL.
 */
static void map_setup_render_data(SDL_Surface *surface,
                                  map_render_data_t *data,
                                  int *x,
                                  int *y,
                                  int *w,
                                  int *h) {
    HARD_ASSERT(surface != NULL);
    HARD_ASSERT(data != NULL);

    data->x = map_width - (map_width / 2) - 1;
    data->y = map_height - (map_height / 2) - 1;
    data->cell = MAP_CELL_GET_MIDDLE(data->x, data->y);
    struct MapCell *base_cells = level_cells[MAP2_DEPTH_INDEX(0)];
    if (data->world_surface && base_cells != NULL) {
        struct MapCell *base_cell =
            map_cache_cell(base_cells, data->x + MAP_STARTX, data->y + MAP_STARTY);
        data->player_height_offset = get_top_floor_height(base_cell, MapData.player_sub_layer);
    } else {
        data->player_height_offset = get_top_floor_height(data->cell, MapData.player_sub_layer);
    }

    if (data->world_surface) {
        data->midx = map_width * MAP_FOW_SIZE / 2;
        data->midy = map_height * MAP_FOW_SIZE / 2;

        int maxw = surface->w / 2.0 / (MAP_TILE_POS_XOFF / 2.0);
        int maxh = surface->h / 2.0 / (MAP_TILE_POS_YOFF / 2.0);
        int maxtiles = MAX(maxh, maxw);

        if (x != NULL) {
            *x = data->midx - maxtiles;
            if (*x < 0) {
                *x = 0;
            }
        }

        if (y != NULL) {
            *y = data->midy - maxtiles;
            if (*y < 0) {
                *y = 0;
            }
        }

        if (w != NULL) {
            *w = data->midx + maxtiles;
            if (*w > map_width * MAP_FOW_SIZE) {
                *w = map_width * MAP_FOW_SIZE;
            }
        }

        if (h != NULL) {
            *h = data->midy + maxtiles;
            if (*h > map_height * MAP_FOW_SIZE) {
                *h = map_height * MAP_FOW_SIZE;
            }
        }
    } else {
        if (x != NULL) {
            *x = 0;
        }

        if (y != NULL) {
            *y = 0;
        }

        if (w != NULL) {
            *w = map_width * MAP_FOW_SIZE;
        }

        if (h != NULL) {
            *h = map_height * MAP_FOW_SIZE;
        }
    }
}

/** Choose the visible floor whose light sample represents a map cell. */
static uint8_t map_lighting_sub_layer(const struct MapCell *cell) {
    uint8_t selected = MIN(MapData.player_sub_layer, NUM_SUB_LAYERS - 1);
    int selected_height = cell->height[GET_MAP_LAYER(LAYER_FLOOR, selected)];

    for (uint8_t sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
        uint8_t floor_layer = GET_MAP_LAYER(LAYER_FLOOR, sub_layer);
        int height = cell->height[floor_layer];

        if (cell->faces[floor_layer] != 0 && height >= selected_height) {
            selected = sub_layer;
            selected_height = height;
        }
    }

    return selected;
}

/**
 * Resolve a light sample without treating an unseen map-cache cell as dark.
 *
 * Newly exposed cells arrive incrementally after a map scroll. Until their
 * authoritative light values arrive, use the average of the closest known
 * ring. This extends the known field naturally at map and FOW boundaries and
 * prevents temporary dark bands from influencing nearby structures.
 */
static uint8_t map_lighting_level(int x, int y, const struct MapCell *cell, uint8_t sub_layer) {
    int cache_width = map_width * MAP_FOW_SIZE;
    int cache_height = map_height * MAP_FOW_SIZE;

    if (cell->light_known[sub_layer]) {
        return cell->light_level[sub_layer];
    }

    /* The rasterizer includes a two-cell border around the drawable map.
     * Searching one cell beyond that is enough to extend authoritative
     * samples across the boundary without turning cache-key generation into
     * an unbounded nearest-neighbour scan while map data is still arriving. */
    const int search_radius = 3;
    for (int radius = 1; radius <= search_radius; radius++) {
        unsigned int total = 0;
        unsigned int samples = 0;

        for (int offset_x = -radius; offset_x <= radius; offset_x++) {
            for (int offset_y = -radius; offset_y <= radius; offset_y++) {
                if (abs(offset_x) != radius && abs(offset_y) != radius) {
                    continue;
                }

                int sample_x = x + offset_x;
                int sample_y = y + offset_y;
                if (sample_x < 0 || sample_x >= cache_width || sample_y < 0 ||
                    sample_y >= cache_height) {
                    continue;
                }

                struct MapCell *sample_cell = MAP_CELL_GET(sample_x, sample_y);
                uint8_t sample_sub_layer = map_lighting_sub_layer(sample_cell);
                if (!sample_cell->light_known[sample_sub_layer]) {
                    continue;
                }

                total += sample_cell->light_level[sample_sub_layer];
                samples++;
            }
        }

        if (samples != 0) {
            return (uint8_t)((total + samples / 2) / samples);
        }
    }

    return 0;
}

/** Project one cell's selected light sample into map-widget coordinates. */
static lighting_vertex_t
map_lighting_vertex(SDL_Surface *surface, const map_render_data_t *data, int x, int y) {
    struct MapCell *cell = MAP_CELL_GET(x, y);
    uint8_t sub_layer = map_lighting_sub_layer(cell);
    int height = MAX(0, cell->height[GET_MAP_LAYER(LAYER_FLOOR, sub_layer)]);

    lighting_vertex_t vertex = {
        .x = surface->w / 2 + (x - data->midx) * MAP_TILE_YOFF - (y - data->midy) * MAP_TILE_YOFF,
        .y = surface->h / 2 + (x - data->midx) * MAP_TILE_XOFF + (y - data->midy) * MAP_TILE_XOFF -
             height + data->player_height_offset - data->depth * MAP_LEVEL_PIXEL_HEIGHT -
             map_level_support_height(x, y, data->depth),
        .level = map_lighting_level(x, y, cell, sub_layer),
    };
    return vertex;
}

/** Add one integer to a stable FNV-1a lightmap cache key. */
static uint64_t map_lighting_hash_value(uint64_t hash, uint64_t value) {
    for (size_t i = 0; i < sizeof(value); i++) {
        hash ^= value & UINT8_MAX;
        hash *= UINT64_C(1099511628211);
        value >>= 8;
    }

    return hash;
}

/** Build a constant-time key from explicit map-lighting invalidation state. */
static uint64_t map_lighting_cache_key(SDL_Surface *surface,
                                       const map_render_data_t *data,
                                       int x,
                                       int y,
                                       int w,
                                       int h) {
    uint64_t hash = UINT64_C(14695981039346656037);

    hash = map_lighting_hash_value(hash, level_lighting_revision[current_level_index]);
    if (data->depth > 0) {
        hash = map_lighting_hash_value(hash, level_lighting_revision[MAP2_DEPTH_INDEX(0)]);
    }
    hash = map_lighting_hash_value(hash, (uint32_t)surface->w);
    hash = map_lighting_hash_value(hash, (uint32_t)surface->h);
    hash = map_lighting_hash_value(hash, (uint32_t)map_width);
    hash = map_lighting_hash_value(hash, (uint32_t)map_height);
    hash = map_lighting_hash_value(hash, (uint32_t)data->midx);
    hash = map_lighting_hash_value(hash, (uint32_t)data->midy);
    hash = map_lighting_hash_value(hash, (uint32_t)data->player_height_offset);
    hash = map_lighting_hash_value(hash, (uint8_t)data->depth);
    hash = map_lighting_hash_value(hash, MapData.player_sub_layer);
    hash = map_lighting_hash_value(hash, (uint32_t)x);
    hash = map_lighting_hash_value(hash, (uint32_t)y);
    hash = map_lighting_hash_value(hash, (uint32_t)w);
    hash = map_lighting_hash_value(hash, (uint32_t)h);

    return hash;
}

/** Rasterize and composite the interpolated map light field. */
static void map_draw_lighting(SDL_Surface *surface,
                              SDL_Surface *destination,
                              map_render_data_t *data,
                              int x,
                              int y,
                              int w,
                              int h) {
    int cache_width = map_width * MAP_FOW_SIZE;
    int cache_height = map_height * MAP_FOW_SIZE;
    int start_x = MAX(0, x - 2);
    int start_y = MAX(0, y - 2);
    int end_x = MIN(cache_width - 1, w + 1);
    int end_y = MIN(cache_height - 1, h + 1);

    if (lighting_needs_update()) {
        int vertex_width = end_x - start_x + 1;
        int vertex_height = end_y - start_y + 1;
        lighting_vertex_t *vertices =
            xmalloc((size_t)vertex_width * (size_t)vertex_height * sizeof(*vertices));
        for (int vertex_x = start_x; vertex_x <= end_x; vertex_x++) {
            for (int vertex_y = start_y; vertex_y <= end_y; vertex_y++) {
                vertices[(size_t)(vertex_x - start_x) * (size_t)vertex_height +
                         (size_t)(vertex_y - start_y)] =
                    map_lighting_vertex(surface, data, vertex_x, vertex_y);
            }
        }

        for (int cell_x = start_x; cell_x < end_x; cell_x++) {
            for (int cell_y = start_y; cell_y < end_y; cell_y++) {
                int left = surface->w / 2 + (cell_x - data->midx) * MAP_TILE_YOFF -
                           (cell_y + 1 - data->midy) * MAP_TILE_YOFF;
                int right = surface->w / 2 + (cell_x + 1 - data->midx) * MAP_TILE_YOFF -
                            (cell_y - data->midy) * MAP_TILE_YOFF;
                if (right < 0 || left >= surface->w) {
                    continue;
                }

                size_t vertex =
                    (size_t)(cell_x - start_x) * (size_t)vertex_height + (size_t)(cell_y - start_y);
                lighting_vertex_t quad[4] = {
                    vertices[vertex],
                    vertices[vertex + (size_t)vertex_height],
                    vertices[vertex + (size_t)vertex_height + 1],
                    vertices[vertex + 1],
                };
                lighting_draw_quad(quad);
            }
        }
        free(vertices);
    }

    lighting_render(destination);
}

/**
 * Draw the map.
 *
 * @param surface
 * Surface to render on.
 */
static void map_draw_level(SDL_Surface *surface,
                           SDL_Surface *ground_surface,
                           int depth,
                           bool primary_level,
                           bool allow_smooth_lighting,
                           map_render_context_t *render_context) {
    HARD_ASSERT(surface != NULL);
    HARD_ASSERT(ground_surface != NULL);

    map_render_data_t data = {
        .world_surface = true,
        .primary_level = primary_level,
        .depth = depth,
        .render_context = render_context,
    };
    int x, y, w, h;
    map_setup_render_data(surface, &data, &x, &y, &w, &h);
    data.smooth_lighting =
        allow_smooth_lighting && setting_get_int(OPT_CAT_MAP, OPT_SMOOTH_LIGHTING);
    if (data.smooth_lighting) {
        uint64_t cache_key = map_lighting_cache_key(surface, &data, x, y, w, h);
        data.smooth_lighting = lighting_begin(surface->w, surface->h, cache_key);
        data.lightmap_pending = data.smooth_lighting;

        /* Positive-depth ground is composited through a color-keyed scratch
         * surface. Applying the screen-sized alpha lightmap to that surface
         * changes transparent background pixels and makes the
         * higher level erase parts of the levels below it. Keep those floor
         * sprites on the discrete path while still building the continuous
         * field used by their wall faces. */
        if (!primary_level) {
            data.lightmap_pending = false;
        }
    }

    /* Draw floor and fmasks. */
    bool ground_present = false;
    uint64_t profile_ground_started = render_profiler_begin();
    for (data.x = x; data.x < w; data.x++) {
        for (data.y = y; data.y < h; data.y++) {
            if (!map_should_draw(surface, &data)) {
                continue;
            }

            for (data.layer = LAYER_FLOOR; data.layer <= LAYER_FMASK; data.layer++) {
                if (data.cell->priority[0] & (1 << (data.layer - 1))) {
                    continue;
                }

                ground_present |= data.cell->faces[GET_MAP_LAYER(data.layer, data.sub_layer)] != 0;
                if (primary_level) {
                    draw_map_object(ground_surface, &data);
                } else {
                    data.defer_rendering = true;
                    draw_map_object(surface, &data);
                    data.defer_rendering = false;
                }
            }
        }
    }
    render_profiler_end(RENDER_PROFILE_MAP_GROUND, profile_ground_started);

    /* The screen-space lightmap is correct for ground geometry. Elevated
     * sprites project over unrelated cells, so light those using the owning
     * tile's level instead of applying the ground field over them. */
    if (data.smooth_lighting) {
        uint64_t profile_lighting_started = render_profiler_begin();
        map_draw_lighting(surface,
                          primary_level && ground_present ? ground_surface : NULL,
                          &data,
                          x,
                          y,
                          w,
                          h);
        render_profiler_end(RENDER_PROFILE_LIGHTING, profile_lighting_started);
        data.lightmap_pending = false;
    }

    if (primary_level && ground_present) {
        surface_show(surface, 0, 0, NULL, ground_surface);
    }

    uint8_t floor_layer_pl = GET_MAP_LAYER(LAYER_FLOOR, MapData.player_sub_layer);

    /* Now draw everything else. */
    data.defer_rendering = true;
    uint64_t profile_objects_started = render_profiler_begin();
    for (data.x = x; data.x < w; data.x++) {
        for (data.y = y; data.y < h; data.y++) {
            if (!map_should_draw(surface, &data)) {
                continue;
            }

            for (data.layer = LAYER_FLOOR; data.layer <= NUM_LAYERS; data.layer++) {
                for (data.sub_layer = 0; data.sub_layer < NUM_SUB_LAYERS; data.sub_layer++) {
                    if (data.sub_layer == 0 &&
                        (data.layer == LAYER_FLOOR || data.layer == LAYER_FMASK)) {
                        continue;
                    }

                    /* Skip objects on the effect layer with non-zero sub-layer
                     * because they will be rendered later. */
                    if (data.layer == LAYER_EFFECT && data.sub_layer != 0) {
                        uint8_t effect_layer = GET_MAP_LAYER(LAYER_EFFECT, data.sub_layer);
                        uint8_t floor_layer = GET_MAP_LAYER(LAYER_FLOOR, MapData.player_sub_layer);
                        if (data.cell->height[effect_layer] >= data.cell->height[floor_layer]) {
                            continue;
                        }
                    }

                    if (data.cell->priority[data.sub_layer] & (1 << (data.layer - 1))) {
                        continue;
                    }

                    draw_map_object(surface, &data);
                }
            }

            for (data.sub_layer = 0; data.sub_layer < NUM_SUB_LAYERS; data.sub_layer++) {
                uint8_t map_layer = GET_MAP_LAYER(LAYER_FLOOR, data.sub_layer);
                if (data.cell->height[map_layer] > data.cell->height[floor_layer_pl]) {
                    continue;
                }

                for (data.layer = LAYER_FLOOR; data.layer <= NUM_LAYERS; data.layer++) {
                    if (!(data.cell->priority[data.sub_layer] & (1 << (data.layer - 1)))) {
                        continue;
                    }

                    if (data.layer == LAYER_EFFECT && data.sub_layer != 0) {
                        map_layer = GET_MAP_LAYER(LAYER_EFFECT, data.sub_layer);
                        if (data.cell->height[map_layer] >= data.cell->height[floor_layer_pl]) {
                            continue;
                        }
                    }

                    draw_map_object(surface, &data);
                }
            }

            for (data.layer = LAYER_FLOOR; data.layer <= NUM_LAYERS; data.layer++) {
                if (!(data.cell->priority[0] & (1 << (data.layer - 1)))) {
                    continue;
                }

                draw_map_object(surface, &data);
            }

            data.layer = LAYER_EFFECT;

            for (data.sub_layer = NUM_SUB_LAYERS - 1; data.sub_layer >= 1; data.sub_layer--) {
                if (data.cell->priority[data.sub_layer] & (1 << (LAYER_EFFECT - 1))) {
                    continue;
                }

                uint8_t map_layer = GET_MAP_LAYER(LAYER_EFFECT, data.sub_layer);
                if (data.cell->height[map_layer] < data.cell->height[floor_layer_pl]) {
                    continue;
                }

                if (data.world_surface && map_should_cull(surface, &data)) {
                    continue;
                }

                draw_map_object(surface, &data);
            }

            for (data.sub_layer = 0; data.sub_layer < NUM_SUB_LAYERS; data.sub_layer++) {
                uint8_t map_layer = GET_MAP_LAYER(LAYER_FLOOR, data.sub_layer);
                if (data.cell->height[map_layer] <= data.cell->height[floor_layer_pl]) {
                    continue;
                }

                for (data.layer = LAYER_FLOOR; data.layer <= NUM_LAYERS; data.layer++) {
                    if (!(data.cell->priority[data.sub_layer] & (1 << (data.layer - 1)))) {
                        continue;
                    }

                    draw_map_object(surface, &data);
                }
            }

            data.layer = LAYER_EFFECT;

            for (data.sub_layer = NUM_SUB_LAYERS - 1; data.sub_layer >= 1; data.sub_layer--) {
                uint8_t map_layer = GET_MAP_LAYER(LAYER_EFFECT, data.sub_layer);
                if (data.cell->height[map_layer] < data.cell->height[floor_layer_pl]) {
                    continue;
                }

                uint8_t map_layer2 = GET_MAP_LAYER(LAYER_FLOOR, data.sub_layer - 1);
                if (data.cell->height[map_layer] <= data.cell->height[map_layer2]) {
                    continue;
                }

                map_layer2 = GET_MAP_LAYER(LAYER_EFFECT, data.sub_layer - 1);
                if (data.cell->height[map_layer] <= data.cell->height[map_layer2]) {
                    continue;
                }

                if (data.world_surface && map_should_cull(surface, &data)) {
                    continue;
                }

                draw_map_object(surface, &data);
            }

            if (data.cell->priority[0] & (1 << (LAYER_WALL - 1)) &&
                data.cell->height[GET_MAP_LAYER(LAYER_WALL, 0)] > 0) {
                data.layer = LAYER_WALL;
                data.sub_layer = 0;
                draw_map_object(surface, &data);
            }
        }
    }

    if (!primary_level) {
        render_profiler_end(RENDER_PROFILE_MAP_OBJECTS, profile_objects_started);
        return;
    }

    for (data.x = x; data.x < w; data.x++) {
        for (data.y = y; data.y < h; data.y++) {
            if (!map_should_draw(surface, &data)) {
                continue;
            }

            for (data.sub_layer = NUM_SUB_LAYERS - 1; data.sub_layer >= 1; data.sub_layer--) {
                uint8_t map_layer = GET_MAP_LAYER(LAYER_EFFECT, data.sub_layer);
                if (data.cell->height[map_layer] != 0 && data.cell->faces[map_layer] != 0) {
                    data.cell = NULL;
                    break;
                }
            }

            if (data.cell == NULL) {
                continue;
            }

            data.layer = LAYER_LIVING;
            data.alpha_forced = 100;

            for (data.sub_layer = 0; data.sub_layer < NUM_SUB_LAYERS; data.sub_layer++) {
                draw_map_object(surface, &data);
            }
        }
    }
    render_profiler_end(RENDER_PROFILE_MAP_OBJECTS, profile_objects_started);

#undef CALCULATE_POSITIONS
}

/** Sort projected map sprites back-to-front across every physical level. */
static int map_render_command_compare(const void *left_ptr, const void *right_ptr) {
    const map_render_command_t *left = left_ptr;
    const map_render_command_t *right = right_ptr;

    if (left->sort_y != right->sort_y) {
        return left->sort_y < right->sort_y ? -1 : 1;
    }

    if (left->sort_x != right->sort_x) {
        return left->sort_x < right->sort_x ? -1 : 1;
    }

    if (left->sequence != right->sequence) {
        return left->sequence < right->sequence ? -1 : 1;
    }

    return 0;
}

/** Return whether two projected, non-empty sprite bounds overlap. */
static bool map_render_command_overlaps(const map_render_command_t *left,
                                        const map_render_command_t *right) {
    return left->bounds_x < right->bounds_x + right->bounds_w &&
           right->bounds_x < left->bounds_x + left->bounds_w &&
           left->bounds_y < right->bounds_y + right->bounds_h &&
           right->bounds_y < left->bounds_y + left->bounds_h;
}

/** Return whether an opaque later sprite hides a substantial part of a door. */
static bool map_render_command_covers_door(const map_render_command_t *door,
                                           const map_render_command_t *occluder) {
    if (!map_render_command_overlaps(door, occluder)) {
        return false;
    }

    /* Rotated/zoomed sprites are uncommon structural geometry. Their projected
     * bounds remain the safe fallback because mapping a transformed source
     * pixel back exactly would duplicate the rotozoom implementation. */
    if (door->transformed || occluder->transformed) {
        return true;
    }

    bool door_locked = false;
    bool occluder_locked = false;
    if (SDL_MUSTLOCK(door->source)) {
        if (!SDL_LockSurface(door->source)) {
            return false;
        }
        door_locked = true;
    }
    if (occluder->source != door->source && SDL_MUSTLOCK(occluder->source)) {
        if (!SDL_LockSurface(occluder->source)) {
            if (door_locked) {
                SDL_UnlockSurface(door->source);
            }
            return false;
        }
        occluder_locked = true;
    }

    size_t door_pixels = 0;
    for (int y = 0; y < door->source->h; y++) {
        for (int x = 0; x < door->source->w; x++) {
            door_pixels += surface_pixel_visible(door->source, x, y);
        }
    }

    bool covered = false;
    int door_copies = door->draw_double ? 2 : 1;
    int occluder_copies = occluder->draw_double ? 2 : 1;
    for (int door_copy = 0; door_copy < door_copies && !covered; door_copy++) {
        int door_y = door->y - door_copy * 22;
        for (int occluder_copy = 0; occluder_copy < occluder_copies && !covered; occluder_copy++) {
            int occluder_y = occluder->y - occluder_copy * 22;
            int x_start = MAX(door->x, occluder->x);
            int x_end = MIN(door->x + door->source->w, occluder->x + occluder->source->w);
            int y_start = MAX(door_y, occluder_y);
            int y_end = MIN(door_y + door->source->h, occluder_y + occluder->source->h);
            size_t covered_pixels = 0;

            for (int y = y_start; y < y_end && !covered; y++) {
                for (int x = x_start; x < x_end; x++) {
                    if (!surface_pixel_visible(door->source, x - door->x, y - door_y) ||
                        !surface_pixel_visible(occluder->source, x - occluder->x, y - occluder_y)) {
                        continue;
                    }

                    covered_pixels++;
                    if (covered_pixels * 2 >= door_pixels) {
                        covered = true;
                        break;
                    }
                }
            }
        }
    }

    if (occluder_locked) {
        SDL_UnlockSurface(occluder->source);
    }
    if (door_locked) {
        SDL_UnlockSurface(door->source);
    }
    return covered;
}

/** Mark nearby doors that are actually covered in the final painter order. */
static void map_render_commands_find_door_hints(map_render_context_t *context) {
    int player_x = map_width * MAP_FOW_SIZE / 2;
    int player_y = map_height * MAP_FOW_SIZE / 2;

    for (size_t door_index = 0; door_index < context->commands_num; door_index++) {
        map_render_command_t *door = &context->commands[door_index];
        if (!door->door || door->depth != 0) {
            continue;
        }

        int distance_x = door->tile_x - player_x;
        int distance_y = door->tile_y - player_y;
        if (distance_x * distance_x + distance_y * distance_y >
            DOOR_HINT_RADIUS * DOOR_HINT_RADIUS) {
            continue;
        }

        /* Only later commands can cover this door in the final painter order.
         * The queue contains every linked physical level, so upper walls and
         * roofs are handled without directional or per-level special cases. */
        for (size_t occluder_index = door_index + 1; occluder_index < context->commands_num;
             occluder_index++) {
            map_render_command_t *occluder = &context->commands[occluder_index];
            if (occluder->object_layer != LAYER_WALL || occluder->door ||
                (occluder->effects.alpha != 0 && occluder->effects.alpha < 128) ||
                !map_render_command_covers_door(door, occluder)) {
                continue;
            }

            door->door_hint = true;
            break;
        }
    }
}

/** Paint all projected sprites in one isometric order. */
static void
map_render_commands(SDL_Surface *surface, map_render_context_t *context, bool door_hints_enabled) {
    uint64_t profile_paint_started = render_profiler_begin();
    if (context->commands_num > 1) {
        qsort(context->commands,
              context->commands_num,
              sizeof(*context->commands),
              map_render_command_compare);
    }

    if (door_hints_enabled) {
        map_render_commands_find_door_hints(context);
    }

    int selected_depth = MAP2_MAX_DEPTH + 1;
    for (size_t i = 0; i < context->commands_num; i++) {
        map_render_command_t *command = &context->commands[i];
        if (selected_depth != command->depth) {
            SOFT_ASSERT(lighting_select_level(command->depth),
                        "Could not select lighting context for depth %d",
                        command->depth);
            selected_depth = command->depth;
        }

        surface_show_effects(surface,
                             command->x,
                             command->y,
                             NULL,
                             command->source,
                             &command->effects);
        if (command->draw_double) {
            surface_show_effects(surface,
                                 command->x,
                                 command->y - 22,
                                 NULL,
                                 command->source,
                                 &command->effects);
        }
    }

    if (door_hints_enabled) {
        for (size_t i = 0; i < context->commands_num; i++) {
            const map_render_command_t *command = &context->commands[i];
            if (!command->door_hint) {
                continue;
            }

            sprite_effects_t effects = {0};
            effects.zoom_x = command->effects.zoom_x;
            effects.zoom_y = command->effects.zoom_y;
            effects.rotate = command->effects.rotate;
            snprintf(VS(effects.outline), "%s", DOOR_HINT_COLOR);
            surface_show_effects(surface, command->x, command->y, NULL, command->source, &effects);
            if (command->draw_double) {
                surface_show_effects(surface,
                                     command->x,
                                     command->y - 22,
                                     NULL,
                                     command->source,
                                     &effects);
            }
        }
    }

    free(context->commands);
    context->commands = NULL;
    context->commands_num = 0;
    render_profiler_end(RENDER_PROFILE_MAP_PAINT, profile_paint_started);
}

/** Draw map annotations and target UI after the unified world pass. */
static void map_draw_ui(SDL_Surface *surface, map_render_context_t *context) {
    uint64_t profile_ui_started = render_profiler_begin();
    map_draw_annotations(surface, context);

    for (size_t i = 0; i < context->tiles_num; i++) {
        SDL_Rect box = {
            .x = context->tiles[i].x,
            .y = context->tiles[i].y,
            .w = MAP_TILE_POS_XOFF,
            .h = MAP_TILE_POS_YOFF,
        };
        text_show_format(surface,
                         FONT("arial", 9),
                         box.x,
                         box.y,
                         COLOR_WHITE,
                         TEXT_OUTLINE | TEXT_VALIGN_CENTER | TEXT_ALIGN_CENTER,
                         &box,
                         "%d,%d",
                         context->tiles[i].w,
                         context->tiles[i].h);
    }
    free(context->tiles);
    context->tiles = NULL;
    context->tiles_num = 0;

    if (context->target_cell != NULL && cpl.target_code != 0) {
        const char *hp_color;

        if (cpl.target_hp > 90) {
            hp_color = COLOR_GREEN;
        } else if (cpl.target_hp > 75) {
            hp_color = COLOR_DGOLD;
        } else if (cpl.target_hp > 50) {
            hp_color = COLOR_HGOLD;
        } else if (cpl.target_hp > 25) {
            hp_color = COLOR_YELLOW;
        } else if (cpl.target_hp > 10) {
            hp_color = COLOR_ORANGE;
        } else {
            hp_color = COLOR_RED;
        }

        if (!(setting_get_int(OPT_CAT_MAP, OPT_PLAYER_NAMES) &&
              context->target_cell->pname[context->target_sub_layer][0] != '\0')) {
            text_show(surface,
                      FONT_SANS9,
                      cpl.target_name,
                      context->target_rect.x + context->target_rect.w / 2 -
                          text_get_width(FONT_SANS9, cpl.target_name, 0) / 2,
                      context->target_rect.y - 15,
                      cpl.target_color,
                      TEXT_OUTLINE,
                      NULL);
        }

        rectangle_create(surface,
                         context->target_rect.x - 2,
                         context->target_rect.y - 2,
                         1,
                         5,
                         hp_color);
        rectangle_create(surface,
                         context->target_rect.x - 2,
                         context->target_rect.y - 2,
                         3,
                         1,
                         hp_color);
        rectangle_create(surface,
                         context->target_rect.x - 2,
                         context->target_rect.y + 2,
                         3,
                         1,
                         hp_color);
        rectangle_create(surface,
                         context->target_rect.x + context->target_rect.w + 1,
                         context->target_rect.y - 2,
                         1,
                         5,
                         hp_color);
        rectangle_create(surface,
                         context->target_rect.x + context->target_rect.w - 1,
                         context->target_rect.y - 2,
                         3,
                         1,
                         hp_color);
        rectangle_create(surface,
                         context->target_rect.x + context->target_rect.w - 1,
                         context->target_rect.y + 2,
                         3,
                         1,
                         hp_color);

        context->target_rect.w =
            context->target_rect.w / 100.0 * context->target_cell->probe[context->target_sub_layer];
        context->target_rect.w = MAX(1, MIN(100, context->target_rect.w));
        rectangle_create(surface,
                         context->target_rect.x,
                         context->target_rect.y,
                         context->target_rect.w,
                         context->target_rect.h,
                         hp_color);
    }

    render_profiler_end(RENDER_PROFILE_MAP_UI, profile_ui_started);
}

/** Draw independently cached levels through one projected painter order. */
void map_draw_map(SDL_Surface *surface) {
    HARD_ASSERT(surface != NULL);

    uint64_t profile_map_started = render_profiler_begin();

    bool primary_surface = cur_widget[MAP_ID] != NULL && surface == cur_widget[MAP_ID]->surface;
    size_t surface_index = primary_surface ? 0 : 1;
    SDL_Surface **level_surface = &map_level_surfaces[surface_index];
    map_render_context_t render_context = {0};

    if (*level_surface == NULL || (*level_surface)->w != surface->w ||
        (*level_surface)->h != surface->h) {
        if (*level_surface != NULL) {
            SDL_DestroySurface(*level_surface);
        }

        *level_surface = SDL_CreateSurface(surface->w, surface->h, surface->format);
        if (*level_surface == NULL) {
            LOG(ERROR, "Could not create map level surface: %s", SDL_GetError());
            render_profiler_end(RENDER_PROFILE_MAP, profile_map_started);
            return;
        }
        Uint32 black = pixel_format_map_rgb((*level_surface)->format, 0, 0, 0);
        SDL_SetSurfaceColorKey(*level_surface, true, black);
        SDL_SetSurfaceRLE(*level_surface, true);
    }

    for (int depth = -MAP2_MAX_DEPTH; depth <= MAP2_MAX_DEPTH; depth++) {
        uint16_t bit = UINT16_C(1) << MAP2_DEPTH_INDEX(depth);
        if (!(map_level_mask & bit) || !map_select_level(depth, false) ||
            !lighting_select_level(depth)) {
            continue;
        }

        if (depth == 0) {
            SDL_FillSurfaceRect(*level_surface,
                                NULL,
                                pixel_format_map_rgb((*level_surface)->format, 0, 0, 0));
        }
        map_draw_level(surface,
                       *level_surface,
                       depth,
                       depth == 0,
                       primary_surface,
                       &render_context);
    }

    map_render_commands(surface, &render_context, primary_surface);
    map_draw_ui(surface, &render_context);
    map_select_level(0, true);
    lighting_select_level(0);
    render_profiler_end(RENDER_PROFILE_MAP, profile_map_started);
}

/**
 * Draw one sprite on map.
 * @param x
 * X position.
 * @param y
 * Y position.
 * @param surface
 * What to draw.
 */
void map_draw_one(int x, int y, SDL_Surface *surface) {
    map_render_data_t data = {.world_surface = true};

    map_setup_render_data(cur_widget[MAP_ID]->surface, &data, NULL, NULL, NULL, NULL);

    data.x = x;
    data.y = y;
    SOFT_ASSERT(map_should_draw(cur_widget[MAP_ID]->surface, &data),
                "map_should_draw() returned false");

    if (surface->w > MAP_TILE_POS_XOFF) {
        data.xpos -= (surface->w - MAP_TILE_POS_XOFF) / 2;
    }

    if (data.cell->faces[0] != 0) {
        data.ypos -= get_top_floor_height(data.cell, MapData.player_sub_layer);
        data.ypos += data.player_height_offset;
    }

    double zoom = setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0;
    data.xpos *= zoom;
    data.ypos *= zoom;

    sprite_effects_t effects = {0};
    effects.zoom_x = 100.0 * zoom;
    effects.zoom_y = 100.0 * zoom;

    /* Outside of the "visible" area; always render as fog of war
     * (grayscale). */
    if (x < map_width * (MAP_FOW_SIZE / 2) || x >= map_width * (MAP_FOW_SIZE / 2) + map_width ||
        y < map_height * (MAP_FOW_SIZE / 2) || y >= map_height * (MAP_FOW_SIZE / 2) + map_height) {
        BIT_SET(effects.flags, SPRITE_FLAG_FOW);
    }

    surface_show_effects(ScreenSurface,
                         widget_x(cur_widget[MAP_ID]) + data.xpos,
                         widget_y(cur_widget[MAP_ID]) + data.ypos,
                         NULL,
                         surface,
                         &effects);
}

/**
 * Send a command to move the player to the specified square.
 *
 * @param tx
 * Square X position.
 * @param ty
 * Square Y position.
 */
static void send_move_path(int tx, int ty) {
    packet_struct *packet;

    if (tx < 0 || ty < 0 || tx >= map_width || ty >= map_height) {
        return;
    }

    packet = packet_new(SERVER_CMD_MOVE_PATH, 8, 0);
    packet_writer_write_uint8(packet, tx);
    packet_writer_write_uint8(packet, ty);
    socket_send_packet(packet);
}

/**
 * Send a command to target an NPC.
 * @param tx
 * NPC's X position.
 * @param ty
 * NPC's Y position.
 * @param count
 * NPC's UID.
 */
static void send_target(int x, int y, uint32_t count) {
    packet_struct *packet;

    if ((x < 0 || y < 0 || x >= map_width || y >= map_height) && !(x == -1 && y == -1)) {
        return;
    }

    packet = packet_new(SERVER_CMD_TARGET, 16, 0);

    if (x == -1 && y == -1) {
        packet_writer_write_uint8(packet, CMD_TARGET_CLEAR);
    } else {
        packet_writer_write_uint8(packet, CMD_TARGET_MAPXY);
        packet_writer_write_uint8(packet, x);
        packet_writer_write_uint8(packet, y);
        packet_writer_write_uint32(packet, count);
    }

    socket_send_packet(packet);
}

/**
 * Compare distances between two targets on the map.
 * @param a
 * First target.
 * @param b
 * Second target.
 * @return
 * Comparison result.
 */
static int map_target_cmp(const void *a, const void *b) {
    double x, y, x2, y2;
    unsigned long dist1, dist2;

    x = ((const map_target_struct *)a)->x - (map_width / 2.0f);
    y = ((const map_target_struct *)a)->y - (map_height / 2.0f);

    x2 = ((const map_target_struct *)b)->x - (map_width / 2.0f);
    y2 = ((const map_target_struct *)b)->y - (map_height / 2.0f);

    dist1 = isqrt(x * x + y * y);
    dist2 = isqrt(x2 * x2 + y2 * y2);

    if (dist1 < dist2) {
        return -1;
    } else if (dist1 > dist2) {
        return 1;
    } else {
        return 0;
    }
}

/**
 * Target something on the map.
 * @param is_friend
 * 1 if targeting friendlies only.
 */
void map_target_handle(uint8_t is_friend) {
    int x, y, layer;
    struct MapCell *cell;
    UT_array *targets;
    UT_icd icd = {sizeof(map_target_struct), NULL, NULL, NULL};
    map_target_struct *p;
    uint32_t curr_target;

    if (cpl.target_is_friend != is_friend) {
        cpl.target_object_index = 0;
    }

    utarray_new(targets, &icd);
    curr_target = 0;

    for (x = 0; x < map_width; x++) {
        for (y = 0; y < map_height; y++) {
            cell = MAP_CELL_GET_MIDDLE(x, y);

            if (cell->fow) {
                continue;
            }

            for (int sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
                layer = GET_MAP_LAYER(LAYER_LIVING, sub_layer);
                if (cell->faces[layer] && cell->target_object_count[sub_layer] &&
                    cell->target_is_friend[sub_layer] == is_friend) {
                    map_target_struct target;

                    target.count = cell->target_object_count[sub_layer];
                    target.x = x;
                    target.y = y;
                    utarray_push_back(targets, &target);

                    if (cell->probe[sub_layer] != 0) {
                        curr_target = target.count;
                    }
                }
            }
        }
    }

    utarray_sort(targets, map_target_cmp);

    if (cpl.target_object_index >= utarray_len(targets)) {
        cpl.target_object_index = 0;
    }

    if (cpl.target_object_index == 0) {
        p = (map_target_struct *)utarray_front(targets);

        if (p != NULL && p->count == curr_target) {
            cpl.target_object_index++;
        }
    }

    p = (map_target_struct *)utarray_eltptr(targets, cpl.target_object_index);

    if (p != NULL) {
        send_target(p->x, p->y, p->count);
        cpl.target_object_index++;
    } else if (cpl.target_is_friend != is_friend) {
        send_target(-1, -1, 0);
    }

    cpl.target_is_friend = is_friend;

    utarray_free(targets);
}

/**
 * Transform mouse coordinates to tile coordinates on map.
 *
 * Both 'tx' and 'ty' can be NULL, which is useful if you only want to
 * check if the mouse is over a valid map tile.
 *
 * @param mx
 * Mouse X.
 * @param my
 * Mouse Y.
 * @param[out] tx Will contain tile X, unless function returns false.
 * @param[out] ty Will contain tile Y, unless function returns false.
 * @return
 * True on success, false on failure.
 */
bool mouse_to_tile_coords(int mx, int my, int *tx, int *ty) {
    map_render_data_t data = {.world_surface = true};
    int x, y, w, h;
    map_setup_render_data(cur_widget[MAP_ID]->surface, &data, &x, &y, &w, &h);

    double zoom = setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0;

    mx -= widget_x(cur_widget[MAP_ID]);
    my -= widget_y(cur_widget[MAP_ID]);

    for (data.x = w - 1; data.x >= x; data.x--) {
        for (data.y = h - 1; data.y >= y; data.y--) {
            if (!map_should_draw(cur_widget[MAP_ID]->surface, &data)) {
                continue;
            }

            if ((data.cell->fow || MapData.height_diff) &&
                abs(get_top_floor_height(data.cell, MapData.player_sub_layer) -
                    data.player_height_offset) > HEIGHT_MAX_RENDER) {
                continue;
            }

            data.xpos *= zoom;
            data.ypos *= zoom;

            if (data.cell->faces[0] != 0) {
                int height = get_top_floor_height(data.cell, MapData.player_sub_layer);
                data.ypos = (data.ypos - height * zoom) + data.player_height_offset * zoom;
            }

            uint32_t stretch = 0;
            int16_t max_height = 0;

            for (int sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
                int16_t height = data.cell->height[sub_layer * NUM_LAYERS];
                if (height > max_height) {
                    max_height = height;
                    stretch = data.cell->stretch[sub_layer];
                }
            }

            int stretch_height = (stretch >> 24) & 0xff;

            /* See if this square matches our 48x24 box shape. */
            if (mx >= data.xpos && mx <= data.xpos + (MAP_TILE_POS_XOFF * zoom) &&
                my >= data.ypos && my <= data.ypos + (MAP_TILE_YOFF + stretch_height) * zoom) {
                if (tilestretcher_coords_in_tile(stretch,
                                                 (mx - data.xpos) / zoom,
                                                 (my - data.ypos) / zoom)) {
                    if (tx != NULL) {
                        *tx = data.x;
                    }

                    if (ty != NULL) {
                        *ty = data.y;
                    }

                    return true;
                }
            }
        }
    }

    return false;
}

/**
 * Handle the mouse firing gesture.
 *
 * @return
 * True if the gesture was handled, false otherwise.
 */
bool map_mouse_fire(void) {
    int x, y;
    SDL_MouseButtonFlags state = mouse_get_state(&x, &y);

    if ((state != (SDL_BUTTON_MASK(SDL_BUTTON_RIGHT) | SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) &&
         state != SDL_BUTTON_MASK(SDL_BUTTON_MIDDLE))) {
        return false;
    }

    int tx, ty;
    if (!mouse_to_tile_coords(x, y, &tx, &ty)) {
        return false;
    }

    int rx = tx - map_width * (MAP_FOW_SIZE / 2);
    int ry = ty - map_height * (MAP_FOW_SIZE / 2);

    cpl.fire_on = 1;
    move_keys(dir_from_tile_coords(rx, ry));
    cpl.fire_on = 0;
    return true;
}

/**
 * Handle the "Walk Here" option in map widget menu.
 * @param widget
 * Map widget.
 * @param menuitem
 * Menu item.
 * @param event
 * Event.
 */
static void menu_map_walk_here(widgetdata *widget, widgetdata *menuitem, SDL_Event *event) {
    int tx, ty;

    if (mouse_to_tile_coords(cur_widget[MENU_ID]->x, cur_widget[MENU_ID]->y, &tx, &ty)) {
        int rx = tx - map_width * (MAP_FOW_SIZE / 2);
        int ry = ty - map_height * (MAP_FOW_SIZE / 2);
        send_move_path(rx, ry);
    }
}

/**
 * Handle the "Talk To NPC" option in map widget menu.
 * @param widget
 * Map widget.
 * @param menuitem
 * Menu item.
 * @param event
 * Event.
 */
static void menu_map_talk_to(widgetdata *widget, widgetdata *menuitem, SDL_Event *event) {
    int tx, ty;

    if (mouse_to_tile_coords(cur_widget[MENU_ID]->x, cur_widget[MENU_ID]->y, &tx, &ty)) {
        int rx = tx - map_width * (MAP_FOW_SIZE / 2);
        int ry = ty - map_height * (MAP_FOW_SIZE / 2);
        send_target(rx, ry, 0);
        keybind_process_command("?HELLO");
    }
}

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget) {
    static int gfx_toggle = 0;
    SDL_Rect box;
    int mx, my;

    if (widget->surface == NULL || widget->surface->w != widget->w ||
        widget->surface->h != widget->h) {
        if (widget->surface != NULL) {
            SDL_DestroySurface(widget->surface);
            map_redraw_flag = 1;
        }

        widget->surface = surface_create_rgb(get_video_flags(),
                                             widget->w,
                                             widget->h,
                                             video_get_bpp(),
                                             0,
                                             0,
                                             0,
                                             0);
        if (widget->surface == NULL) {
            LOG(ERROR, "Could not create map widget surface: %s", SDL_GetError());
            return;
        }
    }

    /* Make sure the map widget is always the last to handle events for. */
    SetPriorityWidget_reverse(widget);

    double zoom = setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0;
    if (widget_set_zoom(widget, zoom)) {
        map_redraw_flag = 1;
    }

    /* We re-create the map only when there is a change. */
    if (map_redraw_flag) {
        SDL_FillSurfaceRect(widget->surface, NULL, 0);
        map_draw_map(widget->surface);
        map_redraw_flag = 0;
        effect_sprites_play();

        if (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) != 100) {
            if (zoomed) {
                SDL_DestroySurface(zoomed);
            }

            zoomed = zoomSurface(widget->surface,
                                 setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0,
                                 setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0,
                                 setting_get_int(OPT_CAT_CLIENT, OPT_ZOOM_SMOOTH));
            if (zoomed == NULL) {
                LOG(ERROR, "Could not resize map surface: %s", SDL_GetError());
            }
        }
    }

    box.x = widget_x(widget);
    box.y = widget_y(widget);

    if (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) == 100) {
        SDL_BlitSurface(widget->surface, NULL, ScreenSurface, &box);
    } else {
        SDL_BlitSurface(zoomed != NULL ? zoomed : widget->surface, NULL, ScreenSurface, &box);
    }

    if (map_show_mouse && widget_mouse_event.owner == cur_widget[MAP_ID]) {
        int tx, ty;
        if (!mouse_to_tile_coords(cursor_x, cursor_y, &tx, &ty)) {
            map_show_mouse = false;
        } else {
            map_draw_one(tx, ty, TEXTURE_CLIENT("square_highlight"));
        }
    }

    /* The damage numbers */
    map_anims_play();

    map_render_data_t data = {0};
    map_setup_render_data(widget->surface, &data, NULL, NULL, NULL, NULL);

    int xpos = widget_x(widget) + widget_w(widget) / 2;
    int ypos = widget_y(widget) + widget_h(widget) / 2;
    ypos -= MAP_TILE_POS_YOFF * 1.5 + 7;

    /* Draw warning icons above player */
    if ((gfx_toggle++ & 63) < 25) {
        int warn = setting_get_int(OPT_CAT_MAP, OPT_HEALTH_WARNING);
        double hp_percent = (double)cpl.stats.hp / cpl.stats.maxhp * 100.0;
        if (warn != 0 && warn >= hp_percent) {
            SDL_Surface *texture = TEXTURE_CLIENT("warn_hp");
            surface_show(ScreenSurface,
                         xpos - texture->w / 2,
                         ypos - texture->h / 2,
                         NULL,
                         texture);
        }
    } else {
        int warn = setting_get_int(OPT_CAT_MAP, OPT_FOOD_WARNING);
        double food_percent = (double)cpl.stats.food / 1000.0 * 100.0;
        if (warn != 0 && warn >= food_percent) {
            SDL_Surface *texture = TEXTURE_CLIENT("warn_food");
            surface_show(ScreenSurface,
                         xpos - texture->w / 2,
                         ypos - texture->h / 2,
                         NULL,
                         texture);
        }
    }

    /* Process message animations */
    if (msg_anim.message[0] != '\0') {
        if ((LastTick - msg_anim.tick) < 3000) {
            int bmoff, y_offset;
            char *msg, *cp;

            bmoff = (int)((50.0f / 3.0f) * ((float)(LastTick - msg_anim.tick) / 1000.0f) *
                              ((float)(LastTick - msg_anim.tick) / 1000.0f) +
                          ((int)(150.0f * ((float)(LastTick - msg_anim.tick) / 3000.0f))));
            y_offset = 0;
            msg = xstrdup(msg_anim.message);

            cp = strtok(msg, "\n");

            while (cp) {
                text_show(ScreenSurface,
                          FONT_SERIF16,
                          cp,
                          widget_x(widget) + widget_w(widget) / 2 -
                              text_get_width(FONT_SERIF16, cp, TEXT_OUTLINE) / 2,
                          widget_y(widget) + 300 - bmoff + y_offset,
                          msg_anim.color,
                          TEXT_OUTLINE | TEXT_MARKUP,
                          NULL);
                y_offset += FONT_HEIGHT(FONT_SERIF16);
                cp = strtok(NULL, "\n");
            }

            free(msg);
            widget->redraw++;
        } else {
            msg_anim.message[0] = '\0';
        }
    }

    /* Holding the right mouse button for some time, create a menu. */
    if (mouse_get_state(&mx, &my) == SDL_BUTTON_MASK(SDL_BUTTON_RIGHT) && right_click_ticks != -1 &&
        SDL_GetTicks() - right_click_ticks > 500) {
        widgetdata *menu;

        menu = create_menu(mx, my, widget);
        add_menuitem(menu, "Walk Here", &menu_map_walk_here, MENU_NORMAL, 0);
        add_menuitem(menu, "Talk To NPC", &menu_map_talk_to, MENU_NORMAL, 0);
        widget_menu_standard_items(widget, menu);
        menu_finalize(menu);
        right_click_ticks = -1;
    }
}

/** @copydoc widgetdata::event_func */
static int widget_event(widgetdata *widget, SDL_Event *event) {
    if (!EVENT_IS_MOUSE(event)) {
        return 0;
    }

    /* Check if the mouse is in play field. */
    int tx, ty;
    if (!mouse_to_tile_coords(event_mouse_x(event), event_mouse_y(event), &tx, &ty)) {
        return 0;
    }

    int rx = tx - map_width * (MAP_FOW_SIZE / 2);
    int ry = ty - map_height * (MAP_FOW_SIZE / 2);

    if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        /* Send target command if we released the right button in time;
         * otherwise the widget menu will be created. */
        if (event->button.button == SDL_BUTTON_RIGHT && SDL_GetTicks() - right_click_ticks < 500) {
            send_target(rx, ry, 0);
        }

        right_click_ticks = -1;
        return 1;
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (event->button.button == SDL_BUTTON_RIGHT) {
            right_click_ticks = SDL_GetTicks();
        } else if (mouse_get_state(NULL, NULL) == SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) {
            /* Running */

            if (cpl.fire_on || cpl.run_on) {
                move_keys(dir_from_tile_coords(rx, ry));
            } else {
                send_move_path(rx, ry);
            }
        }

        return 1;
    } else if (event->type == SDL_EVENT_MOUSE_MOTION) {
        if (tx != old_map_mouse_x || ty != old_map_mouse_y) {
            old_map_mouse_x = tx;
            old_map_mouse_y = ty;
            map_show_mouse = true;

            return 1;
        }
    }

    return 0;
}

/** @copydoc widgetdata::background_func */
static void widget_background(widgetdata *widget, int draw) {
    if (!widget->redraw) {
        region_map_ready(MapData.region_map);
    }
}

/** @copydoc widgetdata::deinit_func */
static void widget_deinit(widgetdata *widget) {
    lighting_deinit();

    for (size_t i = 0; i < arraysize(map_level_surfaces); i++) {
        if (map_level_surfaces[i] != NULL) {
            SDL_DestroySurface(map_level_surfaces[i]);
            map_level_surfaces[i] = NULL;
        }
    }

    for (size_t i = 0; i < arraysize(level_cells); i++) {
        free(level_cells[i]);
        level_cells[i] = NULL;
    }
    cells = NULL;

    region_map_free(MapData.region_map);
    MapData.region_map = NULL;
}

/**
 * Initialize one map widget.
 */
void widget_map_init(widgetdata *widget) {
    HARD_ASSERT(MapData.region_map == NULL);

    MapData.region_map = region_map_create();

    widget->draw_func = widget_draw;
    widget->event_func = widget_event;
    widget->background_func = widget_background;
    widget->deinit_func = widget_deinit;
    widget->menu_handle_func = NULL;

    SetPriorityWidget_reverse(widget);
}

/**
 * Add an animation.
 * @param type
 * Animation type, one of @ref ANIM_xxx.
 * @param mapx
 * Map X.
 * @param mapy
 * Map Y.
 * @param sub_layer
 * Sub-layer.
 * @param value
 * Value to display.
 * @return
 * Created animation.
 */
struct map_anim *map_anims_add(int type, int mapx, int mapy, int sub_layer, int depth, int value) {
    map_anim_t *anim;
    int num_ticks;

    anim = xcalloc(1, sizeof(*anim));

    DL_APPEND(first_anim, anim);

    /* Type */
    anim->type = type;

    /* Map coordinates */
    anim->mapx = mapx + MAP_STARTX;
    anim->mapy = mapy + MAP_STARTY;

    /* Sub-layer. */
    anim->sub_layer = sub_layer;
    anim->depth = depth;
    /* Amount of damage */
    anim->value = value;

    /* Current time in MilliSeconds */
    anim->start_tick = LastTick;

    switch (type) {
        case ANIM_DAMAGE:
            /* How many ticks to display */
            num_ticks = 850;
            anim->last_tick = anim->start_tick + num_ticks;
            /* 850 ticks 25 pixel move up */
            anim->yoff = -(25.0f / 850.0f);
            break;

        case ANIM_KILL:
            /* How many ticks to display */
            num_ticks = 850;
            anim->last_tick = anim->start_tick + num_ticks;
            /* 850 ticks 25 pixel move up */
            anim->yoff = -(25.0f / 850.0f);
            break;
    }

    return anim;
}

/**
 * Remove a map animation.
 * @param anim
 * The animation to remove.
 */
void maps_anims_remove(map_anim_t *anim) {
    HARD_ASSERT(anim != NULL);

    DL_DELETE(first_anim, anim);

    free(anim);
}

/**
 * Adjust the X/Y coordinates of map animations due to a map scroll.
 * @param xoff
 * X offset.
 * @param Yoff
 * Y offset.
 */
void map_anims_mapscroll(int xoff, int yoff) {
    map_anim_t *anim;
    DL_FOREACH(first_anim, anim) {
        anim->mapx -= xoff;
        anim->mapy -= yoff;
    }
}

/**
 * Clear map animations.
 */
void map_anims_clear(void) {
    map_anim_t *anim, *tmp;
    DL_FOREACH_SAFE(first_anim, anim, tmp) {
        maps_anims_remove(anim);
    }
}

/**
 * Play map animations.
 */
void map_anims_play(void) {
    map_render_data_t data = {.world_surface = true, .primary_level = true};
    map_select_level(0, true);
    map_setup_render_data(cur_widget[MAP_ID]->surface, &data, NULL, NULL, NULL, NULL);

    map_anim_t *anim, *tmp;
    DL_FOREACH_SAFE(first_anim, anim, tmp) {
        /* Have we passed the last tick */
        if (LastTick > anim->last_tick) {
            maps_anims_remove(anim);
            continue;
        }

        if (!map_select_level(anim->depth, false)) {
            continue;
        }

        data.depth = anim->depth;
        data.x = anim->mapx;
        data.y = anim->mapy;
        if (!map_should_draw(cur_widget[MAP_ID]->surface, &data)) {
            continue;
        }

        data.xpos += MAP_TILE_POS_XOFF / 2;
        data.ypos -= MAP_TILE_POS_YOFF;

        uint32_t num_ticks = LastTick - anim->start_tick;
        data.ypos += num_ticks * anim->yoff;
        data.xpos += num_ticks * anim->xoff;

        char buf[32];
        switch (anim->type) {
            case ANIM_DAMAGE: {
                snprintf(VS(buf), "%d", abs(anim->value));
                int wd = text_get_width(FONT_MONO10, buf, TEXT_OUTLINE);
                const char *color = anim->value < 0 ? COLOR_GREEN : COLOR_ORANGE;

                text_show(ScreenSurface,
                          FONT_MONO10,
                          buf,
                          data.xpos - wd / 2,
                          data.ypos,
                          color,
                          TEXT_OUTLINE,
                          NULL);
                break;
            }

            case ANIM_KILL: {
                snprintf(VS(buf), "%d", anim->value);
                int wd = text_get_width(FONT_MONO10, buf, TEXT_OUTLINE);
                int ht = text_get_height(FONT_MONO10, buf, 0);
                SDL_Surface *texture = TEXTURE_CLIENT("death");
                surface_show(ScreenSurface,
                             data.xpos - texture->w / 2,
                             data.ypos - ht / 2 + 2,
                             NULL,
                             texture);

                text_show(ScreenSurface,
                          FONT_MONO10,
                          buf,
                          data.xpos - wd / 2,
                          data.ypos,
                          COLOR_ORANGE,
                          TEXT_OUTLINE,
                          NULL);

                break;
            }

            default:
                LOG(ERROR, "Unknown animation type: %d", anim->type);
                break;
        }
    }

    map_select_level(0, true);
}

/**
 * Check whether the damage animations need redrawing.
 * @return
 * 1 if the damage animations need redrawing, 0 otherwise.
 */
int map_anims_need_redraw(void) {
    return first_anim != NULL;
}
