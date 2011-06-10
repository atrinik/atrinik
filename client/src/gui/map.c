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
 * Map drawing. */

#include <include.h>

/** Font used for the map name. */
#define MAP_NAME_FONT FONT_SERIF14

static struct Map the_map;
static SDL_Surface *zoomed = NULL;

/** Current shown map: mapname, length, etc */
_mapdata MapData;

static _multi_part_obj MultiArchs[16];

/** Holds coordinates of the last map square the mouse was over. */
static int old_map_mouse_x = 0, old_map_mouse_y = 0;
/**
 * When the right button was pressed on the map widget. -1 = not
 * pressed. */
static int right_click_ticks = -1;

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
	SDL_Rect box;

	box.w = widget->wd;
	box.h = 0;
	string_blt(ScreenSurface, MAP_NAME_FONT, MapData.name, widget->x1, widget->y1, COLOR_SIMPLE(COLOR_HGOLD), TEXT_MARKUP, &box);
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

	for (x = 0; x < options.map_size_x; x++)
	{
		for (y = 0; y < options.map_size_y; y++)
		{
			if (x + dx < 0 || x + dx >= options.map_size_x || y + dy < 0 || y + dy >= options.map_size_y)
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

	for (x = 0; x < options.map_size_x; x++)
	{
		for (y = 0; y < options.map_size_y; y++)
		{
			xpos = x * MAP_TILE_YOFF - y * MAP_TILE_YOFF;
			ypos = x * MAP_TILE_XOFF + y * MAP_TILE_XOFF;
			sprite_blt_map(Bitmaps[BITMAP_BLACKTILE], xpos, ypos, NULL, NULL, 0, 0, 0);
		}
	}
}

/**
 * Update map's name.
 * @param name New map name. */
void update_map_name(const char *name)
{
	widgetdata *widget;

	strncpy(MapData.name, name, sizeof(MapData.name) - 1);
	MapData.name[sizeof(MapData.name) - 1] = '\0';

	/* We need to update all mapname widgets on the screen now.
	 * Not that there should be more than one at a time, but just in case. */
	for (widget = cur_widget[MAPNAME_ID]; widget; widget = widget->type_next)
	{
		resize_widget(widget, RESIZE_RIGHT, string_get_width(MAP_NAME_FONT, name, TEXT_MARKUP));
		resize_widget(widget, RESIZE_BOTTOM, string_get_height(MAP_NAME_FONT, name, TEXT_MARKUP));
	}
}

/**
 * Update map's weather.
 * @param weather New weather. */
void update_map_weather(const char *weather)
{
	effect_start(weather);
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
	if (x >= 0 && x < options.map_size_x && y >= 0 && y < options.map_size_y)
	{
		return the_map.cells[x][y].height[1];
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

	if (x < 0 || y < 0 || x >= options.map_size_x || y >= options.map_size_y)
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

	for (x = 0; x < options.map_size_x; x++)
	{
		for (y = 0; y < options.map_size_y; y++)
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
 * @param probe Target's HP bar.
 * @param zoom How much to zoom the face by.
 * @param align X align.
 * @param rotate Rotation in degrees. */
void map_set_data(int x, int y, int layer, sint16 face, uint8 quick_pos, uint8 obj_flags, const char *name, uint8 name_color, sint16 height, uint8 probe, sint16 zoom, sint16 align, uint8 draw_double, uint8 alpha, sint16 rotate)
{
	the_map.cells[x][y].faces[layer] = face;
	the_map.cells[x][y].flags[layer] = obj_flags;

	the_map.cells[x][y].probe[layer] = probe;
	the_map.cells[x][y].quick_pos[layer] = quick_pos;

	the_map.cells[x][y].pcolor[layer] = name_color;
	strncpy(the_map.cells[x][y].pname[layer], name, sizeof(the_map.cells[x][y].pname[layer]));
	the_map.cells[x][y].height[layer] = height;
	the_map.cells[x][y].zoom[layer] = zoom;
	the_map.cells[x][y].align[layer] = align;
	the_map.cells[x][y].draw_double[layer] = draw_double;
	the_map.cells[x][y].alpha[layer] = alpha;
	the_map.cells[x][y].rotate[layer] = rotate;
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
		the_map.cells[x][y].height[i] = 0;
		the_map.cells[x][y].zoom[i] = 0;
		the_map.cells[x][y].align[i] = 0;
		the_map.cells[x][y].rotate[i] = 0;
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
	int bitmap_h, bitmap_w;

	bltfx.surface = NULL;
	xpos = MAP_START_XOFF + x * MAP_TILE_YOFF - y * MAP_TILE_YOFF;
	ypos = MAP_START_YOFF + x * MAP_TILE_XOFF + y * MAP_TILE_XOFF;
	face = map->faces[layer];

	if (face <= 0 || face >= MAX_FACE_TILES)
	{
		return;
	}

	face_sprite = FaceList[face].sprite;

	if (!face_sprite)
	{
		return;
	}

	bitmap_h = face_sprite->bitmap->h;
	bitmap_w = face_sprite->bitmap->w;

	if (map->zoom[layer] && map->zoom[layer] != 100)
	{
		zoomSurfaceSize(bitmap_w, bitmap_h, map->zoom[layer] / 100.0, map->zoom[layer] / 100.0, &bitmap_w, &bitmap_h);
	}

	/* We have a set quick_pos = multi tile */
	if (map->quick_pos[layer])
	{
		mnr = map->quick_pos[layer];
		mid = mnr >> 4;
		mnr &= 0x0f;
		xml = MultiArchs[mid].xlen;
		yl = ypos - MultiArchs[mid].part[mnr].yoff + MultiArchs[mid].ylen - bitmap_h;

		/* we allow overlapping x borders - we simply center then */
		xl = 0;

		if (bitmap_w > MultiArchs[mid].xlen)
		{
			xl = (MultiArchs[mid].xlen - bitmap_w) >> 1;
		}

		xmpos = xpos - MultiArchs[mid].part[mnr].xoff;
		xl += xmpos;
	}
	/* Single tile... */
	else
	{
		/* First, we calc the shift positions */
		xml = MAP_TILE_POS_XOFF;
		yl = (ypos + MAP_TILE_POS_YOFF) - bitmap_h;
		xmpos = xl = xpos;

		if (bitmap_w > MAP_TILE_POS_XOFF)
		{
			xl -= (bitmap_w - MAP_TILE_POS_XOFF) / 2;
		}
	}

	if (map->align[layer])
	{
		xl += map->align[layer];
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
	bltfx.alpha = 0;

	if (cpl.stats.flags & SF_INFRAVISION && layer == 6 && map->darkness < 150)
	{
		bltfx.flags |= BLTFX_FLAG_RED;
	}
	else
	{
		bltfx.flags |= BLTFX_FLAG_DARK;
	}

	if (map->flags[layer] & FFLAG_INVISIBLE)
	{
		bltfx.flags &= ~BLTFX_FLAG_DARK;
		bltfx.flags |= BLTFX_FLAG_GREY;
	}

	if (map->alpha[layer])
	{
		bltfx.flags &= ~BLTFX_FLAG_DARK;
		bltfx.flags |= BLTFX_FLAG_SRCALPHA;
		bltfx.alpha = map->alpha[layer];
	}

	stretch = 0;

	if (layer <= 2 && map->stretch)
	{
		bltfx.flags |= BLTFX_FLAG_STRETCH;
		stretch = map->stretch;
	}

	yl = (yl - map->height[1]) + player_height_offset;

	if (layer > 1)
	{
		yl -= map->height[layer];
	}

	sprite_blt_map(face_sprite, xl, yl, NULL, &bltfx, stretch, map->zoom[layer], map->rotate[layer]);

	/* Double faces are shown twice, one above the other, when not lower
	 * on the screen than the player. This simulates high walls without
	 * obscuring the user's view. */
	if (map->draw_double[layer])
	{
		sprite_blt_map(face_sprite, xl, yl - 22, NULL, &bltfx, stretch, map->zoom[layer], map->rotate[layer]);
	}

	if (xml == MAP_TILE_POS_XOFF)
	{
		xtemp = (int) (((double) xml / 100.0) * 25.0);
	}
	else
	{
		xtemp = (int) (((double) xml / 100.0) * 20.0);
	}

	/* Do we have a playername? Then print it! */
	if (options.player_names && map->pname[layer][0])
	{
		if (options.player_names == 1 || (options.player_names == 2 && strncasecmp(map->pname[layer], cpl.name, strlen(map->pname[layer]))) || (options.player_names == 3 && !strncasecmp(map->pname[layer], cpl.name, strlen(map->pname[layer]))))
		{
			string_blt(cur_widget[MAP_ID]->widgetSF, FONT_SANS9, map->pname[layer], xmpos + xtemp + (xml - xtemp * 2) / 2 - string_get_width(FONT_SANS9, map->pname[layer], 0) / 2 - 2, yl - 24, COLOR_SIMPLE(map->pcolor[layer]), TEXT_OUTLINE, NULL);
		}
	}

	/* Perhaps the object has a marked effect, blit it now */
	if (map->flags[layer])
	{
		if (map->flags[layer] & FFLAG_SLEEP)
		{
			sprite_blt_map(Bitmaps[BITMAP_SLEEP], xl + bitmap_w / 2, yl - 5, NULL, NULL, 0, map->zoom[layer], map->rotate[layer]);
		}

		if (map->flags[layer] & FFLAG_CONFUSED)
		{
			sprite_blt_map(Bitmaps[BITMAP_CONFUSE], xl + bitmap_w / 2 - 1, yl - 4, NULL, NULL, 0, map->zoom[layer], map->rotate[layer]);
		}

		if (map->flags[layer] & FFLAG_SCARED)
		{
			sprite_blt_map(Bitmaps[BITMAP_SCARED], xl + bitmap_w / 2 + 10, yl - 4, NULL, NULL, 0, map->zoom[layer], map->rotate[layer]);
		}

		if (map->flags[layer] & FFLAG_BLINDED)
		{
			sprite_blt_map(Bitmaps[BITMAP_BLIND], xl + bitmap_w / 2 + 3, yl - 6, NULL, NULL, 0, map->zoom[layer], map->rotate[layer]);
		}

		if (map->flags[layer] & FFLAG_PARALYZED)
		{
			sprite_blt_map(Bitmaps[BITMAP_PARALYZE], xl + bitmap_w / 2 + 2, yl + 3, NULL, NULL, 0, map->zoom[layer], map->rotate[layer]);
			sprite_blt_map(Bitmaps[BITMAP_PARALYZE], xl + bitmap_w / 2 + 9, yl + 3, NULL, NULL, 0, map->zoom[layer], map->rotate[layer]);
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

		sdl_col = SDL_MapRGB(cur_widget[MAP_ID]->widgetSF->format, Bitmaps[BITMAP_PALETTE]->bitmap->format->palette->colors[hp_col].r, Bitmaps[BITMAP_PALETTE]->bitmap->format->palette->colors[hp_col].g, Bitmaps[BITMAP_PALETTE]->bitmap->format->palette->colors[hp_col].b);

		/* First draw the bar */
		rect.x = xmpos + xtemp - 1;
		rect.y = yl - 9;
		rect.h = 1;
		SDL_FillRect(cur_widget[MAP_ID]->widgetSF, &rect, sdl_col);
		/* Horizontal lines of left bracket */
		rect.h = 1;
		rect.w = 3;
		rect.x = xmpos + xtemp - 3;
		rect.y = yl - 11;
		SDL_FillRect(cur_widget[MAP_ID]->widgetSF, &rect, sdl_col);
		rect.y = yl - 7;
		SDL_FillRect(cur_widget[MAP_ID]->widgetSF, &rect, sdl_col);
		/* Horizontal lines of right bracket */
		rect.x = xmpos + xtemp + (xml - xtemp * 2) - 3;
		SDL_FillRect(cur_widget[MAP_ID]->widgetSF, &rect, sdl_col);
		rect.y = yl - 11;
		SDL_FillRect(cur_widget[MAP_ID]->widgetSF, &rect, sdl_col);
		/* Vertical lines */
		rect.w = 1;
		rect.h = 5;
		rect.x = xmpos + xtemp - 3;
		rect.y = yl - 11;
		SDL_FillRect(cur_widget[MAP_ID]->widgetSF, &rect, sdl_col);
		rect.x = xmpos + xtemp + (xml - xtemp * 2) - 1;
		SDL_FillRect(cur_widget[MAP_ID]->widgetSF, &rect, sdl_col);

		/* Draw the name of target if it's not a player */
		if (!(options.player_names && map->pname[layer][0]))
		{
			string_blt(cur_widget[MAP_ID]->widgetSF, FONT_SANS9, cpl.target_name, xmpos + xtemp + (xml - xtemp * 2) / 2 - string_get_width(FONT_SANS9, cpl.target_name, 0) / 2 - 2, yl - 24, COLOR_SIMPLE(cpl.target_color), TEXT_OUTLINE, NULL);
		}

		/* Draw HP remaining percent */
		if (cpl.target_hp > 0)
		{
			char hp_text[9];

			snprintf(hp_text, sizeof(hp_text), "HP: %d%%", cpl.target_hp);
			string_blt(cur_widget[MAP_ID]->widgetSF, FONT_SANS9, hp_text, xmpos + xtemp + (xml - xtemp * 2) / 2 - string_get_width(FONT_SANS9, hp_text, 0) / 2 - 2, yl - 36, COLOR_SIMPLE(hp_col), TEXT_OUTLINE, NULL);
		}
	}
}

/**
 * Draw the map. */
void map_draw_map()
{
	int player_height_offset;
	int x, y, layer;
	int tx, ty;

	player_height_offset = the_map.cells[options.map_size_x - (options.map_size_x / 2) - 1][options.map_size_y - (options.map_size_y / 2) - 1].height[1];

	/* First draw floor and floor masks. */
	for (x = 0; x < options.map_size_x; x++)
	{
		for (y = 0; y < options.map_size_y; y++)
		{
			for (layer = 1; layer <= 2; layer++)
			{
				draw_map_object(x, y, layer, player_height_offset);
			}
		}
	}

	/* Now draw everything else. */
	for (x = 0; x < options.map_size_x; x++)
	{
		for (y = 0; y < options.map_size_y; y++)
		{
			for (layer = 3; layer <= MAX_LAYERS; layer++)
			{
				draw_map_object(x, y, layer, player_height_offset);
			}
		}
	}

	if (cpl.menustatus == MENU_NO && widget_mouse_event.owner == cur_widget[MAP_ID] && mouse_to_tile_coords(x_custom_cursor, y_custom_cursor, &tx, &ty))
	{
		map_draw_one(tx, ty, Bitmaps[BITMAP_SQUARE_HIGHLIGHT]);
	}
}

/**
 * Draw one sprite on map.
 * @param x X position.
 * @param y Y position.
 * @param sprite What to draw. */
void map_draw_one(int x, int y, _Sprite *sprite)
{
	int xpos = MAP_START_XOFF + x * MAP_TILE_YOFF - y * MAP_TILE_YOFF;
	int ypos = (MAP_START_YOFF + x * MAP_TILE_XOFF + y * MAP_TILE_XOFF) + MAP_TILE_POS_YOFF - sprite->bitmap->h;

	if (sprite->bitmap->w > MAP_TILE_POS_XOFF)
	{
		xpos -= (sprite->bitmap->w - MAP_TILE_POS_XOFF) / 2;
	}

	if (the_map.cells[x][y].faces[1])
	{
		ypos = (ypos - (the_map.cells[x][y].height[1])) + (the_map.cells[options.map_size_x - (options.map_size_x / 2) - 1][options.map_size_y - (options.map_size_y / 2) - 1].height[1]);
	}

	sprite_blt_map(sprite, xpos, ypos, NULL, NULL, 0, 0, 0);
}

/**
 * Send a command to move the player to the specified square.
 * @param tx Square X position.
 * @param ty Square Y position. */
static void send_move_path(int tx, int ty)
{
	SockList sl;
	uint8 buf[HUGE_BUF];

	sl.buf = buf;
	sl.len = 0;
	SockList_AddString(&sl, "mp ");
	SockList_AddChar(&sl, tx);
	SockList_AddChar(&sl, ty);
	send_socklist(sl);
}

/**
 * Send a command to target an NPC.
 * @param tx NPC's X position.
 * @param ty NPC's Y position. */
static void send_target(int tx, int ty)
{
	char buf[MAX_BUF];

	snprintf(buf, sizeof(buf), "/target !%d %d", tx, ty);
	send_command(buf);
}

/**
 * Handle mouse events inside the map widget.
 * @param widget The widget.
 * @param event The event to handle. */
void widget_map_mevent(widgetdata *widget, SDL_Event *event)
{
	int tx, ty;

	(void) widget;

	/* Check if the mouse is in play field. */
	if (!mouse_to_tile_coords(event->motion.x, event->motion.y, &tx, &ty))
	{
		return;
	}

	if (event->type == SDL_MOUSEBUTTONUP)
	{
		/* Send target command if we released the right button in time;
		 * otherwise the widget menu will be created. */
		if (event->button.button == SDL_BUTTON_RIGHT && SDL_GetTicks() - right_click_ticks < 500)
		{
			send_target(tx, ty);
		}

		right_click_ticks = -1;
	}
	else if (event->type == SDL_MOUSEBUTTONDOWN)
	{
		if (event->button.button == SDL_BUTTON_RIGHT)
		{
			right_click_ticks = SDL_GetTicks();
		}
		/* Running */
		else if (SDL_GetMouseState(NULL, NULL) == SDL_BUTTON_LEFT)
		{
			if (cpl.fire_on || cpl.run_on)
			{
				move_keys(dir_from_tile_coords(tx, ty));
			}
			else
			{
				send_move_path(tx, ty);
			}
		}
	}
	else if (event->type == SDL_MOUSEMOTION)
	{
		if (tx != old_map_mouse_x || ty != old_map_mouse_y)
		{
			map_redraw_flag = 1;
			old_map_mouse_x = tx;
			old_map_mouse_y = ty;
		}
	}
}

/**
 * Press the "Walk Here" option in map widget menu. */
static void menu_map_walk_here(widgetdata *widget, int x, int y)
{
	int tx, ty;

	(void) widget;
	(void) x;
	(void) y;

	if (mouse_to_tile_coords(cur_widget[MENU_ID]->x1, cur_widget[MENU_ID]->y1, &tx, &ty))
	{
		send_move_path(tx, ty);
	}
}

/**
 * Press the "Talk To NPC" option in map widget menu. */
static void menu_map_talk_to(widgetdata *widget, int x, int y)
{
	int tx, ty;

	(void) widget;
	(void) x;
	(void) y;

	if (mouse_to_tile_coords(cur_widget[MENU_ID]->x1, cur_widget[MENU_ID]->y1, &tx, &ty))
	{
		send_target(tx, ty);
		send_command("/t_tell hi");
	}
}

/**
 * Render the map widget.
 * @param widget The widget. */
void widget_map_render(widgetdata *widget)
{
	static int gfx_toggle = 0;
	SDL_Rect box;
	int mx, my;

	if (!widget->widgetSF)
	{
		widget->widgetSF = SDL_CreateRGBSurface(get_video_flags(), 850, 600, options.used_video_bpp, 0, 0, 0, 0);
	}

	/* Make sure the map widget is always the last to handle events for. */
	SetPriorityWidget_reverse(widget);

	if (options.zoom != 100)
	{
		int w, h;

		zoomSurfaceSize(widget->widgetSF->w, widget->widgetSF->h, options.zoom / 100.0, options.zoom / 100.0, &w, &h);
		widget->wd = w;
		widget->ht = h;
	}

	/* We recreate the map only when there is a change. */
	if (map_redraw_flag && (!popup_get_visible() || popup_overlay_need_update(popup_get_visible())))
	{
		SDL_FillRect(widget->widgetSF, NULL, 0);
		map_draw_map();
		map_redraw_flag = 0;
		effect_sprites_play();

		if (options.zoom != 100)
		{
			if (zoomed)
			{
				SDL_FreeSurface(zoomed);
			}

			zoomed = zoomSurface(widget->widgetSF, options.zoom / 100.0, options.zoom / 100.0, options.zoom_smooth);
		}
	}

	box.x = widget->x1;
	box.y = widget->y1;

	if (options.zoom == 100)
	{
		SDL_BlitSurface(widget->widgetSF, NULL, ScreenSurface, &box);
	}
	else
	{
		SDL_BlitSurface(zoomed, NULL, ScreenSurface, &box);
	}

	/* The damage numbers */
	play_anims();

	/* Draw warning icons above player */
	if ((gfx_toggle++ & 63) < 25)
	{
		if (options.warning_hp && ((float) cpl.stats.hp / (float) cpl.stats.maxhp) * 100 <= options.warning_hp)
		{
			sprite_blt(Bitmaps[BITMAP_WARN_HP], widget->x1 + 393 * (options.zoom / 100.0), widget->y1 + 298 * (options.zoom / 100.0), NULL, NULL);
		}
	}
	else
	{
		/* Low food */
		if (options.warning_food && ((float) cpl.stats.food / 1000.0f) * 100 <= options.warning_food)
		{
			sprite_blt(Bitmaps[BITMAP_WARN_FOOD], widget->x1 + 390 * (options.zoom / 100.0), widget->y1 + 294 * (options.zoom / 100.0), NULL, NULL);
		}
	}

	/* Holding the right mouse button for some time, create a menu. */
	if (SDL_GetMouseState(&mx, &my) == SDL_BUTTON(SDL_BUTTON_RIGHT) && right_click_ticks != -1 && SDL_GetTicks() - right_click_ticks > 500)
	{
		widgetdata *menu;

		menu = create_menu(mx, my, widget);
		add_menuitem(menu, "Walk Here", &menu_map_walk_here, MENU_NORMAL, 0);
		add_menuitem(menu, "Talk To NPC", &menu_map_talk_to, MENU_NORMAL, 0);
		add_menuitem(menu, "Move Widget", &menu_move_widget, MENU_NORMAL, 0);
		menu_finalize(menu);
		right_click_ticks = -1;
	}
}

/** Tile offsets used in mouse_to_tile_coords(). */
const char tile_off[MAP_TILE_YOFF][MAP_TILE_POS_XOFF] =
{
	"000000000000000000000022221111111111111111111111",
	"000000000000000000002222222211111111111111111111",
	"000000000000000000222222222222111111111111111111",
	"000000000000000022222222222222221111111111111111",
	"000000000000002222222222222222222211111111111111",
	"000000000000222222222222222222222222111111111111",
	"000000000022222222222222222222222222221111111111",
	"000000002222222222222222222222222222222211111111",
	"000000222222222222222222222222222222222222111111",
	"000022222222222222222222222222222222222222221111",
	"002222222222222222222222222222222222222222222211",
	"222222222222222222222222222222222222222222222222",
	"332222222222222222222222222222222222222222222244",
	"333322222222222222222222222222222222222222224444",
	"333333222222222222222222222222222222222222444444",
	"333333332222222222222222222222222222222244444444",
	"333333333322222222222222222222222222224444444444",
	"333333333333222222222222222222222222444444444444",
	"333333333333332222222222222222222244444444444444",
	"333333333333333322222222222222224444444444444444",
	"333333333333333333222222222222444444444444444444",
	"333333333333333333332222222244444444444444444444",
	"333333333333333333333322224444444444444444444444"
};

/**
 * Transform mouse coordinates to tile coordinates on map.
 *
 * Both 'tx' and 'ty' can be NULL, which is useful if you only want to
 * check if the mouse is over a valid map tile.
 * @param mx Mouse X.
 * @param my Mouse Y.
 * @param[out] tx Will contain tile X, unless function returns 0.
 * @param[out] ty Will contain tile Y, unless function returns 0.
 * @retval 1 Successfully transformed mouse coordinates into tile ones.
 * @retval 0 Failed to transform the coordinates; 'tx' and 'ty' are
 * left untouched. */
int mouse_to_tile_coords(int mx, int my, int *tx, int *ty)
{
	int x, y, xpos, ypos;

	/* Adjust mouse x/y, making it look as if the map was drawn from
	 * top left corner, in order to simplify comparisons below. */
	mx -= (MAP_START_XOFF * (options.zoom / 100.0)) + cur_widget[MAP_ID]->x1;
	my -= (MAP_START_YOFF * (options.zoom / 100.0)) + cur_widget[MAP_ID]->y1;

	/* Go through all the map squares. */
	for (x = options.map_size_x - 1; x >= 0; x--)
	{
		for (y = options.map_size_y - 1; y >= 0; y--)
		{
			/* X/Y position of the map square. */
			xpos = (x * MAP_TILE_YOFF - y * MAP_TILE_YOFF) * (options.zoom / 100.0);
			ypos = (x * MAP_TILE_XOFF + y * MAP_TILE_XOFF) * (options.zoom / 100.0);

			if (the_map.cells[x][y].faces[1])
			{
				ypos = (ypos - (the_map.cells[x][y].height[1]) * (options.zoom / 100.0)) + (the_map.cells[options.map_size_x - (options.map_size_x / 2) - 1][options.map_size_y - (options.map_size_y / 2) - 1].height[1]) * (options.zoom / 100.0);
			}

			/* See if this square matches our 48x24 box shape. */
			if (mx >= xpos && mx < xpos + (MAP_TILE_POS_XOFF * (options.zoom / 100.0)) && my >= ypos && my < ypos + (MAP_TILE_YOFF * (options.zoom / 100.0)))
			{
				/* See if the square matches isometric 48x24 tile. */
				if (tile_off[(int) ((my - ypos) / (options.zoom / 100.0))][(int) ((mx - xpos) / (options.zoom / 100.0))] == '2')
				{
					if (tx)
					{
						*tx = x;
					}

					if (ty)
					{
						*ty = y;
					}

					return 1;
				}
			}
		}
	}

	return 0;
}
