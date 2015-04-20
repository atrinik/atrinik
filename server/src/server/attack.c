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
 * This handles all attacks, magical or not. */

#include <global.h>

/**
 * Names of attack types to use when saving them to file.
 * @warning Cannot contain spaces. Use underscores instead. */
char *attack_save[NROFATTACKS] = {
    "impact",   "slash", "cleave",      "pierce",    "weaponmagic",
    "fire",     "cold",  "electricity", "poison",    "acid",
    "magic",    "mind",  "blind",       "paralyze",  "force",
    "godpower", "chaos", "drain",       "slow",      "confusion",
    "internal"
};

/**
 * Short description of names of the attack types. */
char *attack_name[NROFATTACKS] = {
    "impact",   "slash", "cleave",      "pierce",    "weapon magic",
    "fire",     "cold",  "electricity", "poison",    "acid",
    "magic",    "mind",  "blind",       "paralyze",  "force",
    "godpower", "chaos", "drain",       "slow",      "confusion",
    "internal"
};

#define ATTACK_HIT_DAMAGE(_op, _anum)       dam = dam * ((double) _op->attack[_anum] * (double) 0.01); dam >= 1.0f ? (damage = (int) dam) : (damage = 1)
#define ATTACK_PROTECT_DAMAGE(_op, _anum)   dam = dam * ((double) (100 - _op->protection[_anum]) * (double) 0.01)

static int get_attack_mode(object **target, object **hitter, int *simple_attack);
static int abort_attack(object *target, object *hitter, int simple_attack);
static int attack_ob_simple(object *op, object *hitter, int base_dam, int base_wc);
static void send_attack_msg(object *op, object *hitter, int attacknum, int dam, int damage);
static int hit_player_attacktype(object *op, object *hitter, int damage, uint32_t attacknum);
static void poison_player(object *op, object *hitter, float dam);
static void slow_living(object *op);
static void blind_living(object *op, object *hitter, int dam);
static int adj_attackroll(object *hitter, object *target);
static int is_aimed_missile(object *op);

/**
 * Simple wrapper for attack_ob_simple(), will use hitter's values.
 * @param op Victim.
 * @param hitter Attacker.
 * @return Dealt damage. */
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
 * @param op Victim.
 * @param hitter Attacker.
 * @param base_dam Damage to do.
 * @param base_wc WC to hit with.
 * @return Dealt damage. */
static int attack_ob_simple(object *op, object *hitter, int base_dam, int base_wc)
{
    int simple_attack, roll, dam = 0;
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

    /* Adjust roll for various situations. */
    if (!simple_attack) {
        roll += adj_attackroll(hitter, op);
    }

    hitter->anim_flags |= ANIM_FLAG_ATTACKING;
    hitter->anim_flags &= ~ANIM_FLAG_STOP_ATTACKING;

    if (get_rangevector(hitter, op, &dir, RV_NO_DISTANCE)) {
        HEAD(hitter)->direction = dir.direction;
    }

    /* See if we hit the creature */
    if (roll >= hitter->stats.wc_range || op->stats.ac <= base_wc + roll) {
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
            hit_player(hitter, rndm(0, op->stats.dam), op, AT_PHYSICAL);

            if (was_destroyed(op, op_tag) || was_destroyed(hitter, hitter_tag) || abort_attack(op, hitter, simple_attack)) {
                return dam;
            }
        }

        dam = hit_player(op, rndm(hitdam / 2 + 1, hitdam), hitter, AT_PHYSICAL);

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
 * Object is attacked by something.
 *
 * This isn't used just for players, but in fact most objects.
 * @param op Object to be hit.
 * @param dam Base damage - protections/vulnerabilities/slaying matches
 * can modify it.
 * @param hitter What is hitting the object.
 * @param type Attacktype.
 * @return Dealt damage. */
int hit_player(object *op, int dam, object *hitter, int type)
{
    object *hit_obj, *hitter_owner, *target_obj;
    int maxdam = 0;
    int attacknum, hit_level;
    int simple_attack;
    int rtn_kill = 0;

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

    /* Get from hitter object the right skill level. */
    if (hit_obj->type == PLAYER) {
        hit_level = SK_level(hit_obj);
    } else {
        hit_level = hitter->level;
    }

    /* Do not let friendly objects attack each other. */
    if (is_friend_of(hit_obj, op)) {
        return 0;
    }

    if (hit_level > target_obj->level && hit_obj->type == MONSTER) {
        dam += (int) ((float) (dam / 2) * ((float) (hit_level - target_obj->level) / (target_obj->level > 25 ? 25.0f : (float) target_obj->level)));
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

    /* Go through and hit the player with each attacktype, one by one.
     * hit_player_attacktype only figures out the damage, doesn't inflict
     * it. It will do the appropriate action for attacktypes with
     * effects (slow, paralization, etc). */
    for (attacknum = 0; attacknum < NROFATTACKS; attacknum++) {
        if (hitter->attack[attacknum]) {
            maxdam += hit_player_attacktype(op, hitter, dam, attacknum);
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

    op->last_damage += maxdam;

    /* Damage the target got */
    op->stats.hp -= maxdam;

    /* Check to see if monster runs away. */
    if ((op->stats.hp >= 0) && QUERY_FLAG(op, FLAG_MONSTER) && op->stats.hp < (signed short) (((float) op->run_away / 100.0f) * (float) op->stats.maxhp)) {
        SET_FLAG(op, FLAG_RUN_AWAY);
    }

    /* rtn_kill is here negative! */
    if ((rtn_kill = kill_object(op, dam, hitter, type))) {
        return (maxdam + rtn_kill + 1);
    }

    return maxdam;
}

/**
 * Attack a spot on the map.
 * @param op Object hitting the map.
 * @param dir Direction op is hitting/going.
 * @param reduce Whether to reduce the damage for multi-arch monsters.
 * This will make it so that part of 4-tiles monster only gets hit for
 * 1/4 of the damage, making storms fairer against multi-arch monsters. */
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

        hit_player(tmp, dam, op, AT_INTERNAL);
    }
    FOR_MAP_LAYER_END
}

/**
 * Handles one attacktype's damage.
 *
 * This doesn't damage the creature, but returns how much it should
 * take. However, it will do other effects (paralyzation, slow, etc).
 * @param op Victim of the attack.
 * @param hitter Attacker.
 * @param damage Maximum dealt damage.
 * @param attacknum Number of the attacktype of the attack.
 * @return Damage to actually do. */
static int hit_player_attacktype(object *op, object *hitter, int damage, uint32_t attacknum)
{
    double dam = (double) damage;

    /* Sanity check */
    if (dam < 0) {
        return 0;
    }

    if (hitter->slaying) {
        if (((op->race != NULL) && strstr(hitter->slaying, op->race)) || (op->arch && (op->arch->name != NULL) &&  strstr(op->arch->name, hitter->slaying))) {
            if (QUERY_FLAG(hitter, FLAG_IS_ASSASSINATION)) {
                damage = (int) ((double) damage * 2.25);
            } else {
                damage = (int) ((double) damage * 1.75);
            }

            dam = (double) damage;
        }
    }

    /* AT_INTERNAL is supposed to do exactly dam. Put a case here so
     * people can't mess with that or it otherwise get confused. */
    if (attacknum == ATNR_INTERNAL) {
        /* Adjust damage */
        dam = dam * ((double) hitter->attack[ATNR_INTERNAL] / 100.0);

        /* handle special object attacks */
        /* we have a poison force object (that's the poison we had inserted) */
        if (hitter->type == POISONING) {
            /* Map to poison... */
            attacknum = ATNR_POISON;

            if (op->protection[attacknum] == 100) {
                dam = 0;
                send_attack_msg(op, hitter, attacknum, (int) dam, damage);
                return 0;
            }

            /* Reduce to % protection */
            ATTACK_PROTECT_DAMAGE(op, attacknum);
        }

        if (damage && dam < 1.0) {
            dam = 1.0;
        }

        send_attack_msg(op, hitter, attacknum, (int) dam, damage);
        return (int) dam;
    }

    /* Quick check for immunity - if so, we skip here.
     * Our formula is (100 - resist) / 100 - so test for 100 = zero division */
    if (op->protection[attacknum] == 100) {
        ATTACK_HIT_DAMAGE(hitter, attacknum);
        send_attack_msg(op, hitter, attacknum, 0, dam);
        return 0;
    }

    switch (attacknum) {
    case ATNR_IMPACT:
    case ATNR_SLASH:
    case ATNR_CLEAVE:
    case ATNR_PIERCE:
        check_physically_infect(op, hitter);

        ATTACK_HIT_DAMAGE(hitter, attacknum);
        ATTACK_PROTECT_DAMAGE(op, attacknum);

        if (damage && dam < 1.0) {
            dam = 1.0;
        }

        send_attack_msg(op, hitter, attacknum, (int) dam, damage);
        break;

    case ATNR_POISON:
        ATTACK_HIT_DAMAGE(hitter, attacknum);
        ATTACK_PROTECT_DAMAGE(op, attacknum);

        if (damage && dam < 1.0) {
            dam = 1.0;
        }

        send_attack_msg(op, hitter, attacknum, (int) dam, damage);

        if (dam && IS_LIVE(op)) {
            poison_player(op, hitter, (float) dam);
        }

        break;

    case ATNR_CONFUSION:
    case ATNR_SLOW:
    case ATNR_PARALYZE:
    case ATNR_BLIND:
    {
        int level_diff = MIN(MAXLEVEL, MAX(0, op->level - hitter->level));

        if (op->speed && (QUERY_FLAG(op, FLAG_MONSTER) || op->type == PLAYER) && !(rndm(0, (attacknum == ATNR_SLOW ? 6 : 3) - 1)) && ((rndm(1, 20) + op->protection[attacknum] / 10) < savethrow[level_diff])) {
            if (attacknum == ATNR_CONFUSION) {
                if (hitter->type == PLAYER) {
                    draw_info_format(COLOR_ORANGE, hitter, "You confuse %s!", op->name);
                }

                if (op->type == PLAYER) {
                    draw_info_format(COLOR_PURPLE, op, "%s confused you!", hitter->name);
                }

                confuse_living(op);
            } else if (attacknum == ATNR_SLOW) {
                if (hitter->type == PLAYER) {
                    draw_info_format(COLOR_ORANGE, hitter, "You slow %s!", op->name);
                }

                if (op->type == PLAYER) {
                    draw_info_format(COLOR_PURPLE, op, "%s slowed you!", hitter->name);
                }

                slow_living(op);
            } else if (attacknum == ATNR_PARALYZE) {
                if (hitter->type == PLAYER) {
                    draw_info_format(COLOR_ORANGE, hitter, "You paralyze %s!", op->name);
                }

                if (op->type == PLAYER) {
                    draw_info_format(COLOR_PURPLE, op, "%s paralyzed you!", hitter->name);
                }

                paralyze_living(op, (int) dam);
            } else if (attacknum == ATNR_BLIND && !QUERY_FLAG(op, FLAG_UNDEAD)) {
                if (hitter->type == PLAYER) {
                    draw_info_format(COLOR_ORANGE, hitter, "You blind %s!", op->name);
                }

                if (op->type == PLAYER) {
                    draw_info_format(COLOR_PURPLE, op, "%s blinded you!", hitter->name);
                }

                blind_living(op, hitter, (int) dam);
            }
        }

        dam = 0;
    }

        break;

    default:
        ATTACK_HIT_DAMAGE(hitter, attacknum);
        ATTACK_PROTECT_DAMAGE(op, attacknum);

        if (damage && dam < 1.0) {
            dam = 1.0;
        }

        send_attack_msg(op, hitter, attacknum, (int) dam, damage);
        break;
    }

    return (int) dam;
}

/**
 * Send attack message for players.
 * @param op Victim of the attack.
 * @param hitter Attacker.
 * @param attacknum ID of the attack type.
 * @param dam Actual damage done.
 * @param damage How much damage should have been done, not counting
 * resists/protections/etc. */
static void send_attack_msg(object *op, object *hitter, int attacknum, int dam, int damage)
{
    object *orig_hitter = hitter;

    if (op->type == PLAYER) {
        draw_info_format(COLOR_PURPLE, op, "%s hit you for %d (%d) damage.", hitter->name, dam, dam - damage);
    }

    if (hitter->type == PLAYER || ((hitter = get_owner(hitter)) && hitter->type == PLAYER)) {
        draw_info_format(COLOR_ORANGE, hitter, "You hit %s for %d (%d) with %s.", op->name, dam, dam - damage, attacknum == ATNR_INTERNAL ? orig_hitter->name : attack_name[attacknum]);
    }
}

/**
 * One player gets exp by killing a monster.
 * @param op Player. This should be the killer.
 * @param exp_gain Experience to gain.
 * @param skill Skill that was used to kill the monster. */
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
 * @param op Player that killed the monster.
 * @param exp_gain Experience to share.
 * @param skill Skill that was used to kill the monster. */
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
 * @param op What is being killed.
 * @param dam Damage done to it.
 * @param hitter What is hitting it.
 * @param type The attacktype.
 * @return Dealt damage. */
int kill_object(object *op, int dam, object *hitter, int type)
{
    int maxdam, battleg;
    int64_t exp_gain = 0;
    object *owner;

    /* Still got some HP left? */
    if (op->stats.hp > 0) {
        return -1;
    }

    if (op->type == PLAYER && CONTR(op)->tgm) {
        return 0;
    }

    /* Trigger the DEATH event */
    if (trigger_event(EVENT_DEATH, hitter, op, NULL, NULL, type, 0, 0, SCRIPT_FIX_ALL)) {
        return 0;
    }

    maxdam = op->stats.hp - 1;

    /* Only when some damage is stored, and we're on a map. */
    if (op->damage_round_tag == global_round_tag && op->map) {
        SET_MAP_DAMAGE(op->map, op->x, op->y, op->last_damage);
        SET_MAP_RTAG(op->map, op->x, op->y, global_round_tag);
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
        if (owner != hitter) {
            draw_info_format(COLOR_WHITE, owner, "You killed %s with %s.", query_name(op, NULL), query_name(hitter, NULL));
        } else {
            draw_info_format(COLOR_WHITE, owner, "You killed %s.", query_name(op, NULL));
        }

        if (op->type == MONSTER) {
            shstr *faction, *faction_kill_penalty;

            CONTR(owner)->stat_kills_mob++;
            statistic_update("kills", owner, 1, op->name);

            if ((faction = object_get_value(op, "faction")) && (faction_kill_penalty = object_get_value(op, "faction_kill_penalty"))) {
                player_faction_reputation_update(CONTR(owner), faction, -atoll(faction_kill_penalty));
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
        if (get_owner(hitter)) {
            draw_info_format(COLOR_WHITE, NULL, "%s killed %s with %s%s.", hitter->owner->name, query_name(op, NULL), query_name(hitter, NULL), battleg ? " (duel)" : "");
        } else {
            draw_info_format(COLOR_WHITE, NULL, "%s killed %s%s.", hitter->name, op->name, battleg ? " (duel)" : "");
        }

        /* Update player's killer. */
        if (owner->type == PLAYER) {
            char race[MAX_BUF];

            snprintf(CONTR(op)->killer, sizeof(CONTR(op)->killer), "%s the %s", owner->name, player_get_race_class(owner, race, sizeof(race)));
        } else {
            snprintf(CONTR(op)->killer, sizeof(CONTR(op)->killer), "%s", owner->name);
        }

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

    return maxdam;
}

/**
 * Find correct parameters for attack, do some sanity checks.
 * @param target Will point to victim's head.
 * @param hitter Will point to hitter's head.
 * @param simple_attack Will be 1 if one of victim or target isn't on a
 * map, 0 otherwise.
 * @return 0 if hitter can attack target, 1 otherwise. */
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
 * @param target Who is attacked.
 * @param hitter Who is attacking.
 * @param simple_attack Previous mode as returned by get_attack_mode().
 * @return 1 if the relation has changed, 0 otherwise. */
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
 * @param op Victim.
 * @param hitter Who is attacking.
 * @param dam Damage to deal. */
static void poison_player(object *op, object *hitter, float dam)
{
    archetype *at;
    object *tmp;
    int dam2;

    /* We only poison players and mobs! */
    if (op->type != PLAYER && !QUERY_FLAG(op, FLAG_MONSTER)) {
        return;
    }

    if (hitter->type == POISONING) {
        return;
    }

    at = find_archetype("poisoning");
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
            logger_print(LOG(BUG), "Failed to clone arch poisoning.");
            return;
        }

        tmp->level = hitter->level;
        tmp->stats.dam = dam2;
        /* So we get credit for poisoning kills */
        copy_owner(tmp, hitter);
        SET_FLAG(tmp, FLAG_APPLIED);
        insert_ob_in_ob(tmp, op);

        if (op->type == PLAYER) {
            draw_info_format(COLOR_WHITE, op, "%s has poisoned you!", query_name(hitter, NULL));
        } else {
            if (hitter->type == PLAYER) {
                draw_info_format(COLOR_WHITE, hitter, "You poisoned %s!", query_name(op, NULL));
            } else if (get_owner(hitter) && hitter->owner->type == PLAYER) {
                draw_info_format(COLOR_WHITE, hitter->owner, "%s poisoned %s!", query_name(hitter, NULL), query_name(op, NULL));
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
 * @param op Victim. */
static void slow_living(object *op)
{
    archetype *at = find_archetype("slowness");
    object *tmp;

    if (at == NULL) {
        logger_print(LOG(BUG), "Can't find slowness archetype.");
        return;
    }

    if ((tmp = present_arch_in_ob(at, op)) == NULL) {
        tmp = arch_to_object(at);
        tmp = insert_ob_in_ob(tmp, op);
        draw_info(COLOR_WHITE, op, "The world suddenly moves very fast!");
    } else {
        tmp->stats.food++;
    }

    SET_FLAG(tmp, FLAG_APPLIED);
    tmp->speed_left = 0;
}

/**
 * Confuse a living thing.
 * @param op Victim. */
void confuse_living(object *op)
{
    object *tmp;
    int maxduration;

    tmp = present_in_ob(CONFUSION, op);

    if (!tmp) {
        tmp = get_archetype("confusion");
        tmp = insert_ob_in_ob(tmp, op);
    }

    /* Duration added per hit and max. duration of confusion both depend
     * on the player's resistance */
    tmp->stats.food += MAX(1, 5 * (100 - op->protection[ATNR_CONFUSION]) / 100);
    maxduration = MAX(2, 30 * (100 - op->protection[ATNR_CONFUSION]) / 100);

    if (tmp->stats.food > maxduration) {
        tmp->stats.food = maxduration;
    }

    if (op->type == PLAYER && !QUERY_FLAG(op, FLAG_CONFUSED)) {
        draw_info(COLOR_WHITE, op, "You suddenly feel very confused!");
    }

    SET_FLAG(op, FLAG_CONFUSED);
}

/**
 * Blind a living thing.
 * @param op Victim.
 * @param hitter Who is attacking.
 * @param dam Damage to deal. */
static void blind_living(object *op, object *hitter, int dam)
{
    object *tmp, *owner;

    /* Save some work if we know it isn't going to affect the player */
    if (op->protection[ATNR_BLIND] == 100) {
        return;
    }

    tmp = present_in_ob(BLINDNESS, op);

    if (!tmp) {
        tmp = get_archetype("blindness");
        SET_FLAG(tmp, FLAG_BLIND);
        SET_FLAG(tmp, FLAG_APPLIED);
        /* Use floats so we don't lose too much precision due to rounding
         * errors.
         * speed is a float anyways. */
        tmp->speed = tmp->speed * ((float) 100.0 - (float) op->protection[ATNR_BLIND]) / (float) 100;

        tmp = insert_ob_in_ob(tmp, op);

        if (hitter->owner) {
            owner = get_owner(hitter);
        } else {
            owner = hitter;
        }

        draw_info_format(COLOR_WHITE, owner, "Your attack blinds %s!", query_name(op, NULL));
    }

    tmp->stats.food += dam;

    if (tmp->stats.food > 10) {
        tmp->stats.food = 10;
    }
}

/**
 * Paralyze a living thing.
 * @param op Victim.
 * @param dam Damage to deal. */
void paralyze_living(object *op, int dam)
{
    float effect, max;

    /* Do this as a float - otherwise, rounding might very well reduce this to 0
     * */
    effect = (float) dam * (float) 3.0 * ((float) 100.0 - (float) op->protection[ATNR_PARALYZE]) / (float) 100;

    if (effect == 0) {
        return;
    }

    /* We mark this object as paralyzed */
    SET_FLAG(op, FLAG_PARALYZED);

    op->speed_left -= FABS(op->speed) * effect;

    /* Max number of ticks to be affected for. */
    max = ((float) 100 - (float) op->protection[ATNR_PARALYZE]) / (float) 2;

    if (op->speed_left < -(FABS(op->speed) * max)) {
        op->speed_left = (float) -(FABS(op->speed) * max);
    }
}

/**
 * Adjustments to attack rolls by various conditions.
 * @param hitter Who is hitting.
 * @param target Victim of the attack.
 * @return Adjustment to attack roll. */
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

    return adjust;
}

/**
 * Determine if the object is an 'aimed' missile.
 * @param op Object to check.
 * @return 1 if aimed missile, 0 otherwise. */
static int is_aimed_missile(object *op)
{
    if (op && QUERY_FLAG(op, FLAG_FLYING) && op->type == ARROW) {
        return 1;
    }

    return 0;
}

/**
 * Test if objects are in range for melee attack.
 * @param hitter Attacker.
 * @param enemy Enemy.
 * @retval 0 Enemy target is not in melee range.
 * @retval 1 Target is in range and we're facing it. */
int is_melee_range(object *hitter, object *enemy)
{
    int xt, yt, s;
    object *tmp;
    mapstruct *mt;

    /* Check squares around */
    for (s = 0; s < 9; s++) {
        xt = hitter->x + freearr_x[s];
        yt = hitter->y + freearr_y[s];

        if (!(mt = get_map_from_coord(hitter->map, &xt, &yt))) {
            continue;
        }

        for (tmp = enemy; tmp != NULL; tmp = tmp->more) {
            /* Strike! */
            if (tmp->map == mt && tmp->x == xt && tmp->y == yt) {
                return 1;
            }
        }
    }

    return 0;
}
