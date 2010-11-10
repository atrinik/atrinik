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
 * The region map dialog. */

#include <include.h>
#include <region_map.h>

/** cURL data pointer to the downloaded map PNG. */
static curl_data *data_png = NULL;
/** cURL data pointer to the downloaded map definitions text file. */
static curl_data *data_def = NULL;
/** Pointer to the currently displayed map surface. */
static SDL_Surface *region_map_png = NULL;
/**
 * Pointer to the original map surface. This is same as ::region_map_png
 * unless zoom is in effect, in which case ::region_map_png points to the
 * zoomed surface. */
static SDL_Surface *region_map_png_orig;
/** Definitions parsed from ::data_def file. */
static region_map_def *rm_def = NULL;
/** Path name of the map the player is on. */
static char current_map[MAX_BUF];
/** X position of the player. */
static sint16 current_x;
/** Y position of the player. */
static sint16 current_y;
/** Array of label names parsed from the socket command. */
static char **cmd_labels = NULL;
/** Number of entries in ::cmd_labels. */
static size_t num_cmd_labels = 0;
/** Array of tooltip names parsed from the socket command. */
static char **cmd_tooltips = NULL;
/** Number of entries in ::cmd_tooltips. */
static size_t num_cmd_tooltips = 0;
/** Currently applied zoom. */
static int region_map_zoom;
/** Contains coordinates for the region map surface. */
static SDL_Rect region_map_pos;
/** Count for mouse clicks. */
static uint32 region_mouse_ticks = 0;

/**
 * Find a map by path in ::rm_def.
 * @param path Map path to find.
 * @return Pointer to the map if found, NULL otherwise. */
static region_map_struct *rm_def_get_map(const char *path)
{
	size_t i;

	for (i = 0; i < rm_def->num_maps; i++)
	{
		if (!strcmp(rm_def->maps[i].path, path))
		{
			return &rm_def->maps[i];
		}
	}

	return NULL;
}

/**
 * Find unique label name in ::rm_def.
 * @param name Label name to find.
 * @return Pointer to the label if found, NULL otherwise. */
static region_label_struct *rm_find_label(const char *name)
{
	size_t i;

	for (i = 0; i < rm_def->num_labels; i++)
	{
		if (!strcmp(rm_def->labels[i].name, name))
		{
			return &rm_def->labels[i];
		}
	}

	return NULL;
}

/**
 * Find unique tooltip name in ::rm_def.
 * @param name Tooltip name to find.
 * @return Pointer to the tooltip if found, NULL otherwise. */
static region_map_tooltip *rm_find_tooltip(const char *name)
{
	size_t i;

	for (i = 0; i < rm_def->num_tooltips; i++)
	{
		if (!strcmp(rm_def->tooltips[i].name, name))
		{
			return &rm_def->tooltips[i];
		}
	}

	return NULL;
}

/**
 * Initializes the map definitions from a string.
 * @param str String to initialize from. */
static void rm_def_create(char *str)
{
	char *cp;
	size_t i;
	region_label_struct *label;

	rm_def = calloc(1, sizeof(region_map_def));
	rm_def->pixel_size = 1;
	rm_def->map_size_x = 24;
	rm_def->map_size_y = 24;

	cp = strtok(str, "\n");

	while (cp)
	{
		if (!strncmp(cp, "pixel_size ", 11))
		{
			rm_def->pixel_size = atoi(cp + 11);
		}
		else if (!strncmp(cp, "map_size_x ", 11))
		{
			rm_def->map_size_x = atoi(cp + 11);
		}
		else if (!strncmp(cp, "map_size_y ", 11))
		{
			rm_def->map_size_y = atoi(cp + 11);
		}
		/* Map command, add it to the list of maps. */
		else if (!strncmp(cp, "map ", 4))
		{
			char path[HUGE_BUF * 4];

			rm_def->maps = realloc(rm_def->maps, sizeof(*rm_def->maps) * (rm_def->num_maps + 1));

			if (sscanf(cp + 4, "%x %x %s", &rm_def->maps[rm_def->num_maps].xpos, &rm_def->maps[rm_def->num_maps].ypos, path) == 3)
			{
				rm_def->maps[rm_def->num_maps].path = strdup(path);
			}

			rm_def->num_maps++;
		}
		/* Add label. */
		else if (!strncmp(cp, "label ", 6))
		{
			int x, y;
			char label_name[MAX_BUF], label_text[HUGE_BUF * 2];

			if (sscanf(cp + 6, "%x %x %s %8191[^\n]", &x, &y, label_name, label_text) == 4)
			{
				rm_def->labels = realloc(rm_def->labels, sizeof(*rm_def->labels) * (rm_def->num_labels + 1));
				rm_def->labels[rm_def->num_labels].hidden = -1;
				rm_def->labels[rm_def->num_labels].x = x;
				rm_def->labels[rm_def->num_labels].y = y;
				convert_newline(label_text);
				rm_def->labels[rm_def->num_labels].name = strdup(label_name);
				rm_def->labels[rm_def->num_labels].text = strdup(label_text);
				rm_def->num_labels++;
			}
		}
		/* Hide the previously added label. */
		else if (!strncmp(cp, "label_hide", 10))
		{
			rm_def->labels[rm_def->num_labels - 1].hidden = 1;
		}
		/* Add tooltip. */
		else if (!strncmp(cp, "tooltip ", 8))
		{
			int x, y, w, h;
			char tooltip_name[MAX_BUF], tooltip[HUGE_BUF * 2];

			if (sscanf(cp + 8, "%x %x %x %x %s %8191[^\n]", &x, &y, &w, &h, tooltip_name, tooltip) == 6)
			{
				rm_def->tooltips = realloc(rm_def->tooltips, sizeof(*rm_def->tooltips) * (rm_def->num_tooltips + 1));
				rm_def->tooltips[rm_def->num_tooltips].hidden = -1;
				rm_def->tooltips[rm_def->num_tooltips].outline = 0;
				rm_def->tooltips[rm_def->num_tooltips].x = x;
				rm_def->tooltips[rm_def->num_tooltips].y = y;
				rm_def->tooltips[rm_def->num_tooltips].w = w;
				rm_def->tooltips[rm_def->num_tooltips].h = h;
				convert_newline(tooltip);

				rm_def->tooltips[rm_def->num_tooltips].text = strdup(tooltip);
				rm_def->tooltips[rm_def->num_tooltips].name = strdup(tooltip_name);
				rm_def->num_tooltips++;
			}
		}
		else if (!strncmp(cp, "tooltip_hide", 12))
		{
			rm_def->tooltips[rm_def->num_tooltips - 1].hidden = 1;
		}
		else if (!strncmp(cp, "t_outline ", 10))
		{
			int r, g, b, outline_size = 1;

			if (sscanf(cp + 10, "#%2X%2X%2X %d", &r, &g, &b, &outline_size) >= 3)
			{
				rm_def->tooltips[rm_def->num_tooltips - 1].outline = 1;
				rm_def->tooltips[rm_def->num_tooltips - 1].outline_color.r = r;
				rm_def->tooltips[rm_def->num_tooltips - 1].outline_color.g = g;
				rm_def->tooltips[rm_def->num_tooltips - 1].outline_color.b = b;
				rm_def->tooltips[rm_def->num_tooltips - 1].outline_size = outline_size;
			}
		}
		else if (!strncmp(cp, "t_outline", 9))
		{
			rm_def->tooltips[rm_def->num_tooltips - 1].outline = 1;
			rm_def->tooltips[rm_def->num_tooltips - 1].outline_size = 1;
			rm_def->tooltips[rm_def->num_tooltips - 1].outline_color.r = 255;
			rm_def->tooltips[rm_def->num_tooltips - 1].outline_color.g = 0;
			rm_def->tooltips[rm_def->num_tooltips - 1].outline_color.b = 0;
		}

		cp = strtok(NULL, "\n");
	}

	/* Parse the labels from the server command. */
	for (i = 0; i < num_cmd_labels; i++)
	{
		label = rm_find_label(cmd_labels[i]);

		/* Unhide the label if we found it. */
		if (label)
		{
			label->hidden = 0;
		}

		free(cmd_labels[i]);
	}

	/* Don't need the labels from the command anymore, free them. */
	if (cmd_labels)
	{
		free(cmd_labels);
		cmd_labels = NULL;
	}

	num_cmd_labels = 0;

	/* Parse the tooltips from the server command. */
	for (i = 0; i < num_cmd_tooltips; i++)
	{
		region_map_tooltip *tooltip = rm_find_tooltip(cmd_tooltips[i]);

		/* Unhide the tooltip if we found it. */
		if (tooltip)
		{
			tooltip->hidden = 0;
		}

		free(cmd_tooltips[i]);
	}

	/* Don't need the tooltip from the command anymore, free them. */
	if (cmd_tooltips)
	{
		free(cmd_tooltips);
		cmd_tooltips = NULL;
	}

	num_cmd_tooltips = 0;
}

/**
 * Free ::rm_def. */
static void rm_def_free()
{
	size_t i;

	/* Free all maps. */
	for (i = 0; i < rm_def->num_maps; i++)
	{
		free(rm_def->maps[i].path);
	}

	free(rm_def->maps);

	/* Free labels. */
	for (i = 0; i < rm_def->num_labels; i++)
	{
		free(rm_def->labels[i].name);
		free(rm_def->labels[i].text);
	}

	if (rm_def->labels)
	{
		free(rm_def->labels);
	}

	/* Free tooltips. */
	for (i = 0; i < rm_def->num_tooltips; i++)
	{
		free(rm_def->tooltips[i].name);
		free(rm_def->tooltips[i].text);
	}

	if (rm_def->tooltips)
	{
		free(rm_def->tooltips);
	}

	free(rm_def);
	rm_def = NULL;
}

/**
 * Check if the specified label name exists in ::cmd_labels.
 * @param name Label name to check for.
 * @return 1 if the label name exists in ::cmd_labels, 0 otherwise. */
static int rm_label_in_cmd(const char *name)
{
	size_t i;

	for (i = 0; i < num_cmd_labels; i++)
	{
		if (!strcmp(cmd_labels[i], name))
		{
			return 1;
		}
	}

	return 0;
}

/**
 * Check if the specified tooltip name exists in ::cmd_tooltips.
 * @param name Tooltip name to check for.
 * @return 1 if the tooltip name exists in ::cmd_tooltips, 0 otherwise. */
static int rm_tooltip_in_cmd(const char *name)
{
	size_t i;

	for (i = 0; i < num_cmd_tooltips; i++)
	{
		if (!strcmp(cmd_tooltips[i], name))
		{
			return 1;
		}
	}

	return 0;
}

/**
 * Check if the region map is the same as the one we already loaded
 * previously.
 * @param url URL of the region map image.
 * @return 1 if it's same, 0 otherwise. */
static int region_map_is_same(const char *url)
{
	if (options.disable_rm_cache)
	{
		return 0;
	}

	/* Try to check labels from definitions. */
	if (rm_def)
	{
		size_t i;
		uint8 in_cmd;

		for (i = 0; i < rm_def->num_labels; i++)
		{
			in_cmd = rm_label_in_cmd(rm_def->labels[i].name);

			/* Not the same if the label should be shown or
			 * re-hidden. */
			if ((rm_def->labels[i].hidden == 1 && in_cmd) || (rm_def->labels[i].hidden == 0 && !in_cmd))
			{
				return 0;
			}
		}

		for (i = 0; i < rm_def->num_tooltips; i++)
		{
			in_cmd = rm_tooltip_in_cmd(rm_def->tooltips[i].name);

			/* Not the same if the tooltip should be shown or
			 * re-hidden. */
			if ((rm_def->tooltips[i].hidden == 1 && in_cmd) || (rm_def->tooltips[i].hidden == 0 && !in_cmd))
			{
				return 0;
			}
		}
	}

	/* Is the image URL the same? */
	if (data_png && !strcmp(url, data_png->url) && curl_download_finished(data_png) == 1 && curl_download_finished(data_def) == 1)
	{
		return 1;
	}

	return 0;
}

/**
 * Parse a region map command from the server.
 * @param data Data to parse.
 * @param len Length of 'data'. */
void RegionMapCmd(uint8 *data, int len)
{
	char region[MAX_BUF], url_base[HUGE_BUF], url[HUGE_BUF], text[HUGE_BUF];
	int pos = 0;

	/* Get the player's map, X and Y. */
	GetString_String(data, &pos, current_map, sizeof(current_map));
	current_x = GetShort_String(data + pos);
	pos += 2;
	current_y = GetShort_String(data + pos);
	pos += 2;
	/* Get the region and the URL base for the maps. */
	GetString_String(data, &pos, region, sizeof(region));
	GetString_String(data, &pos, url_base, sizeof(url_base));

	/* Rest of the data packet may be labels/tooltips/etc. */
	while (pos < len)
	{
		uint8 type = data[pos++];

		GetString_String(data, &pos, text, sizeof(text));

		if (type == RM_TYPE_LABEL)
		{
			cmd_labels = realloc(cmd_labels, sizeof(*cmd_labels) * (num_cmd_labels + 1));
			cmd_labels[num_cmd_labels] = strdup(text);
			num_cmd_labels++;
		}
		else if (type == RM_TYPE_TOOLTIP)
		{
			cmd_tooltips = realloc(cmd_tooltips, sizeof(*cmd_tooltips) * (num_cmd_tooltips + 1));
			cmd_tooltips[num_cmd_tooltips] = strdup(text);
			num_cmd_tooltips++;
		}
	}

	cpl.menustatus = MENU_REGION_MAP;

	/* Construct URL for the image. */
	snprintf(url, sizeof(url), "%s/%s.png", url_base, region);

	/* Free old map surface. */
	if (region_map_png)
	{
		/* If zoom was used, we have to free both. */
		if (region_map_png != region_map_png_orig)
		{
			SDL_FreeSurface(region_map_png);
		}

		SDL_FreeSurface(region_map_png_orig);
		region_map_png = NULL;
	}

	/* Default zoom. */
	region_map_zoom = RM_ZOOM_DEFAULT;

	region_map_pos.x = 0;
	region_map_pos.y = 0;
	region_map_pos.w = Bitmaps[BITMAP_BOOK]->bitmap->w - RM_BORDER_SIZE * 2;
	region_map_pos.h = Bitmaps[BITMAP_BOOK]->bitmap->h - RM_BORDER_SIZE * 2;

	/* The map is the same, no downloading needed. */
	if (region_map_is_same(url))
	{
		return;
	}

	/* Free old cURL data and the parsed definitions. */
	if (data_png)
	{
		curl_data_free(data_png);
		curl_data_free(data_def);
		rm_def_free();
	}

	/* Start the downloads. */
	data_png = curl_download_start(url);
	snprintf(url, sizeof(url), "%s/%s.def", url_base, region);
	data_def = curl_download_start(url);
}

/**
 * Resize the region map surface. */
static void region_map_resize()
{
	region_map_struct *map;

	/* Free old zoomed surface if applicable. */
	if (region_map_png != region_map_png_orig)
	{
		SDL_FreeSurface(region_map_png);
	}

	/* Zoom the surface. */
	region_map_png = zoomSurface(region_map_png_orig, region_map_zoom / 100.0, region_map_zoom / 100.0, options.zoom_smooth);
	map = rm_def_get_map(current_map);

	/* Adjust the positions, so the map is centered on the player. */
	if (map)
	{
		region_map_pos.x = ((map->xpos + current_x * rm_def->pixel_size) * (region_map_zoom / 100.0) - region_map_pos.w / 2);
		region_map_pos.y = ((map->ypos + current_y * rm_def->pixel_size) * (region_map_zoom / 100.0) - region_map_pos.h / 2);
	}
	else
	{
		region_map_pos.x = region_map_png->w / 2 - region_map_pos.w / 2;
		region_map_pos.y = region_map_png->h / 2 - region_map_pos.h / 2;
	}

	surface_pan(region_map_png, &region_map_pos);
}

/**
 * Handle key press.
 * @param key Key. */
void region_map_handle_key(SDLKey key)
{
	int pos = RM_SCROLL;

	/* Shift is held, increase the scrolling 'speed'. */
	if (SDL_GetModState() & KMOD_SHIFT)
	{
		pos = RM_SCROLL_SHIFT;
	}

	if (key == SDLK_UP)
	{
		region_map_pos.y -= pos;
	}
	else if (key == SDLK_DOWN)
	{
		region_map_pos.y += pos;
	}
	else if (key == SDLK_LEFT)
	{
		region_map_pos.x -= pos;
	}
	else if (key == SDLK_RIGHT)
	{
		region_map_pos.x += pos;
	}
	else
	{
		return;
	}

	surface_pan(region_map_png, &region_map_pos);
}

/**
 * Handle mouse event.
 * @param event The event. */
void region_map_handle_event(SDL_Event *event)
{
	if (event->type == SDL_MOUSEBUTTONDOWN)
	{
		/* Zoom in. */
		if (event->button.button == SDL_BUTTON_WHEELUP)
		{
			if (region_map_zoom < RM_ZOOM_MAX)
			{
				region_map_zoom += RM_ZOOM_PROGRESS;
				region_map_resize();
			}
		}
		/* Zoom out. */
		else if (event->button.button == SDL_BUTTON_WHEELDOWN)
		{
			if (region_map_zoom > RM_ZOOM_MIN)
			{
				region_map_zoom -= RM_ZOOM_PROGRESS;
				region_map_resize();
			}
		}
	}
}

/**
 * Show the region map menu. */
void region_map_show()
{
	int x, y, ret_png, ret_def;
	SDL_Rect box, dest;
	int state, mx, my;
	size_t i;

	/* Show the background. */
	x = BOOK_BACKGROUND_X;
	y = BOOK_BACKGROUND_Y;
	sprite_blt(Bitmaps[BITMAP_BOOK], x, y, NULL, NULL);

	box.x = x + RM_BORDER_SIZE;
	box.y = y + RM_BORDER_SIZE;
	box.w = region_map_pos.w;
	box.h = region_map_pos.h;

	/* Show a close button. */
	if (button_show(BITMAP_BUTTON_ROUND, -1, BITMAP_BUTTON_ROUND_DOWN, box.x + box.w, y + 10, "X", FONT_ARIAL10, COLOR_SIMPLE(COLOR_WHITE), COLOR_SIMPLE(COLOR_BLACK), COLOR_SIMPLE(COLOR_HGOLD), COLOR_SIMPLE(COLOR_BLACK)))
	{
		cpl.menustatus = MENU_NO;
		map_udate_flag = 2;
		reset_keys();
	}

	/* Check the status of the downloads. */
	ret_png = curl_download_finished(data_png);
	ret_def = curl_download_finished(data_def);

	/* We failed. */
	if (ret_png == -1 || ret_def == -1)
	{
		string_blt(ScreenSurface, FONT_SERIF14, "Connection timed out.", box.x, box.y, COLOR_SIMPLE(COLOR_BLACK), TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);
		return;
	}

	/* Still in progress. */
	if (ret_png == 0 || ret_def == 0)
	{
		string_blt(ScreenSurface, FONT_SERIF14, "Downloading the map, please wait...", box.x, box.y, COLOR_SIMPLE(COLOR_BLACK), TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);
		return;
	}

	/* Parse the definitions. */
	if (!rm_def)
	{
		rm_def_create(data_def->memory);
	}

	/* Create the map surface. */
	if (!region_map_png)
	{
		SDL_Rect marker;
		region_map_struct *map;

		/* Create the surface from downloaded data. */
		region_map_png = region_map_png_orig = IMG_Load_RW(SDL_RWFromMem(data_png->memory, data_png->size), 1);
		map = rm_def_get_map(current_map);

		/* Valid map? */
		if (map)
		{
			/* Calculate the player's marker position. */
			marker.x = map->xpos + current_x * rm_def->pixel_size - Bitmaps[BITMAP_MAP_MARKER]->bitmap->w / 2 + rm_def->pixel_size / 2;
			marker.y = map->ypos + current_y * rm_def->pixel_size - Bitmaps[BITMAP_MAP_MARKER]->bitmap->h + rm_def->pixel_size;
			SDL_BlitSurface(Bitmaps[BITMAP_MAP_MARKER]->bitmap, NULL, region_map_png, &marker);

			/* Center the map on the player. */
			region_map_pos.x = (map->xpos + current_x * rm_def->pixel_size) - region_map_pos.w / 2;
			region_map_pos.y = (map->ypos + current_y * rm_def->pixel_size) - region_map_pos.h / 2;
		}
		else
		{
			region_map_pos.x = region_map_png->w / 2 - region_map_pos.w / 2;
			region_map_pos.y = region_map_png->h / 2 - region_map_pos.h / 2;
			surface_pan(region_map_png, &region_map_pos);
		}

		surface_pan(region_map_png, &region_map_pos);

		/* Blit the labels. */
		for (i = 0; i < rm_def->num_labels; i++)
		{
			if (rm_def->labels[i].hidden < 1)
			{
				string_blt(region_map_png, FONT_SERIF20, rm_def->labels[i].text, rm_def->labels[i].x, rm_def->labels[i].y, COLOR_SIMPLE(COLOR_HGOLD), TEXT_MARKUP | TEXT_OUTLINE, NULL);
			}
		}

		for (i = 0; i < rm_def->num_tooltips; i++)
		{
			if (rm_def->tooltips[i].hidden < 1 && rm_def->tooltips[i].outline)
			{
				border_create(region_map_png, rm_def->tooltips[i].x, rm_def->tooltips[i].y, rm_def->tooltips[i].w, rm_def->tooltips[i].h, SDL_MapRGB(region_map_png->format, rm_def->tooltips[i].outline_color.r, rm_def->tooltips[i].outline_color.g, rm_def->tooltips[i].outline_color.b), rm_def->tooltips[i].outline_size);
			}
		}
	}

	state = SDL_GetMouseState(&mx, &my);

	/* Move the map around with the mouse. */
	if ((state == SDL_BUTTON(SDL_BUTTON_LEFT) || state == SDL_BUTTON(SDL_BUTTON_MIDDLE)) && mx > box.x && my < box.x + box.w && my > box.y && my < box.y + box.h && (!region_mouse_ticks || state == SDL_BUTTON(SDL_BUTTON_MIDDLE) || SDL_GetTicks() - region_mouse_ticks > 125))
	{
		region_mouse_ticks = SDL_GetTicks();

		if (state == SDL_BUTTON(SDL_BUTTON_LEFT))
		{
			/* The clicked position will become centered, unless it's too
			 * close to an edge of the map, of course. */
			region_map_pos.x += mx - box.x - region_map_pos.w / 2;
			region_map_pos.y += my - box.y - region_map_pos.h / 2;
			surface_pan(region_map_png, &region_map_pos);
		}
		else if (state == SDL_BUTTON(SDL_BUTTON_MIDDLE))
		{
			size_t i;
			int xpos, ypos;

			xpos = region_map_pos.x + mx - box.x;
			ypos = region_map_pos.y + my - box.y;

			for (i = 0; i < rm_def->num_maps; i++)
			{
				if (xpos >= rm_def->maps[i].xpos * (region_map_zoom / 100.0) && xpos <= (rm_def->maps[i].xpos + (rm_def->map_size_x * rm_def->pixel_size)) * (region_map_zoom / 100.0) && ypos >= rm_def->maps[i].ypos * (region_map_zoom / 100.0) && ypos <= (rm_def->maps[i].ypos + (rm_def->map_size_y * rm_def->pixel_size)) * (region_map_zoom / 100.0))
				{
					char buf[HUGE_BUF];

					xpos = (xpos - rm_def->maps[i].xpos * (region_map_zoom / 100.0)) / (rm_def->pixel_size * (region_map_zoom / 100.0));
					ypos = (ypos - rm_def->maps[i].ypos * (region_map_zoom / 100.0)) / (rm_def->pixel_size * (region_map_zoom / 100.0));
					snprintf(buf, sizeof(buf), "/goto %s %d %d", rm_def->maps[i].path, xpos, ypos);
					send_command(buf);

					cpl.menustatus = MENU_NO;
					map_udate_flag = 2;
					reset_keys();
					break;
				}
			}
		}
	}

	dest.x = box.x;
	dest.y = box.y;

	/* Actually blit the map. */
	SDL_BlitSurface(region_map_png, &region_map_pos, ScreenSurface, &dest);

	for (i = 0; i < rm_def->num_tooltips; i++)
	{
		if (rm_def->tooltips[i].hidden < 1 && region_map_pos.x + mx - box.x >= rm_def->tooltips[i].x * (region_map_zoom / 100.0) && region_map_pos.x + mx - box.x <= (rm_def->tooltips[i].x + rm_def->tooltips[i].w) * (region_map_zoom / 100.0) && region_map_pos.y + my - box.y >= rm_def->tooltips[i].y * (region_map_zoom / 100.0) && region_map_pos.y + my - box.y <= (rm_def->tooltips[i].y + rm_def->tooltips[i].h) * (region_map_zoom / 100.0))
		{
			x = box.x + box.w + RM_BORDER_SIZE;
			sprite_blt(Bitmaps[BITMAP_NEWS_BG], x, y, NULL, NULL);
			box.w = Bitmaps[BITMAP_NEWS_BG]->bitmap->w - 8 * 2;
			box.h = Bitmaps[BITMAP_NEWS_BG]->bitmap->w - 8 * 2;
			string_blt(ScreenSurface, FONT_ARIAL11, rm_def->tooltips[i].text, x + 8, y + 8, COLOR_SIMPLE(COLOR_WHITE), TEXT_MARKUP | TEXT_WORD_WRAP | TEXT_OUTLINE, &box);
			break;
		}
	}
}
