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
 * Functions related to attributes, weight, experience, which concern
 * only living things.
 */

#include <global.h>
#include <monster_data.h>
#include <arch.h>
#include <plugin.h>
#include <player.h>
#include <object.h>

/** When we carry more than this of our weight_limit, we get encumbered. */
#define ENCUMBRANCE_LIMIT 65.0

/**
 * Bonus to melee/ranged damage. Based on strength.
 */
double dam_bonus[MAX_STAT + 1] = {
    -2.5,                           // 0
    -2.0, -2.0, -1.5, -1.5, -1.5,   // 1-5
    -1.0, -1.0, -1.0, -0.5, -0.5,   // 6-10
    0.0, 0.0, 0.0, 0.0, 0.0,        // 11-15
    0.5, 0.5, 1.0, 1.0, 1.5,        // 16-20
    1.5, 1.5, 2.0, 2.0, 2.5,        // 21-25
    3.0, 3.5, 4.0, 4.5, 5.0,        // 26-30
};

/**
 * WC bonus. Based on dexterity.
 */
int wc_bonus[MAX_STAT + 1] = {
    -5,                     // 0
    -4, -4, -3, -3, -3,     // 1-5
    -2, -2, -2, -1, -1,     // 6-10
    0, 0, 0, 0, 0,          // 11-15
    1, 1, 2, 2, 3,          // 16-20
    3, 3, 4, 4, 5,          // 21-25
    5, 5, 6, 7, 8,          // 26-30
};

/**
 * Maximum health points bonus. Based on constitution.
 */
static double hp_bonus[MAX_STAT + 1] = {
    -0.8,                               // 0
    -0.6, -0.5, -0.4, -0.35, -0.3,      // 1-5
    -0.25, -0.2, -0.15, -0.11, -0.07,   // 6-10
    0.0, 0.0, 0.0, 0.0, 0.0,            // 11-15
    0.1, 0.15, 0.2, 0.25, 0.3,          // 16-20
    0.35, 0.4, 0.45, 0.5, 0.55,         // 21-25
    0.6, 0.7, 0.8, 0.9, 1.0             // 26-30
};

/**
 * Maximum spell points bonus. Primarily based on intelligence, secondarily on
 * power.
 */
static double sp_bonus[MAX_STAT + 1] = {
    -0.8,                               // 0
    -0.6, -0.5, -0.4, -0.35, -0.3,      // 1-5
    -0.25, -0.2, -0.15, -0.11, -0.07,   // 6-10
    0.0, 0.0, 0.0, 0.0, 0.0,            // 11-15
    0.1, 0.2, 0.3, 0.4, 0.5,            // 16-20
    0.6, 0.7, 0.8, 0.9, 1.0,            // 21-25
    1.1, 1.4, 1.6, 1.8, 2.0,            // 26-30
};

/**
 * Speed bonus. Based on dexterity.
 */
float speed_bonus[MAX_STAT + 1] = {
    -0.4,                                   // 0
    -0.4, -0.3, -0.3, -0.2,                 // 1-5
    -0.2, -0.2, -0.1, -0.1, -0.1,           // 6-10
    -0.05, 0.0, 0.0, 0.0, 0.025, 0.05,      // 11-15
    0.075, 0.1, 0.125, 0.15, 0.175, 0.2,    // 16-20
    0.225, 0.25, 0.275, 0.3,                // 21-25
    0.325, 0.35, 0.4, 0.45, 0.5,            // 26-30
};

/**
 * Falling damage mitigation. Based on dexterity.
 */
double falling_mitigation[MAX_STAT + 1] = {
    2.0,                            // 0
    1.9, 1.8, 1.7, 1.6, 1.5,        // 1-5
    1.4, 1.3, 1.2, 1.1, 1.0,        // 6-10
    1.05, 1.0, 1.0, 1.0, 1.0,       // 11-15
    0.98, 0.96, 0.94, 0.92, 0.9,    // 16-20
    0.88, 0.84, 0.80, 0.77, 0.73,   // 21-25
    0.7, 0.65, 0.6, 0.55, 0.5,      // 26-30
};

/**
 * The absolute most a character can carry - a character can't pick stuff
 * up if it would put him above this limit.
 *
 * Value is in grams, so we don't need to do conversion later
 *
 * These limits are probably overly generous, but being there were no
 * values before, you need to start someplace.
 */
uint32_t weight_limit[MAX_STAT + 1] = {
    20000,
    25000,  30000,  35000,  40000,  50000,
    60000,  70000,  80000,  90000,  100000,
    110000, 120000, 130000, 140000, 150000,
    165000, 180000, 195000, 210000, 225000,
    240000, 255000, 270000, 285000, 300000,
    325000, 350000, 375000, 400000, 450000
};

/**
 * Probability to learn a spell or skill, based on intelligence or
 * wisdom.
 */
int learn_spell[MAX_STAT + 1] = {
    0, 0, 0, 1, 2, 4, 8, 12, 16, 25, 36, 45, 55, 65, 70, 75, 80, 85, 90, 95, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 100, 100
};

/**
 * Probability for monsters to signal their friends, based on their intelligence
 * stat.
 */
int monster_signal_chance[MAX_STAT + 1] = {
    0, // 0
    0, 0, 0, 0, 0, // 1-5
    20, 18, 16, 14, 13, // 6-10
    12, 11, 10, 9, 8, // 11-15
    7, 6, 5, 4, 3, // 16-20
    2, 1, 1, 1, 1, // 21-25
    1, 1, 1, 1, 1, // 26-30
};

/**
 * Probability to avoid something.
 */
int savethrow[MAXLEVEL + 1] = {
    18,
    18, 17, 16, 15, 14, 14, 13, 13, 12, 12, 12, 11, 11, 11, 11, 10, 10, 10, 10, 9,
    9, 9, 9, 9, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 7, 7, 6, 6, 6,
    6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

/** Message when a player is drained of a stat. */
static const char *const drain_msg[NUM_STATS] = {
    "Oh no! You are weakened!",
    "You're feeling clumsy!",
    "You feel less healthy",
    "Watch out, your mind is going!",
    "Your spirit feels drained!"
};

/** Message when a player has a stat restored. */
const char *const restore_msg[NUM_STATS] = {
    "You feel your strength return.",
    "You feel your agility return.",
    "You feel your health return.",
    "You feel your memory return.",
    "You feel your spirits return."
};

/** Message when a player increases a stat. */
static const char *const gain_msg[NUM_STATS] = {
    "You feel stronger.",
    "You feel more agile.",
    "You feel healthy.",
    "You feel smarter.",
    "You feel more potent."
};

/** Message when a player decreases a stat. */
const char *const lose_msg[NUM_STATS] = {
    "You feel weaker!",
    "You feel clumsy!",
    "You feel less healthy!",
    "You feel stupid!",
    "You feel less potent!"
};

/** Names of stats. */
const char *const statname[NUM_STATS] = {
    "strength",
    "dexterity",
    "constitution",
    "intelligence",
    "power"
};

/** Short names of stats. */
const char *const short_stat_name[NUM_STATS] = {
    "Str",
    "Dex",
    "Con",
    "Int",
    "Pow"
};

/**
 * Sets Str/Dex/con/Wis/Cha/Int/Pow in stats to value, depending on what
 * attr is (STR to POW).
 * @param stats
 * Item to modify. Must not be NULL.
 * @param attr
 * Attribute to change.
 * @param value
 * New value.
 */
void set_attr_value(living *stats, int attr, int8_t value)
{
    switch (attr) {
    case STR:
        stats->Str = value;
        break;

    case DEX:
        stats->Dex = value;
        break;

    case CON:
        stats->Con = value;
        break;

    case POW:
        stats->Pow = value;
        break;

    case INT:
        stats->Int = value;
        break;
    }
}

/**
 * Like set_attr_value(), but instead the value (which can be negative)
 * is added to the specified stat.
 *
 * Checking is performed to make sure old value + new value doesn't overflow
 * the stat integer.
 * @param stats
 * Item to modify. Must not be NULL.
 * @param attr
 * Attribute to change.
 * @param value
 * Delta (can be positive).
 */
void change_attr_value(living *stats, int attr, int8_t value)
{
    int16_t result;

    if (value == 0) {
        return;
    }

    result = get_attr_value(stats, attr) + value;

    /* Prevent possible overflow of the stat. */
    if (result > INT8_MAX || result < INT8_MIN) {
        return;
    }

    set_attr_value(stats, attr, result);
}

/**
 * Gets the value of a stat.
 * @param stats
 * Item from which to get stat.
 * @param attr
 * Attribute to get.
 * @return
 * Specified attribute, 0 if not found.
 * @see set_attr_value().
 */
int8_t get_attr_value(const living *stats, int attr)
{
    switch (attr) {
    case STR:
        return stats->Str;

    case DEX:
        return stats->Dex;

    case CON:
        return stats->Con;

    case INT:
        return stats->Int;

    case POW:
        return stats->Pow;
    }

    return 0;
}

/**
 * Ensures that all stats (str/dex/con/wis/cha/int) are within the
 * passed in range of MIN_STAT and MAX_STAT.
 * @param stats
 * Attributes to check.
 */
void check_stat_bounds(living *stats)
{
    int i, v;

    for (i = 0; i < NUM_STATS; i++) {
        v = get_attr_value(stats, i);

        if (v > MAX_STAT) {
            set_attr_value(stats, i, MAX_STAT);
        } else if (v < MIN_STAT) {
            set_attr_value(stats, i, MIN_STAT);
        }
    }
}

/**
 * Drains a random stat from op.
 * @param op
 * Object to drain.
 */
void drain_stat(object *op)
{
    drain_specific_stat(op, rndm(1, NUM_STATS) - 1);
}

/**
 * Drain a specified stat from op.
 * @param op
 * Victim to drain.
 * @param deplete_stats
 * Statistic to drain.
 */
void drain_specific_stat(object *op, int deplete_stats)
{
    object *tmp;
    archetype_t *at = arch_find("depletion");

    if (!at) {
        LOG(BUG, "Couldn't find archetype depletion.");
        return;
    } else {
        tmp = object_find_arch(op, at);

        if (!tmp) {
            tmp = arch_to_object(at);
            tmp = object_insert_into(tmp, op, 0);
            SET_FLAG(tmp, FLAG_APPLIED);
        }
    }

    draw_info(COLOR_GRAY, op, drain_msg[deplete_stats]);
    change_attr_value(&tmp->stats, deplete_stats, -1);
    living_update_player(op);
}

/**
 * Propagate stats an item gives to a living object, eg, a player.
 * @param op
 * Living object.
 * @param item
 * Item with the stats to give.
 */
static void
living_apply_flags (object       *op,
                    const object *item)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(item != NULL);

    op->path_attuned |= item->path_attuned;
    op->path_repelled |= item->path_repelled;
    op->path_denied |= item->path_denied;

    op->terrain_flag |= item->terrain_type;

    if (QUERY_FLAG(item, FLAG_LIFESAVE)) {
        SET_FLAG(op, FLAG_LIFESAVE);
    }

    if (QUERY_FLAG(item, FLAG_REFL_SPELL)) {
        SET_FLAG(op, FLAG_REFL_SPELL);
    }

    if (QUERY_FLAG(item, FLAG_REFL_MISSILE)) {
        SET_FLAG(op, FLAG_REFL_MISSILE);
    }

    if (QUERY_FLAG(item, FLAG_STEALTH)) {
        SET_FLAG(op, FLAG_STEALTH);
    }

    if (QUERY_FLAG(item, FLAG_XRAYS)) {
        SET_FLAG(op, FLAG_XRAYS);

        if (op->type == PLAYER) {
            CONTR(op)->update_los = 1;
        }
    }

    if (QUERY_FLAG(item, FLAG_BLIND)) {
        SET_FLAG(op, FLAG_BLIND);

        if (op->type == PLAYER) {
            CONTR(op)->update_los = 1;
        }
    }

    if (QUERY_FLAG(item, FLAG_SEE_IN_DARK)) {
        SET_FLAG(op, FLAG_SEE_IN_DARK);
    }

    if (QUERY_FLAG(item, FLAG_CAN_PASS_THRU)) {
        SET_FLAG(op, FLAG_CAN_PASS_THRU);
    }

    if (QUERY_FLAG(item, FLAG_MAKE_ETHEREAL)) {
        SET_FLAG(op, FLAG_CAN_PASS_THRU);
        SET_FLAG(op, FLAG_IS_ETHEREAL);
    }

    if (QUERY_FLAG(item, FLAG_MAKE_INVISIBLE)) {
        SET_FLAG(op, FLAG_IS_INVISIBLE);

        if (op->type == PLAYER) {
            CONTR(op)->cs->ext_title_flag = 1;
        }
    }

    if (QUERY_FLAG(item, FLAG_SEE_INVISIBLE)) {
        SET_FLAG(op, FLAG_SEE_INVISIBLE);
    }

    if (QUERY_FLAG(item, FLAG_FLYING)) {
        SET_FLAG(op, FLAG_FLYING);
    }

    if (QUERY_FLAG(item, FLAG_UNDEAD)) {
        SET_FLAG(op, FLAG_UNDEAD);
    }

    if (QUERY_FLAG(item, FLAG_CONFUSED)) {
        SET_FLAG(op, FLAG_CONFUSED);
    }
}

/**
 * Update player's stats that are acquired from the specified item.
 *
 * @param pl
 * Player.
 * @param op
 * Player object.
 * @param item
 * Item the stats are acquired from.
 * @param[out] attacks Attack types.
 * @param[out] protect_bonus Protection bonuses.
 * @param[out] protect_malus Protection maluses.
 * @param[out] max_bonus_hp Bonus to max HP.
 * @param[out] max_bonus_hp Bonus to max SP.
 * @param[out] max_speed Maximum speed.
 * @param[out] added_speed Added speed.
 * @param[out] bonus_speed Bonus speed.
 */
static void
living_update_player_item (player       *pl,
                           object       *op,
                           const object *item,
                           double       *attacks,
                           double       *protect_bonus,
                           double       *protect_malus,
                           int          *max_bonus_hp,
                           int          *max_bonus_sp,
                           double       *max_speed,
                           double       *added_speed,
                           double       *bonus_speed)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(item != NULL);
    HARD_ASSERT(protect_bonus != NULL);
    HARD_ASSERT(protect_malus != NULL);
    HARD_ASSERT(max_bonus_hp != NULL);
    HARD_ASSERT(max_bonus_sp != NULL);
    HARD_ASSERT(added_speed != NULL);
    HARD_ASSERT(bonus_speed != NULL);

    /* If this is an armour piece with a speed cap, update player's max speed
     * if it's higher than the cap. */
    if (IS_ARMOR(item) && ARMOUR_SPEED(item) != 0 &&
        *max_speed > ARMOUR_SPEED(item) / 10.0) {
        *max_speed = ARMOUR_SPEED(item) / 10.0;
    }

    /* Add the armour class, factoring in the item's magic. */
    if (item->stats.ac != 0) {
        op->stats.ac += item->stats.ac + item->magic;
    }

    if (item->block != 0) {
        op->block += item->block + item->magic;
    }

    if (item->absorb != 0) {
        op->absorb += item->absorb + item->magic;
    }

    /* Add stats for non-ranged items. */
    if (!OBJECT_IS_RANGED(item)) {
        /* Add the weapon class, factoring in the item's magic. */
        if (item->stats.wc != 0) {
            op->stats.wc += item->stats.wc + item->magic;
        }

        /* Add the damage, factoring in the item's magic. This affects melee
         * weapons damage. */
        if (item->stats.dam) {
            int16_t dam = item->stats.dam + item->magic;
            op->stats.dam += dam;

            if (!IS_WEAPON(item)) {
                pl->dam_bonus += dam;
            }
        }

        for (int i = 0; i < NROFATTACKS; i++) {
            double mod = item->item_condition / 100.0;

            if (item->protection[i] > 0) {
                double bonus = 100.0 - protect_bonus[i];
                bonus *= item->protection[i] * mod;
                bonus /= 100.0;
                protect_bonus[i] += bonus;
            } else if (item->protection[i] < 0) {
                double malus = 100.0 - protect_malus[i];
                malus *= -item->protection[i];
                malus /= 100.0;
                protect_malus[i] += malus;
            }

            if (item->attack[i] > 0) {
                attacks[i] += item->attack[i] * mod;
            }
        }

        /* Add the speed modifier. */
        if (item->stats.exp != 0 && item->type != SKILL) {
            if (item->stats.exp > 0) {
                *added_speed += item->stats.exp / 3.0;
                *bonus_speed += 1.0 + item->stats.exp / 3.0;
            } else {
                *added_speed += item->stats.exp;
            }
        }
    }

    /* If it's not any sort of weapon, add some extra stats such as max HP/SP,
     * digestion, etc. */
    if (!IS_WEAPON(item) && !OBJECT_IS_RANGED(item)) {
        *max_bonus_hp += item->stats.maxhp;
        *max_bonus_sp += item->stats.maxsp;

        pl->digestion += item->stats.food;
        pl->gen_sp += item->stats.sp;
        pl->gen_hp += item->stats.hp;
        pl->gen_sp_armour += item->last_heal;
    }

    pl->item_power += item->item_power;

    for (int i = 0; i < NUM_STATS; i++) {
        int8_t value = get_attr_value(&item->stats, i);
        change_attr_value(&op->stats, i, value);
    }

    living_apply_flags(op, item);
}

/**
 * Updates player object based on the applied items in their inventory.
 * @param op
 * Player to update.
 */
void living_update_player(object *op)
{
    HARD_ASSERT(op != NULL);

    op = HEAD(op);

    SOFT_ASSERT(op->type == PLAYER, "Called with non-player: %s",
                object_get_str(op));

    if (QUERY_FLAG(op, FLAG_NO_FIX_PLAYER)) {
        return;
    }

    player *pl = CONTR(op);

    pl->digestion = 3;
    pl->gen_hp = 1;
    pl->gen_sp = 1;
    pl->gen_sp_armour = 0;
    pl->item_power = 0;
    pl->quest_container = NULL;
    pl->dam_bonus = 0;

    for (int i = 0; i < NUM_STATS; i++) {
        int8_t value = get_attr_value(&op->arch->clone.stats, i);
        set_attr_value(&op->stats, i, value);
    }

    op->stats.wc = op->arch->clone.stats.wc;
    op->stats.ac = op->arch->clone.stats.ac;
    op->stats.dam = op->arch->clone.stats.dam;

    op->block = op->arch->clone.block;
    op->absorb = op->arch->clone.absorb;

    op->stats.maxhp = op->arch->clone.stats.maxhp;
    op->stats.maxsp = op->arch->clone.stats.maxsp;

    op->stats.wc_range = op->arch->clone.stats.wc_range;

    op->speed = op->arch->clone.speed;
    op->weapon_speed = 0;
    op->path_attuned = op->arch->clone.path_attuned;
    op->path_repelled = op->arch->clone.path_repelled;
    op->path_denied = op->arch->clone.path_denied;
    /* Reset terrain moving abilities */
    op->terrain_flag = op->arch->clone.terrain_flag;

    FREE_AND_CLEAR_HASH(op->slaying);

    if (!QUERY_FLAG(&op->arch->clone, FLAG_XRAYS)) {
        CLEAR_FLAG(op, FLAG_XRAYS);
    }

    if (!QUERY_FLAG(&op->arch->clone, FLAG_CAN_PASS_THRU)) {
        CLEAR_MULTI_FLAG(op, FLAG_CAN_PASS_THRU);
    }

    if (!QUERY_FLAG(&op->arch->clone, FLAG_IS_ETHEREAL)) {
        CLEAR_MULTI_FLAG(op, FLAG_IS_ETHEREAL);
    }

    if (!QUERY_FLAG(&op->arch->clone, FLAG_LIFESAVE)) {
        CLEAR_FLAG(op, FLAG_LIFESAVE);
    }

    if (!QUERY_FLAG(&op->arch->clone, FLAG_STEALTH)) {
        CLEAR_FLAG(op, FLAG_STEALTH);
    }

    if (!QUERY_FLAG(&op->arch->clone, FLAG_BLIND)) {
        CLEAR_FLAG(op, FLAG_BLIND);
    }

    if (!QUERY_FLAG(&op->arch->clone, FLAG_FLYING)) {
        CLEAR_MULTI_FLAG(op, FLAG_FLYING);
    }

    if (!QUERY_FLAG(&op->arch->clone, FLAG_REFL_SPELL)) {
        CLEAR_FLAG(op, FLAG_REFL_SPELL);
    }

    if (!QUERY_FLAG(&op->arch->clone, FLAG_REFL_MISSILE)) {
        CLEAR_FLAG(op, FLAG_REFL_MISSILE);
    }

    if (!QUERY_FLAG(&op->arch->clone, FLAG_UNDEAD)) {
        CLEAR_FLAG(op, FLAG_UNDEAD);
    }

    if (!QUERY_FLAG(&op->arch->clone, FLAG_SEE_IN_DARK)) {
        CLEAR_FLAG(op, FLAG_SEE_IN_DARK);
    }

    if (!QUERY_FLAG(&op->arch->clone, FLAG_IS_INVISIBLE)) {
        CLEAR_FLAG(op, FLAG_IS_INVISIBLE);
    }

    if (!QUERY_FLAG(&op->arch->clone, FLAG_SEE_INVISIBLE)) {
        CLEAR_FLAG(op, FLAG_SEE_INVISIBLE);
    }

    if (!QUERY_FLAG(&op->arch->clone, FLAG_CONFUSED)) {
        CLEAR_FLAG(op, FLAG_CONFUSED);
    }

    if (!QUERY_FLAG(&op->arch->clone, FLAG_BLIND)) {
        CLEAR_FLAG(op, FLAG_BLIND);
    }

    if (!QUERY_FLAG(&op->arch->clone, FLAG_IS_ASSASSINATION)) {
        CLEAR_FLAG(op, FLAG_IS_ASSASSINATION);
    }

    /* Initializing player arrays from the values in player archetype clone */
    memset(&pl->equipment, 0, sizeof(pl->equipment));
    memset(&pl->skill_ptr, 0, sizeof(pl->skill_ptr));
    memcpy(&op->protection, &op->arch->clone.protection,
           sizeof(op->protection));
    memcpy(&op->attack, &op->arch->clone.attack, sizeof(op->attack));

    int8_t old_glow = op->glow_radius;
    int8_t light = op->arch->clone.glow_radius;

    double attacks[NROFATTACKS] = {0};
    double protect_bonus[NROFATTACKS] = {0};
    double protect_malus[NROFATTACKS] = {0};

    int protect_exact_bonus[NROFATTACKS] = {0};
    int protect_exact_malus[NROFATTACKS] = {0};
    int8_t potion_protection_bonus[NROFATTACKS] = {0};
    int8_t potion_protection_malus[NROFATTACKS] = {0};
    uint8_t potion_attack[NROFATTACKS] = {0};

    double max_speed = 9.0;
    double added_speed = 0.0;
    double bonus_speed = 0.0;
    double speed_reduce_from_disease = 1.0;

    bool ring_left = false;
    bool ring_right = false;

    int max_bonus_hp = 0;
    int max_bonus_sp = 0;

    FOR_INV_PREPARE(op, tmp) {
        if (tmp->type == QUEST_CONTAINER) {
            if (likely(pl->quest_container == NULL)) {
                pl->quest_container = tmp;
            } else {
                LOG(ERROR, "Found duplicate quest container object %s",
                    object_get_str(tmp));
                object_remove(tmp, 0);
                object_destroy(tmp);
            }

            continue;
        }

        if (tmp->type == SCROLL || tmp->type == POTION ||
            (tmp->type == CONTAINER && !OBJECT_IS_AMMO(tmp)) ||
            tmp->type == LIGHT_REFILL) {
            /* Skip objects that do not give any stats, and require no
             * further handling. */
            continue;
        }

        if (tmp->glow_radius > light) {
            if (tmp->type != LIGHT_APPLY || QUERY_FLAG(tmp, FLAG_APPLIED)) {
                light = tmp->glow_radius;
            }
        }

        if (tmp->type == SKILL) {
            if (likely(pl->skill_ptr[tmp->stats.sp] == NULL)) {
                pl->skill_ptr[tmp->stats.sp] = tmp;
            } else {
                LOG(ERROR, "Found duplicate skill object %s",
                    object_get_str(tmp));
                object_remove(tmp, 0);
                object_destroy(tmp);
                continue;
            }
        }

        /* Rest of the code only deals with applied objects, so skip those
         * that are not. */
        if (!QUERY_FLAG(tmp, FLAG_APPLIED)) {
            continue;
        }

        if (tmp->type == POTION_EFFECT) {
            for (int i = 0; i < NUM_STATS; i++) {
                int8_t value = get_attr_value(&tmp->stats, i);
                change_attr_value(&op->stats, i, value);
            }

            for (int i = 0; i < NROFATTACKS; i++) {
                if (tmp->protection[i] > potion_protection_bonus[i]) {
                    potion_protection_bonus[i] = tmp->protection[i];
                } else if (tmp->protection[i] <
                           potion_protection_malus[i]) {
                    potion_protection_malus[i] = tmp->protection[i];
                }

                if (tmp->attack[i] > potion_attack[i]) {
                    potion_attack[i] = tmp->attack[i];
                }
            }

            living_apply_flags(op, tmp);
        } else if (tmp->type == FORCE ||
                   tmp->type == POISONING ||
                   tmp->type == DISEASE ||
                   tmp->type == SYMPTOM) {
            if (ARMOUR_SPEED(tmp) != 0 &&
                max_speed > ARMOUR_SPEED(tmp) / 10.0) {
                max_speed = ARMOUR_SPEED(tmp) / 10.0;
            }

            for (int i = 0; i < NUM_STATS; i++) {
                int8_t value = get_attr_value(&tmp->stats, i);
                change_attr_value(&op->stats, i, value);
            }

            if (tmp->type != DISEASE && tmp->type != SYMPTOM &&
                tmp->type != POISONING) {
                if (tmp->stats.wc != 0) {
                    op->stats.wc += tmp->stats.wc + tmp->magic;
                }

                if (tmp->stats.dam != 0) {
                    int16_t dam = tmp->stats.dam + tmp->magic;
                    op->stats.dam += dam;
                    pl->dam_bonus += dam;
                }

                if (tmp->stats.ac != 0) {
                    op->stats.ac += tmp->stats.ac + tmp->magic;
                }

                op->stats.maxhp += tmp->stats.maxhp;
                op->stats.maxsp += tmp->stats.maxsp;
            }

            if (tmp->type == DISEASE || tmp->type == SYMPTOM) {
                speed_reduce_from_disease = tmp->last_sp / 100.0;
                if (DBL_EQUAL(speed_reduce_from_disease, 0.0)) {
                    speed_reduce_from_disease = 1.0;
                }
            } else if (tmp->stats.exp != 0) {
                if (tmp->stats.exp > 0) {
                    added_speed += tmp->stats.exp / 3.0;
                    bonus_speed += 1.0 + tmp->stats.exp / 3.0;
                } else {
                    added_speed += tmp->stats.exp;
                }
            }

            for (int i = 0; i < NROFATTACKS; i++) {
                if (tmp->protection[i] > protect_exact_bonus[i]) {
                    protect_exact_bonus[i] = tmp->protection[i];
                } else if (tmp->protection[i] < 0) {
                    protect_exact_malus[i] += -tmp->protection[i];
                }

                if (tmp->attack[i] > 0 && tmp->type != DISEASE &&
                    tmp->type != SYMPTOM && tmp->type != POISONING) {
                    unsigned int attack = op->attack[i] + tmp->attack[i];
                    op->attack[i] = MIN(UINT8_MAX, attack);
                }
            }

            living_apply_flags(op, tmp);
        } else if (OBJECT_IS_AMMO(tmp)) {
            pl->equipment[PLAYER_EQUIP_AMMO] = tmp;
        } else if (tmp->type == AMULET) {
            pl->equipment[PLAYER_EQUIP_AMULET] = tmp;
        } else if (tmp->type == WEAPON) {
            pl->equipment[PLAYER_EQUIP_WEAPON] = tmp;
        } else if (OBJECT_IS_RANGED(tmp)) {
            pl->equipment[PLAYER_EQUIP_WEAPON_RANGED] = tmp;
        } else if (tmp->type == GLOVES) {
            pl->equipment[PLAYER_EQUIP_GAUNTLETS] = tmp;
        } else if (tmp->type == RING) {
            if (!ring_left) {
                pl->equipment[PLAYER_EQUIP_RING_LEFT] = tmp;
                ring_left = true;
            } else if (!ring_right) {
                pl->equipment[PLAYER_EQUIP_RING_RIGHT] = tmp;
                ring_right = true;
            } else {
                LOG(ERROR, "Unexpected applied ring: %s",
                    object_get_str(tmp));
                CLEAR_FLAG(tmp, FLAG_APPLIED);
            }
        } else if (tmp->type == HELMET) {
            pl->equipment[PLAYER_EQUIP_HELM] = tmp;
        } else if (tmp->type == ARMOUR) {
            pl->equipment[PLAYER_EQUIP_ARMOUR] = tmp;
        } else if (tmp->type == GIRDLE) {
            pl->equipment[PLAYER_EQUIP_BELT] = tmp;
        } else if (tmp->type == PANTS) {
            pl->equipment[PLAYER_EQUIP_PANTS] = tmp;
        } else if (tmp->type == BOOTS) {
            pl->equipment[PLAYER_EQUIP_BOOTS] = tmp;
        } else if (tmp->type == CLOAK) {
            pl->equipment[PLAYER_EQUIP_CLOAK] = tmp;
        } else if (tmp->type == BRACERS) {
            pl->equipment[PLAYER_EQUIP_BRACERS] = tmp;
        } else if (tmp->type == SHIELD) {
            pl->equipment[PLAYER_EQUIP_SHIELD] = tmp;
        } else if (tmp->type == LIGHT_APPLY) {
            pl->equipment[PLAYER_EQUIP_LIGHT] = tmp;
        } else if (tmp->type == RING && ring_left) {
            pl->equipment[PLAYER_EQUIP_RING_RIGHT] = tmp;
        } else if (tmp->type == SKILL_ITEM) {
            pl->equipment[PLAYER_EQUIP_SKILL_ITEM] = tmp;
        } else if (tmp->type == TRINKET) {
            living_update_player_item(pl,
                                      op,
                                      tmp,
                                      attacks,
                                      protect_bonus,
                                      protect_malus,
                                      &max_bonus_hp,
                                      &max_bonus_sp,
                                      &max_speed,
                                      &added_speed,
                                      &bonus_speed);
        } else {
            LOG(BUG, "Unexpected applied object: %s",
                    object_get_str(tmp));
            CLEAR_FLAG(tmp, FLAG_APPLIED);
        }
    } FOR_INV_FINISH();

    for (int i = 0; i < PLAYER_EQUIP_MAX; i++) {
        if (pl->equipment[i] == NULL) {
            continue;
        }

        /* No bonuses from the shield, if a two-handed weapon or ranged weapon
         * is equipped. */
        if (i == PLAYER_EQUIP_SHIELD &&
            ((pl->equipment[PLAYER_EQUIP_WEAPON] != NULL &&
              QUERY_FLAG(pl->equipment[PLAYER_EQUIP_WEAPON],
                         FLAG_TWO_HANDED)) ||
             (pl->equipment[PLAYER_EQUIP_WEAPON_RANGED] != NULL &&
              QUERY_FLAG(pl->equipment[PLAYER_EQUIP_WEAPON_RANGED],
                         FLAG_TWO_HANDED)))) {
            continue;
        }

        if (i == PLAYER_EQUIP_AMMO || i == PLAYER_EQUIP_LIGHT ||
            i == PLAYER_EQUIP_SKILL_ITEM) {
            continue;
        }

        living_update_player_item(pl,
                                  op,
                                  pl->equipment[i],
                                  attacks,
                                  protect_bonus,
                                  protect_malus,
                                  &max_bonus_hp,
                                  &max_bonus_sp,
                                  &max_speed,
                                  &added_speed,
                                  &bonus_speed);
    }

    for (int i = 0; i < NROFATTACKS; i++) {
        unsigned int attack = op->attack[i] + attacks[i] + potion_attack[i];
        op->attack[i] = MIN(UINT8_MAX, attack);

        /* Add in the potion protections. */
        if (potion_protection_bonus[i] > 0) {
            double bonus = 100.0 - protect_bonus[i];
            bonus *= potion_protection_bonus[i];
            bonus /= 100.0;
            protect_bonus[i] += bonus;
        }

        if (potion_protection_malus[i] < 0) {
            double malus = 100.0 - protect_malus[i];
            malus *= -potion_protection_malus[i];
            malus /= 100.0;
            protect_malus[i] += malus;
        }

        int protection = protect_bonus[i] + protect_exact_bonus[i] -
                         protect_malus[i] - protect_exact_malus[i];
        op->protection[i] = MIN(100, MAX(-100, protection));
    }

    check_stat_bounds(&op->stats);

    /* Now the speed thing... */
    op->speed += speed_bonus[op->stats.Dex];

    if (added_speed >= 0.0) {
        op->speed += added_speed / 10.0;
    } else {
        op->speed /= 1.0 - added_speed;
    }

    if (op->speed > max_speed) {
        op->speed = max_speed;
    }

    /* Calculate real speed */
    op->speed += bonus_speed / 10.0f;

    /* Put a lower limit on speed. Note with this speed, you move once every
     * 100 ticks or so. This amounts to once every 12 seconds of realtime. */
    op->speed = op->speed * speed_reduce_from_disease;

    /* Don't reduce under this value */
    if (op->speed < 0.01f) {
        op->speed = 0.01f;
    } else if (!pl->tgm) {
        /* Max kg we can carry */
        double f = (weight_limit[op->stats.Str] / 100.0f) * ENCUMBRANCE_LIMIT;

        if (op->carrying > f) {
            if (op->carrying >= weight_limit[op->stats.Str]) {
                op->speed = 0.01f;
            } else {
                /* Total encumbrance weight part */
                f = (weight_limit[op->stats.Str] - f);
                /* Value from 0.0 to 1.0 encumbrance */
                f = (weight_limit[op->stats.Str] - op->carrying) / f;

                if (f < 0.0f) {
                    f = 0.0f;
                } else if (f > 1.0f) {
                    f = 1.0f;
                }

                op->speed *= f;

                if (op->speed < 0.01f) {
                    op->speed = 0.01f;
                }
            }
        }
    }

    object_update_speed(op);

    /* The player is invisible; shouldn't emit any light. */
    if (QUERY_FLAG(op, FLAG_IS_INVISIBLE)) {
        light = 0;
    }

    op->glow_radius = light;

    if (op->map != NULL && old_glow != light) {
        adjust_light_source(op->map, op->x, op->y, light - old_glow);
    }

    op->stats.ac += op->level;

    op->stats.maxhp *= op->level + 3;
    op->stats.maxsp *= (pl->skill_ptr[SK_WIZARDRY_SPELLS] != NULL ?
                        pl->skill_ptr[SK_WIZARDRY_SPELLS]->level : 1) + 3;

    /* Now adjust with the % of the stats malus/bonus. */
    op->stats.maxhp += op->stats.maxhp * hp_bonus[op->stats.Con];
    op->stats.maxhp += max_bonus_hp;

    double maxsp = op->stats.maxsp;
    op->stats.maxsp += maxsp * sp_bonus[op->stats.Int];
    op->stats.maxsp += maxsp * (sp_bonus[op->stats.Pow] / 2.5);
    op->stats.maxsp += max_bonus_sp;

    if (op->stats.maxhp < 1) {
        op->stats.maxhp = 1;
    }

    if (op->stats.maxsp < 1) {
        op->stats.maxsp = 1;
    }

    if (op->stats.hp == -1) {
        op->stats.hp = op->stats.maxhp;
    }

    if (op->stats.sp == -1) {
        op->stats.sp = op->stats.maxsp;
    }

    /* Cap the pools to <= max */
    if (op->stats.hp > op->stats.maxhp) {
        op->stats.hp = op->stats.maxhp;
    }

    if (op->stats.sp > op->stats.maxsp) {
        op->stats.sp = op->stats.maxsp;
    }

    object *weapon = pl->equipment[PLAYER_EQUIP_WEAPON];
    if (weapon != NULL && weapon->type == WEAPON && weapon->item_skill != 0) {
        op->weapon_speed = weapon->last_grace;
        op->stats.wc += SKILL_LEVEL(pl, weapon->item_skill - 1);
        double dam = op->stats.dam;
        dam *= LEVEL_DAMAGE(SKILL_LEVEL(pl, weapon->item_skill - 1));
        dam *= weapon->item_condition / 100.0;
        op->stats.dam = dam;

        if (weapon->slaying != NULL) {
            FREE_AND_ADD_REF_HASH(op->slaying, weapon->slaying);
        }

        if (QUERY_FLAG(weapon, FLAG_IS_ASSASSINATION)) {
            SET_FLAG(op, FLAG_IS_ASSASSINATION);
        }
    } else {
        if (pl->skill_ptr[SK_UNARMED] != NULL) {
            op->weapon_speed = pl->skill_ptr[SK_UNARMED]->last_grace;

            for (int i = 0; i < NROFATTACKS; i++) {
                if (pl->skill_ptr[SK_UNARMED]->attack[i] == 0) {
                    continue;
                }

                unsigned int attack = op->attack[i] +
                                      pl->skill_ptr[SK_UNARMED]->attack[i];
                op->attack[i] = MIN(UINT8_MAX, attack);
            }
        }

        op->stats.wc += SKILL_LEVEL(pl, SK_UNARMED);
        op->stats.dam *= LEVEL_DAMAGE(SKILL_LEVEL(pl, SK_UNARMED)) / 2.0;
    }

    /* Add damage bonus based on strength. */
    op->stats.dam += op->stats.dam * dam_bonus[op->stats.Str] / 10.0;
    if (op->stats.dam < 1) {
        op->stats.dam = 1;
    }

    /* Add WC bonus based on dexterity. */
    op->stats.wc += wc_bonus[op->stats.Dex];

    if (pl->quest_container == NULL) {
        LOG(ERROR, "Player %s has no quest container, fixing.",
            object_get_str(op));
        object *quest_container = arch_get(QUEST_CONTAINER_ARCHETYPE);
        object_insert_into(quest_container, op, 0);
        pl->quest_container = quest_container;
    }
}

/**
 * Like living_update_player(), but for monsters.
 * @param op
 * The monster.
 */
void living_update_monster(object *op)
{
    object *base;

    HARD_ASSERT(op != NULL);

    op = HEAD(op);

    SOFT_ASSERT(op->type == MONSTER, "Called with non-monster: %s",
            object_get_str(op));

    if (QUERY_FLAG(op, FLAG_NO_FIX_PLAYER)) {
        return;
    }

    if (op->custom_attrset == NULL) {
        monster_data_init(op);
    }

    /* Will insert or/and return base info */
    base = living_get_base_info(op);

    CLEAR_FLAG(op, FLAG_READY_BOW);
    op->absorb = 0;
    op->block = 0;
    memcpy(&op->protection, &op->arch->clone.protection,
           sizeof(op->protection));

    op->stats.maxhp = (base->stats.maxhp * (op->level + 3) + (op->level / 2) *
            base->stats.maxhp) / 10;
    op->stats.maxsp = base->stats.maxsp * (op->level + 1);

    if (op->stats.hp == -1) {
        op->stats.hp = op->stats.maxhp;
    }

    if (op->stats.sp == -1) {
        op->stats.sp = op->stats.maxsp;
    }

    /* Cap the pools to <= max */
    if (op->stats.hp > op->stats.maxhp) {
        op->stats.hp = op->stats.maxhp;
    }

    if (op->stats.sp > op->stats.maxsp) {
        op->stats.sp = op->stats.maxsp;
    }

    op->stats.ac = base->stats.ac + op->level;
    /* + level / 3 to catch up the equipment improvements of
     * the players in armour items. */
    op->stats.wc = base->stats.wc + op->level + (op->level / 3);
    op->stats.dam = base->stats.dam;

    if (base->stats.wc_range) {
        op->stats.wc_range = base->stats.wc_range;
    } else {
        /* Default value if not set in arch */
        op->stats.wc_range = 20;
    }

    for (object *tmp = op->inv; tmp != NULL; tmp = tmp->below) {
        CLEAR_FLAG(tmp, FLAG_APPLIED);

        /* Check for bow and use it! */
        if (tmp->type == BOW && QUERY_FLAG(op, FLAG_USE_BOW) &&
                check_good_weapon(op, tmp)) {
            SET_FLAG(tmp, FLAG_APPLIED);
            SET_FLAG(op, FLAG_READY_BOW);
        } else if (QUERY_FLAG(op, FLAG_USE_ARMOUR) && IS_ARMOR(tmp) &&
                check_good_armour(op, tmp)) {
            SET_FLAG(tmp, FLAG_APPLIED);
        } else if (QUERY_FLAG(op, FLAG_USE_WEAPON) && tmp->type == WEAPON &&
                check_good_weapon(op, tmp)) {
            SET_FLAG(tmp, FLAG_APPLIED);
        } else if (tmp->type == EVENT_OBJECT && tmp->sub_type == EVENT_AI &&
                tmp->path_attuned & EVENT_FLAG(EVENT_AI_GUARD_STOP)) {
            op->behavior |= BEHAVIOR_GUARD;
        }

        if (QUERY_FLAG(tmp, FLAG_APPLIED)) {
            if (tmp->type == WEAPON) {
                if (tmp->stats.dam != 0) {
                    op->stats.dam += tmp->stats.dam + tmp->magic;
                }

                if (tmp->stats.wc != 0) {
                    op->stats.wc += tmp->stats.wc + tmp->magic;
                }
            } else if (IS_ARMOR(tmp)) {
                for (int i = 0; i < NROFATTACKS; i++) {
                    int protect = op->protection[i] + tmp->protection[i];
                    protect = MIN(100, protect);
                    op->protection[i] = protect;
                }

                if (tmp->stats.ac != 0) {
                    op->stats.ac += tmp->stats.ac + tmp->magic;
                }
            }

            if (tmp->block != 0) {
                op->block += tmp->block + tmp->magic;
            }

            if (tmp->absorb != 0) {
                op->absorb += tmp->absorb + tmp->magic;
            }
        }
    }

    if (op->more && QUERY_FLAG(op, FLAG_FRIENDLY)) {
        SET_MULTI_FLAG(op, FLAG_FRIENDLY);
    }

    op->stats.dam = (int16_t) (((double) op->stats.dam *
            ((LEVEL_DAMAGE(op->level) + MAX(LEVEL_DAMAGE(op->level / 3.0) -
            0.75f, 0.0)) * (0.925f + 0.05 * (op->level / 10.0)))) / 10.0f);

    /* Add a special decrease of power for monsters level 1-5 */
    if (op->level <= 5) {
        double d = 1.0f - ((0.35f / 5.0f) * (double) (6 - op->level));

        op->stats.dam = (int16_t) ((double) op->stats.dam * d);

        if (op->stats.dam < 1) {
            op->stats.dam = 1;
        }

        op->stats.maxhp = (int32_t) ((double) op->stats.maxhp * d);

        if (op->stats.maxhp < 1) {
            op->stats.maxhp = 1;
        }

        if (op->stats.hp > op->stats.maxhp) {
            op->stats.hp = op->stats.maxhp;
        }
    }

    set_mobile_speed(op, 0);
}

/**
 * Displays information about the object's updated stats.
 * @param op
 * Object.
 * @param refop
 * Old state of the object.
 * @param refpl
 * Old state of the player.
 * @return
 * Number of stats changed (approximate).
 */
static int living_update_display(object *op, object *refop, player *refpl)
{
    int ret, i, stat_new, stat_old;
    player *pl;

    HARD_ASSERT(op != NULL);
    HARD_ASSERT(refop != NULL);
    HARD_ASSERT(refpl != NULL);

    /* Only players for now. */
    HARD_ASSERT(op->type == PLAYER);

    pl = CONTR(op);

    ret = 0;

    if (MIN(op->attack[ATNR_CONFUSION], 1) !=
            MIN(refop->attack[ATNR_CONFUSION], 1)) {
        ret++;

        if (op->attack[ATNR_CONFUSION] != 0) {
            draw_info(COLOR_WHITE, op, "Your hands begin to glow red.");
        } else {
            draw_info(COLOR_GRAY, op, "Your hands stop glowing red.");
        }
    }

    if (QUERY_FLAG(op, FLAG_LIFESAVE) != QUERY_FLAG(refop, FLAG_LIFESAVE)) {
        ret++;

        if (QUERY_FLAG(op, FLAG_LIFESAVE)) {
            draw_info(COLOR_WHITE, op, "You feel very protected.");
        } else {
            draw_info(COLOR_GRAY, op, "You don't feel protected anymore.");
        }
    }

    if (QUERY_FLAG(op, FLAG_REFL_MISSILE) !=
            QUERY_FLAG(refop, FLAG_REFL_MISSILE)) {
        ret++;

        if (QUERY_FLAG(op, FLAG_REFL_MISSILE)) {
            draw_info(COLOR_WHITE, op, "You feel more safe now, somehow.");
        } else {
            draw_info(COLOR_GRAY, op, "Suddenly you feel less safe, somehow.");
        }
    }

    if (QUERY_FLAG(op, FLAG_REFL_SPELL) !=
            QUERY_FLAG(refop, FLAG_REFL_SPELL)) {
        ret++;

        if (QUERY_FLAG(op, FLAG_REFL_SPELL)) {
            draw_info(COLOR_WHITE, op, "A magic force shimmers around you.");
        } else {
            draw_info(COLOR_GRAY, op, "The magic force fades away.");
        }
    }

    if (QUERY_FLAG(op, FLAG_FLYING) != QUERY_FLAG(refop, FLAG_FLYING)) {
        ret++;

        if (QUERY_FLAG(op, FLAG_FLYING)) {
            draw_info(COLOR_WHITE, op, "You start to float in the air!");
        } else {
            draw_info(COLOR_GRAY, op, "You float down to the ground.");
        }
    }

    /* Becoming UNDEAD... a special treatment for this flag. Only those not
     * originally undead may change their status */
    if (QUERY_FLAG(op, FLAG_UNDEAD) != QUERY_FLAG(refop, FLAG_UNDEAD) &&
            !QUERY_FLAG(&op->arch->clone, FLAG_UNDEAD)) {
        ret++;

        if (QUERY_FLAG(op, FLAG_UNDEAD)) {
            FREE_AND_COPY_HASH(op->race, "undead");
            draw_info(COLOR_GRAY, op, "Your lifeforce drains away!");
        } else {
            FREE_AND_CLEAR_HASH(op->race);

            if (op->arch->clone.race) {
                FREE_AND_COPY_HASH(op->race, op->arch->clone.race);
            }

            draw_info(COLOR_WHITE, op, "Your lifeforce returns!");
        }
    }

    if (QUERY_FLAG(op, FLAG_STEALTH) != QUERY_FLAG(refop, FLAG_STEALTH)) {
        ret++;

        if (QUERY_FLAG(op, FLAG_STEALTH)) {
            draw_info(COLOR_WHITE, op, "You walk quieter.");
        } else {
            draw_info(COLOR_GRAY, op, "You walk noisier.");
        }
    }

    if (QUERY_FLAG(op, FLAG_SEE_INVISIBLE) !=
            QUERY_FLAG(refop, FLAG_SEE_INVISIBLE)) {
        ret++;

        if (QUERY_FLAG(op, FLAG_SEE_INVISIBLE)) {
            draw_info(COLOR_WHITE, op, "You see invisible things.");
        } else {
            draw_info(COLOR_GRAY, op, "Your vision becomes less clear.");
        }
    }

    if (QUERY_FLAG(op, FLAG_IS_INVISIBLE) !=
            QUERY_FLAG(refop, FLAG_IS_INVISIBLE)) {
        ret++;

        if (QUERY_FLAG(op, FLAG_IS_INVISIBLE)) {
            draw_info(COLOR_WHITE, op, "You become transparent.");
        } else {
            draw_info(COLOR_GRAY, op, "You can see yourself.");
        }
    }

    if (QUERY_FLAG(op, FLAG_BLIND) !=
            QUERY_FLAG(refop, FLAG_BLIND)) {
        ret++;

        if (QUERY_FLAG(op, FLAG_BLIND)) {
            draw_info(COLOR_GRAY, op, "You are blinded.");
        } else {
            draw_info(COLOR_WHITE, op, "Your vision returns.");
        }

        pl->update_los = 1;
    }

    if (QUERY_FLAG(op, FLAG_CONFUSED) !=
            QUERY_FLAG(refop, FLAG_CONFUSED)) {
        ret++;

        if (QUERY_FLAG(op, FLAG_CONFUSED)) {
            draw_info(COLOR_GRAY, op, "You suddenly feel very confused!");
        } else {
            draw_info(COLOR_WHITE, op, "You regain your senses.");
        }
    }

    if (QUERY_FLAG(op, FLAG_SEE_IN_DARK) !=
            QUERY_FLAG(refop, FLAG_SEE_IN_DARK)) {
        ret++;

        if (QUERY_FLAG(op, FLAG_SEE_IN_DARK)) {
            draw_info(COLOR_WHITE, op, "Your vision is better in the dark.");
        } else {
            draw_info(COLOR_GRAY, op, "You see less well in the dark.");
        }
    }

    if (QUERY_FLAG(op, FLAG_XRAYS) != QUERY_FLAG(refop, FLAG_XRAYS)) {
        ret++;

        if (QUERY_FLAG(op, FLAG_XRAYS)) {
            draw_info(COLOR_GRAY, op, "Everything becomes transparent.");
        } else {
            draw_info(COLOR_GRAY, op, "Everything suddenly looks very solid.");
        }
    }

    if (op->stats.maxhp != refop->stats.maxhp) {
        ret++;

        if (op->stats.maxhp > refop->stats.maxhp) {
            draw_info(COLOR_WHITE, op, "You feel more healthy!");
        } else {
            draw_info(COLOR_GRAY, op, "You feel much less healthy!");
        }
    }

    if (op->stats.maxsp != refop->stats.maxsp) {
        ret++;

        if (op->stats.maxsp > refop->stats.maxsp) {
            draw_info(COLOR_WHITE, op, "You feel one with the powers of "
                    "magic!");
        } else {
            draw_info(COLOR_GRAY, op, "You suddenly feel more mundane.");
        }
    }

    if (pl->gen_hp != refpl->gen_hp) {
        ret++;

        if (pl->gen_hp > refpl->gen_hp) {
            draw_info(COLOR_WHITE, op, "You feel your health regeneration "
                    "speeding up.");
        } else {
            draw_info(COLOR_GRAY, op, "You feel your health regeneration "
                    "slowing down.");
        }
    }

    if (pl->gen_sp != refpl->gen_sp) {
        ret++;

        if (pl->gen_sp > refpl->gen_sp) {
            draw_info(COLOR_WHITE, op, "You feel your mana regeneration "
                    "speeding up.");
        } else {
            draw_info(COLOR_GRAY, op, "You feel your mana regeneration "
                    "slowing down.");
        }
    }

    if (pl->gen_sp_armour != refpl->gen_sp_armour) {
        ret++;

        if (pl->gen_sp_armour > refpl->gen_sp_armour) {
            draw_info(COLOR_GRAY, op, "You feel the weight of your armour "
                    "impairing your ability to concentrate on magic.");
        } else {
            draw_info(COLOR_WHITE, op, "You feel able to concentrate clearer.");
        }
    }

    if (pl->digestion != refpl->digestion) {
        ret++;

        if (pl->digestion > refpl->digestion) {
            draw_info(COLOR_WHITE, op, "You feel your digestion slowing down.");
        } else {
            draw_info(COLOR_GRAY, op, "You feel your digestion speeding up.");
        }
    }

    /* Messages for changed protections */
    for (i = 0; i < NROFATTACKS; i++) {
        if (op->protection[i] != refop->protection[i]) {
            ret++;

            if (op->protection[i] > refop->protection[i]) {
                draw_info_format(COLOR_GREEN, op, "Your protection to %s rises "
                        "to %d%%.", attack_name[i], op->protection[i]);
            } else {
                draw_info_format(COLOR_BLUE, op, "Your protection to %s drops "
                        "to %d%%.", attack_name[i], op->protection[i]);
            }
        }
    }

    /* Messages for changed attuned/repelled/denied paths. */
    if (op->path_attuned != refop->path_attuned ||
            op->path_repelled != refop->path_repelled ||
            op->path_denied != refop->path_denied) {
        uint32_t path;

        for (i = 0; i < PATH_NUM; i++) {
            path = 1U << i;

            if ((op->path_attuned & path) && !(refop->path_attuned & path)) {
                draw_info_format(COLOR_WHITE, op, "You feel your magical "
                        "powers attuned towards the path of %s.",
                        spellpathnames[i]);
            } else if ((refop->path_attuned & path) &&
                    !(op->path_attuned & path)) {
                draw_info_format(COLOR_GRAY, op, "You no longer feel your "
                        "magical powers attuned towards the path of %s.",
                        spellpathnames[i]);
            }

            if ((op->path_repelled & path) && !(refop->path_repelled & path)) {
                draw_info_format(COLOR_WHITE, op, "You feel your magical "
                        "powers repelled from the path of %s.",
                        spellpathnames[i]);
            } else if ((refop->path_repelled & path) &&
                    !(op->path_repelled & path)) {
                draw_info_format(COLOR_GRAY, op, "You no longer feel your "
                        "magical powers repelled from the path of %s.",
                        spellpathnames[i]);
            }

            if ((op->path_denied & path) && !(refop->path_denied & path)) {
                draw_info_format(COLOR_WHITE, op, "You feel your magical "
                        "powers shift, and become unable to cast spells from "
                        "the path of %s.", spellpathnames[i]);
            } else if ((refop->path_denied & path) &&
                    !(op->path_denied & path)) {
                draw_info_format(COLOR_GRAY, op, "You feel your magical powers "
                        "shift, and become able to cast spells from the path "
                        "of %s.", spellpathnames[i]);
            }
        }
    }

    for (i = 0; i < NUM_STATS; i++) {
        stat_new = get_attr_value(&op->stats, i);
        stat_old = get_attr_value(&refop->stats, i);

        if (stat_new != stat_old) {
            ret++;

            if (stat_new > stat_old) {
                draw_info(COLOR_WHITE, op, gain_msg[i]);
            } else {
                draw_info(COLOR_GRAY, op, lose_msg[i]);
            }
        }
    }

    if (pl->item_power != refpl->item_power &&
        (pl->item_power > settings.item_power_factor * op->level ||
         refpl->item_power > settings.item_power_factor * op->level)) {
        if (pl->item_power > refpl->item_power) {
            draw_info(COLOR_GRAY, op, "You feel the combined power of your "
                                      "equipped items begin gnawing on your "
                                      "soul!");
        } else if (pl->item_power > settings.item_power_factor * op->level) {
            draw_info(COLOR_GRAY, op, "You feel your soul return closer to "
                                      "normal.");
        } else {
            draw_info(COLOR_WHITE, op, "You feel your soul return to normal.");
        }
    }

    return ret;
}

/**
 * Updates all abilities given by applied objects in the inventory of the given
 * object.
 * @param op
 * Object to update.
 * @return
 * Approximate number of changed stats.
 */
int living_update(object *op)
{
    HARD_ASSERT(op != NULL);

    op = HEAD(op);

    if (QUERY_FLAG(op, FLAG_NO_FIX_PLAYER)) {
        return 0;
    }

    if (op->type == PLAYER) {
        object refop;
        player refpl, *pl;

        pl = CONTR(op);

        /* Remember both the old state of the object and the player. */
        memcpy(&refop, op, sizeof(refop));
        memcpy(&refpl, pl, sizeof(refpl));

        /* Update player. */
        living_update_player(op);

        return living_update_display(op, &refop, &refpl);
    }

    if (QUERY_FLAG(op, FLAG_MONSTER)) {
        /* Update monster. */
        living_update_monster(op);
        return 0;
    }

    LOG(DEVEL, "Updating unhandled object: %s", object_get_str(op));
    return 0;
}

/**
 * Find the base info object in the specified monster.
 * @param op
 * Monster object. Must not be NULL, cannot be a tail part and must
 * be of type MONSTER.
 * @return
 * Pointer to the base info if found, NULL otherwise.
 */
object *living_find_base_info(object *op)
{
    HARD_ASSERT(op != NULL);

    SOFT_ASSERT_RC(op->head == NULL, NULL, "Called on non-head part: %s",
            object_get_str(op));
    SOFT_ASSERT_RC(op->type == MONSTER, NULL, "Object is not a monster: %s",
            object_get_str(op));

    for (object *tmp = op->inv; tmp != NULL; tmp = tmp->below) {
        if (tmp->type == BASE_INFO) {
            return tmp;
        }
    }

    return NULL;
}

/**
 * Acquire the base info object of the specified monster object. If the base
 * info doesn't exist yet, it is created automatically.
 * @param op
 * Monster. Must not be NULL, cannot be a tail part and must be of
 * type MONSTER.
 * @return
 * Pointer to the base info object, NULL on failure.
 */
object *living_get_base_info(object *op)
{
    HARD_ASSERT(op != NULL);

    SOFT_ASSERT_RC(op->head == NULL, NULL, "Called on non-head part: %s",
            object_get_str(op));
    SOFT_ASSERT_RC(op->type == MONSTER, NULL, "Object is not a monster: %s",
            object_get_str(op));

    object *tmp = living_find_base_info(op);
    if (tmp != NULL) {
        return tmp;
    }

    tmp = object_get();
    tmp->arch = op->arch;
    /* Copy without putting it on active list */
    object_copy(tmp, op, true);
    tmp->type = BASE_INFO;
    tmp->speed_left = tmp->speed;
    /* Ensure this object will not be active in any way */
    tmp->speed = 0.0f;
    tmp->face = arches[ARCH_BASE_INFO]->clone.face;
    SET_FLAG(tmp, FLAG_NO_DROP);
    CLEAR_FLAG(tmp, FLAG_ANIMATE);
    CLEAR_FLAG(tmp, FLAG_FRIENDLY);
    CLEAR_FLAG(tmp, FLAG_MONSTER);
    /* And put it in the monster. */
    tmp = object_insert_into(tmp, op, 0);

    /* Store position (for returning home after aggro is lost). */
    object *env = object_get_env(op);
    if (env->map != NULL) {
        tmp->x = env->x;
        tmp->y = env->y;
        FREE_AND_ADD_REF_HASH(tmp->slaying, env->map->path);
    }

    return tmp;
}

/**
 * Set the movement speed of a monster.
 * 1/5 = mob is slowed (by magic)
 * 2/5 = normal mob speed - moving normal
 * 3/5 = mob is moving fast
 * 4/5 = mov is running/attack speed
 * 5/5 = mob is hasted and moving full speed
 * @param op
 * Monster.
 * @param idx
 * Index.
 */
void set_mobile_speed(object *op, int idx)
{
    object *base;
    double speed, tmp;

    base = living_get_base_info(op);

    speed = base->speed_left;
    tmp = op->speed;

    if (idx) {
        op->speed = speed * idx;
    } else {
        /* We will generate the speed by setting of the monster */

        /* If not slowed... */
        if (!QUERY_FLAG(op, FLAG_SLOW_MOVE)) {
            speed += base->speed_left;
        }

        /* Valid enemy - monster is fighting! */
        if (OBJECT_VALID(op->enemy, op->enemy_count)) {
            speed += base->speed_left * 2;
        }

        op->speed = speed;
    }

    /* Update speed if needed */
    if (DBL_EQUAL(tmp, 0.0) != DBL_EQUAL(op->speed, 0.0)) {
        object_update_speed(op);
    }
}
