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
 * Effects header file. */

#ifndef EFFECTS_H
#define EFFECT_H

/**
 * @defgroup WIND_BLOW_xxx Wind blow directions
 * Wind blow directions.
 *@{*/
/** No wind blow. */
#define WIND_BLOW_NONE 0
/** Blowing to the left. */
#define WIND_BLOW_LEFT 1
/** Blowing to the right. */
#define WIND_BLOW_RIGHT 2
/** Blowing randomly. */
#define WIND_BLOW_RANDOM 3
/*@}*/

/** One effect definition in a linked list. */
typedef struct effect_struct
{
    /** Next effect in the list. */
    struct effect_struct *next;

    /** Name of this effect. */
    char name[MAX_BUF];

    /**
     * Chance to change the way the wind blows, should be a value between
     * 0.0 (always) and 1.0 (never). Default is 0.98. */
    double wind_chance;

    /**
     * Controls how often to create a new sprite, should be a value between
     * 0.0 (never) and 100.0 (always). Default is 60.0. */
    double sprite_chance;

    /** Start of the currently shown list of sprites. */
    struct effect_sprite *sprites;

    /** End of the currently shown list of sprites. */
    struct effect_sprite *sprites_end;

    /** Linked list of sprite definitions. */
    struct effect_sprite_def *sprite_defs;

    /** Total chance value of all sprites in ::sprite_defs. */
    int chance_total;

    /** Wind modifier. */
    long wind;

    /** Direction this sprite is getting blown into, one of @ref WIND_BLOW_xxx.
     * */
    uint8 wind_blow_dir;

    /**
     * Delay in ticks that must pass until another sprite can be created
     * (regardless of the actual chance to create one). Default is 0. */
    uint32 delay;

    /** When a sprite was last created, in ticks. */
    uint32 delay_ticks;

    /** Maximum number of visible sprites, -1 for infinite (default). */
    int max_sprites;

    /** Wind blow modification (how strongly the wind blows to its direction).
     * */
    double wind_mod;

    /** Sprites per move, defaults to 1. */
    int sprites_per_move;

    /** Sound effect filename. */
    char sound_effect[MAX_BUF];

    /** Volume for the sound effect, defaults to 100 (percent). */
    int sound_volume;

    /**
     * Channel the sound effect is playing on, or -1 if there isn't any
     * sound effect playing. */
    int sound_channel;

    /** Map overlay. */
    struct effect_overlay *overlay;
} effect_struct;

/** One sprite currently shown. */
typedef struct effect_sprite
{
    /** Next sprite. */
    struct effect_sprite *next;

    /** Previous sprite. */
    struct effect_sprite *prev;

    /** Current X position of the sprite. */
    int x;

    /** Current Y position of the sprite. */
    int y;

    /** Last time the sprite was moved, in ticks. */
    uint32 delay_ticks;

    /** When the sprite was created. */
    uint32 created_tick;

    /** Settings of this sprite. */
    struct effect_sprite_def *def;
} effect_sprite;

/** Sprite definition; holds various settings of a single sprite. */
typedef struct effect_sprite_def
{
    /** Next sprite definition in a linked list. */
    struct effect_sprite_def *next;

    /** ID of in-game sprite to use. */
    int id;

    /** Name of the in-game sprite to use. */
    char *name;

    /**
     * Weight of the sprite: affects how fast it falls down, and gets
     * blown away by wind, default is 1.0. */
    double weight;

    /** Weight modification, default is 2.0. */
    double weight_mod;

    /**
     * Chance to use this sprite: the higher the number in comparison to
     * other sprites in the list, the more likely it is to be used; same as
     * artifacts work. 1 by default. */
    int chance;

    /** How long to delay until another movement, in ticks. Default is 0. */
    uint32 delay;

    /** Non-zero value to enable wind simulation. Default is 1. */
    uint8 wind;

    /**
     * How much to wiggle when falling down, 0.0 to fall straight down.
     * Default 1.0. */
    double wiggle;

    /**
     * How much to affect randomization part of wind blowing simulation (0.0 to
     * disable the randomization). Default is 1.0. */
    double wind_mod;

    /** X position of the sprite, -1 for random (default). */
    int x;

    /** Y position of the sprite, -1 for random (default). */
    int y;

    /** X position modifier, 0 by default. */
    int xpos;

    /** Y position modifier, 0 by default. */
    int ypos;

    /** Whether to go from bottom to top, instead of top to bottom, 0 by
     * default. */
    uint8 reverse;

    /**
     * Initial Y starting position; will be randomized between 0-value.
     * Default is 60.0. */
    double y_rndm;

    /** X calculation modification. */
    double x_mod;

    /** Y calculation modification. */
    double y_mod;

    /**
     * Whether to affect side-checking of sprites with the sprite's
     * width, 1 by default. */
    uint8 x_check_mod;

    /**
     * Whether to affect side-checking of sprites with the sprite's
     * height, 1 by default. */
    uint8 y_check_mod;

    /**
     * Whether to kill sprites that go too far to the left (off
     * screen), 1 by default. */
    uint8 kill_side_left;

    /**
     * Whether to kill sprites that go too far to the right (off
     * screen), 0 by default. */
    uint8 kill_side_right;

    /**
     * How much to zoom this sprite, 0 by default (no zoom), 100 = same
     * as current sprite, 200 = twice as big. */
    uint16 zoom;

    /**
     * Warp sprites going too far to the sides (off screen), 1 by default. */
    uint8 warp_sides;

    /** Time to live (in ticks). */
    uint32 ttl;

    /**
     * Sound effect to play each time this sprite is created. By default,
     * no sound is played. */
    char sound_file[MAX_BUF];

    /** Volume of the sound effect to play. Default is 100. */
    uint8 sound_volume;
} effect_sprite_def;

/** One single color in the overlay configuration. */
typedef struct effect_overlay_col
{
    /** Initial value; if -1, will use the original color value. */
    sint16 val;

    /** Modification of the color. */
    double mod[5];

    /** Min random range. */
    uint8 rndm_min;

    /** Max random range. */
    uint8 rndm_max;
} effect_overlay_col;

/**
 * Holds information about effect overlay. */
typedef struct effect_overlay
{
    /** Array of the configuration; R(ed), G(reen), B(lue), A(lpha). */
    effect_overlay_col col[4];
} effect_overlay;

/** Macro used in effect_scale(). */
#define EFFECT_SCALE_ADJUST(i, overlay) \
    (i) = (overlay)->col[idx].val == -1 ? vals[idx] : (overlay)->col[idx].val; \
    (i) += (int) (((double) vals[0] * (overlay)->col[idx].mod[0] + (double) vals[1] * (overlay)->col[idx].mod[1] + (double) vals[2] * (overlay)->col[idx].mod[2] + (double) vals[3] * (overlay)->col[idx].mod[3]) * (overlay)->col[idx].mod[4]); \
    \
    if ((overlay)->col[idx].rndm_max != 0) \
    { \
        (i) += rndm((overlay)->col[idx].rndm_min, (overlay)->col[idx].rndm_max); \
    } \
    \
    (i) = MAX(0, MIN(255, (i))); \
    idx++;

#endif
