/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
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
 * Handles code for @ref POISON "poison" objects. */

#include <global.h>
#include <sproto.h>

/**
 * Apply poisoned object.
 * @param op The object applying this.
 * @param tmp The poison object. */
void apply_poison(object *op, object *tmp)
{
	if (op->type == PLAYER)
	{
		play_sound_player_only(CONTR(op), SOUND_DRINK_POISON,SOUND_NORMAL, 0, 0);
		new_draw_info(NDI_UNIQUE, 0, op, "Yech! That tasted poisonous!");
		strcpy(CONTR(op)->killer, "poisonous food");
	}

	if (tmp->stats.dam)
	{
		/* internal damage part will take care about our poison */
		hit_player(op, tmp->stats.dam, tmp, AT_POISON);
	}

	op->stats.food -= op->stats.food / 4;
	decrease_ob(tmp);
}
