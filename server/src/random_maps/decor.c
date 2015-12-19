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
 * These functions handle decoration in random maps.
 */

#include <global.h>

/**
 * Put the decor into the map.
 * @param map
 * Map to add decor to.
 * @param layout
 * Layout of the map, as was generated.
 * @param RP
 * Parameters of the random map.
 */
void put_decor(mapstruct *map, char **layout, RMParms *RP)
{
    mapstruct *decor_map;
    char style_name[256];
    int i, j;
    object *new_decor_object;

    if (RP->decorstyle[0] == '\0' || !strcmp(RP->decorstyle, "none")) {
        return;
    }

    snprintf(style_name, sizeof(style_name), "/styles/decorstyles");
    decor_map = find_style(style_name, RP->decorstyle, -1);

    if (decor_map == NULL) {
        return;
    }

    for (i = 0; i < RP->Xsize - 1; i++) {
        for (j = 0; j < RP->Ysize - 1; j++) {
            if (RP->decorchance > 0 && !rndm_chance(RP->decorchance)) {
                continue;
            }

            new_decor_object = pick_random_object(decor_map);

            if (layout[i][j] == (new_decor_object->type == WALL && QUERY_FLAG(new_decor_object, FLAG_IS_TURNABLE) ? '#' : '\0')) {
                object *this_object = get_object();

                copy_object(new_decor_object, this_object, 0);
                this_object->x = i;
                this_object->y = j;

                if (new_decor_object->type == WALL && QUERY_FLAG(new_decor_object, FLAG_IS_TURNABLE) && surround_flag2(layout, i, j, RP) & (4 | 8)) {
                    this_object->direction = 7;
                    SET_ANIMATION(this_object, (NUM_ANIMATIONS(this_object) / NUM_FACINGS(this_object)) * this_object->direction);
                }

                insert_ob_in_map(this_object, map, NULL, INS_NO_MERGE | INS_NO_WALK_ON);
            }
        }
    }
}
