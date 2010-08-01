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
		else if (spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][i].flag == LIST_ENTRY_USED)
		{
			StringBlt(ScreenSurface, &SystemFont, spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][i].name, x + TXT_START_NAME, y + TXT_Y_START, COLOR_GREY, NULL, NULL);
		}
	}

	x += 160;
	y += 120;

	/* Print spell description */
	if (spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][spell_list_set.entry_nr].flag != LIST_ENTRY_UNUSED)
	{
		char *tmpbuf, *cp;
		int tmp_y = 0, width = 0, len;

		/* Selected */
		if (spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][spell_list_set.entry_nr].flag == LIST_ENTRY_KNOWN && mb && mx > x - 40 && mx < x - 10 && my > y + 10 && my < y + 43)
		{
			dblclk = 0;
			check_menu_keys(MENU_SPELL, SDLK_RETURN);
		}

		blit_face(spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][spell_list_set.entry_nr].icon, x - 42, y + 10);

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

		tmpbuf = strdup(spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][spell_list_set.entry_nr].desc);
		cp = strtok(tmpbuf, " ");

		/* Loop through spaces */
		while (cp)
		{
			len = get_string_pixel_length(cp, &SystemFont) + SystemFont.c[' '].w + SystemFont.char_offset;

			/* Do we need to adjust for the next line? */
			if (width + len > MAX_MS_DESC_LINE)
			{
				width = 0;
				tmp_y += 12;
			}

			/* We hit the max */
			if (tmp_y >= 48)
			{
				break;
			}

			StringBlt(ScreenSurface, &SystemFont, cp, x - 2 + width, y + 1 + tmp_y, COLOR_BLACK, NULL, NULL);
			StringBlt(ScreenSurface, &SystemFont, cp, x - 3 + width, y + tmp_y, COLOR_WHITE, NULL, NULL);
			width += len;

			cp = strtok(NULL, " ");
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
	size_t i, ii, iii;
	FILE *fp;
	struct stat sb;
	size_t st_size, numread;
	char *contents, line[HUGE_BUF];

	for (i = 0; i < SPELL_LIST_MAX; i++)
	{
		for (ii = 0; ii < SPELL_LIST_CLASS; ii++)
		{
			for (iii = 0; iii < DIALOG_LIST_ENTRY; iii++)
			{
				spell_list[i].entry[ii][iii].flag = LIST_ENTRY_UNUSED;
				spell_list[i].entry[ii][iii].name[0] = '\0';
			}
		}
	}

	spell_list_set.class_nr = 0;
	spell_list_set.entry_nr = 0;
	spell_list_set.group_nr = 0;

	srv_client_files[SRV_FILE_SPELLS_V2].len = 0;
	srv_client_files[SRV_FILE_SPELLS_V2].crc = 0;

	LOG(llevInfo, "Reading %s...\n", FILE_CLIENT_SPELLS);
	fp = fopen_wrapper(FILE_CLIENT_SPELLS, "r");

	if (!fp)
	{
		return;
	}

	fstat(fileno(fp), &sb);
	st_size = sb.st_size;
	srv_client_files[SRV_FILE_SPELLS_V2].len = st_size;

	contents = malloc(st_size);
	numread = fread(contents, 1, st_size, fp);
	srv_client_files[SRV_FILE_SPELLS_V2].crc = crc32(1L, (const unsigned char FAR *) contents, numread);
	free(contents);
	rewind(fp);

	while (fgets(line, sizeof(line) - 1, fp))
	{
		char *spell_name, *icon, desc[HUGE_BUF];
		int spell_type, spell_path;

		line[strlen(line) - 1] = '\0';
		spell_name = strdup(line);

		if (!fgets(line, sizeof(line) - 1, fp))
		{
			LOG(llevBug, "  Got unexpected EOF reading spells file.\n");
			break;
		}

		spell_type = atoi(line);

		if (!fgets(line, sizeof(line) - 1, fp))
		{
			LOG(llevBug, "  Got unexpected EOF reading spells file.\n");
			break;
		}

		spell_path = atoi(line);

		if (!fgets(line, sizeof(line) - 1, fp))
		{
			LOG(llevBug, "  Got unexpected EOF reading spells file.\n");
			break;
		}

		line[strlen(line) - 1] = '\0';
		icon = strdup(line);
		desc[0] = '\0';

		while (fgets(line, sizeof(line) - 1, fp))
		{
			if (!strcmp(line, "end\n"))
			{
				_spell_list_entry *entry;

				for (i = 0; i < DIALOG_LIST_ENTRY; i++)
				{
					if (spell_list[spell_path].entry[spell_type - 1][i].flag == LIST_ENTRY_UNUSED)
					{
						break;
					}
				}

				if (i == DIALOG_LIST_ENTRY)
				{
					break;
				}

				entry = &spell_list[spell_path].entry[spell_type - 1][i];
				entry->flag = LIST_ENTRY_USED;
				strncpy(entry->name, spell_name, sizeof(entry->name));
				desc[strlen(desc) - 1] = '\0';
				strncpy(entry->desc, desc, sizeof(entry->desc));
				strncpy(entry->icon_name, icon, sizeof(entry->icon_name));
				entry->icon = get_bmap_id(entry->icon_name);
				free(icon);
				free(spell_name);
				break;
			}

			strncat(desc, line, sizeof(desc) - strlen(desc) - 1);
		}
	}

	fclose(fp);
}

/**
 * Find a spell in the ::spell_list based on its name.
 * @param name Spell name to find.
 * @param[out] spell_group Will contain the spell's group.
 * @param[out] spell_class Will contain the spell's class.
 * @param[out] spell_nr Will contain the spell's nr.
 * @return 1 if the spell was found, 0 otherwise. */
int find_spell(const char *name, int *spell_group, int *spell_class, int *spell_nr)
{
	for (*spell_group = 0; *spell_group < SPELL_LIST_MAX; *spell_group += 1)
	{
		for (*spell_class = 0; *spell_class < SPELL_LIST_CLASS; *spell_class += 1)
		{
			for (*spell_nr = 0; *spell_nr < DIALOG_LIST_ENTRY; *spell_nr += 1)
			{
				if (spell_list[*spell_group].entry[*spell_class][*spell_nr].flag == LIST_ENTRY_UNUSED)
				{
					continue;
				}

				if (!strcmp(spell_list[*spell_group].entry[*spell_class][*spell_nr].name, name))
				{
					return 1;
				}
			}
		}
	}

	return 0;
}
