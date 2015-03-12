/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
 *                                                                       *
 * Fork from Crossfire (Multiplayer game for X-windows).                 *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
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

/**
 * @file
 * Server main related functions. */

#include <global.h>
#include <check_proto.h>

/** Object used in process_events(). */
static object marker;

/**
 * @defgroup first_xxx Beginnings of linked lists.
 *@{*/
/** First player. */
player *first_player;
/** First map. */
mapstruct *first_map;
/** First treasure. */
treasurelist *first_treasurelist;
/** First artifact. */
artifactlist *first_artifactlist;
/*@}*/

/** Last player. */
player *last_player;

uint32 global_round_tag;
int process_delay;

static long shutdown_time;
static uint8 shutdown_active = 0;

static void process_players1(void);
static void process_players2(void);
static void dequeue_path_requests(void);
static void do_specials(void);

/**
 * Shows version information.
 * @param op If NULL the version is logged using LOG(), otherwise it is
 * shown to the player object using draw_info_format(). */
void version(object *op)
{
    if (op) {
        int revision;

        revision = bzr_get_revision();

        if (revision) {
            draw_info_format(COLOR_WHITE, op, "This is Atrinik v%s (r%d)", PACKAGE_VERSION, revision);
        } else {
            draw_info_format(COLOR_WHITE, op, "This is Atrinik v%s", PACKAGE_VERSION);
        }
    } else {
        logger_print(LOG(INFO), "This is Atrinik v%s.", PACKAGE_VERSION);
    }
}

/**
 * All this really is is a glorified object_removeject that also updates the
 * counts on the map if needed and sets map timeout if needed.
 * @param op The object leaving the map. */
void leave_map(object *op)
{
    object_remove(op, 0);

    if (!op->map->player_first) {
        set_map_timeout(op->map);
    }

    op->map = NULL;
    CONTR(op)->last_update = NULL;
}

/**
 * Sets map timeout value.
 * @param map The map to set the timeout for. */
void set_map_timeout(mapstruct *map)
{
#if MAP_DEFAULTTIMEOUT
    uint32 swap_time = MAP_SWAP_TIME(map);

    if (swap_time == 0) {
        swap_time = MAP_DEFAULTTIMEOUT;
    }

    if (swap_time >= MAP_MAXTIMEOUT) {
        swap_time = MAP_MAXTIMEOUT;
    }

    map->timeout = swap_time;
#else
    /* Save out the map. */
    swap_map(map, 0);
#endif
}

/**
 * Do all player-related stuff before objects have been updated.
 * @sa process_players2(). */
static void process_players1(void)
{
    player *pl, *plnext;
    int retval;

    for (pl = first_player; pl; pl = plnext) {
        plnext = pl->next;

        while ((retval = handle_newcs_player(pl)) == 1) {
        }

        if (retval == -1) {
            continue;
        }

        /* Update the next pointer, in case the above handle_newcs_player()
         * call(s) caused the previous next pointer to be freed (double
         * login kicked the playing character, for example). */
        plnext = pl->next;

        if (pl->followed_player[0]) {
            player *followed = find_player(pl->followed_player);

            if (followed && followed->ob && followed->ob->map) {
                rv_vector rv;

                if (!on_same_map(pl->ob, followed->ob) || (get_rangevector(pl->ob, followed->ob, &rv, 0) && (rv.distance > 4 || rv.distance_z != 0))) {
                    int space = find_free_spot(pl->ob->arch, pl->ob, followed->ob->map, followed->ob->x, followed->ob->y, 1, SIZEOFFREE2 + 1);

                    if (space != -1 && followed->ob->x + freearr_x[space] >= 0 && followed->ob->y + freearr_y[space] >= 0 && followed->ob->x + freearr_x[space] < MAP_WIDTH(followed->ob->map) && followed->ob->y + freearr_y[space] < MAP_HEIGHT(followed->ob->map)) {
                        object_remove(pl->ob, 0);
                        pl->ob->x = followed->ob->x + freearr_x[space];
                        pl->ob->y = followed->ob->y + freearr_y[space];
                        insert_ob_in_map(pl->ob, followed->ob->map, NULL, 0);
                    }
                }
            } else {
                draw_info_format(COLOR_RED, pl->ob, "Player %s left.", pl->followed_player);
                pl->followed_player[0] = '\0';
            }
        }

        /* Use the target system to hit our target - don't hit friendly
         * objects, ourselves or when we are not in combat mode. */
        if (pl->target_object && OBJECT_ACTIVE(pl->target_object) && pl->target_object_count != pl->ob->count && !is_friend_of(pl->ob, pl->target_object)) {
            if (global_round_tag >= pl->action_attack) {
                /* Now we force target as enemy */
                pl->ob->enemy = pl->target_object;
                pl->ob->enemy_count = pl->target_object_count;

                if (!OBJECT_VALID(pl->ob->enemy, pl->ob->enemy_count) || pl->ob->enemy->owner == pl->ob) {
                    pl->ob->enemy = NULL;
                } else if (is_melee_range(pl->ob, pl->ob->enemy)) {
                    if (!OBJECT_VALID(pl->ob->enemy->enemy, pl->ob->enemy->enemy_count)) {
                        set_npc_enemy(pl->ob->enemy, pl->ob, NULL);
                    } else {
                        /* Our target already has an enemy - then note we had
                         * attacked */
                        pl->ob->enemy->attacked_by = pl->ob;
                        pl->ob->enemy->attacked_by_distance = 1;
                    }

                    skill_attack(pl->ob->enemy, pl->ob, 0, NULL);

                    pl->action_attack = global_round_tag + pl->ob->weapon_speed;

                    pl->action_timer = (float) (pl->action_attack - global_round_tag) / MAX_TICKS;
                    pl->last_action_timer = 0;
                }
            }
        }

        if (pl->move_path) {
            player_path_handle(pl);
        }

        do_some_living(pl->ob);

#ifdef AUTOSAVE

        /* Check for ST_PLAYING state so that we don't try to save off when
         * the player is logging in. */
        if ((pl->last_save_tick + AUTOSAVE) < pticks && pl->socket.state == ST_PLAYING) {
            player_save(pl->ob);
            pl->last_save_tick = pticks;
            hiscore_check(pl->ob, 1);
        }
#endif

        /* Update total playing time. */
        if (pl->socket.state == ST_PLAYING && time(NULL) > pl->last_stat_time_played) {
            pl->last_stat_time_played = time(NULL);

            if (pl->afk) {
                pl->stat_time_afk++;
            } else {
                pl->stat_time_played++;
            }
        }
    }
}

/**
 * Do all player-related stuff after objects have been updated.
 * @sa process_players1(). */
static void process_players2(void)
{
    player *pl;

    for (pl = first_player; pl; pl = pl->next) {
        /* Check if our target is still valid - if not, update client. */
        if (pl->ob->map && (!pl->target_object || (pl->target_object != pl->ob && pl->target_object_count != pl->target_object->count) || QUERY_FLAG(pl->target_object, FLAG_SYS_OBJECT) || (QUERY_FLAG(pl->target_object, FLAG_IS_INVISIBLE) && !QUERY_FLAG(pl->ob, FLAG_SEE_INVISIBLE)))) {
            send_target_command(pl);
        }

        if (pl->ob->speed_left > pl->ob->speed) {
            pl->ob->speed_left = pl->ob->speed;
        }
    }
}

/**
 * Process objects with speed, like teleporters, players, etc.
 * @param map If not NULL, only process objects on that map. */
void process_events(mapstruct *map)
{
    object *op;
    tag_t tag;

    process_players1();

    /* Put marker object at beginning of active list */
    marker.active_next = active_objects;

    if (marker.active_next) {
        marker.active_next->active_prev = &marker;
    }

    marker.active_prev = NULL;
    active_objects = &marker;

    while (marker.active_next) {
        op = marker.active_next;
        tag = op->count;

        /* Move marker forward - swap op and marker */
        op->active_prev = marker.active_prev;

        if (op->active_prev) {
            op->active_prev->active_next = op;
        } else {
            active_objects = op;
        }

        marker.active_next = op->active_next;

        if (marker.active_next) {
            marker.active_next->active_prev = &marker;
        }

        marker.active_prev = op;
        op->active_next = &marker;

        /* Now process op */
        if (OBJECT_FREE(op)) {
            logger_print(LOG(BUG), "Free object on active list");
            op->speed = 0;
            update_ob_speed(op);
            continue;
        }

        if (QUERY_FLAG(op, FLAG_REMOVED)) {
            op->speed = 0;
            update_ob_speed(op);
            continue;
        }

        if (!op->speed) {
            logger_print(LOG(BUG), "Object %s (%s, type:%d count:%d) has no speed, but is on active list", op->arch->name, query_name(op, NULL), op->type, op->count);
            update_ob_speed(op);
            continue;
        }

        if (op->map == NULL && op->env == NULL && op->name && op->type != MAP && map == NULL) {
            logger_print(LOG(BUG), "Object without map or inventory is on active list: %s (%d)", query_name(op, NULL), op->count);
            op->speed = 0;
            update_ob_speed(op);
            continue;
        }

        if (map && op->map != map) {
            continue;
        }

        /* As long we are > 0, we are not ready to swing. */
        if (op->weapon_speed_left > 0) {
            op->weapon_speed_left -= op->weapon_speed;
        }

        if (op->speed_left > 0) {
            --op->speed_left;
            object_process(op);

            if (was_destroyed(op, tag)) {
                continue;
            }
        }

        if (op->anim_flags & ANIM_FLAG_STOP_MOVING) {
            op->anim_flags &= ~(ANIM_FLAG_MOVING | ANIM_FLAG_STOP_MOVING);
        }

        if (op->anim_flags & ANIM_FLAG_STOP_ATTACKING) {
            if (op->enemy == NULL || !is_melee_range(op, op->enemy)) {
                op->anim_flags &= ~ANIM_FLAG_ATTACKING;
            }

            op->anim_flags &= ~ANIM_FLAG_STOP_ATTACKING;
        }

        /* Handle archetype-field anim_speed differently when it comes to
         * the animation. If we have a value on this we don't animate it
         * at speed-events. */
        if (QUERY_FLAG(op, FLAG_ANIMATE)) {
            if (op->last_anim >= op->anim_speed) {
                animate_object(op);
                op->last_anim = 1;

                if (op->anim_flags & ANIM_FLAG_ATTACKING) {
                    op->anim_flags |= ANIM_FLAG_STOP_ATTACKING;
                }

                if (op->anim_flags & ANIM_FLAG_MOVING) {
                    if ((op->anim_flags & ANIM_FLAG_ATTACKING &&
                            !(op->anim_flags & ANIM_FLAG_STOP_ATTACKING)) ||
                            op->type == PLAYER ||
                            !OBJECT_VALID(op->enemy, op->enemy_count)) {
                        op->anim_flags |= ANIM_FLAG_STOP_MOVING;
                    }
                }
            } else {
                op->last_anim++;
            }
        }

        if (op->speed_left <= 0) {
            op->speed_left += FABS(op->speed);
        }
    }

    /* Remove marker object from active list */
    if (marker.active_prev) {
        marker.active_prev->active_next = NULL;
    } else {
        active_objects = NULL;
    }

    process_players2();
}

/**
 * Clean temporary map files. */
void clean_tmp_files(void)
{
    mapstruct *m, *tmp;

    /* We save the maps - it may not be intuitive why, but if there are
     * unique items, we need to save the map so they get saved off. */
    DL_FOREACH_SAFE(first_map, m, tmp)
    {
        if (m->in_memory == MAP_IN_MEMORY) {
            if (settings.recycle_tmp_maps) {
                swap_map(m, 0);
            } else {
                new_save_map(m, 0);
                clean_tmp_map(m);
            }
        }
    }

    /* Write the clock */
    write_todclock();

    if (settings.recycle_tmp_maps) {
        write_map_log();
    }
}

/**
 * Shut down the server, saving and freeing all data. */
void server_shutdown(void)
{
    player_disconnect_all();
    clean_tmp_files();
    exit(0);
}

/**
 * Dequeue path requests.
 * @todo Only compute time if there is something more in the queue,
 * something like if (path_request_queue_empty()) { break; } */
static void dequeue_path_requests(void)
{
#ifdef LEFTOVER_CPU_FOR_PATHFINDING
    static struct timeval new_time;
    long leftover_sec, leftover_usec;
    object *wp;

    while ((wp = path_get_next_request())) {
        waypoint_compute_path(wp);

        (void) GETTIMEOFDAY(&new_time);

        leftover_sec = last_time.tv_sec - new_time.tv_sec;
        leftover_usec = max_time - (new_time.tv_usec - last_time.tv_usec);

        /* This is very ugly, but probably the fastest for our use: */
        while (leftover_usec < 0) {
            leftover_usec += 1000000;
            leftover_sec -= 1;
        }
        while (leftover_usec > 1000000) {
            leftover_usec -= 1000000;
            leftover_sec += 1;
        }

        /* Try to save about 10 ms */
        if (leftover_sec < 1 && leftover_usec < 10000) {
            break;
        }
    }
#else
    object *wp = path_get_next_request();

    if (wp) {
        waypoint_compute_path(wp);
    }
#endif
}

/**
 * Swap one apartment (unique) map for another.
 * @param mapold Old map path.
 * @param mapnew Map to switch for.
 * @param x X position where player's items from old map will go to.
 * @param y Y position where player's items from old map will go to.
 * @param op Player we're doing the switching for.
 * @return 1 on success, 0 on failure. */
int swap_apartments(const char *mapold, const char *mapnew, int x, int y, object *op)
{
    char *cleanpath, *path;
    int i, j;
    object *ob, *tmp, *tmp2;
    mapstruct *oldmap, *newmap;

    /* So we can transfer our items from the old apartment. */
    cleanpath = estrdup(mapold);
    string_replace_char(cleanpath, "/", '$');
    path = player_make_path(op->name, cleanpath);
    oldmap = ready_map_name(path, MAP_PLAYER_UNIQUE);
    efree(path);
    efree(cleanpath);

    if (!oldmap) {
        logger_print(LOG(BUG), "Could not get oldmap using ready_map_name().");
        return 0;
    }

    /* Our new map. */
    cleanpath = estrdup(mapnew);
    string_replace_char(cleanpath, "/", '$');
    path = player_make_path(op->name, cleanpath);
    newmap = ready_map_name(path, MAP_PLAYER_UNIQUE);
    efree(path);
    efree(cleanpath);

    if (!newmap) {
        logger_print(LOG(BUG), "Could not get newmap using ready_map_name().");
        return 0;
    }

    /* Go through every square on old apartment map, looking for things
     * to transfer. */
    for (i = 0; i < MAP_WIDTH(oldmap); i++) {
        for (j = 0; j < MAP_HEIGHT(oldmap); j++) {
            for (ob = GET_MAP_OB(oldmap, i, j); ob; ob = tmp2) {
                tmp2 = ob->above;

                /* We teleport any possible players here to emergency map. */
                if (ob->type == PLAYER) {
                    object_enter_map(ob, NULL, NULL, 0, 0, 0);
                    continue;
                }

                /* If it's sys_object 1, there's no need to transfer it. */
                if (QUERY_FLAG(ob, FLAG_SYS_OBJECT)) {
                    continue;
                }

                /* A pickable item... Tranfer it */
                if (!QUERY_FLAG(ob, FLAG_NO_PICK)) {
                    object_remove(ob, 0);
                    ob->x = x;
                    ob->y = y;
                    insert_ob_in_map(ob, newmap, NULL, INS_NO_MERGE | INS_NO_WALK_ON);
                } else {
                    /* Fixed part of map */

                    /* Now we test for containers, because player
                     * can have items stored in it. So, go through
                     * the container and look for things to transfer. */
                    for (tmp = ob->inv; tmp; tmp = tmp2) {
                        tmp2 = tmp->below;

                        if (QUERY_FLAG(tmp, FLAG_SYS_OBJECT) || QUERY_FLAG(tmp, FLAG_NO_PICK)) {
                            continue;
                        }

                        object_remove(tmp, 0);
                        tmp->x = x;
                        tmp->y = y;
                        insert_ob_in_map(tmp, newmap, NULL, INS_NO_MERGE | INS_NO_WALK_ON);
                    }
                }
            }
        }
    }

    /* Save the map */
    new_save_map(newmap, 0);

    /* Check for old save bed */
    if (strcmp(oldmap->path, CONTR(op)->savebed_map) == 0) {
        strcpy(CONTR(op)->savebed_map, EMERGENCY_MAPPATH);
        CONTR(op)->bed_x = EMERGENCY_X;
        CONTR(op)->bed_y = EMERGENCY_Y;
    }

    unlink(oldmap->path);

    /* Free the maps */
    free_map(newmap, 1);
    free_map(oldmap, 1);

    return 1;
}

/**
 * Collection of functions to call from time to time. */
static void do_specials(void)
{
    if (!(pticks % 2)) {
        dequeue_path_requests();
    }

    if (!(pticks % PTICKS_PER_CLOCK)) {
        tick_the_clock();
    }

    /* Clears the tmp-files of maps which have reset */
    if (!(pticks % 509)) {
        flush_old_maps();
    }

    if (*settings.server_host != '\0' && !(pticks % 2521)) {
        metaserver_info_update();
    }
}

void shutdown_timer_start(long secs)
{
    shutdown_time = pticks + secs * MAX_TICKS;
    shutdown_active = 1;
}

void shutdown_timer_stop(void)
{
    shutdown_active = 0;
}

static int shutdown_timer_check(void)
{
    if (!shutdown_active) {
        return 0;
    }

    if (pticks >= shutdown_time) {
        return 1;
    }

    if (!((shutdown_time - pticks) % (60 * MAX_TICKS)) || pticks == shutdown_time - 5 * MAX_TICKS) {
        draw_info_type_format(CHAT_TYPE_CHAT, NULL, COLOR_GREEN, NULL, "[Server]: Server will shut down in %02"FMT64U ":%02"FMT64U " minutes.", (uint64) ((shutdown_time - pticks) / MAX_TICKS / 60), (uint64) ((shutdown_time - pticks) / MAX_TICKS % 60));
    }

    return 0;
}

/**
 * The main function.
 * @param argc Number of arguments.
 * @param argv Arguments.
 * @return 0. */
int main(int argc, char **argv)
{
#ifdef WIN32
    /* Open all files in binary mode by default. */
    _set_fmode(_O_BINARY);
#endif

    init(argc, argv);

    if (settings.unit_tests) {
#ifdef HAVE_CHECK
        logger_print(LOG(INFO), "Running unit tests...");
        cleanup();
        check_main();
        exit(0);
#else
        logger_print(LOG(ERROR), "Unit tests have not been compiled, aborting.");
        exit(1);
#endif
    }

    atexit(cleanup);

    if (settings.world_maker) {
#ifdef HAVE_WORLD_MAKER
        logger_print(LOG(INFO), "Running the world maker...");
        world_maker();
        exit(0);
#else
        logger_print(LOG(ERROR), "Cannot run world maker; server was not compiled with libgd, exiting...");
        exit(1);
#endif
    }

    console_start_thread();
    memset(&marker, 0, sizeof(struct obj));
    process_delay = 0;

    logger_print(LOG(INFO), "Server ready. Waiting for connections...");

    for (; ; ) {
        if (shutdown_timer_check()) {
            break;
        }

        console_command_handle();

        doeric_server();

        if (++process_delay >= max_time_multiplier) {
            process_delay = 0;

            /* Global round ticker. */
            global_round_tag++;
            pticks++;

            /* "do" something with objects with speed */
            process_events(NULL);

            /* Removes unused maps after a certain timeout */
            check_active_maps();

            /* Routines called from time to time. */
            do_specials();

            trigger_global_event(GEVENT_TICK, NULL, NULL);
        }

        doeric_server_write();

        /* Sleep proper amount of time before next tick */
        sleep_delta();
    }

    server_shutdown();

    return 0;
}
