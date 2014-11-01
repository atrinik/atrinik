#ifndef __CPROTO__
/* src/commands/permission/arrest.c */
extern void command_arrest(object *op, const char *command, char *params);
/* src/commands/permission/ban.c */
extern void command_ban(object *op, const char *command, char *params);
/* src/commands/permission/follow.c */
extern void command_follow(object *op, const char *command, char *params);
/* src/commands/permission/freeze.c */
extern void command_freeze(object *op, const char *command, char *params);
/* src/commands/permission/kick.c */
extern void command_kick(object *op, const char *command, char *params);
/* src/commands/permission/mod_chat.c */
extern void command_mod_chat(object *op, const char *command, char *params);
/* src/commands/permission/no_chat.c */
extern void command_no_chat(object *op, const char *command, char *params);
/* src/commands/permission/opsay.c */
extern void command_opsay(object *op, const char *command, char *params);
/* src/commands/permission/resetmap.c */
extern void command_resetmap(object *op, const char *command, char *params);
/* src/commands/permission/server_chat.c */
extern void command_server_chat(object *op, const char *command, char *params);
/* src/commands/permission/settime.c */
extern void command_settime(object *op, const char *command, char *params);
/* src/commands/permission/shutdown.c */
extern void command_shutdown(object *op, const char *command, char *params);
/* src/commands/permission/tcl.c */
extern void command_tcl(object *op, const char *command, char *params);
/* src/commands/permission/tgm.c */
extern void command_tgm(object *op, const char *command, char *params);
/* src/commands/permission/tli.c */
extern void command_tli(object *op, const char *command, char *params);
/* src/commands/permission/tls.c */
extern void command_tls(object *op, const char *command, char *params);
/* src/commands/permission/tp.c */
extern void command_tp(object *op, const char *command, char *params);
/* src/commands/permission/tphere.c */
extern void command_tphere(object *op, const char *command, char *params);
/* src/commands/permission/tsi.c */
extern void command_tsi(object *op, const char *command, char *params);
/* src/commands/player/afk.c */
extern void command_afk(object *op, const char *command, char *params);
/* src/commands/player/apply.c */
extern void command_apply(object *op, const char *command, char *params);
/* src/commands/player/chat.c */
extern void command_chat(object *op, const char *command, char *params);
/* src/commands/player/drop.c */
extern void command_drop(object *op, const char *command, char *params);
/* src/commands/player/gsay.c */
extern void command_gsay(object *op, const char *command, char *params);
/* src/commands/player/hiscore.c */
extern void command_hiscore(object *op, const char *command, char *params);
/* src/commands/player/left.c */
extern void command_left(object *op, const char *command, char *params);
/* src/commands/player/me.c */
extern void command_me(object *op, const char *command, char *params);
/* src/commands/player/motd.c */
extern void command_motd(object *op, const char *command, char *params);
/* src/commands/player/my.c */
extern void command_my(object *op, const char *command, char *params);
/* src/commands/player/party.c */
extern void command_party(object *op, const char *command, char *params);
/* src/commands/player/push.c */
extern void command_push(object *op, const char *command, char *params);
/* src/commands/player/region_map.c */
extern void command_region_map(object *op, const char *command, char *params);
/* src/commands/player/rename.c */
extern void command_rename(object *op, const char *command, char *params);
/* src/commands/player/reply.c */
extern void command_reply(object *op, const char *command, char *params);
/* src/commands/player/right.c */
extern void command_right(object *op, const char *command, char *params);
/* src/commands/player/say.c */
extern void command_say(object *op, const char *command, char *params);
/* src/commands/player/statistics.c */
extern void command_statistics(object *op, const char *command, char *params);
/* src/commands/player/take.c */
extern void command_take(object *op, const char *command, char *params);
/* src/commands/player/tell.c */
extern void command_tell(object *op, const char *command, char *params);
/* src/commands/player/time.c */
extern void command_time(object *op, const char *command, char *params);
/* src/commands/player/version.c */
extern void command_version(object *op, const char *command, char *params);
/* src/commands/player/whereami.c */
extern void command_whereami(object *op, const char *command, char *params);
/* src/commands/player/who.c */
extern void command_who(object *op, const char *command, char *params);
/* src/loaders/map_header.c */
extern FILE *yy_map_headerin;
extern FILE *yy_map_headerout;
extern int yy_map_headerlineno;
extern int yy_map_header_flex_debug;
extern char *yy_map_headertext;
extern int map_lex_load(mapstruct *m);
extern void yy_map_headerrestart(FILE *input_file);
extern void yy_map_headerpop_buffer_state(void);
extern int yy_map_headerget_lineno(void);
extern FILE *yy_map_headerget_in(void);
extern FILE *yy_map_headerget_out(void);
extern char *yy_map_headerget_text(void);
extern void yy_map_headerset_lineno(int line_number);
extern void yy_map_headerset_in(FILE *in_str);
extern void yy_map_headerset_out(FILE *out_str);
extern int yy_map_headerget_debug(void);
extern void yy_map_headerset_debug(int bdebug);
extern int yy_map_headerlex_destroy(void);
extern void yy_map_headerfree(void *ptr);
extern int map_set_variable(mapstruct *m, char *buf);
extern void free_map_header_loader(void);
extern int load_map_header(mapstruct *m, FILE *fp);
extern void save_map_header(mapstruct *m, FILE *fp, int flag);
/* src/loaders/object.c */
extern FILE *yy_objectin;
extern FILE *yy_objectout;
extern int yy_objectlineno;
extern int yy_object_flex_debug;
extern char *yy_objecttext;
extern int lex_load(int *depth, object **items, int maxdepth, int map_flags, int linemode);
extern void yy_objectrestart(FILE *input_file);
extern void yy_objectpop_buffer_state(void);
extern int yy_objectget_lineno(void);
extern FILE *yy_objectget_in(void);
extern FILE *yy_objectget_out(void);
extern char *yy_objectget_text(void);
extern void yy_objectset_lineno(int line_number);
extern void yy_objectset_in(FILE *in_str);
extern void yy_objectset_out(FILE *out_str);
extern int yy_objectget_debug(void);
extern void yy_objectset_debug(int bdebug);
extern int yy_objectlex_destroy(void);
extern void yy_objectfree(void *ptr);
extern int yyerror(char *s);
extern void free_object_loader(void);
extern void delete_loader_buffer(void *buffer);
extern void *create_loader_buffer(void *fp);
extern int load_object(void *fp, object *op, void *mybuffer, int bufstate, int map_flags);
extern int set_variable(object *op, const char *buf);
extern void get_ob_diff(StringBuffer *sb, object *op, object *op2);
extern void save_object(FILE *fp, object *op);
/* src/loaders/random_map.c */
extern FILE *yy_random_mapin;
extern FILE *yy_random_mapout;
extern int yy_random_maplineno;
extern int yy_random_map_flex_debug;
extern char *yy_random_maptext;
extern int rmap_lex_read(RMParms *RP);
extern void yy_random_maprestart(FILE *input_file);
extern void yy_random_mappop_buffer_state(void);
extern int yy_random_mapget_lineno(void);
extern FILE *yy_random_mapget_in(void);
extern FILE *yy_random_mapget_out(void);
extern char *yy_random_mapget_text(void);
extern void yy_random_mapset_lineno(int line_number);
extern void yy_random_mapset_in(FILE *in_str);
extern void yy_random_mapset_out(FILE *out_str);
extern int yy_random_mapget_debug(void);
extern void yy_random_mapset_debug(int bdebug);
extern int yy_random_maplex_destroy(void);
extern void yy_random_mapfree(void *ptr);
extern int load_parameters(FILE *fp, int bufstate, RMParms *RP);
extern int set_random_map_variable(RMParms *rp, const char *buf);
extern void free_random_map_loader(void);
/* src/random_maps/decor.c */
extern void put_decor(mapstruct *map, char **layout, RMParms *RP);
/* src/random_maps/door.c */
extern int surround_check2(char **layout, int x, int y, int Xsize, int Ysize);
extern void put_doors(mapstruct *the_map, char **maze, char *doorstyle, RMParms *RP);
/* src/random_maps/exit.c */
extern void find_in_layout(int mode, char target, int *fx, int *fy, char **layout, RMParms *RP);
extern void place_exits(mapstruct *map, char **maze, char *exitstyle, int orientation, RMParms *RP);
extern void unblock_exits(mapstruct *map, char **maze, RMParms *RP);
/* src/random_maps/expand2x.c */
extern char **expand2x(char **layout, int xsize, int ysize);
/* src/random_maps/floor.c */
extern mapstruct *make_map_floor(char *floorstyle, RMParms *RP);
/* src/random_maps/maze_gen.c */
extern char **maze_gen(int xsize, int ysize, int option);
/* src/random_maps/monster.c */
extern void place_monsters(mapstruct *map, char *monsterstyle, int difficulty, RMParms *RP);
/* src/random_maps/random_map.c */
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
/* src/random_maps/rogue_layout.c */
extern int surround_check(char **layout, int i, int j, int Xsize, int Ysize);
extern char **roguelike_layout_gen(int xsize, int ysize, int options);
/* src/random_maps/room_gen_onion.c */
extern char **map_gen_onion(int xsize, int ysize, int option, int layers);
extern void centered_onion(char **maze, int xsize, int ysize, int option, int layers);
extern void bottom_centered_onion(char **maze, int xsize, int ysize, int option, int layers);
extern void draw_onion(char **maze, float *xlocations, float *ylocations, int layers);
extern void make_doors(char **maze, float *xlocations, float *ylocations, int layers, int options);
extern void bottom_right_centered_onion(char **maze, int xsize, int ysize, int option, int layers);
/* src/random_maps/room_gen_spiral.c */
extern char **map_gen_spiral(int xsize, int ysize, int option);
extern void connect_spirals(int xsize, int ysize, int sym, char **layout);
/* src/random_maps/snake.c */
extern char **make_snake_layout(int xsize, int ysize);
/* src/random_maps/square_spiral.c */
extern void find_top_left_corner(char **maze, int *cx, int *cy);
extern char **make_square_spiral_layout(int xsize, int ysize);
/* src/random_maps/style.c */
extern int load_dir(const char *dir, char ***namelist, int skip_dirs);
extern mapstruct *styles;
extern mapstruct *load_style_map(char *style_name);
extern mapstruct *find_style(char *dirname, char *stylename, int difficulty);
extern object *pick_random_object(mapstruct *style);
extern void free_style_maps(void);
/* src/random_maps/wall.c */
extern int surround_flag(char **layout, int i, int j, RMParms *RP);
extern int surround_flag2(char **layout, int i, int j, RMParms *RP);
extern int surround_flag3(mapstruct *map, int i, int j, RMParms *RP);
extern int surround_flag4(mapstruct *map, int i, int j, RMParms *RP);
extern void make_map_walls(mapstruct *map, char **layout, char *w_style, RMParms *RP);
extern object *pick_joined_wall(object *the_wall, char **layout, int i, int j, RMParms *RP);
extern object *retrofit_joined_wall(mapstruct *the_map, int i, int j, int insert_flag, RMParms *RP);
/* src/server/account.c */
extern void account_init(void);
extern void account_deinit(void);
extern char *account_make_path(const char *name);
extern void account_login(socket_struct *ns, char *name, char *password);
extern void account_register(socket_struct *ns, char *name, char *password, char *password2);
extern void account_new_char(socket_struct *ns, char *name, char *archname);
extern void account_login_char(socket_struct *ns, char *name);
extern void account_logout_char(socket_struct *ns, player *pl);
extern void account_password_change(socket_struct *ns, char *password, char *password_new, char *password_new2);
/* src/server/anim.c */
extern Animations *animations;
extern int num_animations;
extern int animations_allocated;
extern void free_all_anim(void);
extern void init_anim(void);
extern int find_animation(char *name);
extern void animate_object(object *op, int count);
extern void animate_turning(object *op);
/* src/server/apply.c */
extern int manual_apply(object *op, object *tmp, int aflag);
extern int player_apply(object *pl, object *op, int aflag, int quiet);
extern void player_apply_below(object *pl);
/* src/server/arch.c */
extern int arch_init;
extern archetype *first_archetype;
extern archetype *wp_archetype;
extern archetype *empty_archetype;
extern archetype *base_info_archetype;
extern archetype *level_up_arch;
extern archetype *find_archetype(const char *name);
extern void arch_add(archetype *at);
extern void init_archetypes(void);
extern void free_all_archs(void);
extern object *arch_to_object(archetype *at);
extern object *create_singularity(const char *name);
extern object *get_archetype(const char *name);
/* src/server/attack.c */
extern char *attack_save[NROFATTACKS];
extern char *attack_name[NROFATTACKS];
extern int attack_ob(object *op, object *hitter);
extern int hit_player(object *op, int dam, object *hitter, int type);
extern void hit_map(object *op, int dir, int reduce);
extern int kill_object(object *op, int dam, object *hitter, int type);
extern void confuse_living(object *op);
extern void paralyze_living(object *op, int dam);
extern int is_melee_range(object *hitter, object *enemy);
/* src/server/ban.c */
extern void ban_init(void);
extern void ban_deinit(void);
extern void load_bans_file(void);
extern void save_bans_file(void);
extern int checkbanned(const char *name, char *ip);
extern int add_ban(char *input);
extern int remove_ban(char *input);
extern void list_bans(object *op);
/* src/server/cache.c */
extern cache_struct *cache_find(shstr *key);
extern int cache_add(const char *key, void *ptr, uint32 flags);
extern int cache_remove(shstr *key);
extern void cache_remove_all(void);
extern void cache_remove_by_flags(uint32 flags);
/* src/server/commands.c */
extern void toolkit_commands_init(void);
extern void toolkit_commands_deinit(void);
extern void commands_add(const char *name, command_func handle_func, double delay, uint64 flags);
extern int commands_check_permission(player *pl, const char *command);
extern void commands_handle(object *op, char *cmd);
/* src/server/connection.c */
extern void connection_object_add(object *op, mapstruct *map, int connected);
extern void connection_object_remove(object *op);
extern int connection_object_get_value(object *op);
extern void connection_trigger(object *op, int state);
extern void connection_trigger_button(object *op, int state);
/* src/server/exp.c */
extern uint64 new_levels[115 + 2];
extern _level_color level_color[201];
extern uint64 level_exp(int level, double expmul);
extern sint64 add_exp(object *op, sint64 exp_gain, int skill_nr, int exact);
extern int exp_lvl_adj(object *who, object *op);
extern float calc_level_difference(int who_lvl, int op_lvl);
/* src/server/gods.c */
extern object *find_god(const char *name);
extern const char *determine_god(object *op);
/* src/server/hiscore.c */
extern void hiscore_init(void);
extern void hiscore_check(object *op, int quiet);
extern void hiscore_display(object *op, int max, const char *match);
/* src/server/image.c */
extern New_Face *new_faces;
extern New_Face *blank_face;
extern New_Face *next_item_face;
extern New_Face *prev_item_face;
extern int nroffiles;
extern int nrofpixmaps;
extern int read_bmap_names(void);
extern int find_face(char *name, int error);
extern void free_all_images(void);
/* src/server/init.c */
extern struct settings_struct settings;
extern shstr_constants shstr_cons;
extern int world_darkness;
extern unsigned long todtick;
extern archetype *level_up_arch;
extern char first_map_path[256];
extern void free_strings(void);
extern void cleanup(void);
extern void init_globals(void);
extern void write_todclock(void);
extern void init(int argc, char **argv);
/* src/server/item.c */
extern char *describe_protections(object *op, int newline);
extern char *query_weight(object *op);
extern char *get_levelnumber(int i);
extern char *query_short_name(object *op, object *caller);
extern char *query_name(object *op, object *caller);
extern char *query_material_name(object *op);
extern char *query_base_name(object *op, object *caller);
extern char *describe_item(object *op);
extern int need_identify(object *op);
extern void identify(object *op);
extern void set_trapped_flag(object *op);
/* src/server/light.c */
extern void adjust_light_source(mapstruct *map, int x, int y, int light);
extern void check_light_source_list(mapstruct *map);
extern void remove_light_source_list(mapstruct *map);
/* src/server/links.c */
extern mempool_struct *pool_objectlink;
extern void objectlink_init(void);
extern void objectlink_deinit(void);
extern objectlink *get_objectlink(void);
extern void free_objectlink(objectlink *ol);
extern void free_objectlinkpt(objectlink *obp);
extern objectlink *objectlink_link(objectlink **startptr, objectlink **endptr, objectlink *afterptr, objectlink *beforeptr, objectlink *objptr);
extern objectlink *objectlink_unlink(objectlink **startptr, objectlink **endptr, objectlink *objptr);
/* src/server/living.c */
extern int dam_bonus[30 + 1];
extern int thaco_bonus[30 + 1];
extern float speed_bonus[30 + 1];
extern uint32 weight_limit[30 + 1];
extern int learn_spell[30 + 1];
extern int savethrow[115 + 1];
extern const char *const restore_msg[7];
extern const char *const lose_msg[7];
extern const char *const statname[7];
extern const char *const short_stat_name[7];
extern void set_attr_value(living *stats, int attr, sint8 value);
extern void change_attr_value(living *stats, int attr, sint8 value);
extern sint8 get_attr_value(living *stats, int attr);
extern void check_stat_bounds(living *stats);
extern int change_abil(object *op, object *tmp);
extern void drain_stat(object *op);
extern void drain_specific_stat(object *op, int deplete_stats);
extern void fix_player(object *op);
extern void fix_monster(object *op);
extern object *insert_base_info_object(object *op);
extern object *find_base_info_object(object *op);
extern void set_mobile_speed(object *op, int idx);
/* src/server/los.c */
extern void init_block(void);
extern void set_block(int x, int y, int bx, int by);
extern void update_los(object *op);
extern void clear_los(object *op);
extern int obj_in_line_of_sight(object *obj, rv_vector *rv);
/* src/server/main.c */
extern player *first_player;
extern mapstruct *first_map;
extern treasurelist *first_treasurelist;
extern artifactlist *first_artifactlist;
extern player *last_player;
extern uint32 global_round_tag;
extern void version(object *op);
extern void leave_map(object *op);
extern void set_map_timeout(mapstruct *map);
extern void process_events(mapstruct *map);
extern void clean_tmp_files(void);
extern void server_shutdown(void);
extern int swap_apartments(const char *mapold, const char *mapnew, int x, int y, object *op);
extern void shutdown_timer_start(long secs);
extern void shutdown_timer_stop(void);
extern int main(int argc, char **argv);
/* src/server/map.c */
extern int global_darkness_table[7 + 1];
extern int map_tiled_reverse[8];
extern mapstruct *has_been_loaded_sh(shstr *name);
extern char *create_pathname(const char *name);
extern int wall(mapstruct *m, int x, int y);
extern int blocks_view(mapstruct *m, int x, int y);
extern int blocks_magic(mapstruct *m, int x, int y);
extern int blocked(object *op, mapstruct *m, int x, int y, int terrain);
extern int blocked_link(object *op, int xoff, int yoff);
extern int blocked_link_2(object *op, mapstruct *map, int x, int y);
extern int blocked_tile(object *op, mapstruct *m, int x, int y);
extern int arch_blocked(archetype *at, object *op, mapstruct *m, int x, int y);
extern void set_map_darkness(mapstruct *m, int value);
extern mapstruct *get_linked_map(void);
extern mapstruct *get_empty_map(int sizex, int sizey);
extern mapstruct *load_original_map(const char *filename, int flags);
extern int new_save_map(mapstruct *m, int flag);
extern void free_map(mapstruct *m, int flag);
extern void delete_map(mapstruct *m);
extern mapstruct *ready_map_name(const char *name, int flags);
extern void clean_tmp_map(mapstruct *m);
extern void free_all_maps(void);
extern void update_position(mapstruct *m, int x, int y);
extern void set_map_reset_time(mapstruct *map);
extern mapstruct *get_map_from_coord(mapstruct *m, int *x, int *y);
extern mapstruct *get_map_from_coord2(mapstruct *m, int *x, int *y);
extern int get_rangevector(object *op1, object *op2, rv_vector *retval, int flags);
extern int get_rangevector_from_mapcoords(mapstruct *map1, int x, int y, mapstruct *map2, int x2, int y2, rv_vector *retval, int flags);
extern int on_same_map(object *op1, object *op2);
extern int players_on_map(mapstruct *m);
extern int wall_blocked(mapstruct *m, int x, int y);
extern int map_get_darkness(mapstruct *m, int x, int y, object **mirror);
extern int map_path_isabs(const char *path);
extern char *map_get_path(mapstruct *m, const char *path, uint8 unique, const char *name);
extern mapstruct *map_force_reset(mapstruct *m);
/* src/server/move.c */
extern int get_random_dir(void);
extern int get_randomized_dir(int dir);
extern int move_ob(object *op, int dir, object *originator);
extern int transfer_ob(object *op, int x, int y, int randomly, object *originator, object *trap);
extern int teleport(object *teleporter, uint8 tele_type, object *user);
extern int push_ob(object *op, int dir, object *pusher);
/* src/server/object.c */
extern object *active_objects;
extern const char *gender_noun[4];
extern const char *gender_subjective[4];
extern const char *gender_subjective_upper[4];
extern const char *gender_objective[4];
extern const char *gender_possessive[4];
extern const char *gender_reflexive[4];
extern materialtype materials[13];
extern material_real_struct material_real[13 * 64 + 1];
extern void init_materials(void);
extern int freearr_x[49];
extern int freearr_y[49];
extern int maxfree[49];
extern int freedir[49];
extern void (*object_initializers[256])(object *);
extern const char *object_flag_names[135 + 1];
extern int CAN_MERGE(object *ob1, object *ob2);
extern object *object_merge(object *op);
extern signed long sum_weight(object *op);
extern void add_weight(object *op, sint32 weight);
extern void sub_weight(object *op, sint32 weight);
extern object *get_env_recursive(object *op);
extern object *is_player_inv(object *op);
extern void dump_object(object *op, StringBuffer *sb);
extern void dump_object_rec(object *op, StringBuffer *sb);
extern object *get_owner(object *op);
extern void clear_owner(object *op);
extern void set_owner(object *op, object *owner);
extern void copy_owner(object *op, object *clone_ob);
extern void initialize_object(object *op);
extern void copy_object(object *op2, object *op, int no_speed);
extern void copy_object_with_inv(object *src_ob, object *dest_ob);
extern void object_init(void);
extern void object_deinit(void);
extern object *get_object(void);
extern void update_turn_face(object *op);
extern void update_ob_speed(object *op);
extern void update_object(object *op, int action);
extern void drop_ob_inv(object *ob);
extern void object_destroy_inv(object *ob);
extern void object_destroy(object *ob);
extern void destruct_ob(object *op);
extern void object_remove(object *op, int flags);
extern object *insert_ob_in_map(object *op, mapstruct *m, object *originator, int flag);
extern int object_check_move_on(object *op, object *originator);
extern void replace_insert_ob_in_map(char *arch_string, object *op);
extern object *object_stack_get(object *op, uint32 nrof);
extern object *object_stack_get_reinsert(object *op, uint32 nrof);
extern object *object_stack_get_removed(object *op, uint32 nrof);
extern object *decrease_ob_nr(object *op, uint32 i);
extern object *object_insert_into(object *op, object *where, int flag);
extern object *insert_ob_in_ob(object *op, object *where);
extern object *present_arch(archetype *at, mapstruct *m, int x, int y);
extern object *present(uint8 type, mapstruct *m, int x, int y);
extern object *present_in_ob(uint8 type, object *op);
extern object *present_arch_in_ob(archetype *at, object *op);
extern int find_free_spot(archetype *at, object *op, mapstruct *m, int x, int y, int start, int stop);
extern int find_first_free_spot(archetype *at, object *op, mapstruct *m, int x, int y);
extern int find_first_free_spot2(archetype *at, mapstruct *m, int x, int y, int start, int range);
extern void permute(int *arr, int begin, int end);
extern void get_search_arr(int *search_arr);
extern int find_dir_2(int x, int y);
extern int absdir(int d);
extern int dirdiff(int dir1, int dir2);
extern int get_dir_to_target(object *op, object *target, rv_vector *range_vector);
extern int can_pick(object *who, object *item);
extern object *object_create_clone(object *asrc);
extern int was_destroyed(object *op, tag_t old_tag);
extern object *load_object_str(char *obstr);
extern int auto_apply(object *op);
extern int can_see_monsterP(mapstruct *m, int x, int y, int dir);
extern void free_key_values(object *op);
extern key_value *object_get_key_link(const object *ob, const char *key);
extern const char *object_get_value(const object *op, const char *const key);
extern int object_set_value(object *op, const char *key, const char *value, int add_key);
extern void init_object_initializers(void);
extern int item_matched_string(object *pl, object *op, const char *name);
extern int object_get_gender(object *op);
extern void object_reverse_inventory(object *op);
extern int object_enter_map(object *op, object *exit_ob, mapstruct *m, int x, int y, uint8 fixed_pos);
/* src/server/object_methods.c */
extern object_methods object_type_methods[160];
extern object_methods object_methods_base;
extern void object_methods_init(void);
extern int object_apply(object *op, object *applier, int aflags);
extern void object_process(object *op);
extern char *object_describe(object *op, object *observer, char *buf, size_t size);
extern int object_move_on(object *op, object *victim, object *originator, int state);
extern int object_trigger(object *op, object *cause, int state);
extern int object_trigger_button(object *op, object *cause, int state);
extern void object_callback_remove_map(object *op);
extern void object_callback_remove_inv(object *op);
extern object *object_projectile_fire(object *op, object *shooter, int dir);
extern object *object_projectile_move(object *op);
extern int object_projectile_hit(object *op, object *victim);
extern object *object_projectile_stop(object *op, int reason);
extern int object_ranged_fire(object *op, object *shooter, int dir, double *delay);
/* src/server/party.c */
extern const char *const party_loot_modes[PARTY_LOOT_MAX];
extern const char *const party_loot_modes_help[PARTY_LOOT_MAX];
extern party_struct *first_party;
extern void party_init(void);
extern void party_deinit(void);
extern void add_party_member(party_struct *party, object *op);
extern void remove_party_member(party_struct *party, object *op);
extern void form_party(object *op, const char *name);
extern party_struct *find_party(const char *name);
extern int party_can_open_corpse(object *pl, object *corpse);
extern void party_handle_corpse(object *pl, object *corpse);
extern void send_party_message(party_struct *party, const char *msg, int flag, object *op, object *except);
extern void remove_party(party_struct *party);
extern void party_update_who(player *pl);
/* src/server/pathfinder.c */
extern void request_new_path(object *waypoint);
extern object *get_next_requested_path(void);
extern shstr *encode_path(path_node *path);
extern int get_path_next(shstr *buf, sint16 *off, shstr **mappath, mapstruct **map, int *x, int *y);
extern path_node *compress_path(path_node *path);
extern path_node *find_path(object *op, mapstruct *map1, int x, int y, mapstruct *map2, int x2, int y2);
/* src/server/plugins.c */
extern struct plugin_hooklist hooklist;
extern object *get_event_object(object *op, int event_nr);
extern void display_plugins_list(object *op);
extern void init_plugins(void);
extern void init_plugin(const char *pluginfile);
extern void remove_plugin(const char *id);
extern void remove_plugins(void);
extern void map_event_obj_init(object *ob);
extern void map_event_free(map_event *tmp);
extern int trigger_map_event(int event_id, mapstruct *m, object *activator, object *other, object *other2, const char *text, int parm);
extern void trigger_global_event(int event_type, void *parm1, void *parm2);
extern int trigger_event(int event_type, object *const activator, object *const me, object *const other, const char *msg, int parm1, int parm2, int parm3, int flags);
/* src/server/quest.c */
extern void check_quest(object *op, object *quest_container);
/* src/server/race.c */
extern const char *item_races[13];
extern ob_race *race_find(shstr *name);
extern ob_race *race_get_random(void);
extern void race_init(void);
extern void race_free(void);
/* src/server/re-cmp.c */
extern const char *re_cmp(const char *str, const char *regexp);
/* src/server/readable.c */
extern int book_overflow(const char *buf1, const char *buf2, size_t booksize);
extern void init_readable(void);
extern object *get_random_mon(void);
extern void tailor_readable_ob(object *book, int msg_type);
extern void free_all_readable(void);
/* src/server/region.c */
extern region_struct *first_region;
extern void regions_init(void);
extern void regions_free(void);
extern region_struct *region_find_by_name(const char *region_name);
extern char *region_get_longname(const region_struct *region);
extern char *region_get_msg(const region_struct *region);
extern int region_enter_jail(object *op);
/* src/server/rune.c */
extern int trap_see(object *op, object *trap, int level);
extern int trap_show(object *trap, object *where);
extern int trap_disarm(object *disarmer, object *trap);
extern void trap_adjust(object *trap, int difficulty);
/* src/server/shop.c */
extern sint64 query_cost(object *tmp, object *who, int flag);
extern char *cost_string_from_value(sint64 cost);
extern char *query_cost_string(object *tmp, object *who, int flag);
extern sint64 query_money(object *op);
extern int pay_for_amount(sint64 to_pay, object *pl);
extern int pay_for_item(object *op, object *pl);
extern int get_payment(object *pl, object *op);
extern void sell_item(object *op, object *pl, sint64 value);
extern int get_money_from_string(const char *text, struct _money_block *money);
extern int query_money_type(object *op, int value);
extern sint64 remove_money_type(object *who, object *op, sint64 value, sint64 amount);
extern void insert_money_in_player(object *pl, object *money, uint32 nrof);
extern object *bank_get_info(object *op);
extern object *bank_create_info(object *op);
extern object *bank_get_create_info(object *op);
extern sint64 bank_get_balance(object *op);
extern int bank_deposit(object *op, const char *text, sint64 *value);
extern int bank_withdraw(object *op, const char *text, sint64 *value);
extern sint64 insert_coins(object *pl, sint64 value);
/* src/server/skill_util.c */
extern float stat_exp_mult[30 + 1];
extern sint64 do_skill(object *op, int dir, const char *params);
extern sint64 calc_skill_exp(object *who, object *op, int level);
extern void init_new_exp_system(void);
extern int check_skill_to_fire(object *op, object *weapon);
extern void link_player_skills(object *pl);
extern int change_skill(object *who, int sk_index);
extern int skill_attack(object *tmp, object *pl, int dir, char *string);
extern int SK_level(object *op);
extern object *SK_skill(object *op);
/* src/server/skills.c */
extern skill_struct skills[NROFSKILLS];
extern sint64 find_traps(object *pl, int level);
extern sint64 remove_trap(object *op);
/* src/server/spell_effect.c */
extern void cast_magic_storm(object *op, object *tmp, int lvl);
extern int recharge(object *op);
extern int cast_create_food(object *op, object *caster, int dir, const char *stringarg);
extern int cast_wor(object *op, object *caster);
extern int cast_destruction(object *op, object *caster, int dam, int attacktype);
extern int cast_heal_around(object *op, int level, int type);
extern int cast_heal(object *op, int level, object *target, int spell_type);
extern int cast_change_attr(object *op, object *caster, object *target, int spell_type);
extern int remove_curse(object *op, object *target, int type, int src);
extern int do_cast_identify(object *tmp, object *op, int mode, int *done, int level);
extern int cast_identify(object *op, int level, object *single_ob, int mode);
extern int cast_consecrate(object *op);
extern int finger_of_death(object *op, object *target);
extern int cast_cause_disease(object *op, object *caster, int dir, archetype *disease_arch, int type);
extern int cast_transform_wealth(object *op);
/* src/server/spell_util.c */
extern spell_struct spells[52];
extern char *spellpathnames[20];
extern archetype *spellarch[52];
extern void init_spells(void);
extern int insert_spell_effect(char *archname, mapstruct *m, int x, int y);
extern spell_struct *find_spell(int spelltype);
extern int cast_spell(object *op, object *caster, int dir, int type, int ability, int item, const char *stringarg);
extern int cast_create_obj(object *op, object *new_op, int dir);
extern int fire_bolt(object *op, object *caster, int dir, int type);
extern int fire_arch_from_position(object *op, object *caster, sint16 x, sint16 y, int dir, archetype *at, int type, object *target);
extern int cast_cone(object *op, object *caster, int dir, int strength, int spell_type, archetype *spell_arch);
extern void cone_drop(object *op);
extern void explode_object(object *op);
extern void check_fired_arch(object *op);
extern int find_target_for_spell(object *op, object **target, uint32 flags);
extern int SP_level_dam_adjust(object *caster, int spell_type, int base_dam, int exact);
extern int SP_level_strength_adjust(object *caster, int spell_type);
extern int SP_level_spellpoint_cost(object *caster, int spell_type, int caster_level);
extern void fire_swarm(object *op, object *caster, int dir, archetype *swarm_type, int spell_type, int n, int magic);
/* src/server/statistics.c */
extern void statistics_init(void);
extern void statistic_update(const char *type, object *op, sint64 i, const char *buf);
extern void statistics_player_logout(player *pl);
/* src/server/swap.c */
extern void write_map_log(void);
extern void read_map_log(void);
extern void swap_map(mapstruct *map, int force_flag);
extern void check_active_maps(void);
extern void flush_old_maps(void);
/* src/server/time.c */
extern long max_time;
extern long pticks;
extern struct timeval last_time;
extern const char *season_name[4];
extern const char *weekdays[7];
extern const char *month_name[12];
extern const char *periodsofday[10];
extern const int periodsofday_hours[24];
extern void reset_sleep(void);
extern void sleep_delta(void);
extern void set_max_time(long t);
extern void get_tod(timeofday_t *tod);
extern void print_tod(object *op);
extern void time_info(object *op);
extern long seconds(void);
/* src/server/treasure.c */
extern char *coins[4 + 1];
extern archetype *coins_arch[4];
extern void load_treasures(void);
extern void init_artifacts(void);
extern void init_archetype_pointers(void);
extern treasurelist *find_treasurelist(const char *name);
extern object *generate_treasure(treasurelist *t, int difficulty, int a_chance);
extern void create_treasure(treasurelist *t, object *op, int flag, int difficulty, int t_style, int a_chance, int tries, struct _change_arch *arch_change);
extern void set_abs_magic(object *op, int magic);
extern int fix_generated_item(object **op_ptr, object *creator, int difficulty, int a_chance, int t_style, int max_magic, int fix_magic, int chance_magic, int flags);
extern artifactlist *find_artifactlist(int type);
extern archetype *find_artifact_archtype(const char *name);
extern artifact *find_artifact_type(const char *name, int type);
extern void give_artifact_abilities(object *op, artifact *art);
extern int generate_artifact(object *op, int difficulty, int t_style, int a_chance);
extern void free_all_treasures(void);
extern int get_environment_level(object *op);
extern object *create_artifact(object *op, char *artifactname);
/* src/server/weather.c */
extern const int season_timechange[4][24];
extern void init_world_darkness(void);
extern void tick_the_clock(void);
/* src/skills/construction.c */
extern void construction_do(object *op, int dir);
/* src/skills/inscription.c */
extern int skill_inscription(object *op, const char *params);
/* src/socket/image.c */
extern int is_valid_faceset(int fsn);
extern void free_socket_images(void);
extern void read_client_images(void);
extern void socket_command_ask_face(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos);
extern void face_get_data(int face, uint8 **ptr, uint16 *len);
/* src/socket/info.c */
extern void draw_info_send(uint8 type, const char *name, const char *color, socket_struct *ns, const char *buf);
extern void draw_info_full(uint8 type, const char *name, const char *color, StringBuffer *sb_capture, object *pl, const char *buf);
extern void draw_info_full_format(uint8 type, const char *name, const char *color, StringBuffer *sb_capture, object *pl, const char *format, ...) __attribute__((format(printf, 6, 7)));
extern void draw_info_type(uint8 type, const char *name, const char *color, object *pl, const char *buf);
extern void draw_info_type_format(uint8 type, const char *name, const char *color, object *pl, const char *format, ...) __attribute__((format(printf, 5, 6)));
extern void draw_info(const char *color, object *pl, const char *buf);
extern void draw_info_format(const char *color, object *pl, const char *format, ...) __attribute__((format(printf, 3, 4)));
extern void draw_info_map(uint8 type, const char *name, const char *color, mapstruct *map, int x, int y, int dist, object *op, object *op2, const char *buf);
/* src/socket/init.c */
extern Socket_Info socket_info;
extern socket_struct *init_sockets;
extern void init_connection(socket_struct *ns, const char *from_ip);
extern void init_ericserver(void);
extern void free_all_newserver(void);
extern void free_newsocket(socket_struct *ns);
extern void init_srv_files(void);
/* src/socket/item.c */
extern unsigned int query_flags(object *op);
extern void esrv_draw_look(object *pl);
extern void esrv_close_container(object *op);
extern void esrv_send_inventory(object *pl, object *op);
extern void esrv_update_item(int flags, object *op);
extern void esrv_send_item(object *op);
extern void esrv_del_item(object *op);
extern object *esrv_get_ob_from_count(object *pl, tag_t count);
extern void socket_command_item_examine(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos);
extern void send_quickslots(player *pl);
extern void socket_command_quickslot(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos);
extern void socket_command_item_apply(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos);
extern void socket_command_item_lock(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos);
extern void socket_command_item_mark(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos);
extern void esrv_move_object(object *pl, tag_t to, tag_t tag, long nrof);
/* src/socket/loop.c */
extern void handle_client(socket_struct *ns, player *pl);
extern void remove_ns_dead_player(player *pl);
extern void doeric_server(void);
extern void doeric_server_write(void);
/* src/socket/lowlevel.c */
extern int socket_recv(socket_struct *ns);
extern void socket_enable_no_delay(int fd);
extern void socket_disable_no_delay(int fd);
extern void socket_buffer_clear(socket_struct *ns);
extern void socket_buffer_write(socket_struct *ns);
extern void socket_send_packet(socket_struct *ns, packet_struct *packet);
/* src/socket/metaserver.c */
extern void metaserver_info_update(void);
extern void metaserver_init(void);
/* src/socket/request.c */
extern void socket_command_setup(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos);
extern void socket_command_player_cmd(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos);
extern void socket_command_version(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos);
extern void socket_command_item_move(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos);
extern void esrv_update_stats(player *pl);
extern void esrv_new_player(player *pl, uint32 weight);
extern void draw_map_text_anim(object *pl, const char *color, const char *text);
extern void draw_client_map(object *pl);
extern void packet_append_map_name(packet_struct *packet, object *op, object *map_info);
extern void packet_append_map_music(packet_struct *packet, object *op, object *map_info);
extern void packet_append_map_weather(packet_struct *packet, object *op, object *map_info);
extern void draw_client_map2(object *pl);
extern void socket_command_quest_list(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos);
extern void socket_command_clear(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos);
extern void socket_command_move_path(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos);
extern void socket_command_fire(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos);
extern void socket_command_keepalive(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos);
extern void socket_command_move(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos);
extern void send_target_command(player *pl);
extern void socket_command_account(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos);
extern void generate_quick_name(player *pl);
extern void socket_command_target(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos);
extern void socket_command_talk(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos);
extern void socket_command_control(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos);
/* src/socket/sounds.c */
extern void play_sound_player_only(player *pl, int type, const char *filename, int x, int y, int loop, int volume);
extern void play_sound_map(mapstruct *map, int type, const char *filename, int x, int y, int loop, int volume);
/* src/socket/updates.c */
extern void updates_init(void);
extern void socket_command_request_update(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos);
/* src/types/ability.c */
extern void object_type_init_ability(void);
/* src/types/amulet.c */
extern void object_type_init_amulet(void);
/* src/types/armour.c */
extern void object_type_init_armour(void);
/* src/types/arrow.c */
extern sint16 arrow_get_wc(object *op, object *bow, object *arrow);
extern sint16 arrow_get_damage(object *op, object *bow, object *arrow);
extern object *arrow_find(object *op, shstr *type);
extern void object_type_init_arrow(void);
/* src/types/base_info.c */
extern void object_type_init_base_info(void);
/* src/types/beacon.c */
extern void beacon_add(object *ob);
extern void beacon_remove(object *ob);
extern object *beacon_locate(shstr *name);
extern void object_type_init_beacon(void);
/* src/types/blindness.c */
extern void object_type_init_blindness(void);
/* src/types/book.c */
extern void object_type_init_book(void);
/* src/types/boots.c */
extern void object_type_init_boots(void);
/* src/types/bow.c */
extern sint32 bow_get_ws(object *bow, object *arrow);
extern int bow_get_skill(object *bow);
extern void object_type_init_bow(void);
/* src/types/bracers.c */
extern void object_type_init_bracers(void);
/* src/types/bullet.c */
extern int bullet_reflect(object *op, mapstruct *m, int x, int y);
extern void object_type_init_bullet(void);
/* src/types/button.c */
extern void object_type_init_button(void);
/* src/types/check_inv.c */
extern object *check_inv(object *op, object *ob);
extern void object_type_init_check_inv(void);
/* src/types/class.c */
extern void object_type_init_class(void);
/* src/types/client_map_info.c */
extern void object_type_init_client_map_info(void);
/* src/types/cloak.c */
extern void object_type_init_cloak(void);
/* src/types/clock.c */
extern void object_type_init_clock(void);
/* src/types/common/apply.c */
extern int common_object_apply(object *op, object *applier, int aflags);
extern int object_apply_item(object *op, object *applier, int aflags);
/* src/types/common/describe.c */
extern void common_object_describe(object *op, object *observer, char *buf, size_t size);
/* src/types/common/move_on.c */
extern int common_object_move_on(object *op, object *victim, object *originator, int state);
/* src/types/common/process.c */
extern int common_object_process_pre(object *op);
extern void common_object_process(object *op);
/* src/types/common/projectile.c */
extern void common_object_projectile_process(object *op);
extern object *common_object_projectile_move(object *op);
extern object *common_object_projectile_stop_missile(object *op, int reason);
extern object *common_object_projectile_stop_spell(object *op, int reason);
extern object *common_object_projectile_fire_missile(object *op, object *shooter, int dir);
extern int common_object_projectile_hit(object *op, object *victim);
extern int common_object_projectile_move_on(object *op, object *victim, object *originator, int state);
/* src/types/compass.c */
extern void object_type_init_compass(void);
/* src/types/cone.c */
extern void object_type_init_cone(void);
/* src/types/confusion.c */
extern void object_type_init_confusion(void);
/* src/types/container.c */
extern int check_magical_container(object *op, object *container);
extern int container_close(object *applier, object *op);
extern void object_type_init_container(void);
/* src/types/corpse.c */
extern void object_type_init_corpse(void);
/* src/types/creator.c */
extern void object_type_init_creator(void);
/* src/types/dead_object.c */
extern void object_type_init_dead_object(void);
/* src/types/detector.c */
extern void object_type_init_detector(void);
/* src/types/director.c */
extern void object_type_init_director(void);
/* src/types/disease.c */
extern int move_disease(object *disease);
extern int infect_object(object *victim, object *disease, int force);
extern void move_symptom(object *symptom);
extern void check_physically_infect(object *victim, object *hitter);
extern int cure_disease(object *sufferer, object *caster);
extern int reduce_symptoms(object *sufferer, int reduction);
extern void object_type_init_disease(void);
/* src/types/door.c */
extern int door_try_open(object *op, mapstruct *m, int x, int y, int test);
extern object *find_key(object *op, object *door);
extern void object_type_init_door(void);
/* src/types/drink.c */
extern void object_type_init_drink(void);
/* src/types/duplicator.c */
extern void object_type_init_duplicator(void);
/* src/types/event_obj.c */
extern void object_type_init_event_obj(void);
/* src/types/exit.c */
extern void object_type_init_exit(void);
/* src/types/experience.c */
extern void object_type_init_experience(void);
/* src/types/firewall.c */
extern void object_type_init_firewall(void);
/* src/types/flesh.c */
extern void object_type_init_flesh(void);
/* src/types/floor.c */
extern void object_type_init_floor(void);
/* src/types/food.c */
extern void object_type_init_food(void);
/* src/types/force.c */
extern void object_type_init_force(void);
/* src/types/gate.c */
extern void object_type_init_gate(void);
/* src/types/gem.c */
extern void object_type_init_gem(void);
/* src/types/girdle.c */
extern void object_type_init_girdle(void);
/* src/types/gloves.c */
extern void object_type_init_gloves(void);
/* src/types/god.c */
extern void object_type_init_god(void);
/* src/types/gravestone.c */
extern const char *gravestone_text(object *op);
extern void object_type_init_gravestone(void);
/* src/types/greaves.c */
extern void object_type_init_greaves(void);
/* src/types/handle.c */
extern void object_type_init_handle(void);
/* src/types/helmet.c */
extern void object_type_init_helmet(void);
/* src/types/holy_altar.c */
extern void object_type_init_holy_altar(void);
/* src/types/inorganic.c */
extern void object_type_init_inorganic(void);
/* src/types/jewel.c */
extern void object_type_init_jewel(void);
/* src/types/key.c */
extern void object_type_init_key(void);
/* src/types/light_apply.c */
extern void object_type_init_light_apply(void);
/* src/types/light_refill.c */
extern void object_type_init_light_refill(void);
/* src/types/light_source.c */
extern void object_type_init_light_source(void);
/* src/types/lightning.c */
extern void object_type_init_lightning(void);
/* src/types/magic_mirror.c */
extern void magic_mirror_init(object *mirror);
extern void magic_mirror_deinit(object *mirror);
extern mapstruct *magic_mirror_get_map(object *mirror);
extern void object_type_init_magic_mirror(void);
/* src/types/map.c */
extern void object_type_init_map(void);
/* src/types/map_event_obj.c */
extern void object_type_init_map_event_obj(void);
/* src/types/map_info.c */
extern void map_info_init(object *info);
extern void object_type_init_map_info(void);
/* src/types/marker.c */
extern void object_type_init_marker(void);
/* src/types/material.c */
extern void object_type_init_material(void);
/* src/types/misc_object.c */
extern void object_type_init_misc_object(void);
/* src/types/money.c */
extern void object_type_init_money(void);
/* src/types/monster.c */
extern void set_npc_enemy(object *npc, object *enemy, rv_vector *rv);
extern void monster_enemy_signal(object *npc, object *enemy);
extern object *check_enemy(object *npc, rv_vector *rv);
extern object *find_enemy(object *npc, rv_vector *rv);
extern void object_type_init_monster(void);
extern int talk_to_npc(object *op, object *npc, char *txt);
extern int faction_is_friend_of(object *mon, object *pl);
extern int is_friend_of(object *op, object *obj);
extern int check_good_weapon(object *who, object *item);
extern int check_good_armour(object *who, object *item);
/* src/types/nugget.c */
extern void object_type_init_nugget(void);
/* src/types/organic.c */
extern void object_type_init_organic(void);
/* src/types/pearl.c */
extern void object_type_init_pearl(void);
/* src/types/pedestal.c */
extern int pedestal_matches_obj(object *op, object *tmp);
extern void object_type_init_pedestal(void);
/* src/types/player.c */
extern mempool_struct *pool_player;
extern void player_init(void);
extern void player_deinit(void);
extern void player_disconnect_all(void);
extern player *find_player(const char *plname);
extern void display_motd(object *op);
extern void free_player(player *pl);
extern void give_initial_items(object *pl, treasurelist *items);
extern int handle_newcs_player(player *pl);
extern void do_some_living(object *op);
extern void kill_player(object *op);
extern void cast_dust(object *op, object *throw_ob, int dir);
extern int pvp_area(object *attacker, object *victim);
extern object *find_skill(object *op, int skillnr);
extern int player_can_carry(object *pl, uint32 weight);
extern char *player_get_race_class(object *op, char *buf, size_t size);
extern void player_path_add(player *pl, mapstruct *map, sint16 x, sint16 y);
extern void player_path_clear(player *pl);
extern void player_path_handle(player *pl);
extern sint64 player_faction_reputation(player *pl, shstr *faction);
extern void player_faction_reputation_update(player *pl, shstr *faction, sint64 add);
extern int player_has_region_map(player *pl, region_struct *r);
extern char *player_sanitize_input(char *str);
extern void player_cleanup_name(char *str);
extern object *find_marked_object(object *op);
extern char *long_desc(object *tmp, object *caller);
extern void examine(object *op, object *tmp, StringBuffer *sb_capture);
extern int sack_can_hold(object *pl, object *sack, object *op, int nrof);
extern void pick_up(object *op, object *alt, int no_mevent);
extern void put_object_in_sack(object *op, object *sack, object *tmp, long nrof);
extern void drop_object(object *op, object *tmp, long nrof, int no_mevent);
extern void drop(object *op, object *tmp, int no_mevent);
extern char *player_make_path(const char *name, const char *ext);
extern int player_exists(const char *name);
extern void player_save(object *op);
extern void player_login(socket_struct *ns, const char *name, archetype *at);
extern void object_type_init_player(void);
/* src/types/player_mover.c */
extern void object_type_init_player_mover(void);
/* src/types/poisoning.c */
extern void object_type_init_poisoning(void);
/* src/types/potion.c */
extern void object_type_init_potion(void);
/* src/types/potion_effect.c */
extern void object_type_init_potion_effect(void);
/* src/types/power_crystal.c */
extern void object_type_init_power_crystal(void);
/* src/types/quest_container.c */
extern void object_type_init_quest_container(void);
/* src/types/random_drop.c */
extern void object_type_init_random_drop(void);
/* src/types/ring.c */
extern void object_type_init_ring(void);
/* src/types/rod.c */
extern void object_type_init_rod(void);
/* src/types/rune.c */
extern void rune_spring(object *op, object *victim);
extern void object_type_init_rune(void);
/* src/types/savebed.c */
extern void object_type_init_savebed(void);
/* src/types/scroll.c */
extern void object_type_init_scroll(void);
/* src/types/shield.c */
extern void object_type_init_shield(void);
/* src/types/shop_floor.c */
extern void object_type_init_shop_floor(void);
/* src/types/shop_mat.c */
extern void object_type_init_shop_mat(void);
/* src/types/sign.c */
extern void object_type_init_sign(void);
/* src/types/skill.c */
extern void object_type_init_skill(void);
/* src/types/skill_item.c */
extern void object_type_init_skill_item(void);
/* src/types/sound_ambient.c */
extern void sound_ambient_init(object *ob);
extern void object_type_init_sound_ambient(void);
/* src/types/spawn_point.c */
extern void spawn_point_enemy_signal(object *op);
extern void object_type_init_spawn_point(void);
/* src/types/spawn_point_info.c */
extern void object_type_init_spawn_point_info(void);
/* src/types/spawn_point_mob.c */
extern void object_type_init_spawn_point_mob(void);
/* src/types/spell.c */
extern void object_type_init_spell(void);
/* src/types/spinner.c */
extern void object_type_init_spinner(void);
/* src/types/swarm_spell.c */
extern void object_type_init_swarm_spell(void);
/* src/types/symptom.c */
extern void object_type_init_symptom(void);
/* src/types/teleporter.c */
extern void object_type_init_teleporter(void);
/* src/types/treasure.c */
extern void object_type_init_treasure(void);
/* src/types/wall.c */
extern void object_type_init_wall(void);
/* src/types/wand.c */
extern void object_type_init_wand(void);
/* src/types/waypoint.c */
extern object *get_active_waypoint(object *op);
extern object *get_aggro_waypoint(object *op);
extern object *get_return_waypoint(object *op);
extern void waypoint_compute_path(object *waypoint);
extern void waypoint_move(object *op, object *waypoint);
extern void object_type_init_waypoint(void);
/* src/types/wealth.c */
extern void object_type_init_wealth(void);
/* src/types/weapon.c */
extern void object_type_init_weapon(void);
/* src/types/word_of_recall.c */
extern void object_type_init_word_of_recall(void);
/* src/toolkit/binreloc.c */
extern void toolkit_binreloc_init(void);
extern void toolkit_binreloc_deinit(void);
extern char *binreloc_find_exe(const char *default_exe);
extern char *binreloc_find_exe_dir(const char *default_dir);
extern char *binreloc_find_prefix(const char *default_prefix);
extern char *binreloc_find_bin_dir(const char *default_bin_dir);
extern char *binreloc_find_sbin_dir(const char *default_sbin_dir);
extern char *binreloc_find_data_dir(const char *default_data_dir);
extern char *binreloc_find_locale_dir(const char *default_locale_dir);
extern char *binreloc_find_lib_dir(const char *default_lib_dir);
extern char *binreloc_find_libexec_dir(const char *default_libexec_dir);
extern char *binreloc_find_etc_dir(const char *default_etc_dir);
/* src/toolkit/bzr.c */
extern void toolkit_bzr_init(void);
extern void toolkit_bzr_deinit(void);
extern int bzr_get_revision(void);
/* src/toolkit/clioptions.c */
extern void toolkit_clioptions_init(void);
extern void toolkit_clioptions_deinit(void);
extern void clioptions_add(const char *longname, const char *shortname, clioptions_handler_func handle_func, uint8 argument, const char *desc_brief, const char *desc);
extern void clioptions_parse(int argc, char *argv[]);
extern int clioptions_load_config(const char *path, const char *category);
/* src/toolkit/colorspace.c */
extern void toolkit_colorspace_init(void);
extern void toolkit_colorspace_deinit(void);
extern double colorspace_rgb_max(const double rgb[3]);
extern double colorspace_rgb_min(const double rgb[3]);
extern void colorspace_rgb2hsv(const double rgb[3], double hsv[3]);
extern void colorspace_hsv2rgb(const double hsv[3], double rgb[3]);
/* src/toolkit/console.c */
extern void toolkit_console_init(void);
extern int console_start_thread(void);
extern void toolkit_console_deinit(void);
extern void console_command_add(const char *command, console_command_func handle_func, const char *desc_brief, const char *desc);
extern void console_command_handle(void);
/* src/toolkit/datetime.c */
extern void toolkit_datetime_init(void);
extern void toolkit_datetime_deinit(void);
extern time_t datetime_getutc(void);
extern time_t datetime_utctolocal(time_t t);
/* src/toolkit/logger.c */
extern void toolkit_logger_init(void);
extern void toolkit_logger_deinit(void);
extern void logger_open_log(const char *path);
extern FILE *logger_get_logfile(void);
extern logger_level logger_get_level(const char *name);
extern void logger_set_filter_stdout(const char *str);
extern void logger_set_filter_logfile(const char *str);
extern void logger_set_print_func(logger_print_func func);
extern void logger_do_print(const char *str);
extern void logger_print(logger_level level, const char *function, uint64 line, const char *format, ...) __attribute__((format(printf, 4, 5)));
/* src/toolkit/math.c */
extern void toolkit_math_init(void);
extern void toolkit_math_deinit(void);
extern unsigned long isqrt(unsigned long n);
extern int rndm(int min, int max);
extern int rndm_chance(uint32 n);
/* src/toolkit/memory.c */
extern void toolkit_memory_init(void);
extern void toolkit_memory_deinit(void);
extern void *memory_emalloc(size_t size);
extern void memory_efree(void *ptr);
extern void *memory_ecalloc(size_t nmemb, size_t size);
extern void *memory_erealloc(void *ptr, size_t size);
extern void *memory_reallocz(void *ptr, size_t old_size, size_t new_size);
/* src/toolkit/mempool.c */
extern mempool_chunk_struct end_marker;
extern void toolkit_mempool_init(void);
extern void toolkit_mempool_deinit(void);
extern uint32 nearest_pow_two_exp(uint32 n);
extern void setup_poolfunctions(mempool_struct *pool, chunk_constructor constructor, chunk_destructor destructor);
extern mempool_struct *mempool_create(const char *description, uint32 expand, uint32 size, uint32 flags, chunk_initialisator initialisator, chunk_deinitialisator deinitialisator, chunk_constructor constructor, chunk_destructor destructor);
extern void mempool_free(mempool_struct *pool);
extern void *get_poolchunk_array_real(mempool_struct *pool, uint32 arraysize_exp);
extern void return_poolchunk_array_real(void *data, uint32 arraysize_exp, mempool_struct *pool);
/* src/toolkit/packet.c */
extern void toolkit_packet_init(void);
extern void toolkit_packet_deinit(void);
extern packet_struct *packet_new(uint8 type, size_t size, size_t expand);
extern void packet_free(packet_struct *packet);
extern void packet_compress(packet_struct *packet);
extern void packet_enable_ndelay(packet_struct *packet);
extern void packet_set_pos(packet_struct *packet, size_t pos);
extern size_t packet_get_pos(packet_struct *packet);
extern packet_struct *packet_dup(packet_struct *packet);
extern void packet_delete(packet_struct *packet, size_t pos, size_t len);
extern void packet_merge(packet_struct *src, packet_struct *dst);
extern void packet_append_uint8(packet_struct *packet, uint8 data);
extern void packet_append_sint8(packet_struct *packet, sint8 data);
extern void packet_append_uint16(packet_struct *packet, uint16 data);
extern void packet_append_sint16(packet_struct *packet, sint16 data);
extern void packet_append_uint32(packet_struct *packet, uint32 data);
extern void packet_append_sint32(packet_struct *packet, sint32 data);
extern void packet_append_uint64(packet_struct *packet, uint64 data);
extern void packet_append_sint64(packet_struct *packet, sint64 data);
extern void packet_append_data_len(packet_struct *packet, uint8 *data, size_t len);
extern void packet_append_string(packet_struct *packet, const char *data);
extern void packet_append_string_terminated(packet_struct *packet, const char *data);
extern uint8 packet_to_uint8(uint8 *data, size_t len, size_t *pos);
extern sint8 packet_to_sint8(uint8 *data, size_t len, size_t *pos);
extern uint16 packet_to_uint16(uint8 *data, size_t len, size_t *pos);
extern sint16 packet_to_sint16(uint8 *data, size_t len, size_t *pos);
extern uint32 packet_to_uint32(uint8 *data, size_t len, size_t *pos);
extern sint32 packet_to_sint32(uint8 *data, size_t len, size_t *pos);
extern uint64 packet_to_uint64(uint8 *data, size_t len, size_t *pos);
extern sint64 packet_to_sint64(uint8 *data, size_t len, size_t *pos);
extern char *packet_to_string(uint8 *data, size_t len, size_t *pos, char *dest, size_t dest_size);
extern void packet_to_stringbuffer(uint8 *data, size_t len, size_t *pos, StringBuffer *sb);
/* src/toolkit/path.c */
extern void toolkit_path_init(void);
extern void toolkit_path_deinit(void);
extern char *path_join(const char *path, const char *path2);
extern char *path_dirname(const char *path);
extern char *path_basename(const char *path);
extern char *path_normalize(const char *path);
extern void path_ensure_directories(const char *path);
extern int path_copy_file(const char *src, FILE *dst, const char *mode);
extern int path_exists(const char *path);
extern int path_touch(const char *path);
extern size_t path_size(const char *path);
extern char *path_file_contents(const char *path);
/* src/toolkit/pbkdf2.c */
extern void PKCS5_PBKDF2_HMAC_SHA2(unsigned char *password, size_t plen, unsigned char *salt, size_t slen, const unsigned long iteration_count, const unsigned long key_length, unsigned char *output);
/* src/toolkit/porting.c */
extern void toolkit_porting_init(void);
extern void toolkit_porting_deinit(void);
/* src/toolkit/sha1.c */
extern void toolkit_sha1_init(void);
extern void toolkit_sha1_deinit(void);
extern void sha1_starts(sha1_context *ctx);
extern void sha1_update(sha1_context *ctx, const unsigned char *input, size_t ilen);
extern void sha1_finish(sha1_context *ctx, unsigned char output[20]);
extern void sha1(const unsigned char *input, size_t ilen, unsigned char output[20]);
extern int sha1_file(const char *path, unsigned char output[20]);
extern void sha1_hmac_starts(sha1_context *ctx, const unsigned char *key, size_t keylen);
extern void sha1_hmac_update(sha1_context *ctx, const unsigned char *input, size_t ilen);
extern void sha1_hmac_finish(sha1_context *ctx, unsigned char output[20]);
extern void sha1_hmac_reset(sha1_context *ctx);
extern void sha1_hmac(const unsigned char *key, size_t keylen, const unsigned char *input, size_t ilen, unsigned char output[20]);
/* src/toolkit/shstr.c */
extern void toolkit_shstr_init(void);
extern void toolkit_shstr_deinit(void);
extern shstr *add_string(const char *str);
extern shstr *add_refcount(shstr *str);
extern int query_refcount(shstr *str);
extern shstr *find_string(const char *str);
extern void free_string_shared(shstr *str);
/* src/toolkit/signals.c */
extern void toolkit_signals_init(void);
extern void toolkit_signals_deinit(void);
/* src/toolkit/socket.c */
extern void toolkit_socket_init(void);
extern void toolkit_socket_deinit(void);
extern socket_t *socket_create(const char *host, uint16 port);
extern int socket_connect(socket_t *sc);
extern void socket_destroy(socket_t *sc);
extern SSL *socket_ssl_create(socket_t *sc, SSL_CTX *ctx);
extern void socket_ssl_destroy(SSL *ssl);
/* src/toolkit/string.c */
extern void toolkit_string_init(void);
extern void toolkit_string_deinit(void);
extern char *string_estrdup(const char *s);
extern char *string_estrndup(const char *s, size_t n);
extern void string_replace(const char *src, const char *key, const char *replacement, char *result, size_t resultsize);
extern void string_replace_char(char *str, const char *key, const char replacement);
extern size_t string_split(char *str, char *array[], size_t array_size, char sep);
extern void string_replace_unprintable_chars(char *buf);
extern char *string_format_number_comma(uint64 num);
extern void string_toupper(char *str);
extern void string_tolower(char *str);
extern char *string_whitespace_trim(char *str);
extern char *string_whitespace_squeeze(char *str);
extern void string_newline_to_literal(char *str);
extern const char *string_get_word(const char *str, size_t *pos, char delim, char *word, size_t wordsize, int surround);
extern void string_skip_word(const char *str, size_t *i, int dir);
extern int string_isdigit(const char *str);
extern void string_capitalize(char *str);
extern void string_title(char *str);
extern int string_startswith(const char *str, const char *cmp);
extern int string_endswith(const char *str, const char *cmp);
extern char *string_sub(const char *str, ssize_t start, ssize_t end);
extern int string_isempty(const char *str);
extern int string_iswhite(const char *str);
extern int char_contains(const char c, const char *key);
extern int string_contains(const char *str, const char *key);
extern int string_contains_other(const char *str, const char *key);
extern char *string_create_char_range(char start, char end);
extern char *string_join(const char *delim, ...);
extern char *string_join_array(const char *delim, char **array, size_t arraysize);
extern char *string_repeat(const char *str, size_t num);
extern size_t snprintfcat(char *buf, size_t size, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
extern size_t string_tohex(const unsigned char *str, size_t len, char *result, size_t resultsize);
extern size_t string_fromhex(char *str, size_t len, unsigned char *result, size_t resultsize);
/* src/toolkit/stringbuffer.c */
extern void toolkit_stringbuffer_init(void);
extern void toolkit_stringbuffer_deinit(void);
extern StringBuffer *stringbuffer_new(void);
extern char *stringbuffer_finish(StringBuffer *sb);
extern const char *stringbuffer_finish_shared(StringBuffer *sb);
extern void stringbuffer_append_string_len(StringBuffer *sb, const char *str, size_t len);
extern void stringbuffer_append_string(StringBuffer *sb, const char *str);
extern void stringbuffer_append_printf(StringBuffer *sb, const char *format, ...) __attribute__((format(printf, 2, 3)));
extern void stringbuffer_append_stringbuffer(StringBuffer *sb, const StringBuffer *sb2);
extern void stringbuffer_append_char(StringBuffer *sb, const char c);
extern size_t stringbuffer_length(StringBuffer *sb);
extern ssize_t stringbuffer_index(StringBuffer *sb, char c);
extern ssize_t stringbuffer_rindex(StringBuffer *sb, char c);
/* src/toolkit/toolkit.c */
extern void toolkit_import_register(toolkit_func func);
extern int toolkit_check_imported(toolkit_func func);
extern void toolkit_deinit(void);
/* src/toolkit/x11.c */
extern void toolkit_x11_init(void);
extern void toolkit_x11_deinit(void);
extern x11_window_type x11_window_get_parent(x11_display_type display, x11_window_type win);
extern void x11_window_activate(x11_display_type display, x11_window_type win, uint8 switch_desktop);
extern int x11_clipboard_register_events(void);
extern int x11_clipboard_set(x11_display_type display, x11_window_type win, const char *str);
extern char *x11_clipboard_get(x11_display_type display, x11_window_type win);
/* src/loaders/map_header.c */
extern FILE *yy_map_headerin;
extern FILE *yy_map_headerout;
extern int yy_map_headerlineno;
extern int yy_map_header_flex_debug;
extern char *yy_map_headertext;
extern int map_lex_load(mapstruct *m);
extern void yy_map_headerrestart(FILE *input_file);
extern void yy_map_headerpop_buffer_state(void);
extern int yy_map_headerget_lineno(void);
extern FILE *yy_map_headerget_in(void);
extern FILE *yy_map_headerget_out(void);
extern char *yy_map_headerget_text(void);
extern void yy_map_headerset_lineno(int line_number);
extern void yy_map_headerset_in(FILE *in_str);
extern void yy_map_headerset_out(FILE *out_str);
extern int yy_map_headerget_debug(void);
extern void yy_map_headerset_debug(int bdebug);
extern int yy_map_headerlex_destroy(void);
extern void yy_map_headerfree(void *ptr);
extern int map_set_variable(mapstruct *m, char *buf);
extern void free_map_header_loader(void);
extern int load_map_header(mapstruct *m, FILE *fp);
extern void save_map_header(mapstruct *m, FILE *fp, int flag);
/* src/loaders/object.c */
extern FILE *yy_objectin;
extern FILE *yy_objectout;
extern int yy_objectlineno;
extern int yy_object_flex_debug;
extern char *yy_objecttext;
extern int lex_load(int *depth, object **items, int maxdepth, int map_flags, int linemode);
extern void yy_objectrestart(FILE *input_file);
extern void yy_objectpop_buffer_state(void);
extern int yy_objectget_lineno(void);
extern FILE *yy_objectget_in(void);
extern FILE *yy_objectget_out(void);
extern char *yy_objectget_text(void);
extern void yy_objectset_lineno(int line_number);
extern void yy_objectset_in(FILE *in_str);
extern void yy_objectset_out(FILE *out_str);
extern int yy_objectget_debug(void);
extern void yy_objectset_debug(int bdebug);
extern int yy_objectlex_destroy(void);
extern void yy_objectfree(void *ptr);
extern int yyerror(char *s);
extern void free_object_loader(void);
extern void delete_loader_buffer(void *buffer);
extern void *create_loader_buffer(void *fp);
extern int load_object(void *fp, object *op, void *mybuffer, int bufstate, int map_flags);
extern int set_variable(object *op, const char *buf);
extern void get_ob_diff(StringBuffer *sb, object *op, object *op2);
extern void save_object(FILE *fp, object *op);
/* src/loaders/random_map.c */
extern FILE *yy_random_mapin;
extern FILE *yy_random_mapout;
extern int yy_random_maplineno;
extern int yy_random_map_flex_debug;
extern char *yy_random_maptext;
extern int rmap_lex_read(RMParms *RP);
extern void yy_random_maprestart(FILE *input_file);
extern void yy_random_mappop_buffer_state(void);
extern int yy_random_mapget_lineno(void);
extern FILE *yy_random_mapget_in(void);
extern FILE *yy_random_mapget_out(void);
extern char *yy_random_mapget_text(void);
extern void yy_random_mapset_lineno(int line_number);
extern void yy_random_mapset_in(FILE *in_str);
extern void yy_random_mapset_out(FILE *out_str);
extern int yy_random_mapget_debug(void);
extern void yy_random_mapset_debug(int bdebug);
extern int yy_random_maplex_destroy(void);
extern void yy_random_mapfree(void *ptr);
extern int load_parameters(FILE *fp, int bufstate, RMParms *RP);
extern int set_random_map_variable(RMParms *rp, const char *buf);
extern void free_random_map_loader(void);
#endif
