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
 * Handles code related to @ref SIGN "signs".
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <packet.h>
#include <plugin.h>
#include <player.h>
#include <object.h>
#include <object_methods.h>
#include <check_inv.h>

/** @copydoc object_methods_t::apply_func */
static int
apply_func (object *op, object *applier, int aflags)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(applier != NULL);

    /* No point in non-players applying signs. */
    if (applier->type != PLAYER) {
        return OBJECT_METHOD_OK;
    }

    shstr *notification_msg = object_get_value(op, "notification_message");
    if (op->msg == NULL &&
        op->title == NULL &&
        notification_msg == NULL &&
        !HAS_EVENT(op, EVENT_SAY)) {
        draw_info(COLOR_WHITE, applier, "Nothing is written on it.");
        return OBJECT_METHOD_OK;
    }

    if (op->stats.food != 0) {
        if (op->last_eat >= op->stats.food) {
            if (!QUERY_FLAG(op, FLAG_SYS_OBJECT)) {
                draw_info(COLOR_WHITE, applier, "You cannot read it anymore.");
            }

            return OBJECT_METHOD_OK;
        }

        op->last_eat++;
    }

    if (QUERY_FLAG(applier, FLAG_BLIND) && !QUERY_FLAG(op, FLAG_SYS_OBJECT)) {
        draw_info(COLOR_WHITE, applier, "You are unable to read while blind.");
        return OBJECT_METHOD_OK;
    }

    if (op->slaying != NULL || op->stats.hp != 0 || op->race != NULL) {
        object *match = check_inv(op, applier);

        if ((match != NULL && op->last_sp != 0) ||
            (match == NULL && op->last_sp == 0)) {
            if (!QUERY_FLAG(op, FLAG_SYS_OBJECT)) {
                draw_info(COLOR_WHITE, applier,
                          "You are unable to decipher the strange symbols.");
            }

            return OBJECT_METHOD_OK;
        }
    }

    if (op->direction != 0 && QUERY_FLAG(op, FLAG_SYS_OBJECT)) {
        if (applier->direction != absdir(op->direction + 4) &&
            !(QUERY_FLAG(op, FLAG_SPLITTING) &&
              (applier->direction == absdir(op->direction - 5) ||
               applier->direction == absdir(op->direction + 5)))) {
            return OBJECT_METHOD_OK;
        }
    }

    /* Handle the 'say' event. */
    if (trigger_event(EVENT_SAY, applier, op, NULL, NULL, 0, 0, 0, 0)) {
        return OBJECT_METHOD_OK;
    }

    if (op->title != NULL) {
        play_sound_player_only(CONTR(applier),
                               CMD_SOUND_EFFECT,
                               op->title,
                               0,
                               0,
                               0,
                               0);
    }

    if (op->msg != NULL) {
        draw_info(COLOR_NAVY, applier, op->msg);
    }

    /* Add notification message, if any. */
    if (notification_msg != NULL) {
        shstr *notification_action =
            object_get_value(op, "notification_action");
        shstr *notification_shortcut =
            object_get_value(op, "notification_shortcut");
        shstr *notification_delay =
            object_get_value(op, "notification_delay");

        packet_struct *packet = packet_new(CLIENT_CMD_NOTIFICATION, 256, 512);

        packet_debug_data(packet, 0, "\nNotification command type");
        packet_append_uint8(packet, CMD_NOTIFICATION_TEXT);
        packet_debug_data(packet, 0, "Text");
        packet_append_string_terminated(packet, notification_msg);

        if (notification_action != NULL) {
            packet_debug_data(packet, 0, "\nNotification command type");
            packet_append_uint8(packet, CMD_NOTIFICATION_ACTION);
            packet_debug_data(packet, 0, "Action");
            packet_append_string_terminated(packet, notification_action);
        }

        if (notification_shortcut != NULL) {
            packet_debug_data(packet, 0, "\nNotification command type");
            packet_append_uint8(packet, CMD_NOTIFICATION_SHORTCUT);
            packet_debug_data(packet, 0, "Shortcut");
            packet_append_string_terminated(packet, notification_shortcut);
        }

        if (notification_delay != NULL) {
            packet_debug_data(packet, 0, "\nNotification command type");
            packet_append_uint8(packet, CMD_NOTIFICATION_DELAY);
            packet_debug_data(packet, 0, "Delay");
            packet_append_uint32(packet, atoi(notification_delay));
        }

        socket_send_packet(&CONTR(applier)->socket, packet);
    }

    return OBJECT_METHOD_OK;
}

/** @copydoc object_methods_t::move_on_func */
static int
move_on_func (object *op, object *victim, object *originator, int state)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(victim != NULL);

    return apply_func(op, victim, 0);
}

/** @copydoc object_methods_t::trigger_func */
static int
trigger_func (object *op, object *cause, int state)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(cause != NULL);

    /* If the event is caused by a move-off and the object that caused
     * the event is a player, return immediately. This is done because
     * events are asynchronous, so when this code runs because of the
     * move-off, it means the player that caused the event was removed
     * from the map (temporarily, to be inserted immediately afterwards,
     * but the event is caused by the removal), and also from the linked
     * list of map's players, thus even if we proceeded as normal, the
     * activator would not get the message/music/etc data from the sign
     * object, only other nearby players. */
    if (state == 0 && cause->type == PLAYER) {
        return OBJECT_METHOD_OK;
    }

    if (op->stats.food != 0) {
        if (op->last_eat >= op->stats.food) {
            return OBJECT_METHOD_OK;
        }

        op->last_eat++;
    }

    if (op->title != NULL) {
        play_sound_map(op->map,
                       CMD_SOUND_EFFECT,
                       op->title,
                       op->x,
                       op->y,
                       0,
                       0);
    }

    if (op->msg != NULL) {
        draw_info_map(CHAT_TYPE_GAME,
                      NULL,
                      COLOR_NAVY,
                      op->map,
                      op->x,
                      op->y,
                      MAP_INFO_NORMAL,
                      NULL,
                      NULL,
                      op->msg);
    }

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the sign type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(sign)
{
    OBJECT_METHODS(SIGN)->apply_func = apply_func;
    OBJECT_METHODS(SIGN)->move_on_func = move_on_func;
    OBJECT_METHODS(SIGN)->trigger_func = trigger_func;
}
