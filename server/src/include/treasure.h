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
 * Defines and variables used by the artifact generation routines. */

#ifndef TREASURE_H
#define TREASURE_H

/** Chance for an item to become artifact if not magical. */
#define CHANCE_FOR_ARTIFACT 20

/** Number of coin types */
#define NUM_COINS 4

/**
 * @defgroup GT_xxx Treasure generation flags
 * Treasure generation flags.
 *@{*/
/**
 * Put generated treasure below the object instead of inside the object's
 * inventory. */
#define GT_ENVIRONMENT 0x0001
#define GT_INVISIBLE 0x0002
/** Generated items have ::FLAG_STARTEQUIP set */
#define GT_STARTEQUIP 0x0004
/** Unused. */
#define GT_APPLY 0x0008
/** Don't generate bad/cursed items. Used for new player's equipment. */
#define GT_ONLY_GOOD 0x0010
/** When object has been generated, send its information to player. */
#define GT_UPDATE_INV 0x0020
/** Set value of all created treasures to 0. */
#define GT_NO_VALUE 0x0040
/*@}*/

/**
 * When a treasure got cloned from archlist, we want to perhaps change
 * some default values. All values in this structure will override the
 * default arch. */
typedef struct _change_arch
{
    /** If not NULL, copy this over the original arch name. */
    const char *name;

    /** If not NULL, copy this over the original arch title. */
    const char *title;

    /** If not NULL, copy this over the original arch slaying. */
    const char *slaying;

    /** Item's race */
    int item_race;

    /** The real, fixed material value */
    int material;

    /** Find a material matching this quality */
    int material_quality;

    /** Using ::material_quality, find quality inside this range */
    int material_range;

    /** Quality value. It overwrites the material default value */
    int quality;

    /** Used for random quality range */
    int quality_range;
} _change_arch;

/**
 * Treasure is one element in a linked list, which together consists of a
 * complete treasurelist.
 *
 * Any arch can point to a treasurelist to get generated standard
 * treasure when an archetype of that type is generated. */
typedef struct treasurestruct
{
    /** Which item this link can be */
    struct archt *item;

    /** If not NULL, name of list to use instead */
    const char *name;

    /** Next treasure item in a linked list */
    struct treasurestruct *next;

    /** If this item was generated, use this link instead of ->next */
    struct treasurestruct *next_yes;

    /** If this item was not generated, then continue here */
    struct treasurestruct *next_no;

    /**
     * Local t_style (will overrule global one) - used from artifacts.
     * @see @ref treasure_style */
    int t_style;

    /** Value from 0 - 1000. Chance of item is magic. */
    int magic_chance;

    /**
     * If this value is not 0, use this as fixed magic value.
     * if it 0, look at magic to generate perhaps a random magic value */
    int magic_fix;

    /**
     * Chance to generate artifact from this.
     *
     * Possible values of artifact chance:
     *
     * - <b>-1</b>: Ignore this value (<b>default</b>)
     * - <b>0</b>: NEVER make an artifact for this treasure.
     * - <b>1 - 100</b>: % chance of making an artifact from this
     *   treasure. */
    int artifact_chance;

    /** Max magic bonus to item */
    int magic;

    /**
     * If the entry is a list transition, it contains the difficulty
     * required to go to the new list */
    int difficulty;

    /** Random 1 to nrof items are generated */
    uint16 nrof;

    /** Will overrule chance: if set (not -1) it will create 1 / chance_single
     * */
    sint16 chance_fix;

    /** Percent chance for this item */
    uint8 chance;

    /** Override default arch values if set in treasure list */
    struct _change_arch change_arch;
} treasure;

/** Treasure list structure */
typedef struct treasureliststruct
{
    /** Usually monster name/combination */
    const char *name;

    /**
     * Global style (used from artifacts file).
     * @see @ref treasure_style */
    int t_style;

    /** Artifact chance */
    int artifact_chance;

    /** If set, this will overrule total_chance. */
    sint16 chance_fix;

    /**
     * If non zero, only 1 item on this list should be generated.
     *
     * The total_chance contains the sum of the chance for this list. */
    sint16 total_chance;

    /** Next treasure-item in linked list */
    struct treasureliststruct *next;

    /** Items in this list, linked */
    struct treasurestruct *items;
} treasurelist;

#endif
