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
 * Implements map type widgets.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <region_map.h>
#include <packet.h>
#include <toolkit_string.h>
#include <bresenham.h>

/**
 * Map cells.
 */
static struct MapCell *cells;
/**
 * Map's width.
 */
static int map_width;
/**
 * Map's height.
 */
static int map_height;
/**
 * Zoomed map.
 */
static SDL_Surface *zoomed = NULL;
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
static int old_map_mouse_x = 0, old_map_mouse_y = 0;
/**
 * When the right button was pressed on the map widget. -1 = not
 * pressed.
 */
static int right_click_ticks = -1;

/**
 * If 1, will print tile coordinates.
 */
static int tiles_debug = 0;

static int get_top_floor_height(struct MapCell *cell, int sub_layer);

/**
 * Enable map tiles debug.
 */
void clioptions_option_tiles_debug(const char *arg)
{
    tiles_debug = 1;
}

/**
 * Add an animation.
 * @param type Animation type, one of @ref ANIM_xxx.
 * @param mapx Map X.
 * @param mapy Map Y.
 * @param sub_layer Sub-layer.
 * @param value Value to display.
 * @return Created animation.
 */
struct map_anim *map_anims_add(int type, int mapx, int mapy, int sub_layer,
        int value)
{
    map_anim_t *anim;
    int num_ticks;

    anim = ecalloc(1, sizeof(*anim));

    DL_APPEND(first_anim, anim);

    /* Type */
    anim->type = type;

    /* Map coordinates */
    anim->mapx = mapx + MAP_STARTX;
    anim->mapy = mapy + MAP_STARTY;

    /* Sub-layer. */
    anim->sub_layer = sub_layer;
    /* Amount of damage */
    anim->value = value;

    /* Starting Y position */
    anim->y = -5;

    /* Current time in MilliSeconds */
    anim->start_tick = LastTick;

    switch (type) {
    case ANIM_DAMAGE:
        /* How many ticks to display */
        num_ticks = 850;
        anim->last_tick = anim->start_tick + num_ticks;
        /* 850 ticks 25 pixel move up */
        anim->yoff = (25.0f / 850.0f);
        break;

    case ANIM_KILL:
        /* How many ticks to display */
        num_ticks = 850;
        anim->last_tick = anim->start_tick + num_ticks;
        /* 850 ticks 25 pixel move up */
        anim->yoff = (25.0f / 850.0f);
        break;
    }

    return anim;
}

/**
 * Remove a map animation.
 * @param anim The animation to remove.
 */
void maps_anims_remove(map_anim_t *anim)
{
    HARD_ASSERT(anim != NULL);

    DL_DELETE(first_anim, anim);

    efree(anim);
}

/**
 * Adjust the X/Y coordinates of map animations due to a map scroll.
 * @param xoff X offset.
 * @param Yoff Y offset.
 */
void map_anims_mapscroll(int xoff, int yoff)
{
    map_anim_t *anim;

    DL_FOREACH(first_anim, anim)
    {
        anim->mapx -= xoff;
        anim->mapy -= yoff;
    }
}

/**
 * Clear map animations.
 */
void map_anims_clear(void)
{
    map_anim_t *anim, *tmp;

    DL_FOREACH_SAFE(first_anim, anim, tmp)
    {
        maps_anims_remove(anim);
    }
}

/**
 * Play map animations.
 */
void map_anims_play(void)
{
    map_anim_t *anim, *tmp;
    int xpos, ypos, tmp_off;
    int num_ticks;
    char buf[32];
    int tmp_y;

    int player_height_offset = get_top_floor_height(MAP_CELL_GET_MIDDLE(
            map_width - (map_width / 2) - 1, map_height - (map_height / 2) - 1),
            MapData.player_sub_layer);

    DL_FOREACH_SAFE(first_anim, anim, tmp)
    {
        /* Have we passed the last tick */
        if (LastTick > anim->last_tick) {
            maps_anims_remove(anim);
            continue;
        }

        num_ticks = LastTick - anim->start_tick;

        if (anim->mapx < MAP_STARTX || anim->mapx >= MAP_STARTX + MAP_WIDTH ||
                anim->mapy < MAP_STARTY || anim->mapy >= MAP_STARTY +
                MAP_HEIGHT) {
            continue;
        }

        tmp_y = anim->y - (int) ((float) num_ticks * anim->yoff);
        xpos = cur_widget[MAP_ID]->x + (int) ((MAP_START_XOFF + (anim->mapx -
                MAP_STARTX) * MAP_TILE_YOFF - (anim->mapy - MAP_STARTY - 1) *
                MAP_TILE_YOFF - 4) * (setting_get_int(OPT_CAT_MAP,
                OPT_MAP_ZOOM) / 100.0));
        ypos = cur_widget[MAP_ID]->y + (int) ((MAP_START_YOFF + (anim->mapx -
                MAP_STARTX) * MAP_TILE_XOFF + (anim->mapy - MAP_STARTY - 1) *
                MAP_TILE_XOFF - 34) * (setting_get_int(OPT_CAT_MAP,
                OPT_MAP_ZOOM) / 100.0));
        ypos += player_height_offset;
        ypos -= MAP_CELL_GET(anim->mapx, anim->mapy)->height[
                GET_MAP_LAYER(LAYER_FLOOR, anim->sub_layer)];

        switch (anim->type) {
        case ANIM_DAMAGE:
            if (anim->value < 0) {
                snprintf(buf, sizeof(buf), "%d", abs(anim->value));
                text_show(ScreenSurface, FONT_MONO10, buf, xpos + anim->x + 4 -
                        (int) strlen(buf) * 4 + 1, ypos + tmp_y + 1,
                        COLOR_GREEN, TEXT_OUTLINE, NULL);
            } else {
                snprintf(buf, sizeof(buf), "%d", anim->value);
                text_show(ScreenSurface, FONT_MONO10, buf, xpos + anim->x + 4 -
                        (int) strlen(buf) * 4 + 1, ypos + tmp_y + 1,
                        COLOR_ORANGE, TEXT_OUTLINE, NULL);
            }

            break;

        case ANIM_KILL:
            surface_show(ScreenSurface, xpos + anim->x - 5, ypos + tmp_y - 4,
                    NULL, TEXTURE_CLIENT("death"));
            snprintf(buf, sizeof(buf), "%d", anim->value);

            tmp_off = 0;

            /* Let's check the size of the value */
            if (anim->value < 10) {
                tmp_off = 6;
            } else if (anim->value < 100) {
                tmp_off = 0;
            } else if (anim->value < 1000) {
                tmp_off = -6;
            } else if (anim->value < 10000) {
                tmp_off = -12;
            }

            text_show(ScreenSurface, FONT_MONO10, buf, xpos + anim->x + tmp_off,
                    ypos + tmp_y, COLOR_ORANGE, TEXT_OUTLINE, NULL);

            break;

        default:
            LOG(BUG, "Unknown animation type");
            break;
        }
    }
}

/**
 * Check whether the damage animations need redrawing.
 * @return 1 if the damage animations need redrawing, 0 otherwise.
 */
int map_anims_need_redraw(void)
{
    return first_anim != NULL;
}

/**
 * Loads multi-arch object data offsets.
 */
void load_mapdef_dat(void)
{
    FILE *stream;
    int i, ii, x, y, d[32];
    char line[MAX_BUF];

    stream = fopen_wrapper(ARCHDEF_FILE, "r");

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
                "%d %d %d %d %d %d %d %d %d %d %d %d %d %d", &x, &y, &d[0],
                &d[1], &d[2], &d[3], &d[4], &d[5], &d[6], &d[7], &d[8], &d[9],
                &d[10], &d[11], &d[12], &d[13], &d[14], &d[15], &d[16], &d[17],
                &d[18], &d[19], &d[20], &d[21], &d[22], &d[23], &d[24], &d[25],
                &d[26], &d[27], &d[28], &d[29], &d[30], &d[31]);
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
 * @param hard Hard reset
 */
void clear_map(bool hard)
{
    size_t cells_size;

    /* Cache the map width and height. */
    map_width = setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH);
    map_height = setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT);

    cells_size = sizeof(*cells) * map_width * MAP_FOW_SIZE * map_height *
            MAP_FOW_SIZE;

    if (cells == NULL) {
        cells = emalloc(cells_size);
    }

    memset(cells, 0, cells_size);
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
 * @param w New width.
 * @param h New height.
 */
void map_update_size(int w, int h)
{
    int old_w, old_h, x, y;
    struct MapCell *cell;

    old_w = map_width;
    old_h = map_height;

    if (w != 0) {
        map_width = w;
    }

    if (h != 0) {
        map_height = h;
    }

    display_mapscroll(old_w - map_width, old_h - map_height,
            old_w * MAP_FOW_SIZE, old_h * MAP_FOW_SIZE);

    /* Clear all the Fog of War cells. */
    for (x = 0; x < map_width * MAP_FOW_SIZE; x++) {
        for (y = 0; y < map_height * MAP_FOW_SIZE; y++) {
            cell = MAP_CELL_GET(x, y);

            if (cell->fow) {
                memset(cell, 0, sizeof(struct MapCell));
            }
        }
    }
}

/**
 * Scroll the map.
 * @param dx X offset.
 * @param dy Y offset.
 * @param old_w Old width. 0 if width hasn't changed.
 * @param old_h Old height. 0 if height hasn't changed.
 */
void display_mapscroll(int dx, int dy, int old_w, int old_h)
{
    int x, y, w, h;
    struct MapCell *cells_old;

    w = map_width * MAP_FOW_SIZE;
    h = map_height * MAP_FOW_SIZE;

    if (old_w == 0) {
        old_w = w;
    }

    if (old_h == 0) {
        old_h = h;
    }

    cells_old = cells;
    cells = emalloc(sizeof(*cells) * w * h);

    for (x = 0; x < w; x++) {
        for (y = 0; y < h; y++) {
            if (x + dx < 0 || x + dx >= old_w || y + dy < 0 ||
                    y + dy >= old_h) {
                memset(&(cells[y * w + x]), 0, sizeof(struct MapCell));
            } else {
                memcpy(&(cells[y * w + x]),
                        &(cells_old[(y + dy) * old_w + x + dx]),
                        sizeof(struct MapCell));
            }

            if (x < map_width * (MAP_FOW_SIZE / 2) ||
                    x >= map_width + map_width * (MAP_FOW_SIZE / 2) ||
                    y < map_height * (MAP_FOW_SIZE / 2) ||
                    y >= map_height + map_height * (MAP_FOW_SIZE / 2)) {
                cells[y * w + x].fow = 1;
            }
        }
    }

    efree(cells_old);

    sound_ambient_mapcroll(dx, dy);
    map_anims_mapscroll(dx, dy);
    cpl.target_object_index = 0;
}

/**
 * Update map's name.
 * @param name New map name.
 */
void update_map_name(const char *name)
{
    snprintf(MapData.name_new, sizeof(MapData.name_new), "%s", name);
}

/**
 * Update map's weather.
 * @param weather New weather.
 */
void update_map_weather(const char *weather)
{
    effect_start(weather);
}

/**
 * Update map's height difference rendering flag.
 */
void update_map_height_diff(uint8_t height_diff)
{
    MapData.height_diff = height_diff;
}

/**
 * Update map's region name.
 * @param region_name New region name.
 */
void update_map_region_name(const char *region_name)
{
    if (strcmp(MapData.region_name, region_name) == 0) {
        return;
    }

    snprintf(VS(MapData.region_name), "%s", region_name);
    region_map_update(MapData.region_map, region_name);
}

/**
 * Update map's region long name.
 * @param region_longname New region long name.
 */
void update_map_region_longname(const char *region_longname)
{
    snprintf(VS(MapData.region_longname), "%s", region_longname);
}

/**
 * Update map's path.
 * @param map_path New map path.
 */
void update_map_path(const char *map_path)
{
    snprintf(VS(MapData.map_path), "%s", map_path);
}

/**
 * Updates the map's in_building state flag.
 *
 * When entering a building, clears FoW objects from effect layer with non-zero
 * sub-layer.
 * @param in_building New in_building state.
 */
void map_update_in_building(uint8_t in_building)
{
    if (in_building && !MapData.in_building) {
        int x, y;
        struct MapCell *cell;
        int layer, sub_layer;

        for (x = 0; x < map_width * MAP_FOW_SIZE; x++) {
            for (y = 0; y < map_height * MAP_FOW_SIZE; y++) {
                cell = MAP_CELL_GET(x, y);

                if (!cell->fow) {
                    continue;
                }

                for (sub_layer = MapData.player_sub_layer + 1;
                        sub_layer < NUM_SUB_LAYERS; sub_layer++) {
                    for (layer = LAYER_FLOOR; layer <= NUM_LAYERS; layer++) {
                        cell->faces[GET_MAP_LAYER(layer, sub_layer)] = 0;
                    }
                }
            }
        }
    }

    MapData.in_building = in_building;
}

/**
 * Get player's direction.
 * @return Player's direction.
 */
int map_get_player_direction(void)
{
    struct MapCell *cell;
    int direction;

    cell = MAP_CELL_GET_MIDDLE(map_width - (map_width / 2) - 1,
            map_height - (map_height / 2) - 1);

    direction = cell->anim_facing[GET_MAP_LAYER(LAYER_LIVING,
            MapData.player_sub_layer)];

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
void map_get_real_coords(int *x, int *y)
{
    *x = MapData.posx - (map_width / 2);
    *y = MapData.posy - (map_height / 2);
}

/**
 * Initialize map's data.
 * @param xl Map width.
 * @param yl Map height.
 * @param px Player's X position.
 * @param py Player's Y position.
 */
void init_map_data(int xl, int yl, int px, int py)
{
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

/**
 * Calculate height of X/Y coordinate on the specified cell.
 *
 * Checks for X/Y overflows.
 * @param x X position.
 * @param y Y position.
 * @param w Max width.
 * @param h Max height.
 * @return The height.
 */
static int calc_map_cell_height(int x, int y, int w, int h, int sub_layer,
        int my_height)
{
    if (x >= 0 && x < w && y >= 0 && y < h) {
        bool is_building_wall = false;

        for (int i = 1; i < NUM_SUB_LAYERS; i++) {
            if (cells[y * w + x].faces[GET_MAP_LAYER(LAYER_EFFECT, i)] != 0 &&
                    cells[y * w + x].height[GET_MAP_LAYER(LAYER_FLOOR, i)]
                    != 0) {
                is_building_wall = true;
                break;
            }
        }

        if (my_height < 0 && (sub_layer != 0 || is_building_wall)) {
            for (sub_layer = NUM_SUB_LAYERS - 1; sub_layer >= 0; sub_layer--) {
                int height = cells[y * w + x].height[GET_MAP_LAYER(LAYER_FLOOR,
                        sub_layer)];

                if (height != 0) {
                    return height;
                }
            }

            return 0;
        }

        return cells[y * w + x].height[GET_MAP_LAYER(LAYER_FLOOR, sub_layer)];
    }

    return 0;
}

/**
 * Align tile stretch based on X/Y.
 * @param x X position.
 * @param y Y position.
 * @param w Max width.
 * @param h Max height.
 * @param sub_layer Sub-layer.
 */
static void align_tile_stretch(int x, int y, int w, int h, int sub_layer)
{
    int top, bottom, right, left, min_ht;
    int32_t stretch;
    int nw_height, n_height, ne_height, sw_height, s_height, se_height,
            w_height, e_height, my_height;

    if (x < 0 || y < 0 || x >= w || y >= h) {
        return;
    }

    my_height = calc_map_cell_height(x, y, w, h, sub_layer, 0);
    nw_height = calc_map_cell_height(x - 1, y - 1, w, h, sub_layer, my_height);
    n_height = calc_map_cell_height(x, y - 1, w, h, sub_layer, my_height);
    ne_height = calc_map_cell_height(x + 1, y - 1, w, h, sub_layer, my_height);
    sw_height = calc_map_cell_height(x - 1, y + 1, w, h, sub_layer, my_height);
    s_height = calc_map_cell_height(x, y + 1, w, h, sub_layer, my_height);
    se_height = calc_map_cell_height(x + 1, y + 1, w, h, sub_layer, my_height);
    w_height = calc_map_cell_height(x - 1, y, w, h, sub_layer, my_height);
    e_height = calc_map_cell_height(x + 1, y, w, h, sub_layer, my_height);

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
    cells[y * w + x].stretch[sub_layer] = stretch;
}

/**
 * Adjust the tile stretch of a map.
 *
 * Goes through the whole map and for each coordinate calls align_tile_stretch()
 * in all directions. This is done to fix any inconsistencies, since the map
 * command doesn't send us the whole map all over again, but only new/changes
 * parts.
 */
void adjust_tile_stretch(void)
{
    int xoff, yoff, w, h, x, y, sub_layer;

    xoff = map_width * (MAP_FOW_SIZE / 2);
    yoff = map_height * (MAP_FOW_SIZE / 2);
    w = map_width * MAP_FOW_SIZE;
    h = map_height * MAP_FOW_SIZE;

    for (x = xoff; x < xoff + map_width; x++) {
        for (y = yoff; y < yoff + map_height; y++) {
            for (sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
                align_tile_stretch(x, y, w, h, sub_layer);
            }
        }
    }
}

/**
 * Set data for map cell.
 *
 * If FOW was previously set on this cell, cell data is cleared.
 * @param x X of the cell.
 * @param y Y of the cell.
 * @param layer Layer we're doing this for.
 * @param face Face to set.
 * @param quick_pos Is this a multi-arch?
 * @param obj_flags Flags.
 * @param name Player's name.
 * @param name_color Player's name color.
 * @param height Z position of the tile.
 * @param probe Target's HP bar.
 * @param zoom How much to zoom the face by.
 * @param align X align.
 * @param rotate Rotation in degrees.
 * @param infravision Whether to show the object in red.
 */
void map_set_data(int x, int y, int layer, int16_t face,
        uint8_t quick_pos, uint8_t obj_flags, const char *name,
        const char *name_color, int16_t height, uint8_t probe, int16_t zoom_x,
        int16_t zoom_y, int16_t align, uint8_t draw_double, uint8_t alpha,
        int16_t rotate, uint8_t infravision, uint32_t target_object_count,
        uint8_t target_is_friend, uint8_t anim_speed, uint8_t anim_facing,
        uint8_t anim_flags, uint8_t anim_state, uint8_t priority,
        uint8_t secondpass, const char *glow, uint8_t glow_speed)
{
    struct MapCell *cell;
    int sub_layer;

    cell = MAP_CELL_GET_MIDDLE(x, y);
    sub_layer = layer / NUM_LAYERS;

    if (cell->fow) {
        int i;

        cell->fow = 0;

        for (i = 0; i < NUM_REAL_LAYERS; i++) {
            cell->faces[i] = 0;
            cell->flags[i] = 0;
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
        }
    }

    if (anim_speed != 0 && cell->faces[layer] != face) {
        cell->anim_state[layer] = 0;
    }

    cell->priority[sub_layer] |= priority << (((layer % NUM_LAYERS) + 1) - 1);
    cell->secondpass[sub_layer] |= secondpass << (((layer % NUM_LAYERS) + 1) -
            1);

    cell->faces[layer] = face;
    cell->flags[layer] = obj_flags;

    cell->probe[layer] = probe;
    cell->quick_pos[layer] = quick_pos;

    snprintf(VS(cell->pcolor[layer]), "%s", name_color);
    snprintf(VS(cell->pname[layer]), "%s", name);
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

    if (cell->target_object_count[layer] != target_object_count ||
            cell->target_is_friend[layer] != target_is_friend) {
        cpl.target_object_index = 0;
    }

    cell->target_object_count[layer] = target_object_count;
    cell->target_is_friend[layer] = target_is_friend;

    cell->anim_speed[layer] = anim_speed;
    cell->anim_facing[layer] = anim_facing;

    if (((layer % NUM_LAYERS) + 1) == LAYER_LIVING) {
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
        check_animation_status(face);
    } else {
        request_face(face);
    }
}

/**
 * Clear map's cell.
 *
 * In reality, this only clears some data on the cell, and sets the FOW flag
 * to mark that the cell is actually FOW.
 * @param x X of the cell.
 * @param y Y of the cell.
 */
void map_clear_cell(int x, int y)
{
    struct MapCell *cell;
    int layer;

    cell = MAP_CELL_GET_MIDDLE(x, y);
    cell->fow = 1;

    for (layer = 0; layer < NUM_REAL_LAYERS; layer++) {
        cell->probe[layer] = 0;
        cell->target_object_count[layer] = 0;
        cell->target_is_friend[layer] = 0;
        cell->pname[layer][0] = '\0';
    }
}

/**
 * Set darkness for map's cell.
 * @param x X of the cell.
 * @param y Y of the cell.
 * @param sub_layer Sub-layer.
 * @param darkness Darkness to set.
 */
void map_set_darkness(int x, int y, int sub_layer, uint8_t darkness)
{
    struct MapCell *cell;

    cell = MAP_CELL_GET_MIDDLE(x, y);
    cell->darkness[sub_layer] = darkness;
}

/**
 * Get the height of the topmost floor on the specified square.
 * @param x X position.
 * @param y Y position.
 * @return The height.
 */
static int get_top_floor_height(struct MapCell *cell, int sub_layer)
{
    int16_t height;

    height = cell->height[GET_MAP_LAYER(LAYER_FLOOR, sub_layer)];

    return MAX(0, height);
}

static void map_animate_object(struct MapCell *cell, int layer)
{
    Animations *animation;

    if (cell->faces[layer] == 0 || cell->anim_speed[layer] == 0 ||
            cell->anim_facing[layer] == 0) {
        return;
    }

    animation = &animations[cell->faces[layer]];

    if (animation->num_animations == 0) {
        return;
    }

    if (!(cell->flags[layer] & FFLAG_SLEEP) &&
            !(cell->flags[layer] & FFLAG_PARALYZED)) {
        cell->anim_state[layer]++;
        map_redraw_flag = 1;
    }

    /* If beyond drawable states, reset */
    if (cell->anim_state[layer] >=
            animation->num_animations / animation->facings) {
        cell->anim_state[layer] = 0;
    }
}

void map_animate(void)
{
    int x, y, layer;
    struct MapCell *cell;

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

static uint16_t map_object_get_face(struct MapCell *cell, int layer)
{
    int sub_layer, dir, state;
    Animations *animation;

    if (cell->anim_speed[layer] == 0) {
        return cell->faces[layer];
    }

    animation = &animations[cell->faces[layer]];

    if (animation->num_animations == 0) {
        return cell->faces[layer];
    }

    sub_layer = layer / NUM_LAYERS;
    dir = cell->anim_facing[layer] - 1;
    state = 0;

    if (animation->facings == 9) {
        state = dir * (animation->num_animations / 9);
    } else if (animation->facings >= 25) {
        if (cell->anim_flags[sub_layer] & ANIM_FLAG_ATTACKING) {
            dir += 16;
        } else if (cell->anim_flags[sub_layer] & ANIM_FLAG_MOVING) {
            dir += 8;
        }

        state = dir * (animation->num_animations / animation->facings);
    }

    return animation->faces[cell->anim_state[layer] + state];
}

/**
 * Draw a single object on the map.
 * @param x X position of the object.
 * @param y Y position of the object.
 * @param k Layer.
 * @param player_height_offset Player's height offset.
 * @param[out] target_cell Where to store the map cell the player's
 * target is at, if any.
 * @param[out] target_layer Where to store the layer the player's target
 * is at, if any.
 * @param target_rect Where to store coordinate info for target.
 */
static void draw_map_object(SDL_Surface *surface, struct MapCell *cell,
        int x, int y, int layer, int sub_layer, int player_height_offset,
        struct MapCell **target_cell, int *target_layer, SDL_Rect *target_rect,
        SDL_Rect **tiles, size_t *tiles_num, uint8_t alpha_forced)
{

    sprite_struct *face_sprite;
    int ypos, xpos;
    int xl, yl;
    int xml, xmpos, xtemp = 0;
    uint16_t face;
    int mid, mnr;
    int bitmap_h, bitmap_w;
    sprite_effects_t effects;

    xpos = MAP_START_XOFF + x * MAP_TILE_YOFF - y * MAP_TILE_YOFF;
    ypos = MAP_START_YOFF + x * MAP_TILE_XOFF + y * MAP_TILE_XOFF;

    if (surface != cur_widget[MAP_ID]->surface) {
        xpos -= map_width * MAP_TILE_YOFF - map_height * MAP_TILE_YOFF * (MAP_FOW_SIZE / 2);
        ypos -= map_width * MAP_TILE_XOFF + map_height * MAP_TILE_XOFF * (MAP_FOW_SIZE / 2);
        ypos -= MAP_START_YOFF;
        ypos += MAP_START_YOFF * (MAP_FOW_SIZE / 2);
        ypos -= map_height * MAP_TILE_XOFF / 2;
    }

    face = map_object_get_face(cell, GET_MAP_LAYER(layer, sub_layer));

    if (face <= 0 || face >= MAX_FACE_TILES) {
        return;
    }

    face_sprite = FaceList[face].sprite;

    if (!face_sprite) {
        return;
    }

    if ((cell->fow || MapData.height_diff) && abs(get_top_floor_height(
            cell, sub_layer) - player_height_offset) > HEIGHT_MAX_RENDER) {
        if (surface == cur_widget[MAP_ID]->surface) {
            return;
        }
    }

    bitmap_h = face_sprite->bitmap->h;
    bitmap_w = face_sprite->bitmap->w;

    memset(&effects, 0, sizeof(effects));
    effects.rotate = cell->rotate[GET_MAP_LAYER(layer, sub_layer)];
    effects.zoom_x = cell->zoom_x[GET_MAP_LAYER(layer, sub_layer)];
    effects.zoom_y = cell->zoom_y[GET_MAP_LAYER(layer, sub_layer)];

    if (effects.rotate) {
        rotozoomSurfaceSizeXY(bitmap_w, bitmap_h,
                effects.rotate, effects.zoom_x ? effects.zoom_x / 100.0 : 1.0,
                effects.zoom_y ? effects.zoom_y / 100.0 : 1.0,
                &bitmap_w, &bitmap_h);
    } else if ((effects.zoom_x && effects.zoom_x != 100) ||
            (effects.zoom_y && effects.zoom_y != 100)) {
        zoomSurfaceSize(bitmap_w, bitmap_h,
                effects.zoom_x ? effects.zoom_x / 100.0 : 1.0,
                effects.zoom_y ? effects.zoom_y / 100.0 : 1.0,
                &bitmap_w, &bitmap_h);
    }

    /* Multi-part object? */
    if (cell->quick_pos[GET_MAP_LAYER(layer, sub_layer)]) {
        mnr = cell->quick_pos[GET_MAP_LAYER(layer, sub_layer)];
        mid = mnr >> 4;
        mnr &= 0x0f;
        xml = MultiArchs[mid].xlen;
        yl = ypos - MultiArchs[mid].part[mnr].yoff + MultiArchs[mid].ylen -
                bitmap_h;

        /* Center overlapping X borders */
        xl = 0;

        if (bitmap_w > MultiArchs[mid].xlen) {
            xl = (MultiArchs[mid].xlen - bitmap_w) >> 1;
        }

        xmpos = xpos - MultiArchs[mid].part[mnr].xoff;
        xl += xmpos;
    } else {
        /* Calculate offsets */
        xml = MAP_TILE_POS_XOFF;
        yl = (ypos + MAP_TILE_POS_YOFF) - bitmap_h;
        xmpos = xl = xpos;

        if (bitmap_w > MAP_TILE_POS_XOFF) {
            xl -= (bitmap_w - MAP_TILE_POS_XOFF) / 2;
        }
    }

    if (cell->align[GET_MAP_LAYER(layer, sub_layer)]) {
        xl += cell->align[GET_MAP_LAYER(layer, sub_layer)];
    }

    snprintf(VS(effects.glow), "%s",
            cell->glow[GET_MAP_LAYER(layer, sub_layer)]);
    effects.glow_speed = cell->glow_speed[GET_MAP_LAYER(layer, sub_layer)];
    effects.glow_state = cell->glow_state[GET_MAP_LAYER(layer, sub_layer)];

    if (effect_has_overlay()) {
        BIT_SET(effects.flags, SPRITE_FLAG_EFFECTS);
    }

    if (cell->fow) {
        BIT_SET(effects.flags, SPRITE_FLAG_FOW);
    } else if (cell->infravision[GET_MAP_LAYER(layer, sub_layer)]) {
        BIT_SET(effects.flags, SPRITE_FLAG_RED);
    } else if (cell->flags[GET_MAP_LAYER(layer, sub_layer)] & FFLAG_INVISIBLE) {
        BIT_SET(effects.flags, SPRITE_FLAG_GRAY);
    } else {
        BIT_SET(effects.flags, SPRITE_FLAG_DARK);
    }

    if (surface != cur_widget[MAP_ID]->surface) {
        BITMASK_CLEAR(effects.flags, BIT_MASK(SPRITE_FLAG_RED) |
                BIT_MASK(SPRITE_FLAG_FOW));
        BIT_SET(effects.flags, SPRITE_FLAG_DARK);
    }

    if (BIT_QUERY(effects.flags, SPRITE_FLAG_DARK)) {
        if (cell->darkness[sub_layer] == 210) {
            effects.dark_level = 0;
        } else if (cell->darkness[sub_layer] == 180) {
            effects.dark_level = 1;
        } else if (cell->darkness[sub_layer] == 150) {
            effects.dark_level = 2;
        } else if (cell->darkness[sub_layer] == 120) {
            effects.dark_level = 3;
        } else if (cell->darkness[sub_layer] == 90) {
            effects.dark_level = 4;
        } else if (cell->darkness[sub_layer] == 60) {
            effects.dark_level = 5;
        } else if (cell->darkness[sub_layer] == 0) {
            effects.dark_level = 7;
        } else {
            effects.dark_level = 6;
        }
    }

    if (cell->alpha[GET_MAP_LAYER(layer, sub_layer)]) {
        effects.alpha = cell->alpha[GET_MAP_LAYER(layer, sub_layer)];
    }

    if (alpha_forced != 0) {
        effects.alpha = effects.alpha != 0 ? MIN(effects.alpha, alpha_forced) :
            alpha_forced;
    }

    if (layer <= 2 && cell->stretch[sub_layer]) {
        effects.stretch = cell->stretch[sub_layer];
    }

    if (layer == LAYER_LIVING || layer == LAYER_EFFECT || layer == LAYER_ITEM ||
            layer == LAYER_ITEM2) {
        yl -= get_top_floor_height(cell, sub_layer);
    } else {
        yl -= cell->height[GET_MAP_LAYER(LAYER_FLOOR, sub_layer)];
    }

    yl += player_height_offset;

    if (layer > 1) {
        yl -= cell->height[GET_MAP_LAYER(layer, sub_layer)];
    }

    surface_show_effects(surface, xl, yl, NULL, face_sprite->bitmap, &effects);

    /* Double faces are shown twice, one above the other, when not lower
     * on the screen than the player. This simulates high walls without
     * obscuring the user's view. */
    if (cell->draw_double[GET_MAP_LAYER(layer, sub_layer)]) {
        surface_show_effects(surface, xl, yl - 22, NULL, face_sprite->bitmap,
                &effects);
    }

    if (surface != cur_widget[MAP_ID]->surface) {
        return;
    }

    if (xml == MAP_TILE_POS_XOFF) {
        xtemp = (int) (((double) xml / 100.0) * 25.0);
    } else {
        xtemp = (int) (((double) xml / 100.0) * 20.0);
    }

    /* Do we have a playername? Then print it! */
    if (cell->pname[GET_MAP_LAYER(layer, sub_layer)][0] != '\0' &&
            setting_get_int(OPT_CAT_MAP, OPT_PLAYER_NAMES)) {
        uint8_t draw_name;
        char *name;

        draw_name = 0;
        name = cell->pname[GET_MAP_LAYER(layer, sub_layer)];

        if (setting_get_int(OPT_CAT_MAP, OPT_PLAYER_NAMES) == 1) {
            draw_name = 1;
        } else if (setting_get_int(OPT_CAT_MAP, OPT_PLAYER_NAMES) == 2) {
            if (strncasecmp(name, cpl.name, strlen(name)) != 0) {
                draw_name = 1;
            }
        } else if (setting_get_int(OPT_CAT_MAP, OPT_PLAYER_NAMES) == 3) {
            if (strncasecmp(name, cpl.name, strlen(name)) == 0) {
                draw_name = 1;
            }
        }

        if (draw_name) {
            text_show(surface, FONT_SANS9, name,
                    xmpos + xtemp + (xml - xtemp * 2) / 2 -
                    text_get_width(FONT_SANS9, name, 0) / 2 - 2, yl - 24,
                    cell->pcolor[GET_MAP_LAYER(layer, sub_layer)], TEXT_OUTLINE,
                    NULL);
        }
    }

    /* Perhaps the object has a marked effect, show it. */
    if (cell->flags[GET_MAP_LAYER(layer, sub_layer)]) {
        sprite_effects_t effects2;

        memset(&effects2, 0, sizeof(effects2));
        effects2.alpha = effects.alpha;
        effects2.stretch = effects.stretch;
        effects2.zoom_x = effects.zoom_x;
        effects2.zoom_y = effects.zoom_y;
        effects2.rotate = effects.rotate;

        if (cell->flags[GET_MAP_LAYER(layer, sub_layer)] & FFLAG_SLEEP) {
            surface_show_effects(surface, xl + bitmap_w / 2,
                    yl - 5, NULL, TEXTURE_CLIENT("sleep"), &effects2);
        }

        if (cell->flags[GET_MAP_LAYER(layer, sub_layer)] & FFLAG_CONFUSED) {
            surface_show_effects(surface, xl + bitmap_w /
                    2 - 1, yl - 4, NULL, TEXTURE_CLIENT("confused"), &effects2);
        }

        if (cell->flags[GET_MAP_LAYER(layer, sub_layer)] & FFLAG_SCARED) {
            surface_show_effects(surface, xl + bitmap_w /
                    2 + 10, yl - 4, NULL, TEXTURE_CLIENT("scared"), &effects2);
        }

        if (cell->flags[GET_MAP_LAYER(layer, sub_layer)] & FFLAG_BLINDED) {
            surface_show_effects(surface, xl + bitmap_w /
                    2 + 3, yl - 6, NULL, TEXTURE_CLIENT("blind"), &effects2);
        }

        if (cell->flags[GET_MAP_LAYER(layer, sub_layer)] & FFLAG_PARALYZED) {
            surface_show_effects(surface, xl + bitmap_w /
                    2 + 3, yl + 3, NULL, TEXTURE_CLIENT("paralyzed"),
                    &effects2);
        }
    }

    if (layer == LAYER_FLOOR && tiles_debug) {
        *tiles = erealloc(*tiles, sizeof(**tiles) * ((*tiles_num) + 1));
        (*tiles)[*tiles_num].x = xl;
        (*tiles)[*tiles_num].y = yl;
        (*tiles)[*tiles_num].w = x;
        (*tiles)[*tiles_num].h = y;
        (*tiles_num)++;
    }

    if (cell->probe[GET_MAP_LAYER(layer, sub_layer)]) {
        *target_cell = cell;
        *target_layer = GET_MAP_LAYER(layer, sub_layer);
        target_rect->x = xmpos + xtemp;
        target_rect->y = yl - 9;
        target_rect->w = (xml - xtemp * 2);
        target_rect->h = 1;
    }
}

/**
 * Calculates whether the specified coordinates are behind a wall.
 * @param dx Start X.
 * @param dy Start Y.
 * @param sx End X.
 * @param sy End Y.
 * @return Whether the coordinates @p dx and @p dy are behind a wall or not.
 */
static bool obj_is_behind_wall(int dx, int dy, int sx, int sy)
{
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

/**
 * Draw the map. */
void map_draw_map(SDL_Surface *surface)
{
    int player_height_offset;
    int x, y, layer, sub_layer, target_layer;
    struct MapCell *target_cell, *cell;
    SDL_Rect target_rect, *tiles;
    int tx, ty, w, h;
    size_t tiles_num;

    player_height_offset = get_top_floor_height(MAP_CELL_GET_MIDDLE(
            map_width - (map_width / 2) - 1, map_height - (map_height / 2) - 1),
            MapData.player_sub_layer);
    target_cell = NULL;
    tiles = NULL;
    tiles_num = 0;

    if (surface == cur_widget[MAP_ID]->surface) {
        w = map_width;
        h = map_height;
    } else {
        w = map_width * MAP_FOW_SIZE;
        h = map_height * MAP_FOW_SIZE;
    }

#define MAP_CELL_GET_MIDDLE_IF(_x, _y, _w, _h) \
    ((_w) == map_width && (_h) == map_height) ? MAP_CELL_GET_MIDDLE(_x, _y) : \
        MAP_CELL_GET(_x, _y)

    /* Draw floor and fmasks. */
    for (x = 0; x < w; x++) {
        for (y = 0; y < h; y++) {
            cell = MAP_CELL_GET_MIDDLE_IF(x, y, w, h);

            for (layer = LAYER_FLOOR; layer <= LAYER_FMASK; layer++) {
                if (cell->priority[0] & (1 << (layer - 1))) {
                    continue;
                }

                draw_map_object(surface, cell, x, y, layer, 0,
                        player_height_offset, &target_cell, &target_layer,
                        &target_rect, &tiles, &tiles_num, 0);
            }
        }
    }

    /* Now draw everything else. */
    for (x = 0; x < w; x++) {
        for (y = 0; y < h; y++) {
            cell = MAP_CELL_GET_MIDDLE_IF(x, y, w, h);

            for (layer = LAYER_FLOOR; layer <= NUM_LAYERS; layer++) {
                for (sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
                    if (sub_layer == 0 && (layer == LAYER_FLOOR ||
                            layer == LAYER_FMASK)) {
                        continue;
                    }

                    /* Skip objects on the effect layer with non-zero sub-layer
                     * because they will be rendered later. */
                    if (layer == LAYER_EFFECT && sub_layer != 0) {
                        if (cell->height[GET_MAP_LAYER(LAYER_EFFECT,
                                sub_layer)] >= cell->height[GET_MAP_LAYER(
                                LAYER_FLOOR, MapData.player_sub_layer)]) {
                            continue;
                        }
                    }

                    if (cell->priority[sub_layer] & (1 << (layer - 1))) {
                        continue;
                    }

                    draw_map_object(surface, cell, x, y, layer, sub_layer,
                            player_height_offset, &target_cell, &target_layer,
                            &target_rect, &tiles, &tiles_num, 0);
                }
            }

            for (sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
                if (cell->height[GET_MAP_LAYER(LAYER_FLOOR, sub_layer)] >=
                        cell->height[GET_MAP_LAYER(LAYER_FLOOR,
                        MapData.player_sub_layer)]) {
                    continue;
                }

                for (layer = LAYER_FLOOR; layer <= NUM_LAYERS; layer++) {
                    if (!(cell->priority[sub_layer] & (1 << (layer - 1)))) {
                        continue;
                    }

                    if (layer == LAYER_EFFECT && sub_layer != 0) {
                        if (cell->height[GET_MAP_LAYER(LAYER_EFFECT,
                                sub_layer)] >= cell->height[GET_MAP_LAYER(
                                LAYER_FLOOR, MapData.player_sub_layer)]) {
                            continue;
                        }
                    }

                    draw_map_object(surface, cell, x, y, layer, sub_layer,
                            player_height_offset, &target_cell, &target_layer,
                            &target_rect, &tiles, &tiles_num, 0);
                }
            }

            for (layer = LAYER_FLOOR; layer <= NUM_LAYERS; layer++) {
                if (!(cell->priority[0] & (1 << (layer - 1)))) {
                    continue;
                }

                draw_map_object(surface, cell, x, y, layer, 0,
                        player_height_offset, &target_cell, &target_layer,
                        &target_rect, &tiles, &tiles_num, 0);
            }

            for (sub_layer = NUM_SUB_LAYERS - 1; sub_layer >= 1; sub_layer--) {
                if (cell->priority[sub_layer] & (1 << (LAYER_EFFECT - 1))) {
                    continue;
                }

                if (cell->height[GET_MAP_LAYER(LAYER_EFFECT, sub_layer)] <
                        cell->height[GET_MAP_LAYER(LAYER_FLOOR,
                        MapData.player_sub_layer)]) {
                    continue;
                }

                int distance_x = x - map_width / 2;
                int distance_y = y - map_height / 2;
                int distance = isqrt(distance_x * distance_x +
                                     distance_y * distance_y);

                if (w == map_width && h == map_height && distance <= 3 &&
                        (x >= map_width / 2 && y >= map_height / 2)) {
                    bool cull = false;
                    int range = 2;

                    for (int sub_layer2 = NUM_SUB_LAYERS - 1; sub_layer2 > 0;
                            sub_layer2--) {
                        if (cell->height[GET_MAP_LAYER(LAYER_EFFECT,
                                sub_layer2)] - player_height_offset > 50) {
                            range = 0;
                        }
                    }

                    if (range == 0) {
                        cull = true;
                    }

                    for (int nx = x - range; nx <= x && !cull; nx++) {
                        for (int ny = y - range; ny <= y && !cull; ny++) {
                            MapCell *cell2 = MAP_CELL_GET_MIDDLE_IF(nx, ny,
                                    w, h);

                            for (int sub_layer2 = 0;
                                    sub_layer2 < NUM_SUB_LAYERS;
                                    sub_layer2++) {
                                if (cell2->secondpass[sub_layer2] &
                                        (1 << (LAYER_WALL - 1)) &&
                                        !obj_is_behind_wall(nx, ny,
                                        map_width / 2, map_height / 2)) {
                                    cull = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (cull && range != 0) {
                        continue;
                    }
                }

                draw_map_object(surface, cell, x, y, LAYER_EFFECT, sub_layer,
                        player_height_offset, &target_cell, &target_layer,
                        &target_rect, &tiles, &tiles_num, 0);
            }

            for (sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
                if (cell->height[GET_MAP_LAYER(LAYER_FLOOR, sub_layer)] <
                        cell->height[GET_MAP_LAYER(LAYER_FLOOR,
                        MapData.player_sub_layer)]) {
                    continue;
                }

                for (layer = LAYER_FLOOR; layer <= NUM_LAYERS; layer++) {
                    if (!(cell->priority[sub_layer] & (1 << (layer - 1)))) {
                        continue;
                    }

                    if (layer == LAYER_EFFECT && sub_layer != 0) {
                        if (cell->height[GET_MAP_LAYER(LAYER_FLOOR,
                                sub_layer)] < cell->height[GET_MAP_LAYER(
                                LAYER_FLOOR, MapData.player_sub_layer)]) {
                            continue;
                        }
                    }

                    draw_map_object(surface, cell, x, y, layer, sub_layer,
                            player_height_offset, &target_cell, &target_layer,
                            &target_rect, &tiles, &tiles_num, 0);
                }
            }

            for (sub_layer = NUM_SUB_LAYERS - 1; sub_layer >= 1; sub_layer--) {
                if (cell->height[GET_MAP_LAYER(LAYER_EFFECT, sub_layer)] <
                        cell->height[GET_MAP_LAYER(LAYER_FLOOR,
                        MapData.player_sub_layer)]) {
                    continue;
                }

                if (cell->height[GET_MAP_LAYER(LAYER_EFFECT,
                        sub_layer)] <= cell->height[GET_MAP_LAYER(
                        LAYER_FLOOR, sub_layer - 1)]) {
                    continue;
                }

                if (cell->height[GET_MAP_LAYER(LAYER_EFFECT,
                        sub_layer)] <= cell->height[GET_MAP_LAYER(
                        LAYER_EFFECT, sub_layer - 1)]) {
                    continue;
                }

                draw_map_object(surface, cell, x, y, LAYER_EFFECT, sub_layer,
                        player_height_offset, &target_cell, &target_layer,
                        &target_rect, &tiles, &tiles_num, 0);
            }
        }
    }

    if (surface != cur_widget[MAP_ID]->surface) {
        return;
    }

    for (x = 0; x < w; x++) {
        for (y = 0; y < h; y++) {
            cell = MAP_CELL_GET_MIDDLE_IF(x, y, w, h);

            for (sub_layer = NUM_SUB_LAYERS - 1; sub_layer >= 1;
                    sub_layer--) {
                if (cell->height[GET_MAP_LAYER(LAYER_EFFECT, sub_layer)] != 0 &&
                        cell->faces[GET_MAP_LAYER(LAYER_EFFECT,
                        sub_layer)] != 0) {
                    cell = NULL;
                    break;
                }
            }

            if (cell == NULL) {
                continue;
            }

            for (sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
                draw_map_object(surface, cell, x, y, LAYER_LIVING, sub_layer,
                        player_height_offset, &target_cell, &target_layer,
                        &target_rect, &tiles, &tiles_num, 100);
            }
        }
    }

    if (tiles != NULL) {
        size_t i;
        SDL_Rect box;

        for (i = 0; i < tiles_num; i++) {
            box.x = tiles[i].x;
            box.y = tiles[i].y;
            box.w = MAP_TILE_POS_XOFF;
            box.h = MAP_TILE_POS_YOFF;
            text_show_format(cur_widget[MAP_ID]->surface, FONT("arial", 9),
                    box.x, box.y, COLOR_WHITE, TEXT_OUTLINE |
                    TEXT_VALIGN_CENTER | TEXT_ALIGN_CENTER, &box, "%d,%d",
                    tiles[i].w, tiles[i].h);
        }

        efree(tiles);
    }

    if (widget_mouse_event.owner == cur_widget[MAP_ID] &&
            mouse_to_tile_coords(cursor_x, cursor_y, &tx, &ty)) {
        map_draw_one(tx, ty, TEXTURE_CLIENT("square_highlight"));
    }

    if (target_cell) {
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
                target_cell->pname[target_layer][0] != '\0')) {
            text_show(cur_widget[MAP_ID]->surface, FONT_SANS9, cpl.target_name,
                    target_rect.x + target_rect.w / 2 -
                    text_get_width(FONT_SANS9, cpl.target_name, 0) / 2,
                    target_rect.y - 15, cpl.target_color, TEXT_OUTLINE, NULL);
        }

        rectangle_create(cur_widget[MAP_ID]->surface, target_rect.x - 2,
                target_rect.y - 2, 1, 5, hp_color);
        rectangle_create(cur_widget[MAP_ID]->surface, target_rect.x - 2,
                target_rect.y - 2, 3, 1, hp_color);
        rectangle_create(cur_widget[MAP_ID]->surface, target_rect.x - 2,
                target_rect.y + 2, 3, 1, hp_color);
        rectangle_create(cur_widget[MAP_ID]->surface, target_rect.x +
                target_rect.w + 1, target_rect.y - 2, 1, 5, hp_color);
        rectangle_create(cur_widget[MAP_ID]->surface, target_rect.x +
                target_rect.w - 1, target_rect.y - 2, 3, 1, hp_color);
        rectangle_create(cur_widget[MAP_ID]->surface, target_rect.x +
                target_rect.w - 1, target_rect.y + 2, 3, 1, hp_color);

        target_rect.w = MAX(1, MIN(100, (int) (((double) target_rect.w /
                100.0) * (double) target_cell->probe[target_layer])));
        rectangle_create(cur_widget[MAP_ID]->surface, target_rect.x,
                target_rect.y, target_rect.w, target_rect.h, hp_color);
    }

#undef MAP_CELL_GET_MIDDLE_IF
}

/**
 * Draw one sprite on map.
 * @param x X position.
 * @param y Y position.
 * @param surface What to draw.
 */
void map_draw_one(int x, int y, SDL_Surface *surface)
{
    int xpos;
    int ypos;
    struct MapCell *cell;

    xpos = MAP_START_XOFF + x * MAP_TILE_YOFF - y * MAP_TILE_YOFF;
    ypos = (MAP_START_YOFF + x * MAP_TILE_XOFF + y * MAP_TILE_XOFF) +
            MAP_TILE_POS_YOFF - surface->h;

    if (surface->w > MAP_TILE_POS_XOFF) {
        xpos -= (surface->w - MAP_TILE_POS_XOFF) / 2;
    }

    cell = MAP_CELL_GET_MIDDLE(x, y);

    if (cell->faces[0] != 0) {
        ypos = (ypos - get_top_floor_height(cell, MapData.player_sub_layer)) +
                get_top_floor_height(MAP_CELL_GET_MIDDLE(
                map_width - (map_width / 2) - 1,
                map_height - (map_height / 2) - 1), MapData.player_sub_layer);
    }

    surface_show_effects(cur_widget[MAP_ID]->surface, xpos, ypos, NULL, surface,
            NULL);
}

/**
 * Send a command to move the player to the specified square.
 * @param tx Square X position.
 * @param ty Square Y position.
 */
static void send_move_path(int tx, int ty)
{
    packet_struct *packet;

    packet = packet_new(SERVER_CMD_MOVE_PATH, 8, 0);
    packet_append_uint8(packet, tx);
    packet_append_uint8(packet, ty);
    socket_send_packet(packet);
}

/**
 * Send a command to target an NPC.
 * @param tx NPC's X position.
 * @param ty NPC's Y position.
 * @param count NPC's UID.
 */
static void send_target(int x, int y, uint32_t count)
{
    packet_struct *packet;

    packet = packet_new(SERVER_CMD_TARGET, 16, 0);

    if (x == -1 && y == -1) {
        packet_append_uint8(packet, CMD_TARGET_CLEAR);
    } else {
        packet_append_uint8(packet, CMD_TARGET_MAPXY);
        packet_append_uint8(packet, x);
        packet_append_uint8(packet, y);
        packet_append_uint32(packet, count);
    }

    socket_send_packet(packet);
}

/**
 * Compare distances between two targets on the map.
 * @param a First target.
 * @param b Second target.
 * @return Comparison result.
 */
static int map_target_cmp(const void *a, const void *b)
{
    double x, y, x2, y2;
    unsigned long dist1, dist2;

    x = ((const map_target_struct *) a)->x - (map_width / 2.0f);
    y = ((const map_target_struct *) a)->y - (map_height / 2.0f);

    x2 = ((const map_target_struct *) b)->x - (map_width / 2.0f);
    y2 = ((const map_target_struct *) b)->y - (map_height / 2.0f);

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
 * @param is_friend 1 if targeting friendlies only.
 */
void map_target_handle(uint8_t is_friend)
{
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

            for (layer = 0; layer < NUM_REAL_LAYERS; layer++) {
                if (cell->faces[layer] && cell->target_object_count[layer] &&
                        cell->target_is_friend[layer] == is_friend) {
                    map_target_struct target;

                    target.count = cell->target_object_count[layer];
                    target.x = x;
                    target.y = y;
                    utarray_push_back(targets, &target);

                    if (cell->probe[layer] != 0) {
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
        p = (map_target_struct *) utarray_front(targets);

        if (p != NULL && p->count == curr_target) {
            cpl.target_object_index++;
        }
    }

    p = (map_target_struct *) utarray_eltptr(targets, cpl.target_object_index);

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
 * @param mx Mouse X.
 * @param my Mouse Y.
 * @param[out] tx Will contain tile X, unless function returns 0.
 * @param[out] ty Will contain tile Y, unless function returns 0.
 * @retval 1 Successfully transformed mouse coordinates into tile ones.
 * @retval 0 Failed to transform the coordinates; 'tx' and 'ty' are
 * left untouched.
 */
int mouse_to_tile_coords(int mx, int my, int *tx, int *ty)
{
    int x, y, xpos, ypos, player_height_offset, sub_layer;
    int16_t height, max_height;
    double zoom;
    struct MapCell *cell;
    uint32_t stretch;

    /* Adjust mouse x/y, making it look as if the map was drawn from
     * top left corner, in order to simplify comparisons below. */
    mx -= (MAP_START_XOFF * (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) /
            100.0)) + cur_widget[MAP_ID]->x;
    my -= (MAP_START_YOFF * (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) /
            100.0)) + cur_widget[MAP_ID]->y;

    player_height_offset = get_top_floor_height(MAP_CELL_GET_MIDDLE(
            map_width - (map_width / 2) - 1, map_height - (map_height / 2) - 1),
            MapData.player_sub_layer);
    zoom = (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0);

    /* Go through all the map squares. */
    for (x = map_width - 1; x >= 0; x--) {
        for (y = map_height - 1; y >= 0; y--) {
            cell = MAP_CELL_GET_MIDDLE(x, y);

            /* X/Y position of the map square. */
            xpos = (x * MAP_TILE_YOFF - y * MAP_TILE_YOFF) * zoom;
            ypos = (x * MAP_TILE_XOFF + y * MAP_TILE_XOFF) * zoom;

            if ((cell->fow || MapData.height_diff) && abs(get_top_floor_height(
                    cell, MapData.player_sub_layer) - player_height_offset) >
                    HEIGHT_MAX_RENDER) {
                continue;
            }

            if (cell->faces[0] != 0) {
                ypos = (ypos - (get_top_floor_height(cell, MapData.player_sub_layer)) * zoom) +
                        (player_height_offset) * zoom;
            }

            stretch = 0;
            max_height = 0;

            for (sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
                height = cell->height[sub_layer * NUM_LAYERS];

                if (height > max_height) {
                    max_height = height;
                    stretch = cell->stretch[sub_layer];
                }
            }

            /* See if this square matches our 48x24 box shape. */
            if (mx >= xpos && mx <= xpos + (MAP_TILE_POS_XOFF * zoom) &&
                    my >= ypos && my <= ypos + ((MAP_TILE_YOFF +
                    ((stretch >> 24) & 0xff)) * zoom)) {
                if (tilestretcher_coords_in_tile(stretch,
                        (int) ((mx - xpos) / zoom),
                        (int) ((my - ypos) / zoom))) {
                    if (tx) {
                        *tx = x;
                    }

                    if (ty) {
                        *ty = y;
                    }

                    return 1;
                }
            }
        }
    }

    return 0;
}

/**
 * Handle the "Walk Here" option in map widget menu.
 * @param widget Map widget.
 * @param menuitem Menu item.
 * @param event Event.
 */
static void menu_map_walk_here(widgetdata *widget, widgetdata *menuitem,
        SDL_Event *event)
{
    int tx, ty;

    if (mouse_to_tile_coords(cur_widget[MENU_ID]->x, cur_widget[MENU_ID]->y,
            &tx, &ty)) {
        send_move_path(tx, ty);
    }
}

/**
 * Handle the "Talk To NPC" option in map widget menu.
 * @param widget Map widget.
 * @param menuitem Menu item.
 * @param event Event.
 */
static void menu_map_talk_to(widgetdata *widget, widgetdata *menuitem,
        SDL_Event *event)
{
    int tx, ty;

    if (mouse_to_tile_coords(cur_widget[MENU_ID]->x, cur_widget[MENU_ID]->y,
            &tx, &ty)) {
        send_target(tx, ty, 0);
        keybind_process_command("?HELLO");
    }
}

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
    static int gfx_toggle = 0;
    SDL_Rect box;
    int mx, my;

    if (!widget->surface) {
        widget->surface = SDL_CreateRGBSurface(get_video_flags(), 850, 600,
                video_get_bpp(), 0, 0, 0, 0);
    }

    /* Make sure the map widget is always the last to handle events for. */
    SetPriorityWidget_reverse(widget);

    if (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) != 100) {
        int w, h;

        zoomSurfaceSize(widget->surface->w, widget->surface->h,
                setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0,
                setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0, &w, &h);
        widget->w = w;
        widget->h = h;
    }

    /* We re-create the map only when there is a change. */
    if (map_redraw_flag) {
        SDL_FillRect(widget->surface, NULL, 0);
        map_draw_map(widget->surface);
        map_redraw_flag = 0;
        effect_sprites_play();

        if (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) != 100) {
            if (zoomed) {
                SDL_FreeSurface(zoomed);
            }

            zoomed = zoomSurface(widget->surface,
                    setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0,
                    setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0,
                    setting_get_int(OPT_CAT_CLIENT, OPT_ZOOM_SMOOTH));
        }
    }

    box.x = widget->x;
    box.y = widget->y;

    if (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) == 100) {
        SDL_BlitSurface(widget->surface, NULL, ScreenSurface, &box);
    } else {
        SDL_BlitSurface(zoomed, NULL, ScreenSurface, &box);
    }

    /* The damage numbers */
    map_anims_play();

    /* Draw warning icons above player */
    if ((gfx_toggle++ & 63) < 25) {
        if (setting_get_int(OPT_CAT_MAP, OPT_HEALTH_WARNING) &&
                ((float) cpl.stats.hp / (float) cpl.stats.maxhp) * 100 <=
                setting_get_int(OPT_CAT_MAP, OPT_HEALTH_WARNING)) {
            surface_show(ScreenSurface, widget->x + 393 *
                    (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0),
                    widget->y + 298 *
                    (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0),
                    NULL, TEXTURE_CLIENT("warn_hp"));
        }
    } else {
        /* Low food */
        if (setting_get_int(OPT_CAT_MAP, OPT_FOOD_WARNING) &&
                ((float) cpl.stats.food / 1000.0f) * 100 <=
                setting_get_int(OPT_CAT_MAP, OPT_FOOD_WARNING)) {
            surface_show(ScreenSurface, widget->x + 390 *
                    (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0),
                    widget->y + 294 *
                    (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0),
                    NULL, TEXTURE_CLIENT("warn_food"));
        }
    }

    /* Process message animations */
    if (msg_anim.message[0] != '\0') {
        if ((LastTick - msg_anim.tick) < 3000) {
            int bmoff, y_offset;
            char *msg, *cp;

            bmoff = (int) ((50.0f / 3.0f) * ((float) (LastTick - msg_anim.tick)
                    / 1000.0f) * ((float) (LastTick - msg_anim.tick) /
                    1000.0f) + ((int) (150.0f * ((float) (LastTick -
                    msg_anim.tick) / 3000.0f))));
            y_offset = 0;
            msg = estrdup(msg_anim.message);

            cp = strtok(msg, "\n");

            while (cp) {
                text_show(ScreenSurface, FONT_SERIF16, cp, widget->x +
                        widget->surface->w / 2 - text_get_width(FONT_SERIF16,
                        cp, TEXT_OUTLINE) / 2, widget->y + 300 - bmoff +
                        y_offset, msg_anim.color, TEXT_OUTLINE | TEXT_MARKUP,
                        NULL);
                y_offset += FONT_HEIGHT(FONT_SERIF16);
                cp = strtok(NULL, "\n");
            }

            efree(msg);
            widget->redraw++;
        } else {
            msg_anim.message[0] = '\0';
        }
    }

    /* Holding the right mouse button for some time, create a menu. */
    if (SDL_GetMouseState(&mx, &my) == SDL_BUTTON(SDL_BUTTON_RIGHT) &&
            right_click_ticks != -1 &&
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
static int widget_event(widgetdata *widget, SDL_Event *event)
{
    int tx, ty;

    if (!EVENT_IS_MOUSE(event)) {
        return 0;
    }

    /* Check if the mouse is in play field. */
    if (!mouse_to_tile_coords(event->motion.x, event->motion.y, &tx, &ty)) {
        return 0;
    }

    if (event->type == SDL_MOUSEBUTTONUP) {
        /* Send target command if we released the right button in time;
         * otherwise the widget menu will be created. */
        if (event->button.button == SDL_BUTTON_RIGHT &&
                SDL_GetTicks() - right_click_ticks < 500) {
            send_target(tx, ty, 0);
        }

        right_click_ticks = -1;
        return 1;
    } else if (event->type == SDL_MOUSEBUTTONDOWN) {
        if (event->button.button == SDL_BUTTON_RIGHT) {
            right_click_ticks = SDL_GetTicks();
        } else if (SDL_GetMouseState(NULL, NULL) == SDL_BUTTON_LEFT) {
            /* Running */

            if (cpl.fire_on || cpl.run_on) {
                move_keys(dir_from_tile_coords(tx, ty));
            } else {
                send_move_path(tx, ty);
            }
        }

        return 1;
    } else if (event->type == SDL_MOUSEMOTION) {
        if (tx != old_map_mouse_x || ty != old_map_mouse_y) {
            map_redraw_flag = 1;
            old_map_mouse_x = tx;
            old_map_mouse_y = ty;

            return 1;
        }
    }

    return 0;
}

/** @copydoc widgetdata::background_func */
static void widget_background(widgetdata *widget, int draw)
{
    if (!widget->redraw) {
        region_map_ready(MapData.region_map);
    }
}

/** @copydoc widgetdata::deinit_func */
static void widget_deinit(widgetdata *widget)
{
    if (cells != NULL) {
        efree(cells);
        cells = NULL;
    }

    region_map_free(MapData.region_map);
    MapData.region_map = NULL;
}

/**
 * Initialize one map widget.
 */
void widget_map_init(widgetdata *widget)
{
    HARD_ASSERT(MapData.region_map == NULL);

    MapData.region_map = region_map_create();

    widget->draw_func = widget_draw;
    widget->event_func = widget_event;
    widget->background_func = widget_background;
    widget->deinit_func = widget_deinit;
    widget->menu_handle_func = NULL;

    SetPriorityWidget_reverse(widget);
}
