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
 * Recipe header file.
 */

#ifndef RECIPE_H
#define RECIPE_H

/** Recipe structure */
typedef struct recipestruct {
    /** Distinguishing name of product */
    const char *title;

    /** The archetype of the final product made */
    const char *arch_name;

    /**
     * Chance that recipe for this item will appear in an alchemical
     * grimoire
     */
    int chance;

    /** An index value derived from formula ingredients */
    int index;

    /**
     * If defined, one of the formula ingredients is used as the basis
     * for the product object
     */
    int transmute;

    /** The maximum number of items produced by the recipe */
    int yield;

    /** Comma delimited list of ingredients */
    shstr_list_t *ingred;

    /** Keycode needed to use the recipe */
    const char *keycode;

    /** Next recipe */
    struct recipestruct *next;
} recipe;

/** Recipe list structure */
typedef struct recipeliststruct {
    /** Total chance */
    int total_chance;

    /** Number of recipes in this list */
    int number;

    /** Pointer to first recipe in this list */
    struct recipestruct *items;

    /** Pointer to next recipe list */
    struct recipeliststruct *next;
} recipelist;

#endif
