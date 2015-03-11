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
 * Player related functions. */

#include <global.h>
#include <loader.h>

static int save_life(object *op);
static void remove_unpaid_objects(object *op, object *env);

/**
 * Player memory pool. */
mempool_struct *pool_player;

/**
 * Initialize the player API. */
void player_init(void)
{
    pool_player = mempool_create("players", 25, sizeof(player),
            MEMPOOL_ALLOW_FREEING, NULL, NULL, NULL, NULL);
}

/**
 * Deinitialize the player API. */
void player_deinit(void)
{
    while (first_player) {
        free_player(first_player);
    }
}

/**
 * Disconnect all currently connected players. */
void player_disconnect_all(void)
{
    while (first_player) {
        first_player->socket.state = ST_DEAD;
        remove_ns_dead_player(first_player);
    }
}

/**
 * Loop through the player list and find player specified by plname.
 * @param plname The player name to find.
 * @return Player structure if found, NULL otherwise. */
player *find_player(const char *plname)
{
    player *pl;

    for (pl = first_player; pl; pl = pl->next) {
        if (strncasecmp(pl->ob->name, plname, MAX_NAME) == 0) {
            return pl;
        }
    }

    return NULL;
}

/**
 * Grab the Message of the Day from a file.
 *
 * First motd_custom is tried, and if that doesn't exist, motd is used
 * instead.
 * @param op Player object to print the message to. */
void display_motd(object *op)
{
    char buf[MAX_BUF];
    FILE *fp;

    snprintf(buf, sizeof(buf), "%s/motd_custom", settings.datapath);

    if ((fp = fopen(buf, "r")) == NULL) {
        snprintf(buf, sizeof(buf), "%s/motd", settings.datapath);

        if ((fp = fopen(buf, "r")) == NULL) {
            return;
        }
    }

    while (fgets(buf, MAX_BUF, fp) != NULL) {
        char *cp;

        if (buf[0] == '#' || buf[0] == '\n') {
            continue;
        }

        cp = strchr(buf, '\n');

        if (cp != NULL) {
            *cp = '\0';
        }

        draw_info(COLOR_WHITE, op, buf);
    }

    fclose(fp);
    draw_info(COLOR_WHITE, op, " ");
}

/**
 * Returns the player structure. If 'p' is null, we create a new one.
 * Otherwise, we recycle the one that is passed.
 * @param p Player structure to recycle or NULL for new structure.
 * @return The player structure. */
static player *get_player(player *p)
{
    if (!p) {
        p = mempool_get(pool_player);

        if (!last_player) {
            first_player = last_player = p;
        } else {
            last_player->next = p;
            p->prev = last_player;
            last_player = p;
        }
    } else {
        /* Clears basically the entire player structure except
         * for next and socket. */
        memset((char *) p + offsetof(player, maplevel), 0, sizeof(player) - offsetof(player, maplevel));
    }

#ifdef AUTOSAVE
    p->last_save_tick = 9999999;
#endif

    p->target_hp = -1;
    p->gen_sp_armour = 0;
    p->last_speed = -1;
    p->update_los = 1;

    p->last_stats.exp = -1;

    return p;
}

/**
 * Free a player structure. Takes care of removing this player from the
 * list of players, and frees the socket for this player.
 * @param pl The player structure to free. */
void free_player(player *pl)
{
    pl->socket.state = ST_DEAD;

    /* Free command permissions. */
    if (pl->cmd_permissions) {
        int i;

        for (i = 0; i < pl->num_cmd_permissions; i++) {
            if (pl->cmd_permissions[i]) {
                efree(pl->cmd_permissions[i]);
            }
        }

        efree(pl->cmd_permissions);
    }

    if (pl->faction_ids) {
        int i;

        for (i = 0; i < pl->num_faction_ids; i++) {
            FREE_ONLY_HASH(pl->faction_ids[i]);
        }

        efree(pl->faction_ids);
    }

    if (pl->faction_reputation) {
        efree(pl->faction_reputation);
    }

    if (pl->region_maps) {
        int i;

        for (i = 0; i < pl->num_region_maps; i++) {
            if (pl->region_maps[i]) {
                efree(pl->region_maps[i]);
            }
        }

        efree(pl->region_maps);
    }

    player_path_clear(pl);

    /* Now remove from list of players. */
    if (pl->prev) {
        pl->prev->next = pl->next;
    } else {
        first_player = pl->next;
    }

    if (pl->next) {
        pl->next->prev = pl->prev;
    } else {
        last_player = pl->prev;
    }

    free_newsocket(&pl->socket);

    if (pl->ob) {
        SET_FLAG(pl->ob, FLAG_NO_FIX_PLAYER);

        if (!QUERY_FLAG(pl->ob, FLAG_REMOVED)) {
            object_remove(pl->ob, 0);
        }

        object_destroy(pl->ob);
    }
}

/**
 * Give initial items to object pl. This is used when player creates a
 * new character.
 * @param pl The player object.
 * @param items Treasure list of items to give. */
void give_initial_items(object *pl, treasurelist *items)
{
    object *op, *next = NULL;

    if (pl->randomitems) {
        create_treasure(items, pl, GT_ONLY_GOOD | GT_NO_VALUE, 1, T_STYLE_UNSET, ART_CHANCE_UNSET, 0, NULL);
    }

    for (op = pl->inv; op; op = next) {
        next = op->below;

        /* Forces get applied by default */
        if (op->type == FORCE) {
            SET_FLAG(op, FLAG_APPLIED);
        }

        /* We never give weapons/armour if they cannot be used by this
         * player due to race restrictions */
        if (pl->type == PLAYER) {
            if ((!QUERY_FLAG(pl, FLAG_USE_ARMOUR) && (op->type == ARMOUR || op->type == BOOTS || op->type == CLOAK || op->type == HELMET || op->type == SHIELD || op->type == GLOVES || op->type == BRACERS || op->type == GIRDLE)) || (!QUERY_FLAG(pl, FLAG_USE_WEAPON) && op->type == WEAPON)) {
                object_remove(op, 0);
                object_destroy(op);
                continue;
            }
        }

        /* Give starting characters identified, uncursed, and undamned
         * items. */
        if (need_identify(op)) {
            SET_FLAG(op, FLAG_IDENTIFIED);
            CLEAR_FLAG(op, FLAG_CURSED);
            CLEAR_FLAG(op, FLAG_DAMNED);
        }

        /* Apply initial armor */
        if (IS_ARMOR(op)) {
            manual_apply(pl, op, 0);
        }
    }
}

/**
 * This is similar to handle_player(), but is only used by the new
 * client/server stuff.
 *
 * This is sort of special, in that the new client/server actually uses
 * the new speed values for commands.
 * @param pl Player to handle.
 * @retval -1 Player is invalid.
 * @retval 0 No more actions to do.
 * @retval 1 There are more actions we can do. */
int handle_newcs_player(player *pl)
{
    if (!pl->ob || !OBJECT_ACTIVE(pl->ob)) {
        return -1;
    }

    handle_client(&pl->socket, pl);

    if (!pl->ob || !OBJECT_ACTIVE(pl->ob) || pl->socket.state == ST_DEAD) {
        return -1;
    }

    /* Check speed. */
    if (pl->ob->speed_left < 0.0f) {
        return 0;
    }

    /* If we are here, we're never paralyzed anymore. */
    CLEAR_FLAG(pl->ob, FLAG_PARALYZED);

    if (CONTR(pl->ob)->run_on) {
        /* All move commands take 1 tick, at least for now. */
        pl->ob->speed_left--;
        move_object(pl->ob, pl->run_on_dir + 1);

        if (pl->ob->speed_left > 0) {
            return 1;
        } else {
            return 0;
        }
    }

    return 0;
}

/**
 * Can the player be saved by an item?
 * @param op Player to try to save.
 * @retval 1 Player had his life saved by an item, first item saving life
 * is removed.
 * @retval 0 Player had no life-saving item. */
static int save_life(object *op)
{
    object *tmp;

    if (!QUERY_FLAG(op, FLAG_LIFESAVE)) {
        return 0;
    }

    for (tmp = op->inv; tmp != NULL; tmp = tmp->below) {
        if (QUERY_FLAG(tmp, FLAG_APPLIED) && QUERY_FLAG(tmp, FLAG_LIFESAVE)) {
            play_sound_map(op->map, CMD_SOUND_EFFECT, "explosion.ogg", op->x, op->y, 0, 0);
            draw_info_format(COLOR_WHITE, op, "Your %s vibrates violently, then evaporates.", query_name(tmp, NULL));

            object_remove(tmp, 0);
            object_destroy(tmp);
            CLEAR_FLAG(op, FLAG_LIFESAVE);

            if (op->stats.hp < 0) {
                op->stats.hp = op->stats.maxhp;
            }

            if (op->stats.food < 0) {
                op->stats.food = 999;
            }

            /* Bring him home. */
            object_enter_map(op, NULL, ready_map_name(CONTR(op)->savebed_map, 0), CONTR(op)->bed_x, CONTR(op)->bed_y, 1);
            return 1;
        }
    }

    logger_print(LOG(BUG), "LIFESAVE set without applied object.");
    CLEAR_FLAG(op, FLAG_LIFESAVE);
    /* Bring him home. */
    object_enter_map(op, NULL, ready_map_name(CONTR(op)->savebed_map, 0), CONTR(op)->bed_x, CONTR(op)->bed_y, 1);
    return 0;
}

/**
 * This goes through the inventory and removes unpaid objects, and puts
 * them back in the map (location and map determined by values of env).
 * This function will descend into containers.
 * @param op Object to start the search from.
 * @param env Map location determined by this object. */
static void remove_unpaid_objects(object *op, object *env)
{
    object *next;

    while (op) {
        /* Make sure we have a good value, in case
         * we remove object 'op' */
        next = op->below;

        if (QUERY_FLAG(op, FLAG_UNPAID)) {
            object_remove(op, 0);
            op->x = env->x;
            op->y = env->y;
            insert_ob_in_map(op, env->map, NULL, 0);
        } else if (op->inv) {
            remove_unpaid_objects(op->inv, env);
        }

        op = next;
    }
}

/**
 * Figures out how much hp/mana points to regenerate.
 * @param regen Regeneration value used for client (for example,
 * player::gen_client_hp).
 * @param regen_remainder Pointer to regen remainder (for example,
 * player::gen_hp_remainder).
 * @return How much to regenerate. */
static int get_regen_amount(uint16 regen, uint16 *regen_remainder)
{
    int ret = 0;
    float division;

    /* Check whether it's time to update the remainder variable (which
     * will distribute the remainder evenly over time). */
    if (pticks % 8 == 0) {
        *regen_remainder += regen;
    }

    /* First check if we can distribute it evenly, if not, try to remove
     * leftovers, if any. */
    for (division = (float) MAX_TICKS; ; division = 1.0f) {
        if (*regen_remainder / 10.0f / division >= 1.0f) {
            int add = (int) *regen_remainder / 10.0f / division;

            ret += add;
            *regen_remainder -= add * 10;
            break;
        }

        if (division == 1.0f) {
            break;
        }
    }

    return ret;
}

/**
 * Regenerate player's hp/mana, decrease food, etc.
 *
 * We will only regenerate HP and mana if the player has some food in their
 * stomach.
 * @param op Player. */
void do_some_living(object *op)
{
    int last_food = op->stats.food;
    int gen_hp, gen_sp;
    int rate_hp = 2000;
    int rate_sp = 1200;
    int add;

    gen_hp = (CONTR(op)->gen_hp * (rate_hp / 20)) + (op->stats.maxhp / 4);
    gen_sp = (CONTR(op)->gen_sp * (rate_sp / 20)) + op->stats.maxsp;

    gen_sp = gen_sp * 10 / MAX(CONTR(op)->gen_sp_armour, 10);

    /* Update client's regen rates. */
    CONTR(op)->gen_client_hp = ((float) MAX_TICKS / ((float) rate_hp / (MAX(gen_hp, 20) + 10))) * 10.0f;
    CONTR(op)->gen_client_sp = ((float) MAX_TICKS / ((float) rate_sp / (MAX(gen_sp, 20) + 10))) * 10.0f;

    /* Regenerate hit points. */
    if (op->stats.hp < op->stats.maxhp && op->stats.food) {
        add = get_regen_amount(CONTR(op)->gen_client_hp, &CONTR(op)->gen_hp_remainder);

        if (add) {
            op->stats.hp += add;
            CONTR(op)->stat_hp_regen += add;

            if (op->stats.hp > op->stats.maxhp) {
                op->stats.hp = op->stats.maxhp;
            }

            if (!CONTR(op)->tgm) {
                op->stats.food--;

                if (CONTR(op)->digestion < 0) {
                    op->stats.food += CONTR(op)->digestion;
                } else if (CONTR(op)->digestion > 0 && rndm(0, CONTR(op)->digestion)) {
                    op->stats.food = last_food;
                }
            }
        }
    } else {
        CONTR(op)->gen_hp_remainder = 0;
    }

    /* Regenerate mana. */
    if (op->stats.sp < op->stats.maxsp && op->stats.food) {
        add = get_regen_amount(CONTR(op)->gen_client_sp, &CONTR(op)->gen_sp_remainder);

        if (add) {
            op->stats.sp += add;
            CONTR(op)->stat_sp_regen += add;

            if (op->stats.sp > op->stats.maxsp) {
                op->stats.sp = op->stats.maxsp;
            }

            if (!CONTR(op)->tgm) {
                op->stats.food--;

                if (CONTR(op)->digestion < 0) {
                    op->stats.food += CONTR(op)->digestion;
                } else if (CONTR(op)->digestion > 0 && rndm(0, CONTR(op)->digestion)) {
                    op->stats.food = last_food;
                }
            }
        }
    } else {
        CONTR(op)->gen_sp_remainder = 0;
    }

    /* Digestion */
    if (--op->last_eat < 0) {
        int bonus = MAX(CONTR(op)->digestion, 0);
        int penalty = MAX(-CONTR(op)->digestion, 0);

        if (CONTR(op)->gen_hp > 0) {
            op->last_eat = 25 * (1 + bonus) / (CONTR(op)->gen_hp + penalty + 1);
        } else {
            op->last_eat = 25 * (1 + bonus) / (penalty + 1);
        }

        if (!CONTR(op)->tgm) {
            op->stats.food--;
        }
    }

    if (op->stats.food < 0 && op->stats.hp >= 0) {
        object *tmp, *flesh = NULL;

        for (tmp = op->inv; tmp; tmp = tmp->below) {
            if (!QUERY_FLAG(tmp, FLAG_UNPAID)) {
                if (tmp->type == FOOD || tmp->type == DRINK) {
                    draw_info(COLOR_WHITE, op, "You blindly grab for a bite of food.");
                    manual_apply(op, tmp, 0);

                    if (op->stats.food >= 0 || op->stats.hp < 0) {
                        break;
                    }
                } else if (tmp->type == FLESH) {
                    flesh = tmp;
                }
            }
        }

        /* If player is still starving, it means they don't have any food, so
         * eat flesh instead. */
        if (op->stats.food < 0 && op->stats.hp >= 0 && flesh) {
            draw_info(COLOR_WHITE, op, "You blindly grab for a bite of food.");
            manual_apply(op, flesh, 0);
        }
    }

    while (op->stats.food < 0 && op->stats.hp > 0) {
        op->stats.food++;
        op->stats.hp--;
    }

    if ((op->stats.hp <= 0 || op->stats.food < 0) && !CONTR(op)->tgm) {
        draw_info_format(COLOR_WHITE, NULL, "%s starved to death.", op->name);
        snprintf(CONTR(op)->killer, sizeof(CONTR(op)->killer), "starvation");
        kill_player(op);
    }
}

/**
 * If the player should die (lack of hp, food, etc), we call this.
 *
 * Will remove diseases, apply death penalties, and so on.
 * @param op The player in jeopardy. */
void kill_player(object *op)
{
    char buf[HUGE_BUF];
    object *tmp;

    if (pvp_area(NULL, op)) {
        draw_info(COLOR_NAVY, op, "You have been defeated in combat!\nLocal medics have saved your life...");

        /* Restore player */
        cast_heal(op, MAXLEVEL, op, SP_CURE_POISON);
        /* Remove any disease */
        cure_disease(op, NULL);
        op->stats.hp = op->stats.maxhp;
        op->stats.sp = op->stats.maxsp;

        if (op->stats.food <= 0) {
            op->stats.food = 999;
        }

        /* Create a bodypart-trophy to make the winner happy */
        tmp = arch_to_object(find_archetype("finger"));

        if (tmp) {
            char race[MAX_BUF];

            snprintf(buf, sizeof(buf), "%s's finger", op->name);
            FREE_AND_COPY_HASH(tmp->name, buf);
            snprintf(buf, sizeof(buf), "This finger has been cut off %s the %s, when %s was defeated at level %d by %s.", op->name, player_get_race_class(op, race, sizeof(race)), gender_subjective[object_get_gender(op)], op->level, CONTR(op)->killer[0] == '\0' ? "something nasty" : CONTR(op)->killer);
            FREE_AND_COPY_HASH(tmp->msg, buf);
            tmp->value = 0;
            tmp->material = 0;
            tmp->type = 0;
            tmp->x = op->x, tmp->y = op->y;
            insert_ob_in_map(tmp, op->map, op, 0);
        }

        CONTR(op)->killer[0] = '\0';

        /* Teleport defeated player to new destination */
        transfer_ob(op, MAP_ENTER_X(op->map), MAP_ENTER_Y(op->map), 0, NULL, NULL);
        return;
    }

    if (save_life(op)) {
        return;
    }

    /* Trigger the DEATH event */
    if (trigger_event(EVENT_DEATH, NULL, op, NULL, NULL, 0, 0, 0, SCRIPT_FIX_ALL)) {
        return;
    }

    CONTR(op)->stat_deaths++;

    /* Trigger the global GDEATH event */
    trigger_global_event(GEVENT_PLAYER_DEATH, NULL, op);

    play_sound_player_only(CONTR(op), CMD_SOUND_EFFECT, "playerdead.ogg", 0, 0, 0, 0);

    /* Put a gravestone up where the character 'almost' died. */
    tmp = arch_to_object(find_archetype("gravestone"));
    snprintf(buf, sizeof(buf), "%s's gravestone", op->name);
    FREE_AND_COPY_HASH(tmp->name, buf);
    FREE_AND_COPY_HASH(tmp->msg, gravestone_text(op));
    tmp->x = op->x;
    tmp->y = op->y;
    insert_ob_in_map(tmp, op->map, NULL, 0);

    /* Subtract the experience points, if we died because of food give us
     * food, and reset HP... */

    /* Remove any poisoning the character may be suffering. */
    cast_heal(op, MAXLEVEL, op, SP_CURE_POISON);
    /* Remove any disease */
    cure_disease(op, NULL);

    if (op->stats.food <= 0) {
        op->stats.food = 999;
    }

    op->stats.hp = op->stats.maxhp;
    op->stats.sp = op->stats.maxsp;

    hiscore_check(op, 1);

    /* Otherwise the highscore can get entries like 'xxx was killed by pudding
     * on map Wilderness' even if they were killed in a dungeon. */
    CONTR(op)->killer[0] = '\0';

    /* Check to see if the player is in a shop. Ii so, then check to see
     * if the player has any unpaid items. If so, remove them and put
     * them back in the map. */
    tmp = GET_MAP_OB(op->map, op->x, op->y);

    if (tmp && tmp->type == SHOP_FLOOR) {
        remove_unpaid_objects(op->inv, op);
    }

    /* Move player to his current respawn position (last savebed). */
    object_enter_map(op, NULL, ready_map_name(CONTR(op)->savebed_map, 0), CONTR(op)->bed_x, CONTR(op)->bed_y, 1);

    /* Show a nasty message */
    draw_info(COLOR_WHITE, op, "YOU HAVE DIED.");
    player_save(op);
}

/**
 * Handles object throwing objects of type "DUST".
 * @todo This function needs to be rewritten. Works for area effect
 * spells only now.
 * @param op Object throwing.
 * @param throw_ob What to throw.
 * @param dir Direction to throw into. */
void cast_dust(object *op, object *throw_ob, int dir)
{
    archetype *arch = NULL;

    if (!(spells[throw_ob->stats.sp].flags & SPELL_DESC_DIRECTION)) {
        logger_print(LOG(BUG), "Warning, dust %s is not AoE spell!!", query_name(throw_ob, NULL));
        return;
    }

    if (spells[throw_ob->stats.sp].archname) {
        arch = find_archetype(spells[throw_ob->stats.sp].archname);
    }

    /* Casting POTION 'dusts' is really use_magic_item skill */
    if (op->type == PLAYER && throw_ob->type == POTION && !change_skill(op, SK_MAGIC_DEVICES)) {
        return;
    }

    if (throw_ob->type == POTION && arch != NULL) {
        cast_cone(op, throw_ob, dir, 10, throw_ob->stats.sp, arch);
    } else if ((arch = find_archetype("dust_effect")) != NULL) {
        /* dust_effect */
        cast_cone(op, throw_ob, dir, 1, 0, arch);
    } else {
        /* Problem occurred! */
        logger_print(LOG(BUG), "can't find an archetype to use!");
    }

    if (op->type == PLAYER && arch) {
        draw_info_format(COLOR_WHITE, op, "You cast %s.", query_name(throw_ob, NULL));
    }

    if (op->chosen_skill) {
        op->chosen_skill->stats.maxsp = throw_ob->last_grace;
    }

    if (!QUERY_FLAG(throw_ob, FLAG_REMOVED)) {
        destruct_ob(throw_ob);
    }
}

/**
 * Test for PVP area.
 *
 * If only one object is given, it tests for that. Otherwise if two
 * objects are given, both objects must be in PVP area.
 *
 * Considers parties.
 * @param attacker First object.
 * @param victim Second object.
 * @return 1 if PVP is possible, 0 otherwise. */
int pvp_area(object *attacker, object *victim)
{
    /* No attacking of party members. */
    if (attacker && victim && attacker->type == PLAYER && victim->type == PLAYER && CONTR(attacker)->party != NULL && CONTR(victim)->party != NULL && CONTR(attacker)->party == CONTR(victim)->party) {
        return 0;
    }

    if (attacker && victim && attacker == victim) {
        return 0;
    }

    if (attacker && attacker->map) {
        if (!(attacker->map->map_flags & MAP_FLAG_PVP) || GET_MAP_FLAGS(attacker->map, attacker->x, attacker->y) & P_NO_PVP || GET_MAP_SPACE_PTR(attacker->map, attacker->x, attacker->y)->extra_flags & MSP_EXTRA_NO_PVP) {
            return 0;
        }
    }

    if (victim && victim->map) {
        if (!(victim->map->map_flags & MAP_FLAG_PVP) || GET_MAP_FLAGS(victim->map, victim->x, victim->y) & P_NO_PVP || GET_MAP_SPACE_PTR(victim->map, victim->x, victim->y)->extra_flags & MSP_EXTRA_NO_PVP) {
            return 0;
        }
    }

    return 1;
}

/**
 * Looks for the skill and returns a pointer to it if found.
 * @param op The object to look for the skill in.
 * @param skillnr Skill ID.
 * @return The skill if found, NULL otherwise. */
object *find_skill(object *op, int skillnr)
{
    object *tmp;

    for (tmp = op->inv; tmp; tmp = tmp->below) {
        if (tmp->type == SKILL && tmp->stats.sp == skillnr) {
            return tmp;
        }
    }

    return NULL;
}

/**
 * Check whether player can carry the specified weight.
 * @param pl Player.
 * @param weight Weight to check.
 * @return 1 if the player can carry that weight, 0 otherwise. */
int player_can_carry(object *pl, uint32 weight)
{
    uint32 effective_weight_limit;

    if (pl->stats.Str <= MAX_STAT) {
        effective_weight_limit = weight_limit[pl->stats.Str];
    } else {
        effective_weight_limit = weight_limit[MAX_STAT];
    }

    return (pl->carrying + weight) < effective_weight_limit;
}

/**
 * Combine player's race with their class (if there is one).
 * @param op Player.
 * @param buf Buffer to write into.
 * @param size Size of 'buf'.
 * @return 'buf'. */
char *player_get_race_class(object *op, char *buf, size_t size)
{
    strncpy(buf, op->race, size - 1);

    if (CONTR(op)->class_ob) {
        shstr *name_female;

        strncat(buf, " ", size - strlen(buf) - 1);

        if (object_get_gender(op) == GENDER_FEMALE && (name_female = object_get_value(CONTR(op)->class_ob, "name_female"))) {
            strncat(buf, name_female, size - strlen(buf) - 1);
        } else {
            strncat(buf, CONTR(op)->class_ob->name, size - strlen(buf) - 1);
        }
    }

    return buf;
}

/**
 * Add a new path to player's paths queue.
 * @param pl Player to add the path for.
 * @param map Map we want to reach.
 * @param x X we want to reach.
 * @param y Y we want to reach. */
void player_path_add(player *pl, mapstruct *map, sint16 x, sint16 y)
{
    player_path *path = emalloc(sizeof(player_path));

    /* Initialize the values. */
    path->map = map;
    path->x = x;
    path->y = y;
    path->next = NULL;
    path->fails = 0;

    if (!pl->move_path) {
        pl->move_path = pl->move_path_end = path;
    } else {
        pl->move_path_end->next = path;
        pl->move_path_end = path;
    }
}

/**
 * Clear all queued paths.
 * @param pl Player to clear paths for. */
void player_path_clear(player *pl)
{
    player_path *path, *next;

    if (!pl->move_path) {
        return;
    }

    for (path = pl->move_path; path; path = next) {
        next = path->next;
        efree(path);
    }

    pl->move_path = NULL;
    pl->move_path_end = NULL;
}

/**
 * Handle player moving along pre-calculated path.
 * @param pl Player. */
void player_path_handle(player *pl)
{
    while (pl->ob->speed_left >= 0.0f && pl->move_path) {
        player_path *tmp = pl->move_path;
        rv_vector rv;

        /* Make sure the map exists and is loaded, then get the range vector. */
        if (!tmp->map || tmp->map->in_memory != MAP_IN_MEMORY || !get_rangevector_from_mapcoords(pl->ob->map, pl->ob->x, pl->ob->y, tmp->map, tmp->x, tmp->y, &rv, 0)) {
            /* Something went wrong (map not loaded or we got teleported
             * somewhere), clear all queued paths. */
            player_path_clear(pl);
            return;
        } else {
            int dir = rv.direction;
            mapstruct *map;
            int x, y;

            map = pl->ob->map;
            x = pl->ob->x;
            y = pl->ob->y;

            if (map == tmp->map && x == tmp->x && y == tmp->y) {
                /* The player is already on the correct tile, so there's nothing
                 * to do -- they might have been teleported, for example. */
                dir = 0;
            } else {
                /* Move to the direction directly. */
                dir = move_object(pl->ob, dir);

                if (dir == 0) {
                    int diff;

                    /* Try to move around corners otherwise. */
                    for (diff = 1; diff <= 2; diff++) {
                        /* Try left or right first? */
                        int m = 1 - (RANDOM() & 2);

                        dir = move_object(pl->ob, absdir(dir + diff * m));

                        if (dir == 0) {
                            dir = move_object(pl->ob, absdir(dir + diff * m));
                        }

                        if (dir != 0) {
                            break;
                        }
                    }
                }
            }

            if (dir == -1) {
                return;
            }

            x += freearr_x[dir];
            y += freearr_y[dir];

            map = get_map_from_coord(map, &x, &y);

            /* See if we succeeded in moving where we wanted. */
            if (map == tmp->map && x == tmp->x && y == tmp->y) {
                pl->move_path = tmp->next;
                efree(tmp);
            } else if ((rv.distance <= 1 && dir != 0) ||
                    tmp->fails > PLAYER_PATH_MAX_FAILS) {
                /* Clear all paths if we above check failed: this can happen
                 * if we got teleported somewhere else by a teleporter or a
                 * shop mat, in which case the player most likely doesn't want
                 * to move to the original destination. Also see if we failed
                 * to move to destination too many times already. */
                player_path_clear(pl);
                return;
            } else {
                /* Not any of the above; we failed to move where we wanted. */
                tmp->fails++;
            }

            pl->ob->speed_left--;
        }
    }
}

/**
 * Get player's reputation for the specified faction.
 * @param pl The player.
 * @param faction The faction name.
 * @return The faction reputation. */
sint64 player_faction_reputation(player *pl, shstr *faction)
{
    int i;

    for (i = 0; i < pl->num_faction_ids; i++) {
        if (pl->faction_ids[i] == faction) {
            return pl->faction_reputation[i];
        }
    }

    return 0;
}

/**
 * Update player's faction reputation.
 * @param pl The player.
 * @param faction Name of the faction.
 * @param add How much to modify the player's faction reputation (if
 * any). */
void player_faction_reputation_update(player *pl, shstr *faction, sint64 add)
{
    int i;

    for (i = 0; i < pl->num_faction_ids; i++) {
        if (pl->faction_ids[i] == faction) {
            pl->faction_reputation[i] += add;
            return;
        }
    }

    pl->faction_ids = erealloc(pl->faction_ids, sizeof(*pl->faction_ids) * (pl->num_faction_ids + 1));
    pl->faction_reputation = erealloc(pl->faction_reputation, sizeof(*pl->faction_reputation) * (pl->num_faction_ids + 1));
    pl->faction_ids[pl->num_faction_ids] = add_string(faction);
    pl->faction_reputation[pl->num_faction_ids] = add;
    pl->num_faction_ids++;
}

/**
 * Check whether player has a region map of the specified region.
 * @param pl The player.
 * @param r The region to check.
 * @return 1 if the player has region map of the specified region, 0
 * otherwise. */
int player_has_region_map(player *pl, region_struct *r)
{
    int i;

    for (i = 0; i < pl->num_region_maps; i++) {
        if (pl->region_maps[i] && !strcmp(r->name, pl->region_maps[i])) {
            return 1;
        }
    }

    return 0;
}

/**
 * Sanitize player's text input, removing extraneous whitespace,
 * unprintable characters, etc.
 * @param str Input to sanitize.
 * @return Sanitized input; can be NULL if there's nothing in the string
 * left. */
char *player_sanitize_input(char *str)
{
    if (!str) {
        return NULL;
    }

    string_replace_unprintable_chars(str);
    string_whitespace_squeeze(str);
    string_whitespace_trim(str);

    return *str == '\0' ? NULL : str;
}

/**
 * Cleans up a string that is, presumably, a player name.
 * @param str The player name to clean up. */
void player_cleanup_name(char *str)
{
    string_whitespace_trim(str);
    string_capitalize(str);
}

/**
 * Recursive helper function for find_marked_object() to search for
 * marked object in containers.
 * @param op Object. Should be a player.
 * @param marked Marked object.
 * @param marked_count Marked count.
 * @return The object if found, NULL otherwise. */
static object *find_marked_object_rec(object *op, object **marked, uint32 *marked_count)
{
    object *tmp, *tmp2;

    /* This may seem like overkill, but we need to make sure that they
     * player hasn't dropped the item. We use count on the off chance
     * that an item got reincarnated at some point. */
    for (tmp = op->inv; tmp; tmp = tmp->below) {
        if (IS_INVISIBLE(tmp, op)) {
            continue;
        }

        if (tmp == *marked) {
            if (tmp->count == *marked_count) {
                return tmp;
            } else {
                *marked = NULL;
                *marked_count = 0;
                return NULL;
            }
        } else if (tmp->inv) {
            tmp2 = find_marked_object_rec(tmp, marked, marked_count);

            if (tmp2) {
                return tmp2;
            }

            if (*marked == NULL) {
                return NULL;
            }
        }
    }

    return NULL;
}

/**
 * Return the object the player has marked.
 * @param op Object. Should be a player.
 * @return Marked object if still valid, NULL otherwise. */
object *find_marked_object(object *op)
{
    if (op->type != PLAYER || !op || !CONTR(op) || !CONTR(op)->mark) {
        return NULL;
    }

    return find_marked_object_rec(op, &CONTR(op)->mark, &CONTR(op)->mark_count);
}

/**
 * Player examines a living object.
 * @param op Player.
 * @param tmp Object being examined. */
static void examine_living(object *op, object *tmp, StringBuffer *sb_capture)
{
    object *mon = tmp->head ? tmp->head : tmp;
    int val, val2, i, gender;

    gender = object_get_gender(mon);

    if (QUERY_FLAG(mon, FLAG_IS_GOOD)) {
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s is a good aligned %s %s.", gender_subjective_upper[gender], gender_noun[gender], mon->race);
    } else if (QUERY_FLAG(mon, FLAG_IS_EVIL)) {
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s is an evil aligned %s %s.", gender_subjective_upper[gender], gender_noun[gender], mon->race);
    } else if (QUERY_FLAG(mon, FLAG_IS_NEUTRAL)) {
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s is a neutral aligned %s %s.", gender_subjective_upper[gender], gender_noun[gender], mon->race);
    } else {
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s is a %s %s.", gender_subjective_upper[gender], gender_noun[gender], mon->race);
    }

    draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s is level %d.", gender_subjective_upper[gender], mon->level);
    draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s has a base damage of %d and hp of %d.", gender_subjective_upper[gender], mon->stats.dam, mon->stats.maxhp);
    draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s has a wc of %d and ac of %d.", gender_subjective_upper[gender], mon->stats.wc, mon->stats.ac);

    for (val = val2 = -1, i = 0; i < NROFATTACKS; i++) {
        if (mon->protection[i] > 0) {
            val = i;
        } else if (mon->protection[i] < 0) {
            val2 = i;
        }
    }

    if (val >= 0) {
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s can naturally resist some attacks.", gender_subjective_upper[gender]);
    }

    if (val2 >= 0) {
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s is naturally vulnerable to some attacks.", gender_subjective_upper[gender]);
    }

    for (val = -1, val2 = i = 0; i < NROFATTACKS; i++) {
        if (mon->protection[i] > val2) {
            val = i;
            val2 = mon->protection[i];
        }
    }

    if (val >= 0) {
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "Best armour protection seems to be for %s.", attack_name[val]);
    }

    if (QUERY_FLAG(mon, FLAG_UNDEAD)) {
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s is an undead force.", gender_subjective_upper[gender]);
    }

    switch ((mon->stats.hp + 1) * 4 / (mon->stats.maxhp + 1)) {
    case 1:
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s is in a bad shape.", gender_subjective_upper[gender]);
        break;

    case 2:
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s is hurt.", gender_subjective_upper[gender]);
        break;

    case 3:
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s is somewhat hurt.", gender_subjective_upper[gender]);
        break;

    default:
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s is in excellent shape.", gender_subjective_upper[gender]);
        break;
    }

    if (present_in_ob(POISONING, mon) != NULL) {
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s looks very ill.", gender_subjective_upper[gender]);
    }
}

/**
 * Long description of an object.
 * @param tmp Object to get description of.
 * @param caller Caller.
 * @return The returned description. */
char *long_desc(object *tmp, object *caller)
{
    static char buf[VERY_BIG_BUF];
    char *cp;

    if (tmp == NULL) {
        return "";
    }

    buf[0] = '\0';

    switch (tmp->type) {
    case RING:
    case SKILL:
    case WEAPON:
    case ARMOUR:
    case BRACERS:
    case HELMET:
    case SHIELD:
    case BOOTS:
    case GLOVES:
    case AMULET:
    case GIRDLE:
    case POTION:
    case BOW:
    case ARROW:
    case CLOAK:
    case FOOD:
    case DRINK:
    case WAND:
    case ROD:
    case FLESH:
    case BOOK:
    case CONTAINER:

        if (*(cp = describe_item(tmp)) != '\0') {
            size_t len;

            strncat(buf, query_name(tmp, caller), VERY_BIG_BUF - 1);

            buf[VERY_BIG_BUF - 1] = '\0';
            len = strlen(buf);

            if (len < VERY_BIG_BUF - 5 && ((tmp->type != AMULET && tmp->type != RING) || tmp->title)) {
                /* Since we know the length, we save a few CPU cycles by
                 * using
                 * it instead of calling strcat */
                strcpy(buf + len, " ");
                len++;
                strncpy(buf + len, cp, VERY_BIG_BUF - len - 1);
                buf[VERY_BIG_BUF - 1] = '\0';
            }
        }

        break;
    }

    if (buf[0] == '\0') {
        strncat(buf, query_name(tmp, caller), VERY_BIG_BUF - 1);
        buf[VERY_BIG_BUF - 1] = '\0';
    }

    return buf;
}

/**
 * Player examines some object.
 * @param op Player.
 * @param tmp Object to examine. */
void examine(object *op, object *tmp, StringBuffer *sb_capture)
{
    int i;

    if (tmp == NULL) {
        return;
    }

    draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "That is %s%s", long_desc(tmp, op), !QUERY_FLAG(tmp, FLAG_IDENTIFIED) && need_identify(tmp) ? " (unidentified)" : "");

    if (tmp->custom_name) {
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "You name it %s.", tmp->custom_name);
    }

    if (QUERY_FLAG(tmp, FLAG_MONSTER) || tmp->type == PLAYER) {
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s.", describe_item(tmp->head ? tmp->head : tmp));
        examine_living(op, tmp, sb_capture);
    } else if (QUERY_FLAG(tmp, FLAG_IDENTIFIED)) {
        /* We don't double use the item_xxx arch commands, so they are always valid */

        if (QUERY_FLAG(tmp, FLAG_IS_GOOD)) {
            draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "It is good aligned.");
        } else if (QUERY_FLAG(tmp, FLAG_IS_EVIL)) {
            draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "It is evil aligned.");
        } else if (QUERY_FLAG(tmp, FLAG_IS_NEUTRAL)) {
            draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "It is neutral aligned.");
        }

        if (tmp->item_level) {
            if (tmp->item_skill) {
                draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "It needs a level of %d in %s to use.", tmp->item_level, skills[tmp->item_skill - 1].name);
            } else {
                draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "It needs a level of %d to use.", tmp->item_level);
            }
        }

        if (tmp->item_quality) {
            if (QUERY_FLAG(tmp, FLAG_INDESTRUCTIBLE)) {
                draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "Qua: %d Con: Indestructible.", tmp->item_quality);
            } else {
                draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "Qua: %d Con: %d.", tmp->item_quality, tmp->item_condition);
            }
        }
    }

    switch (tmp->type) {
    case BOOK:
    {
        if (tmp->msg) {
            draw_info_full(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "Something is written in it.");

            if (op->type == PLAYER && !QUERY_FLAG(tmp, FLAG_NO_SKILL_IDENT)) {
                int level = CONTR(op)->skill_ptr[SK_LITERACY]->level;

                /* Gray. */
                if (tmp->level < level_color[level].green) {
                    draw_info_full(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "It seems to contain no knowledge you could learn from.");
                } else if (tmp->level < level_color[level].blue) {
                    /* Green. */
                    draw_info_full(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "It seems to contain tiny bits of knowledge you could learn from.");
                } else if (tmp->level < level_color[level].yellow) {
                    /* Blue. */
                    draw_info_full(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "It seems to contain a small amount of knowledge you could learn from.");
                } else if (tmp->level < level_color[level].orange) {
                    /* Yellow. */
                    draw_info_full(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "It seems to contain an average amount of knowledge you could learn from.");
                } else if (tmp->level < level_color[level].red) {
                    /* Orange. */
                    draw_info_full(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "It seems to contain a moderate amount of knowledge you could learn from.");
                } else if (tmp->level < level_color[level].purple) {
                    /* Red. */
                    draw_info_full(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "It seems to contain a fair amount of knowledge you could learn from.");
                } else {
                    /* Purple. */
                    draw_info_full(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "It seems to contain a great amount of knowledge you could learn from.");
                }
            }
        }

        break;
    }

    case CONTAINER:
    {
        if (QUERY_FLAG(tmp, FLAG_IDENTIFIED)) {
            if (tmp->race != NULL) {
                if (tmp->weight_limit) {
                    draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "It can hold only %s and its weight limit is %.1f kg.", tmp->race, (float) tmp->weight_limit / 1000.0f);
                } else {
                    draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "It can hold only %s.", tmp->race);
                }

                /* Has magic modifier? */
                if (tmp->weapon_speed != 1.0f) {
                    /* Bad */
                    if (tmp->weapon_speed > 1.0f) {
                        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "It increases the weight of items inside by %.1f%%.", tmp->weapon_speed * 100.0f);
                    } else {
                        /* Good */
                        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "It decreases the weight of items inside by %.1f%%.", 100.0f - (tmp->weapon_speed * 100.0f));
                    }
                }
            } else {
                if (tmp->weight_limit) {
                    draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "Its weight limit is %.1f kg.", (float) tmp->weight_limit / 1000.0f);
                }
            }

            if (tmp->weapon_speed == 1.0f) {
                draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "It contains %3.3f kg.", (float) tmp->carrying / 1000.0f);
            } else if (tmp->weapon_speed > 1.0f) {
                draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "It contains %3.3f kg, increased to %3.3f kg.", (float) tmp->damage_round_tag / 1000.0f, (float) tmp->carrying / 1000.0f);
            } else {
                draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "It contains %3.3f kg, decreased to %3.3f kg.", (float) tmp->damage_round_tag / 1000.0f, (float) tmp->carrying / 1000.0f);
            }
        }

        break;
    }

    case WAND:
    {
        if (QUERY_FLAG(tmp, FLAG_IDENTIFIED)) {
            draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "It has %d charges left.", tmp->stats.food);
        }

        break;
    }

    case POWER_CRYSTAL:
    {
        /* Avoid division by zero... */
        if (tmp->stats.maxsp == 0) {
            draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "It has capacity of %d.", tmp->stats.maxsp);
        } else {
            const char *charge;

            i = (tmp->stats.sp * 10) / tmp->stats.maxsp;

            if (tmp->stats.sp == 0) {
                charge = "empty";
            } else if (i == 0) {
                charge = "almost empty";
            } else if (i < 3) {
                charge = "partially filled";
            } else if (i < 6) {
                charge = "half full";
            } else if (i < 9) {
                charge = "well charged";
            } else if (tmp->stats.sp == tmp->stats.maxsp) {
                charge = "fully charged";
            } else {
                charge = "almost full";
            }

            /* Higher capacity crystals */
            if (tmp->stats.maxsp > 1000) {
                i = (tmp->stats.maxsp % 1000) / 100;

                if (i != 0) {
                    draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "It has capacity of %d.%dk and is %s.", tmp->stats.maxsp / 1000, i, charge);
                } else {
                    draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "It has capacity of %dk and is %s.", tmp->stats.maxsp / 1000, charge);
                }
            } else {
                draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "It has capacity of %d and is %s.", tmp->stats.maxsp, charge);
            }
        }

        break;
    }
    }

    if (tmp->material && (need_identify(tmp) && QUERY_FLAG(tmp, FLAG_IDENTIFIED))) {
        char buf[MAX_BUF];

        snprintf(buf, sizeof(buf), "%s made of: ", tmp->nrof > 1 ? "They are" : "It is");

        for (i = 0; i < NROFMATERIALS; i++) {
            if (tmp->material & (1 << i)) {
                snprintfcat(buf, sizeof(buf), "%s ", materials[i].name);
            }
        }

        draw_info_full(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, buf);
    }

    if (tmp->weight) {
        float weight = (float) (tmp->nrof ? tmp->weight * (int) tmp->nrof : tmp->weight) / 1000.0f;

        if (tmp->type == MONSTER) {
            draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s weighs %3.3f kg.", gender_subjective_upper[object_get_gender(tmp)], weight);
        } else if (tmp->type == PLAYER) {
            draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s weighs %3.3f kg and is carrying %3.3f kg.", gender_subjective_upper[object_get_gender(tmp)], weight, (float) tmp->carrying / 1000.0f);
        } else {
            draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, tmp->nrof > 1 ? "They weigh %3.3f kg." : "It weighs %3.3f kg.", weight);
        }
    }

    if (QUERY_FLAG(tmp, FLAG_STARTEQUIP)) {
        /* Unpaid clone shop item */
        if (QUERY_FLAG(tmp, FLAG_UNPAID)) {
            draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s would cost you %s.", tmp->nrof > 1 ? "They" : "It", query_cost_string(tmp, op, COST_BUY));
        } else {
            /* God-given item */
            draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s god-given item%s.", tmp->nrof > 1 ? "They are" : "It is a", tmp->nrof > 1 ? "s" : "");

            if (QUERY_FLAG(tmp, FLAG_IDENTIFIED)) {
                if (tmp->value) {
                    draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "But %s worth %s.", tmp->nrof > 1 ? "they are" : "it is", query_cost_string(tmp, op, COST_TRUE));
                } else {
                    draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s worthless.", tmp->nrof > 1 ? "They are" : "It is");
                }
            }
        }
    } else if (tmp->value && !IS_LIVE(tmp)) {
        if (QUERY_FLAG(tmp, FLAG_IDENTIFIED)) {
            if (QUERY_FLAG(tmp, FLAG_UNPAID)) {
                draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s would cost you %s.", tmp->nrof > 1 ? "They" : "It", query_cost_string(tmp, op, COST_BUY));
            } else {
                draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s worth %s.", tmp->nrof > 1 ? "They are" : "It is", query_cost_string(tmp, op, COST_TRUE));
            }
        }

        if (!QUERY_FLAG(tmp, FLAG_UNPAID) && tmp->type != MONEY) {
            object *floor_ob;

            floor_ob = GET_MAP_OB_LAYER(op->map, op->x, op->y, LAYER_FLOOR, 0);

            if (floor_ob && floor_ob->type == SHOP_FLOOR) {
                draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "This shop will pay you %s.", query_cost_string(tmp, op, COST_SELL));
            }
        }
    } else if (!IS_LIVE(tmp)) {
        if (QUERY_FLAG(tmp, FLAG_IDENTIFIED)) {
            if (QUERY_FLAG(tmp, FLAG_UNPAID)) {
                draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s would cost nothing.", tmp->nrof > 1 ? "They" : "It");
            } else {
                draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s worthless.", tmp->nrof > 1 ? "They are" : "It is");
            }
        }
    }

    /* Does the object have a message?  Don't show message for all object
     * types - especially if the first entry is a match */
    if (tmp->msg && tmp->type != EXIT && tmp->type != BOOK && tmp->type != CORPSE && !QUERY_FLAG(tmp, FLAG_WALK_ON) && strncasecmp(tmp->msg, "@match", 7)) {
        /* This is just a hack so when identifying the items, we print
         * out the extra message */
        if ((need_identify(tmp) || QUERY_FLAG(tmp, FLAG_QUEST_ITEM)) && QUERY_FLAG(tmp, FLAG_IDENTIFIED)) {
            draw_info_full(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "The object has a story:");
            draw_info_full(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, tmp->msg);
        }
    }

    trigger_map_event(MEVENT_EXAMINE, op->map, op, tmp, NULL, NULL, 0);

    /* Blank line */
    draw_info_full(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, " ");
}

/**
 * Check if an item op can be put into a sack. If pl exists then tell
 * a player the reason of failure.
 * @param pl Player object.
 * @param sack The sack.
 * @param op The object to check.
 * @param nrof Number of objects we want to put in.
 * @return 1 if the object will fit, 0 if it will not. */
int sack_can_hold(object *pl, object *sack, object *op, int nrof)
{
    char buf[MAX_BUF];

    buf[0] = '\0';

    if (!QUERY_FLAG(sack, FLAG_APPLIED)) {
        snprintf(buf, sizeof(buf), "The %s is not active.", query_name(sack, NULL));
    }

    if (sack == op) {
        snprintf(buf, sizeof(buf), "You can't put the %s into itself.", query_name(sack, NULL));
    }

    if ((sack->race && (sack->sub_type & 1) != ST1_CONTAINER_CORPSE) && (sack->race != op->race || op->type == CONTAINER || (sack->stats.food && sack->stats.food != op->type))) {
        snprintf(buf, sizeof(buf), "You can put only %s into the %s.", sack->race, query_name(sack, NULL));
    }

    if (sack->weight_limit && sack->carrying + (sint32) ((float) (((nrof ? nrof : 1) * op->weight) + op->carrying) * sack->weapon_speed) > (sint32) sack->weight_limit) {
        snprintf(buf, sizeof(buf), "That won't fit in the %s!", query_name(sack, NULL));
    }

    if (buf[0]) {
        if (pl) {
            draw_info(COLOR_WHITE, pl, buf);
        }

        return 0;
    }

    return 1;
}

static object *get_pickup_object(object *pl, object *op, int nrof)
{
    if (QUERY_FLAG(op, FLAG_UNPAID) && QUERY_FLAG(op, FLAG_NO_PICK)) {
        op = object_create_clone(op);
        CLEAR_FLAG(op, FLAG_NO_PICK);
        SET_FLAG(op, FLAG_STARTEQUIP);
        op->nrof = nrof;

        draw_info_format(COLOR_WHITE, pl, "You pick up %s for %s from the storage.", query_name(op, NULL), query_cost_string(op, pl, COST_BUY));
    } else {
        op = object_stack_get_removed(op, nrof);

        if (QUERY_FLAG(op, FLAG_UNPAID)) {
            draw_info_format(COLOR_WHITE, pl, "%s will cost you %s.", query_name(op, NULL), query_cost_string(op, pl, COST_BUY));
        } else {
            draw_info_format(COLOR_WHITE, pl, "You pick up the %s.", query_name(op, NULL));
        }
    }

    op->sub_layer = 0;

    return op;
}

/**
 * Pick up object.
 * @param pl Object that is picking up the object.
 * @param op Object to put tmp into.
 * @param tmp Object to pick up.
 * @param nrof Number to pick up (0 means all of them).
 * @param no_mevent If 1, no map-wide pickup event will be triggered. */
static void pick_up_object(object *pl, object *op, object *tmp, int nrof, int no_mevent)
{
    int tmp_nrof = tmp->nrof ? tmp->nrof : 1;

    /* IF the player is flying & trying to take the item out of a container
     * that is in his inventory, let him.  tmp->env points to the container
     * (sack, luggage, etc), tmp->env->env then points to the player (nested
     * containers not allowed as of now) */
    if (QUERY_FLAG(pl, FLAG_FLYING) && is_player_inv(tmp) != pl) {
        draw_info(COLOR_WHITE, pl, "You are levitating, you can't reach the ground!");
        return;
    }

    if (nrof > tmp_nrof || nrof == 0) {
        nrof = tmp_nrof;
    }

    if (!player_can_carry(pl, WEIGHT_NROF(tmp, nrof))) {
        draw_info(COLOR_WHITE, pl, "That item is too heavy for you to pick up.");
        return;
    }

    if (tmp->type == ARROW) {
        object_projectile_stop(tmp, OBJECT_PROJECTILE_PICKUP);
    }

    /* Trigger the PICKUP event */
    if (trigger_event(EVENT_PICKUP, pl, tmp, op, NULL, nrof, 0, 0, SCRIPT_FIX_ACTIVATOR)) {
        return;
    }

    /* Trigger the map-wide pick up event. */
    if (!no_mevent && pl->map && pl->map->events && trigger_map_event(MEVENT_PICK, pl->map, pl, tmp, op, NULL, nrof)) {
        return;
    }

    if (pl->type == PLAYER) {
        CONTR(pl)->stat_items_picked++;
    }

    tmp = get_pickup_object(pl, tmp, nrof);
    insert_ob_in_ob(tmp, op);
}

/**
 * Try to pick up an item.
 * @param op Object trying to pick up.
 * @param alt Optional object op is trying to pick. If NULL, try to pick
 * first item under op.
 * @param no_mevent If 1, no map-wide pickup event will be triggered. */
void pick_up(object *op, object *alt, int no_mevent)
{
    int count;
    object *tmp = NULL;

    /* Decide which object to pick. */
    if (alt) {
        if (!can_pick(op, alt)) {
            draw_info_format(COLOR_WHITE, op, "You can't pick up %s.", alt->name);
            return;
        }

        tmp = alt;
    } else {
        if (op->below == NULL || !can_pick(op, op->below)) {
            draw_info(COLOR_WHITE, op, "There is nothing to pick up here.");
            return;
        }

        tmp = op->below;
    }

    if (!can_pick(op, tmp)) {
        return;
    }

    if (op->type == PLAYER) {
        count = CONTR(op)->count;

        if (count == 0) {
            count = tmp->nrof;
        }
    } else {
        count = tmp->nrof;
    }

    /* Container is open, so use it */
    if (op->type == PLAYER && CONTR(op)->container != NULL &&
            CONTR(op)->container != tmp) {
        alt = CONTR(op)->container;

        if (alt != tmp->env && !sack_can_hold(op, alt, tmp, count) && !check_magical_container(tmp, alt)) {
            return;
        }
    } else {
        /* Con container pickup */

        for (alt = op->inv; alt; alt = alt->below) {
            if (alt->type == CONTAINER && QUERY_FLAG(alt, FLAG_APPLIED) && alt->race && alt->race == tmp->race && sack_can_hold(NULL, alt, tmp, count) && !check_magical_container(tmp, alt)) {
                /* Perfect match */
                break;
            }
        }

        if (!alt) {
            for (alt = op->inv; alt; alt = alt->below) {
                if (alt->type == CONTAINER && QUERY_FLAG(alt, FLAG_APPLIED) && sack_can_hold(NULL, alt, tmp, count) && !check_magical_container(tmp, alt)) {
                    /* General container comes next */
                    break;
                }
            }
        }

        /* No free containers */
        if (!alt) {
            alt = op;
        }
    }

    if (tmp->env == alt) {
        alt = op;
    }

    /* Startequip items are not allowed to be put into containers. */
    if (op->type == PLAYER && alt->type == CONTAINER && QUERY_FLAG(tmp, FLAG_STARTEQUIP)) {
        draw_info(COLOR_WHITE, op, "This object cannot be put into containers!");
        return;
    }

    pick_up_object(op, alt, tmp, count, no_mevent);

    if (op->type == PLAYER) {
        CONTR(op)->count = 0;
    }
}

/**
 * Player tries to put object into sack, if nrof is non zero, then
 * nrof objects is tried to put into sack.
 * @param op Player object.
 * @param sack The sack.
 * @param tmp The object to put into sack.
 * @param nrof Number of items to put into sack (0 for all). */
void put_object_in_sack(object *op, object *sack, object *tmp, long nrof)
{
    char buf[MAX_BUF];
    int tmp_nrof = tmp->nrof ? tmp->nrof : 1;

    if (op->type != PLAYER) {
        return;
    }

    /* Can't put an object in itself */
    if (sack == tmp) {
        return;
    }

    if (sack->type != CONTAINER) {
        draw_info_format(COLOR_WHITE, op, "The %s is not a container.", query_name(sack, NULL));
        return;
    }

    if (check_magical_container(tmp, sack)) {
        draw_info(COLOR_WHITE, op, "You can't put a magical container into another magical container.");
        return;
    }

    if (tmp->map && sack->env) {
        if (trigger_event(EVENT_PICKUP, op, tmp, sack, NULL, nrof, 0, 0, SCRIPT_FIX_ACTIVATOR)) {
            return;
        }
    }

    /* Trigger the map-wide put event. */
    if (op->map && op->map->events && trigger_map_event(MEVENT_PUT, op->map, op, tmp, sack, NULL, nrof)) {
        return;
    }

    if (nrof > tmp_nrof || nrof == 0) {
        nrof = tmp_nrof;
    }

    if (!sack_can_hold(op, sack, tmp, nrof)) {
        return;
    }

    if (QUERY_FLAG(tmp, FLAG_APPLIED)) {
        if (object_apply_item(tmp, op, AP_UNAPPLY | AP_NO_MERGE) != OBJECT_METHOD_OK) {
            return;
        }
    }

    tmp = get_pickup_object(op, tmp, nrof);

    snprintf(buf, sizeof(buf), "You put the %s in %s.", query_name(tmp, NULL), query_name(sack, NULL));
    insert_ob_in_ob(tmp, sack);
    draw_info(COLOR_WHITE, op, buf);
    /* This is overkill, fix_player() is called somewhere in object.c */
    fix_player(op);
}

/**
 * Drop an object onto the floor.
 * @param op Player object.
 * @param tmp The object to drop.
 * @param nrof Number of items to drop (0 for all).
 * @param no_mevent If 1, no map-wide event will be triggered. */
void drop_object(object *op, object *tmp, long nrof, int no_mevent)
{
    object *floor_ob;

    if (QUERY_FLAG(tmp, FLAG_NO_DROP)) {
        draw_info(COLOR_WHITE, op, "You can't drop that item.");
        return;
    }

    /* Trigger the map-wide drop event. */
    if (!no_mevent && op->map && op->map->events && trigger_map_event(MEVENT_DROP, op->map, op, tmp, NULL, NULL, nrof)) {
        return;
    }

    if (QUERY_FLAG(tmp, FLAG_APPLIED)) {
        /* Can't unapply it */
        if (object_apply_item(tmp, op, AP_UNAPPLY | AP_NO_MERGE) != OBJECT_METHOD_OK) {
            return;
        }
    }

    /* Trigger the DROP event */
    if (trigger_event(EVENT_DROP, op, tmp, NULL, NULL, nrof, 0, 0, SCRIPT_FIX_ACTIVATOR)) {
        return;
    }

    tmp = object_stack_get_removed(tmp, nrof);

    if (op->type == PLAYER) {
        CONTR(op)->stat_items_dropped++;
    }

    if (QUERY_FLAG(tmp, FLAG_STARTEQUIP) || QUERY_FLAG(tmp, FLAG_UNPAID)) {
        if (op->type == PLAYER) {
            draw_info_format(COLOR_WHITE, op, "You drop the %s.", query_name(tmp, NULL));

            if (QUERY_FLAG(tmp, FLAG_UNPAID)) {
                draw_info(COLOR_WHITE, op, "The shop magic put it back to the storage.");

                floor_ob = GET_MAP_OB_LAYER(op->map, op->x, op->y, LAYER_FLOOR, 0);

                /* If the player is standing on a unique shop floor or unique
                 * randomitems shop floor, drop the object back to the floor */
                if (floor_ob && floor_ob->type == SHOP_FLOOR && (QUERY_FLAG(floor_ob, FLAG_IS_MAGICAL) || (floor_ob->randomitems && QUERY_FLAG(floor_ob, FLAG_CURSED)))) {
                    tmp->x = op->x;
                    tmp->y = op->y;
                    insert_ob_in_map(tmp, op->map, op, 0);
                }
            } else {
                draw_info(COLOR_WHITE, op, "The god-given item vanishes to nowhere as you drop it!");
            }
        }

        fix_player(op);
        return;
    }

    /* If SAVE_INTERVAL is commented out, we never want to save
     * the player here. */
#ifdef SAVE_INTERVAL

    if (op->type == PLAYER && !QUERY_FLAG(tmp, FLAG_UNPAID) && (tmp->nrof ? tmp->value * tmp->nrof : tmp->value > 2000) && (CONTR(op)->last_save_time + SAVE_INTERVAL) <= time(NULL)) {
        player_save(op);
        CONTR(op)->last_save_time = time(NULL);
    }
#endif

    floor_ob = GET_MAP_OB_LAYER(op->map, op->x, op->y, LAYER_FLOOR, 0);

    if (floor_ob && floor_ob->type == SHOP_FLOOR && !QUERY_FLAG(tmp, FLAG_UNPAID) && tmp->type != MONEY) {
        sell_item(tmp, op, -1);

        /* Ok, we have really sold it - not only dropped. Run this only
         * if the floor is not magical (i.e., unique shop) */
        if (QUERY_FLAG(tmp, FLAG_UNPAID) && !QUERY_FLAG(floor_ob, FLAG_IS_MAGICAL)) {
            if (op->type == PLAYER) {
                draw_info(COLOR_WHITE, op, "The shop magic put it to the storage.");
            }

            fix_player(op);
            return;
        }
    }

    tmp->x = op->x;
    tmp->y = op->y;
    tmp->sub_layer = op->sub_layer;

    insert_ob_in_map(tmp, op->map, op, 0);

    SET_FLAG(op, FLAG_NO_APPLY);
    object_remove(op, 0);
    insert_ob_in_map(op, op->map, op, INS_NO_MERGE | INS_NO_WALK_ON);
    CLEAR_FLAG(op, FLAG_NO_APPLY);

    /* Need to update the weight for the player */
    if (op->type == PLAYER) {
        fix_player(op);
    }
}

/**
 * Drop an item, either on the floor or in a container.
 * @param op Who is dropping an item.
 * @param tmp What object to drop.
 * @param no_mevent If 1, no drop map-wide event will be triggered. */
void drop(object *op, object *tmp, int no_mevent)
{
    if (tmp == NULL) {
        draw_info(COLOR_WHITE, op, "You don't have anything to drop.");
        return;
    }

    if (QUERY_FLAG(tmp, FLAG_INV_LOCKED)) {
        draw_info(COLOR_WHITE, op, "This item is locked.");
        return;
    }

    if (QUERY_FLAG(tmp, FLAG_NO_DROP)) {
        draw_info(COLOR_WHITE, op, "You can't drop that item.");
        return;
    }

    if (op->type == PLAYER) {
        if (CONTR(op)->container) {
            put_object_in_sack(op, CONTR(op)->container, tmp, CONTR(op)->count);
        } else {
            drop_object(op, tmp, CONTR(op)->count, no_mevent);
        }

        CONTR(op)->count = 0;
    } else {
        drop_object(op, tmp, 0, no_mevent);
    }
}

char *player_make_path(const char *name, const char *ext)
{
    StringBuffer *sb;
    char *name_lower, *cp;
    size_t i;

    sb = stringbuffer_new();
    stringbuffer_append_printf(sb, "%s/players/", settings.datapath);
    name_lower = estrdup(name);
    string_tolower(name_lower);

    for (i = 0; i < settings.limits[ALLOWED_CHARS_CHARNAME][0] - 1; i++) {
        stringbuffer_append_string_len(sb, name_lower, i + 1);
        stringbuffer_append_string(sb, "/");
    }

    stringbuffer_append_printf(sb, "%s/%s", name_lower, ext);

    efree(name_lower);
    cp = stringbuffer_finish(sb);

    return cp;
}

int player_exists(const char *name)
{
    char *path;
    int ret;

    path = player_make_path(name, "player.dat");
    ret = path_exists(path);
    efree(path);

    return ret;
}

void player_save(object *op)
{
    player *pl;
    char *path, pathtmp[HUGE_BUF];
    FILE *fp;
    int i;

    /* Is this a map players can't save on? */
    if (op->map && MAP_PLAYER_NO_SAVE(op->map)) {
        return;
    }

    pl = CONTR(op);
    path = player_make_path(op->name, "player.dat");

    path_ensure_directories(path);
    snprintf(pathtmp, sizeof(pathtmp), "%s.tmp", path);
    rename(path, pathtmp);

    fp = fopen(path, "w");

    if (!fp) {
        draw_info(COLOR_WHITE, op, "Can't open file for saving.");
        logger_print(LOG(BUG), "Can't open file for saving: %s.", path);
        rename(pathtmp, path);
        efree(path);
        return;
    }

    fprintf(fp, "no_chat %d\n", pl->no_chat);
    fprintf(fp, "tcl %d\n", pl->tcl);
    fprintf(fp, "tgm %d\n", pl->tgm);
    fprintf(fp, "tsi %d\n", pl->tsi);
    fprintf(fp, "tli %d\n", pl->tli);
    fprintf(fp, "tls %d\n", pl->tls);
    fprintf(fp, "gen_hp %d\n", pl->gen_hp);
    fprintf(fp, "gen_sp %d\n", pl->gen_sp);
    fprintf(fp, "digestion %d\n", pl->digestion);
    fprintf(fp, "map %s\n", op->map ? op->map->path : EMERGENCY_MAPPATH);
    fprintf(fp, "bed_map %s\n", pl->savebed_map);
    fprintf(fp, "bed_x %d\nbed_y %d\n", pl->bed_x, pl->bed_y);

    for (i = 0; i < pl->num_cmd_permissions; i++) {
        if (pl->cmd_permissions[i]) {
            fprintf(fp, "cmd_permission %s\n", pl->cmd_permissions[i]);
        }
    }

    for (i = 0; i < pl->num_faction_ids; i++) {
        if (pl->faction_ids[i]) {
            fprintf(fp, "faction %s %"FMT64 "\n", pl->faction_ids[i], pl->faction_reputation[i]);
        }
    }

    for (i = 0; i < pl->num_region_maps; i++) {
        if (pl->region_maps[i]) {
            fprintf(fp, "rmap %s\n", pl->region_maps[i]);
        }
    }

    fprintf(fp, "fame %"FMT64 "\n", pl->fame);
    fprintf(fp, "endplst\n");

    SET_FLAG(op, FLAG_NO_FIX_PLAYER);
    save_object(fp, op);
    CLEAR_FLAG(op, FLAG_NO_FIX_PLAYER);

    /* Make sure the write succeeded */
    if (fclose(fp) == EOF) {
        draw_info(COLOR_WHITE, op, "Can't save character.");
        rename(pathtmp, path);
        efree(path);
        return;
    }

    chmod(path, SAVE_MODE);
    unlink(pathtmp);
}

static int player_load(player *pl, const char *path)
{
    FILE *fp;
    char buf[MAX_BUF], *end;
    void *loaderbuf;

    if (path_size(path) == 0) {
        return 0;
    }

    fp = fopen(path, "rb");

    if (!fp) {
        return 0;
    }

    while (fgets(buf, sizeof(buf), fp)) {
        end = strchr(buf, '\n');

        if (end) {
            *end = '\0';
        }

        if (strcmp(buf, "endplst") == 0) {
            break;
        } else if (strncmp(buf, "no_chat ", 8) == 0) {
            pl->no_chat = atoi(buf + 8);
        } else if (strncmp(buf, "tcl ", 4) == 0) {
            pl->tcl = atoi(buf + 4);
        } else if (strncmp(buf, "tgm ", 4) == 0) {
            pl->tgm = atoi(buf + 4);
        } else if (strncmp(buf, "tsi ", 4) == 0) {
            pl->tsi = atoi(buf + 4);
        } else if (strncmp(buf, "tli ", 4) == 0) {
            pl->tli = atoi(buf + 4);
        } else if (strncmp(buf, "tls ", 4) == 0) {
            pl->tls = atoi(buf + 4);
        } else if (strncmp(buf, "map ", 4) == 0) {
            strncpy(pl->maplevel, buf + 4, sizeof(pl->maplevel) - 1);
            pl->maplevel[sizeof(pl->maplevel) - 1] = '\0';
        } else if (strncmp(buf, "bed_map ", 8) == 0) {
            strncpy(pl->savebed_map, buf + 8, sizeof(pl->savebed_map) - 1);
            pl->savebed_map[sizeof(pl->savebed_map) - 1] = '\0';
        } else if (strncmp(buf, "bed_x ", 5) == 0) {
            pl->bed_x = atoi(buf + 5);
        } else if (strncmp(buf, "bed_y ", 5) == 0) {
            pl->bed_y = atoi(buf + 5);
        } else if (strncmp(buf, "cmd_permission ", 15) == 0) {
            pl->cmd_permissions = erealloc(pl->cmd_permissions, sizeof(char *) * (pl->num_cmd_permissions + 1));
            pl->cmd_permissions[pl->num_cmd_permissions] = estrdup(buf + 15);
            pl->num_cmd_permissions++;
        } else if (strncmp(buf, "faction ", 8) == 0) {
            size_t pos;
            char faction_id[MAX_BUF];
            sint64 rep;

            pos = 8;

            if (string_get_word(buf, &pos, ' ', faction_id, sizeof(faction_id), 0)) {
                rep = atoll(buf + pos);

                pl->faction_ids = erealloc(pl->faction_ids, sizeof(*pl->faction_ids) * (pl->num_faction_ids + 1));
                pl->faction_reputation = erealloc(pl->faction_reputation, sizeof(*pl->faction_reputation) * (pl->num_faction_ids + 1));
                pl->faction_ids[pl->num_faction_ids] = add_string(faction_id);
                pl->faction_reputation[pl->num_faction_ids] = rep;
                pl->num_faction_ids++;
            }
        } else if (strncmp(buf, "fame ", 5) == 0) {
            pl->fame = atoi(buf + 5);
        } else if (strncmp(buf, "rmap ", 5) == 0) {
            pl->region_maps = erealloc(pl->region_maps, sizeof(*pl->region_maps) * (pl->num_region_maps + 1));
            pl->region_maps[pl->num_region_maps] = estrdup(buf + 5);
            pl->num_region_maps++;
        }
    }

    SET_FLAG(pl->ob, FLAG_NO_FIX_PLAYER);
    loaderbuf = create_loader_buffer(fp);
    load_object(fp, pl->ob, loaderbuf, LO_REPEAT, 0);
    delete_loader_buffer(loaderbuf);
    CLEAR_FLAG(pl->ob, FLAG_NO_FIX_PLAYER);
    fclose(fp);

    /* The inventory of players is loaded in reverse order, so we need to
     * reorder it. */
    object_reverse_inventory(pl->ob);

    return 1;
}

static void player_create(player *pl, const char *path, archetype *at, const char *name)
{
    copy_object(&at->clone, pl->ob, 0);
    pl->ob->custom_attrset = pl;
    FREE_AND_COPY_HASH(pl->ob->name, name);

    SET_FLAG(pl->ob, FLAG_NO_FIX_PLAYER);
    give_initial_items(pl->ob, pl->ob->randomitems);
    trigger_global_event(GEVENT_BORN, pl->ob, NULL);
    CLEAR_FLAG(pl->ob, FLAG_NO_FIX_PLAYER);

    strncpy(pl->maplevel, first_map_path, sizeof(pl->maplevel) - 1);
    pl->maplevel[sizeof(pl->maplevel) - 1] = '\0';
    pl->ob->x = -1;
    pl->ob->y = -1;
}

object *player_get_dummy(void)
{
    player *pl;

    pl = get_player(NULL);
    pl->ob = get_archetype("human_male");
    pl->ob->custom_attrset = pl;

    return pl->ob;
}

void player_login(socket_struct *ns, const char *name, archetype *at)
{
    player *pl;
    char *path;
    mapstruct *m;

    /* Not in the login procedure, can't login. */
    if (ns->state != ST_LOGIN) {
        return;
    }

    pl = find_player(name);

    if (pl) {
        pl->socket.state = ST_DEAD;
        remove_ns_dead_player(pl);
    }

    if (checkbanned(name, ns->host)) {
        logger_print(LOG(SYSTEM), "Ban: Banned player tried to login. [%s@%s]", name, ns->host);
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns, "Connection refused due to a ban.");
        ns->state = ST_ZOMBIE;
        return;
    }

    logger_print(LOG(INFO), "Login %s from IP %s", name, ns->host);

    pl = get_player(NULL);
    memcpy(&pl->socket, ns, sizeof(socket_struct));

    /* Basically, the add_player copies the socket structure into
     * the player structure, so this one (which is from init_sockets)
     * is not needed anymore. */
    ns->login_count = 0;
    ns->keepalive = 0;
    socket_info.nconns--;
    ns->state = ST_AVAILABLE;

    /* Create a new object for the player object data. */
    pl->ob = get_object();

#ifdef SAVE_INTERVAL
    pl->last_save_time = time(NULL);
#endif

#ifdef AUTOSAVE
    pl->last_save_tick = pticks;
#endif

    path = player_make_path(name, "player.dat");

    if (!player_load(pl, path)) {
        player_create(pl, path, at, name);
    }

    pl->ob->custom_attrset = pl;
    pl->ob->speed_left = 0.5;

    sum_weight(pl->ob);
    fix_player(pl->ob);
    link_player_skills(pl->ob);

    pl->socket.state = ST_PLAYING;

    display_motd(pl->ob);
    draw_info_format(COLOR_DK_ORANGE, NULL, "%s has entered the game.", pl->ob->name);
    trigger_global_event(GEVENT_LOGIN, pl, pl->socket.host);

    m = ready_map_name(pl->maplevel, 0);

    if (!m && strncmp(pl->maplevel, "/random/", 8) == 0) {
        object_enter_map(pl->ob, NULL, ready_map_name(pl->savebed_map, 0), pl->bed_x, pl->bed_y, 1);
    } else {
        object_enter_map(pl->ob, NULL, m, pl->ob->x, pl->ob->y, 1);
    }

    /* No savebed map yet, initialize it. */
    if (*pl->savebed_map == '\0') {
        strncpy(pl->savebed_map, pl->maplevel, sizeof(pl->savebed_map) - 1);
        pl->savebed_map[sizeof(pl->savebed_map) - 1] = '\0';

        pl->bed_x = pl->ob->x;
        pl->bed_y = pl->ob->y;
    }

    pl->socket.update_tile = 0;
    pl->socket.look_position = 0;
    pl->socket.ext_title_flag = 1;

    /* No direction; default to southeast. */
    if (!pl->ob->direction) {
        pl->ob->direction = SOUTHEAST;
    }

    SET_ANIMATION(pl->ob, (NUM_ANIMATIONS(pl->ob) / NUM_FACINGS(pl->ob)) * pl->ob->direction);

    esrv_new_player(pl, pl->ob->weight + pl->ob->carrying);
    esrv_send_inventory(pl->ob, pl->ob);
    send_quickslots(pl);

    if (pl->ob->map && pl->ob->map->events) {
        trigger_map_event(MEVENT_LOGIN, pl->ob->map, pl->ob, NULL, NULL, NULL, 0);
    }

    efree(path);
}

/** @copydoc object_methods::remove_map_func */
static void remove_map_func(object *op)
{
    player *pl;

    if (op->map->in_memory == MAP_SAVING) {
        return;
    }

    pl = CONTR(op);

    /* Remove player from the map's linked list of players. */
    if (pl->map_below) {
        CONTR(pl->map_below)->map_above = pl->map_above;
    } else {
        op->map->player_first = pl->map_above;
    }

    if (pl->map_above) {
        CONTR(pl->map_above)->map_below = pl->map_below;
    }

    pl->map_below = pl->map_above = NULL;
    pl->update_los = 1;

    /* If the player has a container open that is not in their inventory,
     * close it. */
    if (pl->container && pl->container->env != op) {
        container_close(op, NULL);
    }
}

/**
 * Initialize the player type object methods. */
void object_type_init_player(void)
{
    object_type_methods[PLAYER].remove_map_func = remove_map_func;
}
