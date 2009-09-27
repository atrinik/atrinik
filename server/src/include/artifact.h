/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
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
 * Artifact related structures */

/** The artifact structure. */
typedef struct artifactstruct
{
	/** Memory block with artifacts parse commands for loader.c */
	char *parse_text;

	/** The fake arch name when chained to arch list */
	const char *name;

	/** We use this as marker for def_at is valid and quick name access */
	const char *def_at_name;

	/** Next artifact in the list */
	struct artifactstruct *next;

	linked_char *allowed;

	/** The base archetype object - this is chained to arch list */
	archetype def_at;

	/** Treasure style. */
	int t_style;

	/** Chance */
	uint16 chance;

	/** Difficulty */
	uint8 difficulty;
} artifact;

/** Artifact list structure. */
typedef struct artifactliststruct
{
	/** Next list */
	struct artifactliststruct *next;

	/** Items in this artifact list */
	struct artifactstruct *items;

	/** Sum of chance for are artifacts on this list */
	uint16 total_chance;

	/**
	 * Object type that this list represents.
	 * -1 are "Allowed none" items. They are called explicit by name */
	sint16 type;
} artifactlist;
