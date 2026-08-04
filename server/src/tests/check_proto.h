#ifndef __CPROTO__
/* src/tests/check.c */
extern void check_setup(void);
extern void check_teardown(void);
extern void check_test_setup(void);
extern void check_test_teardown(void);
extern void check_setup_env_pl(mapstruct **map, object **pl);
extern void check_run_suite(Suite *suite, const char *file);
extern int check_main(int argc, char **argv);
#endif
