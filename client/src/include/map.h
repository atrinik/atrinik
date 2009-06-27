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

#if !defined(__MAP_H)
#define __MAP_H

#define MAP_START_XOFF 376

#define MAP_START_YOFF 143
#define MAP_TILE_POS_YOFF 23
#define MAP_TILE_POS_YOFF2 12
#define MAP_TILE_POS_XOFF 48
#define MAP_TILE_POS_XOFF2 24

#define MAP_TILE_XOFF 12
#define MAP_TILE_YOFF 24

/* table of pre definded multi arch objects.
 * mpart_id and mpart_nr in the arches are commited from server
 * to analyze the exaclty tile position inside a mpart object.
 *
 * The way of determinate the starting and shift points is explained
 * in the dev/multi_arch folder of the arches, where the multi arch templates &
 * masks are.
 */

typedef struct _multi_part_tile {
    int xoff;       /* X-offset */
    int yoff;       /* Y-offset */
}_multi_part_tile;

typedef struct _multi_part_obj {
    int xlen;                   /* natural xlen of the whole multi arch */
    int ylen;                   /* same for ylen */
    _multi_part_tile part[16];
}_multi_part_obj;

typedef struct _mapdata
{
    char name[256];
    char music[256];
    int xlen;
    int ylen;
    int posx;
    int posy;
}_mapdata;

struct MapCell {
    short faces[MAXFACES];
    short pos[MAXFACES];
    int fog_of_war;
    uint8 ext[MAXFACES];
    char pname[MAXFACES][32];
    uint8 probe[MAXFACES];
    uint8 darkness;
}MapCell;

struct Map {
	struct MapCell cells[MAP_MAX_SIZE][MAP_MAX_SIZE];
}Map;

typedef struct {
	int x;
	int y;
} MapPos;

extern _mapdata MapData;
extern _multi_part_obj MultiArchs[16];

extern void clear_map(void);
extern void display_map_clearcell(long x, long y);
extern void set_map_darkness(int x, int y, uint8 darkness);
extern void set_map_face(int x, int y, int layer, int face, int pos, int ext, char *name);
extern void map_draw_map(void);
extern void display_mapscroll ( int dx, int dy );
extern void InitMapData(char *name, int xl, int yl, int px, int py, char *bg_music);
extern void set_map_ext(int x, int y, int layer,int ext, int probe);
extern void map_draw_map_clear(void);
extern void load_mapdef_dat(void);
extern void adjust_map_cache(int x, int y);
extern int get_tile_position( int mx, int my, int *tx, int *ty );

#endif
