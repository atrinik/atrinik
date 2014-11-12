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
 * Client settings header files. */

#ifndef SETTINGS_H
#define SETTINGS_H

/**
 * The different option categories.
 * @anchor OPT_CAT_xxx. */
enum
{
    /** General. */
    OPT_CAT_GENERAL,
    /** Client-related. */
    OPT_CAT_CLIENT,
    /** Map-related. */
    OPT_CAT_MAP,
    /** Sound/music. */
    OPT_CAT_SOUND,
    /** Development. */
    OPT_CAT_DEVEL
};

/**
 * @defgroup OPT_xxx Option IDs
 * Options IDs.
 *@{*/

/**
 * Options in the ::OPT_CAT_GENERAL category. */
enum
{
    /** Whether to target self if no target is selected. */
    OPT_TARGET_SELF,
    /** Collect mode. */
    OPT_COLLECT_MODE,
    /** How to display experience. */
    OPT_EXP_DISPLAY,
    /** How to display chat timestamps. */
    OPT_CHAT_TIMESTAMPS,
    /** Maximum number of chat lines. */
    OPT_MAX_CHAT_LINES,
    /** Maximum number of lines in text input history. */
    OPT_MAX_INPUT_HISTORY_LINES,
    /** Widget snap radius. */
    OPT_SNAP_RADIUS
};

/**
 * Options in the ::OPT_CAT_CLIENT category. */
enum
{
    /** Resolution. */
    OPT_RESOLUTION,
    /** Fullscreen enabled? */
    OPT_FULLSCREEN,
    /** Smooth zoom enabled? */
    OPT_ZOOM_SMOOTH,
    /** Speed of key repeat. */
    OPT_KEY_REPEAT_SPEED,
    /** FPS limit. */
    OPT_FPS_LIMIT,
    /** Whether to disable server file updates. */
    OPT_DISABLE_FILE_UPDATES,
    /** Minimize latency at the expense of outgoing bandwidth. */
    OPT_MINIMIZE_LATENCY,
    /** Whether to allow dragging widgets off-screen. */
    OPT_OFFSCREEN_WIDGETS,
    /** Background color of text windows. */
    OPT_TEXT_WINDOW_BG_COLOR,
    /** Transparency of text windows. */
    OPT_TEXT_WINDOW_TRANSPARENCY,

    /** Internal: stores the current resolution width. */
    OPT_RESOLUTION_X,
    /** Internal: stores the current resolution height. */
    OPT_RESOLUTION_Y
};

/**
 * Options in the ::OPT_CAT_MAP category. */
enum
{
    /** Which player names to show. */
    OPT_PLAYER_NAMES,
    /** How much to zoom the map. */
    OPT_MAP_ZOOM,
    /** Health warning. */
    OPT_HEALTH_WARNING,
    /** Food warning. */
    OPT_FOOD_WARNING,
    /** Map width in tiles. */
    OPT_MAP_WIDTH,
    /** Map height in tiles. */
    OPT_MAP_HEIGHT
};

/**
 * Options in the ::OPT_CAT_SOUND category. */
enum
{
    /** Music volume. */
    OPT_VOLUME_MUSIC,
    /** Sound volume. */
    OPT_VOLUME_SOUND,
    /** Use 3D (angular) sound effects. */
    OPT_3D_SOUNDS
};

/**
 * Options in the ::OPT_CAT_DEVEL category. */
enum
{
    /** Whether to show FPS. */
    OPT_SHOW_FPS,
    /** Whether to always try to reload graphics from gfx_user directory. */
    OPT_RELOAD_GFX,
    /** Whether to disable /region_map cache. */
    OPT_DISABLE_RM_CACHE,
    /** Whether to enable the operator interface. */
    OPT_OPERATOR
};

/*@}*/

/**
 * Various setting types.
 * @anchor OPT_TYPE_xxx */
enum
{
    /** Bool (checkbox). */
    OPT_TYPE_BOOL,
    /** Number input. */
    OPT_TYPE_INPUT_NUM,
    /** Text input. */
    OPT_TYPE_INPUT_TEXT,
    /** Range. */
    OPT_TYPE_RANGE,
    /** Select - text options. */
    OPT_TYPE_SELECT,
    /** Integer - internal type. */
    OPT_TYPE_INT,
    /** Color picker. */
    OPT_TYPE_COLOR,

    /** Number of the different options. */
    OPT_TYPE_NUM
};

/** Range setting data. */
typedef struct setting_range
{
    /** Min value for the setting. */
    sint64 min;

    /** Max value for the setting. */
    sint64 max;

    /** How much to advance by each time. */
    sint64 advance;
} setting_range;

/**
 * Select setting - contains a list of text options the user may choose
 * from. */
typedef struct setting_select
{
    /** Array of the options. */
    char **options;

    /** Number of options. */
    size_t options_len;
} setting_select;

/**
 * A single setting. */
typedef struct setting_struct
{
    /** Name of the setting. */
    char *name;

    /** Description of the setting. */
    char *desc;

    /** Type of the setting - one of @ref SETTING_TYPE_xxx. */
    uint8 type;

    /** Whether the setting is internal, and should not be shown to the user. */
    uint8 internal;

    /** Custom data; for settings like select, range, etc. */
    void *custom_attrset;

    /** Setting value. */
    union
    {
        /** String value. */
        char *str;

        /** Integer value. */
        sint64 i;
    } val;
} setting_struct;

/**
 * One setting category. */
typedef struct setting_category
{
    /** Name of the category. */
    char *name;

    /** All the settings this category contains. */
    setting_struct **settings;

    /** Number of the settings. */
    size_t settings_num;
} setting_category;

/** Macro to get ::setting_select structure from ::setting_struct. */
#define SETTING_SELECT(_setting) ((setting_select *) (_setting)->custom_attrset)
/** Macro to get ::setting_range structure from ::setting_struct. */
#define SETTING_RANGE(_setting) ((setting_range *) (_setting)->custom_attrset)

/**
 * Step in the keybinding settings. */
enum
{
    /** Entering a command. */
    KEYBIND_STEP_COMMAND,
    /** Entering a shortcut. */
    KEYBIND_STEP_KEY,
    /** Done, waiting for the user to press Apply button. */
    KEYBIND_STEP_DONE
};

#endif
