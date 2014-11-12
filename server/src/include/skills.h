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
 * Skills header. */

#ifndef SKILLS_H
#define SKILLS_H

/** Skill numbers. */
enum skillnrs
{
    /** Player can attempt alchemical recipes. */
    SK_ALCHEMY,
    /** Can read books */
    SK_LITERACY,
    /** Sells items to shops at Cha + level-based bonus (30 max) */
    SK_BARGAINING,
    /** Player is able to alter maps like apartment in real-time. */
    SK_CONSTRUCTION,

    /** Can attack hand-to-hand */
    SK_UNARMED,
    /** @deprecated */
    SK_KARATE,
    /** Player can throw items */
    SK_THROWING,

    /** Player can cast magic spells */
    SK_WIZARDRY_SPELLS,
    /** Player use wands/horns/rods */
    SK_MAGIC_DEVICES,
    /** @deprecated */
    SK_PRAYING,

    /** Player can find traps better */
    SK_FIND_TRAPS,
    /** Player can remove traps */
    SK_REMOVE_TRAPS,

    /** Player can use bows. */
    SK_BOW_ARCHERY,
    /** Player can use crossbows. */
    SK_CROSSBOW_ARCHERY,
    /** Player can use slings. */
    SK_SLING_ARCHERY,

    /** Player can use slash weapons. */
    SK_SLASH_WEAPONS,
    /** Player can use cleave weapons. */
    SK_CLEAVE_WEAPONS,
    /** Player can use pierce weapons. */
    SK_PIERCE_WEAPONS,
    /** Player can attack with impact weapons */
    SK_IMPACT_WEAPONS,

    /** Player can use two-handed weapons. */
    SK_TWOHANDS,
    /** Player can use polearms. */
    SK_POLEARMS,

    /** Inscription. */
    SK_INSCRIPTION,

    /** Number of the skills, always last. */
    NROFSKILLS
};

/** Skill structure for the skills array. */
typedef struct skill_struct
{
    /** How to describe it to the player */
    char *name;

    /** Pointer to the skill archetype in the archlist */
    archetype *at;

    /** Base number of ticks it takes to use the skill */
    short time;
} skill_struct;

#endif
