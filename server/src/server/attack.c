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

/**
 * Names of attack types to use when saving them to file.
 * @warning Cannot contain spaces. Use underscores instead.
 */
const char *const attack_save[NROFATTACKS] = {
    "impact",   "slash", "cleave",      "pierce",    "weaponmagic",
    "fire",     "cold",  "electricity", "poison",    "acid",
    "magic",    "mind",  "blind",       "paralyze",  "force",
    "godpower", "chaos", "drain",       "slow",      "confusion",
    "internal"
};

/**
 * Short description of names of the attack types.
 */
const char *const attack_name[NROFATTACKS] = {
    "impact",   "slash", "cleave",      "pierce",    "weapon magic",
    "fire",     "cold",  "electricity", "poison",    "acid",
    "magic",    "mind",  "blind",       "paralyze",  "force",
    "godpower", "chaos", "drain",       "slow",      "confusion",
    "internal"
};

static int get_attack_mode(object **target, object **hitter, int *simple_attack);
static int abort_attack(object *target, object *hitter, int simple_attack);
static int attack_ob_simple(object *op, object *hitter, int base_dam, int base_wc);
static void poison_player(object *op, object *hitter, float dam);
static void slow_living(object *op);
static int adj_attackroll(object *hitter, object *target);
static int is_aimed_missile(object *op);

/**
 * Simple wrapper for attack_ob_simple(), will use hitter's values.
 * @param op
 * Victim.
 * @param hitter
 * Attacker.
 * @return
 * Dealt damage.
 */
int attack_ob(object *op, object *hitter)
{
    if (op->head) {
        op = op->head;
    }

    if (hitter->head) {
        hitter = hitter->head;
    }

    return attack_ob_simple(op, hitter, hitter->stats.dam, hitter->stats.wc);
}

/**
 * Handles simple attack cases.
 * @param op
 * Victim.
 * @param hitter
 * Attacker.
 * @param base_dam
 * Damage to do.
 * @param base_wc
 * WC to hit with.
 * @return
 * Dealt damage.
 */
static int attack_ob_simple(object *op, object *hitter, int base_dam, int base_wc)
{
    int simple_attack, roll, adjust, dam = 0;
    tag_t op_tag, hitter_tag;
    rv_vector dir;

    if (op->head) {
        op = op->head;
    }

    if (hitter->head) {
        hitter = hitter->head;
    }

    if (get_attack_mode(&op, &hitter, &simple_attack)) {
        return 1;
    }

    /* Trigger the ATTACK event */
    trigger_event(EVENT_ATTACK, hitter, hitter, op, NULL, 0, base_dam, base_wc, SCRIPT_FIX_ALL);

    op_tag = op->count;
    hitter_tag = hitter->count;

    if (!hitter->stats.wc_range) {
        hitter->stats.wc_range = 20;
    }

    roll = rndm(0, hitter->stats.wc_range);

    if (get_rangevector(hitter, op, &dir, RV_NO_DISTANCE)) {
        HEAD(hitter)->direction = dir.direction;
    }

    adjust = 0;

    /* Adjust roll for various situations. */
    if (!simple_attack) {
        mapstruct *enemy_map;
        uint16_t enemy_x, enemy_y;

        adjust += adj_attackroll(hitter, op);

        if (hitter->type == MONSTER && monster_data_enemy_get_coords(hitter,
                &enemy_map, &enemy_x, &enemy_y)) {
            rv_vector rv;

            if (!get_rangevector_from_mapcoords(hitter->map, hitter->x,
                    hitter->y, enemy_map, enemy_x, enemy_y, &rv, 0) ||
                    rv.direction != dir.direction) {
                adjust -= 10;
            }
        }
    }

    hitter->anim_flags |= ANIM_FLAG_ATTACKING;
    hitter->anim_flags &= ~ANIM_FLAG_STOP_ATTACKING;

    if (op->type == PLAYER) {
        CONTR(op)->last_combat = pticks;
    }

    if (hitter->type == PLAYER) {
        CONTR(hitter)->last_combat = pticks;
    }

    /* See if we hit the creature */
    if (roll >= hitter->stats.wc_range ||
            op->stats.ac <= base_wc + roll + adjust) {
        int hitdam = base_dam;

        /* At this point NO ONE will still sleep */
        CLEAR_FLAG(op, FLAG_SLEEP);

        if (hitter->type == ARROW) {
            play_sound_map(hitter->map, CMD_SOUND_EFFECT, "arrow_hit.ogg", hitter->x, hitter->y, 0, 0);
        } else {
            if (hitter->attack[ATNR_SLASH]) {
                play_sound_map(hitter->map, CMD_SOUND_EFFECT, "hit_slash.ogg", hitter->x, hitter->y, 0, 0);
            } else if (hitter->attack[ATNR_CLEAVE]) {
                play_sound_map(hitter->map, CMD_SOUND_EFFECT, "hit_cleave.ogg", hitter->x, hitter->y, 0, 0);
            } else if (hitter->attack[ATNR_IMPACT]) {
                play_sound_map(hitter->map, CMD_SOUND_EFFECT, "hit_impact.ogg", hitter->x, hitter->y, 0, 0);
            } else {
                play_sound_map(hitter->map, CMD_SOUND_EFFECT, "hit_pierce.ogg", hitter->x, hitter->y, 0, 0);
            }
        }

        /* Need to do at least 1 damage, otherwise there is no point
         * to go further and it will cause FPE's below. */
        if (hitdam <= 0) {
            hitdam = 1;
        }

        /* Handle monsters that hit back */
        if (!simple_attack && QUERY_FLAG(op, FLAG_HITBACK) && IS_LIVE(hitter)) {
            hit_player(hitter, rndm(0, op->stats.dam), op);

            if (was_destroyed(op, op_tag) || was_destroyed(hitter, hitter_tag) || abort_attack(op, hitter, simple_attack)) {
                return dam;
            }
        }

        dam = hit_player(op, rndm(hitdam * 0.8 + 1, hitdam), hitter);

        if (was_destroyed(op, op_tag) || was_destroyed(hitter, hitter_tag) || abort_attack(op, hitter, simple_attack)) {
            return dam;
        }
    } else {
        /* We missed */

        if (hitter->type != ARROW) {
            if (hitter->type == PLAYER) {
                play_sound_map(hitter->map, CMD_SOUND_EFFECT, "miss_player1.ogg", hitter->x, hitter->y, 0, 0);
                draw_info_format(COLOR_ORANGE, hitter, "You miss %s!", op->name);

                if (op->type == PLAYER) {
                    draw_info_format(COLOR_PURPLE, op, "%s misses you!", hitter->name);
                }
            } else {
                play_sound_map(hitter->map, CMD_SOUND_EFFECT, "miss_mob1.ogg", hitter->x, hitter->y, 0, 0);
                draw_info_format(COLOR_PURPLE, op, "%s misses you!", hitter->name);
            }
        }
    }

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
        object *hit_obj = get_owner(hitter);
        if (hit_obj == NULL) {
            hit_obj = hitter;
        }

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
                 _attacks atnr,
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
hit_player_attacktype (object  *op,
                       object  *hitter,
                       double   dam,
                       double   dam_orig,
                       _attacks atnr)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(hitter != NULL);
    HARD_ASSERT(dam >= 0.0);

    /* Adjust the damage based on the attacker's attack type value. */
    dam *= hitter->attack[atnr] / 100.0;
    if (dam_orig > 0 && dam < 1.0) {
        dam = 1.0;
    }

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
            poison_player(op, hitter, dam);
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
                confuse_living(op);
            } else if (atnr == ATNR_SLOW) {
                draw_info_format(COLOR_ORANGE, hitter,
                                 "You slow %s!", op->name);
                draw_info_format(COLOR_PURPLE, op,
                                 "%s slowed you!", hitter->name);
                slow_living(op);
            } else if (atnr == ATNR_PARALYZE) {
                draw_info_format(COLOR_ORANGE, hitter,
                                 "You paralyze %s!", op->name);
                draw_info_format(COLOR_PURPLE, op,
                                 "%s paralyzed you!", hitter->name);
                paralyze_living(op, dam);
            } else if (atnr == ATNR_BLIND && !QUERY_FLAG(op, FLAG_UNDEAD)) {
                draw_info_format(COLOR_ORANGE, hitter,
                                 "You blind %s!", op->name);
                draw_info_format(COLOR_PURPLE, op,
                                 "%s blinded you!", hitter->name);
                blind_living(op, hitter, dam);
            }
        }

        dam = 0.0;
        break;
    }

    default:
        ATTACK_PROTECT_DAMAGE();
        send_attack_msg(op, hitter, atnr, dam, dam_orig);
        break;
    }

    return dam;
}

/**
 * Object is attacked by something.
 *
 * This isn't used just for players, but in fact most objects.
 * @param op
 * Object to be hit.
 * @param dam
 * Base damage - protections/vulnerabilities/slaying matches
 * can modify it.
 * @param hitter
 * What is hitting the object.
 * @return
 * Dealt damage.
 */
int hit_player(object *op, int dam, object *hitter)
{
    object *hit_obj, *hitter_owner, *target_obj;
    int simple_attack;

    /* If our target has no_damage 1 set, we can't hurt him. */
    if ((op->type == PLAYER && CONTR(op)->tgm) || QUERY_FLAG(op, FLAG_INVULNERABLE)) {
        return 0;
    }

    if (hitter->head) {
        hitter = hitter->head;
    }

    if (op->head) {
        op = op->head;
    }

    /* Check if the object to hit has any HP left */
    if (op->stats.hp < 0) {
        return 0;
    }

    /* Get the hitter's owner. */
    hitter_owner = get_owner(hitter);

    /* Sanity check: If the hitter has ownercount (so it had an owner)
     * but the owner itself is no longer valid, we won't do any damage,
     * otherwise player could fire an arrow, logout, and the arrow itself
     * would cause damage to anything it hits, even friendly creatures. */
    if (hitter->ownercount && !hitter_owner) {
        return 0;
    }

    if (hitter_owner) {
        hit_obj = hitter_owner;
    } else {
        hit_obj = hitter;
    }

    if (!(target_obj = get_owner(op))) {
        target_obj = op;
    }

    /* Do not let friendly objects attack each other. */
    if (is_friend_of(hit_obj, op)) {
        return 0;
    }

    /* Check for PVP areas. */
    if (op->type == PLAYER || (get_owner(op) && op->owner->type == PLAYER)) {
        if (hitter->type == PLAYER || (get_owner(hitter) && hitter->owner->type == PLAYER)) {
            if (!pvp_area(op->type == PLAYER ? op : get_owner(op), hitter->type == PLAYER ? hitter : get_owner(hitter))) {
                return 0;
            }
        }
    }

    /* Check objects are valid, on same map and set them to head when
     * needed. */
    if (get_attack_mode(&op, &hitter, &simple_attack)) {
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
        for (_attacks atnr = 0; atnr < NROFATTACKS; atnr++) {
            if (hitter->attack[atnr] != 0) {
                maxdam += hit_player_attacktype(op,
                                                hitter,
                                                damage,
                                                dam_orig,
                                                atnr);
            }
        }
    }

    /* If one gets attacked, the attacker will become the enemy */
    if (!OBJECT_VALID(op->enemy, op->enemy_count) && !IS_INVISIBLE(hit_obj, op) && !QUERY_FLAG(hit_obj, FLAG_INVULNERABLE)) {
        set_npc_enemy(op, hit_obj, NULL);
    }

    /* This is needed to send the hit number animations to the clients */
    if (op->damage_round_tag != global_round_tag) {
        op->last_damage = 0;
        op->damage_round_tag = global_round_tag;
    }

    if (hit_obj->type == PLAYER) {
        CONTR(hit_obj)->stat_damage_dealt += maxdam;
    }

    if (target_obj->type == PLAYER) {
        CONTR(target_obj)->stat_damage_taken += maxdam;
    }

    if (hit_obj->type == PLAYER) {
        CONTR(hit_obj)->last_combat = pticks;
    }

    if (target_obj->type == PLAYER) {
        CONTR(target_obj)->last_combat = pticks;
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
    if ((op->stats.hp >= 0) && QUERY_FLAG(op, FLAG_MONSTER) && op->stats.hp < (signed short) (((float) op->run_away / 100.0f) * (float) op->stats.maxhp)) {
        SET_FLAG(op, FLAG_RUN_AWAY);
    }

    /* Reached 0 or less HP, kill the object. */
    if (op->stats.hp <= 0) {
        kill_object(op, hitter);
    }

    return maxdam;
}

/**
 * Attack a spot on the map.
 * @param op
 * Object hitting the map.
 * @param dir
 * Direction op is hitting/going.
 * @param reduce
 * Whether to reduce the damage for multi-arch monsters.
 * This will make it so that part of 4-tiles monster only gets hit for
 * 1/4 of the damage, making storms fairer against multi-arch monsters.
 */
void hit_map(object *op, int dir, int reduce)
{
    object *tmp, *owner;
    mapstruct *m;
    int x, y;
    int16_t dam;

    if (OBJECT_FREE(op)) {
        return;
    }

    op = HEAD(op);

    if (!op->map || !op->stats.dam) {
        return;
    }

    x = op->x + freearr_x[dir];
    y = op->y + freearr_y[dir];
    m = get_map_from_coord(op->map, &x, &y);

    if (!m) {
        return;
    }

    owner = get_owner(op);

    if (!owner) {
        owner = op;
    }

    owner = HEAD(owner);

    FOR_MAP_LAYER_BEGIN(m, x, y, LAYER_LIVING, -1, tmp)
    {
        tmp = HEAD(tmp);

        /* Cones with race set can only damage members of that race. */
        if (op->type == CONE && op->race && tmp->race != op->race) {
            continue;
        }

        /* Check friendship status. */
        if (is_friend_of(owner, tmp)) {
            continue;
        }

        dam = op->stats.dam;

        if (tmp->quick_pos && reduce) {
            dam /= (tmp->quick_pos >> 4) + 1;
        }

        hit_player(tmp, dam, op);
    }
    FOR_MAP_LAYER_END
}

/**
 * One player gets exp by killing a monster.
 * @param op
 * Player. This should be the killer.
 * @param exp_gain
 * Experience to gain.
 * @param skill
 * Skill that was used to kill the monster.
 */
static void share_kill_exp_one(object *op, int64_t exp_gain, object *skill)
{
    if (exp_gain) {
        add_exp(op, exp_gain, skill->stats.sp, 0);
    } else {
        draw_info(COLOR_WHITE, op, "Your enemy wasn't worth any experience to you.");
    }
}

/**
 * Share experience gained by killing a monster. This will fairly share
 * experience between party members, or if none are present, it will use
 * share_kill_exp_one() instead.
 * @param op
 * Player that killed the monster.
 * @param exp_gain
 * Experience to share.
 * @param skill
 * Skill that was used to kill the monster.
 */
static void share_kill_exp(object *op, int64_t exp_gain, object *skill)
{
    int shares = 0, count = 0;
    party_struct *party;
    objectlink *ol;

    if (!CONTR(op)->party) {
        share_kill_exp_one(op, exp_gain, skill);
        return;
    }

    party = CONTR(op)->party;

    for (ol = party->members; ol; ol = ol->next) {
        if (on_same_map(ol->objlink.ob, op)) {
            count++;
            shares += (CONTR(ol->objlink.ob)->skill_ptr[skill->stats.sp]->level + 4);
        }
    }

    if (count <= 1 || shares > exp_gain) {
        share_kill_exp_one(op, exp_gain, skill);
    } else {
        int64_t share = exp_gain / shares, given = 0, nexp;

        for (ol = party->members; ol; ol = ol->next) {
            if (ol->objlink.ob != op && on_same_map(ol->objlink.ob, op)) {
                nexp = (CONTR(ol->objlink.ob)->skill_ptr[skill->stats.sp]->level + 4) * share;
                add_exp(ol->objlink.ob, nexp, skill->stats.sp, 0);
                given += nexp;
            }
        }

        exp_gain -= given;
        share_kill_exp_one(op, exp_gain, skill);
    }
}

/**
 * An object was killed, handle various things (logging, messages, ...).
 * @param op
 * What is being killed.
 * @param hitter
 * What is hitting it.
 * @retval true Object was killed.
 * @retval false Object was not killed.
 */
bool kill_object(object *op, object *hitter)
{
    int battleg;
    int64_t exp_gain = 0;
    object *owner;

    if (op->type == PLAYER && CONTR(op)->tgm) {
        return false;
    }

    /* Trigger the DEATH event */
    if (trigger_event(EVENT_DEATH, hitter, op, NULL, NULL, 0, 0, 0, SCRIPT_FIX_ALL)) {
        return false;
    }

    /* Only when some damage is stored, and we're on a map. */
    if (op->damage_round_tag == global_round_tag && op->map) {
        SET_MAP_DAMAGE(op->map, op->x, op->y, op->sub_layer, op->last_damage);
        SET_MAP_RTAG(op->map, op->x, op->y, op->sub_layer, global_round_tag);
    }

    if (op->map) {
        play_sound_map(op->map, CMD_SOUND_EFFECT, "kill.ogg", op->x, op->y, 0, 0);
    }

    /* Figure out who to credit for the kill. */
    owner = get_owner(hitter);

    if (!owner) {
        owner = hitter;
    }

    /* Is the victim in PvP area? */
    battleg = pvp_area(NULL, op);

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

    /* Killed a player in PvP area. */
    if (battleg && op->type == PLAYER && owner->type == PLAYER) {
        draw_info(COLOR_WHITE, owner, "Your foe has fallen!\nVICTORY!!!");
    }

    /* Killed a monster and it wasn't in PvP area, so give exp. */
    if (!battleg && owner->type == PLAYER && op->type != PLAYER) {
        object *skill;

        /* Figure out the skill that should gain experience. If the hitter
         * has chosen_skill set, we will use that. */
        if (hitter->chosen_skill) {
            skill = hitter->chosen_skill;
        } else {
            /* Otherwise try to use owner's chosen_skill. */

            skill = owner->chosen_skill;
        }

        if (skill) {
            /* Calculate how much experience to gain. */
            exp_gain = calc_skill_exp(owner, op, skill->level);
            /* Give the experience, sharing it with party members if applicable.
             * */
            share_kill_exp(owner, exp_gain, skill);
        }
    }

    /* Player has been killed. */
    if (op->type == PLAYER) {
        /* Tell everyone that this player has died. */
        char *name = object_get_name_s(op, NULL);
        char *hitter_name = object_get_name_s(hitter, NULL);
        char *owner_name = object_get_name_s(owner, NULL);

        if (get_owner(hitter)) {
            draw_info_format(COLOR_WHITE, NULL, "%s killed %s with %s%s.",
                    owner_name, name, hitter_name, battleg ? " (duel)" : "");
        } else {
            draw_info_format(COLOR_WHITE, NULL, "%s killed %s%s.",
                    hitter_name, name, battleg ? " (duel)" : "");
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
        /* Monster or something else has been killed. */

        /* Remove the monster from the active list. */
        op->speed = 0.0f;
        update_ob_speed(op);

        /* Rules:
         * 1. Monster will drop corpse for his target, not the killer (unless
         * killer == target).
         * 2. NPC kill hit will overwrite player target on drop.
         * 3. Kill hit will count if target was an NPC. */
        if (owner->type != PLAYER || !op->enemy || op->enemy->type != PLAYER) {
            op->enemy = owner;
            op->enemy_count = owner->count;
        }

        /* Monster killed another monster. */
        if (hitter->type == MONSTER || (get_owner(hitter) && hitter->owner->type == MONSTER)) {
            /* No loot */
            SET_FLAG(op, FLAG_STARTEQUIP);
            /* Force an empty corpse though. */
            SET_FLAG(op, FLAG_CORPSE_FORCED);
        } else if (!exp_gain) {
            /* No exp, no loot and no corpse. */

            SET_FLAG(op, FLAG_STARTEQUIP);
        }

        destruct_ob(op);
    }

    return true;
}

/**
 * Find correct parameters for attack, do some sanity checks.
 * @param target
 * Will point to victim's head.
 * @param hitter
 * Will point to hitter's head.
 * @param simple_attack
 * Will be 1 if one of victim or target isn't on a
 * map, 0 otherwise.
 * @return
 * 0 if hitter can attack target, 1 otherwise.
 */
static int get_attack_mode(object **target, object **hitter, int *simple_attack)
{
    if (OBJECT_FREE(*target) || OBJECT_FREE(*hitter)) {
        return 1;
    }

    if ((*target)->head) {
        *target = (*target)->head;
    }

    if ((*hitter)->head) {
        *hitter = (*hitter)->head;
    }

    if ((*hitter)->env != NULL || (*target)->env != NULL || (*hitter)->map == NULL) {
        *simple_attack = 1;
        return 0;
    }

    if (QUERY_FLAG(*target, FLAG_REMOVED) || QUERY_FLAG(*hitter, FLAG_REMOVED)) {
        return 1;
    }

    *simple_attack = 0;
    return 0;
}

/**
 * Check if target and hitter are still in a relation similar to the one
 * determined by get_attack_mode().
 * @param target
 * Who is attacked.
 * @param hitter
 * Who is attacking.
 * @param simple_attack
 * Previous mode as returned by get_attack_mode().
 * @return
 * 1 if the relation has changed, 0 otherwise.
 */
static int abort_attack(object *target, object *hitter, int simple_attack)
{
    int new_mode;

    if (hitter->env == target || target->env == hitter) {
        new_mode = 1;
    } else if (QUERY_FLAG(target, FLAG_REMOVED) || QUERY_FLAG(hitter, FLAG_REMOVED) || hitter->map == NULL) {
        return 1;
    } else {
        new_mode = 0;
    }

    return new_mode != simple_attack;
}

/**
 * Poison a living thing.
 * @param op
 * Victim.
 * @param hitter
 * Who is attacking.
 * @param dam
 * Damage to deal.
 */
static void poison_player(object *op, object *hitter, float dam)
{
    archetype_t *at;
    object *tmp;
    int dam2;

    /* We only poison players and mobs! */
    if (op->type != PLAYER && !QUERY_FLAG(op, FLAG_MONSTER)) {
        return;
    }

    if (hitter->type == POISONING) {
        return;
    }

    at = arch_find("poisoning");
    tmp = present_arch_in_ob(at, op);

    dam /= 2.0f;
    dam2 = (int) ((int) dam + rndm(0, dam + 1));

    if (dam2 > op->stats.maxhp / 3) {
        dam2 = op->stats.maxhp / 3;
    } else if (dam2 < 1) {
        dam2 = 1;
    }

    if (tmp == NULL) {
        if ((tmp = arch_to_object(at)) == NULL) {
            LOG(BUG, "Failed to clone arch poisoning.");
            return;
        }

        tmp->level = hitter->level;
        tmp->stats.dam = dam2;

        /* So we get credit for poisoning kills */
        if (IS_LIVE(hitter)) {
            set_owner(tmp, hitter);
        }

        SET_FLAG(tmp, FLAG_APPLIED);
        insert_ob_in_ob(tmp, op);

        if (op->type == PLAYER) {
            char *name = object_get_name_s(hitter, op);
            draw_info_format(COLOR_WHITE, op, "%s has poisoned you!", name);
            efree(name);
        } else {
            if (hitter->type == PLAYER) {
                char *name = object_get_name_s(op, hitter);
                draw_info_format(COLOR_WHITE, hitter, "You poisoned %s!", name);
                efree(name);
            } else if (get_owner(hitter) && hitter->owner->type == PLAYER) {
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
 * Slow a living thing.
 * @param op
 * Victim.
 */
static void slow_living(object *op)
{
    archetype_t *at = arch_find("slowness");
    object *tmp;

    if (at == NULL) {
        LOG(BUG, "Can't find slowness archetype.");
        return;
    }

    if ((tmp = present_arch_in_ob(at, op)) == NULL) {
        tmp = arch_to_object(at);
        SET_FLAG(tmp, FLAG_APPLIED);
        tmp = insert_ob_in_ob(tmp, op);
        draw_info(COLOR_WHITE, op, "The world suddenly moves very fast!");
    } else {
        tmp->stats.food++;
    }

    tmp->speed_left = 0;
    esrv_update_item(UPD_EXTRA, tmp);
}

/**
 * Confuse a living thing.
 * @param op
 * Victim.
 */
void confuse_living(object *op)
{
    archetype_t *at;
    object *tmp;
    int maxduration;

    at = arch_find("confusion");
    tmp = present_arch_in_ob(at, op);

    if (!tmp) {
        tmp = arch_to_object(at);
        SET_FLAG(tmp, FLAG_APPLIED);
        tmp = insert_ob_in_ob(tmp, op);
    }

    /* Duration added per hit and max. duration of confusion both depend
     * on the player's resistance */
    tmp->stats.food += MAX(1, 5 * (100 - op->protection[ATNR_CONFUSION]) / 100);
    maxduration = MAX(2, 30 * (100 - op->protection[ATNR_CONFUSION]) / 100);

    if (tmp->stats.food > maxduration) {
        tmp->stats.food = maxduration;
    }

    tmp->speed_left = 0;
    esrv_update_item(UPD_EXTRA, tmp);
}

/**
 * Blind a living thing.
 * @param op
 * Victim.
 * @param hitter
 * Who is attacking.
 * @param dam
 * Damage to deal.
 */
void blind_living(object *op, object *hitter, int dam)
{
    archetype_t *at;
    object *tmp, *owner;

    /* Save some work if we know it isn't going to affect the player */
    if (op->protection[ATNR_BLIND] == 100) {
        return;
    }

    at = arch_find("blindness");
    tmp = present_arch_in_ob(at, op);

    if (!tmp) {
        tmp = arch_to_object(at);
        SET_FLAG(tmp, FLAG_APPLIED);
        /* Use floats so we don't lose too much precision due to rounding
         * errors.
         * speed is a float anyways. */
        tmp->speed = tmp->speed * ((float) 100.0 - (float) op->protection[ATNR_BLIND]) / (float) 100;

        tmp = insert_ob_in_ob(tmp, op);

        if (hitter != op) {
            if (hitter->owner) {
                owner = get_owner(hitter);
            } else {
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
 * @param op
 * Victim.
 * @param dam
 * Damage to deal.
 */
void paralyze_living(object *op, int dam)
{
    double effect, max;

    effect = (double) dam * 3.0;
    effect = effect * (100.0 - (double) op->protection[ATNR_PARALYZE]) / 100.0;

    if (effect < 0.000001) {
        return;
    }

    /* We mark this object as paralyzed */
    SET_FLAG(op, FLAG_PARALYZED);

    op->speed_left -= FABS(op->speed) * effect;

    /* Max number of ticks to be affected for. */
    max = (100.0 - (double) op->protection[ATNR_PARALYZE]) / 2.0;

    if (op->speed_left < -(FABS(op->speed) * max)) {
        op->speed_left = -(FABS(op->speed) * max);
    }
}

/**
 * Cause damage due to falling.
 * @param op
 * Object.
 * @param fall_floors
 * Number of floors the object fell down.
 */
void fall_damage_living(object *op, int fall_floors)
{
    HARD_ASSERT(op != NULL);
    SOFT_ASSERT(IS_LIVE(op), "Object is not alive: %s", object_get_str(op));

    int8_t dex = op->type == PLAYER ? op->stats.Dex : MAX_STAT / 2;

    if (((MAX_STAT - dex + MAX_STAT - rndm(1, dex))) * MAX(1, fall_floors - 1) <
            MAX_STAT / 4) {
        return;
    }

    object *damager = arch_get("falling");
    damager->level = op->level;
    damager->stats.dam = ((op->weight + op->carrying) / 2500 *
            MIN(10, fall_floors)) * falling_mitigation[op->stats.Dex];

    if (damager->stats.dam <= 0) {
        damager->stats.dam = 1;
    }

    damager->stats.dam = rndm(damager->stats.dam / 2 + 1,
            damager->stats.dam + 1) - 1;

    hit_player(op, damager->stats.dam, damager);
    object_destroy(damager);
}

/**
 * Adjustments to attack rolls by various conditions.
 * @param hitter
 * Who is hitting.
 * @param target
 * Victim of the attack.
 * @return
 * Adjustment to attack roll.
 */
static int adj_attackroll(object *hitter, object *target)
{
    object *attacker = hitter;
    int adjust = 0;

    /* Safety */
    if (!target || !hitter || !hitter->map || !target->map || !on_same_map(hitter, target)) {
        return 0;
    }

    /* Aimed missiles use the owning object's sight */
    if (is_aimed_missile(hitter)) {
        if ((attacker = get_owner(hitter)) == NULL) {
            attacker = hitter;
        }
    } else if (!IS_LIVE(hitter)) {
        return 0;
    }

    /* Invisible means, we can't see it - same for blind */
    if (IS_INVISIBLE(target, attacker) || QUERY_FLAG(attacker, FLAG_BLIND)) {
        adjust -= 12;
    }

    if (QUERY_FLAG(attacker, FLAG_SCARED)) {
        adjust -= 3;
    }

    if (QUERY_FLAG(target, FLAG_UNAGGRESSIVE)) {
        adjust += 1;
    }

    if (QUERY_FLAG(target, FLAG_SCARED)) {
        adjust += 1;
    }

    if (QUERY_FLAG(attacker, FLAG_CONFUSED)) {
        adjust -= 3;
    }

    /* If we attack at a different 'altitude' it's harder */
    if (QUERY_FLAG(attacker, FLAG_FLYING) != QUERY_FLAG(target, FLAG_FLYING)) {
        adjust -= 2;
    }

    if (hitter->direction == target->direction) {
        /* Backstab */
        adjust += 5;
    } else if (hitter->direction == absdir(target->direction - 1) ||
            hitter->direction == absdir(target->direction + 1)) {
        /* Sidestab */
        adjust += 2;
    }

    return adjust;
}

/**
 * Determine if the object is an 'aimed' missile.
 * @param op
 * Object to check.
 * @return
 * 1 if aimed missile, 0 otherwise.
 */
static int is_aimed_missile(object *op)
{
    if (op && QUERY_FLAG(op, FLAG_FLYING) && op->type == ARROW) {
        return 1;
    }

    return 0;
}

/**
 * Test if objects are in range for melee attack.
 * @param hitter
 * Attacker.
 * @param enemy
 * Enemy -- the target.
 * @return
 * True if the target is in melee range, false otherwise.
 */
bool is_melee_range(object *hitter, object *enemy)
{
    HARD_ASSERT(hitter != NULL);
    HARD_ASSERT(enemy != NULL);

    SOFT_ASSERT_RC(hitter->head == NULL, false, "Called on tail part: %s",
            object_get_str(hitter));
    SOFT_ASSERT_RC(enemy->head == NULL, false, "Called on tail part: %s",
            object_get_str(enemy));

    for (object *hitter_part = hitter; hitter_part != NULL;
            hitter_part = hitter_part->more) {
        for (int i = 0; i <= SIZEOFFREE1; i++) {
            int x = hitter_part->x + freearr_x[i];
            int y = hitter_part->y + freearr_y[i];
            mapstruct *m = get_map_from_coord(hitter_part->map, &x, &y);
            if (m == NULL) {
                continue;
            }

            for (object *enemy_part = enemy; enemy_part != NULL;
                    enemy_part = enemy_part->more) {
                if (enemy_part->map == m && enemy_part->x == x &&
                        enemy_part->y == y) {
                    return true;
                }
            }
        }
    }

    return false;
}
