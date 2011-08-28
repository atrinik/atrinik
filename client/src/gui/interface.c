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
static interface_struct *interface = NULL;

/**
 * Destroy the interface data, if any. */
static void interface_destroy()
{
	if (!interface)
	{
		return;
	}

	free(interface->message);
	free(interface->title);
	utarray_free(interface->links);
	free(interface);

	interface = NULL;
}

/** @copydoc text_anchor_handle_func */
static int text_anchor_handle(const char *anchor_action, const char *buf, size_t len)
{
	(void) len;

	if (anchor_action[0] == '\0' && buf[0] != '/')
	{
		StringBuffer *sb = stringbuffer_new();
		char *cp;

		stringbuffer_append_printf(sb, "/t_tell %s", buf);
		cp = stringbuffer_finish(sb);
		send_command_check(cp);
		free(cp);

		return 1;
	}
	else if (!strcmp(anchor_action, "close"))
	{
		interface->destroy = 1;
	}

	return 0;
}

static void interface_execute_link(size_t link_id)
{
	char **p;
	text_blit_info info;

	p = (char **) utarray_eltptr(interface->links, link_id);

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
	if (interface->redraw)
	{
		_BLTFX bltfx;
		SDL_Rect box;

		bltfx.surface = popup->surface;
		bltfx.flags = 0;
		bltfx.alpha = 0;
		sprite_blt(Bitmaps[popup->bitmap_id], 0, 0, NULL, &bltfx);

		if (interface->icon != -1 && FaceList[interface->icon].sprite)
		{
			int icon_w, icon_h, icon_orig_w, icon_orig_h;
			_Sprite *icon_sprite;
			SDL_Rect icon_box, icon_dst;
			double zoom_factor;

			icon_sprite = FaceList[interface->icon].sprite;
			icon_w = icon_orig_w = icon_sprite->bitmap->w - icon_sprite->border_left - icon_sprite->border_right;
			icon_h = icon_orig_h = icon_sprite->bitmap->h - icon_sprite->border_up - icon_sprite->border_down;

			if (icon_w > INTERFACE_ICON_WIDTH)
			{
				zoom_factor = (double) INTERFACE_ICON_WIDTH / icon_w;
				icon_w *= zoom_factor;
				icon_h *= zoom_factor;
			}

			if (icon_h > INTERFACE_ICON_HEIGHT)
			{
				zoom_factor = (double) INTERFACE_ICON_HEIGHT / icon_h;
				icon_w *= zoom_factor;
				icon_h *= zoom_factor;
			}

			icon_box.x = icon_sprite->border_left;
			icon_box.y = icon_sprite->border_up;
			icon_box.w = icon_w;
			icon_box.h = icon_h;

			icon_dst.x = INTERFACE_ICON_STARTX + INTERFACE_ICON_WIDTH / 2 - icon_w / 2;
			icon_dst.y = INTERFACE_ICON_STARTY + INTERFACE_ICON_HEIGHT / 2 - icon_h / 2;

			if (icon_w != icon_orig_w || icon_h != icon_orig_h)
			{
				SDL_Surface *tmp_icon;
				double zoom_x, zoom_y;

				zoom_x = (double) icon_w / icon_orig_w;
				zoom_y = (double) icon_h / icon_orig_h;

				tmp_icon = zoomSurface(icon_sprite->bitmap, zoom_x, zoom_y, setting_get_int(OPT_CAT_CLIENT, OPT_ZOOM_SMOOTH));

				icon_box.x *= zoom_x;
				icon_box.y *= zoom_y;

				SDL_BlitSurface(tmp_icon, &icon_box, popup->surface, &icon_dst);
				SDL_FreeSurface(tmp_icon);
			}
			else
			{
				SDL_BlitSurface(icon_sprite->bitmap, &icon_box, popup->surface, &icon_dst);
			}
		}

		text_offset_set(popup->x, popup->y);
		box.w = 350;
		box.h = FONT_HEIGHT(FONT_SERIF14);
		string_blt(popup->surface, FONT_SERIF14, interface->title, 80, 38 + 22 / 2 - box.h / 2, COLOR_HGOLD, TEXT_MARKUP | TEXT_WORD_WRAP, &box);

		box.w = INTERFACE_TEXT_WIDTH;
		box.h = INTERFACE_TEXT_HEIGHT;
		box.x = 0;
		box.y = interface->scroll_offset;
		text_set_anchor_handle(text_anchor_handle);
		string_blt(popup->surface, interface->font, interface->message, INTERFACE_TEXT_STARTX, INTERFACE_TEXT_STARTY, COLOR_WHITE, TEXT_WORD_WRAP | TEXT_MARKUP | TEXT_LINES_SKIP, &box);
		text_set_anchor_handle(NULL);
		text_offset_reset();

		interface->redraw = 0;
	}

	return !interface->destroy;
}

/** @copydoc popup_struct::draw_func_post */
static int popup_draw_func_post(popup_struct *popup)
{
	scrollbar_render(&interface->scrollbar, ScreenSurface, popup->x + 432, popup->y + 71);
	sprite_blt(Bitmaps[BITMAP_INTERFACE_BORDER], popup->x, popup->y, NULL, NULL);
	return 1;
}

/** @copydoc popup_struct::destroy_callback_func */
static int popup_destroy_callback(popup_struct *popup)
{
	(void) popup;
	interface_destroy();
	return 1;
}

/** @copydoc popup_struct::event_func */
static int popup_event_func(popup_struct *popup, SDL_Event *event)
{
	if (scrollbar_event(&interface->scrollbar, event))
	{
		return 1;
	}
	else if (event->type == SDL_KEYDOWN)
	{
		switch (event->key.keysym.sym)
		{
			case SDLK_DOWN:
				scrollbar_scroll_adjust(&interface->scrollbar, 1);
				return 1;

			case SDLK_UP:
				scrollbar_scroll_adjust(&interface->scrollbar, -1);
				return 1;

			case SDLK_PAGEDOWN:
				scrollbar_scroll_adjust(&interface->scrollbar, interface->scrollbar.max_lines);
				return 1;

			case SDLK_PAGEUP:
				scrollbar_scroll_adjust(&interface->scrollbar, -interface->scrollbar.max_lines);
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
				interface_execute_link(event->key.keysym.sym - SDLK_1);
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
				interface_execute_link(event->key.keysym.sym - SDLK_KP1);
				return 1;

			default:
				break;
		}
	}
	else if (event->type == SDL_MOUSEBUTTONDOWN && event->motion.x >= popup->x && event->motion.x < popup->x + Bitmaps[popup->bitmap_id]->bitmap->w && event->motion.y >= popup->y && event->motion.y < popup->y + Bitmaps[popup->bitmap_id]->bitmap->h)
	{
		if (event->button.button == SDL_BUTTON_WHEELDOWN)
		{
			scrollbar_scroll_adjust(&interface->scrollbar, 1);
		}
		else if (event->button.button == SDL_BUTTON_WHEELUP)
		{
			scrollbar_scroll_adjust(&interface->scrollbar, -1);
		}
		else if (event->button.button == SDL_BUTTON_LEFT)
		{
			interface->redraw = 1;
		}

		return 1;
	}

	return -1;
}

/**
 * Handle interface binary command.
 * @param data Data to parse.
 * @param len Length of 'data'. */
void cmd_interface(uint8 *data, int len)
{
	int pos = 0;
	uint8 text_input = 0;
	char type, text_input_content[HUGE_BUF];
	StringBuffer *sb_message;
	size_t links_len, i;
	SDL_Rect box;

	if (!interface)
	{
		popup_struct *popup;

		popup = popup_create(BITMAP_INTERFACE);
		popup->draw_func = popup_draw_func;
		popup->draw_func_post = popup_draw_func_post;
		popup->destroy_callback_func = popup_destroy_callback;
		popup->event_func = popup_event_func;
		popup->disable_bitmap_blit = 1;
		popup->close_button_xoff = 10;
		popup->close_button_yoff = 9;
	}

	/* Destroy previous interface data. */
	interface_destroy();

	/* Create new interface. */
	interface = calloc(1, sizeof(*interface));
	interface->redraw = 1;
	interface->font = FONT_ARIAL11;
	utarray_new(interface->links, &ut_str_icd);
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
				utarray_push_back(interface->links, &cp);
				break;
			}

			case CMD_INTERFACE_ICON:
			{
				char icon[MAX_BUF];

				GetString_String(data, &pos, icon, sizeof(icon));
				interface->icon = get_bmap_id(icon);
				break;
			}

			case CMD_INTERFACE_TITLE:
			{
				char title[HUGE_BUF];

				GetString_String(data, &pos, title, sizeof(title));
				interface->title = strdup(title);
				break;
			}

			case CMD_INTERFACE_INPUT:
				text_input = 1;
				GetString_String(data, &pos, text_input_content, sizeof(text_input_content));
				break;

			default:
				break;
		}
	}

	if (text_input)
	{
	}

	links_len = utarray_len(interface->links);

	if (links_len)
	{
		stringbuffer_append_string(sb_message, "\n");
	}

	for (i = 0; i < links_len; i++)
	{
		stringbuffer_append_string(sb_message, "\n");

		if (links_len < 9)
		{
			stringbuffer_append_printf(sb_message, "<c=#AF7817>[%"FMT64U"]</c> ", (uint64) i + 1);
		}

		stringbuffer_append_string(sb_message, *((char **) utarray_eltptr(interface->links, i)));
	}

	interface->message = stringbuffer_finish(sb_message);

	box.w = INTERFACE_TEXT_WIDTH;
	box.h = INTERFACE_TEXT_HEIGHT;
	string_blt(NULL, interface->font, interface->message, INTERFACE_TEXT_STARTX, INTERFACE_TEXT_STARTY, COLOR_WHITE, TEXT_WORD_WRAP | TEXT_MARKUP | TEXT_LINES_CALC, &box);
	interface->num_lines = box.h;

	scrollbar_create(&interface->scrollbar, 11, 434, &interface->scroll_offset, &interface->num_lines, box.y);
	interface->scrollbar.redraw = &interface->redraw;
}

void interface_redraw()
{
	if (interface)
	{
		interface->redraw = 1;
	}
}
