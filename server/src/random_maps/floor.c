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
#include <random_map.h>
#include <rproto.h>

/*  make a map and layout the floor.  */

mapstruct *make_map_floor(char **layout, char *floorstyle,RMParms *RP) {
  char styledirname[256];
  char stylefilepath[256];
  mapstruct *style_map=0;
  object *the_floor;
  mapstruct *newMap =0; /* (mapstruct *) calloc(sizeof(mapstruct),1); */

  /* allocate the map */
  newMap = get_empty_map(RP->Xsize,RP->Ysize);

  /* get the style map */
  sprintf(styledirname,"%s","/styles/floorstyles");
  sprintf(stylefilepath,"%s/%s",styledirname,floorstyle);
  style_map = find_style(styledirname,floorstyle,-1);
  if(style_map == 0) return newMap;

  /* fill up the map with the given floor style */
  if((the_floor=pick_random_object(style_map))!=NULL) {
	 int i,j;
	 for(i=0;i<RP->Xsize;i++)
		for(j=0;j<RP->Ysize;j++) {
		  object *thisfloor=arch_to_object(the_floor->arch);
		  thisfloor->x = i; thisfloor->y = j;
		  insert_ob_in_map(thisfloor,newMap,thisfloor,INS_NO_MERGE | INS_NO_WALK_ON);
		}
  }
  return newMap;
}
