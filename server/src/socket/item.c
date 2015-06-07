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
 * This contains item logic for client/server. It doesn't contain the
 * actual commands that send the data, but does contain the logic for
 * what items should be sent. */

#include <global.h>
#include <packet.h>
#include <arch.h>

static int check_container(object *pl, object *con);

/** This is the maximum number of bytes we expect any item to take up. */
#define MAXITEMLEN 300

/**
 * This is a similar to query_name, but returns flags to be sent to
 * client.
 * @param op Object to query the flags for.
 * @return Flags. */
unsigned int query_flags(object *op)
{
    uint32_t flags = 0;

    if (QUERY_FLAG(op, FLAG_APPLIED)) {
        flags |= CS_FLAG_APPLIED;
    }

    if (op->type == CONTAINER && (op->attacked_by || (!op->env && QUERY_FLAG(op, FLAG_APPLIED)))) {
        flags |= CS_FLAG_CONTAINER_OPEN;
    }

    if (QUERY_FLAG(op, FLAG_IS_TRAPPED)) {
        flags |= CS_FLAG_IS_TRAPPED;
    }

    if (QUERY_FLAG(op, FLAG_IDENTIFIED) || QUERY_FLAG(op, FLAG_APPLIED)) {
        if (QUERY_FLAG(op, FLAG_DAMNED)) {
            flags |= CS_FLAG_DAMNED;
        } else if (QUERY_FLAG(op, FLAG_CURSED)) {
            flags |= CS_FLAG_CURSED;
        }
    }

    if (QUERY_FLAG(op, FLAG_IS_MAGICAL) && QUERY_FLAG(op, FLAG_IDENTIFIED)) {
        flags |= CS_FLAG_IS_MAGICAL;
    }

    if (QUERY_FLAG(op, FLAG_UNPAID)) {
        flags |= CS_FLAG_UNPAID;
    }

    if (QUERY_FLAG(op, FLAG_INV_LOCKED)) {
        flags |= CS_FLAG_LOCKED;
    }

    if (QUERY_FLAG(op, FLAG_TWO_HANDED)) {
        flags |= CS_FLAG_WEAPON_2H;
    }

    return flags;
}

/**
 * Add data about object to a packet.
 * @param packet Packet to append to.
 * @param op Object to add information about.
 * @param pl Player that will receive the data.
 * @param flags Combination of @ref UPD_XXX.
 * @param level Inventory level.
 */
static void add_object_to_packet(packet_struct *packet, object *op, object *pl,
        uint32_t flags, int level)
{
    packet_debug_data(packet, level, "\nTag");
    packet_append_uint32(packet, op->count);

    if (flags & UPD_LOCATION) {
        packet_debug_data(packet, level, "Location");
        packet_append_uint32(packet, op->env ? op->env->count : 0);
    }

    if (flags & UPD_FLAGS) {
        packet_debug_data(packet, level, "Flags");
        packet_append_uint32(packet, query_flags(op));
    }

    if (flags & UPD_WEIGHT) {
        packet_debug_data(packet, level, "Weight");
        packet_append_uint32(packet, WEIGHT(op));
    }

    if (flags & UPD_FACE) {
        packet_debug_data(packet, level, "Face");

        if (op->inv_face && QUERY_FLAG(op, FLAG_IDENTIFIED)) {
            packet_append_uint16(packet, op->inv_face->number);
        } else {
            packet_append_uint16(packet, op->face->number);
        }
    }

    if (flags & UPD_DIRECTION) {
        packet_debug_data(packet, level, "Direction");
        packet_append_uint8(packet, op->direction);
    }

    if (flags & UPD_TYPE) {
        packet_debug(packet, level, "Item info");
        packet_debug_data(packet, level + 1, "Type");
        packet_append_uint8(packet, op->type);
        packet_debug_data(packet, level + 1, "Sub-type");
        packet_append_uint8(packet, op->sub_type);

        packet_debug_data(packet, level + 1, "Quality");

        if (QUERY_FLAG(op, FLAG_IDENTIFIED)) {
            uint8_t item_level, item_skill;

            if (op->type == BOOK_SPELL && op->stats.sp >= 0 &&
                    op->stats.sp < NROFREALSPELLS) {
                spell_struct *spell = &spells[op->stats.sp];
                item_level = spell->at->clone.level;
                item_skill = SK_WIZARDRY_SPELLS + 1;
            } else {
                item_level = op->item_level;
                item_skill = op->item_skill;
            }

            packet_append_uint8(packet, op->item_quality);
            packet_debug_data(packet, level + 1, "Condition");
            packet_append_uint8(packet, op->item_condition);
            packet_debug_data(packet, level + 1, "Level");
            packet_append_uint8(packet, item_level);

            packet_debug_data(packet, level + 1, "Skill object ID");

            if (item_skill && CONTR(pl)->skill_ptr[item_skill - 1] != NULL) {
                packet_append_uint32(packet,
                        CONTR(pl)->skill_ptr[item_skill - 1]->count);
            } else {
                packet_append_uint32(packet, 0);
            }
        } else {
            packet_append_uint8(packet, 255);
        }
    }

    if (flags & UPD_NAME) {
        packet_debug_data(packet, level, "Name");
        packet_append_string_terminated(packet, op->custom_name ?
            op->custom_name : query_base_name(op, pl));
    }

    if (flags & UPD_ANIM) {
        packet_debug_data(packet, level, "Animation");

        if (QUERY_FLAG(op, FLAG_ANIMATE)) {
            packet_append_uint16(packet, op->animation_id);
        } else {
            packet_append_uint16(packet, 0);
        }
    }

    if (flags & UPD_ANIMSPEED) {
        int anim_speed = 0;

        if (QUERY_FLAG(op, FLAG_ANIMATE)) {
            if (op->anim_speed) {
                anim_speed = op->anim_speed;
            } else {
                if (FABS(op->speed) < 0.001) {
                    anim_speed = 255;
                } else if (FABS(op->speed) >= 1.0) {
                    anim_speed = 1;
                } else {
                    anim_speed = (int) (1.0 / FABS(op->speed));
                }
            }

            if (anim_speed > 255) {
                anim_speed = 255;
            }
        }

        packet_debug_data(packet, level, "Animation speed");
        packet_append_uint8(packet, anim_speed);
    }

    if (flags & UPD_NROF) {
        packet_debug_data(packet, level, "Nrof");
        packet_append_uint32(packet, op->nrof);
    }

    if (flags & UPD_EXTRA) {
        if (op->type == SPELL) {
            packet_debug(packet, level, "Spell info:\n");
            packet_debug_data(packet, level + 1, "Cost");
            packet_append_uint16(packet, SP_level_spellpoint_cost(pl,
                    op->stats.sp,
                    CONTR(pl)->skill_ptr[SK_WIZARDRY_SPELLS]->level));
            packet_debug_data(packet, level + 1, "Path");
            packet_append_uint32(packet, spells[op->stats.sp].path);
            packet_debug_data(packet, level + 1, "Flags");
            packet_append_uint32(packet, spells[op->stats.sp].flags);
            packet_debug_data(packet, level + 1, "Message");
            packet_append_string_terminated(packet, op->msg ? op->msg : "");
        } else if (op->type == SKILL) {
            packet_debug(packet, level, "Skill info:\n");
            packet_debug_data(packet, level + 1, "Level");
            packet_append_uint8(packet, op->level);
            packet_debug_data(packet, level + 1, "Experience");
            packet_append_int64(packet, op->stats.exp);
        } else if (op->type == FORCE || op->type == POISONING) {
            int32_t sec;

            packet_debug(packet, level, "Force info:\n");

            sec = -1;

            if (QUERY_FLAG(op, FLAG_IS_USED_UP)) {
                sec = (int) (op->speed_left / op->speed / (float) MAX_TICKS +
                        (1.0 / op->speed / (float) MAX_TICKS *
                        (float) op->stats.food - 1));
                sec = ABS(sec);
            }

            packet_debug_data(packet, level + 1, "Seconds");
            packet_append_int32(packet, sec);
            packet_debug_data(packet, level + 1, "Message");
            packet_append_string_terminated(packet,
                    op->msg != NULL ? op->msg : "");
        }
    }

    if (flags & UPD_GLOW && CONTR(pl)->socket.socket_version >= 1060) {
        packet_debug_data(packet, level, "Glow color");
        packet_append_string_terminated(packet,
                op->glow != NULL ? op->glow : "");
        packet_debug_data(packet, level, "Glow speed");
        packet_append_uint8(packet, op->glow_speed);
    }
}

/**
 * Recursively draw inventory of an object for DMs.
 * @param pl DM.
 * @param packet Packet to append to.
 * @param op Object of which inventory is going to be sent.
 * @param level Inventory level.
 */
static void esrv_draw_look_rec(object *pl, packet_struct *packet, object *op,
        int level)
{
    object *tmp;

    packet_debug(packet, level, "Inventory:");

    packet_debug_data(packet, level + 1, "\nTag");
    packet_append_uint32(packet, 0);
    packet_debug_data(packet, level + 1, "Flags");
    packet_append_uint32(packet, 0);
    packet_debug_data(packet, level + 1, "Weight");
    packet_append_int32(packet, -1);
    packet_debug_data(packet, level + 1, "Face");
    packet_append_uint16(packet, blank_face->number);
    packet_debug_data(packet, level + 1, "Direction");
    packet_append_uint8(packet, 0);
    packet_debug_data(packet, level + 1, "Name");
    packet_append_string_terminated(packet, "in inventory");
    packet_debug_data(packet, level + 1, "Animation");
    packet_append_uint16(packet, 0);
    packet_debug_data(packet, level + 1, "Animation speed");
    packet_append_uint8(packet, 0);
    packet_debug_data(packet, level + 1, "Nrof");
    packet_append_uint32(packet, 0);
    packet_debug_data(packet, level + 1, "Glow color");
    packet_append_string_terminated(packet, "");
    packet_debug_data(packet, level + 1, "Glow speed");
    packet_append_uint8(packet, 0);

    for (tmp = op->inv; tmp; tmp = tmp->below) {
        add_object_to_packet(packet, HEAD(tmp), pl, UPD_FLAGS | UPD_WEIGHT |
                UPD_FACE | UPD_DIRECTION | UPD_NAME | UPD_ANIM | UPD_ANIMSPEED |
                UPD_NROF | UPD_GLOW, level + 1);

        if (tmp->inv && tmp->type != PLAYER) {
            esrv_draw_look_rec(pl, packet, tmp, level + 1);
        }
    }

    packet_debug_data(packet, level + 1, "\nTag");
    packet_append_uint32(packet, 0);
    packet_debug_data(packet, level + 1, "Flags");
    packet_append_uint32(packet, 0);
    packet_debug_data(packet, level + 1, "Weight");
    packet_append_int32(packet, -1);
    packet_debug_data(packet, level + 1, "Face");
    packet_append_uint16(packet, blank_face->number);
    packet_debug_data(packet, level + 1, "Direction");
    packet_append_uint8(packet, 0);
    packet_debug_data(packet, level + 1, "Name");
    packet_append_string_terminated(packet, "end inventory");
    packet_debug_data(packet, level + 1, "Animation");
    packet_append_uint16(packet, 0);
    packet_debug_data(packet, level + 1, "Animation speed");
    packet_append_uint8(packet, 0);
    packet_debug_data(packet, level + 1, "Nrof");
    packet_append_uint32(packet, 0);
    packet_debug_data(packet, level + 1, "Glow color");
    packet_append_string_terminated(packet, "");
    packet_debug_data(packet, level + 1, "Glow speed");
    packet_append_uint8(packet, 0);
}

/**
 * Draw the look window. Don't need to do animations here.
 *
 * This sends all the faces to the client, not just updates. This is
 * because object ordering would otherwise be inconsistent.
 * @param pl Player to draw the look window for.
 * @todo The hacky way of embedding look_position into the virtual next/previous
 * object tags should be changed.
 */
void esrv_draw_look(object *pl)
{
    packet_struct *packet;
    object *tmp, *last;
    int start_look = 0, end_look = 0;

    if (QUERY_FLAG(pl, FLAG_REMOVED) || pl->map == NULL ||
            pl->map->in_memory != MAP_IN_MEMORY ||
            OUT_OF_MAP(pl->map, pl->x, pl->y)) {
        return;
    }

    /* Grab last (top) object without browsing the objects. */
    tmp = GET_MAP_OB_LAST(pl->map, pl->x, pl->y);

    packet = packet_new(CLIENT_CMD_ITEM, 512, 256);
    packet_enable_ndelay(packet);
    packet_debug_data(packet, 0, "Container mode flag");
    packet_append_uint32(packet, 0);
    packet_debug_data(packet, 0, "Target inventory ID");
    packet_append_uint32(packet, 0);
    packet_debug_data(packet, 0, "End flag");
    packet_append_uint8(packet, 1);

    packet_debug(packet, 0, "Below inventory:\n");

    if (CONTR(pl)->socket.look_position) {
        packet_debug_data(packet, 1, "\nTag");
        packet_append_uint32(packet, 0x80000000 |
                (CONTR(pl)->socket.look_position - NUM_LOOK_OBJECTS));
        packet_debug_data(packet, 1, "Flags");
        packet_append_uint32(packet, 0);
        packet_debug_data(packet, 1, "Weight");
        packet_append_int32(packet, -1);
        packet_debug_data(packet, 1, "Face");
        packet_append_uint16(packet, prev_item_face->number);
        packet_debug_data(packet, 1, "Direction");
        packet_append_uint8(packet, 0);
        packet_debug_data(packet, 1, "Name");
        packet_append_string_terminated(packet, "Previous group of items");
        packet_debug_data(packet, 1, "Animation");
        packet_append_uint16(packet, 0);
        packet_debug_data(packet, 1, "Animation speed");
        packet_append_uint8(packet, 0);
        packet_debug_data(packet, 1, "Nrof");
        packet_append_uint32(packet, 0);
        packet_debug_data(packet, 1, "Glow color");
        packet_append_string_terminated(packet, "");
        packet_debug_data(packet, 1, "Glow speed");
        packet_append_uint8(packet, 0);
    }

    for (last = NULL; tmp != last; tmp = tmp->below) {
        if (tmp == pl) {
            continue;
        }

        /* Skip map mask, sys_objects and invisible objects when we can't
         * see them. */
        if ((tmp->layer <= LAYER_FMASK || IS_INVISIBLE(tmp, pl)) &&
                !CONTR(pl)->tsi) {
            continue;
        }

        if (++start_look < CONTR(pl)->socket.look_position) {
            continue;
        }

        /* If we have too many items to send, send a 'next group' object
         * and leave here. */
        if (++end_look > NUM_LOOK_OBJECTS) {
            packet_debug_data(packet, 1, "\nTag");
            packet_append_uint32(packet, 0x80000000 |
                    (CONTR(pl)->socket.look_position + NUM_LOOK_OBJECTS));
            packet_debug_data(packet, 1, "Flags");
            packet_append_uint32(packet, 0);
            packet_debug_data(packet, 1, "Weight");
            packet_append_int32(packet, -1);
            packet_debug_data(packet, 1, "Face");
            packet_append_uint16(packet, next_item_face->number);
            packet_debug_data(packet, 1, "Direction");
            packet_append_uint8(packet, 0);
            packet_debug_data(packet, 1, "Name");
            packet_append_string_terminated(packet, "Next group of items");
            packet_debug_data(packet, 1, "Animation");
            packet_append_uint16(packet, 0);
            packet_debug_data(packet, 1, "Animation speed");
            packet_append_uint8(packet, 0);
            packet_debug_data(packet, 1, "Nrof");
            packet_append_uint32(packet, 0);
            packet_debug_data(packet, 1, "Glow color");
            packet_append_string_terminated(packet, "");
            packet_debug_data(packet, 1, "Glow speed");
            packet_append_uint8(packet, 0);
            break;
        }

        add_object_to_packet(packet, HEAD(tmp), pl, UPD_FLAGS | UPD_WEIGHT |
                UPD_FACE | UPD_DIRECTION | UPD_NAME | UPD_ANIM |
                UPD_ANIMSPEED | UPD_NROF | UPD_GLOW, 1);

        if (CONTR(pl)->tsi && tmp->inv && tmp->type != PLAYER) {
            esrv_draw_look_rec(pl, packet, tmp, 1);
        }
    }

    socket_send_packet(&CONTR(pl)->socket, packet);
}

/**
 * Close a container.
 * @param op Player to close container of. */
void esrv_close_container(object *op)
{
    packet_struct *packet;

    packet = packet_new(CLIENT_CMD_ITEM, 32, 0);
    packet_enable_ndelay(packet);
    packet_debug_data(packet, 0, "Container mode flag");
    packet_append_int32(packet, -1);
    packet_debug_data(packet, 0, "Target inventory ID");
    packet_append_int32(packet, -1);
    socket_send_packet(&CONTR(op)->socket, packet);
}

/**
 * Sends a whole inventory of an object.
 * @param pl Player to send the inventory to.
 * @param op Object to send inventory of. Can be same as 'pl' to send
 * inventory of the player. */
void esrv_send_inventory(object *pl, object *op)
{
    packet_struct *packet;
    object *tmp;

    packet = packet_new(CLIENT_CMD_ITEM, 128, 256);
    packet_enable_ndelay(packet);
    packet_debug_data(packet, 0, "Container mode flag");

    /* In this case we're sending a container inventory */
    if (pl != op) {
        /* Container mode flag */
        packet_append_int32(packet, -1);
    } else {
        packet_append_int32(packet, op->count);
    }

    packet_debug_data(packet, 0, "Target inventory ID");
    packet_append_uint32(packet, op->count);
    packet_debug_data(packet, 0, "End flag");
    packet_append_uint8(packet, 1);

    for (tmp = op->inv; tmp; tmp = tmp->below) {
        if (IS_INVISIBLE(tmp, pl)) {
            continue;
        }

        add_object_to_packet(packet, tmp, pl, UPD_FLAGS | UPD_WEIGHT |
                UPD_FACE | UPD_DIRECTION | UPD_TYPE | UPD_NAME | UPD_ANIM |
                UPD_ANIMSPEED | UPD_NROF | UPD_EXTRA | UPD_GLOW, 0);
    }

    socket_send_packet(&CONTR(pl)->socket, packet);
}

/**
 * Informs client about new object data.
 * @param flags List of values to update.
 * @param pl The player.
 * @param op The object to update. */
static void esrv_update_item_send(int flags, object *pl, object *op)
{
    packet_struct *packet;

    if (!CONTR(pl)) {
        return;
    }

    /* Can't see that item... */
    if (IS_INVISIBLE(op, pl)) {
        return;
    }

    packet = packet_new(CLIENT_CMD_ITEM_UPDATE, 64, 128);
    packet_enable_ndelay(packet);
    packet_debug_data(packet, 0, "Flags");
    packet_append_uint16(packet, flags);
    add_object_to_packet(packet, op, pl, flags, 0);
    socket_send_packet(&CONTR(pl)->socket, packet);
}

/**
 * Updates specified data about the specified object for all involved
 * clients.
 * @param flags List of values to update.
 * @param op The object to update. */
void esrv_update_item(int flags, object *op)
{
    if (op->type == PLAYER) {
        esrv_update_item_send(flags, op, op);
    } else if (op->env) {
        if (op->env->type == CONTAINER) {
            object *tmp;

            for (tmp = op->env->attacked_by; tmp; tmp = CONTR(tmp)->container_above) {
                esrv_update_item_send(flags, tmp, op);
            }
        } else if (op->env->type == PLAYER) {
            esrv_update_item_send(flags, op->env, op);
        }
    }
}

/**
 * Informs a client about the specified object.
 * @param pl The player.
 * @param op Object to send information of. */
static void esrv_send_item_send(object *pl, object *op)
{
    packet_struct *packet;

    if (!CONTR(pl) || CONTR(pl)->socket.state != ST_PLAYING) {
        return;
    }

    /* Can't see that item... */
    if (IS_INVISIBLE(op, pl)) {
        return;
    }

    packet = packet_new(CLIENT_CMD_ITEM, 64, 128);
    packet_enable_ndelay(packet);
    packet_debug_data(packet, 0, "Container mode flag");
    packet_append_int32(packet, -4);
    packet_debug_data(packet, 0, "Target inventory ID");
    packet_append_uint32(packet, op->env->count);
    packet_debug_data(packet, 0, "End flag");
    packet_append_uint8(packet, 0);
    add_object_to_packet(packet, HEAD(op), pl, UPD_FLAGS | UPD_WEIGHT |
            UPD_FACE | UPD_DIRECTION | UPD_TYPE | UPD_NAME | UPD_ANIM |
            UPD_ANIMSPEED | UPD_NROF | UPD_EXTRA | UPD_GLOW, 0);
    socket_send_packet(&CONTR(pl)->socket, packet);
}

/**
 * Informs all involved clients about the specified object.
 * @param op Object to send information of. */
void esrv_send_item(object *op)
{
    object *tmp;

    /* No object or object is not in inventory, nothing to do here. */
    if (!op || !op->env) {
        return;
    }

    if (op->env->type == CONTAINER) {
        /* Send the item information to all players that are looking
         * inside this container. */
        for (tmp = op->env->attacked_by; tmp; tmp = CONTR(tmp)->container_above) {
            esrv_send_item_send(tmp, op);
        }
    } else if (op->env->type == PLAYER) {
        esrv_send_item_send(op->env, op);
    }
}

/**
 * Informs a client about item deletion.
 * @param pl Player.
 * @param op The item that was deleted. */
static void esrv_del_item_send(object *pl, object *op)
{
    packet_struct *packet;

    if (!CONTR(pl)) {
        return;
    }

    /* Can't see that item... */
    if (IS_INVISIBLE(op, pl)) {
        return;
    }

    packet = packet_new(CLIENT_CMD_ITEM_DELETE, 16, 0);
    packet_enable_ndelay(packet);
    packet_debug_data(packet, 0, "Object ID");
    packet_append_uint32(packet, op->count);
    socket_send_packet(&CONTR(pl)->socket, packet);
}

/**
 * Informs involved clients about item deletion.
 * @param op The item that was deleted. */
void esrv_del_item(object *op)
{
    /* No object or object is not inside an inventory, nothing to do. */
    if (!op || !op->env) {
        return;
    }

    if (op->env->type == CONTAINER) {
        object *tmp;

        for (tmp = op->env->attacked_by; tmp; tmp = CONTR(tmp)->container_above) {
            esrv_del_item_send(tmp, op);
        }
    } else if (op->env->type == PLAYER) {
        esrv_del_item_send(op->env, op);
    }
}

/**
 * Recursive part of esrv_get_ob_from_count(). */
static object *get_ob_from_count_rec(object *pl, object *where, tag_t count)
{
    object *tmp, *head, *tmp2;

    for (tmp = where; tmp; tmp = tmp->below) {
        head = HEAD(tmp);

        if (head->count == count) {
            return head;
        } else if (head->inv && (CONTR(pl)->tsi || (head->type == CONTAINER && CONTR(pl)->container == head))) {
            tmp2 = get_ob_from_count_rec(pl, head->inv, count);

            if (tmp2) {
                return tmp2;
            }
        }
    }

    return NULL;
}

/**
 * Looks for an object player's client requested in player's inventory
 * and below player.
 * @param pl Player.
 * @param count ID of the object to look for.
 * @return The found object, NULL if it can't be found. */
object *esrv_get_ob_from_count(object *pl, tag_t count)
{
    object *tmp;

    if (pl->count == count) {
        return pl;
    }

    tmp = get_ob_from_count_rec(pl, pl->inv, count);

    if (tmp) {
        return tmp;
    }

    tmp = get_ob_from_count_rec(pl, GET_MAP_OB_LAST(pl->map, pl->x, pl->y), count);

    if (tmp) {
        return tmp;
    }

    return NULL;
}

void socket_command_item_examine(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos)
{
    tag_t tag;
    object *op;

    tag = packet_to_uint32(data, len, &pos);

    if (!tag) {
        return;
    }

    op = esrv_get_ob_from_count(pl->ob, tag);

    if (!op) {
        return;
    }

    examine(pl->ob, op, NULL);

    if (pl->tsi) {
        StringBuffer *sb;
        char *cp;

        sb = stringbuffer_new();
        dump_object(op, sb);
        cp = stringbuffer_finish(sb);
        draw_info(COLOR_WHITE, pl->ob, cp);
        efree(cp);
    }
}

/**
 * Remove any quickslots of player 'pl' matching slot 'slot'.
 * @param slot ID of the quickslot to look for.
 * @param pl Inside which player to search in. */
static void remove_quickslot(uint8_t slot, player *pl)
{
    object *tmp;

    for (tmp = pl->ob->inv; tmp; tmp = tmp->below) {
        if (tmp->quickslot && tmp->quickslot == slot) {
            tmp->quickslot = 0;
        }
    }
}

/**
 * Send quickslots to player.
 * @param pl Player to send the quickslots to. */
void send_quickslots(player *pl)
{
    packet_struct *packet;
    object *tmp;

    packet = packet_new(CLIENT_CMD_QUICKSLOT, 256, 256);

    for (tmp = pl->ob->inv; tmp; tmp = tmp->below) {
        if (tmp->quickslot) {
            packet_debug_data(packet, 0, "\nQuickslot ID");
            packet_append_uint8(packet, tmp->quickslot - 1);
            packet_debug_data(packet, 0, "Object ID");
            packet_append_uint32(packet, tmp->count);
        }
    }

    socket_send_packet(&pl->socket, packet);
}

void socket_command_quickslot(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos)
{
    uint8_t quickslot;
    int32_t tag;
    object *op;

    quickslot = packet_to_uint8(data, len, &pos);

    if (quickslot < 1 || quickslot > MAX_QUICKSLOT) {
        return;
    }

    tag = packet_to_int32(data, len, &pos);

    if (!tag) {
        return;
    }

    remove_quickslot(quickslot, pl);

    if (tag != -1) {
        op = esrv_get_ob_from_count(pl->ob, tag);

        if (!op) {
            return;
        }

        op->quickslot = quickslot;
    }
}

void socket_command_item_apply(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos)
{
    uint32_t tag;
    object *op;

    tag = packet_to_uint32(data, len, &pos);

    if (!tag) {
        return;
    }

    op = esrv_get_ob_from_count(pl->ob, tag);

    if (QUERY_FLAG(pl->ob, FLAG_REMOVED)) {
        return;
    }

    /* If the high bit is set, player applied a pseudo object. */
    if (tag & 0x80000000) {
        pl->socket.look_position = tag & 0x7fffffff;
        pl->socket.update_tile = 0;
        return;
    }

    if (!op) {
        return;
    }

    player_apply(pl->ob, op, 0, 0);
}

void socket_command_item_lock(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos)
{
    tag_t tag;
    object *op;

    tag = packet_to_uint32(data, len, &pos);

    if (!tag) {
        return;
    }

    op = esrv_get_ob_from_count(pl->ob, tag);

    if (!op) {
        return;
    }

    /* Only lock item inside the player's own inventory */
    if (is_player_inv(op) != pl->ob) {
        draw_info(COLOR_WHITE, pl->ob, "You can't lock items outside your inventory!");
        return;
    }

    TOGGLE_FLAG(op, FLAG_INV_LOCKED);
    esrv_update_item(UPD_FLAGS, op);
}

void socket_command_item_mark(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos)
{
    tag_t tag;
    object *op;

    tag = packet_to_uint32(data, len, &pos);

    if (!tag) {
        return;
    }

    op = esrv_get_ob_from_count(pl->ob, tag);

    if (!op) {
        return;
    }

    if (pl->mark_count == op->count) {
        draw_info_format(COLOR_WHITE, pl->ob, "Unmarked item %s.", query_name(op, NULL));
        pl->mark = NULL;
        pl->mark_count = 0;
    } else {
        draw_info_format(COLOR_WHITE, pl->ob, "Marked item %s.", query_name(op, NULL));
        pl->mark_count = op->count;
        pl->mark = op;
    }
}

/**
 * Move an object to a new location.
 * @param pl Player.
 * @param to ID of the object to move the object. If 0, it's on ground.
 * @param tag ID of the object to drop.
 * @param nrof How many objects to drop. */
void esrv_move_object(object *pl, tag_t to, tag_t tag, long nrof)
{
    object *op, *env;
    int tmp;

    op = esrv_get_ob_from_count(pl, tag);

    if (!op) {
        return;
    }

    /* drop it to the ground */
    if (!to) {
        if (op->map && !op->env) {
            return;
        }

        CLEAR_FLAG(pl, FLAG_INV_LOCKED);

        if ((tmp = check_container(pl, op))) {
            draw_info(COLOR_WHITE, pl, "First remove all god-given items from this container!");
        } else if (QUERY_FLAG(pl, FLAG_INV_LOCKED)) {
            draw_info(COLOR_WHITE, pl, "You can't drop a container with locked items inside!");
        } else {
            drop_object(pl, op, nrof, 0);
        }

        CLEAR_FLAG(pl, FLAG_INV_LOCKED);

        return;
    } else if (to == pl->count || (to == op->count && !op->env)) {
        /* Pick it up to the inventory */

        /* Return if player has already picked it up */
        if (op->env == pl) {
            return;
        }

        CONTR(pl)->count = nrof;
        /* It goes in player inv or readied container */
        pick_up(pl, op, 0);
        return;
    }

    /* If not dropped or picked up, we are putting it into a sack */
    env = esrv_get_ob_from_count(pl, to);

    if (!env) {
        return;
    }

    /* put_object_in_sack() presumes that necessary sanity checking has
     * already been done (eg, it can be picked up and fits in in a sack,
     * so check for those things. We should also check and make sure env
     * is in fact a container for that matter. */
    if (env->type == CONTAINER && can_pick(pl, op) && sack_can_hold(pl, env, op, nrof)) {
        CLEAR_FLAG(pl, FLAG_INV_LOCKED);
        tmp = check_container(pl, op);

        if (QUERY_FLAG(pl, FLAG_INV_LOCKED) && env->env != pl) {
            draw_info(COLOR_WHITE, pl, "You can't drop a container with locked items inside!");
        } else if (tmp && env->env != pl) {
            draw_info(COLOR_WHITE, pl, "First remove all god-given items from this container!");
        } else if (QUERY_FLAG(op, FLAG_STARTEQUIP) && env->env != pl) {
            draw_info(COLOR_WHITE, pl, "You can't store god-given items outside your inventory!");
        } else {
            put_object_in_sack(pl, env, op, nrof);
        }

        CLEAR_FLAG(pl, FLAG_INV_LOCKED);

        return;
    }
}

/**
 * Check if container can be dropped.
 *
 * Locked or FLAG_STARTEQUIP items cannot be dropped, so we check if the
 * container carries one (or one of containers in that container).
 * @param pl Player.
 * @param con Container.
 * @return 0 if it can be dropped, non-zero otherwise. */
static int check_container(object *pl, object *con)
{
    object *current, *next;
    int ret = 0;

    /* Only check stuff *inside* a container */
    if (con->type != CONTAINER) {
        return 0;
    }

    for (current = con->inv; current != NULL; current = next) {
        next = current->below;
        ret += check_container(pl, current);

        if (QUERY_FLAG(current, FLAG_STARTEQUIP)) {
            ret += 1;
        }

        if (QUERY_FLAG(current, FLAG_INV_LOCKED)) {
            SET_FLAG(pl, FLAG_INV_LOCKED);
        }
    }

    return ret;
}
