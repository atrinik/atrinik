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
 * This handles all attacks, magical or not.
 */

#include <global.h>
#include <monster_data.h>
#include <faction.h>
#include <plugin.h>
#include <arch.h>
#include <attack.h>
#include <player.h>
#include <object.h>
#include <exp.h>

/**
 * Names of attack types to use when saving them to file.
 *
 * @warning Cannot contain spaces. Use underscores instead.
 */
const char *const attack_save[NROFATTACKS] = {
    "impact",   "slash",     "cleave",      "pierce",    "weaponmagic",
    "fire",     "cold",      "electricity", "poison",    "acid",
    "magic",    "lifesteal", "blind",       "paralyze",  "force",
    "godpower", "chaos",     "drain",       "slow",      "confusion",
    "internal"
};

/**
 * Short description of names of the attack types.
 */
const char *const attack_name[NROFATTACKS] = {
    "impact",   "slash",     "cleave",      "pierce",    "weapon magic",
    "fire",     "cold",      "electricity", "poison",    "acid",
    "magic",    "lifesteal", "blind",       "paralyze",  "force",
    "godpower", "chaos",     "drain",       "slow",      "confusion",
    "internal"
};

/**
 * Perform sanity checks to make sure the attacker can actually attack the
 * target.
 *
 * @param op
 * The victim.
 * @param hitter
 * The attacker.
 * @param[out] attack_map
 * On success, will contain a value indicating whether the attack is being
 * done on a map or not.
 * @return
 * True if the attacker can hit the victim, false otherwise.
 */
static int
attack_check_sanity (object *op, object *hitter, bool *attack_map)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(hitter != NULL);

    if (OBJECT_FREE(op) || OBJECT_FREE(hitter)) {
        return false;
    }

    if (hitter->env != NULL || op->env != NULL ||
        hitter->map == NULL || op->map == NULL) {
        if (attack_map != NULL) {
            *attack_map = false;
        }

        return true;
    }

    if (QUERY_FLAG(op, FLAG_REMOVED) || QUERY_FLAG(hitter, FLAG_REMOVED)) {
        return false;
    }

    if (attack_map != NULL) {
        *attack_map = true;
    }

    return true;
}

/**
 * Check if the attacker and the victim are still in a relation similar to the
 * one determined by attack_check_sanity().
 *
 * @param op
 * The victim.
 * @param hitter
 * The attacker.
 * @param attack_map
 * Previous mode as returned by attack_check_sanity().
 * @return
 * true if the relation has changed, false otherwise.
 */
static bool
attack_check_abort (object *op, object *hitter, bool attack_map)
{
    bool new_attack_map;

    if (hitter->env == op || op->env == hitter) {
        new_attack_map = false;
    } else if (QUERY_FLAG(op, FLAG_REMOVED) ||
               QUERY_FLAG(hitter, FLAG_REMOVED) ||
               hitter->map == NULL ||
               op->map == NULL) {
        return true;
    } else {
        new_attack_map = true;
    }

    return new_attack_map != attack_map;
}

/**
 * Adjustments to attack rolls by various conditions.
 *
 * Essentially simulates advantaged/disadvantaged rolls.
 *
 * @param op
 * Victim of the attack.
 * @param hitter
 * Who is attacking.
 * @return
 * Adjustment to attack roll.
 */
static int
attack_roll_adjust (object *op, object *hitter)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(hitter != NULL);
    SOFT_ASSERT_RC(op->map != NULL, 0, "Object without map: %s",
                   object_get_str(op));
    SOFT_ASSERT_RC(hitter->map != NULL, 0, "Object without map: %s",
                   object_get_str(hitter));

    if (!on_same_map(op, hitter)) {
        return 0;
    }

    if (OBJECT_IS_PROJECTILE(hitter)) {
        object *owner = get_owner(hitter);
        if (owner != NULL) {
            hitter = owner;
        }
    }

    if (!IS_LIVE(hitter)) {
        return 0;
    }

    int adjust = 0;

    /* Invisible means, we can't see it - same for blind. */
    if (IS_INVISIBLE(op, hitter) || QUERY_FLAG(hitter, FLAG_BLIND)) {
        adjust -= 12;
    }

    if (QUERY_FLAG(hitter, FLAG_SCARED)) {
        adjust -= 3;
    }

    if (QUERY_FLAG(op, FLAG_SCARED)) {
        adjust += 1;
    }

    if (QUERY_FLAG(op, FLAG_UNAGGRESSIVE)) {
        adjust += 1;
    }

    if (QUERY_FLAG(op, FLAG_CONFUSED)) {
        adjust += 1;
    }

    if (QUERY_FLAG(hitter, FLAG_CONFUSED)) {
        adjust -= 3;
    }

    /* If we attack at a different 'altitude' it's harder */
    if (QUERY_FLAG(hitter, FLAG_FLYING) != QUERY_FLAG(op, FLAG_FLYING)) {
        adjust -= 2;
    }

    if (hitter->direction == op->direction) {
        /* Backstab */
        adjust += 5;
    } else if (hitter->direction == absdir(op->direction - 1) ||
               hitter->direction == absdir(op->direction + 1)) {
        /* Sidestab */
        adjust += 2;
    }

    /* If the monster had to turn to attack since it last saw its enemy, it's
     * a fairly large disadvantage. */
    mapstruct *enemy_map;
    uint16_t enemy_x, enemy_y;
    if (hitter->type == MONSTER &&
        monster_data_enemy_get_coords(hitter,
                                      &enemy_map,
                                      &enemy_x,
                                      &enemy_y)) {
        rv_vector rv;
        if (!get_rangevector_from_mapcoords(hitter->map,
                                            hitter->x,
                                            hitter->y,
                                            enemy_map,
                                            enemy_x,
                                            enemy_y,
                                            &rv,
                                            0) ||
            rv.direction != hitter->direction) {
            adjust -= 6;
        }
    }

    return adjust;
}

/**
 * Make one object hit another for some amount of damage, primarily determined
 * by the attacker's damage attribute.
 *
 * The attacker's WC and the victim's AC will be considered, and a roll will be
 * done to see if the attacker is successful in hitting the victim.
 *
 * @param op
 * Victim.
 * @param hitter
 * Attacker.
 * @return
 * Dealt damage.
 */
int
attack_object (object *op, object *hitter)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(hitter != NULL);

    op = HEAD(op);
    hitter = HEAD(hitter);

    bool attack_map;
    if (!attack_check_sanity(op, hitter, &attack_map)) {
        return 0;
    }

    /* Trigger the ATTACK event */
    int ret = trigger_event(EVENT_ATTACK,
                            hitter,
                            hitter,
                            op,
                            NULL,
                            0,
                            hitter->stats.dam,
                            hitter->stats.wc,
                            0);
    if (ret != 0) {
        return MAX(0, ret);
    }

    /* Face the victim. */
    rv_vector dir;
    if (get_rangevector(hitter, op, &dir, 0)) {
        hitter->direction = dir.direction;
    }

    /* Update the animation flags to use the attacking animation. */
    hitter->anim_flags |= ANIM_FLAG_ATTACKING;
    hitter->anim_flags &= ~ANIM_FLAG_STOP_ATTACKING;

    if (op->type == PLAYER) {
        CONTR(op)->last_combat = pticks;
    }

    if (hitter->type == PLAYER) {
        CONTR(hitter)->last_combat = pticks;
    }

    if (unlikely(hitter->stats.dam == 0)) {
        return 0;
    }

    int roll_adjust = 0;
    if (attack_map) {
        roll_adjust += attack_roll_adjust(op, hitter);
    }

    if (hitter->stats.wc_range == 0) {
        log_error("Hitter with no wc_range, fixing: %s",
                  object_get_str(hitter));
        hitter->stats.wc_range = 20;
    }

    /* Roll to try to hit the creature. */
    int roll = rndm(1, hitter->stats.wc_range);
    if (roll != hitter->stats.wc_range &&
        op->stats.ac > hitter->stats.wc + roll + roll_adjust) {
        /* We missed. */
        if (hitter->type == ARROW) {
            return 0;
        }

        if (hitter->type == PLAYER) {
            play_sound_map(hitter->map,
                           CMD_SOUND_EFFECT,
                           "miss_player1.ogg",
                           hitter->x,
                           hitter->y,
                           0,
                           0);
            draw_info_format(COLOR_ORANGE, hitter, "You miss %s!", op->name);
            draw_info_format(COLOR_PURPLE, op, "%s misses you!", hitter->name);
        } else {
            play_sound_map(hitter->map,
                           CMD_SOUND_EFFECT,
                           "miss_mob1.ogg",
                           hitter->x,
                           hitter->y,
                           0,
                           0);
            draw_info_format(COLOR_PURPLE, op, "%s misses you!", hitter->name);
        }

        return 0;
    }

    /* At this point, the victim will never sleep any longer. */
    CLEAR_FLAG(op, FLAG_SLEEP);

    const char *sound;
    if (hitter->type == ARROW) {
        sound = "arrow_hit.ogg";
    } else if (hitter->attack[ATNR_SLASH] != 0) {
        sound = "hit_slash.ogg";
    } else if (hitter->attack[ATNR_CLEAVE] != 0) {
        sound = "hit_cleave.ogg";
    } else if (hitter->attack[ATNR_IMPACT] != 0) {
        sound = "hit_impact.ogg";
    } else {
        sound = "hit_pierce.ogg";
    }

    /* Play a hit sound. */
    play_sound_map(hitter->map,
                   CMD_SOUND_EFFECT,
                   sound,
                   hitter->x,
                   hitter->y,
                   0,
                   0);

    int dam;
    OBJECTS_DESTROYED_BEGIN(op, hitter) {
        if (attack_map && QUERY_FLAG(op, FLAG_HITBACK) && IS_LIVE(hitter)) {
            dam = attack_hit(hitter, op, rndm(0, op->stats.dam));
            if (OBJECTS_DESTROYED_ANY(op, hitter) ||
                attack_check_abort(op, hitter, attack_map)) {
                return dam;
            }
        }

        int real_dam = rndm(hitter->stats.dam * 0.8 + 1.0, hitter->stats.dam);
        dam = attack_hit(op, hitter, real_dam);

        /* Remove the if directives if you add ANY code dealing with op/hitter
         * after this point. */
#if 0
        if (OBJECTS_DESTROYED_ANY(op, hitter) ||
            attack_check_abort(op, hitter, attack_map)) {
            return dam;
        }
#endif
    } OBJECTS_DESTROYED_END();

    return dam;
}

/**
 * Attempt to block a hit using a melee weapon or a shield.
 *
 * @param op
 * Victim of the attacker.
 * @param hitter
 * The attacker.
 * @param[out] damage
 * The damage to do; may be modified.
 * @return
 * True if the attack was completely blocked, false otherwise.
 */
static bool
attack_block_hit (object *op, object *hitter, double *damage)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(hitter != NULL);
    HARD_ASSERT(damage != NULL);

    /* Only melee hits from living attackers and projectiles can be blocked. */
    if (!IS_LIVE(hitter) && !OBJECT_IS_PROJECTILE(hitter)) {
        return false;
    }

    /* Players must have a melee weapon skill readied. */
    if (op->type == PLAYER && op->chosen_skill != NULL &&
        !SKILL_IS_MELEE(op->chosen_skill->stats.sp)) {
        return false;
    }

    if (op->block != 0) {
        object *hit_obj = OWNER(hitter);

        int chance = 20 + hit_obj->level - op->level;
        chance -= rndm(op->block / 2.0 + 0.5, op->block);
        chance = MAX(3, chance);

        if (rndm_chance(chance)) {
            return true;
        }
    }

    if (op->absorb == 0) {
        return false;
    }

    /* Absorb some of the damage. */
    double absorb = (100.0 - MIN(90, op->absorb)) / 100.0;
    (*damage) *= absorb;
    return false;
}

/**
 * Send message about an attack for the involved players.
 *
 * @param op
 * Victim of the attack.
 * @param hitter
 * Attacker.
 * @param atnr
 * ID of the attack type.
 * @param dam_done
 * Actual damage done.
 * @param dam_orig
 * How much damage should have been done, not counting protections.
 */
static void
send_attack_msg (object  *op,
                 object  *hitter,
                 atnr_t atnr,
                 int      dam_done,
                 int      dam_orig)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(hitter != NULL);

    if (op->type == PLAYER) {
        draw_info_format(COLOR_PURPLE, op,
                         "%s hit you for %d (%d) damage.",
                         hitter->name, dam_done, dam_done - dam_orig);
    }

    const char *hitter_name =
        atnr == ATNR_INTERNAL ? hitter->name : attack_name[atnr];
    if (hitter->type == PLAYER || ((hitter = get_owner(hitter)) != NULL &&
                                   hitter->type == PLAYER)) {
        draw_info_format(COLOR_ORANGE, hitter,
                         "You hit %s for %d (%d) with %s.",
                         op->name, dam_done, dam_done - dam_orig, hitter_name);
    }
}

/**
 * Handles one attacktype's damage.
 *
 * This doesn't damage the creature, but returns how much it should
 * take. However, it will do other effects (paralyzation, slow, etc).
 *
 * @param op
 * Victim of the attack.
 * @param hitter
 * Attacker.
 * @param dam
 * Damage to deal.
 * @param dam_orig
 * Original damage that ought to have been done (not counting
 * protections/blocking/etc).
 * @param atnr
 * ID of the attacktype of the attack.
 * @return
 * Damage to actually do.
 */
static double
attack_hit_attacktype (object  *op,
                       object  *hitter,
                       double   dam,
                       double   dam_orig,
                       atnr_t atnr)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(hitter != NULL);
    HARD_ASSERT(dam >= 0.0);

    /* Adjust the damage based on the attacker's attack type value. */
    dam *= hitter->attack[atnr] / 100.0;
    if (dam_orig > 0 && dam < 1.0) {
        dam = 1.0;
    }

    dam_orig = dam;

#define ATTACK_PROTECT_DAMAGE()                         \
    do {                                                \
        dam *= (100.0 - op->protection[atnr]) / 100.0;  \
    } while (0)

    /* AT_INTERNAL is supposed to do exactly 'dam' amount of damage. */
    if (atnr == ATNR_INTERNAL) {
        /* Handle the poisoning object type. */
        if (hitter->type == POISONING) {
            /* Map to poison... */
            atnr = ATNR_POISON;

            if (op->protection[atnr] == 100) {
                send_attack_msg(op, hitter, atnr, 0.0, dam_orig);
                return 0.0;
            }

            ATTACK_PROTECT_DAMAGE();
        }

        send_attack_msg(op, hitter, atnr, dam, dam_orig);
        return dam;
    }

    /* Check for complete immunity. */
    if (op->protection[atnr] == 100) {
        send_attack_msg(op, hitter, atnr, 0, dam_orig);
        return 0.0;
    }

    switch (atnr) {
    case ATNR_IMPACT:
    case ATNR_SLASH:
    case ATNR_CLEAVE:
    case ATNR_PIERCE:
        check_physically_infect(op, hitter);
        ATTACK_PROTECT_DAMAGE();
        send_attack_msg(op, hitter, atnr, dam, dam_orig);
        break;

    case ATNR_POISON:
        ATTACK_PROTECT_DAMAGE();
        send_attack_msg(op, hitter, atnr, dam, dam_orig);

        if (dam > 0.0 && IS_LIVE(op)) {
            attack_perform_poison(op, hitter, dam);
        }

        break;

    case ATNR_CONFUSION:
    case ATNR_SLOW:
    case ATNR_PARALYZE:
    case ATNR_BLIND: {
        int ldiff = MIN(MAXLEVEL, MAX(0, op->level - hitter->level));

        if (!DBL_EQUAL(op->speed, 0.0) && IS_LIVE(op) &&
            rndm_chance(atnr == ATNR_SLOW ? 6 : 3) &&
            ((rndm(1, 20) + op->protection[atnr] / 10) < savethrow[ldiff])) {
            if (atnr == ATNR_CONFUSION) {
                draw_info_format(COLOR_ORANGE, hitter,
                                 "You confuse %s!", op->name);
                draw_info_format(COLOR_PURPLE, op,
                                 "%s confused you!", hitter->name);
                attack_perform_confusion(op);
            } else if (atnr == ATNR_SLOW) {
                draw_info_format(COLOR_ORANGE, hitter,
                                 "You slow %s!", op->name);
                draw_info_format(COLOR_PURPLE, op,
                                 "%s slowed you!", hitter->name);
                attack_perform_slow(op);
            } else if (atnr == ATNR_PARALYZE) {
                draw_info_format(COLOR_ORANGE, hitter,
                                 "You paralyze %s!", op->name);
                draw_info_format(COLOR_PURPLE, op,
                                 "%s paralyzed you!", hitter->name);
                attack_peform_paralyze(op, dam);
            } else if (atnr == ATNR_BLIND && !QUERY_FLAG(op, FLAG_UNDEAD)) {
                draw_info_format(COLOR_ORANGE, hitter,
                                 "You blind %s!", op->name);
                draw_info_format(COLOR_PURPLE, op,
                                 "%s blinded you!", hitter->name);
                attack_perform_blind(op, hitter, dam);
            }
        }

        dam = 0.0;
        break;
    }

    case ATNR_LIFESTEAL: {
        ATTACK_PROTECT_DAMAGE();
        send_attack_msg(op, hitter, atnr, dam, dam_orig);

        object *owner = OWNER(hitter);
        owner->stats.hp += dam;
        if (owner->stats.hp > owner->stats.maxhp) {
            owner->stats.hp = owner->stats.maxhp;
        }

        break;
    }

    default:
        ATTACK_PROTECT_DAMAGE();
        send_attack_msg(op, hitter, atnr, dam, dam_orig);
        break;
    }

    return dam;

#undef ATTACK_PROTECT_DAMAGE
}

/**
 * Hit the specified object for the given amount of damage.
 *
 * @param op
 * Object to be hit.
 * @param hitter
 * What is hitting the object.
 * @param dam
 * Base damage. Protections, slaying, blocking, etc, will be taken into
 * account.
 * @return
 * Dealt damage.
 */
int
attack_hit (object *op, object *hitter, int dam)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(hitter != NULL);

    op = HEAD(op);
    hitter = HEAD(hitter);

    /* Cannot hit players in god-mode. */
    if (op->type == PLAYER && CONTR(op)->tgm) {
        return 0;
    }

    /* Objects with the invulnerable flag cannot be damaged. */
    if (QUERY_FLAG(op, FLAG_INVULNERABLE)) {
        return 0;
    }

    /* The object doesn't have any HP left. */
    if (unlikely(op->stats.hp < 0)) {
        return 0;
    }

    object *hitter_owner = get_owner(hitter);

    /* Sanity check: If the hitter has ownercount (so it had an owner)
     * but the owner itself is no longer valid, we won't do any damage,
     * otherwise player could fire an arrow, logout, and the arrow itself
     * would cause damage to anything it hits, even friendly creatures. */
    if (unlikely(hitter->ownercount != 0 && hitter_owner == NULL)) {
        return 0;
    }

    /* Check if the object to hit has any HP left */
    if (op->stats.hp < 0) {
        return 0;
    }

    if (hitter_owner == NULL) {
        hitter_owner = hitter;
    }

    /* Do not let friendly objects attack each other. */
    if (is_friend_of(hitter_owner, op)) {
        return 0;
    }

    /* Check for PVP areas. */
    if (op->type == PLAYER && hitter_owner->type == PLAYER &&
        !pvp_area(op, hitter_owner)) {
        return 0;
    }

    if (!attack_check_sanity(op, hitter, NULL)) {
        return 0;
    }

    double damage = dam;
    if (hitter->slaying != NULL && hitter->slaying == op->race) {
        if (QUERY_FLAG(hitter, FLAG_IS_ASSASSINATION)) {
            damage *= 2.25;
        } else {
            damage *= 1.75;
        }
    }

    if (hitter_owner->type == MONSTER && hitter_owner->level > op->level) {
        double modifier = hitter_owner->level - op->level;
        modifier /= MIN(20, op->level);
        damage += dam / 2.0 * modifier;
    }

    double dam_orig = damage;
    double maxdam = 0.0;

    /* Try to block the attack. */
    if (attack_block_hit(op, hitter, &damage)) {
        draw_info_format(COLOR_PURPLE, hitter, "%s blocked your attack!",
                         op->name);
        draw_info_format(COLOR_ORANGE, op, "You block %s!",
                         hitter->name);
    } else if (damage > 0.0) {
        /* Go through and hit the player with each attacktype, one by one.
         * hit_player_attacktype only figures out the damage, doesn't inflict
         * it. It will do the appropriate action for attacktypes with
         * effects (slow, paralization, etc). */
        for (atnr_t atnr = 0; atnr < NROFATTACKS; atnr++) {
            if (hitter->attack[atnr] != 0) {
                maxdam += attack_hit_attacktype(op,
                                                hitter,
                                                damage,
                                                dam_orig,
                                                atnr);
            }
        }
    }

    /* If one gets attacked, the attacker will become the enemy */
    if (!OBJECT_VALID(op->enemy, op->enemy_count) &&
        !IS_INVISIBLE(hitter_owner, op) &&
        !QUERY_FLAG(op, FLAG_INVULNERABLE)) {
        set_npc_enemy(op, hitter_owner, NULL);
    }

    /* Reset the cache so that we send damage numbers to clients. */
    if (op->damage_round_tag != global_round_tag) {
        op->last_damage = 0;
        op->damage_round_tag = global_round_tag;
    }

    if (hitter_owner->type == PLAYER) {
        CONTR(hitter_owner)->last_combat = pticks;
        CONTR(hitter_owner)->stat_damage_dealt += maxdam;
    }

    if (op->type == PLAYER) {
        CONTR(op)->last_combat = pticks;
        CONTR(op)->stat_damage_taken += maxdam;
    }

    op->last_damage += maxdam;

    /* For the purposes of statistics and damage visible on-screen, we want to
     * show the full damage. However, to the function's callers, we only want
     * to return the total damage dealt to the object, capping it at the
     * object's hp. */
    if (maxdam > op->stats.hp) {
        maxdam = op->stats.hp;
    }

    /* Damage the target got */
    op->stats.hp -= maxdam;

    /* Check to see if monster runs away. */
    if (op->stats.hp >= 0 && QUERY_FLAG(op, FLAG_MONSTER) &&
        op->stats.hp < op->run_away / 100.0 * op->stats.maxhp) {
        SET_FLAG(op, FLAG_RUN_AWAY);
    }

    /* Reached 0 or less HP, kill the object. */
    if (op->stats.hp <= 0) {
        attack_kill(op, hitter);
    }

    return maxdam;
}

/**
 * Attack a spot on the map.
 *
 * @param op
 * Object hitting the map.
 * @param dir
 * Direction op is hitting/going.
 * @param multi_reduce
 * Whether to reduce the damage for multi-arch monsters.
 * This will make it so that part of 4-tiles monster only gets hit for
 * 1/4 of the damage, making cone spells more fair against multi-arch
 * monsters.
 */
void
attack_hit_map (object *op, int dir, bool multi_reduce)
{
    HARD_ASSERT(op != NULL);

    if (OBJECT_FREE(op)) {
        return;
    }

    op = HEAD(op);

    if (op->map == NULL || op->stats.dam == 0) {
        return;
    }

    int x = op->x + freearr_x[dir];
    int y = op->y + freearr_y[dir];
    mapstruct *m = get_map_from_coord(op->map, &x, &y);
    if (m == NULL) {
        return;
    }

    object *owner = OWNER(op);
    owner = HEAD(owner);

    object *tmp;
    FOR_MAP_LAYER_BEGIN(m, x, y, LAYER_LIVING, -1, tmp) {
        tmp = HEAD(tmp);

        /* Cones with race set can only damage members of that race. */
        if (op->type == CONE && op->race != NULL && tmp->race != op->race) {
            continue;
        }

        /* Check friendship status. */
        if (is_friend_of(owner, tmp)) {
            continue;
        }

        int dam = op->stats.dam;
        if (multi_reduce && tmp->quick_pos != 0) {
            dam /= (tmp->quick_pos >> 4) + 1;
        }

        attack_hit(tmp, op, dam);
    } FOR_MAP_LAYER_END
}

/**
 * One player gets exp by killing a monster.
 *
 * @param op
 * Player. This should be the killer.
 * @param exp_gain
 * Experience to gain.
 * @param skill
 * Skill that was used to kill the monster.
 */
static inline void
share_kill_exp_one (object *op, int64_t exp_gain, object *skill)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(skill != NULL);

    if (exp_gain != 0) {
        add_exp(op, exp_gain, skill->stats.sp, 0);
    } else {
        draw_info(COLOR_WHITE, op,
                  "Your enemy wasn't worth any experience to you.");
    }
}

/**
 * Share experience gained by killing a monster. This will fairly share
 * experience between party members, or if none are present, it will use
 * share_kill_exp_one() instead.
 *
 * @param op
 * Player that killed the monster.
 * @param exp_gain
 * Experience to share.
 * @param skill
 * Skill that was used to kill the monster.
 */
static void
share_kill_exp (object *op, int64_t exp_gain, object *skill)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(skill != NULL);

    int shares = 0, count = 0;

    /* No party, no sharing. */
    if (CONTR(op)->party == NULL) {
        share_kill_exp_one(op, exp_gain, skill);
        return;
    }

    party_struct *party = CONTR(op)->party;
    for (objectlink *ol = party->members; ol != NULL; ol = ol->next) {
        if (!on_same_map(ol->objlink.ob, op)) {
            continue;
        }

        object *player_skill =
            CONTR(ol->objlink.ob)->skill_ptr[skill->stats.sp];
        if (player_skill == NULL) {
            continue;
        }

        count++;
        shares += player_skill->level + 4;
    }

    if (count <= 1 || shares > exp_gain) {
        share_kill_exp_one(op, exp_gain, skill);
    } else {
        int64_t share = exp_gain / shares;
        int64_t given = 0;
        for (objectlink *ol = party->members; ol != NULL; ol = ol->next) {
            if (ol->objlink.ob == op || !on_same_map(ol->objlink.ob, op)) {
                continue;
            }

            object *player_skill =
                CONTR(ol->objlink.ob)->skill_ptr[skill->stats.sp];
            if (player_skill == NULL) {
                continue;
            }

            given += add_exp(ol->objlink.ob,
                             (player_skill->level + 4) * share,
                             skill->stats.sp,
                             0);
        }

        exp_gain -= given;
        share_kill_exp_one(op, exp_gain, skill);
    }
}

/**
 * An object was killed, handle various things (logging, messages, etc).
 *
 * @param op
 * What is being killed.
 * @param hitter
 * Who killed 'op'.
 * @retval true Object was killed.
 * @retval false Object was not killed.
 */
bool
attack_kill (object *op, object *hitter)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(hitter != NULL);

    /* Players in god-mode cannot die. */
    if (op->type == PLAYER && CONTR(op)->tgm) {
        return false;
    }

    /* Trigger the DEATH event. */
    if (trigger_event(EVENT_DEATH, hitter, op, NULL, NULL, 0, 0, 0, 0) != 0) {
        return false;
    }

    /* Only when some damage is stored, and we're on a map. */
    if (op->damage_round_tag == global_round_tag && op->map != NULL) {
        SET_MAP_DAMAGE(op->map, op->x, op->y, op->sub_layer, op->last_damage);
        SET_MAP_RTAG(op->map, op->x, op->y, op->sub_layer, global_round_tag);
    }

    if (op->map != NULL) {
        play_sound_map(op->map,
                       CMD_SOUND_EFFECT,
                       "kill.ogg",
                       op->x,
                       op->y,
                       0,
                       0);
    }

    /* Figure out who to credit for the kill. */
    object *owner = OWNER(hitter);

    /* Player killed something. */
    if (owner->type == PLAYER) {
        char *name = object_get_name_s(op, owner);
        if (owner != hitter) {
            char *hitter_name = object_get_name_s(hitter, owner);
            draw_info_format(COLOR_WHITE, owner, "You killed %s with %s.", name,
                             hitter_name);
            efree(hitter_name);
        } else {
            draw_info_format(COLOR_WHITE, owner, "You killed %s.", name);
        }
        efree(name);

        if (op->type == MONSTER) {
            CONTR(owner)->stat_kills_mob++;
            statistic_update("kills", owner, 1, op->name);

            if (object_get_value(op, "was_provoked") == NULL) {
                shstr *faction_name = object_get_value(op, "faction");
                if (faction_name != NULL) {
                    faction_t faction = faction_find(faction_name);
                    if (faction != NULL) {
                        faction_update_kill(faction, CONTR(owner));
                    } else {
                        LOG(ERROR, "Invalid faction: %s for %s", faction_name,
                            object_get_str(op));
                    }
                }
            }
        } else if (op->type == PLAYER) {
            CONTR(owner)->stat_kills_pvp++;
        }
    }

    bool is_pvp = pvp_area(NULL, op);

    /* Killed a player in PvP area. */
    if (is_pvp && op->type == PLAYER && owner->type == PLAYER) {
        draw_info(COLOR_WHITE, owner, "Your foe has fallen!\nVICTORY!!!");
    }

    int64_t exp_gain = 0;

    /* Killed a monster and it wasn't in PvP area, so give exp. */
    if (!is_pvp && owner->type == PLAYER && op->type != PLAYER) {
        /* Figure out the skill that should gain experience. If the hitter
         * has chosen_skill set, we will use that, otherwise try to use
         * owner's chosen_skill. */
        object *skill = hitter->chosen_skill;
        if (skill == NULL) {
            skill = owner->chosen_skill;
        }

        if (skill != NULL) {
            /* Calculate how much experience to gain. */
            exp_gain = calc_skill_exp(owner, op, skill->level);
            /* Give the experience, sharing it with party members if
             * applicable. */
            share_kill_exp(owner, exp_gain, skill);
        }
    }

    /* Player has been killed. */
    if (op->type == PLAYER) {
        /* Tell everyone that this player has died. */
        char *name = object_get_name_s(op, NULL);
        char *hitter_name = object_get_name_s(hitter, NULL);
        char *owner_name = object_get_name_s(owner, NULL);

        if (owner != NULL) {
            draw_info_format(COLOR_WHITE, NULL, "%s killed %s with %s%s.",
                             owner_name,
                             name,
                             hitter_name,
                             is_pvp ? " (duel)" : "");
        } else {
            draw_info_format(COLOR_WHITE, NULL, "%s killed %s%s.",
                             hitter_name,
                             name,
                             is_pvp ? " (duel)" : "");
        }

        /* Update player's killer. */
        if (owner->type == PLAYER) {
            snprintf(VS(CONTR(op)->killer),
                     "%s the %s",
                     owner_name,
                     owner->race);
        } else {
            snprintf(VS(CONTR(op)->killer), "%s", owner_name);
        }

        efree(name);
        efree(hitter_name);
        efree(owner_name);

        /* And actually kill the player. */
        kill_player(op);
    } else {
        /* Monster or something else has been killed, so remove it from the
         * active list. */
        op->speed = 0.0;
        update_ob_speed(op);

        /*
         * Rules:
         * 1. Monster will drop corpse for his target, not the killer (unless
         *    killer == target).
         * 2. NPC kill hit will overwrite player target on drop.
         * 3. Kill hit will count if target was an NPC.
         */
        if (owner->type != PLAYER ||
            op->enemy == NULL ||
            op->enemy->type != PLAYER) {
            op->enemy = owner;
            op->enemy_count = owner->count;
        }

        /* Monster killed another monster. */
        if (owner->type == MONSTER) {
            /* No loot */
            SET_FLAG(op, FLAG_STARTEQUIP);
            /* Force an empty corpse though. */
            SET_FLAG(op, FLAG_CORPSE_FORCED);
        } else if (exp_gain == 0) {
            /* No exp, no loot and no corpse. */
            SET_FLAG(op, FLAG_STARTEQUIP);
        }

        destruct_ob(op);
    }

    return true;
}

/**
 * Poison a living object.
 *
 * @param op
 * Victim.
 * @param hitter
 * Who is attacking.
 * @param dam
 * Damage to deal.
 */
void
attack_perform_poison (object *op, object *hitter, double dam)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(hitter != NULL);

    /* We only poison living things. */
    if (!IS_LIVE(op)) {
        return;
    }

    /* Make sure poisoning doesn't stack more poisoning by itself. */
    if (hitter->type == POISONING) {
        return;
    }

    /* Save some work if we know it isn't going to affect the player */
    if (op->protection[ATNR_POISON] == 100) {
        return;
    }

    static archetype_t *at = NULL;
    if (at == NULL) {
        at = arch_find("poisoning");
        SOFT_ASSERT(at != NULL, "Could not find poisoning archetype");
    }

    int dam2 = rndm(dam / 2.0 + 1.0, dam);
    if (dam2 > op->stats.maxhp / 3) {
        dam2 = op->stats.maxhp / 3;
    } else if (dam2 < 1) {
        dam2 = 1;
    }

    object *tmp = present_arch_in_ob(at, op);
    if (tmp == NULL) {
        tmp = arch_to_object(at);
        tmp->level = hitter->level;
        tmp->stats.dam = dam2;

        /* So we get credit for poisoning kills */
        if (IS_LIVE(hitter)) {
            set_owner(tmp, hitter);
        }

        SET_FLAG(tmp, FLAG_APPLIED);
        tmp = insert_ob_in_ob(tmp, op);
        SOFT_ASSERT(tmp != NULL, "Failed to insert poisoning into %s",
                    object_get_str(op));

        if (op->type == PLAYER) {
            char *name = object_get_name_s(hitter, op);
            draw_info_format(COLOR_WHITE, op, "%s has poisoned you!", name);
            efree(name);
        } else {
            if (hitter->type == PLAYER) {
                char *name = object_get_name_s(op, hitter);
                draw_info_format(COLOR_WHITE, hitter, "You poisoned %s!", name);
                efree(name);
            } else if (get_owner(hitter) != NULL &&
                       hitter->owner->type == PLAYER) {
                char *name = object_get_name_s(op, hitter->owner);
                char *hitter_name = object_get_name_s(hitter, hitter->owner);
                draw_info_format(COLOR_WHITE, hitter->owner, "%s poisoned %s!",
                                 hitter_name, name);
                efree(name);
                efree(hitter_name);
            }
        }

        tmp->speed_left = 0;
    } else {
        tmp->stats.food++;
        esrv_update_item(UPD_EXTRA, tmp);

        if (dam2 > tmp->stats.dam) {
            tmp->stats.dam = dam2;
        }
    }
}

/**
 * Slow a living object.
 *
 * @param op
 * Victim.
 */
void
attack_perform_slow (object *op)
{
    HARD_ASSERT(op != NULL);

    static archetype_t *at = NULL;
    if (at == NULL) {
        at = arch_find("slowness");
        SOFT_ASSERT(at != NULL, "Could not find slowness archetype");
    }

    /* Save some work if we know it isn't going to affect the player */
    if (op->protection[ATNR_SLOW] == 100) {
        return;
    }

    object *tmp = present_arch_in_ob(at, op);
    if (tmp == NULL) {
        tmp = arch_to_object(at);
        SET_FLAG(tmp, FLAG_APPLIED);
        tmp = insert_ob_in_ob(tmp, op);
        SOFT_ASSERT(tmp != NULL, "Failed to insert slowness into %s",
                    object_get_str(op));
        draw_info(COLOR_WHITE, op, "The world suddenly moves very fast!");
    } else {
        tmp->stats.food++;
        esrv_update_item(UPD_EXTRA, tmp);
    }

    tmp->speed_left = 0;
}

/**
 * Confuse a living object.
 *
 * @param op
 * Victim.
 */
void
attack_perform_confusion (object *op)
{
    HARD_ASSERT(op != NULL);

    static archetype_t *at = NULL;
    if (at == NULL) {
        at = arch_find("confusion");
        SOFT_ASSERT(at != NULL, "Could not find confusion archetype");
    }

    /* Save some work if we know it isn't going to affect the player */
    if (op->protection[ATNR_CONFUSION] == 100) {
        return;
    }

    object *tmp = present_arch_in_ob(at, op);
    if (tmp == NULL) {
        tmp = arch_to_object(at);
        SET_FLAG(tmp, FLAG_APPLIED);
        tmp = insert_ob_in_ob(tmp, op);
        SOFT_ASSERT(tmp != NULL, "Could not insert confusion into %s",
                    object_get_str(op));
    }

    /* Duration added per hit and max. duration of confusion both depend
     * on the player's resistance */
    tmp->stats.food += MAX(1, 5 * (100 - op->protection[ATNR_CONFUSION]) / 100);
    int maxduration = MAX(2, 30 * (100 - op->protection[ATNR_CONFUSION]) / 100);

    if (tmp->stats.food > maxduration) {
        tmp->stats.food = maxduration;
    }

    tmp->speed_left = 0;
    esrv_update_item(UPD_EXTRA, tmp);
}

/**
 * Blind a living object.
 *
 * @param op
 * Victim.
 * @param hitter
 * Who is attacking.
 * @param dam
 * Damage to deal.
 */
void
attack_perform_blind (object *op, object *hitter, double dam)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(hitter != NULL);

    static archetype_t *at = NULL;
    if (at == NULL) {
        at = arch_find("blindness");
        SOFT_ASSERT(at != NULL, "Could not find blindness archetype");
    }

    /* Save some work if we know it isn't going to affect the player */
    if (op->protection[ATNR_BLIND] == 100) {
        return;
    }

    object *tmp = present_arch_in_ob(at, op);
    if (tmp == NULL) {
        tmp = arch_to_object(at);
        SET_FLAG(tmp, FLAG_APPLIED);
        tmp->speed = tmp->speed * (100.0 - op->protection[ATNR_BLIND]) / 100.0;
        tmp = insert_ob_in_ob(tmp, op);
        SOFT_ASSERT(tmp != NULL, "Failed to insert blindness into %s",
                    object_get_str(op));

        if (hitter != op) {
            object *owner = get_owner(hitter);
            if (owner == NULL) {
                owner = hitter;
            }

            char *name = object_get_name_s(op, owner);
            draw_info_format(COLOR_WHITE, owner, "Your attack blinds %s!",
                             name);
            efree(name);
        }
    }

    tmp->stats.food += dam;

    if (tmp->stats.food > 10) {
        tmp->stats.food = 10;
    }

    tmp->speed_left = 0;
    esrv_update_item(UPD_EXTRA, tmp);
}

/**
 * Paralyze a living thing.
 *
 * @param op
 * Victim.
 * @param dam
 * Damage to deal.
 */
void
attack_peform_paralyze (object *op, double dam)
{
    HARD_ASSERT(op != NULL);

    /* Save some work if we know it isn't going to affect the player */
    if (op->protection[ATNR_PARALYZE] == 100) {
        return;
    }

    double effect = dam * 3.0;
    effect *= (100.0 - (double) op->protection[ATNR_PARALYZE]) / 100.0;
    if (DBL_EQUAL(effect, 0.0)) {
        return;
    }

    /* We mark this object as paralyzed */
    SET_FLAG(op, FLAG_PARALYZED);

    op->speed_left -= FABS(op->speed) * effect;

    /* Max number of ticks to be affected for. */
    double max = (100.0 - op->protection[ATNR_PARALYZE]) / 2.0;
    if (op->speed_left < -(FABS(op->speed) * max)) {
        op->speed_left = -(FABS(op->speed) * max);
    }
}

/**
 * Cause damage due to falling.
 *
 * @param op
 * Object.
 * @param fall_floors
 * Number of floors the object fell down.
 */
void
attack_perform_fall (object *op, int fall_floors)
{
    HARD_ASSERT(op != NULL);
    SOFT_ASSERT(IS_LIVE(op), "Object is not alive: %s", object_get_str(op));

    int8_t dex = op->type == PLAYER ? op->stats.Dex : MAX_STAT / 2;

    int roll = MAX_STAT - dex + MAX_STAT - rndm(1, dex);
    roll *= MAX(1, fall_floors - 1);
    if (roll < MAX_STAT / 4) {
        return;
    }

    object *damager = arch_get("falling");
    damager->level = op->level;

    double dam = (op->weight + op->carrying) / 250.00 * MIN(10.0, fall_floors);
    dam *= falling_mitigation[dex];
    if (dam < 1.0) {
        dam = 1.0;
    }

    damager->stats.dam = dam;
    damager->stats.dam = rndm(damager->stats.dam / 2.0 + 1.0,
                              damager->stats.dam);

    attack_hit(op, damager, damager->stats.dam);
    object_destroy(damager);
}

/**
 * Test if objects are in range for melee attack.
 *
 * @param hitter
 * Attacker.
 * @param enemy
 * Enemy -- the target.
 * @return
 * True if the target is in melee range, false otherwise.
 */
bool
attack_is_melee_range (object *hitter, object *enemy)
{
    HARD_ASSERT(hitter != NULL);
    HARD_ASSERT(enemy != NULL);

    SOFT_ASSERT_RC(hitter->head == NULL, false, "Called on tail part: %s",
                   object_get_str(hitter));
    SOFT_ASSERT_RC(enemy->head == NULL, false, "Called on tail part: %s",
                   object_get_str(enemy));

    for (object *hitter_part = hitter;
         hitter_part != NULL;
         hitter_part = hitter_part->more) {
        for (int i = 0; i <= SIZEOFFREE1; i++) {
            int x = hitter_part->x + freearr_x[i];
            int y = hitter_part->y + freearr_y[i];
            mapstruct *m = get_map_from_coord(hitter_part->map, &x, &y);
            if (m == NULL) {
                continue;
            }

            for (object *enemy_part = enemy;
                 enemy_part != NULL;
                 enemy_part = enemy_part->more) {
                if (enemy_part->map == m &&
                    enemy_part->x == x &&
                    enemy_part->y == y) {
                    return true;
                }
            }
        }
    }

    return false;
}
