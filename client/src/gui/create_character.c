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
 *  */

#include <include.h>

int dialog_new_char_warn = 0;

/** Create list set */
struct _dialog_list_set create_list_set;

#define CREATE_Y0 120

/** The attributes, like Strength, Charisma, etc. */
static const char *attributes[7] =
{
	"STR", "DEX", "CON", "INT", "WIS", "POW", "CHA"
};

/** Number of ::attributes. */
#define NUM_ATTRIBUTES (sizeof(attributes) / sizeof(attributes[0]))

/** The possible genders. */
static const char *gender[] =
{
	"male", "female", "hermaphrodite", "neuter"
};

/**
 * Blit a face.
 * @param id ID of the face.
 * @param x X position.
 * @param y Y position. */
void blit_face(int id, int x, int y)
{
	if (id == -1 || !FaceList[id].sprite)
	{
		return;
	}

	if (FaceList[id].sprite->status != SPRITE_STATUS_LOADED)
	{
		return;
	}

	sprite_blt(FaceList[id].sprite, x, y, NULL, NULL);
}

/**
 * Show hero creation window. */
void show_newplayer_server()
{
	int id = 0;
	int x, y, i;
	char buf[256];
	int mx, my, mb;
	int delta;
	_server_char *tmpc;

	mb = SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT);
	x = 25;
	y = Screensize->y / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->h / 2;
	sprite_blt(Bitmaps[BITMAP_DIALOG_BG], x, y, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_DIALOG_TITLE_CREATION], x + 250 - Bitmaps[BITMAP_DIALOG_TITLE_CREATION]->bitmap->w / 2, y + 15, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_PENTAGRAM], x + 25, y + 430, NULL, NULL);
	add_close_button(x, y, MENU_CREATE);

	/* Print all attributes */
	StringBlt(ScreenSurface, &SystemFont, "Welcome!", x + 131, y + 64, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Welcome!", x + 130, y + 63, COLOR_WHITE, NULL, NULL);
	sprintf(buf, "Use ~%c%c~ and ~%c%c~ cursor keys to setup your stats.", ASCII_UP, ASCII_DOWN, ASCII_RIGHT, ASCII_LEFT);
	StringBlt(ScreenSurface, &SystemFont, buf, x + 131, y + 76, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, buf, x + 130, y + 75, COLOR_WHITE, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Adjust the stats using *all* your points.", x + 131, y + 89, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Adjust the stats using *all* your points.", x + 130, y + 88, COLOR_WHITE, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Press ~C~ to create your new character.", x + 131, y + 101, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Press ~C~ to create your new character.", x + 130, y + 100, COLOR_WHITE, NULL, NULL);

	/* Create button */
	if (add_button(x + 30, y + 397, id++, BITMAP_DIALOG_BUTTON_UP, "Create", "~C~reate"))
	{
		check_menu_keys(MENU_CREATE, SDLK_c);
	}

	if (dialog_new_char_warn == 1)
	{
		StringBlt(ScreenSurface, &SystemFont, "  ** ASSIGN ALL **", x + 21, y + 368, COLOR_BLACK, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, "  ** ASSIGN ALL **", x + 20, y + 367, COLOR_HGOLD, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, "** POINTS FIRST **", x + 21, y + 380, COLOR_BLACK, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, "** POINTS FIRST **", x + 20, y + 379, COLOR_HGOLD, NULL, NULL);
	}

	/* Draw attributes */
	StringBlt(ScreenSurface, &SystemFont, "Points:", x + 130, y + CREATE_Y0 + 3 * 17, COLOR_WHITE, NULL, NULL);

	if (new_character.stat_points)
	{
		sprintf(buf, "%.2d  LEFT", new_character.stat_points);
	}
	else
	{
		sprintf(buf, "%.2d", new_character.stat_points);
	}

	StringBlt(ScreenSurface, &SystemFont, buf, x + 171, y + CREATE_Y0 + 3 * 17 + 1, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, buf, x + 170, y + CREATE_Y0 + 3 * 17, COLOR_HGOLD, NULL, NULL);

	if (create_list_set.entry_nr > 8)
	{
		create_list_set.entry_nr = 8;
	}

	for (i = 0; i < (int) NUM_ATTRIBUTES; i++)
	{
		id += 2;
		sprintf(buf, "%s:", attributes[i]);

		if (create_list_set.entry_nr == i + 2)
		{
			StringBlt(ScreenSurface, &SystemFont, buf, x + 130, y + CREATE_Y0 + (i + 4) * 17, COLOR_GREEN, NULL, NULL);
		}
		else
		{
			StringBlt(ScreenSurface, &SystemFont, buf, x + 130, y + CREATE_Y0 + (i + 4) * 17, COLOR_WHITE, NULL, NULL);
		}

		sprintf(buf, "%.2d", new_character.stats[i]);

		if (create_list_set.entry_nr == i + 2)
		{
			delta = add_rangebox(x + 170, y + CREATE_Y0 + (i + 4) * 17, id, 20, 0, buf, COLOR_GREEN);
		}
		else
		{
			delta = add_rangebox(x + 170, y + CREATE_Y0 + (i + 4) * 17, id, 20, 0, buf, COLOR_WHITE);
		}

		/* Keyboard event */
		if (create_list_set.key_change && create_list_set.entry_nr == i + 2)
		{
			delta = create_list_set.key_change;
			create_list_set.key_change = 0;
		}

		if (delta)
		{
			dialog_new_char_warn = 0;

			if (delta > 0)
			{
				if (new_character.stats[i] + 1 <= new_character.stats_max[i] && new_character.stat_points)
				{
					new_character.stats[i]++;
					new_character.stat_points--;
				}
			}
			else
			{
				if (new_character.stats[i] - 1 >= new_character.stats_min[i])
				{
					new_character.stats[i]--;
					new_character.stat_points++;
				}
			}
		}
	}

	for (i = 0, tmpc = first_server_char; tmpc; tmpc = tmpc->next, i++)
	{
		SDL_Rect box;
		box.h = 55;
		box.w = 55;
		box.x = x + 125 + i * 60;
		box.y = y + 320;

		SDL_FillRect(ScreenSurface, &box, sdl_gray2);
		blit_face(tmpc->pic_id,box.x + 5, box.y + 5);
		StringBlt(ScreenSurface, &Font6x3Out, tmpc->name, box.x + 10, box.y + 40, COLOR_WHITE, NULL, NULL);

		if (!strcmp(tmpc->name, new_character.name))
		{
			sprite_blt(Bitmaps[BITMAP_NCHAR_MARKER], box.x, box.y, NULL, NULL);
		}
	}

	if (create_list_set.entry_nr == 0)
	{
		StringBlt(ScreenSurface, &SystemFont, "Race:", x + 130, y + CREATE_Y0 + 0 * 17 + 2, COLOR_GREEN, NULL, NULL);
	}
	else
	{
		StringBlt(ScreenSurface, &SystemFont, "Race:", x + 130, y + CREATE_Y0 + 0 * 17 + 2, COLOR_WHITE, NULL, NULL);
	}

	if (create_list_set.entry_nr == 0)
	{
		delta = add_rangebox(x + 170, y + CREATE_Y0 + 0 * 17, ++id, 80, 0, new_character.name, COLOR_GREEN);
	}
	else
	{
		delta = add_rangebox(x + 170, y + CREATE_Y0 + 0 * 17, ++id, 80, 0, new_character.name, COLOR_WHITE);
	}

	if (create_list_set.key_change && create_list_set.entry_nr == 0)
	{
		delta = create_list_set.key_change;
		create_list_set.key_change = 0;
	}

	/* Init new race */
	if (delta)
	{
		int g;

		for (tmpc = first_server_char; tmpc; tmpc = tmpc->next)
		{
			/* Get our current template */
			if (!strcmp(tmpc->name, new_character.name))
			{
				/* Get next template */
				if (delta > 0)
				{
					tmpc = tmpc->next;

					if (!tmpc)
					{
						tmpc = first_server_char;
					}
				}
				else
				{
					tmpc = tmpc->prev;

					if (!tmpc)
					{
						/* Get last node */
						for (tmpc = first_server_char; tmpc->next; tmpc = tmpc->next)
						{
						}
					}
				}

				memcpy(&new_character, tmpc, sizeof(_server_char));

				/* Adjust gender */
				for (g = 0; g < 4; g++)
				{
					if (new_character.gender[g])
					{
						new_character.gender_selected = g;
						break;
					}
				}

				break;
			}
		}
	}

	if (create_list_set.entry_nr == 1)
	{
		StringBlt(ScreenSurface, &SystemFont, "Gender:", x + 130, y + CREATE_Y0 + 1 * 17 + 2, COLOR_GREEN, NULL, NULL);
	}
	else
	{
		StringBlt(ScreenSurface, &SystemFont, "Gender:", x + 130, y + CREATE_Y0 + 1 * 17 + 2, COLOR_WHITE, NULL, NULL);
	}

	if (create_list_set.entry_nr == 1)
	{
		delta = add_rangebox(x + 170, y + CREATE_Y0 + 1 * 17, ++id, 80, 0, gender[new_character.gender_selected], COLOR_GREEN);
	}
	else
	{
		delta = add_rangebox(x + 170, y + CREATE_Y0 + 1 * 17, ++id, 80, 0, gender[new_character.gender_selected], COLOR_WHITE);
	}

	if (create_list_set.key_change && create_list_set.entry_nr == 1)
	{
		delta = create_list_set.key_change;
		create_list_set.key_change = 0;
	}

	if (delta)
	{
		int g, tmp_g;

		/* +1 */
		if (delta > 0)
		{
			for (g = 0; g < 4; g++)
			{
				tmp_g = ++new_character.gender_selected;

				if (tmp_g > 3)
				{
					tmp_g -= 4;
				}

				if (new_character.gender[tmp_g])
				{
					new_character.gender_selected = tmp_g;
					break;
				}
			}
		}
		else
		{
			for (g = 3; g >= 0; g--)
			{
				tmp_g = new_character.gender_selected - g;

				if (tmp_g < 0)
				{
					tmp_g += 4;
				}

				if (new_character.gender[tmp_g])
				{
					new_character.gender_selected = tmp_g;
					break;
				}
			}
		}
	}

	/* Draw player image */
	StringBlt(ScreenSurface, &SystemFont, cpl.name, x + 40, y + 85, COLOR_WHITE, NULL, NULL);

	blit_face(new_character.face_id[new_character.gender_selected], x + 35, y + 100);
	sprintf(buf, "HP: ~%d~", new_character.bar[0] * 4 + new_character.bar_add[0]);
	StringBlt(ScreenSurface, &SystemFont, buf, x + 36, y + 146, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, buf, x + 35, y + 145, COLOR_WHITE, NULL, NULL);
	sprintf(buf, "SP: ~%d~", new_character.bar[1] * 2 + new_character.bar_add[1]);
	StringBlt(ScreenSurface, &SystemFont, buf, x + 36, y + 157, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, buf, x + 35, y + 156, COLOR_WHITE, NULL, NULL);
	sprintf(buf, "GR: ~%d~", new_character.bar[2] * 2 + new_character.bar_add[2]);
	StringBlt(ScreenSurface, &SystemFont, buf, x + 36, y + 168, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, buf, x + 35, y + 167, COLOR_WHITE, NULL, NULL);

	StringBlt(ScreenSurface, &SystemFont, new_character.desc[0], x + 160, y + 434, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, new_character.desc[0], x + 159, y + 433, COLOR_WHITE, NULL, NULL);

	StringBlt(ScreenSurface, &SystemFont, new_character.desc[1], x + 160, y + 446, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, new_character.desc[1], x + 159, y + 445, COLOR_WHITE, NULL, NULL);

	StringBlt(ScreenSurface, &SystemFont, new_character.desc[2], x + 160, y + 458, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, new_character.desc[2], x + 159, y + 457, COLOR_WHITE, NULL, NULL);

	StringBlt(ScreenSurface, &SystemFont, new_character.desc[3], x + 160, y + 470, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, new_character.desc[3], x + 159, y + 469, COLOR_WHITE, NULL, NULL);

	if (!mb)
	{
		active_button = -1;
	}
}
