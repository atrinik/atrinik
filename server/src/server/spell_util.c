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
 * Spell related helper functions. */

#include <global.h>
#include <spellist.h>
#include <toolkit_string.h>

/**
 * Array of pointers to archetypes used by the spells for quick
 * access. */
archetype *spellarch[NROFREALSPELLS];

/**
 * Initialize spells. */
void init_spells(void)
{
    static int init_spells_done = 0;
    int i;

    if (init_spells_done) {
        return;
    }

    init_spells_done = 1;

    for (i = 0; i < NROFREALSPELLS; i++) {
        char spellname[MAX_BUF], tmpresult[MAX_BUF];
        archetype *at;

        string_replace(spells[i].name, " ", "_", tmpresult, sizeof(spellname));
        snprintf(spellname, sizeof(spellname), "spell_%s", tmpresult);

        at = find_archetype(spellname);

        if (!at) {
            logger_print(LOG(ERROR), "Could not find required archetype %s.", spellname);
            exit(1);
        }

        spells[i].at = at;

        at->clone.stats.sp = i;

        if (spells[i].archname) {
            if ((spellarch[i] = find_archetype(spells[i].archname)) == NULL) {
                logger_print(LOG(ERROR), "Spell %s needs arch %s, your archetypes file is out of date.", spells[i].name, spells[i].archname);
                exit(1);
            }
        } else {
            spellarch[i] = NULL;
        }
    }
}

/**
 * Inserts a spell effect on map.
 * @param archname Spell effect arch.
 * @param m Map.
 * @param x X position on map.
 * @param y Y position on map.
 * @return 1 on failure, 0 otherwise. */
int insert_spell_effect(char *archname, mapstruct *m, int x, int y)
{
    archetype *effect_arch;
    object *effect_ob;

    if (!archname || !m) {
        logger_print(LOG(BUG), "archname or map NULL.");
        return 1;
    }

    if (!(effect_arch = find_archetype(archname))) {
        logger_print(LOG(BUG), "Couldn't find effect arch (%s).", archname);
        return 1;
    }

    /* Prepare effect */
    effect_ob = arch_to_object(effect_arch);
    effect_ob->map = m;
    effect_ob->x = x;
    effect_ob->y = y;

    if (!insert_ob_in_map(effect_ob, m, NULL, 0)) {
        logger_print(LOG(BUG), "effect arch (%s) out of map (%s) (%d,%d) or failed insertion.", archname, effect_ob->map->name, x, y);

        /* Something is wrong - kill object */
        if (!QUERY_FLAG(effect_ob, FLAG_REMOVED)) {
            object_remove(effect_ob, 0);
        }

        return 1;
    }

    return 0;
}

/**
 * Find a spell in the ::spells array.
 * @param spelltype ID of the spell to find.
 * @return The spell from the ::spells array, NULL if not found. */
spell_struct *find_spell(int spelltype)
{
    if (spelltype < 0 || spelltype >= NROFREALSPELLS) {
        return NULL;
    }

    return &spells[spelltype];
}

/**
 * Cast a spell.
 * @param op The creature that is owner of the object that is casting the
 * spell.
 * @param caster The actual object (wand, potion) casting the spell. Can
 * be same as op.
 * @param dir Direction to cast in.
 * @param type Spell ID.
 * @param ability If true, the spell is the innate ability of a monster
 * (ie, don't check for blocks_magic(), and don't add AT_MAGIC to attacktype).
 * @param item The type of object that is casting the spell.
 * @param stringarg Any options that are being used.
 * @return 0 on failure, non-zero on success and is used by caller to
 * drain mana. */
int cast_spell(object *op, object *caster, int dir, int type, int ability, int item, const char *stringarg)
{
    spell_struct *s;
    object *target = NULL;
    int success = 0, duration, spell_cost = 0;

    if (op == NULL && caster != NULL) {
        op = caster;
    } else if (caster == NULL && op != NULL) {
        caster = op;
    } else if (op == NULL && caster == NULL) {
        logger_print(LOG(BUG), "Both 'op' and 'caster' are NULL.");
        return 0;
    }

    s = find_spell(type);

    if (s == NULL) {
        logger_print(LOG(BUG), "Unknown spell: %d", type);
        return 0;
    }

    /* Get the base duration */
    duration = spells[type].bdur;

    /* Script NPCs can ALWAYS cast - even in no spell areas! */
    if (item == CAST_NPC) {
        /* If CAST_NPC, this usually comes from a script,
         * and caster is the NPC and op the target. */
        target = op;
        op = caster;
    } else {
        /* It looks like the only properties we ever care about from the casting
         * object (caster) is spell paths and level. */
        object *cast_op = op;
        MapSpace *msp;

        /* Caster has a map? Then we use caster */
        if (caster->map) {
            cast_op = caster;
        }

        /* No magic. */
        if (MAP_NOMAGIC(cast_op->map)) {
            draw_info(COLOR_WHITE, op, "Powerful countermagic cancels all spellcasting here!");
            return 0;
        }

        msp = GET_MAP_SPACE_PTR(cast_op->map, cast_op->x, cast_op->y);

        /* No harm spell and not town safe. */
        if ((MAP_NOHARM(cast_op->map) || (msp->extra_flags & MSP_EXTRA_NO_HARM)) && !(MAP_NOHARM(cast_op->map) && (msp->extra_flags & MSP_EXTRA_NO_HARM)) && !(spells[type].flags & SPELL_DESC_TOWN)) {
            draw_info(COLOR_WHITE, op, "Powerful countermagic cancels all harmful magic here!");
            return 0;
        }

        if (op->type == PLAYER) {
            /* Cancel player spells which are denied, but only real spells (not
             * potions, wands, etc). */
            if (item == CAST_NORMAL) {
                if (caster->path_denied & s->path) {
                    draw_info(COLOR_WHITE, op, "It is denied for you to cast that spell.");
                    return 0;
                }

                if (op->type != PLAYER || !CONTR(op)->tgm) {
                    if (op->stats.sp < SP_level_spellpoint_cost(caster, type, -1)) {
                        draw_info(COLOR_WHITE, op, "You don't have enough mana.");
                        return 0;
                    }
                }
            }
        }

        if (item == CAST_POTION) {
            /* If the potion casts a self spell, don't use the facing
             * direction. */
            if (spells[type].flags & SPELL_DESC_SELF) {
                target = op;
                dir = 0;
            }
        } else if (find_target_for_spell(op, &target, spells[type].flags) == 0) {
            draw_info_format(COLOR_WHITE, op, "You can't cast that spell on %s!", target ? target->name : "yourself");
            return 0;
        }

        /* If valid target is not in range for selected spell, skip casting. */
        if (target) {
            rv_vector rv;

            if (!get_rangevector_from_mapcoords(op->map, op->x, op->y, target->map, target->x, target->y, &rv, RV_DIAGONAL_DISTANCE) || rv.distance > (unsigned int) spells[type].range) {
                draw_info(COLOR_WHITE, op, "Your target is out of range!");
                return 0;
            }
        }

        if (op->type == PLAYER && target == op && CONTR(op)->target_object != op) {
            draw_info(COLOR_WHITE, op, "You auto-target yourself with this spell!");
        }

        if (!ability && blocks_magic(op->map, op->x, op->y)) {
            if (op->type != PLAYER) {
                return 0;
            }

            if (caster == op) {
                draw_info(COLOR_WHITE, op, "Something blocks your spellcasting.");
            } else {
                draw_info_format(COLOR_WHITE, op, "Something blocks the magic of your %s.", query_base_name(caster, op));
            }

            return 0;
        }
    }

    /* A last sanity check: are caster and target *really* valid? */
    if (!OBJECT_ACTIVE(caster) || (target && !OBJECT_ACTIVE(target))) {
        return 0;
    }

    /* Trigger the map-wide spell event. */
    if (op->map && op->map->events) {
        int retval = trigger_map_event(MEVENT_SPELL_CAST, op->map, op, caster, NULL, stringarg, type);

        /* So the plugin's return value can affect the returned value. */
        if (retval) {
            return retval - 1;
        }
    }

    if (caster->type == PLAYER) {
        CONTR(caster)->stat_spells_cast++;
    }

    /* We need to calculate the spell point cost before the spell actually
     * does something, otherwise the following can happen (example):
     * Player has 7 mana left, kills a monster with magic bullet (which costs 7
     * mana) while standing right next to it, magic bullet kills the monster
     * before
     * we reach the return here, player levels up, cost of magic bullet
     * increases
     * from 7 to 8. So the function would return 8 instead of 7, resulting in
     * the
     * player's mana being -1. */
    if (item != CAST_NPC) {
        spell_cost = SP_level_spellpoint_cost(caster, type, -1);
    }

    switch ((enum spellnrs) type) {
    case SP_RESTORATION:
    case SP_CURE_CONFUSION:
    case SP_MINOR_HEAL:
    case SP_GREATER_HEAL:
    case SP_CURE_POISON:
    case SP_CURE_DISEASE:
        success = cast_heal(op, SK_level(caster), target, type);
        break;

    case SP_REMOVE_CURSE:
    case SP_REMOVE_DAMNATION:
        success = remove_curse(op, target, type, item);
        break;

    case SP_STRENGTH:
    case SP_PROT_COLD:
    case SP_PROT_FIRE:
    case SP_PROT_ELEC:
    case SP_PROT_POISON:
        success = cast_change_attr(op, caster, target, type);
        break;

    case SP_IDENTIFY:
        success = cast_identify(target, SK_level(caster), NULL, IDENTIFY_NORMAL);
        break;

        /* Spells after this use direction and not a target */
    case SP_ICESTORM:
    case SP_FIRESTORM:
    case SP_HOLYWORD:
        success = cast_cone(op, caster, dir, duration, type, spellarch[type]);
        break;

    case SP_PROBE:

        if (!dir) {
            examine(op, op, NULL);
            success = 1;
        } else {
            success = fire_arch_from_position(op, caster, op->x, op->y, dir, spellarch[type], type, NULL);
        }

        break;

    case SP_BULLET:
    case SP_CAUSE_LIGHT:
    case SP_MAGIC_MISSILE:
        success = fire_arch_from_position(op, caster, op->x, op->y, dir, spellarch[type], type, target);
        break;

    case SP_WOR:
        success = cast_wor(op, caster);
        break;

    case SP_CREATE_FOOD:
        success = cast_create_food(op, caster, dir, stringarg);
        break;

    case SP_CHARGING:
        success = recharge(op);
        break;

    case SP_CONSECRATE:
        success = cast_consecrate(op);
        break;

    case SP_CAUSE_COLD:
    case SP_CAUSE_FLU:
    case SP_CAUSE_LEPROSY:
    case SP_CAUSE_SMALLPOX:
    case SP_CAUSE_PNEUMONIC_PLAGUE:
        success = cast_cause_disease(op, caster, dir, spellarch[type], type);
        break;

    case SP_FINGER_DEATH:
        success = finger_of_death(op, target);
        break;

    case SP_POISON_FOG:
    case SP_METEOR:
    case SP_ASTEROID:
        success = fire_arch_from_position(op, caster, op->x, op->y, dir, spellarch[type], type, NULL);
        break;

    case SP_METEOR_SWARM:
        success = 1;
        fire_swarm(op, caster, dir, spellarch[type], SP_METEOR, 3, 0);
        break;

    case SP_FROST_NOVA:
        success = 1;
        fire_swarm(op, caster, dir, spellarch[type], SP_ASTEROID, 3, 0);
        break;

    case SP_BULLET_SWARM:
        success = 1;
        fire_swarm(op, caster, dir, spellarch[type], SP_BULLET, 5, 0);
        break;

    case SP_BULLET_STORM:
        success = 1;
        fire_swarm(op, caster, dir, spellarch[type], SP_BULLET, 3, 0);
        break;

    case SP_DESTRUCTION:
        success = cast_destruction(op, caster, 5 + op->stats.Int, AT_MAGIC);
        break;

    case SP_TRANSFORM_WEALTH:
        success = cast_transform_wealth(op);
        break;

    case SP_RAIN_HEAL:
    case SP_PARTY_HEAL:
        success = cast_heal_around(op, SK_level(caster), type);
        break;

    case SP_FROSTBOLT:
    case SP_FIREBOLT:
    case SP_LIGHTNING:
    case SP_FORKED_LIGHTNING:
    case SP_NEGABOLT:
        success = fire_bolt(op, caster, dir, type);
        break;

    default:
        logger_print(LOG(BUG), "Invalid spell: %d", type);
        break;
    }

    play_sound_map(op->map, CMD_SOUND_EFFECT, spells[type].sound, op->x, op->y, 0, 0);

    if (item == CAST_NPC) {
        return success;
    }

    return success ? spell_cost : 0;
}

/**
 * Creates object new_op in direction dir or if that is blocked, beneath
 * the player (op).
 * @param op Who is casting.
 * @param new_op Object to insert.
 * @param dir Direction to insert into. Can be 0.
 * @return Direction that the object was actually placed in. */
int cast_create_obj(object *op, object *new_op, int dir)
{
    mapstruct *mt;
    int xt, yt;

    xt = op->x + freearr_x[dir];
    yt = op->y + freearr_y[dir];

    if (!(mt = get_map_from_coord(op->map, &xt, &yt))) {
        return 0;
    }

    if (dir && blocked(op, mt, xt, yt, op->terrain_flag)) {
        draw_info(COLOR_WHITE, op, "Something is in the way.\nYou cast it at your feet.");
        dir = 0;
    }

    xt = op->x + freearr_x[dir];
    yt = op->y + freearr_y[dir];

    if (!(mt = get_map_from_coord(op->map, &xt, &yt))) {
        return 0;
    }

    new_op->x = xt;
    new_op->y = yt;
    new_op->map = mt;
    insert_ob_in_map(new_op, mt, op, 0);
    return dir;
}

/**
 * Cast a bolt-like spell.
 * @param op Who is casting the spell.
 * @param caster What object is casting the spell (rod, ...).
 * @param dir Firing direction.
 * @param type Spell ID.
 * @retval 0 No bolt could be fired.
 * @retval 1 Bolt was fired (but may have been destroyed already). */
int fire_bolt(object *op, object *caster, int dir, int type)
{
    object *tmp;

    if (!spellarch[type]) {
        return 0;
    }

    if (!dir) {
        draw_info(COLOR_WHITE, op, "You can't fire that at yourself!");
        return 0;
    }

    if (wall(op->map, op->x + freearr_x[dir], op->y + freearr_y[dir])) {
        draw_info(COLOR_WHITE, op, "There is something in the way.");
        return 0;
    }

    tmp = arch_to_object(spellarch[type]);

    if (!tmp) {
        return 0;
    }

    tmp->stats.dam = (int16_t) SP_level_dam_adjust(caster, type, tmp->stats.dam, 0);
    tmp->last_sp = spells[type].bdur + SP_level_strength_adjust(caster, type);

    tmp->direction = dir;
    tmp->x = op->x;
    tmp->y = op->y;

    set_owner(tmp, op);
    tmp->level = SK_level(caster);

    if (QUERY_FLAG(tmp, FLAG_IS_TURNABLE)) {
        SET_ANIMATION(tmp, (NUM_ANIMATIONS(tmp) / NUM_FACINGS(tmp)) * tmp->direction);
    }

    tmp = insert_ob_in_map(tmp, op->map, op, 0);

    if (!tmp) {
        return 0;
    }

    object_process(tmp);

    return 1;
}

/**
 * Fires an archetype.
 * @param op Person firing the object.
 * @param caster Object casting the spell.
 * @param x X position where to fire the spell.
 * @param y Y position where to fire the spell.
 * @param dir Direction to fire in.
 * @param at The archetype to fire.
 * @param type Spell ID.
 * @return 0 on failure, 1 on success. */
int fire_arch_from_position(object *op, object *caster, int16_t x, int16_t y, int dir, archetype *at, int type, object *target)
{
    object *tmp, *env;

    if (at == NULL) {
        return 0;
    }

    for (env = op; env->env != NULL; env = env->env) {
    }

    if (env->map == NULL) {
        return 0;
    }

    tmp = arch_to_object(at);

    if (tmp == NULL) {
        return 0;
    }

    tmp->stats.sp = type;
    tmp->stats.dam = (int16_t) SP_level_dam_adjust(caster, type, tmp->stats.dam, 0);
    tmp->stats.hp = spells[type].bdur + SP_level_strength_adjust(caster, type);
    tmp->x = x, tmp->y = y;
    tmp->direction = dir;

    if (target) {
        tmp->enemy = target;
        tmp->enemy_count = target->count;
    }

    if (get_owner(op) != NULL) {
        copy_owner(tmp, op);
    } else {
        set_owner(tmp, op);
    }

    tmp->level = SK_level(caster);

    if (QUERY_FLAG(tmp, FLAG_IS_TURNABLE)) {
        SET_ANIMATION(tmp, (NUM_ANIMATIONS(tmp) / NUM_FACINGS(tmp)) * dir);
    }

    if ((tmp = insert_ob_in_map(tmp, op->map, op, 0)) == NULL) {
        return 1;
    }

    object_process(tmp);

    return 1;
}

/**
 * Casts a cone spell.
 * @param op Person firing the object.
 * @param caster Object casting the spell.
 * @param dir Direction to fire in.
 * @param strength Strength of the spell.
 * @param spell_type ID of the spell.
 * @param spell_arch Spell's arch.
 * @retval 0 Couldn't cast.
 * @retval 1 Successful cast. */
int cast_cone(object *op, object *caster, int dir, int strength, int spell_type, archetype *spell_arch)
{
    object *tmp;
    int i, success = 0, range_min = -1, range_max = 1;
    uint32_t count_ref;

    if (!dir) {
        range_min = -3, range_max = 4, strength /= 2;
    }

    /* Our initial spell object */
    tmp = arch_to_object(spell_arch);

    if (!tmp) {
        logger_print(LOG(BUG), "arch_to_object() failed!? (%s)", spell_arch->name);
        return 0;
    }

    count_ref = tmp->count;

    for (i = range_min; i <= range_max; i++) {
        int x = op->x + freearr_x[absdir(dir + i)], y = op->y + freearr_y[absdir(dir + i)];

        if (wall(op->map, x, y)) {
            continue;
        }

        success = 1;

        if (!tmp) {
            tmp = arch_to_object(spell_arch);
        }

        set_owner(tmp, op);
        copy_owner(tmp, op);
        /* *very* important - miss this and the spells go really wild! */
        tmp->weight_limit = count_ref;

        tmp->level = SK_level(caster);
        tmp->x = x, tmp->y = y;

        if (dir) {
            tmp->stats.sp = dir;
        } else {
            tmp->stats.sp = i;
        }

        tmp->stats.hp = strength;
        tmp->stats.dam = (int16_t) SP_level_dam_adjust(caster, spell_type, tmp->stats.dam, 0);
        tmp->stats.maxhp = tmp->count;

        if (!QUERY_FLAG(tmp, FLAG_FLYING)) {
            logger_print(LOG(DEBUG), "arch %s doesn't have flying 1", spell_arch->name);
        }

        if ((!QUERY_FLAG(tmp, FLAG_WALK_ON) || !QUERY_FLAG(tmp, FLAG_FLY_ON)) && tmp->stats.dam) {
            logger_print(LOG(DEBUG), "arch %s doesn't have walk_on 1 and fly_on 1", spell_arch->name);
        }

        if (!insert_ob_in_map(tmp, op->map, op, 0)) {
            return 0;
        }

        if (tmp->other_arch) {
            cone_drop(tmp);
        }

        tmp = NULL;
    }

    /* Can happen when we can't drop anything */
    if (tmp) {
        /* Was not inserted */
        if (!QUERY_FLAG(tmp, FLAG_REMOVED)) {
            object_remove(tmp, 0);
        }

        object_destroy(tmp);
    }

    return success;
}

/**
 * Drops an object based on what is in the cone's "other_arch".
 * @param op The object. */
void cone_drop(object *op)
{
    object *new_ob = arch_to_object(op->other_arch);

    new_ob->x = op->x;
    new_ob->y = op->y;
    new_ob->stats.food = op->stats.hp;
    new_ob->level = op->level;
    set_owner(new_ob, op->owner);

    if (op->chosen_skill) {
        new_ob->chosen_skill = op->chosen_skill;
    }

    insert_ob_in_map(new_ob, op->map, op, 0);
}

/**
 * Causes an object to explode, eg, a firebullet, poison cloud ball, etc.
 * @param op The object to explode. */
void explode_object(object *op)
{
    tag_t op_tag = op->count;

    play_sound_map(op->map, CMD_SOUND_EFFECT, "explosion.ogg", op->x, op->y, 0, 0);

    if (op->other_arch == NULL) {
        logger_print(LOG(BUG), "op %s without other_arch", query_name(op, NULL));
        object_remove(op, 0);
        object_destroy(op);
        return;
    }

    cast_cone(op, op, 0, spells[op->stats.sp].bdur, op->stats.sp,
            op->other_arch);
    hit_map(op, 0, 0);

    /* remove the firebullet */
    if (!was_destroyed(op, op_tag)) {
        object_remove(op, 0);
        object_destroy(op);
    }
}

/**
 * If we are here, the arch (spell) we check was able to move to this
 * place. wall() has failed, including reflection checking.
 *
 * Look for a target.
 * @param op The spell object. */
void check_fired_arch(object *op)
{
    tag_t op_tag = op->count, tmp_tag;
    object *tmp, *hitter, *head;
    int dam;

    /* we return here if we have NOTHING blocking here */
    if (!blocked(op, op->map, op->x, op->y, op->terrain_flag)) {
        return;
    }

    if (op->other_arch) {
        explode_object(op);
        return;
    }

    hitter = get_owner(op);

    if (!hitter) {
        hitter = op;
    } else if (hitter->head) {
        hitter = hitter->head;
    }

    for (tmp = GET_MAP_OB(op->map, op->x, op->y); tmp != NULL; tmp = tmp->above) {
        head = tmp->head;

        if (!head) {
            head = tmp;
        }

        if (!IS_LIVE(tmp)) {
            continue;
        }

        /* Let friends fire through friends */
        if (is_friend_of(hitter, head) || head == hitter) {
            continue;
        }

        tmp_tag = tmp->count;

        dam = hit_player(tmp, op->stats.dam, op, AT_INTERNAL);

        if (was_destroyed(op, op_tag) || !was_destroyed(tmp, tmp_tag) || (op->stats.dam -= dam) < 0) {
            if (!QUERY_FLAG(op, FLAG_REMOVED)) {
                object_remove(op, 0);
                object_destroy(op);
                return;
            }
        }
    }
}

/**
 * Detect target for casting a spell.
 * @param op Caster.
 * @param[out] target Will contain target for the spell we're casting.
 * @param flags Spell flags.
 * @return 1 if the object can cast the spell on the target, 0
 * otherwise. */
int find_target_for_spell(object *op, object **target, uint32_t flags)
{
    object *tmp;

    /* Default target is nothing. */
    *target = NULL;

    /* We cast something on the map... No target */
    if (flags & SPELL_DESC_DIRECTION) {
        return 1;
    }

    /* A player has invoked this spell. */
    if (op->type == PLAYER) {
        /* Try to cast on self but only when really no friendly or enemy is set.
         * */
        if ((flags & SPELL_DESC_SELF) && !(flags & (SPELL_DESC_ENEMY | SPELL_DESC_FRIENDLY))) {
            /* Self... and no other tests */
            *target = op;
            return 1;
        }

        tmp = CONTR(op)->target_object;

        /* Let's check our target - we have one? friend or enemy? */
        if (!tmp || !OBJECT_ACTIVE(tmp) || tmp == CONTR(op)->ob || CONTR(op)->target_object_count != tmp->count) {
            /* Can we cast this on self? */
            if (flags & SPELL_DESC_SELF) {
                /* Right, we are target */
                *target = op;
                return 1;
            }
        } else {
            /* We have a target and it's not self */

            if (is_friend_of(op, tmp)) {
                if (flags & SPELL_DESC_FRIENDLY) {
                    *target = tmp;
                    return 1;
                }

                if (flags & SPELL_DESC_SELF) {
                    *target = op;
                    return 1;
                }

                /* Can't cast unfriendly spells on friendly creatures, but we
                 * set target
                 * so the message player gets is accurate. */
                if (flags & SPELL_DESC_ENEMY) {
                    *target = tmp;
                    return 0;
                }
            } else {
                if (flags & SPELL_DESC_ENEMY) {
                    *target = tmp;
                    return 1;
                }

                if (flags & SPELL_DESC_SELF) {
                    *target = op;
                    return 1;
                }
            }
        }
    } else {
        /* A monster or rune/firewall/etc */

        if ((flags & SPELL_DESC_SELF) && !(flags & (SPELL_DESC_ENEMY | SPELL_DESC_FRIENDLY))) {
            *target = op;
            return 1;
        } else if ((flags & SPELL_DESC_ENEMY) && op->enemy && OBJECT_ACTIVE(op->enemy) && op->enemy->count == op->enemy_count) {
            *target = op->enemy;
            return 1;
        } else {
            *target = op;
            return 1;
        }
    }

    /* Invalid target/spell or whatever */
    return 0;
}

/**
 * Returns adjusted damage based on the caster.
 * @param caster Who is casting.
 * @param spell_type Spell ID we're adjusting.
 * @param base_dam Base damage.
 * @param exact Return exact damage, unadjusted by random percent?
 * @return Adjusted damage. */
int SP_level_dam_adjust(object *caster, int spell_type, int base_dam, int exact)
{
    int level = SK_level(caster);
    int16_t dam;

    /* Sanity check */
    if (level <= 0 || level > MAXLEVEL) {
        logger_print(LOG(BUG), "object %s has invalid level %d", query_name(caster, NULL), level);

        if (level <= 0) {
            level = 1;
        } else {
            level = MAXLEVEL;
        }
    }

    /* get a base damage when we don't have one from caller */
    if (base_dam == -1) {
        base_dam = spells[spell_type].bdam;
    }

    dam = (int16_t) ((float) base_dam * LEVEL_DAMAGE(level) * PATH_DMG_MULT(caster, find_spell(spell_type)));

    if (exact || !dam) {
        return dam;
    }

    return rndm(dam * 0.8f + 1, dam);
}

/**
 * Adjust the strength of the spell based on level.
 * @param caster Who is casting.
 * @param spell_type Spell ID we're adjusting.
 * @return Adjusted strength. */
int SP_level_strength_adjust(object *caster, int spell_type)
{
    int level = SK_level(caster);
    int adj = (level);

    if (adj < 0) {
        adj = 0;
    }

    if (spells[spell_type].ldur) {
        adj /= spells[spell_type].ldur;
    } else {
        adj = 0;
    }

    return adj;
}

/**
 * Scales the spellpoint cost of a spell by it's increased effectiveness.
 * Some of the lower level spells become incredibly vicious at high
 * levels. Very cheap mass destruction. This function is intended to keep
 * the sp cost related to the effectiveness.
 * @param caster What is casting the spell.
 * @param spell_type Spell ID.
 * @param caster_level Level of caster. If -1, will use SK_level() to
 * determine caster's level. Will also avoid affecting the cost with paths.
 * @return Spell points cost. */
int SP_level_spellpoint_cost(object *caster, int spell_type, int caster_level)
{
    spell_struct *s = find_spell(spell_type);
    int level = (caster_level == -1 ? SK_level(caster) : caster_level), sp;

    if (spells[spell_type].spl) {
        sp = (int) (spells[spell_type].sp * (1.0 + (MAX(0, (float) (level) / (float) spells[spell_type].spl))));
    } else {
        sp = spells[spell_type].sp;
    }

    if (caster_level == -1) {
        sp = (int) ((float) sp * (float) PATH_SP_MULT(caster, s));
    }

    return sp;
}

/**
 * The following routine creates a swarm of objects. It actually sets up
 * a specific swarm object, which then fires off all the parts of the
 * swarm.
 * @param op Who is casting.
 * @param caster What object is casting.
 * @param dir Cast direction.
 * @param swarm_type Archetype of the swarm.
 * @param spell_type ID of the spell.
 * @param n The number to be fired.
 * @param magic Magic. */
void fire_swarm(object *op, object *caster, int dir, archetype *swarm_type, int spell_type, int n, int magic)
{
    object *tmp = get_archetype("swarm_spell");

    tmp->x = op->x;
    tmp->y = op->y;
    /* Needed so that if swarm elements kill, caster gets xp. */
    set_owner(tmp, op);
    /* Needed later, to get level dep. right.*/
    tmp->level = SK_level(caster);
    /* Needed later, see move_swarm_spell */
    tmp->stats.sp = spell_type;

    tmp->magic = magic;
    /* n in swarm */
    tmp->stats.hp = n;
    /* The archetype of the things to be fired */
    tmp->other_arch = swarm_type;
    tmp->direction = dir;

    insert_ob_in_map(tmp, op->map, op, 0);
}
