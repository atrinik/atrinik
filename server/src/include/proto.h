#ifndef __CPROTO__
/* src/commands/chat.c */
extern int command_say(object *op, char *params);
extern int command_dmsay(object *op, char *params);
extern int command_shout(object *op, char *params);
extern int command_tell(object *op, char *params);
extern int command_t_tell(object *op, char *params);
extern int command_nod(object *op, char *params);
extern int command_dance(object *op, char *params);
extern int command_kiss(object *op, char *params);
extern int command_bounce(object *op, char *params);
extern int command_smile(object *op, char *params);
extern int command_cackle(object *op, char *params);
extern int command_laugh(object *op, char *params);
extern int command_giggle(object *op, char *params);
extern int command_shake(object *op, char *params);
extern int command_puke(object *op, char *params);
extern int command_growl(object *op, char *params);
extern int command_scream(object *op, char *params);
extern int command_sigh(object *op, char *params);
extern int command_sulk(object *op, char *params);
extern int command_hug(object *op, char *params);
extern int command_cry(object *op, char *params);
extern int command_poke(object *op, char *params);
extern int command_accuse(object *op, char *params);
extern int command_grin(object *op, char *params);
extern int command_bow(object *op, char *params);
extern int command_clap(object *op, char *params);
extern int command_blush(object *op, char *params);
extern int command_burp(object *op, char *params);
extern int command_chuckle(object *op, char *params);
extern int command_cough(object *op, char *params);
extern int command_flip(object *op, char *params);
extern int command_frown(object *op, char *params);
extern int command_gasp(object *op, char *params);
extern int command_glare(object *op, char *params);
extern int command_groan(object *op, char *params);
extern int command_hiccup(object *op, char *params);
extern int command_lick(object *op, char *params);
extern int command_pout(object *op, char *params);
extern int command_shiver(object *op, char *params);
extern int command_shrug(object *op, char *params);
extern int command_slap(object *op, char *params);
extern int command_smirk(object *op, char *params);
extern int command_snap(object *op, char *params);
extern int command_sneeze(object *op, char *params);
extern int command_snicker(object *op, char *params);
extern int command_sniff(object *op, char *params);
extern int command_snore(object *op, char *params);
extern int command_spit(object *op, char *params);
extern int command_strut(object *op, char *params);
extern int command_thank(object *op, char *params);
extern int command_twiddle(object *op, char *params);
extern int command_wave(object *op, char *params);
extern int command_whistle(object *op, char *params);
extern int command_wink(object *op, char *params);
extern int command_yawn(object *op, char *params);
extern int command_beg(object *op, char *params);
extern int command_bleed(object *op, char *params);
extern int command_cringe(object *op, char *params);
extern int command_think(object *op, char *params);
extern int command_me(object *op, char *params);
extern int command_stare(object *op, char *params);
extern int command_sneer(object *op, char *params);
extern int command_wince(object *op, char *params);
extern int command_facepalm(object *op, char *params);
extern int command_my(object *op, char *params);
/* src/commands/commands.c */
extern CommArray_s Commands[];
extern const int CommandsSize;
extern CommArray_s CommunicationCommands[];
extern const int CommunicationCommandSize;
extern CommArray_s WizCommands[];
extern const int WizCommandsSize;
extern void init_commands(void);
extern CommArray_s *find_command_element(char *cmd, CommArray_s *commarray, int commsize);
extern int can_do_wiz_command(player *pl, const char *command);
extern int execute_newserver_command(object *pl, char *command);
/* src/commands/misc.c */
extern void maps_info(object *op);
extern int command_motd(object *op, char *params);
extern int command_who(object *op, char *params);
extern int command_mapinfo(object *op, char *params);
extern int command_time(object *op, char *params);
extern int command_hiscore(object *op, char *params);
extern int command_version(object *op, char *params);
extern int command_praying(object *op, char *params);
extern int onoff_value(char *line);
extern void receive_player_name(object *op);
extern void receive_player_password(object *op);
extern int command_save(object *op, char *params);
extern int command_afk(object *op, char *params);
extern int command_gsay(object *op, char *params);
extern int command_party(object *op, char *params);
extern int command_whereami(object *op, char *params);
extern int command_ms_privacy(object *op, char *params);
extern int command_statistics(object *op, char *params);
extern int command_region_map(object *op, char *params);
/* src/commands/move.c */
extern int command_east(object *op, char *params);
extern int command_north(object *op, char *params);
extern int command_northeast(object *op, char *params);
extern int command_northwest(object *op, char *params);
extern int command_south(object *op, char *params);
extern int command_southeast(object *op, char *params);
extern int command_southwest(object *op, char *params);
extern int command_west(object *op, char *params);
extern int command_stay(object *op, char *params);
extern int command_turn_right(object *op, char *params);
extern int command_turn_left(object *op, char *params);
extern int command_push_object(object *op, char *params);
/* src/commands/new.c */
extern int command_run(object *op, char *params);
extern int command_run_stop(object *op, char *params);
extern void send_target_command(player *pl);
extern int command_combat(object *op, char *params);
extern int command_target(object *op, char *params);
extern void new_chars_init(void);
extern void command_new_char(char *params, int len, player *pl);
extern void send_spelllist_cmd(object *op, const char *spellname, int mode);
extern void send_skilllist_cmd(object *op, object *skillp, int mode);
extern void send_ready_skill(object *op, const char *skillname);
extern void generate_ext_title(player *pl);
/* src/commands/object.c */
extern object *find_best_object_match(object *pl, char *params);
extern int command_uskill(object *pl, char *params);
extern int command_rskill(object *pl, char *params);
extern int command_apply(object *op, char *params);
extern int sack_can_hold(object *pl, object *sack, object *op, int nrof);
extern void pick_up(object *op, object *alt, int no_mevent);
extern void put_object_in_sack(object *op, object *sack, object *tmp, long nrof);
extern void drop_object(object *op, object *tmp, long nrof, int no_mevent);
extern void drop(object *op, object *tmp, int no_mevent);
extern int command_take(object *op, char *params);
extern int command_drop(object *op, char *params);
extern object *find_marked_object(object *op);
extern char *long_desc(object *tmp, object *caller);
extern void examine(object *op, object *tmp, StringBuffer *sb_capture);
extern int command_rename_item(object *op, char *params);
/* src/commands/range.c */
extern int command_cast_spell(object *op, char *params);
extern int fire_cast_spell(object *op, char *params);
extern int legal_range(object *op, int r);
/* src/commands/wiz.c */
extern int command_kick(object *ob, char *params);
extern int command_shutdown_now(object *op, char *params);
extern int command_goto(object *op, char *params);
extern int command_freeze(object *op, char *params);
extern int command_summon(object *op, char *params);
extern int command_teleport(object *op, char *params);
extern int command_speed(object *op, char *params);
extern int command_resetmap(object *op, char *params);
extern int command_nowiz(object *op, char *params);
extern int command_dm(object *op, char *params);
extern void shutdown_agent(int timer, char *reason);
extern int command_ban(object *op, char *params);
extern int command_debug(object *op, char *params);
extern int command_wizpass(object *op, char *params);
extern int command_dm_stealth(object *op, char *params);
extern int command_dm_light(object *op, char *params);
extern int command_dm_password(object *op, char *params);
extern int command_shutdown(object *op, char *params);
extern int command_follow(object *op, char *params);
extern int command_arrest(object *op, char *params);
extern int command_cmd_permission(object *op, char *params);
extern int command_no_shout(object *op, char *params);
extern int command_server_shout(object *op, char *params);
extern int command_mod_shout(object *op, char *params);
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
extern object *find_special_prayer_mark(object *op, int spell);
extern void do_learn_spell(object *op, int spell, int special_prayer);
extern void do_forget_spell(object *op, int spell);
extern int manual_apply(object *op, object *tmp, int aflag);
extern int player_apply(object *pl, object *op, int aflag, int quiet);
extern void player_apply_below(object *pl);
extern int apply_special(object *who, object *op, int aflags);
extern int monster_apply_special(object *who, object *op, int aflags);
/* src/server/arch.c */
extern int arch_init;
extern archetype *first_archetype;
extern archetype *wp_archetype;
extern archetype *empty_archetype;
extern archetype *base_info_archetype;
extern archetype *level_up_arch;
extern archetype *find_archetype(const char *name);
extern void arch_add(archetype *at);
extern archetype *get_skill_archetype(int skillnr);
extern void init_archetypes(void);
extern void dump_all_archetypes(void);
extern void free_all_archs(void);
extern object *arch_to_object(archetype *at);
extern object *create_singularity(const char *name);
extern object *get_archetype(const char *name);
/* src/server/attack.c */
extern char *attack_save[NROFATTACKS];
extern char *attack_name[NROFATTACKS];
extern int attack_ob(object *op, object *hitter);
extern int hit_player(object *op, int dam, object *hitter, int type);
extern int hit_map(object *op, int dir, int reduce);
extern int kill_object(object *op, int dam, object *hitter, int type);
extern object *hit_with_arrow(object *op, object *victim);
extern void confuse_living(object *op);
extern void paralyze_living(object *op, int dam);
extern int is_melee_range(object *hitter, object *enemy);
extern int spell_attack_missed(object *hitter, object *enemy);
/* src/server/ban.c */
extern void load_bans_file(void);
extern void save_bans_file(void);
extern int checkbanned(const char *name, char *ip);
extern int add_ban(char *input);
extern int remove_ban(char *input);
extern void list_bans(object *op);
/* src/server/button.c */
extern void connection_trigger(object *op, int state);
extern void push_button(object *op);
extern void update_button(object *op);
extern void update_buttons(mapstruct *m);
extern void use_trigger(object *op);
extern int check_trigger(object *op, object *cause);
extern void add_button_link(object *button, mapstruct *map, int connected);
extern void remove_button_link(object *op);
extern int get_button_value(object *button);
extern void do_mood_floor(object *op);
extern object *check_inv_recursive(object *op, const object *trig);
extern void check_inv(object *op, object *trig);
/* src/server/cache.c */
extern cache_struct *cache_find(shstr *key);
extern int cache_add(const char *key, void *ptr, uint32 flags);
extern int cache_remove(shstr *key);
extern void cache_remove_all(void);
extern void cache_remove_by_flags(uint32 flags);
/* src/server/daemon.c */
extern void become_daemon(char *filename);
/* src/server/exp.c */
extern uint64 new_levels[115 + 2];
extern _level_color level_color[201];
extern uint64 level_exp(int level, double expmul);
extern sint64 add_exp(object *op, sint64 exp, int skill_nr, int exact);
extern void player_lvl_adj(object *who, object *op);
extern sint64 adjust_exp(object *pl, object *op, sint64 exp);
extern void apply_death_exp_penalty(object *op);
extern float calc_level_difference(int who_lvl, int op_lvl);
extern uint64 calculate_total_exp(object *op);
/* src/server/gods.c */
extern object *find_god(const char *name);
extern void pray_at_altar(object *pl, object *altar);
extern void become_follower(object *op, object *new_god);
extern const char *determine_god(object *op);
extern archetype *determine_holy_arch(object *god, const char *type);
/* src/server/hiscore.c */
extern void hiscore_init(void);
extern void hiscore_check(object *op, int quiet);
extern void hiscore_display(object *op, int max, const char *match);
/* src/server/holy.c */
extern void init_gods(void);
extern godlink *get_rand_god(void);
extern object *pntr_to_god_obj(godlink *godlnk);
extern void free_all_god(void);
extern void dump_gods(void);
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
/* src/server/info.c */
extern void dump_abilities(void);
extern void print_monsters(void);
/* src/server/init.c */
extern struct Settings settings;
extern shstr_constants shstr_cons;
extern int world_darkness;
extern unsigned long todtick;
extern long init_done;
extern FILE *logfile;
extern long nroftreasures;
extern long nrofartifacts;
extern long nrofallowedstr;
extern char first_map_path[256];
extern void free_strings(void);
extern void init_library(void);
extern void init_globals(void);
extern void write_todclock(void);
extern void init(int argc, char **argv);
extern void compile_info(void);
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
extern objectlink *get_objectlink(void);
extern void free_objectlink(objectlink *ol);
extern void free_objectlinkpt(objectlink *obp);
extern objectlink *objectlink_link(objectlink **startptr, objectlink **endptr, objectlink *afterptr, objectlink *beforeptr, objectlink *objptr);
extern objectlink *objectlink_unlink(objectlink **startptr, objectlink **endptr, objectlink *objptr);
/* src/server/living.c */
extern int dam_bonus[30 + 1];
extern int thaco_bonus[30 + 1];
extern float cha_bonus[30 + 1];
extern float speed_bonus[30 + 1];
extern uint32 weight_limit[30 + 1];
extern int learn_spell[30 + 1];
extern int cleric_chance[30 + 1];
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
/* src/server/logger.c */
extern long nroferrors;
extern void LOG(LogLevel logLevel, const char *format, ...) __attribute__((format(printf, 2, 3)));
/* src/server/login.c */
extern void emergency_save(int flag);
extern int check_name(player *pl, char *name);
extern int save_player(object *op, int flag);
extern void check_login(object *op);
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
extern godlink *first_god;
extern player *last_player;
extern uint32 global_round_tag;
extern void fatal(int err);
extern void version(object *op);
extern char *crypt_string(char *str, char *salt);
extern int check_password(char *typed, char *crypted);
extern void enter_player_savebed(object *op);
extern void leave_map(object *op);
extern void set_map_timeout(mapstruct *map);
extern char *clean_path(const char *file);
extern void enter_exit(object *op, object *exit_ob);
extern void process_events(mapstruct *map);
extern void clean_tmp_files(void);
extern void cleanup(void);
extern int swap_apartments(const char *mapold, const char *mapnew, int x, int y, object *op);
extern int main(int argc, char **argv);
/* src/server/map.c */
extern int global_darkness_table[7 + 1];
extern int map_tiled_reverse[8];
extern mapstruct *has_been_loaded_sh(shstr *name);
extern char *create_pathname(const char *name);
extern int check_path(const char *name, int prepend_dir);
extern char *normalize_path(const char *src, const char *dst, char *path);
extern int wall(mapstruct *m, int x, int y);
extern int blocks_view(mapstruct *m, int x, int y);
extern int blocks_magic(mapstruct *m, int x, int y);
extern int blocks_cleric(mapstruct *m, int x, int y);
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
extern int get_rangevector_from_mapcoords(mapstruct *map1, int x1, int y1, mapstruct *map2, int x2, int y2, rv_vector *retval, int flags);
extern int on_same_map(object *op1, object *op2);
extern int players_on_map(mapstruct *m);
extern int wall_blocked(mapstruct *m, int x, int y);
extern void SockList_AddMapName(SockList *sl, object *pl, mapstruct *map, object *map_info);
extern void SockList_AddMapMusic(SockList *sl, object *pl, mapstruct *map, object *map_info);
extern void SockList_AddMapWeather(SockList *sl, object *pl, mapstruct *map, object *map_info);
/* src/server/mempool.c */
extern struct mempool_chunk end_marker;
extern int nrof_mempools;
extern struct mempool *mempools[32];
extern struct mempool *pool_object;
extern struct mempool *pool_objectlink;
extern struct mempool *pool_player;
extern struct mempool *pool_bans;
extern struct mempool *pool_parties;
extern uint32 nearest_pow_two_exp(uint32 n);
extern void setup_poolfunctions(struct mempool *pool, chunk_constructor constructor, chunk_destructor destructor);
extern struct mempool *create_mempool(const char *description, uint32 expand, uint32 size, uint32 flags, chunk_initialisator initialisator, chunk_deinitialisator deinitialisator, chunk_constructor constructor, chunk_destructor destructor);
extern void init_mempools(void);
extern void free_mempools(void);
extern void *get_poolchunk_array_real(struct mempool *pool, uint32 arraysize_exp);
extern void return_poolchunk_array_real(void *data, uint32 arraysize_exp, struct mempool *pool);
/* src/server/move.c */
extern int move_ob(object *op, int dir, object *originator);
extern int transfer_ob(object *op, int x, int y, int randomly, object *originator, object *trap);
extern int teleport(object *teleporter, uint8 tele_type, object *user);
extern int push_ob(object *op, int dir, object *pusher);
extern int missile_reflection_adjust(object *op, int flag);
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
extern object *merge_ob(object *op, object *top);
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
extern object *get_object(void);
extern void update_turn_face(object *op);
extern void update_ob_speed(object *op);
extern void update_object(object *op, int action);
extern void drop_ob_inv(object *ob);
extern void object_destroy(object *ob);
extern void destruct_ob(object *op);
extern void remove_ob(object *op);
extern object *insert_ob_in_map(object *op, mapstruct *m, object *originator, int flag);
extern int object_check_move_on(object *op, object *originator);
extern void replace_insert_ob_in_map(char *arch_string, object *op);
extern object *get_split_ob(object *orig_ob, int nr, char *err, size_t size);
extern object *decrease_ob_nr(object *op, uint32 i);
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
/* src/server/object_methods.c */
extern object_methods object_type_methods[160];
extern object_methods object_methods_base;
extern void object_methods_init_one(object_methods *methods);
extern void object_methods_init(void);
extern int object_apply(object *op, object *applier, int aflags);
extern void object_process(object *op);
extern char *object_describe(object *op, object *observer, char *buf, size_t size);
extern int object_move_on(object *op, object *victim, object *originator);
extern int object_trigger(object *op, object *cause, int state);
extern object *stop_item(object *op);
extern void fix_stopped_item(object *op, mapstruct *map, object *originator);
/* src/server/party.c */
extern const char *const party_loot_modes[PARTY_LOOT_MAX];
extern const char *const party_loot_modes_help[PARTY_LOOT_MAX];
extern party_struct *first_party;
extern void add_party_member(party_struct *party, object *op);
extern void remove_party_member(party_struct *party, object *op);
extern void form_party(object *op, const char *name);
extern party_struct *find_party(const char *name);
extern sint16 party_member_get_skill(object *op, object *skill);
extern int party_can_open_corpse(object *pl, object *corpse);
extern void party_handle_corpse(object *pl, object *corpse);
extern void send_party_message(party_struct *party, const char *msg, int flag, object *op);
extern void remove_party(party_struct *party);
extern void party_update_who(player *pl);
/* src/server/pathfinder.c */
extern void request_new_path(object *waypoint);
extern object *get_next_requested_path(void);
extern shstr *encode_path(path_node *path);
extern int get_path_next(shstr *buf, sint16 *off, shstr **mappath, mapstruct **map, int *x, int *y);
extern path_node *compress_path(path_node *path);
extern path_node *find_path(object *op, mapstruct *map1, int x1, int y1, mapstruct *map2, int x2, int y2);
/* src/server/plugins.c */
extern struct plugin_hooklist hooklist;
extern object *get_event_object(object *op, int event_nr);
extern CommArray_s *find_plugin_command(const char *cmd);
extern void display_plugins_list(object *op);
extern void init_plugins(void);
extern void init_plugin(const char *pluginfile);
extern void remove_plugin(const char *id);
extern void remove_plugins(void);
extern void map_event_obj_init(object *ob);
extern void map_event_free(map_event *tmp);
extern void map_event_obj_deinit(object *ob);
extern int trigger_map_event(int event_id, mapstruct *m, object *activator, object *other, object *other2, const char *text, int parm);
extern void trigger_global_event(int event_type, void *parm1, void *parm2);
extern int trigger_event(int event_type, object *const activator, object *const me, object *const other, const char *msg, int parm1, int parm2, int parm3, int flags);
/* src/server/porting.c */
extern char *tempnam_local(const char *dir, const char *pfx);
extern char *strdup_local(const char *str);
extern char *strerror_local(int errnum);
extern unsigned long isqrt(unsigned long n);
extern char *uncomp[4][3];
extern FILE *open_and_uncompress(const char *name, int flag, int *compressed);
extern void close_and_delete(FILE *fp, int compressed);
extern void make_path_to_file(char *filename);
extern const char *strcasestr_local(const char *s, const char *find);
/* src/server/quest.c */
extern void check_quest(object *op, object *quest_container);
/* src/server/race.c */
extern const char *item_races[13];
extern ob_race *race_find(shstr *name);
extern ob_race *race_get_random(void);
extern void race_init(void);
extern void race_dump(void);
extern void race_free(void);
/* src/server/readable.c */
extern int book_overflow(const char *buf1, const char *buf2, size_t booksize);
extern void init_readable(void);
extern object *get_random_mon(void);
extern void tailor_readable_ob(object *book, int msg_type);
extern void free_all_readable(void);
/* src/server/re-cmp.c */
extern const char *re_cmp(const char *str, const char *regexp);
/* src/server/region.c */
extern region *first_region;
extern region *get_region_by_name(const char *region_name);
extern char *get_region_longname(const region *r);
extern char *get_region_msg(const region *r);
extern object *get_jail_exit(object *op);
extern void init_regions(void);
extern void free_regions(void);
/* src/server/rune.c */
extern void spring_trap(object *trap, object *victim);
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
/* src/server/shstr.c */
extern void init_hash_table(void);
extern shstr *add_string(const char *str);
extern shstr *add_refcount(shstr *str);
extern int query_refcount(shstr *str);
extern shstr *find_string(const char *str);
extern void free_string_shared(shstr *str);
extern void ss_dump_statistics(char *buf, size_t size);
extern void ss_dump_table(int what, char *buf, size_t size);
/* src/server/skills.c */
extern skill_struct skills[NROFSKILLS];
extern sint64 find_traps(object *pl, int level);
extern sint64 remove_trap(object *op);
extern object *find_throw_tag(object *op);
extern void do_throw(object *op, object *toss_item, int dir);
/* src/server/skill_util.c */
extern float stat_exp_mult[30 + 1];
extern int find_skill_exp_level(object *pl, int item_skill);
extern char *find_skill_exp_skillname(int item_skill);
extern sint64 do_skill(object *op, int dir, const char *params);
extern sint64 calc_skill_exp(object *who, object *op, int level);
extern void init_new_exp_system(void);
extern void free_exp_objects(void);
extern void dump_skills(void);
extern int check_skill_known(object *op, int skillnr);
extern int lookup_skill_by_name(const char *string);
extern int check_skill_to_fire(object *who);
extern int check_skill_to_apply(object *who, object *item);
extern int init_player_exp(object *pl);
extern void unlink_skill(object *skillop);
extern void link_player_skills(object *pl);
extern int link_player_skill(object *pl, object *skillop);
extern int learn_skill(object *pl, object *scroll, char *name, int skillnr, int scroll_flag);
extern int use_skill(object *op, char *string);
extern int change_skill(object *who, int sk_index);
extern int skill_attack(object *tmp, object *pl, int dir, char *string);
extern int SK_level(object *op);
extern object *SK_skill(object *op);
extern float get_skill_time(object *op, int skillnr);
extern int check_skill_action_time(object *op, object *skill);
/* src/server/spell_effect.c */
extern void cast_magic_storm(object *op, object *tmp, int lvl);
extern int recharge(object *op);
extern int cast_create_food(object *op, object *caster, int dir, const char *stringarg);
extern int probe(object *op);
extern int cast_wor(object *op, object *caster);
extern int cast_destruction(object *op, object *caster, int dam, int attacktype);
extern int cast_heal_around(object *op, int level, int type);
extern int cast_heal(object *op, int level, object *target, int spell_type);
extern int cast_change_attr(object *op, object *caster, object *target, int spell_type);
extern int remove_depletion(object *op, object *target);
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
extern void dump_spells(void);
extern int insert_spell_effect(char *archname, mapstruct *m, int x, int y);
extern spell_struct *find_spell(int spelltype);
extern int check_spell_known(object *op, int spell_type);
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
/* src/server/stringbuffer.c */
extern StringBuffer *stringbuffer_new(void);
extern char *stringbuffer_finish(StringBuffer *sb);
extern const char *stringbuffer_finish_shared(StringBuffer *sb);
extern void stringbuffer_append_string(StringBuffer *sb, const char *str);
extern void stringbuffer_append_printf(StringBuffer *sb, const char *format, ...) __attribute__((format(printf, 2, 3)));
extern void stringbuffer_append_stringbuffer(StringBuffer *sb, const StringBuffer *sb2);
extern size_t stringbuffer_length(StringBuffer *sb);
/* src/server/swap.c */
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
extern void dump_artifacts(void);
extern void give_artifact_abilities(object *op, artifact *art);
extern int generate_artifact(object *op, int difficulty, int t_style, int a_chance);
extern void free_all_treasures(void);
extern void dump_monster_treasure(const char *name);
extern int get_environment_level(object *op);
extern object *create_artifact(object *op, char *artifactname);
/* src/server/utils.c */
extern int rndm(int min, int max);
extern int rndm_chance(uint32 n);
extern int look_up_spell_name(const char *spname);
extern void replace(const char *src, const char *key, const char *replacement, char *result, size_t resultsize);
extern char *cleanup_string(char *ustring);
extern const char *get_word_from_string(const char *str, int *pos);
extern void adjust_player_name(char *name);
extern void replace_unprintable_chars(char *buf);
extern size_t split_string(char *str, char *array[], size_t array_size, char sep);
extern int get_random_dir(void);
extern int get_randomized_dir(int dir);
extern int buf_overflow(const char *buf1, const char *buf2, size_t bufsize);
extern char *cleanup_chat_string(char *ustring);
extern char *format_number_comma(uint64 num);
extern void copy_file(const char *filename, FILE *fpout);
extern void convert_newline(char *str);
extern void string_remove_markup(char *str);
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
extern void SendFaceCmd(char *buf, int len, socket_struct *ns);
extern int esrv_send_face(socket_struct *ns, short face_num);
extern void face_get_data(int face, uint8 **ptr, uint16 *len);
/* src/socket/info.c */
extern void draw_info_full(int flags, const char *color, StringBuffer *sb_capture, object *pl, const char *buf);
extern void draw_info_full_format(int flags, const char *color, StringBuffer *sb_capture, object *pl, const char *format, ...) __attribute__((format(printf, 5, 6)));
extern void draw_info_flags(int flags, const char *color, object *pl, const char *buf);
extern void draw_info_flags_format(int flags, const char *color, object *pl, const char *format, ...) __attribute__((format(printf, 4, 5)));
extern void draw_info(const char *color, object *pl, const char *buf);
extern void draw_info_format(const char *color, object *pl, const char *format, ...) __attribute__((format(printf, 3, 4)));
extern void draw_info_map(int flags, const char *color, mapstruct *map, int x, int y, int dist, object *op, object *op2, const char *buf);
extern void send_socket_message(const char *color, socket_struct *ns, const char *buf);
/* src/socket/init.c */
extern _srv_client_files SrvClientFiles[SRV_CLIENT_FILES];
extern Socket_Info socket_info;
extern socket_struct *init_sockets;
extern void init_connection(socket_struct *ns, const char *from_ip);
extern void init_ericserver(void);
extern void free_all_newserver(void);
extern void free_newsocket(socket_struct *ns);
extern void init_srv_files(void);
extern void free_srv_files(void);
extern void send_srv_file(socket_struct *ns, int id);
/* src/socket/item.c */
extern unsigned int query_flags(object *op);
extern void esrv_draw_look(object *pl);
extern void esrv_close_container(object *op);
extern void esrv_send_inventory(object *pl, object *op);
extern void esrv_update_item(int flags, object *op);
extern void esrv_send_item(object *op);
extern void esrv_del_item(object *op);
extern object *esrv_get_ob_from_count(object *pl, tag_t count);
extern void ExamineCmd(char *buf, int len, player *pl);
extern void send_quickslots(player *pl);
extern void QuickSlotCmd(uint8 *buf, int len, player *pl);
extern void ApplyCmd(char *buf, int len, player *pl);
extern void LockItem(uint8 *data, int len, player *pl);
extern void MarkItem(uint8 *data, int len, player *pl);
extern void esrv_move_object(object *pl, tag_t to, tag_t tag, long nrof);
extern void cmd_ready_send(player *pl, tag_t tag, int type);
extern int cmd_ready_determine(object *tmp);
extern void cmd_ready_clear(object *op, int type);
/* src/socket/loop.c */
extern void handle_client(socket_struct *ns, player *pl);
extern void watchdog(void);
extern void remove_ns_dead_player(player *pl);
extern void doeric_server(void);
extern void doeric_server_write(void);
/* src/socket/lowlevel.c */
extern void SockList_AddString(SockList *sl, const char *data);
extern char *GetString_String(uint8 *data, int len, int *pos, char *dest, size_t dest_size);
extern int SockList_ReadPacket(socket_struct *ns, int len);
extern int SockList_ReadCommand(SockList *sl, SockList *sl2);
extern void socket_enable_no_delay(int fd);
extern void socket_disable_no_delay(int fd);
extern void socket_buffer_clear(socket_struct *ns);
extern void socket_buffer_write(socket_struct *ns);
extern void Send_With_Handling(socket_struct *ns, SockList *msg);
extern void Write_String_To_Socket(socket_struct *ns, char cmd, const char *buf, int len);
/* src/socket/metaserver.c */
extern void metaserver_info_update(void);
extern void metaserver_init(void);
/* src/socket/request.c */
extern void SetUp(char *buf, int len, socket_struct *ns);
extern void AddMeCmd(char *buf, int len, socket_struct *ns);
extern void PlayerCmd(char *buf, int len, player *pl);
extern void ReplyCmd(char *buf, int len, player *pl);
extern void RequestFileCmd(char *buf, int len, socket_struct *ns);
extern void VersionCmd(char *buf, int len, socket_struct *ns);
extern void MoveCmd(char *buf, int len, player *pl);
extern void send_query(socket_struct *ns, uint8 flags, char *text);
extern void add_skill_to_skilllist(object *skill, StringBuffer *sb);
extern void esrv_update_skills(player *pl);
extern void esrv_update_stats(player *pl);
extern void esrv_new_player(player *pl, uint32 weight);
extern void draw_client_map(object *pl);
extern void draw_client_map2(object *pl);
extern void ShopCmd(char *buf, int len, player *pl);
extern void QuestListCmd(char *data, int len, player *pl);
extern void command_clear_cmds(char *buf, int len, socket_struct *ns);
extern void SetSound(char *buf, int len, socket_struct *ns);
extern void command_move_path(uint8 *buf, int len, player *pl);
extern void cmd_ready(uint8 *buf, int len, player *pl);
extern void command_fire(uint8 *buf, int len, player *pl);
extern void cmd_keepalive(char *buf, int len, socket_struct *ns);
extern void cmd_password_change(uint8 *buf, int len, player *pl);
/* src/socket/sounds.c */
extern void play_sound_player_only(player *pl, int type, const char *filename, int x, int y, int loop, int volume);
extern void play_sound_map(mapstruct *map, int type, const char *filename, int x, int y, int loop, int volume);
/* src/socket/updates.c */
extern void updates_init(void);
extern void cmd_request_update(char *buf, int len, socket_struct *ns);
/* src/types/common/apply.c */
extern int common_object_apply(object *op, object *applier, int aflags);
/* src/types/common/describe.c */
extern void common_object_describe(object *op, object *observer, char *buf, size_t size);
/* src/types/common/process.c */
extern int common_object_process(object *op);
/* src/types/altar.c */
extern int apply_altar(object *altar, object *sacrifice, object *originator);
extern int check_altar_sacrifice(object *altar, object *sacrifice);
extern int operate_altar(object *altar, object **sacrifice);
/* src/types/arrow.c */
extern sint32 bow_get_ws(object *bow, object *arrow);
extern sint16 arrow_get_wc(object *op, object *bow, object *arrow);
extern sint16 arrow_get_damage(object *op, object *bow, object *arrow);
extern int bow_get_skill(object *bow);
extern object *arrow_find(object *op, shstr *type);
extern void bow_fire(object *op, int dir);
extern object *fix_stopped_arrow(object *op);
extern void move_arrow(object *op);
extern void stop_arrow(object *op);
/* src/types/beacon.c */
extern void beacon_add(object *ob);
extern void beacon_remove(object *ob);
extern object *beacon_locate(shstr *name);
/* src/types/book.c */
extern void object_type_init_book(void);
/* src/types/bullet.c */
extern int bullet_reflect(object *op, mapstruct *m, int x, int y);
extern void object_type_init_bullet(void);
/* src/types/cone.c */
extern void object_type_init_cone(void);
/* src/types/container.c */
extern int esrv_apply_container(object *op, object *sack);
extern int container_link(player *pl, object *sack);
extern int container_unlink(player *pl, object *sack);
extern void free_container_monster(object *monster, object *op);
extern int check_magical_container(object *op, object *container);
/* src/types/creator.c */
extern void object_type_init_creator(void);
/* src/types/detector.c */
extern void object_type_init_detector(void);
/* src/types/disease.c */
extern int move_disease(object *disease);
extern int infect_object(object *victim, object *disease, int force);
extern void move_symptom(object *symptom);
extern void check_physically_infect(object *victim, object *hitter);
extern int cure_disease(object *sufferer, object *caster);
extern int reduce_symptoms(object *sufferer, int reduction);
/* src/types/door.c */
extern int door_try_open(object *op, mapstruct *m, int x, int y, int test);
extern object *find_key(object *op, object *door);
extern void object_type_init_door(void);
/* src/types/duplicator.c */
extern void object_type_init_duplicator(void);
/* src/types/firewall.c */
extern void object_type_init_firewall(void);
/* src/types/food.c */
extern void apply_food(object *op, object *tmp);
extern void create_food_force(object *who, object *food, object *force);
extern void eat_special_food(object *who, object *food);
/* src/types/gate.c */
extern void move_gate(object *op);
extern void move_timed_gate(object *op);
/* src/types/gravestone.c */
extern const char *gravestone_text(object *op);
/* src/types/light.c */
extern void apply_player_light_refill(object *who, object *op);
extern void apply_player_light(object *who, object *op);
/* src/types/lightning.c */
extern void object_type_init_lightning(void);
/* src/types/magic_mirror.c */
extern void magic_mirror_init(object *mirror);
extern void magic_mirror_deinit(object *mirror);
extern mapstruct *magic_mirror_get_map(object *mirror);
/* src/types/map_info.c */
extern void map_info_init(object *info);
/* src/types/marker.c */
extern void object_type_init_marker(void);
/* src/types/monster.c */
extern void set_npc_enemy(object *npc, object *enemy, rv_vector *rv);
extern object *check_enemy(object *npc, rv_vector *rv);
extern object *find_enemy(object *npc, rv_vector *rv);
extern int move_monster(object *op);
extern void communicate(object *op, char *txt);
extern int talk_to_npc(object *op, object *npc, char *txt);
extern int faction_is_friend_of(object *mon, object *pl);
extern int is_friend_of(object *op, object *obj);
extern int check_good_weapon(object *who, object *item);
extern int check_good_armour(object *who, object *item);
/* src/types/pit.c */
extern void object_type_init_pit(void);
/* src/types/player.c */
extern player *find_player(const char *plname);
extern void display_motd(object *op);
extern int playername_ok(char *cp);
extern void free_player(player *pl);
extern int add_player(socket_struct *ns);
extern void give_initial_items(object *pl, treasurelist *items);
extern void get_name(object *op);
extern void get_password(object *op);
extern void confirm_password(object *op);
extern object *find_arrow(object *op, const char *type);
extern void fire(object *op, int dir);
extern int move_player(object *op, int dir);
extern int handle_newcs_player(player *pl);
extern void do_some_living(object *op);
extern void kill_player(object *op);
extern void cast_dust(object *op, object *throw_ob, int dir);
extern int pvp_area(object *attacker, object *victim);
extern int player_exists(char *player_name);
extern object *find_skill(object *op, int skillnr);
extern int player_can_carry(object *pl, uint32 weight);
extern char *player_get_race_class(object *op, char *buf, size_t size);
extern void player_path_add(player *pl, mapstruct *map, sint16 x, sint16 y);
extern void player_path_clear(player *pl);
extern void player_path_handle(player *pl);
extern sint64 player_faction_reputation(player *pl, shstr *faction);
extern void player_faction_reputation_update(player *pl, shstr *faction, sint64 add);
extern int player_has_region_map(player *pl, region *r);
/* src/types/player_mover.c */
extern void object_type_init_playermover(void);
/* src/types/poison.c */
extern void apply_poison(object *op, object *tmp);
extern void poison_more(object *op);
/* src/types/potion.c */
extern int apply_potion(object *op, object *tmp);
/* src/types/power_crystal.c */
extern void apply_power_crystal(object *op, object *crystal);
/* src/types/rod.c */
extern void drain_rod_charge(object *rod);
extern void fix_rod_speed(object *rod);
extern void object_type_init_rod(void);
/* src/types/savebed.c */
extern void apply_savebed(object *op);
/* src/types/scroll.c */
extern void apply_scroll(object *op, object *tmp);
/* src/types/shop_mat.c */
extern void object_type_init_shop_mat(void);
/* src/types/sign.c */
extern void object_type_init_sign(void);
/* src/types/sound_ambient.c */
extern void sound_ambient_init(object *ob);
/* src/types/spawn_point.c */
extern void object_type_init_spawn_point(void);
/* src/types/swarm_spell.c */
extern void object_type_init_swarm_spell(void);
/* src/types/teleporter.c */
extern void object_type_init_teleporter(void);
/* src/types/waypoint.c */
extern object *get_active_waypoint(object *op);
extern object *get_aggro_waypoint(object *op);
extern object *get_return_waypoint(object *op);
extern void waypoint_compute_path(object *waypoint);
extern void waypoint_move(object *op, object *waypoint);
/* src/loaders/map_header.c */
extern int yy_map_headerleng;
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
extern int yy_map_headerget_leng(void);
extern char *yy_map_headerget_text(void);
extern void yy_map_headerset_lineno(int line_number);
extern void yy_map_headerset_in(FILE *in_str);
extern void yy_map_headerset_out(FILE *out_str);
extern int yy_map_headerget_debug(void);
extern void yy_map_headerset_debug(int bdebug);
extern int yy_map_headerlex_destroy(void);
extern void yy_map_headerfree(void *ptr);
extern int map_set_variable(mapstruct *m, char *buf);
extern int load_map_header(mapstruct *m, FILE *fp);
extern void save_map_header(mapstruct *m, FILE *fp, int flag);
/* src/loaders/object.c */
extern int yy_objectleng;
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
extern int yy_objectget_leng(void);
extern char *yy_objectget_text(void);
extern void yy_objectset_lineno(int line_number);
extern void yy_objectset_in(FILE *in_str);
extern void yy_objectset_out(FILE *out_str);
extern int yy_objectget_debug(void);
extern void yy_objectset_debug(int bdebug);
extern int yy_objectlex_destroy(void);
extern void yy_objectfree(void *ptr);
extern int yyerror(char *s);
extern void delete_loader_buffer(void *buffer);
extern void *create_loader_buffer(void *fp);
extern int load_object(void *fp, object *op, void *mybuffer, int bufstate, int map_flags);
extern int set_variable(object *op, char *buf);
extern void get_ob_diff(StringBuffer *sb, object *op, object *op2);
extern void save_object(FILE *fp, object *op);
/* src/loaders/random_map.c*/
extern int yy_random_mapleng;
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
extern int yy_random_mapget_leng(void);
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
#endif
