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
 * Handles player statistics.
 *
 * Many statistics are currently cached in the player's structure as
 * uint64 for performance reasons, as it's not very efficient to send a
 * statistic update each time the player plays for another second, or
 * casts a spell, or regenerates some health/mana. Thus, such
 * statistics are only updated in statistics_player_logout(), which is
 * called when the player logs out. */

#include <global.h>

/** File descriptor used for sending datagrams. */
static int fd = -1;
/** Destination address. */
static struct sockaddr_in insock;

/**
 * Initialize statistics; sets up the datagram file descriptor, etc. */
void statistics_init(void)
{
	struct protoent *protoent;

	protoent = getprotobyname("udp");

	if (!protoent)
	{
		return;
	}

	fd = socket(PF_INET, SOCK_DGRAM, protoent->p_proto);

	if (fd == -1)
	{
		return;
	}

	insock.sin_family = AF_INET;
	insock.sin_port = htons((unsigned short) 13324);
	insock.sin_addr.s_addr = inet_addr("127.0.0.1");
}

/**
 * Update a particular statistic of a player.
 * @param type The statistic type - a string that will be recognized by
 * the statistics server and stored appropriately.
 * @param op The player.
 * @param i Integer value to store. If 0, will not do any updating.
 * @param buf Optional string buffer to send. */
void statistic_update(const char *type, object *op, sint64 i, const char *buf)
{
	packet_struct *packet;

	if (!i || fd == -1)
	{
		return;
	}

	packet = packet_new(0, 256, 256);
	packet_append_string_terminated(packet, type);
	packet_append_string_terminated(packet, op->name);
	packet_append_sint64(packet, i);

	if (buf)
	{
		packet_append_string_terminated(packet, buf);
	}

	sendto(fd, (void *) packet->data, packet->len, 0, (struct sockaddr *) &insock, sizeof(insock));
	packet_free(packet);
}

/**
 * Handle player logging out, in order to update cached statistics from
 * the player's data structure.
 * @param pl The player. */
void statistics_player_logout(player *pl)
{
	statistic_update("deaths", pl->ob, pl->stat_deaths, NULL);
	statistic_update("kills_mob", pl->ob, pl->stat_kills_mob, NULL);
	statistic_update("kills_pvp", pl->ob, pl->stat_kills_pvp, NULL);
	statistic_update("damage_taken", pl->ob, pl->stat_damage_taken, NULL);
	statistic_update("damage_dealt", pl->ob, pl->stat_damage_dealt, NULL);
	statistic_update("hp_regen", pl->ob, pl->stat_hp_regen, NULL);
	statistic_update("sp_regen", pl->ob, pl->stat_sp_regen, NULL);
	statistic_update("food_consumed", pl->ob, pl->stat_food_consumed, NULL);
	statistic_update("food_num_consumed", pl->ob, pl->stat_food_num_consumed, NULL);
	statistic_update("damage_healed", pl->ob, pl->stat_damage_healed, NULL);
	statistic_update("damage_healed_other", pl->ob, pl->stat_damage_healed_other, NULL);
	statistic_update("damage_heal_received", pl->ob, pl->stat_damage_heal_received, NULL);
	statistic_update("steps_taken", pl->ob, pl->stat_steps_taken, NULL);
	statistic_update("spells_cast", pl->ob, pl->stat_spells_cast, NULL);
	statistic_update("time_played", pl->ob, pl->stat_time_played, NULL);
	statistic_update("time_afk", pl->ob, pl->stat_time_afk, NULL);
	statistic_update("arrows_fired", pl->ob, pl->stat_arrows_fired, NULL);
	statistic_update("missiles_thrown", pl->ob, pl->stat_missiles_thrown, NULL);
	statistic_update("books_read", pl->ob, pl->stat_books_read, NULL);
	statistic_update("potions_used", pl->ob, pl->stat_potions_used, NULL);
	statistic_update("scrolls_used", pl->ob, pl->stat_scrolls_used, NULL);
	statistic_update("exp_gained", pl->ob, pl->stat_exp_gained, NULL);
	statistic_update("items_dropped", pl->ob, pl->stat_items_dropped, NULL);
	statistic_update("items_picked", pl->ob, pl->stat_items_picked, NULL);
	statistic_update("corpses_searched", pl->ob, pl->stat_corpses_searched, NULL);
	statistic_update("afk_used", pl->ob, pl->stat_afk_used, NULL);
	statistic_update("formed_party", pl->ob, pl->stat_formed_party, NULL);
	statistic_update("joined_party", pl->ob, pl->stat_joined_party, NULL);
	statistic_update("renamed_items", pl->ob, pl->stat_renamed_items, NULL);
	statistic_update("emotes_used", pl->ob, pl->stat_emotes_used, NULL);
	statistic_update("books_inscribed", pl->ob, pl->stat_books_inscribed, NULL);
}
