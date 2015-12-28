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
 * Various spell effects.
 */

#include <global.h>
#include <arch.h>
#include <player.h>
#include <object.h>

/**
 * This is really used mostly for spell fumbles at the like.
 * @param op
 * What is casting this.
 * @param tmp
 * Object to propagate.
 * @param lvl
 * How nasty should the propagation be.
 */
void cast_magic_storm(object *op, object *tmp, int lvl)
{
    /* Error */
    if (!tmp) {
        return;
    }

    tmp->level = SK_level(op);
    tmp->x = op->x;
    tmp->y = op->y;

    /* increase the area of destruction */
    tmp->stats.hp += lvl / 5;
    /* nasty recoils! */
    tmp->stats.dam = lvl;
    tmp->stats.maxhp = tmp->count;
    insert_ob_in_map(tmp, op->map, op, 0);
}

/**
 * Recharge wands.
 * @param op
 * Who is casting.
 * @retval 0 Nothing happened.
 * @retval 1 Wand was recharged, or destroyed.
 */
int recharge(object *op)
{
    object *wand = find_marked_object(op);
    int cap;

    if (wand == NULL || wand->type != WAND) {
        draw_info(COLOR_RED, op, "You need to mark the wand you want to recharge.");
        return 0;
    }

    char *name = object_get_name_s(wand, op);

    if (wand->stats.sp < 0 || wand->stats.sp >= NROFREALSPELLS || !spells[wand->stats.sp].charges) {
        draw_info_format(COLOR_RED, op, "The %s cannot be recharged.", name);
        efree(name);
        return 0;
    }

    if (rndm_chance(6)) {
        draw_info_format(COLOR_WHITE, op, "The %s vibrates violently, then "
                "explodes!", name);
        play_sound_map(op->map, CMD_SOUND_EFFECT, "explosion.ogg", op->x, op->y, 0, 0);
        object_remove(wand, 0);
        object_destroy(wand);
        efree(name);
        return 1;
    }

    draw_info_format(COLOR_WHITE, op, "The %s glows with power.", name);

    wand->stats.food += 12 + rndm(1, spells[wand->stats.sp].charges);
    cap = spells[wand->stats.sp].charges + 12;

    /* Place a cap on it. */
    if (wand->stats.food > cap) {
        wand->stats.food = cap;
    }

    if (wand->arch && QUERY_FLAG(&wand->arch->clone, FLAG_ANIMATE)) {
        SET_FLAG(wand, FLAG_ANIMATE);
        wand->speed = wand->arch->clone.speed;
        update_ob_speed(wand);
    }

    efree(name);
    return 1;
}

/**
 * Create food.
 *
 * Allows the choice of what sort of food object to make.
 * If stringarg is NULL, it will create food dependent on level.
 * @param op
 * Who is casting.
 * @param caster
 * What is casting.
 * @param dir
 * Casting direction.
 * @param stringarg
 * Optional parameter specifying what kind of items to
 * create.
 * @retval 0 No food created.
 * @retval 1 Food was created.
 * @todo Looping the global arch table is not an ideal case in terms of
 * performance...
 */
int cast_create_food(object *op, object *caster, int dir, const char *stringarg)
{
    int food_value = 50 * SP_level_dam_adjust(caster, SP_CREATE_FOOD, false);

    archetype_t *at = NULL;
    if (stringarg != NULL) {
        at = arch_find(stringarg);

        if (at == NULL || ((at->clone.type != FOOD &&
                at->clone.type != DRINK) || (at->clone.stats.food >
                food_value))) {
            stringarg = NULL;
            at = NULL;
        }
    }

    if (stringarg == NULL) {
        archetype_t *at_tmp, *tmp;
        HASH_ITER(hh, arch_table, at_tmp, tmp) {
            /* Not food or a drink */
            if (at_tmp->clone.type != FOOD && at_tmp->clone.type != DRINK) {
                continue;
            }

            /* Food value is higher than what is creatable, skip. */
            if (at_tmp->clone.stats.food > food_value) {
                continue;
            }

            /* Don't have a food arch yet, or the current one has a higher food
             * value, so take it instead. */
            if (at == NULL || at_tmp->clone.stats.food > at->clone.stats.food) {
                at = at_tmp;
            }
        }
    }

    /* Pretty unlikely (there are some very low food items), but you
     * never know */
    if (!at) {
        draw_info(COLOR_WHITE, op, "You don't have enough experience to create any food.");
        return 0;
    }

    food_value /= at->clone.stats.food;
    object *new_op = get_object();
    copy_object(&at->clone, new_op, 0);
    new_op->nrof = food_value;

    new_op->value = 0;
    SET_FLAG(new_op, FLAG_STARTEQUIP);
    SET_FLAG(new_op, FLAG_IDENTIFIED);

    if (new_op->nrof < 1) {
        new_op->nrof = 1;
    }

    cast_create_obj(op, new_op, dir);
    return 1;
}

/**
 * Word of recall causes the player to return 'home'.
 *
 * We put a force into the player object, so that there is a time delay
 * effect.
 * @param op
 * Who is casting.
 * @param caster
 * What is casting.
 * @return
 * 1 on success, 0 otherwise.
 */
int cast_wor(object *op, object *caster)
{
    object *dummy;

    if (op->type != PLAYER) {
        return 0;
    }

    if (blocks_magic(op->map, op->x, op->y)) {
        draw_info(COLOR_WHITE, op, "Something blocks your spell.");
        return 0;
    }

    dummy = arch_get("force");
    SOFT_ASSERT_RC(dummy != NULL, 0, "Failed to find 'force' archetype, "
            "op: %s, caster: %s", object_get_str(op), object_get_str(caster));

    /* Better insert the spell in the player */
    if (op->owner) {
        op = op->owner;
    }

    dummy->speed = 0.002f * ((float) (spells[SP_WOR].bdur + SP_level_strength_adjust(caster, SP_WOR)));
    update_ob_speed(dummy);
    dummy->speed_left = -1;
    dummy->type = WORD_OF_RECALL;

    FREE_AND_COPY_HASH(EXIT_PATH(dummy), CONTR(op)->savebed_map);
    EXIT_X(dummy) = CONTR(op)->bed_x;
    EXIT_Y(dummy) = CONTR(op)->bed_y;

    insert_ob_in_ob(dummy, op);
    draw_info(COLOR_WHITE, op, "You feel a force starting to build up inside you.");

    return 1;
}

/**
 * Hit all enemies around the caster.
 * @param op
 * Who is casting.
 * @param caster
 * What object is casting.
 * @param dam
 * Base damage to do.
 * @param attacktype
 * Attacktype.
 */
void cast_destruction(object *op, object *caster, int dam)
{
    int i, j, range, xt, yt;
    object *tmp, *hitter;
    mapstruct *m;

    /* The hitter object. */
    hitter = arch_to_object(spellarch[SP_DESTRUCTION]);
    set_owner(hitter, op);
    hitter->level = SK_level(caster);

    /* Calculate maximum range of the spell */
    range = MAX(SP_level_strength_adjust(caster, SP_DESTRUCTION), spells[SP_DESTRUCTION].bdur);
    dam += SP_level_dam_adjust(caster, SP_DESTRUCTION, false);

    for (i = -range; i < range + 1; i++) {
        for (j = -range; j < range + 1; j++) {
            xt = op->x + i;
            yt = op->y + j;

            if (!(m = get_map_from_coord(op->map, &xt, &yt))) {
                continue;
            }

            /* Nothing alive here? Move on... */
            if (!(GET_MAP_FLAGS(m, xt, yt) & (P_IS_MONSTER | P_IS_PLAYER))) {
                continue;
            }

            /* Try to get an object to hit */
            for (tmp = GET_MAP_OB(m, xt, yt); tmp; tmp = tmp->above) {
                /* Get head. */
                if (tmp->head) {
                    tmp = tmp->head;
                }

                /* Skip the caster and not alive objects. */
                if (tmp == caster || !IS_LIVE(tmp)) {
                    continue;
                }

                if (!is_friend_of(op, tmp)) {
                    int16_t damage = dam;

                    if (tmp->quick_pos) {
                        damage /= (tmp->quick_pos >> 4) + 1;
                    }

                    attack_hit(tmp, hitter, damage);
                    break;
                }
            }
        }
    }
}

/**
 * Cast an area of effect healing spell.
 * @param op
 * Object.
 * @param level
 * Level of the spell being cast.
 * @param type
 * ID of the spell.
 * @return
 * 1 on success, 0 on failure.
 */
int cast_heal_around(object *op, int level, int type)
{
    int success = 0;

    switch (type) {
    case SP_RAIN_HEAL:
    {
        int i, x, y;
        mapstruct *m;
        object *tmp;

        for (i = 0; i <= SIZEOFFREE1; i++) {
            x = op->x + freearr_x[i];
            y = op->y + freearr_y[i];

            if (!(m = get_map_from_coord(op->map, &x, &y))) {
                continue;
            }

            if (!(GET_MAP_FLAGS(m, x, y) & (P_IS_MONSTER | P_IS_PLAYER))) {
                continue;
            }

            for (tmp = GET_MAP_OB_LAYER(m, x, y, LAYER_LIVING, 0); tmp && tmp->layer == LAYER_LIVING; tmp = tmp->above) {
                tmp = HEAD(tmp);

                if (tmp == op || !IS_LIVE(tmp) || !is_friend_of(op, tmp)) {
                    continue;
                }

                cast_heal(op, level, tmp, SP_MINOR_HEAL);
                success = 1;
            }
        }

        break;
    }

    case SP_PARTY_HEAL:
    {
        objectlink *ol;

        if (op->type != PLAYER) {
            return 0;
        } else if (!CONTR(op)->party) {
            draw_info(COLOR_WHITE, op, "You need to be in a party to cast this spell.");
            return 0;
        }

        for (ol = CONTR(op)->party->members; ol; ol = ol->next) {
            if (on_same_map(ol->objlink.ob, op)) {
                cast_heal(op, level, ol->objlink.ob, SP_MINOR_HEAL);
            }
        }

        success = 1;
        break;
    }
    }

    return success;
}

/**
 * Heals something.
 * @param op
 * Who is casting.
 * @param level
 * Level of the skill.
 * @param target
 * Target.
 * @param spell_type
 * ID of the spell.
 */
int cast_heal(object *op, int level, object *target, int spell_type)
{
    archetype_t *at;
    object *temp;
    int heal = 0, success = 0;

    if (op == NULL || target == NULL) {
        log_error("Target or caster is NULL, op: %s, target: %s",
                object_get_str(op), object_get_str(target));
        return 0;
    }

    switch (spell_type) {
    case SP_CURE_DISEASE:

        if (cure_disease(target, op)) {
            success = 1;
        }

        break;

    case SP_CURE_POISON:
        at = arch_find("poisoning");

        if (op != target && target->type == PLAYER) {
            draw_info_format(COLOR_WHITE, target, "%s casts cure poison on you!", op->name ? op->name : "Someone");
        }

        if (op != target && op->type == PLAYER) {
            draw_info_format(COLOR_WHITE, op, "You cast cure poison on %s!", target->name ? target->name : "someone");
        }

        for (temp = target->inv; temp != NULL; temp = temp->below) {
            if (temp->arch == at) {
                success = 1;
                temp->stats.food = 1;
            }
        }

        if (success) {
            if (target->type == PLAYER) {
                draw_info(COLOR_WHITE, target, "Your body feels cleansed.");
            }

            if (op != target && op->type == PLAYER) {
                draw_info_format(COLOR_WHITE, op, "%s's body seems cleansed.", target->name ? target->name : "Someone");
            }
        } else {
            if (target->type == PLAYER) {
                draw_info(COLOR_WHITE, target, "You are not poisoned.");
            }

            if (op != target && op->type == PLAYER) {
                draw_info_format(COLOR_WHITE, op, "%s is not poisoned.", target->name ? target->name : "Someone");
            }
        }

        break;

    case SP_CURE_CONFUSION:
        at = arch_find("confusion");

        if (op != target && target->type == PLAYER) {
            draw_info_format(COLOR_WHITE, target, "%s casts cure confusion on you!", op->name ? op->name : "Someone");
        }

        if (op != target && op->type == PLAYER) {
            draw_info_format(COLOR_WHITE, op, "You cast cure confusion on %s!", target->name ? target->name : "someone");
        }

        for (temp = target->inv; temp != NULL; temp = temp->below) {
            if (temp->arch == at) {
                success = 1;
                temp->stats.food = 1;
            }
        }

        if (success) {
            if (target->type == PLAYER) {
                draw_info(COLOR_WHITE, target, "Your mind feels clearer.");
            }

            if (op != target && op->type == PLAYER) {
                draw_info_format(COLOR_WHITE, op, "%s's mind seems clearer.", target->name ? target->name : "Someone");
            }
        } else {
            if (target->type == PLAYER) {
                draw_info(COLOR_WHITE, target, "You are not confused.");
            }

            if (op != target && op->type == PLAYER) {
                draw_info_format(COLOR_WHITE, op, "%s is not confused.", target->name ? target->name : "Someone");
            }
        }

        break;

    case SP_MINOR_HEAL:
        success = 1;
        heal = rndm(2, 5 + level) + 6;

        if (op->type == PLAYER) {
            if (heal > 0) {
                draw_info_format(COLOR_WHITE, op, "The spell heals %s for %d hp!", op == target ? "you" : target->name, heal);
            } else {
                draw_info(COLOR_WHITE, op, "The healing spell fails!");
            }
        }

        if (op != target && target->type == PLAYER) {
            if (heal > 0) {
                draw_info_format(COLOR_WHITE, target, "%s casts minor healing on you healing %d hp!", op->name, heal);
            } else {
                draw_info_format(COLOR_WHITE, target, "%s casts minor healing on you but it fails!", op->name);
            }
        }

        break;

    case SP_GREATER_HEAL:
        success = 1;
        heal = rndm(4, 5 + level) + rndm(4, 5 + level) + 12;

        if (op->type == PLAYER) {
            if (heal > 0) {
                draw_info_format(COLOR_WHITE, op, "The spell heals %s for %d hp!", op == target ? "you" : target->name, heal);
            } else {
                draw_info(COLOR_WHITE, op, "The healing spell fails!");
            }
        }

        if (op != target && target->type == PLAYER) {
            if (heal > 0) {
                draw_info_format(COLOR_WHITE, target, "%s casts greater healing on you healing %d hp!", op->name, heal);
            } else {
                draw_info_format(COLOR_WHITE, target, "%s casts greater healing on you but it fails!", op->name);
            }
        }

        break;

    case SP_RESTORATION:

        if (cast_heal(op, level, target, SP_CURE_POISON)) {
            success = 1;
        }

        if (cast_heal(op, level, target, SP_CURE_CONFUSION)) {
            success = 1;
        }

        if (cast_heal(op, level, target, SP_CURE_DISEASE)) {
            success = 1;
        }

        if (target->stats.food < 999) {
            success = 1;
            target->stats.food = 999;
        }

        if (cast_heal(op, level, target, SP_MINOR_HEAL)) {
            success = 1;
        }

        return success;
    }

    if (heal > 0) {
        if (reduce_symptoms(target, heal)) {
            success = 1;
        }

        if (target->stats.hp < target->stats.maxhp) {
            if (target == op) {
                if (op->type == PLAYER) {
                    CONTR(op)->stat_damage_healed += MIN(heal, target->stats.maxhp - target->stats.hp);
                }
            } else {
                if (op->type == PLAYER) {
                    CONTR(op)->stat_damage_healed_other += MIN(heal, target->stats.maxhp - target->stats.hp);
                }

                if (target->type == PLAYER) {
                    CONTR(target)->stat_damage_heal_received += MIN(heal, target->stats.maxhp - target->stats.hp);
                }
            }

            success = 1;
            target->stats.hp += heal;

            if (target->stats.hp > target->stats.maxhp) {
                target->stats.hp = target->stats.maxhp;
            }
        }

        if (target->damage_round_tag != global_round_tag) {
            target->last_damage = 0;
            target->damage_round_tag = global_round_tag;
        }

        target->last_damage -= heal;
    }

    if (success) {
        op->speed_left = -FABS(op->speed) * 3;
    }

    if (insert_spell_effect(spells[spell_type].archname, target->map,
            target->x, target->y)) {
        log_error("Failed to insert spell effect, spell: %d, op: %s, "
                "target: %s", spell_type, object_get_str(op),
                object_get_str(target));
    }

    return success;
}

/**
 * Cast some stat-improving spell.
 * @param op
 * Who is casting.
 * @param caster
 * What is casting.
 * @param target
 * Target of the caster; who is receiving the spell.
 * @param spell_type
 * ID of the spell.
 * @retval 0 Spell failed.
 * @retval 1 Spell was successful.
 */
int cast_change_attr(object *op, object *caster, object *target, int spell_type)
{
    object *tmp = target, *tmp2 = NULL, *force = NULL;
    int is_refresh = 0, i = 0;

    if (tmp == NULL) {
        return 0;
    }

    /* We ID the buff force with spell_type... if we find one, we have
     * old effect. If not, we create a fresh force. */
    for (tmp2 = tmp->inv; tmp2 != NULL; tmp2 = tmp2->below) {
        if (tmp2->type == FORCE) {
            if (tmp2->value == spell_type) {
                /* The old effect will be "refreshed" */
                force = tmp2;
                is_refresh = 1;
                draw_info(COLOR_WHITE, op, "You recast the spell while in effect.");
            }
        }
    }

    if (force == NULL) {
        force = arch_get("force_effect");
    }

    /* Mark this force with the originating spell */
    force->value = spell_type;

    switch (spell_type) {
    case SP_STRENGTH:
        force->speed_left = -1;

        if (op->type == PLAYER && op != tmp) {
            draw_info_format(COLOR_WHITE, tmp, "%s casts strength on you!", op->name ? op->name : "Someone");
        }

        if (force->stats.Str < 2) {
            force->stats.Str++;

            if (op->type == PLAYER && op != tmp) {
                draw_info_format(COLOR_WHITE, op, "%s gets stronger.", tmp->name ? tmp->name : "Someone");
            }
        } else {
            draw_info(COLOR_WHITE, tmp, "You don't grow stronger but the spell is refreshed.");

            if (op->type == PLAYER && op != tmp) {
                draw_info_format(COLOR_WHITE, op, "%s doesn't grow stronger but the spell is refreshed.", tmp->name ? tmp->name : "Someone");
            }
        }

        if (insert_spell_effect(spells[SP_STRENGTH].archname, target->map,
                target->x, target->y)) {
            log_error("Failed to insert spell effect, spell: %d, op: %s, "
                    "caster: %s, target: %s", spell_type, object_get_str(op),
                    object_get_str(caster), object_get_str(target));
        }

        break;

    /* Attacktype protection spells */
    case SP_PROT_COLD:
        i = ATNR_COLD;
        break;

    case SP_PROT_FIRE:
        i = ATNR_FIRE;
        break;

    case SP_PROT_ELEC:
        i = ATNR_ELECTRICITY;
        break;

    case SP_PROT_POISON:
        i = ATNR_POISON;
        break;
    }

    if (i) {
        draw_info_format(COLOR_WHITE, op, "Your protection to %s grows.", attack_name[i]);
        force->protection[i] = MIN(SP_level_dam_adjust(caster, spell_type,
                                                       false), 50);
    }

    force->speed_left = -1 - SP_level_strength_adjust(caster, spell_type) * 0.1f;

    if (!is_refresh) {
        SET_FLAG(force, FLAG_APPLIED);
        SET_FLAG(force, FLAG_IS_USED_UP);
        force->face = spells[spell_type].at->clone.face;
        FREE_AND_COPY_HASH(force->name, spells[spell_type].name);

        if (spells[spell_type].at->clone.msg) {
            FREE_AND_COPY_HASH(force->msg, spells[spell_type].at->clone.msg);
        }

        force = insert_ob_in_ob(force, tmp);
        if (force == NULL) {
            log_error("Failed to create force for spell %d, op: %s, "
                    "caster: %s, target: %s", spell_type, object_get_str(op),
                    object_get_str(caster), object_get_str(target));
        }
    } else {
        esrv_update_item(UPD_EXTRA, force);
    }

    return 1;
}

/**
 * Cast remove depletion spell.
 *
 * @param op
 * Object casting this.
 * @param target
 * Target.
 * @return
 * 0 on failure or if there's no depletion, number of stats cured
 * otherwise.
 */
int
cast_remove_depletion (object *op, object *target)
{
    HARD_ASSERT(op != NULL);
    SOFT_ASSERT_RC(target != NULL, 0, "Target is NULL");

    static archetype_t *at = NULL;
    if (at == NULL) {
        at = arch_find("depletion");
        if (at == NULL) {
            LOG(ERROR, "Could not find depletion archetype");
            return 0;
        }
    }

    if (target->type != PLAYER) {
        char *name = object_get_base_name_s(target, op);
        draw_info_format(COLOR_WHITE, op, "You cast remove depletion on %s.",
                         name);
        efree(name);
        draw_info(COLOR_WHITE, op, "There is no depletion.");
        return 0;
    }

    if (op != target) {
        if (op->type == PLAYER) {
            char *name = object_get_base_name_s(target, op);
            draw_info_format(COLOR_WHITE, op,
                             "You cast remove depletion on %s.", name);
            efree(name);
        } else if (target->type == PLAYER) {
            char *name = object_get_base_name_s(op, target);
            draw_info_format(COLOR_WHITE, target,
                             "%s casts remove depletion on you.", name);
            efree(name);
        }
    }

    int success = 0;

    object *depletion = present_arch_in_ob(at, target);
    if (depletion != NULL) {
        for (int i = 0; i < NUM_STATS; i++) {
            if (get_attr_value(&depletion->stats, i) != 0) {
                success++;
                draw_info(COLOR_WHITE, target, restore_msg[i]);
            }
        }

        SET_FLAG(target, FLAG_NO_FIX_PLAYER);
        object_remove(depletion, 0);
        object_destroy(depletion);
        CLEAR_FLAG(target, FLAG_NO_FIX_PLAYER);
        living_update_player(target);
    }

    if (op != target && op->type == PLAYER) {
        if (success != 0) {
            draw_info(COLOR_WHITE, op, "Your spell removes some depletion.");
        } else {
            draw_info(COLOR_WHITE, op, "There is no depletion.");
        }
    }

    if (op != target && target->type == PLAYER && success == 0) {
        draw_info(COLOR_WHITE, target, "There is no depletion.");
    }

    insert_spell_effect(spells[SP_REMOVE_DEPLETION].archname,
                        target->map,
                        target->x,
                        target->y);

    return success;
}

/**
 * Cast remove curse or remove damnation.
 * @param op
 * Caster object.
 * @param target
 * Target.
 * @param type
 * ID of the spell.
 * @param src
 * Where the spell comes from.
 * @return
 * 0 on failure / no cursed items, number of objects uncursed
 * otherwise.
 */
int remove_curse(object *op, object *target, int type, int src)
{
    object *tmp;
    int success = 0;

    if (!op || !target) {
        return 0;
    }

    if (op != target) {
        if (op->type == PLAYER) {
            char *name = object_get_base_name_s(target, op);
            draw_info_format(COLOR_WHITE, op, "You cast remove %s on %s.",
                    type == SP_REMOVE_CURSE ? "curse" : "damnation", name);
            efree(name);
        } else if (target->type == PLAYER) {
            char *name = object_get_base_name_s(op, target);
            draw_info_format(COLOR_WHITE, target, "%s casts remove %s on you.",
                    name, type == SP_REMOVE_CURSE ? "curse" : "damnation");
            efree(name);
        }
    }

    /* Player remove xx only removes applied stuff, npc remove clears ALL */
    for (tmp = target->inv; tmp; tmp = tmp->below) {
        if ((src == CAST_NPC || QUERY_FLAG(tmp, FLAG_APPLIED)) && (QUERY_FLAG(tmp, FLAG_CURSED) || (type == SP_REMOVE_DAMNATION && QUERY_FLAG(tmp, FLAG_DAMNED)))) {
            if (tmp->level <= SK_level(op)) {
                success++;

                if (type == SP_REMOVE_DAMNATION) {
                    CLEAR_FLAG(tmp, FLAG_DAMNED);
                }

                CLEAR_FLAG(tmp, FLAG_CURSED);
                esrv_send_item(tmp);
            } else {
                /* Level of the items is too high for this remove curse */

                if (target->type == PLAYER) {
                    char *name = object_get_base_name_s(tmp, target);
                    draw_info_format(COLOR_WHITE, target, "The %s's curse is "
                            "stronger than the spell!", name);
                    efree(name);
                } else if (op != target && op->type == PLAYER) {
                    char *name = object_get_base_name_s(tmp, op);
                    char *target_name = object_get_base_name_s(target, op);
                    draw_info_format(COLOR_WHITE, op, "The %s's curse of %s is "
                            "stronger than your spell!", name, target_name);
                    efree(name);
                    efree(target_name);
                }
            }
        }
    }

    if (op != target && op->type == PLAYER) {
        if (success) {
            draw_info(COLOR_WHITE, op, "Your spell removes some curses.");
        } else {
            char *name = object_get_base_name_s(target, op);
            draw_info_format(COLOR_WHITE, op, "%s's items seem uncursed.",
                    name);
            efree(name);
        }
    }

    if (target->type == PLAYER) {
        if (success) {
            draw_info(COLOR_WHITE, target, "You feel like someone is helping you.");
        } else {
            if (src == CAST_NORMAL) {
                draw_info(COLOR_WHITE, target, "You are not using any cursed items.");
            } else {
                draw_info(COLOR_WHITE, target, "You hear maniacal laughter in the distance.");
            }
        }
    }

    insert_spell_effect(spells[SP_REMOVE_CURSE].archname, target->map, target->x, target->y);

    return success;
}

/**
 * Actually identify an object when casting identify.
 * @param tmp
 * What to identify.
 * @param op
 * Who is receiving the spell effect.
 * @param mode
 * One of @ref identify_modes.
 * @param[out] done Contains the number of objects identified so far.
 * @param level
 * Maximum level of items we can identify.
 * @return
 * 1 if we can keep identifying items, 0 otherwise.
 */
int do_cast_identify(object *tmp, object *op, int mode, int *done, int level)
{
    if (QUERY_FLAG(tmp, FLAG_IDENTIFIED) || IS_SYS_INVISIBLE(tmp) || !need_identify(tmp)) {
        return 1;
    }

    if (level < tmp->level) {
        if (op->type == PLAYER) {
            char *name = object_get_base_name_s(tmp, op);
            draw_info_format(COLOR_WHITE, op, "The %s is too powerful for this "
                    "identify!", name);
            efree(name);
        }
    } else {
        identify(tmp);

        if (op->type == PLAYER) {
            char *name = object_get_name_description_s(tmp, op);
            draw_info_format(COLOR_WHITE, op, "You have %s.", name);
            efree(name);

            if (tmp->msg != NULL && tmp->type != BOOK) {
                draw_info(COLOR_WHITE, op, "The item has a story:");
                draw_info(COLOR_WHITE, op, tmp->msg);
            }
        }

        *done += 1;
    }

    if (mode == IDENTIFY_NORMAL && op->type == PLAYER && *done > CONTR(op)->skill_ptr[SK_LITERACY]->level + op->stats.Int) {
        return 0;
    }

    return 1;
}

/**
 * Cast identify spell.
 * @param op
 * Object receiving the spell effects.
 * @param level
 * Level of the identification.
 * @param single_ob
 * If set, and mode is @ref IDENTIFY_MARKED, only
 * this object will be identified, otherwise contents of this object.
 * If NULL, the inventory of 'op' will be identified.
 * @param mode
 * One of @ref identify_modes.
 * @return
 * Number of objects identified.
 */
int cast_identify(object *op, int level, object *single_ob, int mode)
{
    int done = 0;

    insert_spell_effect(spells[SP_IDENTIFY].archname, op->map, op->x, op->y);

    if (mode == IDENTIFY_MARKED) {
        SOFT_ASSERT_RC(single_ob != NULL, 0, "single_ob is NULL for object: %s",
                object_get_str(op));
        do_cast_identify(single_ob, op, mode, &done, level);
    } else {
        object *tmp = op->inv;

        if (single_ob && single_ob->type == CONTAINER) {
            tmp = single_ob->inv;
        }

        for (; tmp; tmp = tmp->below) {
            if (!do_cast_identify(tmp, op, mode, &done, level)) {
                break;
            }
        }
    }

    if (op->type == PLAYER && !done) {
        draw_info(COLOR_WHITE, op, "You can't reach anything unidentified in your inventory.");
    }

    return done;
}

/**
 * A spell to make an altar your god's.
 * @param op
 * Who is casting.
 * @retval 0 No consecration happened.
 * @retval 1 An altar was consecrated.
 */
int cast_consecrate(object *op)
{
    object *tmp, *god = find_god(determine_god(op));

    if (!god) {
        draw_info(COLOR_WHITE, op, "You can't consecrate anything if you don't worship a god!");
        return 0;
    }

    for (tmp = op->below; tmp; tmp = tmp->below) {
        if (QUERY_FLAG(tmp, FLAG_IS_FLOOR)) {
            break;
        }

        if (tmp->type == HOLY_ALTAR) {
            /* We use SK_level here instead of path_level mod because I think
             * all the gods should give equal chance of re-consecrating altars
             * */
            if (tmp->level > SK_level(op)) {
                draw_info_format(COLOR_WHITE, op, "You are not powerful enough to reconsecrate the %s.", tmp->name);
                return 0;
            } else if (tmp->other_arch == god->arch) {
                draw_info_format(COLOR_WHITE, op, "That altar is already consecrated to %s.", god->name);
                return 0;
            } else {
                char buf[MAX_BUF], *cp;
                object *new_altar;

                snprintf(buf, sizeof(buf), "altar_%s", god->name);

                for (cp = buf; *cp != '\0'; cp++) {
                    *cp = tolower(*cp);
                }

                new_altar = arch_get(buf);
                new_altar->level = tmp->level;
                new_altar->x = tmp->x;
                new_altar->y = tmp->y;
                new_altar->direction = tmp->direction;

                if (QUERY_FLAG(new_altar, FLAG_IS_TURNABLE)) {
                    SET_ANIMATION(new_altar, (NUM_ANIMATIONS(new_altar) / NUM_FACINGS(new_altar)) * new_altar->direction);
                }

                if (QUERY_FLAG(tmp, FLAG_IS_BUILDABLE)) {
                    SET_FLAG(new_altar, FLAG_IS_BUILDABLE);
                }

                insert_ob_in_map(new_altar, tmp->map, NULL, 0);
                object_remove(tmp, 0);

                draw_info_format(COLOR_WHITE, op, "You consecrated the altar to %s!", god->name);
                return 1;
            }
        }
    }

    draw_info(COLOR_WHITE, op, "You are not standing over an altar!");
    return 0;
}

/**
 * Finger of death spell.
 *
 * If target is undead, the spell will restore target to max health
 * instead of damaging it.
 * @param op
 * Caster.
 * @param target
 * Target.
 * @return
 * 1.
 */
int finger_of_death(object *op, object *target)
{
    object *hitter;
    int dam;

    if (QUERY_FLAG(target, FLAG_UNDEAD)) {
        char *name = object_get_name_s(target, op);
        draw_info_format(COLOR_WHITE, op, "The spell seems ineffective against "
                "the %s!", name);
        efree(name);

        if (!OBJECT_VALID(target->enemy, target->enemy_count)) {
            set_npc_enemy(target, op, NULL);
        }

        return 1;
    }

    /* We create a hitter object -- the spell */
    hitter = arch_to_object(spellarch[SP_FINGER_DEATH]);
    hitter->level = SK_level(op);
    set_owner(hitter, op);
    hitter->x = target->x;
    hitter->y = target->y;
    insert_ob_in_map(hitter, target->map, op, 0);

    dam = SP_level_dam_adjust(op, SP_FINGER_DEATH, false);
    attack_hit(target, hitter, dam);
    object_remove(hitter, 0);

    return 1;
}

/**
 * Let's try to infect something.
 * @param op
 * Who is casting.
 * @param caster
 * What object is casting.
 * @param dir
 * Cast direction.
 * @param disease_arch
 * Archetype of the disease.
 * @param type
 * ID of the spell.
 * @retval 0 No one caught anything.
 * @retval 1 At least one living was affected.
 */
int cast_cause_disease(object *op, object *caster, int dir, struct archetype *disease_arch, int type)
{
    int x = op->x, y = op->y, i, xt, yt;
    object *walk;
    mapstruct *m;

    /* Search in a line for a victim */
    for (i = 0; i < 5; i++) {
        x += freearr_x[dir];
        y += freearr_y[dir];
        xt = x;
        yt = y;

        if (!(m = get_map_from_coord(op->map, &xt, &yt))) {
            continue;
        }

        /* Check map flags for alive object */
        if (!(GET_MAP_FLAGS(m, xt, yt) & P_IS_MONSTER)) {
            continue;
        }

        /* Search this square for a victim */
        for (walk = GET_MAP_OB(m, xt, yt); walk; walk = walk->above) {
            object *disease;
            int dam, strength;

            /* Found a victim */
            if (!QUERY_FLAG(walk, FLAG_MONSTER) && (walk->type != PLAYER || !pvp_area(op, walk))) {
                continue;
            }

            disease = arch_to_object(disease_arch);
            dam = SP_level_dam_adjust(caster, type, false);
            strength = SP_level_strength_adjust(caster, type);

            set_owner(disease, op);
            disease->stats.exp = 0;
            disease->level = SK_level(caster);

            /* Do level adjustments */
            if (disease->stats.wc) {
                disease->stats.wc += strength / 2;
            }

            if (disease->magic > 0) {
                disease->magic += strength / 4;
            }

            if (disease->stats.maxhp > 0) {
                disease->stats.maxhp += strength;
            }

            if (disease->stats.dam) {
                if (disease->stats.dam > 0) {
                    disease->stats.dam += dam;
                } else {
                    disease->stats.dam -= dam;
                }
            }

            if (disease->last_sp) {
                disease->last_sp -= 2 * dam;

                if (disease->last_sp < 1) {
                    disease->last_sp = 1;
                }
            }

            if (disease->stats.maxsp) {
                if (disease->stats.maxsp > 0) {
                    disease->stats.maxsp += dam;
                } else {
                    disease->stats.maxsp -= dam;
                }
            }

            if (disease->stats.ac) {
                disease->stats.ac += dam;
            }

            if (disease->last_eat) {
                disease->last_eat -= dam;
            }

            if (disease->stats.hp) {
                disease->stats.hp -= dam;
            }

            if (disease->stats.sp) {
                disease->stats.sp -= dam;
            }

            if (infect_object(walk, disease, 1)) {
                draw_info_format(COLOR_WHITE, op, "You inflict %s on %s!", disease->name, walk->name);
                return 1;
            }
        }

        /* No more infecting through walls. */
        if (wall(m, xt, yt)) {
            return 0;
        }
    }

    draw_info(COLOR_WHITE, op, "No one caught anything!");
    return 0;
}

/**
 * Transform wealth spell.
 * @param op
 * Who is casting.
 * @return
 * 1 on success, 0 otherwise.
 */
int cast_transform_wealth(object *op)
{
    object *marked;
    int64_t val;

    if (op->type != PLAYER) {
        return 0;
    }

    /* Find the marked wealth. */
    marked = find_marked_object(op);

    if (!marked) {
        draw_info(COLOR_WHITE, op, "You need to mark an object to cast this spell.");
        return 0;
    }

    /* Check that it's really money. */
    if (marked->type != MONEY) {
        draw_info(COLOR_WHITE, op, "You can only cast this spell on wealth objects.");
        return 0;
    }

    char *name = object_get_name_s(marked, op);

    /* Only allow coppers and silvers to be transformed. */
    if (strcmp(marked->arch->name, coins[NUM_COINS - 1]) && strcmp(marked->arch->name, coins[NUM_COINS - 2])) {
        draw_info_format(COLOR_WHITE, op, "You don't see a way to transform "
                "%s.", name);
        efree(name);
        return 0;
    }

    /* Figure out our value of money to give to player. */
    val = (marked->value * (marked->nrof ? marked->nrof : 1)) * TRANSFORM_WEALTH_SACRIFICE;
    /* We remove the money. */
    object_remove(marked, 0);
    /* Now give the player the new money. */
    shop_insert_coins(op, val);
    draw_info_format(COLOR_WHITE, op, "You transform %s into %s.", name,
            shop_get_cost_string(val));
    efree(name);
    return 1;
}
