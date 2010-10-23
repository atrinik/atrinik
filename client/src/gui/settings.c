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

/** Option list set */
struct _dialog_list_set option_list_set;

/** Option tabs. */
const char *opt_tab[] =
{
	"General",
	"Client",
	"Map",
	"Sound",
	"Fullscreen flags",
	"Windowed flags",
	"Debug",
	NULL
};

/** Selection types */
enum
{
	SEL_BUTTON,
	SEL_CHECKBOX,
	SEL_RANGE,
	SEL_TEXT
};

/** The actual options. */
_option opt[] =
{
	/* General */
	{"Playerdoll:", "Whether to always show the playerdoll.\nIf unchecked, the playerdoll is only shown while the inventory is open.", "", SEL_CHECKBOX, 0, 1, 1, 0, &options.playerdoll, VAL_BOOL},
	{"Show yourself targeted:", "Show your name in the target area instead of blank.", "", SEL_CHECKBOX, 0, 1, 1, 0, &options.show_target_self, VAL_BOOL},
	{"Show Tooltips:", "Show tooltips when hovering with the mouse over items.", "", SEL_CHECKBOX, 0, 1, 1, 1, &options.show_tooltips, VAL_BOOL},
	{"Key-info in Dialog menus:", "", "", SEL_CHECKBOX, 0, 1, 1, 1, &options.show_d_key_infos, VAL_BOOL},
	{"Collect All Items:", "Don't ask for number of items to get, just get all of them.", "", SEL_CHECKBOX, 0, 1, 1, 0, &options.collectAll, VAL_BOOL},
	{"Exp display:", "The format key is: ~4nl~ = For next level; ~tnl~ = Till next level;\n~LExp~ = Level exp; ~TExp~ = Total exp;", "Level/LExp#LExp\\%#LExp/LExp 4nl#TExp/TExp 4nl#(LExp\\%) LExp tnl", SEL_RANGE, 0, 4, 1, 4, &options.expDisplay, VAL_INT},
	{"Chat Timestamps:", "Show a timestamp before each chat message.", "Disabled#HH:MM#HH:MM:SS#H:MM AM/PM#H:MM:SS AM/PM", SEL_RANGE, 0, 4, 1, 0, &options.chat_timestamp, VAL_INT},
	{"Font size in chat boxes:", "Font size used in chat boxes on left and right.", "10px#11px#12px#13px#14px#15px#16px", SEL_RANGE, 0, 6, 1, 0, &options.chat_font_size, VAL_INT},
	{"Maximum chat lines:", "Maximum number of lines in the chat boxes.", "", SEL_RANGE, 20, 1000, 10, 200, &options.chat_max_lines, VAL_INT},
	{"#", "", "", 0, 0, 0, 0, 0, 0, 0},

	/* Client */
	{"Fullscreen:", "Toogle fullscreen to windowed mode.\nNOTE: You need to restart the client.", "", SEL_CHECKBOX, 0, 1, 1, 0, &options.fullscreen, VAL_BOOL},
	{"Resolution:", "The resolution of the screen/window.\nIf you change to lower resolutions your GUI-windows may be hidden.", "Custom#800x600#960x600#1024x768#1100x700#1280x720#1280x800#1280x960#1280x1024#1440x900#1400x1050#1600x1200#1680x1050#1920x1080#1920x1200#2048x1536#2560x1600", SEL_RANGE, 0, 15, 1, 0, &options.resolution, VAL_INT},
	{"Automatic bpp:", "Use always the same bits per pixel like your default windows.\nNOTE: You need to restart the client.", "", SEL_CHECKBOX, 0, 1, 1, 1, &options.auto_bpp_flag, VAL_BOOL},
	{"Colordeep:", "Use this bpp for fullscreen mode. Overruled by automatic bpp.\nNOTE: You need to restart the client.", "8 bpp#16 bpp#32 bpp", SEL_RANGE, 0, 2, 1, 1, &options.video_bpp, VAL_INT},
	{"Textwindows use alpha:", "Make the text window transparent.\nWARNING: Don't use this if you have a very slow computer.", "", SEL_CHECKBOX, 0, 1, 1, 1, &options.use_TextwinAlpha, VAL_INT},
	{"Textwindows alpha value:", "Transparent value. Higher = darker", "", SEL_RANGE, 0, 255, 5, 110, &options.textwin_alpha, VAL_INT},
	{"Save CPU time with sleep():", "Client eats less cput time when set.", "", SEL_CHECKBOX, 0, 1, 1, 0, &options.max_speed, VAL_BOOL},
	{"Sleep time in ms:", "Time the client will sleep. Used with Save CPU time.", "", SEL_RANGE, 0, 1000, 1, 10, &options.sleep, VAL_INT},
	{"Key repeat speed:", "How fast to repeat a held down key.", "Off#Slow#Medium#Fast", SEL_RANGE, 0, 3, 1, 2, &options.key_repeat, VAL_INT},
	{"Disable file updates:", "If on, will not update sound effects/background music/etc on server\nconnect. This may be useful for users with low bandwidth.", "", SEL_CHECKBOX, 0, 1, 1, 0, &options.disable_updates, VAL_BOOL},
	{"#", "", "", 0, 0, 0, 0, 0, 0, 0},

	/* Map */
	{"Player Names:", "Show names of players above their heads.", "show no names#show all names#show only other#show only your", SEL_RANGE, 0, 3,1, 2, &options.player_names, VAL_INT},
	{"Playfield start X:", "The X-position of the playfield.", "", SEL_RANGE, -200, 1000, 10, 0, &options.mapstart_x, VAL_INT},
	{"Playfield start Y:", "The Y-position of the playfield.", "", SEL_RANGE, -200, 700, 10, 10, &options.mapstart_y, VAL_INT},
	{"Playfield zoom:", "The zoom percentage of the playfield.", "", SEL_RANGE, 50, 200, 5, 100, &options.zoom, VAL_INT},
	{"Smooth zoom:", "Whether to use smooth zoom on the playfield.\nWarning: Very CPU intensive.", "", SEL_CHECKBOX, 0, 1, 1, 1, &options.zoom_smooth, VAL_BOOL},
	{"Low health warning:", "Shows a low health warning above your head.\nActivated if health is less than the given percent value.", "", SEL_RANGE, 0, 100, 5, 0, &options.warning_hp, VAL_INT},
	{"Low food warning:", "Shows a low food warning above your head.\nActivated if food is less than the given percent value.", "", SEL_RANGE, 0, 100, 5, 5, &options.warning_food, VAL_INT},
	{"#", "", "", 0, 0, 0, 0, 0, 0, 0},

	/* Sound */
	{"Sound volume:", "Set sound volume for effects.", "", SEL_RANGE, 0, 100, 5, 100, &options.sound_volume, VAL_INT},
	{"Music volume:", "Set music volume for background.", "", SEL_RANGE, 0, 100, 5, 80, &options.music_volume, VAL_INT},
	{"#", "", "", 0, 0, 0, 0, 0, 0, 0},

	/* Fullscreen Flags */
	{"Hardware Surface:", "Don't change unless you know what you're doing\nNOTE: You need to restart the client.", "", SEL_CHECKBOX, 0, 1, 1, 0, &options.Full_HWSURFACE, VAL_BOOL},
	{"Software Surface:", "Don't change unless you know what you're doing.\nNOTE: You need to restart the client.", "", SEL_CHECKBOX, 0, 1, 1, 1, &options.Full_SWSURFACE, VAL_BOOL},
	{"Hardware Accel:", "Don't change unless you know what you're doing.\nNOTE: You need to restart the client.", "",SEL_CHECKBOX, 0, 1, 1, 1, &options.Full_HWACCEL, VAL_BOOL},
	{"Doublebuffer:", "Don't change unless you know what you're doing.\nNOTE: You need to restart the client.", "",SEL_CHECKBOX, 0, 1, 1, 0, &options.Full_DOUBLEBUF, VAL_BOOL},
	{"Any format:", "Don't change unless you know what you're doing.\nNOTE: You need to restart the client.", "",SEL_CHECKBOX, 0, 1, 1, 1, &options.Full_ANYFORMAT, VAL_BOOL},
	{"Async blit:", "Don't change unless you know what you're doing.\nNOTE: You need to restart the client.", "",SEL_CHECKBOX, 0, 1, 1, 0, &options.Full_ASYNCBLIT, VAL_BOOL},
	{"Hardware Palette:", "Don't change unless you know what you're doing.\nNOTE: You need to restart the client.", "",SEL_CHECKBOX, 0, 1, 1, 1, &options.Full_HWPALETTE, VAL_BOOL},
	{"Resizeable:", "Don't change unless you know what you're doing.\nNOTE: You need to restart the client.", "",SEL_CHECKBOX, 0, 1, 1, 0, &options.Full_RESIZABLE, VAL_BOOL},
	{"No frame:", "Don't change unless you know what you're doing.\nNOTE: You need to restart the client.", "", SEL_CHECKBOX, 0, 1, 1, 0, &options.Full_NOFRAME, VAL_BOOL},
	{"RLE accel:", "Don't change unless you know what you're doing.\nNOTE: You need to restart the client.", "", SEL_CHECKBOX, 0, 1, 1, 1, &options.Full_RLEACCEL, VAL_BOOL},
	{"#", "", "", 0, 0, 0, 0, 0, 0, 0},

	/* Windowed flags */
	{"Window Hardware Surface:", "Don't change unless you know what you're doing.\nNOTE: You need to restart the client.", "", SEL_CHECKBOX, 0, 1, 1, 0, &options.Win_HWSURFACE, VAL_BOOL},
	{"Window Software Surface:", "Don't change unless you know what you're doing.\nNOTE: You need to restart the client.", "", SEL_CHECKBOX, 0, 1, 1, 1, &options.Win_SWSURFACE, VAL_BOOL},
	{"Window Hardware Accel:", "Don't change unless you know what you're doing.\nNOTE: You need to restart the client.", "", SEL_CHECKBOX, 0, 1, 1, 0, &options.Win_HWACCEL, VAL_BOOL},
	{"Window Doublebuffer:", "Don't change unless you know what you're doing.\nNOTE: You need to restart the client.", "", SEL_CHECKBOX, 0, 1, 1, 0, &options.Win_DOUBLEBUF, VAL_BOOL},
	{"Window Any format:", "Don't change unless you know what you're doing.\nNOTE: You need to restart the client.", "", SEL_CHECKBOX, 0, 1, 1, 1, &options.Win_ANYFORMAT, VAL_BOOL},
	{"Window Async blit:", "Don't change unless you know what you're doing.\nNOTE: You need to restart the client.", "", SEL_CHECKBOX, 0, 1, 1, 0, &options.Win_ASYNCBLIT, VAL_BOOL},
	{"Window Hardware Palette:", "Don't change unless you know what you're doing.\nNOTE: You need to restart the client.", "", SEL_CHECKBOX, 0, 1, 1, 1, &options.Win_HWPALETTE, VAL_BOOL},
	{"Window Resizeable:", "Don't change unless you know what you're doing.\nNOTE: You need to restart the client.", "", SEL_CHECKBOX, 0, 1, 1, 1, &options.Win_RESIZABLE, VAL_BOOL},
	{"Window No frame:", "Don't change unless you know what you're doing.\nNOTE: You need to restart the client.", "", SEL_CHECKBOX, 0, 1, 1, 0, &options.Win_NOFRAME, VAL_BOOL},
	{"Window RLE accel:", "Don't change unless you know what you're doing.\nNOTE: You need to restart the client.", "", SEL_CHECKBOX, 0, 1, 1, 1, &options.Win_RLEACCEL, VAL_BOOL},
	{"#", "", "", 0, 0, 0, 0, 0, 0, 0},

	/* Debug */
	{"Show Framerate:", "Show the framerate.", "", SEL_CHECKBOX, 0, 1, 1, 0, &options.show_frame, VAL_BOOL},
	{"Force Redraw:", "Forces the system to redraw EVERY frame.", "", SEL_CHECKBOX, 0, 1, 1, 0, &options.force_redraw, VAL_BOOL},
	{"Use Update Rect:", "", "", SEL_CHECKBOX, 0, 1, 1, 0, &options.use_rect, VAL_BOOL},
	{"Reload user's graphics", "If on, always try to reload faces from user's graphics (gfx_user)\ndirectory, even if they have been reloaded previously.\nThis is especially useful when creating new images and testing out how\nthey look in the game.", "", SEL_CHECKBOX, 0, 1, 1, 0, &options.reload_gfx_user, VAL_BOOL},
	{"#", "", "", 0, 0, 0, 0, 0, 0, 0},

	{0, "", "", 0, 0, 0, 0, 0, 0, 0},
};

/**
 * Parse a given value to a string.
 * @param value Value to parse.
 * @param type @ref value_type "Type" of the value.
 * @return Static character array representing the given value. */
static char *get_value(void *value, int type)
{
	static char txt_value[20];

	switch (type)
	{
		case VAL_INT:
			snprintf(txt_value, sizeof(txt_value), "%d", *((int *) value));
			return txt_value;

		case VAL_U32:
			snprintf(txt_value, sizeof(txt_value), "%d", *((uint32 *) value));
			return txt_value;

		case VAL_CHAR:
			snprintf(txt_value, sizeof(txt_value), "%d", *((uint8 *) value));
			return txt_value;

		default:
			return NULL;
	}
}

/**
 * Draw all options for the actual options group.
 * @param x X position.
 * @param y Y position. */
void optwin_draw_options(int x, int y)
{
#define LEN_NAME 111
	int i = -1, pos = 0, max = 0;
	/* for info text */
	int y2 = y + 344;
	int mxy_opt = -1;
	int page = option_list_set.group_nr;
	int id = 0;
	int mx, my, mb, tmp;

	mb = SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT);

	/* Find actual page */
	while (page && opt[++i].name)
	{
		if (opt[i].name[0] == '#')
		{
			page--;
		}
	}

	/* Draw actual page */
	while (opt[++i].name && opt[i].name[0] != '#')
	{
		max++;
		StringBlt(ScreenSurface, &SystemFont, opt[i].name, x + 1, y + 3, COLOR_BLACK, NULL, NULL);

		switch (opt[i].sel_type)
		{
			case SEL_CHECKBOX:
				tmp = COLOR_WHITE;

				if (option_list_set.entry_nr == max - 1)
				{
					tmp = COLOR_HGOLD;
					/* Remember this tab for later use */
					if (mxy_opt == -1)
					{
						mxy_opt = i;
					}
				}

				if (mx > x && mx < x + 280 && my > y && my < y + 20 )
				{
					tmp = COLOR_GREEN;
					/* Remember this tab for later use */
					mxy_opt = i;
				}

				StringBlt(ScreenSurface, &SystemFont, opt[i].name, x, y + 2, tmp, NULL, NULL);

				sprite_blt(Bitmaps[BITMAP_DIALOG_CHECKER], x + LEN_NAME, y, NULL, NULL);

				if (*((int *) opt[i].value) == 1)
				{
					StringBlt(ScreenSurface, &SystemFont, "X", x + LEN_NAME + 8, y + 2, COLOR_BLACK, NULL, NULL);
					StringBlt(ScreenSurface, &SystemFont, "X", x + LEN_NAME + 7, y + 1, COLOR_WHITE, NULL, NULL);
				}

				if ((pos == option_list_set.entry_nr && option_list_set.key_change) || (mb && mb_clicked && active_button < 0 && mx > x + LEN_NAME && mx < x + LEN_NAME + 20 && my > y && my < y + 18))
				{
					mb_clicked = 0;
					option_list_set.key_change = 0;

					if (*((int *) opt[i].value) == 1)
					{
						*((int *) opt[i].value) = 0;
					}
					else
					{
						*((int *) opt[i].value) = 1;
					}
				}
				break;

			case SEL_RANGE:
			{
#define LEN_VALUE 100
				SDL_Rect box;
				box.x = x + LEN_NAME, box.y = y + 1;
				box.h = 16, box.w = LEN_VALUE;

				tmp = COLOR_WHITE;

				if (option_list_set.entry_nr == max - 1)
				{
					tmp = COLOR_HGOLD;

					/* Remember this tab for later use */
					if (mxy_opt == -1)
					{
						mxy_opt = i;
					}
				}

				if (mx > x && mx < x + 280 && my > y && my < y + 20)
				{
					tmp = COLOR_GREEN;
					/* Remember this tab for later use */
					mxy_opt = i;
				}

				StringBlt(ScreenSurface, &SystemFont, opt[i].name, x, y + 2, tmp, NULL, NULL);
				SDL_FillRect(ScreenSurface, &box, 0);

				if (*opt[i].val_text == '\0')
				{
					StringBlt(ScreenSurface, &SystemFont, get_value(opt[i].value, opt[i].value_type), box.x + 2, y + 2, COLOR_WHITE, NULL, NULL);
				}
				else
				{
#define MAX_LEN 40
					char text[MAX_LEN + 1];
					int o= *((int *) opt[i].value);
					int p = 0, q = -1;

					/* Find starting position of string */
					while (o && opt[i].val_text[p])
					{
						if (opt[i].val_text[p++] == '#')
						{
							o--;
						}
					}

					/* Find ending position of string */
					while (q++ < MAX_LEN  && opt[i].val_text[p])
					{
						if ((text[q] = opt[i].val_text[p++]) == '#')
						{
							break;
						}
					}

					text[q] = '\0';
					StringBlt(ScreenSurface, &SystemFont,text, box.x + 2, y + 2, COLOR_WHITE, NULL, NULL);
#undef MAX_LEN
				}

				sprite_blt(Bitmaps[BITMAP_DIALOG_RANGE_OFF], x + LEN_NAME + LEN_VALUE, y, NULL, NULL);

				/* Keyboard event */
				if (option_list_set.key_change && option_list_set.entry_nr == pos)
				{
					if (option_list_set.key_change == -1)
					{
						add_value(opt[i].value, opt[i].value_type,-opt[i].deltaRange, opt[i].minRange, opt[i].maxRange);
					}
					else if (option_list_set.key_change == 1)
					{
						add_value(opt[i].value, opt[i].value_type, opt[i].deltaRange, opt[i].minRange, opt[i].maxRange);
					}

					option_list_set.key_change = 0;
				}

				if (mx > x + LEN_NAME + LEN_VALUE && mx < x + LEN_NAME + LEN_VALUE + 14 && my > y && my < y + 18)
				{
					/* 2 buttons per row */
					if (mb && active_button < 0)
					{
						active_button = id + 1;
					}

					if (active_button == id + 1)
					{
						sprite_blt(Bitmaps[BITMAP_DIALOG_RANGE_L], x + LEN_NAME + LEN_VALUE, y, NULL, NULL);

						if (!mb)
						{
							add_value(opt[i].value, opt[i].value_type, -opt[i].deltaRange, opt[i].minRange, opt[i].maxRange);
						}
					}
				}
				else if (mx > x + LEN_NAME + LEN_VALUE + 14 && mx < x + LEN_NAME + LEN_VALUE + 28 && my > y && my < y + 18)
				{
					if (mb && active_button < 0)
					{
						active_button = id;
					}

					if (active_button == id)
					{
						sprite_blt(Bitmaps[BITMAP_DIALOG_RANGE_R], x + LEN_NAME+LEN_VALUE + 14, y, NULL, NULL);

						if (!mb)
						{
							add_value(opt[i].value, opt[i].value_type, opt[i].deltaRange, opt[i].minRange, opt[i].maxRange);
						}
					}
				}
#undef LEN_VALUE
				break;
			}

			case SEL_BUTTON:
				sprite_blt(Bitmaps[BITMAP_DIALOG_BUTTON_UP], x, y, NULL, NULL);
				break;
		}

		y += 20;
		pos++;
		id += 2;
	}

	if (option_list_set.entry_nr > max - 1)
	{
		option_list_set.entry_nr = max - 1;
	}

	/* Print the info text */
	x += 20;

	if (mxy_opt >= 0)
	{
		char *cp, buf[MAX_BUF];
		int y_tmp = 0;

		strncpy(buf, opt[mxy_opt].info, sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = '\0';
		cp = strtok(buf, "\n");

		while (cp)
		{
			StringBlt(ScreenSurface, &SystemFont, cp, x + 11, y2 + 1 + y_tmp, COLOR_BLACK, NULL, NULL);
			StringBlt(ScreenSurface, &SystemFont, cp, x + 10, y2 + y_tmp, COLOR_WHITE, NULL, NULL);
			y_tmp += 12;
			cp = strtok(NULL, "\n");
		}
	}
#undef LEN_NAME
}

/**
 * Show the options window. */
void show_optwin()
{
	char buf[128];
	int x, y;
	int mx, my, mb;
	int numButton = 0;

	mb = SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT);
	x = Screensize->x / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->w / 2;
	y = Screensize->y / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->h / 2;
	sprite_blt(Bitmaps[BITMAP_DIALOG_BG], x, y, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_DIALOG_TITLE_OPTIONS], x + 250 - Bitmaps[BITMAP_DIALOG_TITLE_OPTIONS]->bitmap->w / 2, y + 14, NULL, NULL);
	add_close_button(x, y, MENU_OPTION);

	draw_tabs(opt_tab, &option_list_set.group_nr, "Option Group", x + 8, y + 70);
	optwin_draw_options(x + 130, y + 90);

	sprintf(buf, "~SHIFT~ + ~%c%c~ to select group            ~%c%c~ to select option          ~%c%c~ to change option", ASCII_UP, ASCII_DOWN, ASCII_UP, ASCII_DOWN, ASCII_RIGHT, ASCII_LEFT);
	StringBlt(ScreenSurface, &SystemFont, buf, x + 135, y + 410, COLOR_WHITE, NULL, NULL);

	/* Mark active entry */
	StringBlt(ScreenSurface, &SystemFont, ">", x + TXT_START_NAME - 15, y + 10 + TXT_Y_START + option_list_set.entry_nr * 20, COLOR_HGOLD, NULL, NULL);

	/* save button */
	if (add_button(x + 25, y + 454, numButton++, BITMAP_DIALOG_BUTTON_UP, "Done", "~D~one"))
	{
		check_menu_keys(MENU_OPTION, SDLK_d);
	}

	if (!mb)
	{
		active_button = -1;
	}
}
