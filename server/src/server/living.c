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
 * only living things. */

#include <global.h>

/** When we carry more than this of our weight_limit, we get encumbered. */
#define ENCUMBRANCE_LIMIT 65.0f

/**
 * dam_bonus, thaco_bonus, weight limit all are based on strength. */
int dam_bonus[MAX_STAT + 1] = {
    -5, -4, -4, -3, -3, -3, -2, -2, -2, -1, -1,
    0, 0, 0, 0, 0,
    1, 1, 2, 2, 3, 3, 3, 4, 4, 5, 6, 7, 8, 10, 12
};

/** THAC0 bonus - WC bonus */
int thaco_bonus[MAX_STAT + 1] = {
    -5, -4, -4, -3, -3, -3, -2, -2, -2, -1, -1,
    0, 0, 0, 0, 0,
    1, 1, 2, 2, 3, 3, 3, 4, 4, 5, 5, 5, 6, 7, 8
};

/**
 * Constitution bonus. */
static float con_bonus[MAX_STAT + 1] = {
    -0.8f, -0.6f, -0.5f, -0.4f, -0.35f, -0.3f, -0.25f, -0.2f, -0.15f, -0.11f, -0.07f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.1f, 0.15f, 0.2f, 0.25f, 0.3f, 0.35f, 0.4f, 0.45f, 0.5f, 0.55f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f
};

/**
 * Power bonus. */
static float pow_bonus[MAX_STAT + 1] = {
    -0.8f, -0.6f, -0.5f, -0.4f, -0.35f, -0.3f, -0.25f, -0.2f, -0.15f, -0.11f, -0.07f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.4f, 1.6f, 1.8f, 2.0f
};

/**
 * Speed bonus. Uses dexterity as its stat. */
float speed_bonus[MAX_STAT + 1] = {
    -0.4f, -0.4f, -0.3f, -0.3f, -0.2f,
    -0.2f, -0.2f, -0.1f, -0.1f, -0.1f,
    -0.05f, 0.0, 0.0f, 0.0f, 0.025f, 0.05f,
    0.075f, 0.1f, 0.125f, 0.15f, 0.175f, 0.2f,
    0.225f, 0.25f, 0.275f, 0.3f,
    0.325f, 0.35f, 0.4f, 0.45f, 0.5f
};

/**
 * The absolute most a character can carry - a character can't pick stuff
 * up if it would put him above this limit.
 *
 * Value is in grams, so we don't need to do conversion later
 *
 * These limits are probably overly generous, but being there were no
 * values before, you need to start someplace. */
uint32 weight_limit[MAX_STAT + 1] = {
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
 * wisdom. */
int learn_spell[MAX_STAT + 1] = {
    0, 0, 0, 1, 2, 4, 8, 12, 16, 25, 36, 45, 55, 65, 70, 75, 80, 85, 90, 95, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 100, 100
};

/**
 * Probability to avoid something. */
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
    "You suddenly begin to lose your memory!",
    "Your face gets distorted!",
    "Watch out, your mind is going!",
    "Your spirit feels drained!"
};

/** Message when a player has a stat restored. */
const char *const restore_msg[NUM_STATS] = {
    "You feel your strength return.",
    "You feel your agility return.",
    "You feel your health return.",
    "You feel your wisdom return.",
    "You feel your charisma return.",
    "You feel your memory return.",
    "You feel your spirits return."
};

/** Message when a player increases a stat. */
static const char *const gain_msg[NUM_STATS] = {
    "You feel stronger.",
    "You feel more agile.",
    "You feel healthy.",
    "You feel wiser.",
    "You seem to look better.",
    "You feel smarter.",
    "You feel more potent."
};

/** Message when a player decreases a stat. */
const char *const lose_msg[NUM_STATS] = {
    "You feel weaker!",
    "You feel clumsy!",
    "You feel less healthy!",
    "You lose some of your memory!",
    "You look ugly!",
    "You feel stupid!",
    "You feel less potent!"
};

/** Names of stats. */
const char *const statname[NUM_STATS] = {
    "strength",
    "dexterity",
    "constitution",
    "wisdom",
    "charisma",
    "intelligence",
    "power"
};

/** Short names of stats. */
const char *const short_stat_name[NUM_STATS] = {
    "Str",
    "Dex",
    "Con",
    "Wis",
    "Cha",
    "Int",
    "Pow"
};

/**
 * Sets Str/Dex/con/Wis/Cha/Int/Pow in stats to value, depending on what
 * attr is (STR to POW).
 * @param stats Item to modify. Must not be NULL.
 * @param attr Attribute to change.
 * @param value New value. */
void set_attr_value(living *stats, int attr, sint8 value)
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

    case WIS:
        stats->Wis = value;
        break;

    case POW:
        stats->Pow = value;
        break;

    case CHA:
        stats->Cha = value;
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
 * @param stats Item to modify. Must not be NULL.
 * @param attr Attribute to change.
 * @param value Delta (can be positive). */
void change_attr_value(living *stats, int attr, sint8 value)
{
    sint16 result;

    if (value == 0) {
        return;
    }

    result = get_attr_value(stats, attr) + value;

    /* Prevent possible overflow of the stat. */
    if (result > SINT8_MAX || result < SINT8_MIN) {
        return;
    }

    set_attr_value(stats, attr, result);
}

/**
 * Gets the value of a stat.
 * @param stats Item from which to get stat.
 * @param attr Attribute to get.
 * @return Specified attribute, 0 if not found.
 * @see set_attr_value(). */
sint8 get_attr_value(living *stats, int attr)
{
    switch (attr) {
    case STR:
        return stats->Str;

    case DEX:
        return stats->Dex;

    case CON:
        return stats->Con;

    case WIS:
        return stats->Wis;

    case CHA:
        return stats->Cha;

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
 * @param stats Attributes to check. */
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
 * @param op Object to drain. */
void drain_stat(object *op)
{
    drain_specific_stat(op, rndm(1, NUM_STATS) - 1);
}

/**
 * Drain a specified stat from op.
 * @param op Victim to drain.
 * @param deplete_stats Statistic to drain. */
void drain_specific_stat(object *op, int deplete_stats)
{
    object *tmp;
    archetype *at = find_archetype("depletion");

    if (!at) {
        logger_print(LOG(BUG), "Couldn't find archetype depletion.");
        return;
    } else {
        tmp = present_arch_in_ob(at, op);

        if (!tmp) {
            tmp = arch_to_object(at);
            tmp = insert_ob_in_ob(tmp, op);
            SET_FLAG(tmp, FLAG_APPLIED);
        }
    }

    draw_info(COLOR_WHITE, op, drain_msg[deplete_stats]);
    change_attr_value(&tmp->stats, deplete_stats, -1);
    living_update(op);
}

static void living_apply_flags(object *op, object *tmp)
{
    op->path_attuned |= tmp->path_attuned;
    op->path_repelled |= tmp->path_repelled;
    op->path_denied |= tmp->path_denied;

    op->terrain_flag |= tmp->terrain_type;

    if (QUERY_FLAG(tmp, FLAG_LIFESAVE)) {
        SET_FLAG(op, FLAG_LIFESAVE);
    }

    if (QUERY_FLAG(tmp, FLAG_REFL_SPELL)) {
        SET_FLAG(op, FLAG_REFL_SPELL);
    }

    if (QUERY_FLAG(tmp, FLAG_REFL_MISSILE)) {
        SET_FLAG(op, FLAG_REFL_MISSILE);
    }

    if (QUERY_FLAG(tmp, FLAG_STEALTH)) {
        SET_FLAG(op, FLAG_STEALTH);
    }

    if (QUERY_FLAG(tmp, FLAG_XRAYS)) {
        SET_FLAG(op, FLAG_XRAYS);

        if (op->type == PLAYER) {
            CONTR(op)->update_los = 1;
        }
    }

    if (QUERY_FLAG(tmp, FLAG_BLIND)) {
        SET_FLAG(op, FLAG_BLIND);

        if (op->type == PLAYER) {
            CONTR(op)->update_los = 1;
        }
    }

    if (QUERY_FLAG(tmp, FLAG_SEE_IN_DARK)) {
        SET_FLAG(op, FLAG_SEE_IN_DARK);
    }

    if (QUERY_FLAG(tmp, FLAG_CAN_PASS_THRU)) {
        SET_FLAG(op, FLAG_CAN_PASS_THRU);
    }

    if (QUERY_FLAG(tmp, FLAG_MAKE_ETHEREAL)) {
        SET_FLAG(op, FLAG_CAN_PASS_THRU);
        SET_FLAG(op, FLAG_IS_ETHEREAL);
    }

    if (QUERY_FLAG(tmp, FLAG_MAKE_INVISIBLE)) {
        SET_FLAG(op, FLAG_IS_INVISIBLE);

        if (op->type == PLAYER) {
            CONTR(op)->socket.ext_title_flag = 1;
        }
    }

    if (QUERY_FLAG(tmp, FLAG_SEE_INVISIBLE)) {
        SET_FLAG(op, FLAG_SEE_INVISIBLE);
    }

    if (QUERY_FLAG(tmp, FLAG_FLYING)) {
        SET_FLAG(op, FLAG_FLYING);
    }

    if (QUERY_FLAG(tmp, FLAG_UNDEAD)) {
        SET_FLAG(op, FLAG_UNDEAD);
    }
}

/**
 * Updates player object based on the applied items in their inventory.
 * @param op Player to update.
 */
void living_update_player(object *op)
{
    int ring_count = 0;
    int old_glow, max_boni_hp = 0, max_boni_sp = 0;
    int i, j, light;
    int protect_boni[NROFATTACKS], protect_mali[NROFATTACKS],
            protect_exact_boni[NROFATTACKS], protect_exact_mali[NROFATTACKS];
    int potion_protection_bonus[NROFATTACKS],
            potion_protection_malus[NROFATTACKS], potion_attack[NROFATTACKS];
    object *tmp;
    float max = 9, added_speed = 0, bonus_speed = 0,
            speed_reduce_from_disease = 1;
    player *pl;

    HARD_ASSERT(op != NULL);

    op = HEAD(op);

    SOFT_ASSERT(op->type == PLAYER, "Called with non-player: %s",
            object_get_str(op));

    if (QUERY_FLAG(op, FLAG_NO_FIX_PLAYER)) {
        return;
    }

    pl = CONTR(op);

    pl->digestion = 3;
    pl->gen_hp = 1;
    pl->gen_sp = 1;
    pl->gen_sp_armour = 0;
    pl->item_power = 0;

    for (i = 0; i < NUM_STATS; i++) {
        set_attr_value(&op->stats, i,
                get_attr_value(&op->arch->clone.stats, i));
    }

    op->stats.wc = op->arch->clone.stats.wc;
    op->stats.ac = op->arch->clone.stats.ac;
    op->stats.dam = op->arch->clone.stats.dam;

    op->stats.maxhp = op->arch->clone.stats.maxhp;
    op->stats.maxsp = op->arch->clone.stats.maxsp;

    op->stats.wc_range = op->arch->clone.stats.wc_range;

    old_glow = op->glow_radius;
    light = op->arch->clone.glow_radius;

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

    memset(&protect_boni, 0, sizeof(protect_boni));
    memset(&protect_mali, 0, sizeof(protect_mali));
    memset(&protect_exact_boni, 0, sizeof(protect_exact_boni));
    memset(&protect_exact_mali, 0, sizeof(protect_exact_mali));
    memset(&potion_protection_bonus, 0, sizeof(potion_protection_bonus));
    memset(&potion_protection_malus, 0, sizeof(potion_protection_malus));
    memset(&potion_attack, 0, sizeof(potion_attack));

    /* Initializing player arrays from the values in player archetype clone */
    memset(&pl->equipment, 0, sizeof(pl->equipment));
    memset(&pl->skill_ptr, 0, sizeof(pl->skill_ptr));
    memcpy(&op->protection, &op->arch->clone.protection,
            sizeof(op->protection));
    memcpy(&op->attack, &op->arch->clone.attack, sizeof(op->attack));

    for (tmp = op->inv; tmp; tmp = tmp->below) {
        if (tmp->type == QUEST_CONTAINER) {
            pl->quest_container = tmp;
            continue;
        }

        if (tmp->type == SCROLL || tmp->type == POTION ||
                (tmp->type == CONTAINER && !OBJECT_IS_AMMO(tmp)) ||
                tmp->type == LIGHT_REFILL) {
            continue;
        }

        if (tmp->glow_radius > light) {
            if (tmp->type != LIGHT_APPLY || QUERY_FLAG(tmp, FLAG_APPLIED)) {
                light = tmp->glow_radius;
            }
        }

        if (tmp->type == SKILL) {
            pl->skill_ptr[tmp->stats.sp] = tmp;
        }

        if (QUERY_FLAG(tmp, FLAG_APPLIED)) {
            if (tmp->type == POTION_EFFECT) {
                for (i = 0; i < NUM_STATS; i++) {
                    change_attr_value(&op->stats, i,
                            get_attr_value(&tmp->stats, i));
                }

                for (i = 0; i < NROFATTACKS; i++) {
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
            } else if (tmp->type == CLASS || tmp->type == FORCE ||
                    tmp->type == POISONING || tmp->type == DISEASE ||
                    tmp->type == SYMPTOM) {
                if (tmp->type == CLASS) {
                    pl->class_ob = tmp;
                }

                if (ARMOUR_SPEED(tmp) &&
                        (float) ARMOUR_SPEED(tmp) / 10.0f < max) {
                    max = ARMOUR_SPEED(tmp) / 10.0f;
                }

                for (i = 0; i < NUM_STATS; i++) {
                    change_attr_value(&op->stats, i,
                            get_attr_value(&tmp->stats, i));
                }

                if (tmp->type != DISEASE && tmp->type != SYMPTOM &&
                        tmp->type != POISONING) {
                    if (tmp->stats.wc) {
                        op->stats.wc += tmp->stats.wc + tmp->magic;
                    }

                    if (tmp->stats.dam) {
                        op->stats.dam += tmp->stats.dam + tmp->magic;
                    }

                    if (tmp->stats.ac) {
                        op->stats.ac += (tmp->stats.ac + tmp->magic);
                    }

                    op->stats.maxhp += tmp->stats.maxhp;
                    op->stats.maxsp += tmp->stats.maxsp;
                }

                if (tmp->type == DISEASE || tmp->type == SYMPTOM) {
                    speed_reduce_from_disease = (float) tmp->last_sp / 100.0f;

                    if (speed_reduce_from_disease == 0.0f) {
                        speed_reduce_from_disease = 1.0f;
                    }
                }

                for (i = 0; i < NROFATTACKS; i++) {
                    if (tmp->protection[i] > protect_exact_boni[i]) {
                        protect_exact_boni[i] = tmp->protection[i];
                    } else if (tmp->protection[i] < 0) {
                        protect_exact_mali[i] += (-tmp->protection[i]);
                    }

                    if (tmp->type != DISEASE && tmp->type != SYMPTOM &&
                            tmp->type != POISONING) {
                        if (tmp->attack[i] > 0) {
                            op->attack[i] = MIN(UINT8_MAX, op->attack[i] +
                                    tmp->attack[i]);
                        }
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
            } else if (tmp->type == RING && ring_count == 0) {
                pl->equipment[PLAYER_EQUIP_RING_RIGHT] = tmp;
                ring_count++;
            } else if (tmp->type == HELMET) {
                pl->equipment[PLAYER_EQUIP_HELM] = tmp;
            } else if (tmp->type == ARMOUR) {
                pl->equipment[PLAYER_EQUIP_ARMOUR] = tmp;
            } else if (tmp->type == GIRDLE) {
                pl->equipment[PLAYER_EQUIP_BELT] = tmp;
            } else if (tmp->type == GREAVES) {
                pl->equipment[PLAYER_EQUIP_GREAVES] = tmp;
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
            } else if (tmp->type == RING && ring_count == 1) {
                pl->equipment[PLAYER_EQUIP_RING_LEFT] = tmp;
                ring_count++;
            } else {
                logger_print(LOG(BUG), "Unexpected applied object: %s",
                        object_get_str(tmp));
                CLEAR_FLAG(tmp, FLAG_APPLIED);
            }
        }
    }

    for (i = 0; i < PLAYER_EQUIP_MAX; i++) {
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

        if (i == PLAYER_EQUIP_AMMO || i == PLAYER_EQUIP_LIGHT) {
            continue;
        }

        /* Used for ALL armours except rings and amulets */
        if (IS_ARMOR(pl->equipment[i]) && ARMOUR_SPEED(pl->equipment[i]) &&
                (float) ARMOUR_SPEED(pl->equipment[i]) / 10.0f < max) {
            max = ARMOUR_SPEED(pl->equipment[i]) / 10.0f;
        }

        if (pl->equipment[i]->stats.ac) {
            op->stats.ac += pl->equipment[i]->stats.ac +
                    pl->equipment[i]->magic;
        }

        if (!OBJECT_IS_RANGED(pl->equipment[i])) {
            if (pl->equipment[i]->stats.wc) {
                op->stats.wc += pl->equipment[i]->stats.wc +
                        pl->equipment[i]->magic;
            }

            if (pl->equipment[i]->stats.dam) {
                op->stats.dam += pl->equipment[i]->stats.dam +
                        pl->equipment[i]->magic;
            }

            for (j = 0; j < NROFATTACKS; j++) {
                if (pl->equipment[i]->protection[j] > 0) {
                    protect_boni[j] += ((100 - protect_boni[j]) * (int)
                            ((float) pl->equipment[i]->protection[j] *
                            ((float) pl->equipment[i]->item_condition /
                            100.0f))) / 100;
                } else if (pl->equipment[i]->protection[j] < 0) {
                    protect_mali[j] += ((100 - protect_mali[j]) *
                            (-pl->equipment[i]->protection[j])) / 100;
                }

                if (pl->equipment[i]->attack[j] > 0) {
                    op->attack[j] = MIN(UINT8_MAX, op->attack[j] + (int)
                            ((float) pl->equipment[i]->attack[j] * ((float)
                            pl->equipment[i]->item_condition / 100.0f)));
                }
            }

            if (pl->equipment[i]->stats.exp &&
                    pl->equipment[i]->type != SKILL) {
                if (pl->equipment[i]->stats.exp > 0) {
                    added_speed += (float) pl->equipment[i]->stats.exp / 3.0f;
                    bonus_speed += 1.0f + (float) pl->equipment[i]->stats.exp /
                            3.0f;
                } else {
                    added_speed += (float) pl->equipment[i]->stats.exp;
                }
            }
        }

        if (!IS_WEAPON(pl->equipment[i]) &&
                !OBJECT_IS_RANGED(pl->equipment[i])) {
            max_boni_hp += pl->equipment[i]->stats.maxhp;
            max_boni_sp += pl->equipment[i]->stats.maxsp;

            pl->digestion += pl->equipment[i]->stats.food;
            pl->gen_sp += pl->equipment[i]->stats.sp;
            pl->gen_hp += pl->equipment[i]->stats.hp;
            pl->gen_sp_armour += pl->equipment[i]->last_heal;
        }

        pl->item_power += pl->equipment[i]->item_power;

        for (j = 0; j < NUM_STATS; j++) {
            change_attr_value(&op->stats, j,
                    get_attr_value(&pl->equipment[i]->stats, j));
        }

        living_apply_flags(op, pl->equipment[i]);
    }

    for (i = 0; i < NROFATTACKS; i++) {
        if (potion_attack[i]) {
            op->attack[i] = MIN(UINT8_MAX, op->attack[i] + potion_attack[i]);
        }
    }

    for (i = 0; i < NROFATTACKS; i++) {
        int ptemp;

        /* Add in the potion protections. */
        if (potion_protection_bonus[i] > 0) {
            protect_boni[i] += ((100 - protect_boni[i]) *
                    potion_protection_bonus[i]) / 100;
        }

        if (potion_protection_malus[i] < 0) {
            protect_mali[i] += ((100 - protect_mali[i]) *
                    (-potion_protection_malus[i])) / 100;
        }

        ptemp = protect_boni[i] + protect_exact_boni[i] - protect_mali[i] -
                protect_exact_mali[i];
        op->protection[i] = MIN(100, MAX(-100, ptemp));
    }

    check_stat_bounds(&op->stats);

    /* Now the speed thing... */
    op->speed += speed_bonus[op->stats.Dex];

    if (added_speed >= 0) {
        op->speed += added_speed / 10.0f;
    } else {
        op->speed /= 1.0f - added_speed;
    }

    if (op->speed > max) {
        op->speed = max;
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
        double f = ((double) weight_limit[op->stats.Str] / 100.0f) *
        ENCUMBRANCE_LIMIT;

        if (((sint32) f) <= op->carrying) {
            if (op->carrying >= (sint32) weight_limit[op->stats.Str]) {
                op->speed = 0.01f;
            } else {
                /* Total encumbrance weight part */
                f = ((double) weight_limit[op->stats.Str] - f);
                /* Value from 0.0 to 1.0 encumbrance */
                f = ((double) weight_limit[op->stats.Str] - op->carrying) / f;

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

    update_ob_speed(op);

    if (QUERY_FLAG(op, FLAG_IS_INVISIBLE)) {
        light = 0;
    }

    op->glow_radius = light;

    if (op->map && old_glow != light) {
        adjust_light_source(op->map, op->x, op->y, light - old_glow);
    }

    op->stats.ac += op->level;

    op->stats.maxhp *= op->level + 3;
    op->stats.maxsp *= pl->skill_ptr[SK_WIZARDRY_SPELLS] != NULL ?
        pl->skill_ptr[SK_WIZARDRY_SPELLS]->level : 1 + 3;

    /* Now adjust with the % of the stats mali/boni. */
    op->stats.maxhp += (int) ((float) op->stats.maxhp *
            con_bonus[op->stats.Con]) + max_boni_hp;
    op->stats.maxsp += (int) ((float) op->stats.maxsp *
            pow_bonus[op->stats.Pow]) + max_boni_sp;

    /* HP/SP adjustments coming from class-defining object. */
    if (CONTR(op)->class_ob) {
        if (CONTR(op)->class_ob->stats.hp) {
            op->stats.maxhp += ((float) op->stats.maxhp / 100.0f) *
                    (float) CONTR(op)->class_ob->stats.hp;
        }

        if (CONTR(op)->class_ob->stats.sp) {
            op->stats.maxsp += ((float) op->stats.maxsp / 100.0f) *
                    (float) CONTR(op)->class_ob->stats.sp;
        }
    }

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

    if (pl->equipment[PLAYER_EQUIP_WEAPON] != NULL &&
            pl->equipment[PLAYER_EQUIP_WEAPON]->type == WEAPON &&
            pl->equipment[PLAYER_EQUIP_WEAPON]->item_skill) {
        op->weapon_speed = pl->equipment[PLAYER_EQUIP_WEAPON]->last_grace;
        op->stats.wc += SKILL_LEVEL(pl,
                pl->equipment[PLAYER_EQUIP_WEAPON]->item_skill - 1);
        op->stats.dam = (float) op->stats.dam * LEVEL_DAMAGE(SKILL_LEVEL(pl,
                pl->equipment[PLAYER_EQUIP_WEAPON]->item_skill - 1));
        op->stats.dam *= (float)
                (pl->equipment[PLAYER_EQUIP_WEAPON]->item_condition) / 100.0f;
    } else {
        if (pl->skill_ptr[SK_UNARMED]) {
            op->weapon_speed = pl->skill_ptr[SK_UNARMED]->last_grace;

            for (i = 0; i < NROFATTACKS; i++) {
                if (pl->skill_ptr[SK_UNARMED]->attack[i]) {
                    op->attack[i] = MIN(UINT8_MAX, op->attack[i] +
                            pl->skill_ptr[SK_UNARMED]->attack[i]);
                }
            }
        }

        op->stats.wc += SKILL_LEVEL(pl, SK_UNARMED);
        op->stats.dam = (float) op->stats.dam * LEVEL_DAMAGE(SKILL_LEVEL(pl,
                SK_UNARMED)) / 2;
    }

    /* Now the last adds - stat bonus to damage and WC */
    op->stats.dam += dam_bonus[op->stats.Str];

    if (op->stats.dam < 0) {
        op->stats.dam = 0;
    }

    op->stats.wc += thaco_bonus[op->stats.Dex];

    if (pl->quest_container == NULL) {
        object *quest_container = get_archetype(QUEST_CONTAINER_ARCHETYPE);

        logger_print(LOG(BUG), "Player %s had no quest container, fixing.",
                op->name);
        insert_ob_in_ob(quest_container, op);
        pl->quest_container = quest_container;
    }
}

/**
 * Like living_update_player(), but for monsters.
 * @param op The monster.
 */
void living_update_monster(object *op)
{
    object *base, *tmp;
    float tmp_add;

    HARD_ASSERT(op != NULL);

    op = HEAD(op);

    SOFT_ASSERT(op->type == MONSTER, "Called with non-monster: %s",
            object_get_str(op));

    if (QUERY_FLAG(op, FLAG_NO_FIX_PLAYER)) {
        return;
    }

    /* Will insert or/and return base info */
    base = insert_base_info_object(op);

    CLEAR_FLAG(op, FLAG_READY_BOW);

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
    /* + level / 4 to catch up the equipment improvements of
     * the players in armour items. */
    op->stats.wc = base->stats.wc + op->level + (op->level / 4);
    op->stats.dam = base->stats.dam;

    if (base->stats.wc_range) {
        op->stats.wc_range = base->stats.wc_range;
    } else {
        /* Default value if not set in arch */
        op->stats.wc_range = 20;
    }

    for (tmp = op->inv; tmp; tmp = tmp->below) {
        /* Check for bow and use it! */
        if (tmp->type == BOW) {
            if (QUERY_FLAG(op, FLAG_USE_BOW)) {
                SET_FLAG(tmp, FLAG_APPLIED);
                SET_FLAG(op, FLAG_READY_BOW);
            } else {
                CLEAR_FLAG(tmp, FLAG_APPLIED);
            }
        } else if (QUERY_FLAG(op, FLAG_USE_ARMOUR) && IS_ARMOR(tmp) &&
                check_good_armour(op, tmp)) {
            SET_FLAG(tmp, FLAG_APPLIED);
        } else if (QUERY_FLAG(op, FLAG_USE_WEAPON) && tmp->type == WEAPON &&
                check_good_weapon(op, tmp)) {
            SET_FLAG(tmp, FLAG_APPLIED);
        }

        if (QUERY_FLAG(tmp, FLAG_APPLIED)) {
            int i;

            if (tmp->type == WEAPON) {
                op->stats.dam += tmp->stats.dam;
                op->stats.wc += tmp->stats.wc;
            } else if (IS_ARMOR(tmp)) {
                for (i = 0; i < NROFATTACKS; i++) {
                    op->protection[i] = MIN(op->protection[i] +
                            tmp->protection[i], 15);
                }

                op->stats.ac += tmp->stats.ac;
            }
        }
    }

    if ((tmp_add = LEVEL_DAMAGE(op->level / 3) - 0.75f) < 0) {
        tmp_add = 0;
    }

    if (op->more && QUERY_FLAG(op, FLAG_FRIENDLY)) {
        SET_MULTI_FLAG(op, FLAG_FRIENDLY);
    }

    op->stats.dam = (sint16) (((float) op->stats.dam *
            ((LEVEL_DAMAGE(op->level < 0 ? 0 : op->level) + tmp_add) *
            (0.925f + 0.05 * (op->level / 10)))) / 10.0f);

    /* Add a special decrease of power for monsters level 1-5 */
    if (op->level <= 5) {
        float d = 1.0f - ((0.35f / 5.0f) * (float) (6 - op->level));

        op->stats.dam = (int) ((float) op->stats.dam * d);

        if (op->stats.dam < 1) {
            op->stats.dam = 1;
        }

        op->stats.maxhp = (int) ((float) op->stats.maxhp * d);

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
 * @param op Object.
 * @param refop Old state of the object.
 * @param refpl Old state of the player.
 * @return Number of stats changed (approximate).
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
        uint32 path;

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

    return ret;
}

/**
 * Updates all abilities given by applied objects in the inventory of the given
 * object.
 * @param op Object to update.
 * @return Approximate number of changed stats.
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

    log(LOG(DEVEL), "Updating unhandled object: %s", object_get_str(op));
    return 0;
}

/**
 * Insert and initialize base info object in object op.
 * @param op Object.
 * @return Pointer to the inserted base info object. */
object *insert_base_info_object(object *op)
{
    object *tmp, *head = op, *env;

    if (op->head) {
        head = op->head;
    }

    if (op->type == PLAYER) {
        logger_print(LOG(BUG), "Try to inserting base_info in player %s!", query_name(head, NULL));
        return NULL;
    }

    if ((tmp = find_base_info_object(head))) {
        return tmp;
    }

    tmp = get_object();
    tmp->arch = op->arch;
    /* Copy without putting it on active list */
    copy_object(head, tmp, 1);
    tmp->type = BASE_INFO;
    tmp->speed_left = tmp->speed;
    /* Ensure this object will not be active in any way */
    tmp->speed = 0.0f;
    tmp->face = base_info_archetype->clone.face;
    SET_FLAG(tmp, FLAG_NO_DROP);
    CLEAR_FLAG(tmp, FLAG_ANIMATE);
    CLEAR_FLAG(tmp, FLAG_FRIENDLY);
    CLEAR_FLAG(tmp, FLAG_MONSTER);
    /* And put it in the mob */
    insert_ob_in_ob(tmp, head);

    env = get_env_recursive(op);

    /* Store position (for returning home after aggro is lost...) */
    if (env->map) {
        tmp->x = env->x;
        tmp->y = env->y;
        FREE_AND_ADD_REF_HASH(tmp->slaying, env->map->path);
    }

    return tmp;
}

/**
 * Find base info object in monster.
 * @param op Monster object.
 * @return Pointer to the base info if found, NULL otherwise. */
object *find_base_info_object(object *op)
{
    object *tmp;

    for (tmp = op->inv; tmp; tmp = tmp->below) {
        if (tmp->type == BASE_INFO) {
            return tmp;
        }
    }

    return NULL;
}

/**
 * Set the movement speed of a monster.
 * 1/5 = mob is slowed (by magic)
 * 2/5 = normal mob speed - moving normal
 * 3/5 = mob is moving fast
 * 4/5 = mov is running/attack speed
 * 5/5 = mob is hasted and moving full speed
 * @param op Monster.
 * @param idx Index. */
void set_mobile_speed(object *op, int idx)
{
    object *base;
    float speed, tmp;

    base = insert_base_info_object(op);

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
    if ((tmp && !op->speed) || (!tmp && op->speed)) {
        update_ob_speed(op);
    }
}
