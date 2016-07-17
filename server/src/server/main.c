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
 * Server main related functions.
 */

#include <global.h>
#include <gitversion.h>
#include <toolkit_string.h>
#include <plugin.h>
#include <arch.h>
#include <player.h>
#include <object.h>
#include <player.h>
#include <object_methods.h>
#include <waypoint.h>
#include <server.h>
#include <process.h>

#ifdef HAVE_CHECK
#   include <check.h>
#   include <check_proto.h>
#endif

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
struct artifact_list *first_artifactlist;
/*@}*/

/** Last player. */
player *last_player;

uint32_t global_round_tag;
int process_delay;

static long shutdown_time;
static uint8_t shutdown_active = 0;

static void dequeue_path_requests(void);
static void do_specials(void);

/**
 * Shows version information.
 * @param op
 * If NULL the version is logged using LOG(), otherwise it is
 * shown to the player object using draw_info_format().
 */
void version(object *op)
{
    char buf[HUGE_BUF];

    snprintf(VS(buf), "This is Atrinik v%s", PACKAGE_VERSION);
#ifdef GITVERSION
    snprintfcat(VS(buf), "%s", " (" STRINGIFY(GITBRANCH) "/"
            STRINGIFY(GITVERSION) " by " STRINGIFY(GITAUTHOR) ")");
#endif

    if (op != NULL) {
        draw_info(COLOR_WHITE, op, buf);
    } else {
        LOG(INFO, "%s", buf);
    }
}

/**
 * All this really is is a glorified object_removeject that also updates the
 * counts on the map if needed and sets map timeout if needed.
 * @param op
 * The object leaving the map.
 */
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
 * @param map
 * The map to set the timeout for.
 */
void set_map_timeout(mapstruct *map)
{
#if MAP_DEFAULTTIMEOUT
    uint32_t swap_time = MAP_SWAP_TIME(map);

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
 * Process objects with speed, like teleporters, players, etc.
 */
void
process_events (void)
{
    object *op;
    tag_t tag;

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
        if (unlikely(OBJECT_FREE(op))) {
            LOG(ERROR, "Free object on active list");
            op->speed = 0;
            object_update_speed(op);
            continue;
        }

        if (unlikely(QUERY_FLAG(op, FLAG_REMOVED))) {
            /*
             * This is not actually an error; object_remove() doesn't remove
             * active objects from the active list, since the two most common
             * next steps are to either: re-insert the object elsewhere (for
             * which we would have to re-add it to the active list), or destroy
             * the object altogether (which does remove it from the active
             * list).
             *
             * For now, just drop a DEVEL message about this case, so we can
             * get a better idea of the objects that rely on this behavior.
             */
            LOG(DEVEL, "Removed object on active list: %s", object_get_str(op));
            op->speed = 0;
            object_update_speed(op);
            continue;
        }

        if (unlikely(DBL_EQUAL(op->speed, 0.0))) {
            LOG(ERROR, "Object has no speed, but is on active list: %s",
                object_get_str(op));
            object_update_speed(op);
            continue;
        }

        if (unlikely(op->map == NULL && op->env == NULL)) {
            LOG(ERROR, "Object without map or inventory is on active list: %s",
                object_get_str(op));
            op->speed = 0;
            object_update_speed(op);
            continue;
        }

        /* As long we are > 0, we are not ready to swing. */
        if (op->weapon_speed_left > 0) {
            op->weapon_speed_left -= op->weapon_speed;
        }

        if (op->speed_left <= 0) {
            op->speed_left += FABS(op->speed);
        }

        if (op->type == PLAYER && op->speed_left > op->speed) {
            op->speed_left = op->speed;
        }

        if (op->speed_left >= 0 || op->type == PLAYER) {
            if (op->type != PLAYER) {
                --op->speed_left;
            }

            object_process(op);

            if (OBJECT_DESTROYED(op, tag)) {
                continue;
            }
        }

        if (op->anim_flags & ANIM_FLAG_STOP_MOVING) {
            op->anim_flags &= ~(ANIM_FLAG_MOVING | ANIM_FLAG_STOP_MOVING);
        }

        if (op->anim_flags & ANIM_FLAG_STOP_ATTACKING) {
            if (op->enemy == NULL || !attack_is_melee_range(op, op->enemy)) {
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
    }

    /* Remove marker object from active list */
    if (marker.active_prev) {
        marker.active_prev->active_next = NULL;
    } else {
        active_objects = NULL;
    }
}

/**
 * Clean temporary map files.
 */
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
 * Shut down the server, saving and freeing all data.
 */
void server_shutdown(void)
{
    player_disconnect_all();
    clean_tmp_files();
    exit(0);
}

/**
 * Dequeue path requests.
 * @todo Only compute time if there is something more in the queue,
 * something like if (path_request_queue_empty()) { break; }
 */
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
 * @param mapold
 * Old map path.
 * @param mapnew
 * Map to switch for.
 * @param x
 * X position where player's items from old map will go to.
 * @param y
 * Y position where player's items from old map will go to.
 * @param op
 * Player we're doing the switching for.
 * @return
 * 1 on success, 0 on failure.
 */
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
    oldmap = ready_map_name(path, NULL, MAP_PLAYER_UNIQUE);
    efree(path);
    efree(cleanpath);

    if (!oldmap) {
        LOG(BUG, "Could not get oldmap using ready_map_name().");
        return 0;
    }

    /* Our new map. */
    cleanpath = estrdup(mapnew);
    string_replace_char(cleanpath, "/", '$');
    path = player_make_path(op->name, cleanpath);
    newmap = ready_map_name(path, NULL, MAP_PLAYER_UNIQUE);
    efree(path);
    efree(cleanpath);

    if (!newmap) {
        LOG(BUG, "Could not get newmap using ready_map_name().");
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
                    object_enter_map(ob, NULL, NULL, 0, 0, false);
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
                    object_insert_map(ob, newmap, NULL, INS_NO_MERGE | INS_NO_WALK_ON);
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
                        object_insert_map(tmp, newmap, NULL, INS_NO_MERGE | INS_NO_WALK_ON);
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
 * Collection of functions to call from time to time.
 */
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

    if (!(pticks % 40)) {
        memory_check_all();
    }

    if (!(pticks % 80)) {
        process_check_all();
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

    if (((shutdown_time - pticks) % (long) (60 * MAX_TICKS)) == 0 ||
            pticks == shutdown_time - (long) (5 * MAX_TICKS)) {
        draw_info_type_format(CHAT_TYPE_CHAT, NULL, COLOR_GREEN, NULL,
                "[Server]: Server will shut down in %02"PRIu64
                ":%02"PRIu64 " minutes.", (uint64_t) ((shutdown_time - pticks) /
                MAX_TICKS / 60), (uint64_t) ((shutdown_time - pticks) / (long)
                MAX_TICKS % 60));
    }

    return 0;
}

/**
 * Main processing function, called from main().
 */
void main_process(void)
{
    /* Global round ticker. */
    global_round_tag++;
    pticks++;

    /* "do" something with objects with speed */
    process_events();

    /* Removes unused maps after a certain timeout */
    check_active_maps();

    /* Routines called from time to time. */
    do_specials();

    trigger_global_event(GEVENT_TICK, NULL, NULL);
}

/**
 * The main function.
 * @param argc
 * Number of arguments.
 * @param argv
 * Arguments.
 * @return
 * 0.
 */
int main(int argc, char **argv)
{
#ifdef WIN32
    /* Open all files in binary mode by default. */
    _set_fmode(_O_BINARY);
#endif

    init(argc, argv);
    memset(&marker, 0, sizeof(struct obj));

    if (settings.plugin_unit_tests) {
        LOG(INFO, "Running plugin unit tests...");
        object *activator = player_get_dummy(PLAYER_TESTING_NAME1, NULL);
        object *me = player_get_dummy(PLAYER_TESTING_NAME2, NULL);
        trigger_unit_event(activator, me);

        if (!settings.unit_tests) {
            cleanup();
            exit(0);
        }
    }

    if (settings.unit_tests) {
#ifdef HAVE_CHECK
        LOG(INFO, "Running unit tests...");
        cleanup();
        check_main(argc, argv);
        exit(0);
#else
        LOG(ERROR, "Unit tests have not been compiled, aborting.");
        exit(1);
#endif
    }

    atexit(cleanup);

    if (settings.world_maker) {
#ifdef HAVE_WORLD_MAKER
        LOG(INFO, "Running the world maker...");
        world_maker();
        exit(0);
#else
        LOG(ERROR, "Cannot run world maker; server was not compiled with libgd, exiting...");
        exit(1);
#endif
    }

    if (!settings.no_console) {
        console_start_thread();
    }

    process_delay = 0;

    LOG(INFO, "Server ready. Waiting for connections...");

    for (; ; ) {
        if (unlikely(shutdown_timer_check())) {
            break;
        }

        console_command_handle();
        socket_server_process();

        if (++process_delay >= max_time_multiplier) {
            process_delay = 0;
            main_process();
        }

        socket_server_post_process();

        /* Sleep proper amount of time before next tick */
        sleep_delta();
    }

    server_shutdown();

    return 0;
}
