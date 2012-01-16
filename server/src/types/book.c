/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
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
 * Handles code related to @ref BOOK "books". */

#include <global.h>

/**
 * Maximum amount of difference in levels between the book's level and
 * the player's literacy skill. */
#define BOOK_LEVEL_DIFF 12

/**
 * Affects @ref BOOK_LEVEL_DIFF, depending on the player's intelligence
 * stat. If the player is intelligent enough, they may be able to read
 * higher level books; if their intelligence is too low, the maximum level
 * books they can read will decrease. */
static int book_level_mod[MAX_STAT + 1] =
{
	-9,
	-8, -7, -6, -5, -4,
	-4, -3, -2, -2, -1,
	0, 0, 0, 0, 0,
	1, 1, 2, 2, 3,
	3, 3, 4, 4, 5,
	6, 7, 8, 9, 10
};

/**
 * The higher your wisdom, the more you are able to make use of the
 * knowledge you read from books. Thus, you get more experience by
 * reading books the more wisdom you have, and less experience if you
 * have unnaturally low wisdom. */
static double book_exp_mod[MAX_STAT + 1] =
{
	-3.00f,
	-2.00f, -1.90f, -1.80f, -1.70f, -1.60f,
	-1.50f, -1.40f, -1.30f, -1.20f, -1.10f,
	1.00f, 1.00f, 1.00f, 1.00f, 1.00f,
	1.05f, 1.10f, 1.15f, 1.20f, 1.30f,
	1.35f, 1.40f, 1.50f, 1.55f, 1.60f,
	1.70f, 1.75f, 1.85f, 1.90f, 2.00f
};

/** @copydoc object_methods::apply_func */
static int apply_func(object *op, object *applier, int aflags)
{
	int lev_diff;
	packet_struct *packet;

	(void) aflags;

	if (applier->type != PLAYER)
	{
		return OBJECT_METHOD_OK;
	}

	if (QUERY_FLAG(applier, FLAG_BLIND))
	{
		draw_info(COLOR_WHITE, applier, "You are unable to read while blind.");
		return OBJECT_METHOD_OK;
	}

	if (op->msg == NULL)
	{
		draw_info_format(COLOR_WHITE, applier, "You open the %s and find it empty.", op->name);
		return OBJECT_METHOD_OK;
	}

	/* Need a literacy skill to read stuff! */
	if (!change_skill(applier, SK_LITERACY))
	{
		draw_info(COLOR_WHITE, applier, "You are unable to decipher the strange symbols.");
		return OBJECT_METHOD_OK;
	}

	lev_diff = op->level - (SK_level(applier) + BOOK_LEVEL_DIFF + book_level_mod[applier->stats.Int]);

	if (lev_diff > 0)
	{
		if (lev_diff < 2)
		{
			draw_info(COLOR_WHITE, applier, "This book is just barely beyond your comprehension.");
		}
		else if (lev_diff < 3)
		{
			draw_info(COLOR_WHITE, applier, "This book is slightly beyond your comprehension.");
		}
		else if (lev_diff < 5)
		{
			draw_info(COLOR_WHITE, applier, "This book is beyond your comprehension.");
		}
		else if (lev_diff < 8)
		{
			draw_info(COLOR_WHITE, applier, "This book is quite a bit beyond your comprehension.");
		}
		else if (lev_diff < 15)
		{
			draw_info(COLOR_WHITE, applier, "This book is way beyond your comprehension.");
		}
		else
		{
			draw_info(COLOR_WHITE, applier, "This book is totally beyond your comprehension.");
		}

		return OBJECT_METHOD_OK;
	}

	draw_info_format(COLOR_WHITE, applier, "You open the %s and start reading.", op->name);
	CONTR(applier)->stat_books_read++;

	packet = packet_new(CLIENT_CMD_BOOK, 512, 512);
	packet_append_string(packet, "<book>");
	packet_append_string(packet, query_base_name(op, applier));
	packet_append_string(packet, "</book>");
	packet_append_string_terminated(packet, op->msg);
	socket_send_packet(&CONTR(applier)->socket, packet);

	/* Gain xp from reading but only if not read before. */
	if (!QUERY_FLAG(op, FLAG_NO_SKILL_IDENT))
	{
		sint64 exp_gain, old_exp;

		CONTR(applier)->stat_unique_books_read++;

		/* Store original exp value. We want to keep the experience cap
		 * from calc_skill_exp() below, so we temporarily adjust the exp
		 * of the book, instead of adjusting the return value. */
		old_exp = op->stats.exp;
		/* Adjust the experience based on player's wisdom. */
		op->stats.exp = (sint64) ((double) op->stats.exp * book_exp_mod[applier->stats.Wis]);

		if (!QUERY_FLAG(op, FLAG_IDENTIFIED))
		{
			/* Because they just identified it too. */
			op->stats.exp *= 1.5f;
			identify(op);
		}

		exp_gain = calc_skill_exp(applier, op, -1);
		add_exp(applier, exp_gain, applier->chosen_skill->stats.sp, 0);

		/* So no more exp gained from this book. */
		SET_FLAG(op, FLAG_NO_SKILL_IDENT);
		/* Restore old experience value. */
		op->stats.exp = old_exp;
	}

	return OBJECT_METHOD_OK;
}

/**
 * Initialize the book type object methods. */
void object_type_init_book(void)
{
	object_type_methods[BOOK].apply_func = apply_func;
}
