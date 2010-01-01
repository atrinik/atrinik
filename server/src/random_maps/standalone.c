/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
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

#define LO_NEWFILE 2

/* the main routine for making a standalone version. */

#include <time.h>
#include <stdio.h>
#include <global.h>
#include <random_map.h>
#include <rproto.h>

int main(int argc, char *argv[])
{
	char InFileName[1024],OutFileName[1024];
	mapstruct *newMap;
	RMParms rp;
	FILE *fp;

	if (argc < 3)
	{
		printf("\nUsage:  %s inputfile outputfile\n",argv[0]);
		exit(0);
	}
	strcpy(InFileName,argv[1]);
	strcpy(OutFileName,argv[2]);

	init_globals();
	init_library();
	init_archetypes();
	init_artifacts();
	init_formulae();
	init_readable();

	init_gods();
	memset(&rp, 0, sizeof(RMParms));
	rp.Xsize=-1;
	rp.Ysize=-1;
	if ((fp=fopen(InFileName, "r"))==NULL)
	{
		fprintf(stderr,"\nError: can not open %s\n", InFileName);
		exit(1);
	}
	load_parameters(fp, LO_NEWFILE, &rp);
	fclose(fp);
	newMap = generate_random_map(OutFileName, &rp);
	new_save_map(newMap,1);
	exit(0);
}

void set_map_timeout() {}   /* doesn't need to do anything */

#include <global.h>


/* some plagarized code from apply.c--I needed just these two functions
without all the rest of the junk, so.... */
int auto_apply (object *op)
{
	object *tmp = NULL;
	int i;

	switch (op->type)
	{
		case SHOP_FLOOR:
			if (op->randomitems==NULL) return 0;
			do
			{
				i=10; /* let's give it 10 tries */
				while ((tmp=generate_treasure(op->randomitems,op->map == NULL ?  op->stats.exp: op->map->difficulty, op->randomitems->artifact_chance))==NULL&&--i);
				if (tmp==NULL)
					return 0;
				if (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED))
				{
					free_object(tmp);
					tmp = NULL;
				}
			}
			while (!tmp);

			tmp->x=op->x,tmp->y=op->y;
			SET_FLAG(tmp,FLAG_UNPAID);
			insert_ob_in_map(tmp,op->map,NULL,0);
			CLEAR_FLAG(op,FLAG_AUTO_APPLY);
			identify(tmp);
			break;

		case TREASURE:
			create_treasure(op->randomitems, op, GT_ENVIRONMENT,
							op->map == NULL ?  op->stats.exp: op->map->difficulty,T_STYLE_UNSET, ART_CHANCE_UNSET, 0,NULL);
			remove_ob(op);
			break;
	}

	return tmp ? 1 : 0;
}

