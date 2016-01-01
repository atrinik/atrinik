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
 * Object methods system.
 */

#include <global.h>
#include <plugin.h>
#include <arch.h>
#include <player.h>
#include <object.h>
#include <object_methods.h>

/**
 * Registered method handlers.
 */
static object_methods_t object_type_methods[OBJECT_TYPE_MAX];

/**
 * The base object type methods; all object types inherit from this.
 */
static object_methods_t object_methods_base;

/**
 * Recursion counter for object_move_on().
 */
static int object_move_on_recursion_depth = 0;

#define OBJECT_TYPE_DECLARE
#include <object_type_list.h>
#undef OBJECT_TYPE_DECLARE

/**
 * Initialize one object methods structure.
 *
 * @param methods
 * What to initialize.
 */
static void
object_methods_init_one (object_methods_t *methods)
{
    memset(methods, 0, sizeof(*methods));
}

/**
 * Initializes the object methods system.
 */
void
object_methods_init (void)
{
    object_methods_init_one(&object_methods_base);
    object_methods_base.apply_func =
        common_object_apply;
    object_methods_base.describe_func =
        common_object_describe;
    object_methods_base.process_func =
        common_object_process;
    object_methods_base.move_on_func =
        common_object_move_on;
    object_methods_base.projectile_fire_func =
        common_object_projectile_fire_missile;
    object_methods_base.projectile_move_func =
        common_object_projectile_move;
    object_methods_base.projectile_hit_func =
        common_object_projectile_hit;
    object_methods_base.projectile_stop_func =
        common_object_projectile_stop_missile;

    for (size_t i = 0; i < arraysize(object_type_methods); i++) {
        object_methods_init_one(&object_type_methods[i]);
        object_type_methods[i].fallback = &object_methods_base;
    }

#include <object_type_list.h>
}

/**
 * Acquire object methods for the specified object type.
 *
 * @param type
 * The object type.
 * @return
 * The object methods for the specified type, never NULL.
 */
object_methods_t *
object_methods_get (int type)
{
    HARD_ASSERT(type >= 0 && type < OBJECT_TYPE_MAX);
    return &object_type_methods[type];
}

/** @copydoc object_methods_t::init_func */
void
object_cb_init (object *op)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(op->head == NULL);

    for (object_methods_t *methods = &object_type_methods[op->type];
         methods != NULL;
         methods = methods->fallback) {
        if (methods->init_func != NULL) {
            methods->init_func(op);
            break;
        }
    }
}

/** @copydoc object_methods_t::deinit_func */
void
object_cb_deinit (object *op)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(op->head == NULL);

    for (object_methods_t *methods = &object_type_methods[op->type];
         methods != NULL;
         methods = methods->fallback) {
        if (methods->deinit_func != NULL) {
            methods->deinit_func(op);
            break;
        }
    }
}

/** @copydoc object_methods_t::apply_func */
int
object_apply (object *op, object *applier, int aflags)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(applier != NULL);

    applier = HEAD(applier);

    for (object_methods_t *methods = &object_type_methods[op->type];
         methods != NULL;
         methods = methods->fallback) {
        if (methods->apply_func != NULL) {
            return methods->apply_func(op, applier, aflags);
        }
    }

    return OBJECT_METHOD_UNHANDLED;
}

/** @copydoc object_methods_t::process_func */
void
object_process (object *op)
{
    HARD_ASSERT(op != NULL);

    /* No need to process objects inside creators. */
    if (op->env != NULL && op->env->type == CREATOR) {
        return;
    }

    if (common_object_process_pre(op)) {
        return;
    }

    if (HAS_EVENT(op, EVENT_TIME) &&
        trigger_event(EVENT_TIME, NULL, op, NULL, NULL, 0, 0, 0, 0) != 0) {
        return;
    }

    for (object_methods_t *methods = &object_type_methods[op->type];
         methods != NULL;
         methods = methods->fallback) {
        if (methods->process_func != NULL) {
            methods->process_func(op);
            return;
        }
    }
}

/** @copydoc object_methods_t::describe_func */
char *
object_describe (object *op, object *observer, char *buf, size_t size)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(observer != NULL);
    HARD_ASSERT(buf != NULL);

    for (object_methods_t *methods = &object_type_methods[op->type];
         methods != NULL;
         methods = methods->fallback) {
        if (methods->describe_func != NULL) {
            methods->describe_func(op, observer, buf, size);
            return buf;
        }
    }

    buf[0] = '\0';
    return buf;
}

/** @copydoc object_methods_t::move_on_func */
int
object_move_on (object *op, object *victim, object *originator, int state)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(victim != NULL);

    op = HEAD(op);
    victim = HEAD(victim);

    if (trigger_event(EVENT_TRIGGER,
                      victim,
                      op,
                      originator,
                      NULL,
                      0,
                      0,
                      0,
                      0) != 0) {
        return OBJECT_METHOD_OK;
    }

    for (object_methods_t *methods = &object_type_methods[op->type];
         methods != NULL;
         methods = methods->fallback) {
        if (methods->move_on_func != NULL) {
            if (object_move_on_recursion_depth >= 500) {
                LOG(INFO,
                    "Aborting recursion, op: %s, victim: %s",
                    object_get_str(op),
                    object_get_str(victim));
                return OBJECT_METHOD_OK;
            }

            object_move_on_recursion_depth++;
            int ret = methods->move_on_func(op, victim, originator, state);
            object_move_on_recursion_depth--;

            return ret;
        }
    }

    return OBJECT_METHOD_UNHANDLED;
}

/** @copydoc object_methods_t::trigger_func */
int
object_trigger (object *op, object *cause, int state)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(cause != NULL);

    for (object_methods_t *methods = &object_type_methods[op->type];
         methods != NULL;
         methods = methods->fallback) {
        if (methods->trigger_func != NULL) {
            return methods->trigger_func(op, cause, state);
        }
    }

    return OBJECT_METHOD_UNHANDLED;
}

/** @copydoc object_methods_t::trigger_button_func */
int
object_trigger_button (object *op, object *cause, int state)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(cause != NULL);

    for (object_methods_t *methods = &object_type_methods[op->type];
         methods != NULL;
         methods = methods->fallback) {
        if (methods->trigger_button_func != NULL) {
            return methods->trigger_button_func(op, cause, state);
        }
    }

    return OBJECT_METHOD_UNHANDLED;
}

/** @copydoc object_methods_t::insert_map_func */
void
object_cb_insert_map (object *op)
{
    HARD_ASSERT(op != NULL);

    for (object_methods_t *methods = &object_type_methods[op->type];
         methods != NULL;
         methods = methods->fallback) {
        if (methods->insert_map_func != NULL) {
            methods->insert_map_func(op);
            return;
        }
    }
}

/** @copydoc object_methods_t::remove_map_func */
void
object_cb_remove_map (object *op)
{
    HARD_ASSERT(op != NULL);

    for (object_methods_t *methods = &object_type_methods[op->type];
         methods != NULL;
         methods = methods->fallback) {
        if (methods->remove_map_func != NULL) {
            methods->remove_map_func(op);
            return;
        }
    }
}

/** @copydoc object_methods_t::remove_inv_func */
void
object_cb_remove_inv (object *op)
{
    HARD_ASSERT(op != NULL);

    for (object_methods_t *methods = &object_type_methods[op->type];
         methods != NULL;
         methods = methods->fallback) {
        if (methods->remove_inv_func != NULL) {
            methods->remove_inv_func(op);
            return;
        }
    }
}

/** @copydoc object_methods_t::projectile_fire_func */
object *
object_projectile_fire (object *op, object *shooter, int dir)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(shooter != NULL);

    for (object_methods_t *methods = &object_type_methods[op->type];
         methods != NULL;
         methods = methods->fallback) {
        if (methods->projectile_fire_func != NULL) {
            return methods->projectile_fire_func(op, shooter, dir);
        }
    }

    return NULL;
}

/** @copydoc object_methods_t::projectile_move_func */
object *
object_projectile_move (object *op)
{
    HARD_ASSERT(op != NULL);

    for (object_methods_t *methods = &object_type_methods[op->type];
         methods != NULL;
         methods = methods->fallback) {
        if (methods->projectile_move_func != NULL) {
            return methods->projectile_move_func(op);
        }
    }

    return NULL;
}

/** @copydoc object_methods_t::projectile_hit_func */
int
object_projectile_hit (object *op, object *victim)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(victim != NULL);

    for (object_methods_t *methods = &object_type_methods[op->type];
         methods != NULL;
         methods = methods->fallback) {
        if (methods->projectile_hit_func != NULL) {
            return methods->projectile_hit_func(op, victim);
        }
    }

    return OBJECT_METHOD_UNHANDLED;
}

/** @copydoc object_methods_t::projectile_stop_func */
object *
object_projectile_stop (object *op, int reason)
{
    HARD_ASSERT(op != NULL);

    if (trigger_event(EVENT_STOP, NULL, op, NULL, NULL, 0, 0, 0, 0) != 0) {
        return op;
    }

    for (object_methods_t *methods = &object_type_methods[op->type];
         methods != NULL;
         methods = methods->fallback) {
        if (methods->projectile_stop_func != NULL) {
            return methods->projectile_stop_func(op, reason);
        }
    }

    return NULL;
}

/** @copydoc object_methods_t::ranged_fire_func */
int
object_ranged_fire (object *op, object *shooter, int dir, double *delay)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(shooter != NULL);

    /* No direction, try to get a direction to the player's target, if any. */
    if (dir == 0 &&
        op->type == BOW &&
        shooter->type == PLAYER &&
        OBJECT_VALID(CONTR(shooter)->target_object,
                     CONTR(shooter)->target_object_count)) {
        rv_vector rv;
        dir = get_dir_to_target(shooter, CONTR(shooter)->target_object, &rv);
    }

    if (dir == 0) {
        dir = shooter->direction;
        SOFT_ASSERT_RC(dir != 0,
                       OBJECT_METHOD_UNHANDLED,
                       "Direction is zero, op: %s, shooter: %s",
                       object_get_str(op),
                       object_get_str(shooter));
    }

    if (QUERY_FLAG(shooter, FLAG_CONFUSED)) {
        dir = get_randomized_dir(dir);
    }

    shooter->direction = dir;

    for (object_methods_t *methods = &object_type_methods[op->type];
         methods != NULL;
         methods = methods->fallback) {
        if (methods->ranged_fire_func != NULL) {
            return methods->ranged_fire_func(op, shooter, dir, delay);
        }
    }

    return OBJECT_METHOD_UNHANDLED;
}
