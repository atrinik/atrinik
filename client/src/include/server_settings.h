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
 * Header file for server settings information. */

#ifndef SERVER_SETTINGS_H
#define SERVER_SETTINGS_H

/** Text IDs from server_settings file. */
enum
{
	SERVER_TEXT_STEP0,
	SERVER_TEXT_STEP1,
	SERVER_TEXT_STEP2,

	SERVER_TEXT_STATS,
	SERVER_TEXT_LOGIN,

	SERVER_TEXT_PROTECTION_GROUPS,
	SERVER_TEXT_PROTECTION_LETTERS,
	SERVER_TEXT_PROTECTION_FULL,

	SERVER_TEXT_SPELL_PATHS,

	SERVER_TEXT_MAX
};

/** One character. */
typedef struct char_struct
{
	/** The race name. */
	char *name;

	/** Base HP. */
	uint32 base_hp;

	/** Base SP. */
	uint16 base_sp;

	/** Archetypes of the race's genders. */
	char *gender_archetypes[GENDER_MAX];

	/** Face names of the race's genders. */
	char *gender_faces[GENDER_MAX];

	/** Maximum number of points to assign to stats. */
	uint16 points_max;

	/** Base stats. */
	int stats_base[NUM_STATS];

	/** Minimum values of stats. */
	int stats_min[NUM_STATS];

	/** Maximum values of stats. */
	int stats_max[NUM_STATS];

	/** Description of the race. */
	char *desc;
} char_struct;

/**
 * Server settings structure, initialized from the server_settings srv
 * file. */
typedef struct server_settings
{
	/** Maximum reachable level. */
	uint32 max_level;

	/** Experience needed for each level. */
	sint64 *level_exp;

	/** Races that can be selected to be played. */
	char_struct *characters;

	/** Number of server_settings::characters. */
	size_t num_characters;

	/** Server-configured strings. */
	char *text[SERVER_TEXT_MAX];

	/** Protection few-letter acronyms. */
	char protection_letters[20][MAX_BUF];

	/** Full protection names. */
	char protection_full[20][MAX_BUF];

	/** Spell path names. */
	char spell_paths[SPELL_PATH_NUM][MAX_BUF];
} server_settings;

#endif
