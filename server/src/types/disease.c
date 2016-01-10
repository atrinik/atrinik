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
 * This file contains all the code implementing @ref DISEASE "diseases",
 * except for odds and ends in @ref attack.c and in @ref living.c.
 *
 * Diseases may be contagious. They are objects which exist in a player's
 * inventory. They themselves do nothing, except modify
 * @ref SYMPTOM "symptoms", or spread to other live objects.
 * @ref SYMPTOM "symptoms" are what actually damage the player.
 */

#include <global.h>
#include <arch.h>
#include <exp.h>
#include <object.h>
#include <object_methods.h>
#include <disease.h>

/**
 * Check if victim is susceptible to disease.
 *
 * @param op
 * Disease to check.
 * @param victim
 * Victim.
 * @return
 * True if the victim can be infected, false otherwise.
 */
static bool
disease_is_susceptible (object *op, object *victim)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(victim != NULL);

    if (!IS_LIVE(victim)) {
        return false;
    }

    if (!QUERY_FLAG(victim, FLAG_UNDEAD) && strstr(op->race, "*") != NULL) {
        return true;
    }

    if (QUERY_FLAG(victim, FLAG_UNDEAD) && strstr(op->race, "undead") != NULL) {
        return true;
    }

    if ((victim->race != NULL && strstr(op->race, victim->race) != NULL) ||
        strstr(op->race, victim->name) != NULL) {
        return true;
    }

    return false;
}

/**
 * Find a symptom for a disease in disease's env.
 *
 * @param op
 * The disease.
 * @return
 * Matching symptom object, NULL if not found.
 */
static object *
disease_find_symptom (object *op)
{
    HARD_ASSERT(op != NULL);

    FOR_INV_PREPARE(op->env, tmp) {
        if (tmp->type == SYMPTOM && strcmp(tmp->name, op->name) == 0) {
            return tmp;
        }
    } FOR_INV_FINISH();

    return NULL;
}

/**
 * Remove any symptoms of disease.
 *
 * @param op
 * The disease.
 */
static void
disease_remove_symptoms (object *op)
{
    HARD_ASSERT(op != NULL);

    object *symptom = disease_find_symptom(op);
    if (symptom != NULL) {
        object_destruct(symptom);
    }
}

/**
 * Searches around for more victims to infect.
 *
 * @param op
 * Disease infecting.
 */
static void
disease_check_infection (object *op)
{
    HARD_ASSERT(op != NULL);

    int x, y;
    mapstruct *map;
    if (op->env != NULL) {
        x = op->env->x;
        y = op->env->y;
        map = op->env->map;
    } else {
        x = op->x;
        y = op->y;
        map = op->map;
    }

    if (map == NULL) {
        return;
    }

    int range = abs(op->magic);
    for (int i = -range; i <= range; i++) {
        for (int j = -range; j <= range; j++) {
            int xt = x + i;
            int yt = y + j;
            mapstruct *m = get_map_from_coord(map, &xt, &yt);
            if (m == NULL) {
                continue;
            }

            if (!(GET_MAP_FLAGS(m, xt, yt) & (P_IS_MONSTER | P_IS_PLAYER))) {
                continue;
            }

            FOR_MAP_PREPARE(m, xt, yt, tmp) {
                if (!IS_LIVE(tmp)) {
                    continue;
                }

                rv_vector rv;
                if (!get_rangevector(op->env ? op->env : op, tmp, &rv, 0) ||
                    !obj_in_line_of_sight(tmp, &rv)) {
                    continue;
                }

                disease_infect(op, tmp, 0);
            } FOR_MAP_FINISH();
        }
    }
}

/**
 * This function monitors the symptoms caused by the disease (if any),
 * causes symptoms, and modifies existing symptoms in the case of
 * existing diseases.
 *
 * @param op
 * The disease.
 */
static void
disease_do_symptoms (object *op)
{
    HARD_ASSERT(op != NULL);

    object *victim = op->env;
    /* No-one to inflict symptoms on. */
    if (victim == NULL) {
        return;
    }

    victim = HEAD(victim);
    object *symptom = disease_find_symptom(op);

    /* No symptom? Generate one. */
    if (symptom == NULL) {
        /* First check and see if the carrier of the disease
         * is immune. If so, no symptoms!  */
        if (!disease_is_susceptible(op, victim)) {
            return;
        }

        FOR_INV_PREPARE(victim, tmp) {
            if (tmp->type != SIGN) {
                continue;
            }

            if (strcmp(tmp->name, op->name) == 0 && tmp->level >= op->level) {
                return;
            }
        } FOR_INV_FINISH();

        object *new_symptom = arch_get("symptom");

        /* Something special done with dam. We want diseases to be more
         * random in what they'll kill, so we'll make the damage they
         * do random. */
        if (op->stats.dam != 0) {
            /* Reduce the damage, on average, 50%, and making things random. */
            int16_t dam = rndm(1, FABS(op->stats.dam));
            if (op->stats.dam < 0) {
                dam = -dam;
            }
            new_symptom->stats.dam = dam;
        }

        new_symptom->stats.maxsp = op->stats.maxsp;
        new_symptom->stats.food = -1;

        FREE_AND_COPY_HASH(new_symptom->name, op->name);
        new_symptom->level = op->level;
        new_symptom->speed = op->speed;
        new_symptom->value = 0;
        new_symptom->stats.Str = op->stats.Str;
        new_symptom->stats.Dex = op->stats.Dex;
        new_symptom->stats.Con = op->stats.Con;
        new_symptom->stats.Int = op->stats.Int;
        new_symptom->stats.Pow = op->stats.Pow;
        new_symptom->stats.sp  = op->stats.sp;
        new_symptom->stats.food = op->last_eat;
        new_symptom->stats.maxsp = op->stats.maxsp;
        new_symptom->last_sp = op->last_sp;
        new_symptom->stats.exp = 0;
        new_symptom->stats.hp = op->stats.hp;
        FREE_AND_COPY_HASH(new_symptom->msg, op->msg);
        new_symptom->other_arch = op->other_arch;

        for (int i = 0; i < NROFATTACKS; i++) {
            if (op->attack[i] != 0) {
                new_symptom->attack[i] = op->attack[i];
            }
        }

        object_owner_set(new_symptom, op->owner);

        /* Unfortunately, set_owner does the wrong thing to the skills pointers
         * resulting in exp going into the owners *current* chosen skill. */
        new_symptom->chosen_skill = op->chosen_skill;

        CLEAR_FLAG(new_symptom, FLAG_NO_PASS);
        object_insert_into(new_symptom, victim, 0);
        return;
    }

    /* Now deal with progressing diseases: we increase the negative effects
     * caused by the symptoms.  */
    if (op->stats.ac != 0) {
        symptom->value += op->stats.ac;
        double scale = 1.0 + symptom->value / 100.0;

        symptom->stats.Str *= scale;
        symptom->stats.Dex *= scale;
        symptom->stats.Con *= scale;
        symptom->stats.Int *= scale;
        symptom->stats.Pow *= scale;
        symptom->stats.dam *= scale;
        symptom->stats.sp *= scale;
        symptom->stats.food = scale * op->last_eat;
        symptom->stats.maxsp *= scale;
        symptom->last_sp *= scale;
        symptom->stats.exp = 0;
        symptom->stats.hp *= scale;
        FREE_AND_COPY_HASH(symptom->msg, op->msg);
        symptom->other_arch = op->other_arch;
    }

    SET_FLAG(symptom, FLAG_APPLIED);
    living_update(victim);
}

/**
 * Grants immunity to a disease.
 *
 * @param op
 * Disease to grant immunity to.
 */
static void
disease_grant_immunity (object *op)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(op->env != NULL);

    /* Don't give immunity to this disease if last_heal is set. */
    if (op->last_heal != 0) {
        return;
    }

    /* Try to update an existing immunity. */
    FOR_INV_PREPARE(op->env, tmp) {
        if (tmp->type == SIGN && strcmp(op->name, tmp->name) == 0) {
            tmp->level = op->level;
            return;
        }
    } FOR_INV_FINISH();

    object *immunity = arch_get("immunity");
    FREE_AND_COPY_HASH(immunity->name, op->name);
    immunity->level = op->level;
    CLEAR_FLAG(immunity, FLAG_NO_PASS);
    object_insert_into(immunity, op->env, 0);
}

/** @copydoc object_methods_t::process_func */
static void
process_func (object *op)
{
    HARD_ASSERT(op != NULL);

    /* Determine if the disease is inside or outside of someone.
     * If outside, we decrement 'value' until we're gone. */
    if (op->env == NULL) {
        op->value--;

        if (op->value == 0) {
            /* Drop inv since disease may carry secondary infections. */
            object_destruct(op);
            return;
        }
    } else {
        /* If we're inside a person, have the disease run its course.
         * Negative foods denote "perpetual" diseases. */
        if (op->stats.food > 0 && disease_is_susceptible(op, op->env)) {
            op->stats.food--;

            if (op->stats.food == 0) {
                /* Remove the symptoms of this disease. */
                disease_remove_symptoms(op);
                disease_grant_immunity(op);
                /* Drop inv since disease may carry secondary infections. */
                object_destruct(op);
                return;
            }
        }
    }

    /* Check to see if we infect others. */
    disease_check_infection(op);

    /* Impose or modify the symptoms of the disease. */
    if (op->env != NULL) {
        disease_do_symptoms(op);
    }
}

/**
 * Initialize the disease type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(disease)
{
    OBJECT_METHODS(DISEASE)->process_func = process_func;
}

/**
 * Try to infect something with a disease. Rules are:
 * - Objects with immunity aren't infectable.
 * - Objects already infected aren't infectable.
 * - Dead objects aren't infectable.
 * - Undead objects are infectable only if specifically named.
 *
 * @param victim
 * Victim to try infect.
 * @param disease
 * The disease.
 * @param force
 * Don't do a random check for infection. Other checks
 * (susceptible to disease, not immune, and so on) are still done.
 * @return
 * True if the victim was infected, false otherwise.
 */
bool
disease_infect (object *op, object *victim, bool force)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(victim != NULL);

    victim = HEAD(victim);

    if (!IS_LIVE(victim)) {
        return false;
    }

    object *owner = object_owner(op);
    if (owner != NULL && is_friend_of(owner, victim)) {
        return false;
    }

    if (!disease_is_susceptible(op, victim)) {
        return false;
    }

    if (!force && rndm(0, 126) >= op->stats.wc) {
        return false;
    }

    FOR_INV_PREPARE(victim, tmp) {
        if (tmp->type == SIGN || tmp->type == DISEASE) {
            if (strcmp(tmp->name, op->name) == 0 && tmp->level >= op->level) {
                return false;
            }
        }
    } FOR_INV_FINISH();

    /* If we've gotten this far, go ahead and infect the victim. */
    object *new_disease = object_get();
    object_copy(new_disease, op, false);
    new_disease->stats.food = -1;
    new_disease->value = op->stats.maxhp;
    /* self-limiting factor */
    new_disease->stats.wc -= op->last_grace;

    /* Unfortunately, set_owner does the wrong thing to the skills pointers
     * resulting in exp going into the owners *current* chosen skill. */
    if (owner != NULL) {
        object_owner_set(new_disease, op->owner);
        new_disease->chosen_skill = op->chosen_skill;
    }

    new_disease = object_insert_into(new_disease, victim, 0);
    SOFT_ASSERT_RC(new_disease != NULL, false,
                   "Failed to insert disease into %s",
                   object_get_str(victim));
    CLEAR_FLAG(new_disease, FLAG_NO_PASS);

    if (new_disease->owner != NULL && new_disease->owner->type == PLAYER) {
        char buf[MAX_BUF];

        /* if the disease has a title, it has a special infection message */
        if (new_disease->title != NULL) {
            snprintf(VS(buf), "%s %s!!", op->title, victim->name);
        } else {
            snprintf(VS(buf), "You infect %s with your disease, %s!",
                     victim->name, new_disease->name);
        }

        if (victim->type == PLAYER) {
            draw_info(COLOR_RED, new_disease->owner, buf);
        } else {
            draw_info(COLOR_WHITE, new_disease->owner, buf);
        }
    }

    if (victim->type == PLAYER) {
        draw_info(COLOR_RED, victim, "You suddenly feel ill.");
    }

    return true;
}

/**
 * Possibly infect due to direct physical contact.
 *
 * @param op
 * The victim.
 * @param hitter
 * The hitter.
 */
void
disease_physically_infect (object *op, object *hitter)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(hitter != NULL);

    FOR_INV_PREPARE(hitter, tmp) {
        if (tmp->type == DISEASE) {
            disease_infect(tmp, op, 0);
        }
    } FOR_INV_FINISH();
}

/**
 * Do the cure disease stuff, from the spell "cure disease".
 *
 * @param op
 * Who is getting cured.
 * @param caster
 * Spell object used for curing. If NULL all diseases are removed,
 * otherwise only those of lower level than caster or randomly chosen.
 * @return
 * True if at least one disease was cured, false otherwise.
 */
bool
disease_cure (object *op, object *caster)
{
    HARD_ASSERT(op != NULL);

    if (caster != NULL) {
        draw_info_format(COLOR_WHITE, op, "%s casts cure disease on you!",
                         caster->name);
    }

    bool success = false;
    bool is_diseased = false;
    FOR_INV_PREPARE(op, tmp) {
        if (tmp->type != DISEASE) {
            continue;
        }

        is_diseased = true;

        int diff;
        if (caster == NULL) {
            diff = 1;
        } else {
            diff = tmp->level - caster->level;
        }

        if (diff > 1 && !rndm_chance(diff)) {
            draw_info_format(COLOR_WHITE, op,
                             "The disease %s resists the cure spell!",
                             tmp->name);

            if (caster != NULL) {
                draw_info_format(COLOR_WHITE, caster,
                                 "The disease %s resists the cure spell!",
                                 tmp->name);
            }

            continue;
        }

        draw_info_format(COLOR_WHITE, op,
                         "You are healed from disease %s.",
                         tmp->name);

        if (caster != NULL) {
            draw_info_format(COLOR_WHITE, caster,
                             "You heal %s from disease %s.",
                             op->name, tmp->name);
        }

        disease_remove_symptoms(tmp);
        object_remove(tmp, 0);
        object_destroy(tmp);
        success = true;
    } FOR_INV_FINISH();

    if (!is_diseased) {
        draw_info(COLOR_WHITE, op, "You are not diseased!");

        if (caster != NULL) {
            draw_info_format(COLOR_WHITE, caster, "%s is not diseased!",
                             op->name);
        }
    }

    return success;
}

/**
 * Reduces disease progression.
 *
 * @param op
 * The sufferer.
 * @param reduction
 * How much to reduce the disease progression.
 * @return
 * True if we actually reduce a disease, false otherwise.
 */
bool
disease_reduce_symptoms (object *op, int reduction)
{
    HARD_ASSERT(op != NULL);

    bool success = false;
    FOR_INV_PREPARE(op, tmp) {
        if (tmp->type != SYMPTOM) {
            continue;
        }

        success = true;
        tmp->value = MAX(0, tmp->value - 2 * reduction);
        /* Give the disease time to modify this symptom,
         * and reduce its severity. */
        tmp->speed_left = 0;
    } FOR_INV_FINISH();

    if (success) {
        draw_info(COLOR_WHITE, op, "Your illness seems less severe.");
    }

    return success;
}
