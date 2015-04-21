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
 * Handle exit placement in map. */

#include <global.h>

/**
 * Find a character in the layout.
 * @param mode How to look:
 * - <b>1</b>: From top/left to bottom/right.
 * - <b>2</b>: From top/right to bottom/left.
 * - <b>3</b>: From bottom/left to top/right.
 * - <b>4</b>: From bottom/right to top/left.
 * - <b>Other</b>: One random order is chosen.
 * @param target Character to search.
 * @param layout Maze layout.
 * @param RP Random map parameters. */
void find_in_layout(int mode, char target, int *fx, int *fy, char **layout, RMParms *RP)
{
    int M, x, y;

    *fx = -1;
    *fy = -1;

    /* if a starting point isn't given, pick one */
    if (mode < 1 || mode > 4) {
        M = RANDOM() % 4 + 1;
    } else {
        M = mode;
    }

    /* four different search starting points and methods so that
     *    we can do something different for symmetrical maps instead of
     *    the same damned thing every time. */
    switch (M) {
        /* Search from top left down/right */
    case 1:
    {
        for (x = 1; x < RP->Xsize; x++) {
            for (y = 1; y < RP->Ysize; y++) {
                if (layout[x][y] == target) {
                    *fx = x;
                    *fy = y;

                    return;
                }
            }
        }

        break;
    }

        /* Search from top right down/left */
    case 2:
    {
        for (x = RP->Xsize - 2; x > 0; x--) {
            for (y = 1; y < RP->Ysize - 1; y++) {
                if (layout[x][y] == target) {
                    *fx = x;
                    *fy = y;

                    return;
                }
            }
        }

        break;
    }

        /* Search from bottom-left up-right */
    case 3:
    {
        for (x = 1; x < RP->Xsize - 1; x++) {
            for (y = RP->Ysize - 2; y > 0; y--) {
                if (layout[x][y] == target) {
                    *fx = x;
                    *fy = y;

                    return;
                }
            }
        }

        break;
    }

        /* Search from bottom-right up-left */
    case 4:
    {
        for (x = RP->Xsize - 2; x > 0; x--) {
            for (y = RP->Ysize - 2; y > 0; y--) {
                if (layout[x][y] == target) {
                    *fx = x;
                    *fy = y;

                    return;
                }
            }
        }

        break;
    }
    }
}

/**
 * Place exits in the map.
 * @param map Map to put exits into.
 * @param maze Map layout.
 * @param exitstyle What style to use. If NULL uses a random one.
 * @param orientation How exits should be oriented:
 * - <b>0</b>: Random.
 * - <b>1</b>: Descending dungeon.
 * - <b>2</b>: Ascending dungeon.
 * @param RP Random map parameters.
 * @note unblock_exits() should be called at some point, as exits will be
 * blocking everything to avoid putting other objects on them. */
void place_exits(mapstruct *map, char **maze, char *exitstyle, int orientation, RMParms *RP)
{
    mapstruct *style_map_down = NULL, *style_map_up = NULL;
    object *the_exit_down;
    object *the_exit_up;
    /* magic mouth saying this is a random map. */
    object *random_sign;
    int cx = -1, cy = -1;
    int upx = -1, upy = -1;
    int downx = -1, downy = -1, j;

    if (orientation == 0) {
        orientation = RANDOM() % 6 + 1;
    }

    switch (orientation) {
    case 1:
        style_map_up = find_style("/styles/exitstyles/up", exitstyle, -1);
        style_map_down = find_style("/styles/exitstyles/down", exitstyle, -1);

        break;

    case 2:
        style_map_up = find_style("/styles/exitstyles/down", exitstyle, -1);
        style_map_down = find_style("/styles/exitstyles/up", exitstyle, -1);

        break;

    default:
        style_map_up = find_style("/styles/exitstyles/generic", exitstyle, -1);
        style_map_down = style_map_up;

        break;
    }

    if (style_map_up == NULL) {
        the_exit_up = arch_to_object(find_archetype("exit"));
    } else {
        object *tmp = pick_random_object(style_map_up);
        the_exit_up = arch_to_object(tmp->arch);
    }

    /* we need a down exit only if we're recursing. */
    if (RP->dungeon_level < RP->dungeon_depth || RP->final_map[0] != 0) {
        if (style_map_down == NULL) {
            the_exit_down = arch_to_object(find_archetype("exit"));
        } else {
            object *tmp = pick_random_object(style_map_down);
            the_exit_down = arch_to_object(tmp->arch);
        }
    } else {
        the_exit_down = NULL;
    }

    /* Set up the up exit */
    the_exit_up->stats.hp = RP->origin_x;
    the_exit_up->stats.sp = RP->origin_y;
    FREE_AND_COPY_HASH(the_exit_up->slaying, RP->origin_map);

    /* figure out where to put the entrance */

    /* First, look for a '<' char */
    find_in_layout(0, '<', &upx, &upy, maze, RP);

    /* next, look for a C, the map center.  */
    find_in_layout(0, 'C', &cx, &cy, maze, RP);

    /* If we didn't find an up, find an empty place far from the center */
    if (upx == -1 && cx != -1) {
        if (cx > RP->Xsize / 2) {
            upx = 1;
        } else {
            upx = RP->Xsize - 2;
        }

        if (cy > RP->Ysize / 2) {
            upy = 1;
        } else {
            upy = RP->Ysize - 2;
        }

        /* find an empty place far from the center */
        if (upx == 1 && upy == 1) {
            find_in_layout(1, 0, &upx, &upy, maze, RP);
        } else if (upx == 1 && upy > 1) {
            find_in_layout(3, 0, &upx, &upy, maze, RP);
        } else if (upx > 1 && upy == 1) {
            find_in_layout(2, 0, &upx, &upy, maze, RP);
        } else if (upx > 1 && upy > 1) {
            find_in_layout(4, 0, &upx, &upy, maze, RP);
        }
    }

    /* No indication of where to place the exit, so just place it. */
    if (upx == -1) {
        find_in_layout(0, 0, &upx, &upy, maze, RP);
    }

    the_exit_up->x = upx;
    the_exit_up->y = upy;

    /* Surround the exits with notices that this is a random map. */
    for (j = 1; j < 9; j++) {
        if (!wall_blocked(map, the_exit_up->x + freearr_x[j], the_exit_up->y + freearr_y[j])) {
            char buf[MAX_BUF];

            random_sign = get_archetype("sign");
            random_sign->x = the_exit_up->x + freearr_x[j];
            random_sign->y = the_exit_up->y + freearr_y[j];

            snprintf(buf, sizeof(buf), "This is a random map.\nLevel: %d\n", RP->dungeon_level);
            FREE_AND_COPY_HASH(random_sign->msg, buf);
            insert_ob_in_map(random_sign, map, NULL, INS_NO_MERGE | INS_NO_WALK_ON);
        }
    }

    /* Block the exit so things don't get dumped on top of it. */
    SET_FLAG(the_exit_up, FLAG_NO_PASS);
    insert_ob_in_map(the_exit_up, map, NULL, INS_NO_MERGE | INS_NO_WALK_ON);
    maze[the_exit_up->x][the_exit_up->y] = '<';

    /* Set the starting x, y for this map */
    MAP_ENTER_X(map) = the_exit_up->x;
    MAP_ENTER_Y(map) = the_exit_up->y;

    /* First, look for a '>' character */
    find_in_layout(0, '>', &downx, &downy, maze, RP);

    /* If no > is found use C */
    if (downx == -1) {
        downx = cx;
        downy = cy;
    }

    /* make the other exit far away from this one if
     * there's no center. */
    if (downx == -1) {
        if (upx > RP->Xsize / 2) {
            downx = 1;
        } else {
            downx = RP->Xsize - 2;
        }

        if (upy > RP->Ysize / 2) {
            downy = 1;
        } else {
            downy = RP->Ysize - 2;
        }

        /* find an empty place far from the entrance */
        if (downx == 1 && downy == 1) {
            find_in_layout(1, 0, &downx, &downy, maze, RP);
        } else if (downx == 1 && downy > 1) {
            find_in_layout(3, 0, &downx, &downy, maze, RP);
        } else if (downx > 1 && downy == 1) {
            find_in_layout(2, 0, &downx, &downy, maze, RP);
        } else if (downx > 1 && downy > 1) {
            find_in_layout(4, 0, &downx, &downy, maze, RP);
        }
    }

    /* No indication of where to place the down exit, so just place it */
    if (downx == -1) {
        find_in_layout(0, 0, &downx, &downy, maze, RP);
    }

    if (the_exit_down) {
        int i = find_first_free_spot(the_exit_down->arch, NULL, map, downx, downy);

        the_exit_down->x = downx + freearr_x[i];
        the_exit_down->y = downy + freearr_y[i];

        RP->origin_x = the_exit_down->x;
        RP->origin_y = the_exit_down->y;

        /* the identifier for making a random map. */
        if (RP->dungeon_level >= RP->dungeon_depth && RP->final_map[0] != '\0') {
            mapstruct *new_map;
            object *the_exit_back = arch_to_object(the_exit_up->arch), *tmp;

            /* load it */
            if ((new_map = ready_map_name(RP->final_map, NULL, 0)) == NULL) {
                return;
            }

            FREE_AND_COPY_HASH(the_exit_down->slaying, RP->final_map);
            FREE_AND_COPY_HASH(new_map->path, RP->final_map);
            the_exit_down->stats.hp = MAP_ENTER_X(new_map);
            the_exit_down->stats.sp = MAP_ENTER_Y(new_map);

            for (tmp = GET_MAP_OB(new_map, MAP_ENTER_X(new_map), MAP_ENTER_Y(new_map)); tmp; tmp = tmp->above) {
                /* Remove exit back to previous random map.  There should only
                 * be one
                 * which is why we break out.  To try to process more than one
                 * would require keeping a 'next' pointer, ad free_object kills
                 * tmp, which
                 * breaks the for loop. */
                if (tmp->type == EXIT) {
                    object_remove(tmp, 0);
                    object_destroy(tmp);
                    break;
                }
            }

            /* Setup the exit back */
            FREE_AND_ADD_REF_HASH(the_exit_back->slaying, map->path);
            the_exit_back->stats.hp = the_exit_down->x;
            the_exit_back->stats.sp = the_exit_down->y;
            the_exit_back->x = MAP_ENTER_X(new_map);
            the_exit_back->y = MAP_ENTER_Y(new_map);

            insert_ob_in_map(the_exit_back, new_map, NULL, INS_NO_MERGE | INS_NO_WALK_ON);

            /* So it gets swapped out */
            set_map_timeout(new_map);
        } else {
            char buf[2048];

            write_map_parameters_to_string(buf, RP);
            FREE_AND_COPY_HASH(the_exit_down->msg, buf);
            FREE_AND_COPY_HASH(the_exit_down->slaying, "/random/");
            the_exit_down->stats.hp = 0;
            the_exit_down->stats.sp = 0;
        }

        /* Block the exit so things don't get dumped on top of it. */
        SET_FLAG(the_exit_down, FLAG_NO_PASS);

        insert_ob_in_map(the_exit_down, map, NULL, INS_NO_MERGE | INS_NO_WALK_ON);

        maze[the_exit_down->x][the_exit_down->y] = '>';
    }
}

/**
 * This function unblocks the exits. We blocked them to keep things from
 * being dumped on them during the other phases of random map generation.
 * @param map Map to alter.
 * @param maze Map layout.
 * @param RP Random map parameters. */
void unblock_exits(mapstruct *map, char **maze, RMParms *RP)
{
    int x, y;
    object *walk;

    for (x = 0; x < RP->Xsize; x++) {
        for (y = 0; y < RP->Ysize; y++) {
            if (maze[x][y] == '>' || maze[x][y] == '<') {
                for (walk = GET_MAP_OB(map, x, y); walk != NULL; walk = walk->above) {
                    if (QUERY_FLAG(walk, FLAG_NO_PASS) && walk->type != DOOR) {
                        CLEAR_FLAG(walk, FLAG_NO_PASS);
                        update_object(walk, UP_OBJ_FLAGS);
                    }
                }
            }
        }
    }
}
