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
 * Implements the interface used by NPCs and the like. */

#include <global.h>

/**
 * The current interface data. */
static interface_struct *interface_data = NULL;
/**
 * The interface popup. */
static popup_struct *interface_popup = NULL;
/**
 * Button buffers. */
static button_struct button_hello, button_close;

/**
 * Destroy the interface data, if any. */
static void interface_destroy(void)
{
	if (!interface_data)
	{
		return;
	}

	free(interface_data->message);
	free(interface_data->title);

	if (interface_data->icon)
	{
		free(interface_data->icon);
	}

	if (interface_data->text_input_prepend)
	{
		free(interface_data->text_input_prepend);
	}

	utarray_free(interface_data->links);
	free(interface_data);

	interface_data = NULL;
}

/** @copydoc text_anchor_handle_func */
static int text_anchor_handle(const char *anchor_action, const char *buf, size_t len)
{
	(void) len;

	if (anchor_action[0] == '\0' && buf[0] != '/')
	{
		if (!interface_data->progressed || SDL_GetTicks() >= interface_data->progressed_ticks)
		{
			StringBuffer *sb = stringbuffer_new();
			char *cp;

			stringbuffer_append_printf(sb, "/t_tell %s", buf);
			cp = stringbuffer_finish(sb);
			send_command_check(cp);
			free(cp);

			interface_data->progressed = 1;
			interface_data->progressed_ticks = SDL_GetTicks() + INTERFACE_PROGRESSED_TICKS;
		}

		return 1;
	}
	else if (!strcmp(anchor_action, "close"))
	{
		interface_data->destroy = 1;
		return 1;
	}

	return 0;
}

static void interface_execute_link(size_t link_id)
{
	char **p;
	text_blit_info info;

	p = (char **) utarray_eltptr(interface_data->links, link_id);

	if (!p)
	{
		return;
	}

	text_anchor_parse(&info, *p);
	text_set_anchor_handle(text_anchor_handle);
	text_anchor_execute(&info);
	text_set_anchor_handle(NULL);
}

/** @copydoc popup_struct::draw_func */
static int popup_draw_func(popup_struct *popup)
{
	if (popup->redraw)
	{
		_BLTFX bltfx;
		SDL_Rect box;

		bltfx.surface = popup->surface;
		bltfx.flags = 0;
		bltfx.alpha = 0;
		sprite_blt(Bitmaps[popup->bitmap_id], 0, 0, NULL, &bltfx);

		if (interface_data->icon)
		{
			string_blt_format(popup->surface, FONT_ARIAL10, INTERFACE_ICON_STARTX, INTERFACE_ICON_STARTY, COLOR_WHITE, TEXT_MARKUP, NULL, "<icon=%s %d %d>", interface_data->icon, INTERFACE_ICON_WIDTH, INTERFACE_ICON_HEIGHT);
		}

		text_offset_set(popup->x, popup->y);
		box.w = INTERFACE_TITLE_WIDTH;
		box.h = FONT_HEIGHT(FONT_SERIF14);
		string_blt(popup->surface, FONT_SERIF14, interface_data->title, INTERFACE_TITLE_STARTX, INTERFACE_TITLE_STARTY + INTERFACE_TITLE_HEIGHT / 2 - box.h / 2, COLOR_HGOLD, TEXT_MARKUP | TEXT_WORD_WRAP, &box);

		box.w = INTERFACE_TEXT_WIDTH;
		box.h = INTERFACE_TEXT_HEIGHT;
		box.x = 0;
		box.y = interface_data->scroll_offset;
		text_set_anchor_handle(text_anchor_handle);
		text_set_selection(&popup->selection_start, &popup->selection_end, &popup->selection_started);
		string_blt(popup->surface, interface_data->font, interface_data->message, INTERFACE_TEXT_STARTX, INTERFACE_TEXT_STARTY, COLOR_WHITE, TEXT_WORD_WRAP | TEXT_MARKUP | TEXT_LINES_SKIP, &box);
		text_set_selection(NULL, NULL, NULL);
		text_set_anchor_handle(NULL);
		text_offset_reset();

		popup->redraw = 0;
	}

	return !interface_data->destroy;
}

/** @copydoc popup_struct::draw_func_post */
static int popup_draw_func_post(popup_struct *popup)
{
	scrollbar_render(&interface_data->scrollbar, ScreenSurface, popup->x + 432, popup->y + 71);

	button_hello.x = popup->x + INTERFACE_BUTTON_HELLO_STARTX;
	button_hello.y = popup->y + INTERFACE_BUTTON_HELLO_STARTY;
	button_render(&button_hello, "Hello");

	button_close.x = popup->x + INTERFACE_BUTTON_CLOSE_STARTX;
	button_close.y = popup->y + INTERFACE_BUTTON_CLOSE_STARTY;
	button_render(&button_close, "Close");

	if (text_input_string_flag)
	{
		SDL_Rect dst;

		dst.x = popup->x + popup->surface->w / 2 - Bitmaps[BITMAP_LOGIN_INP]->bitmap->w / 2;
		dst.y = popup->y + popup->surface->h - Bitmaps[BITMAP_LOGIN_INP]->bitmap->h - 15;
		dst.w = Bitmaps[BITMAP_LOGIN_INP]->bitmap->w;
		dst.h = Bitmaps[BITMAP_LOGIN_INP]->bitmap->h;
		text_input_show(ScreenSurface, dst.x, dst.y, FONT_ARIAL11, text_input_string, COLOR_WHITE, 0, BITMAP_LOGIN_INP, NULL);
	}

	sprite_blt(Bitmaps[BITMAP_INTERFACE_BORDER], popup->x, popup->y, NULL, NULL);
	return 1;
}

/** @copydoc popup_struct::destroy_callback_func */
static int popup_destroy_callback(popup_struct *popup)
{
	(void) popup;
	interface_destroy();
	text_input_close();
	interface_popup = NULL;
	return 1;
}

/** @copydoc popup_button::event_func */
static int popup_button_event_func(popup_button *button)
{
	(void) button;
	help_show("npc interface");
	return 1;
}

/**
 * Handles clicking the 'hello' button. */
static void button_hello_event(void)
{
	if (!interface_data->progressed || SDL_GetTicks() >= interface_data->progressed_ticks)
	{
		keybind_process_command("?HELLO");
		interface_data->progressed = 1;
		interface_data->progressed_ticks = SDL_GetTicks() + INTERFACE_PROGRESSED_TICKS;
	}
}

/** @copydoc popup_struct::event_func */
static int popup_event_func(popup_struct *popup, SDL_Event *event)
{
	if (scrollbar_event(&interface_data->scrollbar, event))
	{
		return 1;
	}
	else if (button_event(&button_hello, event))
	{
		button_hello_event();
		return 1;
	}
	else if (button_event(&button_close, event))
	{
		popup_destroy(popup);
		return 1;
	}
	else if (event->type == SDL_KEYDOWN)
	{
		if (text_input_string_flag)
		{
			if (event->key.keysym.sym == SDLK_TAB && interface_data->allow_tab)
			{
				text_input_add_char('\t');
			}
			else if (event->key.keysym.sym == SDLK_RETURN || event->key.keysym.sym == SDLK_KP_ENTER || event->key.keysym.sym == SDLK_TAB)
			{
				char *input_string;

				input_string = strdup(text_input_string);

				if (!interface_data->input_cleanup_disable)
				{
					whitespace_squeeze(input_string);
					whitespace_trim(input_string);
				}

				if (input_string[0] != '\0' || interface_data->input_allow_empty)
				{
					StringBuffer *sb;
					char *cp;

					sb = stringbuffer_new();

					if (!interface_data->text_input_prepend || interface_data->text_input_prepend[0] != '/')
					{
						stringbuffer_append_string(sb, "/t_tell ");
					}

					if (interface_data->text_input_prepend)
					{
						stringbuffer_append_string(sb, interface_data->text_input_prepend);
					}

					stringbuffer_append_string(sb, input_string);

					cp = stringbuffer_finish(sb);
					send_command_check(cp);
					free(cp);
				}

				free(input_string);

				text_input_close();
				return 1;
			}
			else if (event->key.keysym.sym == SDLK_ESCAPE)
			{
				text_input_close();
				return 1;
			}
			else if (text_input_handle(&event->key))
			{
				return 1;
			}
		}

		switch (event->key.keysym.sym)
		{
			case SDLK_DOWN:
				scrollbar_scroll_adjust(&interface_data->scrollbar, 1);
				return 1;

			case SDLK_UP:
				scrollbar_scroll_adjust(&interface_data->scrollbar, -1);
				return 1;

			case SDLK_PAGEDOWN:
				scrollbar_scroll_adjust(&interface_data->scrollbar, interface_data->scrollbar.max_lines);
				return 1;

			case SDLK_PAGEUP:
				scrollbar_scroll_adjust(&interface_data->scrollbar, -interface_data->scrollbar.max_lines);
				return 1;

			case SDLK_1:
			case SDLK_2:
			case SDLK_3:
			case SDLK_4:
			case SDLK_5:
			case SDLK_6:
			case SDLK_7:
			case SDLK_8:
			case SDLK_9:
				if (!keys[event->key.keysym.sym].repeated)
				{
					interface_execute_link(event->key.keysym.sym - SDLK_1);
				}

				return 1;

			case SDLK_KP1:
			case SDLK_KP2:
			case SDLK_KP3:
			case SDLK_KP4:
			case SDLK_KP5:
			case SDLK_KP6:
			case SDLK_KP7:
			case SDLK_KP8:
			case SDLK_KP9:
				if (!keys[event->key.keysym.sym].repeated)
				{
					interface_execute_link(event->key.keysym.sym - SDLK_KP1);
				}

				return 1;

			case SDLK_RETURN:
			case SDLK_KP_ENTER:
				text_input_open(255);
				return 1;

			default:
				break;
		}

		if (keybind_command_matches_event("?HELLO", &event->key) && !keys[event->key.keysym.sym].repeated)
		{
			button_hello_event();
			return 1;
		}
	}
	else if (event->type == SDL_MOUSEBUTTONDOWN && event->motion.x >= popup->x && event->motion.x < popup->x + Bitmaps[popup->bitmap_id]->bitmap->w && event->motion.y >= popup->y && event->motion.y < popup->y + Bitmaps[popup->bitmap_id]->bitmap->h)
	{
		if (event->button.button == SDL_BUTTON_WHEELDOWN)
		{
			scrollbar_scroll_adjust(&interface_data->scrollbar, 1);
			return 1;
		}
		else if (event->button.button == SDL_BUTTON_WHEELUP)
		{
			scrollbar_scroll_adjust(&interface_data->scrollbar, -1);
			return 1;
		}
	}

	return -1;
}

/** @copydoc popup_struct::clipboard_copy_func */
static const char *popup_clipboard_copy_func(popup_struct *popup)
{
	(void) popup;
	return interface_data->message;
}

/**
 * Handle interface binary command.
 * @param data Data to parse.
 * @param len Length of 'data'. */
void cmd_interface(uint8 *data, int len)
{
	int pos = 0;
	uint8 text_input = 0, scroll_bottom = 0;
	char type, text_input_content[HUGE_BUF];
	StringBuffer *sb_message;
	size_t links_len, i;
	SDL_Rect box;

	if (!data || !len)
	{
		if (interface_data)
		{
			interface_data->destroy = 1;
		}

		return;
	}

	if (!interface_popup)
	{
		interface_popup = popup_create(BITMAP_INTERFACE);
		interface_popup->draw_func = popup_draw_func;
		interface_popup->draw_func_post = popup_draw_func_post;
		interface_popup->destroy_callback_func = popup_destroy_callback;
		interface_popup->event_func = popup_event_func;
		interface_popup->clipboard_copy_func = popup_clipboard_copy_func;
		interface_popup->disable_bitmap_blit = 1;

		interface_popup->button_left.event_func = popup_button_event_func;
		interface_popup->button_left.x = 380;
		interface_popup->button_left.y = 4;
		popup_button_set_text(&interface_popup->button_left, "?");

		interface_popup->button_right.x = 411;
		interface_popup->button_right.y = 4;
	}

	/* Make sure text input is not open. */
	text_input_close();

	/* Destroy previous interface data. */
	interface_destroy();

	/* Create new interface. */
	interface_data = calloc(1, sizeof(*interface_data));
	interface_popup->redraw = 1;
	interface_popup->selection_start = interface_popup->selection_end = -1;
	interface_data->font = FONT_ARIAL11;
	utarray_new(interface_data->links, &ut_str_icd);
	sb_message = stringbuffer_new();

	/* Parse the data. */
	while (pos < len)
	{
		type = data[pos++];

		switch (type)
		{
			case CMD_INTERFACE_TEXT:
			{
				char message[HUGE_BUF * 8];

				GetString_String(data, &pos, message, sizeof(message));
				stringbuffer_append_string(sb_message, message);
				break;
			}

			case CMD_INTERFACE_LINK:
			{
				char interface_link[HUGE_BUF], *cp;

				GetString_String(data, &pos, interface_link, sizeof(interface_link));
				cp = interface_link;
				utarray_push_back(interface_data->links, &cp);
				break;
			}

			case CMD_INTERFACE_ICON:
			{
				char icon[MAX_BUF];

				GetString_String(data, &pos, icon, sizeof(icon));
				interface_data->icon = strdup(icon);
				break;
			}

			case CMD_INTERFACE_TITLE:
			{
				char title[HUGE_BUF];

				GetString_String(data, &pos, title, sizeof(title));
				interface_data->title = strdup(title);
				break;
			}

			case CMD_INTERFACE_INPUT:
				text_input = 1;
				GetString_String(data, &pos, text_input_content, sizeof(text_input_content));
				break;

			case CMD_INTERFACE_INPUT_PREPEND:
			{
				char text_input_prepend[HUGE_BUF];

				GetString_String(data, &pos, text_input_prepend, sizeof(text_input_prepend));
				interface_data->text_input_prepend = strdup(text_input_prepend);
				break;
			}

			case CMD_INTERFACE_ALLOW_TAB:
				interface_data->allow_tab = 1;
				break;

			case CMD_INTERFACE_INPUT_CLEANUP_DISABLE:
				interface_data->input_cleanup_disable = 1;
				break;

			case CMD_INTERFACE_INPUT_ALLOW_EMPTY:
				interface_data->input_allow_empty = 1;
				break;

			case CMD_INTERFACE_SCROLL_BOTTOM:
				scroll_bottom = 1;
				break;

			default:
				break;
		}
	}

	if (text_input)
	{
		text_input_open(255);
		text_input_set_string(text_input_content);
	}

	links_len = utarray_len(interface_data->links);

	if (links_len)
	{
		stringbuffer_append_string(sb_message, "\n");
	}

	for (i = 0; i < links_len; i++)
	{
		stringbuffer_append_string(sb_message, "\n");

		if (i < 9)
		{
			stringbuffer_append_printf(sb_message, "<c=#AF7817>[%"FMT64U"]</c> ", (uint64) i + 1);
		}

		stringbuffer_append_string(sb_message, *((char **) utarray_eltptr(interface_data->links, i)));
	}

	interface_data->message = stringbuffer_finish(sb_message);

	box.w = INTERFACE_TEXT_WIDTH;
	box.h = INTERFACE_TEXT_HEIGHT;
	string_blt(NULL, interface_data->font, interface_data->message, INTERFACE_TEXT_STARTX, INTERFACE_TEXT_STARTY, COLOR_WHITE, TEXT_WORD_WRAP | TEXT_MARKUP | TEXT_LINES_CALC, &box);
	interface_data->num_lines = box.h;

	scrollbar_create(&interface_data->scrollbar, 11, 434, &interface_data->scroll_offset, &interface_data->num_lines, box.y);
	interface_data->scrollbar.redraw = &interface_popup->redraw;

	if (scroll_bottom)
	{
		interface_data->scroll_offset = interface_data->num_lines - box.y;
	}

	button_create(&button_hello);
	button_create(&button_close);

	button_hello.bitmap = button_close.bitmap = BITMAP_BUTTON_LARGE;
	button_hello.bitmap_over = button_close.bitmap_over = BITMAP_BUTTON_LARGE_HOVER;
	button_hello.bitmap_pressed = button_close.bitmap_pressed = BITMAP_BUTTON_LARGE_DOWN;
	button_hello.font = button_close.font = FONT_ARIAL13;
}

/**
 * Redraw the interface. */
void interface_redraw(void)
{
	if (interface_popup)
	{
		interface_popup->redraw = 1;
	}
}
