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
 * Race header file. */

#ifndef RACE_H
#define RACE_H

/**
 * A single race. */
typedef struct ob_race {
    /**
     * Name of this race. */
    shstr *name;

    /**
     * The default corpse archetype of this race. */
    struct archt *corpse;

    /**
     * Linked list of monsters belonging to this race. */
    struct oblnk *members;

    /**
     * Number of monsters belonging to this race. */
    int num_members;
} ob_race;

/**
 * The default corpse archetype name, used for races that do not
 * define their own corpse. */
#define RACE_CORPSE_DEFAULT "corpse_default"

/**
 * Marks no race. */
#define RACE_TYPE_NONE 0

/**
 * Number of races in ::item_races. */
#define NROF_ITEM_RACES 13

#endif
