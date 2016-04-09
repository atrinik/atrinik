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
 * Region map API.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <region_map.h>
#include <toolkit_string.h>
#include <path.h>

#ifndef __CPROTO__

static UT_icd icd = {sizeof(region_map_fow_tile_t), NULL, NULL, NULL};

static region_map_def_t *region_map_def_new(void);
static void region_map_def_load(region_map_def_t *def, const char *str);
static void region_map_def_free(region_map_def_t *def);
static region_map_fow_t *region_map_fow_new(void);
static void region_map_fow_create(region_map_t *region_map);
static void region_map_fow_free(region_map_t *region_map);
static void region_map_fow_reset(region_map_t *region_map);

/**
 * Allocates and initializes a new region map structure.
 * @return
 * Region map.
 */
region_map_t *region_map_create(void)
{
    region_map_t *region_map;

    region_map = ecalloc(1, sizeof(*region_map));
    region_map->zoom = 100;
    region_map->def = region_map_def_new();
    region_map->fow = region_map_fow_new();

    return region_map;
}

/**
 * Clone a region map structure.
 * @param region_map
 *
 * @return
 *
 */
region_map_t *region_map_clone(region_map_t *region_map)
{
    region_map_t *clone;

    clone = ecalloc(1, sizeof(*clone));
    clone->zoom = 100;
    clone->surface = SDL_DisplayFormat(region_map->surface);
    clone->def = region_map->def;
    clone->def->refcount++;
    clone->fow = region_map->fow;
    clone->fow->refcount++;

    return clone;
}

/**
 * Frees a region map and all data associated with it.
 * @param region_map
 * Region map.
 */
void region_map_free(region_map_t *region_map)
{
    HARD_ASSERT(region_map != NULL);
    HARD_ASSERT(region_map->def != NULL);
    HARD_ASSERT(region_map->fow != NULL);

    region_map_reset(region_map);
    efree(region_map->def);
    efree(region_map->fow);
    efree(region_map);
}

/**
 * Resets data associated with the region map.
 * @param region_map
 * Region map.
 */
void region_map_reset(region_map_t *region_map)
{
    HARD_ASSERT(region_map != NULL);
    HARD_ASSERT(region_map->def != NULL);
    HARD_ASSERT(region_map->fow != NULL);

    memset(&region_map->pos, 0, sizeof(region_map->pos));

    if (region_map->data_png != NULL) {
        curl_data_free(region_map->data_png);
        region_map->data_png = NULL;
    }

    if (region_map->data_def != NULL) {
        curl_data_free(region_map->data_def);
        region_map->data_def = NULL;
    }

    /* The fog of war freeing makes use of region_map->surface, so it must
     * be freed before the surface... */
    if (--region_map->fow->refcount == 0) {
        region_map_fow_free(region_map);
        efree(region_map->fow);
    }

    region_map->fow = region_map_fow_new();

    if (region_map->surface != NULL) {
        SDL_FreeSurface(region_map->surface);
        region_map->surface = NULL;
    }

    if (region_map->zoomed != NULL) {
        SDL_FreeSurface(region_map->zoomed);
        region_map->zoomed = NULL;
    }

    if (--region_map->def->refcount == 0) {
        region_map_def_free(region_map->def);
        efree(region_map->def);
    }

    region_map->def = region_map_def_new();
}

/*
 * Updates the specified region map, resetting it and scheduling
 * a re-download of the image and definition files.
 * @param region_map
 * Region map.
 * @param region_name
 * Region name.
 */
void region_map_update(region_map_t *region_map, const char *region_name)
{
    char url[HUGE_BUF], buf[HUGE_BUF], *path;

    region_map_reset(region_map);

    /* Download the image. */
    snprintf(VS(url), "%s/client-maps/%s.png", cpl.http_url, region_name);
    snprintf(VS(buf), "client-maps/%s.png", region_name);
    path = file_path_server(buf);
    region_map->data_png = curl_download_start(url, path);
    efree(path);

    /* Download the definitions. */
    snprintf(VS(url), "%s/client-maps/%s.def", cpl.http_url, region_name);
    snprintf(VS(buf), "client-maps/%s.def", region_name);
    path = file_path_server(buf);
    region_map->data_def = curl_download_start(url, path);
    efree(path);

    snprintf(VS(buf), "client-maps/%s.tiles", region_name);
    region_map->fow->path = file_path_player(buf);
}

/**
 * Checks if the specified region map is ready to be rendered.
 * @param region_map
 * Region map.
 * @return
 * Whether the region map is ready to be rendered.
 */
bool region_map_ready(region_map_t *region_map)
{
    SDL_Surface *img;
    size_t i;

    HARD_ASSERT(region_map != NULL);

    if (region_map->surface != NULL) {
        return true;
    }

    if (region_map->data_png == NULL || region_map->data_def == NULL) {
        return false;
    }

    SOFT_ASSERT_RC(region_map->zoomed == NULL, false,
            "Region map already has a zoomed surface.");

    if (curl_download_get_state(region_map->data_png) != CURL_STATE_OK) {
        return false;
    }

    if (curl_download_get_state(region_map->data_def) != CURL_STATE_OK) {
        return false;
    }

    img = IMG_Load_RW(SDL_RWFromMem(region_map->data_png->memory,
            region_map->data_png->size), 1);
    region_map->surface = SDL_DisplayFormat(img);
    SDL_FreeSurface(img);

    region_map_pan(region_map);
    region_map_def_load(region_map->def, region_map->data_def->memory);

    /* Draw the labels. */
    for (i = 0; i < region_map->def->num_labels; i++) {
        text_show(region_map->surface, FONT_SERIF20,
                region_map->def->labels[i].text,
                region_map->def->labels[i].x, region_map->def->labels[i].y,
                COLOR_HGOLD, TEXT_MARKUP | TEXT_OUTLINE, NULL);
    }

    for (i = 0; i < region_map->def->num_tooltips; i++) {
        if (!region_map->def->tooltips[i].outline) {
            continue;
        }

        border_create(region_map->surface,
                region_map->def->tooltips[i].x, region_map->def->tooltips[i].y,
                region_map->def->tooltips[i].w, region_map->def->tooltips[i].h,
                SDL_MapRGB(region_map->surface->format,
                region_map->def->tooltips[i].outline_color.r,
                region_map->def->tooltips[i].outline_color.g,
                region_map->def->tooltips[i].outline_color.b),
                region_map->def->tooltips[i].outline_size);
    }

    if (region_map->fow->surface == NULL ||
            region_map->fow->surface->w != region_map->surface->w ||
            region_map->fow->surface->h != region_map->surface->h) {
        region_map_fow_reset(region_map);
        region_map_fow_create(region_map);
    }

    curl_data_free(region_map->data_png);
    region_map->data_png = NULL;

    curl_data_free(region_map->data_def);
    region_map->data_def = NULL;

    minimap_redraw_flag = 1;

    return true;
}

/**
 * Find a map identified by its path in the region map's definitions.
 * @param region_map
 * Region map to search in.
 * @param map_path
 * Map path to find.
 * @return
 * Pointer to the map if found, NULL otherwise.
 */
region_map_def_map_t *region_map_find_map(region_map_t *region_map,
        const char *map_path)
{
    size_t i;

    HARD_ASSERT(region_map != NULL);
    HARD_ASSERT(map_path != NULL);

    for (i = 0; i < region_map->def->num_maps; i++) {
        if (strcmp(region_map->def->maps[i].path, map_path) == 0) {
            return &region_map->def->maps[i];
        }
    }

    return NULL;
}

/**
 * Get the region map image surface.
 * @param region_map
 * Region map.
 * @return
 * Image surface, never NULL.
 */
SDL_Surface *region_map_surface(region_map_t *region_map)
{
    HARD_ASSERT(region_map != NULL);
    HARD_ASSERT(region_map->surface != NULL);

    if (region_map->zoomed != NULL) {
        return region_map->zoomed;
    }

    return region_map->surface;
}

void region_map_pan(region_map_t *region_map)
{
    region_map_def_map_t *map;

    HARD_ASSERT(region_map != NULL);
    HARD_ASSERT(region_map->surface != NULL);

    map = region_map_find_map(region_map, MapData.map_path);

    if (map != NULL) {
        region_map->pos.x = (map->xpos + MapData.posx *
                region_map->def->pixel_size) * (region_map->zoom / 100.0) -
                region_map->pos.w / 2.0;
        region_map->pos.y = (map->ypos + MapData.posy *
                region_map->def->pixel_size) * (region_map->zoom / 100.0) -
                region_map->pos.h / 2.0;
    } else {
        region_map->pos.x = region_map->surface->w / 2 - region_map->pos.w / 2;
        region_map->pos.y = region_map->surface->h / 2 - region_map->pos.h / 2;
    }

    surface_pan(region_map_surface(region_map), &region_map->pos);
}

/**
 * Resize the region map.
 * @param region_map
 * Region map to resize.
 * @param adjust
 * How much to zoom by.
 */
void region_map_resize(region_map_t *region_map, int adjust)
{
    float delta;

    HARD_ASSERT(region_map != NULL);
    HARD_ASSERT(region_map->fow != NULL);
    SOFT_ASSERT(region_map->surface != NULL, "Region map's surface is NULL.");

    region_map->zoom += adjust;

    if (region_map->zoomed != NULL) {
        SDL_FreeSurface(region_map->zoomed);
        region_map->zoomed = NULL;
    }

    if (region_map->fow_zoomed != NULL) {
        SDL_FreeSurface(region_map->fow_zoomed);
        region_map->fow_zoomed = NULL;
    }

    if (region_map->zoom != 100) {
        /* Zoom the surface. */
        region_map->zoomed = zoomSurface(region_map->surface,
                region_map->zoom / 100.0, region_map->zoom / 100.0, 0);
        region_map->fow_zoomed = zoomSurface(region_map->fow->surface,
                region_map->zoom / 100.0, region_map->zoom / 100.0, 0);
        SDL_SetColorKey(region_map->fow_zoomed, SDL_SRCCOLORKEY,
                SDL_MapRGB(region_map->fow_zoomed->format, 255, 255, 255));
    }

    if (adjust > 0) {
        delta = (region_map->zoom / 100.0 - 0.1f);
    } else {
        delta = (region_map->zoom / 100.0 + 0.1f);
    }

    region_map->pos.x += region_map->pos.x / delta * (adjust / 100.0) +
            region_map->pos.w / delta * (adjust / 100.0) / 2;
    region_map->pos.y += region_map->pos.y / delta * (adjust / 100.0) +
            region_map->pos.h / delta * (adjust / 100.0) / 2;

    surface_pan(region_map_surface(region_map), &region_map->pos);
}

/**
 * Render a region map marker, showing the player's position on the region map.
 * @param region_map
 * Region map.
 * @param surface
 * Where to render.
 * @param x
 * X coordinate.
 * @param y
 * Y coordinate.
 */
void region_map_render_marker(region_map_t *region_map, SDL_Surface *surface,
        int x, int y)
{
    SDL_Surface *marker;
    SDL_Rect box, srcbox;
    region_map_def_map_t *map;

    map = region_map_find_map(region_map, MapData.map_path);

    if (map == NULL) {
        return;
    }

    /* TODO: Could cache this */
    marker = rotozoomSurface(TEXTURE_CLIENT("map_marker"),
            -((map_get_player_direction() - 1) * 45),
            region_map->zoom / 100.0, 1);
    /* Calculate the player's marker position. */
    box.x = x + (map->xpos + MapData.posx * region_map->def->pixel_size) *
            (region_map->zoom / 100.0) - marker->w / 2.0 +
            region_map->def->pixel_size / 2.0 - region_map->pos.x;
    box.y = y + (map->ypos + MapData.posy * region_map->def->pixel_size) *
            (region_map->zoom / 100.0) - marker->h / 2.0 +
            region_map->def->pixel_size / 2.0 - region_map->pos.y;

    srcbox.x = MAX(0, x - box.x);
    srcbox.y = MAX(0, y - box.y);
    srcbox.w = MAX(0, region_map->pos.w - (box.x - x));
    srcbox.h = MAX(0, region_map->pos.h - (box.y - y));

    box.x += srcbox.x;
    box.y += srcbox.y;

    SDL_BlitSurface(marker, &srcbox, surface, &box);
    SDL_FreeSurface(marker);
}

void region_map_render_fow(region_map_t *region_map, SDL_Surface *surface,
        int x, int y)
{
    SDL_Rect box;

    if (region_map->fow->surface == NULL) {
        return;
    }

    box.x = x;
    box.y = y;
    SDL_BlitSurface(region_map_fow_surface(region_map), &region_map->pos,
            surface, &box);
}

/**
 * Allocates a new ::region_map_def_t structure.
 * @return
 * The allocated structure.
 */
static region_map_def_t *region_map_def_new(void)
{
    region_map_def_t *def;

    def = ecalloc(1, sizeof(*def));
    def->refcount = 1;

    return def;
}

/**
 * Loads region map definitions from a string.
 * @param def
 * Definitions structure to load into.
 * @param str
 * String to load from.
 */
static void region_map_def_load(region_map_def_t *def, const char *str)
{
    char line[HUGE_BUF], *cps[10], region[MAX_BUF];
    size_t pos, pos2;
    region_map_def_map_t *def_map;

    def->pixel_size = 1;
    def->map_size_x = 24;
    def->map_size_y = 24;

    pos = 0;

    while (string_get_word(str, &pos, '\n', VS(line), 0)) {
        if (strcmp(line, "t_outline") == 0) {
            if (def->num_tooltips == 0) {
                LOG(ERROR, "No tooltips defined.");
                continue;
            }

            def->tooltips[def->num_tooltips - 1].outline = 1;
            def->tooltips[def->num_tooltips - 1].outline_size = 1;
            def->tooltips[def->num_tooltips - 1].outline_color.r = 255;
            def->tooltips[def->num_tooltips - 1].outline_color.g = 0;
            def->tooltips[def->num_tooltips - 1].outline_color.b = 0;
            continue;
        }

        if (string_split(line, cps, 2, ' ') != 2) {
            LOG(ERROR, "Invalid line in definitions file: %s", line);
            continue;
        }

        if (strcmp(cps[0], "pixel_size") == 0) {
            def->pixel_size = atoi(cps[1]);
        } else if (strcmp(cps[0], "map_size_x") == 0) {
            def->map_size_x = atoi(cps[1]);
        } else if (strcmp(cps[0], "map_size_y") == 0) {
            def->map_size_y = atoi(cps[1]);
        } else if (strcmp(cps[0], "map") == 0) {
            if (string_split(cps[1], cps, 4, ' ') < 3) {
                LOG(ERROR, "Invalid map in definitions file: %s", cps[1]);
                continue;
            }

            def->maps = erealloc(def->maps, sizeof(*def->maps) *
                    (def->num_maps + 1));
            def_map = &def->maps[def->num_maps];
            def->num_maps++;

            def_map->xpos = strtoul(cps[0], NULL, 16);
            def_map->ypos = strtoul(cps[1], NULL, 16);
            def_map->path = estrdup(cps[2]);
            def_map->regions = NULL;
            def_map->regions_num = 0;

            if (cps[3] != NULL) {
                pos2 = 0;

                while (string_get_word(cps[3], &pos2, ',', VS(region), 0)) {
                    def_map->regions = erealloc(def_map->regions,
                            sizeof(*def_map->regions) *
                            (def_map->regions_num + 1));
                    def_map->regions[def_map->regions_num] = estrdup(region);
                    def_map->regions_num++;
                }
            }
        } else if (strcmp(cps[0], "label") == 0) {
            if (string_split(cps[1], cps, 4, ' ') != 4) {
                LOG(ERROR, "Invalid label in definitions file: %s",
                        cps[1]);
                continue;
            }

            def->labels = erealloc(def->labels, sizeof(*def->labels) *
                    (def->num_labels + 1));
            def->labels[def->num_labels].x = strtoul(cps[0], NULL, 16);
            def->labels[def->num_labels].y = strtoul(cps[1], NULL, 16);
            def->labels[def->num_labels].name = estrdup(cps[2]);
            def->labels[def->num_labels].text = estrdup(cps[3]);
            string_newline_to_literal(def->labels[def->num_labels].text);
            def->num_labels++;
        } else if (strcmp(cps[0], "label_hide") == 0) {

        } else if (strcmp(cps[0], "tooltip") == 0) {
            if (string_split(cps[1], cps, 6, ' ') != 6) {
                LOG(ERROR, "Invalid tooltip in definitions file: %s",
                        cps[1]);
                continue;
            }

            def->tooltips = erealloc(def->tooltips, sizeof(*def->tooltips) *
                    (def->num_tooltips + 1));
            def->tooltips[def->num_tooltips].outline = 0;
            def->tooltips[def->num_tooltips].x = strtoul(cps[0], NULL, 16);
            def->tooltips[def->num_tooltips].y = strtoul(cps[1], NULL, 16);
            def->tooltips[def->num_tooltips].w = strtoul(cps[2], NULL, 16);
            def->tooltips[def->num_tooltips].h = strtoul(cps[3], NULL, 16);
            def->tooltips[def->num_tooltips].name = estrdup(cps[4]);
            def->tooltips[def->num_tooltips].text = estrdup(cps[5]);
            string_newline_to_literal(def->tooltips[def->num_tooltips].text);
            def->num_tooltips++;
        } else if (strcmp(cps[0], "t_outline") == 0) {
            if (string_split(cps[1], cps, 2, ' ') != 2) {
                LOG(ERROR, "Invalid t_outline in definitions file: %s",
                        cps[1]);
                continue;
            }

            if (def->num_tooltips == 0) {
                LOG(ERROR, "No tooltips defined.");
                continue;
            }

            if (!text_color_parse(cps[0],
                    &def->tooltips[def->num_tooltips - 1].outline_color)) {
                LOG(ERROR, "Color in invalid format: %s", cps[0]);
                continue;
            }

            def->tooltips[def->num_tooltips - 1].outline = 1;
            def->tooltips[def->num_tooltips - 1].outline_size = atoi(cps[1]);
        }
    }
}

/**
 * Frees data inside a region_map_def_t structure.
 *
 * Note that the structure itself is not freed.
 * @param def
 * Region map definitions structure.
 */
static void region_map_def_free(region_map_def_t *def)
{
    size_t i;

    HARD_ASSERT(def != NULL);

    /* Free all maps. */
    for (i = 0; i < def->num_maps; i++) {
        efree(def->maps[i].path);

        if (def->maps[i].regions != NULL) {
            for (size_t j = 0; j < def->maps[i].regions_num; j++) {
                efree(def->maps[i].regions[j]);
            }

            efree(def->maps[i].regions);
        }
    }

    if (def->maps != NULL) {
        efree(def->maps);
        def->maps = NULL;
        def->num_maps = 0;
    }

    /* Free labels. */
    for (i = 0; i < def->num_labels; i++) {
        efree(def->labels[i].name);
        efree(def->labels[i].text);
    }

    if (def->labels != NULL) {
        efree(def->labels);
        def->labels = NULL;
        def->num_labels = 0;
    }

    /* Free tooltips. */
    for (i = 0; i < def->num_tooltips; i++) {
        efree(def->tooltips[i].name);
        efree(def->tooltips[i].text);
    }

    if (def->tooltips != NULL) {
        efree(def->tooltips);
        def->tooltips = NULL;
        def->num_tooltips = 0;
    }
}

/**
 * Allocates a new ::region_map_fow_t structure.
 * @return
 * The allocated structure.
 */
static region_map_fow_t *region_map_fow_new(void)
{
    region_map_fow_t *fow;

    fow = ecalloc(1, sizeof(*fow));
    fow->refcount = 1;
    utarray_new(fow->tiles, &icd);

    return fow;
}

static void region_map_fow_create(region_map_t *region_map)
{
    FILE *fp;

    HARD_ASSERT(region_map->fow != NULL);
    HARD_ASSERT(region_map->fow->path != NULL);
    HARD_ASSERT(region_map->fow->bitmap == NULL);

    fp = path_fopen(region_map->fow->path, "r");

    if (fp != NULL) {
        struct stat statbuf;

        if (fstat(fileno(fp), &statbuf) == -1) {
            LOG(ERROR, "Could not stat %s: %d (%s)", region_map->fow->path,
                    errno, strerror(errno));
        } else if ((size_t) statbuf.st_size ==
                RM_MAP_FOW_BITMAP_SIZE(region_map)) {
            region_map->fow->bitmap = emalloc(statbuf.st_size);

            if (fread(region_map->fow->bitmap, 1, statbuf.st_size, fp) !=
                    (size_t) statbuf.st_size) {
                LOG(ERROR, "Could not read %"PRIu64" bytes from %s: %d "
                        "(%s)", (uint64_t) statbuf.st_size, region_map->fow->path,
                        errno, strerror(errno));
                efree(region_map->fow->bitmap);
                region_map->fow->bitmap = NULL;
            }
        }

        fclose(fp);
    }

    if (region_map->fow->bitmap == NULL) {
        region_map->fow->bitmap = ecalloc(1,
                RM_MAP_FOW_BITMAP_SIZE(region_map));
    }

    region_map_fow_update(region_map);
}

static void region_map_fow_free(region_map_t *region_map)
{
    unsigned i;
    region_map_fow_tile_t *tile;

    HARD_ASSERT(region_map != NULL);
    HARD_ASSERT(region_map->fow != NULL);

    region_map_fow_reset(region_map);

    if (region_map->fow->tiles != NULL) {
        for (i = 0; i < utarray_len(region_map->fow->tiles); i++) {
            tile = (region_map_fow_tile_t *) utarray_eltptr(
                    region_map->fow->tiles, i);
            efree(tile->path);
        }

        utarray_free(region_map->fow->tiles);
        region_map->fow->tiles = NULL;
    }

    if (region_map->fow->path != NULL) {
        efree(region_map->fow->path);
        region_map->fow->path = NULL;
    }
}

static void region_map_fow_reset(region_map_t *region_map)
{
    HARD_ASSERT(region_map != NULL);
    HARD_ASSERT(region_map->fow != NULL);

    if (region_map->fow->surface != NULL) {
        SDL_FreeSurface(region_map->fow->surface);
        region_map->fow->surface = NULL;
    }

    if (region_map->fow_zoomed != NULL) {
        SDL_FreeSurface(region_map->fow_zoomed);
        region_map->fow_zoomed = NULL;
    }

    if (region_map->fow->bitmap != NULL) {
        FILE *fp;

        HARD_ASSERT(region_map->surface != NULL);
        HARD_ASSERT(region_map->fow->path != NULL);

        fp = path_fopen(region_map->fow->path, "w");

        if (fp != NULL) {
            fwrite(region_map->fow->bitmap, 1,
                    RM_MAP_FOW_BITMAP_SIZE(region_map), fp);
            fclose(fp);
        }

        efree(region_map->fow->bitmap);
        region_map->fow->bitmap = NULL;
    }
}

static bool region_map_fow_update_regions(region_map_t *region_map,
        const uint32_t *color)
{
    bool ret = false;
    region_map_def_map_t *def_map, *def_map_regions;
    UT_array *regions;

    utarray_new(regions, &ut_str_icd);

    for (object *op = cpl.ob->inv; op != NULL; op = op->next) {
        if (op->itype != TYPE_REGION_MAP) {
            continue;
        }

        char *cp = op->s_name;
        utarray_push_back(regions, &cp);
    }

    utarray_sort(regions, ut_str_sort);

    def_map_regions = NULL;

    for (size_t i = 0; i < region_map->def->num_maps; i++) {
        def_map = &region_map->def->maps[i];

        if (def_map->regions != NULL) {
            def_map_regions = def_map;
        }

        if (def_map_regions == NULL) {
            continue;
        }

        for (size_t j = 0; j < def_map_regions->regions_num; j++) {
            if (utarray_find(regions, &def_map_regions->regions[j],
                    ut_str_sort) == NULL) {
                continue;
            }

            if (color != NULL) {
                SDL_Rect box;

                box.x = def_map->xpos;
                box.y = def_map->ypos;
                box.w = region_map->def->map_size_x *
                        region_map->def->pixel_size;
                box.h = region_map->def->map_size_y *
                        region_map->def->pixel_size;
                SDL_FillRect(region_map->fow->surface, &box, *color);
            }

            ret = true;
        }
    }

    utarray_free(regions);

    return ret;
}

void region_map_fow_update(region_map_t *region_map)
{
    region_map_def_map_t *def_map;
    int rowsize, x, y;
    uint32_t color;
    SDL_Surface *surface;

    HARD_ASSERT(region_map != NULL);
    HARD_ASSERT(region_map->fow != NULL);
    HARD_ASSERT(region_map->def != NULL);

    if (region_map->surface == NULL) {
        /* Not yet loaded, nothing to do. */
        return;
    }

    if (region_map->fow->tiles != NULL) {
        region_map_fow_tile_t *tile;

        /* Now that the definitions are loaded, we can go back through the tiles
         * data and actually set the visited tiles. */
        for (size_t i = 0; i < utarray_len(region_map->fow->tiles); i++) {
            tile = (region_map_fow_tile_t *) utarray_eltptr(
                    region_map->fow->tiles, i);
            def_map = region_map_find_map(region_map, tile->path);
            efree(tile->path);

            if (def_map != NULL) {
                region_map_fow_set_visited(region_map, def_map, NULL,
                        tile->x, tile->y);
            }
        }

        utarray_free(region_map->fow->tiles);
        region_map->fow->tiles = NULL;
    }

    if (region_map->fow->surface == NULL) {
        region_map->fow->surface = SDL_CreateRGBSurface(get_video_flags(),
                region_map->surface->w, region_map->surface->h, video_get_bpp(),
                0, 0, 0, 0);
    }

    SDL_FillRect(region_map->fow->surface, NULL, 0);
    rowsize = (region_map->surface->w / region_map->def->pixel_size + 31) / 32;
    color = SDL_MapRGB(region_map->fow->surface->format, 255, 255, 255);

    for (y = 0; y < region_map->surface->h / region_map->def->pixel_size; y++) {
        for (x = 0; x < region_map->surface->w / region_map->def->pixel_size;
                x++) {
            SDL_Rect box;

            if (x % 32 == 0 && (region_map->fow->bitmap[(x / 32) + rowsize *
                    y] == 0xffffffff)) {
                /* If this entire 32 tiles area is visible, then we just
                 * draw one wide rectangle. */
                box.x = x * region_map->def->pixel_size;
                box.w = region_map->def->pixel_size * 32;
                x += 32 - 1;
            } else if (region_map->fow->bitmap[(x / 32) + rowsize * y] &
                    (1U << (x % 32))) {
                /* The tile is visible */
                box.x = x * region_map->def->pixel_size;
                box.w = region_map->def->pixel_size;
            } else {
                continue;
            }

            box.y = y * region_map->def->pixel_size;
            box.h = region_map->def->pixel_size;
            SDL_FillRect(region_map->fow->surface, &box, color);
        }
    }

    region_map_fow_update_regions(region_map, &color);

    SDL_SetColorKey(region_map->fow->surface, SDL_SRCCOLORKEY, color);
    surface = SDL_DisplayFormat(region_map->fow->surface);
    SDL_FreeSurface(region_map->fow->surface);
    region_map->fow->surface = surface;
}

bool region_map_fow_set_visited(region_map_t *region_map,
        region_map_def_map_t *map, const char *map_path, int x, int y)
{
    int rowsize;
    ssize_t idx;

    HARD_ASSERT(region_map != NULL);
    HARD_ASSERT(region_map->fow != NULL);
    HARD_ASSERT(map != NULL || map_path != NULL);

    if (map == NULL) {
        region_map_fow_tile_t tile;

        if (region_map->fow->tiles == NULL) {
            return false;
        }

        tile.path = estrdup(map_path);
        tile.x = x;
        tile.y = y;
        utarray_push_back(region_map->fow->tiles, &tile);
        return false;
    }

    HARD_ASSERT(region_map->fow->bitmap != NULL);

    x += map->xpos / region_map->def->pixel_size;
    y += map->ypos / region_map->def->pixel_size;

    if (x < 0 || x >= region_map->surface->w / region_map->def->pixel_size ||
            y < 0 || y >= region_map->surface->h /
            region_map->def->pixel_size) {
        return false;
    }

    if (region_map_fow_is_visited(region_map, x, y)) {
        return false;
    }

    rowsize = (region_map->surface->w / region_map->def->pixel_size + 31) / 32;
    idx = (x / 32) + rowsize * y;

    SOFT_ASSERT_RC(idx >= 0 && (size_t) idx < RM_MAP_FOW_BITMAP_SIZE(
            region_map) / sizeof(*region_map->fow->bitmap), false,
            "Attempt to write at an invalid position: %"PRIu64" max: %"PRIu64,
            (uint64_t) idx, (uint64_t) RM_MAP_FOW_BITMAP_SIZE(region_map) /
            sizeof(*region_map->fow->bitmap));

    region_map->fow->bitmap[idx] |= (1U << (x % 32));
    return true;
}

bool region_map_fow_is_visited(region_map_t *region_map, int x, int y)
{
    int rowsize;
    ssize_t idx;

    HARD_ASSERT(region_map != NULL);
    HARD_ASSERT(region_map->fow != NULL);

    rowsize = (region_map->surface->w / region_map->def->pixel_size + 31) / 32;
    idx = (x / 32) + rowsize * y;

    SOFT_ASSERT_RC(idx >= 0 && (size_t) idx < RM_MAP_FOW_BITMAP_SIZE(
            region_map) / sizeof(*region_map->fow->bitmap), false,
            "Attempt to read at an invalid position: %"PRIu64" max: %"PRIu64,
            (uint64_t) idx, (uint64_t) RM_MAP_FOW_BITMAP_SIZE(region_map) /
            sizeof(*region_map->fow->bitmap));

    return region_map->fow->bitmap[idx] & (1U << (x % 32));
}

bool region_map_fow_is_visible(region_map_t *region_map, int x, int y)
{
    if (region_map_fow_is_visited(region_map, x, y)) {
        return true;
    }

    if (region_map_fow_update_regions(region_map, NULL)) {
        return true;
    }

    return false;
}

/**
 * Get the region map fow image surface.
 * @param region_map
 * Region map.
 * @return
 * Image surface, never NULL.
 */
SDL_Surface *region_map_fow_surface(region_map_t *region_map)
{
    HARD_ASSERT(region_map->fow != NULL);
    HARD_ASSERT(region_map->fow != NULL);
    HARD_ASSERT(region_map->fow->surface != NULL);

    if (region_map->fow_zoomed != NULL) {
        return region_map->fow_zoomed;
    }

    return region_map->fow->surface;
}

#endif
