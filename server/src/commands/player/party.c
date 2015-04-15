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
 * Implements the /party command.
 *
 * @author Alex Tokar */

#include <global.h>
#include <packet.h>

/** @copydoc command_func */
void command_party(object *op, const char *command, char *params)
{
    char buf[MAX_BUF];

    if (!params) {
        if (!CONTR(op)->party) {
            draw_info(COLOR_WHITE, op, "You are not a member of any party.");
            draw_info(COLOR_WHITE, op, "For help try: /party help");
        } else {
            draw_info_format(COLOR_WHITE, op, "You are a member of party %s (leader: %s).", CONTR(op)->party->name, CONTR(op)->party->leader);
        }
    } else if (!strcmp(params, "help")) {
        draw_info(COLOR_WHITE, op, "To form a party type: /party form <partyname>");
        draw_info(COLOR_WHITE, op, "To join a party type: /party join <partyname>");
        draw_info(COLOR_WHITE, op, "If the party has a password, it will prompt you for it.");
        draw_info(COLOR_WHITE, op, "For a list of current parties type: /party list");
        draw_info(COLOR_WHITE, op, "To leave a party type: /party leave");
        draw_info(COLOR_WHITE, op, "To change a password for a party type: /party password <password>");
        draw_info(COLOR_WHITE, op, "There is a 8 character max for password.");
        draw_info(COLOR_WHITE, op, "To talk to party members type: /party say <msg> or /gsay <msg>");
        draw_info(COLOR_WHITE, op, "To see who is in your party: /party who");
        draw_info(COLOR_WHITE, op, "To change the party's looting mode: /party loot mode");
        draw_info(COLOR_WHITE, op, "To kick another player from your party: /party kick <name>");
        draw_info(COLOR_WHITE, op, "To change party leader: /party leader <name>");
    } else if (!strncmp(params, "say ", 4)) {
        if (!CONTR(op)->party) {
            draw_info(COLOR_WHITE, op, "You are not a member of any party.");
            return;
        }

        params = player_sanitize_input(params + 4);

        if (!params) {
            return;
        }

        snprintf(buf, sizeof(buf), "[%s] %s says: %s", CONTR(op)->party->name, op->name, params);
        send_party_message(CONTR(op)->party, buf, PARTY_MESSAGE_CHAT, op, NULL);
        logger_print(LOG(CHAT), "[PARTY] [%s] [%s] %s", op->name, CONTR(op)->party->name, params);
    } else if (!strcmp(params, "leave")) {
        if (!CONTR(op)->party) {
            draw_info(COLOR_WHITE, op, "You are not a member of any party.");
            return;
        }

        draw_info_format(COLOR_WHITE, op, "You leave party %s.", CONTR(op)->party->name);
        snprintf(buf, sizeof(buf), "%s leaves party %s.", op->name, CONTR(op)->party->name);
        send_party_message(CONTR(op)->party, buf, PARTY_MESSAGE_STATUS, op, op);

        remove_party_member(CONTR(op)->party, op);
    } else if (!strncmp(params, "password ", 9)) {
        if (!CONTR(op)->party) {
            draw_info(COLOR_RED, op, "You are not a member of any party.");
            return;
        }

        if (CONTR(op)->party->leader != op->name) {
            draw_info(COLOR_RED, op, "Only the party's leader can change the password.");
            return;
        }

        strncpy(CONTR(op)->party->passwd, params + 9, sizeof(CONTR(op)->party->passwd) - 1);
        snprintf(buf, sizeof(buf), "The password for party %s changed to '%s'.", CONTR(op)->party->name, CONTR(op)->party->passwd);
        send_party_message(CONTR(op)->party, buf, PARTY_MESSAGE_STATUS, op, NULL);
    } else if (!strncmp(params, "form ", 5)) {
        params = player_sanitize_input(params + 5);

        if (!params) {
            draw_info(COLOR_RED, op, "Invalid party name to form.");
            return;
        }

        if (CONTR(op)->party) {
            draw_info(COLOR_RED, op, "You must leave your current party before forming a new one.");
            return;
        }

        if (find_party(params)) {
            draw_info_format(COLOR_WHITE, op, "The party %s already exists, pick another name.", params);
            return;
        }

        form_party(op, params);
    } else if (!strncmp(params, "loot", 4)) {
        size_t i;

        if (!CONTR(op)->party) {
            draw_info(COLOR_RED, op, "You are not a member of any party.");
            return;
        }

        params = player_sanitize_input(params + 4);

        if (!params) {
            draw_info_format(COLOR_WHITE, op, "Current looting mode: [green]%s[/green].", party_loot_modes[CONTR(op)->party->loot]);
            return;
        }

        if (CONTR(op)->party->leader != op->name) {
            draw_info(COLOR_RED, op, "Only the party's leader can change the looting mode.");
            return;
        }

        for (i = 0; i < PARTY_LOOT_MAX; i++) {
            if (!strcmp(params, party_loot_modes[i])) {
                CONTR(op)->party->loot = i;
                snprintf(buf, sizeof(buf), "Party looting mode changed to '%s'.", party_loot_modes[i]);
                send_party_message(CONTR(op)->party, buf, PARTY_MESSAGE_STATUS, op, NULL);
                return;
            }
        }

        draw_info(COLOR_WHITE, op, "Invalid looting mode. Valid modes are:");

        for (i = 0; i < PARTY_LOOT_MAX; i++) {
            draw_info_format(COLOR_WHITE, op, "[green]%s[/green]: %s.", party_loot_modes[i], party_loot_modes_help[i]);
        }
    } else if (!strncmp(params, "kick", 4)) {
        objectlink *ol;

        if (!CONTR(op)->party) {
            draw_info(COLOR_RED, op, "You are not a member of any party.");
            return;
        }

        if (CONTR(op)->party->leader != op->name) {
            draw_info(COLOR_RED, op, "Only the party's leader can kick other members of the party.");
            return;
        }

        params = player_sanitize_input(params + 4);

        if (!params) {
            draw_info(COLOR_WHITE, op, "Whom do you want to kick from the party?");
            return;
        }

        if (!strncasecmp(op->name, params, MAX_NAME)) {
            draw_info(COLOR_RED, op, "You cannot kick yourself.");
            return;
        }

        for (ol = CONTR(op)->party->members; ol; ol = ol->next) {
            if (!strncasecmp(ol->objlink.ob->name, params, MAX_NAME)) {
                remove_party_member(CONTR(op)->party, ol->objlink.ob);
                snprintf(buf, sizeof(buf), "%s has been kicked from the party.", ol->objlink.ob->name);
                send_party_message(CONTR(op)->party, buf, PARTY_MESSAGE_STATUS, op, NULL);
                draw_info_format(COLOR_RED, ol->objlink.ob, "You have been kicked from the party '%s'.", CONTR(op)->party->name);
                return;
            }
        }

        draw_info(COLOR_RED, op, "There's no player with that name in your party.");
    } else if (!strncmp(params, "leader ", 7)) {
        player *pl;

        if (!CONTR(op)->party) {
            draw_info(COLOR_RED, op, "You are not a member of any party.");
            return;
        }

        if (CONTR(op)->party->leader != op->name) {
            draw_info(COLOR_RED, op, "Only the party's leader can change the leader.");
            return;
        }

        pl = find_player(params + 7);

        if (!pl) {
            draw_info(COLOR_RED, op, "No such player.");
            return;
        }

        if (pl->ob == op) {
            draw_info(COLOR_RED, op, "You are already the party leader.");
            return;
        }

        if (!pl->party || pl->party != CONTR(op)->party) {
            draw_info(COLOR_RED, op, "That player is not a member of your party.");
            return;
        }

        FREE_AND_ADD_REF_HASH(pl->party->leader, pl->ob->name);
        draw_info_format(COLOR_WHITE, pl->ob, "You are the new leader of party %s!", pl->party->name);
        draw_info_format(COLOR_GREEN, op, "%s is the new leader of your party.", pl->ob->name);
    } else {
        packet_struct *packet;
        party_struct *party;

        if (!strcmp(params, "list")) {
            packet = packet_new(CLIENT_CMD_PARTY, 128, 256);
            packet_debug_data(packet, 0, "Party command type");
            packet_append_uint8(packet, CMD_PARTY_LIST);

            for (party = first_party; party; party = party->next) {
                packet_debug_data(packet, 0, "\nParty name");
                packet_append_string_terminated(packet, party->name);
                packet_debug_data(packet, 0, "Leader");
                packet_append_string_terminated(packet, party->leader);
            }

            socket_send_packet(&CONTR(op)->socket, packet);
        } else if (!strcmp(params, "who")) {
            objectlink *ol;

            if (!CONTR(op)->party) {
                draw_info(COLOR_RED, op, "You are not a member of any party.");
                return;
            }

            packet = packet_new(CLIENT_CMD_PARTY, 128, 256);
            packet_debug_data(packet, 0, "Party command type");
            packet_append_uint8(packet, CMD_PARTY_WHO);

            for (ol = CONTR(op)->party->members; ol; ol = ol->next) {
                packet_debug_data(packet, 0, "\nMember name");
                packet_append_string_terminated(packet, ol->objlink.ob->name);
                packet_debug_data(packet, 0, "Health");
                packet_append_uint8(packet, MAX(1, MIN(
                        (double) ol->objlink.ob->stats.hp /
                        ol->objlink.ob->stats.maxhp * 100.0f, 100)));
                packet_debug_data(packet, 0, "Mana");
                packet_append_uint8(packet, MAX(1, MIN(
                        (double) ol->objlink.ob->stats.sp /
                        ol->objlink.ob->stats.maxsp * 100.0f, 100)));
            }

            socket_send_packet(&CONTR(op)->socket, packet);
        } else if (!strncmp(params, "join ", 5)) {
            char *cps[2];

            if (CONTR(op)->party) {
                draw_info(COLOR_WHITE, op, "You must leave your current party before joining another.");
                return;
            }

            string_split(params + 5, cps, arraysize(cps), '\t');

            if (!cps[0] || !(party = find_party(cps[0]))) {
                draw_info(COLOR_WHITE, op, "No such party.");
                return;
            }

            /* If party password is not set or they've typed correct password...
             * */
            if (party->passwd[0] == '\0' || (cps[1] && strcmp(party->passwd, cps[1]) == 0)) {
                add_party_member(party, op);
                CONTR(op)->stat_joined_party++;
                draw_info_format(COLOR_GREEN, op, "You have joined party: %s.", party->name);
                snprintf(buf, sizeof(buf), "%s joined party %s.", op->name, party->name);
                send_party_message(party, buf, PARTY_MESSAGE_STATUS, op, op);
                return;
            } else if (cps[1]) {
                /* Party password was typed but it wasn't correct. */
                draw_info(COLOR_RED, op, "Incorrect party password.");
                return;
            } else {
                /* Otherwise ask them to type the password */
                draw_info(COLOR_YELLOW, op, "That party requires a password. Type it now, or press ESC to cancel joining.");
                packet = packet_new(CLIENT_CMD_PARTY, 64, 64);
                packet_debug_data(packet, 0, "Party command type");
                packet_append_uint8(packet, CMD_PARTY_PASSWORD);
                packet_debug_data(packet, 0, "Party name");
                packet_append_string_terminated(packet, party->name);
                socket_send_packet(&CONTR(op)->socket, packet);
            }
        }
    }
}
