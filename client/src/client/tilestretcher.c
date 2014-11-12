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
 * Tile stretching routines.
 *
 * Tile stretching stretches all floor tiles and modifies y position of
 * all other layer objects, for example, floor tile with "z 4" would be
 * stretched, and all objects above it would have z increased by 4. Layer
 * 2 objects are also stretched.
 *
 * align_tile_stretch() calculates how much to stretch each tile in all
 * 4 directions based on the surrounding tiles.
 *
 * @todo Support for negative z (so surrounding tiles can have "z 0", and
 * the middle one "z 4" to make it look like a hole, for example).
 * @todo Most fmasks (grass, for example) are not properly stretched.
 *
 * @author James "JLittle" Little
 * @author Documentation by: Karon Webb */

#include <global.h>

/**
 * Pixel y-co-ordinate (offset) of the edge of a standard
 * (i.e. un-stretched) tile for each x co-ordinate pixel.
 * In our isometric view the displayed "depth" is half the width.
 * The tile pixels then form a rhombus on the VDU:
 *    @verbatim
 *                         //\\
 *    ^                   //    \\
 |                 //        \\
 |    y               //            \\
 |           W     //                \\    N
 |               //                    \\
 |             //                        \\
 |           //                            \\
 |         //                                \\
 |       //                                    \\
 |     //                                        \\
 |    //                                            \\
 \\                                            //
 \\                                        //
 \\                                    //
 \\                                //
 \\                            //
 \\                        //
 \\                    //
 \\          S     \\                //    E
 \\            //
 \\        //
 |                   \\    //
 + -                   \\//                x - >
 +    @endverbatim
 */
static int std_tile_half_len[] =
{
    0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9,
    10, 10, 11, 11, 11, 11, 10, 10, 9, 9, 8, 8, 7, 7, 6, 6, 5,
    5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0
};

/** Line information structure. */
typedef struct line_and_slope
{
    /** Starting X coordinate. */
    int sx;

    /** Starting Y coordinate. */
    int sy;

    /** Ending X coordinate. */
    int end_x;

    /** Ending Y coordinate. */
    int end_y;

    /**
     * Vertical distance per horizontal, e.g. 2 means y moves by 2 for
     * each x; 1/2 means x means 2 for each y (zero if the line is
     * vertical). */
    float slope;
} line_and_slope;

/**
 * Calculate line information given its start and end co-ordinates.
 *
 * This populates a ::line_and_slope structure.
 * @param dest What line information structure to populate.
 * @param sx Starting X.
 * @param sy Starting Y.
 * @param ex Ending X.
 * @param ey Ending Y. * */
static void determine_line(line_and_slope *dest, int sx, int sy, int ex, int ey)
{
    float y_diff, x_diff, slope;

    if (sy > ey) {
        y_diff = sy - ey;
    }
    else {
        y_diff = ey - sy;
    }

    if (sx > ex) {
        x_diff = sx - ex;
    }
    else {
        x_diff = ex - sx;
    }

    if (x_diff == 0) {
        slope = 0.0;
    }
    else {
        slope = y_diff / x_diff;
    }

    dest->sx = sx;
    dest->sy = sy;
    dest->end_x = ex;
    dest->end_y = ey;
    dest->slope = slope;
}

/**
 * Adds a colour to the palette in a bitmap ("surface"). */
int add_color_to_surface(SDL_Surface *dest, Uint8 red, Uint8 green, Uint8 blue)
{
    int i;
    Uint8 ncol = dest->format->palette->ncolors;
    SDL_Color colors[256];

    for (i = 0; i < ncol; i++) {
        colors[i].r = dest->format->palette->colors[i].r;
        colors[i].g = dest->format->palette->colors[i].g;
        colors[i].b = dest->format->palette->colors[i].b;
    }

    colors[ncol].r = red;
    colors[ncol].g = green;
    colors[ncol].b = blue;
    ncol++;

    SDL_SetColors(dest, colors, 0, ncol);
    dest->format->palette->ncolors = ncol;

    return 0;
}

/**
 * Copy a pixel from a source pixel map's co-ordinates to a target pixel
 * map's co-ordinates, adjusting the pixel's brightness.
 *
 * Pseudo-code:
 * @code
 *    PIXEL(dest, x2, y2) = AdjustPixelBrightness(PIXEL(src, x1, y1),
 * brightness)
 * @endcode
 * @note The pixel colour is added to the palette in the destination bitmap
 * if necessary.
 * @warning brightness above 1.0 is (apparently) allowed but brightness < 0
 * is not tested and could cause problems. */
int copy_pixel_to_pixel(SDL_Surface *src, SDL_Surface *dest, int x, int y, int x2, int y2, float brightness)
{
    Uint32 color;
    Uint8 red, green, blue, alpha, alpha_2;
    Uint8 red_2, green_2, blue_2;
    Uint16 n;

    if (x < 0 || y < 0 || x2 < 0 || y2 < 0) {
        return 0;
    }

    if (x >= src->w || x2 >= dest->w || y >= src->h || y2 >= dest->h) {
        return 0;
    }

    color = getpixel(src, x, y);

    /* No need to copy transparent pixels */
    if (src->format->BitsPerPixel == 8 && (color == src->format->colorkey)) {
        return 0;
    }

    SDL_GetRGBA(color, src->format, &red, &green, &blue, &alpha);

    if (alpha == 0) {
        return 0;
    }

    /* We must clamp to 255 since it is allowable for brightness to exceed 1.0
     * */
    n = (Uint16) red * brightness;
    red = (n <= 255) ? n : 255;
    n = (Uint16) green * brightness;
    green = (n <= 255) ? n : 255;
    n = (Uint16) blue * brightness;
    blue = (n <= 255) ? n : 255;

    color = SDL_MapRGBA(dest->format, red, green, blue, alpha);

    if (color == dest->format->colorkey) {
        blue += 256 >> (8 - dest->format->Bloss);
        color = SDL_MapRGBA(dest->format, red, green, blue, alpha);
    }

    if (dest->format->BitsPerPixel == 8) {
        SDL_GetRGBA(color, dest->format, &red_2, &green_2, &blue_2, &alpha_2);

        if (red != red_2 || green != green_2 || blue != blue_2 || alpha != alpha_2) {
            add_color_to_surface(dest, red, green, blue);
            color = SDL_MapRGBA(dest->format, red, green, blue, alpha);
        }
    }

    putpixel(dest, x2, y2, color);
    return 0;
}

/**
 * Copy a vertical line from a source pixel map's co-ordinates to a target
 * pixel map's co-ordinates, adjusting all the pixels' brightness.
 *
 * This also applies a transmutation, "stretching" or "shrinking the line
 * as appropriate:
 *
 *  If the target is twice the length it will receive pairs of pixels from
 *  the source, and if half the length every other pixel.
 *
 * brightness applies to all pixels: above 1.0 is (apparently) allowed but
 * brightness < 0 is not tested and could cause problems.
 *
 * Set extra non-zero to duplicate the top pixel if the target line height
 * is shorter than the source.
 * @note To allow for rounding, it is recommended to set extra unless the
 * target line is the same length or shorter.
 * @note There is no "smoothing" or "averaging" done at all - the result
 * could benefit hugely by adding logic to do things like that.
 * @note This function would be horribly inefficient if ever used to shrink
 * a long line down to a few pixels. */
int copy_vertical_line(SDL_Surface *src, SDL_Surface *dest, int src_x, int src_sy, int src_ey, int dest_x, int dest_sy, int dest_ey, float brightness, int extra)
{
    int src_h, dest_h, y;
    float ratio;

    SDL_LockSurface(src);
    SDL_LockSurface(dest);

    if (src_sy > src_ey) {
        int tmp = src_sy;

        src_sy = src_ey;
        src_ey = tmp;
    }

    if (dest_sy > dest_ey) {
        int tmp = dest_sy;

        dest_sy = dest_ey;
        dest_ey = tmp;
    }

    src_h = src_ey - src_sy;
    dest_h = dest_ey - dest_sy;

    /* Special cases */
    if (dest_h == 0) {
        if (src_h == 0) {
            copy_pixel_to_pixel(src, dest, src_x, src_sy, dest_x, dest_sy, brightness);

            SDL_UnlockSurface(src);
            SDL_UnlockSurface(dest);
            return 0;
        }
        else {
            copy_pixel_to_pixel(src, dest, src_x, (src_ey - src_sy) / 2, dest_x, dest_sy, brightness);

            SDL_UnlockSurface(src);
            SDL_UnlockSurface(dest);
            return 0;
        }
    }

    if (src_h == 0) {
        Uint32 color = getpixel(src, src_x, src_sy);

        for (y = dest_sy; y <= dest_ey; y++) {
            putpixel(dest, dest_x, y, color);
        }

        return 0;
    }

    /* The stretching */
    ratio = (float) src_h / (float) dest_h;

    for (y = 0; y <= dest_h; y++) {
        int go_y = dest_sy + y;
        int get_y = src_sy + (y * ratio);

        copy_pixel_to_pixel(src, dest, src_x, get_y, dest_x, go_y, brightness);
    }

    if (extra) {
        if (dest_ey + 1 < dest->h) {
            copy_pixel_to_pixel(src, dest, src_x, src_ey, dest_x, dest_ey + 1, brightness);
        }
    }

    SDL_UnlockSurface(src);
    SDL_UnlockSurface(dest);
    return 0;
}

/**
 * Generates the bitmap ("surface") for displaying a tile after stretching.
 *
 * Stretching takes place from the horizontal centre, so from a "standard"
 * viewpoint (i.e. some distance away from the viewed object), horizontal
 * stretching should be done with either e or w <= 0 and the other > 0
 * (the part of the object appearing the original size centres on the zero).
 *
 * Their absolute difference controls the brightness of the edges, so at the
 * moment a horizontal line of horizontally stretched objects will appear to
 * have vertical "bars" - especially as the underlying pixel stretching only
 * occurs vertically (i.e. the measured "width" doesn't change).
 *
 * Recommended limit is that neither e nor w exceed n+9. Major problems may
 * occur if the total horizontal stretching (i.e. ABS(e - w)) exceeds 25.
 *
 * Parameter documentation repeated in the body:
 *  - In the isometric view, closer objects are displayed lower down
 *  - on the VDU (smaller y co-ordinate) and further objects are
 *    displayed higher up (larger y co-ordinate).
 *  - The SW corner moves closer the more West the tile is stretched.
 *  - The NE corner moves closer the more East the tile is stretched.
 *  - Both the SW and NE corners move further away the more North the
 *    tile is stretched.
 *  - Both the SE and NW corners move close the more South and further
 *    away the more North the tile is stretched */
SDL_Surface *tile_stretch(SDL_Surface *src, int n, int e, int s, int w)
{
    SDL_Surface *destination, *tmp;
    float e_dark = 1.0, w_dark = 1.0;
    /* If set, copy_vertical_line will attempt to extend the line further
     * by 1 pixel (no idea why this is named "flat") */
    int flat;
    int sx, sy, ex, ey;
    int ln_num;
    int dest_sx, dest_sy, dest_ex, dest_ey;
    float dest_slope;
    int dest_sy_2, dest_ey_2;
    float dest_slope_2;
    int dest_x_inc, dest_y_inc;
    float kicker, kicker_2;
    int dest_y_inc_2;
    int x, y, y2;
    int at_least_one;
    int src_len;
    Uint32 color;
    Uint8 red, green, blue, alpha;
    /* Only index numbers 0-3 are actually used */
    line_and_slope dest_lines[10];

    /* Initialisation and housekeeping */
    SDL_LockSurface(src);

    tmp = SDL_CreateRGBSurface(src->flags, src->w, src->h + n, src->format->BitsPerPixel, src->format->Rmask, src->format->Gmask, src->format->Bmask, src->format->Amask);

    destination = SDL_DisplayFormatAlpha(tmp);
    SDL_FreeSurface(tmp);
    SDL_LockSurface(destination);

    color = getpixel(src, 0, 0);

    SDL_GetRGBA(color, src->format, &red, &green, &blue, &alpha);

    if (src->format->BitsPerPixel == 8) {
        add_color_to_surface(destination, red, green, blue);
    }

    /* We fill with black and full trans... */
    color = SDL_MapRGBA(destination->format, 0, 0, 0, 0);
    SDL_FillRect( destination, NULL, color);

    if (src->format->BitsPerPixel == 8) {
        SDL_SetColorKey(destination, SDL_SRCCOLORKEY, color);
    }

    /* If the target is the same size we don't want copy_vertical_line()
     * to try to extent the line by 1 pixel */
    if (n == 0 && e == 0 && w == 0 && s == 0) {
        flat = 0;
    }
    else {
        flat = 1;
    }

    /* Calculate the darkness (contrast) of one or both sides */
    if (w > e) {
        w_dark = 1.0 - ((w - e) / 25.0);

        if (n > 0 || s > 0) {
            e_dark = w_dark;
        }
    }

    if (e > w) {
        e_dark = 1.0 + ((e - w) / 25.0);

        if (s > 0 || n > 0) {
            w_dark = e_dark;
        }
    }

    /* Calculate the information about the lines which will form the
     * edge of the stretched tile (see the comments above
     * std_tile_half_len for a picture).
     *
     * In the isometric view, closer objects are displayed lower down
     * on the VDU (smaller y co-ordinate) and further objects are
     * displayed higher up (larger y co-ordinate).
     *
     * The SW corner moves closer the more West the tile is stretched.
     * The NE corner moves closer the more East the tile is stretched.
     * Both the SW and NE corners move further away the more North the
     * tile is stretched.
     * Both the SE and NW corners move close the more South and further
     * away the more North the tile is stretched */

    /* 0: Southern edge: SW to SE corner */
    sx = 2;
    sy = (10 - w) + n;
    ex = 22;
    ey = 0;
    determine_line(&dest_lines[0], sx, sy, ex, ey);
    /* 1: Western edge: SW to NW corner */
    sx = 2;
    sy = (12 - w) + n;
    ex = 22;
    ey = 22 + n - s;
    determine_line(&dest_lines[1], sx, sy, ex, ey);
    /* 2: Eastern edge: NE to SE corner */
    sx = 45;
    sy = (10 - e) + n;
    ex = 25;
    ey = 0;
    determine_line(&dest_lines[2], sx, sy, ex, ey);
    /* 3: Northern edge: NE to NW corner */
    sx = 45;
    sy = (12 - e) + n;
    ex = 25;
    ey = 22 + n - s;
    determine_line(&dest_lines[3], sx, sy, ex, ey);

    /* loop information:
     * effective loop control:
     * for (ln_num = 0; ln_num < 4; ln_num += 2) */
    for (ln_num = 0; ln_num < 4; ln_num++) {
        /* see "effective loop control" */
        if (ln_num == 1 || ln_num == 3) {
            continue;
        }

        /* Extract the information for the first, i.e. bottom, line (S or E
         * edge) */
        dest_sx = dest_lines[ln_num].sx;
        dest_sy = dest_lines[ln_num].sy;
        dest_ex = dest_lines[ln_num].end_x;
        dest_ey = dest_lines[ln_num].end_y;
        dest_slope = dest_lines[ln_num].slope;

        /* ln_num is always either 0 or 2 here */
        if (ln_num == 0 || ln_num == 2) {
            /* Extract the information for the second, i.e. top, line (W or N
             * edge) */
            dest_sy_2 = dest_lines[ln_num + 1].sy;
            dest_ey_2 = dest_lines[ln_num + 1].end_y;
            dest_slope_2 = dest_lines[ln_num + 1].slope;
        }
        else {
            /* Dead code: information about the second line is the same as the
             * first! */
            dest_sy_2 = dest_lines[ln_num].sy;
            dest_ey_2 = dest_lines[ln_num].end_y;
            dest_slope_2 = dest_lines[ln_num].slope;
        }

        /* Calculate the direction of the y co-ordinate */
        if (dest_sy > dest_ey) {
            dest_y_inc = -1;
        }
        else {
            dest_y_inc = 1;
        }

        /* Calculate the direction of the x co-ordinate */
        if (dest_sx > dest_ex) {
            dest_x_inc = -1;
        }
        else {
            dest_x_inc = 1;
        }

        /* Calculate the direction of the 2nd y co-ordinate */
        if (dest_sy_2 > dest_ey_2) {
            dest_y_inc_2 = -1;
        }
        else {
            dest_y_inc_2 = 1;
        }

        /* Initialise loop controls: "kicker" means the co-ordinate
         * crosses the line (another weird name) */
        x = dest_sx;
        y = dest_sy;
        kicker = 0.0;
        y2 = dest_sy_2;
        kicker_2 = 0.0;
        /* Make sure at least one row of pixels is output (who chose that name?)
         * */
        at_least_one = 0;

        /* Main inner loop to draw each vertical line in the stretched bitmap
         * loop information:
         *
         * horizontal (or vertical) edges are drawn with only one line of pixels
         * (at_least_one)
         *
         * effective loop control when non-horizontal:
         * for (x1 = dest_sx; x1 != dest_ex; x1 += dest_x_inc) */
        while (((dest_slope != 0.0) && (x != dest_ex) && (y != dest_ey)) || ((at_least_one == 0) && (dest_slope == 0.0))) {
            /* Exit the loop after the first iteration if the line is exactly
             * horizontal (or vertical) */
            at_least_one = 1;

            if (kicker >= 1.0) {
                kicker = kicker - 1.0;
                y = y + dest_y_inc;
            }

            if (kicker_2 >= 1.0) {
                kicker_2 = kicker_2 - 1.0;
                y2 = y2 + dest_y_inc_2;
            }

            /* Choose y co-ordinates either side of the central horizontal */
            src_len = std_tile_half_len[x];

            if (ln_num < 2) {
                copy_vertical_line(src, destination, x, 11 + src_len, 11 - src_len, x, y, y2, w_dark, flat);
            }
            else {
                copy_vertical_line(src, destination, x, 11 + src_len, 11 - src_len, x, y, y2, e_dark, flat);
            }

            x = x + dest_x_inc;

            kicker = kicker + dest_slope;
            kicker_2 = kicker_2 + dest_slope_2;
        }
    }

    for (x = 22; x < 22 + 2; x++) {
        copy_vertical_line(src, destination, x, 0, 23, x, 0, 23 + n - s, w_dark, flat);
    }

    for (x = 24; x < 24 + 2; x++) {
        copy_vertical_line(src, destination, x, 0, 23, x, 0, 23 + n - s, e_dark, flat);
    }

    for (x = 0; x < 2; x++) {
        copy_pixel_to_pixel(src,destination, x, 11, x, 11 + n - w, w_dark);
    }

    for (x = 46; x < 48; x++) {
        copy_pixel_to_pixel(src, destination, x, 11, x, 11 + n - e, e_dark);
    }

    SDL_UnlockSurface(src);
    SDL_UnlockSurface(destination);
    return destination;
}
