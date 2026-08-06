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
 * Random map generation main routines.
 * @todo Explain process, layout signs (# C D < >) and such.
 */

#include <global.h>
#include <server_main.h>
#include <toolkit/string.h>

/**
 * Dumps specified layout using printf().
 * @param layout
 * The layout to dump.
 * @param RP
 * Layout parameters.
 */
void dump_layout(char **layout, RMParms *RP) {
    int i, j;

    for (i = 0; i < RP->Xsize; i++) {
        for (j = 0; j < RP->Ysize; j++) {
            if (layout[i][j] == '\0') {
                layout[i][j] = ' ';
            }

            printf("%c", layout[i][j]);

            if (layout[i][j] == ' ') {
                layout[i][j] = '\0';
            }
        }

        printf("\n");
    }
}

/**
 * Main random map routine. Generates a random map based on specified
 * parameters.
 * @param OutFileName
 * The path the map should have.
 * @param RP
 * Parameters for generation.
 * @return
 * Pointer to the generated map.
 */
mapstruct *generate_random_map(char *OutFileName, RMParms *RP) {
    char **layout;
    mapstruct *theMap;
    int i;

    uint64_t seed = RP->random_seed == 0 ? rndm_u64() : (uint64_t)(uint32_t)RP->random_seed;
    rng_seed(&RP->rng, seed);

    if (RP->difficulty == 0) {
        /* use this instead of a map difficulty */
        RP->difficulty = RP->dungeon_level;
    } else {
        RP->difficulty_given = 1;
    }

    if (RP->expand2x > 0) {
        RP->Xsize /= 2;
        RP->Ysize /= 2;
    }

    layout = layoutgen(RP);

    if (RP->level_increment > 0) {
        RP->dungeon_level += RP->level_increment;
    } else {
        RP->dungeon_level++;
    }

    /* rotate the layout randomly */
    layout = rotate_layout(layout, rng_range(&RP->rng, 0, 3), RP);

#ifdef RMAP_DEBUG
    dump_layout(layout, RP);
#endif

    /* allocate the map and set the floor */
    theMap = make_map_floor(RP->floorstyle, RP);

    /* set the name of the map. */
    FREE_AND_COPY_HASH(theMap->path, OutFileName);

    FREE_AND_COPY_HASH(theMap->name, RP->dungeon_name[0] ? RP->dungeon_name : OutFileName);

    if (RP->bg_music[0] != '\0') {
        FREE_AND_COPY_HASH(theMap->bg_music, RP->bg_music);
    }

    theMap->difficulty = RP->dungeon_level;

    make_map_walls(theMap, layout, RP->wallstyle, RP);

    put_doors(theMap, layout, RP->doorstyle, RP);

    place_exits(theMap, layout, RP->exitstyle, RP->orientation, RP);

    place_monsters(theMap, RP->monsterstyle, RP->difficulty, RP);

    put_decor(theMap, layout, RP);

    unblock_exits(theMap, layout, RP);
    set_map_darkness(theMap, RP->darkness);

    for (i = 0; i < RP->Xsize; i++) {
        free(layout[i]);
    }

    free(layout);

    return theMap;
}

/**
 * This function builds the actual layout.
 * Selects the layout based on parameters and gives it whatever
 * arguments it needs.
 * @param RP
 * Random map parameters.
 * @return
 * The built layout, must be freed by caller.
 */
char **layoutgen(RMParms *RP) {
    char **maze = NULL;

    if (RP->symmetry != NO_SYM) {
        if (RP->Xsize < 15) {
            RP->Xsize = rng_range(&RP->rng, 15, 39);
        }

        if (RP->Ysize < 15) {
            RP->Ysize = rng_range(&RP->rng, 15, 39);
        }
    } else {
        /* Has to be at least 7 for square spirals to work */
        if (RP->Xsize < 7) {
            RP->Xsize = rng_range(&RP->rng, 15, 39);
        }

        if (RP->Ysize < 7) {
            RP->Ysize = rng_range(&RP->rng, 15, 39);
        }
    }

    if (RP->symmetry == RANDOM_SYM) {
        RP->symmetry_used = rng_range(&RP->rng, 1, XY_SYM);

        if (RP->symmetry_used == Y_SYM || RP->symmetry_used == XY_SYM) {
            RP->Ysize = RP->Ysize / 2 + 1;
        }

        if (RP->symmetry_used == X_SYM || RP->symmetry_used == XY_SYM) {
            RP->Xsize = RP->Xsize / 2 + 1;
        }
    } else {
        RP->symmetry_used = RP->symmetry;
    }

    if (RP->symmetry == Y_SYM || RP->symmetry == XY_SYM) {
        RP->Ysize = RP->Ysize / 2 + 1;
    }

    if (RP->symmetry == X_SYM || RP->symmetry == XY_SYM) {
        RP->Xsize = RP->Xsize / 2 + 1;
    }

    if (strstr(RP->layoutstyle, "onion")) {
        maze =
            map_gen_onion(RP->Xsize, RP->Ysize, RP->layoutoptions1, RP->layoutoptions2, &RP->rng);

        RP->map_layout_style = ONION_LAYOUT;

        if (rng_chance(&RP->rng, 3) && !(RP->layoutoptions1 & OPT_WALLS_ONLY)) {
            roomify_layout(maze, RP);
        }
    } else if (strstr(RP->layoutstyle, "maze")) {
        maze = maze_gen(RP->Xsize, RP->Ysize, RP->layoutoptions1, &RP->rng);

        RP->map_layout_style = MAZE_LAYOUT;

        if (rng_chance(&RP->rng, 2)) {
            doorify_layout(maze, RP);
        }
    } else if (strstr(RP->layoutstyle, "spiral")) {
        maze = map_gen_spiral(RP->Xsize, RP->Ysize, RP->layoutoptions1, &RP->rng);

        RP->map_layout_style = SPIRAL_LAYOUT;

        if (rng_chance(&RP->rng, 2)) {
            doorify_layout(maze, RP);
        }
    } else if (strstr(RP->layoutstyle, "rogue")) {
        maze = roguelike_layout_gen(RP->Xsize, RP->Ysize, RP->layoutoptions1, &RP->rng);

        RP->map_layout_style = ROGUELIKE_LAYOUT;
    } else if (strstr(RP->layoutstyle, "snake")) {
        maze = make_snake_layout(RP->Xsize, RP->Ysize, &RP->rng);

        RP->map_layout_style = SNAKE_LAYOUT;

        if (rng_chance(&RP->rng, 2)) {
            roomify_layout(maze, RP);
        }
    } else if (strstr(RP->layoutstyle, "squarespiral")) {
        maze = make_square_spiral_layout(RP->Xsize, RP->Ysize, &RP->rng);

        RP->map_layout_style = SQUARE_SPIRAL_LAYOUT;

        if (rng_chance(&RP->rng, 2)) {
            roomify_layout(maze, RP);
        }
    }

    /* unknown or unspecified layout type, pick one at random */
    if (maze == NULL) {
        switch (rng_range(&RP->rng, 0, NROFLAYOUTS - 1)) {
            case 0:
                maze = maze_gen(RP->Xsize, RP->Ysize, rng_range(&RP->rng, 0, 1), &RP->rng);

                RP->map_layout_style = MAZE_LAYOUT;

                if (rng_chance(&RP->rng, 2)) {
                    doorify_layout(maze, RP);
                }

                break;

            case 1:
                maze = map_gen_onion(RP->Xsize,
                                     RP->Ysize,
                                     RP->layoutoptions1,
                                     RP->layoutoptions2,
                                     &RP->rng);

                RP->map_layout_style = ONION_LAYOUT;

                if (rng_chance(&RP->rng, 3) && !(RP->layoutoptions1 & OPT_WALLS_ONLY)) {
                    roomify_layout(maze, RP);
                }

                break;

            case 2:
                maze = map_gen_spiral(RP->Xsize, RP->Ysize, RP->layoutoptions1, &RP->rng);

                RP->map_layout_style = SPIRAL_LAYOUT;

                if (rng_chance(&RP->rng, 2)) {
                    doorify_layout(maze, RP);
                }

                break;

            case 3:
                maze = roguelike_layout_gen(RP->Xsize, RP->Ysize, RP->layoutoptions1, &RP->rng);

                RP->map_layout_style = ROGUELIKE_LAYOUT;

                break;

            case 4:
                maze = make_snake_layout(RP->Xsize, RP->Ysize, &RP->rng);

                RP->map_layout_style = SNAKE_LAYOUT;

                if (rng_chance(&RP->rng, 2)) {
                    roomify_layout(maze, RP);
                }

                break;

            case 5:
                maze = make_square_spiral_layout(RP->Xsize, RP->Ysize, &RP->rng);

                RP->map_layout_style = SQUARE_SPIRAL_LAYOUT;

                if (rng_chance(&RP->rng, 2)) {
                    roomify_layout(maze, RP);
                }

                break;
        }
    }

    maze = symmetrize_layout(maze, RP->symmetry_used, RP);

#ifdef RMAP_DEBUG
    dump_layout(maze, RP);
#endif

    if (RP->expand2x) {
        maze = expand2x(maze, RP->Xsize, RP->Ysize);

        RP->Xsize = RP->Xsize * 2 - 1;
        RP->Ysize = RP->Ysize * 2 - 1;
    }

    return maze;
}

/**
 * Takes a map and makes it symmetric: adjusts Xsize and Ysize to produce
 * a symmetric map.
 * @param maze
 * Layout to symmetrize. Will be freed by this function.
 * @param sym
 * how to make symmetric, a @ref SYM_xxx value.
 * @param RP
 * Random map parameters.
 * @return
 * New layout, must be freed by caller.
 */
char **symmetrize_layout(char **maze, int sym, RMParms *RP) {
    int i, j, Xsize_orig = RP->Xsize, Ysize_orig = RP->Ysize;
    char **sym_maze;

    /* tell everyone else what sort of symmetry is used.*/
    RP->symmetry_used = sym;

    if (sym == NO_SYM) {
        RP->Xsize = Xsize_orig;
        RP->Ysize = Ysize_orig;

        return maze;
    }

    /* pick new sizes */
    RP->Xsize = ((sym == X_SYM || sym == XY_SYM) ? RP->Xsize * 2 - 3 : RP->Xsize);
    RP->Ysize = ((sym == Y_SYM || sym == XY_SYM) ? RP->Ysize * 2 - 3 : RP->Ysize);

    sym_maze = xcalloc(RP->Xsize, sizeof(*sym_maze));

    for (i = 0; i < RP->Xsize; i++) {
        sym_maze[i] = xcalloc(sizeof(char), RP->Ysize);
    }

    if (sym == X_SYM) {
        for (i = 0; i < RP->Xsize / 2 + 1; i++) {
            for (j = 0; j < RP->Ysize; j++) {
                sym_maze[i][j] = maze[i][j];
                sym_maze[RP->Xsize - i - 1][j] = maze[i][j];
            }
        }
    }

    if (sym == Y_SYM) {
        for (i = 0; i < RP->Xsize; i++) {
            for (j = 0; j < RP->Ysize / 2 + 1; j++) {
                sym_maze[i][j] = maze[i][j];
                sym_maze[i][RP->Ysize - j - 1] = maze[i][j];
            }
        }
    }

    if (sym == XY_SYM) {
        for (i = 0; i < RP->Xsize / 2 + 1; i++) {
            for (j = 0; j < RP->Ysize / 2 + 1; j++) {
                sym_maze[i][j] = maze[i][j];
                sym_maze[i][RP->Ysize - j - 1] = maze[i][j];

                sym_maze[RP->Xsize - i - 1][j] = maze[i][j];
                sym_maze[RP->Xsize - i - 1][RP->Ysize - j - 1] = maze[i][j];
            }
        }
    }

    /* delete the old maze */
    for (i = 0; i < Xsize_orig; i++) {
        free(maze[i]);
    }

    free(maze);

    /* reconnect disjointed spirals */
    if (RP->map_layout_style == SPIRAL_LAYOUT) {
        connect_spirals(RP->Xsize, RP->Ysize, sym, sym_maze);
    }

    /* reconnect disjointed nethack mazes:  the routine for
     *    spirals will do the trick?*/
    if (RP->map_layout_style == ROGUELIKE_LAYOUT) {
        connect_spirals(RP->Xsize, RP->Ysize, sym, sym_maze);
    }

    return sym_maze;
}

/**
 * Takes  a map and rotates it. This completes the onion layouts, making
 * them possibly centered on any wall.
 *
 * It'll modify Xsize and Ysize if they're swapped.
 * @param maze
 * Layout to rotate, will be freed by this function.
 * @param rotation
 * How to rotate:
 * - <b>0</b>: Don't do anything.
 * - <b>1</b>: Rotate 90 degrees clockwise.
 * - <b>2</b>: Rotate 180 degrees.
 * - <b>3</b>: Rotate 90 degrees counter-clockwise.
 * @param RP
 * Random map parameters.
 * @return
 * New layout, must be freed be caller. NULL if invalid
 * rotation.
 */
char **rotate_layout(char **maze, int rotation, RMParms *RP) {
    char **new_maze;
    int i, j;

    switch (rotation) {
        case 0:
            return maze;
            break;

            /* a reflection */
        case 2: {
            char *new = xmallocarray((size_t)RP->Xsize * (size_t)RP->Ysize, sizeof(*new));

            /* make a copy */
            for (i = 0; i < RP->Xsize; i++) {
                for (j = 0; j < RP->Ysize; j++) {
                    new[i * RP->Ysize + j] = maze[i][j];
                }
            }

            /* copy a reflection back */
            for (i = 0; i < RP->Xsize; i++) {
                for (j = 0; j < RP->Ysize; j++) {
                    maze[i][j] = new[(RP->Xsize - i - 1) * RP->Ysize + RP->Ysize - j - 1];
                }
            }

            free(new);
            return maze;
            break;
        }

        case 1:
        case 3: {
            int swap;

            new_maze = xcalloc(RP->Ysize, sizeof(*new_maze));

            for (i = 0; i < RP->Ysize; i++) {
                new_maze[i] = xcalloc(sizeof(char), RP->Xsize);
            }

            /* swap x and y */
            if (rotation == 1) {
                for (i = 0; i < RP->Xsize; i++) {
                    for (j = 0; j < RP->Ysize; j++) {
                        new_maze[j][i] = maze[i][j];
                    }
                }
            }

            /* swap x and y */
            if (rotation == 3) {
                for (i = 0; i < RP->Xsize; i++) {
                    for (j = 0; j < RP->Ysize; j++) {
                        new_maze[j][i] = maze[RP->Xsize - i - 1][RP->Ysize - j - 1];
                    }
                }
            }

            /* delete the old layout */
            for (i = 0; i < RP->Xsize; i++) {
                free(maze[i]);
            }

            free(maze);

            swap = RP->Ysize;
            RP->Ysize = RP->Xsize;
            RP->Xsize = swap;
            return new_maze;
            break;
        }
    }

    return NULL;
}

/**
 * Take a layout and make some rooms in it. Works best on onions.
 * @param maze
 * Layout to alter.
 * @param RP
 * Random map parameters.
 */
void roomify_layout(char **maze, RMParms *RP) {
    int tries = RP->Xsize * RP->Ysize / 30, ti;

    for (ti = 0; ti < tries; ti++) {
        /* starting location for looking at creating a door */
        int dx, dy;
        /* results of checking on creating walls. */
        int cx, cy;

        dx = rng_range(&RP->rng, 0, RP->Xsize - 1);
        dy = rng_range(&RP->rng, 0, RP->Ysize - 1);

        /* horizontal */
        cx = can_make_wall(maze, dx, dy, 0, RP);

        /* vertical */
        cy = can_make_wall(maze, dx, dy, 1, RP);

        if (cx == -1) {
            if (cy != -1) {
                make_wall(maze, dx, dy, 1);
            }

            continue;
        }

        if (cy == -1) {
            make_wall(maze, dx, dy, 0);

            continue;
        }

        if (cx < cy) {
            make_wall(maze, dx, dy, 0);
        } else {
            make_wall(maze, dx, dy, 1);
        }
    }
}

/**
 * Checks the layout to see if we can stick a horizontal (dir = 0) wall
 * (or vertical, dir == 1) here which ends up on other walls sensibly.
 * @param maze
 * Layout.
 * @param dx
 * X coordinate to check
 * @param dy
 * Y coordinate to check
 * @param dir
 * Direction:
 * - <b>0</b>: Horizontally.
 * - <b>1</b>: Vertically.
 * @param RP
 * Random map parameters.
 * @return
 * -1 if wall can't be made, possibly wall length otherwise.
 */
int can_make_wall(char **maze, int dx, int dy, int dir, RMParms *RP) {
    int i1, length = 0;

    /* don't make walls if we're on the edge. */
    if (dx == 0 || dx == (RP->Xsize - 1) || dy == 0 || dy == (RP->Ysize - 1)) {
        return -1;
    }

    /* don't make walls if we're ON a wall. */
    if (maze[dx][dy] != '\0') {
        return -1;
    }

    /* horizontal */
    if (dir == 0) {
        int y = dy;

        for (i1 = dx - 1; i1 > 0; i1--) {
            int sindex = surround_flag2(maze, i1, y, RP);

            if (sindex == 1) {
                break;
            }

            /* can't make horizontal wall here */
            if (sindex != 0) {
                return -1;
            }

            /* can't make horizontal wall here */
            if (maze[i1][y] != '\0') {
                return -1;
            }

            length++;
        }

        for (i1 = dx + 1; i1 < RP->Xsize - 1; i1++) {
            int sindex = surround_flag2(maze, i1, y, RP);

            if (sindex == 2) {
                break;
            }

            /* can't make horizontal wall here */
            if (sindex != 0) {
                return -1;
            }

            /* can't make horizontal wall here */
            if (maze[i1][y] != '\0') {
                return -1;
            }

            length++;
        }

        return length;
    } else {
        int x = dx;

        /* vertical */

        for (i1 = dy - 1; i1 > 0; i1--) {
            int sindex = surround_flag2(maze, x, i1, RP);

            if (sindex == 4) {
                break;
            }

            /* can't make vertical wall here */
            if (sindex != 0) {
                return -1;
            }

            /* can't make vertical wall here */
            if (maze[x][i1] != '\0') {
                return -1;
            }

            length++;
        }

        for (i1 = dy + 1; i1 < RP->Ysize - 1; i1++) {
            int sindex = surround_flag2(maze, x, i1, RP);

            if (sindex == 8) {
                break;
            }

            /* can't make vertical wall here */
            if (sindex != 0) {
                return -1;
            }

            /* can't make vertical wall here */
            if (maze[x][i1] != '\0') {
                return -1;
            }

            length++;
        }

        return length;
    }
}

/**
 * Cuts the layout horizontally or vertically by a wall with a door.
 * @param maze
 * Layout.
 * @param x
 * X position where to put the door.
 * @param y
 * Y position where to put the door.
 * @param dir
 * Wall direction:
 * - <b>0</b>: Horizontally.
 * - <b>1</b>: Vertically.
 * @return
 * Always returns 0.
 */
int make_wall(char **maze, int x, int y, int dir) {
    int i1;

    /* mark a door */
    maze[x][y] = 'D';

    switch (dir) {
            /* horizontal */
        case 0:
            for (i1 = x - 1; maze[i1][y] == '\0'; i1--) {
                maze[i1][y] = '#';
            }

            for (i1 = x + 1; maze[i1][y] == '\0'; i1++) {
                maze[i1][y] = '#';
            }

            break;

            /* vertical */
        case 1:
            for (i1 = y - 1; maze[x][i1] == '\0'; i1--) {
                maze[x][i1] = '#';
            }

            for (i1 = y + 1; maze[x][i1] == '\0'; i1++) {
                maze[x][i1] = '#';
            }

            break;
    }

    return 0;
}

/**
 * Puts doors at appropriate locations in a layout.
 * @param maze
 * Layout.
 * @param RP
 * Random map parameters.
 */
void doorify_layout(char **maze, RMParms *RP) {
    /* reasonable number of doors. */
    int ndoors = RP->Xsize * RP->Ysize / 60;
    int *doorlist_x, *doorlist_y;
    /* # of available doorlocations */
    int doorlocs = 0;
    int i, j;

    size_t doorlist_size = (size_t)RP->Xsize * (size_t)RP->Ysize;
    doorlist_x = xmallocarray(doorlist_size, sizeof(*doorlist_x));
    doorlist_y = xmallocarray(doorlist_size, sizeof(*doorlist_y));

    /* make a list of possible door locations */
    for (i = 1; i < RP->Xsize - 1; i++) {
        for (j = 1; j < RP->Ysize - 1; j++) {
            int sindex = surround_flag(maze, i, j, RP);

            /* these are possible door sindex */
            if (sindex == 3 || sindex == 12) {
                doorlist_x[doorlocs] = i;
                doorlist_y[doorlocs] = j;

                doorlocs++;
            }
        }
    }

    while (ndoors > 0 && doorlocs > 0) {
        int di = rng_range(&RP->rng, 0, doorlocs - 1), sindex;

        i = doorlist_x[di];
        j = doorlist_y[di];

        sindex = surround_flag(maze, i, j, RP);

        /* these are possible door sindex */
        if (sindex == 3 || sindex == 12) {
            maze[i][j] = 'D';
            ndoors--;
        }

        /* reduce the size of the list */
        doorlocs--;

        doorlist_x[di] = doorlist_x[doorlocs];
        doorlist_y[di] = doorlist_y[doorlocs];
    }

    free(doorlist_x);
    free(doorlist_y);
}

/**
 * Creates a suitable message for exit from RP.
 * @param RP
 * Parameters to convert to message.
 * @return
 * Newly allocated parameter string. Must be freed.
 */
char *write_map_parameters_to_string(RMParms *RP) {
    StringBuffer *buf = stringbuffer_new();

    stringbuffer_append_printf(buf, "xsize %d\nysize %d\n", RP->Xsize, RP->Ysize);

    if (RP->wallstyle[0]) {
        stringbuffer_append_printf(buf, "wallstyle %s\n", RP->wallstyle);
    }

    if (RP->floorstyle[0]) {
        stringbuffer_append_printf(buf, "floorstyle %s\n", RP->floorstyle);
    }

    if (RP->monsterstyle[0]) {
        stringbuffer_append_printf(buf, "monsterstyle %s\n", RP->monsterstyle);
    }

    if (RP->layoutstyle[0]) {
        stringbuffer_append_printf(buf, "layoutstyle %s\n", RP->layoutstyle);
    }

    if (RP->decorstyle[0]) {
        stringbuffer_append_printf(buf, "decorstyle %s\n", RP->decorstyle);
    }

    if (RP->doorstyle[0]) {
        stringbuffer_append_printf(buf, "doorstyle %s\n", RP->doorstyle);
    }

    if (RP->dungeon_name[0]) {
        stringbuffer_append_printf(buf, "dungeon_name %s\n", RP->dungeon_name);
    }

    if (RP->exitstyle[0]) {
        stringbuffer_append_printf(buf, "exitstyle %s\n", RP->exitstyle);
    }

    if (RP->final_map[0]) {
        stringbuffer_append_printf(buf, "final_map %s\n", RP->final_map);
    }

    if (RP->expand2x) {
        stringbuffer_append_printf(buf, "expand2x %d\n", RP->expand2x);
    }

    if (RP->layoutoptions1) {
        stringbuffer_append_printf(buf, "layoutoptions1 %d\n", RP->layoutoptions1);
    }

    if (RP->layoutoptions2) {
        stringbuffer_append_printf(buf, "layoutoptions2 %d\n", RP->layoutoptions2);
    }

    if (RP->layoutoptions3) {
        stringbuffer_append_printf(buf, "layoutoptions3 %d\n", RP->layoutoptions3);
    }

    if (RP->symmetry) {
        stringbuffer_append_printf(buf, "symmetry %d\n", RP->symmetry);
    }

    if (RP->difficulty && RP->difficulty_given) {
        stringbuffer_append_printf(buf, "difficulty %d\n", RP->difficulty);
    }

    stringbuffer_append_printf(buf, "dungeon_level %d\n", RP->dungeon_level);

    if (RP->dungeon_depth) {
        stringbuffer_append_printf(buf, "dungeon_depth %d\n", RP->dungeon_depth);
    }

    if (RP->decorchance) {
        stringbuffer_append_printf(buf, "decorchance %d\n", RP->decorchance);
    }

    if (RP->orientation) {
        stringbuffer_append_printf(buf, "orientation %d\n", RP->orientation);
    }

    if (RP->random_seed) {
        stringbuffer_append_printf(buf, "random_seed %d\n", RP->random_seed + 1);
    }

    if (RP->num_monsters) {
        stringbuffer_append_printf(buf, "num_monsters %d\n", RP->num_monsters);
    }

    if (RP->darkness) {
        stringbuffer_append_printf(buf, "darkness %d\n", RP->darkness);
    }

    if (RP->level_increment) {
        stringbuffer_append_printf(buf, "level_increment %d\n", RP->level_increment);
    }

    if (RP->bg_music[0]) {
        stringbuffer_append_printf(buf, "bg_music %s\n", RP->bg_music);
    }

    return stringbuffer_finish(buf);
}
