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
 * This file handles party related code. */

#include <global.h>
#include <packet.h>

/**
 * String representations of the party looting modes. */
const char *const party_loot_modes[PARTY_LOOT_MAX] = {
    "normal", "leader", "owner", "random", "split"
};

/**
 * Explanation of the party modes. */
const char *const party_loot_modes_help[PARTY_LOOT_MAX] = {
    "everyone in the party is able to loot the corpse",
    "only the party leader can loot the corpse",
    "only the corpse owner can loot the corpse; standard behavior when outside of a party",
    "loot is randomly split between party members when the corpse is opened",
    "loot is evenly split between party members when the corpse is opened"
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
    pool_party = mempool_create("parties", 25, sizeof(party_struct),
            MEMPOOL_ALLOW_FREEING, NULL, NULL, NULL, NULL);
}

/**
 * Deinitialize the party API. */
void party_deinit(void)
{
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
    packet_debug_data(packet, 0, "Party command type");
    packet_append_uint8(packet, CMD_PARTY_JOIN);
    packet_debug_data(packet, 0, "Party name");
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
            free_objectlink_simple(ol);
            break;
        }
    }

    SOFT_ASSERT(ol != NULL, "Could not find player %s in party members!",
            object_get_str(op));

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
    } else if (op->name == party->leader) {
        /* Otherwise choose a new leader, if the old one left. */

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
    party_struct *party = mempool_get(pool_party);

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
 * Randomly split corpse's loot between party's members.
 * @param pl Player that opened the corpse.
 * @param corpse The corpse. */
static void party_loot_random(object *pl, object *corpse)
{
    int count = 0, pl_id;
    party_struct *party;
    objectlink *ol;
    object *tmp, *tmp_next;

    party = CONTR(pl)->party;

    for (ol = party->members; ol; ol = ol->next) {
        if (on_same_map(ol->objlink.ob, pl)) {
            count++;
        }
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
 * Evenly split corpse's loot between party's members.
 * @param pl Player that opened the corpse.
 * @param corpse The corpse. */
static void party_loot_split(object *pl, object *corpse)
{
    party_struct *party;
    objectlink *ol, *ol_loot, *ol_next;
    uint32_t count;
    int64_t value;
    object *tmp, *next;

    party = CONTR(pl)->party;
    ol_loot = NULL;
    count = 0;
    value = 0;

    for (ol = party->members; ol != NULL; ol = ol->next) {
        if (on_same_map(ol->objlink.ob, pl)) {
            if (party->loot_idx == count) {
                ol_loot = ol;
            }

            count++;
        }
    }

    if (party->loot_idx >= count) {
        party->loot_idx = 0;
        ol_loot = party->members;
    }

    /* Sanity check. */
    if (ol_loot == NULL) {
        return;
    }

    for (tmp = corpse->inv; tmp; tmp = next) {
        next = tmp->below;

        /* Skip unpickable objects. */
        if (!can_pick(pl, tmp)) {
            continue;
        }

        if (tmp->type == MONEY) {
            value += tmp->value * tmp->nrof;
            object_remove(tmp, 0);
            continue;
        }

        for (ol = ol_loot; ol != NULL; ol = ol_next) {
            ol_next = ol->next;

            if (ol_next == NULL) {
                ol_next = party->members;
            }

            if (on_same_map(ol->objlink.ob, pl) && player_can_carry(ol->objlink.ob, WEIGHT_NROF(tmp, tmp->nrof))) {
                draw_info_format(COLOR_BLUE, ol->objlink.ob, "You receive the %s.", query_name(tmp, NULL));
                object_remove(tmp, 0);
                insert_ob_in_ob(tmp, ol->objlink.ob);
                break;
            }

            if (ol == ol_loot) {
                break;
            }
        }

        party->loot_idx++;
        ol_loot = ol_loot->next;

        if (party->loot_idx >= count) {
            party->loot_idx = 0;
            ol_loot = party->members;
        }
    }

    if (value > 0) {
        int64_t value_split;
        uint32_t num;

        for (num = 0, ol = party->members; ol; ol = ol->next) {
            if (on_same_map(ol->objlink.ob, pl)) {
                value_split = value / count;

                if (num == 0) {
                    value_split += value % count;
                }

                draw_info_format(COLOR_BLUE, ol->objlink.ob, "You receive %s.", cost_string_from_value(value_split));
                insert_coins(ol->objlink.ob, value_split);

                num++;
            }
        }
    }
}

/**
 * Do any special handling after a party corpse has been opened.
 * @param pl Player who opened the corpse.
 * @param corpse The corpse. */
void party_handle_corpse(object *pl, object *corpse)
{
    object *tmp, *next;

    /* Sanity check. */
    if (!CONTR(pl)->party) {
        return;
    }

    /* Reclaim arrows. */
    for (tmp = corpse->inv; tmp; tmp = next) {
        next = tmp->below;

        if (tmp->type == ARROW && OBJECT_VALID(tmp->attacked_by, tmp->attacked_by_count) &&
                tmp->attacked_by->type == PLAYER && CONTR(tmp->attacked_by)->party == CONTR(pl)->party &&
                on_same_map(tmp->attacked_by, pl)) {
            if (can_pick(tmp->attacked_by, tmp) && player_can_carry(tmp->attacked_by, WEIGHT_NROF(tmp, tmp->nrof))) {
                pick_up(tmp->attacked_by, tmp, 0);
            }
        }
    }

    switch (CONTR(pl)->party->loot) {
    case PARTY_LOOT_RANDOM:
        party_loot_random(pl, corpse);
        break;

    case PARTY_LOOT_SPLIT:
        party_loot_split(pl, corpse);
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
        } else if (flag == PARTY_MESSAGE_CHAT) {
            draw_info_type(CHAT_TYPE_PARTY, op->name, COLOR_YELLOW, ol->objlink.ob, msg);
        }
    }
}

/**
 * Remove a party.
 * @param party The party to remove. */
void remove_party(party_struct *party)
{
    party_struct *tmp, *prev = NULL;

    while (party->members != NULL) {
        CONTR(party->members->objlink.ob)->party = NULL;
        objectlink_unlink(&party->members, NULL, party->members);
        free_objectlink_simple(party->members);
    }

    for (tmp = first_party; tmp; prev = tmp, tmp = tmp->next) {
        if (tmp == party) {
            if (!prev) {
                first_party = tmp->next;
            } else {
                prev->next = tmp->next;
            }

            break;
        }
    }

    FREE_AND_CLEAR_HASH(party->name);
    FREE_AND_CLEAR_HASH(party->leader);
    mempool_return(pool_party, party);
}

/**
 * Update player's hp/sp/etc in the party widget of all party members.
 * @param pl Player. */
void party_update_who(player *pl)
{
    uint8_t hp, sp;

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
