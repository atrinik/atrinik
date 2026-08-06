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
 * This file contains core skill handling.
 */

#include <global.h>
#include <book.h>
#include <skillist.h>
#include <object.h>
#include <player.h>
#include <rune.h>

typedef enum trap_search_result {
    TRAP_SEARCH_NONE,
    TRAP_SEARCH_FOUND,
    TRAP_SEARCH_SIGNS
} trap_search_result;

/**
 * Search an object's inventory for traps.
 *
 * @param pl
 * Player searching.
 * @param where
 * Object whose inventory is searched.
 * @param search_level
 * Effective trap-search level.
 * @return
 * Best search result found in the inventory.
 */
static trap_search_result find_traps_in_object(object *pl, object *where, int search_level) {
    trap_search_result result = TRAP_SEARCH_NONE;

    FOR_INV_PREPARE(where, trap) {
        if (trap->type != RUNE) {
            continue;
        }

        if (trap_see(pl, trap, search_level)) {
            trap_show(trap, where);
            result = TRAP_SEARCH_FOUND;
        } else if (result == TRAP_SEARCH_NONE &&
                   MAX(trap->level, trap->stats.Int) <= search_level * 1.8f) {
            result = TRAP_SEARCH_SIGNS;
        }
    }
    FOR_INV_FINISH();

    return result;
}

/**
 * Disarm all discovered traps in an object's inventory.
 *
 * @param pl
 * Player disarming the traps.
 * @param where
 * Object whose inventory is checked.
 */
static tag_t remove_traps_from_object(object *pl, object *where) {
    FOR_INV_PREPARE(where, trap) {
        if (trap->type != RUNE || trap->stats.Int > 1) {
            continue;
        }

        if (QUERY_FLAG(trap, FLAG_SYS_OBJECT) || QUERY_FLAG(trap, FLAG_IS_INVISIBLE)) {
            trap_show(trap, where);
        }

        int result = trap_disarm(pl, trap);
        if (result != TRAP_DISARM_SUCCESS) {
            return result == TRAP_DISARM_TRIPPED ? trap->count : 0;
        }
    }
    FOR_INV_FINISH();

    return 0;
}

/**
 * Search a container for a trap without disarming it.
 *
 * @param pl
 * Player searching the container.
 * @param container
 * Container to search.
 * @return
 * True if at least one trap was discovered.
 */
bool traps_detect_in_container(object *pl, object *container) {
    HARD_ASSERT(pl != NULL);
    HARD_ASSERT(container != NULL);

    if (pl->type != PLAYER || CONTR(pl)->skill_ptr[SK_FIND_TRAPS] == NULL) {
        return false;
    }

    return find_traps_in_object(pl, container, trap_skill_rating(pl, SK_FIND_TRAPS)) ==
           TRAP_SEARCH_FOUND;
}

/**
 * Automatically search for and disarm traps before a player opens a container.
 *
 * The attempt is made only when the player knows both required skills. Failed
 * detection or disarming retains the normal container-opening behavior, which
 * can still spring the remaining trap.
 *
 * @param pl
 * Player opening the container.
 * @param container
 * Container about to be opened.
 */
tag_t traps_auto_disarm(object *pl, object *container) {
    HARD_ASSERT(pl != NULL);
    HARD_ASSERT(container != NULL);

    if (pl->type != PLAYER || CONTR(pl)->skill_ptr[SK_FIND_TRAPS] == NULL ||
        CONTR(pl)->skill_ptr[SK_REMOVE_TRAPS] == NULL) {
        return 0;
    }

    if (QUERY_FLAG(container, FLAG_IS_TRAPPED)) {
        return remove_traps_from_object(pl, container);
    }

    if (traps_detect_in_container(pl, container)) {
        return remove_traps_from_object(pl, container);
    }

    return 0;
}

/**
 * Checks for traps on the spaces around the player or in certain
 * objects.
 * @param pl
 * Player searching.
 */
void find_traps(object *pl) {
    int suc = TRAP_SEARCH_NONE;
    int search_level = trap_skill_rating(pl, SK_FIND_TRAPS);

    /* First we search all around us for runes and traps, which are
     * all type RUNE */
    for (int i = 0; i < 9; i++) {
        /* Check everything in the square for trapness */
        int xt = pl->x + freearr_x[i];
        int yt = pl->y + freearr_y[i];

        mapstruct *m = get_map_from_coord(pl->map, &xt, &yt);
        if (m == NULL) {
            continue;
        }

        for (object *tmp = GET_MAP_OB(m, xt, yt); tmp != NULL; tmp = tmp->above) {
            /* And now we'd better do an inventory traversal of each
             * of these objects' inventory */
            if (pl != tmp && (tmp->type == PLAYER || tmp->type == MONSTER)) {
                continue;
            }

            trap_search_result result = find_traps_in_object(pl, tmp, search_level);
            if (result == TRAP_SEARCH_FOUND) {
                suc = TRAP_SEARCH_FOUND;
            } else if (suc == TRAP_SEARCH_NONE && result == TRAP_SEARCH_SIGNS) {
                suc = TRAP_SEARCH_SIGNS;
            }

            if (tmp->type == RUNE) {
                if (trap_see(pl, tmp, search_level)) {
                    trap_show(tmp, tmp);
                    suc = TRAP_SEARCH_FOUND;
                } else {
                    /* Give out a "we have found signs of traps"
                     * if the traps level is not 1.8 times higher. */
                    if (suc == TRAP_SEARCH_NONE &&
                        MAX(tmp->level, tmp->stats.Int) <= search_level * 1.8f) {
                        suc = TRAP_SEARCH_SIGNS;
                    }
                }
            }
        }
    }

    if (suc == TRAP_SEARCH_NONE) {
        draw_info(COLOR_WHITE, pl, "You can't detect any trap here.");
    } else if (suc == TRAP_SEARCH_SIGNS) {
        draw_info(COLOR_WHITE, pl, "You detect trap signs!");
    }
}

/**
 * This skill will disarm any previously discovered trap.
 * @param op
 * Player disarming.
 */
void remove_trap(object *op) {
    for (int i = 0; i < 9; i++) {
        int x = op->x + freearr_x[i];
        int y = op->y + freearr_y[i];

        mapstruct *m = get_map_from_coord(op->map, &x, &y);
        if (m == NULL) {
            continue;
        }

        /* Check everything in the square for trapness */
        for (object *tmp = GET_MAP_OB(m, x, y); tmp != NULL; tmp = tmp->above) {
            /* And now we'd better do an inventory traversal of each
             * of these objects' inventory */
            FOR_INV_PREPARE(tmp, trap) {
                if (trap->type == RUNE && trap->stats.Int <= 1) {
                    if (QUERY_FLAG(trap, FLAG_SYS_OBJECT) || QUERY_FLAG(trap, FLAG_IS_INVISIBLE)) {
                        trap_show(trap, tmp);
                    }

                    trap_disarm(op, trap);
                    return;
                }
            }
            FOR_INV_FINISH();

            if (tmp->type == RUNE && tmp->stats.Int <= 1) {
                if (QUERY_FLAG(tmp, FLAG_SYS_OBJECT) || QUERY_FLAG(tmp, FLAG_IS_INVISIBLE)) {
                    trap_show(tmp, tmp);
                }

                trap_disarm(op, tmp);
                return;
            }
        }
    }

    draw_info(COLOR_WHITE, op, "There is no trap to remove nearby.");
}
