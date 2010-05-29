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
 * Implements most client dialogs (GUIs). */

#include "include.h"
#define TXT_START_NAME  136
#define TXT_START_LEVEL 370
#define TXT_START_EXP   400
#define TXT_Y_START      82
#define X_COL1 136
#define X_COL2 290
#define X_COL3 430

int dialog_new_char_warn = 0;

/** Selection types */
enum
{
	SEL_BUTTON,
	SEL_CHECKBOX,
	SEL_RANGE,
	SEL_TEXT
};

static int active_button = -1;

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

/** Dialog Y offset. */
int dialog_yoff = 0;

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
	{"Font size in chat boxes:", "Font size used in chat boxes on left and right.", "Normal#Bigger", SEL_RANGE, 0, 1, 1, 0, &options.chat_font_size, VAL_INT},
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
	{"#", "", "", 0, 0, 0, 0, 0, 0, 0},

	/* Map */
	{"Player Names:", "Show names of players above their heads.", "show no names#show all names#show only other#show only your", SEL_RANGE, 0, 3,1, 2, &options.player_names, VAL_INT},
	{"Playfield start X:", "The X-position of the playfield.", "", SEL_RANGE, -200, 1000, 10, 0, &options.mapstart_x, VAL_INT},
	{"Playfield start Y:", "The Y-position of the playfield.", "", SEL_RANGE, -200, 700, 10, 10, &options.mapstart_y, VAL_INT},
	{"Playfield zoom:", "The zoom percentage of the playfield.", "", SEL_RANGE, 50, 200, 5, 100, &options.zoom, VAL_INT},
	{"Smooth zoom:", "Whether to use smooth zoom on the playfield.\nWarning: Very CPU intensive.", "", SEL_CHECKBOX, 0, 1, 1, 0, &options.zoom_smooth, VAL_BOOL},
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

/** The skill groups. */
static const char *skill_tab[] =
{
	"Agility",
	"Mental",
	"Magic",
	"Person",
	"Physique",
	"Wisdom",
	"Misc",
	NULL
};

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
 * Draw a single frame.
 * @param x X position.
 * @param y Y position.
 * @param w Width of the frame.
 * @param h Height of the frame. */
void draw_frame(int x, int y, int w, int h)
{
	SDL_Rect box;

	box.x = x;
	box.y = y;
	box.h = h;
	box.w = 1;
	SDL_FillRect(ScreenSurface, &box, sdl_gray4);
	box.x = x + w;
	box.h++;
	SDL_FillRect(ScreenSurface, &box, sdl_gray3);
	box.x = x;
	box.y+= h;
	box.w = w;
	box.h = 1;
	SDL_FillRect(ScreenSurface, &box, sdl_gray4);
	box.x++;
	box.y = y;
	SDL_FillRect(ScreenSurface, &box, sdl_gray3);
}

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
 * Add a close button and handle mouse events over it.
 * @param x X position.
 * @param y Y position.
 * @param menu Menu the button is on. */
void add_close_button(int x, int y, int menu)
{
	int mx, my, mb;

	mb = SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT);
	StringBlt(ScreenSurface, &SystemFont, "X", x + 463, y + 28, COLOR_BLACK, NULL, NULL);

	if (mx >x + 459 && mx < x + 469 && my > y + 27 && my < y + 39)
	{
		StringBlt(ScreenSurface, &SystemFont, "X", x + 462, y + 27, COLOR_HGOLD, NULL, NULL);

		if (mb && mb_clicked)
		{
			check_menu_keys(menu, SDLK_ESCAPE);
		}
	}
	else
	{
		StringBlt(ScreenSurface, &SystemFont, "X", x + 462, y + 27, COLOR_WHITE, NULL, NULL);
	}
}

/**
 * Add a button and handle mouse events over it.
 * @param x X position.
 * @param y Y position.
 * @param id Button ID.
 * @param gfxNr ID of the button bitmap to draw.
 * @param text Text to draw.
 * @param text_h Text for keyboard user.
 * @return 1 if the button was pressed, 0 otherwise. */
static int add_button(int x, int y, int id, int gfxNr, char *text, char *text_h)
{
	char *text_sel;
	int ret = 0, mx, my, mb;

	mb = SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT);

	if (text_h)
	{
		text_sel = text_h;
	}
	else
	{
		text_sel = text;
	}

	sprite_blt(Bitmaps[gfxNr], x, y, NULL, NULL);

	if (mx >x && my >y && mx < x + Bitmaps[gfxNr]->bitmap->w && my < y + Bitmaps[gfxNr]->bitmap->h)
	{
		if (mb && mb_clicked && active_button < 0)
		{
			active_button = id;
		}

		if (active_button == id)
		{
			sprite_blt(Bitmaps[gfxNr + 1], x, y++, NULL, NULL);

			if (!mb)
			{
				ret = 1;
			}
		}

		StringBlt(ScreenSurface, &SystemFont, text, x + 11, y + 2, COLOR_BLACK, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, text_sel, x + 10, y + 1, COLOR_HGOLD, NULL, NULL);
	}
	else
	{
		StringBlt(ScreenSurface, &SystemFont, text, x + 11, y + 2, COLOR_BLACK, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, text_sel, x + 10, y + 1, COLOR_WHITE, NULL, NULL);
	}

	return ret;
}

/**
 * Add a group button and handle mouse events over it.
 * @param x X position.
 * @param y Y position.
 * @param id Button ID.
 * @param gfxNr ID of the button bitmap to draw.
 * @param text Text to draw.
 * @param text_h Text for keyboard user.
 * @return 1 if the button was pressed, 0 otherwise. */
static int add_gr_button(int x, int y, int id, int gfxNr, const char *text, const char *text_h)
{
	const char *text_sel;
	int ret = 0;
	int mx, my, mb;

	mb = SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT);
	/* use text_h (=highlighted char for keyboard user) if available. */
	if (text_h)
		text_sel = text_h;
	else
		text_sel = text;

	if (id)
		sprite_blt(Bitmaps[++gfxNr], x, y++, NULL, NULL);
	else
		sprite_blt(Bitmaps[gfxNr], x, y, NULL, NULL);

	StringBlt(ScreenSurface, &SystemFont, text, x + 11, y + 2, COLOR_BLACK, NULL, NULL);

	if (mx >x && my >y && mx < x + Bitmaps[gfxNr]->bitmap->w && my < y + Bitmaps[gfxNr]->bitmap->h)
	{
		if (mb && mb_clicked)
			ret = 1;

		StringBlt(ScreenSurface, &SystemFont, text_sel, x + 10, y + 1, COLOR_HGOLD, NULL, NULL);
	}
	else
		StringBlt(ScreenSurface, &SystemFont, text_sel, x + 10, y + 1, COLOR_WHITE, NULL, NULL);

	return ret;
}

/**
 * Add a range box.
 * @param x X position.
 * @param y Y position.
 * @param id Button ID.
 * @param text_w Size of text field.
 * @param text_x If 0 text field will be left of rangebox, if 1 text field
 * will be right of rangebox.
 * @param text Text.
 * @param color Color.
 * @return  */
int add_rangebox(int x, int y, int id, int text_w, int text_x, const char *text, int color)
{
	static int active = -1;
	SDL_Rect box;
	int mx, my, mb;
	int width = Bitmaps[BITMAP_DIALOG_RANGE_OFF]->bitmap->w;

	mb = SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT);

	/* Box right of text */
	if (text_x)
	{
		text_x = x + width + 2;
	}
	else
	{
		text_x = x;
		x += text_w;
	}

	sprite_blt(Bitmaps[BITMAP_DIALOG_RANGE_OFF], x, y, NULL, NULL);
	box.x = text_x - 2;
	box.y = y + 1;
	box.w = text_w + 2;
	box.h = 16;
	SDL_FillRect(ScreenSurface, &box, sdl_gray1);
	StringBlt(ScreenSurface, &SystemFont, text, text_x + 1, y + 3, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, text, text_x, y + 2, color, NULL, NULL);

	/* Check for action */
	if (mx >x && my > y && my < y + 18)
	{
		if (mx < x + width / 2)
		{
			if (mb && active < 0)
			{
				active = id;
			}

			if (active == id)
			{
				sprite_blt(Bitmaps[BITMAP_DIALOG_RANGE_L], x, y, NULL, NULL);

				if (!mb)
				{
					active = -1;
					return -1;
				}
			}
		}
		else if (mx < x + width)
		{
			/* A rangebox has 2 buttons */
			if (mb && active < 0)
			{
				active = id + 1;
			}

			if (active == id + 1)
			{
				sprite_blt(Bitmaps[BITMAP_DIALOG_RANGE_R], x + width / 2, y, NULL, NULL);

				if (!mb)
				{
					active = -1;
					return 1;
				}
			}
		}
	}

	return 0;
}

/**
 * Add offset to an integer value.
 * @param value Value to add to.
 * @param type @ref value_type "Type" of the value.
 * @param offset What to add to value.
 * @param min Minimum value.
 * @param max Maximum value. */
void add_value(void *value, int type, int offset, int min, int max)
{
	switch (type)
	{
		case VAL_INT:
			*((int *) value) += offset;

			if (*((int *) value) > max)
			{
				*((int *) value) = max;
			}

			if (*((int *) value) < min)
			{
				*((int *) value) = min;
			}

			break;

		case VAL_U32:
			*((uint32 *) value) += offset;

			if (*((uint32 *) value) > (uint32) max)
			{
				*((uint32 *) value) = (uint32) max;
			}

			if (*((uint32 *) value) < (uint32) min)
			{
				*((uint32 *) value) = (uint32) min;
			}

			break;

		default:
			break;
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
 * Draw tabs on the left side of a window.
 * @param tabs[] The tabs to draw.
 * @param[out] act_tab Currently active tab.
 * @param head_text Header text, above the tabs.
 * @param x X position.
 * @param y Y position. */
static void draw_tabs(const char *tabs[], int *act_tab, const char *head_text, int x, int y)
{
	int i = -1;
	int mx, my, mb;
	static int active = 0;

	mb = SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT);
	sprite_blt(Bitmaps[BITMAP_DIALOG_TAB_START], x, y - 10, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_DIALOG_TAB], x, y, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, head_text, x + 15, y + 4, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, head_text, x + 14, y + 3, COLOR_WHITE, NULL, NULL);
	y += 17;
	sprite_blt(Bitmaps[BITMAP_DIALOG_TAB], x, y, NULL, NULL);
	y += 17;

	while (tabs[++i])
	{
		sprite_blt(Bitmaps[BITMAP_DIALOG_TAB], x, y, NULL, NULL);

		if (i == *act_tab)
		{
			sprite_blt(Bitmaps[BITMAP_DIALOG_TAB_SEL], x, y, NULL, NULL);
		}

		StringBlt(ScreenSurface, &SystemFont, tabs[i], x + 25, y + 4, COLOR_BLACK, NULL, NULL);

		if (mx > x && mx < x + 100 && my > y && my < y + 17)
		{
			StringBlt(ScreenSurface, &SystemFont, tabs[i], x + 24, y + 3, COLOR_HGOLD, NULL, NULL);

			if (mb && mb_clicked)
			{
				active = 1;
			}

			if (active)
			{
				*act_tab = i;
			}
		}
		else
		{
			StringBlt(ScreenSurface, &SystemFont, tabs[i], x + 24, y + 3, COLOR_WHITE, NULL, NULL);
		}

		y += 17;
	}

	sprite_blt(Bitmaps[BITMAP_DIALOG_TAB_STOP], x, y, NULL, NULL);

	if (!mb)
	{
		active = 0;
	}
}

/**
 * Show the skill list window. */
void show_skilllist()
{
	SDL_Rect box;
	char buf[256];
	int x, y, i;
	int mx, my, mb;
	static int active = 0, dblclk = 0;
	static Uint32 Ticks = 0;

	mb = SDL_GetMouseState(&mx, &my);

	/* Background */
	x= Screensize->x / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->w / 2;
	y= Screensize->y / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->h / 2;
	sprite_blt(Bitmaps[BITMAP_DIALOG_BG], x, y, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_DIALOG_TITLE_SKILL], x + 250 - Bitmaps[BITMAP_DIALOG_TITLE_SKILL]->bitmap->w / 2, y + 14, NULL, NULL);
	add_close_button(x, y, MENU_SKILL);

	/* Tabs */
	draw_tabs(skill_tab, &skill_list_set.group_nr, "Skill Group", x + 8, y + 70);

	sprintf(buf, "~SHIFT~ + ~%c%c~ to select group                  ~%c%c~ to select skill                    ~RETURN~ for use", ASCII_UP, ASCII_DOWN, ASCII_UP, ASCII_DOWN);
	StringBlt(ScreenSurface, &SystemFont, buf, x + 135, y + 410, COLOR_WHITE, NULL, NULL);

	/* Headline */
	StringBlt(ScreenSurface, &SystemFont, "Name", x + TXT_START_NAME + 1, y + TXT_Y_START - 1, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Name", x + TXT_START_NAME, y + TXT_Y_START - 2, COLOR_WHITE, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Level", x + TXT_START_LEVEL + 1, y + TXT_Y_START - 1, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Level", x + TXT_START_LEVEL, y + TXT_Y_START - 2, COLOR_WHITE, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Experience", x + TXT_START_EXP + 1, y + TXT_Y_START - 1, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Experience", x + TXT_START_EXP, y + TXT_Y_START - 2, COLOR_WHITE, NULL, NULL);

	box.x = x + 133;
	box.y = y + TXT_Y_START + 1;
	box.w = 329;
	box.h = 12;

	/* Frame for selection field */
	draw_frame(box.x - 1, box.y + 11, box.w + 1, 313);

	/* Print skill entries */
	if (!(mb & SDL_BUTTON(SDL_BUTTON_LEFT)))
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

				if (skill_list[skill_list_set.group_nr].entry[skill_list_set.entry_nr].flag == LIST_ENTRY_KNOWN)
				{
					check_menu_keys(MENU_SKILL, SDLK_RETURN);
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

			if (active && skill_list_set.entry_nr != (my - y - 12 - TXT_Y_START) / 12)
			{
				skill_list_set.entry_nr = (my - y - 12 - TXT_Y_START) / 12;

				dblclk = 0;
			}
		}
	}

	for (i = 0; i < DIALOG_LIST_ENTRY; i++)
	{
		y += 12;
		box.y += 12;

		if (i != skill_list_set.entry_nr)
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

		if (skill_list[skill_list_set.group_nr].entry[i].flag == LIST_ENTRY_KNOWN)
		{
			StringBlt(ScreenSurface, &SystemFont, skill_list[skill_list_set.group_nr].entry[i].name, x + TXT_START_NAME, y + TXT_Y_START, COLOR_WHITE, NULL, NULL);

			if (skill_list[skill_list_set.group_nr].entry[i].exp == -1)
			{
				strcpy(buf, "**");
			}
			else
			{
				sprintf(buf, "%d", skill_list[skill_list_set.group_nr].entry[i].exp_level);
			}

			StringBlt(ScreenSurface, &SystemFont, buf, x + TXT_START_LEVEL, y + TXT_Y_START, COLOR_WHITE, NULL, NULL);

			if (skill_list[skill_list_set.group_nr].entry[i].exp == -1)
			{
				strcpy(buf, "**");
			}
			else if (skill_list[skill_list_set.group_nr].entry[i].exp == -2)
			{
				strcpy(buf, "**");
			}
			else
			{
				sprintf(buf, "%"FMT64, skill_list[skill_list_set.group_nr].entry[i].exp);
			}

			StringBlt(ScreenSurface, &SystemFont, buf, x + TXT_START_EXP, y + TXT_Y_START, COLOR_WHITE, NULL, NULL);
		}
	}

	x += 160;
	y += 120;

	/* Print skill description */
	if (skill_list[skill_list_set.group_nr].entry[skill_list_set.entry_nr].flag >= LIST_ENTRY_KNOWN)
	{
		/* Selected */
		if ((mb & SDL_BUTTON(SDL_BUTTON_LEFT)) && mx > x - 40 && mx < x - 10 && my > y + 10 && my < y + 43)
		{
			check_menu_keys(MENU_SKILL, SDLK_RETURN);
		}

		sprite_blt(skill_list[skill_list_set.group_nr].entry[skill_list_set.entry_nr].icon, x - 42, y + 10, NULL, NULL);

		/* Print textblock */
		for (i = 0; i <= 3; i++)
		{
			StringBlt(ScreenSurface, &SystemFont, &skill_list[skill_list_set.group_nr].entry[skill_list_set.entry_nr].desc[i][0], x - 2, y + 1, COLOR_BLACK, NULL, NULL);
			StringBlt(ScreenSurface, &SystemFont, &skill_list[skill_list_set.group_nr].entry[skill_list_set.entry_nr].desc[i][0], x - 3, y, COLOR_WHITE, NULL, NULL);
			y += 13;
		}
	}

	if (!mb)
	{
		active_button = -1;
	}
}

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

/**
 * Show the keybinds window. */
void show_keybind()
{
	SDL_Rect box;
	char buf[256];
	int x, x2, y, y2, i;
	int mx, my, mb;
	int numButton =0;

	mb = SDL_GetMouseState(&mx, &my);

	/* Background */
	x = Screensize->x / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->w / 2;
	y = Screensize->y / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->h / 2;
	sprite_blt(Bitmaps[BITMAP_DIALOG_BG], x, y, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_DIALOG_TITLE_KEYBIND], x + 250 - Bitmaps[BITMAP_DIALOG_TITLE_KEYBIND]->bitmap->w / 2, y + 17, NULL, NULL);
	add_close_button(x, y, MENU_KEYBIND);

	sprintf(buf, "~SHIFT~ + ~%c%c~ to select group         ~%c%c~ to select macro          ~RETURN~ to change/create", ASCII_UP, ASCII_DOWN, ASCII_UP, ASCII_DOWN);
	StringBlt(ScreenSurface, &SystemFont, buf, x + 125, y + 410, COLOR_WHITE, NULL, NULL);

	/* Draw group tabs */
	i = 0;
	x2 = x + 8;
	y2 = y + 70;

	sprite_blt(Bitmaps[BITMAP_DIALOG_TAB_START], x2, y2 - 10, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_DIALOG_TAB], x2, y2, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Group", x2 + 15, y2 + 4, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Group", x2 + 14, y2 + 3, COLOR_WHITE, NULL, NULL);

	y2 += 17;
	sprite_blt(Bitmaps[BITMAP_DIALOG_TAB], x2, y2, NULL, NULL);
	y2 += 17;

	while (i < BINDKEY_LIST_MAX && bindkey_list[i].name[0])
	{
		sprite_blt(Bitmaps[BITMAP_DIALOG_TAB], x2, y2, NULL, NULL);

		if (i == bindkey_list_set.group_nr)
		{
			sprite_blt(Bitmaps[BITMAP_DIALOG_TAB_SEL], x2, y2, NULL, NULL);
		}

		StringBlt(ScreenSurface, &SystemFont, bindkey_list[i].name, x2 + 25, y2 + 4, COLOR_BLACK, NULL, NULL);

		if (mx > x2 && mx < x2 + 100 && my > y2 && my < y2 + 17)
		{
			StringBlt(ScreenSurface, &SystemFont, bindkey_list[i].name, x2 + 24, y2 + 3, COLOR_HGOLD, NULL, NULL);

			if (mb && bindkey_list_set.group_nr != i)
			{
				bindkey_list_set.group_nr = i;
			}
		}
		else
		{
			StringBlt(ScreenSurface, &SystemFont, bindkey_list[i].name, x2 + 24, y2 + 3, COLOR_WHITE, NULL, NULL);
		}

		y2 += 17;
		i++;
	}

	sprite_blt(Bitmaps[BITMAP_DIALOG_TAB_STOP], x2, y2, NULL, NULL);

	/* Headline */
	StringBlt(ScreenSurface, &SystemFont, "Macro", x + X_COL1 + 1, y + TXT_Y_START - 1, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Macro", x + X_COL1, y + TXT_Y_START - 2, COLOR_WHITE, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Key", x + X_COL2 + 1, y + TXT_Y_START - 1, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Key", x + X_COL2, y + TXT_Y_START - 2, COLOR_WHITE, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Repeat", x+X_COL3 + 1, y + TXT_Y_START - 1, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "~R~epeat", x+X_COL3, y + TXT_Y_START - 2, COLOR_WHITE, NULL, NULL);

	/* Save button */
	if (add_button(x + 25, y + 454, numButton++, BITMAP_DIALOG_BUTTON_UP, "Done", "~D~one"))
	{
		check_menu_keys(MENU_KEYBIND, SDLK_d);
	}

	box.x = x + 133;
	box.y = y + TXT_Y_START + 1;
	box.w = 329;
	box.h = 12;

	/* Frame for selection field */
	draw_frame(box.x - 1, box.y + 11, box.w + 1, 313);
	y2 = y;

	/* Print keybind entries */
	for (i = 0; i <OPTWIN_MAX_OPT; i++)
	{
		y += 12;
		box.y += 12;

		if (mb && mx > x + TXT_START_NAME && mx < x + TXT_START_NAME + 327 && my > y + TXT_Y_START && my < y + 12 + TXT_Y_START)
		{
			/* Cancel edit */
			if (mb & SDL_BUTTON(SDL_BUTTON_LEFT))
			{
				InputStringEscFlag = 1;
				keybind_status = KEYBIND_STATUS_NO;
			}

			bindkey_list_set.entry_nr = i;

			if ((mb & SDL_BUTTON(SDL_BUTTON_RIGHT)) && mb_clicked && mx > x + X_COL1)
			{
				/* Macro name + key value */
				if (mx < x + X_COL3)
				{
					check_menu_keys(MENU_KEYBIND, SDLK_RETURN);
				}
				/* Key repeat */
				else
				{
					check_menu_keys(MENU_KEYBIND, SDLK_r);
				}

				mb_clicked = 0;
			}
		}

		if (i != bindkey_list_set.entry_nr)
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

		if (bindkey_list[bindkey_list_set.group_nr].entry[i].text[0])
		{
			StringBlt(ScreenSurface, &SystemFont, bindkey_list[bindkey_list_set.group_nr].entry[i].text, x + X_COL1, y + TXT_Y_START, COLOR_WHITE, NULL, NULL);
			StringBlt(ScreenSurface, &SystemFont, bindkey_list[bindkey_list_set.group_nr].entry[i].keyname, x + X_COL2, y + TXT_Y_START, COLOR_WHITE, NULL, NULL);

			if (bindkey_list[bindkey_list_set.group_nr].entry[i].repeatflag)
			{
				StringBlt(ScreenSurface, &SystemFont, "on", x + X_COL3, y + TXT_Y_START, COLOR_WHITE, NULL, NULL);
			}
			else
			{
				StringBlt(ScreenSurface, &SystemFont, "off", x + X_COL3, y + TXT_Y_START, COLOR_WHITE, NULL, NULL);
			}
		}
	}

	y2 += 13 + TXT_Y_START + bindkey_list_set.entry_nr * 12;
	box.y = y2;

	if (keybind_status == KEYBIND_STATUS_EDIT)
	{
		box.w = X_COL2 - X_COL1;
		SDL_FillRect(ScreenSurface, &box, 0);
		StringBlt(ScreenSurface, &SystemFont, show_input_string(InputString, &SystemFont, box.w), x + X_COL1, y2, COLOR_WHITE, NULL, NULL);
	}
	else if (keybind_status == KEYBIND_STATUS_EDITKEY)
	{
		box.x += X_COL2 - X_COL1;
		box.w = X_COL3 - X_COL2;

		SDL_FillRect(ScreenSurface, &box, 0);
		StringBlt(ScreenSurface, &SystemFont, "Press a key!", x + X_COL2, y2, COLOR_WHITE, NULL, NULL);
	}

	if (!mb)
	{
		active_button = -1;
	}
}

/**
 * Blit a face.
 * @param id ID of the face.
 * @param x X position.
 * @param y Y position. */
static void blit_face(int id, int x, int y)
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

#define CREATE_Y0 120
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

/**
 * Show login window. */
void show_login_server()
{
	SDL_Rect box;
	char buf[MAX_BUF];
	int x, y, i;
	int mx, my, mb, t, progress;

	mb = SDL_GetMouseState(&mx, &my);

	/* Background */
	x = 25;
	y = Screensize->y / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->h / 2;
	sprite_blt(Bitmaps[BITMAP_DIALOG_BG], x, y, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_LOGO270], x + 20, y + 85, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_DIALOG_TITLE_LOGIN], x + 250 - Bitmaps[BITMAP_DIALOG_TITLE_LOGIN]->bitmap->w / 2, y + 17, NULL, NULL);

	t = x + 275;
	x += 170;
	y += 100;
	draw_frame(x - 3, y - 3, 211, 168);
	box.x = x - 2;
	box.y = y - 2;
	box.w = 210;
	box.h = 17;

	StringBlt(ScreenSurface, &SystemFont, "Server", t + 1 - 21, y - 35, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Server", t - 21 , y - 36, COLOR_WHITE, NULL, NULL);

	t -= get_string_pixel_length(selected_server->name, &BigFont) / 2;
	StringBlt(ScreenSurface, &BigFont, selected_server->name, t + 1, y - 21, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &BigFont, selected_server->name, t, y - 22, COLOR_HGOLD, NULL, NULL);

	SDL_FillRect(ScreenSurface, &box, sdl_gray3);
	box.y = y + 15;
	box.h = 150;
	SDL_FillRect(ScreenSurface, &box, sdl_gray4);

	StringBlt(ScreenSurface, &SystemFont, "- UPDATING FILES- ", x + 58, y + 1, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "- UPDATING FILES -", x + 57, y, COLOR_WHITE, NULL, NULL);

	if (request_file_chain >= 0)
	{
		StringBlt(ScreenSurface, &SystemFont, "Updating settings file from server...", x + 2, y + 20, COLOR_WHITE, NULL, NULL);
	}

	if (request_file_chain > 1)
	{
		StringBlt(ScreenSurface, &SystemFont, "Updating spells file from server...", x + 2, y + 32, COLOR_WHITE, NULL, NULL);
	}

	if (request_file_chain > 3)
	{
		StringBlt(ScreenSurface, &SystemFont, "Updating skills file from server...", x + 2, y + 44, COLOR_WHITE, NULL, NULL);
	}

	if (request_file_chain > 5)
	{
		StringBlt(ScreenSurface, &SystemFont, "Updating bmaps file from server...", x + 2, y + 56, COLOR_WHITE, NULL, NULL);
	}

	if (request_file_chain > 7)
	{
		StringBlt(ScreenSurface, &SystemFont, "Updating anims file from server...", x + 2, y + 68, COLOR_WHITE, NULL, NULL);
	}

	if (request_file_chain > 9)
	{
		StringBlt(ScreenSurface, &SystemFont, "Updating help files from server...", x + 2, y + 80, COLOR_WHITE, NULL, NULL);
	}

	if (request_file_chain > 11)
	{
		StringBlt(ScreenSurface, &SystemFont, "Sync files..." , x + 2, y + 92, COLOR_WHITE, NULL, NULL);
	}

	/* If set, we have requested something and the stuff in the socket buffer is our file! */
	if (request_file_chain == 1 || request_file_chain == 3 || request_file_chain ==  5 || request_file_chain == 7 || request_file_chain == 9 || request_file_chain == 11)
	{
		snprintf(buf, sizeof(buf), "received ~%d~ bytes", csocket.inbuf.len);
		StringBlt(ScreenSurface, &SystemFont, buf, x + 1, y + 150, COLOR_WHITE, NULL, NULL);
	}

	/* Update progress bar of requested files */
	sprite_blt(Bitmaps[BITMAP_PROGRESS_BACK], x, y + (168 - Bitmaps[BITMAP_PROGRESS_BACK]->bitmap->h - 5), NULL, NULL);

	progress = MIN(100, request_file_chain * 9);
	box.x = 0;
	box.y = 0;
	box.h = Bitmaps[BITMAP_PROGRESS]->bitmap->h;
	box.w = (int) ((float) Bitmaps[BITMAP_PROGRESS]->bitmap->w / 100 * progress);
	sprite_blt(Bitmaps[BITMAP_PROGRESS], x, y + (168 - Bitmaps[BITMAP_PROGRESS]->bitmap->h - 5), &box, NULL);

	/* Login user part */
	if (GameStatus == GAME_STATUS_REQUEST_FILES)
	{
		return;
	}

	StringBlt(ScreenSurface, &SystemFont, "done.", x + 2, y + 104, COLOR_WHITE, NULL, NULL);
	y += 180;

	StringBlt(ScreenSurface, &SystemFont, "Enter your name", x, y, COLOR_HGOLD, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_LOGIN_INP], x - 2, y + 15, NULL, NULL);

	if (GameStatus == GAME_STATUS_NAME)
	{
		strcpy(buf, show_input_string(InputString, &SystemFont, Bitmaps[BITMAP_LOGIN_INP]->bitmap->w - 16));
		buf[0] = toupper(buf[0]);
		StringBlt(ScreenSurface, &SystemFont, buf, x + 2, y + 17, COLOR_WHITE, NULL, NULL);
	}
	else
	{
		cpl.name[0] = toupper(cpl.name[0]);
		StringBlt(ScreenSurface, &SystemFont, cpl.name, x + 2, y + 17, COLOR_WHITE, NULL, NULL);
	}

	StringBlt(ScreenSurface, &SystemFont, "Enter your password", x + 2, y + 40, COLOR_HGOLD, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_LOGIN_INP], x - 2, y + 55, NULL, NULL);

	if (GameStatus == GAME_STATUS_PSWD)
	{
		strcpy(buf, show_input_string(InputString, &SystemFont, Bitmaps[BITMAP_LOGIN_INP]->bitmap->w - 16));

		for (i = 0; i < CurrentCursorPos; i++)
		{
			buf[i] = '*';
		}

		for (i = CurrentCursorPos + 1; i < (int) strlen(InputString) + 1; i++)
		{
			buf[i] = '*';
		}

		buf[i] = '\0';
		StringBlt(ScreenSurface, &SystemFont, buf, x + 2, y + 57, COLOR_WHITE, NULL, NULL);
	}
	else
	{
		for (i = 0; i < (int) strlen(cpl.password); i++)
		{
			buf[i] = '*';
		}

		buf[i] = '\0';
		StringBlt(ScreenSurface, &SystemFont, buf, x + 2, y + 57, COLOR_WHITE, NULL, NULL);
	}

	if (GameStatus == GAME_STATUS_VERIFYPSWD)
	{
		StringBlt(ScreenSurface, &SystemFont, "New Character: Verify Password", x + 2, y + 80, COLOR_HGOLD, NULL, NULL);
		sprite_blt(Bitmaps[BITMAP_LOGIN_INP], x - 2, y + 95, NULL, NULL);
		strcpy(buf, show_input_string(InputString, &SystemFont, Bitmaps[BITMAP_LOGIN_INP]->bitmap->w - 16));

		for (i = 0; i < (int) strlen(InputString); i++)
		{
			buf[i] = '*';
		}

		buf[i] = '\0';
		StringBlt(ScreenSurface, &SystemFont, buf, x + 2, y + 97, COLOR_WHITE, NULL, NULL);
	}

	y += 160;
	StringBlt(ScreenSurface, &SystemFont, "To start playing enter your character ~name~ and ~password~.", x - 10, y + 1, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "To start playing enter your character ~name~ and ~password~.", x - 11, y, COLOR_WHITE, NULL, NULL);

	y += 12;
	StringBlt(ScreenSurface, &SystemFont, "You will be asked to ~verify~ the password for new characters.", x - 10, y + 1, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "You will be asked to ~verify~ the password for new characters.", x - 11, y, COLOR_WHITE, NULL, NULL);
}

/**
 * Show metaserver window.
 * @param node First server.
 * @param metaserver_sel Selected server. */
void show_meta_server(server_struct *node, int metaserver_sel)
{
	int x, y, i;
	char buf[1024];
	SDL_Rect rec_name, rec_desc, box;
	int mx, my, mb;

	mb = SDL_GetMouseState(&mx, &my);

	/* Background */
	x = 25;
	y = Screensize->y / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->h / 2;

	sprite_blt(Bitmaps[BITMAP_DIALOG_BG], x, y, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_LOGO270], x + 20, y + 85, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_DIALOG_TITLE_SERVER], x + 250 - Bitmaps[BITMAP_DIALOG_TITLE_SERVER]->bitmap->w / 2, y + 15, NULL, NULL);

	rec_name.w = 272;
	rec_desc.w = 325;

	box.x = x + 133;
	box.y = y + TXT_Y_START + 1;
	box.w = 329;
	box.h = 12;

	/* Frame for selection field */
	draw_frame(box.x - 1, box.y + 11, box.w + 1, 313);

	/* Frame for scrollbar */
	draw_frame(box.x + box.w + 4, box.y + 11, 10, 313);

	/* Show scrollbar, and adjust its position by yoff */
	blt_window_slider(Bitmaps[BITMAP_SLIDER_LONG], metaserver_count - 18, 8, dialog_yoff, -1, box.x + box.w + 5, box.y + 12);

	if (metaserver_connecting)
	{
		StringBlt(ScreenSurface, &SystemFont, "Connecting to metaserver, please wait...", x + TXT_START_NAME + 81, y + TXT_Y_START - 13, COLOR_BLACK, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, "Connecting to metaserver, please wait...", x + TXT_START_NAME + 80, y + TXT_Y_START - 14, COLOR_HGOLD, NULL, NULL);
	}
	else
	{
		StringBlt(ScreenSurface, &SystemFont, "Select a server.", x + TXT_START_NAME + 126, y + TXT_Y_START - 13, COLOR_BLACK, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, "Select a server.", x + TXT_START_NAME + 125, y + TXT_Y_START - 14, COLOR_GREEN, NULL, NULL);
	}

	/* we should prepare for this the SystemFontOut */
	StringBlt(ScreenSurface, &SystemFont, "Servers", x + TXT_START_NAME + 1, y + TXT_Y_START - 1, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Servers", x + TXT_START_NAME, y + TXT_Y_START - 2, COLOR_WHITE, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Port", x + 380, y + TXT_Y_START - 1, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Port", x + 379, y + TXT_Y_START - 2, COLOR_WHITE, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Players", x + 416, y + TXT_Y_START - 1, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Players", x + 415, y + TXT_Y_START - 2, COLOR_WHITE, NULL, NULL);

	sprintf(buf, "Use cursors ~%c%c~ to select server                                  press ~RETURN~ to connect", ASCII_UP, ASCII_DOWN);
	StringBlt(ScreenSurface, &SystemFont, buf, x + 140, y + 410, COLOR_WHITE, NULL, NULL);

	for (i = 0; i < OPTWIN_MAX_OPT; i++)
	{
		box.y += 12;

		if (i & 1)
		{
			SDL_FillRect(ScreenSurface, &box, sdl_gray2);
		}
		else
		{
			SDL_FillRect(ScreenSurface, &box, sdl_gray1);
		}
	}

	for (i = 0; i < metaserver_count && node; i++, node = node->next)
	{
		if (i < dialog_yoff)
		{
			continue;
		}
		/* Never more than maximum */
		else if (i - dialog_yoff >= DIALOG_LIST_ENTRY)
		{
			break;
		}

		if (i == metaserver_sel)
		{
			int tmp_y = 0, width = 0;
			char tmpbuf[MAX_BUF], *cp;

			snprintf(buf, sizeof(buf), "version %s", node->version);

			StringBlt(ScreenSurface, &SystemFont, buf, x + 160, y + 433, COLOR_BLACK, NULL, NULL);
			StringBlt(ScreenSurface, &SystemFont, buf, x + 159, y + 432, COLOR_WHITE, NULL, NULL);

			strncpy(tmpbuf, node->desc, sizeof(tmpbuf));
			cp = strtok(tmpbuf, " ");

			/* Loop through spaces */
			while (cp)
			{
				int len = get_string_pixel_length(cp, &SystemFont) + SystemFont.c[' '].w + SystemFont.char_offset;

				/* Do we need to adjust for the next line? */
				if (width + len > MAX_MS_DESC_LINE)
				{
					width = 0;
					tmp_y += 12;

					/* We hit the max */
					if (tmp_y >= MAX_MS_DESC_Y)
					{
						break;
					}
				}

				StringBlt(ScreenSurface, &SystemFont, cp, x + 160 + width, y + 446 + tmp_y, COLOR_BLACK, &rec_desc, NULL);
				StringBlt(ScreenSurface, &SystemFont, cp, x + 159 + width, y + 445 + tmp_y, COLOR_HGOLD, &rec_desc, NULL);
				width += len;

				cp = strtok(NULL, " ");
			}

			box.y = y + TXT_Y_START + 13 + (i - dialog_yoff) * 12;
			SDL_FillRect(ScreenSurface, &box, sdl_blue1);
		}

		StringBlt(ScreenSurface, &SystemFont, node->name, x + 137, y + 94 + (i - dialog_yoff) * 12, COLOR_WHITE, &rec_name, NULL);

		sprintf(buf, "%d", node->port);
		StringBlt(ScreenSurface, &SystemFont, buf, x + 380, y + 94 + (i - dialog_yoff) * 12, COLOR_WHITE, NULL, NULL);

		if (node->player >= 0)
		{
			sprintf(buf, "%d", node->player);
		}
		else
		{
			strcpy(buf, "-");
		}

		StringBlt(ScreenSurface, &SystemFont, buf, x + 416, y + 94 + (i - dialog_yoff) * 12, COLOR_WHITE, NULL, NULL);
	}
}

/**
 * Called on mouse event in metaserver dialog.
 * @param e SDL event. */
void metaserver_mouse(SDL_Event *e)
{
	/* Mousewheel up/down */
	if (e->button.button == 4 || e->button.button == 5)
	{
		/* Scroll down... */
		if (e->button.button == 5)
		{
			if (metaserver_sel < metaserver_count - 1)
			{
				metaserver_sel++;
			}

			if (metaserver_sel >= DIALOG_LIST_ENTRY + dialog_yoff)
			{
				dialog_yoff++;
			}
		}
		/* .. or up */
		else
		{
			if (metaserver_sel)
			{
				metaserver_sel--;
			}

			if (dialog_yoff > metaserver_sel)
			{
				dialog_yoff--;
			}
		}

		/* Sanity checks for going out of bounds. */
		if (dialog_yoff < 0 || metaserver_count < DIALOG_LIST_ENTRY)
		{
			dialog_yoff = 0;
		}
		else if (dialog_yoff >= metaserver_count - DIALOG_LIST_ENTRY)
		{
			dialog_yoff = metaserver_count - DIALOG_LIST_ENTRY;
		}

		if (metaserver_sel < 0)
		{
			metaserver_sel = 0;
		}
		else if (metaserver_sel >= metaserver_count)
		{
			metaserver_sel = metaserver_count;
		}
	}
}
