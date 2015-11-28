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
 * Quest related code.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <toolkit_string.h>
#include <plugin.h>
#include <arch.h>

/**
 * Find a quest inside the specified quest object.
 * @param quest The quest object.
 * @param quest_name Name of the quest.
 * @return The object that is used to represent the quest in the quest
 * object, NULL if no matching quest found.
 */
static object *quest_find(object *quest, shstr *quest_name)
{
    object *tmp;

    /* Go through the objects in the quest container */
    for (tmp = quest->inv; tmp != NULL; tmp = tmp->below) {
        if (tmp->name == quest_name) {
            return tmp;
        }
    }

    return NULL;
}

/**
 * Check if specified player object has a given quest item in their inventory
 * (name and arch name are compared).
 * @note This function is recursive and will call itself on any
 * non-system inventories inside the player until it finds a matching item.
 * @param op The player object.
 * @param quest_item The quest item we'll be comparing values from.
 * @param flag Flag to compare the quest item against, -1 for no flag
 * comparison.
 * @param[out] num If not NULL, will contain number of matching objects
 * found and the return value will always be 0.
 * @return 1 if the player has the quest item, 0 otherwise.
 */
static int quest_item_check(object *op, object *quest_item, int flag,
        int64_t *num)
{
    object *tmp;

    HARD_ASSERT(op != NULL);
    HARD_ASSERT(quest_item != NULL);

    /* Go through the objects in the object's inventory. */
    for (tmp = op->inv; tmp != NULL; tmp = tmp->below) {
        /* Compare the values. */
        if (tmp->name == quest_item->name &&
                tmp->arch->name == quest_item->arch->name &&
                (flag == -1 || QUERY_FLAG(tmp, flag))) {
            if (num != NULL) {
                *num += MAX(1, tmp->nrof);
            } else {
                return 1;
            }
        }

        /* If it has inventory and is not a system object, check the
         * inventory as well. */
        if (tmp->inv && !QUERY_FLAG(tmp, FLAG_SYS_OBJECT)) {
            int ret;

            ret = quest_item_check(tmp, quest_item, flag, num);

            if (ret && num == NULL) {
                return 1;
            }
        }
    }

    return 0;
}

/**
 * Handle the item drop quest type.
 * @param op Player object.
 * @param quest Quest object.
 * @param quest_pl Quest object in the player, can be NULL.
 * @param item Quest item.
 */
static void quest_check_item_drop(object *op, object *quest, object *quest_pl,
        object *item)
{
    object *clone;
    char buf[MAX_BUF];

    if (item == NULL) {
        LOG(BUG, "Quest '%s' without an item.", quest->name);
        return;
    }

    if (!QUERY_FLAG(item, FLAG_ONE_DROP) &&
            quest_item_check(op, item, -1, NULL)) {
        /* Not one-drop, but we already have the quest object (keys, for
         * example). */
        return;
    } else if (QUERY_FLAG(item, FLAG_ONE_DROP) && quest_pl != NULL) {
        /* One-drop and we already found this quest object before (Rhun's cloak,
         * for example). */
        return;
    }

    /* Create the one-drop item. */
    clone = get_object();
    copy_object_with_inv(item, clone);
    SET_FLAG(clone, FLAG_IDENTIFIED);

    /* Insert the quest item inside the player. */
    insert_ob_in_ob(clone, op);

    if (QUERY_FLAG(item, FLAG_ONE_DROP)) {
        /* Create a quest object in the player's container, so that the item
         * will never drop for them again. */
        quest_pl = arch_get(QUEST_CONTAINER_ARCHETYPE);

        /* Mark this quest as completed. */
        quest_pl->magic = QUEST_STATUS_COMPLETED;
        /* Store the quest UID and name. */
        FREE_AND_COPY_HASH(quest_pl->name, quest->name);
        FREE_AND_COPY_HASH(quest_pl->race, QUEST_NAME(quest));
        /* Insert it inside player's quest container. */
        insert_ob_in_ob(quest_pl, CONTR(op)->quest_container);

        snprintf(VS(buf), "You solved the one drop quest %s!\n",
                QUEST_NAME(quest_pl));
    } else {
        char *name = object_get_short_name_s(clone, op);
        snprintf(VS(buf), "You found the special drop %s!\n", name);
        efree(name);
    }

    draw_map_text_anim(op, COLOR_NAVY, buf);
    draw_info(COLOR_NAVY, op, buf);
    play_sound_player_only(CONTR(op), CMD_SOUND_EFFECT, "event01.ogg",
            0, 0, 0, 0);
}

/**
 * Handle the kill quest type.
 * @param op Player object.
 * @param quest Quest object.
 * @param quest_pl Quest object in the player, can be NULL.
 * @param item Quest item.
 */
static void quest_check_kill(object *op, object *quest, object *quest_pl,
        object *item)
{
    if (item != NULL) {
        LOG(BUG, "Quest '%s' with an item.", quest->name);
        return;
    }

    if (quest_pl == NULL || quest_pl->magic != QUEST_STATUS_STARTED) {
        /* Quest must be started. */
        return;
    }

    /* The +1 makes it so no messages are displayed when player
     * kills more same monsters. */
    if (quest_pl->last_sp <= quest_pl->last_grace + 1) {
        quest_pl->last_sp++;
    }

    /* Display an informative message about the quest status. */
    if (quest_pl->last_sp <= quest_pl->last_grace) {
        char buf[MAX_BUF];

        snprintf(VS(buf), "Quest [b]%s[/b]", QUEST_NAME(quest_pl));

        if (quest_pl->last_sp == quest_pl->last_grace) {
            play_sound_player_only(CONTR(op), CMD_SOUND_EFFECT, "event01.ogg",
                    0, 0, 0, 0);
            snprintfcat(VS(buf), " completed!\n");
        } else {
            snprintfcat(VS(buf), " %d/%d.\n", quest_pl->last_sp,
                    quest_pl->last_grace);
        }

        draw_map_text_anim(op, COLOR_NAVY, buf);
        draw_info(COLOR_NAVY, op, buf);
    }
}

/**
 * Handle the item quest type.
 * @param op Player object.
 * @param quest Quest object.
 * @param quest_pl Quest object in the player, can be NULL.
 * @param item Quest item.
 */
static void quest_check_item(object *op, object *quest, object *quest_pl,
        object *item)
{
    object *clone;
    int64_t num;
    char buf[MAX_BUF];

    if (item == NULL) {
        LOG(BUG, "Quest '%s' without an item.", quest->name);
        return;
    }

    if (quest_pl == NULL || quest_pl->magic != QUEST_STATUS_STARTED) {
        /* Quest must be started. */
        return;
    }

    /* Make sure we haven't found a suitable number of these items yet. */
    if (quest_pl->last_grace > 1) {
        num = 0;
        quest_item_check(op, item, FLAG_QUEST_ITEM, &num);

        if (num >= quest_pl->last_grace) {
            return;
        }
    } else {
        if (quest_item_check(op, item, FLAG_QUEST_ITEM, NULL)) {
            return;
        }
    }

    /* Create a new quest item. */
    clone = get_object();
    copy_object_with_inv(item, clone);
    SET_FLAG(clone, FLAG_QUEST_ITEM);
    SET_FLAG(clone, FLAG_STARTEQUIP);
    CLEAR_FLAG(clone, FLAG_SYS_OBJECT);

    char *name = object_get_base_name_s(clone, op);
    snprintf(VS(buf), "Quest [b]%s[/b]: You found the quest item %s",
            QUEST_NAME(quest_pl), name);
    efree(name);

    if (quest_pl->last_grace > 1) {
        snprintfcat(VS(buf), " (%"PRId64"/%d)", num + MAX(1, clone->nrof),
                quest_pl->last_grace);
    }

    snprintfcat(VS(buf), "!\n");

    draw_map_text_anim(op, COLOR_NAVY, buf);
    draw_info(COLOR_NAVY, op, buf);

    /* Insert the quest item inside the player. */
    insert_ob_in_ob(clone, op);
    play_sound_player_only(CONTR(op), CMD_SOUND_EFFECT, "event01.ogg",
            0, 0, 0, 0);
}

/**
 * Called from quest_handle(). Handles the parsing of a specific quest object.
 * @param op Player object.
 * @param quest Quest object.
 * @param quest_pl Quest object in the player, can be NULL.
 */
static void quest_object_handle(object *op, object *quest, object *quest_pl)
{
    object *item;

    HARD_ASSERT(op != NULL);
    HARD_ASSERT(quest != NULL);

    SOFT_ASSERT(op->type == PLAYER, "Object is not a player: %s",
            object_get_str(op));
    SOFT_ASSERT(quest->type == QUEST_CONTAINER, "Quest is not a quest "
            "container: %s", object_get_str(quest));
    SOFT_ASSERT(quest_pl == NULL || quest_pl->type == QUEST_CONTAINER,
            "Invalid quest_pl supplied: %p", quest_pl);

    /* Trigger the TRIGGER event */
    if (trigger_event(EVENT_TRIGGER, op, quest, quest_pl, NULL, 0, 0, 0,
            SCRIPT_FIX_NOTHING)) {
        return;
    }

    for (item = quest->inv; item != NULL; item = item->below) {
        if (!QUERY_FLAG(item, FLAG_SYS_OBJECT)) {
            break;
        }
    }

    switch (quest->sub_type) {
    case QUEST_TYPE_ITEM_DROP:
        quest_check_item_drop(op, quest, quest_pl, item);
        break;

    case QUEST_TYPE_KILL:
        quest_check_kill(op, quest, quest_pl, item);
        break;

    case QUEST_TYPE_ITEM:
        quest_check_item(op, quest, quest_pl, item);
        break;

    default:
        LOG(BUG, "Quest '%s' has unknown sub type: %d",
                quest->name, quest->sub_type);
        break;
    }
}

/**
 * When a monster drops inventory and there is quest object in it, this
 * function is called to parse the quest object and its contents for any
 * possible quests the player may be running.
 * @warning <b>ONLY</b> call on player objects.
 * @param op The player object.
 * @param quest The quest.
 */
void quest_handle(object *op, object *quest)
{
    object *quest_pl;

    HARD_ASSERT(op != NULL);
    HARD_ASSERT(quest != NULL);

    SOFT_ASSERT(op->type == PLAYER, "Object is not a player: %s",
            object_get_str(op));
    SOFT_ASSERT(quest->type == QUEST_CONTAINER, "Quest is not a quest "
            "container: %s", object_get_str(quest));

    quest_pl = quest_find(CONTR(op)->quest_container, quest->name);

    /* Handle multi-part quests. */
    if (quest->sub_type == QUEST_TYPE_NONE) {
        object *tmp;

        /* Player doesn't have such quest. */
        if (quest_pl == NULL) {
            return;
        }

        for (tmp = quest->inv; tmp != NULL; tmp = tmp->below) {
            quest_object_handle(op, tmp, quest_find(quest_pl, tmp->name));
        }
    } else {
        quest_object_handle(op, quest, quest_pl);
    }
}
