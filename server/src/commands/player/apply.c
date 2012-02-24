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
 * Implements the /apply command.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Search the inventory of 'pl' for what matches best with params.
 * @param pl Player object.
 * @param params Parameters string.
 * @return Best match, or NULL if no match. */
static object *find_best_object_match(object *pl, const char *params)
{
	object *tmp, *best;
	int match_val, tmpmatch;

	if (!params || *params == '\0')
	{
		return NULL;
	}

	best = NULL;
	match_val = 0;

	for (tmp = pl->inv; tmp; tmp = tmp->below)
	{
		if (IS_INVISIBLE(tmp, pl))
		{
			continue;
		}

		tmpmatch = item_matched_string(pl, tmp, params);

		if (tmpmatch > match_val)
		{
			match_val = tmpmatch;
			best = tmp;
		}
	}

	return best;
}

/** @copydoc command_func */
void command_apply(object *op, const char *command, char *params)
{
	enum apply_flag aflag;
	size_t pos;
	char word[MAX_BUF];
	object *ob;

	if (!params)
	{
		player_apply_below(op);
		return;
	}

	aflag = 0;
	pos = 0;

	if (string_get_word(params, &pos, ' ', word, sizeof(word)))
	{
		if (strcmp(word, "-a") == 0)
		{
			aflag = AP_APPLY;
		}
		else
		{
			pos = 0;
		}
	}

	if (string_get_word(params, &pos, ' ', word, sizeof(word)))
	{
		if (strcmp(word, "-u") == 0)
		{
			aflag = AP_UNAPPLY;
		}
		else
		{
			pos = 0;
		}
	}

	ob = find_best_object_match(op, params + pos);

	if (ob)
	{
		player_apply(op, ob, aflag, 0);
	}
	else
	{
		draw_info(COLOR_WHITE, op, "Could not find any matching item.");
	}
}
