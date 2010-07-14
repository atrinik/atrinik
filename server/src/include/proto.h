#ifndef __CPROTO__
/* commands/chat.c */
int command_say(object *op, char *params);
int command_dmsay(object *op, char *params);
int command_shout(object *op, char *params);
int command_tell(object *op, char *params);
int command_t_tell(object *op, char *params);
int command_nod(object *op, char *params);
int command_dance(object *op, char *params);
int command_kiss(object *op, char *params);
int command_bounce(object *op, char *params);
int command_smile(object *op, char *params);
int command_cackle(object *op, char *params);
int command_laugh(object *op, char *params);
int command_giggle(object *op, char *params);
int command_shake(object *op, char *params);
int command_puke(object *op, char *params);
int command_growl(object *op, char *params);
int command_scream(object *op, char *params);
int command_sigh(object *op, char *params);
int command_sulk(object *op, char *params);
int command_hug(object *op, char *params);
int command_cry(object *op, char *params);
int command_poke(object *op, char *params);
int command_accuse(object *op, char *params);
int command_grin(object *op, char *params);
int command_bow(object *op, char *params);
int command_clap(object *op, char *params);
int command_blush(object *op, char *params);
int command_burp(object *op, char *params);
int command_chuckle(object *op, char *params);
int command_cough(object *op, char *params);
int command_flip(object *op, char *params);
int command_frown(object *op, char *params);
int command_gasp(object *op, char *params);
int command_glare(object *op, char *params);
int command_groan(object *op, char *params);
int command_hiccup(object *op, char *params);
int command_lick(object *op, char *params);
int command_pout(object *op, char *params);
int command_shiver(object *op, char *params);
int command_shrug(object *op, char *params);
int command_slap(object *op, char *params);
int command_smirk(object *op, char *params);
int command_snap(object *op, char *params);
int command_sneeze(object *op, char *params);
int command_snicker(object *op, char *params);
int command_sniff(object *op, char *params);
int command_snore(object *op, char *params);
int command_spit(object *op, char *params);
int command_strut(object *op, char *params);
int command_thank(object *op, char *params);
int command_twiddle(object *op, char *params);
int command_wave(object *op, char *params);
int command_whistle(object *op, char *params);
int command_wink(object *op, char *params);
int command_yawn(object *op, char *params);
int command_beg(object *op, char *params);
int command_bleed(object *op, char *params);
int command_cringe(object *op, char *params);
int command_think(object *op, char *params);
int command_me(object *op, char *params);

/* commands/commands.c */
void init_commands();
CommArray_s *find_command_element(char *cmd, CommArray_s *commarray, int commsize);
int can_do_wiz_command(player *pl, const char *command);
int execute_newserver_command(object *pl, char *command);

/* commands/misc.c */
void map_info(object *op);
int command_motd(object *op, char *params);
void malloc_info(object *op);
int command_who(object *op, char *params);
int command_mapinfo(object *op, char *params);
int command_time(object *op, char *params);
int command_hiscore(object *op, char *params);
int command_version(object *op, char *params);
int command_praying(object *op, char *params);
int onoff_value(char *line);
void receive_player_name(object *op);
void receive_player_password(object *op);
int command_save(object *op, char *params);
int command_afk(object *op, char *params);
int command_gsay(object *op, char *params);
int command_party(object *op, char *params);
int command_whereami(object *op, char *params);
int command_ms_privacy(object *op, char *params);
int command_statistics(object *op, char *params);

/* commands/move.c */
int command_east(object *op, char *params);
int command_north(object *op, char *params);
int command_northeast(object *op, char *params);
int command_northwest(object *op, char *params);
int command_south(object *op, char *params);
int command_southeast(object *op, char *params);
int command_southwest(object *op, char *params);
int command_west(object *op, char *params);
int command_stay(object *op, char *params);
int command_turn_right(object *op, char *params);
int command_turn_left(object *op, char *params);
int command_push_object(object *op, char *params);

/* commands/new.c */
int command_run(object *op, char *params);
int command_run_stop(object *op, char *params);
void send_target_command(player *pl);
int command_combat(object *op, char *params);
int command_target(object *op, char *params);
void command_new_char(char *params, int len, player *pl);
void command_fire(char *params, int len, player *pl);
void send_mapstats_cmd(object *op, struct mapdef *map);
void send_spelllist_cmd(object *op, const char *spellname, int mode);
void send_skilllist_cmd(object *op, object *skillp, int mode);
void send_ready_skill(object *op, const char *skillname);
void generate_ext_title(player *pl);

/* commands/object.c */
object *find_best_object_match(object *pl, char *params);
int command_uskill(object *pl, char *params);
int command_rskill(object *pl, char *params);
int command_apply(object *op, char *params);
int sack_can_hold(object *pl, object *sack, object *op, int nrof);
void pick_up(object *op, object *alt);
void put_object_in_sack(object *op, object *sack, object *tmp, long nrof);
void drop_object(object *op, object *tmp, long nrof);
void drop(object *op, object *tmp);
int command_take(object *op, char *params);
int command_drop(object *op, char *params);
object *find_marked_object(object *op);
void examine_living(object *op, object *tmp);
char *long_desc(object *tmp, object *caller);
void examine(object *op, object *tmp);
int command_rename_item(object *op, char *params);

/* commands/range.c */
int command_cast_spell(object *op, char *params);
int fire_cast_spell(object *op, char *params);
int legal_range(object *op, int r);

/* commands/wiz.c */
int command_setgod(object *op, char *params);
int command_kick(object *ob, char *params);
int command_shutdown_now(object *op, char *params);
int command_goto(object *op, char *params);
int command_freeze(object *op, char *params);
int command_summon(object *op, char *params);
int command_teleport(object *op, char *params);
int command_create(object *op, char *params);
int command_inventory(object *op, char *params);
int command_dump(object *op, char *params);
int command_patch(object *op, char *params);
int command_remove(object *op, char *params);
int command_addexp(object *op, char *params);
int command_speed(object *op, char *params);
int command_stats(object *op, char *params);
int command_resetmap(object *op, char *params);
int command_nowiz(object *op, char *params);
int command_dm(object *op, char *params);
int command_learn_spell(object *op, char *params);
int command_learn_special_prayer(object *op, char *params);
int command_forget_spell(object *op, char *params);
int command_listplugins(object *op, char *params);
int command_loadplugin(object *op, char *params);
int command_unloadplugin(object *op, char *params);
void shutdown_agent(int timer, char *reason);
int command_motd_set(object *op, char *params);
int command_ban(object *op, char *params);
int command_debug(object *op, char *params);
int command_dumpbelowfull(object *op, char *params);
int command_dumpbelow(object *op, char *params);
int command_wizpass(object *op, char *params);
int command_dumpallarchetypes(object *op, char *params);
int command_dm_stealth(object *op, char *params);
int command_dm_light(object *op, char *params);
int command_dm_password(object *op, char *params);
int command_dumpactivelist(object *op, char *params);
int command_shutdown(object *op, char *params);
int command_setmaplight(object *op, char *params);
int command_dumpmap(object *op, char *params);
int command_dumpallmaps(object *op, char *params);
int command_malloc(object *op, char *params);
int command_maps(object *op, char *params);
int command_strings(object *op, char *params);
int command_ssdumptable(object *op, char *params);
int command_follow(object *op, char *params);
int command_insert_into(object *op, char *params);
int command_arrest(object *op, char *params);
int command_cmd_permission(object *op, char *params);
int command_map_save(object *op, char *params);
int command_map_reset(object *op, char *params);
int command_map_patch(object *op, char *params);
int command_no_shout(object *op, char *params);
int command_dmtake(object *op, char *params);

/* loaders/map_header.c */
int map_lex_load(mapstruct *m);
void yy_map_headerrestart(FILE *input_file);
void yy_map_headerpop_buffer_state();
int yy_map_headerget_lineno();
FILE *yy_map_headerget_in();
FILE *yy_map_headerget_out();
int yy_map_headerget_leng();
char *yy_map_headerget_text();
void yy_map_headerset_lineno(int line_number);
void yy_map_headerset_in(FILE *in_str);
void yy_map_headerset_out(FILE *out_str);
int yy_map_headerget_debug();
void yy_map_headerset_debug(int bdebug);
int yy_map_headerlex_destroy();
void yy_map_headerfree(void *ptr);
int map_set_variable(mapstruct *m, char *buf);
int load_map_header(mapstruct *m, FILE *fp);
void save_map_header(mapstruct *m, FILE *fp, int flag);

/* loaders/object.c */
int lex_load(int *depth, object **items, int maxdepth, int map_flags, int linemode);
void yy_objectrestart(FILE *input_file);
void yy_objectpop_buffer_state();
int yy_objectget_lineno();
FILE *yy_objectget_in();
FILE *yy_objectget_out();
int yy_objectget_leng();
char *yy_objectget_text();
void yy_objectset_lineno(int line_number);
void yy_objectset_in(FILE *in_str);
void yy_objectset_out(FILE *out_str);
int yy_objectget_debug();
void yy_objectset_debug(int bdebug);
int yy_objectlex_destroy();
void yy_objectfree(void *ptr);
int yyerror(char *s);
void delete_loader_buffer(void *buffer);
void *create_loader_buffer(void *fp);
int load_object(void *fp, object *op, void *mybuffer, int bufstate, int map_flags);
int set_variable(object *op, char *buf);
void get_ob_diff(StringBuffer *sb, object *op, object *op2);
void save_object(FILE *fp, object *op, int flag);

/* loaders/random_map.c */
int rmap_lex_read(RMParms *RP);
void yy_random_maprestart(FILE *input_file);
void yy_random_mappop_buffer_state();
int yy_random_mapget_lineno();
FILE *yy_random_mapget_in();
FILE *yy_random_mapget_out();
int yy_random_mapget_leng();
char *yy_random_mapget_text();
void yy_random_mapset_lineno(int line_number);
void yy_random_mapset_in(FILE *in_str);
void yy_random_mapset_out(FILE *out_str);
int yy_random_mapget_debug();
void yy_random_mapset_debug(int bdebug);
int yy_random_maplex_destroy();
void yy_random_mapfree(void *ptr);
int load_parameters(FILE *fp, int bufstate, RMParms *RP);
int set_random_map_variable(RMParms *rp, const char *buf);

/* random_maps/decor.c */
void put_decor(mapstruct *map, char **layout, RMParms *RP);

/* random_maps/door.c */
int surround_check2(char **layout, int x, int y, int Xsize, int Ysize);
void put_doors(mapstruct *the_map, char **maze, char *doorstyle, RMParms *RP);

/* random_maps/exit.c */
void find_in_layout(int mode, char target, int *fx, int *fy, char **layout, RMParms *RP);
void place_exits(mapstruct *map, char **maze, char *exitstyle, int orientation, RMParms *RP);
void unblock_exits(mapstruct *map, char **maze, RMParms *RP);

/* random_maps/expand2x.c */
char **expand2x(char **layout, int xsize, int ysize);

/* random_maps/floor.c */
mapstruct *make_map_floor(char *floorstyle, RMParms *RP);

/* random_maps/maze_gen.c */
char **maze_gen(int xsize, int ysize, int option);

/* random_maps/monster.c */
void insert_multisquare_ob_in_map(object *new_obj, mapstruct *map);
void place_monsters(mapstruct *map, char *monsterstyle, int difficulty, RMParms *RP);

/* random_maps/random_map.c */
void dump_layout(char **layout, RMParms *RP);
mapstruct *generate_random_map(char *OutFileName, RMParms *RP);
char **layoutgen(RMParms *RP);
char **symmetrize_layout(char **maze, int sym, RMParms *RP);
char **rotate_layout(char **maze, int rotation, RMParms *RP);
void roomify_layout(char **maze, RMParms *RP);
int can_make_wall(char **maze, int dx, int dy, int dir, RMParms *RP);
int make_wall(char **maze, int x, int y, int dir);
void doorify_layout(char **maze, RMParms *RP);
void write_map_parameters_to_string(char *buf, RMParms *RP);

/* random_maps/rogue_layout.c */
int surround_check(char **layout, int i, int j, int Xsize, int Ysize);
char **roguelike_layout_gen(int xsize, int ysize, int options);

/* random_maps/room_gen_onion.c */
char **map_gen_onion(int xsize, int ysize, int option, int layers);
void centered_onion(char **maze, int xsize, int ysize, int option, int layers);
void bottom_centered_onion(char **maze, int xsize, int ysize, int option, int layers);
void draw_onion(char **maze, float *xlocations, float *ylocations, int layers);
void make_doors(char **maze, float *xlocations, float *ylocations, int layers, int options);
void bottom_right_centered_onion(char **maze, int xsize, int ysize, int option, int layers);

/* random_maps/room_gen_spiral.c */
char **map_gen_spiral(int xsize, int ysize, int option);
void connect_spirals(int xsize, int ysize, int sym, char **layout);

/* random_maps/snake.c */
char **make_snake_layout(int xsize, int ysize);

/* random_maps/square_spiral.c */
void find_top_left_corner(char **maze, int *cx, int *cy);
char **make_square_spiral_layout(int xsize, int ysize);

/* random_maps/style.c */
int load_dir(const char *dir, char ***namelist, int skip_dirs);
mapstruct *load_style_map(char *style_name);
mapstruct *find_style(char *dirname, char *stylename, int difficulty);
object *pick_random_object(mapstruct *style);
void free_style_maps();

/* random_maps/wall.c */
int surround_flag(char **layout, int i, int j, RMParms *RP);
int surround_flag2(char **layout, int i, int j, RMParms *RP);
int surround_flag3(mapstruct *map, int i, int j, RMParms *RP);
int surround_flag4(mapstruct *map, int i, int j, RMParms *RP);
void make_map_walls(mapstruct *map, char **layout, char *w_style, RMParms *RP);
object *pick_joined_wall(object *the_wall, char **layout, int i, int j, RMParms *RP);
object *retrofit_joined_wall(mapstruct *the_map, int i, int j, int insert_flag, RMParms *RP);

/* server/alchemy.c */
int use_alchemy(object *op);

/* server/anim.c */
void free_all_anim();
void init_anim();
int find_animation(char *name);
void animate_object(object *op, int count);

/* server/apply.c */
void move_apply(object *trap, object *victim, object *originator, int flags);
object *find_special_prayer_mark(object *op, int spell);
void do_learn_spell(object *op, int spell, int special_prayer);
void do_forget_spell(object *op, int spell);
int manual_apply(object *op, object *tmp, int aflag);
int player_apply(object *pl, object *op, int aflag, int quiet);
void player_apply_below(object *pl);
int apply_special(object *who, object *op, int aflags);
int monster_apply_special(object *who, object *op, int aflags);

/* server/arch.c */
archetype *get_skill_archetype(int skillnr);
void init_archetypes();
void arch_info(object *op);
void dump_all_archetypes();
void free_all_archs();
object *arch_to_object(archetype *at);
object *create_singularity(const char *name);
object *get_archetype(const char *name);
archetype *find_archetype(const char *name);

/* server/attack.c */
int attack_ob(object *op, object *hitter);
int hit_player(object *op, int dam, object *hitter, int type);
int hit_map(object *op, int dir, int reduce);
int kill_object(object *op, int dam, object *hitter, int type);
object *hit_with_arrow(object *op, object *victim);
void confuse_living(object *op);
void paralyze_living(object *op, int dam);
int is_melee_range(object *hitter, object *enemy);

/* server/ban.c */
void load_bans_file();
void save_bans_file();
int checkbanned(const char *name, char *ip);
int add_ban(char *input);
int remove_ban(char *input);
void list_bans(object *op);

/* server/button.c */
void push_button(object *op);
void update_button(object *op);
void update_buttons(mapstruct *m);
void use_trigger(object *op);
int check_trigger(object *op, object *cause);
void add_button_link(object *button, mapstruct *map, int connected);
void remove_button_link(object *op);
int get_button_value(object *button);
void do_mood_floor(object *op);
object *check_inv_recursive(object *op, const object *trig);
void check_inv(object *op, object *trig);

/* server/daemon.c */
void become_daemon(char *filename);

/* server/exp.c */
uint64 level_exp(int level, double expmul);
sint64 add_exp(object *op, sint64 exp, int skill_nr);
void player_lvl_adj(object *who, object *op);
sint64 adjust_exp(object *pl, object *op, sint64 exp);
void apply_death_exp_penalty(object *op);
float calc_level_difference(int who_lvl, int op_lvl);
uint64 calculate_total_exp(object *op);

/* server/gods.c */
object *find_god(const char *name);
void pray_at_altar(object *pl, object *altar);
void become_follower(object *op, object *new_god);
const char *determine_god(object *op);
archetype *determine_holy_arch(object *god, const char *type);

/* server/hiscore.c */
void hiscore_init();
void hiscore_check(object *op, int quiet);
void hiscore_display(object *op, int max, const char *match);

/* server/holy.c */
void init_gods();
godlink *get_rand_god();
object *pntr_to_god_obj(godlink *godlnk);
void free_all_god();
void dump_gods();

/* server/image.c */
int read_bmap_names();
int find_face(char *name, int error);
void free_all_images();

/* server/info.c */
void dump_abilities();
void print_monsters();

/* server/init.c */
void free_strings();
void init_library();
void init_globals();
void write_todclock();
void init(int argc, char **argv);
void compile_info();

/* server/item.c */
char *describe_protections(object *op, int newline);
char *query_weight(object *op);
char *get_levelnumber(int i);
char *query_short_name(object *op, object *caller);
char *query_name(object *op, object *caller);
char *query_base_name(object *op, object *caller);
char *describe_item(object *op);
int need_identify(object *op);
void identify(object *op);
void set_trapped_flag(object *op);

/* server/links.c */
objectlink *get_objectlink();
void free_objectlink(objectlink *ol);
void free_objectlinkpt(objectlink *obp);
objectlink *objectlink_link(objectlink **startptr, objectlink **endptr, objectlink *afterptr, objectlink *beforeptr, objectlink *objptr);
objectlink *objectlink_unlink(objectlink **startptr, objectlink **endptr, objectlink *objptr);

/* server/living.c */
void set_attr_value(living *stats, int attr, sint8 value);
void change_attr_value(living *stats, int attr, sint8 value);
sint8 get_attr_value(living *stats, int attr);
void check_stat_bounds(living *stats);
int change_abil(object *op, object *tmp);
void drain_stat(object *op);
void drain_specific_stat(object *op, int deplete_stats);
void fix_player(object *op);
void fix_monster(object *op);
object *insert_base_info_object(object *op);
object *find_base_info_object(object *op);
void set_mobile_speed(object *op, int index);

/* server/logger.c */
void LOG(LogLevel logLevel, const char *format, ...) __attribute__((format(printf, 2, 3)));

/* server/login.c */
void emergency_save(int flag);
int check_name(player *pl, char *name);
int save_player(object *op, int flag);
void check_login(object *op);

/* server/los.c */
void init_block();
void set_block(int x, int y, int bx, int by);
void update_los(object *op);
void clear_los(object *op);
void adjust_light_source(mapstruct *map, int x, int y, int light);
void check_light_source_list(mapstruct *map);
void remove_light_source_list(mapstruct *map);
int obj_in_line_of_sight(object *obj, rv_vector *rv);

/* server/main.c */
void fatal(int err);
void version(object *op);
char *crypt_string(char *str, char *salt);
int check_password(char *typed, char *crypted);
void enter_player_savebed(object *op);
void leave_map(object *op);
void set_map_timeout(mapstruct *map);
char *clean_path(const char *file);
void enter_exit(object *op, object *exit_ob);
void process_events(mapstruct *map);
void clean_tmp_files();
void cleanup();
int swap_apartments(char *mapold, char *mapnew, int x, int y, object *op);
int main(int argc, char **argv);

/* server/map.c */
mapstruct *has_been_loaded_sh(shstr *name);
char *create_pathname(const char *name);
int check_path(const char *name, int prepend_dir);
char *normalize_path(const char *src, const char *dst, char *path);
void dump_map(mapstruct *m);
void dump_all_maps();
int wall(mapstruct *m, int x, int y);
int blocks_view(mapstruct *m, int x, int y);
int blocks_magic(mapstruct *m, int x, int y);
int blocks_cleric(mapstruct *m, int x, int y);
int blocked(object *op, mapstruct *m, int x, int y, int terrain);
int blocked_link(object *op, int xoff, int yoff);
int blocked_link_2(object *op, mapstruct *map, int x, int y);
int blocked_tile(object *op, mapstruct *m, int x, int y);
int arch_blocked(archetype *at, object *op, mapstruct *m, int x, int y);
void set_map_darkness(mapstruct *m, int value);
mapstruct *get_linked_map();
mapstruct *get_empty_map(int sizex, int sizey);
mapstruct *load_original_map(const char *filename, int flags);
int new_save_map(mapstruct *m, int flag);
void free_map(mapstruct *m, int flag);
void delete_map(mapstruct *m);
mapstruct *ready_map_name(const char *name, int flags);
void clean_tmp_map(mapstruct *m);
void free_all_maps();
void update_position(mapstruct *m, int x, int y);
void set_map_reset_time(mapstruct *map);
mapstruct *get_map_from_coord(mapstruct *m, int *x, int *y);
mapstruct *get_map_from_coord2(mapstruct *m, int *x, int *y);
int get_rangevector(object *op1, object *op2, rv_vector *retval, int flags);
int get_rangevector_from_mapcoords(mapstruct *map1, int x1, int y1, mapstruct *map2, int x2, int y2, rv_vector *retval, int flags);
int on_same_map(object *op1, object *op2);
int players_on_map(mapstruct *m);
int wall_blocked(mapstruct *m, int x, int y);

/* server/mempool.c */
uint32 nearest_pow_two_exp(uint32 n);
void setup_poolfunctions(struct mempool *pool, chunk_constructor constructor, chunk_destructor destructor);
struct mempool *create_mempool(const char *description, uint32 expand, uint32 size, uint32 flags, chunk_initialisator initialisator, chunk_deinitialisator deinitialisator, chunk_constructor constructor, chunk_destructor destructor);
void init_mempools();
void free_mempools();
void *get_poolchunk_array_real(struct mempool *pool, uint32 arraysize_exp);
void return_poolchunk_array_real(void *data, uint32 arraysize_exp, struct mempool *pool);
void dump_mempool_statistics(object *op, int *sum_used, int *sum_alloc);

/* server/move.c */
int move_ob(object *op, int dir, object *originator);
int transfer_ob(object *op, int x, int y, int randomly, object *originator, object *trap);
int teleport(object *teleporter, uint8 tele_type, object *user);
int push_ob(object *op, int dir, object *pusher);
int missile_reflection_adjust(object *op, int flag);

/* server/object.c */
void init_materials();
void mark_object_removed(object *ob);
void object_gc();
int CAN_MERGE(object *ob1, object *ob2);
object *merge_ob(object *op, object *top);
signed long sum_weight(object *op);
void add_weight(object *op, sint32 weight);
void sub_weight(object *op, sint32 weight);
object *get_env_recursive(object *op);
object *is_player_inv(object *op);
void dump_object(object *op, StringBuffer *sb);
object *get_owner(object *op);
void clear_owner(object *op);
void set_owner(object *op, object *owner);
void copy_owner(object *op, object *clone);
void initialize_object(object *op);
void copy_object(object *op2, object *op, int no_speed);
void copy_object_with_inv(object *src_ob, object *dest_ob);
object *get_object();
void update_turn_face(object *op);
void update_ob_speed(object *op);
void update_object(object *op, int action);
void drop_ob_inv(object *ob);
void destroy_object(object *ob);
void destruct_ob(object *op);
void remove_ob(object *op);
object *insert_ob_in_map(object *op, mapstruct *m, object *originator, int flag);
void replace_insert_ob_in_map(char *arch_string, object *op);
object *get_split_ob(object *orig_ob, int nr, char *err, size_t size);
object *decrease_ob_nr(object *op, uint32 i);
object *insert_ob_in_ob(object *op, object *where);
int check_walk_on(object *op, object *originator, int flags);
int check_walk_off(object *op, object *originator, int flags);
object *present_arch(archetype *at, mapstruct *m, int x, int y);
object *present(uint8 type, mapstruct *m, int x, int y);
object *present_in_ob(uint8 type, object *op);
object *present_arch_in_ob(archetype *at, object *op);
int find_free_spot(archetype *at, object *op, mapstruct *m, int x, int y, int start, int stop);
int find_first_free_spot(archetype *at, object *op, mapstruct *m, int x, int y);
int find_first_free_spot2(archetype *at, mapstruct *m, int x, int y, int start, int range);
void get_search_arr(int *search_arr);
int find_dir_2(int x, int y);
int absdir(int d);
int dirdiff(int dir1, int dir2);
int get_dir_to_target(object *op, object *target, rv_vector *range_vector);
int can_pick(object *who, object *item);
object *object_create_clone(object *asrc);
int was_destroyed(object *op, tag_t old_tag);
object *load_object_str(char *obstr);
int auto_apply(object *op);
int can_see_monsterP(mapstruct *m, int x, int y, int dir);
void free_key_values(object *op);
key_value *object_get_key_link(const object *ob, const char *key);
const char *object_get_value(const object *op, const char *const key);
int object_set_value(object *op, const char *key, const char *value, int add_key);
void init_object_initializers();
int item_matched_string(object *pl, object *op, const char *name);
int object_get_gender(object *op);

/* server/object_process.c */
object *stop_item(object *op);
void fix_stopped_item(object *op, mapstruct *map, object *originator);
void move_firewall(object *op);
void process_object(object *op);

/* server/party.c */
void add_party_member(party_struct *party, object *op);
void remove_party_member(party_struct *party, object *op);
party_struct *make_party(char *name);
void form_party(object *op, char *name);
party_struct *find_party(const char *name);
sint16 party_member_get_skill(object *op, object *skill);
int party_can_open_corpse(object *pl, object *corpse);
void party_handle_corpse(object *pl, object *corpse);
void send_party_message(party_struct *party, char *msg, int flag, object *op);
void remove_party(party_struct *party);
void PartyCmd(char *buf, int len, player *pl);

/* server/pathfinder.c */
void request_new_path(object *waypoint);
object *get_next_requested_path();
shstr *encode_path(path_node *path);
int get_path_next(shstr *buf, sint16 *off, shstr **mappath, mapstruct **map, int *x, int *y);
path_node *compress_path(path_node *path);
path_node *find_path(object *op, mapstruct *map1, int x1, int y1, mapstruct *map2, int x2, int y2);

/* server/player_shop.c */
void player_shop_open(char *data, player *pl);
void player_shop_close(player *pl);
void player_shop_load(char *data, player *pl);
void player_shop_examine(char *data, player *pl);
void player_shop_buy(char *data, player *pl);

/* server/plugins.c */
object *get_event_object(object *op, int event_nr);
CommArray_s *find_plugin_command(const char *cmd);
void display_plugins_list(object *op);
void init_plugins();
void init_plugin(const char *pluginfile);
void remove_plugin(const char *id);
void remove_plugins();
void map_event_obj_init(object *ob);
void map_event_free(map_event *tmp);
void map_event_obj_deinit(object *ob);
int trigger_map_event(int event_id, mapstruct *m, object *activator, object *other, char *text, int parm);
void trigger_global_event(int event_type, void *parm1, void *parm2);
int trigger_event(int event_type, object *const activator, object *const me, object *const other, const char *msg, int parm1, int parm2, int parm3, int flags);

/* server/porting.c */
char *tempnam_local(const char *dir, const char *pfx);
char *strdup_local(const char *str);
char *strerror_local(int errnum);
unsigned long isqrt(unsigned long n);
FILE *open_and_uncompress(const char *name, int flag, int *compressed);
void close_and_delete(FILE *fp, int compressed);
void make_path_to_file(char *filename);
const char *strcasestr_local(const char *s, const char *find);

/* server/quest.c */
void check_quest(object *op, object *quest_container);

/* server/race.c */
ob_race *race_find(shstr *name);
ob_race *race_get_random();
void race_init();
void race_dump();
void race_free();

/* server/readable.c */
int book_overflow(const char *buf1, const char *buf2, size_t booksize);
void init_readable();
object *get_random_mon();
void tailor_readable_ob(object *book, int msg_type);
void free_all_readable();

/* server/recipe.c */
recipelist *get_formulalist(int i);
void init_formulae();
void dump_alchemy();
void dump_alchemy_costs();
int strtoint(const char *buf);
artifact *locate_recipe_artifact(recipe *rp);
recipe *get_random_recipe(recipelist *rpl);
void free_all_recipes();

/* server/re-cmp.c */
const char *re_cmp(const char *str, const char *regexp);

/* server/region.c */
region *get_region_by_name(const char *region_name);
char *get_region_longname(const region *r);
char *get_region_msg(const region *r);
object *get_jail_exit(object *op);
void init_regions();
void free_regions();

/* server/rune.c */
void spring_trap(object *trap, object *victim);
int trap_see(object *op, object *trap, int level);
int trap_show(object *trap, object *where);
int trap_disarm(object *disarmer, object *trap);
void trap_adjust(object *trap, int difficulty);

/* server/shop.c */
sint64 query_cost(object *tmp, object *who, int flag);
char *cost_string_from_value(sint64 cost);
char *query_cost_string(object *tmp, object *who, int flag);
sint64 query_money(object *op);
int pay_for_amount(sint64 to_pay, object *pl);
int pay_for_item(object *op, object *pl);
int get_payment(object *pl, object *op);
void sell_item(object *op, object *pl, sint64 value);
int get_money_from_string(char *text, struct _money_block *money);
int query_money_type(object *op, int value);
sint64 remove_money_type(object *who, object *op, sint64 value, sint64 amount);
void insert_money_in_player(object *pl, object *money, uint32 nrof);
int bank_deposit(object *op, object *bank, char *text);
int bank_withdraw(object *op, object *bank, char *text);
sint64 insert_coins(object *pl, sint64 value);

/* server/shstr.c */
void init_hash_table();
shstr *add_string(const char *str);
shstr *add_refcount(shstr *str);
int query_refcount(shstr *str);
shstr *find_string(const char *str);
void free_string_shared(shstr *str);
void ss_dump_statistics(char *buf, size_t size);
void ss_dump_table(int what, char *buf, size_t size);

/* server/skills.c */
sint64 find_traps(object *pl, int level);
sint64 remove_trap(object *op);
object *find_throw_tag(object *op, tag_t tag);
void do_throw(object *op, object *toss_item, int dir);

/* server/skill_util.c */
int find_skill_exp_level(object *pl, int item_skill);
char *find_skill_exp_skillname(int item_skill);
sint64 do_skill(object *op, int dir);
sint64 calc_skill_exp(object *who, object *op, int level);
void init_new_exp_system();
void free_exp_objects();
void dump_skills();
int check_skill_known(object *op, int skillnr);
int lookup_skill_by_name(char *string);
int check_skill_to_fire(object *who);
int check_skill_to_apply(object *who, object *item);
int init_player_exp(object *pl);
void unlink_skill(object *skillop);
void link_player_skills(object *pl);
int link_player_skill(object *pl, object *skillop);
int learn_skill(object *pl, object *scroll, char *name, int skillnr, int scroll_flag);
int use_skill(object *op, char *string);
int change_skill(object *who, int sk_index);
int skill_attack(object *tmp, object *pl, int dir, char *string);
int SK_level(object *op);
object *SK_skill(object *op);
float get_skill_time(object *op, int skillnr);
int check_skill_action_time(object *op, object *skill);

/* server/spell_effect.c */
void cast_magic_storm(object *op, object *tmp, int lvl);
int recharge(object *op);
int cast_create_food(object *op, object *caster, int dir, char *stringarg);
int probe(object *op);
int cast_wor(object *op, object *caster);
int cast_create_town_portal(object *op);
int cast_destruction(object *op, object *caster, int dam, int attacktype);
int cast_heal_around(object *op, int level, int type);
int cast_heal(object *op, int level, object *target, int spell_type);
int cast_change_attr(object *op, object *caster, object *target, int spell_type);
int create_bomb(object *op, object *caster, int dir, int spell_type);
void animate_bomb(object *op);
int remove_depletion(object *op, object *target);
int remove_curse(object *op, object *target, int type, SpellTypeFrom src);
int do_cast_identify(object *tmp, object *op, int mode, int *done, int level);
int cast_identify(object *op, int level, object *single_ob, int mode);
int cast_consecrate(object *op);
int finger_of_death(object *op, object *target);
int cast_cause_disease(object *op, object *caster, int dir, archetype *disease_arch, int type);
int cast_transform_wealth(object *op);

/* server/spell_util.c */
void init_spells();
void dump_spells();
int insert_spell_effect(char *archname, mapstruct *m, int x, int y);
spell *find_spell(int spelltype);
int check_spell_known(object *op, int spell_type);
int cast_spell(object *op, object *caster, int dir, int type, int ability, SpellTypeFrom item, char *stringarg);
int cast_create_obj(object *op, object *new_op, int dir);
int fire_bolt(object *op, object *caster, int dir, int type);
int fire_arch_from_position(object *op, object *caster, sint16 x, sint16 y, int dir, archetype *at, int type, object *target);
int cast_cone(object *op, object *caster, int dir, int strength, int spell_type, archetype *spell_arch);
void cone_drop(object *op);
void move_cone(object *op);
void forklightning(object *op, object *tmp);
int reflwall(mapstruct *m, int x, int y, object *sp_op);
void move_bolt(object *op);
void explode_object(object *op);
void check_fired_arch(object *op);
void move_fired_arch(object *op);
int find_target_for_spell(object *op, object **target, uint32 flags);
int SP_level_dam_adjust(object *caster, int spell_type, int base_dam);
int SP_level_strength_adjust(object *caster, int spell_type);
int SP_level_spellpoint_cost(object *caster, int spell_type, int caster_level);
void move_swarm_spell(object *op);
void fire_swarm(object *op, object *caster, int dir, archetype *swarm_type, int spell_type, int n, int magic);

/* server/stringbuffer.c */
StringBuffer *stringbuffer_new();
char *stringbuffer_finish(StringBuffer *sb);
const char *stringbuffer_finish_shared(StringBuffer *sb);
void stringbuffer_append_string(StringBuffer *sb, const char *str);
void stringbuffer_append_printf(StringBuffer *sb, const char *format, ...) __attribute__((format(printf, 2, 3)));
void stringbuffer_append_stringbuffer(StringBuffer *sb, const StringBuffer *sb2);
size_t stringbuffer_length(StringBuffer *sb);

/* server/swap.c */
void read_map_log();
void swap_map(mapstruct *map, int force_flag);
void check_active_maps();
void flush_old_maps();

/* server/time.c */
void reset_sleep();
void sleep_delta();
void set_max_time(long t);
void get_tod(timeofday_t *tod);
void print_tod(object *op);
void time_info(object *op);
long seconds();

/* server/timers.c */
void cftimer_process_timers();
int cftimer_create(int id, long delay, object *ob, int mode);
int cftimer_destroy(int id);
int cftimer_find_free_id();
void cftimer_init();

/* server/treasure.c */
void load_treasures();
void init_artifacts();
void init_archetype_pointers();
treasurelist *find_treasurelist(const char *name);
object *generate_treasure(treasurelist *t, int difficulty, int a_chance);
void create_treasure(treasurelist *t, object *op, int flag, int difficulty, int t_style, int a_chance, int tries, struct _change_arch *arch_change);
void set_abs_magic(object *op, int magic);
int fix_generated_item(object **op_ptr, object *creator, int difficulty, int a_chance, int t_style, int max_magic, int fix_magic, int chance_magic, int flags);
artifactlist *find_artifactlist(int type);
archetype *find_artifact_archtype(const char *name);
void dump_artifacts();
void give_artifact_abilities(object *op, artifact *art);
int generate_artifact(object *op, int difficulty, int t_style, int a_chance);
void free_all_treasures();
void dump_monster_treasure(const char *name);
int get_enviroment_level(object *op);
object *create_artifact(object *op, char *artifactname);

/* server/utils.c */
int rndm(int min, int max);
int rndm_chance(uint32 n);
int look_up_spell_name(const char *spname);
void replace(const char *src, const char *key, const char *replacement, char *result, size_t resultsize);
char *cleanup_string(char *ustring);
char *get_word_from_string(char *str, int *pos);
void adjust_player_name(char *name);
void replace_unprintable_chars(char *buf);
size_t split_string(char *str, char *array[], size_t array_size, char sep);
int get_random_dir();
int get_randomized_dir(int dir);
int buf_overflow(const char *buf1, const char *buf2, size_t bufsize);
char *cleanup_chat_string(char *ustring);
char *format_number_comma(uint64 num);
void copy_file(const char *filename, FILE *fpout);
void convert_newline(char *str);

/* server/weather.c */
void init_world_darkness();
void tick_the_clock();

/* skills/construction.c */
void construction_do(object *op, int dir);

/* socket/image.c */
int is_valid_faceset(int fsn);
void free_socket_images();
void read_client_images();
void SendFaceCmd(char *buf, int len, socket_struct *ns);
int esrv_send_face(socket_struct *ns, short face_num);

/* socket/info.c */
void new_draw_info(int flags, object *pl, const char *buf);
void new_draw_info_format(int flags, object *pl, char *format, ...) __attribute__((format(printf, 3, 4)));
void new_info_map(int color, mapstruct *map, int x, int y, int dist, const char *str);
void new_info_map_except(int color, mapstruct *map, int x, int y, int dist, object *op1, object *op, const char *str);
void send_socket_message(int flags, socket_struct *ns, const char *buf);

/* socket/init.c */
void init_connection(socket_struct *ns, const char *from_ip);
void init_ericserver();
void free_all_newserver();
void free_newsocket(socket_struct *ns);
void init_srv_files();
void free_srv_files();
void send_srv_file(socket_struct *ns, int id);

/* socket/item.c */
unsigned int query_flags(object *op);
void esrv_draw_look(object *pl);
void esrv_close_container(object *op);
void esrv_send_inventory(object *pl, object *op);
void esrv_update_item(int flags, object *pl, object *op);
void esrv_send_item(object *pl, object *op);
void esrv_del_item(player *pl, int tag, object *cont);
object *esrv_get_ob_from_count(object *pl, tag_t count);
void ExamineCmd(char *buf, int len, player *pl);
void send_quickslots(player *pl);
void QuickSlotCmd(uint8 *buf, int len, player *pl);
void ApplyCmd(char *buf, int len, player *pl);
void LockItem(uint8 *data, int len, player *pl);
void MarkItem(uint8 *data, int len, player *pl);
void esrv_move_object(object *pl, tag_t to, tag_t tag, long nrof);

/* socket/loop.c */
void handle_client(socket_struct *ns, player *pl);
void watchdog();
void remove_ns_dead_player(player *pl);
void doeric_server();
void doeric_server_write();

/* socket/lowlevel.c */
void SockList_AddString(SockList *sl, char *data);
int SockList_ReadPacket(socket_struct *ns, int len);
int SockList_ReadCommand(SockList *sl, SockList *sl2);
void socket_buffer_clear(socket_struct *ns);
void socket_buffer_write(socket_struct *ns);
void Send_With_Handling(socket_struct *ns, SockList *msg);
void Write_String_To_Socket(socket_struct *ns, char cmd, char *buf, int len);

/* socket/metaserver.c */
void metaserver_info_update();
void metaserver_init();

/* socket/request.c */
void SetUp(char *buf, int len, socket_struct *ns);
void AddMeCmd(char *buf, int len, socket_struct *ns);
void PlayerCmd(char *buf, int len, player *pl);
void ReplyCmd(char *buf, int len, player *pl);
void RequestFileCmd(char *buf, int len, socket_struct *ns);
void VersionCmd(char *buf, int len, socket_struct *ns);
void MapNewmapCmd(player *pl);
void MoveCmd(char *buf, int len, player *pl);
void send_query(socket_struct *ns, uint8 flags, char *text);
void add_skill_to_skilllist(object *skill, StringBuffer *sb);
void esrv_update_skills(player *pl);
void esrv_update_stats(player *pl);
void esrv_new_player(player *pl, uint32 weight);
void draw_client_map(object *pl);
void draw_client_map2(object *pl);
void ShopCmd(char *buf, int len, player *pl);
void QuestListCmd(char *data, int len, player *pl);
void command_clear_cmds(char *buf, int len, socket_struct *ns);
void SetSound(char *buf, int len, socket_struct *ns);

/* socket/sounds.c */
void play_sound_player_only(player *pl, int type, const char *filename, int x, int y, int loop, int volume);
void play_sound_map(mapstruct *map, int type, const char *filename, int x, int y, int loop, int volume);

/* socket/updates.c */
void updates_init();
void cmd_request_update(char *buf, int len, socket_struct *ns);

/* types/altar.c */
int apply_altar(object *altar, object *sacrifice, object *originator);
int check_altar_sacrifice(object *altar, object *sacrifice);
int operate_altar(object *altar, object **sacrifice);

/* types/armour_improver.c */
void apply_armour_improver(object *op, object *tmp);

/* types/arrow.c */
object *fix_stopped_arrow(object *op);
void move_arrow(object *op);
void stop_arrow(object *op);

/* types/beacon.c */
void beacon_add(object *ob);
void beacon_remove(object *ob);
object *beacon_locate(const char *name);

/* types/book.c */
void apply_book(object *op, object *tmp);

/* types/container.c */
int esrv_apply_container(object *op, object *sack);
int container_link(player *pl, object *sack);
int container_unlink(player *pl, object *sack);
void free_container_monster(object *monster, object *op);
int check_magical_container(object *op, object *container);

/* types/converter.c */
int convert_item(object *item, object *converter);

/* types/creator.c */
void move_creator(object *op);

/* types/deep_swamp.c */
void walk_on_deep_swamp(object *op, object *victim);
void move_deep_swamp(object *op);

/* types/detector.c */
void move_detector(object *op);

/* types/disease.c */
int move_disease(object *disease);
int infect_object(object *victim, object *disease, int force);
void move_symptom(object *symptom);
void check_physically_infect(object *victim, object *hitter);
int cure_disease(object *sufferer, object *caster);
int reduce_symptoms(object *sufferer, int reduction);

/* types/door.c */
int open_door(object *op, mapstruct *m, int x, int y, int mode);
object *find_key(object *op, object *door);
void open_locked_door(object *op, object *opener);
void close_locked_door(object *op);

/* types/food.c */
void apply_food(object *op, object *tmp);
void create_food_force(object *who, object *food, object *force);
void eat_special_food(object *who, object *food);

/* types/gate.c */
void move_gate(object *op);
void move_timed_gate(object *op);

/* types/gravestone.c */
const char *gravestone_text(object *op);

/* types/identify_altar.c */
int apply_identify_altar(object *money, object *altar, object *pl);

/* types/light.c */
void apply_player_light_refill(object *who, object *op);
void apply_player_light(object *who, object *op);
void apply_lighter(object *who, object *lighter);

/* types/marker.c */
void move_marker(object *op);

/* types/monster.c */
void set_npc_enemy(object *npc, object *enemy, rv_vector *rv);
object *check_enemy(object *npc, rv_vector *rv);
object *find_enemy(object *npc, rv_vector *rv);
int move_monster(object *op);
void communicate(object *op, char *txt);
int talk_to_npc(object *op, object *npc, char *txt);
int is_friend_of(object *op, object *obj);
int check_good_weapon(object *who, object *item);
int check_good_armour(object *who, object *item);

/* types/pit.c */
void move_pit(object *op);

/* types/player.c */
player *find_player(char *plname);
void display_motd(object *op);
int playername_ok(char *cp);
void free_player(player *pl);
int add_player(socket_struct *ns);
void give_initial_items(object *pl, treasurelist *items);
void get_name(object *op);
void get_password(object *op);
void confirm_password(object *op);
object *find_arrow(object *op, const char *type);
void fire(object *op, int dir);
int move_player(object *op, int dir);
int handle_newcs_player(player *pl);
void do_some_living(object *op);
void kill_player(object *op);
void cast_dust(object *op, object *throw_ob, int dir);
int pvp_area(object *attacker, object *victim);
int player_exists(char *player_name);
object *find_skill(object *op, int skillnr);
int player_can_carry(object *pl, object *ob, uint32 nrof);
char *player_get_race_class(object *op, char *buf, size_t size);

/* types/player_mover.c */
void move_player_mover(object *op);

/* types/poison.c */
void apply_poison(object *op, object *tmp);
void poison_more(object *op);

/* types/potion.c */
int apply_potion(object *op, object *tmp);

/* types/power_crystal.c */
void apply_power_crystal(object *op, object *crystal);

/* types/rod.c */
void regenerate_rod(object *rod);
void drain_rod_charge(object *rod);
void fix_rod_speed(object *rod);

/* types/savebed.c */
void apply_savebed(object *op);

/* types/scroll.c */
void apply_scroll(object *op, object *tmp);

/* types/shop_mat.c */
int apply_shop_mat(object *shop_mat, object *op);

/* types/sign.c */
void apply_sign(object *op, object *sign);

/* types/skillscroll.c */
void apply_skillscroll(object *op, object *tmp);

/* types/spawn_point.c */
void spawn_point(object *op);

/* types/spellbook.c */
void apply_spellbook(object *op, object *tmp);

/* types/teleporter.c */
void move_teleporter(object *op);

/* types/treasure.c */
void apply_treasure(object *op, object *tmp);

/* types/waypoint.c */
object *get_active_waypoint(object *op);
object *get_aggro_waypoint(object *op);
object *get_return_waypoint(object *op);
void waypoint_compute_path(object *waypoint);
void waypoint_move(object *op, object *waypoint);

/* types/weapon_improver.c */
void apply_weapon_improver(object *op, object *tmp);
int check_weapon_power(object *who, int improvs);
#endif
