#ifndef __CPROTO__
/* src/tests/check.c */
extern void check_setup(void);
extern void check_teardown(void);
extern void check_test_setup(void);
extern void check_test_teardown(void);
extern void check_setup_env_pl(mapstruct **map, object **pl);
extern void check_run_suite(Suite *suite, const char *file);
extern void check_main(int argc, char **argv);
/* src/tests/bugs/check_85.c */
extern void check_bug_85(void);
/* src/tests/unit/commands/check_object.c */
extern void check_commands_object(void);
/* src/tests/unit/server/check_arch.c */
extern void check_server_arch(void);
/* src/tests/unit/server/check_ban.c */
extern void check_server_ban(void);
/* src/tests/unit/server/check_cache.c */
extern void check_server_cache(void);
/* src/tests/unit/server/check_memory.c */
extern void check_server_memory(void);
/* src/tests/unit/server/check_object.c */
extern void check_server_object(void);
/* src/tests/unit/server/check_packet.c */
extern void check_server_packet(void);
/* src/tests/unit/server/check_pbkdf2.c */
extern void check_server_pbkdf2(void);
/* src/tests/unit/server/check_re_cmp.c */
extern void check_server_re_cmp(void);
/* src/tests/unit/server/check_shstr.c */
extern void check_server_shstr(void);
/* src/tests/unit/server/check_string.c */
extern void check_server_string(void);
/* src/tests/unit/server/check_stringbuffer.c */
extern void check_server_stringbuffer(void);
/* src/tests/unit/server/check_utils.c */
extern void check_server_utils(void);
/* src/tests/unit/types/check_light_apply.c */
extern void check_types_light_apply(void);
/* src/tests/unit/types/check_sound_ambient.c */
extern void check_types_sound_ambient(void);
#endif
