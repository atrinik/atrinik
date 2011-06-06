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
 * The functions in this file are purely meant to generate information
 * in differently formatted output, mainly about monsters. */

#include <global.h>

/** Dump to standard out the abilities of all monsters. */
void dump_abilities()
{
	archetype *at;

	for (at = first_archetype; at; at = at->next)
	{
		const char *ch, *gen_name = "";
		archetype *gen;

		if (!QUERY_FLAG(&at->clone, FLAG_MONSTER))
		{
			continue;
		}

		/* Get rid of e.g. multiple black puddings */
		if (QUERY_FLAG(&at->clone, FLAG_CHANGING))
		{
			continue;
		}

		for (gen = first_archetype; gen; gen = gen->next)
		{
			if (gen->clone.other_arch && gen->clone.other_arch == at)
			{
				gen_name = gen->name;
				break;
			}
		}

		ch = describe_item(&at->clone);
		LOG(llevInfo, "%-16s|%6"FMT64"|%4d|%3d|%s|%s|%s\n", at->clone.name, at->clone.stats.exp, at->clone.stats.hp, at->clone.stats.ac, ch, at->name, gen_name);
	}
}

/** Like dump_abilities(), but with an alternative way of output. */
void print_monsters()
{
	archetype *at;
	object *op;
	int i;

	LOG(llevInfo, "               |     |   |    |    |                                  attacks/ resistances                                                                                              |\n");
	LOG(llevInfo, "monster        | hp  |dam| ac | wc | phy mag fir ele cld cfs acd drn wmg ght poi slo par tim fea cnc dep dth chs csp gpw hwd bln int lst sla cle pie net son dem psi |  exp   | new exp |\n");
	LOG(llevInfo, "-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");

	for (at = first_archetype; at != NULL; at = at->next)
	{
		op = arch_to_object(at);

		if (QUERY_FLAG(op, FLAG_MONSTER))
		{
			LOG(llevInfo, "%-15s|%5d|%3d|%4d|%4d|", op->arch->name, op->stats.maxhp, op->stats.dam, op->stats.ac, op->stats.wc);

			for (i = 0; i < NROFATTACKS; i++)
			{
				LOG(llevInfo, "%4d", op->attack[i]);
			}

			LOG(llevInfo, " |\n               |     |   |    |    |");

			for (i = 0; i < NROFATTACKS; i++)
			{
				LOG(llevInfo, "%4d", op->protection[i]);
			}

			LOG(llevInfo, " |%8"FMT64"|\n", op->stats.exp);
		}
	}
}
