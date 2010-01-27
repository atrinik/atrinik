/* anim.c */
extern void free_all_anim();
extern void init_anim();
extern int find_animation(char *name);
extern void animate_object(object *op, int count);

/* arch.c */
extern object *get_archetype_by_object_name(const char *name);
extern archetype *get_skill_archetype(int skillnr);
extern int item_matched_string(object *pl, object *op, const char *name);
extern void init_archetypes();
extern void arch_info(object *op);
extern void dump_all_archetypes();
extern void free_all_archs();
extern object *arch_to_object(archetype *at);
extern object *get_archetype(const char *name);
extern archetype *find_archetype(const char *name);
extern object *clone_arch(int type);

/* button.c */
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

/* exp.c */
extern uint32 level_exp(int level, double expmul);
extern sint32 add_exp(object *op, int exp, int skill_nr);
extern void player_lvl_adj(object *who, object *op);
extern int adjust_exp(object *pl, object *op, int exp);
extern void apply_death_exp_penalty(object *op);
extern float calc_level_difference(int who_lvl, int op_lvl);

/* friend.c */
extern void add_friendly_object(object *op);
extern void remove_friendly_object(object *op);
extern void dump_friendly_objects();

/* holy.c */
extern void init_gods();
extern godlink *get_rand_god();
extern object *pntr_to_god_obj(godlink *godlnk);
extern void free_all_god();
extern void dump_gods();

/* info.c */
extern void dump_abilities();
extern void print_monsters();

/* image.c */
extern int read_bmap_names();
extern int find_face(char *name, int error);
extern void free_all_images();

/* item.c */
extern char *describe_resistance(object *op, int newline);
extern char *query_weight(object *op);
extern char *get_levelnumber(int i);
extern char *query_short_name(object *op, object *caller);
extern char *query_name(object *op, object *caller);
extern char *query_base_name(object *op, object *caller);
extern char *describe_item(object *op);
extern int need_identify(object *op);
extern void identify(object *op);
extern void set_trapped_flag(object *op);

/* links.c */
extern objectlink *get_objectlink();
extern void free_objectlink(objectlink *ol);
extern void free_objectlinkpt(objectlink *obp);
extern objectlink *objectlink_link(objectlink **startptr, objectlink **endptr, objectlink *afterptr, objectlink *beforeptr, objectlink *objptr);
extern objectlink *objectlink_unlink(objectlink **startptr, objectlink **endptr, objectlink *objptr);

/* living.c */
extern void set_attr_value(living *stats, int attr, sint8 value);
extern void change_attr_value(living *stats, int attr, sint8 value);
extern sint8 get_attr_value(living *stats, int attr);
extern void check_stat_bounds(living *stats);
extern int change_abil(object *op, object *tmp);
extern void drain_stat(object *op);
extern void drain_specific_stat(object *op, int deplete_stats);
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
extern void yypop_buffer_state();
extern int yyget_lineno();
extern FILE *yyget_in();
extern FILE *yyget_out();
extern int yyget_leng();
extern char *yyget_text();
extern void yyset_lineno(int line_number);
extern void yyset_in(FILE *in_str);
extern void yyset_out(FILE *out_str);
extern int yyget_debug();
extern void yyset_debug(int bdebug);
extern int yylex_destroy();
extern void yyfree(void *ptr);
extern int yyerror(char *s);
extern void delete_loader_buffer(void *buffer);
extern void *create_loader_buffer(void *fp);
extern int load_object(void *fp, object *op, void *mybuffer, int bufstate, int map_flags);
extern int set_variable(object *op, char *buf);
extern void get_ob_diff(StringBuffer *sb, object *op, object *op2);
extern void save_object(FILE *fp, object *op, int flag);

/* logger.c */
extern void LOG(LogLevel logLevel, const char *format, ...);

/* los.c */
extern void init_block();
extern void set_block(int x, int y, int bx, int by);
extern void update_los(object *op);
extern void clear_los(object *op);
extern void adjust_light_source(mapstruct *map, int x, int y, int light);
extern void check_light_source_list(mapstruct *map);
extern void remove_light_source_list(mapstruct *map);
extern int obj_in_line_of_sight(object *obj, rv_vector *rv);

/* map.c */
extern mapstruct *has_been_loaded_sh(const char *name);
extern char *create_pathname(const char *name);
extern char *normalize_path(const char *src, const char *dst, char *path);
extern void dump_map(mapstruct *m);
extern void dump_all_maps();
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
extern mapstruct *get_linked_map();
extern mapstruct *get_empty_map(int sizex, int sizey);
extern mapstruct *load_original_map(const char *filename, int flags);
extern int new_save_map(mapstruct *m, int flag);
extern void free_map(mapstruct *m, int flag);
extern void delete_map(mapstruct *m);
extern mapstruct *ready_map_name(const char *name, int flags);
extern void clean_tmp_map(mapstruct *m);
extern void free_all_maps();
extern void update_position(mapstruct *m, int x, int y);
extern void set_map_reset_time(mapstruct *map);
extern mapstruct *get_map_from_coord(mapstruct *m, int *x, int *y);
extern mapstruct *get_map_from_coord2(mapstruct *m, int *x, int *y);
extern int get_rangevector(object *op1, object *op2, rv_vector *retval, int flags);
extern int get_rangevector_from_mapcoords(mapstruct *map1, int x1, int y1, mapstruct *map2, int x2, int y2, rv_vector *retval, int flags);
extern int on_same_map(object *op1, object *op2);
extern int players_on_map(mapstruct *m);
extern int wall_blocked(mapstruct *m, int x, int y);

/* mempool.c */
extern uint32 nearest_pow_two_exp(uint32 n);
extern void setup_poolfunctions(struct mempool *pool, chunk_constructor constructor, chunk_destructor destructor);
extern struct mempool *create_mempool(const char *description, uint32 expand, uint32 size, uint32 flags, chunk_initialisator initialisator, chunk_deinitialisator deinitialisator, chunk_constructor constructor, chunk_destructor destructor);
extern void init_mempools();
extern void free_mempools();
extern void *get_poolchunk_array_real(struct mempool *pool, uint32 arraysize_exp);
extern void return_poolchunk_array_real(void *data, uint32 arraysize_exp, struct mempool *pool);
extern void dump_mempool_statistics(object *op, int *sum_used, int *sum_alloc);

/* object.c */
extern void init_materials();
extern void mark_object_removed(object *ob);
extern void object_gc();
extern int CAN_MERGE(object *ob1, object *ob2);
extern object *merge_ob(object *op, object *top);
extern signed long sum_weight(object *op);
extern object *is_player_inv(object *op);
extern void dump_object(object *op, StringBuffer *sb);
extern void free_all_object_data();
extern object *get_owner(object *op);
extern void clear_owner(object *op);
extern void set_owner(object *op, object *owner);
extern void copy_owner(object *op, object *clone);
extern void initialize_object(object *op);
extern void copy_object(object *op2, object *op);
extern void copy_object_data(object *op2, object *op);
extern object *get_object();
extern void update_turn_face(object *op);
extern void update_ob_speed(object *op);
extern void update_object(object *op, int action);
extern void drop_ob_inv(object *ob);
extern void destroy_object(object *ob);
extern void destruct_ob(object *op);
extern void remove_ob(object *op);
extern object *insert_ob_in_map(object *op, mapstruct *m, object *originator, int flag);
extern void replace_insert_ob_in_map(char *arch_string, object *op);
extern object *get_split_ob(object *orig_ob, int nr, char *err, size_t size);
extern object *decrease_ob_nr(object *op, uint32 i);
extern object *insert_ob_in_ob(object *op, object *where);
extern int check_walk_on(object *op, object *originator, int flags);
extern int check_walk_off(object *op, object *originator, int flags);
extern object *present_arch(archetype *at, mapstruct *m, int x, int y);
extern object *present(uint8 type, mapstruct *m, int x, int y);
extern object *present_in_ob(uint8 type, object *op);
extern object *present_arch_in_ob(archetype *at, object *op);
extern int find_free_spot(archetype *at, object *op, mapstruct *m, int x, int y, int start, int stop);
extern int find_first_free_spot(archetype *at, mapstruct *m, int x, int y);
extern int find_first_free_spot2(archetype *at, mapstruct *m, int x, int y, int start, int range);
extern void get_search_arr(int *search_arr);
extern int find_dir(mapstruct *m, int x, int y, object *exclude);
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
extern void init_object_initializers();

/* porting.c */
extern char *tempnam_local(const char *dir, const char *pfx);
extern char *strdup_local(const char *str);
extern char *strerror_local(int errnum);
extern int isqrt(int n);
extern FILE *open_and_uncompress(const char *name, int flag, int *compressed);
extern void close_and_delete(FILE *fp, int compressed);
extern void make_path_to_file(char *filename);
extern const char *strcasestr_local(const char *s, const char *find);

/* quest.c */
extern object *create_quest_container(object *op);
extern void check_quest(object *op, object *quest_container);

/* player.c */
extern object *find_skill(object *op, int skillnr);
extern int atnr_is_dragon_enabled(int attacknr);
extern int is_dragon_pl(object *op);

/* re-cmp.c */
extern const char *re_cmp(const char *str, const char *regexp);

/* region.c */
extern region *get_region_by_name(const char *region_name);
extern char *get_region_longname(const region *r);
extern char *get_region_msg(const region *r);
extern object *get_jail_exit(object *op);
extern void init_regions();
extern void free_regions();

/* readable.c */
extern int book_overflow(const char *buf1, const char *buf2, int booksize);
extern void free_mon_info();
extern void init_readable();
extern object *get_random_mon();
extern void tailor_readable_ob(object *book, int msg_type);
extern void free_all_readable();
extern void write_book_archive();

/* recipe.c */
extern recipelist *get_formulalist(int i);
extern void init_formulae();
extern void dump_alchemy();
extern void dump_alchemy_costs();
extern int strtoint(const char *buf);
extern artifact *locate_recipe_artifact(recipe *rp);
extern recipe *get_random_recipe(recipelist *rpl);
extern void free_all_recipes();

/* shstr.c */
extern void init_hash_table();
extern shstr *add_string(const char *str);
extern shstr *add_refcount(shstr *str);
extern int query_refcount(shstr *str);
extern shstr *find_string(const char *str);
extern void free_string_shared(shstr *str);
extern void ss_dump_statistics(char *buf, size_t size);
extern void ss_dump_table(int what, char *buf, size_t size);

/* stringbuffer.c */
extern StringBuffer *stringbuffer_new();
extern char *stringbuffer_finish(StringBuffer *sb);
extern const char *stringbuffer_finish_shared(StringBuffer *sb);
extern void stringbuffer_append_string(StringBuffer *sb, const char *str);
extern void stringbuffer_append_printf(StringBuffer *sb, const char *format, ...);
extern void stringbuffer_append_stringbuffer(StringBuffer *sb, const StringBuffer *sb2);

/* time.c */
extern void reset_sleep();
extern void sleep_delta();
extern void set_max_time(long t);
extern void get_tod(timeofday_t *tod);
extern void print_tod(object *op);
extern void time_info(object *op);
extern long seconds();

/* treasure.c */
extern void load_treasures();
extern void init_artifacts();
extern void init_archetype_pointers();
extern treasurelist *find_treasurelist(const char *name);
extern object *generate_treasure(treasurelist *t, int difficulty, int a_chance);
extern void create_treasure(treasurelist *t, object *op, int flag, int difficulty, int t_style, int a_chance, int tries, struct _change_arch *arch_change);
extern void set_abs_magic(object *op, int magic);
extern int fix_generated_item(object **op_ptr, object *creator, int difficulty, int a_chance, int t_style, int max_magic, int fix_magic, int chance_magic, int flags);
extern artifactlist *find_artifactlist(int type);
extern archetype *find_artifact_archtype(const char *name);
extern void dump_artifacts();
extern void give_artifact_abilities(object *op, artifact *art);
extern int generate_artifact(object *op, int difficulty, int t_style, int a_chance);
extern void free_all_treasures();
extern void dump_monster_treasure(const char *name);
extern int get_enviroment_level(object *op);
extern object *create_artifact(object *op, char *artifactname);

/* utils.c */
extern int random_roll(int min, int max, const object *op, int goodbad);
extern int die_roll(int num, int size, const object *op, int goodbad);
extern int rndm(int min, int max);
extern int look_up_spell_name(const char *spname);
extern void replace(const char *src, const char *key, const char *replacement, char *result, size_t resultsize);
extern racelink *find_racelink(const char *name);
extern char *cleanup_string(char *ustring);
extern char *get_word_from_string(char *str, int *pos);
extern void adjust_player_name(char *name);
extern void replace_unprintable_chars(char *buf);
extern size_t split_string(char *str, char *array[], size_t array_size, char sep);
extern int get_random_dir();
extern int get_randomized_dir(int dir);
extern int buf_overflow(const char *buf1, const char *buf2, size_t bufsize);
