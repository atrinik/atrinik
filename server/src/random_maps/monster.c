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

#include <global.h>
#include <random_map.h>
#include <rproto.h>

/* some monsters are multisquare, and these guys require special
	handling. */

void	 insert_multisquare_ob_in_map(object *new_obj,mapstruct *map)
{
	int x,y;
	archetype *at;
	object *old_seg;
	object *head;
	/* first insert the head */
	insert_ob_in_map(new_obj,map,new_obj,INS_NO_MERGE | INS_NO_WALK_ON);

	x = new_obj->x;
	y = new_obj->y;
	old_seg=new_obj;
	head = new_obj;
	for (at=new_obj->arch->more;at!=NULL;at=at->more)
	{
		object *new_seg;
		new_seg = arch_to_object(at);
		new_seg->x = x + at->clone.x;
		new_seg->y = y + at->clone.y;
		new_seg->map = old_seg->map;
		insert_ob_in_map(new_seg,new_seg->map, new_seg,INS_NO_MERGE | INS_NO_WALK_ON);
		new_seg->head = head;
		old_seg->more = new_seg;
		old_seg = new_seg;
	}
	old_seg->more = NULL;


}


/*  place some monsters into the map. */
void place_monsters(mapstruct *map, char *monsterstyle, int difficulty,RMParms *RP)
{
	char styledirname[256];
	mapstruct *style_map=0;
	long unsigned int total_experience;  /* used for matching difficulty */
	int failed_placements;
	long unsigned int exp_per_sq;
	int number_monsters=0;

	sprintf(styledirname,"%s","/styles/monsterstyles");
	style_map = find_style(styledirname,monsterstyle,difficulty);
	if (style_map == 0) return;

	/* fill up the map with random monsters from the monster style*/

	total_experience = 0;
	failed_placements = 0;
	exp_per_sq = 0;
	while (exp_per_sq <= level_exp(difficulty,1.0) && failed_placements < 100
			&& number_monsters < (RP->Xsize * RP->Ysize)/8)
	{
		object *this_monster=pick_random_object(style_map);
		int x,y,freeindex;
		if (this_monster == NULL) return; /* no monster?? */
		x = RANDOM() % RP->Xsize;
		y = RANDOM() % RP->Ysize;
		freeindex = find_first_free_spot(this_monster->arch,map,x,y);
		if (freeindex!=-1)
		{
			object *new_monster = arch_to_object(this_monster->arch);
			x += freearr_x[freeindex];
			y += freearr_y[freeindex];
			copy_object_with_inv(this_monster,new_monster);
			new_monster->x = x;
			new_monster->y = y;
			insert_multisquare_ob_in_map(new_monster,map);
			total_experience+= this_monster->stats.exp;
			number_monsters++;
			RP->total_map_hp+=new_monster->stats.hp;  /*  a global count */
		}
		else
		{
			failed_placements++;
		}
		exp_per_sq= (long unsigned int)((double)1000*total_experience)/(MAP_WIDTH(map)*MAP_HEIGHT(map)+1);
	}
}
