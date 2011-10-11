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
 * Text input API. */

#include <global.h>

/** Current text input string. */
char text_input_string[MAX_INPUT_STRING];
static char text_input_string_editing[MAX_INPUT_STRING];
/** Number of characters in the text input string. */
int text_input_count;
/** Cursor position in the text input string. */
static int text_input_cursor_pos = 0;
/** Maximum allowed characters in the text input string. */
static int text_input_max;

/** Text input history. */
static UT_array *text_input_history = NULL;
/** Stores console history. */
static UT_array *console_history = NULL;
/**
 * Position in the text input history -- used when browsing through the
 * history. */
static size_t text_input_history_pos = 0;

/** If 1, we have an open console. */
int text_input_string_flag;
/** If 1, we submitted some text using the console. */
int text_input_string_end_flag;
/** If 1, ESC was pressed while entering some text to console. */
int text_input_string_esc_flag;
/** When the console was opened. */
uint32 text_input_opened;

/**
 * Calculate X offset for centering a text input bitmap.
 * @return The offset. */
int text_input_center_offset(void)
{
	return Bitmaps[BITMAP_LOGIN_INP]->bitmap->w / 2;
}

/**
 * Draw text input's background (the bitmap).
 * @param surface Surface to draw on.
 * @param x X position.
 * @param y Y position.
 * @param bitmap Bitmap to use. */
void text_input_draw_background(SDL_Surface *surface, int x, int y, int bitmap)
{
	_BLTFX bltfx;

	bltfx.surface = surface;
	bltfx.flags = 0;
	bltfx.alpha = 0;
	sprite_blt(Bitmaps[bitmap], x, y, NULL, &bltfx);
}

/**
 * Draw text input's text.
 * @param surface Surface to draw on.
 * @param x X position.
 * @param y Y position.
 * @param font Font to use.
 * @param text Text to draw.
 * @param color_notation Color to use.
 * @param flags Text @ref TEXT_xxx "flags".
 * @param bitmap Bitmap to use.
 * @param box Contains coordinates to use and maximum string width. */
void text_input_draw_text(SDL_Surface *surface, int x, int y, int font, const char *text, const char *color_notation, uint64 flags, int bitmap, SDL_Rect *box)
{
	if (!box)
	{
		SDL_Rect box2;

		box2.x = 0;
		box2.y = 0;
		box2.w = Bitmaps[bitmap]->bitmap->w;
		box2.h = Bitmaps[bitmap]->bitmap->h;
		box = &box2;
	}

	x += 6 + box->x;
	y += box->h / 2 - FONT_HEIGHT(font) / 2 + box->y;

	box->w = box->w - 13 - box->x;
	box->h = FONT_HEIGHT(font);
	box->x = 0;
	box->y = 0;

	string_blt(surface, font, text, x, y, color_notation, flags | TEXT_WIDTH, box);
}

/**
 * Show text input.
 * @param surface Surface to use.
 * @param x X position.
 * @param y Y position.
 * @param font Font to use.
 * @param text Text to draw.
 * @param color_notation Color to use.
 * @param flags Text @ref TEXT_xxx "flags".
 * @param bitmap Bitmap to use.
 * @param box Contains coordinates to use and maximum string width. */
void text_input_show(SDL_Surface *surface, int x, int y, int font, const char *text, const char *color_notation, uint64 flags, int bitmap, SDL_Rect *box)
{
	char buf[HUGE_BUF];
	SDL_Rect box2;
	size_t pos = text_input_cursor_pos;
	const char *cp = text;
	int underscore_width = glyph_get_width(font, '_');
	text_blit_info info;

	blt_character_init(&info);

	box2.w = 0;

	/* Figure out the width by going backwards. */
	while (pos > 0)
	{
		/* Reached the maximum yet? */
		if (box2.w + glyph_get_width(font, *(cp + pos)) + underscore_width > Bitmaps[bitmap]->bitmap->w - 13 - (box ? box->x * 2 : 0))
		{
			break;
		}

		blt_character(&font, font, NULL, &box2, cp + pos, NULL, NULL, 0, NULL, NULL, &info);
		pos--;
	}

	/* Adjust the text position if necessary. */
	if (pos)
	{
		text += pos;
	}

	/* Draw the background. */
	text_input_draw_background(surface, x, y, bitmap);
	strncpy(buf, text, text_input_cursor_pos - pos);
	buf[text_input_cursor_pos - pos] = '_';
	buf[text_input_cursor_pos - pos + 1] = '\0';

	if (text + (text_input_cursor_pos - pos))
	{
		strcpy(buf + (text_input_cursor_pos - pos + 1), text + (text_input_cursor_pos - pos));
	}

	/* Draw the text. */
	text_input_draw_text(surface, x, y, font, buf, color_notation, flags, bitmap, box);
}

/**
 * Clear text input. */
void text_input_clear(void)
{
	text_input_string[0] = '\0';
	text_input_count = 0;
	text_input_history_pos = 0;
	text_input_history = NULL;
	text_input_cursor_pos = 0;
	text_input_string_flag = 0;
	text_input_string_end_flag = 0;
	text_input_string_esc_flag = 0;
}

/**
 * Open text input.
 * @param maxchar Maximum number of allowed characters. */
void text_input_open(int maxchar)
{
	int interval, delay;

	interval = 120 / (setting_get_int(OPT_CAT_CLIENT, OPT_KEY_REPEAT_SPEED) + 1);
	delay = interval + 300 / (setting_get_int(OPT_CAT_CLIENT, OPT_KEY_REPEAT_SPEED) + 1);

	text_input_clear();
	text_input_max = maxchar;
	SDL_EnableKeyRepeat(delay, interval);

	if (cpl.input_mode != INPUT_MODE_NUMBER)
	{
		cpl.inventory_focus = BELOW_INV_ID;
	}

	/* Raise the text/number input widget. */
	if (cpl.input_mode == INPUT_MODE_NUMBER)
	{
		SetPriorityWidget(cur_widget[IN_NUMBER_ID]);
	}
	else if (cpl.input_mode == INPUT_MODE_CONSOLE)
	{
		SetPriorityWidget(cur_widget[IN_CONSOLE_ID]);
		sound_play_effect("console.ogg", 100);
	}

	text_input_string_flag = 1;
	text_input_opened = SDL_GetTicks();
	text_input_history = NULL;

	if (cpl.input_mode == INPUT_MODE_CONSOLE)
	{
		if (!console_history)
		{
			utarray_new(console_history, &ut_str_icd);
		}

		text_input_set_history(console_history);
	}
}

/**
 * Close previously opened text input. */
void text_input_close(void)
{
	cpl.input_mode = INPUT_MODE_NO;
	SDL_EnableKeyRepeat(0, SDL_DEFAULT_REPEAT_INTERVAL);
	text_input_string_flag = 0;
	text_input_string_end_flag = 0;
	text_input_string_esc_flag = 0;
	keybind_state_ensure();
}

/**
 * Add string to the text input history.
 * @param text The text to add to the history. */
void text_input_history_add(const char *text)
{
	char **p;

	if (!text_input_history)
	{
		return;
	}

	p = (char **) utarray_back(text_input_history);

	if (p && !strcmp(*p, text))
	{
		return;
	}

	utarray_push_back(text_input_history, &text);

	if (utarray_len(text_input_history) > (size_t) setting_get_int(OPT_CAT_GENERAL, OPT_MAX_INPUT_HISTORY_LINES))
	{
		utarray_erase(text_input_history, 0, utarray_len(text_input_history) - (size_t) setting_get_int(OPT_CAT_GENERAL, OPT_MAX_INPUT_HISTORY_LINES));
	}
}

/**
 * Set the history array to use.
 * @param history The array to use. */
void text_input_set_history(UT_array *history)
{
	text_input_history = history;
	text_input_history_pos = 0;
}

/**
 * Put string to the text input.
 * @param text The string. */
void text_input_set_string(const char *text)
{
	/* Copy to input buffer. */
	strncpy(text_input_string, text, sizeof(text_input_string) - 1);
	text_input_string[sizeof(text_input_string) - 1] = '\0';
	/* Set cursor after inserted text. */
	text_input_cursor_pos = text_input_count = strlen(text);
}

/**
 * Add character to the text input.
 * @param c Character to add. */
void text_input_add_char(char c)
{
	int i;

	if (text_input_count >= text_input_max)
	{
		return;
	}

	i = text_input_count;

	while (i >= text_input_cursor_pos)
	{
		text_input_string[i + 1] = text_input_string[i];
		i--;
	}

	text_input_string[text_input_cursor_pos] = c;
	text_input_cursor_pos++;
	text_input_count++;
	text_input_string[text_input_count] = '\0';
}

/**
 * Skips whitespace and the first word in the input string.
 * @param[out] i Position to adjust.
 * @param left If 1, skip to the left, to the right otherwise. */
static void text_input_skip_word(int *i, int left)
{
	/* Skip whitespace. */
	while (text_input_string[*i] == ' ' && (left ? *i >= 0 : *i < text_input_count))
	{
		*i += left ? -1 : 1;
	}

	/* Skip a word. */
	while (text_input_string[*i] != ' ' && (left ? *i >= 0 : *i < text_input_count))
	{
		*i += left ? -1 : 1;
	}
}

/**
 * Handle text input keyboard event.
 * @param key The keyboard event.
 * @return 1 if the event was handled, 0 otherwise. */
int text_input_handle(SDL_KeyboardEvent *key)
{
	int i;

	if (key->type != SDL_KEYDOWN)
	{
		return 0;
	}

	if (keybind_command_matches_event("?PASTE", key))
	{
		char *clipboard_contents;

		clipboard_contents = clipboard_get();

		if (clipboard_contents)
		{
			strncat(text_input_string, clipboard_contents, sizeof(text_input_string) - text_input_count - 1);
			text_input_cursor_pos = text_input_count = strlen(text_input_string);

			for (i = 0; i < text_input_count; i++)
			{
				if (text_input_string[i] < ' ' || text_input_string[i] > '~')
				{
					text_input_string[i] = ' ';
				}
			}

			free(clipboard_contents);
		}

		return 1;
	}

	switch (key->keysym.sym)
	{
		case SDLK_ESCAPE:
			SDL_EnableKeyRepeat(0, SDL_DEFAULT_REPEAT_INTERVAL);
			text_input_string_esc_flag = 1;
			return 1;

		case SDLK_KP_ENTER:
		case SDLK_RETURN:
		case SDLK_TAB:
			if (key->keysym.sym != SDLK_TAB || GameStatus < GAME_STATUS_WAITFORPLAY)
			{
				SDL_EnableKeyRepeat(0 , SDL_DEFAULT_REPEAT_INTERVAL);
				text_input_string_flag = 0;
				/* Mark that we've got something here. */
				text_input_string_end_flag = 1;
				text_input_history_add(text_input_string);
				return 1;
			}
			else if (key->keysym.sym == SDLK_TAB)
			{
				help_handle_tabulator();
				return 1;
			}

			break;

		/* Erases the previous character or word if CTRL is pressed. */
		case SDLK_BACKSPACE:
			if (text_input_count && text_input_cursor_pos)
			{
				int ii = text_input_cursor_pos;

				/* Where we will end up, by default one character back. */
				i = ii - 1;

				if (key->keysym.mod & KMOD_CTRL)
				{
					text_input_skip_word(&i, 1);
					/* We end up at the beginning of the current word. */
					i++;
				}

				while (ii <= text_input_count)
				{
					text_input_string[i++] = text_input_string[ii++];
				}

				text_input_cursor_pos -= (ii - i);
				text_input_count -= (ii - i);
			}

			return 1;

		case SDLK_DELETE:
		{
			int ii = text_input_cursor_pos;

			/* Where we will end up, by default one character ahead. */
			i = ii + 1;

			if (ii == text_input_count)
			{
				return 1;
			}

			if (key->keysym.mod & KMOD_CTRL)
			{
				text_input_skip_word(&i, 0);
			}

			while (i <= text_input_count)
			{
				text_input_string[ii++] = text_input_string[i++];
			}

			text_input_count -= (i - ii);
			return 1;
		}

		/* Shifts a character or a word if CTRL is pressed. */
		case SDLK_LEFT:
			if (key->keysym.mod & KMOD_CTRL)
			{
				i = text_input_cursor_pos - 1;
				text_input_skip_word(&i, 1);
				/* Places the cursor on the first letter of this word. */
				text_input_cursor_pos = i + 1;
			}
			else if (text_input_cursor_pos > 0)
			{
				text_input_cursor_pos--;
			}

			return 1;

		/* Shifts a character or a word if CTRL is pressed. */
		case SDLK_RIGHT:
			if (key->keysym.mod & KMOD_CTRL)
			{
				i = text_input_cursor_pos;
				text_input_skip_word(&i, 0);
				/* Places the cursor right after the skipped word. */
				text_input_cursor_pos = i;
			}
			else if (text_input_cursor_pos < text_input_count)
			{
				text_input_cursor_pos++;
			}

			return 1;

		/* Scroll forward in history. */
		case SDLK_UP:
			if (text_input_history)
			{
				char **p;

				p = (char **) utarray_eltptr(text_input_history, utarray_len(text_input_history) - 1 - text_input_history_pos);

				if (p)
				{
					if (text_input_history_pos == 0)
					{
						strncpy(text_input_string_editing, text_input_string, sizeof(text_input_string_editing) - 1);
						text_input_string_editing[sizeof(text_input_string_editing) - 1] = '\0';
					}

					text_input_history_pos++;
					text_input_set_string(*p);
				}
			}

			return 1;

		/* Scroll backward in history. */
		case SDLK_DOWN:
			if (text_input_history)
			{
				if (text_input_history_pos > 0)
				{
					text_input_history_pos--;

					if (text_input_history_pos == 0)
					{
						text_input_set_string(text_input_string_editing);
						text_input_string_editing[0] = '\0';
					}
					else
					{
						char **p;

						p = (char **) utarray_eltptr(text_input_history, utarray_len(text_input_history) - text_input_history_pos);

						if (p)
						{
							text_input_set_string(*p);
						}
					}
				}
				else if (text_input_string[0] != '\0')
				{
					text_input_history_add(text_input_string);
					text_input_set_string("");
				}
			}

			return 1;

		/* Go to the start of the text input. */
		case SDLK_HOME:
			text_input_cursor_pos = 0;
			return 1;

		/* Go to the end of the text input. */
		case SDLK_END:
			text_input_cursor_pos = text_input_count;
			return 1;

		default:
		{
			char c;

			/* We want only numbers in number mode - even when shift is held. */
			if (cpl.input_mode == INPUT_MODE_NUMBER)
			{
				switch (key->keysym.sym)
				{
					case SDLK_0:
					case SDLK_KP0:
						c = '0';
						break;

					case SDLK_KP1:
					case SDLK_1:
						c = '1';
						break;

					case SDLK_KP2:
					case SDLK_2:
						c = '2';
						break;

					case SDLK_KP3:
					case SDLK_3:
						c = '3';
						break;

					case SDLK_KP4:
					case SDLK_4:
						c = '4';
						break;

					case SDLK_KP5:
					case SDLK_5:
						c = '5';
						break;

					case SDLK_KP6:
					case SDLK_6:
						c = '6';
						break;

					case SDLK_KP7:
					case SDLK_7:
						c = '7';
						break;

					case SDLK_KP8:
					case SDLK_8:
						c = '8';
						break;

					case SDLK_KP9:
					case SDLK_9:
						c = '9';
						break;

					default:
						c = '\0';
						break;
				}

				if (c)
				{
					text_input_add_char(c);
					return 1;
				}
			}
			else
			{
				c = key->keysym.unicode & 0xff;

				if (c >= 32)
				{
					if (key->keysym.mod & KMOD_SHIFT)
					{
						c = toupper(c);
					}

					text_input_add_char(c);
					return 1;
				}
			}

			break;
		}
	}

	return 0;
}
