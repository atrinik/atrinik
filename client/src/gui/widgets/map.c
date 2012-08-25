/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Crossfire (Multiplayer game for X-windows).                 *
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
 * Implements map type widgets.
 *
 * @author Alex Tokar */

#include <global.h>

static struct Map the_map;
static SDL_Surface *zoomed = NULL;

/** Current shown map: mapname, length, etc */
_mapdata MapData;

_multi_part_obj MultiArchs[16];

/** Holds coordinates of the last map square the mouse was over. */
static int old_map_mouse_x = 0, old_map_mouse_y = 0;
/**
 * When the right button was pressed on the map widget. -1 = not
 * pressed. */
static int right_click_ticks = -1;

/* Load the multi arch offsets */
void load_mapdef_dat(void)
{
	FILE *stream;
	int i, ii, x, y, d[32];
	char line[256];

	if (!(stream = fopen_wrapper(ARCHDEF_FILE, "r")))
	{
		logger_print(LOG(BUG), "Can't find file %s", ARCHDEF_FILE);
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
 * Clear the map. */
void clear_map(void)
{
	memset(&the_map, 0, sizeof(Map));
	sound_ambient_clear();
}

/**
 * Scroll the map.
 * @param dx X.
 * @param dy Y. */
void display_mapscroll(int dx, int dy)
{
	int x, y;
	struct Map newmap;

	for (x = 0; x < setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH); x++)
	{
		for (y = 0; y < setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT); y++)
		{
			if (x + dx < 0 || x + dx >= setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH) || y + dy < 0 || y + dy >= setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT))
			{
				memset((char *) &(newmap.cells[x][y]), 0, sizeof(struct MapCell));
			}
			else
			{
				memcpy((char *) &(newmap.cells[x][y]), (char *) &(the_map.cells[x + dx][y + dy]), sizeof(struct MapCell));
			}
		}
	}

	memcpy((char *) &the_map, (char *) &newmap, sizeof(struct Map));
	sound_ambient_mapcroll(dx, dy);
	cpl.target_object_index = 0;
}

/**
 * Update map's name.
 * @param name New map name. */
void update_map_name(const char *name)
{
	strncpy(MapData.name_new, name, sizeof(MapData.name_new) - 1);
	MapData.name_new[sizeof(MapData.name_new) - 1] = '\0';
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
static int calc_map_cell_height(int x, int y, int sub_layer)
{
	if (x >= 0 && x < setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH) && y >= 0 && y < setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT))
	{
		return the_map.cells[x][y].height[GET_MAP_LAYER(LAYER_FLOOR, sub_layer)];
	}

	return 0;
}

#define MAX_STRETCH 8
#define MAX_STRETCH_DIAG 12

/**
 * Align tile stretch based on X/Y.
 * @param x X position.
 * @param y Y position. */
static void align_tile_stretch(int x, int y, int sub_layer)
{
	uint8 top, bottom, right, left, min_ht;
	uint32 h;
	int nw_height, n_height, ne_height, sw_height, s_height, se_height, w_height, e_height, my_height;

	if (x < 0 || y < 0 || x >= setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH) || y >= setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT))
	{
		return;
	}

	nw_height = calc_map_cell_height(x - 1, y - 1, sub_layer);
	n_height = calc_map_cell_height(x, y - 1, sub_layer);
	ne_height = calc_map_cell_height(x + 1, y - 1, sub_layer);
	sw_height = calc_map_cell_height(x - 1, y + 1, sub_layer);
	s_height = calc_map_cell_height(x, y + 1, sub_layer);
	se_height = calc_map_cell_height(x + 1, y + 1, sub_layer);
	w_height = calc_map_cell_height(x - 1, y, sub_layer);
	e_height = calc_map_cell_height(x + 1, y, sub_layer);
	my_height = calc_map_cell_height(x, y, sub_layer);

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
	the_map.cells[x][y].stretch[sub_layer] = h;
}

/**
 * Adjust the tile stretch of a map.
 *
 * Goes through the whole map and for each coordinate calls align_tile_stretch()
 * in all directions. This is done to fix any inconsistencies, since the map
 * command doesn't send us the whole map all over again, but only new/changes
 * parts. */
void adjust_tile_stretch(void)
{
	int x, y, sub_layer;

	for (x = 0; x < setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH); x++)
	{
		for (y = 0; y < setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT); y++)
		{
			for (sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++)
			{
				align_tile_stretch(x - 1, y - 1, sub_layer);
				align_tile_stretch(x , y - 1, sub_layer);
				align_tile_stretch(x + 1, y - 1, sub_layer);
				align_tile_stretch(x + 1, y, sub_layer);

				align_tile_stretch(x + 1, y + 1, sub_layer);
				align_tile_stretch(x, y + 1, sub_layer);
				align_tile_stretch(x - 1, y + 1, sub_layer);
				align_tile_stretch(x - 1, y, sub_layer);

				align_tile_stretch(x, y, sub_layer);
			}
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
 * @param rotate Rotation in degrees.
 * @param infravision Whether to show the object in red. */
void map_set_data(int x, int y, int layer, sint16 face, uint8 quick_pos, uint8 obj_flags, const char *name, const char *name_color, sint16 height, uint8 probe, sint16 zoom_x, sint16 zoom_y, sint16 align, uint8 draw_double, uint8 alpha, sint16 rotate, uint8 infravision, uint32 target_object_count, uint8 target_is_friend)
{
	the_map.cells[x][y].faces[layer] = face;
	the_map.cells[x][y].flags[layer] = obj_flags;

	the_map.cells[x][y].probe[layer] = probe;
	the_map.cells[x][y].quick_pos[layer] = quick_pos;

	strncpy(the_map.cells[x][y].pcolor[layer], name_color, sizeof(the_map.cells[x][y].pcolor[layer]) - 1);
	the_map.cells[x][y].pcolor[layer][sizeof(the_map.cells[x][y].pcolor[layer]) - 1] = '\0';

	strncpy(the_map.cells[x][y].pname[layer], name, sizeof(the_map.cells[x][y].pname[layer]) - 1);
	the_map.cells[x][y].pname[layer][sizeof(the_map.cells[x][y].pname[layer]) - 1] = '\0';

	the_map.cells[x][y].height[layer] = height;
	the_map.cells[x][y].zoom_x[layer] = zoom_x;
	the_map.cells[x][y].zoom_y[layer] = zoom_y;
	the_map.cells[x][y].align[layer] = align;
	the_map.cells[x][y].draw_double[layer] = draw_double;
	the_map.cells[x][y].alpha[layer] = alpha;
	the_map.cells[x][y].rotate[layer] = rotate;
	the_map.cells[x][y].infravision[layer] = infravision;
	the_map.cells[x][y].target_object_count[layer] = target_object_count;
	the_map.cells[x][y].target_is_friend[layer] = target_is_friend;
}

/**
 * Clear map's cell.
 * @param x X of the cell.
 * @param y Y of the cell. */
void map_clear_cell(int x, int y)
{
	int layer;

	the_map.cells[x][y].darkness = 0;

	for (layer = 0; layer < NUM_REAL_LAYERS; layer++)
	{
		the_map.cells[x][y].faces[layer] = 0;
		the_map.cells[x][y].flags[layer] = 0;
		the_map.cells[x][y].probe[layer] = 0;
		the_map.cells[x][y].quick_pos[layer] = 0;
		the_map.cells[x][y].pname[layer][0] = '\0';
		the_map.cells[x][y].height[layer] = 0;
		the_map.cells[x][y].zoom_x[layer] = 0;
		the_map.cells[x][y].zoom_y[layer] = 0;
		the_map.cells[x][y].align[layer] = 0;
		the_map.cells[x][y].rotate[layer] = 0;
		the_map.cells[x][y].infravision[layer] = 0;
		the_map.cells[x][y].target_object_count[layer] = 0;
		the_map.cells[x][y].target_is_friend[layer] = 0;
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
 * Get the height of the topmost floor on the specified square.
 * @param x X position.
 * @param y Y position.
 * @return The height. */
static int get_top_floor_height(int x, int y)
{
	int top_height, height, sub_layer;

	top_height = 0;

	for (sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++)
	{
		height = the_map.cells[x][y].height[GET_MAP_LAYER(LAYER_FLOOR, sub_layer)];

		if (height > top_height)
		{
			top_height = height;
		}
	}

	return top_height;
}

/**
 * Draw a single object on the map.
 * @param x X position of the object.
 * @param y Y position of the object.
 * @param k Layer.
 * @param player_height_offset Player's height offset.
 * @param[out] target_cell Where to store the map cell the player's
 * target is at, if any.
 * @param[out] target_layer Where to store the layer the player's target
 * is at, if any.
 * @param target_rect Where to store coordinate info for target. */
static void draw_map_object(int x, int y, int layer, int sub_layer, int player_height_offset, struct MapCell **target_cell, int *target_layer, SDL_Rect *target_rect)
{
	struct MapCell *map = &the_map.cells[x][y];
	sprite_struct *face_sprite;
	int ypos, xpos;
	int xl, yl, temp;
	int xml, xmpos, xtemp = 0;
	uint16 face;
	int mid, mnr;
	uint32 stretch, flags;
	int bitmap_h, bitmap_w;
	int zoom_x, zoom_y;
	uint8 dark_level, alpha;

	xpos = MAP_START_XOFF + x * MAP_TILE_YOFF - y * MAP_TILE_YOFF;
	ypos = MAP_START_YOFF + x * MAP_TILE_XOFF + y * MAP_TILE_XOFF;
	face = map->faces[GET_MAP_LAYER(layer, sub_layer)];

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

	zoom_x = map->zoom_x[GET_MAP_LAYER(layer, sub_layer)];
	zoom_y = map->zoom_y[GET_MAP_LAYER(layer, sub_layer)];

	if (map->rotate[GET_MAP_LAYER(layer, sub_layer)])
	{
		rotozoomSurfaceSizeXY(bitmap_w, bitmap_h, map->rotate[GET_MAP_LAYER(layer, sub_layer)], zoom_x ? zoom_x / 100.0 : 1.0, zoom_y ? zoom_y / 100.0 : 1.0, &bitmap_w, &bitmap_h);
	}
	else if ((zoom_x && zoom_x != 100) || (zoom_y && zoom_y != 100))
	{
		zoomSurfaceSize(bitmap_w, bitmap_h, zoom_x ? zoom_x / 100.0 : 1.0, zoom_y ? zoom_y / 100.0 : 1.0, &bitmap_w, &bitmap_h);
	}

	/* We have a set quick_pos = multi tile */
	if (map->quick_pos[GET_MAP_LAYER(layer, sub_layer)])
	{
		mnr = map->quick_pos[GET_MAP_LAYER(layer, sub_layer)];
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

	if (map->align[GET_MAP_LAYER(layer, sub_layer)])
	{
		xl += map->align[GET_MAP_LAYER(layer, sub_layer)];
	}

	/* Draw the face in the darkness level the tile pos has */
	temp = map->darkness;

	if (temp == 210)
	{
		dark_level = 0;
	}
	else if (temp == 180)
	{
		dark_level = 1;
	}
	else if (temp == 150)
	{
		dark_level = 2;
	}
	else if (temp == 120)
	{
		dark_level = 3;
	}
	else if (temp == 90)
	{
		dark_level = 4;
	}
	else if (temp == 60)
	{
		dark_level = 5;
	}
	else if (temp == 0)
	{
		dark_level = 7;
	}
	else
	{
		dark_level = 6;
	}

	flags = 0;
	alpha = 0;
	stretch = 0;

	if (map->infravision[GET_MAP_LAYER(layer, sub_layer)])
	{
		flags |= SPRITE_FLAG_RED;
	}
	else
	{
		flags |= SPRITE_FLAG_DARK;
	}

	if (map->alpha[GET_MAP_LAYER(layer, sub_layer)])
	{
		alpha = map->alpha[GET_MAP_LAYER(layer, sub_layer)];
	}

	if (layer <= 2 && map->stretch[sub_layer])
	{
		stretch = map->stretch[sub_layer];
	}

	if (layer == LAYER_LIVING || layer == LAYER_EFFECT || layer == LAYER_ITEM || layer == LAYER_ITEM2)
	{
		yl -= get_top_floor_height(x, y);
	}
	else
	{
		yl -= map->height[GET_MAP_LAYER(LAYER_FLOOR, sub_layer)];
	}

	yl += player_height_offset;

	if (layer > 1)
	{
		yl -= map->height[GET_MAP_LAYER(layer, sub_layer)];
	}

	map_sprite_show(cur_widget[MAP_ID]->surface, xl, yl, NULL, face_sprite, flags, dark_level, alpha, stretch, map->zoom_x[GET_MAP_LAYER(layer, sub_layer)], map->zoom_y[GET_MAP_LAYER(layer, sub_layer)], map->rotate[GET_MAP_LAYER(layer, sub_layer)]);

	/* Double faces are shown twice, one above the other, when not lower
	 * on the screen than the player. This simulates high walls without
	 * obscuring the user's view. */
	if (map->draw_double[GET_MAP_LAYER(layer, sub_layer)])
	{
		map_sprite_show(cur_widget[MAP_ID]->surface, xl, yl - 22, NULL, face_sprite, flags, dark_level, alpha, stretch, map->zoom_x[GET_MAP_LAYER(layer, sub_layer)], map->zoom_y[GET_MAP_LAYER(layer, sub_layer)], map->rotate[GET_MAP_LAYER(layer, sub_layer)]);
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
	if (setting_get_int(OPT_CAT_MAP, OPT_PLAYER_NAMES) && map->pname[GET_MAP_LAYER(layer, sub_layer)][0])
	{
		if (setting_get_int(OPT_CAT_MAP, OPT_PLAYER_NAMES) == 1 || (setting_get_int(OPT_CAT_MAP, OPT_PLAYER_NAMES) == 2 && strncasecmp(map->pname[GET_MAP_LAYER(layer, sub_layer)], cpl.name, strlen(map->pname[GET_MAP_LAYER(layer, sub_layer)]))) || (setting_get_int(OPT_CAT_MAP, OPT_PLAYER_NAMES) == 3 && !strncasecmp(map->pname[GET_MAP_LAYER(layer, sub_layer)], cpl.name, strlen(map->pname[GET_MAP_LAYER(layer, sub_layer)]))))
		{
			text_show(cur_widget[MAP_ID]->surface, FONT_SANS9, map->pname[GET_MAP_LAYER(layer, sub_layer)], xmpos + xtemp + (xml - xtemp * 2) / 2 - text_get_width(FONT_SANS9, map->pname[GET_MAP_LAYER(layer, sub_layer)], 0) / 2 - 2, yl - 24, map->pcolor[GET_MAP_LAYER(layer, sub_layer)], TEXT_OUTLINE, NULL);
		}
	}

	/* Perhaps the object has a marked effect, show it. */
	if (map->flags[GET_MAP_LAYER(layer, sub_layer)])
	{
		if (map->flags[GET_MAP_LAYER(layer, sub_layer)] & FFLAG_SLEEP)
		{
			surface_show_effects(cur_widget[MAP_ID]->surface, xl + bitmap_w / 2, yl - 5, NULL, TEXTURE_CLIENT("sleep"), alpha, stretch, map->zoom_x[GET_MAP_LAYER(layer, sub_layer)], map->zoom_y[GET_MAP_LAYER(layer, sub_layer)], map->rotate[GET_MAP_LAYER(layer, sub_layer)]);
		}

		if (map->flags[GET_MAP_LAYER(layer, sub_layer)] & FFLAG_CONFUSED)
		{
			surface_show_effects(cur_widget[MAP_ID]->surface, xl + bitmap_w / 2 - 1, yl - 4, NULL, TEXTURE_CLIENT("confused"), alpha, stretch, map->zoom_x[GET_MAP_LAYER(layer, sub_layer)], map->zoom_y[GET_MAP_LAYER(layer, sub_layer)], map->rotate[GET_MAP_LAYER(layer, sub_layer)]);
		}

		if (map->flags[GET_MAP_LAYER(layer, sub_layer)] & FFLAG_SCARED)
		{
			surface_show_effects(cur_widget[MAP_ID]->surface, xl + bitmap_w / 2 + 10, yl - 4, NULL, TEXTURE_CLIENT("scared"), alpha, stretch, map->zoom_x[GET_MAP_LAYER(layer, sub_layer)], map->zoom_y[GET_MAP_LAYER(layer, sub_layer)], map->rotate[GET_MAP_LAYER(layer, sub_layer)]);
		}

		if (map->flags[GET_MAP_LAYER(layer, sub_layer)] & FFLAG_BLINDED)
		{
			surface_show_effects(cur_widget[MAP_ID]->surface, xl + bitmap_w / 2 + 3, yl - 6, NULL, TEXTURE_CLIENT("blind"), alpha, stretch, map->zoom_x[GET_MAP_LAYER(layer, sub_layer)], map->zoom_y[GET_MAP_LAYER(layer, sub_layer)], map->rotate[GET_MAP_LAYER(layer, sub_layer)]);
		}

		if (map->flags[GET_MAP_LAYER(layer, sub_layer)] & FFLAG_PARALYZED)
		{
			surface_show_effects(cur_widget[MAP_ID]->surface, xl + bitmap_w / 2 + 3, yl + 3, NULL, TEXTURE_CLIENT("paralyzed"), alpha, stretch, map->zoom_x[GET_MAP_LAYER(layer, sub_layer)], map->zoom_y[GET_MAP_LAYER(layer, sub_layer)], map->rotate[GET_MAP_LAYER(layer, sub_layer)]);
		}
	}

	if (map->probe[GET_MAP_LAYER(layer, sub_layer)])
	{
		*target_cell = map;
		*target_layer = GET_MAP_LAYER(layer, sub_layer);
		target_rect->x = xmpos + xtemp;
		target_rect->y = yl - 9;
		target_rect->w = (xml - xtemp * 2);
		target_rect->h = 1;
	}
}

/**
 * Draw the map. */
void map_draw_map(void)
{
	int player_height_offset;
	int x, y, layer, sub_layer, target_layer;
	struct MapCell *target_cell;
	SDL_Rect target_rect;
	int tx, ty;

	player_height_offset = get_top_floor_height(setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH) - (setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH) / 2) - 1, setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT) - (setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT) / 2) - 1);
	target_cell = NULL;

	/* Draw floor and fmasks. */
	for (x = 0; x < setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH); x++)
	{
		for (y = 0; y < setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT); y++)
		{
			draw_map_object(x, y, LAYER_FLOOR, 0, player_height_offset, &target_cell, &target_layer, &target_rect);
			draw_map_object(x, y, LAYER_FMASK, 0, player_height_offset, &target_cell, &target_layer, &target_rect);
		}
	}

	/* Now draw everything else. */
	for (x = 0; x < setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH); x++)
	{
		for (y = 0; y < setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT); y++)
		{
			for (layer = LAYER_FLOOR; layer <= NUM_LAYERS; layer++)
			{
				for (sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++)
				{
					if (sub_layer == 0 && (layer == LAYER_FLOOR || layer == LAYER_FMASK))
					{
						continue;
					}

					draw_map_object(x, y, layer, sub_layer, player_height_offset, &target_cell, &target_layer, &target_rect);
				}
			}
		}
	}

	if (widget_mouse_event.owner == cur_widget[MAP_ID] && mouse_to_tile_coords(cursor_x, cursor_y, &tx, &ty))
	{
		map_draw_one(tx, ty, TEXTURE_CLIENT("square_highlight"));
	}

	if (target_cell)
	{
		const char *hp_color;

		if (cpl.target_hp > 90)
		{
			hp_color = COLOR_GREEN;
		}
		else if (cpl.target_hp > 75)
		{
			hp_color = COLOR_DGOLD;
		}
		else if (cpl.target_hp > 50)
		{
			hp_color = COLOR_HGOLD;
		}
		else if (cpl.target_hp > 25)
		{
			hp_color = COLOR_YELLOW;
		}
		else if (cpl.target_hp > 10)
		{
			hp_color = COLOR_ORANGE;
		}
		else
		{
			hp_color = COLOR_RED;
		}

		if (!(setting_get_int(OPT_CAT_MAP, OPT_PLAYER_NAMES) && target_cell->pname[target_layer][0]))
		{
			text_show(cur_widget[MAP_ID]->surface, FONT_SANS9, cpl.target_name, target_rect.x + target_rect.w / 2 - text_get_width(FONT_SANS9, cpl.target_name, 0) / 2, target_rect.y - 15, cpl.target_color, TEXT_OUTLINE, NULL);
		}

		rectangle_create(cur_widget[MAP_ID]->surface, target_rect.x - 2, target_rect.y - 2, 1, 5, hp_color);
		rectangle_create(cur_widget[MAP_ID]->surface, target_rect.x - 2, target_rect.y - 2, 3, 1, hp_color);
		rectangle_create(cur_widget[MAP_ID]->surface, target_rect.x - 2, target_rect.y + 2, 3, 1, hp_color);
		rectangle_create(cur_widget[MAP_ID]->surface, target_rect.x + target_rect.w + 1, target_rect.y - 2, 1, 5, hp_color);
		rectangle_create(cur_widget[MAP_ID]->surface, target_rect.x + target_rect.w - 1, target_rect.y - 2, 3, 1, hp_color);
		rectangle_create(cur_widget[MAP_ID]->surface, target_rect.x + target_rect.w - 1, target_rect.y + 2, 3, 1, hp_color);

		target_rect.w = MAX(1, MIN(100, (int) (((double) target_rect.w / 100.0) * (double) target_cell->probe[target_layer])));
		rectangle_create(cur_widget[MAP_ID]->surface, target_rect.x, target_rect.y, target_rect.w, target_rect.h, hp_color);
	}
}

/**
 * Draw one sprite on map.
 * @param x X position.
 * @param y Y position.
 * @param surface What to draw. */
void map_draw_one(int x, int y, SDL_Surface *surface)
{
	int xpos = MAP_START_XOFF + x * MAP_TILE_YOFF - y * MAP_TILE_YOFF;
	int ypos = (MAP_START_YOFF + x * MAP_TILE_XOFF + y * MAP_TILE_XOFF) + MAP_TILE_POS_YOFF - surface->h;

	if (surface->w > MAP_TILE_POS_XOFF)
	{
		xpos -= (surface->w - MAP_TILE_POS_XOFF) / 2;
	}

	if (the_map.cells[x][y].faces[1])
	{
		ypos = (ypos - get_top_floor_height(x, y)) + get_top_floor_height(setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH) - (setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH) / 2) - 1, setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT) - (setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT) / 2) - 1);
	}

	surface_show_effects(cur_widget[MAP_ID]->surface, xpos, ypos, NULL, surface, 0, 0, 0, 0, 0);
}

/**
 * Send a command to move the player to the specified square.
 * @param tx Square X position.
 * @param ty Square Y position. */
static void send_move_path(int tx, int ty)
{
	packet_struct *packet;

	packet = packet_new(SERVER_CMD_MOVE_PATH, 8, 0);
	packet_append_uint8(packet, tx);
	packet_append_uint8(packet, ty);
	socket_send_packet(packet);
}

/**
 * Send a command to target an NPC.
 * @param tx NPC's X position.
 * @param ty NPC's Y position.
 * @param count NPC's UID. */
static void send_target(int x, int y, uint32 count)
{
	packet_struct *packet;

	packet = packet_new(SERVER_CMD_TARGET, 16, 0);

	if (x == -1 && y == -1)
	{
		packet_append_uint8(packet, CMD_TARGET_CLEAR);
	}
	else
	{
		packet_append_uint8(packet, CMD_TARGET_MAPXY);
		packet_append_uint8(packet, x);
		packet_append_uint8(packet, y);
		packet_append_uint32(packet, count);
	}

	socket_send_packet(packet);
}

static int map_target_cmp(const void *a, const void *b)
{
	int x1, y1, x2, y2;
	unsigned long dist1, dist2;

	x1 = ((map_target_struct *) a)->x - (setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH) / 2);
	y1 = ((map_target_struct *) a)->y - (setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT) / 2);

	x2 = ((map_target_struct *) b)->x - (setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH) / 2);
	y2 = ((map_target_struct *) b)->y - (setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT) / 2);

	dist1 = isqrt(x1 * x1 + y1 * y1);
	dist2 = isqrt(x2 * x2 + y2 * y2);

	if (dist1 < dist2)
	{
		return -1;
	}
	else if (dist1 > dist2)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

void map_target_handle(uint8 is_friend)
{
	int x, y, layer;
	UT_array *targets;
	UT_icd icd = {sizeof(map_target_struct), NULL, NULL, NULL};
	map_target_struct *p;
	uint32 curr_target;

	if (cpl.target_is_friend != is_friend)
	{
		cpl.target_object_index = 0;
	}

	utarray_new(targets, &icd);
	curr_target = 0;

	for (x = 0; x < setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH); x++)
	{
		for (y = 0; y < setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT); y++)
		{
			for (layer = 0; layer < NUM_REAL_LAYERS; layer++)
			{
				if (the_map.cells[x][y].faces[layer] && the_map.cells[x][y].target_object_count[layer] && the_map.cells[x][y].target_is_friend[layer] == is_friend)
				{
					map_target_struct target;

					target.count = the_map.cells[x][y].target_object_count[layer];
					target.x = x;
					target.y = y;
					utarray_push_back(targets, &target);

					if (the_map.cells[x][y].probe[layer])
					{
						curr_target = target.count;
					}
				}
			}
		}
	}

	utarray_sort(targets, map_target_cmp);

	if (cpl.target_object_index >= utarray_len(targets))
	{
		cpl.target_object_index = 0;
	}

	if (cpl.target_object_index == 0)
	{
		p = (map_target_struct *) utarray_front(targets);

		if (p && p->count == curr_target)
		{
			cpl.target_object_index++;
		}
	}

	p = (map_target_struct *) utarray_eltptr(targets, cpl.target_object_index);

	if (p)
	{
		send_target(p->x, p->y, p->count);
		cpl.target_object_index++;
	}
	else if (cpl.target_is_friend != is_friend)
	{
		send_target(-1, -1, 0);
	}

	cpl.target_is_friend = is_friend;

	utarray_free(targets);
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
	mx -= (MAP_START_XOFF * (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0)) + cur_widget[MAP_ID]->x;
	my -= (MAP_START_YOFF * (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0)) + cur_widget[MAP_ID]->y;

	/* Go through all the map squares. */
	for (x = setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH) - 1; x >= 0; x--)
	{
		for (y = setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT) - 1; y >= 0; y--)
		{
			/* X/Y position of the map square. */
			xpos = (x * MAP_TILE_YOFF - y * MAP_TILE_YOFF) * (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0);
			ypos = (x * MAP_TILE_XOFF + y * MAP_TILE_XOFF) * (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0);

			if (the_map.cells[x][y].faces[1])
			{
				ypos = (ypos - (get_top_floor_height(x, y)) * (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0)) + (get_top_floor_height(setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH) - (setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH) / 2) - 1, setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT) - (setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT) / 2) - 1)) * (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0);
			}

			/* See if this square matches our 48x24 box shape. */
			if (mx >= xpos && mx < xpos + (MAP_TILE_POS_XOFF * (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0)) && my >= ypos && my < ypos + (MAP_TILE_YOFF * (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0)))
			{
				/* See if the square matches isometric 48x24 tile. */
				if (tile_off[(int) ((my - ypos) / (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0))][(int) ((mx - xpos) / (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0))] == '2')
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

/**
 * Press the "Walk Here" option in map widget menu. */
static void menu_map_walk_here(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
	int tx, ty;

	if (mouse_to_tile_coords(cur_widget[MENU_ID]->x, cur_widget[MENU_ID]->y, &tx, &ty))
	{
		send_move_path(tx, ty);
	}
}

/**
 * Press the "Talk To NPC" option in map widget menu. */
static void menu_map_talk_to(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
	int tx, ty;

	if (mouse_to_tile_coords(cur_widget[MENU_ID]->x, cur_widget[MENU_ID]->y, &tx, &ty))
	{
		send_target(tx, ty, 0);
		keybind_process_command("?HELLO");
	}
}

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
	static int gfx_toggle = 0;
	SDL_Rect box;
	int mx, my;

	if (!widget->surface)
	{
		widget->surface = SDL_CreateRGBSurface(get_video_flags(), 850, 600, video_get_bpp(), 0, 0, 0, 0);
	}

	/* Make sure the map widget is always the last to handle events for. */
	SetPriorityWidget_reverse(widget);

	if (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) != 100)
	{
		int w, h;

		zoomSurfaceSize(widget->surface->w, widget->surface->h, setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0, setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0, &w, &h);
		widget->w = w;
		widget->h = h;
	}

	/* We recreate the map only when there is a change. */
	if (map_redraw_flag)
	{
		SDL_FillRect(widget->surface, NULL, 0);
		map_draw_map();
		map_redraw_flag = 0;
		effect_sprites_play();

		if (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) != 100)
		{
			if (zoomed)
			{
				SDL_FreeSurface(zoomed);
			}

			zoomed = zoomSurface(widget->surface, setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0, setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0, setting_get_int(OPT_CAT_CLIENT, OPT_ZOOM_SMOOTH));
		}
	}

	box.x = widget->x;
	box.y = widget->y;

	if (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) == 100)
	{
		SDL_BlitSurface(widget->surface, NULL, ScreenSurface, &box);
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
		if (setting_get_int(OPT_CAT_MAP, OPT_HEALTH_WARNING) && ((float) cpl.stats.hp / (float) cpl.stats.maxhp) * 100 <= setting_get_int(OPT_CAT_MAP, OPT_HEALTH_WARNING))
		{
			surface_show(ScreenSurface, widget->x + 393 * (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0), widget->y + 298 * (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0), NULL, TEXTURE_CLIENT("warn_hp"));
		}
	}
	else
	{
		/* Low food */
		if (setting_get_int(OPT_CAT_MAP, OPT_FOOD_WARNING) && ((float) cpl.stats.food / 1000.0f) * 100 <= setting_get_int(OPT_CAT_MAP, OPT_FOOD_WARNING))
		{
			surface_show(ScreenSurface, widget->x + 390 * (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0), widget->y + 294 * (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0), NULL, TEXTURE_CLIENT("warn_food"));
		}
	}

	/* Process message animations */
	if (msg_anim.message[0] != '\0')
	{
		if ((LastTick - msg_anim.tick) < 3000)
		{
			int bmoff = (int) ((50.0f / 3.0f) * ((float) (LastTick - msg_anim.tick) / 1000.0f) * ((float) (LastTick - msg_anim.tick) / 1000.0f) + ((int) (150.0f * ((float) (LastTick - msg_anim.tick) / 3000.0f)))), y_offset = 0;
			char *msg = strdup(msg_anim.message), *cp;

			cp = strtok(msg, "\n");

			while (cp)
			{
				text_show(ScreenSurface, FONT_SERIF16, cp, widget->x + widget->surface->w / 2 - text_get_width(FONT_SERIF16, cp, TEXT_OUTLINE) / 2, widget->y + 300 - bmoff + y_offset, msg_anim.color, TEXT_OUTLINE | TEXT_MARKUP, NULL);
				y_offset += FONT_HEIGHT(FONT_SERIF16);
				cp = strtok(NULL, "\n");
			}

			free(msg);
		}
		else
		{
			msg_anim.message[0] = '\0';
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

/** @copydoc widgetdata::event_func */
static int widget_event(widgetdata *widget, SDL_Event *event)
{
	int tx, ty;

	if (!EVENT_IS_MOUSE(event))
	{
		return 0;
	}

	/* Check if the mouse is in play field. */
	if (!mouse_to_tile_coords(event->motion.x, event->motion.y, &tx, &ty))
	{
		return 0;
	}

	if (event->type == SDL_MOUSEBUTTONUP)
	{
		/* Send target command if we released the right button in time;
		 * otherwise the widget menu will be created. */
		if (event->button.button == SDL_BUTTON_RIGHT && SDL_GetTicks() - right_click_ticks < 500)
		{
			send_target(tx, ty, 0);
		}

		right_click_ticks = -1;
		return 1;
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

		return 1;
	}
	else if (event->type == SDL_MOUSEMOTION)
	{
		if (tx != old_map_mouse_x || ty != old_map_mouse_y)
		{
			map_redraw_flag = 1;
			old_map_mouse_x = tx;
			old_map_mouse_y = ty;

			return 1;
		}
	}

	return 0;
}

/**
 * Initialize one map widget. */
void widget_map_init(widgetdata *widget)
{
	widget->draw_func = widget_draw;
	widget->event_func = widget_event;
	widget->menu_handle_func = NULL;

	SetPriorityWidget_reverse(widget);
}
