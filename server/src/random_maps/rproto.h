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

#ifndef __CEXTRACT__
#ifdef __STDC__

extern void dump_layout(char **layout, RMParms *RP);
extern mapstruct *generate_random_map(char *OutFileName, RMParms *RP);
extern char **layoutgen(RMParms *RP);
extern char **symmetrize_layout(char **maze, int sym, RMParms *RP);
extern char ** rotate_layout(char **maze, int rotation, RMParms *RP);
extern void roomify_layout(char **maze, RMParms *RP);
extern int can_make_wall(char **maze, int dx, int dy, int dir, RMParms *RP);
extern int make_wall(char **maze, int x, int y, int dir);
extern void doorify_layout(char **maze, RMParms *RP);
extern void write_map_parameters_to_string(char *buf, RMParms *RP);
extern void write_parameters_to_string(char *buf, int xsize_n, int ysize_n, char *wallstyle_n, char *floorstyle_n, char *monsterstyle_n, char *treasurestyle_n, char *layoutstyle_n, char *decorstyle_n, char *doorstyle_n, char *exitstyle_n, char *final_map_n, char *this_map_n, int layoutoptions1_n, int layoutoptions2_n, int layoutoptions3_n, int symmetry_n, int dungeon_depth_n, int dungeon_level_n, int difficulty_n, int difficulty_given_n, int decoroptions_n, int orientation_n, int origin_x_n, int origin_y_n, int random_seed_n, int treasureoptions_n);
extern void copy_object_with_inv(object *src_ob, object *dest_ob);
extern char **map_gen_onion(int xsize, int ysize, int option, int layers);
extern void centered_onion(char **maze, int xsize, int ysize, int option, int layers);
extern void bottom_centered_onion(char **maze, int xsize, int ysize, int option, int layers);
extern void draw_onion(char **maze, float *xlocations, float *ylocations, int layers);
extern void make_doors(char **maze, float *xlocations, float *ylocations, int layers, int options);
extern void bottom_right_centered_onion(char **maze, int xsize, int ysize, int option, int layers);
extern char **map_gen_spiral(int xsize, int ysize, int option);
extern void connect_spirals(int xsize, int ysize, int sym, char **layout);
extern char **maze_gen(int xsize, int ysize, int option);
extern int rmap_lex_read(RMParms *RP);
extern void rmaprestart(FILE *input_file);
extern void rmap_load_buffer_state(void);
extern int load_parameters(FILE *fp, int bufstate, RMParms *RP);
extern int set_random_map_variable(RMParms *rp, const char *buf);
extern mapstruct *make_map_floor (char *floorstyle, RMParms *RP);
extern int surround_flag(char **layout, int i, int j, RMParms *RP);
extern int surround_flag2(char **layout, int i, int j, RMParms *RP);
extern int surround_flag3(mapstruct *map, int i, int j, RMParms *RP);
extern int surround_flag4(mapstruct *map, int i, int j, RMParms *RP);
extern void make_map_walls(mapstruct *map, char **layout, char *w_style, RMParms *RP);
extern object *pick_joined_wall(object *the_wall, char **layout, int i, int j, RMParms *RP);
extern object * retrofit_joined_wall(mapstruct *the_map, int i, int j, int insert_flag, RMParms *RP);
extern void insert_multisquare_ob_in_map(object *new_obj, mapstruct *map);
extern void place_monsters(mapstruct *map, char *monsterstyle, int difficulty, RMParms *RP);
extern void put_doors(mapstruct *the_map, char **maze, char *doorstyle, RMParms *RP);
extern int obj_count_in_map(mapstruct *map, int x, int y);
extern void put_decor(mapstruct *map, char **maze, char *decorstyle, int decor_option, RMParms *RP);
extern void find_in_layout(int mode, char target, int *fx, int *fy, char **layout, RMParms *RP);
extern void place_exits(mapstruct *map, char **maze, char *exitstyle, int orientation, RMParms *RP);
extern void unblock_exits(mapstruct *map, char **maze, RMParms *RP);
extern int wall_blocked(mapstruct *m, int x, int y);
extern void place_treasure(mapstruct *map, char **layout, char *treasure_style, int treasureoptions, RMParms *RP);
extern object * place_chest(int treasureoptions, int x, int y, mapstruct *map, mapstruct *style_map, int n_treasures, RMParms *RP);
extern object *find_closest_monster(mapstruct *map, int x, int y);
extern int keyplace(mapstruct *map, int x, int y, char *keycode, int door_flag, int n_keys, RMParms *RP);
extern object *find_monster_in_room_recursive(char **layout, mapstruct *map, int x, int y, RMParms *RP);
extern object *find_monster_in_room(mapstruct *map, int x, int y, RMParms *RP);
extern void find_spot_in_room(mapstruct *map, int x, int y, int *kx, int *ky, RMParms *RP);
extern void find_enclosed_spot(mapstruct *map, int *cx, int *cy, RMParms *RP);
extern void remove_monsters(int x, int y, mapstruct *map);
extern object ** surround_by_doors(mapstruct *map,char **maze, int x, int y, int opts);
extern object *door_in_square(mapstruct *map, int x, int y);
extern void find_doors_in_room_recursive(char **layout, mapstruct *map, int x, int y, object **doorlist, int *ndoors, RMParms *RP);
extern object** find_doors_in_room(mapstruct *map, int x, int y, RMParms *RP);
extern void lock_and_hide_doors(object **doorlist, mapstruct *map, int opts, RMParms *RP);
extern void nuke_map_region(mapstruct *map, int xstart, int ystart, int xsize, int ysize);
extern void include_map_in_map(mapstruct *dest_map, mapstruct *in_map, int x, int y);
extern int find_spot_for_submap(mapstruct *map, char **layout, int *ix, int *iy, int xsize, int ysize);
extern void place_fountain_with_specials(mapstruct *map);
extern void place_special_exit(mapstruct * map, int hole_type, RMParms *RP);
extern void place_specials_in_map(mapstruct *map, char **layout, RMParms *RP);
extern int select_regular_files(const struct dirent *the_entry);
extern mapstruct *load_style_map(char *style_name);
extern mapstruct *find_style(char *dirname, char *stylename, int difficulty);
extern object *pick_random_object(mapstruct *style);
extern void free_style_maps(void);
extern int surround_check(char **layout, int i, int j, int Xsize, int Ysize);
extern char **roguelike_layout_gen(int xsize, int ysize, int options);
extern char **make_snake_layout(int xsize, int ysize);
extern void find_top_left_corner(char **maze, int *cx, int *cy);
extern char **make_square_spiral_layout(int xsize, int ysize);
extern char **expand2x(char **layout, int xsize, int ysize);
extern int surround_check2(char **layout, int i, int j, int Xsize, int Ysize);

#endif
#endif
