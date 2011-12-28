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
 * Inscription skill code. */

#include <global.h>
#include <book.h>

/**
 * Write message into a book object.
 * @param op Player writing the message.
 * @param msg The message to write.
 * @param marked Book to write into.
 * @return Experience gained. */
static int inscribe_book(object *op, const char *msg, object *marked)
{
	char buf[BOOK_BUF];

	if (!msg || *msg == '\0')
	{
		draw_info(COLOR_WHITE, op, "No message to write. Usage:\n/use_skill inscription <message>");
		return 0;
	}

	/* Prevent cheating. */
	if (strcasestr(msg, "endmsg"))
	{
		draw_info(COLOR_WHITE, op, "Trying to cheat now are we?");
		LOG(llevInfo, "%s tried to write a bogus message using inscription skill.\n", op->name);
		return 0;
	}

	/* Check if we can fit the message into the book. */
	if (book_overflow(marked->msg, msg, sizeof(buf)))
	{
		draw_info_format(COLOR_WHITE, op, "Your message won't fit in the %s.", query_short_name(marked, op));
		return 0;
	}

	if (marked->msg)
	{
		snprintf(buf, sizeof(buf), "%s\n%s\n", marked->msg, msg);
	}
	else
	{
		snprintf(buf, sizeof(buf), "%s\n", msg);
	}

	FREE_AND_COPY_HASH(marked->msg, buf);
	draw_info_format(COLOR_WHITE, op, "You write in the %s.", query_short_name(marked, op));
	CONTR(op)->stat_books_inscribed++;

	return strlen(msg);
}

/**
 * Execute the inscription skill.
 * @param op Player.
 * @param params String option for the skill [the message to write].
 * @return Experience gained. */
int skill_inscription(object *op, const char *params)
{
	object *skill_item, *marked;

	/* Can't write anything without being able to read... */
	if (!find_skill(op, SK_LITERACY))
	{
		draw_info(COLOR_WHITE, op, "You must learn to read before you can write!");
		return 0;
	}

	skill_item = CONTR(op)->equipment[PLAYER_EQUIP_SKILL_ITEM];

	if (!skill_item)
	{
		draw_info(COLOR_WHITE, op, "You need to apply a writing pen to use this skill.");
		return 0;
	}

	if (skill_item->stats.sp != SK_INSCRIPTION)
	{
		draw_info_format(COLOR_WHITE, op, "The %s cannot be used with this skill.", query_short_name(skill_item, NULL));
		return 0;
	}

	if (QUERY_FLAG(op, FLAG_BLIND) && !QUERY_FLAG(op, FLAG_WIZ))
	{
		draw_info(COLOR_WHITE, op, "You are unable to write while blind.");
		return 0;
	}

	marked = find_marked_object(op);

	if (!marked)
	{
		draw_info(COLOR_WHITE, op, "You don't have any marked item to write on.");
		return 0;
	}

	if (marked->type == BOOK)
	{
		return inscribe_book(op, params, marked);
	}

	draw_info_format(COLOR_WHITE, op, "You can't write on the %s.", query_short_name(marked, NULL));
	return 0;
}
