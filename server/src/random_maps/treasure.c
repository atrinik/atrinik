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

/*  placing treasure in maps, where appropriate.  */



#include <global.h>
#include <random_map.h>
#include <rproto.h>

/* some defines for various options which can be set. */

#define CONCENTRATED 1 /* all the treasure is at the C's for onions. */
#define HIDDEN 2   /* doors to treasure are hidden. */
#define KEYREQUIRED 4 /* chest has a key, which is placed randomly in the map. */
#define DOORED 8   /* treasure has doors around it. */
#define TRAPPED 16 /* trap dropped in same location as chest. */
#define SPARSE 32  /* 1/2 as much treasure as default */
#define RICH 64   /* 2x as much treasure as default */
#define FILLED 128  /* Fill/tile the entire map with treasure */
#define LAST_OPTION 64  /* set this to the last real option, for random */

#define NO_PASS_DOORS 0
#define PASS_DOORS 1


/* returns true if square x,y has P_NO_PASS set, which is true for walls
	and doors but not monsters. */
int wall_blocked(mapstruct *m, int x, int y)
{
	int r;
	if (!(m=out_of_map(m,&x,&y)))
		return 1;
	r = GET_MAP_FLAGS(m,x,y) & (P_NO_PASS|P_PASS_THRU );
	return r;
}

/* place treasures in the map, given the
map,         (required)
layout,      (required)
treasure style    (may be empty or NULL, or "none" to cause no treasure.)
treasureoptions   (may be 0 for random choices or positive)
*/

void place_treasure(mapstruct *map,char **layout, char *treasure_style,int treasureoptions,RMParms *RP)
{
	char styledirname[256];
	char stylefilepath[256];
	mapstruct *style_map=0;
	int num_treasures;

	/* bail out if treasure isn't wanted. */
	if (treasure_style) if (!strcmp(treasure_style,"none")) return;
	if (treasureoptions<=0) treasureoptions=RANDOM() % (2*LAST_OPTION);

	/* filter out the mutually exclusive options */
	if ((treasureoptions & RICH) &&(treasureoptions &SPARSE))
	{
		if (RANDOM()%2) treasureoptions -=1;
		else treasureoptions-=2;
	}

	/* pick the number of treasures */
	if (treasureoptions & SPARSE)
		num_treasures = BC_RANDOM(RP->total_map_hp/600+RP->difficulty/2+1);
	else if (treasureoptions & RICH)
		num_treasures = BC_RANDOM(RP->total_map_hp/150+2*RP->difficulty+1);
	else num_treasures = BC_RANDOM(RP->total_map_hp/300+RP->difficulty+1);

	if (num_treasures <= 0 ) return;

	/* get the style map */
	sprintf(styledirname,"%s","/styles/treasurestyles");
	sprintf(stylefilepath,"%s/%s",styledirname,treasure_style);
	style_map = find_style(styledirname,treasure_style,-1);

	/* all the treasure at one spot in the map. */
	if (treasureoptions & CONCENTRATED)
	{

		/* map_layout_style global, and is previously set */
		switch (RP->map_layout_style)
		{
			case ONION_LAYOUT:
			case SPIRAL_LAYOUT:
			case SQUARE_SPIRAL_LAYOUT:
			{
				int i,j;
				/* search the onion for C's or '>', and put treasure there. */
				for (i=0;i<RP->Xsize;i++)
				{
					for (j=0;j<RP->Ysize;j++)
					{
						if (layout[i][j]=='C' || layout[i][j]=='>')
						{
							int tdiv = RP->symmetry_used;
							object **doorlist;
							object *chest;
							if (tdiv==3) tdiv = 2; /* this symmetry uses a divisor of 2*/
							/* don't put a chest on an exit. */
							chest=place_chest(treasureoptions,i,j,map,style_map,num_treasures/tdiv,RP);
							if (!chest) continue; /* if no chest was placed NEXT */
							if (treasureoptions & (DOORED|HIDDEN))
							{
								doorlist=find_doors_in_room(map,i,j,RP);
								lock_and_hide_doors(doorlist,map,treasureoptions,RP);
								free(doorlist);
							}
						}
					}
				}
				break;
			}
			default:
			{
				int i,j,tries;
				object *chest;
				object **doorlist;
				i=j=-1;
				tries=0;
				while (i==-1&&tries<100)
				{
					i = RANDOM()%(RP->Xsize-2)+1;
					j = RANDOM()%(RP->Ysize-2)+1;
					find_enclosed_spot(map,&i,&j,RP);
					if (wall_blocked(map,i,j)) i=-1;
					tries++;
				}
				chest=place_chest(treasureoptions,i,j,map,style_map,num_treasures,RP);
				if (!chest) return;
				i = chest->x;
				j = chest->y;
				if (treasureoptions &( DOORED|HIDDEN))
				{
					doorlist=surround_by_doors(map,layout,i,j,treasureoptions);
					lock_and_hide_doors(doorlist,map,treasureoptions,RP);
					free(doorlist);
				}
			}
		}
	}
	else   /* DIFFUSE treasure layout */
	{
		int ti,i,j;
		for (ti=0;ti<num_treasures;ti++)
		{
			i = RANDOM()%(RP->Xsize-2)+1;
			j = RANDOM()%(RP->Ysize-2)+1;
			place_chest(treasureoptions,i,j,map,style_map,1,RP);
		}
	}
}



/* put a chest into the map, near x and y, with the treasure style
	determined (may be null, or may be a treasure list from lib/treasures,
	if the global variable "treasurestyle" is set to that treasure list's name */

object * place_chest(int treasureoptions,int x, int y,mapstruct *map, mapstruct *style_map,int n_treasures,RMParms *RP)
{
	object *the_chest;
	int i,xl,yl;

	/* first, find a place to put the chest. */
	i = find_first_free_spot(find_archetype("chest"),map,x,y);
	xl = x + freearr_x[i];
	yl = y +  freearr_y[i];

	/* if the placement is blocked, return a fail. */
	if (wall_blocked(map,xl,yl)) return 0;


	/* use the nicer container-chests for multiple treasures...  Allows locking. */
	if (n_treasures > 1)
		the_chest = get_archetype("chest");  /* was "chest_2" */
	else
		the_chest = get_archetype("chest");


	/* put the treasures in the chest. */
	/*  if(style_map) { */
	if (0)   /* don't use treasure style maps for now!  */
	{
		int ti;
		/* if treasurestyle lists a treasure list, use it. */
		treasurelist *tlist=find_treasurelist(RP->treasurestyle);
		if (tlist!=NULL)
			for (ti=0;ti<n_treasures;ti++)   /* use the treasure list */
			{
				object *new_treasure=pick_random_object(style_map);
				insert_ob_in_ob(arch_to_object(new_treasure->arch),the_chest);
			}
		else   /* use the style map */
		{
			the_chest->randomitems=tlist;
			the_chest->stats.hp = n_treasures;
		}
	}
	else   /* neither style_map no treasure list given */
	{
		treasurelist *tlist=find_treasurelist("chest");
		the_chest->randomitems=tlist;
		the_chest->stats.hp = n_treasures;
	}

	/* stick a trap in the chest if required  */
	if (treasureoptions & TRAPPED)
	{
		mapstruct *trap_map=find_style("/styles/trapstyles","traps",-1);
		object *the_trap;
		if (trap_map)
		{
			the_trap= pick_random_object(trap_map);
			the_trap->stats.Cha = 10+RP->difficulty;
			the_trap->level = BC_RANDOM((3*RP->difficulty)/2);
			if (the_trap)
			{
				object *new_trap;
				new_trap = arch_to_object(the_trap->arch);
				copy_object(new_trap,the_trap);
				new_trap->x = x;
				new_trap->y = y;
				insert_ob_in_ob(new_trap,the_chest);
			}
		}
	}

	/* set the chest lock code, and call the keyplacer routine with
	   the lockcode.  It's not worth bothering to lock the chest if
	   there's only 1 treasure....*/

	if ((treasureoptions & KEYREQUIRED)&&n_treasures>1)
	{
		char keybuf[256];
		sprintf(keybuf,"%d",(int)RANDOM());
		FREE_AND_COPY_HASH(the_chest->slaying, keybuf);
		keyplace(map,x,y,keybuf,PASS_DOORS,1,RP);
	}

	/* actually place the chest. */
	the_chest->x = xl;
	the_chest->y = yl;
	insert_ob_in_map(the_chest,map,NULL,0);
	return the_chest;
}


/* finds the closest monster and returns him, regardless of doors
	or walls */
object *find_closest_monster(mapstruct *map,int x,int y)
{
	int i, lx,ly;
	mapstruct *mt;

	for (i=0;i<SIZEOFFREE;i++)
	{
		lx=x+freearr_x[i];
		ly=y+freearr_y[i];

		if (!(mt=out_of_map(map,&lx,&ly)))
			continue;
		/* don't bother searching this square unless the map says life exists.*/
		/* remember that player and player pets/golems are now not alive but is_player */
		if (GET_MAP_FLAGS(mt,lx,ly) & P_IS_ALIVE)
		{
			object *the_monster=get_map_ob(mt,lx,ly);
			for (;the_monster!=NULL&&(!QUERY_FLAG(the_monster,FLAG_MONSTER));the_monster=the_monster->above)
				;
			if (the_monster && QUERY_FLAG(the_monster,FLAG_MONSTER))
				return the_monster;
		}
	}
	return NULL;
}



/* places keys in the map, preferably in something alive.
	keycode is the key's code,
	door_flag is either PASS_DOORS or NO_PASS_DOORS.
	  NO_PASS_DOORS won't cross doors or walls to keyplace, PASS_DOORS will.
	if n_keys is 1, it will place 1 key.  if n_keys >1, it will place 2-4 keys:
	it will place 2-4 keys regardless of what nkeys is provided nkeys > 1.

	The idea is that you call keyplace on x,y where a door is, and it'll make
	sure a key is placed on both sides of the door.
*/

int keyplace(mapstruct *map,int x,int y,char *keycode,int door_flag,int n_keys,RMParms *RP)
{
	int i,j;
	int kx,ky;
	object *the_keymaster; /* the monster that gets the key. */
	object *the_key;

	/* get a key and set its keycode */
	the_key = get_archetype("key2");
	FREE_AND_COPY_HASH(the_key->slaying, keycode);


	if (door_flag==PASS_DOORS)
	{
		int tries=0;
		the_keymaster=NULL;
		while (tries<5&&the_keymaster==NULL)
		{
			i = (RANDOM()%(RP->Xsize-2))+1;
			j = (RANDOM()%(RP->Ysize-2))+1;
			tries++;
			the_keymaster=find_closest_monster(map,i,j);
		}
		/* if we don't find a good keymaster, drop the key on the ground. */
		if (the_keymaster==NULL)
		{
			int freeindex = find_first_free_spot(the_key->arch,map,i,j);
			kx = i + freearr_x[freeindex];
			ky = j + freearr_y[freeindex];
		}
	}
	else    /* NO_PASS_DOORS --we have to work harder.*/
	{
		/* don't try to keyplace if we're sitting on a blocked square and
		   NO_PASS_DOORS is set. */
		if (n_keys==1)
		{
			if (wall_blocked(map,x,y)) return 0;
			the_keymaster=find_monster_in_room(map,x,y,RP);
			if (the_keymaster==NULL) /* if fail, find a spot to drop the key. */
				find_spot_in_room(map,x,y,&kx,&ky,RP);
		}
		else
		{
			int sum=0; /* count how many keys we actually place */
			/* I'm lazy, so just try to place in all 4 directions. */
			sum +=keyplace(map,x+1,y,keycode,NO_PASS_DOORS,1,RP);
			sum +=keyplace(map,x,y+1,keycode,NO_PASS_DOORS,1,RP);
			sum +=keyplace(map,x-1,y,keycode,NO_PASS_DOORS,1,RP);
			sum +=keyplace(map,x,y-1,keycode,NO_PASS_DOORS,1,RP);
			if (sum < 2) /* we might have made a disconnected map-place more keys. */
			{  /* diagnoally this time. */
				keyplace(map,x+1,y+1,keycode,NO_PASS_DOORS,1,RP);
				keyplace(map,x+1,y-1,keycode,NO_PASS_DOORS,1,RP);
				keyplace(map,x-1,y+1,keycode,NO_PASS_DOORS,1,RP);
				keyplace(map,x-1,y-1,keycode,NO_PASS_DOORS,1,RP);
			}
			return 1;
		}
	}

	if (the_keymaster==NULL)
	{
		the_key->x = kx;
		the_key->y = ky;
		insert_ob_in_map(the_key,map,NULL,INS_NO_MERGE | INS_NO_WALK_ON);
		return 1;
	}

	insert_ob_in_ob(the_key,the_keymaster);
	return 1;
}



/* both find_monster_in_room routines need to have access to this. */

object *theMonsterToFind;

/* a recursive routine which will return a monster, eventually,if there is one.
   it does a check-off on the layout, converting 0's to 1's */

object *find_monster_in_room_recursive(char **layout, mapstruct *map, int x, int y, RMParms *RP)
{
	int i,j;
	/* if we've found a monster already, leave */
	if (theMonsterToFind!=NULL) return theMonsterToFind;

	/* bounds check x and y */
	/*  if(!(x >= 0 && y >= 0 && x < RP->Xsize && y < RP->Ysize)) return theMonsterToFind;*/
	if (!(map=out_of_map(map,&x,&y)))
		return theMonsterToFind;

	/* if the square is blocked or searched already, leave */
	if (layout[x][y]!=0) return theMonsterToFind; /* might be NULL, that's fine.*/

	/* check the current square for a monster.  If there is one,
	   set theMonsterToFind and return it. */
	layout[x][y]=1;
	if (GET_MAP_FLAGS(map,x,y) & P_IS_ALIVE)
	{
		object *the_monster = get_map_ob(map,x,y);
		/* check off this point */
		for (;the_monster!=NULL&&(!QUERY_FLAG(the_monster,FLAG_MONSTER));the_monster=the_monster->above);
		if (the_monster && QUERY_FLAG(the_monster,FLAG_MONSTER))
		{
			theMonsterToFind=the_monster;
			return theMonsterToFind;
		}
	}

	/* now search all the 8 squares around recursively for a monster,in random order */
	for (i=RANDOM()%8,j=0; j<8 && theMonsterToFind==NULL;i++,j++)
	{
		theMonsterToFind = find_monster_in_room_recursive(layout,map,x+freearr_x[i%8+1],y+freearr_y[i%8+1],RP);
		if (theMonsterToFind!=NULL) return theMonsterToFind;
	}
	return theMonsterToFind;
}


/* sets up some data structures:  the _recursive form does the
   real work.  */

object *find_monster_in_room(mapstruct *map,int x,int y,RMParms *RP)
{
	char **layout2;
	int i,j;
	theMonsterToFind=0;
	layout2 = (char **) calloc(sizeof(char *),RP->Xsize);
	/* allocate and copy the layout, converting C to 0. */
	for (i=0;i<RP->Xsize;i++)
	{
		layout2[i]=(char *)calloc(sizeof(char),RP->Ysize);
		for (j=0;j<RP->Ysize;j++)
		{
			if (wall_blocked(map,i,j)) layout2[i][j] = '#';
		}
	}
	theMonsterToFind = find_monster_in_room_recursive(layout2,map,x,y,RP);

	/* deallocate the temp. layout */
	for (i=0;i<RP->Xsize;i++)
	{
		free(layout2[i]);
	}
	free(layout2);

	return theMonsterToFind;
}




/* a datastructure needed by find_spot_in_room and find_spot_in_room_recursive */
int *room_free_spots_x;
int *room_free_spots_y;
int number_of_free_spots_in_room;

/* the workhorse routine, which finds the free spots in a room:
a datastructure of free points is set up, and a position chosen from
that datastructure. */

void find_spot_in_room_recursive(char **layout,int x,int y,RMParms *RP)
{
	int i,j;

	/* bounds check x and y */
	if (!(x >= 0 && y >= 0 && x < RP->Xsize && y < RP->Ysize)) return;

	/* if the square is blocked or searched already, leave */
	if (layout[x][y]!=0) return;

	/* set the current square as checked, and add it to the list.
	   set theMonsterToFind and return it. */
	/* check off this point */
	layout[x][y]=1;
	room_free_spots_x[number_of_free_spots_in_room]=x;
	room_free_spots_y[number_of_free_spots_in_room]=y;
	number_of_free_spots_in_room++;
	/* now search all the 8 squares around recursively for free spots,in random order */
	for (i=RANDOM()%8,j=0; j<8 && theMonsterToFind==NULL;i++,j++)
	{
		find_spot_in_room_recursive(layout,x+freearr_x[i%8+1],y+freearr_y[i%8+1],RP);
	}

}

/* find a random non-blocked spot in this room to drop a key. */
void find_spot_in_room(mapstruct *map,int x,int y,int *kx,int *ky,RMParms *RP)
{
	char **layout2;
	int i,j;
	number_of_free_spots_in_room=0;
	room_free_spots_x = (int *)calloc(sizeof(int),RP->Xsize * RP->Ysize);
	room_free_spots_y = (int *)calloc(sizeof(int),RP->Xsize * RP->Ysize);

	layout2 = (char **) calloc(sizeof(char *),RP->Xsize);
	/* allocate and copy the layout, converting C to 0. */
	for (i=0;i<RP->Xsize;i++)
	{
		layout2[i]=(char *)calloc(sizeof(char),RP->Ysize);
		for (j=0;j<RP->Ysize;j++)
		{
			if (wall_blocked(map,i,j)) layout2[i][j] = '#';
		}
	}

	/* setup num_free_spots and room_free_spots */
	find_spot_in_room_recursive(layout2,x,y,RP);

	if (number_of_free_spots_in_room > 0)
	{
		i = RANDOM()%number_of_free_spots_in_room;
		*kx = room_free_spots_x[i];
		*ky = room_free_spots_y[i];
	}

	/* deallocate the temp. layout */
	for (i=0;i<RP->Xsize;i++)
	{
		free(layout2[i]);
	}
	free(layout2);
	free(room_free_spots_x);
	free(room_free_spots_y);
}


/* searches the map for a spot with walls around it.  The more
	walls the better, but it'll settle for 1 wall, or even 0, but
	it'll return 0 if no FREE spots are found.*/

void find_enclosed_spot(mapstruct *map, int *cx, int *cy,RMParms *RP)
{
	int x,y;
	int i;
	x = *cx;
	y=*cy;

	for (i=0;i<SIZEOFFREE;i++)
	{
		int lx,ly,sindex;
		lx = x +freearr_x[i];
		ly = y +freearr_y[i];
		sindex = surround_flag3(map,lx,ly,RP);
		/* if it's blocked on 3 sides, it's enclosed */
		if (sindex==7 || sindex == 11 || sindex == 13 || sindex == 14)
		{
			*cx= lx;
			*cy= ly;
			return;
		}
	}

	/* OK, if we got here, we're obviously someplace where there's no enclosed
	   spots--try to find someplace which is 2x enclosed.  */
	for (i=0;i<SIZEOFFREE;i++)
	{
		int lx,ly,sindex;
		lx = x +freearr_x[i];
		ly = y +freearr_y[i];
		sindex = surround_flag3(map,lx,ly,RP);
		/* if it's blocked on 3 sides, it's enclosed */
		if (sindex==3 || sindex == 5 || sindex == 9 || sindex == 6 || sindex==10 || sindex==12)
		{
			*cx= lx;
			*cy= ly;
			return;
		}
	}

	/* settle for one surround point */
	for (i=0;i<SIZEOFFREE;i++)
	{
		int lx,ly,sindex;
		lx = x +freearr_x[i];
		ly = y +freearr_y[i];
		sindex = surround_flag3(map,lx,ly,RP);
		/* if it's blocked on 3 sides, it's enclosed */
		if (sindex)
		{
			*cx= lx;
			*cy= ly;
			return;
		}
	}
	/* give up and return the closest free spot. */
	i = find_first_free_spot(find_archetype("chest"),map,x,y);
	if (i!=-1&&i<SIZEOFFREE)
	{
		*cx = x +freearr_x[i];
		*cy = y +freearr_y[i];
	}
	/* indicate failure */
	*cx=*cy=-1;
}


void remove_monsters(int x,int y,mapstruct *map)
{
	object *tmp;

	for (tmp=get_map_ob(map,x,y);tmp!=NULL;tmp=tmp->above)
		if (QUERY_FLAG(tmp,FLAG_MONSTER))
		{
			if (tmp->head) tmp=tmp->head;
			remove_ob(tmp);
			tmp=get_map_ob(map,x,y);
			if (tmp==NULL) break;
		};
}


/*  surrounds the point x,y by doors, so as to enclose something, like
	 a chest.  It only goes as far as the 8 squares surrounding, and
	 it'll remove any monsters it finds.*/

object ** surround_by_doors(mapstruct *map,char **layout,int x,int y,int opts)
{
	int i;
	char *doors[2];
	object **doorlist;
	int ndoors_made=0;
	doorlist = (object **) calloc(9, sizeof(object *)); /* 9 doors so we can hold termination null */

	/* this is a list we pick from, for horizontal and vertical doors */
	if (opts&DOORED)
	{
		doors[0]="locked_door2";
		doors[1]="locked_door1";
	}
	else
	{
		doors[0]="door_1";
		doors[1]="door_2";
	}

	/* place doors in all the 8 adjacent unblocked squares. */
	for (i=1;i<9;i++)
	{
		int x1 = x + freearr_x[i], y1 = y+freearr_y[i];

		if (!wall_blocked(map,x1,y1)
				|| layout[x1][y1]=='>')  /* place a door */
		{
			object * new_door=get_archetype( (freearr_x[i]==0)?doors[1]:doors[0]);
			new_door->x = x + freearr_x[i];
			new_door->y = y + freearr_y[i];
			remove_monsters(new_door->x,new_door->y,map);
			insert_ob_in_map(new_door,map,NULL,INS_NO_MERGE | INS_NO_WALK_ON);
			doorlist[ndoors_made]=new_door;
			ndoors_made++;
		}
	}
	return doorlist;
}


/* returns the first door in this square, or NULL if there isn't a door. */
object *door_in_square(mapstruct *map,int x,int y)
{
	object *tmp;
	for (tmp=get_map_ob(map,x,y);tmp!=NULL;tmp=tmp->above)
		if (tmp->type == DOOR || tmp->type== LOCKED_DOOR) return tmp;
	return NULL;
}


/* the workhorse routine, which finds the doors in a room */
void find_doors_in_room_recursive(char **layout,mapstruct *map,int x,int y,object **doorlist,int *ndoors,RMParms *RP)
{
	int i,j;
	object *door;

	/* bounds check x and y */
	if (!(x >= 0 && y >= 0 && x < RP->Xsize && y < RP->Ysize)) return;

	/* if the square is blocked or searched already, leave */
	if (layout[x][y]==1) return;

	/* check off this point */
	if (layout[x][y]=='#')  /* there could be a door here */
	{
		layout[x][y]=1;
		door=door_in_square(map,x,y);
		if (door!=NULL)
		{
			doorlist[*ndoors]=door;
			if (*ndoors>254) /* eek!  out of memory */
			{
				printf("find_doors_in_room_recursive:Too many doors for memory allocated!\n");
				return;
			}
			*ndoors=*ndoors+1;
		}
	}
	else
	{
		layout[x][y]=1;
		/* now search all the 8 squares around recursively for free spots,in random order */
		for (i=RANDOM()%8,j=0; j<8 && theMonsterToFind==NULL;i++,j++)
		{
			find_doors_in_room_recursive(layout,map,x+freearr_x[i%8+1],y+freearr_y[i%8+1],doorlist,ndoors,RP);
		}
	}
}

/* find a random non-blocked spot in this room to drop a key. */
object** find_doors_in_room(mapstruct *map,int x,int y,RMParms *RP)
{
	char **layout2;
	object **doorlist;
	int i,j;
	int ndoors=0;

	doorlist = (object **)calloc(sizeof(int),256);


	layout2 = (char **) calloc(sizeof(char *),RP->Xsize);
	/* allocate and copy the layout, converting C to 0. */
	for (i=0;i<RP->Xsize;i++)
	{
		layout2[i]=(char *)calloc(sizeof(char),RP->Ysize);
		for (j=0;j<RP->Ysize;j++)
		{
			if (wall_blocked(map,i,j)) layout2[i][j] = '#';
		}
	}

	/* setup num_free_spots and room_free_spots */
	find_doors_in_room_recursive(layout2,map,x,y,doorlist,&ndoors,RP);

	/* deallocate the temp. layout */
	for (i=0;i<RP->Xsize;i++)
	{
		free(layout2[i]);
	}
	free(layout2);
	return doorlist;
}



/* locks and/or hides all the doors in doorlist, or does nothing if
	opts doesn't say to lock/hide doors. */

void lock_and_hide_doors(object **doorlist,mapstruct *map,int opts,RMParms *RP)
{
	object *door;
	int i;
	/* lock the doors and hide the keys. */

	if (opts & DOORED)
	{
		for (i=0,door=doorlist[0];doorlist[i]!=NULL;i++)
		{
			object *new_door=get_archetype("locked_door1");
			char keybuf[256];
			door=doorlist[i];
			new_door->face = door->face;
			new_door->x = door->x;
			new_door->y = door->y;
			remove_ob(door);
			doorlist[i]=new_door;
			insert_ob_in_map(new_door,map,NULL,INS_NO_MERGE | INS_NO_WALK_ON);
			sprintf(keybuf,"%d",(int)RANDOM());
			FREE_AND_COPY_HASH(new_door->slaying, keybuf);
			keyplace(map,new_door->x,new_door->y,keybuf,NO_PASS_DOORS,2,RP);
		}
	}

	/* change the faces of the doors and surrounding walls to hide them. */
	if (opts & HIDDEN)
	{
		for (i=0,door=doorlist[0];doorlist[i]!=NULL;i++)
		{
			object *wallface;
			door=doorlist[i];
			wallface=retrofit_joined_wall(map,door->x,door->y,1,RP);
			if (wallface!=NULL)
			{
				retrofit_joined_wall(map,door->x-1,door->y,0,RP);
				retrofit_joined_wall(map,door->x+1,door->y,0,RP);
				retrofit_joined_wall(map,door->x,door->y-1,0,RP);
				retrofit_joined_wall(map,door->x,door->y+1,0,RP);
				door->face = wallface->face;
				if (!QUERY_FLAG(wallface,FLAG_REMOVED)) remove_ob(wallface);
			}
		}
	}
}
