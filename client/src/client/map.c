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

_multi_part_obj MultiArchs[16];

/* Load the multi arch offsets */
void load_mapdef_dat()
{
	FILE *stream;
	int i, ii, x, y, d[32];
	char line[256];

	if (!(stream = fopen_wrapper(ARCHDEF_FILE, "r")))
	{
		LOG(LOG_ERROR, "ERROR: Can't find file %s\n", ARCHDEF_FILE);
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

void clear_map()
{
	memset(&the_map, 0, sizeof(Map));
}

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

void map_draw_map_clear()
{
	int ypos, xpos, x, y;

	for (y = 0; y < MapStatusY; y++)
	{
		for (x = 0; x < MapStatusX; x++)
		{
			xpos = options.mapstart_x + x * MAP_TILE_YOFF - y * MAP_TILE_YOFF;
			ypos = options.mapstart_y + x * MAP_TILE_XOFF + y * MAP_TILE_XOFF;
			sprite_blt_map(Bitmaps[BITMAP_BLACKTILE], xpos, ypos, NULL, NULL, 0);
		}
	}
}

void InitMapData(char *name, int xl, int yl, int px, int py, char *bg_music)
{
	int music_fade = 0;

	if (strcmp(bg_music, "no_music"))
	{
		strcpy(MapData.music, bg_music);

		if (init_media_tag(bg_music))
			music_fade = 1;
	}

	/* There was no music tag or playon tag in this map - fade out */
	if (!music_fade)
	{
		/* Now an interesting problem - when we have some seconds before a fadeout
		 * to a file (and not to "mute") and we want mute now - is it possible that
		 * the mixer callback is called in a different thread? And then this thread
		 * stuck BEHIND the music_new.flag = 1 check - then the fadeout of this mute
		 * will drop whatever - the callback will play the old file.
		 * That the classic thread/semphore problem. */
		sound_fadeout_music(0);
	}

	if (name)
	{
		strcpy(MapData.name, name);
		/* Calculate new width of map name widget */
		cur_widget[MAPNAME_ID].wd = get_string_pixel_length(name, &BigFont);
	}

	if (xl != -1)
		MapData.xlen = xl;

	if (yl != -1)
		MapData.ylen = yl;

	if (px != -1)
		MapData.posx = px;

	if (py != -1)
		MapData.posy = py;

	if (xl > 0)
	{
		clear_map();
	}
}

void set_map_ext(int x, int y, int layer,int ext, int probe)
{
	the_map.cells[x][y].ext[layer] = ext;

	if (probe != -1)
	{
		the_map.cells[x][y].probe[layer] = probe;
	}
}

/**
 * Calculate the real X/Y of map, adjusting the coordinates if overflow
 * should happen.
 * @param x X position.
 * @param y Y position.
 * @return The map. */
static struct MapCell *calc_real_map(int x, int y)
{
	if (x < 0)
	{
		x = 0;
	}

	if (y < 0)
	{
		y = 0;
	}

	if (x >= MapStatusX)
	{
		x = MapStatusX - 1;
	}

	if (y >= MapStatusY)
	{
		y = MapStatusY - 1;
	}

	return &the_map.cells[x][y];
}

#define MAX_STRETCH 8
#define MAX_STRETCH_DIAG 12

/**
 * Align tile stretch based on X/Y.
 * @param x X position.
 * @param y Y position. */
void align_tile_stretch(int x, int y)
{
	struct MapCell *map;
	uint8 top, bottom, right, left, min_ht;
	uint32 h;
	int nw_height, n_height, ne_height, sw_height, s_height, se_height, w_height, e_height, my_height;

	if (x < 0 || y < 0 || x >= MapStatusX || y >= MapStatusY)
	{
		return;
	}

	map = calc_real_map(x - 1, y - 1);
	nw_height = map->height;
	map = calc_real_map(x, y - 1);
	n_height = map->height;
	map = calc_real_map(x + 1, y - 1);
	ne_height = map->height;
	map = calc_real_map(x - 1, y + 1);
	sw_height = map->height;
	map = calc_real_map(x, y + 1);
	s_height = map->height;
	map = calc_real_map(x + 1, y + 1);
	se_height = map->height;
	map = calc_real_map(x - 1, y);
	w_height = map->height;
	map = calc_real_map(x + 1, y);
	e_height = map->height;
	map = calc_real_map(x, y);
	my_height = map->height;

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

void set_map_face(int x, int y, int layer, int face, int pos, int ext, char *name, int name_color, sint16 height)
{
	the_map.cells[x][y].faces[layer] = face;

	if (!face)
		ext = 0;

	if (ext != -1)
		the_map.cells[x][y].ext[layer] = ext;

	the_map.cells[x][y].pos[layer] = pos;
	the_map.cells[x][y].pcolor[layer] = name_color;
	strcpy(the_map.cells[x][y].pname[layer], name);

	/* See if we need to stretch this tile */
	if (layer == 0)
	{
		the_map.cells[x][y].height = height;

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

void display_map_clearcell(long x, long y)
{
	int i;

	the_map.cells[x][y].darkness = 0;

	for (i = 0; i < MAXFACES; i++)
	{
		the_map.cells[x][y].pname[i][0] = '\0';
		the_map.cells[x][y].faces[i] = 0;
		the_map.cells[x][y].ext[i] = 0;
		the_map.cells[x][y].pos[i] = 0;
		the_map.cells[x][y].probe[i] = 0;
	}
}

void set_map_darkness(int x, int y, uint8 darkness)
{
	if (darkness != the_map.cells[x][y].darkness)
		the_map.cells[x][y].darkness = darkness;
}

void map_draw_map()
{
	struct MapCell *map;
	_Sprite *face_sprite;
	int ypos, xpos;
	int x, y, k, xl, yl, temp, kk, kt, yt, xt, alpha;
	int xml, xmpos, xtemp = 0;
	uint16 index, index_tmp;
	int mid, mnr;
	_BLTFX bltfx;
	SDL_Rect rect;

	/* We should move this later to a better position, this only for testing here */
	_Sprite player_dummy;
	SDL_Surface bmap;
	int player_posx, player_posy;
	int player_pixx, player_pixy;
	uint32 stretch = 0;
	int player_height_offset;

	player_posx = MapStatusX - (MapStatusX / 2) - 1;
	player_posy = MapStatusY - (MapStatusY / 2) - 1;
	player_pixx = MAP_START_XOFF + player_posx * MAP_TILE_YOFF - player_posy * MAP_TILE_YOFF + 20;
	player_pixy = 0 + player_posx * MAP_TILE_XOFF + player_posy * MAP_TILE_XOFF - 14;
	player_dummy.border_left = -5;
	player_dummy.border_right = 0;
	player_dummy.border_up = 0;
	player_dummy.border_down = -5;
	player_dummy.bitmap = &bmap;
	bmap.h = 33;
	bmap.w = 35;
	player_pixy = (player_pixy + MAP_TILE_POS_YOFF) - bmap.h;
	bltfx.surface = NULL;
	bltfx.alpha = 128;

 	player_height_offset = the_map.cells[player_posx][player_posy].height;

	/* We draw floor & mask as layer wise (layer 0 & 1) */
	for (kk = 0; kk < MAXFACES - 1; kk++)
	{
		for (alpha = 0; alpha < MAP_MAX_SIZE; alpha++)
		{
			xt = yt = -1;

			while (xt < alpha || yt < alpha)
			{
				/* Draw x row from 0 to alpha with y = alpha */
				if (xt < alpha)
				{
					x = ++xt;
					y = alpha;
				}
				/* x row is drawn, now draw y row from 0 to alpha with x = alpha */
				else
				{
					y = ++yt;
					x = alpha;
				}

				/* And we draw layer 2 and 3 at once on a node */
				if (kk < 2)
					kt = kk;
				else
					kt = kk + 1;

				for (k = kk; k <= kt; k++)
				{
					xpos = MAP_START_XOFF + x * MAP_TILE_YOFF - y * MAP_TILE_YOFF;
					ypos = MAP_START_YOFF + x * MAP_TILE_XOFF + y * MAP_TILE_XOFF;

					if (!debug_layer[k])
						continue;

					map = &the_map.cells[x][y];

					if ((index_tmp = map->faces[k]) > 0)
					{
						index = index_tmp &~ 0x8000;
						face_sprite = FaceList[index].sprite;

						if (!face_sprite)
						{
							index = MAX_FACE_TILES - 1;
							face_sprite = FaceList[index].sprite;
						}

						if (face_sprite)
						{
							/* We have a set quick_pos = multi tile */
							if (map->pos[k])
							{
								mnr = map->pos[k];
								mid = mnr >> 4;
								mnr &= 0x0f;
								xml = MultiArchs[mid].xlen;
								yl = ypos - MultiArchs[mid].part[mnr].yoff + MultiArchs[mid].ylen - face_sprite->bitmap->h;

								/* we allow overlapping x borders - we simply center then */
								xl = 0;

								if (face_sprite->bitmap->w > MultiArchs[mid].xlen)
									xl = (MultiArchs[mid].xlen - face_sprite->bitmap->w) >> 1;

								xmpos = xpos - MultiArchs[mid].part[mnr].xoff;
								xl += xmpos;

#if 0
								snprintf(buf, sizeof(buf), "ID:%d NR:%d yoff:%d yl:%d", mid, mnr, MultiArchs[mid].part[mnr].yoff, yl);
								draw_info(buf, COLOR_RED);
#endif
							}
							/* Single tile... */
							else
							{
								/* First, we calc the shift positions */
								xml = MAP_TILE_POS_XOFF;
								yl = (ypos + MAP_TILE_POS_YOFF) - face_sprite->bitmap->h;
								xmpos = xl = xpos;

								if (face_sprite->bitmap->w > MAP_TILE_POS_XOFF)
									xl -= (face_sprite->bitmap->w - MAP_TILE_POS_XOFF) / 2;
							}

							/* Blt the face in the darkness level, the tile pos has */
							temp = map->darkness;

							if (temp == 210)
								bltfx.dark_level = 0;
							else if (temp == 180)
								bltfx.dark_level = 1;
							else if (temp == 150)
								bltfx.dark_level = 2;
							else if (temp == 120)
								bltfx.dark_level = 3;
							else if (temp == 90)
								bltfx.dark_level = 4;
							else if (temp == 60)
								bltfx.dark_level = 5;
							else if (temp == 0)
								bltfx.dark_level = 7;
							else
								bltfx.dark_level = 6;

							/* All done, just blt the face */
							bltfx.flags = 0;

							if (k && ((x > player_posx  && y >= player_posy) || (x >= player_posx && y > player_posy)))
							{
								if (face_sprite && face_sprite->bitmap && k > 1)
								{
									if (sprite_collision(player_pixx, player_pixy, xl, yl, &player_dummy, face_sprite))
										bltfx.flags = BLTFX_FLAG_SRCALPHA;
								}
							}

							if (map->fog_of_war)
								bltfx.flags |= BLTFX_FLAG_FOW;
							else if (cpl.stats.flags & SF_INFRAVISION && index_tmp & 0x8000 && map->darkness < 150)
								bltfx.flags |= BLTFX_FLAG_RED;
							else if (cpl.stats.flags & SF_XRAYS)
								bltfx.flags |= BLTFX_FLAG_GREY;
							else
								bltfx.flags |= BLTFX_FLAG_DARK;

							if (map->ext[k] & FFLAG_INVISIBLE && !(bltfx.flags & BLTFX_FLAG_FOW))
							{
								bltfx.flags &= ~BLTFX_FLAG_DARK;
								bltfx.flags |= BLTFX_FLAG_SRCALPHA | BLTFX_FLAG_GREY;
							}
							else if (map->ext[k] & FFLAG_ETHEREAL && !(bltfx.flags & BLTFX_FLAG_FOW))
							{
								bltfx.flags &= ~BLTFX_FLAG_DARK;
								bltfx.flags |= BLTFX_FLAG_SRCALPHA;
							}

							stretch = 0;

							if (kk < 2 && map->stretch)
							{
								bltfx.flags |= BLTFX_FLAG_STRETCH;
								stretch = map->stretch;
							}

							yl = (yl - map->height) + player_height_offset;

							/* These faces are only shown when they are in a
							 * position which would be visible to the player. */
							if (FaceList[index].flags & FACE_FLAG_UP)
							{
								/* If the face is dir [0124568] and in
								 * the top or right quadrant or on the
								 * central square, blt it. */
								if (FaceList[index].flags & FACE_FLAG_D1)
								{
									if (((x <= (MAP_MAX_SIZE - 1) / 2) && (y <= (MAP_MAX_SIZE - 1) / 2)) || ((x > (MAP_MAX_SIZE - 1) / 2) && (y < (MAP_MAX_SIZE - 1) / 2)))
									{
										sprite_blt_map(face_sprite, xl, yl, NULL, &bltfx, 0);
									}
								}

								/* If the face is dir [0234768] and in
								 * the top or left quadrant or on the
								 * central square, blt it. */
								if (FaceList[index].flags & FACE_FLAG_D3)
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
							else if (FaceList[index].flags & FACE_FLAG_DOUBLE)
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
							if (options.player_names && map->pname[k][0])
							{
								if (options.player_names == 1 || (options.player_names == 2 && strnicmp(map->pname[k], cpl.rankandname, strlen(map->pname[k]))) || (options.player_names == 3 && !strnicmp(map->pname[k], cpl.rankandname, strlen(map->pname[k]))))
								{
									StringBlt(ScreenSurfaceMap, &Font6x3Out, map->pname[k], xpos - (strlen(map->pname[k]) * 2) + 22, ypos - 48, map->pcolor[k], NULL, NULL);
								}
							}

							/* Perhaps the object has a marked effect, blt it now */
							if (map->ext[k])
							{
								if (map->ext[k] & FFLAG_SLEEP)
								{
									sprite_blt_map(Bitmaps[BITMAP_SLEEP], xl + face_sprite->bitmap->w / 2, yl - 5, NULL, NULL, 0);
								}

								if (map->ext[k] & FFLAG_CONFUSED)
								{
									sprite_blt_map(Bitmaps[BITMAP_CONFUSE], xl + face_sprite->bitmap->w / 2 - 1, yl - 4, NULL, NULL, 0);
								}

								if (map->ext[k] & FFLAG_SCARED)
								{
									sprite_blt_map(Bitmaps[BITMAP_SCARED], xl + face_sprite->bitmap->w / 2 + 10, yl - 4, NULL, NULL, 0);
								}

								if (map->ext[k] & FFLAG_BLINDED)
								{
									sprite_blt_map(Bitmaps[BITMAP_BLIND], xl + face_sprite->bitmap->w / 2 + 3, yl - 6, NULL, NULL, 0);
								}

								if (map->ext[k] & FFLAG_PARALYZED)
								{
									sprite_blt_map(Bitmaps[BITMAP_PARALYZE], xl + face_sprite->bitmap->w / 2 + 2, yl + 3, NULL, NULL, 0);
									sprite_blt_map(Bitmaps[BITMAP_PARALYZE], xl + face_sprite->bitmap->w / 2 + 9, yl + 3, NULL, NULL, 0);
								}

								if (map->ext[k] & FFLAG_PROBE && cpl.target_code != 0)
								{
									if (face_sprite)
									{
										int hp_col;
										Uint32 sdl_col;

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

										mid = map->probe[k];

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
										if (!(options.player_names && map->pname[k][0]))
										{
											StringBlt(ScreenSurfaceMap, &Font6x3Out, cpl.target_name, xpos - (strlen(cpl.target_name)*2) + 22, yl - 26, cpl.target_color, NULL, NULL);
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
							}
						}
					}
				}
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
