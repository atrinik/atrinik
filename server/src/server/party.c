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
 * This file handles party related code. */

#include <global.h>

/**
 * String representations of the party looting modes. */
const char *const party_loot_modes[PARTY_LOOT_MAX] =
{
    "normal", "leader", "random"
};

/**
 * Explanation of the party modes. */
const char *const party_loot_modes_help[PARTY_LOOT_MAX] =
{
    "everyone is able to loot the corpse",
    "only the leader can loot the corpse",
    "loot is randomly split between party members when the corpse is opened"
};

/** The party list. */
party_struct *first_party = NULL;

/**
 * Party memory pool. */
static mempool_struct *pool_party;

/**
 * Initialize the party API. */
void party_init(void)
{
    pool_party = mempool_create("parties", 25, sizeof(party_struct), 0, NULL, NULL, NULL, NULL);
}

/**
 * Deinitialize the party API. */
void party_deinit(void)
{
    mempool_free(pool_party);
}

/**
 * Add a player to party's member list.
 * @param party Party to add the player to.
 * @param op The player to add. */
void add_party_member(party_struct *party, object *op)
{
    objectlink *ol;
    packet_struct *packet;

    ol = get_objectlink();
    /* Add the player to the party's linked list of members. */
    ol->objlink.ob = op;
    objectlink_link(&party->members, NULL, NULL, party->members, ol);
    /* And set up player's pointer to the party. */
    CONTR(op)->party = party;

    packet = packet_new(CLIENT_CMD_PARTY, 64, 64);
    packet_append_uint8(packet, CMD_PARTY_JOIN);
    packet_append_string_terminated(packet, party->name);
    socket_send_packet(&CONTR(op)->socket, packet);

    CONTR(op)->last_party_hp = 0;
    CONTR(op)->last_party_sp = 0;
}

/**
 * Remove player from party's member list.
 * @param party Party to remove the player from.
 * @param op The player to remove. */
void remove_party_member(party_struct *party, object *op)
{
    objectlink *ol;
    packet_struct *packet;

    /* Go through the party members, and remove the player that is
     * leaving. */
    for (ol = party->members; ol; ol = ol->next) {
        if (ol->objlink.ob == op) {
            objectlink_unlink(&party->members, NULL, ol);
            break;
        }
    }

    if (party->members) {
        packet = packet_new(CLIENT_CMD_PARTY, 64, 64);
        packet_append_uint8(packet, CMD_PARTY_REMOVE_MEMBER);
        packet_append_string_terminated(packet, op->name);

        for (ol = party->members; ol; ol = ol->next) {
            socket_send_packet(&CONTR(ol->objlink.ob)->socket, packet_dup(packet));
        }

        packet_free(packet);
    }

    /* If no members left, remove the party. */
    if (!party->members) {
        remove_party(CONTR(op)->party);
    }
    /* Otherwise choose a new leader, if the old one left. */
    else if (op->name == party->leader) {
        FREE_AND_ADD_REF_HASH(party->leader, party->members->objlink.ob->name);
        draw_info_format(COLOR_WHITE, party->members->objlink.ob, "You are the new leader of party %s!", party->name);
    }

    packet = packet_new(CLIENT_CMD_PARTY, 4, 0);
    packet_append_uint8(packet, CMD_PARTY_LEAVE);
    socket_send_packet(&CONTR(op)->socket, packet);

    CONTR(op)->party = NULL;
}

/**
 * Initialize a new party structure.
 * @param name Name of the new party.
 * @return The initialized party structure. */
static party_struct *make_party(const char *name)
{
    party_struct *party = (party_struct *) get_poolchunk(pool_party);

    memset(party, 0, sizeof(party_struct));
    FREE_AND_COPY_HASH(party->name, name);

    party->next = first_party;
    first_party = party;

    return party;
}

/**
 * Form a new party.
 * @param op Object forming the party.
 * @param name Name of the party. */
void form_party(object *op, const char *name)
{
    party_struct *party = make_party(name);

    add_party_member(party, op);
    draw_info_format(COLOR_WHITE, op, "You have formed party: %s", name);
    FREE_AND_ADD_REF_HASH(party->leader, op->name);
    CONTR(op)->stat_formed_party++;
}

/**
 * Find a party by name.
 * @param name Party name to find.
 * @return Party if found, NULL otherwise. */
party_struct *find_party(const char *name)
{
    party_struct *tmp;

    for (tmp = first_party; tmp; tmp = tmp->next) {
        if (!strcmp(tmp->name, name)) {
            return tmp;
        }
    }

    return NULL;
}

/**
 * Randomly split corpse's loot between party's members.
 * @param pl Player that opened the corpse.
 * @param corpse The corpse. */
static void party_loot_random(object *pl, object *corpse)
{
    int count = 0, pl_id;
    party_struct *party = CONTR(pl)->party;
    objectlink *ol;
    object *tmp, *tmp_next;

    for (ol = party->members; ol; ol = ol->next) {
        if (on_same_map(ol->objlink.ob, pl)) {
            count++;
        }
    }

    if (count == 1) {
        return;
    }

    for (tmp = corpse->inv; tmp; tmp = tmp_next) {
        int num = 1;

        tmp_next = tmp->below;

        /* Skip unpickable objects. */
        if (!can_pick(pl, tmp)) {
            continue;
        }

        pl_id = rndm(1, count);

        for (ol = party->members; ol; ol = ol->next) {
            if (on_same_map(ol->objlink.ob, pl)) {
                if (num == pl_id) {
                    if (player_can_carry(ol->objlink.ob, WEIGHT_NROF(tmp, tmp->nrof))) {
                        draw_info_format(COLOR_BLUE, ol->objlink.ob, "You receive the %s.", query_name(tmp, NULL));
                        object_remove(tmp, 0);
                        insert_ob_in_ob(tmp, ol->objlink.ob);
                    }

                    break;
                }

                num++;
            }
        }
    }
}

/**
 * Check if player can open a party corpse.
 * @param pl Player trying to open the corpse.
 * @param corpse The corpse.
 * @return 1 if we can open the corpse, 0 otherwise. */
int party_can_open_corpse(object *pl, object *corpse)
{
    /* Check if the player is in the same party. */
    if (!CONTR(pl)->party || corpse->slaying != CONTR(pl)->party->name) {
        draw_info(COLOR_WHITE, pl, "It's not your party's bounty.");
        return 0;
    }

    switch (CONTR(pl)->party->loot) {
        /* Normal: anyone can access it. */
        case PARTY_LOOT_NORMAL:
        default:
            return 1;

        /* Only leader can access it. */
        case PARTY_LOOT_LEADER:

            if (pl->name != CONTR(pl)->party->leader) {
                draw_info(COLOR_WHITE, pl, "You're not the party's leader.");
                return 0;
            }

            return 1;
    }
}

/**
 * Do any special handling after a party corpse has been opened.
 * @param pl Player who opened the corpse.
 * @param corpse The corpse. */
void party_handle_corpse(object *pl, object *corpse)
{
    /* Sanity check. */
    if (!CONTR(pl)->party) {
        return;
    }

    switch (CONTR(pl)->party->loot) {
        case PARTY_LOOT_RANDOM:
            party_loot_random(pl, corpse);
            break;
    }
}

/**
 * Send a message to party.
 * @param party Party to send the message to.
 * @param msg Message to send.
 * @param flag One of @ref PARTY_MESSAGE_xxx "party message flags".
 * @param op Player sending the message. If not NULL, this player will
 * not receive the message. */
void send_party_message(party_struct *party, const char *msg, int flag, object *op, object *except)
{
    objectlink *ol;

    for (ol = party->members; ol; ol = ol->next) {
        if (ol->objlink.ob == except) {
            continue;
        }

        if (flag == PARTY_MESSAGE_STATUS) {
            draw_info(COLOR_YELLOW, ol->objlink.ob, msg);
        }
        else if (flag == PARTY_MESSAGE_CHAT) {
            draw_info_type(CHAT_TYPE_PARTY, op->name, COLOR_YELLOW, ol->objlink.ob, msg);
        }
    }
}

/**
 * Remove a party.
 * @param party The party to remove. */
void remove_party(party_struct *party)
{
    objectlink *ol;
    party_struct *tmp, *prev = NULL;

    for (ol = party->members; ol; ol = ol->next) {
        CONTR(ol->objlink.ob)->party = NULL;
        objectlink_unlink(&party->members, NULL, ol);
        return_poolchunk(ol, pool_objectlink);
    }

    for (tmp = first_party; tmp; prev = tmp, tmp = tmp->next) {
        if (tmp == party) {
            if (!prev) {
                first_party = tmp->next;
            }
            else {
                prev->next = tmp->next;
            }

            break;
        }
    }

    FREE_AND_CLEAR_HASH(party->name);
    FREE_AND_CLEAR_HASH(party->leader);
    return_poolchunk(party, pool_party);
}

/**
 * Update player's hp/sp/etc in the party widget of all party members.
 * @param pl Player. */
void party_update_who(player *pl)
{
    uint8 hp, sp;

    if (!pl->party) {
        return;
    }

    hp = MAX(1, MIN((double) pl->ob->stats.hp / pl->ob->stats.maxhp * 100.0f, 100));
    sp = MAX(1, MIN((double) pl->ob->stats.sp / pl->ob->stats.maxsp * 100.0f, 100));

    if (hp != pl->last_party_hp || sp != pl->last_party_sp) {
        packet_struct *packet;
        objectlink *ol;

        packet = packet_new(CLIENT_CMD_PARTY, 64, 64);
        packet_append_uint8(packet, CMD_PARTY_UPDATE);
        packet_append_string_terminated(packet, pl->ob->name);
        packet_append_uint8(packet, hp);
        packet_append_uint8(packet, sp);

        for (ol = pl->party->members; ol; ol = ol->next) {
            socket_send_packet(&CONTR(ol->objlink.ob)->socket, packet_dup(packet));
        }

        packet_free(packet);
    }
}
