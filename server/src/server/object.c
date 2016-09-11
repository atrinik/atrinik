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
 * Object related code.
 */

#include <global.h>
#include <loader.h>
#include <toolkit_string.h>
#include <monster_data.h>
#include <plugin.h>
#include <arch.h>
#include <object.h>
#include <player.h>
#include <object_methods.h>
#include <door.h>

/** List of active objects that need to be processed */
object *active_objects;

/**
 * Gender nouns.
 */
const char *gender_noun[GENDER_MAX] = {
    "neuter", "male", "female", "hermaphrodite"
};
/**
 * Subjective pronouns.
 */
const char *gender_subjective[GENDER_MAX] = {
    "it", "he", "she", "it"
};
/**
 * Subjective pronouns, with first letter in uppercase.
 */
const char *gender_subjective_upper[GENDER_MAX] = {
    "It", "He", "She", "It"
};
/**
 * Objective pronouns.
 */
const char *gender_objective[GENDER_MAX] = {
    "it", "him", "her", "it"
};
/**
 * Possessive pronouns.
 */
const char *gender_possessive[GENDER_MAX] = {
    "its", "his", "her", "its"
};
/**
 * Reflexive pronouns.
 */
const char *gender_reflexive[GENDER_MAX] = {
    "itself", "himself", "herself", "itself"
};

/**
 * X offset when searching around a spot.
 */
int freearr_x[SIZEOFFREE] = {
    /* Same tile */
    0,
    /* One square away */
    0, 1, 1, 1, 0, -1, -1, -1,
    /* Two squares away */
    0, 1, 2, 2, 2, 2, 2, 1, 0, -1, -2, -2, -2, -2, -2, -1,
    /* Three squares away */
    0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 2, 1, 0, -1, -2, -3, -3,
    -3, -3, -3, -3, -3, -2, -1,
};

/**
 * Y offset when searching around a spot.
 */
int freearr_y[SIZEOFFREE] = {
    /* Same tile */
    0,
    /* One square away */
    -1, -1, 0, 1, 1, 1, 0, -1,
    /* Two squares away */
    -2, -2, -2, -1, 0, 1, 2, 2, 2, 2, 2, 1, 0, -1, -2, -2,
    /* Three squares away */
    -3, -3, -3, -3, -2, -1, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3,
    2, 1, 0, -1, -2, -3, -3, -3,
};

/**
 * Number of spots around a location, including that location (except for 0).
 */
int maxfree[SIZEOFFREE] = {
    /* Same tile */
    0,
    /* One square away */
    9, 10, 13, 14, 17, 18, 21, 22,
    /* Two squares away */
    25, 26, 27, 30, 31, 32, 33, 36, 37, 39, 39, 42, 43, 44, 45, 48,
    /* Three squares away */
    49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
    49, 49, 49, 49, 49, 49, 49, 49,
};

/**
 * Direction we're pointing on this spot.
 */
int freedir[SIZEOFFREE] = {
    /* Same tile */
    0,
    /* One square away */
    1, 2, 3, 4, 5, 6, 7, 8,
    /* Two squares away */
    1, 2, 2, 2, 3, 4, 4, 4, 5, 6, 6, 6, 7, 8, 8, 8,
    /* Three squares away */
    1, 2, 2, 2, 2, 2, 3, 4, 4, 4, 4, 4, 5, 6, 6, 6, 6, 6, 7, 8, 8, 8, 8, 8,
};

/**
 * The object memory pool.
 */
static mempool_struct *pool_object;

/**
 * This is a list of pointers that correspond to the FLAG_.. values.
 * This is a simple 1:1 mapping - if FLAG_FRIENDLY is 15, then the 15'th
 * element of this array should match that name.
 *
 * If an entry is NULL, that is a flag not to be loaded/saved.
 * @see flag_defines
 */
const char *object_flag_names[NUM_FLAGS + 1] = {
    "sleep", "confused", NULL, "scared", "is_blind",
    "is_invisible", "is_ethereal", "is_good", "no_pick", "walk_on",
    "no_pass", "is_animated", "slow_move", "flying", "monster",
    "friendly", NULL, "been_applied", "auto_apply", NULL,
    "is_neutral", "see_invisible", "can_roll", "connect_reset", "is_turnable",
    "walk_off", "fly_on", "fly_off", "is_used_up", "identified",
    "reflecting", "changing", "splitting", "hitback", "startequip",
    "blocksview", "undead", "can_stack", "unaggressive", "reflect_missile",
    "reflect_spell", "no_magic", "no_fix_player", "is_evil", "soulbound",
    "run_away", "pass_thru", "can_pass_thru", "outdoor", "unique",
    "no_drop", "is_indestructible", "can_cast_spell", NULL, "two_handed",
    "can_use_bow", "can_use_armour", "can_use_weapon", "connect_no_push",
    "connect_no_release", "has_ready_bow", "xrays", NULL, "is_floor",
    "lifesave", "is_magical", NULL, "stand_still", "random_move", "only_attack",
    NULL, "stealth", NULL, NULL, "cursed",
    "damned", "is_buildable", "no_pvp", NULL, NULL,
    "is_thrown", NULL, NULL, "is_male", "is_female",
    "applied", "inv_locked", NULL, NULL, NULL,
    "has_ready_weapon", "no_skill_ident", NULL, "can_see_in_dark", "is_cauldron",
    "is_dust", NULL, "one_hit", "draw_double_always", "berserk",
    "no_attack", "invulnerable", "quest_item", "is_trapped", NULL,
    NULL, NULL, NULL, NULL, NULL,
    "sys_object", "use_fix_pos", "unpaid", "hidden", "make_invisible",
    "make_ethereal", "is_player", "is_named", NULL, "no_teleport",
    "corpse", "corpse_forced", "player_only", NULL, "one_drop",
    "cursed_perm", "damned_perm", "door_closed", "is_spell", "is_missile",
    "draw_direction", "draw_double", "is_assassin", NULL, "no_save",
    NULL
};

/** @copydoc chunk_debugger */
static void
object_debugger (object *op, char *buf, size_t size)
{
    snprintf(buf, size, "count: %d", op->count);

    if (op->name != NULL) {
        SET_FLAG(op, FLAG_IDENTIFIED);
        char *name = object_get_name_s(op, NULL);
        snprintfcat(buf, size, " name: %s", name);
        efree(name);
    }

    snprintfcat(buf, size, " coords: %d, %d", op->x, op->y);
}

/** @copydoc chunk_validator */
static bool
object_validator (object *op)
{
    return op->count != 0 && !QUERY_FLAG(op, FLAG_REMOVED);
}

/**
 * Initialize the object API.
 */
void
object_init (void)
{
    pool_object = mempool_create("objects",
                                 OBJECT_EXPAND,
                                 sizeof(object),
                                 MEMPOOL_ALLOW_FREEING,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL);
    mempool_set_debugger(pool_object, (chunk_debugger) object_debugger);
    mempool_set_validator(pool_object, (chunk_validator) object_validator);
}

/**
 * Deinitialize the object API.
 */
void
object_deinit (void)
{
}

/**
 * Compares value lists.
 *
 * @param op
 * What to search.
 * @param cmp
 * Where to search.
 * @return
 * True if every key_values in 'op' has a partner with the same value
 * in 'cmp'.
 */
static inline bool
object_can_merge_key_values_one (const object *op, const object *cmp)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(cmp != NULL);

    /* For each field in wants. */
    for (key_value_t *op_field = op->key_values;
         op_field != NULL;
         op_field = op_field->next) {
        key_value_t *cmp_field = object_get_key_link(cmp, op_field->key);
        if (cmp_field == NULL) {
            return false;
        }

        /* Found the matching field. */
        if (cmp_field->value != op_field->value) {
            return false;
        }
    }

    return true;
}

/**
 * Check if two objects have the same key values and thus can be merged.
 *
 * @param ob1
 * Object to check.
 * @param ob2
 * Object to check.
 * @return
 * True if ob1 has the same key_values as ob2.
 */
static inline bool
object_can_merge_key_values (const object *ob1, const object *ob2)
{
    HARD_ASSERT(ob1 != NULL);
    HARD_ASSERT(ob2 != NULL);

    return (object_can_merge_key_values_one(ob1, ob2) &&
            object_can_merge_key_values_one(ob2, ob1));
}

/**
 * Examines two objects, and returns true if they can be merged together.
 *
 * @param ob1
 * The first object.
 * @param ob2
 * The second object.
 * @return
 * True if the two object can merge, false otherwise.
 */
bool
object_can_merge (object *ob1, object *ob2)
{
    HARD_ASSERT(ob1 != NULL);
    HARD_ASSERT(ob2 != NULL);

    if (!QUERY_FLAG(ob1, FLAG_CAN_STACK) && ob1->type != EVENT_OBJECT) {
        return false;
    }

    if (ob1 == ob2) {
        return false;
    }

    /* Do not merge objects if nrof would overflow. We use INT32_MAX
     * because int32_t is often used to store nrof instead of uint32_t. */
    if (ob1->nrof + ob2->nrof > INT32_MAX) {
        return false;
    }

    /* Do not merge objects with different layer/sub-layer. */
    if (ob1->layer != ob2->layer || ob1->sub_layer != ob2->sub_layer) {
        return false;
    }

    /* Do not allow merging objects if either has nrof of 0 and it's
     * not an event object (those normally have nrof of 0 but they are
     * allowed to merge). */
    if ((ob1->nrof == 0 || ob2->nrof == 0) && ob1->type != EVENT_OBJECT) {
        return false;
    }

    /* Do not ever merge objects with glow radius, since more objects with
     * the same glow_radius actually generate more light than one object. */
    if (ob1->glow_radius || ob2->glow_radius) {
        return false;
    }

    /* Do not merge arrows with different owners. */
    if (ob1->type == ARROW && ob2->type == ARROW &&
        ob1->attacked_by_count != 0 && ob2->attacked_by_count != 0 &&
        ob1->attacked_by_count != ob2->attacked_by_count) {
        return false;
    }

    /* Check attributes that cannot ever merge if they're different. */
    if (ob1->arch               != ob2->arch ||
        ob1->item_condition     != ob2->item_condition ||
        ob1->item_level         != ob2->item_level ||
        ob1->item_power         != ob2->item_power ||
        ob1->item_quality       != ob2->item_quality ||
        ob1->item_race          != ob2->item_race ||
        ob1->item_skill         != ob2->item_skill ||
        ob1->last_grace         != ob2->last_grace ||
        ob1->level              != ob2->level ||
        ob1->magic              != ob2->magic ||
        ob1->material           != ob2->material ||
        ob1->material_real      != ob2->material_real ||
        ob1->other_arch         != ob2->other_arch ||
        ob1->path_attuned       != ob2->path_attuned ||
        ob1->path_denied        != ob2->path_denied ||
        ob1->path_repelled      != ob2->path_repelled ||
        ob1->randomitems        != ob2->randomitems ||
        ob1->sub_type           != ob2->sub_type ||
        ob1->terrain_flag       != ob2->terrain_flag ||
        ob1->terrain_type       != ob2->terrain_type ||
        ob1->type               != ob2->type ||
        ob1->value              != ob2->value ||
        ob1->weight             != ob2->weight) {
        return false;
    }

    if (!DBL_EQUAL(ob1->speed, ob2->speed) ||
        !DBL_EQUAL(ob1->weapon_speed, ob2->weapon_speed)) {
        return false;
    }

    /* If the inventory consists only of event objects, and the event objects
     * are the same, allow merging. */
    if (ob1->inv != NULL || ob2->inv != NULL) {
        if (ob1->inv == NULL || ob2->inv == NULL) {
            return false;
        }

        /* Check that all inv objects are event objects */
        object *tmp1, *tmp2;
        for (tmp1 = ob1->inv, tmp2 = ob2->inv;
             tmp1 != NULL && tmp2 != NULL;
             tmp1 = tmp1->below, tmp2 = tmp2->below) {
            if (tmp1->type != EVENT_OBJECT || tmp2->type != EVENT_OBJECT) {
                return false;
            }
        }

        if (tmp1 != NULL || tmp2 != NULL) {
            /* Different number of event objects. */
            return false;
        }

        for (tmp1 = ob1->inv; tmp1 != NULL; tmp1 = tmp1->below) {
            for (tmp2 = ob2->inv; tmp2 != NULL; tmp2 = tmp2->below) {
                if (object_can_merge(tmp1, tmp2)) {
                    break;
                }
            }

            /* Couldn't find something to merge event from ob1 with? */
            if (tmp2 == NULL) {
                return false;
            }
        }
    }

    /* Check the shared strings of both objects. */
    if (ob1->name           != ob2->name ||
        ob1->title          != ob2->title ||
        ob1->race           != ob2->race ||
        ob1->slaying        != ob2->slaying ||
        ob1->msg            != ob2->msg ||
        ob1->artifact       != ob2->artifact ||
        ob1->custom_name    != ob2->custom_name ||
        ob1->glow           != ob2->glow) {
        return false;
    }

    /* Compare arrays and structures the object has (stats, protections, etc) */
    if (memcmp(&ob1->stats, &ob2->stats, sizeof(living)) != 0 ||
        memcmp(&ob1->attack, &ob2->attack, sizeof(ob1->attack)) != 0 ||
        memcmp(&ob1->protection,
               &ob2->protection,
               sizeof(ob1->protection)) != 0) {
        return false;
    }

    /* Ignore REMOVED and BEEN_APPLIED */
    if ((ob1->flags[0] | FLAG_BITMASK(FLAG_REMOVED) |
         FLAG_BITMASK(FLAG_BEEN_APPLIED)) !=
        (ob2->flags[0] | FLAG_BITMASK(FLAG_REMOVED) |
         FLAG_BITMASK(FLAG_BEEN_APPLIED)) ||
        (ob1->flags[1]) != (ob2->flags[1]) ||
        (ob1->flags[2] | FLAG_BITMASK(FLAG_APPLIED)) !=
        (ob2->flags[2] | FLAG_BITMASK(FLAG_APPLIED)) ||
        (ob1->flags[3]) != (ob2->flags[3])) {
        return false;
    }

    /* Compare face and animation IDs. */
    if (ob1->face               != ob2->face ||
        ob1->inv_face           != ob2->inv_face ||
        ob1->animation_id       != ob2->animation_id ||
        ob1->inv_animation_id   != ob2->inv_animation_id) {
        return false;
    }

    /* Avoid merging empty containers. */
    if (ob1->type == CONTAINER) {
        return false;
    }

    /* At least one of these has key_values. */
    if (ob1->key_values != NULL || ob2->key_values != NULL) {
        /* One has fields, but the other one doesn't. */
        if ((ob1->key_values == NULL) != (ob2->key_values == NULL)) {
            return false;
        }

        return object_can_merge_key_values(ob1, ob2);
    }

    return true;
}

/**
 * Tries to merge 'op' with items above and below the object.
 *
 * @param op
 * Object to merge.
 * @return
 * 'op', or the object 'op' was merged into.
 */
object *
object_merge (object *op)
{
    HARD_ASSERT(op != NULL);

    if (op->nrof == 0 || !QUERY_FLAG(op, FLAG_CAN_STACK)) {
        return op;
    }

    object *tmp;
    if (op->map != NULL) {
        tmp = GET_MAP_OB_LAST(op->map, op->x, op->y);
    } else if (op->env != NULL) {
        tmp = op->env->inv;
    } else {
        return op;
    }

    for ( ; tmp != NULL; tmp = tmp->below) {
        if (tmp != op && object_can_merge(op, tmp)) {
            tmp->nrof += op->nrof;
            object_update(tmp, UP_OBJ_FACE);
            esrv_update_item(UPD_NROF, tmp);

            object_remove(op, REMOVE_NO_WEIGHT);
            object_destroy(op);
            return tmp;
        }
    }

    return op;
}

/**
 * Recursive function to calculate the weight an object is carrying.
 *
 * It goes through in figures out how much containers are carrying, and
 * sums it up.
 *
 * @param op
 * The object to calculate the weight for
 * @return
 * The calculated weight
 */
uint32_t
object_weight_sum (object *op)
{
    HARD_ASSERT(op != NULL);

    if (QUERY_FLAG(op, FLAG_SYS_OBJECT)) {
        return 0;
    }

    uint32_t sum = 0;
    for (object *tmp = op->inv; tmp != NULL; tmp = tmp->below) {
        if (QUERY_FLAG(tmp, FLAG_SYS_OBJECT)) {
            continue;
        }

        if (tmp->inv != NULL) {
            object_weight_sum(tmp);
        }

        sum += WEIGHT_NROF(tmp, tmp->nrof);
    }

    if (op->type == CONTAINER && !DBL_EQUAL(op->weapon_speed, 1.0)) {
        /* We'll store the calculated value in damage_round_tag, so
         * we can use that as 'cache' for unmodified carrying weight.
         * This allows us to reliably calculate the weight again in
         * object_weight_add() and object_weight_sub() without
         * rounding errors. */
        op->damage_round_tag = sum;
        sum = sum * op->weapon_speed;
    }

    op->carrying = sum;
    return sum;
}

/**
 * Adds the specified weight to an object, and also updates how much the
 * environment(s) is/are carrying.
 *
 * @param op
 * The object.
 * @param weight
 * The weight to add.
 */
void
object_weight_add (object *op, uint32_t weight)
{
    HARD_ASSERT(op != NULL);

    while (op != NULL) {
        if (op->type == CONTAINER && !DBL_EQUAL(op->weapon_speed, 1.0)) {
            uint32_t old_carrying = op->carrying;
            op->damage_round_tag += weight;
            op->carrying = op->damage_round_tag * op->weapon_speed;
            weight = op->carrying - old_carrying;
        } else {
            op->carrying += weight;
        }

        if (op->env != NULL && op->env->type == PLAYER) {
            esrv_update_item(UPD_WEIGHT, op);
        }

        op = op->env;
    }
}

/**
 * Recursively (outwards) subtracts a number from the weight of an object
 * (and what is carried by its environment(s)).
 *
 * @param op
 * The object.
 * @param weight
 * The weight to subtract.
 */
void
object_weight_sub (object *op, uint32_t weight)
{
    HARD_ASSERT(op != NULL);

    while (op != NULL) {
        if (op->type == CONTAINER && !DBL_EQUAL(op->weapon_speed, 1.0)) {
            uint32_t old_carrying = op->carrying;
            op->damage_round_tag -= weight;
            op->carrying = op->damage_round_tag * op->weapon_speed;
            weight = old_carrying - op->carrying;
        } else {
            op->carrying -= weight;
        }

        if (op->env != NULL && op->env->type == PLAYER) {
            esrv_update_item(UPD_WEIGHT, op);
        }

        op = op->env;
    }
}

/**
 * Acquire the outermost environment object for a given object.
 *
 * @param op
 * Object we want the environment of.
 * @return
 * The outermost environment object for a given object. Never NULL.
 */
object *
object_get_env (object *op)
{
    HARD_ASSERT(op != NULL);

    while (op->env != NULL) {
        op = op->env;
    }

    return op;
}

/**
 * Check if the specified object is somewhere inside another object's
 * inventory, regardless of the inventory nesting level.
 *
 * @param op
 * The object to check.
 * @param inv
 * Inventory the object should be in.
 * @return
 * True if the checked object is somewhere inside the specified inventory,
 * false otherwise.
 */
bool
object_is_in_inventory (const object *op, const object *inv)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(inv != NULL);

    do {
        if (op->env == inv) {
            return true;
        }

        op = op->env;
    } while (op != NULL);

    return false;
}

/**
 * Dumps an object.
 *
 * @param op
 * Object to dump. Can be NULL.
 * @param sb
 * Buffer that will contain object information. Must not be NULL.
 */
void
object_dump (const object *op, StringBuffer *sb)
{
    HARD_ASSERT(sb != NULL);

    if (op == NULL) {
        stringbuffer_append_string(sb, "[NULL pointer]");
        return;
    }

    if (op->arch != NULL) {
        stringbuffer_append_printf(sb, "arch %s\n",
                                   op->arch->name != NULL ? op->arch->name :
                                       "(null)");
        get_ob_diff(sb, op, &arches[ARCH_EMPTY_ARCHETYPE]->clone);
        stringbuffer_append_string(sb, "end\n");
    } else {
        stringbuffer_append_string(sb, "Object ");
        stringbuffer_append_string(sb, op->name == NULL ? "(null)" : op->name);
        stringbuffer_append_string(sb, "\nend\n");
    }
}

/**
 * Dump an object, complete with its inventory.
 *
 * @param op
 * Object to dump.
 * @param sb
 * Buffer that will contain object information.
 */
void
object_dump_rec (const object *op, StringBuffer *sb)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(sb != NULL);

    /* Get the difference from the object's archetype. */
    archetype_t *at = op->arch;
    if (at == NULL) {
        /* No archetype, use empty archetype. */
        at = arches[ARCH_EMPTY_ARCHETYPE];
    }

    stringbuffer_append_printf(sb, "arch %s\n", at->name);
    get_ob_diff(sb, op, &at->clone);

    /* Recursively dump the inventory. */
    for (object *tmp = op->inv; tmp != NULL; tmp = tmp->below) {
        object_dump_rec(tmp, sb);
    }

    stringbuffer_append_string(sb, "end\n");
}

/**
 * Clear pointer to owner of an object, including ownercount.
 *
 * @param op
 * The object to clear the owner for.
 */
void
object_owner_clear (object *op)
{
    HARD_ASSERT(op != NULL);
    op->owner = NULL;
    op->ownercount = 0;
}

/**
 * Sets the owner of the object 'op' to the 'owner' object.
 *
 * @param op
 * The object to set the owner for.
 * @param owner
 * The owner.
 * @see object_owner()
 */
static void
object_owner_set_internal (object *op, object *owner)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(owner != NULL);

    while (owner->owner != NULL &&
           owner != owner->owner &&
           owner->ownercount == owner->owner->count) {
        owner = owner->owner;
    }

    /* If the owner still has an owner, we did not resolve to a final owner,
     * so lets not add to that. */
    if (owner->owner != NULL) {
        return;
    }

    op->owner = owner;
    op->ownercount = owner->count;
}

/**
 * Sets the owner and sets the chosen skill pointer owner's current skill.
 *
 * @param op
 * The object.
 * @param owner
 * The owner.
 */
void
object_owner_set (object *op, object *owner)
{
    HARD_ASSERT(op != NULL);

    if (unlikely(owner == NULL)) {
        log_error("Called with NULL owner, object: %s", object_get_str(op));
        return;
    }

    /* Ensure we have a head. */
    owner = HEAD(owner);
    object_owner_set_internal(op, owner);

    if (owner->type == PLAYER) {
        op->chosen_skill = owner->chosen_skill;
    }
}

/**
 * Copies owner from a source object to the specified object.
 *
 * Chosen skill object is set to that of the source object (typically the
 * skill that was currently chosen at the time when the source object's
 * owner was set and not the owner's current skill object).
 *
 * Use this function if player created an object (e.g. fire bullet, swarm
 * spell), and this object creates further objects whose kills should be
 * accounted for the player's original skill, even if player has changed
 * skills in the meanwhile.
 *
 * @param op
 * The object.
 * @param src
 * The source object to copy the owner from.
 */
void
object_owner_copy (object *op, object *src)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(src != NULL);

    object *owner = object_owner(src);
    if (owner == NULL) {
        /* Players don't have owners - they own themselves. Update
         * as appropriate. */
        if (src->type == PLAYER) {
            owner = src;
        } else {
            return;
        }
    }

    object_owner_set_internal(op, owner);
    op->chosen_skill = src->chosen_skill;
}

/**
 * Returns the object which this object marks as being the owner.
 *
 * An ID scheme is used to avoid pointing to objects which have been
 * freed and are now reused. If this is detected, the owner is set to
 * NULL, and NULL is returned.
 *
 * @param op
 * The object to get owner for.
 * @return
 * Owner of the object if any, NULL if no owner.
 */
object *
object_owner (object *op)
{
    HARD_ASSERT(op != NULL);

    if (op->owner == NULL) {
        return NULL;
    }

    if (OBJECT_FREE(op) || op->owner->count != op->ownercount) {
        op->owner = NULL;
        return NULL;
    }

    return op->owner;
}

/**
 * Copy object first frees everything allocated by the second object,
 * and then copies the contents of the first object into the second
 * object, allocating what needs to be allocated.
 *
 * @param op
 * Object to copy to.
 * @param src
 * Object to copy from.
 * @param no_speed
 * If set, do not touch the active list.
 */
void
object_copy (object *op, const object *src, bool no_speed)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(src != NULL);

    bool is_removed = QUERY_FLAG(op, FLAG_REMOVED);

    FREE_ONLY_HASH(op->name);
    FREE_ONLY_HASH(op->title);
    FREE_ONLY_HASH(op->race);
    FREE_ONLY_HASH(op->slaying);
    FREE_ONLY_HASH(op->msg);
    FREE_ONLY_HASH(op->artifact);
    FREE_ONLY_HASH(op->custom_name);
    FREE_ONLY_HASH(op->glow);

    object_free_key_values(op);

    memcpy((char *) op + offsetof(object, name),
           (const char *) src + offsetof(object, name),
           sizeof(object) - offsetof(object, name));

    if (is_removed) {
        SET_FLAG(op, FLAG_REMOVED);
    }

    ADD_REF_NOT_NULL_HASH(op->name);
    ADD_REF_NOT_NULL_HASH(op->title);
    ADD_REF_NOT_NULL_HASH(op->race);
    ADD_REF_NOT_NULL_HASH(op->slaying);
    ADD_REF_NOT_NULL_HASH(op->msg);
    ADD_REF_NOT_NULL_HASH(op->artifact);
    ADD_REF_NOT_NULL_HASH(op->custom_name);
    ADD_REF_NOT_NULL_HASH(op->glow);

    /* Only alter speed_left when we are sure that we have not done it before */
    if (!no_speed && op->speed < 0.0 &&
        DBL_EQUAL(op->speed_left, op->arch->clone.speed_left)) {
        op->speed_left += rndm(0, 90) / 100.0f;
    }

    /* Copy over key_values, if any. */
    if (src->key_values != NULL) {
        op->key_values = NULL;

        for (key_value_t *link = src->key_values, *tail = NULL;
             link != NULL;
             link = link->next) {
            key_value_t *new_link = emalloc(sizeof(*new_link));

            new_link->next = NULL;
            new_link->key = add_refcount(link->key);

            if (link->value != NULL) {
                new_link->value = add_refcount(link->value);
            } else {
                new_link->value = NULL;
            }

            /* Link it up. */
            if (op->key_values == NULL) {
                op->key_values = new_link;
                tail = new_link;
            } else {
                tail->next = new_link;
                tail = new_link;
            }
        }
    }

    if (!no_speed) {
        object_update_speed(op);
    }
}

/**
 * Completely copy an object, duplicating the inventory too.
 *
 * @param op
 * Where to copy.
 * @param src
 * Object to copy.
 */
void
object_copy_full (object *op, const object *src)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(src != NULL);

    object_copy(op, src, false);

    for (object *tmp = src->inv; tmp != NULL; tmp = tmp->below) {
        object *clone = object_get();
        object_copy_full(clone, tmp);
        object_insert_into(clone, op, 0);
    }
}

/**
 * Grabs an object from the list of unused objects, makes sure it is
 * initialized, and returns it.
 *
 * If there are no free objects, expand_objects() is called to get more.
 * @return
 * The new object.
 */
object *
object_get (void)
{

    object *new_obj = mempool_get(pool_object);
    SET_FLAG(new_obj, FLAG_REMOVED);

    static New_Face *blank_face = NULL;
    if (blank_face == NULL) {
        blank_face = &new_faces[find_face(BLANK_FACE_NAME, 0)];
    }
    new_obj->face = blank_face;

    static tag_t count = 0;
    /* Give the object a new (unique) count tag. */
    new_obj->count = ++count;

    return new_obj;
}

/**
 * If an object with the FLAG_IS_TURNABLE flag needs to be updated due to
 * a direction change, this function can be called to update the face
 * variable.
 *
 * @param op
 * The object to update.
 */
void
object_update_turnable (object *op)
{
    HARD_ASSERT(op != NULL);

    if (!QUERY_FLAG(op, FLAG_IS_TURNABLE)) {
        return;
    }

    SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction);
    object_update(op, UP_OBJ_FACE);
}

/**
 * Updates the speed of an object. If the speed changes from 0 to another
 * value, or vice versa, then add/remove the object from the active list.
 *
 * This function needs to be called whenever the speed of an object changes.
 *
 * @param op
 * The object.
 */
void
object_update_speed (object *op)
{
    HARD_ASSERT(op != NULL);

    if (OBJECT_FREE(op) && DBL_EQUAL(op->speed, 0.0)) {
        LOG(ERROR, "Object %s is freed but has speed.", object_get_str(op));
        op->speed = 0.0;
    }

    /* No reason putting the archetypes objects on the speed list,
     * since they never really need to be updated. */
    if (arch_in_init) {
        return;
    }

    /* These are special case objects - they have speed set, but should not be
     * put on the active list. */
    if (op->type == SPAWN_POINT_MOB) {
        return;
    }

    if (FABS(op->speed) > MIN_ACTIVE_SPEED) {
        /* If already on active list, don't do anything */
        if (op->active_next || op->active_prev || op == active_objects) {
            return;
        }

        /* process_events() expects us to insert the object at the beginning
         * of the list. */
        op->active_next = active_objects;

        if (op->active_next != NULL) {
            op->active_next->active_prev = op;
        }

        active_objects = op;
        op->active_prev = NULL;
    } else {
        /* If not on the active list, nothing needs to be done. */
        if (op->active_next == NULL &&
            op->active_prev == NULL &&
            op != active_objects) {
            return;
        }

        if (op->active_prev == NULL) {
            active_objects = op->active_next;

            if (op->active_next != NULL) {
                op->active_next->active_prev = NULL;
            }
        } else {
            op->active_prev->active_next = op->active_next;

            if (op->active_next != NULL) {
                op->active_next->active_prev = op->active_prev;
            }
        }

        op->active_next = NULL;
        op->active_prev = NULL;
    }
}

/**
 * Updates the various map square flags and values depending on 'action'.
 *
 * @param op
 * Object to update.
 * @param action
 * Hint of what the caller believes need to be done. One of
 * @ref UP_OBJ_xxx values.
 */
void
object_update (object *op, int action)
{
    HARD_ASSERT(op != NULL);

    if (op->env != NULL ||
        op->map == NULL ||
        op->map->in_memory == MAP_SAVING) {
        return;
    }

    /* No need to change anything except the map update counter. */
    if (action == UP_OBJ_FACE) {
        INC_MAP_UPDATE_COUNTER(op->map, op->x, op->y);
        return;
    }

    MapSpace *msp = GET_MAP_SPACE_PTR(op->map, op->x, op->y);
    int newflags = msp->flags;
    int flags = newflags;

    if (action == UP_OBJ_INSERT) {
        msp->update_tile++;

        if (op->glow_radius != 0) {
            adjust_light_source(op->map, op->x, op->y, op->glow_radius);
        }

        if (QUERY_FLAG(op, FLAG_NO_PASS) ||
            QUERY_FLAG(op, FLAG_PASS_THRU) ||
            QUERY_FLAG(op, FLAG_DOOR_CLOSED)) {
            newflags |= P_NEED_UPDATE;
        } else if (QUERY_FLAG(op, FLAG_IS_FLOOR)) {
            /* Floors define our node - force an update. */
            newflags |= P_NEED_UPDATE;
            msp->light_value += op->last_sp;
        } else {
            if (op->type == CHECK_INV) {
                newflags |= P_CHECK_INV;
            }

            if (QUERY_FLAG(op, FLAG_MONSTER)) {
                newflags |= P_IS_MONSTER;
            }

            if (QUERY_FLAG(op, FLAG_IS_PLAYER)) {
                newflags |= P_IS_PLAYER;
            }

            if (QUERY_FLAG(op, FLAG_PLAYER_ONLY)) {
                newflags |= P_PLAYER_ONLY;
            }

            if (QUERY_FLAG(op, FLAG_BLOCKSVIEW)) {
                newflags |= P_BLOCKSVIEW;
            }

            if (QUERY_FLAG(op, FLAG_NO_MAGIC)) {
                newflags |= P_NO_MAGIC;
            }

            if (QUERY_FLAG(op, FLAG_WALK_ON)) {
                newflags |= P_WALK_ON;
            }

            if (QUERY_FLAG(op, FLAG_FLY_ON)) {
                newflags |= P_FLY_ON;
            }

            if (QUERY_FLAG(op, FLAG_WALK_OFF)) {
                newflags |= P_WALK_OFF;
            }

            if (QUERY_FLAG(op, FLAG_FLY_OFF)) {
                newflags |= P_FLY_OFF;
            }

            if (QUERY_FLAG(op, FLAG_DOOR_CLOSED)) {
                newflags |= P_DOOR_CLOSED;
            }

            if (QUERY_FLAG(op, FLAG_NO_PVP)) {
                newflags |= P_NO_PVP;
            }

            if (op->type == MAGIC_MIRROR) {
                newflags |= P_MAGIC_MIRROR;
            }

            if (op->type == EXIT) {
                newflags |= P_IS_EXIT;
            }

            if (QUERY_FLAG(op, FLAG_OUTDOOR)) {
                newflags |= P_OUTDOOR;
            }
        }
    } else if (action == UP_OBJ_REMOVE) {
        msp->update_tile++;

        if (op->glow_radius != 0) {
            adjust_light_source(op->map, op->x, op->y, -op->glow_radius);
        }

        /* We must rebuild the flags when one of these flags is touched from our
         * object */
        if (QUERY_FLAG(op, FLAG_MONSTER) ||
            QUERY_FLAG(op, FLAG_IS_PLAYER) ||
            QUERY_FLAG(op, FLAG_BLOCKSVIEW) ||
            QUERY_FLAG(op, FLAG_DOOR_CLOSED) ||
            QUERY_FLAG(op, FLAG_PASS_THRU) ||
            QUERY_FLAG(op, FLAG_NO_PASS) ||
            QUERY_FLAG(op, FLAG_PLAYER_ONLY) ||
            QUERY_FLAG(op, FLAG_NO_MAGIC) ||
            QUERY_FLAG(op, FLAG_WALK_ON) ||
            QUERY_FLAG(op, FLAG_FLY_ON) ||
            QUERY_FLAG(op, FLAG_WALK_OFF) ||
            QUERY_FLAG(op, FLAG_FLY_OFF) ||
            QUERY_FLAG(op, FLAG_IS_FLOOR) ||
            QUERY_FLAG(op, FLAG_OUTDOOR) ||
            QUERY_FLAG(op, FLAG_NO_PVP) ||
            op->type == CHECK_INV ||
            op->type == MAGIC_MIRROR ||
            op->type == EXIT) {
            newflags |= P_NEED_UPDATE;
        }
    } else if (action == UP_OBJ_FLAGS) {
        /* Force flags rebuild but no tile counter. */
        newflags |= P_NEED_UPDATE;
    } else if (action == UP_OBJ_FLAGFACE) {
        /* Force flags rebuild */
        newflags |= P_NEED_UPDATE;
        msp->update_tile++;
    } else if (action == UP_OBJ_ALL) {
        /* Force full tile update */
        newflags |= P_NEED_UPDATE;
        msp->update_tile++;
    } else {
        return;
    }

    if (flags != newflags) {
        /* Rebuild flags */
        if (newflags & P_NEED_UPDATE) {
            msp->flags |= newflags;
            update_position(op->map, op->x, op->y);
        } else {
            msp->flags |= newflags;
        }
    }

    if (op->more != NULL && action != UP_OBJ_INSERT) {
        object_update(op->more, action);
    }
}

/**
 * Drops the inventory of the specified object into its current environment.
 *
 * Makes some decisions whether to actually drop or not, and/or to
 * create a corpse for the stuff.
 *
 * @param op
 * The object to drop the inventory for.
 */
void
object_drop_inventory (object *op)
{
    HARD_ASSERT(op != NULL);

    if (op->type == PLAYER) {
        return;
    }

    if (op->env == NULL &&
        (op->map == NULL || op->map->in_memory != MAP_IN_MEMORY)) {
        return;
    }

    object *enemy;
    if (op->enemy != NULL && op->enemy->type == PLAYER) {
        enemy = op->enemy;
    } else {
        enemy = object_owner(op->enemy);
    }

    object *corpse = NULL;
    /* Create race corpse and/or drop stuff to floor. */
    if ((QUERY_FLAG(op, FLAG_CORPSE) && !QUERY_FLAG(op, FLAG_STARTEQUIP)) ||
        QUERY_FLAG(op, FLAG_CORPSE_FORCED)) {
        ob_race *race_corpse = race_find(op->race);
        if (race_corpse != NULL) {
            corpse = arch_to_object(race_corpse->corpse);
            corpse->x = op->x;
            corpse->y = op->y;
            corpse->map = op->map;
            corpse->weight = op->weight;
        }
    }

    FOR_INV_PREPARE(op, tmp) {
        object_remove(tmp, 0);

        if (tmp->type == QUEST_CONTAINER) {
            if (enemy != NULL && enemy->type == PLAYER &&
                enemy->count == op->enemy_count) {
                quest_handle(enemy, tmp);
            }

            object_destroy(tmp);
            continue;
        }

        if ((QUERY_FLAG(op, FLAG_STARTEQUIP) &&
             !(tmp->type == ARROW && tmp->attacked_by_count != 0)) ||
            (tmp->type != RUNE && (QUERY_FLAG(tmp, FLAG_SYS_OBJECT) ||
                                   QUERY_FLAG(tmp, FLAG_STARTEQUIP) ||
                                   QUERY_FLAG(tmp, FLAG_NO_DROP)))) {
            object_destroy(tmp);
            continue;
        }

        tmp->x = op->x;
        tmp->y = op->y;

        /* Always clear these in case the monster used the item */
        CLEAR_FLAG(tmp, FLAG_APPLIED);
        CLEAR_FLAG(tmp, FLAG_BEEN_APPLIED);

        /* If we have a corpse put the item in it. */
        if (corpse != NULL &&
            !(tmp->type == ARROW && tmp->attacked_by_count != 0 &&
              enemy != NULL && OBJECT_VALID(tmp->attacked_by,
                                            tmp->attacked_by_count) &&
              tmp->attacked_by_count != enemy->count &&
              !(tmp->attacked_by->type == PLAYER &&
                enemy->type == PLAYER &&
                CONTR(tmp->attacked_by)->party != NULL &&
                CONTR(tmp->attacked_by)->party == CONTR(enemy)->party))) {
            object_insert_into(tmp, corpse, 0);
        } else if (tmp->type != RUNE) {
            if (op->env != NULL) {
                object_insert_into(tmp, op->env, 0);
            } else {
                object_insert_map(tmp, op->map, NULL, 0);
            }
        } else {
            object_destroy(tmp);
        }
    } FOR_INV_FINISH();

    /* Drop the corpse. */
    if (corpse != NULL) {
        if (enemy != NULL && enemy->type == PLAYER) {
            if (enemy->count == op->enemy_count) {
                FREE_AND_ADD_REF_HASH(corpse->slaying, enemy->name);
            }
        } else if (QUERY_FLAG(op, FLAG_CORPSE_FORCED)) {
            corpse->stats.food = 5;
        }

        /* Change sub_type to mark this corpse. */
        if (corpse->slaying != NULL) {
            if (CONTR(enemy)->party != NULL &&
                CONTR(enemy)->party->loot != PARTY_LOOT_OWNER) {
                FREE_AND_ADD_REF_HASH(corpse->slaying,
                                      CONTR(enemy)->party->name);
                corpse->sub_type = ST1_CONTAINER_CORPSE_party;
            } else {
                corpse->sub_type = ST1_CONTAINER_CORPSE_player;
            }
        }

        /* Store the original food value. */
        corpse->last_eat = corpse->stats.food;
        corpse->sub_layer = op->sub_layer;

        if (op->env != NULL) {
            corpse = object_insert_into(corpse, op->env, 0);
        } else {
            corpse = object_insert_map(corpse, op->map, NULL, 0);
        }

        SOFT_ASSERT(corpse != NULL,
                    "Failed to insert corpse for %s",
                    object_get_str(op));
        object_reverse_inventory(corpse);
    }
}

/**
 * Destroy (free) inventory of an object. Used internally by object_destroy()
 * to recursively free the object's inventory.
 *
 * @param op
 * Object to free the inventory of.
 */
void
object_destroy_inv (object *op)
{
    HARD_ASSERT(op != NULL);

    SET_FLAG(op, FLAG_NO_FIX_PLAYER);

    FOR_INV_PREPARE(op, tmp) {
        if (tmp->inv != NULL) {
            object_destroy_inv(tmp);
        }

        object_remove(tmp, 0);
        object_destroy(tmp);
    } FOR_INV_FINISH();

    CLEAR_FLAG(op, FLAG_NO_FIX_PLAYER);
}

/**
 * Cleanups and frees everything allocated by an object and gives the
 * memory back to the object mempool.
 *
 * @note The object must have been removed by object_remove() first.
 * @param op
 * The object to destroy (free).
 */
void
object_destroy (object *op)
{
    HARD_ASSERT(op != NULL);

    if (!QUERY_FLAG(op, FLAG_REMOVED)) {
        char buf[HUGE_BUF];

        object_debugger(op, VS(buf));
        LOG(ERROR, "Freeing an object that was not removed: %s", buf);
        return;
    }

    if (op->more != NULL) {
        object_destroy(op->more);
    }

    object_free_key_values(op);

    if (QUERY_FLAG(op, FLAG_IS_LINKED)) {
        connection_object_remove(op);
    }

    /* Remove and free the inventory. */
    object_destroy_inv(op);

    /* Remove object from the active list. */
    op->speed = 0.0;
    object_update_speed(op);

    if (op->head == NULL) {
        object_cb_deinit(op);
    }

    /* Free attached attrsets */
    if (op->custom_attrset) {
        switch (op->type) {
        case PLAYER:
            /* Players are changed into DEAD_OBJECTs when they logout */
        case DEAD_OBJECT:
            mempool_return(pool_player, op->custom_attrset);
            break;

        case MONSTER:
            monster_data_deinit(op);
            break;

        default:
            LOG(ERROR,
                "Custom attrset found in unsupported object %s "
                "(type %d)", object_get_str(op), op->type);
        }

        op->custom_attrset = NULL;
    }

    FREE_AND_CLEAR_HASH2(op->name);
    FREE_AND_CLEAR_HASH2(op->title);
    FREE_AND_CLEAR_HASH2(op->race);
    FREE_AND_CLEAR_HASH2(op->slaying);
    FREE_AND_CLEAR_HASH2(op->msg);
    FREE_AND_CLEAR_HASH2(op->artifact);
    FREE_AND_CLEAR_HASH2(op->custom_name);
    FREE_AND_CLEAR_HASH2(op->glow);

    /* Mark object as freed and invalidate all references to it. */
    op->count = 0;

    /* Return the memory to the mempool. */
    mempool_return(pool_object, op);
}

/**
 * Drop op's inventory on the floor and remove op from the map.
 *
 * Used mainly for physical destruction of normal objects and monsters.
 *
 * @param op
 * Object to destruct.
 */
void
object_destruct (object *op)
{
    SET_FLAG(op, FLAG_NO_FIX_PLAYER);

    if (op->inv != NULL) {
        object_drop_inventory(op);
    }

    object_remove(op, 0);
    object_destroy(op);
}

/**
 * Checks if any objects has a movement type that matches objects that affect
 * this object on this space. Calls object_move_on() to process these events.
 *
 * @param op
 * Object that may trigger something.
 * @param originator
 * Player, monster or other object that caused 'op' to trigger the event.
 * @param state
 * 1 for move on events, 0 for move off events.
 * @return
 * True if 'op' was destroyed, false otherwise.
 */
static int
object_check_move_on (object *op, object *originator, int state)
{
    HARD_ASSERT(op != NULL);

    if (QUERY_FLAG(op, FLAG_NO_APPLY)) {
        return false;
    }

    MapSpace *msp = GET_MAP_SPACE_PTR(op->map, op->x, op->y);
    /* No event on this tile. */
    if (!(msp->flags &
          (state == 1 ? (P_WALK_ON | P_FLY_ON) : (P_WALK_OFF | P_FLY_OFF)))) {
        return false;
    }

    mapstruct *m = op->map;
    int x = op->x;
    int y = op->y;

    OBJECTS_DESTROYED_BEGIN(op) {
        FOR_MAP_PREPARE(op->map, op->x, op->y, tmp) {
            if (tmp == op) {
                continue;
            }

            if (state == 1 && IS_LIVE(op) &&
                (op->type != PLAYER || !CONTR(op)->tcl) &&
                QUERY_FLAG(tmp, FLAG_SLOW_MOVE) &&
                (tmp->terrain_flag == 0 ||
                 tmp->terrain_flag & op->terrain_flag)) {
                op->speed_left -= SLOW_PENALTY(tmp) * FABS(op->speed);
            }

            int flag;
            if (QUERY_FLAG(op, FLAG_FLYING)) {
                flag = state == 1 ? FLAG_FLY_ON : FLAG_FLY_OFF;
            } else {
                flag = state == 1 ? FLAG_WALK_ON : FLAG_WALK_OFF;
            }

            if (!QUERY_FLAG(tmp, flag)) {
                continue;
            }

            object_move_on(tmp, op, originator, state);

            if (OBJECTS_DESTROYED(op)) {
                return true;
            }

            if (op->map != m || op->x != x || op->y != y) {
                return false;
            }
        } FOR_MAP_FINISH();
    } OBJECTS_DESTROYED_END();

    return false;
}

/**
 * This function removes the object op from the linked list of objects
 * which it is currently tied to. When this function is done, the
 * object will have no environment. If the object previously had an
 * environment, the map pointer and x/y coordinates will be updated to
 * the previous environment.
 *
 * @note If you want to remove a lot of items in player's inventory,
 * set FLAG_NO_FIX_PLAYER on the player first and then explicitly call
 * living_update() on the player.
 *
 * @param op
 * Object to remove.
 * @param flags
 * Combination of @ref REMOVAL_xxx.
 */
void
object_remove (object *op, int flags)
{
    HARD_ASSERT(op != NULL);

    if (QUERY_FLAG(op, FLAG_REMOVED)) {
        log_error("Tried to remove an already removed object %s.",
                  object_get_str(op));
        return;
    }

    if (op->more != NULL) {
        object_remove(op->more, flags);
    }

    SET_FLAG(op, FLAG_REMOVED);
    SET_FLAG(op, FLAG_OBJECT_WAS_MOVED);
    op->quickslot = 0;

    /* In this case, the object to be removed is in someone's inventory. */
    if (op->env != NULL) {
        if (!QUERY_FLAG(op, FLAG_SYS_OBJECT) && !(flags & REMOVE_NO_WEIGHT)) {
            object_weight_sub(op->env, WEIGHT_NROF(op, op->nrof));
        }

        object *env = object_get_env(op);

        if (op->above != NULL) {
            op->above->below = op->below;
        } else {
            op->env->inv = op->below;
        }

        if (op->below != NULL) {
            op->below->above = op->above;
        }

        /* We set up values so that it could be inserted into the map,
         * but we don't actually do that - it is up to the caller to
         * decide what we want to do. */
        op->x = op->env->x;
        op->y = op->env->y;
        op->map = op->env->map;

        esrv_del_item(op);
        object_cb_remove_inv(op);

        op->above = NULL;
        op->below = NULL;
        op->env = NULL;

        if (env != op && IS_LIVE(env) && env->map != NULL) {
            living_update(env);
        }
    } else if (op->map != NULL) {
        /* If this is the base layer object, we assign the next object
         * to be it if it is from same layer and sub-layer. */
        MapSpace *msp = GET_MAP_SPACE_PTR(op->map, op->x, op->y);

        if (op->layer != 0 &&
            GET_MAP_SPACE_LAYER(msp, op->layer, op->sub_layer) == op) {
            if (op->above != NULL &&
                op->above->layer == op->layer &&
                op->above->sub_layer == op->sub_layer) {
                SET_MAP_SPACE_LAYER(msp, op->layer, op->sub_layer, op->above);
            } else {
                SET_MAP_SPACE_LAYER(msp, op->layer, op->sub_layer, NULL);
            }
        }

        /* Link the object above us. */
        if (op->above != NULL) {
            op->above->below = op->below;
        } else {
            /* Assign below as last one. */
            SET_MAP_SPACE_LAST(msp, op->below);
        }

        /* Relink the object below us, if there is one. */
        if (op->below != NULL) {
            op->below->above = op->above;
        } else {
            /* First object goes on above it. */
            SET_MAP_SPACE_FIRST(msp, op->above);
        }

        op->above = NULL;
        op->below = NULL;
        op->env = NULL;

        if (op->map->in_memory != MAP_SAVING) {
            msp->update_tile++;
            object_update(op, UP_OBJ_REMOVE);
        }

        object_cb_remove_map(op);

        if (!(flags & REMOVE_NO_WALK_OFF)) {
            object_check_move_on(op, NULL, 0);
        }
    }
}

/**
 * This function inserts the object in the two-way linked list which
 * represents what is on a map.
 *
 * @param op
 * Object to insert.
 * @param m
 * Map to insert into.
 * @param originator
 * What caused op to be inserted.
 * @param flag
 * Combination of @ref INS_xxx "INS_xxx" values.
 * @return
 * NULL if 'op' was destroyed, 'op' otherwise.
 */
object *
object_insert_map (object *op, mapstruct *m, object *originator, int flag)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(m != NULL);

    if (OBJECT_FREE(op)) {
        log_error("Attempted to insert freed object: %s",
                  object_get_str(op));
        return NULL;
    }

    if (!QUERY_FLAG(op, FLAG_REMOVED)) {
        log_error("Attempted to insert non-removed object: %s",
                  object_get_str(op));
        return op;
    }

    if (op->head == NULL && op->arch->more != NULL && op->more == NULL) {
        object *prev = op;
        for (archetype_t *at = op->arch->more; at != NULL; at = at->more) {
            object *tail = arch_to_object(at);

            tail->type = op->type;
            tail->layer = op->layer;
            tail->sub_layer = op->sub_layer;

            tail->head = op;
            prev->more = tail;

            prev = tail;
        }

        SET_OR_CLEAR_MULTI_FLAG(op, FLAG_SYS_OBJECT);
        SET_OR_CLEAR_MULTI_FLAG(op, FLAG_NO_APPLY);
        SET_OR_CLEAR_MULTI_FLAG(op, FLAG_IS_INVISIBLE);
        SET_OR_CLEAR_MULTI_FLAG(op, FLAG_IS_ETHEREAL);
        SET_OR_CLEAR_MULTI_FLAG(op, FLAG_CAN_PASS_THRU);
        SET_OR_CLEAR_MULTI_FLAG(op, FLAG_FLYING);
        SET_OR_CLEAR_MULTI_FLAG(op, FLAG_BLOCKSVIEW);

        SET_OR_CLEAR_MULTI_FLAG_IF_CLONE(op, FLAG_NO_PASS);
        SET_OR_CLEAR_MULTI_FLAG_IF_CLONE(op, FLAG_BLOCKSVIEW);
    }

    /* Attempt to fall down through empty map squares onto maps below, but
     * only if the object is not a player or the player doesn't have collision
     * disabled. */
    int fall_floors = 0;
    if (flag & INS_FALL_THROUGH && (op->type != PLAYER || !CONTR(op)->tcl)) {
        mapstruct *tiled;
        bool found_floor;
        int sub_layer;
        for (tiled = m;
             tiled != NULL;
             tiled = get_map_from_tiled(tiled, TILED_DOWN)) {
            object *floor = GET_MAP_OB_LAYER(tiled,
                                             op->x,
                                             op->y,
                                             LAYER_FLOOR,
                                             op->sub_layer);
            int z = floor != NULL ? floor->z : 0;
            int z_highest = 0;
            sub_layer = -1;
            found_floor = false;

            if (tiled != m) {
                fall_floors++;
            }

            object *floor_tmp;
            FOR_MAP_LAYER_BEGIN(tiled,
                                op->x,
                                op->y,
                                LAYER_FLOOR,
                                -1,
                                floor_tmp) {
                found_floor = true;

                if (tiled == m) {
                    continue;
                }

                if (floor_tmp->z - z > MOVE_MAX_HEIGHT_DIFF) {
                    continue;
                }

                if (floor_tmp->z > z_highest) {
                    z_highest = floor_tmp->z;
                    sub_layer = floor_tmp->sub_layer;
                }
            } FOR_MAP_LAYER_END

            if (found_floor || QUERY_FLAG(op, FLAG_FLYING)) {
                break;
            }
        }

        if (found_floor) {
            if (fall_floors != 0 && object_blocked(op, tiled, op->x, op->y)) {
                int i = map_free_spot_first(tiled, op->x, op->y, op->arch, op);
                if (i != -1) {
                    op->x += freearr_x[i];
                    op->y += freearr_y[i];
                }
            }

            if (sub_layer != -1) {
                op->sub_layer = sub_layer;
            }

            m = tiled;
        }

        flag &= ~INS_FALL_THROUGH;
    }

    if (op->more != NULL) {
        op->more->x = HEAD(op)->x + op->more->arch->clone.x;
        op->more->y = HEAD(op)->y + op->more->arch->clone.y;
        op->more->sub_layer = HEAD(op)->sub_layer;

        if (object_insert_map(op->more, m, originator, flag) == NULL) {
            return NULL;
        }
    }

    CLEAR_FLAG(op, FLAG_REMOVED);

    int x = op->x;
    int y = op->y;
    m = get_map_from_coord(m, &x, &y);
    if (m == NULL) {
        return NULL;
    }

    op->x = x;
    op->y = y;
    op->map = m;

    /* Merge objects if possible. */
    if (op->nrof != 0 && !(flag & INS_NO_MERGE)) {
        for (object *tmp = GET_MAP_OB(m, x, y); tmp != NULL; tmp = tmp->above) {
            if (object_can_merge(op, tmp)) {
                op->nrof += tmp->nrof;
                object_remove(tmp, 0);
                object_destroy(tmp);
                break;
            }
        }
    }

    SET_FLAG(op, FLAG_OBJECT_WAS_MOVED);
    CLEAR_FLAG(op, FLAG_APPLIED);
    CLEAR_FLAG(op, FLAG_INV_LOCKED);

    MapSpace *msp = GET_MAP_SPACE_PTR(op->map, op->x, op->y);

    if (op->layer != 0) {
        object *top = GET_MAP_SPACE_LAYER(msp, op->layer, op->sub_layer);
        if (top == NULL) {
            for (int layer = op->layer;
                 layer <= NUM_LAYERS && top == NULL;
                 layer++) {
                for (int sub_layer = op->sub_layer;
                     sub_layer < NUM_SUB_LAYERS && top == NULL;
                     sub_layer++) {
                    top = GET_MAP_SPACE_LAYER(msp, layer, sub_layer);
                }
            }
        }

        SET_MAP_SPACE_LAYER(msp, op->layer, op->sub_layer, op);

        if (top != NULL) {
            if (top->below != NULL) {
                top->below->above = op;
            } else {
                SET_MAP_SPACE_FIRST(msp, op);
            }

            op->below = top->below;
            top->below = op;
            op->above = top;
        } else {
            top = GET_MAP_SPACE_LAST(msp);
            if (top != NULL) {
                top->above = op;
                op->below = top;
            } else {
                SET_MAP_SPACE_FIRST(msp, op);
            }

            SET_MAP_SPACE_LAST(msp, op);
        }
    } else {
        /* Easy chaining */
        object *top = GET_MAP_SPACE_FIRST(msp);
        if (top != NULL) {
            top->below = op;
            op->above = top;
        } else {
            SET_MAP_SPACE_LAST(msp, op);
        }

        SET_MAP_SPACE_FIRST(msp, op);
    }

    /* Some object-type-specific adjustments/initialization. */
    if (op->type == PLAYER) {
        CONTR(op)->cs->update_tile = 0;
        CONTR(op)->update_los = 1;

        if (op->map->player_first != NULL) {
            CONTR(op->map->player_first)->map_below = op;
            CONTR(op)->map_above = op->map->player_first;
        }

        op->map->player_first = op;
    } else if (op->type == MAP_EVENT_OBJ) {
        map_event_obj_init(op);
    } else {
        object_cb_insert_map(op);
    }

    /* Mark this tile as changed. */
    msp->update_tile++;
    /* Update flags for this tile. */
    object_update(op, UP_OBJ_INSERT);

    /* Attempt to open doors. */
    door_try_open(op, op->map, op->x, op->y, false);

    if (!(flag & INS_NO_WALK_ON) &&
        (msp->flags & (P_WALK_ON | P_FLY_ON) || op->more != NULL) &&
        op->head == NULL) {
        for (object *tmp = op; tmp != NULL; tmp = tmp->more) {
            if (object_check_move_on(tmp, originator, 1)) {
                return NULL;
            }
        }
    }

    if (fall_floors != 0 && IS_LIVE(op)) {
        OBJECTS_DESTROYED_BEGIN(op) {
            attack_perform_fall(op, fall_floors);

            if (OBJECTS_DESTROYED(op)) {
                return NULL;
            }
        } OBJECTS_DESTROYED_END();
    }

    return op;
}

/**
 * Split a stack of objects into another object with the specified quantity.
 *
 * If 'nrof' is more or equal to the nrof of the specified object, the
 * original object is returned instead and no extra work is done.
 *
 * @param op
 * Object to split.
 * @param nrof
 * Number of items to split from the stack.
 * @return
 * Split part of the stack. Can be the original object; never NULL.
 */
object *
object_stack_get (object *op, uint32_t nrof)
{
    HARD_ASSERT(op != NULL);

    nrof = MAX(1, nrof);

    if (MAX(1, op->nrof) <= nrof) {
        return op;
    }

    op->nrof -= nrof;
    object_update(op, UP_OBJ_FACE);
    esrv_update_item(UPD_NROF, op);

    if (op->env != NULL && !QUERY_FLAG(op, FLAG_SYS_OBJECT)) {
        object_weight_sub(op->env, WEIGHT_NROF(op, nrof));
    }

    object *split = object_get();
    object_copy_full(split, op);
    split->nrof = nrof;
    return split;
}

/**
 * Like object_stack_get(), but if a new object is created due to the split,
 * it is inserted in the same environment as the original object.
 *
 * @param op
 * Object to split.
 * @param nrof
 * Number of items to split from the stack.
 * @return
 * Split part of the stack. Can be the original object; never NULL.
 */
object *
object_stack_get_reinsert (object *op, uint32_t nrof)
{
    HARD_ASSERT(op != NULL);

    object *split = object_stack_get(op, nrof);
    if (split != op) {
        if (op->map != NULL) {
            split = object_insert_map(split, op->map, NULL, INS_NO_MERGE);
        } else if (op->env != NULL) {
            split = object_insert_into(split, op->env, INS_NO_MERGE);
        }

        if (split == NULL) {
            return op;
        }
    }

    return split;
}

/**
 * Like object_stack_get(), but if the original object is returned (no new
 * stack is created), it is also removed from its environment.
 *
 * @param op
 * Object to split.
 * @param nrof
 * Number of items to split from the stack.
 * @return
 * Split part of the stack. Can be the original object; never NULL.
 */
object *
object_stack_get_removed (object *op, uint32_t nrof)
{
    HARD_ASSERT(op != NULL);

    object *split = object_stack_get(op, nrof);
    if (split == op) {
        object_remove(split, 0);
    }

    return split;
}

/**
 * Decreases a specified number from the amount of an object. If the amount
 * reaches 0, the object is subsequently removed and freed.
 *
 * This function will send an update to client if op is in a player
 * inventory.
 *
 * @param op
 * Object to decrease.
 * @param nrof
 * Number to remove.
 * @return
 * 'op' if something is left, NULL if the amount reached 0.
 */
object *
object_decrease (object *op, uint32_t nrof)
{
    HARD_ASSERT(op != NULL);

    if (nrof == 0) {
        return op;
    }

    if (nrof > op->nrof) {
        nrof = op->nrof;
    }

    if (QUERY_FLAG(op, FLAG_REMOVED)) {
        op->nrof -= nrof;
    } else {
        if (nrof < op->nrof) {
            op->nrof -= nrof;

            if (op->env != NULL && !QUERY_FLAG(op, FLAG_SYS_OBJECT)) {
                object_weight_sub(op->env, op->weight * nrof);
            }
        } else {
            object_remove(op, 0);
            op->nrof = 0;
        }
    }

    if (op->nrof != 0) {
        object_update(op, UP_OBJ_FACE);
        esrv_update_item(UPD_NROF, op);
        return op;
    }

    object_destroy(op);
    return NULL;
}

/**
 * This function inserts the object op in the linked list inside the
 * object environment.
 *
 * @param op
 * Object to insert. Must be removed. May become invalid after return,
 * so use return value of the function.
 * @param where
 * Object to insert into.
 * @param flag
 * Combination of @ref INS_xxx "INS_xxx" values.
 * @return
 * Pointer to inserted item, which will be different than op if object
 * was merged.
 */
object *
object_insert_into (object *op, object *where, int flag)
{
    HARD_ASSERT(op != NULL);
    SOFT_ASSERT_RC(where != NULL,
                   op,
                   "Attempting to insert %s into nothing.",
                   object_get_str(op));
    SOFT_ASSERT_RC(QUERY_FLAG(op, FLAG_REMOVED),
                   op,
                   "Attempting to insert non-removed object %s into %s",
                   object_get_str(op),
                   object_get_str(where));

    where = HEAD(where);
    op = HEAD(op);

    /* If the object has tail parts, it means the object is a multi-part
     * object that was on a map prior to this insert call. Thus, we will
     * want to destroy the tail parts of this object, so if the object
     * is at some later point inserted on the map again, the tails will
     * be re-created. */
    if (op->more != NULL) {
        for (object *tmp = op->more, *next; tmp != NULL; tmp = next) {
            next = tmp->more;
            object_destroy(tmp);
        }

        op->more = NULL;
    }

    CLEAR_FLAG(op, FLAG_REMOVED);

    if (!QUERY_FLAG(op, FLAG_SYS_OBJECT)) {
        if (!(flag & INS_NO_MERGE)) {
            for (object *tmp = where->inv; tmp != NULL; tmp = tmp->below) {
                if (!QUERY_FLAG(tmp, FLAG_SYS_OBJECT) &&
                    object_can_merge(tmp, op)) {
                    tmp->nrof += op->nrof;
                    esrv_update_item(UPD_NROF, tmp);
                    object_weight_add(where, op->weight * MAX(1, op->nrof));

                    SET_FLAG(op, FLAG_REMOVED);
                    object_destroy(op);

                    return tmp;
                }
            }
        }

        object_weight_add(where, WEIGHT_NROF(op, op->nrof));
    }

    SET_FLAG(op, FLAG_OBJECT_WAS_MOVED);
    op->map = NULL;
    op->env = where;
    op->above = NULL;
    op->below = NULL;
    op->x = 0;
    op->y = 0;

    if (where->inv == NULL) {
        where->inv = op;
    } else {
        op->below = where->inv;
        op->below->above = op;
        where->inv = op;
    }

    /* Check for event object and set the environment's object event flags. */
    if (op->type == EVENT_OBJECT && op->sub_type != 0) {
        where->event_flags |= (1U << (op->sub_type - 1));
    } else if (op->type == QUEST_CONTAINER && where->type == CONTAINER) {
        where->event_flags |= EVENT_FLAG(EVENT_QUEST);
    }

    /* Update living objects if inside living object. */
    object *env = object_get_env(op);
    if (env != op && IS_LIVE(env) && env->map != NULL) {
        living_update(env);
    }

    if (where->type == PLAYER || where->type == CONTAINER) {
        esrv_send_item(op);
    }

    return op;
}

/**
 * Searches for any object with a matching archetype in the inventory
 * of the given object.
 *
 * @param op
 * Where to search.
 * @param at
 * Archetype to search for.
 * @return
 * First matching object, or NULL if none matches.
 */
object *
object_find_arch (object *op, archetype_t *at)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(at != NULL);

    for (object *tmp = op->inv; tmp != NULL; tmp = tmp->below) {
        if (tmp->arch == at) {
            return tmp;
        }
    }

    return NULL;
}

/**
 * Searches for any object with a matching type variable in the
 * inventory of the given object.
 *
 * @param op
 * Object to search in.
 * @param type
 * Type to search for.
 * @return
 * First matching object, or NULL if none matches.
 */
object *
object_find_type (object *op, uint8_t type)
{
    HARD_ASSERT(op != NULL);

    for (object *tmp = op->inv; tmp != NULL; tmp = tmp->below) {
        if (tmp->type == type) {
            return tmp;
        }
    }

    return NULL;
}

/**
 * Get direction from one object to another.
 *
 * @param op
 * The first object.
 * @param target
 * The target object.
 * @param range_vector
 * Range vector pointer to use.
 * @return
 * The direction; zero if no direction can be computed.
 */
int
object_dir_to_target (object *op, object *target)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(target != NULL);

    rv_vector rv;
    if (!get_rangevector(op, target, &rv, 0)) {
        return 0;
    }

    return rv.direction;
}

/**
 * Finds out if an object can be picked up.
 *
 * @note This introduces a weight limitation for monsters.
 * @param who
 * Who is trying to pick up. Can be a monster or a player.
 * @param item
 * Item we're trying to pick up.
 * @return
 * True if it can be picked up, false otherwise.
 */
bool
object_can_pick (const object *op, const object *item)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(item != NULL);
    HARD_ASSERT(op->type == PLAYER || op->type == MONSTER);

    if (item->weight <= 0) {
        return false;
    }

    if (QUERY_FLAG(item, FLAG_NO_PICK) && !QUERY_FLAG(item, FLAG_UNPAID)) {
        return false;
    }

    if (IS_INVISIBLE(item, op) && !QUERY_FLAG(op, FLAG_SEE_INVISIBLE)) {
        return false;
    }

    if (QUERY_FLAG(item, FLAG_SOULBOUND)) {
        if (op->type != PLAYER) {
            return false;
        }

        shstr *name = object_get_value(item, "soulbound_name");
        if (name == NULL) {
            return false;
        }

        if (name != op->name) {
            return false;
        }
    }

    /* Weight limit for monsters */
    if (op->type != PLAYER && item->weight > op->weight / 3) {
        return false;
    }

    return true;
}

/**
 * Create clone from one object to another.
 *
 * @param op
 * Object to clone.
 * @return
 * Clone of op, including inventory and 'more' body parts.
 */
object *
object_clone (const object *op)
{
    HARD_ASSERT(op != NULL);

    op = HEAD(op);

    object *ret = NULL;

    object *prev = NULL;
    for (const object *part = op; part != NULL; part = part->more) {
        object *tmp = object_get();
        object_copy(tmp, part, false);
        tmp->x -= op->x;
        tmp->y -= op->y;

        if (part->head == NULL) {
            ret = tmp;
            tmp->head = NULL;
        } else {
            tmp->head = ret;
        }

        tmp->more = NULL;

        if (prev != NULL) {
            prev->more = tmp;
        }

        prev = tmp;
    }

    /* Copy inventory */
    for (object *tmp = op->inv; tmp != NULL; tmp = tmp->below) {
        object_insert_into(object_clone(tmp), ret, 0);
    }

    return ret;
}

/**
 * Creates an object using a string representing its content.
 *
 * @param str
 * String to load the object from.
 * @return
 * The newly created object, NULL on failure.
 */
object *
object_load_str (const char *str)
{
    HARD_ASSERT(str != NULL);

    object *obj = object_get();
    if (load_object(str, obj, 0) != LL_NORMAL) {
        LOG(ERROR, "load_object() failed.");
        object_destroy(obj);
        return NULL;
    }

    object_weight_sum(obj);
    return obj;
}

/**
 * Zero the key_values on op, decrementing the shared-string refcounts
 * and freeing the links.
 *
 * @param op
 * Object to clear.
 */
void
object_free_key_values (object *op)
{
    HARD_ASSERT(op != NULL);

    key_value_t *field, *tmp;
    LL_FOREACH_SAFE(op->key_values, field, tmp) {
        if (field->key != NULL) {
            free_string_shared(field->key);
        }

        if (field->value != NULL) {
            free_string_shared(field->value);
        }

        efree(field);
    }

    op->key_values = NULL;
}

/**
 * Search for a field by key.
 *
 * @param op
 * Object to search in.
 * @param key
 * Key to search. Must be a shared string.
 * @return
 * The link from the list if pb has a field named key, NULL otherwise.
 */
key_value_t *
object_get_key_link (const object *op, shstr *key)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(key != NULL);

    key_value_t *field;
    LL_FOREACH(op->key_values, field) {
        if (field->key == key) {
            return field;
        }
    }

    return NULL;
}

/**
 * Get an extra value by key.
 *
 * @param op
 * Object to search in.
 * @param key
 * Key of which to retrieve the value. Doesn't need to be a shared string.
 * @return
 * The value if found, NULL otherwise.
 * @note
 * The returned string is shared.
 */
shstr *
object_get_value (const object *op, const char *const key)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(key != NULL);

    shstr *shared_key = find_string(key);
    if (shared_key == NULL) {
        return NULL;
    }

    key_value_t *field;
    LL_FOREACH(op->key_values, field) {
        if (field->key == shared_key) {
            return field->value;
        }
    }

    return NULL;
}

/**
 * Updates or sets a key value.
 *
 * @param op
 * Object to update.
 * @param key
 * Key to set or update. Must be a shared string.
 * @param value
 * Value to set. Doesn't need to be a shared string. Can be NULL.
 * @param add_key
 * If false, will not add the key if it doesn't exist in op.
 * @return
 * True if key was updated or added, false otherwise.
 */
static bool
object_set_value_s (object *op, shstr *key, const char *value, bool add_key)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(key != NULL);

    key_value_t *field;
    key_value_t *last = NULL;
    LL_FOREACH(op->key_values, field) {
        if (field->key != key) {
            last = field;
            continue;
        }

        if (field->value != NULL) {
            free_string_shared(field->value);
        }

        if (value != NULL) {
            field->value = add_string(value);
        } else {
            /* Basically, if the archetype has this key set, we need to
             * store the NULL value so when we save it, we save the empty
             * value so that when we load, we get this value back
             * again. */
            if (object_get_key_link(&op->arch->clone, key)) {
                field->value = NULL;
            } else {
                /* Delete this link */
                if (field->key != NULL) {
                    free_string_shared(field->key);
                }

                if (last != NULL) {
                    last->next = field->next;
                } else {
                    op->key_values = field->next;
                }

                efree(field);
            }
        }

        return true;
    }

    if (!add_key) {
        return false;
    }

    /* There isn't any good reason to store a NULL value in the key/value
     * list. If the archetype has this key, then we should also have it,
     * so shouldn't be here. If user wants to store empty strings, should
     * pass in "". */
    if (value == NULL) {
        return true;
    }

    field = emalloc(sizeof(*field));
    field->key = add_refcount(key);
    field->value = add_string(value);
    /* Usual prepend-addition. */
    field->next = op->key_values;
    op->key_values = field;

    return true;
}

/**
 * Updates the key in op to value.
 *
 * @param op
 * Object to update.
 * @param key
 * Key to set or update. Doesn't need to be a shared string.
 * @param value
 * Value to set. Doesn't need to be a shared string. Can be NULL.
 * @param add_key
 * If false, will not add the key if it doesn't exist in op.
 * @return
 * True if key was updated or added, false otherwise.
 * @note
 * This function is merely a wrapper to object_set_value_s() to ensure
 * the key is a shared string.
 */
bool
object_set_value (object *op, const char *key, const char *value, bool add_key)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(key != NULL);

    bool temp_ref = false;
    shstr *shstr_key = find_string(key);
    if (shstr_key == NULL) {
        shstr_key = add_string(key);
        temp_ref = true;
    }

    bool ret = object_set_value_s(op, shstr_key, value, add_key);

    if (temp_ref) {
        free_string_shared(shstr_key);
    }

    return ret;
}

/**
 * Checks if the specified object matches one of the keywords in the specified
 * string. This is used for example by the /drop and /take commands, but also
 * by the /apply command.
 *
 * Calling function takes care of what action might need to be done and
 * if it is valid (pickup, drop, etc).
 *
 * @param op
 * The item we're trying to match.
 * @param caller
 * Who is trying to match the objects, for the purposes of functions like
 * object_get_name_s().
 * @param str
 * String we're searching.
 * @return
 * Non-zero if we have a match. A higher value means a better match. Zero
 * means no match.
 */
int
object_matches_string (object *op, object *caller, const char *str)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(caller != NULL);
    HARD_ASSERT(str != NULL);

    if (caller->type == PLAYER) {
        CONTR(caller)->count = op->nrof;
    }

    char word[MAX_BUF];
    size_t pos = 0;
    while (string_get_word(str, &pos, ',', VS(word), 0)) {
        char *cp = word;

        /* All is a very generic match - low match value */
        if (strcasecmp(cp, "all") == 0) {
            return 1;
        }

        /* Unpaid is a little more specific */
        if (QUERY_FLAG(op, FLAG_UNPAID) && strcasecmp(cp, "unpaid") == 0) {
            return 2;
        }

        bool identified = QUERY_FLAG(op, FLAG_IDENTIFIED);
        if (identified) {
            if ((QUERY_FLAG(op, FLAG_CURSED) || QUERY_FLAG(op, FLAG_DAMNED)) &&
                strcasecmp(cp, "cursed") == 0) {
                return 2;
            }

            if (QUERY_FLAG(op, FLAG_IS_MAGICAL) &&
                strcasecmp(cp, "magical") == 0) {
                return 2;
            }

            if (op->artifact != NULL &&
                strcasecmp(cp, "artifact") == 0) {
                return 2;
            }
        }

        if (!QUERY_FLAG(op, FLAG_INV_LOCKED) &&
            strcasecmp(cp, "unlocked") == 0) {
            return 2;
        }

        if (identified && strcasecmp(cp, "identified") == 0) {
            return 2;
        }

        if (!identified && strcasecmp(cp, "unidentified") == 0) {
            return 2;
        }

        if ((op->type == FOOD || op->type == DRINK) &&
            strcasecmp(cp, "food") == 0) {
            return 2;
        }

        if ((op->type == GEM || op->type == JEWEL || op->type == NUGGET ||
             op->type == PEARL) && strcasecmp(cp, "valuables") == 0) {
            return 2;
        }

        if (op->type == WEAPON) {
            if (op->item_skill - 1 == SK_IMPACT_WEAPONS) {
                if (strcasecmp(cp, "impact weapons") == 0) {
                    return 2;
                }
            } else if (op->item_skill - 1 == SK_SLASH_WEAPONS) {
                if (strcasecmp(cp, "slash weapons") == 0) {
                    return 2;
                }
            } else if (op->item_skill - 1 == SK_CLEAVE_WEAPONS) {
                if (strcasecmp(cp, "cleave weapons") == 0) {
                    return 2;
                }
            } else if (op->item_skill - 1 == SK_PIERCE_WEAPONS) {
                if (strcasecmp(cp, "pierce weapons") == 0) {
                    return 2;
                }
            }
        } else if (op->type == BOOK) {
            if (strcasecmp(cp, "books") == 0) {
                return 2;
            }

            if (op->msg == NULL && strcasecmp(cp, "empty books") == 0) {
                return 2;
            }

            int book_level[2];
            if (!QUERY_FLAG(op, FLAG_NO_SKILL_IDENT)) {
                if (strcasecmp(cp, "unread books") == 0) {
                    return 2;
                }

                if (sscanf(cp, "unread level %d books", &book_level[0]) == 1 &&
                    op->level == book_level[0]) {
                    return 2;
                }

                if (sscanf(cp, "unread level %d-%d books", &book_level[0],
                           &book_level[1]) == 2 &&
                    op->level >= book_level[0] && op->level <= book_level[1]) {
                    return 2;
                }
            } else {
                if (strcasecmp(cp, "read books") == 0) {
                    return 2;
                }

                if (sscanf(cp, "read level %d books", &book_level[0]) == 1 &&
                    op->level == book_level[0]) {
                    return 2;
                }

                if (sscanf(cp, "read level %d-%d books", &book_level[0],
                           &book_level[1]) == 2 &&
                    op->level >= book_level[0] && op->level <= book_level[1]) {
                    return 2;
                }
            }
        }

        int count = 0;

        /* Allow for things like '100 arrows', but don't accept
         * strings like '+2', '-1' as numbers. */
        if (isdigit(cp[0]) && (count = atoi(cp)) != 0) {
            cp = strchr(cp, ' ');

            /* Get rid of spaces */
            while (cp != NULL && cp[0] == ' ') {
                cp++;
            }
        }

        if (cp == NULL || cp[0] == '\0' || count < 0) {
            return 0;
        }

        char *obj_name = object_get_name_s(op, caller);
        char *base_name = object_get_base_name_s(op, caller);
        char *short_name = object_get_short_name_s(op, caller);

        int retval;
        if (strcasecmp(cp, obj_name) == 0) {
            retval = 20;
        } else if (strcasecmp(cp, short_name) == 0) {
            retval = 18;
        } else if (strcasecmp(cp, base_name) == 0) {
            retval = 16;
        } else if (op->custom_name != NULL &&
                   strcasecmp(cp, op->custom_name) == 0) {
            retval = 15;
        } else if (strncasecmp(cp, base_name, strlen(cp)) == 0) {
            retval = 14;
        } else if (strstr(base_name, cp) != NULL) {
            /* Do substring checks, so things like 'Str+1' will match.
             * retval of these should perhaps be lower - they are lower
             * than the specific strcasecmps above, but still higher than
             * some other match criteria. */
            retval = 12;
        } else if (strstr(short_name, cp) != NULL) {
            retval = 12;
        } else if (op->custom_name != NULL &&
                   strstr(op->custom_name, cp) != NULL) {
            /* Check for partial custom name, but give a really low priority. */
            retval = 3;
        } else {
            retval = 0;
        }

        efree(obj_name);
        efree(base_name);
        efree(short_name);

        if (retval != 0) {
            if (caller->type == PLAYER && count != 0) {
                CONTR(caller)->count = count;
            }

            return retval;
        }
    }

    return 0;
}

/**
 * Get object's gender ID, as defined in #GENDER_xxx.
 *
 * @param op
 * Object to get gender ID of.
 * @return
 * The gender ID.
 */
int
object_get_gender (const object *op)
{
    HARD_ASSERT(op != NULL);

    if (QUERY_FLAG(op, FLAG_IS_MALE)) {
        if (QUERY_FLAG(op, FLAG_IS_FEMALE)) {
            return GENDER_HERMAPHRODITE;
        } else {
            return GENDER_MALE;
        }
    } else if (QUERY_FLAG(op, FLAG_IS_FEMALE)) {
        return GENDER_FEMALE;
    }

    return GENDER_NEUTER;
}

/**
 * Reverses order of all the objects in the specified object's inventory.
 *
 * @param op
 * Object to reverse the inventory of.
 */
void
object_reverse_inventory (object *op)
{
    HARD_ASSERT(op != NULL);

    if (op->inv == NULL) {
        return;
    }

    if (op->inv->inv != NULL) {
        object_reverse_inventory(op->inv);
    }

    object *next = op->inv->below;
    op->inv->above = NULL;
    op->inv->below = NULL;

    while (next != NULL) {
        object *tmp = next;
        next = next->below;

        tmp->above = NULL;
        tmp->below = op->inv;
        tmp->below->above = tmp;
        op->inv = tmp;

        if (tmp->inv != NULL) {
            object_reverse_inventory(tmp);
        }
    }
}

/**
 * Make the specified object enter a map, using either an absolute position
 * with a map pointer and coordinates, or using an exit object.
 *
 * If neither 'm' nor 'exit' is specified, the object enters the emergency
 * map.
 *
 * @param op
 * Object entering a map.
 * @param exit
 * Exit object to use in order to ender the map.
 * @param m
 * Map to enter.
 * @param x
 * X coordinate.
 * @param y
 * Y coordinate.
 * @param fixed_pos
 * If true, will not attempt to find an adjacency square if the original
 * destination is blocked.
 * @return
 * True on success, false on failure.
 */
bool
object_enter_map (object    *op,
                  object    *exit,
                  mapstruct *m,
                  int        x,
                  int        y,
                  bool       fixed_pos)
{
    HARD_ASSERT(op != NULL);

    op = HEAD(op);
    mapstruct *oldmap = op->map;

    if (m == NULL && exit != NULL) {
        if (EXIT_PATH(exit) == NULL) {
            return false;
        }

        x = EXIT_X(exit);
        y = EXIT_Y(exit);
        fixed_pos = QUERY_FLAG(exit, FLAG_USE_FIX_POS);

        if (strcmp(EXIT_PATH(exit), "/random/") == 0) {
            RMParms rp;
            memset(&rp, 0, sizeof(RMParms));

            rp.Xsize = -1;
            rp.Ysize = -1;

            if (exit->msg != NULL) {
                set_random_map_variable(&rp, exit->msg);
            }

            rp.origin_x = exit->x;
            rp.origin_y = exit->y;
            snprintf(VS(rp.origin_map), "%s", op->map->path);

            /* Pick a new pathname for the new map. Currently, we just use a
             * static variable and increment the counter by one each time. */
            static uint64_t reference_number = 0;
            char newmap_name[HUGE_BUF];
            snprintf(VS(newmap_name), "/random/%"PRIu64, reference_number++);

            /* Now to generate the actual map. */
            m = generate_random_map(newmap_name, &rp);

            /* Update the exit_ob so it now points directly at the newly
             * created random map. */
            if (m != NULL) {
                x = EXIT_X(exit) = MAP_ENTER_X(m);
                y = EXIT_Y(exit) = MAP_ENTER_Y(m);
                FREE_AND_COPY_HASH(EXIT_PATH(exit), newmap_name);
                FREE_AND_COPY_HASH(m->path, newmap_name);
            }
        } else if (exit->map != NULL) {
            bool unique = (op->type == PLAYER &&
                           (exit->last_eat == MAP_PLAYER_MAP ||
                            (MAP_UNIQUE(exit->map) &&
                             !map_path_isabs(EXIT_PATH(exit)))));
            char *path = map_get_path(exit->map,
                                      EXIT_PATH(exit),
                                      unique,
                                      op->name);
            m = ready_map_name(path, NULL, 0);
            efree(path);

            /* Failed to load a random map? */
            if (m == NULL &&
                op->type == PLAYER &&
                strncmp(EXIT_PATH(exit), "/random/", 8) == 0) {
                m = ready_map_name(CONTR(op)->savebed_map, NULL, 0);
                return object_enter_map(op,
                                        NULL,
                                        m,
                                        CONTR(op)->bed_x,
                                        CONTR(op)->bed_y,
                                        true);
            }
        } else {
            m = ready_map_name(EXIT_PATH(exit), NULL, MAP_NAME_SHARED);
        }

        if (m == NULL) {
            return false;
        }

        /* If exit is damned, update player's savebed position. */
        if (QUERY_FLAG(exit, FLAG_DAMNED) && op->type == PLAYER) {
            snprintf(VS(CONTR(op)->savebed_map), "%s", m->path);
            CONTR(op)->bed_x = x;
            CONTR(op)->bed_y = y;
            player_save(op);
        }
    }

    if (m == NULL) {
        m = ready_map_name(EMERGENCY_MAPPATH, NULL, 0);
        x = EMERGENCY_X;
        y = EMERGENCY_Y;
        fixed_pos = true;
    }

    if (exit == NULL && MAP_FIXEDLOGIN(m)) {
        x = MAP_ENTER_X(m);
        y = MAP_ENTER_Y(m);
    }

    mapstruct *m2 = get_map_from_coord(m, &x, &y);
    if (m2 == NULL) {
        LOG(ERROR,
            "Invalid exit coordinates (%d,%d): %s",
            x,
            y,
            object_get_str(exit));
        x = MAP_ENTER_X(m);
        y = MAP_ENTER_Y(m);
    } else {
        m = m2;
    }

    if (!fixed_pos && blocked(op, m, x, y, TERRAIN_ALL) != 0) {
        int i = map_free_spot(m, x, y, 1, SIZEOFFREE1, op->arch, NULL);
        if (i != -1) {
            x += freearr_x[i];
            y += freearr_y[i];
        }
    }

    if (!QUERY_FLAG(op, FLAG_REMOVED)) {
        object_remove(op, 0);
    }

    if (exit != NULL) {
        int sub_direction = exit->last_heal - 1 == TILED_UP ? 1 : -1;
        for (int sub_layer = op->sub_layer;
             sub_layer >= 0 && sub_layer < NUM_SUB_LAYERS;
             sub_layer += sub_direction) {
            object *floor = GET_MAP_OB_LAYER(m, x, y, LAYER_FLOOR, sub_layer);
            if (floor != NULL) {
                op->sub_layer = sub_layer;
                break;
            }
        }
    }

    if (op->map != NULL && op->type == PLAYER) {
        trigger_map_event(MEVENT_LEAVE, op->map, op, NULL, NULL, NULL, 0);
    }

    op->x = x;
    op->y = y;
    op = object_insert_map(op, m, NULL, 0);
    if (op == NULL) {
        return false;
    }

    trigger_map_event(MEVENT_ENTER, m, op, NULL, NULL, NULL, 0);
    m->timeout = 0;

    /* Do some action special for players after we have inserted them. */
    if (op->type == PLAYER) {
        if (CONTR(op) != NULL) {
            snprintf(VS(CONTR(op)->maplevel), "%s", m->path);
            CONTR(op)->count = 0;
        }

        /* If the player is changing maps, we need to do some special things
         * Do this after the player is on the new map - otherwise the force
         * swap of the old map does not work. */
        if (oldmap != NULL && oldmap != m && oldmap->player_first == NULL) {
            set_map_timeout(oldmap);
        }
    }

    if (exit != NULL && exit->stats.dam != 0 && op->type == PLAYER) {
        attack_hit(op, exit, exit->stats.dam);
    }

    return true;
}

/**
 * Acquires a string representation of the object that is suitable for
 * debugging purposes, as it includes the object's name, archname, map,
 * environment, etc.
 *
 * This function cycles through internal buffers to use as return values,
 * and is safe to call up to ten times. After that, previously returned
 * pointers will start getting overwritten.
 *
 * @param op
 * Object. Can be NULL.
 * @return
 * String representation of the object.
 */
const char *
object_get_str (const object *op)
{
    static char buf[10][HUGE_BUF * 16];
    static int buf_idx = 0;

    buf_idx++;
    buf_idx %= 10;

    return object_get_str_r(op, VS(buf[buf_idx]));
}

/**
 * Re-entrant version of object_get_str().
 *
 * @param op
 * Object. Can be NULL.
 * @param buf
 * Buffer to use.
 * @param bufsize
 * Size of 'buf'.
 * @return
 * 'buf'.
 */
char *
object_get_str_r (const object *op, char *buf, size_t bufsize)
{
    HARD_ASSERT(buf != NULL);

    if (op == NULL) {
        snprintf(buf, bufsize, "<no object>");
        return buf;
    }

    snprintf(buf, bufsize, "%s UID: %u",
            op->name != NULL ? op->name : "<no name>",
            op->count);

    if (arch_table != NULL && op->arch != NULL && op->arch->name != NULL) {
        snprintfcat(buf, bufsize, " arch: %s", op->arch->name);
    }

    if (first_map != NULL && op->map != NULL) {
        snprintfcat(buf, bufsize, " map: %s [%s] @ %d,%d",
                op->map->name != NULL ? op->map->name : "<no name>",
                op->map->path != NULL ? op->map->path : "<no path>",
                op->x, op->y);
    } else if (op->env != NULL) {
        char buf2[HUGE_BUF];
        snprintfcat(buf, bufsize, " env: [%s]", object_get_str_r(op->env,
                VS(buf2)));
    }

    return buf;
}

/**
 * Checks if the specified coordinates are blocked for the specified object.
 *
 * Takes multi-part objects into account.
 *
 * @param op
 * Object to check.
 * @param m
 * Map.
 * @param x
 * X coordinate.
 * @param y
 * Y coordinate.
 * @return
 * 0 if the tile is not blocked, a combination of @ref map_look_flags
 * otherwise.
 */
int
object_blocked (object *op, mapstruct *m, int x, int y)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(m != NULL);

    SOFT_ASSERT_RC(!OUT_OF_MAP(m, x, y),
                   P_OUT_OF_MAP,
                   "Out of map: %s %d,%d",
                   m->path,
                   x,
                   y);

    op = HEAD(op);

    if (op->more == NULL) {
        return blocked(op, m, x, y, op->terrain_flag);
    }

    for (object *tmp = op; tmp != NULL; tmp = tmp->more) {
        int xt = x + tmp->arch->clone.x;
        int yt = y + tmp->arch->clone.y;
        mapstruct *map = get_map_from_coord(m, &xt, &yt);
        if (map == NULL) {
            return P_OUT_OF_MAP;
        }

        /* If this part is a different part of the head, then skip checking
         * this tile. */
        object *tmp2;
        for (tmp2 = op; tmp2 != NULL; tmp2 = tmp2->more) {
            if (tmp2->map == map && tmp2->x == xt && tmp2->y == yt) {
                break;
            }
        }

        if (tmp2 != NULL) {
            continue;
        }

        int ret = blocked(op, map, xt, yt, op->terrain_flag);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

/**
 * Creates a dummy object.
 *
 * @param name
 * Name to give the dummy object. Can be NULL.
 * @return
 * Object of specified name. It fill have the #FLAG_NO_PICK flag set.
 */
object *
object_create_singularity (const char *name)
{
    char buf[MAX_BUF];
    snprintf(VS(buf), "singularity");
    if (name != NULL) {
        snprintfcat(VS(buf), " (%s)", name);
    }

    object *op = object_get();
    FREE_AND_COPY_HASH(op->name, buf);
    SET_FLAG(op, FLAG_NO_PICK);
    return op;
}

/**
 * Dumps all variables in an object to a file.
 *
 * @param op
 * Object to save.
 * @param fp
 * Where to save the object's text representation. Can be NULL, in which
 * case this is a no-op.
 */
void
object_save (const object *op, FILE *fp)
{
    HARD_ASSERT(op != NULL);

    if (fp == NULL) {
        return;
    }

    archetype_t *at = op->arch;
    if (at == NULL) {
        at = arches[ARCH_EMPTY_ARCHETYPE];
    }

    fprintf(fp, "arch %s\n", at->name);

    StringBuffer *sb = stringbuffer_new();
    get_ob_diff(sb, op, &at->clone);

    char *cp = stringbuffer_finish(sb);
    fputs(cp, fp);
    efree(cp);

    for (object *tmp = op->inv; tmp != NULL; tmp = tmp->below) {
        object_save(tmp, fp);
    }

    fprintf(fp, "end\n");
}
