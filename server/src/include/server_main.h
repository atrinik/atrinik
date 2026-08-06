/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 ************************************************************************/

#ifndef SERVER_MAIN_H
#define SERVER_MAIN_H

#include <stdint.h>

#include <decls.h>

int server_run(int argc, char **argv);

/** Public API implemented in src/server/main.c. */

extern player *first_player;

extern mapstruct *first_map;

extern treasure_list_t *first_treasurelist;

extern struct artifact_list *first_artifactlist;

extern player *last_player;

extern uint32_t global_round_tag;

extern int process_delay;

extern void version(object *op);

extern void leave_map(object *op);

extern void set_map_timeout(mapstruct *map);

extern void process_events(void);

extern void clean_tmp_files(void);

extern void server_shutdown(void);

extern int swap_apartments(const char *mapold, const char *mapnew, int x, int y, object *op);

extern void shutdown_timer_start(long secs);

extern void shutdown_timer_stop(void);

extern void main_process(void);

#endif
