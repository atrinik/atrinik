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
 * Quest related code. */

#include <global.h>

/**
 * Add a one drop quest item to player's quest container, to mark that
 * it has been dropped for that player and will never drop for him again.
 * @param op The player.
 * @param quest_name Name of the quest. */
static void add_one_drop_quest_item(object *op, const char *quest_name)
{
    object *quest_container = get_archetype(QUEST_CONTAINER_ARCHETYPE);

    /* Mark this quest as completed. */
    quest_container->magic = QUEST_STATUS_COMPLETED;
    /* Store the quest name. */
    FREE_AND_COPY_HASH(quest_container->name, quest_name);
    /* Insert it inside player's quest container. */
    insert_ob_in_ob(quest_container, CONTR(op)->quest_container);
}

/**
 * Find a quest inside player's quest container.
 * @param op The player object.
 * @param quest_name Name of the quest.
 * @return The object that is used to represent the quest in the quest
 * container, NULL if no matching quest found. */
static object *find_quest(object *where, const char *quest_name)
{
    object *tmp;

    /* Go through the objects in the quest container */
    for (tmp = where->inv; tmp; tmp = tmp->below) {
        if (tmp->name == quest_name) {
            return tmp;
        }
    }

    return NULL;
}

/**
 * Check if specified player object has a given quest item in their
 * inventory (name and arch name are compared).
 * @note This function is recursive and will call itself on any
 * non-system inventories inside the player until it finds a matching item.
 * @param op The player object.
 * @param quest_item The quest item we'll be comparing values from.
 * @param flag Flag to compare the quest item against, 0 for no flag comparison.
 * @param[out] num If not NULL, will contain number of matching objects
 * found and the return value will always be 0.
 * @return 1 if the player has the quest item, 0 otherwise. */
static int has_quest_item(object *op, object *quest_item, sint32 flag, sint64 *num)
{
    object *tmp;

    /* Go through the objects in the object's inventory. */
    for (tmp = op->inv; tmp; tmp = tmp->below) {
        /* Compare the values. */
        if (tmp->name == quest_item->name && tmp->arch->name == quest_item->arch->name && (!flag || QUERY_FLAG(tmp, flag))) {
            if (num) {
                *num += MAX(1, tmp->nrof);
            }
            else {
                return 1;
            }
        }

        /* If it has inventory and is not a system object, go on recursively. */
        if (tmp->inv && !QUERY_FLAG(tmp, FLAG_SYS_OBJECT)) {
            int ret = has_quest_item(tmp, quest_item, flag, num);

            if (ret && !num) {
                return 1;
            }
        }
    }

    return 0;
}

/**
 * Check quest trigger in a single quest container.
 * @param op Player.
 * @param quest_container Quest container (inside monster/chest/etc).
 * @param quest_object Quest container (inside player), can be NULL. */
static void check_quest_container(object *op, object *quest_container, object *quest_object)
{
    char buf[HUGE_BUF];
    object *tmp;

    /* If this is not a one-drop item quest, it must first be accepted. */
    if (quest_container->sub_type != QUEST_TYPE_ITEM && (!quest_object || quest_object->magic == QUEST_STATUS_COMPLETED)) {
        return;
    }

    tmp = quest_container->inv;

    /* Check for events in this quest container. */
    if (HAS_EVENT(quest_container, EVENT_TRIGGER)) {
        /* Advance through the quest object's inventory, skipping the
         * event object. */
        if (tmp->type == EVENT_OBJECT) {
            tmp = tmp->below;
        }

        if (trigger_event(EVENT_TRIGGER, op, quest_container, tmp, NULL, 0, 0, 0, SCRIPT_FIX_NOTHING)) {
            return;
        }
    }

    switch (quest_container->sub_type) {
        case QUEST_TYPE_ITEM_DROP:
        {
            object *clone_ob;
            int one_drop;

            if (!tmp) {
                return;
            }

            one_drop = QUERY_FLAG(tmp, FLAG_ONE_DROP);

            /* Not one-drop, but we already have the quest object (keys,
             * for example). */
            if (!one_drop && has_quest_item(op, tmp, 0, NULL)) {
                return;
            }
            /* One-drop and we already found this quest object before
             * (Rhun's cloak, for example). */
            else if (one_drop && quest_object) {
                return;
            }

            /* Create the one-drop item. */
            clone_ob = get_object();
            copy_object_with_inv(tmp, clone_ob);
            SET_FLAG(clone_ob, FLAG_IDENTIFIED);

            /* Insert the quest item inside the player. */
            insert_ob_in_ob(clone_ob, op);

            if (one_drop) {
                /* So the item will never drop again for this player. */
                add_one_drop_quest_item(op, quest_container->name);
                snprintf(buf, sizeof(buf), "You solved the one drop quest %s!\n", quest_container->name);
            }
            else {
                snprintf(buf, sizeof(buf), "You found the special drop %s!\n", query_short_name(clone_ob, NULL));
            }

            draw_map_text_anim(op, COLOR_NAVY, buf);
            draw_info(COLOR_NAVY, op, buf);
            play_sound_player_only(CONTR(op), CMD_SOUND_EFFECT, "event01.ogg", 0, 0, 0, 0);
            break;
        }

        case QUEST_TYPE_KILL:
        {
            /* The +1 makes it so no messages are displayed when player
             * kills more same monsters. */
            if (quest_object->last_sp <= quest_object->last_grace + 1) {
                quest_object->last_sp++;
            }

            /* Display an informative message about the quest status. */
            if (quest_object->last_sp <= quest_object->last_grace) {
                if (quest_object->last_sp == quest_object->last_grace) {
                    play_sound_player_only(CONTR(op), CMD_SOUND_EFFECT, "event01.ogg", 0, 0, 0, 0);

                    snprintf(buf, sizeof(buf), "Quest '%s' completed!\n", quest_object->race);
                }
                else {
                    snprintf(buf, sizeof(buf), "Quest %s: %d/%d.\n", quest_object->race, quest_object->last_sp, quest_object->last_grace);
                }

                draw_map_text_anim(op, COLOR_NAVY, buf);
                draw_info(COLOR_NAVY, op, buf);
            }

            break;
        }

        case QUEST_TYPE_ITEM:
        {
            object *clone_ob;
            sint64 num = 0;

            if (!tmp) {
                return;
            }

            /* Have we found this item already? */
            if (quest_object->last_grace > 1) {
                has_quest_item(op, tmp, FLAG_QUEST_ITEM, &num);

                if (num >= quest_object->last_grace) {
                    return;
                }
            }
            else {
                if (has_quest_item(op, tmp, FLAG_QUEST_ITEM, NULL)) {
                    return;
                }
            }

            /* Create a new quest item. */
            clone_ob = get_object();
            copy_object_with_inv(tmp, clone_ob);
            SET_FLAG(clone_ob, FLAG_QUEST_ITEM);
            SET_FLAG(clone_ob, FLAG_STARTEQUIP);
            CLEAR_FLAG(clone_ob, FLAG_SYS_OBJECT);

            /* Insert the quest item inside the player. */
            clone_ob = insert_ob_in_ob(clone_ob, op);

            snprintf(buf, sizeof(buf), "Quest %s: You found the quest item %s (%"FMT64 "/%d)!\n", quest_object->race, query_base_name(clone_ob, NULL), num + MAX(1, clone_ob->nrof), MAX(1, quest_object->last_grace));
            draw_map_text_anim(op, COLOR_NAVY, buf);
            draw_info(COLOR_NAVY, op, buf);

            play_sound_player_only(CONTR(op), CMD_SOUND_EFFECT, "event01.ogg", 0, 0, 0, 0);
            break;
        }

        default:
            logger_print(LOG(BUG), "Quest object '%s' has unknown sub type (%d).", quest_container->name, quest_container->sub_type);
            break;
    }
}

/**
 * When a monster drops inventory and there is quest container object in
 * it, this function is called to parse the quest container and its
 * contents for any possible quests player may be running.
 * @warning <b>ONLY</b> call on player objects.
 * @todo Add support for multiple objects in quest_container's inventory?
 * @param op The player object.
 * @param quest_container The quest container. */
void check_quest(object *op, object *quest_container)
{
    object *quest_object = find_quest(CONTR(op)->quest_container, quest_container->name);

    /* Handle multi-part quests. */
    if (quest_container->sub_type == QUEST_TYPE_NONE) {
        object *tmp;

        /* No quest object, can't go on. */
        if (!quest_object) {
            return;
        }

        for (tmp = quest_container->inv; tmp; tmp = tmp->below) {
            check_quest_container(op, tmp, find_quest(quest_object, tmp->name));
        }
    }
    else {
        check_quest_container(op, quest_container, quest_object);
    }
}
