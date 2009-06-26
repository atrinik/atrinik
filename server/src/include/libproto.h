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
* the Free Software Foundation; either version 3 of the License, or     *
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

/* anim.c */
extern void free_all_anim(void);
extern void init_anim(void);
extern int find_animation(char *name);
extern void animate_object(object *op, int count);

/* arch.c */
extern archetype *find_archetype_by_object_name(const char *name);
extern object *get_archetype_by_object_name(const char *name);
extern archetype *get_skill_archetype(int skillnr);
extern int item_matched_string(object *pl, object *op, const char *name);
extern void init_archetypes(void);
extern void arch_info(object *op);
extern void clear_archetable(void);
extern void init_archetable(void);
extern void dump_arch(archetype *at);
extern void dump_all_archetypes(void);
extern void free_all_archs(void);
extern archetype *get_archetype_struct(void);
extern void first_arch_pass(FILE *fp);
extern void second_arch_pass(FILE *fp_start);
extern void check_generators(void);
extern void load_archetypes(void);
extern object *arch_to_object(archetype *at);
extern object *create_singularity(const char *name);
extern object *get_archetype(const char *name);
extern unsigned long hasharch(const char *str, int tablesize);
extern archetype *find_archetype(const char *name);
extern void add_arch(archetype *at);
extern archetype *type_to_archetype(int type);
extern object *clone_arch(int type);
extern object *ObjectCreateArch(archetype *at);

/* button.c */
extern void push_button(object *op);
extern void update_button(object *op);
extern void update_buttons(mapstruct *m);
extern void use_trigger(object *op);
extern void animate_turning(object *op);
extern int check_altar_sacrifice(object *altar, object *sacrifice);
extern int operate_altar(object *altar, object **sacrifice);
extern void trigger_move(object *op, int state);
extern int check_trigger(object *op, object *cause);
extern void add_button_link(object *button, mapstruct *map, int connected);
extern void remove_button_link(object *op);
extern objectlink *get_button_links(object *button);
extern int get_button_value(object *button);
extern void do_mood_floor(object *op, object *op2);
extern object *check_inv_recursive(object *op, const object *trig);
extern void check_inv(object *op, object *trig);
extern void verify_button_links(mapstruct *map);

/* database.c */
extern int db_open(char *file, sqlite3 **db);
extern int db_prepare(sqlite3 *db, const char *sql, sqlite3_stmt **statement);
extern int db_prepare_format(sqlite3 *db, sqlite3_stmt **statement, const char *format, ...);
extern int db_step(sqlite3_stmt *statement);
extern sqlite3_value *db_column_value(sqlite3_stmt *statement, int col);
extern const unsigned char *db_column_text(sqlite3_stmt *statement, int col);
extern int db_finalize(sqlite3_stmt *statement);
extern int db_close(sqlite3 *db);
extern char *db_sanitize_input(char *sql_input);
extern const char *db_errmsg(sqlite3* db);

/* exp.c */
extern uint32 level_exp(int level, double expmul);
extern sint32 add_exp(object *op, int exp, int skill_nr);
extern void player_lvl_adj(object *who, object *op);
extern void calc_perm_exp(object *op);
extern int adjust_exp(object *pl, object *op, int exp);
extern void apply_death_exp_penalty(object *op);
extern float calc_level_difference(int who_lvl, int op_lvl);

/* friend.c */
extern void add_friendly_object(object *op);
extern void remove_friendly_object(object *op);
extern void dump_friendly_objects(void);
extern void clean_friendly_list(void);

/* glue.c */
extern void init_function_pointers(void);
extern void set_emergency_save(type_func_int addr);
extern void set_clean_tmp_files(type_func_void addr);
extern void set_remove_friendly_object(type_func_ob addr);
extern void set_update_buttons(type_func_map addr);
extern void set_draw_info(type_func_int_int_ob_cchar addr);
extern void set_container_unlink(type_container_unlink_func addr);
extern void set_move_apply(type_move_apply_func addr);
extern void set_monster_check_apply(type_func_ob_ob addr);
extern void set_init_blocksview_players(type_func_void addr);
extern void set_info_map(type_func_int_map_int_int_int_char addr);
extern void set_move_teleporter(type_func_ob addr);
extern void set_move_firewall(type_func_ob addr);
extern void set_trap_adjust(type_func_ob_int addr);
extern void set_move_creator(type_func_ob addr);
extern void set_esrv_send_item(type_func_ob_ob addr);
extern void set_esrv_update_item(type_func_int_ob_ob addr);
extern void set_esrv_del_item(type_func_player_int_ob addr);
extern void set_dragon_gain_func(type_func_dragon_gain addr);
extern void set_send_golem_control_func(type_func_ob_int addr);
extern void fatal(int err);
extern void dummy_function_int(int i);
extern void dummy_function_int_int(int i, int j);
extern void dummy_function_player_int(player *p, int j);
extern void dummy_function_player_int_ob(player *p, int c, object *ob);
extern void dummy_function(void);
extern void dummy_function_map(mapstruct *m);
extern void dummy_function_ob(object *ob);
extern void dummy_function_ob2(object *ob, object *ob2);
extern int dummy_function_ob2int(object *ob, object *ob2);
extern void dummy_function_ob_int(object *ob, int i);
extern void dummy_function_txtnr(char *txt, int nr);
extern void dummy_draw_info(int a, int b, object *ob, const char *txt);
extern void dummy_function_mapstr(int a, mapstruct *map, int x, int y, int dist, const char *str);
extern void dummy_function_int_ob_ob(int n, object *ob, object *ob2);
extern int dummy_container_unlink_func(player *ob, object *ob2);
extern void dummy_move_apply_func(object *ob, object *ob2, object *ob3, int);
extern void dummy_function_dragongain(object *ob, int a1, int a2);

/* holy.c */
extern void init_gods(void);
extern void add_god_to_list(archetype *god_arch);
extern int baptize_altar(object *op);
extern godlink *get_rand_god(void);
extern object *pntr_to_god_obj(godlink *godlnk);
extern void free_all_god(void);
extern void dump_gods(void);

/* info.c */
extern void dump_abilities(void);
extern void print_monsters(void);
extern void bitstostring(long bits, int num, char *str);

/* image.c */
extern int ReadBmapNames(void);
extern int FindFace(char *name, int error);
extern void free_all_images(void);

/* init.c */
extern void init_library(void);
extern void init_environ(void);
extern void init_globals(void);
extern void init_objects(void);
extern void init_defaults(void);
extern void init_dynamic(void);
extern void write_todclock(void);
extern void init_clocks(void);

/* item.c */
extern char *describe_resistance(object *op, int newline);
extern char *describe_attack(object *op, int newline);
extern char *describe_protections(object *op, int newline);
extern char *query_weight(object *op);
extern char *get_levelnumber(int i);
extern char *get_number(int i);
extern char *query_short_name(object *op, object *caller);
extern char *query_name(object *op, object *caller);
extern char *query_base_name(object *op, object *caller);
extern char *describe_item(object *op);
extern int need_identify(object *op);
extern void identify(object *op);
extern void set_traped_flag(object *op);

/* links.c */
extern objectlink *get_objectlink(void);
extern oblinkpt *get_objectlinkpt(void);
extern void free_objectlink(objectlink *ol);
extern void free_objectlinkpt(oblinkpt *obp);

/* living.c */
extern void set_attr_value(living *stats, int attr, signed char value);
extern void change_attr_value(living *stats, int attr, signed char value);
extern signed char get_attr_value(living *stats, int attr);
extern void check_stat_bounds(living *stats);
extern int change_abil(object *op, object *tmp);
extern void drain_stat(object *op);
extern void drain_specific_stat(object *op, int deplete_stats);
extern void change_luck(object *op, int value);
extern void fix_player(object *op);
extern void set_dragon_name(object *pl, object *abil, object *skin);
extern void dragon_level_gain(object *who);
extern void fix_monster(object *op);
extern object *insert_base_info_object(object *op);
extern object *find_base_info_object(object *op);
extern void set_mobile_speed(object *op, int index);

/* loader.c */
extern int lex_load(object *op, int map_flags);
extern void yyrestart(FILE *input_file);
extern void yy_load_buffer_state(void);
extern int yyerror(char *s);
extern void delete_loader_buffer(void *buffer);
extern void *create_loader_buffer(void *fp);
extern int load_object(void *fp, object *op, void *mybuffer, int bufstate, int map_flags);
extern int set_variable(object *op, char *buf);
extern void save_double(char *buf, char *name, double v);
extern void init_vars(void);
extern char *get_ob_diff(object *op, object *op2);
extern void save_map_object(FILE *fp, object *op, int flag);
extern int save_player_object(char *buf, object *op, int flag, size_t len);

/* logger.c */
extern void LOG(LogLevel logLevel, const char *format, ...);

/* los.c */
extern void init_block(void);
extern void set_block(int x, int y, int bx, int by);
extern void update_los(object *op);
extern void expand_sight(object *op);
extern int has_carried_lights(object *op);
extern void inline clear_los(object *op);
extern void print_los(object *op);
extern void make_sure_seen(object *op);
extern void make_sure_not_seen(object *op);
extern void adjust_light_source(mapstruct *map, int x, int y, int light);
extern void check_light_source_list(mapstruct *map);
extern void remove_light_source_list(mapstruct *map);
extern int obj_in_line_of_sight(object *op, object *obj, rv_vector *rv);

/* map.c */
extern mapstruct *has_been_loaded_sh(const char *name);
extern char *create_pathname(const char *name);
extern int check_path(const char *name, int prepend_dir);
extern char *normalize_path(const char *src, const char *dst, char *path);
extern void dump_map(mapstruct *m);
extern void dump_all_maps(void);
extern int wall(mapstruct *m, int x, int y);
extern int blocks_view(mapstruct *m, int x, int y);
extern int blocks_magic(mapstruct *m, int x, int y);
extern int blocks_cleric(mapstruct *m, int x, int y);
extern int blocked(object *op, mapstruct *m, int x, int y, int terrain);
extern int blocked_link(object *op, int xoff, int yoff);
extern int blocked_link_2(object *op, mapstruct *map, int x, int y);
extern int blocked_tile(object *op, mapstruct *m, int x, int y);
extern int arch_blocked(archetype *at, object *op, mapstruct *m, int x, int y);
extern int arch_out_of_map(archetype *at, mapstruct *m, int x, int y);
extern void load_objects(mapstruct *m, FILE *fp, int mapflags);
extern void save_objects(mapstruct *m, FILE *fp, FILE *fp2, int flag);
extern mapstruct *get_linked_map(void);
extern void allocate_map(mapstruct *m);
extern mapstruct *get_empty_map(int sizex, int sizey);
extern mapstruct *load_original_map(const char *filename, int flags);
extern int new_save_map(mapstruct *m, int flag);
extern void free_all_objects(mapstruct *m);
extern void free_map(mapstruct *m, int flag);
extern void delete_map(mapstruct *m);
extern int check_map_owner(mapstruct *map, object *op);
extern char *create_map_owner(mapstruct *map);
extern mapstruct *ready_map_name(const char *name, int flags);
extern void clean_tmp_map(mapstruct *m);
extern void free_all_maps(void);
extern void update_position(mapstruct *m, int x, int y);
extern void set_map_reset_time(mapstruct *map);
extern mapstruct *out_of_map(mapstruct *m, int *x, int *y);
extern mapstruct *out_of_map2(mapstruct *m, int *x, int *y);
extern int get_rangevector(object *op1, object *op2, rv_vector *retval, int flags);
extern int get_rangevector_from_mapcoords(mapstruct *map1, int x1, int y1, mapstruct *map2, int x2, int y2, rv_vector *retval, int flags);
extern int on_same_map(object *op1, object *op2);

/* object.c */
extern void init_mempools();
extern void setup_poolfunctions(mempool_id pool, chunk_constructor constructor, chunk_destructor destructor);
extern void *get_poolchunk(mempool_id pool);
extern void free_empty_puddles(mempool_id pool);
extern void mark_object_removed(object *ob);
extern int CAN_MERGE(object *ob1, object *ob2);
extern object *merge_ob(object *op, object *top);
extern signed long sum_weight(object *op);
extern object *is_player_inv(object *op);
extern void dump_object2(object *op);
extern void dump_object(object *op);
extern void dump_me(object *op, char *outstr);
extern void dump_all_objects(void);
extern object *find_object(int i);
extern object *find_object_name(char *str);
extern void free_all_object_data(void);
extern object *get_owner(object *op);
extern void clear_owner(object *op);
extern void set_owner(object *op, object *owner);
extern void copy_owner(object *op, object *clone);
extern void initialize_object(object *op);
extern void copy_object(object *op2, object *op);
extern void copy_object_data(object *op2, object *op);
extern object *get_object(void);
extern void update_turn_face(object *op);
extern void update_ob_speed(object *op);
extern void update_object(object *op, int action);
extern void destroy_object(object *ob);
extern int count_free(void);
extern int count_used(void);
extern void sub_weight(object *op, sint32 weight);
extern void remove_ob(object *op);
extern void destruct_ob(object *op);
extern void drop_ob_inventory(object *op);
extern void remove_ob_inv(object *op);
extern object *insert_ob_in_map(object *op, mapstruct *m, object *originator, int flag);
extern void replace_insert_ob_in_map(char *arch_string, object *op);
extern object *get_split_ob(object *orig_ob, int nr);
extern object *decrease_ob_nr(object *op, int i);
extern void add_weight(object *op, sint32 weight);
extern object *insert_ob_in_ob(object *op, object *where);
extern int check_walk_on(object *op, object *originator, int flags);
extern int check_walk_off (object *op, object *originator, int flags);
extern object *present_arch(archetype *at, mapstruct *m, int x, int y);
extern object *present(unsigned char type, mapstruct *m, int x, int y);
extern object *present_in_ob(unsigned char type, object *op);
extern object *present_arch_in_ob(archetype *at, object *op);
extern void set_cheat(object *op);
extern int find_free_spot(archetype *at, mapstruct *m, int x, int y, int start, int stop);
extern int find_first_free_spot(archetype *at, mapstruct *m, int x, int y);
extern int find_first_free_spot2(archetype *at, mapstruct *m,int x,int y, int start, int range);
extern int find_dir(mapstruct *m, int x, int y, object *exclude);
extern int find_dir_2(int x, int y);
extern int absdir(int d);
extern int dirdiff(int dir1, int dir2);
extern int can_pick(object *who, object *item);
extern object *ObjectCreateClone(object *asrc);
extern int was_destroyed(object *op, tag_t old_tag);
extern object *load_object_str(char *obstr);
extern void object_gc();
extern int auto_apply(object *op);

/* porting.c */
extern char *tempnam_local(char *dir, char *pfx);
extern void remove_directory(const char *path);
extern char *strdup_local(const char *str);
extern long strtol_local(register char *str, char **ptr, register int base);
extern char *strerror_local(int errnum);
extern int isqrt(int n);
extern char *ltostr10(signed long n);
extern void save_long(char *buf, char *name, long n);
extern FILE *open_and_uncompress(char *name, int flag, int *compressed);
extern void close_and_delete(FILE *fp, int compressed);
extern void make_path_to_file(char *filename);

/* player.c */
extern void free_player(player *pl);
extern object *find_skill(object *op, int skillnr);
extern int atnr_is_dragon_enabled(int attacknr);
extern int is_dragon_pl(object *op);

/* re-cmp.c */
extern char *re_cmp(char *str, char *regexp);

/* readable.c */
extern int nstrtok(const char *buf1, const char *buf2);
extern char *strtoktolin(const char *buf1, const char *buf2);
extern int book_overflow(const char *buf1, const char *buf2, int booksize);
extern void init_readable(void);
extern void change_book(object *book, int msgtype);
extern object *get_random_mon(int level);
extern char *mon_desc(object *mon);
extern object *get_next_mon(object *tmp);
extern char *mon_info_msg(int level, int booksize);
extern char *artifact_msg(int level, int booksize);
extern char *spellpath_msg(int level, int booksize);
extern void make_formula_book(object *book, int level);
extern char *msgfile_msg(int level, int booksize);
extern char *god_info_msg(int level, int booksize);
extern void tailor_readable_ob(object *book, int msg_type);
extern void free_all_readable(void);
extern void write_book_archive(void);

/* recipe.c */
extern recipelist *get_formulalist(int i);
extern void init_formulae(void);
extern void check_formulae(void);
extern void dump_alchemy(void);
extern archetype *find_treasure_by_name(treasure *t, char *name, int depth);
extern long find_ingred_cost(const char *name);
extern void dump_alchemy_costs(void);
extern const char *ingred_name(const char *name);
extern int strtoint(const char *buf);
extern artifact *locate_recipe_artifact(recipe *rp);
extern int numb_ingred(const char *buf);
extern recipelist *get_random_recipelist(void);
extern recipe *get_random_recipe(recipelist *rpl);
extern void free_all_recipes(void);

/* shstr.c */
extern void init_hash_table(void);
extern const char *add_string(const char *str);
extern int query_refcount(const char *str);
extern const char *find_string(const char *str);
extern const char *add_refcount(const char *str);
extern void free_string_shared(const char *str);
extern void ss_dump_statistics(void);
extern char *ss_dump_table(int what);
extern void ss_test_table(void);
extern int buf_overflow(const char *buf1, const char *buf2, int bufsize);

/* time.c */
extern void reset_sleep(void);
extern void log_time(long process_utime);
extern int enough_elapsed_time(void);
extern void sleep_delta(void);
extern void set_max_time(long t);
extern void get_tod(timeofday_t *tod);
extern void print_tod(object *op);
extern void time_info(object *op);
extern long seconds(void);

/* treasure.c */
extern void load_treasures(void);
extern void init_artifacts(void);
extern void init_archetype_pointers(void);
extern treasurelist *find_treasurelist(const char *name);
extern object *generate_treasure(treasurelist *t, int difficulty);
extern void create_treasure(treasurelist *t, object *op, int flag, int difficulty, int t_style, int a_chance, int tries, struct _change_arch *change_arch);
extern void create_all_treasures(treasure *t, object *op, int flag, int difficulty, int t_style, int a_chance, int tries, struct _change_arch *change_arch);
extern void create_one_treasure(treasurelist *tl, object *op, int flag, int difficulty, int t_style, int a_chance, int tries, struct _change_arch *change_arch);
extern void set_abs_magic(object *op, int magic);
extern int set_ring_bonus(object *op, int bonus, int level);
extern int get_magic(int diff);
extern int fix_generated_item(object **op, object *creator, int difficulty, int a_chance, int t_style, int max_magic, int fix_magic, int chance_magic, int flags);
extern artifactlist *find_artifactlist(int type);
extern artifact *find_artifact(const char *name);
extern archetype *find_artifact_archtype(const char *name);
extern void dump_artifacts(void);
extern void dump_monster_treasure_rec(const char *name, treasure *t, int depth);
extern void give_artifact_abilities(object *op, artifact *art);
extern int generate_artifact(object *op, int difficulty, int t_style, int a_chance);
extern void fix_flesh_item(object *item, object *donor);
extern void free_treasurestruct(treasure *t);
extern void free_charlinks(linked_char *lc);
extern void free_artifact(artifact *at);
extern void free_artifactlist(artifactlist *al);
extern void free_all_treasures(void);
extern void dump_monster_treasure(const char *name);
extern int get_enviroment_level(object *op);

/* utils.c */
extern int random_roll(int min, int max, object *op, int goodbad);
extern int die_roll(int num, int size, object *op, int goodbad);
extern int rndm(int min, int max);
extern int look_up_spell_name(const char *spname);
extern racelink *find_racelink(const char *name);
extern char *cleanup_string(char *ustring);
extern char *get_word_from_string(char *str, int *pos);
extern int replace(const char *src, const char *key, const char *replacement, char *result, size_t resultsize);
