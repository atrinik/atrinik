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

/** Spell list entries */
struct _spell_list spell_list[SPELL_LIST_MAX];

/** Spell list set */
struct _dialog_list_set spell_list_set;

/** The spell paths. */
static const char *spell_tab[] =
{
	"Protection", "Fire", "Frost", "Electricity", "Missiles",
	"Self", "Summoning", "Abjuration", "Restoration", "Detonation",
	"Mind", "Creation", "Teleportation", "Information", "Transmutation",
	"Transferrence", "Turning", "Wounding", "Death", "Light",
	NULL
};

/** Spell classes. */
static const char *spell_class[SPELL_LIST_CLASS] =
{
	"Spell", "Prayer"
};

/**
 * Show the spell list. */
void show_spelllist()
{
	SDL_Rect box;
	char buf[256];
	int x,y, i;
	int mx, my, mb;
	static int active = 0, dblclk = 0;
	static Uint32 Ticks = 0;

	mb = SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT);

	/* Background */
	x = Screensize->x / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->w / 2;
	y = Screensize->y / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->h / 2;
	sprite_blt(Bitmaps[BITMAP_DIALOG_BG], x, y, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_DIALOG_TITLE_SPELL], x + 250 - Bitmaps[BITMAP_DIALOG_TITLE_SPELL]->bitmap->w / 2, y + 14, NULL, NULL);
	add_close_button(x, y, MENU_SPELL);

	/* Tabs */
	draw_tabs(spell_tab, &spell_list_set.group_nr, "Spell Path", x + 8, y + 70);

	sprintf(buf, "~SHIFT~ + ~%c%c~ to select path                   ~%c%c~ to select spell                    ~RETURN~ for use", ASCII_UP, ASCII_DOWN, ASCII_UP, ASCII_DOWN);
	StringBlt(ScreenSurface, &SystemFont, buf, x + 135, y + 410, COLOR_WHITE, NULL, NULL);

	/* Spell class buttons */
	for (i = 0; i < SPELL_LIST_CLASS; i++)
	{
		if (add_gr_button(x + 133 + i * 56, y + 75, (spell_list_set.class_nr == i), BITMAP_DIALOG_BUTTON_UP, spell_class[i], NULL))
		{
			spell_list_set.class_nr = i;
		}
	}

	StringBlt(ScreenSurface, &SystemFont, "use ~F1-F8~ for spell to quickbar", x + 250, y + 69, COLOR_WHITE, NULL, NULL);
	sprintf(buf, "use ~%c%c~ to select spell group", ASCII_RIGHT, ASCII_LEFT);
	StringBlt(ScreenSurface, &SystemFont, buf, x + 250, y + 80, COLOR_WHITE, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Cost", x + (Bitmaps[BITMAP_DIALOG_BG]->bitmap->w - 60), y + 80, COLOR_WHITE, NULL, NULL);

	box.x = x + 133;
	box.y = y + TXT_Y_START + 1;
	box.w = 329;
	box.h = 12;

	/* Frame for selection field */
	draw_frame(box.x - 1, box.y + 11, box.w + 1, 313);

	/* Print skill entries */
	if (!mb)
	{
		active = 0;
	}

	if (mx > x + TXT_START_NAME && mx < x + TXT_START_NAME + 327 && my > y + TXT_Y_START && my < y + 12 + TXT_Y_START + DIALOG_LIST_ENTRY * 12)
	{
		if (!mb)
		{
			if (dblclk == 1)
			{
				dblclk = 2;
			}

			if (dblclk == 3)
			{
				dblclk = 0;

				if (spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][spell_list_set.entry_nr].flag == LIST_ENTRY_KNOWN)
				{
					check_menu_keys(MENU_SPELL, SDLK_RETURN);
				}
			}
		}
		else
		{
			if (dblclk == 0)
			{
				dblclk = 1;
				Ticks = SDL_GetTicks();
			}

			if (dblclk == 2)
			{
				dblclk = 3;

				if (SDL_GetTicks() - Ticks > 300)
				{
					dblclk = 0;
				}
			}

			/* mb was pressed in the selection field */
			if (mb_clicked)
			{
				active = 1;
			}

			if (active && spell_list_set.entry_nr != (my - y - 12 - TXT_Y_START) / 12)
			{
				spell_list_set.entry_nr = (my - y - 12 - TXT_Y_START) / 12;
				dblclk = 0;
			}
		}
	}

	for (i = 0; i < DIALOG_LIST_ENTRY; i++)
	{
		y += 12;
		box.y += 12;

		if (i != spell_list_set.entry_nr)
		{
			if (i & 1)
			{
				SDL_FillRect(ScreenSurface, &box, sdl_gray2);
			}
			else
			{
				SDL_FillRect(ScreenSurface, &box, sdl_gray1);
			}
		}
		else
		{
			SDL_FillRect(ScreenSurface, &box, sdl_blue1);
		}

		if (spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][i].flag == LIST_ENTRY_KNOWN)
		{
			StringBlt(ScreenSurface, &SystemFont, spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][i].name, x + TXT_START_NAME, y + TXT_Y_START, COLOR_WHITE, NULL, NULL);
			sprintf(buf, "%5d", spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][i].cost);
			StringBlt(ScreenSurface, &SystemFont, buf, x + (Bitmaps[BITMAP_DIALOG_BG]->bitmap->w - 60), y + TXT_Y_START, COLOR_WHITE, NULL, NULL);
		}
	}

	x += 160;
	y += 120;

	/* Print spell description */
	if (spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][spell_list_set.entry_nr].flag == LIST_ENTRY_KNOWN)
	{
		/* Selected */
		if (mb && mx > x - 40 && mx < x - 10 && my > y + 10 && my < y + 43)
		{
			dblclk = 0;
			check_menu_keys(MENU_SPELL, SDLK_RETURN);
		}

		sprite_blt(spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][spell_list_set.entry_nr].icon, x - 42, y + 10, NULL, NULL);
		sprite_blt(spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][spell_list_set.entry_nr].icon, x - 42, y + 10, NULL, NULL);

		/* Path relationship. */
		if (spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][spell_list_set.entry_nr].path == 'a')
		{
			StringBlt(ScreenSurface, &BigFont, "Attuned", x - 139, y + 25, COLOR_BLACK, NULL, NULL);
			StringBlt(ScreenSurface, &BigFont, "Attuned", x - 140, y + 25, COLOR_HGOLD, NULL, NULL);
		}
		else if (spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][spell_list_set.entry_nr].path == 'r')
		{
			StringBlt(ScreenSurface, &BigFont, "Repelled", x - 139, y + 25, COLOR_BLACK, NULL, NULL);
			StringBlt(ScreenSurface, &BigFont, "Repelled", x - 140, y + 25, COLOR_HGOLD, NULL, NULL);
		}
		else if (spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][spell_list_set.entry_nr].path == 'd')
		{
			StringBlt(ScreenSurface, &BigFont, "Denied", x - 139, y + 25, COLOR_BLACK, NULL, NULL);
			StringBlt(ScreenSurface, &BigFont, "Denied", x - 140, y + 25, COLOR_HGOLD, NULL, NULL);
		}

		/* Print textblock */
		for (i = 0; i < 4; i++)
		{
			StringBlt(ScreenSurface, &SystemFont, &spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][spell_list_set.entry_nr].desc[i][0], x - 2, y + 1, COLOR_BLACK, NULL, NULL);
			StringBlt(ScreenSurface, &SystemFont, &spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][spell_list_set.entry_nr].desc[i][0], x - 3, y, COLOR_WHITE, NULL, NULL);
			y += 13;
		}
	}

	if (!mb)
	{
		active_button = -1;
	}
}

/**
 * Read spells file. */
void read_spells()
{
	int i, ii, panel;
	char type, nchar, *tmp, *tmp2;
	struct stat statbuf;
	FILE *stream;
	char *temp_buf;
	char line[255], name[255], d1[255], d2[255], d3[255], d4[255], icon[128];

	for (i = 0; i < SPELL_LIST_MAX; i++)
	{
		for (ii = 0; ii < DIALOG_LIST_ENTRY; ii++)
		{
			spell_list[i].entry[0][ii].flag = LIST_ENTRY_UNUSED;
			spell_list[i].entry[1][ii].flag = LIST_ENTRY_UNUSED;
			spell_list[i].entry[0][ii].name[0] = 0;
			spell_list[i].entry[1][ii].name[0] = 0;
		}
	}

	spell_list_set.class_nr = 0;
	spell_list_set.entry_nr = 0;
	spell_list_set.group_nr = 0;

	srv_client_files[SRV_CLIENT_SPELLS].len = 0;
	srv_client_files[SRV_CLIENT_SPELLS].crc = 0;
	LOG(llevDebug, "Reading %s...", FILE_CLIENT_SPELLS);

	if ((stream = fopen_wrapper(FILE_CLIENT_SPELLS, "rb")) != NULL)
	{
		/* Temporary load the file and get the data we need for compare with server */
		fstat(fileno(stream), &statbuf);
		i = (int) statbuf.st_size;
		srv_client_files[SRV_CLIENT_SPELLS].len = i;
		temp_buf = malloc(i);

		if (fread(temp_buf, 1, i, stream))
			srv_client_files[SRV_CLIENT_SPELLS].crc = crc32(1L, (const unsigned char FAR *) temp_buf, i);

		free(temp_buf);
		rewind(stream);

		for (i = 0; ; i++)
		{
			if (fgets(line, 255, stream) == NULL)
				break;

			line[250] = 0;
			tmp = strchr(line, '"');
			tmp2 = strchr(tmp + 1, '"');
			*tmp2 = 0;
			strcpy(name, tmp + 1);

			if (fgets(line, 255, stream) == NULL)
				break;

			sscanf(line, "%c %c %d %s", &type, &nchar, &panel, icon);

			if (fgets(line, 255, stream) == NULL)
				break;

			line[250] = 0;
			tmp = strchr(line, '"');
			tmp2 = strchr(tmp + 1, '"');
			*tmp2 = 0;
			strcpy(d1, tmp + 1);

			if (fgets(line, 255, stream) == NULL)
				break;

			line[250] = 0;
			tmp = strchr(line, '"');
			tmp2 = strchr(tmp + 1, '"');
			*tmp2 = 0;
			strcpy(d2, tmp + 1);

			if (fgets(line, 255, stream) == NULL)
				break;

			line[250] = 0;
			tmp = strchr(line, '"');
			tmp2 = strchr(tmp + 1, '"');
			*tmp2 = 0;
			strcpy(d3, tmp + 1);

			if (fgets(line, 255, stream) == NULL)
				break;

			line[250] = 0;
			tmp = strchr(line, '"');
			tmp2 = strchr(tmp + 1, '"');
			*tmp2 = 0;
			strcpy(d4, tmp + 1);
			panel--;

			spell_list[panel].entry[type == 'w' ? 0 : 1][nchar - 'a'].flag = LIST_ENTRY_USED;
			strcpy(spell_list[panel].entry[type == 'w' ? 0 : 1][nchar - 'a'].icon_name, icon);

			sprintf(line, "%s%s", GetIconDirectory(), icon);
			spell_list[panel].entry[type == 'w' ? 0 : 1][nchar - 'a'].icon = sprite_load_file(line, SURFACE_FLAG_DISPLAYFORMAT);

			strcpy(spell_list[panel].entry[type == 'w' ? 0 : 1][nchar - 'a'].name, name);
			strcpy(spell_list[panel].entry[type == 'w' ? 0 : 1][nchar - 'a'].desc[0], d1);
			strcpy(spell_list[panel].entry[type == 'w' ? 0 : 1][nchar - 'a'].desc[1], d2);
			strcpy(spell_list[panel].entry[type == 'w' ? 0 : 1][nchar - 'a'].desc[2], d3);
			strcpy(spell_list[panel].entry[type == 'w' ? 0 : 1][nchar - 'a'].desc[3], d4);
		}

		fclose(stream);
		LOG(llevDebug, " Found file! (%d/%x)", srv_client_files[SRV_CLIENT_SPELLS].len, srv_client_files[SRV_CLIENT_SPELLS].crc);
	}

	LOG(llevDebug, " Done.\n");
}
