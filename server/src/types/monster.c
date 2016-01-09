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
 * Monster memory, NPC interaction, AI, and other related functions
 * are in this file, all used by the @ref MONSTER "monster" type
 * objects.
 */

#include <global.h>
#include <packet.h>
#include <toolkit_string.h>
#include <monster_data.h>
#include <faction.h>
#include <plugin.h>
#include <monster_guard.h>
#include <arch.h>
#include <player.h>
#include <object.h>
#include <player.h>
#include <object_methods.h>
#include <waypoint.h>

static int can_detect_enemy(object *op, object *enemy, rv_vector *rv);
static object *find_nearest_enemy(object *ob);
static int move_randomly(object *op);
static int can_hit(object *ob1, rv_vector *rv);
static object *monster_choose_random_spell(object *monster, uint32_t flags);
static int monster_cast_spell(object *head, object *part, int dir, rv_vector *rv, uint32_t flags);
static int monster_use_bow(object *head, object *part, int dir);
static int dist_att(int dir, object *part, rv_vector *rv);
static int run_att(int dir, object *ob, object *part, rv_vector *rv);
static int hitrun_att(int dir, object *ob);
static int wait_att(int dir, object *ob, object *part, rv_vector *rv);
static int disthit_att(int dir, object *ob, object *part, rv_vector *rv);
static int wait_att2(int dir, rv_vector *rv);
static void circ1_move(object *ob);
static void circ2_move(object *ob);
static void pace_movev(object *ob);
static void pace_moveh(object *ob);
static void pace2_movev(object *ob);
static void pace2_moveh(object *ob);
static void rand_move(object *ob);

/**
 * Update (or clear) an NPC's enemy. Perform most of the housekeeping
 * related to switching enemies
 *
 * You should always use this method to set (or clear) an NPC's enemy.
 *
 * If enemy is given an aggro wp may be set up.
 * If rv is given, it will be filled out with the vector to enemy
 *
 * enemy and/or rv may be NULL.
 * @param npc
 * The NPC object we're setting enemy for.
 * @param enemy
 * The enemy object, NULL if we're clearing the enemy
 * for this NPC.
 * @param rv
 * Range vector of the enemy.
 */
void set_npc_enemy(object *npc, object *enemy, rv_vector *rv)
{
    object *aggro_wp;
    rv_vector rv2;

    /* Do nothing if new enemy == old enemy */
    if (enemy == npc->enemy && (enemy == NULL || enemy->count == npc->enemy_count)) {
        return;
    }

    /* Players don't need waypoints, speed updates or aggro counters */
    if (npc->type == PLAYER) {
        npc->enemy = enemy;

        if (enemy) {
            npc->enemy_count = enemy->count;
        }

        return;
    }

    /* Non-aggro-waypoint related stuff */
    if (enemy) {
        if (rv == NULL) {
            rv = &rv2;
        }

        get_rangevector(npc, enemy, rv, RV_DIAGONAL_DISTANCE);
        npc->enemy_count = enemy->count;

        /* important: that's our "we lose aggro count" - reset to zero here */
        npc->last_eat = 0;

        /* Monster has changed status from normal to attack - let's hear it! */
        if (npc->enemy == NULL && !QUERY_FLAG(npc, FLAG_FRIENDLY)) {
            play_sound_map(npc->map, CMD_SOUND_EFFECT, "growl.ogg", npc->x, npc->y, 0, 0);
        }

        if (QUERY_FLAG(npc, FLAG_UNAGGRESSIVE)) {
            /* The unaggressives look after themselves 8) */
            CLEAR_FLAG(npc, FLAG_UNAGGRESSIVE);
        }
    } else {
        object *base = living_get_base_info(npc);
        object *wp = waypoint_get_active(npc);

        if (base && !wp) {
            object *return_wp = waypoint_get_home(npc);

#ifdef DEBUG_PATHFINDING
            LOG(DEBUG, "%s lost aggro and is returning home (%s:%d,%d)", STRING_OBJ_NAME(npc), base->slaying, base->x, base->y);
#endif

            if (!return_wp) {
                return_wp = arch_to_object(arches[ARCH_WAYPOINT]);
                insert_ob_in_ob(return_wp, npc);
                return_wp->owner = npc;
                return_wp->ownercount = npc->count;
                FREE_AND_ADD_REF_HASH(return_wp->name, shstr_cons.home);
                /* mark as return-home wp */
                SET_FLAG(return_wp, FLAG_REFLECTING);
                /* mark as best-effort wp */
                SET_FLAG(return_wp, FLAG_NO_ATTACK);
            }

            return_wp->stats.hp = base->x;
            return_wp->stats.sp = base->y;
            FREE_AND_ADD_REF_HASH(return_wp->slaying, base->slaying);
            /* Activate wp */
            SET_FLAG(return_wp, FLAG_CURSED);
            /* reset best-effort timer */
            return_wp->stats.Int = 0;

            /* setup move_type to use waypoints */
            return_wp->move_type = npc->move_type;
            npc->move_type = (npc->move_type & LO4) | WPOINT;

            wp = return_wp;
        }

        /* TODO: add a little pause to the active waypoint */
    }

    npc->enemy = enemy;
    /* Update speed */
    set_mobile_speed(npc, 0);

    /* TODO: check intelligence against lower limit to allow pathfind */
    aggro_wp = waypoint_get_aggro(npc);

    /* Create a new aggro wp for npc? */
    if (!aggro_wp && enemy) {
        aggro_wp = arch_to_object(arches[ARCH_WAYPOINT]);
        insert_ob_in_ob(aggro_wp, npc);
        /* Mark as aggro WP */
        SET_FLAG(aggro_wp, FLAG_DAMNED);
        aggro_wp->owner = npc;
#ifdef DEBUG_PATHFINDING
        LOG(DEBUG, "created wp for '%s'", STRING_OBJ_NAME(npc));
#endif
    }

    /* Set up waypoint target (if we actually got a waypoint) */
    if (aggro_wp) {
        if (enemy) {
            aggro_wp->enemy_count = npc->enemy_count;
            aggro_wp->enemy = enemy;
            FREE_AND_ADD_REF_HASH(aggro_wp->name, enemy->name);
#ifdef DEBUG_PATHFINDING
            LOG(DEBUG, "got wp for '%s' -> '%s'", npc->name, enemy->name);
#endif
        } else {
            aggro_wp->enemy = NULL;
#ifdef DEBUG_PATHFINDING
            LOG(DEBUG, "cleared aggro wp for '%s'", npc->name);
#endif
        }
    }

    if (enemy != NULL) {
        monster_enemy_signal(npc, enemy);
        monster_guard_activate_gate(npc, 1);
        monster_data_dialogs_purge(npc);
    } else {
        object_set_value(npc, "was_provoked", NULL, 0);
    }
}

/**
 * Signal all linked monsters on the specified map about a possible enemy.
 * @param npc
 * Monster that is signaling.
 * @param map
 * Map to signal on.
 * @param spawn_point
 * NPC's spawn point.
 * @param dist
 * Maximum distance for non-linked spawn points.
 */
static void monster_enemy_signal_map(object *npc, mapstruct *map,
        object *spawn_point, uint32_t dist)
{
    objectlink *ol;

    for (ol = map->linked_spawn_points; ol != NULL; ol = ol->next) {
        if (ol->objlink.ob == spawn_point) {
            continue;
        }

        /* Ensure the spawn point has a spawned monster. */
        if (!OBJECT_VALID(ol->objlink.ob->enemy, ol->objlink.ob->enemy_count)) {
            continue;
        }

        /* Ensure the spawned monster doesn't yet have an enemy. */
        if (OBJECT_VALID(ol->objlink.ob->enemy->enemy,
                         ol->objlink.ob->enemy->enemy_count)) {
            continue;
        }

        rv_vector rv;

        if ((spawn_point->title == NULL ||
                ol->objlink.ob->title != spawn_point->title) &&
                (!get_rangevector(npc, ol->objlink.ob->enemy, &rv,
                RV_DIAGONAL_DISTANCE) || rv.distance > dist ||
                !monster_is_ally_of(npc, ol->objlink.ob->enemy))) {
            continue;
        }

        if (object_get_value(npc, "was_provoked") != NULL) {
            object_set_value(ol->objlink.ob->enemy, "was_provoked", "1", 1);
        }

        set_npc_enemy(ol->objlink.ob->enemy, npc->enemy, NULL);
    }
}

/**
 * Signal a change in monster's enemy.
 * @param npc
 * Monster/NPC.
 * @param enemy
 * New enemy.
 */
void monster_enemy_signal(object *npc, object *enemy)
{
    object *spawn_point_info;

    HARD_ASSERT(npc != NULL);
    HARD_ASSERT(enemy != NULL);

    SOFT_ASSERT(npc->map != NULL, "NPC has no map: %s", object_get_str(npc));
    SOFT_ASSERT(enemy->map != NULL, "Enemy has no map: %s",
                object_get_str(enemy));

    if (monster_signal_chance[npc->stats.Int] == 0 ||
            !rndm_chance(monster_signal_chance[npc->stats.Int])) {
        return;
    }

    spawn_point_info = present_in_ob(SPAWN_POINT_INFO, npc);

    if (spawn_point_info == NULL || !OBJECT_VALID(spawn_point_info->owner,
            spawn_point_info->ownercount)) {
        return;
    }

    int32_t dist = 0;
    rv_vector rv;

    if (get_rangevector(npc, enemy, &rv, 0)) {
        dist = (int32_t) ((double) npc->item_power * 1.5) - rv.distance;

        if (dist < 0) {
            dist = 0;
        }
    }

    /* Signal the map the spawn point is on. */
    monster_enemy_signal_map(npc, npc->map, spawn_point_info->owner, dist);

    /* Signal all the tiled maps that are in memory. */
    for (size_t i = 0; i < TILED_NUM_DIR; i++) {
        if (npc->map->tile_map[i] != NULL &&
                npc->map->tile_map[i]->in_memory == MAP_IN_MEMORY) {
            monster_enemy_signal_map(npc, npc->map->tile_map[i],
                                     spawn_point_info->owner, dist);
        }
    }
}

/**
 * Checks if NPC's enemy is still valid.
 * @param npc
 * The NPC object.
 * @param rv
 * Range vector of the enemy.
 * @return
 * Enemy object if valid, NULL otherwise.
 */
object *check_enemy(object *npc, rv_vector *rv)
{
    if (npc->enemy == NULL) {
        return NULL;
    }

    if (!OBJECT_VALID(npc->enemy, npc->enemy_count) || npc == npc->enemy || !IS_LIVE(npc->enemy) || is_friend_of(npc, npc->enemy)) {
        set_npc_enemy(npc, NULL, NULL);
        return NULL;
    }

    return can_detect_enemy(npc, npc->enemy, rv) ? npc->enemy : NULL;
}

/**
 * Tries to find an enemy for NPC. We pass the range vector since
 * our caller will find the information useful.
 * @param npc
 * The NPC object.
 * @param rv
 * Range vector.
 * @return
 * Enemy object if found, NULL otherwise.
 */
object *find_enemy(object *npc, rv_vector *rv)
{
    object *tmp = NULL;

    /* If we are berserk, we don't care about others - we attack all we can
     * find. */
    if (QUERY_FLAG(npc, FLAG_BERSERK)) {
        /* Always clear the attacker entry */
        npc->attacked_by = NULL;
        tmp = find_nearest_enemy(npc);

        if (tmp) {
            get_rangevector(npc, tmp, rv, 0);
        }

        return tmp;
    }

    tmp = check_enemy(npc, rv);

    if (!tmp) {
        /* If we have an attacker, check him */
        if (OBJECT_VALID(npc->attacked_by, npc->attacked_by_count) && !IS_INVISIBLE(npc->attacked_by, npc) && !QUERY_FLAG(npc->attacked_by, FLAG_INVULNERABLE)) {
            /* We don't want a fight evil vs evil or good against non evil. */
            if (is_friend_of(npc, npc->attacked_by)) {
                /* Skip it, but let's wake up */
                CLEAR_FLAG(npc, FLAG_SLEEP);
            } else if (on_same_map(npc, npc->attacked_by)) {
                /* The only thing we must know... */

                CLEAR_FLAG(npc, FLAG_SLEEP);
                set_npc_enemy(npc, npc->attacked_by, rv);
                /* Always clear the attacker entry */
                npc->attacked_by = NULL;

                /* Face our attacker */
                return npc->enemy;
            }
        }

        /* We have no legal enemy or attacker, so we try to target a new
         * one. */
        if (!QUERY_FLAG(npc, FLAG_UNAGGRESSIVE)) {
            tmp = find_nearest_enemy(npc);

            if (tmp != npc->enemy) {
                set_npc_enemy(npc, tmp, rv);
            }
        } else if (npc->enemy) {
            /* Make sure to clear the enemy, even if FLAG_UNAGRESSIVE is true */
            set_npc_enemy(npc, NULL, NULL);
        }
    }

    if (tmp != NULL) {
        monster_enemy_signal(npc, tmp);
    }

    return tmp;
}

/**
 * Controls if monster still can see/detect its enemy.
 *
 * Includes visibility but also map and area control.
 * @param op
 * The monster object.
 * @param enemy
 * Monster object's enemy.
 * @param rv
 * Range vector.
 * @return
 * 1 if can see/detect, 0 otherwise.
 */
static int can_detect_enemy(object *op, object *enemy, rv_vector *rv)
{
    /* Will check for legal maps too */
    if (!op || !enemy || !on_same_map(op, enemy)) {
        return 0;
    }

    /* We check for sys_invisible and normal */
    if (IS_INVISIBLE(enemy, op) || QUERY_FLAG(enemy, FLAG_INVULNERABLE)) {
        return 0;
    }

    if (!get_rangevector(op, enemy, rv, 0)) {
        return 0;
    }

    /* If our enemy is too far away ... */
    if ((int) rv->distance >= MAX(MAX_AGGRO_RANGE, op->item_power)) {
        /* Then start counting until our mob loses aggro... */
        if (++op->last_eat > MAX_AGGRO_TIME) {
            set_npc_enemy(op, NULL, NULL);
            return 0;
        }
    } else {
        /* Our mob is aggroed again - because target is in range again */
        op->last_eat = 0;
    }

    return 1;
}

/**
 * Check whether the monster is able to move at will. Certain things
 * may disable normal movement, such as player talking to the monster,
 * and the monster responding to the player and setting a period of time
 * to wait for until resuming movement.
 * @param op
 * Monster.
 * @return
 * Whether the monster can move.
 */
static bool monster_can_move(object *op)
{
    if (monster_data_dialogs_num(op) != 0) {
        return false;
    }

    return true;
}

/** @copydoc object_methods_t::process_func */
static void process_func(object *op)
{
    HARD_ASSERT(op != NULL);

    SOFT_ASSERT(op->head == NULL, "Called on a tail part: %s",
            object_get_str(op));

    /* Monsters not on maps don't do anything. */
    if (op->map == NULL) {
        return;
    }

    /* If we are here, we're never paralyzed anymore */
    CLEAR_FLAG(op, FLAG_PARALYZED);

    /* Cleanup stale interfaces. */
    monster_data_dialogs_cleanup(op);

    /* Here is the heart of the mob attack and target area.
     * find_enemy() checks the old enemy or gets us a new one. */

    object *enemy;
    rv_vector rv;
    /* We never ever attack */
    if (QUERY_FLAG(op, FLAG_NO_ATTACK)) {
        if (op->enemy != NULL) {
            set_npc_enemy(op, NULL, NULL);
        }

        enemy = NULL;
    } else if ((enemy = find_enemy(op, &rv)) != NULL) {
        CLEAR_FLAG(op, FLAG_SLEEP);
        op->direction = rv.direction;

        if (rv.distance <= 1 && rv.distance_z == 0) {
            op->anim_flags |= ANIM_FLAG_ATTACKING;
        }

        if (!enemy->attacked_by || (enemy->attacked_by && enemy->attacked_by_distance > (int) rv.distance)) {
            /* We have an enemy, just tell him we want him dead */
            enemy->attacked_by = op;
            enemy->attacked_by_count = op->count;
            /* Now the attacked foe knows how near we are */
            enemy->attacked_by_distance = (int16_t) rv.distance;
        }
    }

    /* Generate hp, if applicable */
    if (op->stats.Con && op->stats.hp < op->stats.maxhp) {
        if (++op->last_heal > 5) {
            op->last_heal = 0;
            op->stats.hp += op->stats.Con;

            if (op->stats.hp > op->stats.maxhp) {
                op->stats.hp = op->stats.maxhp;
            }
        }

        /* So if the monster has gained enough HP that they are no longer afraid
         * */
        if (QUERY_FLAG(op, FLAG_RUN_AWAY) && op->stats.hp >=
                (int32_t) (((double) op->run_away / 100.0) *
                (double) op->stats.maxhp)) {
            CLEAR_FLAG(op, FLAG_RUN_AWAY);
        }
    }

    /* Generate sp, if applicable */
    if (op->stats.Pow && op->stats.sp < op->stats.maxsp) {
        op->last_sp += (int16_t) (8.0 * (double) op->stats.Pow /
                FABS(op->speed));
        /* causes Pow/16 sp/tick */
        op->stats.sp += op->last_sp / 128;
        op->last_sp %= 128;

        if (op->stats.sp > op->stats.maxsp) {
            op->stats.sp = op->stats.maxsp;
        }
    }

    /* Time to regain some "guts"... */
    if (QUERY_FLAG(op, FLAG_SCARED) && rndm_chance(20)) {
        CLEAR_FLAG(op, FLAG_SCARED);
    }

    if (op->behavior & BEHAVIOR_SPELL_FRIENDLY) {
        if (op->last_grace) {
            op->last_grace--;
        }

        if (op->stats.Dex && rndm_chance(op->stats.Dex)) {
            if (QUERY_FLAG(op, FLAG_CAST_SPELL) && !op->last_grace) {
                if (monster_cast_spell(op, op, 0, NULL, SPELL_DESC_FRIENDLY)) {
                    /* Add monster casting delay */
                    op->last_grace += op->magic;
                }
            }
        }
    }

    /* If we don't have an enemy, do special movement or the like */
    if (enemy == NULL) {
        object *spawn_point_info;

        if (QUERY_FLAG(op, FLAG_ONLY_ATTACK) || ((spawn_point_info = present_in_ob(SPAWN_POINT_INFO, op)) && spawn_point_info->owner && !OBJECT_VALID(spawn_point_info->owner, spawn_point_info->ownercount))) {
            monster_drop_arrows(op);
            object_remove(op, 0);
            object_destroy(op);
            return;
        }

        if (!QUERY_FLAG(op, FLAG_STAND_STILL)) {
            if (op->move_type & HI4) {
                if (!monster_can_move(op)) {
                    return;
                }

                switch (op->move_type & HI4) {
                case CIRCLE1:
                    circ1_move(op);
                    break;

                case CIRCLE2:
                    circ2_move(op);
                    break;

                case PACEV:
                    pace_movev(op);
                    break;

                case PACEH:
                    pace_moveh(op);
                    break;

                case PACEV2:
                    pace2_movev(op);
                    break;

                case PACEH2:
                    pace2_moveh(op);
                    break;

                case RANDO:
                    rand_move(op);
                    break;

                case RANDO2:
                    move_randomly(op);
                    break;

                case WPOINT: {
                    object *wp = waypoint_get_active(op);
                    if (wp != NULL) {
                        waypoint_move(wp, op);
                    }
                    break;
                }
                }

                return;
            } else if (QUERY_FLAG(op, FLAG_RANDOM_MOVE)) {
                if (monster_can_move(op)) {
                    move_randomly(op);
                }
            }
        }

        return;
    }

    object *part = rv.part != NULL ? rv.part : op;
    int dir = rv.direction;

    /* Move the check for scared up here - if the monster was scared,
     * we were not doing any of the logic below, so might as well save
     * a few CPU cycles. */
    if (!QUERY_FLAG(op, FLAG_SCARED)) {
        if (op->last_grace) {
            op->last_grace--;
        }

        if (op->stats.Dex && rndm_chance(op->stats.Dex)) {
            if (QUERY_FLAG(op, FLAG_CAST_SPELL) && !op->last_grace) {
                if (monster_cast_spell(op, part, dir, &rv, SPELL_DESC_DIRECTION | SPELL_DESC_ENEMY | SPELL_DESC_SELF)) {
                    /* Add monster casting delay */
                    op->last_grace += op->magic;
                    return;
                }
            }

            if (QUERY_FLAG(op, FLAG_READY_BOW) && rndm_chance(4)) {
                if (monster_use_bow(op, part, dir) && rndm_chance(2)) {
                    return;
                }
            }
        }
    }

    if (QUERY_FLAG(op, FLAG_SCARED) || QUERY_FLAG(op, FLAG_RUN_AWAY)) {
        dir = absdir(dir + 4);
    }

    if (QUERY_FLAG(op, FLAG_CONFUSED)) {
        dir = get_randomized_dir(dir);
    }

    int special_dir = 0;
    if (!QUERY_FLAG(op, FLAG_SCARED)) {
        if (op->attack_move_type & LO4) {
            switch (op->attack_move_type & LO4) {
            case DISTATT:
                special_dir = dist_att(dir, part, &rv);
                break;

            case RUNATT:
                special_dir = run_att(dir, op, part, &rv);
                break;

            case HITRUN:
                special_dir = hitrun_att(dir, op);
                break;

            case WAITATT:
                special_dir = wait_att(dir, op, part, &rv);
                break;

            case RUSH:
            case ALLRUN:
                special_dir = dir;
                break;

            case DISTHIT:
                special_dir = disthit_att(dir, op, part, &rv);
                break;

            case WAIT2:
                special_dir = wait_att2(dir, &rv);
                break;

            default:
                LOG(DEBUG, "Illegal low mon-move: %d", op->attack_move_type & LO4);
            }

            if (special_dir == 0) {
                return;
            }
        }
    }

    /* Try to move closer to enemy, or follow whatever special attack behavior
     * is */
    if (!QUERY_FLAG(op, FLAG_STAND_STILL) && (QUERY_FLAG(op, FLAG_SCARED) || QUERY_FLAG(op, FLAG_RUN_AWAY) || !can_hit(part, &rv) || ((op->attack_move_type & LO4) && special_dir != dir))) {
        object *aggro_wp = waypoint_get_aggro(op);

        /* TODO: make (intelligent) monsters go to last known position of enemy
         * if out of range/sight */

        /* If special attack move -> follow it instead of going towards enemy */
        if (((op->attack_move_type & LO4) && special_dir != dir)) {
            aggro_wp = NULL;
            dir = special_dir;
        }

        /* If valid aggro wp (and no special attack), and not scared, use it for
         * movement */
        if (aggro_wp && aggro_wp->enemy && aggro_wp->enemy == op->enemy && (rv.distance_z != 0 || rv.distance > 1) && !QUERY_FLAG(op, FLAG_SCARED) && !QUERY_FLAG(op, FLAG_RUN_AWAY)) {
            waypoint_move(aggro_wp, op);
            return;
        } else {
            int maxdiff = (QUERY_FLAG(op, FLAG_ONLY_ATTACK) || rndm_chance(2)) ? 1 : 2;

            /* Can the monster move directly toward player? */
            if (move_object(op, dir)) {
                return;
            }

            /* Try move around corners if !close */
            for (int diff = 1; diff <= maxdiff; diff++) {
                /* try different detours */
                /* Try left or right first? */
                int m = 1 - (RANDOM() & 2);

                if (move_object(op, absdir(dir + diff * m)) ||
                        move_object(op, absdir(dir - diff * m))) {
                    return;
                }
            }
        }
    }

    /* Eneq(@csd.uu.se): Patch to make RUN_AWAY or SCARED monsters move a random
     * direction if they can't move away. */
    if (!QUERY_FLAG(op, FLAG_ONLY_ATTACK) && (QUERY_FLAG(op, FLAG_RUN_AWAY) || QUERY_FLAG(op, FLAG_SCARED))) {
        if (move_randomly(op)) {
            return;
        }
    }

    /* Hit enemy if possible */
    if (!QUERY_FLAG(op, FLAG_SCARED) && !QUERY_FLAG(enemy, FLAG_REMOVED) && can_hit(part, &rv)) {
        if (QUERY_FLAG(op, FLAG_RUN_AWAY)) {
            part->stats.wc -= 10;

            /* As long we are > 0, we are not ready to swing */
            if (op->weapon_speed_left <= 0) {
                skill_attack(enemy, part, 0, NULL);
                op->weapon_speed_left += FABS((int) op->weapon_speed_left) + 1;
            }

            part->stats.wc += 10;
        } else {
            /* As long we are > 0, we are not ready to swing */
            if (op->weapon_speed_left <= 0) {
                skill_attack(enemy, part, 0, NULL);
                op->weapon_speed_left += FABS((int) op->weapon_speed_left) + 1;
            }
        }
    }

    /* Might be freed by ghost-attack or hit-back */
    if (OBJECT_FREE(part)) {
        return;
    }

    if (QUERY_FLAG(op, FLAG_ONLY_ATTACK)) {
        destruct_ob(op);
        return;
    }

    monster_data_enemy_update(op, enemy);
}

/**
 * Initialize the monster type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(monster)
{
    OBJECT_METHODS(MONSTER)->process_func = process_func;
}

/**
 * Check if monster can detect target (invisibility and being in range).
 * @param op
 * Monster.
 * @param target
 * The target to check.
 * @param range
 * Range this object can see.
 * @param srange
 * Stealth range this object can see.
 * @param rv
 * Range vector.
 * @return
 * 1 if can detect target, 0 otherwise.
 */
static int can_detect_target(object *op, object *target, unsigned int range,
        unsigned int srange, rv_vector *rv)
{
    /* We check for sys_invisible and normal */
    if (IS_INVISIBLE(target, op) || QUERY_FLAG(target, FLAG_INVULNERABLE)) {
        return 0;
    }

    if (!get_rangevector(op, target, rv, 0)) {
        return 0;
    }

    if (QUERY_FLAG(target, FLAG_STEALTH) && !QUERY_FLAG(op, FLAG_XRAYS)) {
        if (srange < rv->distance) {
            return 0;
        }
    } else {
        if (range < rv->distance) {
            return 0;
        }
    }

    return 1;
}

/**
 * Finds nearest enemy for a monster.
 * @param ob
 * The monster.
 * @return
 * Nearest enemy, NULL if none.
 */
static object *find_nearest_enemy(object *ob)
{
    object *tmp;
    int aggro_range, aggro_stealth;
    rv_vector rv;
    int i, j, xt, yt;
    mapstruct *m;

    aggro_range = ob->item_power;

    if (ob->enemy || ob->attacked_by) {
        aggro_range += 3;
    }

    if (QUERY_FLAG(ob, FLAG_SLEEP) || QUERY_FLAG(ob, FLAG_BLIND)) {
        aggro_range /= 2;
        aggro_stealth = aggro_range - 2;
    } else {
        aggro_stealth = aggro_range - 2;
    }

    if (aggro_stealth < MIN_MON_RADIUS) {
        aggro_stealth = MIN_MON_RADIUS;
    }

    for (i = -aggro_range; i <= aggro_range; i++) {
        for (j = -aggro_range; j <= aggro_range; j++) {
            xt = ob->x + i;
            yt = ob->y + j;

            if (!(m = get_map_from_coord2(ob->map, &xt, &yt))) {
                continue;
            }

            /* Nothing alive here? Move on... */
            if (!(GET_MAP_FLAGS(m, xt, yt) & (P_IS_MONSTER | P_IS_PLAYER))) {
                continue;
            }

            for (tmp = GET_MAP_OB(m, xt, yt); tmp; tmp = tmp->above) {
                /* Get head. */
                if (tmp->head) {
                    tmp = tmp->head;
                }

                /* Skip the monster looking for enemy and not alive objects. */
                if (tmp == ob || !IS_LIVE(tmp)) {
                    continue;
                }

                if (!can_detect_target(ob, tmp, aggro_range, aggro_stealth,
                        &rv)) {
                    continue;
                }

                if (!obj_in_line_of_sight(tmp, &rv)) {
                    continue;
                }

                /* Now check the friend status, whether we can reach the enemy,
                 * and LOS. */
                if (!is_friend_of(ob, tmp)) {
                    return tmp;
                }

                monster_guard_check(ob, tmp, NULL, rv.distance);
            }
        }
    }

    return NULL;
}

/**
 * Randomly move a monster.
 * @param op
 * The monster object to move.
 * @return
 * 1 if the monster was moved, 0 otherwise.
 */
static int move_randomly(object *op)
{
    int i, r;
    int dirs[8] = {1, 2, 3, 4, 5, 6, 7, 8};

    mapstruct *basemap = NULL;
    rv_vector rv;
    if (op->item_race != 0 || op->item_level != 0) {
        object *base = living_find_base_info(op);
        if (base != NULL) {
            basemap = ready_map_name(base->slaying, NULL, MAP_NAME_SHARED);
            if (basemap != NULL) {
                if (!get_rangevector_from_mapcoords(basemap, base->x, base->y,
                        op->map, op->x, op->y, &rv, RV_NO_DISTANCE)) {
                    basemap = NULL;
                }
            }
        }
    }

    /* Give up to 8 chances for a monster to move randomly */
    for (i = 0; i < 8; i++) {
        int t = dirs[i];

        /* Perform a single random shuffle of the remaining directions */
        r = i + rndm(0, 8 - i - 1);
        dirs[i] = dirs[r];
        dirs[r] = t;

        r = dirs[i];

        /* Check x and y direction of possible move against limit parameters */
        if (basemap != NULL) {
            if (abs(rv.distance_x + freearr_x[r]) > op->item_race) {
                continue;
            }

            if (abs(rv.distance_y + freearr_y[r]) > op->item_level) {
                continue;
            }
        }

        if (HAS_EVENT(op, EVENT_AI)) {
            int ret = trigger_event(EVENT_AI,
                                    NULL,
                                    op,
                                    NULL,
                                    NULL,
                                    EVENT_AI_RANDOM_MOVE,
                                    r,
                                    0,
                                    0);
            if (ret == 1) {
                /* Cancel random movement. */
                return 0;
            } else if (ret == 2) {
                /* Keep trying. */
                continue;
            }
        }

        if (move_object(op, r)) {
            return 1;
        }
    }

    return 0;
}

/**
 * Check if object can hit another object.
 * @param ob1
 * Monster object.
 * @param rv
 * Range vector.
 * @return
 * 1 if can hit, 0 otherwise.
 */
static int can_hit(object *ob1, rv_vector *rv)
{
    if (QUERY_FLAG(ob1, FLAG_CONFUSED) && !rndm_chance(3)) {
        return 0;
    }

    return rv->distance_z == 0 && abs(rv->distance_x) < 2 &&
            abs(rv->distance_y) < 2;
}

#define MAX_KNOWN_SPELLS 20

/**
 * Choose a random spell this monster could cast.
 * @param monster
 * The monster object.
 * @param flags
 * Flags the spell must have.
 * @return
 * Random spell object, NULL if no spell found.
 */
static object *monster_choose_random_spell(object *monster, uint32_t flags)
{
    object * altern[MAX_KNOWN_SPELLS], *tmp;
    spell_struct *spell;
    int i = 0, j;

    for (tmp = monster->inv; tmp != NULL; tmp = tmp->below) {
        if (tmp->type == ABILITY) {
            /* Check and see if it's actually a useful spell */
            if ((spell = find_spell(tmp->stats.sp)) != NULL && !(spell->path & (PATH_INFO | PATH_TRANSMUTE | PATH_TRANSFER | PATH_LIGHT)) && spell->flags & flags) {
                if (tmp->stats.maxsp) {
                    for (j = 0; i < MAX_KNOWN_SPELLS && j < tmp->stats.maxsp; j++) {
                        altern[i++] = tmp;
                    }
                } else {
                    altern[i++] = tmp;
                }

                if (i == MAX_KNOWN_SPELLS) {
                    break;
                }
            }

        }
    }

    if (!i) {
        return NULL;
    }

    return altern[rndm(1, i) - 1];
}

/**
 * Check if it's worth it for monster to cast a spell, based on the target.
 * @param target
 * Target.
 * @param spell_id
 * Spell ID being checked.
 * @return
 * 1 if it's worth it, 0 otherwise.
 */
static int monster_spell_useful(object *target, int spell_id)
{
    switch (spell_id) {
    case SP_MINOR_HEAL:
    case SP_GREATER_HEAL:
        return target->stats.hp != target->stats.maxhp;
    }

    return 1;
}

/**
 * Tries to make a monster cast a spell.
 * @param head
 * Head of the monster.
 * @param part
 * Part of the monster that we use to cast.
 * @param dir
 * Direction to cast.
 * @param rv
 * Range vector describing where the enemy is. If NULL, will attempt
 * to find a friendly object to cast the spell on.
 * @param flags
 * Flags the spell must have.
 * @return
 * 1 if monster casted a spell, 0 otherwise.
 */
static int monster_cast_spell(object *head, object *part, int dir, rv_vector *rv, uint32_t flags)
{
    object *spell_item, *target = NULL;
    spell_struct *sp;
    int sp_typ, ability;

    if ((spell_item = monster_choose_random_spell(head, flags)) == NULL) {
        /* Will be turned on when picking up book */
        CLEAR_FLAG(head, FLAG_CAST_SPELL);
        return 0;
    }

    /* Only considering long range spells if we're not looking for friendly
     * target. */
    if (spell_item->stats.hp != -1 && rv) {
        /* Alternate long-range spell: check how far away enemy is */
        if (rv->distance > 6) {
            sp_typ = spell_item->stats.hp;
        } else {
            sp_typ = spell_item->stats.sp;
        }
    } else {
        sp_typ = spell_item->stats.sp;

        /* Not looking for friendly target, but this is a friendly spell, and
         * it's not the
         * same as long range one? */
        if (rv && spells[sp_typ].flags & SPELL_DESC_FRIENDLY && sp_typ != spell_item->stats.hp) {
            return 0;
        }
    }

    if (sp_typ == -1 || (sp = find_spell(sp_typ)) == NULL) {
        return 0;
    }

    /* Monster doesn't have enough spellpoints */
    if (head->stats.sp < SP_level_spellpoint_cost(head, sp_typ, -1)) {
        return 0;
    }

    /* Spell should be cast on caster (ie, heal, strength) */
    if (sp->flags & SPELL_DESC_SELF) {
        dir = 0;

        if (rv && !monster_spell_useful(head, sp_typ)) {
            return 0;
        }
    }

    if (!rv) {
        int i, j, xt, yt;
        object *tmp;
        mapstruct *m;

        for (i = -sp->range; !target && i <= sp->range; i++) {
            for (j = -sp->range; !target && j <= sp->range; j++) {
                xt = head->x + i;
                yt = head->y + j;

                if (!(m = get_map_from_coord2(head->map, &xt, &yt))) {
                    continue;
                }

                /* Nothing alive here? Move on... */
                if (!(GET_MAP_FLAGS(m, xt, yt) & (P_IS_MONSTER | P_IS_PLAYER))) {
                    continue;
                }

                for (tmp = GET_MAP_OB(m, xt, yt); tmp; tmp = tmp->above) {
                    /* Get head. */
                    if (tmp->head) {
                        tmp = tmp->head;
                    }

                    /* Skip the monster and not alive objects. */
                    if (tmp == head || !IS_LIVE(tmp)) {
                        continue;
                    }

                    if (is_friend_of(head, tmp) && monster_spell_useful(tmp, sp_typ)) {
                        target = tmp;
                        break;
                    }
                }
            }
        }

        if (!target) {
            return 0;
        }
    }

    ability = (spell_item->type == ABILITY && QUERY_FLAG(spell_item, FLAG_IS_MAGICAL));

    head->stats.sp -= SP_level_spellpoint_cost(head, sp_typ, -1);
    /* Add default cast time from spell force to monster */
    head->last_grace += spell_item->last_grace;

    /* If we're casting this spell on another object, we need to adjust some
     * parameters accordingly... */
    if (target) {
        return cast_spell(target, part, dir, sp_typ, ability, CAST_NPC, NULL);
    }

    /* Otherwise a normal cast. */
    return cast_spell(part, part, dir, sp_typ, ability, CAST_NORMAL, NULL);
}

/**
 * Tries to make a (part of a) monster fire a bow.
 * @param head
 * Head of the monster.
 * @param part
 * Part of the monster that we use to fire.
 * @param dir
 * Direction to cast.
 * @return
 * 1 if monster fired something, 0 otherwise.
 */
static int monster_use_bow(object *head, object *part, int dir)
{
    FOR_INV_PREPARE(head, tmp) {
        if (tmp->type == BOW && QUERY_FLAG(tmp, FLAG_APPLIED)) {
            object_ranged_fire(tmp, part, dir, NULL);
            return 1;
        }
    } FOR_INV_FINISH();

    LOG(ERROR, "Monster %s has FLAG_READY_BOW without a bow.",
            object_get_str(head));
    CLEAR_FLAG(head, FLAG_READY_BOW);
    return 0;
}

/**
 * Monster does a distance attack.
 * @param dir
 * Direction.
 * @param part
 * Part of the object.
 * @param rv
 * Range vector.
 * @return
 * New direction.
 */
static int dist_att(int dir, object *part, rv_vector *rv)
{
    if (can_hit(part, rv)) {
        return dir;
    }

    if (rv->distance < 10) {
        return absdir(dir + 4);
    } else if (rv->distance > 18) {
        return dir;
    }

    return 0;
}

/**
 * Monster runs.
 * @param dir
 * Direction.
 * @param ob
 * The monster.
 * @param part
 * Part of the monster.
 * @param rv
 * Range vector.
 * @return
 * New direction.
 */
static int run_att(int dir, object *ob, object *part, rv_vector *rv)
{
    if ((can_hit(part, rv) && ob->move_status < 20) || ob->move_status < 20) {
        ob->move_status++;
        return dir;
    } else if (ob->move_status > 20) {
        ob->move_status = 0;
    }

    return absdir(dir + 4);
}

/**
 * Hit and run type of attack.
 * @param dir
 * Direction.
 * @param ob
 * Monster.
 * @return
 * New direction.
 */
static int hitrun_att(int dir, object *ob)
{
    if (ob->move_status++ < 25) {
        return dir;
    } else if (ob->move_status < 50) {
        return absdir(dir + 4);
    } else {
        ob->move_status = 0;
    }

    return absdir(dir + 4);
}

/**
 * Wait, and attack.
 * @param dir
 * Direction.
 * @param ob
 * Monster.
 * @param part
 * Part of the monster.
 * @param rv
 * Range vector.
 * @return
 * New direction.
 */
static int wait_att(int dir, object *ob, object *part, rv_vector *rv)
{
    if (ob->move_status || can_hit(part, rv)) {
        ob->move_status++;
    }

    if (ob->move_status == 0) {
        return 0;
    } else if (ob->move_status < 10) {
        return dir;
    } else if (ob->move_status < 15) {
        return absdir(dir + 4);
    }

    ob->move_status = 0;
    return 0;
}

/**
 * Distance hit attack.
 * @param dir
 * Direction.
 * @param ob
 * Monster.
 * @param part
 * Part of the monster.
 * @param rv
 * Range vector.
 * @return
 * New direction.
 */
static int disthit_att(int dir, object *ob, object *part, rv_vector *rv)
{
    if (ob->stats.maxhp && (ob->stats.hp * 100) / ob->stats.maxhp < ob->run_away) {
        return absdir(dir + 4);
    }

    return dist_att(dir, part, rv);
}

/**
 * Wait and attack.
 * @param dir
 * Direction.
 * @param rv
 * Range vector.
 * @return
 * New direction.
 */
static int wait_att2(int dir, rv_vector *rv)
{
    if (rv->distance < 9) {
        return absdir(dir + 4);
    }

    return 0;
}

/**
 * Circle type of move.
 * @param ob
 * Monster.
 */
static void circ1_move(object *ob)
{
    static const int circle[12] = {3, 3, 4, 5, 5, 6, 7, 7, 8, 1, 1, 2};

    if (++ob->move_status > 11) {
        ob->move_status = 0;
    }

    if (!move_object(ob, circle[ob->move_status])) {
        move_object(ob, rndm(1, 8));
    }
}

/**
 * Different type of circle type move.
 * @param ob
 * Monster.
 */
static void circ2_move(object *ob)
{
    static const int circle[20] = {3, 3, 3, 4, 4, 5, 5, 5, 6, 6, 7, 7, 7, 8, 8, 1, 1, 1, 2, 2};

    if (++ob->move_status > 19) {
        ob->move_status = 0;
    }

    if (!move_object(ob, circle[ob->move_status])) {
        move_object(ob, rndm(1, 8));
    }
}

/**
 * Vertical pace movement.
 * @param ob
 * Monster.
 */
static void pace_movev(object *ob)
{
    if (ob->move_status++ > 6) {
        ob->move_status = 0;
    }

    if (ob->move_status < 4) {
        move_object(ob, 5);
    } else {
        move_object(ob, 1);
    }
}

/**
 * Horizontal pace movement.
 * @param ob
 * Monster.
 */
static void pace_moveh(object *ob)
{
    if (ob->move_status++ > 6) {
        ob->move_status = 0;
    }

    if (ob->move_status < 4) {
        move_object(ob, 3);
    } else {
        move_object(ob, 7);
    }
}

/**
 * Another type of vertical pace movement.
 * @param ob
 * Monster.
 */
static void pace2_movev(object *ob)
{
    if (ob->move_status++ > 16) {
        ob->move_status = 0;
    }

    if (ob->move_status < 6) {
        move_object(ob, 5);
    } else if (ob->move_status < 8) {
        return;
    } else if (ob->move_status < 13) {
        move_object(ob, 1);
    }
}

/**
 * Another type of horizontal pace movement.
 * @param ob
 * Monster.
 */
static void pace2_moveh(object *ob)
{
    if (ob->move_status++ > 16) {
        ob->move_status = 0;
    }

    if (ob->move_status < 6) {
        move_object(ob, 3);
    } else if (ob->move_status < 8) {
        return;
    } else if (ob->move_status < 13) {
        move_object(ob, 7);
    }
}

/**
 * Random movement.
 * @param ob
 * Monster.
 */
static void rand_move(object *ob)
{
    int i;

    if (ob->move_status < 1 || ob->move_status > 8 || !move_object(ob, ob->move_status || rndm_chance(9))) {
        for (i = 0; i < 5; i++) {
            if (move_object(ob, ob->move_status = rndm(1, 8))) {
                return;
            }
        }
    }
}

/**
 * This function takes the message to be parsed in 'msg', the text to
 * match in 'match', and returns the portion of the message.
 * @param msg
 * Message to parse.
 * @param match
 * Text to try to match.
 * @return
 * Returned portion which should be freed later, NULL if there
 * was no match.
 */
static char *find_matching_message(const char *msg, const char *match)
{
    const char *cp = msg, *cp1, *cp2;
    char regex[MAX_BUF], *cp3;
    int gotmatch = 0;

    while (1) {
        if (strncmp(cp, "@match ", 7)) {
            LOG(DEBUG, "Invalid message: %s", msg);
            return NULL;
        } else {
            /* Find the end of the line, and copy the regex portion into it */
            cp2 = strchr(cp + 7, '\n');

            if (!cp2) {
                LOG(DEBUG, "Found empty match response: %s", msg);
                return NULL;
            }

            strncpy(regex, cp + 7, (cp2 - cp - 7));
            regex[cp2 - cp - 7] = '\0';

            /* Find the next match command */
            cp1 = strstr(cp + 6, "\n@match");

            /* Got a match - handle * as special case. */
            if (regex[0] == '*') {
                gotmatch = 1;
            } else {
                char *pipe_sep, *pnext = NULL;

                /* Need to parse all the | separators.  Our re_cmp isn't
                 * really a fully blown regex parser. */
                for (pipe_sep = regex; pipe_sep; pipe_sep = pnext) {
                    pnext = strchr(pipe_sep, '|');

                    if (pnext) {
                        *pnext = '\0';
                        pnext++;
                    }

                    if (re_cmp(match, pipe_sep)) {
                        gotmatch = 1;
                        break;
                    }
                }
            }

            if (gotmatch) {
                if (cp1) {
                    cp3 = emalloc(cp1 - cp2 + 1);
                    strncpy(cp3, cp2 + 1, cp1 - cp2);
                    cp3[cp1 - cp2 - 1] = '\0';
                } else {
                    /* If no next match, just want the rest of the string */
                    cp3 = estrdup(cp2 + 1);
                }

                return cp3;
            }

            gotmatch = 0;

            if (cp1) {
                cp = cp1 + 1;
            } else {
                return NULL;
            }
        }
    }
}

/**
 * Give an object the chance to handle something being said.
 *
 * Plugin hooks will be called.
 * @param op
 * Who is talking.
 * @param npc
 * Object to try to talk to.
 * @param txt
 * What op is saying.
 * @return
 * 1 if the NPC replied to the player, 0 otherwise.
 */
int talk_to_npc(object *op, object *npc, char *txt)
{
    size_t ret = 0;

    if (HAS_EVENT(npc, EVENT_SAY)) {
        /* Trigger the SAY event */
        ret = trigger_event(EVENT_SAY, op, npc, NULL, txt, 0, 0, 0, 0);
    } else if (npc->msg != NULL && *npc->msg == '@') {
        char *cp = find_matching_message(npc->msg, txt);

        if (cp != NULL) {
            ret = strlen(cp);

            if (op->type == PLAYER) {
                packet_struct *packet = packet_new(CLIENT_CMD_INTERFACE, 256,
                        256);

                packet_debug_data(packet, 0, "\nInterface data type");
                packet_append_uint8(packet, CMD_INTERFACE_TEXT);
                packet_debug_data(packet, 0, "Text");
                packet_append_string_len(packet, cp, ret);
                packet_append_uint8(packet, '\0');

                packet_debug_data(packet, 0, "\nInterface data type");
                packet_append_uint8(packet, CMD_INTERFACE_ANIM);
                packet_debug_data(packet, 0, "Animation ID");
                packet_append_uint16(packet, npc->animation_id);
                packet_debug_data(packet, 0, "Animation speed");
                packet_append_uint8(packet, npc->anim_speed);
                packet_debug_data(packet, 0, "Direction");
                packet_append_uint8(packet, npc->direction);

                packet_debug_data(packet, 0, "\nInterface data type");
                packet_append_uint8(packet, CMD_INTERFACE_TITLE);
                packet_debug_data(packet, 0, "Title");
                packet_append_string_terminated(packet, npc->name);

                socket_send_packet(&CONTR(op)->socket, packet);
            } else {
                char buf[HUGE_BUF];
                char *name = object_get_name_s(npc, NULL);
                snprintf(buf, sizeof(buf), "\n%s says: %s", name, cp);
                efree(name);
                draw_info_map(CHAT_TYPE_GAME, NULL, COLOR_WHITE, op->map, op->x,
                        op->y, MAP_INFO_NORMAL, op, op, buf);
            }

            efree(cp);
        }
    }

    if (op->type == PLAYER) {
        player_set_talking_to(CONTR(op), npc);
    }

    uint32_t secs = INTERFACE_TIMEOUT(ret);
    monster_data_dialogs_add(npc, op, MIN(secs, INTERFACE_TIMEOUT_MAX));

    return ret != 0;
}

/**
 * Check if object op is friend of obj.
 * @param op
 * The first object
 * @param obj
 * The second object to check against the first one
 * @return
 * 1 if both objects are friends, 0 otherwise
 */
int is_friend_of(object *op, object *obj)
{
    if (op == NULL || obj == NULL) {
        return 0;
    }

    /* We are obviously friends with ourselves. */
    if (op == obj) {
        return 1;
    }

    if (!IS_LIVE(op) && !IS_LIVE(obj)) {
        return 0;
    }

    /* Berserk */
    if (QUERY_FLAG(op, FLAG_BERSERK)) {
        return 0;
    }

    /* If on PVP area, they won't be friendly */
    if (pvp_area(op, obj)) {
        return 0;
    }

    if (op->type == PLAYER && obj->type == PLAYER) {
        return 1;
    }

    if (op->type == MONSTER && OBJECT_VALID(op->enemy, op->enemy_count) &&
            obj == op->enemy) {
        return 0;
    }

    if (obj->type == MONSTER && OBJECT_VALID(obj->enemy, obj->enemy_count) &&
            op == obj->enemy) {
        return 0;
    }

    if (op->type == PLAYER && CONTR(op)->combat_force) {
        return 0;
    }

    if (op->type != PLAYER) {
        shstr *name = object_get_value(op, "faction");
        if (name == NULL) {
            return 0;
        }

        faction_t faction = faction_find(name);
        if (faction == NULL || !faction_is_friend(faction, obj)) {
            return 0;
        }
    }

    if (obj->type != PLAYER) {
        shstr *name = object_get_value(obj, "faction");
        if (name == NULL) {
            return 0;
        }

        faction_t faction = faction_find(name);
        if (faction == NULL || !faction_is_friend(faction, op)) {
            return 0;
        }
    }

    return 1;
}

/**
 * Checks if using weapon 'item' would be better for 'who'.
 * @param who
 * Creature considering to apply item.
 * @param item
 * Item to check.
 * @return
 * 1 if item is a better object, 0 otherwise.
 */
int check_good_weapon(object *who, object *item)
{
    object *other_weap;
    int val = 0, i;

    /* Cursed or damned; never better. */
    if (QUERY_FLAG(item, FLAG_CURSED) || QUERY_FLAG(item, FLAG_DAMNED)) {
        return 0;
    }

    for (other_weap = who->inv; other_weap; other_weap = other_weap->below) {
        if (other_weap->type == item->type && QUERY_FLAG(other_weap, FLAG_APPLIED)) {
            break;
        }
    }

    /* No other weapons */
    if (other_weap == NULL) {
        return 1;
    }

    val = item->stats.dam - other_weap->stats.dam;
    val += (item->magic - other_weap->magic) * 3;

    /* Monsters don't really get benefits from things like regen rates
     * from items. But the bonus for their stats are very important. */
    for (i = 0; i < NUM_STATS; i++) {
        val += (get_attr_value(&item->stats, i) - get_attr_value(&other_weap->stats, i)) * 2;
    }

    if (val > 0) {
        CLEAR_FLAG(other_weap, FLAG_APPLIED);
        return 1;
    }

    return 0;
}

/**
 * Checks if using armor 'item' would be better for 'who'.
 * @param who
 * Creature considering to apply item.
 * @param item
 * Item to check.
 * @return
 * 1 if item is a better object, 0 otherwise.
 */
int check_good_armour(object *who, object *item)
{
    object *other_armour;
    int val = 0, i;

    /* Cursed or damned; never better. */
    if (QUERY_FLAG(item, FLAG_CURSED) || QUERY_FLAG(item, FLAG_DAMNED)) {
        return 0;
    }

    for (other_armour = who->inv; other_armour; other_armour = other_armour->below) {
        if (other_armour->type == item->type && QUERY_FLAG(other_armour, FLAG_APPLIED)) {
            break;
        }
    }

    /* No other armour, use the new */
    if (other_armour == NULL) {
        return 1;
    }

    /* See which is better */
    val = item->stats.ac - other_armour->stats.ac;
    val += (item->magic - other_armour->magic) * 3;

    for (i = 0; i < NROFATTACKS; i++) {
        if (item->protection[i] > other_armour->protection[i]) {
            val++;
        } else if (item->protection[i] < other_armour->protection[i]) {
            val--;
        }
    }

    if (val > 0) {
        CLEAR_FLAG(other_armour, FLAG_APPLIED);
        return 1;
    }

    return 0;
}

bool monster_is_ally_of(object *op, object *target)
{
    shstr *op_faction_name = object_get_value(op, "faction");

    if (op_faction_name == NULL) {
        return false;
    }

    shstr *target_faction_name = object_get_value(target, "faction");

    if (target_faction_name == NULL) {
        return false;
    }

    faction_t op_faction = faction_find(op_faction_name);

    if (op_faction == NULL) {
        return false;
    }

    faction_t target_faction = faction_find(target_faction_name);

    if (target_faction == NULL) {
        return false;
    }

    return faction_is_alliance(op_faction, target_faction);
}

/**
 * Makes the specified monster drop the arrows that were stuck into it.
 *
 * @param op Monster. Must be on a map.
 */
void
monster_drop_arrows (object *op)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(op->type == MONSTER);
    SOFT_ASSERT(op->map != NULL, "Monster is not on a map: %s",
                object_get_str(op));

    FOR_INV_PREPARE(op, tmp) {
        if (tmp->type == ARROW && tmp->attacked_by_count != 0) {
            object_remove(tmp, 0);
            tmp->x = op->x;
            tmp->y = op->y;
            insert_ob_in_map(tmp, op->map, op, 0);
        }
    } FOR_INV_FINISH();
}
