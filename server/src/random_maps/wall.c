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
 * Deals with wall management in random maps. */

#include <global.h>

/**
 * Given a layout and a coordinate, tell me which squares
 * up/down/right/left are occupied.
 * @param RP Random map parameters.
 * @return Combination of:
 * - <b>1</b>: Something on left.
 * - <b>2</b>: Something on right.
 * - <b>4</b>: Something on above.
 * - <b>8</b>: Something on below. */
int surround_flag(char **layout, int i, int j, RMParms *RP)
{
    int surround_index = 0;

    if ((i > 0) && layout[i - 1][j] != '\0') {
        surround_index |= 1;
    }

    if ((i < RP->Xsize - 1) && layout[i + 1][j] != '\0') {
        surround_index |= 2;
    }

    if ((j > 0) && layout[i][j - 1] != '\0') {
        surround_index |= 4;
    }

    if ((j < RP->Ysize - 1) && layout[i][j + 1] != '\0') {
        surround_index |= 8;
    }

    return surround_index;
}

/**
 * Given a layout and a coordinate, tell me which squares
 * up/down/right/left are occupied by walls or doors.
 * @param RP Random map parameters.
 * @return Combination of:
 * - <b>1</b>: Wall/door on left.
 * - <b>2</b>: Wall/door on right.
 * - <b>4</b>: Wall/door on above.
 * - <b>8</b>: Wall/door on below. */
int surround_flag2(char **layout, int i, int j, RMParms *RP)
{
    int surround_index = 0;

    if ((i > 0) && (layout[i - 1][j] == '#' || layout[i - 1][j] == 'D')) {
        surround_index |= 1;
    }

    if ((i < RP->Xsize - 1) && (layout[i + 1][j] == '#' || layout[i + 1][j] == 'D')) {
        surround_index |= 2;
    }

    if ((j > 0) && (layout[i][j - 1] == '#' || layout[i][j - 1] == 'D')) {
        surround_index |= 4;
    }

    if ((j < RP->Ysize - 1) && (layout[i][j + 1] == '#' || layout[i][j + 1] == 'D')) {
        surround_index |= 8;
    }

    return surround_index;
}

/**
 * Check a map for blocked spots.
 *
 * Since this is part of the random map code, presumption is that this
 * is not a tiled map.
 *
 * What is considered blocking and not is somewhat hard coded.
 * @param RP Random map parameters.
 * @return Combination of:
 * - <b>1</b>: Blocked on left.
 * - <b>2</b>: Blocked on right.
 * - <b>4</b>: Blocked on above.
 * - <b>8</b>: Blocked on below. */
int surround_flag3(mapstruct *map, int i, int j, RMParms *RP)
{
    int surround_index = 0;

    if ((i > 0) && blocked(NULL, map, i -1, j, TERRAIN_ALL)) {
        surround_index |= 1;
    }

    if ((i < RP->Xsize - 1) && blocked(NULL, map, i + 1, j, TERRAIN_ALL)) {
        surround_index |= 2;
    }

    if ((j > 0) && blocked(NULL, map, i, j - 1, TERRAIN_ALL)) {
        surround_index |= 4;
    }

    if ((j < RP->Ysize - 1) && blocked(NULL, map, i, j + 1, TERRAIN_ALL)) {
        surround_index |= 8;
    }

    return surround_index;
}

/**
 * Check a map for spots with walls.
 *
 * Since this is part of the random map code, presumption is that this
 * is not a tiled map.
 *
 * What is considered blocking and not is somewhat hard coded.
 * @param RP Random map parameters.
 * @return Combination of:
 * - <b>1</b>: Wall on left.
 * - <b>2</b>: Wall on right.
 * - <b>4</b>: Wall on above.
 * - <b>8</b>: Wall on below. */
int surround_flag4(mapstruct *map, int i, int j, RMParms *RP)
{
    int surround_index = 0;

    if ((i > 0) && wall_blocked(map, i - 1, j)) {
        surround_index |= 1;
    }

    if ((i < RP->Xsize - 1) && wall_blocked(map, i + 1, j)) {
        surround_index |= 2;
    }

    if ((j > 0) && wall_blocked(map, i, j- 1)) {
        surround_index |= 4;
    }

    if ((j < RP->Ysize - 1) && wall_blocked(map, i, j + 1)) {
        surround_index |= 8;
    }

    return surround_index;
}

/**
 * Takes a map and a layout, and puts walls in the map (picked from
 * w_style) at '#' marks.
 * @param map Map where walls will be put.
 * @param layout Layout containing walls and such.
 * @param w_style Wall style.
 * @param RP Random map parameters. */
void make_map_walls(mapstruct *map, char **layout, char *w_style, RMParms *RP)
{
    char styledirname[256], stylefilepath[256];
    mapstruct *style_map = NULL;
    object *the_wall;

    /* get the style map */
    if (!strcmp(w_style, "none")) {
        return;
    }

    strncpy(styledirname, "/styles/wallstyles", sizeof(styledirname) - 1);

    snprintf(stylefilepath, sizeof(stylefilepath), "%s/%s", styledirname, w_style);
    style_map = find_style(styledirname, w_style, -1);

    if (style_map == 0) {
        return;
    }

    /* Fill up the map with the given wall style */
    if ((the_wall = pick_random_object(style_map)) != NULL) {
        int i, j;
        char *cp;

        snprintf(RP->wall_name, sizeof(RP->wall_name), "%s", the_wall->arch->name);

        if ((cp = strchr(RP->wall_name, '_')) != NULL) {
            *cp = '\0';
        }

        for (i = 0; i < RP->Xsize; i++) {
            for (j = 0; j < RP->Ysize; j++) {
                if (layout[i][j] == '#') {
                    object *thiswall = pick_joined_wall(the_wall, layout, i, j, RP);

                    thiswall->x = i;
                    thiswall->y = j;

                    /* Make SURE it's a wall */
                    SET_FLAG(thiswall, FLAG_NO_PASS);

                    wall(map, i, j);
                    insert_ob_in_map(thiswall, map, thiswall, INS_NO_MERGE | INS_NO_WALK_ON);
                }
            }
        }
    }
}

/**
 * Picks the right wall type for this square, to make it look nice, and
 * have everything nicely joined.
 *
 * It uses the layout.
 * @param the_wall Wall we want to insert.
 * @param RP Random map parameters.
 * @return Correct wall archetype to fit on the square. */
object *pick_joined_wall(object *the_wall, char **layout, int i, int j, RMParms *RP)
{
    int surround_index = 0, l;
    char wall_name[MAX_BUF];
    archetype *wall_arch = 0;

    strncpy(wall_name, the_wall->arch->name, sizeof(wall_name) - 1);

    /* conventionally, walls are named like this:
     *     wallname_wallcode, where wallcode indicates
     *     a joinedness, and wallname is the wall.
     *     this code depends on the convention for
     *     finding the right wall. */

    /* extract the wall name, which is the text up to the leading _ */
    for (l = sizeof(wall_name); l >= 0; l--) {
        if (wall_name[l] == '_') {
            wall_name[l] = '\0';
            break;
        }
    }

    surround_index = surround_flag2(layout, i, j, RP);

    switch (surround_index) {
        case 0:
            strcat(wall_name, "_0");
            break;

        case 10:
        case 8:
        case 2:
            strcat(wall_name, "_8");
            break;

        case 11:
        case 9:
        case 3:
            strcat(wall_name, "_1");
            break;

        case 12:
        case 4:
        case 14:
        case 6:
            strcat(wall_name, "_3");
            break;

        case 1:
        case 5:
        case 7:
        case 13:
        case 15:
            strcat(wall_name, "_4");
            break;
    }

    wall_arch = find_archetype(wall_name);

    if (wall_arch) {
        return arch_to_object(wall_arch);
    }
    else {
        return arch_to_object(the_wall->arch);
    }
}

/**
 * This takes a map, and changes an existing wall to match what's blocked
 * around it, counting only doors and walls as blocked.
 * @param insert_flag If 1, insert the correct wall into the map,
 * otherwise don't insert.
 * @param RP Random map parameters.
 * @return Correct wall for spot.
 * @todo Merge with pick_joined_wall()? */
object * retrofit_joined_wall(mapstruct *the_map, int i, int j, int insert_flag, RMParms *RP)
{
    int surround_index = 0, l;
    object *the_wall = NULL, *new_wall = NULL;
    archetype *wall_arch = NULL;

    /* First find the wall */
    for (the_wall = GET_MAP_OB(the_map, i, j); the_wall != NULL; the_wall = the_wall->above) {
        if (QUERY_FLAG(the_wall, FLAG_NO_PASS) && the_wall->type != EXIT && the_wall->type != TELEPORTER) {
            break;
        }
    }

    /* if what we found is a door, don't remove it, set the_wall to NULL to
     *     signal that later. */
    if (the_wall && the_wall->type == DOOR) {
        the_wall = NULL;

        /* if we're not supposed to insert a new wall where there wasn't one,
         *     we've gotta leave. */
        if (insert_flag == 0) {
            return NULL;
        }
    }
    else if (the_wall == NULL) {
        return NULL;
    }

    /* Canonicalize the wall name */
    for (l = 0; l < 64; l++) {
        if (RP->wall_name[l] == '_') {
            RP->wall_name[l] = 0;
            break;
        }
    }

    surround_index = surround_flag4(the_map, i, j, RP);

    switch (surround_index) {
        case 0:
            strcat(RP->wall_name, "_0");
            break;

        case 10:
        case 8:
        case 2:
            strcat(RP->wall_name, "_8");
            break;

        case 11:
        case 9:
        case 3:
            strcat(RP->wall_name, "_1");
            break;

        case 12:
        case 4:
        case 14:
        case 6:
            strcat(RP->wall_name, "_3");
            break;

        case 1:
        case 5:
        case 7:
        case 13:
        case 15:
            strcat(RP->wall_name, "_4");
            break;
    }

    wall_arch = find_archetype(RP->wall_name);

    if (wall_arch != NULL) {
        new_wall = arch_to_object(wall_arch);
        new_wall->x = i;
        new_wall->y = j;

        if (the_wall && the_wall->map) {
            object_remove(the_wall, 0);
            object_destroy(the_wall);
        }

        /* Make SURE it's a wall */
        SET_FLAG(new_wall, FLAG_NO_PASS);
        insert_ob_in_map(new_wall, the_map, new_wall, INS_NO_MERGE | INS_NO_WALK_ON);
    }

    return new_wall;
}
