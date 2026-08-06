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
 * Random map related variables.
 */

#ifndef RANDOM_MAP_H
#define RANDOM_MAP_H

#define RM_SIZE 512

/** Random map parameters. */
typedef struct {
    /** Wall style used. */
    char wallstyle[RM_SIZE];

    /** Wall name. */
    char wall_name[RM_SIZE];

    /** Floor style used. */
    char floorstyle[RM_SIZE];

    /** Monster style used. */
    char monsterstyle[RM_SIZE];

    /** Layout style. */
    char layoutstyle[RM_SIZE];

    /** Door style. */
    char doorstyle[RM_SIZE];

    /** Decoration style. */
    char decorstyle[RM_SIZE];

    /** Original map. */
    char origin_map[RM_SIZE];

    /** Final map. */
    char final_map[RM_SIZE];

    /** Exit style (stairs, portals, etc). */
    char exitstyle[RM_SIZE];

    /** Name of the dungeon. */
    char dungeon_name[RM_SIZE];

    /** Background music. */
    char bg_music[RM_SIZE];

    /** X size of the map. */
    int Xsize;

    /** Y size of the map. */
    int Ysize;

    /** Whether to expand twice. */
    int expand2x;

    /** First layout options. */
    int layoutoptions1;

    /** Second layout options. */
    int layoutoptions2;

    /** Third layout options. */
    int layoutoptions3;

    /** Symmetry. */
    int symmetry;

    /** Difficulty of the dungeon. */
    int difficulty;

    /** Was the difficulty given? */
    int difficulty_given;

    /** Level of the dungeon. */
    int dungeon_level;

    /** Depth of the dungeon. */
    int dungeon_depth;

    /** Chance to add decoration. */
    int decorchance;

    /** Orientation. */
    int orientation;

    /** Origin Y. */
    int origin_y;

    /** Origin X. */
    int origin_x;

    /** Random seed value. */
    int random_seed;

    /** Deterministic stream used only while generating this map. */
    rng_state_t rng;

    /** Map layout style. */
    int map_layout_style;

    /** Symmetry used. */
    int symmetry_used;

    /** Number of monsters to generate. */
    int num_monsters;

    /** Darkness of the dungeon. */
    int darkness;

    /** Level increment. */
    int level_increment;
} RMParms;

/**
 * @defgroup RM_LAYOUT Random map layout
 *@{*/
#define ONION_LAYOUT 1
#define MAZE_LAYOUT 2
#define SPIRAL_LAYOUT 3
#define ROGUELIKE_LAYOUT 4
#define SNAKE_LAYOUT 5
#define SQUARE_SPIRAL_LAYOUT 6
#define NROFLAYOUTS 6
/*@}*/

/**
 * @defgroup OPT_xxx Random map layout options.
 *@{*/
/** Random option. */
#define OPT_RANDOM 0
/** Centered. */
#define OPT_CENTERED 1
/** Linear doors (default is nonlinear). */
#define OPT_LINEAR 2
/** Bottom-centered. */
#define OPT_BOTTOM_C 4
/** Bottom-right centered. */
#define OPT_BOTTOM_R 8
/** Irregularly/randomly spaced layers (default: regular). */
#define OPT_IRR_SPACE 16
/** No outer wall. */
#define OPT_WALL_OFF 32
/** Only walls. */
#define OPT_WALLS_ONLY 64
/**< Place walls instead of doors.  Produces broken map. */
#define OPT_NO_DOORS 256
/*@}*/

/**
 * @defgroup SYM_xxx Random map symmetry
 * Symmetry definitions -- used in this file AND in @ref treasure.c, the
 * numerical values matter so don't change them.
 *@{*/

/** Random symmetry. */
#define RANDOM_SYM 0
/** No symmetry. */
#define NO_SYM 1
/** Vertical symmetry. */
#define X_SYM 2
/** Horizontal symmetry. */
#define Y_SYM 3
/** Reflection. */
#define XY_SYM 4
/*@}*/

/** Public API implemented in src/loaders/random_map.c. */

extern int rmap_lex_read(RMParms *RP);

extern int load_parameters(FILE *fp, int bufstate, RMParms *RP);

extern int set_random_map_variable(RMParms *rp, const char *buf);

extern void free_random_map_loader(void);

/** Public API implemented in src/random_maps/decor.c. */

extern void put_decor(mapstruct *map, char **layout, RMParms *RP);

/** Public API implemented in src/random_maps/door.c. */

extern int surround_check2(char **layout, int x, int y, int Xsize, int Ysize);

extern void put_doors(mapstruct *the_map, char **maze, char *doorstyle, RMParms *RP);

/** Public API implemented in src/random_maps/exit.c. */

extern void find_in_layout(int mode, char target, int *fx, int *fy, char **layout, RMParms *RP);

extern void place_exits(mapstruct *map, char **maze, char *exitstyle, int orientation, RMParms *RP);

extern void unblock_exits(mapstruct *map, char **maze, RMParms *RP);

/** Public API implemented in src/random_maps/expand2x.c. */

extern char **expand2x(char **layout, int xsize, int ysize);

/** Public API implemented in src/random_maps/floor.c. */

extern mapstruct *make_map_floor(char *floorstyle, RMParms *RP);

/** Public API implemented in src/random_maps/maze_gen.c. */

extern char **maze_gen(int xsize, int ysize, int option, rng_state_t *rng);

/** Public API implemented in src/random_maps/monster.c. */

extern void place_monsters(mapstruct *map, char *monsterstyle, int difficulty, RMParms *RP);

/** Public API implemented in src/random_maps/random_map.c. */

extern void dump_layout(char **layout, RMParms *RP);

extern mapstruct *generate_random_map(char *OutFileName, RMParms *RP);

extern char **layoutgen(RMParms *RP);

extern char **symmetrize_layout(char **maze, int sym, RMParms *RP);

extern char **rotate_layout(char **maze, int rotation, RMParms *RP);

extern void roomify_layout(char **maze, RMParms *RP);

extern int can_make_wall(char **maze, int dx, int dy, int dir, RMParms *RP);

extern int make_wall(char **maze, int x, int y, int dir);

extern void doorify_layout(char **maze, RMParms *RP);

extern char *write_map_parameters_to_string(RMParms *RP);

/** Public API implemented in src/random_maps/rogue_layout.c. */

extern int surround_check(char **layout, int i, int j, int Xsize, int Ysize);

extern char **roguelike_layout_gen(int xsize, int ysize, int options, rng_state_t *rng);

/** Public API implemented in src/random_maps/room_gen_onion.c. */

extern char **map_gen_onion(int xsize, int ysize, int option, int layers, rng_state_t *rng);

extern void
centered_onion(char **maze, int xsize, int ysize, int option, int layers, rng_state_t *rng);

extern void
bottom_centered_onion(char **maze, int xsize, int ysize, int option, int layers, rng_state_t *rng);

extern void draw_onion(char **maze, float *xlocations, float *ylocations, int layers);

extern void make_doors(char **maze,
                       float *xlocations,
                       float *ylocations,
                       int layers,
                       int options,
                       rng_state_t *rng);

extern void bottom_right_centered_onion(char **maze,
                                        int xsize,
                                        int ysize,
                                        int option,
                                        int layers,
                                        rng_state_t *rng);

/** Public API implemented in src/random_maps/room_gen_spiral.c. */

extern char **map_gen_spiral(int xsize, int ysize, int option, rng_state_t *rng);

extern void connect_spirals(int xsize, int ysize, int sym, char **layout);

/** Public API implemented in src/random_maps/snake.c. */

extern char **make_snake_layout(int xsize, int ysize, rng_state_t *rng);

/** Public API implemented in src/random_maps/square_spiral.c. */

extern void find_top_left_corner(char **maze, int *cx, int *cy);

extern char **make_square_spiral_layout(int xsize, int ysize, rng_state_t *rng);

/** Public API implemented in src/random_maps/style.c. */

extern int load_dir(const char *dir, char ***namelist, int skip_dirs);

extern mapstruct *styles;

extern mapstruct *load_style_map(char *style_name);

extern mapstruct *
find_style(const char *dirname, const char *stylename, int difficulty, rng_state_t *rng);

extern object *pick_random_object(mapstruct *style, rng_state_t *rng);

extern void free_style_maps(void);

/** Public API implemented in src/random_maps/wall.c. */

extern int surround_flag(char **layout, int i, int j, RMParms *RP);

extern int surround_flag2(char **layout, int i, int j, RMParms *RP);

extern int surround_flag3(mapstruct *map, int i, int j, RMParms *RP);

extern int surround_flag4(mapstruct *map, int i, int j, RMParms *RP);

extern void make_map_walls(mapstruct *map, char **layout, char *w_style, RMParms *RP);

extern object *pick_joined_wall(object *the_wall, char **layout, int i, int j, RMParms *RP);

extern object *retrofit_joined_wall(mapstruct *the_map, int i, int j, int insert_flag, RMParms *RP);

#endif
