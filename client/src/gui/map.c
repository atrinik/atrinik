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

/**
 * @file
 * Map drawing. */

#include "include.h"

static struct Map the_map;

/** Current shown map: mapname, length, etc */
_mapdata MapData;

static _multi_part_obj MultiArchs[16];

/* Load the multi arch offsets */
void load_mapdef_dat()
{
	FILE *stream;
	int i, ii, x, y, d[32];
	char line[256];

	if (!(stream = fopen_wrapper(ARCHDEF_FILE, "r")))
	{
		LOG(llevBug, "Can't find file %s\n", ARCHDEF_FILE);
		return;
	}

	for (i = 0; i < 16; i++)
	{
		if (!fgets(line, 255, stream))
		{
			break;
		}

		sscanf(line, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d", &x, &y, &d[0],&d[1], &d[2], &d[3], &d[4], &d[5], &d[6], &d[7], &d[8], &d[9], &d[10], &d[11], &d[12], &d[13], &d[14], &d[15], &d[16], &d[17], &d[18], &d[19], &d[20], &d[21], &d[22], &d[23], &d[24], &d[25], &d[26], &d[27], &d[28], &d[29], &d[30], &d[31]);
		MultiArchs[i].xlen = x;
		MultiArchs[i].ylen = y;

		for (ii = 0; ii < 16; ii++)
		{
			MultiArchs[i].part[ii].xoff = d[ii * 2];
			MultiArchs[i].part[ii].yoff = d[ii * 2 + 1];
		}
	}

	fclose(stream);
}

/**
 * Show map name widget.
 * @param x X position of the map name
 * @param y Y position of the map name */
void widget_show_mapname(widgetdata *widget)
{
	StringBlt(ScreenSurface, &BigFont, MapData.name, widget->x1, widget->y1, COLOR_HGOLD, NULL, NULL);
}

/**
 * Clear the map. */
void clear_map()
{
	memset(&the_map, 0, sizeof(Map));
}

/**
 * Scroll the map.
 * @param dx X.
 * @param dy Y. */
void display_mapscroll(int dx, int dy)
{
	int x, y;
	struct Map newmap;

	for (x = 0; x < MapStatusX; x++)
	{
		for (y = 0; y < MapStatusY; y++)
		{
			if (x + dx < 0 || x + dx >= MapStatusX || y + dy < 0 || y + dy >= MapStatusY)
			{
				memset((char *) &(newmap.cells[x][y]), 0, sizeof(struct MapCell));
			}
			else
			{
				memcpy((char *) &(newmap.cells[x][y]), (char *) &(the_map.cells[x + dx][y + dy]), sizeof(struct MapCell));
			}
		}
	}

	memcpy((char *) &the_map, (char *) & newmap, sizeof(struct Map));
}

/**
 * Draw black tiles over the map. */
void map_draw_map_clear()
{
	int ypos, xpos, x, y;

	for (x = 0; x < MapStatusX; x++)
	{
		for (y = 0; y < MapStatusY; y++)
		{
			xpos = options.mapstart_x + x * MAP_TILE_YOFF - y * MAP_TILE_YOFF;
			ypos = options.mapstart_y + x * MAP_TILE_XOFF + y * MAP_TILE_XOFF;
			sprite_blt_map(Bitmaps[BITMAP_BLACKTILE], xpos, ypos, NULL, NULL, 0);
		}
	}
}

/**
 * Update map's data.
 * @param name Map's name.
 * @param bg_music Map's background music. */
void update_map_data(const char *name, char *bg_music)
{
	widgetdata *widget;

	if (!strcmp(bg_music, "no_music"))
	{
		sound_stop_bg_music();
	}
	else
	{
		strncpy(MapData.music, bg_music, sizeof(MapData.music));
		parse_map_bg_music(bg_music);
	}

	if (name)
	{
		strncpy(MapData.name, name, sizeof(MapData.name));

		/* We need to update all mapname widgets on the screen now.
		 * Not that there should be more than one at a time, but just in case. */
		for (widget = cur_widget[MAPNAME_ID]; widget; widget = widget->type_next)
		{
			resize_widget(widget, RESIZE_RIGHT, get_string_pixel_length(name, &BigFont));
			resize_widget(widget, RESIZE_BOTTOM, BigFont.c[0].h);
		}
	}
}

/**
 * Initialize map's data.
 * @param xl Map width.
 * @param yl Map height.
 * @param px Player's X position.
 * @param py Player's Y position. */
void init_map_data(int xl, int yl, int px, int py)
{
	if (xl != -1)
	{
		MapData.xlen = xl;
	}

	if (yl != -1)
	{
		MapData.ylen = yl;
	}

	if (px != -1)
	{
		MapData.posx = px;
	}

	if (py != -1)
	{
		MapData.posy = py;
	}

	if (xl > 0)
	{
		clear_map();
	}
}

/**
 * Calculate height of X/Y coordinate on ::the_map.
 *
 * Checks for X/Y overflows.
 * @param x X position.
 * @param y Y position.
 * @return The height. */
static int calc_map_cell_height(int x, int y)
{
	if (x >= 0 && x < MapStatusX && y >= 0 && y < MapStatusY)
	{
		return the_map.cells[x][y].height;
	}

	return 0;
}

#define MAX_STRETCH 8
#define MAX_STRETCH_DIAG 12

/**
 * Align tile stretch based on X/Y.
 * @param x X position.
 * @param y Y position. */
void align_tile_stretch(int x, int y)
{
	uint8 top, bottom, right, left, min_ht;
	uint32 h;
	int nw_height, n_height, ne_height, sw_height, s_height, se_height, w_height, e_height, my_height;

	if (x < 0 || y < 0 || x >= MapStatusX || y >= MapStatusY)
	{
		return;
	}

	nw_height = calc_map_cell_height(x - 1, y - 1);
	n_height = calc_map_cell_height(x, y - 1);
	ne_height = calc_map_cell_height(x + 1, y - 1);
	sw_height = calc_map_cell_height(x - 1, y + 1);
	s_height = calc_map_cell_height(x, y + 1);
	se_height = calc_map_cell_height(x + 1, y + 1);
	w_height = calc_map_cell_height(x - 1, y);
	e_height = calc_map_cell_height(x + 1, y);
	my_height = calc_map_cell_height(x, y);

	if (abs(my_height - e_height) > MAX_STRETCH)
	{
		e_height = my_height;
	}

	if (abs(my_height - se_height) > MAX_STRETCH_DIAG)
	{
		se_height = my_height;
	}

	if (abs(my_height - s_height) > MAX_STRETCH)
	{
		s_height = my_height;
	}

	if (abs(my_height - sw_height) > MAX_STRETCH_DIAG)
	{
		sw_height = my_height;
	}

	if (abs(my_height - w_height) > MAX_STRETCH)
	{
		w_height = my_height;
	}

	if (abs(my_height - nw_height) > MAX_STRETCH_DIAG)
	{
		nw_height = my_height;
	}

	if (abs(my_height - n_height) > MAX_STRETCH)
	{
		n_height = my_height;
	}

	if (abs(my_height - ne_height) > MAX_STRETCH_DIAG)
	{
		ne_height = my_height;
	}

	top = MAX(w_height, nw_height);
	top = MAX(top, n_height);
	top = MAX(top, my_height);

	bottom = MAX(s_height, se_height);
	bottom = MAX(bottom, e_height);
	bottom = MAX(bottom, my_height);

	right = MAX(n_height, ne_height);
	right = MAX(right, e_height);
	right = MAX(right, my_height);

	left = MAX(w_height, sw_height);
	left = MAX(left, s_height);
	left = MAX(left, my_height);

	/* Normalize these... */
	min_ht = MIN(top, bottom);
	min_ht = MIN(min_ht, left);
	min_ht = MIN(min_ht, right);
	min_ht = MIN(min_ht, my_height);

	top -= min_ht;
	bottom -= min_ht;
	left -= min_ht;
	right -= min_ht;

	h = bottom + (left << 8) + (right << 16) + (top << 24);
	the_map.cells[x][y].stretch = h;
}

/**
 * Adjust the tile stretch of a map.
 *
 * Goes through the whole map and for each coordinate calls align_tile_stretch()
 * in all directions. This is done to fix any inconsistencies, since the map
 * command doesn't send us the whole map all over again, but only new/changes
 * parts. */
void adjust_tile_stretch()
{
	int x, y;

	for (x = 0; x < MapStatusX; x++)
	{
		for (y = 0; y < MapStatusY; y++)
		{
			align_tile_stretch(x - 1, y - 1);
			align_tile_stretch(x , y - 1);
			align_tile_stretch(x + 1, y - 1);
			align_tile_stretch(x + 1, y);

			align_tile_stretch(x + 1, y + 1);
			align_tile_stretch(x, y + 1);
			align_tile_stretch(x - 1, y + 1);
			align_tile_stretch(x - 1, y);

			align_tile_stretch(x, y);
		}
	}
}

/**
 * Set data for map cell.
 * @param x X of the cell.
 * @param y Y of the cell.
 * @param layer Layer we're doing this for.
 * @param face Face to set.
 * @param quick_pos Is this a multi-arch?
 * @param obj_flags Flags.
 * @param name Player's name.
 * @param name_color Player's name color.
 * @param height Z position of the tile.
 * @param probe Target's HP bar. */
void map_set_data(int x, int y, int layer, sint16 face, uint8 quick_pos, uint8 obj_flags, const char *name, uint8 name_color, sint16 height, uint8 probe)
{
	the_map.cells[x][y].faces[layer] = face;
	the_map.cells[x][y].flags[layer] = obj_flags;

	the_map.cells[x][y].probe[layer] = probe;
	the_map.cells[x][y].quick_pos[layer] = quick_pos;

	the_map.cells[x][y].pcolor[layer] = name_color;
	strncpy(the_map.cells[x][y].pname[layer], name, sizeof(the_map.cells[x][y].pname[layer]));

	if (layer == 1)
	{
		the_map.cells[x][y].height = height;
	}
}

/**
 * Clear map's cell.
 * @param x X of the cell.
 * @param y Y of the cell. */
void map_clear_cell(int x, int y)
{
	int i;

	the_map.cells[x][y].darkness = 0;

	for (i = 1; i <= MAX_LAYERS; i++)
	{
		the_map.cells[x][y].faces[i] = 0;
		the_map.cells[x][y].flags[i] = 0;
		the_map.cells[x][y].probe[i] = 0;
		the_map.cells[x][y].quick_pos[i] = 0;
		the_map.cells[x][y].pname[i][0] = '\0';
	}
}

/**
 * Set darkness for map's cell.
 * @param x X of the cell.
 * @param y Y of the cell.
 * @param darkness Darkness to set. */
void map_set_darkness(int x, int y, uint8 darkness)
{
	the_map.cells[x][y].darkness = darkness;
}

/**
 * Draw a single object on the map.
 * @param x X position of the object.
 * @param y Y position of the object.
 * @param k Layer.
 * @param player_height_offset Player's height offset. */
static void draw_map_object(int x, int y, int layer, int player_height_offset)
{
	struct MapCell *map = &the_map.cells[x][y];
	_Sprite *face_sprite;
	int ypos, xpos;
	int xl, yl, temp;
	int xml, xmpos, xtemp = 0;
	uint16 face;
	int mid, mnr;
	uint32 stretch = 0;
	_BLTFX bltfx;

	bltfx.surface = NULL;
	xpos = MAP_START_XOFF + x * MAP_TILE_YOFF - y * MAP_TILE_YOFF;
	ypos = MAP_START_YOFF + x * MAP_TILE_XOFF + y * MAP_TILE_XOFF;
	face = map->faces[layer];

	if (face <= 0)
	{
		return;
	}

	face_sprite = FaceList[face].sprite;

	if (!face_sprite)
	{
		return;
	}

	/* We have a set quick_pos = multi tile */
	if (map->quick_pos[layer])
	{
		mnr = map->quick_pos[layer];
		mid = mnr >> 4;
		mnr &= 0x0f;
		xml = MultiArchs[mid].xlen;
		yl = ypos - MultiArchs[mid].part[mnr].yoff + MultiArchs[mid].ylen - face_sprite->bitmap->h;

		/* we allow overlapping x borders - we simply center then */
		xl = 0;

		if (face_sprite->bitmap->w > MultiArchs[mid].xlen)
		{
			xl = (MultiArchs[mid].xlen - face_sprite->bitmap->w) >> 1;
		}

		xmpos = xpos - MultiArchs[mid].part[mnr].xoff;
		xl += xmpos;
	}
	/* Single tile... */
	else
	{
		/* First, we calc the shift positions */
		xml = MAP_TILE_POS_XOFF;
		yl = (ypos + MAP_TILE_POS_YOFF) - face_sprite->bitmap->h;
		xmpos = xl = xpos;

		if (face_sprite->bitmap->w > MAP_TILE_POS_XOFF)
		{
			xl -= (face_sprite->bitmap->w - MAP_TILE_POS_XOFF) / 2;
		}
	}

	/* Blit the face in the darkness level the tile pos has */
	temp = map->darkness;

	if (temp == 210)
	{
		bltfx.dark_level = 0;
	}
	else if (temp == 180)
	{
		bltfx.dark_level = 1;
	}
	else if (temp == 150)
	{
		bltfx.dark_level = 2;
	}
	else if (temp == 120)
	{
		bltfx.dark_level = 3;
	}
	else if (temp == 90)
	{
		bltfx.dark_level = 4;
	}
	else if (temp == 60)
	{
		bltfx.dark_level = 5;
	}
	else if (temp == 0)
	{
		bltfx.dark_level = 7;
	}
	else
	{
		bltfx.dark_level = 6;
	}

	bltfx.flags = 0;

	if (map->fog_of_war)
	{
		bltfx.flags |= BLTFX_FLAG_FOW;
	}
	else if (cpl.stats.flags & SF_INFRAVISION && layer == 6 && map->darkness < 150)
	{
		bltfx.flags |= BLTFX_FLAG_RED;
	}
	else
	{
		bltfx.flags |= BLTFX_FLAG_DARK;
	}

	if (map->flags[layer] & FFLAG_INVISIBLE && !(bltfx.flags & BLTFX_FLAG_FOW))
	{
		bltfx.flags &= ~BLTFX_FLAG_DARK;
		bltfx.flags |= BLTFX_FLAG_SRCALPHA | BLTFX_FLAG_GREY;
	}
	else if (map->flags[layer] & FFLAG_ETHEREAL && !(bltfx.flags & BLTFX_FLAG_FOW))
	{
		bltfx.flags &= ~BLTFX_FLAG_DARK;
		bltfx.flags |= BLTFX_FLAG_SRCALPHA;
	}

	stretch = 0;

	if (layer <= 2 && map->stretch)
	{
		bltfx.flags |= BLTFX_FLAG_STRETCH;
		stretch = map->stretch;
	}

	yl = (yl - map->height) + player_height_offset;

	/* These faces are only shown when they are in a
	 * position which would be visible to the player. */
	if (FaceList[face].flags & FACE_FLAG_UP)
	{
		/* If the face is dir [0124568] and in
		 * the top or right quadrant or on the
		 * central square, blt it. */
		if (FaceList[face].flags & FACE_FLAG_D1)
		{
			if (((x <= (MAP_MAX_SIZE - 1) / 2) && (y <= (MAP_MAX_SIZE - 1) / 2)) || ((x > (MAP_MAX_SIZE - 1) / 2) && (y < (MAP_MAX_SIZE - 1) / 2)))
			{
				sprite_blt_map(face_sprite, xl, yl, NULL, &bltfx, 0);
			}
		}

		/* If the face is dir [0234768] and in
		 * the top or left quadrant or on the
		 * central square, blt it. */
		if (FaceList[face].flags & FACE_FLAG_D3)
		{
			if (((x <= (MAP_MAX_SIZE - 1) / 2) && (y <= (MAP_MAX_SIZE - 1) / 2)) || ((x < (MAP_MAX_SIZE - 1) / 2) && (y > (MAP_MAX_SIZE - 1) / 2)))
			{
				sprite_blt_map(face_sprite, xl, yl, NULL, &bltfx, 0);
			}
		}
	}
	/* Double faces are shown twice, one above
	 * the other, when not lower on the screen
	 * than the player. This simulates high walls
	 * without obscuring the user's view. */
	else if (FaceList[face].flags & FACE_FLAG_DOUBLE)
	{
		/* Blt face once in normal position. */
		sprite_blt_map(face_sprite, xl, yl, NULL, &bltfx, 0);

		/* If it's not in the bottom quadrant of
		 * the map, blt it again 'higher up' on
		 * the same square. */
		if (x < (MAP_MAX_SIZE - 1) / 2 || y < (MAP_MAX_SIZE - 1) / 2)
		{
			sprite_blt_map(face_sprite, xl, yl - 22, NULL, &bltfx, 0);
		}
	}
	else
	{
		sprite_blt_map(face_sprite, xl, yl, NULL, &bltfx, stretch);
	}

	/* Do we have a playername? Then print it! */
	if (options.player_names && map->pname[layer][0])
	{
		if (options.player_names == 1 || (options.player_names == 2 && strncasecmp(map->pname[layer], cpl.rankandname, strlen(map->pname[layer]))) || (options.player_names == 3 && !strncasecmp(map->pname[layer], cpl.rankandname, strlen(map->pname[layer]))))
		{
			StringBlt(ScreenSurfaceMap, &Font6x3Out, map->pname[layer], xpos - (strlen(map->pname[layer]) * 2) + 22, yl - 26, map->pcolor[layer], NULL, NULL);
		}
	}

	/* Perhaps the object has a marked effect, blit it now */
	if (map->flags[layer])
	{
		if (map->flags[layer] & FFLAG_SLEEP)
		{
			sprite_blt_map(Bitmaps[BITMAP_SLEEP], xl + face_sprite->bitmap->w / 2, yl - 5, NULL, NULL, 0);
		}

		if (map->flags[layer] & FFLAG_CONFUSED)
		{
			sprite_blt_map(Bitmaps[BITMAP_CONFUSE], xl + face_sprite->bitmap->w / 2 - 1, yl - 4, NULL, NULL, 0);
		}

		if (map->flags[layer] & FFLAG_SCARED)
		{
			sprite_blt_map(Bitmaps[BITMAP_SCARED], xl + face_sprite->bitmap->w / 2 + 10, yl - 4, NULL, NULL, 0);
		}

		if (map->flags[layer] & FFLAG_BLINDED)
		{
			sprite_blt_map(Bitmaps[BITMAP_BLIND], xl + face_sprite->bitmap->w / 2 + 3, yl - 6, NULL, NULL, 0);
		}

		if (map->flags[layer] & FFLAG_PARALYZED)
		{
			sprite_blt_map(Bitmaps[BITMAP_PARALYZE], xl + face_sprite->bitmap->w / 2 + 2, yl + 3, NULL, NULL, 0);
			sprite_blt_map(Bitmaps[BITMAP_PARALYZE], xl + face_sprite->bitmap->w / 2 + 9, yl + 3, NULL, NULL, 0);
		}
	}

	if (map->probe[layer] && cpl.target_code)
	{
		int hp_col;
		Uint32 sdl_col;
		SDL_Rect rect;

		if (cpl.target_hp > 90)
		{
			hp_col = COLOR_GREEN;
		}
		else if (cpl.target_hp > 75)
		{
			hp_col = COLOR_DGOLD;
		}
		else if (cpl.target_hp > 50)
		{
			hp_col = COLOR_HGOLD;
		}
		else if (cpl.target_hp > 25)
		{
			hp_col = COLOR_ORANGE;
		}
		else if (cpl.target_hp > 10)
		{
			hp_col = COLOR_YELLOW;
		}
		else
		{
			hp_col = COLOR_RED;
		}

		if (xml == MAP_TILE_POS_XOFF)
		{
			xtemp = (int) (((double) xml / 100.0) * 25.0);
		}
		else
		{
			xtemp = (int) (((double) xml / 100.0) * 20.0);
		}

		temp = (xml - xtemp * 2) - 1;

		if (temp <= 0)
		{
			temp = 1;
		}

		if (temp >= 300)
		{
			temp = 300;
		}

		mid = map->probe[layer];

		if (mid <= 0)
		{
			mid = 1;
		}

		if (mid > 100)
		{
			mid = 100;
		}

		temp = (int) (((double) temp / 100.0) * (double) mid);
		rect.h = 2;
		rect.w = temp;
		rect.x = 0;
		rect.y = 0;

		sdl_col = SDL_MapRGB(ScreenSurfaceMap->format, Bitmaps[BITMAP_PALETTE]->bitmap->format->palette->colors[hp_col].r, Bitmaps[BITMAP_PALETTE]->bitmap->format->palette->colors[hp_col].g, Bitmaps[BITMAP_PALETTE]->bitmap->format->palette->colors[hp_col].b);

		/* First draw the bar */
		rect.x = xmpos + xtemp - 1;
		rect.y = yl - 9;
		rect.h = 1;
		SDL_FillRect(ScreenSurfaceMap, &rect, sdl_col);
		/* Horizontal lines of left bracked */
		rect.h = 1;
		rect.w = 3;
		rect.x = xmpos + xtemp - 3;
		rect.y = yl - 11;
		SDL_FillRect(ScreenSurfaceMap, &rect, sdl_col);
		rect.y = yl - 7;
		SDL_FillRect(ScreenSurfaceMap, &rect, sdl_col);
		/* Horizontal lines of right bracked */
		rect.x = xmpos + xtemp + (xml - xtemp * 2) - 3;
		SDL_FillRect(ScreenSurfaceMap, &rect, sdl_col);
		rect.y = yl - 11;
		SDL_FillRect(ScreenSurfaceMap, &rect, sdl_col);
		/* Vertical lines */
		rect.w = 1;
		rect.h = 5;
		rect.x = xmpos + xtemp - 3;
		rect.y = yl - 11;
		SDL_FillRect(ScreenSurfaceMap, &rect, sdl_col);
		rect.x = xmpos + xtemp + (xml - xtemp * 2) - 1;
		SDL_FillRect(ScreenSurfaceMap, &rect, sdl_col);

		/* Draw the name of target if it's not a player */
		if (!(options.player_names && map->pname[layer][0]))
		{
			StringBlt(ScreenSurfaceMap, &Font6x3Out, cpl.target_name, xpos - (strlen(cpl.target_name) * 2) + 22, yl - 26, cpl.target_color, NULL, NULL);
		}

		/* Draw HP remaining percent */
		if (cpl.target_hp > 0)
		{
			char hp_text[9];
			int hp_len = snprintf(hp_text, sizeof(hp_text), "HP: %d%%", cpl.target_hp);
			StringBlt(ScreenSurfaceMap, &Font6x3Out, hp_text, xpos - hp_len * 2 + 22, yl - 36, hp_col, NULL, NULL);
		}
	}
}

/**
 * Draw the map. */
void map_draw_map()
{
	int player_height_offset;
	int x, y, layer;

	player_height_offset = the_map.cells[MapStatusX - (MapStatusX / 2) - 1][MapStatusY - (MapStatusY / 2) - 1].height;

	/* First draw floor and floor masks. */
	for (x = 0; x < MAP_MAX_SIZE; x++)
	{
		for (y = 0; y < MAP_MAX_SIZE; y++)
		{
			for (layer = 1; layer <= 2; layer++)
			{
				draw_map_object(x, y, layer, player_height_offset);
			}
		}
	}

	/* Now draw everything else. */
	for (x = 0; x < MAP_MAX_SIZE; x++)
	{
		for (y = 0; y < MAP_MAX_SIZE; y++)
		{
			for (layer = 3; layer <= MAX_LAYERS; layer++)
			{
				draw_map_object(x, y, layer, player_height_offset);
			}
		}
	}
}

#define TILE_ISO_XLEN 48
/* This +1 is the trick to catch the one pixel line between
 * 2 y rows - the tiles don't touch there! */
#define TILE_ISO_YLEN (23 + 1)

/* calc the tile - pos(tx, ty) from mouse pos(x, y).
 * ret: 0 ok;  < 0 not a valid position. */
int get_tile_position(int x, int y, int *tx, int *ty)
{
	if (x < (int) ((options.mapstart_x + MAP_START_XOFF) * (options.zoom / 100.0)))
	{
		x -= (int) (MAP_TILE_POS_XOFF * (options.zoom / 100.0));
	}

	x -= (int) ((options.mapstart_x + MAP_START_XOFF) * (options.zoom / 100.0));
	y -= (int) ((options.mapstart_y + MAP_START_YOFF) * (options.zoom / 100.0));
	*tx = x / (int) (MAP_TILE_POS_XOFF * (options.zoom / 100.0)) + y / (int) (MAP_TILE_YOFF * (options.zoom / 100.0));
	*ty = y / (int) (MAP_TILE_YOFF * (options.zoom / 100.0)) - x / (int) (MAP_TILE_POS_XOFF * (options.zoom / 100.0));

	if (x < 0)
	{
		x += ((int) (MAP_TILE_POS_XOFF * (options.zoom / 100.0)) << 3) - 1;
	}

	x %= (int) (MAP_TILE_POS_XOFF * (options.zoom / 100.0));
	y %= (int) (MAP_TILE_YOFF * (options.zoom / 100.0));

	if (x < (int) (MAP_TILE_POS_XOFF2 * (options.zoom / 100.0)))
	{
		if (x + y + y < (int) (MAP_TILE_POS_XOFF2 * (options.zoom / 100.0)))
		{
			(*tx)--;
		}
		else if (y - x > 0)
		{
			(*ty)++;
		}
	}
	else
	{
		x -= (int) (MAP_TILE_POS_XOFF2 * (options.zoom / 100.0));

		if (x - y - y > 0)
		{
			(*ty)--;
		}
		else if (x + y + y > (int) (MAP_TILE_POS_XOFF * (options.zoom / 100.0)))
		{
			(*tx)++;
		}
	}

	if (*tx < 0 || *tx > MAP_MAX_SIZE || *ty < 0 || *ty > MAP_MAX_SIZE)
	{
		return -1;
	}

	return 0;
}
