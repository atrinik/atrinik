/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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
 * Various skill related functions. */

/* define the following for skills utility debugging */
/* #define SKILL_UTIL_DEBUG */

#include <global.h>

/** Table for stat modification of exp */
float stat_exp_mult[MAX_STAT + 1] =
{
    0.0f,   0.01f,  0.1f,   0.3f,   0.5f,
    0.6f,   0.7f,   0.8f,   0.85f,  0.9f,
    0.95f,  0.96f,  0.97f,  0.98f,  0.99f,
    1.0f,   1.01f,  1.02f,  1.03f,  1.04f,
    1.05f,  1.07f,  1.09f,  1.12f,  1.15f,
    1.2f,   1.3f,   1.4f,   1.5f,   1.7f,
    2.0f
};

/**
 * Used for calculating experience gained in calc_skill_exp(). */
static float lev_exp[MAXLEVEL + 1] =
{
    0.0f,     1.0f,     1.11f,    1.75f,    3.2f,
    5.5f,     10.0f,    20.0f,    35.25f,   66.1f,
    137.0f,   231.58f,  240.00f,  247.62f,  254.55f,
    260.87f,  266.67f,  272.00f,  276.92f,  281.48f,
    285.71f,  289.66f,  293.33f,  296.77f,  300.00f,
    303.03f,  305.88f,  308.57f,  311.11f,  313.51f,
    315.79f,  317.95f,  320.00f,  321.95f,  323.81f,
    325.58f,  327.27f,  328.89f,  330.43f,  331.91f,
    333.33f,  334.69f,  336.00f,  337.25f,  338.46f,
    339.62f,  340.74f,  341.82f,  342.86f,  343.86f,
    344.83f,  345.76f,  346.67f,  347.54f,  348.39f,
    349.21f,  350.00f,  350.77f,  351.52f,  352.24f,
    352.94f,  353.62f,  354.29f,  354.93f,  355.56f,
    356.16f,  356.76f,  357.33f,  357.89f,  358.44f,
    358.97f,  359.49f,  360.00f,  360.49f,  360.98f,
    361.45f,  361.90f,  362.35f,  362.79f,  365.22f,
    367.64f,  369.04f,  373.44f,  378.84f,  384.22f,
    389.59f,  395.96f,  402.32f,  410.67f,  419.01f,
    429.35f,  440.68f,  452.00f,  465.32f,  479.63f,
    494.93f,  510.23f,  527.52f,  545.81f,  562.09f,
    580.37f,  599.64f,  619.91f,  640.17f,  662.43f,
    685.68f,  709.93f,  773.17f,  852.41f,  932.65f,
    1013.88f, 1104.11f, 1213.35f, 1324.60f, 1431.86f,
    1542.13f
};

static int do_skill_attack(object *tmp, object *op, char *string);

/**
 * Main skills use function similar in scope to cast_spell().
 *
 * We handle all requests for skill use outside of some combat here.
 * We require a separate routine outside of fire() so as to allow monsters
 * to utilize skills.
 * @param op The object actually using the skill.
 * @param dir The direction in which the skill is used.
 * @param params String option for the skill.
 * @return 0 on failure of using the skill, non-zero otherwise. */
sint64 do_skill(object *op, int dir, const char *params)
{
    sint64 success = 0;
    int skill = op->chosen_skill->stats.sp;

    /* Trigger the map-wide skill event. */
    if (op->map && op->map->events) {
        int retval = trigger_map_event(MEVENT_SKILL_USED, op->map, op, NULL, NULL, NULL, dir);

        /* So the plugin's return value can affect the returned value. */
        if (retval) {
            return retval - 1;
        }
    }

    switch (skill) {
        case SK_FIND_TRAPS:
            success = find_traps(op, op->level);
            break;

        case SK_REMOVE_TRAPS:
            success = remove_trap(op);
            break;

        case SK_CONSTRUCTION:
            construction_do(op, dir);
            return success;

        case SK_INSCRIPTION:
            success = skill_inscription(op, params);
            break;

        case SK_THROWING:
            if (CONTR(op)->equipment[PLAYER_EQUIP_AMMO]) {
                object_throw(CONTR(op)->equipment[PLAYER_EQUIP_AMMO], op, dir);
            }
            else {
                draw_info(COLOR_WHITE, op, "You don't have any ammunition readied to throw.");
            }

            break;

        default:
            draw_info(COLOR_WHITE, op, "This skill is not usable in this way.");
            return 0;
    }

    /* This is a good place to add experience for successfull use of skills.
     * Note that add_exp() will figure out player/monster experience
     * gain problems. */
    if (success) {
        add_exp(op, success, op->chosen_skill->stats.sp, 0);
    }

    return success;
}

/**
 * Calculates amount of experience can be gained for
 * successfull use of a skill.
 * @param who Player/creature that used the skill.
 * @param op Object that was 'defeated'.
 * @param level Level of the skill. If -1, will get level of who's chosen
 * skill.
 * @return Experience for the skill use. */
sint64 calc_skill_exp(object *who, object *op, int level)
{
    int who_lvl = level;
    sint64 op_exp = 0;
    int op_lvl = 0;
    float exp_mul, max_mul, tmp;

    /* No exp for non players. */
    if (!who || who->type != PLAYER) {
        logger_print(LOG(DEBUG), "called with who != PLAYER or NULL (%s (%s)- %s)", query_name(who, NULL), !who ? "NULL" : "", query_name(op, NULL));
        return 0;
    }

    if (level == -1) {
        /* The related skill level */
        who_lvl = SK_level(who);
    }

    if (!op) {
        op_lvl = who->map->difficulty < 1 ? 1 : who->map->difficulty;
        op_exp = 0;
    }
    /* All other items/living creatures */
    else {
        op_exp = op->stats.exp;
        op_lvl = op->level;
    }

    /* No exp for no level and no exp ;) */
    if (op_lvl < 1 || op_exp < 1) {
        return 0;
    }

    if (who_lvl < 2) {
        max_mul = 0.85f;
    }

    if (who_lvl < 3) {
        max_mul = 0.7f;
    }
    else if (who_lvl < 4) {
        max_mul = 0.6f;
    }
    else if (who_lvl < 5) {
        max_mul = 0.45f;
    }
    else if (who_lvl < 7) {
        max_mul = 0.35f;
    }
    else if (who_lvl < 8) {
        max_mul = 0.3f;
    }
    else {
        max_mul = 0.25f;
    }

    /* We first get a global level difference multiplicator */
    exp_mul = calc_level_difference(who_lvl, op_lvl);
    op_exp = (int) ((float) op_exp * lev_exp[op_lvl] * exp_mul);
    tmp = ((float) (new_levels[who_lvl + 1] - new_levels[who_lvl]) * 0.1f) * max_mul;

    if ((float) op_exp > tmp) {
        op_exp = (int) tmp;
    }

    return op_exp;
}

/**
 * Initialize the experience system. */
void init_new_exp_system(void)
{
    int i;
    archetype *at;
    char buf[MAX_BUF];

    for (i = 0; i < NROFSKILLS; i++) {
        snprintf(buf, sizeof(buf), "skill_%s", skills[i].name);
        string_replace_char(buf, " ", '_');

        at = find_archetype(buf);

        if (!at) {
            continue;
        }

        skills[i].at = at;

        /* Set some default values for the archetype. */
        at->clone.stats.sp = i;
        at->clone.level = 1;
        at->clone.stats.exp = 0;
    }
}

/**
 * Check skill for firing.
 * @param op Who is firing.
 * @param weapon Weapon that is being fired.
 * @return 1 on success, 0 on failure. */
int check_skill_to_fire(object *op, object *weapon)
{
    int skillnr;

    skillnr = -1;

    if (weapon->type == BOW) {
        if (weapon->item_skill) {
            skillnr = weapon->item_skill - 1;
        }
        else {
            skillnr = SK_BOW_ARCHERY;
        }
    }
    else if (weapon->type == SPELL) {
        skillnr = SK_WIZARDRY_SPELLS;
    }
    else if (weapon->type == ROD || weapon->type == WAND) {
        skillnr = SK_MAGIC_DEVICES;
    }
    else if (weapon->type == ARROW) {
        skillnr = SK_THROWING;
    }
    else if (weapon->type == SKILL) {
        skillnr = weapon->stats.sp;
    }

    if (skillnr == -1) {
        return 0;
    }

    return change_skill(op, skillnr);
}

/**
 * Linking skills with experience objects and creating a linked list of
 * skills for later fast access.
 * @param pl Player. */
void link_player_skills(object *pl)
{
    int i;
    object *tmp;

    pl->stats.exp = 0;

    for (i = 0; i < NROFSKILLS; i++) {
        /* Skip unused skill entries. */
        if (!skills[i].at) {
            continue;
        }

        if (!CONTR(pl)->skill_ptr[i]) {
            tmp = object_create_clone(&skills[i].at->clone);
            insert_ob_in_ob(tmp, pl);
            CONTR(pl)->skill_ptr[i] = tmp;
        }

        if (!QUERY_FLAG(CONTR(pl)->skill_ptr[i], FLAG_STAND_STILL)) {
            pl->stats.exp += CONTR(pl)->skill_ptr[i]->stats.exp;

            if (pl->stats.exp >= (sint64) MAX_EXPERIENCE) {
                pl->stats.exp = MAX_EXPERIENCE;
            }
        }

        player_lvl_adj(pl, CONTR(pl)->skill_ptr[i]);
    }

    player_lvl_adj(pl, NULL);
}

/**
 * This changes the object's skill.
 * @param who Living to change skill for.
 * @param sk_index ID of the skill.
 * @return 0 on failure, 1 on success. */
int change_skill(object *who, int sk_index)
{
    if (who->type != PLAYER) {
        return 0;
    }

    if (who->chosen_skill && who->chosen_skill->stats.sp == sk_index) {
        return 1;
    }

    if (CONTR(who)->skill_ptr[sk_index]) {
        who->chosen_skill = CONTR(who)->skill_ptr[sk_index];
        return 1;
    }

    return 0;
}

/**
 * Core routine for use when we attack using the skills system. There
 * aren't too many changes from before, basically this is a 'wrapper' for
 * the old attack system. In essence, this code handles all skill-based
 * attacks, ie hth, missile and melee weapons should be treated here. If
 * an opponent is already supplied by move_object(), we move right onto
 * do_skill_attack(), otherwise we find if an appropriate opponent
 * exists.
 * @param tmp Targetted monster.
 * @param pl What is attacking.
 * @param dir Attack direction.
 * @param string Describes the attack ("karate-chop", "punch", ...).
 * @return 1 if the attack damaged the opponent. */
int skill_attack(object *tmp, object *pl, int dir, char *string)
{
    int xt, yt;
    mapstruct *m;

    if (!dir) {
        dir = pl->facing;
    }

    /* If we don't yet have an opponent, find if one exists, and attack.
     * Legal opponents are the same as outlined in move_object() */
    if (tmp == NULL) {
        xt = pl->x + freearr_x[dir];
        yt = pl->y + freearr_y[dir];

        if (!(m = get_map_from_coord(pl->map, &xt,&yt))) {
            return 0;
        }

        for (tmp = GET_MAP_OB(m, xt, yt); tmp; tmp = tmp->above) {
            if ((IS_LIVE(tmp) && (tmp->head == NULL ? tmp->stats.hp > 0 : tmp->head->stats.hp > 0)) || QUERY_FLAG(tmp, FLAG_CAN_ROLL) || tmp->type == DOOR) {
                if (pl->type == PLAYER && tmp->type == PLAYER && !pvp_area(pl, tmp)) {
                    continue;
                }

                break;
            }
        }
    }

    if (tmp != NULL) {
        return do_skill_attack(tmp, pl, string);
    }

    if (pl->type == PLAYER) {
        draw_info(COLOR_WHITE, pl, "There is nothing to attack!");
    }

    return 0;
}

/**
 * We have got an appropriate opponent from either move_object() or
 * skill_attack(). In this part we get on with attacking, take care of
 * messages from the attack and changes in invisible.
 * Returns true if the attack damaged the opponent.
 * @param tmp Targetted monster.
 * @param op What is attacking.
 * @param string Describes the attack ("karate-chop", "punch", ...).
 * @return 1 if the attack damaged the opponent. */
static int do_skill_attack(object *tmp, object *op, char *string)
{
    int success;
    char *name = query_name(tmp, op);

    if (op->type == PLAYER) {
        if (CONTR(op)->equipment[PLAYER_EQUIP_WEAPON] && CONTR(op)->equipment[PLAYER_EQUIP_WEAPON]->type == WEAPON && CONTR(op)->equipment[PLAYER_EQUIP_WEAPON]->item_skill) {
            op->chosen_skill = CONTR(op)->skill_ptr[CONTR(op)->equipment[PLAYER_EQUIP_WEAPON]->item_skill - 1];
        }
        else {
            op->chosen_skill = CONTR(op)->skill_ptr[SK_UNARMED];
        }
    }

    success = attack_ob(tmp, op);

    /* Print appropriate messages to the player. */
    if (success && string != NULL) {
        if (op->type == PLAYER) {
            draw_info_format(COLOR_WHITE, op, "You %s %s!", string, name);
        }
        else if (tmp->type == PLAYER) {
            draw_info_format(COLOR_WHITE, tmp, "%s %s you!", query_name(op, NULL), string);
        }
    }

    return success;
}

/**
 * Get the level of player's chosen skill.
 * @param op Player.
 * @return The level of the chosen skill, level of the player if no
 * chosen skill. */
int SK_level(object *op)
{
    object *head = op->head ? op->head : op;
    int level;

    if (head->type == PLAYER && head->chosen_skill && head->chosen_skill->level != 0) {
        level = head->chosen_skill->level;
    }
    else {
        level = head->level;
    }

    /* Safety */
    if (level <= 0) {
        level = 1;
    }

    return level;
}

/**
 * Get pointer to player's chosen skill object.
 * @param op Player.
 * @return Chosen skill object, NULL if no chosen skill. */
object *SK_skill(object *op)
{
    object *head = op->head ? op->head : op;

    if (head->type == PLAYER && head->chosen_skill) {
        return head->chosen_skill;
    }

    return NULL;
}
