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
 * The onion room generator.
 *
 * Onion rooms are like this:
 *
 * regular                       random
 *
 * centered, linear onion        bottom/right centered, nonlinear
 *
 * #########################     #########################
 * #                       #     #                       #
 * # ########  ##########  #     #   #####################
 * # #                  #  #     #   #                   #
 * # # ######  ######## #  #     #   #                   #
 * # # #              # #  #     #   #   ######## ########
 * # # # ####  ###### # #  #     #   #   #               #
 * # # # #          # # #  #     #   #   #               #
 * # # # ############ # #  #     #   #   #  ########### ##
 * # # #              # #  #     #   #   #  #            #
 * # # ################ #  #     #   #   #  #    #########
 * # #                  #  #     #       #  #    #       #
 * # ####################  #     #   #   #  #            #
 * #                       #     #   #   #  #    #       #
 * #########################     ######################### */

#include <global.h>

void centered_onion(char **maze, int xsize, int ysize, int option, int layers);
void bottom_centered_onion(char **maze, int xsize, int ysize, int option, int layers);
void bottom_right_centered_onion(char **maze, int xsize, int ysize, int option, int layers);
void draw_onion(char **maze, float *xlocations, float *ylocations, int layers);
void make_doors(char **maze, float *xlocations, float *ylocations, int layers, int options);

/**
 * Generates an onion layout.
 * @param xsize X size of the layout.
 * @param ysize Y size of the layout.
 * @param option Combination of @ref OPT_xxx "OPT_xxx" values.
 * @param layers Number of layers the onion should have.
 * @return The generated layout. */
char **map_gen_onion(int xsize, int ysize, int option, int layers)
{
    int i, j;

    /* Allocate that array, set it up */
    char **maze = ecalloc(sizeof(char *), xsize);

    for (i = 0; i < xsize; i++) {
        maze[i] = ecalloc(sizeof(char), ysize);
    }

    /* Pick some random options if option = 0 */
    if (option == 0) {
        switch (RANDOM() % 3) {
        case 0:
            option |= OPT_CENTERED;
            break;

        case 1:
            option |= OPT_BOTTOM_C;
            break;

        case 2:
            option |= OPT_BOTTOM_R;
            break;
        }

        if (RANDOM() % 2) {
            option |= OPT_LINEAR;
        }

        if (RANDOM() % 2) {
            option |= OPT_IRR_SPACE;
        }
    }

    /* Write the outer walls, if appropriate. */
    if (!(option & OPT_WALL_OFF)) {
        for (i = 0; i < xsize; i++) {
            maze[i][0] = maze[i][ysize - 1] = '#';
        }

        for (j = 0; j < ysize; j++) {
            maze[0][j] = maze[xsize - 1][j] = '#';
        }
    }

    if (option & OPT_WALLS_ONLY) {
        return maze;
    }

    /* pick off the mutually exclusive options */
    if (option & OPT_BOTTOM_R) {
        bottom_right_centered_onion(maze, xsize, ysize, option, layers);
    } else if (option & OPT_BOTTOM_C) {
        bottom_centered_onion(maze, xsize, ysize, option, layers);
    } else if (option & OPT_CENTERED) {
        centered_onion(maze, xsize, ysize, option, layers);
    }

    return maze;
}

/**
 * Creates a centered onion layout.
 * @param maze Maze layout.
 * @param xsize X size of the layout.
 * @param ysize Y size of the layout.
 * @param option Combination of @ref OPT_xxx "OPT_xxx" values.
 * @param layers Number of layers to create. */
void centered_onion(char **maze, int xsize, int ysize, int option, int layers)
{
    int i, maxlayers;
    float *xlocations, *ylocations;

    maxlayers = (MIN(xsize, ysize) - 2) / 5;

    /* map too small to onionize */
    if (!maxlayers) {
        return;
    }

    if (layers > maxlayers) {
        layers = maxlayers;
    }

    if (layers == 0) {
        layers = (RANDOM() % maxlayers) + 1;
    }

    xlocations = ecalloc(sizeof(float), 2 * layers);
    ylocations = ecalloc(sizeof(float), 2 * layers);

    /* Place all the walls */

    /* Randomly spaced */
    if (option & OPT_IRR_SPACE) {
        int x_spaces_available, y_spaces_available;

        /* the "extra" spaces available for spacing between layers */
        x_spaces_available = (xsize - 2) - 6 * layers + 1;
        y_spaces_available = (ysize - 2) - 6 * layers + 1;

        /* Pick an initial random pitch */
        for (i = 0; i < 2 * layers; i++) {
            float xpitch = 2, ypitch = 2;

            if (x_spaces_available > 0) {
                xpitch = 2.0f + (float) (RANDOM() % x_spaces_available + RANDOM() % x_spaces_available + RANDOM() % x_spaces_available) / 3.0f;
            }

            if (y_spaces_available > 0) {
                ypitch = 2.0f + (float) (RANDOM() % y_spaces_available + RANDOM() % y_spaces_available + RANDOM() % y_spaces_available) / 3.0f;
            }

            xlocations[i] = ((i > 0) ? xlocations[i - 1] : 0) + xpitch;
            ylocations[i] = ((i > 0) ? ylocations[i - 1] : 0) + ypitch;

            x_spaces_available -= (int) xpitch - 2;
            y_spaces_available -= (int) ypitch - 2;
        }
    }

    /* evenly spaced */
    if (!(option & OPT_IRR_SPACE)) {
        /* pitch of the onion layers */
        float xpitch, ypitch;

        xpitch = (float) (xsize - 2) / (2.0f * (float) (layers + 1));
        ypitch = (float) (ysize - 2) / (2.0f * (float) (layers + 1));

        xlocations[0] = xpitch;
        ylocations[0] = ypitch;

        for (i = 1; i < 2 * layers; i++) {
            xlocations[i] = xlocations[i - 1] + xpitch;
            ylocations[i] = ylocations[i - 1] + ypitch;
        }
    }

    /* draw all the onion boxes.  */
    draw_onion(maze, xlocations, ylocations, layers);
    make_doors(maze, xlocations, ylocations, layers, option);
}

/**
 * Create a bottom-centered layout.
 * @param maze Maze layout.
 * @param xsize X size of the layout.
 * @param ysize Y size of the layout.
 * @param option Combination of @ref OPT_xxx "OPT_xxx" values.
 * @param layers Number of layers to create. */
void bottom_centered_onion(char **maze, int xsize, int ysize, int option, int layers)
{
    int i, maxlayers;
    float *xlocations, *ylocations;

    maxlayers = (MIN(xsize, ysize) - 2) / 5;

    /* map too small to onionize */
    if (!maxlayers) {
        return;
    }

    if (layers > maxlayers) {
        layers = maxlayers;
    }

    if (layers == 0) {
        layers = (RANDOM() % maxlayers) + 1;
    }

    xlocations = ecalloc(sizeof(float), 2 * layers);
    ylocations = ecalloc(sizeof(float), 2 * layers);

    /* place all the walls */

    /* randomly spaced */
    if (option & OPT_IRR_SPACE) {
        int x_spaces_available, y_spaces_available;

        /* the "extra" spaces available for spacing between layers */
        x_spaces_available = (xsize - 2) - 6 * layers + 1;
        y_spaces_available = (ysize - 2) - 3 * layers + 1;

        /* Pick an initial random pitch */
        for (i = 0; i < 2 * layers; i++) {
            float xpitch = 2, ypitch = 2;

            if (x_spaces_available > 0) {
                xpitch = 2.0f + (float) (RANDOM() % x_spaces_available + RANDOM() % x_spaces_available + RANDOM() % x_spaces_available) / 3.0f;
            }

            if (y_spaces_available > 0) {
                ypitch = 2.0f + (float) (RANDOM() % y_spaces_available + RANDOM() % y_spaces_available + RANDOM() % y_spaces_available) / 3.0f;
            }

            xlocations[i] = ((i > 0) ? xlocations[i - 1] : 0) + xpitch;

            if (i < layers) {
                ylocations[i] = ((i > 0) ? ylocations[i - 1] : 0) + ypitch;
            } else {
                ylocations[i] = (float) (ysize - 1);
            }

            x_spaces_available -= (int) xpitch - 2;
            y_spaces_available -= (int) ypitch - 2;
        }
    }

    /* evenly spaced */
    if (!(option & OPT_IRR_SPACE)) {
        /* pitch of the onion layers */
        float xpitch, ypitch;

        xpitch = (float) (xsize - 2) / (2.0f * (float) (layers + 1));
        ypitch = (float) (ysize - 2) / (float) (layers + 1);

        xlocations[0] = xpitch;
        ylocations[0] = ypitch;

        for (i = 1; i < 2 * layers; i++) {
            xlocations[i] = xlocations[i - 1] + xpitch;

            if (i < layers) {
                ylocations[i] = ylocations[i - 1] + ypitch;
            } else {
                ylocations[i] = (float) (ysize - 1);
            }
        }
    }

    /* draw all the onion boxes.  */
    draw_onion(maze, xlocations, ylocations, layers);
    make_doors(maze, xlocations, ylocations, layers, option);
}

/**
 * Draws the lines in the maze defining the onion layers.
 * @param maze Nap to draw to.
 * @param xlocations Array of X coordinate locations.
 * @param ylocations Array of Y coordinate locations.
 * @param layers Number of layers. */
void draw_onion(char **maze, float *xlocations, float *ylocations, int layers)
{
    int i, j, l;

    for (l = 0; l < layers; l++) {
        int x, x2, y, y2;

        /* horizontal segments */
        y = (int) ylocations[l];
        y2 = (int) ylocations[2 * layers - l - 1];

        for (i = (int) xlocations[l]; i <= (int) xlocations[2 * layers - l - 1]; i++) {
            maze[i][y] = '#';
            maze[i][y2] = '#';
        }

        /* vertical segments */
        x = (int) xlocations[l];
        x2 = (int) xlocations[2 * layers - l - 1];

        for (j = (int) ylocations[l]; j <= (int) ylocations[2 * layers - l - 1]; j++) {
            maze[x][j] = '#';
            maze[x2][j] = '#';
        }
    }
}

/**
 * Add doors to the layout.
 * @param maze Map to draw to.
 * @param xlocations Array of X coordinate locations.
 * @param ylocations Array of Y coordinate locations.
 * @param layers Number of layers.
 * @param options Combination of @ref OPT_xxx "OPT_xxx" values. */
void make_doors(char **maze, float *xlocations, float *ylocations, int layers, int options)
{
    /* number of different walls on which we could place a door */
    int freedoms;
    /* left, 1, top, 2, right, 3, bottom 4 */
    int which_wall;
    int l, x = 0, x2, y = 0, y2;

    /* centered */
    freedoms = 4;

    if (options & OPT_BOTTOM_C) {
        freedoms = 3;
    }

    if (options & OPT_BOTTOM_R) {
        freedoms = 2;
    }

    if (layers <= 0) {
        return;
    }

    /* Pick which wall will have a door. */
    which_wall = RANDOM() % freedoms + 1;

    for (l = 0; l < layers; l++) {
        /* linear door placement. */
        if (options & OPT_LINEAR) {
            switch (which_wall) {
                /* Left hand wall */
            case 1:
                x = (int) xlocations[l];
                y = (int) ((ylocations[l] + ylocations[2 * layers - l - 1]) / 2);

                break;

                /* Top wall placement */
            case 2:
                x = (int) ((xlocations[l] + xlocations[2 * layers - l - 1]) / 2);
                y = (int) ylocations[l];

                break;

                /* Right wall placement */
            case 3:
                x = (int) xlocations[2 * layers - l - 1];
                y = (int) ((ylocations[l] + ylocations[2 * layers - l - 1]) / 2);

                break;

                /* Bottom wall placement */
            case 4:
                x = (int) ((xlocations[l] + xlocations[2 * layers - l - 1]) / 2);
                y = (int) ylocations[2 * layers - l - 1];

                break;
            }
        } else {
            /* random door placement. */

            which_wall = RANDOM() % freedoms + 1;

            switch (which_wall) {
                /* Left hand wall */
            case 1:
                x = (int) xlocations[l];
                y2 = (int) (ylocations[2 * layers - l - 1] - ylocations[l] - 1.0f);

                if (y2 > 0) {
                    y = (int) ylocations[l] + RANDOM() % y2 + 1;
                } else {
                    y = (int) ylocations[l] + 1;
                }

                break;

                /* Top wall placement */
            case 2:
                x2 = (int) ((-xlocations[l] + xlocations[2 * layers - l - 1])) - 1;

                if (x2 > 0) {
                    x = (int) xlocations[l] + RANDOM() % x2 + 1;
                } else {
                    x = (int) xlocations[l] + 1;
                }

                y = (int) ylocations[l];

                break;

                /* Right wall placement */
            case 3:
                x = (int) xlocations[2 * layers - l - 1];
                y2 = (int) ((-ylocations[l] + ylocations[2 * layers - l - 1])) - 1;

                if (y2 > 0) {
                    y = (int) ylocations[l] + RANDOM() % y2 + 1;
                } else {
                    y = (int) ylocations[l] + 1;
                }

                break;

                /* Bottom wall placement */
            case 4:
                x2 = (int) ((-xlocations[l] + xlocations[2 * layers - l - 1])) - 1;

                if (x2 > 0) {
                    x = (int) xlocations[l] + RANDOM() % x2 + 1;
                } else {
                    x = (int) xlocations[l] + 1;
                }

                y = (int) ylocations[2 * layers - l - 1];

                break;
            }
        }

        if (options & OPT_NO_DOORS) {
            /* no door. */
            maze[x][y] = '#';
        } else {
            /* write the door */
            maze[x][y] = 'D';
        }
    }

    /* mark the center of the maze with a C */
    l = layers - 1;
    x = (int) (xlocations[l] + xlocations[2 * layers - l - 1]) / 2;
    y = (int) (ylocations[l] + ylocations[2 * layers - l - 1]) / 2;

    maze[x][y] = 'C';

    /* Not needed anymore */
    efree(xlocations);
    efree(ylocations);
}

/**
 * Create a bottom-right-centered layout.
 * @param maze Maze layout.
 * @param xsize X size of the layout.
 * @param ysize Y size of the layout.
 * @param option Combination of @ref OPT_xxx "OPT_xxx" values.
 * @param layers Number of layers to create. */
void bottom_right_centered_onion(char **maze, int xsize, int ysize, int option, int layers)
{
    int i, maxlayers;
    float *xlocations, *ylocations;

    maxlayers = (MIN(xsize, ysize) - 2) / 5;

    /* map too small to onionize */
    if (!maxlayers) {
        return;
    }

    if (layers > maxlayers) {
        layers = maxlayers;
    }

    if (layers == 0) {
        layers = (RANDOM() % maxlayers) + 1;
    }

    xlocations = ecalloc(sizeof(float), 2 * layers);
    ylocations = ecalloc(sizeof(float), 2 * layers);

    /* place all the walls */

    /* randomly spaced */
    if (option & OPT_IRR_SPACE) {
        int x_spaces_available, y_spaces_available;

        /* the "extra" spaces available for spacing between layers */
        x_spaces_available = (xsize - 2) - 3 * layers + 1;
        y_spaces_available = (ysize - 2) - 3 * layers + 1;

        /* Pick an initial random pitch */
        for (i = 0; i < 2 * layers; i++) {
            float xpitch = 2, ypitch = 2;

            if (x_spaces_available > 0) {
                xpitch = 2.0f + (float) (RANDOM() % x_spaces_available + RANDOM() % x_spaces_available + RANDOM() % x_spaces_available) / 3.0f;
            }

            if (y_spaces_available > 0) {
                ypitch = 2.0f + (float) (RANDOM() % y_spaces_available + RANDOM() % y_spaces_available + RANDOM() % y_spaces_available) / 3.0f;
            }

            if (i < layers) {
                xlocations[i] = ((i > 0) ? xlocations[i - 1] : 0) + xpitch;
            } else {
                xlocations[i] = (float) (xsize - 1);
            }

            if (i < layers) {
                ylocations[i] = ((i > 0) ? ylocations[i - 1] : 0) + ypitch;
            } else {
                ylocations[i] = (float) (ysize - 1);
            }

            x_spaces_available -= (int) xpitch - 2;
            y_spaces_available -= (int) ypitch - 2;
        }

    }

    /* evenly spaced */
    if (!(option & OPT_IRR_SPACE)) {
        /* pitch of the onion layers */
        float xpitch, ypitch;

        xpitch = (float) (xsize - 2) / (float) (2 * layers + 1);
        ypitch = (float) (ysize - 2) / (float) (layers + 1);

        xlocations[0] = xpitch;
        ylocations[0] = ypitch;

        for (i = 1; i < 2 * layers; i++) {
            if (i < layers) {
                xlocations[i] = xlocations[i - 1] + xpitch;
            } else {
                xlocations[i] = (float) (xsize - 1);
            }

            if (i < layers) {
                ylocations[i] = ylocations[i - 1] + ypitch;
            } else {
                ylocations[i] = (float) (ysize - 1);
            }
        }
    }

    /* draw all the onion boxes.  */
    draw_onion(maze, xlocations, ylocations, layers);
    make_doors(maze, xlocations, ylocations, layers, option);
}
