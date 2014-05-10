/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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
 * Handles the world maker code. */

#include <global.h>
#include <loader.h>
#include <gd.h>

/**
 * Array of colors used by the different faces.
 *
 * Each entry is an array consisting of:
 *
 * - average color value of the face
 * - darker shade of the average color
 * - red average
 * - green average
 * - blue average
 */
static int **wm_face_colors;

/** Pixels per tile (3 = 3x3 box). */
#define MAX_PIXELS 3

/** Region. */
typedef struct wm_region
{
    /** Maps in the region. */
    struct
    {
        /** The map. */
        mapstruct *m;

        /** X position. */
        int xpos;

        /** Y position. */
        int ypos;
    } *maps;

    /** Number of maps. */
    size_t num_maps;

    /** X position. */
    int xpos;

    /** Y position. */
    int ypos;

    /** Lowest X position. */
    int xpos_lowest;

    /** Lowest Y position. */
    int ypos_lowest;

    /** Width. */
    int w;

    /** Height. */
    int h;
} wm_region;

/**
 * Initialize the face colors.
 */
static void wm_images_init(void)
{
    int i, x, y;
    gdImagePtr im, im2;
    uint8 *data;
    uint16 len;

    wm_face_colors = emalloc(sizeof(*wm_face_colors) * nrofpixmaps);

    for (i = 0; i < nrofpixmaps; i++) {
        uint64 total = 0, r = 0, g = 0, b = 0;

        /* Get the face's data. */
        face_get_data(i, &data, &len);

        /* Create the image from the data. */
        im = gdImageCreateFromPngPtr(len, data);
        gdImageAlphaBlending(im, 1);
        gdImageSaveAlpha(im, 1);

        im2 = gdImageCreateTrueColor(im->sx, im->sy);
        gdImageCopyResized(im2, im, 0, 0, 0, 0, im2->sx, im2->sy, im->sx,
                           im->sy);

        wm_face_colors[i] = ecalloc(1, sizeof(**wm_face_colors) * 5);

        /* Get the average pixel colors. */
        for (x = 0; x < im2->sx; x++) {
            for (y = 0; y < im2->sy; y++) {
                int color = gdImageGetPixel(im2, x, y);

                if (color) {
                    r += gdImageRed(im2, color);
                    g += gdImageGreen(im2, color);
                    b += gdImageBlue(im2, color);
                    total++;
                }
            }
        }

        /* Store the total. */
        if (total) {
            r /= total;
            g /= total;
            b /= total;
            wm_face_colors[i][0] = gdImageColorResolve(im2, r, g, b);
            wm_face_colors[i][1] = gdImageColorResolve(im2, MIN(r + 10, 255),
                                                            MIN(g + 10, 255),
                                                            MIN(b + 10, 255));
            wm_face_colors[i][2] = r;
            wm_face_colors[i][3] = g;
            wm_face_colors[i][4] = b;
        }

        gdImageDestroy(im);
        gdImageDestroy(im2);
    }
}

/**
 * Render a single object on the image 'im'.
 * @param im Image to render on.
 * @param x X position.
 * @param y Y position.
 * @param ob Object to render.
 * @return 1 if we rendered the object, 0 otherwise.
 */
static int render_object(gdImagePtr im, int x, int y, object *ob)
{
    /* Sanity check. */
    if (!ob) {
        return 0;
    }

    /* No need to do this for tail parts. */
    if (ob->head) {
        return 0;
    }

    /* Only render the following objects:
     * - floor (the terrain, obviously)
     * - walls
     * - doors
     * - exits (stairs, but also useful to see portals)
     * - shop mats
     * - holy altars
     * - signs
     * - misc objects blocking passage (rocks, trees, etc)
     */
    if (ob->layer == LAYER_FLOOR ||
        ob->type == WALL ||
        ob->type == DOOR ||
        ob->type == EXIT ||
        ob->type == SHOP_MAT ||
        ob->type == HOLY_ALTAR ||
        ob->type == SIGN ||
        (ob->type == MISC_OBJECT && QUERY_FLAG(ob, FLAG_NO_PASS))) {
        int px, py, j = 0, max, color;

        max = MAX_PIXELS;

        /* Adjust the maximum position if this is a multi-arch object. */
        if (ob->quick_pos) {
            max *= ((ob->quick_pos >> 4) + 1) / 2;
        }

        /* Draw the pixels. */
        for (px = 0; px < max; px++) {
            for (py = 0; py < max; py++) {
                /* Do shading based on the floor's z. */
                if (ob->type == FLOOR && ob->z) {
                    color = gdImageColorResolve(
                        im,
                        MAX(0, wm_face_colors[ob->face->number][2] - ob->z /
                            2.5),
                        MAX(0, wm_face_colors[ob->face->number][3] - ob->z /
                            2.5),
                        MAX(0, wm_face_colors[ob->face->number][4] - ob->z /
                            2.5)
                    );
                }
                else {
                    color = wm_face_colors[ob->face->number][0];
                }

                gdImageSetPixel(im, x + px, y + py, color);
                j++;
            }
        }

        return 1;
    }

    return 0;
}

/**
 * Add map to region.
 * @param r Region.
 * @param m Map to add.
 */
static void region_add_map(wm_region *r, mapstruct *m)
{
    /* Resize the array. */
    r->maps = erealloc(r->maps, sizeof(*r->maps) * (r->num_maps + 1));
    r->maps[r->num_maps].m = m;
    r->maps[r->num_maps].xpos = r->xpos;
    r->maps[r->num_maps].ypos = r->ypos;
    r->num_maps++;
}

/**
 * Check if the specified map is in the specified region. Parent regions
 * are checked for match.
 * @param m Map.
 * @param name Region to check.
 * @return 1 if it is in the region, 0 otherwise.
 */
static int map_in_region(mapstruct *m, const char *name)
{
    region_struct *r;

    for (r = m->region; r; r = r->parent) {
        if (!strcmp(r->name, name)) {
            return 1;
        }
    }

    return 0;
}

/**
 * Recursively add maps to a region.
 * @param r Region.
 * @param m Start map.
 * @param region_name Region name.
 */
static void region_add_rec(wm_region *r, mapstruct *m, const char *region_name)
{
    int i;

    region_add_map(r, m);

    /* Going through 4 tile paths should be enough. */
    for (i = 0; i < 4; i++) {
        /* No such tile path. */
        if (!m->tile_path[i]) {
            continue;
        }

        /* Load the map if needed. */
        if (!m->tile_map[i]) {
            m->tile_map[i] = ready_map_name(m->tile_path[i], MAP_NAME_SHARED);

            if (!m->tile_map[i]) {
                continue;
            }
        }

        /* If there is a map and it wasn't traversed yet... */
        if (m->tile_map[i] && !m->tile_map[i]->traversed) {
            m->tile_map[i]->traversed = 1;

            /* Is the map in region? */
            if (map_in_region(m->tile_map[i], region_name)) {
                switch (i) {
                    case 0:
                        r->ypos -= MAP_HEIGHT(m->tile_map[i]) * MAX_PIXELS;
                        break;

                    case 1:
                        r->xpos += MAP_WIDTH(m->tile_map[i]) * MAX_PIXELS;
                        break;

                    case 2:
                        r->ypos += MAP_HEIGHT(m->tile_map[i]) * MAX_PIXELS;
                        break;

                    case 3:
                        r->xpos -= MAP_WIDTH(m->tile_map[i]) * MAX_PIXELS;
                        break;
                }

                /* Store the lowest x/y positions to do adjustments later. */
                if (r->xpos < r->xpos_lowest) {
                    r->xpos_lowest = r->xpos;
                }

                if (r->ypos < r->ypos_lowest) {
                    r->ypos_lowest = r->ypos;
                }

                /* Go on recursively. */
                region_add_rec(r, m->tile_map[i], region_name);

                switch (i) {
                    case 0:
                        r->ypos += MAP_HEIGHT(m->tile_map[i]) * MAX_PIXELS;
                        break;

                    case 1:
                        r->xpos -= MAP_WIDTH(m->tile_map[i]) * MAX_PIXELS;
                        break;

                    case 2:
                        r->ypos -= MAP_HEIGHT(m->tile_map[i]) * MAX_PIXELS;
                        break;

                    case 3:
                        r->xpos += MAP_WIDTH(m->tile_map[i]) * MAX_PIXELS;
                        break;
                }
            }
        }
    }
}

/**
 * The main world maker function.
 */
void world_maker(void)
{
    mapstruct *m;
    gdImagePtr im;
    FILE *out;
    size_t i;
    region_struct *r;
    wm_region *wm_r;
    char buf[MAX_BUF];
    int x, y, layer, sub_layer, got_one;
    int xpos = 0, ypos = 0;
    FILE *def_fp;
    object **info_objects = NULL;
    size_t num_info_objects = 0;

    /* Initialize the image colors. */
    wm_images_init();

    /* Go through regions. */
    for (r = first_region; r; r = r->next) {
        /* No first map? */
        if (!r->map_first) {
            continue;
        }

        /* Initialize the region. */
        wm_r = ecalloc(1, sizeof(wm_region));
        /* Open the definitions file. */
        snprintf(VS(buf), "%s/client-maps/%s.def", settings.httppath, r->name);
        path_ensure_directories(buf);

        def_fp = fopen(buf, "w");

        if (!def_fp) {
            logger_print(LOG(ERROR), "Could not open '%s': %s", buf,
                         strerror(errno));
            exit(1);
        }

        /* Load the first map. */
        m = ready_map_name(r->map_first, 0);
        /* Parse the maps recursively. */
        region_add_rec(wm_r, m, r->name);

        snprintf(VS(buf), "%s/client-maps/%s.png", settings.httppath, r->name);
        path_ensure_directories(buf);

        /* Store defaults in the definitions file. */
        fprintf(def_fp, "pixel_size %d\n", MAX_PIXELS);
        fprintf(def_fp, "map_size_x %d\n", MAP_WIDTH(m));
        fprintf(def_fp, "map_size_y %d\n", MAP_HEIGHT(m));

        /* Go through the loaded maps. */
        for (i = 0; i < wm_r->num_maps; i++) {
            int map_w, map_h;

            /* Adjust X and Y positions. */
            wm_r->maps[i].xpos -= wm_r->xpos_lowest;
            wm_r->maps[i].ypos -= wm_r->ypos_lowest;

            /* Calculate the maximum width needed for the actual image. */
            map_w = MAP_WIDTH(wm_r->maps[i].m) * MAX_PIXELS +
                    wm_r->maps[i].xpos;
            map_h = MAP_HEIGHT(wm_r->maps[i].m) * MAX_PIXELS +
                    wm_r->maps[i].ypos;

            if (map_w > wm_r->w) {
                wm_r->w = map_w;
            }

            if (map_h > wm_r->h) {
                wm_r->h = map_h;
            }

            /* Store the map path, labels, etc. */
            fprintf(def_fp, "map %x %x %s\n", wm_r->maps[i].xpos,
                    wm_r->maps[i].ypos, wm_r->maps[i].m->path);
        }

        /* Create the image. */
        im = gdImageCreateTrueColor(wm_r->w, wm_r->h);

        /* Custom background to use? */
        if (r->map_bg) {
            uint32 im_r, im_g, im_b;

            /* Parse HTML color and fill the image with it. */
            if (sscanf(r->map_bg, "#%2X%2X%2X", &im_r, &im_g, &im_b) == 3) {
                gdImageFill(im, 0, 0,
                            gdImageColorAllocate(im, im_r, im_g, im_b));
            }
        }
        /* Transparency otherwise. */
        else {
            gdImageAlphaBlending(im, 1);
            gdImageSaveAlpha(im, 1);
            gdImageFill(im, 0, 0, gdTransparent);
        }

        /* Go through the maps. */
        for (i = 0; i < wm_r->num_maps; i++) {
            object *tmp;

            m = wm_r->maps[i].m;

            /* Draw layer 1 and layer 2 objects. */
            for (x = 0; x < MAP_WIDTH(m); x++) {
                for (y = 0; y < MAP_HEIGHT(m); y++) {
                    got_one = 0;
                    xpos = x * MAX_PIXELS + wm_r->maps[i].xpos;
                    ypos = y * MAX_PIXELS + wm_r->maps[i].ypos;

                    /* Look for map info objects. */
                    for (tmp = GET_MAP_OB(m, x, y);
                         tmp && tmp->layer == LAYER_SYS;
                         tmp = tmp->above) {
                        if (tmp->type != CLIENT_MAP_INFO) {
                            continue;
                        }

                        if (tmp->sub_type != CLIENT_MAP_HIDE && (!tmp->name ||
                                strstr(tmp->name, " ") ||
                                !strcmp(tmp->name, tmp->arch->name))) {
                            continue;
                        }

                        /* Transform newline characters in the message to
                         * literal "\n". */
                        if (tmp->msg) {
                            char msg[HUGE_BUF * 4];

                            string_replace(tmp->msg, "\n", "\\n", msg,
                                           sizeof(msg));
                            FREE_AND_COPY_HASH(tmp->msg, msg);
                        }

                        /* Label. */
                        if (tmp->sub_type == CLIENT_MAP_LABEL) {
                            fprintf(def_fp, "label %x %x %s %s\n",
                                    xpos + tmp->last_heal * MAX_PIXELS,
                                    ypos + tmp->last_sp * MAX_PIXELS,
                                    tmp->name, tmp->msg ? tmp->msg : "???");

                            if (QUERY_FLAG(tmp, FLAG_CURSED)) {
                                fprintf(def_fp, "label_hide\n");
                            }
                        }
                        else {
                            info_objects = erealloc(
                                info_objects,
                                sizeof(*info_objects) * (num_info_objects + 1)
                            );
                            info_objects[num_info_objects] = tmp;
                            num_info_objects++;
                        }
                    }

                    for (layer = LAYER_FLOOR;
                         layer <= LAYER_FMASK;
                         layer++) {
                        for (sub_layer = 0;
                             sub_layer < NUM_SUB_LAYERS;
                             sub_layer++) {
                            if (render_object(
                                    im, xpos, ypos,
                                    GET_MAP_OB_LAYER(m, x, y, layer, sub_layer)
                                )) {
                                got_one = 1;
                            }
                        }
                    }

                    /* Didn't get an object, fill this square with black. */
                    if (!got_one) {
                        gdImageFilledRectangle(
                            im, xpos, ypos,
                            xpos + MAX_PIXELS - 1,
                            ypos + MAX_PIXELS - 1,
                            gdImageColorAllocate(im, 0, 0, 0)
                        );
                    }
                }
            }

            /* Draw the rest of the objects. */
            for (x = 0; x < MAP_WIDTH(m); x++) {
                for (y = 0; y < MAP_HEIGHT(m); y++) {
                    xpos = x * MAX_PIXELS + wm_r->maps[i].xpos;
                    ypos = y * MAX_PIXELS + wm_r->maps[i].ypos;

                    for (layer = LAYER_ITEM;
                         layer < NUM_LAYERS;
                         layer++) {
                        for (sub_layer = 0;
                             sub_layer < NUM_SUB_LAYERS;
                             sub_layer++) {
                            render_object(
                                im, xpos, ypos,
                                GET_MAP_OB_LAYER(m, x, y, layer, sub_layer)
                            );
                        }
                    }
                }
            }
        }

        /* Parse other information objects. */
        for (i = 0; i < num_info_objects; i++) {
            object *tmp = info_objects[i];
            size_t j;

            /* Already parsed this object. */
            if (!tmp) {
                continue;
            }

            /* Get the correct X/Y positions. */
            for (j = 0; j < wm_r->num_maps; j++) {
                if (wm_r->maps[j].m == tmp->map) {
                    xpos = tmp->x * MAX_PIXELS + wm_r->maps[j].xpos;
                    ypos = tmp->y * MAX_PIXELS + wm_r->maps[j].ypos;
                    break;
                }
            }

            /* Hiding part of the map. */
            if (tmp->sub_type == CLIENT_MAP_HIDE) {
                gdImageFilledRectangle(
                    im, xpos, ypos,
                    MIN(xpos + ((tmp->path_attuned + 1) * MAX_PIXELS),
                        (uint32) wm_r->w),
                    MIN(ypos + ((tmp->path_repelled + 1) * MAX_PIXELS),
                        (uint32) wm_r->h),
                    gdImageColorAllocate(im, 0, 0, 0)
                );
            }

            /* Tooltip. */
            if (tmp->msg && tmp->sub_type == CLIENT_MAP_TOOLTIP) {
                /* Tooltip with automatic width/height detection? */
                if (!tmp->path_attuned &&
                    !tmp->path_repelled &&
                    !tmp->item_level &&
                    QUERY_FLAG(tmp, FLAG_STAND_STILL)) {
                    for (j = 0; j < num_info_objects; j++) {
                        rv_vector rv;

                        /* Already parsed object or same as the master info
                         * object. */
                        if (!info_objects[j] || info_objects[j] == tmp) {
                            continue;
                        }

                        /* Not the same name as the master info object? */
                        if (strcmp(info_objects[j]->name, tmp->name)) {
                            continue;
                        }

                        /* Get range vector from the master info object to this
                         * one. */
                        if (!get_rangevector(tmp, info_objects[j], &rv,
                                             RV_RECURSIVE_SEARCH)) {
                            continue;
                        }

                        /* Set the calculated distances. */
                        if (!tmp->path_attuned) {
                            tmp->path_attuned = rv.distance_x;
                        }

                        if (!tmp->path_repelled) {
                            tmp->path_repelled = rv.distance_y;
                        }

                        /* Mark this object as parsed. */
                        info_objects[j] = NULL;
                    }
                }

                /* Write out information about this tooltip. */
                fprintf(def_fp, "tooltip %x %x %x %x %s %s\n",
                        MAX(0, xpos - ((tmp->item_level) * MAX_PIXELS)),
                        MAX(0, ypos - ((tmp->item_level) * MAX_PIXELS)),
                        MIN(xpos + ((tmp->item_level * 2) * MAX_PIXELS +
                            MAX_PIXELS) + (tmp->path_attuned * MAX_PIXELS),
                            (uint32) wm_r->w
                        ) - xpos,
                        MIN(ypos + ((tmp->item_level * 2) * MAX_PIXELS +
                            MAX_PIXELS) + (tmp->path_repelled * MAX_PIXELS),
                            (uint32) wm_r->h
                        ) - ypos,
                        tmp->name, tmp->msg);

                /* Outline set? */
                if (tmp->item_skill) {
                    fprintf(def_fp, "t_outline");

                    /* Store outline's color, if any. */
                    if (tmp->slaying) {
                        fprintf(def_fp, " %s", tmp->slaying);
                    }
                    /* No outline color, but there is non-standard size,
                     * so we must include the default color as well. */
                    else if (tmp->item_skill != 1) {
                        fprintf(def_fp, " #ff0000");
                    }

                    if (tmp->item_skill != 1) {
                        fprintf(def_fp, " %d", tmp->item_skill);
                    }

                    fprintf(def_fp, "\n");
                }

                if (QUERY_FLAG(tmp, FLAG_CURSED)) {
                    fprintf(def_fp, "tooltip_hide\n");
                }
            }
        }

        if (info_objects) {
            efree(info_objects);
            info_objects = NULL;
        }

        num_info_objects = 0;

        /* Free all maps to save memory. */
        free_all_maps();

        /* Write out the image. */
        out = fopen(buf, "wb");

        if (out == NULL) {
            logger_print(LOG(ERROR), "Could not open '%s': %s", buf,
                         strerror(errno));
            exit(1);
        }

        gdImagePng(im, out);
        fclose(out);

        /* Free data. */
        gdImageDestroy(im);
        fclose(def_fp);
        efree(wm_r->maps);
        efree(wm_r);
    }
}
