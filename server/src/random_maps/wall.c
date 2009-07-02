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

/*  Put in the walls and autojoin them.  */


/* given a layout and a coordinate, tell me which squares up/down/right/left
	are occupied. */

int surround_flag(char **layout,int i,int j,RMParms *RP){
  /* 1 = wall to left,
	  2 = wall to right,
	  4 = wall above
	  8 = wall below */
  int surround_index = 0;
  if((i > 0) && layout[i-1][j]!=0) surround_index |=1;
  if((i < RP->Xsize-1) && layout[i+1][j]!=0) surround_index |=2;
  if((j > 0) && layout[i][j-1]!=0) surround_index |=4;
  if((j < RP->Ysize-1) && layout[i][j+1]!=0) surround_index |=8;
  return surround_index;
}


/* like surround_flag, but only walls count.
	*/

int surround_flag2(char **layout,int i,int j,RMParms *RP){
  /* 1 = wall to left,
	  2 = wall to right,
	  4 = wall above
	  8 = wall below */
  int surround_index = 0;
  if((i > 0) && layout[i-1][j]=='#') surround_index |=1;
  if((i < RP->Xsize-1) && layout[i+1][j]=='#') surround_index |=2;
  if((j > 0) && layout[i][j-1]=='#') surround_index |=4;
  if((j < RP->Ysize-1) && layout[i][j+1]=='#') surround_index |=8;
  return surround_index;
}


/* like surround_flag, except it checks  a map, not a layout. */
int surround_flag3(mapstruct *map,int i,int j,RMParms *RP){
  /* 1 =  blocked to left,
	  2 = blocked to right,
	  4 = blocked above
	  8 = blocked below */
  int surround_index = 0;

  if((i > 0) && blocked(NULL, map,i-1,j,TERRAIN_ALL)) surround_index |=1;
  if((i < RP->Xsize-1) && blocked(NULL,map,i+1,j,TERRAIN_ALL)) surround_index |=2;
  if((j > 0) && blocked(NULL, map,i,j-1,TERRAIN_ALL)) surround_index |=4;
  if((j < RP->Ysize-1) && blocked(NULL, map,i,j+1,TERRAIN_ALL)) surround_index |=8;

  return surround_index;
}

/* like surround_flag2, except it checks  a map, not a layout. */

int surround_flag4(mapstruct *map,int i,int j,RMParms *RP){
  /* 1 =  blocked to left,
	  2 = blocked to right,
	  4 = blocked above
	  8 = blocked below */
  int surround_index = 0;

  if((i > 0) && wall_blocked(map,i-1,j)) surround_index |=1;
  if((i < RP->Xsize-1) && wall_blocked(map,i+1,j)) surround_index |=2;
  if((j > 0) && wall_blocked(map,i,j-1)) surround_index |=4;
  if((j < RP->Ysize-1) && wall_blocked(map,i,j+1)) surround_index |=8;

  return surround_index;
}

/* takes a map and a layout, and puts walls in the map (picked from
	w_style) at '#' marks. */

void make_map_walls(mapstruct *map,char **layout, char *w_style,RMParms *RP) {
  char styledirname[256];
  char stylefilepath[256];
  mapstruct *style_map=0;
  object *the_wall;


  /* get the style map */
  if(!strcmp(w_style,"none")) return;
  sprintf(styledirname,"%s","/styles/wallstyles");
  sprintf(stylefilepath,"%s/%s",styledirname,w_style);
  style_map = find_style(styledirname,w_style,-1);
  if(style_map == 0) return;

  /* fill up the map with the given floor style */
  if((the_wall=pick_random_object(style_map))!=NULL) {
	 int i,j;
	char *cp;

	 sprintf(RP->wall_name,"%s",the_wall->arch->name);
	 if ((cp=strchr(RP->wall_name,'_'))!=NULL) *cp=0;
	 for(i=0;i<RP->Xsize;i++)
		for(j=0;j<RP->Ysize;j++) {
		  if(layout[i][j]=='#') {
			 object *thiswall=pick_joined_wall(the_wall,layout,i,j,RP);
			 thiswall->x = i; thiswall->y = j;
			 SET_FLAG(thiswall,FLAG_NO_PASS); /* make SURE it's a wall */
			 wall(map,i,j);
			 insert_ob_in_map(thiswall,map,thiswall,INS_NO_MERGE | INS_NO_WALK_ON);
		  }
		}
  }


}


/* picks the right wall type for this square, to make it look nice,
	and have everything nicely joined.  It uses the layout.  */

object *pick_joined_wall(object *the_wall,char **layout,int i,int j,RMParms *RP) {
  /* 1 = wall to left,
	  2 = wall to right,
	  4 = wall above
	  8 = wall below */
  int surround_index=0;
  int l;
  char wall_name[64];
  archetype *wall_arch=0;

  strcpy(wall_name,the_wall->arch->name);

  /* conventionally, walls are named like this:
	  wallname_wallcode, where wallcode indicates
	  a joinedness, and wallname is the wall.
	  this code depends on the convention for
	  finding the right wall. */

  /* extract the wall name, which is the text up to the leading _ */
  for(l=0;l<64;l++) {
	 if(wall_name[l]=='_') {
		wall_name[l] = 0;
		break;
	 }
  }

  surround_index = surround_flag2(layout,i,j,RP);

  switch(surround_index) {
  case 0:
	 strcat(wall_name,"_0");
	 break;
  case 1:
	 strcat(wall_name,"_1_3");
	 break;
  case 2:
	 strcat(wall_name,"_1_4");
	 break;
  case 3:
	 strcat(wall_name,"_2_1_2");
	 break;
  case 4:
	 strcat(wall_name,"_1_2");
	 break;
  case 5:
	 strcat(wall_name,"_2_2_4");
	 break;
  case 6:
	 strcat(wall_name,"_2_2_1");
	 break;
  case 7:
	 strcat(wall_name,"_3_1");
	 break;
  case 8:
	 strcat(wall_name,"_1_1");
	 break;
  case 9:
	 strcat(wall_name,"_2_2_3");
	 break;
  case 10:
	 strcat(wall_name,"_2_2_2");
	 break;
  case 11:
	 strcat(wall_name,"_3_3");
	 break;
  case 12:
	 strcat(wall_name,"_2_1_1");
	 break;
  case 13:
	 strcat(wall_name,"_3_4");
	 break;
  case 14:
	 strcat(wall_name,"_3_2");
	 break;
  case 15:
	 strcat(wall_name,"_4");
	 break;
  }
  wall_arch = find_archetype(wall_name);
  if(wall_arch) return arch_to_object(wall_arch);
  else {
    nroferrors--;
    return arch_to_object(the_wall->arch);
  }


}


/* this takes a map, and changes an existing wall to match what's blocked
around it, counting only doors and walls as blocked.  If insert_flag is
1, it will go ahead and insert the wall into the map.  If not, it
will only return the wall which would belong there, and doesn't
remove anything.  It depends on the
global, previously-set variable, "wall_name"  */

object * retrofit_joined_wall(mapstruct *the_map,int i,int j,int insert_flag,RMParms *RP) {
  /* 1 = wall to left,
	  2 = wall to right,
	  4 = wall above
	  8 = wall below */
  int surround_index=0;
  int l;
  object *the_wall=0;
  object *new_wall=0;
  archetype * wall_arch=0;

  /* first find the wall */
  for(the_wall = get_map_ob(the_map,i,j);the_wall!=NULL;the_wall=the_wall->above)
    if(QUERY_FLAG(the_wall,FLAG_NO_PASS) && the_wall->type!=EXIT && the_wall->type!=TELEPORTER) break;


  /* if what we found is a door, don't remove it, set the_wall to NULL to
	  signal that later. */
  if(the_wall && (the_wall->type==DOOR || the_wall->type==LOCKED_DOOR) ) {
    the_wall=NULL;
	 /* if we're not supposed to insert a new wall where there wasn't one,
		 we've gotta leave. */
    if(insert_flag==0) return 0;
  }
  else if(the_wall==NULL) return NULL;

  /* canonicalize the wall name */
  for(l=0;l<64;l++) {
	 if(RP->wall_name[l]=='_') {
		RP->wall_name[l] = 0;
		break;
	 }
  }

  surround_index = surround_flag4(the_map,i,j,RP);
  switch(surround_index) {
  case 0:
	 strcat(RP->wall_name,"_0");
	 break;
  case 1:
	 strcat(RP->wall_name,"_1_3");
	 break;
  case 2:
	 strcat(RP->wall_name,"_1_4");
	 break;
  case 3:
	 strcat(RP->wall_name,"_2_1_2");
	 break;
  case 4:
	 strcat(RP->wall_name,"_1_2");
	 break;
  case 5:
	 strcat(RP->wall_name,"_2_2_4");
	 break;
  case 6:
	 strcat(RP->wall_name,"_2_2_1");
	 break;
  case 7:
	 strcat(RP->wall_name,"_3_1");
	 break;
  case 8:
	 strcat(RP->wall_name,"_1_1");
	 break;
  case 9:
	 strcat(RP->wall_name,"_2_2_3");
	 break;
  case 10:
	 strcat(RP->wall_name,"_2_2_2");
	 break;
  case 11:
	 strcat(RP->wall_name,"_3_3");
	 break;
  case 12:
	 strcat(RP->wall_name,"_2_1_1");
	 break;
  case 13:
	 strcat(RP->wall_name,"_3_4");
	 break;
  case 14:
	 strcat(RP->wall_name,"_3_2");
	 break;
  case 15:
	 strcat(RP->wall_name,"_4");
	 break;
  }
  wall_arch = find_archetype(RP->wall_name);
  if(wall_arch!=NULL) {
    new_wall=arch_to_object(wall_arch);
    new_wall->x = i;
    new_wall->y = j;
    if(the_wall && the_wall->map)
      remove_ob(the_wall);
    SET_FLAG(new_wall,FLAG_NO_PASS); /* make SURE it's a wall */
    insert_ob_in_map(new_wall,the_map,new_wall,INS_NO_MERGE | INS_NO_WALK_ON);
  }
  else
    nroferrors--;  /* it's OK not to find an arch. */
    /* wow... manipulating the global error counter for this action...
	 * this is really ill... i will have much fun to bring the random maps
	 * in daimonin on the start... MT-2003.
	 */
  return new_wall;

}


