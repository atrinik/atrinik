#ifndef __CPROTO__
/* src/tests/check.c */
extern void check_setup(void);
extern void check_teardown(void);
extern void check_test_setup(void);
extern void check_test_teardown(void);
extern void check_setup_env_pl(mapstruct **map, object **pl);
extern void check_run_suite(Suite *suite, const char *file);
extern void check_main(int argc, char **argv);
/* src/tests/bugs/cursed_treasures.c */
extern void check_bug_cursed_treasures(void);
/* src/tests/unit/commands/object.c */
extern void check_commands_object(void);
/* src/tests/unit/server/arch.c */
extern void check_server_arch(void);
/* src/tests/unit/server/attack.c */
extern void check_server_attack(void);
/* src/tests/unit/server/ban.c */
extern void check_server_ban(void);
/* src/tests/unit/server/bank.c */
extern void check_server_bank(void);
/* src/tests/unit/server/cache.c */
extern void check_server_cache(void);
/* src/tests/unit/server/memory.c */
extern void check_server_memory(void);
/* src/tests/unit/server/object.c */
extern void check_server_object(void);
/* src/tests/unit/server/packet.c */
extern void check_server_packet(void);
/* src/tests/unit/server/pbkdf2.c */
extern void check_server_pbkdf2(void);
/* src/tests/unit/server/re_cmp.c */
extern void check_server_re_cmp(void);
/* src/tests/unit/server/shop.c */
extern void check_server_shop(void);
/* src/tests/unit/server/shstr.c */
extern void check_server_shstr(void);
/* src/tests/unit/server/string.c */
extern void check_server_string(void);
/* src/tests/unit/server/stringbuffer.c */
extern void check_server_stringbuffer(void);
/* src/tests/unit/types/light_apply.c */
extern void check_types_light_apply(void);
/* src/tests/unit/types/sound_ambient.c */
extern void check_types_sound_ambient(void);
#endif
