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
* the Free Software Foundation; either version 3 of the License, or     *
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

#include <global.h>
#ifndef __CEXTRACT__
#include <sproto.h>
#endif

/**
 * @file egoitem.c */

/* GROS: I put this here, because no other file seemed quite good. */
object *create_artifact(object *op, char *artifactname)
{
    artifactlist *al;
    artifact *art;
    al = find_artifactlist(op->type);

    if (al == NULL)
		return NULL;

	for (art = al->items; art != NULL; art = art->next)
	{
		if (!strcmp(art->name, artifactname))
			give_artifact_abilities(op, art);
	}

	return NULL;
}


/**
 * This function handles the application of power crystals.
 * Power crystals, when applied, either suck power from the applier,
 * if he's at full spellpoints, or gives him power, if it's got
 * spellpoins stored.
 * @param op Object applying the power crystal
 * @param crystal The crystal object
 * @return Always returns 1. */
int apply_power_crystal(object *op, object *crystal)
{
  	int available_power;
  	int power_space;
  	int power_grab;

	available_power = op->stats.sp - op->stats.maxsp;
	power_space = crystal->stats.maxsp - crystal->stats.sp;
	power_grab = 0;

	if (available_power >= 0 && power_space > 0)
		power_grab = (int)MIN ((float)power_space, ((float)0.5 * (float)op->stats.sp));

	if (available_power < 0 && crystal->stats.sp > 0)
		power_grab = -MIN(-available_power, crystal->stats.sp);

	op->stats.sp -= power_grab;
	crystal->stats.sp += power_grab;
	crystal->speed = (float)crystal->stats.sp / (float)crystal->stats.maxsp;
	update_ob_speed(crystal);

	if (op->type == PLAYER)
		esrv_update_item(UPD_ANIMSPEED, op, crystal);

	return 1;
}
