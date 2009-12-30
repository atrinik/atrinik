/* random_map.c */
extern void dump_layout(char **layout, RMParms *RP);
extern mapstruct *generate_random_map(char *OutFileName, RMParms *RP);
extern char **layoutgen(RMParms *RP);
extern char **symmetrize_layout(char **maze, int sym, RMParms *RP);
extern char **rotate_layout(char **maze, int rotation, RMParms *RP);
extern void roomify_layout(char **maze, RMParms *RP);
extern int can_make_wall(char **maze, int dx, int dy, int dir, RMParms *RP);
extern int make_wall(char **maze, int x, int y, int dir);
extern void doorify_layout(char **maze, RMParms *RP);
extern void write_map_parameters_to_string(char *buf, RMParms *RP);

/* room_gen_onion.c */
extern char **map_gen_onion(int xsize, int ysize, int option, int layers);
extern void centered_onion(char **maze, int xsize, int ysize, int option, int layers);
extern void bottom_centered_onion(char **maze, int xsize, int ysize, int option, int layers);
extern void draw_onion(char **maze, float *xlocations, float *ylocations, int layers);
extern void make_doors(char **maze, float *xlocations, float *ylocations, int layers, int options);
extern void bottom_right_centered_onion(char **maze, int xsize, int ysize, int option, int layers);

/* room_gen_spiral.c */
extern char **map_gen_spiral(int xsize, int ysize, int option);
extern void connect_spirals(int xsize, int ysize, int sym, char **layout);

/* maze_gen.c */
extern char **maze_gen(int xsize, int ysize, int option);

/* reader.c */
extern int rmap_lex_read(RMParms *RP);
extern void rmaprestart(FILE *input_file);
extern void rmappop_buffer_state();
extern int rmapget_lineno();
extern FILE *rmapget_in();
extern FILE *rmapget_out();
extern int rmapget_leng();
extern char *rmapget_text();
extern void rmapset_lineno(int line_number);
extern void rmapset_in(FILE *in_str);
extern void rmapset_out(FILE *out_str);
extern int rmapget_debug();
extern void rmapset_debug(int bdebug);
extern int rmaplex_destroy();
extern void rmapfree(void *ptr);
extern int load_parameters(FILE *fp, int bufstate, RMParms *RP);
extern int set_random_map_variable(RMParms *rp, const char *buf);

/* floor.c */
extern mapstruct *make_map_floor(char *floorstyle, RMParms *RP);

/* wall.c */
extern int surround_flag(char **layout, int i, int j, RMParms *RP);
extern int surround_flag2(char **layout, int i, int j, RMParms *RP);
extern int surround_flag3(mapstruct *map, int i, int j, RMParms *RP);
extern int surround_flag4(mapstruct *map, int i, int j, RMParms *RP);
extern void make_map_walls(mapstruct *map, char **layout, char *w_style, RMParms *RP);
extern object *pick_joined_wall(object *the_wall, char **layout, int i, int j, RMParms *RP);
extern object *retrofit_joined_wall(mapstruct *the_map, int i, int j, int insert_flag, RMParms *RP);

/* monster.c */
extern void insert_multisquare_ob_in_map(object *new_obj, mapstruct *map);
extern void place_monsters(mapstruct *map, char *monsterstyle, int difficulty, RMParms *RP);

/* door.c */
extern int surround_check2(char **layout, int x, int y, int Xsize, int Ysize);
extern void put_doors(mapstruct *the_map, char **maze, char *doorstyle, RMParms *RP);

/* decor.c */
extern void put_decor(mapstruct *map, char **layout, RMParms *RP);

/* exit.c */
extern void find_in_layout(int mode, char target, int *fx, int *fy, char **layout, RMParms *RP);
extern void place_exits(mapstruct *map, char **maze, char *exitstyle, int orientation, RMParms *RP);
extern void unblock_exits(mapstruct *map, char **maze, RMParms *RP);

/* style.c */
extern int load_dir(const char *dir, char ***namelist, int skip_dirs);
extern mapstruct *load_style_map(char *style_name);
extern mapstruct *find_style(char *dirname, char *stylename, int difficulty);
extern object *pick_random_object(mapstruct *style);
extern void free_style_maps();

/* rogue_layout.c */
extern int surround_check(char **layout, int i, int j, int Xsize, int Ysize);
extern char **roguelike_layout_gen(int xsize, int ysize, int options);

/* snake.c */
extern char **make_snake_layout(int xsize, int ysize);

/* square_spiral.c */
extern void find_top_left_corner(char **maze, int *cx, int *cy);
extern char **make_square_spiral_layout(int xsize, int ysize);

/* expand2x.c */
extern char **expand2x(char **layout, int xsize, int ysize);
