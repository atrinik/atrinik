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
 * Player related functions.
 */

#include <global.h>
#include <loader.h>
#include <toolkit_string.h>
#include <plugin.h>
#include <monster_data.h>
#include <arch.h>
#include <ban.h>
#include <player.h>
#include <object.h>
#include <exp.h>
#include <object_methods.h>
#include <disease.h>
#include <container.h>
#include <server.h>
#include <path.h>

static int save_life(object *op);
static void remove_unpaid_objects(object *op, object *env);

/**
 * Player memory pool.
 */
mempool_struct *pool_player;

/**
 * Initialize the player API.
 */
void
player_init (void)
{
    pool_player = mempool_create("players", 25, sizeof(player),
            MEMPOOL_ALLOW_FREEING, NULL, NULL, NULL, NULL);
}

/**
 * Deinitialize the player API.
 */
void
player_deinit (void)
{
    while (first_player) {
        free_player(first_player);
    }
}

/**
 * Disconnect all currently connected players.
 */
void
player_disconnect_all (void)
{
    while (first_player) {
        first_player->cs->state = ST_DEAD;
        player_logout(first_player);
    }
}

/**
 * Loop through the player list and find player specified by plname.
 *
 * @param plname
 * The player name to find.
 * @return
 * Player structure if found, NULL otherwise.
 */
player *
find_player (const char *plname)
{
    for (player *pl = first_player; pl != NULL; pl = pl->next) {
        if (strcasecmp(pl->ob->name, plname) == 0) {
            return pl;
        }
    }

    return NULL;
}

/**
 * Loop through the player list and find player specified by plname.
 *
 * @param plname
 * The player name to find. Must be a shared string.
 * @return
 * Player structure if found, NULL otherwise.
 */
player *
find_player_sh (shstr *plname)
{
    for (player *pl = first_player; pl != NULL; pl = pl->next) {
        if (pl->ob->name == plname) {
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
 * @param op
 * Player object to print the message to.
 */
void
display_motd (object *op)
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
 * @param p
 * Player structure to recycle or NULL for new structure.
 * @return
 * The player structure.
 */
static player *
get_player (player *p)
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
 * @param pl
 * The player structure to free.
 */
void
free_player (player *pl)
{
    /* If this player is in a party, leave the party */
    if (pl->party) {
        command_party(pl->ob, "party", "leave");
    }

    pl->cs->state = ST_DEAD;

    /* Free command permissions. */
    if (pl->cmd_permissions != NULL) {
        int i;

        for (i = 0; i < pl->num_cmd_permissions; i++) {
            if (pl->cmd_permissions[i] != NULL) {
                efree(pl->cmd_permissions[i]);
            }
        }

        efree(pl->cmd_permissions);
    }

    if (pl->followed_player != NULL) {
        FREE_AND_CLEAR_HASH(pl->followed_player);
    }

    if (pl->killer != NULL) {
        efree(pl->killer);
    }

    player_faction_t *faction, *tmp;

    HASH_ITER(hh, pl->factions, faction, tmp) {
        player_faction_free(pl, faction);
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

    if (pl->ob) {
        SET_FLAG(pl->ob, FLAG_NO_FIX_PLAYER);

        if (!QUERY_FLAG(pl->ob, FLAG_REMOVED)) {
            object_remove(pl->ob, 0);
        }

        object_destroy(pl->ob);
    }

    free_newsocket(pl->cs);
}

/**
 * Give initial items to object pl. This is used when player creates a
 * new character.
 * @param pl
 * The player object.
 * @param items
 * Treasure list of items to give.
 */
void
give_initial_items (object *pl, treasurelist *items)
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
            if ((!QUERY_FLAG(pl, FLAG_USE_ARMOUR) && IS_ARMOR(op)) ||
                    (!QUERY_FLAG(pl, FLAG_USE_WEAPON) && op->type == WEAPON)) {
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
 * @param pl
 * Player to handle.
 * @retval -1 Player is invalid.
 * @retval 0 No more actions to do.
 * @retval 1 There are more actions we can do.
 */
int
handle_newcs_player (player *pl)
{
    if (!pl->ob || !OBJECT_ACTIVE(pl->ob)) {
        return -1;
    }

    socket_server_handle_client(pl);

    if (!pl->ob || !OBJECT_ACTIVE(pl->ob) || pl->cs->state == ST_DEAD) {
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
 * @param op
 * Player to try to save.
 * @retval 1 Player had his life saved by an item, first item saving life
 * is removed.
 * @retval 0 Player had no life-saving item.
 */
static int
save_life (object *op)
{
    object *tmp;

    if (!QUERY_FLAG(op, FLAG_LIFESAVE)) {
        return 0;
    }

    for (tmp = op->inv; tmp != NULL; tmp = tmp->below) {
        if (QUERY_FLAG(tmp, FLAG_APPLIED) && QUERY_FLAG(tmp, FLAG_LIFESAVE)) {
            play_sound_map(op->map, CMD_SOUND_EFFECT, "explosion.ogg", op->x, op->y, 0, 0);
            char *name = object_get_name_s(tmp, op);
            draw_info_format(COLOR_WHITE, op, "Your %s vibrates violently, "
                    "then evaporates.", name);
            efree(name);

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
            object_enter_map(op,
                             NULL,
                             ready_map_name(CONTR(op)->savebed_map, NULL, 0),
                             CONTR(op)->bed_x,
                             CONTR(op)->bed_y,
                             true);
            return 1;
        }
    }

    LOG(BUG, "LIFESAVE set without applied object.");
    CLEAR_FLAG(op, FLAG_LIFESAVE);
    /* Bring him home. */
    object_enter_map(op,
                     NULL,
                     ready_map_name(CONTR(op)->savebed_map, NULL, 0),
                     CONTR(op)->bed_x,
                     CONTR(op)->bed_y,
                     true);
    return 0;
}

/**
 * This goes through the inventory and removes unpaid objects, and puts
 * them back in the map (location and map determined by values of env).
 * This function will descend into containers.
 * @param op
 * Object to start the search from.
 * @param env
 * Map location determined by this object.
 */
static void
remove_unpaid_objects (object *op, object *env)
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
            object_insert_map(op, env->map, NULL, 0);
        } else if (op->inv) {
            remove_unpaid_objects(op->inv, env);
        }

        op = next;
    }
}

/**
 * Figures out how much hp/mana points to regenerate.
 * @param regen
 * Regeneration value used for client (for example,
 * player::gen_client_hp).
 * @param regen_remainder
 * Pointer to regen remainder (for example,
 * player::gen_hp_remainder).
 * @return
 * How much to regenerate.
 */
static inline int
get_regen_amount (uint16_t regen, uint16_t *regen_remainder)
{
    int ret = 0;
    double division;

    /* Check whether it's time to update the remainder variable (which
     * will distribute the remainder evenly over time). */
    if (pticks % 8 == 0) {
        *regen_remainder += regen;
    }

    /* First check if we can distribute it evenly, if not, try to remove
     * leftovers, if any. */
    for (division = MAX_TICKS; ; division = 1.0) {
        if (*regen_remainder / 10.0 / division >= 1.0) {
            int add = (int) *regen_remainder / 10.0 / division;

            ret += add;
            *regen_remainder -= add * 10;
            break;
        }

        if (DBL_EQUAL(division, 1.0)) {
            break;
        }
    }

    return ret;
}

/**
 * Calculate HP/SP regeneration value.
 *
 * @param speed
 * Regeneration speed.
 * @param rate
 * Regeneration rate.
 * @return
 * Calculated regeneration value.
 */
static inline uint16_t
get_regen_value (double speed, double rate)
{
    double value = MAX_TICKS;
    value /= rate / (MAX(speed, 20.0) + 10.0);
    value *= 10.0;
    return value;
}

/**
 * Regenerate player's hp/mana, decrease food, etc.
 *
 * We will only regenerate HP and mana if the player has some food in their
 * stomach.
 *
 * @param op
 * Player.
 */
static void
player_do_some_living (object *op)
{
    HARD_ASSERT(op != NULL);
    SOFT_ASSERT(op->type == PLAYER, "Not a player object: %s",
                object_get_str(op));

    player *pl = CONTR(op);

    double gen_hp = (pl->gen_hp * (PLAYER_REGEN_HP_RATE / 20.0)) +
                    (op->stats.maxhp / 4.0);

    double gen_sp = (pl->gen_sp * (PLAYER_REGEN_SP_RATE / 20.0)) +
                    op->stats.maxsp;
    gen_sp = gen_sp * 10 / MAX(pl->gen_sp_armour, 10);

    /* Update client's regen rates. */
    pl->gen_client_hp = get_regen_value(gen_hp, PLAYER_REGEN_HP_RATE);
    pl->gen_client_sp = get_regen_value(gen_sp, PLAYER_REGEN_SP_RATE);

    if (pl->skill_ptr[SK_MEDITATION] != NULL) {
        double modifier = (pticks - pl->last_combat) / MAX_TICKS;
        modifier /= PLAYER_REGEN_MODIFIER;
        modifier += 1.0;
        modifier = MIN(PLAYER_REGEN_MODIFIER_MAX, modifier);
        gen_hp *= modifier;
        gen_sp *= modifier;
    }

    uint16_t gen_real_hp = get_regen_value(gen_hp, PLAYER_REGEN_HP_RATE);
    uint16_t gen_real_sp = get_regen_value(gen_sp, PLAYER_REGEN_SP_RATE);

    int16_t last_food = op->stats.food;

    /* Regenerate hit points. */
    if (op->stats.hp < op->stats.maxhp && op->stats.food) {
        int add = get_regen_amount(gen_real_hp, &pl->gen_hp_remainder);
        if (add != 0) {
            op->stats.hp += add;
            pl->stat_hp_regen += add;

            if (op->stats.hp > op->stats.maxhp) {
                op->stats.hp = op->stats.maxhp;
            }

            if (!pl->tgm) {
                op->stats.food--;

                if (pl->digestion < 0) {
                    op->stats.food += pl->digestion;
                } else if (pl->digestion > 0 && rndm(0, pl->digestion)) {
                    op->stats.food = last_food;
                }
            }
        }
    } else {
        pl->gen_hp_remainder = 0;
    }

    /* Regenerate mana. */
    if (op->stats.sp < op->stats.maxsp && op->stats.food) {
        int add = get_regen_amount(gen_real_sp, &pl->gen_sp_remainder);
        if (add != 0) {
            op->stats.sp += add;
            pl->stat_sp_regen += add;

            if (op->stats.sp > op->stats.maxsp) {
                op->stats.sp = op->stats.maxsp;
            }

            if (!pl->tgm) {
                op->stats.food--;

                if (pl->digestion < 0) {
                    op->stats.food += pl->digestion;
                } else if (pl->digestion > 0 && rndm(0, pl->digestion)) {
                    op->stats.food = last_food;
                }
            }
        }
    } else {
        pl->gen_sp_remainder = 0;
    }

    /* Digestion */
    if (--op->last_eat < 0) {
        int bonus = MAX(pl->digestion, 0);
        int penalty = MAX(-pl->digestion, 0);

        if (pl->gen_hp > 0) {
            op->last_eat = 25 * (1 + bonus) / (pl->gen_hp + penalty + 1);
        } else {
            op->last_eat = 25 * (1 + bonus) / (penalty + 1);
        }

        if (!pl->tgm) {
            op->stats.food--;
        }
    }

    if (op->stats.food < 0 && op->stats.hp >= 0) {
        object *flesh = NULL;

        FOR_INV_PREPARE(op, tmp) {
            if (QUERY_FLAG(tmp, FLAG_UNPAID)) {
                continue;
            }

            if (tmp->type == FOOD || tmp->type == DRINK) {
                draw_info(COLOR_WHITE, op,
                          "You blindly grab for a bite of food.");
                manual_apply(op, tmp, 0);

                if (op->stats.food >= 0 || op->stats.hp < 0) {
                    break;
                }
            } else if (tmp->type == FLESH && flesh == NULL) {
                flesh = tmp;
            }
        } FOR_INV_FINISH();

        /* If player is still starving, it means they don't have any food, so
         * eat flesh instead. */
        if (op->stats.food < 0 && op->stats.hp >= 0 && flesh != NULL) {
            draw_info(COLOR_WHITE, op, "You blindly grab for a bite of food.");
            manual_apply(op, flesh, 0);
        }
    }

    /* Lose hitpoints for lack of food. */
    while (op->stats.food < 0 && op->stats.hp > 0) {
        op->stats.food++;
        op->stats.hp--;
    }

    if ((op->stats.hp <= 0 || op->stats.food < 0) && !pl->tgm) {
        draw_info_format(COLOR_WHITE, NULL, "%s starved to death.", op->name);
        player_set_killer(pl, "starvation");
        kill_player(op);
    }
}

/**
 * Deplete player's stats due to dying.
 *
 * @param op
 * Player.
 */
static void
player_death_deplete_stats (object *op)
{
    HARD_ASSERT(op != NULL);

    int num_lose = 1 + op->level / BALSL_NUMBER_LOSSES_RATIO;
    bool lost_stat = false;

    /* Protect low level players for a little while. */
    if (op->level <= 3) {
        num_lose = 0;
    }

    object *depletion = NULL;
    for (int i = 0; i < num_lose; i++) {
        static archetype_t *at = NULL;
        if (at == NULL) {
            at = arch_find("depletion");
            SOFT_ASSERT(at != NULL, "Could not find depletion archetype");
        }

        if (depletion == NULL) {
            depletion = object_find_arch(op, at);
            if (depletion == NULL) {
                depletion = arch_to_object(at);
                depletion = object_insert_into(depletion, op, 0);
                SOFT_ASSERT(depletion != NULL, "Could not insert depletion");
            }
        }

        int stat = rndm(0, NUM_STATS - 1);
        bool lose = true;

        int8_t value = get_attr_value(&depletion->stats, stat);
        if (value < 0) {
            int loss_chance = 1 + op->level / BALSL_LOSS_CHANCE_RATIO;
            int keep_chance = value * value;

            /* There is a maximum depletion total per level. */
            if (value < -1 - op->level / BALSL_MAX_LOSS_RATIO) {
                lose = false;
            } else if (rndm(0, loss_chance + keep_chance - 1) < keep_chance) {
                lose = false;
            }
        }

        if (!lose || value < -50) {
            continue;
        }

        change_attr_value(&depletion->stats, stat, -1);
        SET_FLAG(depletion, FLAG_APPLIED);
        lost_stat = true;
        draw_info(COLOR_GRAY, op, lose_msg[stat]);
    }

    if (lost_stat) {
        living_update_player(op);
    } else {
        draw_info(COLOR_WHITE, op,
                  "For a brief moment you feel a holy presence "
                  "protecting you.");
    }
}

/**
 * If the player should die (lack of hp, food, etc), we call this.
 *
 * Will remove diseases, apply death penalties, and so on.
 * @param op
 * The player in jeopardy.
 */
void
kill_player (object *op)
{
    char buf[HUGE_BUF];
    object *tmp;

    if (pvp_area(NULL, op)) {
        draw_info(COLOR_NAVY, op, "You have been defeated in combat!");
        draw_info(COLOR_NAVY, op, "Local medics have saved your life...");

        /* Restore player */
        cast_heal(op, op, MAXLEVEL, op, SP_CURE_POISON);
        /* Remove any disease */
        disease_cure(op, NULL);
        op->stats.hp = op->stats.maxhp;
        op->stats.sp = op->stats.maxsp;

        if (op->stats.food <= 0) {
            op->stats.food = 999;
        }

        /* Create a bodypart-trophy to make the winner happy */
        tmp = arch_to_object(arch_find("finger"));
        if (tmp != NULL) {
            snprintf(buf, sizeof(buf), "%s's finger", op->name);
            FREE_AND_COPY_HASH(tmp->name, buf);

            const char *killer = player_get_killer(CONTR(op));
            if (killer == NULL) {
                killer = "something nasty";
            }

            StringBuffer *sb = stringbuffer_new();
            stringbuffer_append_printf(sb,
                                       "This finger has been cut off %s the "
                                       "%s, when %s was defeated at level %d "
                                       "by %s.",
                                       op->name,
                                       op->race,
                                       gender_subjective[object_get_gender(op)],
                                       op->level,
                                       killer);
            FREE_AND_COPY_HASH(tmp->msg, stringbuffer_finish_shared(sb));
            tmp->value = 0;
            tmp->material = 0;
            tmp->type = 0;
            tmp->x = op->x, tmp->y = op->y;
            object_insert_map(tmp, op->map, op, 0);
        }

        player_clear_killer(CONTR(op));

        /* Teleport defeated player to new destination */
        transfer_ob(op, MAP_ENTER_X(op->map), MAP_ENTER_Y(op->map), 0, NULL, NULL);
        return;
    }

    if (save_life(op)) {
        return;
    }

    /* Trigger the DEATH event */
    if (trigger_event(EVENT_DEATH, NULL, op, NULL, NULL, 0, 0, 0, 0) != 0) {
        return;
    }

    CONTR(op)->stat_deaths++;

    /* Trigger the global GDEATH event */
    trigger_global_event(GEVENT_PLAYER_DEATH, NULL, op);

    play_sound_player_only(CONTR(op), CMD_SOUND_EFFECT, "playerdead.ogg", 0, 0, 0, 0);

    /* Put a gravestone up where the character 'almost' died. */
    tmp = arch_to_object(arch_find("gravestone"));
    snprintf(VS(buf), "%s's gravestone", op->name);
    FREE_AND_COPY_HASH(tmp->name, buf);
    StringBuffer *sb = stringbuffer_new();
    const char *killer = player_get_killer(CONTR(op));
    if (killer == NULL) {
        killer = "something nasty";
    }
    stringbuffer_append_printf(sb,
                               "R.I.P.\n\nHerein rests the hero %s the %s "
                               "who was killed at level %d by %s.",
                               op->name,
                               op->race,
                               op->level,
                               killer);
    time_t now = time(NULL);
    strftime(VS(buf), "\n\n%b %d %Y", localtime(&now));
    stringbuffer_append_string(sb, buf);
    tmp->msg = stringbuffer_finish_shared(sb);
    tmp->x = op->x;
    tmp->y = op->y;
    object_insert_map(tmp, op->map, NULL, 0);

    player_death_deplete_stats(op);

    /* Remove any poisoning the character may be suffering. */
    cast_heal(op, op, MAXLEVEL, op, SP_CURE_POISON);
    /* Remove any disease */
    disease_cure(op, NULL);

    /* If the player starved to death, give them some food back... */
    if (op->stats.food <= 0) {
        op->stats.food = 500;
    }

    /* Reset HP/SP */
    op->stats.hp = op->stats.maxhp;
    op->stats.sp = op->stats.maxsp;

    hiscore_check(op, 1);

    /* Clear the killer, otherwise the highscore can get entries like
     * 'xxx was killed by pudding on map Wilderness' even if they were
     * killed in a dungeon. */
    player_clear_killer(CONTR(op));

    /* Check to see if the player is in a shop. If so, then check to see
     * if the player has any unpaid items. If so, remove them and put
     * them back in the map. */
    tmp = GET_MAP_OB(op->map, op->x, op->y);

    if (tmp && tmp->type == SHOP_FLOOR) {
        remove_unpaid_objects(op->inv, op);
    }

    /* Move player to his current respawn position (last savebed). */
    object_enter_map(op,
                     NULL,
                     ready_map_name(CONTR(op)->savebed_map, NULL, 0),
                     CONTR(op)->bed_x,
                     CONTR(op)->bed_y,
                     true);

    /* Show a nasty message */
    draw_info(COLOR_WHITE, op, "YOU HAVE DIED.");
    player_save(op);
}

/**
 * Handles object throwing objects of type "DUST".
 * @todo This function needs to be rewritten. Works for area effect
 * spells only now.
 * @param op
 * Object throwing.
 * @param throw_ob
 * What to throw.
 * @param dir
 * Direction to throw into.
 */
void
cast_dust (object *op, object *throw_ob, int dir)
{
    archetype_t *arch = NULL;

    if (!(spells[throw_ob->stats.sp].flags & SPELL_DESC_DIRECTION)) {
        LOG(ERROR, "Warning, dust is not AoE spell: %s",
                object_get_str(throw_ob));
        return;
    }

    if (spells[throw_ob->stats.sp].archname) {
        arch = arch_find(spells[throw_ob->stats.sp].archname);
    }

    /* Casting POTION 'dusts' is really use_magic_item skill */
    if (op->type == PLAYER && throw_ob->type == POTION && !change_skill(op, SK_MAGIC_DEVICES)) {
        return;
    }

    if (throw_ob->type == POTION && arch != NULL) {
        cast_cone(op, throw_ob, dir, 10, throw_ob->stats.sp, arch);
    } else if ((arch = arch_find("dust_effect")) != NULL) {
        /* dust_effect */
        cast_cone(op, throw_ob, dir, 1, 0, arch);
    } else {
        /* Problem occurred! */
        LOG(BUG, "can't find an archetype to use!");
    }

    if (op->type == PLAYER && arch) {
        char *name = object_get_name_s(throw_ob, op);
        draw_info_format(COLOR_WHITE, op, "You cast %s.", name);
        efree(name);
    }

    if (op->chosen_skill) {
        op->chosen_skill->stats.maxsp = throw_ob->last_grace;
    }

    if (!QUERY_FLAG(throw_ob, FLAG_REMOVED)) {
        object_destruct(throw_ob);
    }
}

/**
 * Test for PVP area.
 *
 * If only one object is given, it tests for that. Otherwise if two
 * objects are given, both objects must be in PVP area.
 *
 * Considers parties.
 * @param attacker
 * First object.
 * @param victim
 * Second object.
 * @return
 * 1 if PVP is possible, 0 otherwise.
 */
int
pvp_area (object *attacker, object *victim)
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
 * @param op
 * The object to look for the skill in.
 * @param skillnr
 * Skill ID.
 * @return
 * The skill if found, NULL otherwise.
 */
object *
find_skill (object *op, int skillnr)
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
 * @param pl
 * Player.
 * @param weight
 * Weight to check.
 * @return
 * 1 if the player can carry that weight, 0 otherwise.
 */
int
player_can_carry (object *pl, uint32_t weight)
{
    uint32_t effective_weight_limit;

    if (pl->stats.Str <= MAX_STAT) {
        effective_weight_limit = weight_limit[pl->stats.Str];
    } else {
        effective_weight_limit = weight_limit[MAX_STAT];
    }

    return (pl->carrying + weight) < effective_weight_limit;
}

/**
 * Add a new path to player's paths queue.
 * @param pl
 * Player to add the path for.
 * @param map
 * Map we want to reach.
 * @param x
 * X we want to reach.
 * @param y
 * Y we want to reach.
 */
void
player_path_add (player *pl, mapstruct *map, int16_t x, int16_t y)
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
 * @param pl
 * Player to clear paths for.
 */
void
player_path_clear (player *pl)
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
 * @param pl
 * Player.
 */
void
player_path_handle (player *pl)
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
                        int m = 1 - rndm_chance(2) ? 2 : 0;

                        dir = move_object(pl->ob, absdir(dir + diff * m));

                        if (dir == 0) {
                            dir = move_object(pl->ob, absdir(dir - diff * m));
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
 * Creates a new ::player_faction_t structure and adds it to the specified
 * player.
 * @param pl
 * Player.
 * @param name
 * Name of the faction to create a structure for.
 * @return
 * New ::player_faction_t structure.
 */
player_faction_t *
player_faction_create (player *pl, shstr *name)
{
    HARD_ASSERT(pl != NULL);
    HARD_ASSERT(name != NULL);

    player_faction_t *faction = ecalloc(1, sizeof(*faction));
    faction->name = add_string(name);
    HASH_ADD(hh, pl->factions, name, sizeof(shstr *), faction);

    return faction;
}

/**
 * Frees the specified ::player_faction_t structure, removing it from the
 * player's hash table of factions.
 * @param pl
 * Player.
 * @param faction
 * ::player_faction_t to free.
 */
void
player_faction_free (player *pl, player_faction_t *faction)
{
    HARD_ASSERT(pl != NULL);
    HARD_ASSERT(faction != NULL);

    HASH_DEL(pl->factions, faction);
    free_string_shared(faction->name);
    efree(faction);
}

/**
 * Find the specified faction name in the player's factions hash table.
 * @param pl
 * Player.
 * @param name
 * Name of the faction to find.
 * @return
 * ::player_faction_t if found, NULL otherwise.
 */
player_faction_t *
player_faction_find (player *pl, shstr *name)
{
    HARD_ASSERT(pl != NULL);
    HARD_ASSERT(name != NULL);

    player_faction_t *faction;
    HASH_FIND(hh, pl->factions, &name, sizeof(shstr *), faction);
    return faction;
}

/**
 * Update the player's reputation with a particular faction.
 * @param pl
 * Player.
 * @param name
 * Name of the faction to update.
 * @param reputation
 * Reputation to add/subtract.
 */
void
player_faction_update (player *pl, shstr *name, double reputation)
{
    HARD_ASSERT(pl != NULL);
    HARD_ASSERT(name != NULL);

    player_faction_t *faction = player_faction_find(pl, name);

    if (faction == NULL) {
        faction = player_faction_create(pl, name);
    }

    faction->reputation += reputation;
}

/**
 * Get player's reputation with a particular faction.
 * @param pl
 * Player.
 * @param name
 * Name of the faction.
 * @return
 * Player's reputation with the specified faction.
 */
double
player_faction_reputation (player *pl, shstr *name)
{
    HARD_ASSERT(pl != NULL);
    HARD_ASSERT(name != NULL);

    player_faction_t *faction = player_faction_find(pl, name);

    if (faction == NULL) {
        return 0.0;
    }

    return faction->reputation;
}

/**
 * Sanitize player's text input, removing extraneous whitespace,
 * unprintable characters, etc.
 * @param str
 * Input to sanitize.
 * @return
 * Sanitized input; can be NULL if there's nothing in the string
 * left.
 */
char *
player_sanitize_input (char *str)
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
 * @param str
 * The player name to clean up.
 */
void
player_cleanup_name (char *str)
{
    string_whitespace_trim(str);
    string_capitalize(str);
}

/**
 * Recursive helper function for find_marked_object() to search for
 * marked object in containers.
 * @param op
 * Object. Should be a player.
 * @param marked
 * Marked object.
 * @param marked_count
 * Marked count.
 * @return
 * The object if found, NULL otherwise.
 */
static object *
find_marked_object_rec (object *op, object **marked, uint32_t *marked_count)
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
 * @param op
 * Object. Should be a player.
 * @return
 * Marked object if still valid, NULL otherwise.
 */
object *
find_marked_object (object *op)
{
    if (op->type != PLAYER || !op || !CONTR(op) || !CONTR(op)->mark) {
        return NULL;
    }

    return find_marked_object_rec(op, &CONTR(op)->mark, &CONTR(op)->mark_count);
}

/**
 * Player examines a living object.
 * @param op
 * Player.
 * @param tmp
 * Object being examined.
 */
static void
examine_living (object *op, object *tmp, StringBuffer *sb_capture)
{
    tmp = HEAD(tmp);

    int gender = object_get_gender(tmp);

    if (QUERY_FLAG(tmp, FLAG_IS_GOOD)) {
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op,
                "%s is a good aligned %s %s.", gender_subjective_upper[gender],
                gender_noun[gender], tmp->race);
    } else if (QUERY_FLAG(tmp, FLAG_IS_EVIL)) {
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op,
                "%s is an evil aligned %s %s.", gender_subjective_upper[gender],
                gender_noun[gender], tmp->race);
    } else if (QUERY_FLAG(tmp, FLAG_IS_NEUTRAL)) {
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op,
                "%s is a neutral aligned %s %s.",
                gender_subjective_upper[gender], gender_noun[gender],
                tmp->race);
    } else {
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op,
                "%s is a %s %s.", gender_subjective_upper[gender],
                gender_noun[gender], tmp->race);
    }

    draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op,
            "%s is level %d.", gender_subjective_upper[gender], tmp->level);
    draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op,
            "%s has a base damage of %d and hp of %d.",
            gender_subjective_upper[gender], tmp->stats.dam, tmp->stats.maxhp);
    draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op,
            "%s has a wc of %d and ac of %d.", gender_subjective_upper[gender],
            tmp->stats.wc, tmp->stats.ac);

    bool has_protection = true, has_weakness = false;
    for (int i = 0; i < NROFATTACKS; i++) {
        if (tmp->protection[i] > 0) {
            has_protection = true;
        } else if (tmp->protection[i] < 0) {
            has_weakness = true;
        }
    }

    if (has_protection) {
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op,
                "%s can naturally resist some attacks.",
                gender_subjective_upper[gender]);
    }

    if (has_weakness) {
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op,
                "%s is naturally vulnerable to some attacks.",
                gender_subjective_upper[gender]);
    }

    int8_t highest_protection = 0;
    int best_protection = -1;
    for (int i = 0; i < NROFATTACKS; i++) {
        if (tmp->protection[i] > highest_protection) {
            best_protection = i;
            highest_protection = tmp->protection[i];
        }
    }

    if (best_protection != -1) {
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op,
                "Best armour protection seems to be for %s.",
                attack_name[best_protection]);
    }

    if (QUERY_FLAG(tmp, FLAG_UNDEAD)) {
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op,
                "%s is an undead force.", gender_subjective_upper[gender]);
    }

    switch ((tmp->stats.hp + 1) * 4 / (tmp->stats.maxhp + 1)) {
    case 1:
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op,
                "%s is in a bad shape.", gender_subjective_upper[gender]);
        break;

    case 2:
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op,
                "%s is hurt.", gender_subjective_upper[gender]);
        break;

    case 3:
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op,
                "%s is somewhat hurt.", gender_subjective_upper[gender]);
        break;

    default:
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op,
                "%s is in excellent shape.", gender_subjective_upper[gender]);
        break;
    }

    if (object_find_type(tmp, POISONING) != NULL) {
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op,
                "%s looks very ill.", gender_subjective_upper[gender]);
    }
}

/**
 * Player examines some object.
 * @param op
 * Player.
 * @param tmp
 * Object to examine.
 */
void
examine (object *op, object *tmp, StringBuffer *sb_capture)
{
    int i;

    if (tmp == NULL) {
        return;
    }

    tmp = HEAD(tmp);
    char *name = object_get_name_description_s(tmp, op);
    draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op,
            "That is %s%s", name, !QUERY_FLAG(tmp, FLAG_IDENTIFIED) &&
            need_identify(tmp) ? " (unidentified)" : "");
    efree(name);

    if (tmp->custom_name != NULL) {
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op,
                "You name it %s.", tmp->custom_name);
    }

    if (QUERY_FLAG(tmp, FLAG_MONSTER) || tmp->type == PLAYER) {
        char *desc = stringbuffer_finish(object_get_description(tmp, op, NULL));
        draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op,
                "%s.", desc);
        efree(desc);
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

    case BOOK_SPELL:
        if (tmp->stats.sp >= 0 && tmp->stats.sp < NROFREALSPELLS) {
            int level = spells[tmp->stats.sp].at->clone.level;
            draw_info_full_format(CHAT_TYPE_GAME,
                                  NULL,
                                  COLOR_WHITE,
                                  sb_capture,
                                  op,
                                  "It contains a level %d wizardry spell.",
                                  level);
            int learn_level = spells[tmp->stats.sp].at->clone.level - 15;
            learn_level = MAX(1, learn_level);
            draw_info_full_format(CHAT_TYPE_GAME,
                                  NULL,
                                  COLOR_WHITE,
                                  sb_capture,
                                  op,
                                  "It requires a minimum level of %d in "
                                  "literacy to learn.",
                                  learn_level);
        }

        break;

    case CONTAINER:
    {
        if (QUERY_FLAG(tmp, FLAG_IDENTIFIED)) {
            if (tmp->race != NULL) {
                if (tmp->weight_limit != 0) {
                    draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE,
                            sb_capture, op, "It can hold only %s and its "
                            "weight limit is %.1f kg.", tmp->race,
                            tmp->weight_limit / 1000.0);
                } else {
                    draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE,
                            sb_capture, op, "It can hold only %s.", tmp->race);
                }
            } else {
                if (tmp->weight_limit != 0) {
                    draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE,
                            sb_capture, op, "Its weight limit is %.1f kg.",
                            tmp->weight_limit / 1000.0);
                }
            }

            /* Has a magic modifier? */
            if (!DBL_EQUAL(tmp->weapon_speed, 1.0)) {
                if (tmp->weapon_speed > 1.0) {
                    /* Increases weight of items (bad) */
                    draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE,
                            sb_capture, op, "It increases the weight of items "
                            "inside by %.1f%%.", tmp->weapon_speed * 100.0);
                } else {
                    /* Decreases weight of items (good) */
                    draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE,
                            sb_capture, op, "It decreases the weight of items "
                            "inside by %.1f%%.", 100.0 -
                            (tmp->weapon_speed * 100.0));
                }
            }

            if (DBL_EQUAL(tmp->weapon_speed, 1.0)) {
                draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE,
                        sb_capture, op, "It contains %3.3f kg.",
                        tmp->carrying / 1000.0);
            } else if (tmp->weapon_speed > 1.0) {
                draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE,
                        sb_capture, op, "It contains %3.3f kg, increased to "
                        "%3.3f kg.", tmp->damage_round_tag / 1000.0,
                        tmp->carrying / 1000.0);
            } else {
                draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE,
                        sb_capture, op, "It contains %3.3f kg, decreased to "
                        "%3.3f kg.", tmp->damage_round_tag / 1000.0,
                        tmp->carrying / 1000.0);
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

    if (tmp->weight != 0) {
        double weight = MAX(1, tmp->nrof) * tmp->weight / 1000.0f;

        if (tmp->type == MONSTER) {
            draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s weighs %3.3f kg.", gender_subjective_upper[object_get_gender(tmp)], weight);
        } else if (tmp->type == PLAYER) {
            draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s weighs %3.3f kg and is carrying %3.3f kg.", gender_subjective_upper[object_get_gender(tmp)], weight, (float) tmp->carrying / 1000.0f);
        } else {
            draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, tmp->nrof > 1 ? "They weigh %3.3f kg." : "It weighs %3.3f kg.", weight);
        }
    }

    if (QUERY_FLAG(tmp, FLAG_SOULBOUND)) {
        if (QUERY_FLAG(tmp, FLAG_UNPAID)) {
            draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture,
                                  op, "%s would become soulbound to you.",
                                  tmp->nrof > 1 ? "They" : "It");
        } else {
            shstr *soulbound_name = object_get_value(tmp, "soulbound_name");
            if (soulbound_name == NULL) {
                draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE,
                                      sb_capture, op,
                                      "%s soulbound without an owner.",
                                      tmp->nrof > 1 ? "They are" : "It is");
            } else {
                draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE,
                                      sb_capture, op,
                                      "%s soulbound to %s.",
                                      tmp->nrof > 1 ? "They are" : "It is",
                                      soulbound_name);
            }
        }
    }

    if (QUERY_FLAG(tmp, FLAG_STARTEQUIP)) {
        /* Unpaid clone shop item */
        if (QUERY_FLAG(tmp, FLAG_UNPAID)) {
            draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s would cost you %s.", tmp->nrof > 1 ? "They" : "It", shop_get_cost_string_item(tmp, COST_BUY));
        } else {
            /* God-given item */
            draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s god-given item%s.", tmp->nrof > 1 ? "They are" : "It is a", tmp->nrof > 1 ? "s" : "");

            if (QUERY_FLAG(tmp, FLAG_IDENTIFIED)) {
                if (tmp->value) {
                    draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "But %s worth %s.", tmp->nrof > 1 ? "they are" : "it is", shop_get_cost_string_item(tmp, COST_TRUE));
                } else {
                    draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s worthless.", tmp->nrof > 1 ? "They are" : "It is");
                }
            }
        }
    } else if (tmp->value && !IS_LIVE(tmp)) {
        if (QUERY_FLAG(tmp, FLAG_IDENTIFIED)) {
            if (QUERY_FLAG(tmp, FLAG_UNPAID)) {
                draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s would cost you %s.", tmp->nrof > 1 ? "They" : "It", shop_get_cost_string_item(tmp, COST_BUY));
            } else {
                draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "%s worth %s.", tmp->nrof > 1 ? "They are" : "It is", shop_get_cost_string_item(tmp, COST_TRUE));
            }
        }

        if (!QUERY_FLAG(tmp, FLAG_UNPAID) && tmp->type != MONEY) {
            object *floor_ob;

            floor_ob = GET_MAP_OB_LAYER(op->map, op->x, op->y, LAYER_FLOOR, 0);

            if (floor_ob && floor_ob->type == SHOP_FLOOR) {
                draw_info_full_format(CHAT_TYPE_GAME, NULL, COLOR_WHITE, sb_capture, op, "This shop will pay you %s.", shop_get_cost_string_item(tmp, COST_SELL));
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
 * @param pl
 * Player object.
 * @param sack
 * The sack.
 * @param op
 * The object to check.
 * @param nrof
 * Number of objects we want to put in.
 * @return
 * 1 if the object will fit, 0 if it will not.
 */
int
sack_can_hold (object *pl, object *sack, object *op, int nrof)
{
    if (!QUERY_FLAG(sack, FLAG_APPLIED)) {
        if (pl != NULL) {
            char *name = object_get_name_s(sack, pl);
            draw_info_format(COLOR_WHITE, pl, "The %s is not active.", name);
            efree(name);
        }

        return 0;
    }

    if (sack == op) {
        if (pl != NULL) {
            char *name = object_get_name_s(sack, pl);
            draw_info_format(COLOR_WHITE, pl, "You can't put the %s into "
                    "itself.", name);
            efree(name);
        }

        return 0;
    }

    if ((sack->race && sack->sub_type != ST1_CONTAINER_CORPSE) &&
            (sack->race != op->race || op->type == CONTAINER ||
            (sack->stats.food && sack->stats.food != op->type))) {
        if (pl != NULL) {
            char *name = object_get_name_s(sack, pl);
            draw_info_format(COLOR_WHITE, pl, "You can put only %s into the "
                    "%s.", sack->race, name);
            efree(name);
        }

        return 0;
    }

    if (sack->weight_limit != 0 && sack->carrying + (((MAX(1, nrof) *
            op->weight) + op->carrying) * sack->weapon_speed) >
            sack->weight_limit) {
        if (pl != NULL) {
            char *name = object_get_name_s(sack, pl);
            draw_info_format(COLOR_WHITE, pl, "That won't fit in the %s!",
                    name);
            efree(name);
        }

        return 0;
    }

    return 1;
}

static object *
get_pickup_object (object *pl, object *op, int nrof)
{
    char *name = object_get_name_s(op, pl);

    if (QUERY_FLAG(op, FLAG_UNPAID) && QUERY_FLAG(op, FLAG_NO_PICK)) {
        op = object_clone(op);
        CLEAR_FLAG(op, FLAG_NO_PICK);
        SET_FLAG(op, FLAG_STARTEQUIP);
        op->nrof = nrof;

        draw_info_format(COLOR_WHITE, pl, "You pick up %s for %s from the "
                "storage.", name, shop_get_cost_string_item(op, COST_BUY));
    } else {
        op = object_stack_get_removed(op, nrof);

        if (QUERY_FLAG(op, FLAG_UNPAID)) {
            draw_info_format(COLOR_WHITE, pl, "%s will cost you %s.", name,
                    shop_get_cost_string_item(op, COST_BUY));
        } else {
            draw_info_format(COLOR_WHITE, pl, "You pick up the %s.", name);
        }
    }

    efree(name);
    op->sub_layer = 0;

    return op;
}

/**
 * Pick up object.
 * @param pl
 * Object that is picking up the object.
 * @param op
 * Object to put tmp into.
 * @param tmp
 * Object to pick up.
 * @param nrof
 * Number to pick up (0 means all of them).
 * @param no_mevent
 * If 1, no map-wide pickup event will be triggered.
 */
static void
pick_up_object (object *pl, object *op, object *tmp, int nrof, int no_mevent)
{
    int tmp_nrof = tmp->nrof ? tmp->nrof : 1;

    /* IF the player is flying and trying to take the item out of a container
     * that is in his inventory, let him. */
    if (QUERY_FLAG(pl, FLAG_FLYING) && !object_is_in_inventory(tmp, pl)) {
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
    if (trigger_event(EVENT_PICKUP, pl, tmp, op, NULL, nrof, 0, 0, 0)) {
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
    object_insert_into(tmp, op, 0);
}

/**
 * Try to pick up an item.
 * @param op
 * Object trying to pick up.
 * @param alt
 * Optional object op is trying to pick. If NULL, try to pick
 * first item under op.
 * @param no_mevent
 * If 1, no map-wide pickup event will be triggered.
 */
void
pick_up (object *op, object *alt, int no_mevent)
{
    int count;
    object *tmp = NULL;

    /* Decide which object to pick. */
    if (alt) {
        if (!object_can_pick(op, alt)) {
            draw_info_format(COLOR_WHITE, op, "You can't pick up %s.", alt->name);
            return;
        }

        tmp = alt;
    } else {
        if (op->below == NULL || !object_can_pick(op, op->below)) {
            draw_info(COLOR_WHITE, op, "There is nothing to pick up here.");
            return;
        }

        tmp = op->below;
    }

    if (!object_can_pick(op, tmp)) {
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
            CONTR(op)->container != tmp && CONTR(op)->container != tmp->env) {
        alt = CONTR(op)->container;

        if (alt != tmp->env && !sack_can_hold(op, alt, tmp, count) && !container_check_magical(tmp, alt)) {
            return;
        }
    } else {
        /* Con container pickup */

        alt = NULL;

        if (QUERY_FLAG(tmp, FLAG_IDENTIFIED)) {
            for (alt = op->inv; alt; alt = alt->below) {
                if (alt->type == CONTAINER && QUERY_FLAG(alt, FLAG_APPLIED) && alt->race && alt->race == tmp->race && sack_can_hold(NULL, alt, tmp, count) && !container_check_magical(tmp, alt)) {
                    /* Perfect match */
                    break;
                }
            }
        }

        if (!alt) {
            for (alt = op->inv; alt; alt = alt->below) {
                if (alt->type == CONTAINER && QUERY_FLAG(alt, FLAG_APPLIED) && sack_can_hold(NULL, alt, tmp, count) && !container_check_magical(tmp, alt)) {
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
 * @param op
 * Player object.
 * @param sack
 * The sack.
 * @param tmp
 * The object to put into sack.
 * @param nrof
 * Number of items to put into sack (0 for all).
 */
void
put_object_in_sack (object *op, object *sack, object *tmp, long nrof)
{
    int tmp_nrof = tmp->nrof ? tmp->nrof : 1;

    if (op->type != PLAYER) {
        return;
    }

    /* Can't put an object in itself */
    if (sack == tmp) {
        return;
    }

    if (sack->type != CONTAINER) {
        char *name = object_get_name_s(sack, op);
        draw_info_format(COLOR_WHITE, op, "The %s is not a container.", name);
        efree(name);
        return;
    }

    if (container_check_magical(tmp, sack)) {
        draw_info(COLOR_WHITE, op, "You can't put a magical container into another magical container.");
        return;
    }

    if (tmp->map && sack->env) {
        if (trigger_event(EVENT_PICKUP, op, tmp, sack, NULL, nrof, 0, 0, 0)) {
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
        if (object_apply_item(tmp, op, APPLY_ALWAYS_UNAPPLY | APPLY_NO_MERGE) != OBJECT_METHOD_OK) {
            return;
        }
    }

    tmp = get_pickup_object(op, tmp, nrof);

    char *name = object_get_name_s(sack, op);
    char *tmp_name = object_get_name_s(tmp, op);
    draw_info_format(COLOR_WHITE, op, "You put the %s in %s.", tmp_name, name);
    efree(name);
    efree(tmp_name);

    object_insert_into(tmp, sack, 0);
}

/**
 * Drop an object onto the floor.
 * @param op
 * Player object.
 * @param tmp
 * The object to drop.
 * @param nrof
 * Number of items to drop (0 for all).
 * @param no_mevent
 * If 1, no map-wide event will be triggered.
 */
void
drop_object (object *op, object *tmp, long nrof, int no_mevent)
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
        if (object_apply_item(tmp, op, APPLY_ALWAYS_UNAPPLY | APPLY_NO_MERGE) != OBJECT_METHOD_OK) {
            return;
        }
    }

    /* Trigger the DROP event */
    if (trigger_event(EVENT_DROP, op, tmp, NULL, NULL, nrof, 0, 0, 0)) {
        return;
    }

    tmp = object_stack_get_removed(tmp, nrof);

    if (op->type == PLAYER) {
        CONTR(op)->stat_items_dropped++;
    }

    if (QUERY_FLAG(tmp, FLAG_STARTEQUIP) || QUERY_FLAG(tmp, FLAG_UNPAID)) {
        if (op->type == PLAYER) {
            char *name = object_get_name_s(tmp, op);
            draw_info_format(COLOR_WHITE, op, "You drop the %s.", name);
            efree(name);

            if (QUERY_FLAG(tmp, FLAG_UNPAID)) {
                draw_info(COLOR_WHITE, op, "The shop magic put it back to the storage.");

                floor_ob = GET_MAP_OB_LAYER(op->map, op->x, op->y, LAYER_FLOOR, 0);

                /* If the player is standing on a unique shop floor or unique
                 * randomitems shop floor, drop the object back to the floor */
                if (floor_ob && floor_ob->type == SHOP_FLOOR && (QUERY_FLAG(floor_ob, FLAG_IS_MAGICAL) || (floor_ob->randomitems && QUERY_FLAG(floor_ob, FLAG_CURSED)))) {
                    tmp->x = op->x;
                    tmp->y = op->y;
                    object_insert_map(tmp, op->map, op, 0);
                    return;
                }
            } else {
                draw_info(COLOR_WHITE, op, "The god-given item vanishes to nowhere as you drop it!");
            }
        }

        object_destroy(tmp);
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
        shop_sell_item(op, tmp);

        /* Ok, we have really sold it - not only dropped. Run this only
         * if the floor is not magical (i.e., unique shop) */
        if (QUERY_FLAG(tmp, FLAG_UNPAID) && !QUERY_FLAG(floor_ob, FLAG_IS_MAGICAL)) {
            if (op->type == PLAYER) {
                draw_info(COLOR_WHITE, op, "The shop magic put it to the storage.");
            }

            object_destroy(tmp);
            return;
        }
    }

    tmp->x = op->x;
    tmp->y = op->y;
    tmp->sub_layer = op->sub_layer;

    object_insert_map(tmp, op->map, op, 0);

    SET_FLAG(op, FLAG_NO_APPLY);
    object_remove(op, 0);
    object_insert_map(op, op->map, op, INS_NO_MERGE | INS_NO_WALK_ON);
    CLEAR_FLAG(op, FLAG_NO_APPLY);
}

/**
 * Drop an item, either on the floor or in a container.
 * @param op
 * Who is dropping an item.
 * @param tmp
 * What object to drop.
 * @param no_mevent
 * If 1, no drop map-wide event will be triggered.
 */
void
drop (object *op, object *tmp, int no_mevent)
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

char *
player_make_path (const char *name, const char *ext)
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

int
player_exists (const char *name)
{
    char *path;
    int ret;

    path = player_make_path(name, "player.dat");
    ret = path_exists(path);
    efree(path);

    return ret;
}

/**
 * Saves the specified player.
 *
 * @param op
 * Player object to save.
 */
void
player_save (object *op)
{
    HARD_ASSERT(op != NULL);

    /* Is this a map players can't save on? */
    if (op->map != NULL && MAP_PLAYER_NO_SAVE(op->map)) {
        return;
    }

    char *path = player_make_path(op->name, "player.dat");
    char *path_tmp = player_make_path(op->name, "player.dat.tmp");

    player *pl = CONTR(op);

    path_ensure_directories(path_tmp);
    FILE *fp = fopen(path_tmp, "w");
    if (unlikely(fp == NULL)) {
        LOG(ERROR, "Failure opening %s for writing: %s",
            path_tmp, strerror(errno));
        goto error;
    }

    fprintf(fp, "no_chat %d\n", pl->no_chat);
    fprintf(fp, "tcl %d\n", pl->tcl);
    fprintf(fp, "tgm %d\n", pl->tgm);
    fprintf(fp, "tsi %d\n", pl->tsi);
    fprintf(fp, "tli %d\n", pl->tli);
    fprintf(fp, "tls %d\n", pl->tls);
    fprintf(fp, "map %s\n", op->map ? op->map->path : EMERGENCY_MAPPATH);
    fprintf(fp, "bed_map %s\n", pl->savebed_map);
    fprintf(fp, "bed_x %d\nbed_y %d\n", pl->bed_x, pl->bed_y);

    for (int i = 0; i < pl->num_cmd_permissions; i++) {
        if (unlikely(pl->cmd_permissions[i] == NULL)) {
            continue;
        }

        fprintf(fp, "cmd_permission %s\n", pl->cmd_permissions[i]);
    }

    player_faction_t *faction, *tmp;
    HASH_ITER(hh, pl->factions, faction, tmp) {
        fprintf(fp, "faction %s %e\n", faction->name, faction->reputation);
    }

    fprintf(fp, "fame %"PRId64 "\n", pl->fame);
    fprintf(fp, "endplst\n");

    SET_FLAG(op, FLAG_NO_FIX_PLAYER);
    object_save(op, fp);
    CLEAR_FLAG(op, FLAG_NO_FIX_PLAYER);

    /* Make sure the write succeeded. */
    if (unlikely(fclose(fp) == EOF)) {
        LOG(ERROR, "Failure closing file %s: %s",
            path_tmp, strerror(errno));
        goto error;
    }

    /* Set the correct permissions. */
    if (unlikely(chmod(path_tmp, SAVE_MODE) != 0)) {
        LOG(ERROR, "Failure setting permissions of %s: %s",
            path_tmp, strerror(errno));
        goto error;
    }

    /* Rename the file, removing the .tmp extension. */
    if (unlikely(path_rename(path_tmp, path) != 0)) {
        LOG(ERROR, "Failure renaming %s to %s: %s",
            path_tmp, path, strerror(errno));
        goto error;
    }

    goto out;

error:
    /* Handle errors. */
    draw_info(COLOR_RED, op, "Your character couldn't be saved.");

    /* Try to remove the temporary file if it was created. */
    if (fp != NULL && unlink(path_tmp) != 0) {
        LOG(ERROR, "Failure removing temporary file %s: %s",
            path_tmp, strerror(errno));
    }

out:
    efree(path);
    efree(path_tmp);
}

/**
 * Loads player data from the specified file into the player.
 *
 * @param pl
 * Player.
 * @param path
 * Path to load the data from.
 */
static void
player_load (player *pl, FILE *fp)
{
    HARD_ASSERT(pl != NULL);
    HARD_ASSERT(fp != NULL);

    char buf[HUGE_BUF];
    while (fgets(VS(buf), fp)) {
        char *cp = buf;
        string_strip_newline(cp);

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
            snprintf(VS(pl->maplevel), "%s", buf + 4);
        } else if (strncmp(buf, "bed_map ", 8) == 0) {
            snprintf(VS(pl->savebed_map), "%s", buf + 8);
        } else if (strncmp(buf, "bed_x ", 5) == 0) {
            pl->bed_x = atoi(buf + 5);
        } else if (strncmp(buf, "bed_y ", 5) == 0) {
            pl->bed_y = atoi(buf + 5);
        } else if (strncmp(buf, "cmd_permission ", 15) == 0) {
            pl->cmd_permissions =
                erealloc(pl->cmd_permissions,
                         sizeof(char *) * (pl->num_cmd_permissions + 1));
            pl->cmd_permissions[pl->num_cmd_permissions] = estrdup(buf + 15);
            pl->num_cmd_permissions++;
        } else if (strncmp(buf, "faction ", 8) == 0) {
            size_t pos = 8;
            char faction_name[MAX_BUF];
            if (string_get_word(buf, &pos, ' ', VS(faction_name), 0)) {
                player_faction_t *faction =
                    player_faction_create(pl, faction_name);
                faction->reputation = atof(buf + pos);
            }
        } else if (strncmp(buf, "fame ", 5) == 0) {
            pl->fame = atoll(buf + 5);
        }
    }

    SET_FLAG(pl->ob, FLAG_NO_FIX_PLAYER);
    void *buffer = create_loader_buffer(fp);
    load_object_buffer(buffer, pl->ob, 0);
    delete_loader_buffer(buffer);
    CLEAR_FLAG(pl->ob, FLAG_NO_FIX_PLAYER);

    /* The inventory of players is loaded in reverse order, so we need to
     * reorder it. */
    object_reverse_inventory(pl->ob);
}

/**
 * Create a new player character.
 *
 * @param pl
 * Player.
 * @param at
 * Character's archetype.
 * @param name
 * Name of the player character.
 */
static void
player_create (player *pl, archetype_t *at, const char *name)
{
    HARD_ASSERT(pl != NULL);
    HARD_ASSERT(at != NULL);
    HARD_ASSERT(name != NULL);

    object_copy(pl->ob, &at->clone, false);
    pl->ob->custom_attrset = pl;
    FREE_AND_COPY_HASH(pl->ob->name, name);

    SET_FLAG(pl->ob, FLAG_NO_FIX_PLAYER);
    give_initial_items(pl->ob, pl->ob->randomitems);
    trigger_global_event(GEVENT_BORN, pl->ob, NULL);
    CLEAR_FLAG(pl->ob, FLAG_NO_FIX_PLAYER);

    snprintf(VS(pl->maplevel), "%s", first_map_path);
    pl->ob->x = first_map_x;
    pl->ob->y = first_map_y;
}

/**
 * Creates a dummy player structure and returns a pointer to the player's
 * object.
 *
 * @param name
 * Name of the player to create.
 * @param host
 * IP address of the player.
 * @return
 * Created player object, never NULL. Will abort() in case of failure.
 */
object *
player_get_dummy (const char *name, const char *host)
{
    player *pl;

    pl = get_player(NULL);
    pl->cs = ecalloc(1, sizeof(*pl->cs));
    pl->cs->sc = socket_create(host != NULL ? host : "127.0.0.1",
                               13327,
                               false,
                               SOCKET_ROLE_SERVER,
                               false);
    if (pl->cs->sc == NULL) {
        abort();
    }

    init_connection(pl->cs);

    pl->ob = arch_get("human_male");
    pl->ob->custom_attrset = pl;

    if (name != NULL) {
        FREE_AND_COPY_HASH(pl->ob->name, name);
    }

    snprintf(VS(pl->savebed_map), "%s", EMERGENCY_MAPPATH);
    pl->bed_x = EMERGENCY_X;
    pl->bed_y = EMERGENCY_Y;

    SET_FLAG(pl->ob, FLAG_NO_FIX_PLAYER);
    give_initial_items(pl->ob, pl->ob->randomitems);
    CLEAR_FLAG(pl->ob, FLAG_NO_FIX_PLAYER);
    living_update_player(pl->ob);
    link_player_skills(pl->ob);

    pl->cs->state = ST_PLAYING;
    pl->cs->socket_version = SOCKET_VERSION;
    pl->cs->account = estrdup(ACCOUNT_TESTING_NAME);
    pl->cs->sound = 1;

    object_enter_map(pl->ob, NULL, NULL, 0, 0, false);

    return pl->ob;
}

object *
player_find_spell (object *op, spell_struct *spell)
{
    for (object *tmp = op->inv; tmp != NULL; tmp = tmp->below) {
        if (tmp->type == SPELL && tmp->name == spell->at->clone.name) {
            return tmp;
        }
    }

    return NULL;
}

/**
 * Updates who the player is talking to.
 * @param pl
 * Player.
 * @param npc
 * NPC the player is now talking to.
 */
void
player_set_talking_to (player *pl, object *npc)
{
    HARD_ASSERT(pl != NULL);
    HARD_ASSERT(npc != NULL);

    if (OBJECT_VALID(pl->talking_to, pl->talking_to_count) &&
            pl->talking_to != npc) {
        monster_data_dialogs_remove(pl->talking_to, pl->ob);
    }

    pl->talking_to = npc;
    pl->talking_to_count = npc->count;

    if (pl->target_object != npc || pl->target_object_count != npc->count) {
        pl->target_object = npc;
        pl->target_object_count = npc->count;
        send_target_command(pl);
    }
}

/**
 * Acquire the name of the player's killer, if any.
 *
 * @param pl
 * Player.
 * @return
 * Name of the killer, NULL if none.
 */
const char *
player_get_killer (player *pl)
{
    SOFT_ASSERT_RC(pl != NULL, NULL, "pl is NULL");

    if (string_isempty(pl->killer)) {
        return NULL;
    }

    return pl->killer;
}

/**
 * Store the name of the player's killer.
 *
 * @param pl
 * Player.
 * @param killer
 * Name of the killer to store. Will be copied.
 */
void
player_set_killer (player *pl, const char *killer)
{
    SOFT_ASSERT(pl != NULL, "pl is NULL");
    SOFT_ASSERT(killer != NULL, "killer is NULL");

    if (pl->killer != NULL) {
        efree(pl->killer);
    }

    pl->killer = estrdup(killer);
}

/**
 * Clear the specified player's killer, if any.
 *
 * @param pl
 * Player.
 */
void
player_clear_killer (player *pl)
{
    SOFT_ASSERT(pl != NULL, "pl is NULL");

    if (pl->killer != NULL) {
        efree(pl->killer);
        pl->killer = NULL;
    }
}

/**
 * Perform player login.
 *
 * @param ns
 * Client that wants to log in.
 * @param name
 * Character name to log in to.
 * @param at
 * Character archetype. Will be used to perform new character
 * creation if this is the first time the player is logging in to this
 * character.
 */
void
player_login (socket_struct *ns, const char *name, struct archetype *at)
{
    HARD_ASSERT(ns != NULL);
    HARD_ASSERT(name != NULL);
    HARD_ASSERT(at != NULL);

    /* Not in the login procedure, can't login. */
    if (ns->state != ST_LOGIN) {
        return;
    }

    player *pl = find_player(name);
    if (pl != NULL) {
        pl->cs->state = ST_DEAD;
        player_logout(pl);
    }

    if (ban_check(ns, name)) {
        LOG(SYSTEM, "Ban: Banned player tried to login. [%s, %s]",
            name, socket_get_addr(ns->sc));
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns,
                       "Connection refused due to a ban.");
        ns->state = ST_ZOMBIE;
        return;
    }

    char *path = player_make_path(name, "player.dat");
    FILE *fp = fopen(path, "rb");
    /* This shouldn't happen, because creating a new character creates an
     * empty file (to reserve the character name until the player actually
     * logs in with the character). */
    if (unlikely(fp == NULL)) {
        LOG(ERROR, "Failed to open player data file %s: %s",
            path, strerror(errno));
        ns->state = ST_ZOMBIE;
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns,
                       "Could not open your player file; contact an "
                       "administrator.");
        goto out;
    }

    struct stat statbuf;
    /* Similar to above. */
    if (unlikely(fstat(fileno(fp), &statbuf) != 0)) {
        LOG(ERROR, "Failed to stat player data file %s: %s",
            path, strerror(errno));
        ns->state = ST_ZOMBIE;
        draw_info_send(CHAT_TYPE_GAME, NULL, COLOR_RED, ns,
                       "Could not stat your player file; contact an "
                       "administrator.");
        goto out;
    }

    if (!socket_server_remove(ns)) {
        LOG(ERROR, "Failed to remove socket from managed list: %s",
            socket_get_str(ns->sc));
        goto out;
    }

    LOG(INFO, "Login %s from IP %s", name, socket_get_str(ns->sc));

    pl = get_player(NULL);
    pl->cs = ns;

    /* Create a new object for the player object data. */
    pl->ob = object_get();

#ifdef SAVE_INTERVAL
    pl->last_save_time = time(NULL);
#endif

#ifdef AUTOSAVE
    pl->last_save_tick = pticks;
#endif

    /* If the file is empty, it's a new character. */
    if (statbuf.st_size == 0) {
        player_create(pl, at, name);
    } else {
        player_load(pl, fp);
    }

    pl->ob->custom_attrset = pl;
    pl->ob->speed_left = 0.5;

    object_weight_sum(pl->ob);
    living_update_player(pl->ob);
    link_player_skills(pl->ob);

    pl->cs->state = ST_PLAYING;

    display_motd(pl->ob);
    draw_info_format(COLOR_DK_ORANGE, NULL, "%s has entered the game.", pl->ob->name);
    trigger_global_event(GEVENT_LOGIN, pl, socket_get_addr(pl->cs->sc));

    mapstruct *m = ready_map_name(pl->maplevel, NULL, 0);

    if (!m && strncmp(pl->maplevel, "/random/", 8) == 0) {
        object_enter_map(pl->ob,
                         NULL,
                         ready_map_name(pl->savebed_map, NULL, 0),
                         pl->bed_x,
                         pl->bed_y,
                         true);
    } else {
        object_enter_map(pl->ob,
                         NULL,
                         m,
                         pl->ob->x,
                         pl->ob->y,
                         true);
    }

    /* No savebed map yet, initialize it. */
    if (*pl->savebed_map == '\0') {
        strncpy(pl->savebed_map, pl->maplevel, sizeof(pl->savebed_map) - 1);
        pl->savebed_map[sizeof(pl->savebed_map) - 1] = '\0';

        pl->bed_x = pl->ob->x;
        pl->bed_y = pl->ob->y;
    }

    pl->cs->update_tile = 0;
    pl->cs->look_position = 0;
    pl->cs->ext_title_flag = 1;

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

out:
    if (fp != NULL) {
        fclose(fp);
    }

    efree(path);
}

/**
 * Remove a player from the game that has been disconnected by logging
 * out, the socket connection was interrupted, etc.
 *
 * @param pl
 * The player to remove.
 */
void
player_logout (player *pl)
{
    HARD_ASSERT(pl != NULL);
    SOFT_ASSERT(pl->cs->state == ST_DEAD,
                "Player socket state is: %d",
                pl->cs->state);

    if (pl->ob->type == DEAD_OBJECT) {
        return;
    }

    /* Trigger the global LOGOUT event */
    trigger_global_event(GEVENT_LOGOUT, pl->ob, socket_get_addr(pl->cs->sc));
    statistics_player_logout(pl);

    draw_info_format(COLOR_DK_ORANGE, NULL, "%s left the game.", pl->ob->name);

    player_set_killer(pl, "left");
    hiscore_check(pl->ob, 1);

    /* Be sure we have closed container when we leave */
    container_close(pl->ob, NULL);

    player_save(pl->ob);
    account_logout_char(pl->cs, pl);
    leave_map(pl->ob);

    LOG(SYSTEM, "Connection: dropping connection: %s (%s)",
        socket_get_str(pl->cs->sc),
        pl->ob->name);

    /* To avoid problems with inventory window */
    pl->ob->type = DEAD_OBJECT;
    free_player(pl);
}

/**
 * Handle negative effects caused by equipping items with an item power sum
 * higher than player's maximum item power.
 *
 * @param op
 * Player.
 */
static void
player_item_power_effects (object *op)
{
    HARD_ASSERT(op != NULL);
    SOFT_ASSERT(op->type == PLAYER, "Not a player: %s", object_get_str(op));

    player *pl = CONTR(op);
    if (pl->tgm) {
        /* God mode, no negative effects. */
        return;
    }

    int max = settings.item_power_factor * op->level;
    if (pl->item_power <= max) {
        /* If we're within the maximum item power, nothing to do. */
        return;
    }

    int diff = pl->item_power - max;
    int chance = MAX(1, 100 - diff);
    if (!rndm_chance(MAX_TICKS + 5 + chance)) {
        return;
    }

    if (pticks < pl->item_power_effects) {
        return;
    }

    pl->item_power_effects = pticks + rndm(50, 100) * MAX_TICKS;

    if (diff <= 15 || rndm_chance(diff / 3)) {
        static archetype_t *at = NULL;
        if (at == NULL) {
            at = arch_find("soul_depletion");
            SOFT_ASSERT(at != NULL, "Failed to find soul_depletion arch");
        }

        object *force = object_find_arch(op, at);
        if (force == NULL) {
            force = arch_to_object(at);
            force->speed_left = -1.0;
            force->stats.food = rndm(1, 5) + rndm(0, MAX(1, diff / 3));
            force->stats.food *= rndm(2, 6);
            force->stats.food *= MAX_TICKS;
            force->stats.food *= force->speed;
            if (force->stats.food < 0) {
                force->stats.food = 0;
            }

            force = object_insert_into(force, op, 0);
            SOFT_ASSERT(force != NULL, "Failed to insert force into player %s",
                        object_get_str(op));
        }

        /* Try to pick a random protection/stat/etc to decrease. */
        int tries = 0;
        bool done = false;
        while (!done && tries < 5) {
            switch (rndm(0, 7)) {
            case 0:
            case 1:
            case 2: {
                int num = rndm(0, LAST_PROTECTION - 1);
                if (force->protection[num] > -100) {
                    int prot = force->protection[num];
                    prot -= rndm(1, 5 + diff / 2);
                    if (prot < -100) {
                        prot = -100;
                    }
                    force->protection[num] = prot;
                    done = true;
                }

                break;
            }

            case 3:
            case 4:
            case 5: {
                int num = rndm(0, NUM_STATS - 1);
                int8_t val = get_attr_value(&force->stats, num);
                if (val > -MAX_STAT) {
                    int stat = val - rndm(1, MAX(1, diff / 5));
                    if (stat < -MAX_STAT) {
                        stat = -MAX_STAT;
                    }
                    set_attr_value(&force->stats, num, stat);
                    done = true;
                }

                break;
            }

            case 6:
                if (force->stats.ac > -10) {
                    force->stats.ac--;
                    done = true;
                }

                break;

            case 7:
                if (force->stats.wc > -10) {
                    force->stats.wc--;
                    done = true;
                }

                break;
            }

            tries++;
        }

        if (done) {
            draw_info(COLOR_RED, op, "The combined power of your equipped "
                                     "items begins to sicken your soul!");
            living_update(op);
        }
    } else if (diff > 50 && rndm_chance(MAX(25, 100 - diff))) {
        draw_info(COLOR_RED, op, "The combined power of your equipped items "
                                 "begins to consume your soul!");
        drain_stat(op);
    }  else {
        draw_info(COLOR_RED, op, "The combined power of your equipped items "
                                 "releases wild magic!");
        spell_failure(op, diff);
    }
}

/** @copydoc object_methods_t::remove_map_func */
static void
remove_map_func (object *op)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(op->map != NULL);

    if (op->map->in_memory == MAP_SAVING) {
        return;
    }

    player *pl = CONTR(op);

    /* Remove player from the map's linked list of players. */
    if (pl->map_below != NULL) {
        CONTR(pl->map_below)->map_above = pl->map_above;
    } else {
        op->map->player_first = pl->map_above;
    }

    if (pl->map_above != NULL) {
        CONTR(pl->map_above)->map_below = pl->map_below;
    }

    pl->map_below = pl->map_above = NULL;
    pl->update_los = 1;

    /* If the player has a container open that is not in their inventory,
     * close it. */
    if (pl->container != NULL && pl->container->env != op) {
        container_close(op, NULL);
    }
}

/** @copydoc object_methods_t::process_func */
static void
process_func (object *op)
{
    HARD_ASSERT(op != NULL);

    player *pl = CONTR(op);
    int retval;

    while ((retval = handle_newcs_player(pl)) == 1) {
    }

    if (retval == -1) {
        return;
    }

    if (pl->followed_player != NULL) {
        player *followed = find_player_sh(pl->followed_player);
        if (followed != NULL &&
            followed->ob != NULL &&
            followed->ob->map != NULL) {
            rv_vector rv;

            if (!on_same_map(pl->ob, followed->ob) || (get_rangevector(pl->ob, followed->ob, &rv, 0) && (rv.distance > 4 || rv.distance_z != 0))) {
                int space = map_free_spot(followed->ob->map, followed->ob->x, followed->ob->y, 1, SIZEOFFREE2, pl->ob->arch, pl->ob);

                if (space != -1 && followed->ob->x + freearr_x[space] >= 0 && followed->ob->y + freearr_y[space] >= 0 && followed->ob->x + freearr_x[space] < MAP_WIDTH(followed->ob->map) && followed->ob->y + freearr_y[space] < MAP_HEIGHT(followed->ob->map)) {
                    object_remove(pl->ob, 0);
                    pl->ob->x = followed->ob->x + freearr_x[space];
                    pl->ob->y = followed->ob->y + freearr_y[space];
                    object_insert_map(pl->ob, followed->ob->map, NULL, 0);
                }
            }
        } else {
            draw_info_format(COLOR_RED, pl->ob,
                             "Player %s left.",
                             pl->followed_player);
            FREE_AND_CLEAR_HASH(pl->followed_player);
        }
    }

    /* Use the target system to hit our target - don't hit friendly
     * objects, ourselves or when we are not in combat mode. */
    if (pl->target_object && OBJECT_ACTIVE(pl->target_object) && pl->target_object_count != pl->ob->count && pl->combat && !is_friend_of(pl->ob, pl->target_object)) {
        if (global_round_tag >= pl->action_attack) {
            /* Now we force target as enemy */
            pl->ob->enemy = pl->target_object;
            pl->ob->enemy_count = pl->target_object_count;

            if (!OBJECT_VALID(pl->ob->enemy, pl->ob->enemy_count) || pl->ob->enemy->owner == pl->ob) {
                pl->ob->enemy = NULL;
            } else if (attack_is_melee_range(pl->ob, pl->ob->enemy)) {
                if (!OBJECT_VALID(pl->ob->enemy->enemy, pl->ob->enemy->enemy_count)) {
                    set_npc_enemy(pl->ob->enemy, pl->ob, NULL);
                } else {
                    /* Our target already has an enemy - then note we had
                     * attacked */
                    pl->ob->enemy->attacked_by = pl->ob;
                    pl->ob->enemy->attacked_by_count = pl->ob->count;
                    pl->ob->enemy->attacked_by_distance = 1;
                }

                skill_attack(pl->ob->enemy, pl->ob, 0, NULL);

                pl->action_attack = global_round_tag + pl->ob->weapon_speed;

                pl->action_timer = (float) (pl->action_attack - global_round_tag) / MAX_TICKS;
                pl->last_action_timer = 0;
            }
        }
    }

    if (pl->move_path) {
        player_path_handle(pl);
    }

    player_do_some_living(pl->ob);
    player_item_power_effects(pl->ob);

#ifdef AUTOSAVE

    /* Check for ST_PLAYING state so that we don't try to save off when
     * the player is logging in. */
    if ((pl->last_save_tick + AUTOSAVE) < pticks && pl->cs->state == ST_PLAYING) {
        player_save(pl->ob);
        pl->last_save_tick = pticks;
        hiscore_check(pl->ob, 1);
    }
#endif

    /* Update total playing time. */
    if (pl->cs->state == ST_PLAYING && time(NULL) > pl->last_stat_time_played) {
        pl->last_stat_time_played = time(NULL);

        if (pl->afk) {
            pl->stat_time_afk++;
        } else {
            pl->stat_time_played++;
        }
    }

    /* Check if our target is still valid - if not, update client. */
    if (pl->ob->map && (!pl->target_object || (pl->target_object != pl->ob && pl->target_object_count != pl->target_object->count) || QUERY_FLAG(pl->target_object, FLAG_SYS_OBJECT) || (QUERY_FLAG(pl->target_object, FLAG_IS_INVISIBLE) && !QUERY_FLAG(pl->ob, FLAG_SEE_INVISIBLE)))) {
        send_target_command(pl);
    }
}

/**
 * Initialize the player type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(player)
{
    OBJECT_METHODS(PLAYER)->remove_map_func = remove_map_func;
    OBJECT_METHODS(PLAYER)->process_func = process_func;
}
