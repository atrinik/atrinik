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
 * This file handles various player related functions. This includes
 * both things that operate on the player item, ::cpl structure, or
 * various commands that the player issues.
 *
 * This file does most of the handling of commands from the client to
 * server (see commands.c for server->client)
 *
 * Does most of the work for sending messages to the server
 */

#include <global.h>
#include <packet.h>

/**
 * Gender nouns.
 */
const char *gender_noun[GENDER_MAX] = {
    "neuter", "male", "female", "hermaphrodite"
};
/**
 * Subjective pronouns.
 */
const char *gender_subjective[GENDER_MAX] = {
    "it", "he", "she", "it"
};
/**
 * Subjective pronouns, with first letter in uppercase.
 */
const char *gender_subjective_upper[GENDER_MAX] = {
    "It", "He", "She", "It"
};
/**
 * Objective pronouns.
 */
const char *gender_objective[GENDER_MAX] = {
    "it", "him", "her", "it"
};
/**
 * Possessive pronouns.
 */
const char *gender_possessive[GENDER_MAX] = {
    "its", "his", "her", "its"
};
/**
 * Reflexive pronouns.
 */
const char *gender_reflexive[GENDER_MAX] = {
    "itself", "himself", "herself", "itself"
};

/**
 * Clear the player data like quickslots, inventory items, etc.
 */
void clear_player(void)
{
    objects_deinit();
    skills_deinit();
    spells_deinit();

    memset(&cpl, 0, sizeof(cpl));
    cpl.stats.Str = cpl.stats.Dex = cpl.stats.Con = cpl.stats.Int = cpl.stats.Pow = -1;
    objects_init();
    quickslots_init();
    init_player_data();
    skills_init();
    spells_init();
    WIDGET_REDRAW_ALL(PLAYER_INFO_ID);
}

/**
 * Initialize new player.
 * @param tag
 * Tag of the player.
 * @param name
 * Name of the player.
 * @param weight
 * Weight of the player.
 * @param face
 * Face ID.
 */
void new_player(tag_t tag, long weight, short face)
{
    cpl.ob->tag = tag;
    cpl.ob->weight = (float) weight / 1000;
    cpl.ob->face = face;
}

/**
 * Send apply command to server.
 * @param op
 * Object to apply.
 */
void client_send_apply(object *op)
{
    packet_struct *packet;

    packet = packet_new(SERVER_CMD_ITEM_APPLY, 8, 0);
    packet_append_uint32(packet, op->tag);

    if (op->tag == 0) {
        packet_append_uint8(packet, op->apply_action);
    }

    socket_send_packet(packet);
}

/**
 * Send examine command to server.
 * @param tag
 * Item tag.
 */
void client_send_examine(tag_t tag)
{
    packet_struct *packet;

    packet = packet_new(SERVER_CMD_ITEM_EXAMINE, 8, 0);
    packet_append_uint32(packet, tag);
    socket_send_packet(packet);
}

/**
 * Request nrof of objects of tag get moved to loc.
 * @param loc
 * Location where to move the object.
 * @param tag
 * Item tag.
 * @param nrof
 * Number of objects from tag.
 */
void client_send_move(tag_t loc, tag_t tag, uint32_t nrof)
{
    packet_struct *packet;

    packet = packet_new(SERVER_CMD_ITEM_MOVE, 32, 0);
    packet_append_uint32(packet, loc);
    packet_append_uint32(packet, tag);
    packet_append_uint32(packet, nrof);
    socket_send_packet(packet);
}

/**
 * This should be used for all 'command' processing. Other functions
 * should call this so that proper windowing will be done.
 * @param command
 * Text command.
 * @return
 * 1 if command was sent, 0 otherwise.
 */
void send_command(const char *command)
{
    packet_struct *packet;

    packet = packet_new(SERVER_CMD_PLAYER_CMD, 256, 128);
    packet_append_string_terminated(packet, command);
    socket_send_packet(packet);
}

/**
 * Initialize player data.
 */
void init_player_data(void)
{
    new_player(0, 0, 0);

    /* Focus to the main inventory widget if it's open, otherwise focus to
     * the below inventory one. */
    widgetdata *widget = widget_find(NULL, INVENTORY_ID, "main", NULL);
    if (widget != NULL) {
        if (!widget->show) {
            widget = widget_find(NULL, INVENTORY_ID, "below", NULL);
            SOFT_ASSERT(widget != NULL,
                        "Could not find the below inventory widget");
        }

        cpl.inventory_focus = widget;
        SetPriorityWidget(cpl.inventory_focus);
    }

    cpl.stats.maxsp = 1;
    cpl.stats.maxhp = 1;

    cpl.stats.speed = 1.0;

    cpl.ob->nrof = 1;
    cpl.partyname[0] = cpl.partyjoin[0] = '\0';

    /* Avoid division by 0 errors */
    cpl.stats.maxsp = 1;
    cpl.stats.maxhp = 1;
}

/**
 * Transform gender-string into its @ref GENDER_xxx "ID".
 * @param gender
 * The gender string.
 * @return
 * The gender's ID as one of @ref GENDER_xxx, or -1 if 'gender'
 * didn't match any of the existing genders.
 */
int gender_to_id(const char *gender)
{
    size_t i;

    for (i = 0; i < GENDER_MAX; i++) {
        if (strcmp(gender_noun[i], gender) == 0) {
            return i;
        }
    }

    return -1;
}

void player_draw_exp_progress(SDL_Surface *surface, int x, int y, int64_t xp, uint8_t level)
{
    SDL_Surface *texture_bubble_on, *texture_bubble_off;
    int line_width, offset, i;
    double fractional, integral;
    SDL_Rect box;

    texture_bubble_on = TEXTURE_CLIENT("exp_bubble_on");
    texture_bubble_off = TEXTURE_CLIENT("exp_bubble_off");

    line_width = texture_bubble_on->w * EXP_PROGRESS_BUBBLES;
    offset = (double) texture_bubble_on->h / 2.0 + 0.5;
    fractional = modf(((double) (xp - s_settings->level_exp[level]) / (double) (s_settings->level_exp[level + 1] - s_settings->level_exp[level]) * EXP_PROGRESS_BUBBLES), &integral);

    rectangle_create(surface, x, y, line_width + offset * 2, texture_bubble_on->h + offset * 4, "020202");

    for (i = 0; i < EXP_PROGRESS_BUBBLES; i++) {
        surface_show(surface, x + offset + i * texture_bubble_on->w, y + offset, NULL, i < (int) integral ? texture_bubble_on : texture_bubble_off);
    }

    box.x = x + offset;
    box.y = y + texture_bubble_on->h + offset * 2;
    box.w = line_width;
    box.h = offset;

    rectangle_create(surface, box.x, box.y, box.w, box.h, "404040");

    box.w = (double) box.w * fractional;
    rectangle_create(surface, box.x, box.y, box.w, box.h, "0000ff");

    box.y += offset / 4;
    box.h /= 2;
    rectangle_create(surface, box.x, box.y, box.w, box.h, "4040ff");
}
