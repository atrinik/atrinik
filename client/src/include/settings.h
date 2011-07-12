/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
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

#ifndef SETTINGS_H
#define SETTINGS_H

enum
{
	SETTING_TYPE_NONE,
	SETTING_TYPE_SETTINGS,
	SETTING_TYPE_KEYBINDINGS,
	SETTING_TYPE_PASSWORD
};

enum
{
	OPT_CAT_GENERAL,
	OPT_CAT_CLIENT,
	OPT_CAT_MAP,
	OPT_CAT_SOUND,
	OPT_CAT_DEVEL
};

enum
{
	OPT_PLAYERDOLL,
	OPT_TARGET_SELF,
	OPT_COLLECT_MODE,
	OPT_EXP_DISPLAY,
	OPT_CHAT_TIMESTAMPS,
	OPT_MAX_CHAT_LINES,
	OPT_SNAP_RADIUS
};

enum
{
	OPT_RESOLUTION,
	OPT_FULLSCREEN,
	OPT_MAP_ZOOM_SMOOTH,
	OPT_KEY_REPEAT_SPEED,
	OPT_SLEEP_TIME,
	OPT_DISABLE_FILE_UPDATES,
	OPT_MINIMIZE_LATENCY,
	OPT_OFFSCREEN_WIDGETS,

	OPT_RESOLUTION_X,
	OPT_RESOLUTION_Y
};

enum
{
	OPT_PLAYER_NAMES,
	OPT_MAP_ZOOM,
	OPT_HEALTH_WARNING,
	OPT_FOOD_WARNING,
	OPT_MAP_WIDTH,
	OPT_MAP_HEIGHT
};

enum
{
	OPT_VOLUME_MUSIC,
	OPT_VOLUME_SOUND
};

enum
{
	OPT_SHOW_FPS,
	OPT_RELOAD_GFX,
	OPT_DISABLE_RM_CACHE,
	OPT_QUICKPORT
};

enum
{
	OPT_TYPE_BOOL,
	OPT_TYPE_INPUT_NUM,
	OPT_TYPE_INPUT_TEXT,
	OPT_TYPE_RANGE,
	OPT_TYPE_SELECT,
	OPT_TYPE_INT,

	OPT_TYPE_NUM
};

typedef struct setting_range
{
	sint64 min;

	sint64 max;

	sint64 advance;
} setting_range;

typedef struct setting_select
{
	char **options;

	size_t options_len;
} setting_select;

typedef struct setting_struct
{
	char *name;

	char *desc;

	uint8 type;

	uint8 internal;

	void *custom_attrset;

	union
	{
		char *str;

		sint64 i;
	} val;
} setting_struct;

typedef struct setting_category
{
	char *name;

	setting_struct **settings;

	size_t settings_num;
} setting_category;

#define SETTING_SELECT(_setting) ((setting_select *) (_setting)->custom_attrset)
#define SETTING_RANGE(_setting) ((setting_range *) (_setting)->custom_attrset)

#endif
