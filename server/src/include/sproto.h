#ifndef __CPROTO__
/* alchemy.c */
int use_alchemy(object *op);

/* apply.c */
void move_apply(object *trap, object *victim, object *originator, int flags);
object *find_special_prayer_mark(object *op, int spell);
void do_learn_spell(object *op, int spell, int special_prayer);
void do_forget_spell(object *op, int spell);
int manual_apply(object *op, object *tmp, int aflag);
int player_apply(object *pl, object *op, int aflag, int quiet);
void player_apply_below(object *pl);
int apply_special(object *who, object *op, int aflags);
int monster_apply_special(object *who, object *op, int aflags);

/* attack.c */
int attack_ob(object *op, object *hitter);
int hit_player(object *op, int dam, object *hitter, int type);
int hit_map(object *op, int dir);
int kill_object(object *op, int dam, object *hitter, int type);
object *hit_with_arrow(object *op, object *victim);
void confuse_living(object *op);
void paralyze_living(object *op, int dam);
int is_melee_range(object *hitter, object *enemy);
void save_throw_object(object *op, object *originator);

/* ban.c */
void load_bans_file();
void save_bans_file();
int checkbanned(const char *name, char *ip);
int add_ban(char *input);
int remove_ban(char *input);
void list_bans(object *op);

/* c_chat.c */
char *cleanup_chat_string(char *ustring);
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

/* c_misc.c */
void map_info(object *op);
int command_motd(object *op, char *params);
int command_roll(object *op, char *params);
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

/* c_move.c */
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

/* c_new.c */
int command_run(object *op, char *params);
int command_run_stop(object *op, char *params);
void send_target_command(player *pl);
int command_combat(object *op, char *params);
int command_target(object *op, char *params);
void command_new_char(char *params, int len, player *pl);
void command_face_request(char *params, int len, player *pl);
void command_fire(char *params, int len, player *pl);
void send_mapstats_cmd(object *op, struct mapdef *map);
void send_spelllist_cmd(object *op, const char *spellname, int mode);
void send_skilllist_cmd(object *op, object *skillp, int mode);
void send_ready_skill(object *op, const char *skillname);
void send_golem_control(object *golem, int mode);
void generate_ext_title(player *pl);

/* c_object.c */
object *find_best_object_match(object *pl, char *params);
int command_uskill(object *pl, char *params);
int command_rskill(object *pl, char *params);
int command_apply(object *op, char *params);
int sack_can_hold(object *pl, object *sack, object *op, int nrof);
void pick_up(object *op, object *alt);
void put_object_in_sack(object *op, object *sack, object *tmp, long nrof);
void drop_object(object *op, object *tmp, long nrof);
void drop(object *op, object *tmp);
int command_dropall(object *op, char *params);
int command_drop(object *op, char *params);
object *find_marked_object(object *op);
void examine_living(object *op, object *tmp);
char *long_desc(object *tmp, object *caller);
void examine(object *op, object *tmp);
void inventory(object *op, object *inv);

/* c_range.c */
int command_cast_spell(object *op, char *params);
int fire_cast_spell(object *op, char *params);
int legal_range(object *op, int r);

/* c_wiz.c */
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
void remove_active_DM(object *op);
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

/* commands.c */
void init_commands();
CommArray_s *find_command_element(char *cmd, CommArray_s *commarray, int commsize);
int execute_newserver_command(object *pl, char *command);
emotes_array *find_emote(int emotion, emotes_array *emotes, int emotessize);

/* daemon.c */
void become_daemon(char *filename);

/* hiscore.c */
void hiscore_init();
void hiscore_check(object *op, int quiet);
void hiscore_display(object *op, int max, const char *match);

/* gods.c */
object *find_god(const char *name);
void pray_at_altar(object *pl, object *altar);
void become_follower(object *op, object *new_god);
const char *determine_god(object *op);
archetype *determine_holy_arch(object *god, const char *type);

/* init.c */
void free_strings();
void init_library();
void init_globals();
void write_todclock();
void init(int argc, char **argv);
void compile_info();
void free_racelist();

/* login.c */
void emergency_save(int flag);
int check_name(player *pl, char *name);
int save_player(object *op, int flag);
void check_login(object *op);

/* main.c */
void fatal(int err);
void version(object *op);
char *crypt_string(char *str, char *salt);
int check_password(char *typed, char *crypted);
void enter_player_savebed(object *op);
void leave_map(object *op);
void set_map_timeout(mapstruct *oldmap);
char *clean_path(const char *file);
void enter_exit(object *op, object *exit_ob);
void process_events(mapstruct *map);
void clean_tmp_files();
void cleanup();
int swap_apartments(char *mapold, char *mapnew, int x, int y, object *op);
int main(int argc, char **argv);

/* move.c */
int move_ob(object *op, int dir, object *originator);
int transfer_ob(object *op, int x, int y, int randomly, object *originator, object *trap);
int teleport(object *teleporter, uint8 tele_type, object *user);
void recursive_roll(object *op, int dir, object *pusher);
int try_fit(object *op, int x, int y);
int roll_ob(object *op, int dir, object *pusher);
int push_roll_object(object *op, int dir);
int missile_reflection_adjust(object *op, int flag);

/* party.c */
void add_party_member(partylist_struct *party, object *op);
void remove_party_member(partylist_struct *party, object *op);
partylist_struct *make_party(char *name);
void form_party(object *op, char *name);
partylist_struct *find_party(char *name);
void send_party_message(partylist_struct *party, char *msg, int flag, object *op);
void remove_party(partylist_struct *party);
void PartyCmd(char *buf, int len, player *pl);

/* player_shop.c */
void player_shop_open(char *data, player *pl);
void player_shop_close(player *pl);
void player_shop_load(char *data, player *pl);
void player_shop_examine(char *data, player *pl);
void player_shop_buy(char *data, player *pl);

/* plugins.c */
object *get_event_object(object *op, int event_nr);
CommArray_s *find_plugin_command(const char *cmd);
void display_plugins_list(object *op);
void init_plugins();
void init_plugin(const char *pluginfile);
void remove_plugin(const char *id);
void remove_plugins();
void trigger_global_event(int event_type, void *parm1, void *parm2);
int trigger_event(int event_type, object *const activator, object *const me, object *const other, const char *msg, int parm1, int parm2, int parm3, int flags);

/* rune.c */
void spring_trap(object *trap, object *victim);
int trap_see(object *op, object *trap, int level);
int trap_show(object *trap, object *where);
int trap_disarm(object *disarmer, object *trap, int risk);
void trap_adjust(object *trap, int difficulty);

/* shop.c */
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

/* skills.c */
int find_traps(object *pl, int level);
int remove_trap(object *op);
object *find_throw_tag(object *op, tag_t tag);
void do_throw(object *op, object *toss_item, int dir);

/* skill_util.c */
int find_skill_exp_level(object *pl, int item_skill);
char *find_skill_exp_skillname(int item_skill);
int do_skill(object *op, int dir);
int calc_skill_exp(object *who, object *op, int level);
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
int get_skill_stat1(object *op);
int get_skill_stat2(object *op);
int get_skill_stat3(object *op);

/* spell_effect.c */
void prayer_failure(object *op, int failure, int power);
void cast_magic_storm(object *op, object *tmp, int lvl);
int recharge(object *op);
int cast_create_food(object *op, object *caster, int dir, char *stringarg);
int probe(object *op);
int cast_wor(object *op, object *caster);
int cast_create_town_portal(object *op);
int cast_destruction(object *op, object *caster, int dam, int attacktype);
int cast_heal(object *op, int level, object *target, int spell_type);
int cast_change_attr(object *op, object *caster, object *target, int spell_type);
int create_bomb(object *op, object *caster, int dir, int spell_type);
void animate_bomb(object *op);
int remove_depletion(object *op, object *target);
int remove_curse(object *op, object *target, int type, SpellTypeFrom src);
int cast_identify(object *op, int level, object *single_ob, int mode);
int cast_detection(object *op, object *target, int type);
int cast_consecrate(object *op);
int finger_of_death(object *op, object *target);
int cast_cause_disease(object *op, object *caster, int dir, archetype *disease_arch, int type);
void move_aura(object *aura);

/* spell_util.c */
void init_spells();
void dump_spells();
int insert_spell_effect(char *archname, mapstruct *m, int x, int y);
spell *find_spell(int spelltype);
int check_spell_known(object *op, int spell_type);
int cast_spell(object *op, object *caster, int dir, int type, int ability, SpellTypeFrom item, char *stringarg);
int cast_create_obj(object *op, object *new_op, int dir);
int fire_bolt(object *op, object *caster, int dir, int type);
int fire_arch_from_position(object *op, object *caster, sint16 x, sint16 y, int dir, archetype *at, int type);
int cast_cone(object *op, object *caster, int dir, int strength, int spell_type, archetype *spell_arch);
void cone_drop(object *op);
void move_cone(object *op);
void fire_a_ball(object *op, int dir, int strength);
void explosion(object *op);
void forklightning(object *op, object *tmp);
int reflwall(mapstruct *m, int x, int y, object *sp_op);
void move_bolt(object *op);
void move_golem(object *op);
void control_golem(object *op, int dir);
void move_missile(object *op);
void explode_object(object *op);
void check_fired_arch(object *op);
void move_fired_arch(object *op);
void drain_rod_charge(object *rod);
void fix_rod_speed(object *rod);
int find_target_for_spell(object *op, object **target, uint32 flags);
void move_ball_lightning(object *op);
int spell_find_dir(mapstruct *m, int x, int y, object *exclude);
int SP_level_dam_adjust(object *caster, int spell_type, int base_dam);
int SP_level_strength_adjust(object *caster, int spell_type);
int SP_level_spellpoint_cost(object *caster, int spell_type, int caster_level);
void move_swarm_spell(object *op);
void fire_swarm(object *op, object *caster, int dir, archetype *swarm_type, int spell_type, int n, int magic);
int look_up_spell_by_name(object *op, const char *spname);
void put_a_monster(object *op, const char *monstername);
int summon_hostile_monsters(object *op, int n, const char *monstername);

/* swap.c */
void read_map_log();
void swap_map(mapstruct *map, int force_flag);
void check_active_maps();
void flush_old_maps();

/* time.c */
object *stop_item(object *op);
void fix_stopped_item(object *op, mapstruct *map, object *originator);
void move_firewall(object *op);
void process_object(object *op);

/* timers.c */
void cftimer_process_timers();
int cftimer_create(int id, long delay, object *ob, int mode);
int cftimer_destroy(int id);
int cftimer_find_free_id();
void cftimer_init();

/* pathfinder.c */
void request_new_path(object *waypoint);
object *get_next_requested_path();
shstr *encode_path(path_node *path);
int get_path_next(shstr *buf, sint16 *off, shstr **mappath, mapstruct **map, int *x, int *y);
path_node *compress_path(path_node *path);
path_node *find_path(object *op, mapstruct *map1, int x1, int y1, mapstruct *map2, int x2, int y2);

/* weather.c */
void init_world_darkness();
void tick_the_clock();
#endif
