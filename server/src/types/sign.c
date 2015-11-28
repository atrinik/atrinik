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
 * @author Alex Tokar */

#include <global.h>
#include <packet.h>
#include <plugin.h>

/** @copydoc object_methods::apply_func */
static int apply_func(object *op, object *applier, int aflags)
{
    shstr *notification_msg;

    (void) aflags;

    /* No point in non-players applying signs. */
    if (applier->type != PLAYER) {
        return OBJECT_METHOD_OK;
    }

    notification_msg = object_get_value(op, "notification_message");

    if (!op->msg && !op->title && !notification_msg && !HAS_EVENT(op, EVENT_SAY)) {
        draw_info(COLOR_WHITE, applier, "Nothing is written on it.");
        return OBJECT_METHOD_OK;
    }

    if (op->stats.food) {
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

    if (op->slaying || op->stats.hp || op->race) {
        object *match;

        match = check_inv(op, applier);

        if ((match && op->last_sp) || (!match && !op->last_sp)) {
            if (!QUERY_FLAG(op, FLAG_SYS_OBJECT)) {
                draw_info(COLOR_WHITE, applier, "You are unable to decipher the strange symbols.");
            }

            return OBJECT_METHOD_OK;
        }
    }

    if (op->direction && QUERY_FLAG(op, FLAG_SYS_OBJECT)) {
        if (applier->direction != absdir(op->direction + 4) && !(QUERY_FLAG(op, FLAG_SPLITTING) && (applier->direction == absdir(op->direction - 5) || applier->direction == absdir(op->direction + 5)))) {
            return OBJECT_METHOD_OK;
        }
    }

    /* Handle the 'say' event. */
    if (trigger_event(EVENT_SAY, applier, op, NULL, NULL, 0, 0, 0, 0)) {
        return OBJECT_METHOD_OK;
    }

    if (op->title) {
        play_sound_player_only(CONTR(applier), CMD_SOUND_EFFECT, op->title, 0, 0, 0, 0);
    }

    if (op->msg) {
        draw_info(COLOR_NAVY, applier, op->msg);
    }

    /* Add notification message, if any. */
    if (notification_msg) {
        shstr *notification_action, *notification_shortcut, *notification_delay;
        packet_struct *packet;

        notification_action = object_get_value(op, "notification_action");
        notification_shortcut = object_get_value(op, "notification_shortcut");
        notification_delay = object_get_value(op, "notification_delay");

        packet = packet_new(CLIENT_CMD_NOTIFICATION, 256, 512);

        packet_debug_data(packet, 0, "\nNotification command type");
        packet_append_uint8(packet, CMD_NOTIFICATION_TEXT);
        packet_debug_data(packet, 0, "Text");
        packet_append_string_terminated(packet, notification_msg);

        if (notification_action) {
            packet_debug_data(packet, 0, "\nNotification command type");
            packet_append_uint8(packet, CMD_NOTIFICATION_ACTION);
            packet_debug_data(packet, 0, "Action");
            packet_append_string_terminated(packet, notification_action);
        }

        if (notification_shortcut) {
            packet_debug_data(packet, 0, "\nNotification command type");
            packet_append_uint8(packet, CMD_NOTIFICATION_SHORTCUT);
            packet_debug_data(packet, 0, "Shortcut");
            packet_append_string_terminated(packet, notification_shortcut);
        }

        if (notification_delay) {
            packet_debug_data(packet, 0, "\nNotification command type");
            packet_append_uint8(packet, CMD_NOTIFICATION_DELAY);
            packet_debug_data(packet, 0, "Delay");
            packet_append_uint32(packet, atoi(notification_delay));
        }

        socket_send_packet(&CONTR(applier)->socket, packet);
    }

    return OBJECT_METHOD_OK;
}

/** @copydoc object_methods::move_on_func */
static int move_on_func(object *op, object *victim, object *originator, int state)
{
    (void) originator;
    (void) state;

    return apply_func(op, victim, 0);
}

/** @copydoc object_methods::trigger_func */
static int trigger_func(object *op, object *cause, int state)
{
    /* If the event is caused by a move-off and the object that caused
     * the event is a player, return immediately. This is done because
     * events are asynchronous, so when this code runs because of the
     * move-off, it means the player that caused the event was removed
     * from the map (temporarily, to be inserted immediately afterwards,
     * but the event is caused by the removal), and also from the linked
     * list of map's players, thus even if we proceeded as normal, the
     * activator would not get the message/music/etc data from the sign
     * object, only other nearby players. */
    if (!state && cause->type == PLAYER) {
        return OBJECT_METHOD_OK;
    }

    if (op->stats.food) {
        if (op->last_eat >= op->stats.food) {
            return OBJECT_METHOD_OK;
        }

        op->last_eat++;
    }

    if (op->title) {
        play_sound_map(op->map, CMD_SOUND_EFFECT, op->title, op->x, op->y, 0, 0);
    }

    if (op->msg) {
        draw_info_map(CHAT_TYPE_GAME, NULL, COLOR_NAVY, op->map, op->x, op->y, MAP_INFO_NORMAL, NULL, NULL, op->msg);
    }

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the sign type object methods. */
void object_type_init_sign(void)
{
    object_type_methods[SIGN].apply_func = apply_func;
    object_type_methods[SIGN].move_on_func = move_on_func;
    object_type_methods[SIGN].trigger_func = trigger_func;
}
