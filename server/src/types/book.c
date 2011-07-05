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

/**
 * Apply a book.
 *
 * Sends the book contents to the player object using
 * Send_With_Handling(), but only if the book does not have an APPLY
 * plugin trigger.
 *
 * A book can only be read if you're high enough level compared to the
 * book's level. You will also get experience for reading books, but only
 * if the book has not been read before.
 * @param op The player object applying the book.
 * @param tmp The book. */
void apply_book(object *op, object *tmp)
{
	int lev_diff;
	SockList sl;
	unsigned char sock_buf[MAXSOCKBUF];

	if (QUERY_FLAG(op, FLAG_BLIND) && !QUERY_FLAG(op,FLAG_WIZ))
	{
		new_draw_info(0, COLOR_WHITE, op, "You are unable to read while blind.");
		return;
	}

	if (tmp->msg == NULL)
	{
		new_draw_info_format(0, COLOR_WHITE, op, "You open the %s and find it empty.", tmp->name);
		return;
	}

	/* Need a literacy skill to read stuff! */
	if (!change_skill(op, SK_LITERACY))
	{
		new_draw_info(0, COLOR_WHITE, op, "You are unable to decipher the strange symbols.");
		return;
	}

	lev_diff = tmp->level - (SK_level(op) + BOOK_LEVEL_DIFF + book_level_mod[op->stats.Int]);

	if (!QUERY_FLAG(op, FLAG_WIZ) && lev_diff > 0)
	{
		if (lev_diff < 2)
		{
			new_draw_info(0, COLOR_WHITE, op, "This book is just barely beyond your comprehension.");
		}
		else if (lev_diff < 3)
		{
			new_draw_info(0, COLOR_WHITE, op, "This book is slightly beyond your comprehension.");
		}
		else if (lev_diff < 5)
		{
			new_draw_info(0, COLOR_WHITE, op, "This book is beyond your comprehension.");
		}
		else if (lev_diff < 8)
		{
			new_draw_info(0, COLOR_WHITE, op, "This book is quite a bit beyond your comprehension.");
		}
		else if (lev_diff < 15)
		{
			new_draw_info(0, COLOR_WHITE, op, "This book is way beyond your comprehension.");
		}
		else
		{
			new_draw_info(0, COLOR_WHITE, op, "This book is totally beyond your comprehension.");
		}

		return;
	}

	new_draw_info_format(0, COLOR_WHITE, op, "You open the %s and start reading.", tmp->name);
	CONTR(op)->stat_books_read++;

	sl.buf = sock_buf;
	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_BOOK);

	SockList_AddStringUnterm(&sl, "<book>");
	SockList_AddStringUnterm(&sl, query_base_name(tmp, NULL));
	SockList_AddStringUnterm(&sl, "</book>");
	SockList_AddString(&sl, (char *) tmp->msg);

	Send_With_Handling(&CONTR(op)->socket, &sl);

	/* Gain xp from reading but only if not read before. */
	if (!QUERY_FLAG(tmp, FLAG_NO_SKILL_IDENT))
	{
		sint64 exp_gain, old_exp;

		CONTR(op)->stat_unique_books_read++;

		/* Store original exp value. We want to keep the experience cap
		 * from calc_skill_exp() below, so we temporarily adjust the exp
		 * of the book, instead of adjusting the return value. */
		old_exp = tmp->stats.exp;
		/* Adjust the experience based on player's wisdom. */
		tmp->stats.exp = (sint64) ((double) tmp->stats.exp * book_exp_mod[op->stats.Wis]);

		if (!QUERY_FLAG(tmp, FLAG_IDENTIFIED))
		{
			/* Because they just identified it too. */
			tmp->stats.exp *= 1.5f;
			SET_FLAG(tmp, FLAG_IDENTIFIED);

			/* If in a container, update how it looks. */
			if (tmp->env)
			{
				esrv_update_item(UPD_FLAGS | UPD_NAME, op, tmp);
			}
			else
			{
				CONTR(op)->socket.update_tile = 0;
			}
		}

		exp_gain = calc_skill_exp(op, tmp, -1);
		add_exp(op, exp_gain, op->chosen_skill->stats.sp, 0);

		/* So no more exp gained from this book. */
		SET_FLAG(tmp, FLAG_NO_SKILL_IDENT);
		/* Restore old experience value. */
		tmp->stats.exp = old_exp;
	}
}
