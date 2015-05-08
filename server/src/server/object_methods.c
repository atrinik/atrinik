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
 * Object methods system. */

#include <global.h>
#include <plugin.h>

/**
 * Registered method handlers. */
object_methods object_type_methods[OBJECT_TYPE_MAX];

/**
 * The base object type methods; all object types inherit from this. */
object_methods object_methods_base;

/**
 * Recursion counter for object_move_on(). */
static int object_move_on_recursion_depth = 0;

/**
 * Initialize one object methods structure.
 * @param methods What to initialize. */
static void object_methods_init_one(object_methods *methods)
{
    memset(methods, 0, sizeof(*methods));
}

/**
 * Initializes the object methods system. */
void object_methods_init(void)
{
    size_t i;

    object_methods_init_one(&object_methods_base);
    object_methods_base.apply_func = common_object_apply;
    object_methods_base.describe_func = common_object_describe;
    object_methods_base.process_func = common_object_process;
    object_methods_base.move_on_func = common_object_move_on;
    object_methods_base.projectile_fire_func = common_object_projectile_fire_missile;
    object_methods_base.projectile_move_func = common_object_projectile_move;
    object_methods_base.projectile_hit_func = common_object_projectile_hit;
    object_methods_base.projectile_stop_func = common_object_projectile_stop_missile;

    for (i = 0; i < arraysize(object_type_methods); i++) {
        object_methods_init_one(&object_type_methods[i]);
        object_type_methods[i].fallback = &object_methods_base;
    }

    object_type_init_ability();
    object_type_init_amulet();
    object_type_init_armour();
    object_type_init_arrow();
    object_type_init_base_info();
    object_type_init_beacon();
    object_type_init_blindness();
    object_type_init_book();
    object_type_init_book_spell();
    object_type_init_boots();
    object_type_init_bow();
    object_type_init_bracers();
    object_type_init_bullet();
    object_type_init_button();
    object_type_init_check_inv();
    object_type_init_class();
    object_type_init_client_map_info();
    object_type_init_cloak();
    object_type_init_clock();
    object_type_init_compass();
    object_type_init_cone();
    object_type_init_confusion();
    object_type_init_container();
    object_type_init_corpse();
    object_type_init_creator();
    object_type_init_dead_object();
    object_type_init_detector();
    object_type_init_director();
    object_type_init_disease();
    object_type_init_door();
    object_type_init_drink();
    object_type_init_duplicator();
    object_type_init_event_obj();
    object_type_init_exit();
    object_type_init_experience();
    object_type_init_firewall();
    object_type_init_flesh();
    object_type_init_floor();
    object_type_init_food();
    object_type_init_force();
    object_type_init_gate();
    object_type_init_gem();
    object_type_init_girdle();
    object_type_init_gloves();
    object_type_init_god();
    object_type_init_gravestone();
    object_type_init_greaves();
    object_type_init_handle();
    object_type_init_helmet();
    object_type_init_holy_altar();
    object_type_init_inorganic();
    object_type_init_jewel();
    object_type_init_key();
    object_type_init_light_apply();
    object_type_init_lightning();
    object_type_init_light_refill();
    object_type_init_light_source();
    object_type_init_magic_mirror();
    object_type_init_map();
    object_type_init_map_event_obj();
    object_type_init_map_info();
    object_type_init_marker();
    object_type_init_material();
    object_type_init_misc_object();
    object_type_init_money();
    object_type_init_monster();
    object_type_init_nugget();
    object_type_init_organic();
    object_type_init_pearl();
    object_type_init_pedestal();
    object_type_init_player();
    object_type_init_player_mover();
    object_type_init_poisoning();
    object_type_init_potion();
    object_type_init_potion_effect();
    object_type_init_power_crystal();
    object_type_init_quest_container();
    object_type_init_random_drop();
    object_type_init_ring();
    object_type_init_rod();
    object_type_init_rune();
    object_type_init_savebed();
    object_type_init_scroll();
    object_type_init_shield();
    object_type_init_shop_floor();
    object_type_init_sign();
    object_type_init_skill();
    object_type_init_skill_item();
    object_type_init_sound_ambient();
    object_type_init_spawn_point();
    object_type_init_spawn_point_info();
    object_type_init_spawn_point_mob();
    object_type_init_spell();
    object_type_init_spinner();
    object_type_init_swarm_spell();
    object_type_init_symptom();
    object_type_init_treasure();
    object_type_init_wall();
    object_type_init_wand();
    object_type_init_waypoint();
    object_type_init_wealth();
    object_type_init_weapon();
    object_type_init_word_of_recall();
}

/** @copydoc object_methods::apply_func */
int object_apply(object *op, object *applier, int aflags)
{
    object_methods *methods;

    applier = HEAD(applier);

    for (methods = &object_type_methods[op->type]; methods; methods = methods->fallback) {
        if (methods->apply_func) {
            return methods->apply_func(op, applier, aflags);
        }
    }

    return OBJECT_METHOD_UNHANDLED;
}

/** @copydoc object_methods::process_func */
void object_process(object *op)
{
    object_methods *methods;

    /* No need to process objects inside creators. */
    if (op->env && op->env->type == CREATOR) {
        return;
    }

    if (common_object_process_pre(op)) {
        return;
    }

    if (HAS_EVENT(op, EVENT_TIME)) {
        if (trigger_event(EVENT_TIME, NULL, op, NULL, NULL, 0, 0, 0, SCRIPT_FIX_NOTHING)) {
            return;
        }
    }

    for (methods = &object_type_methods[op->type]; methods; methods = methods->fallback) {
        if (methods->process_func) {
            methods->process_func(op);
            return;
        }
    }
}

/** @copydoc object_methods::describe_func */
char *object_describe(object *op, object *observer, char *buf, size_t size)
{
    object_methods *methods;

    for (methods = &object_type_methods[op->type]; methods; methods = methods->fallback) {
        if (methods->describe_func) {
            methods->describe_func(op, observer, buf, size);
            return buf;
        }
    }

    buf[0] = '\0';
    return buf;
}

/** @copydoc object_methods::move_on_func */
int object_move_on(object *op, object *victim, object *originator, int state)
{
    object_methods *methods;
    int ret;

    op = HEAD(op);
    victim = HEAD(victim);

    if (trigger_event(EVENT_TRIGGER, victim, op, originator, NULL, 0, 0, 0, SCRIPT_FIX_NOTHING)) {
        return OBJECT_METHOD_OK;
    }

    for (methods = &object_type_methods[op->type]; methods; methods = methods->fallback) {
        if (methods->move_on_func) {
            if (object_move_on_recursion_depth >= 500) {
                LOG(DEBUG, "Aborting recursion [op arch %s, name %s; victim arch %s, name %s]", op->arch->name, op->name, victim->arch->name, victim->name);
                return OBJECT_METHOD_OK;
            }

            object_move_on_recursion_depth++;
            ret = methods->move_on_func(op, victim, originator, state);
            object_move_on_recursion_depth--;

            return ret;
        }
    }

    return OBJECT_METHOD_UNHANDLED;
}

/** @copydoc object_methods::trigger_func */
int object_trigger(object *op, object *cause, int state)
{
    object_methods *methods;

    for (methods = &object_type_methods[op->type]; methods; methods = methods->fallback) {
        if (methods->trigger_func) {
            return methods->trigger_func(op, cause, state);
        }
    }

    return OBJECT_METHOD_UNHANDLED;
}

/** @copydoc object_methods::trigger_button_func */
int object_trigger_button(object *op, object *cause, int state)
{
    object_methods *methods;

    for (methods = &object_type_methods[op->type]; methods; methods = methods->fallback) {
        if (methods->trigger_button_func) {
            return methods->trigger_button_func(op, cause, state);
        }
    }

    return OBJECT_METHOD_UNHANDLED;
}

/** @copydoc object_methods::remove_map_func */
void object_callback_remove_map(object *op)
{
    object_methods *methods;

    for (methods = &object_type_methods[op->type]; methods; methods = methods->fallback) {
        if (methods->remove_map_func) {
            methods->remove_map_func(op);
            return;
        }
    }
}

/** @copydoc object_methods::remove_inv_func */
void object_callback_remove_inv(object *op)
{
    object_methods *methods;

    for (methods = &object_type_methods[op->type]; methods; methods = methods->fallback) {
        if (methods->remove_inv_func) {
            methods->remove_inv_func(op);
            return;
        }
    }
}

/** @copydoc object_methods::projectile_fire_func */
object *object_projectile_fire(object *op, object *shooter, int dir)
{
    object_methods *methods;

    for (methods = &object_type_methods[op->type]; methods; methods = methods->fallback) {
        if (methods->projectile_fire_func) {
            return methods->projectile_fire_func(op, shooter, dir);
        }
    }

    return NULL;
}

/** @copydoc object_methods::projectile_move_func */
object *object_projectile_move(object *op)
{
    object_methods *methods;

    for (methods = &object_type_methods[op->type]; methods; methods = methods->fallback) {
        if (methods->projectile_move_func) {
            return methods->projectile_move_func(op);
        }
    }

    return NULL;
}

/** @copydoc object_methods::projectile_hit_func */
int object_projectile_hit(object *op, object *victim)
{
    object_methods *methods;

    for (methods = &object_type_methods[op->type]; methods; methods = methods->fallback) {
        if (methods->projectile_hit_func) {
            return methods->projectile_hit_func(op, victim);
        }
    }

    return OBJECT_METHOD_UNHANDLED;
}

/** @copydoc object_methods::projectile_stop_func */
object *object_projectile_stop(object *op, int reason)
{
    object_methods *methods;

    if (trigger_event(EVENT_STOP, NULL, op, NULL, NULL, 0, 0, 0, SCRIPT_FIX_NOTHING)) {
        return op;
    }

    for (methods = &object_type_methods[op->type]; methods; methods = methods->fallback) {
        if (methods->projectile_stop_func) {
            return methods->projectile_stop_func(op, reason);
        }
    }

    return NULL;
}

/** @copydoc object_methods::ranged_fire_func */
int object_ranged_fire(object *op, object *shooter, int dir, double *delay)
{
    object_methods *methods;

    if (dir == 0 && op->type == BOW && shooter->type == PLAYER && OBJECT_VALID(CONTR(shooter)->target_object, CONTR(shooter)->target_object_count)) {
        rv_vector rv;

        dir = get_dir_to_target(shooter, CONTR(shooter)->target_object, &rv);
    }

    if (dir == 0) {
        dir = shooter->direction;

        /* Should not happen... */
        if (dir == 0) {
            return OBJECT_METHOD_UNHANDLED;
        }
    }

    if (QUERY_FLAG(shooter, FLAG_CONFUSED)) {
        dir = get_randomized_dir(dir);
    }

    shooter->direction = dir;

    for (methods = &object_type_methods[op->type]; methods; methods = methods->fallback) {
        if (methods->ranged_fire_func) {
            return methods->ranged_fire_func(op, shooter, dir, delay);
        }
    }

    return OBJECT_METHOD_UNHANDLED;
}
