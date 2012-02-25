/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Crossfire (Multiplayer game for X-windows).                 *
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
 * This file handles various player related functions. This includes
 * both things that operate on the player item, ::cpl structure, or
 * various commands that the player issues.
 *
 * This file does most of the handling of commands from the client to
 * server (see commands.c for server->client)
 *
 * Does most of the work for sending messages to the server */

#include <global.h>

/**
 * Player doll item positions.
 *
 * Used to determine where to put item sprites on the player doll. */
static int player_doll_positions[PLAYER_DOLL_MAX][2] =
{
	{22, 6},
	{22, 44},
	{22, 82},
	{22, 120},
	{22, 158},

	{62, 6},
	{62, 44},
	{62, 82},
	{62, 120},
	{62, 158},

	{102, 6},
	{102, 44},
	{102, 82},
	{102, 120},
	{102, 158}
};

/**
 * Gender nouns. */
const char *gender_noun[GENDER_MAX] =
{
	"neuter", "male", "female", "hermaphrodite"
};
/**
 * Subjective pronouns. */
const char *gender_subjective[GENDER_MAX] =
{
	"it", "he", "she", "it"
};
/**
 * Subjective pronouns, with first letter in uppercase. */
const char *gender_subjective_upper[GENDER_MAX] =
{
	"It", "He", "She", "It"
};
/**
 * Objective pronouns. */
const char *gender_objective[GENDER_MAX] =
{
	"it", "him", "her", "it"
};
/**
 * Possessive pronouns. */
const char *gender_possessive[GENDER_MAX] =
{
	"its", "his", "her", "its"
};
/**
 * Reflexive pronouns. */
const char *gender_reflexive[GENDER_MAX] =
{
	"itself", "himself", "herself", "itself"
};

/**
 * Clear the player data like quickslots, inventory items, etc. */
void clear_player(void)
{
	objects_deinit();
	memset(&cpl, 0, sizeof(cpl));
	objects_init();
	quickslots_init();
	init_player_data();
	skills_init();
	WIDGET_REDRAW_ALL(SKILL_EXP_ID);
}

/**
 * Initialize new player.
 * @param tag Tag of the player.
 * @param name Name of the player.
 * @param weight Weight of the player.
 * @param face Face ID. */
void new_player(tag_t tag, long weight, short face)
{
	cpl.ob->tag = tag;
	cpl.ob->weight = (float) weight / 1000;
	cpl.ob->face = face;
}

/**
 * Send apply command to server.
 * @param tag Item tag. */
void client_send_apply(tag_t tag)
{
	packet_struct *packet;

	packet = packet_new(SERVER_CMD_ITEM_APPLY, 8, 0);
	packet_append_uint32(packet, tag);
	socket_send_packet(packet);
}

/**
 * Send examine command to server.
 * @param tag Item tag. */
void client_send_examine(tag_t tag)
{
	packet_struct *packet;

	packet = packet_new(SERVER_CMD_ITEM_EXAMINE, 8, 0);
	packet_append_uint32(packet, tag);
	socket_send_packet(packet);
}

/**
 * Request nrof of objects of tag get moved to loc.
 * @param loc Location where to move the object.
 * @param tag Item tag.
 * @param nrof Number of objects from tag. */
void client_send_move(int loc, int tag, int nrof)
{
	packet_struct *packet;

	packet = packet_new(SERVER_CMD_ITEM_MOVE, 32, 0);
	packet_append_uint32(packet, loc);
	packet_append_uint32(packet, tag);
	packet_append_uint32(packet, nrof);
	socket_send_packet(packet);
}

/**
 * This should be used for all 'command' processing. Other functions
 * should call this so that proper windowing will be done.
 * @param command Text command.
 * @return 1 if command was sent, 0 otherwise. */
void send_command(const char *command)
{
	packet_struct *packet;

	packet = packet_new(SERVER_CMD_PLAYER_CMD, 256, 128);
	packet_append_string_terminated(packet, command);
	socket_send_packet(packet);
}

/**
 * Initialize player data. */
void init_player_data(void)
{
	new_player(0, 0, 0);

	cpl.inventory_focus = BELOW_INV_ID;

	cpl.container_tag = -996;

	cpl.stats.maxsp = 1;
	cpl.stats.maxhp = 1;

	cpl.stats.speed = 1;

	cpl.ob->nrof = 1;
	cpl.partyname[0] = cpl.partyjoin[0] = '\0';

	/* Avoid division by 0 errors */
	cpl.stats.maxsp = 1;
	cpl.stats.maxhp = 1;

	cpl.container_tag = -997;
}

/**
 * Mouse event on player data widget.
 * @param widget The widget object.
 * @param x Mouse X.
 * @param y Mouse Y. */
void widget_player_data_event(widgetdata *widget, int x, int y)
{
}

/**
 * Show player data widget with name, gender, title, etc.
 * @param widget The widget object. */
void widget_show_player_data(widgetdata *widget)
{
	SDL_Rect box;

	surface_show(ScreenSurface, widget->x1, widget->y1, NULL, TEXTURE_CLIENT("player_info_bg"));

	box.w = widget->wd - 12;
	box.h = 36;
	string_show(ScreenSurface, FONT_ARIAL10, cpl.ext_title, widget->x1 + 6, widget->y1 + 2, COLOR_HGOLD, TEXT_MARKUP | TEXT_WORD_WRAP, &box);
}

/**
 * Show player stats widget with stats, health, mana, etc.
 * @param widget The widget object. */
void widget_player_stats(widgetdata *widget)
{
	double temp;
	SDL_Rect box;
	int tmp;
	SDL_Surface *texture;

	if (!widget->widgetSF)
	{
		SDL_Surface *texture;

		texture = TEXTURE_CLIENT("main_stats");
		widget->widgetSF = SDL_ConvertSurface(texture, texture->format, texture->flags);
	}

	if (widget->redraw)
	{
		char buf[MAX_BUF];

		widget->redraw = 0;

		surface_show(widget->widgetSF, 0, 0, NULL, TEXTURE_CLIENT("main_stats"));

		/* Health */
		string_show(widget->widgetSF, FONT_ARIAL10, "HP", 58, 10, COLOR_WHITE, 0, NULL);
		snprintf(buf, sizeof(buf), "%d/%d", cpl.stats.hp, cpl.stats.maxhp);
		string_truncate_overflow(FONT_ARIAL10, buf, 90);
		string_show(widget->widgetSF, FONT_ARIAL10, buf, 160 - string_get_width(FONT_ARIAL10, buf, 0), 10, COLOR_GREEN, 0, NULL);

		/* Mana */
		string_show(widget->widgetSF, FONT_ARIAL10, "Mana", 58, 34, COLOR_WHITE, 0, NULL);
		snprintf(buf, sizeof(buf), "%d/%d", cpl.stats.sp, cpl.stats.maxsp);
		string_truncate_overflow(FONT_ARIAL10, buf, 75);
		string_show(widget->widgetSF, FONT_ARIAL10, buf, 160 - string_get_width(FONT_ARIAL10, buf, 0), 34, COLOR_GREEN, 0, NULL);

		/* Food */
		string_show(widget->widgetSF, FONT_ARIAL10, "Food", 58, 83, COLOR_WHITE, 0, NULL);
	}

	box.x = widget->x1;
	box.y = widget->y1;
	SDL_BlitSurface(widget->widgetSF, NULL, ScreenSurface, &box);

	/* Health bar */
	if (cpl.stats.maxhp)
	{
		texture = TEXTURE_CLIENT("hp");
		tmp = cpl.stats.hp;

		if (tmp < 0)
		{
			tmp = 0;
		}

		temp = (double) tmp / (double) cpl.stats.maxhp;
		box.x = 0;
		box.y = 0;
		box.h = texture->h;
		box.w = (int) (texture->w * temp);

		if (tmp && !box.w)
		{
			box.w = 1;
		}

		if (box.w > texture->w)
		{
			box.w = texture->w;
		}

		surface_show(ScreenSurface, widget->x1 + 57, widget->y1 + 23, NULL, TEXTURE_CLIENT("hp_back"));
		surface_show(ScreenSurface, widget->x1 + 57, widget->y1 + 23, &box, texture);
	}

	/* Mana bar */
	if (cpl.stats.maxsp)
	{
		texture = TEXTURE_CLIENT("sp");
		tmp = cpl.stats.sp;

		if (tmp < 0)
		{
			tmp = 0;
		}

		temp = (double) tmp / (double) cpl.stats.maxsp;
		box.x = 0;
		box.y = 0;
		box.h = texture->h;
		box.w = (int) (texture->w * temp);

		if (tmp && !box.w)
		{
			box.w = 1;
		}

		if (box.w > texture->w)
		{
			box.w = texture->w;
		}

		surface_show(ScreenSurface, widget->x1 + 57, widget->y1 + 47, NULL, TEXTURE_CLIENT("sp_back"));
		surface_show(ScreenSurface, widget->x1 + 57, widget->y1 + 47, &box, texture);
	}

	texture = TEXTURE_CLIENT("food");
	/* Food bar */
	tmp = cpl.stats.food;

	if (tmp < 0)
	{
		tmp = 0;
	}

	temp = (double) tmp / 1000;
	box.x = 0;
	box.y = 0;
	box.h = texture->h;
	box.w = (int) (texture->w * temp);

	if (tmp && !box.w)
	{
		box.w = 1;
	}

	if (box.w > texture->w)
	{
		box.w = texture->w;
	}

	surface_show(ScreenSurface, widget->x1 + 87, widget->y1 + 88, NULL, TEXTURE_CLIENT("food_back"));
	surface_show(ScreenSurface, widget->x1 + 87, widget->y1 + 88, &box, texture);
}

/**
 * Show skill groups widget.
 * @param widget The widget object. */
void widget_skillgroups(widgetdata *widget)
{
	SDL_Rect box;

	if (!widget->widgetSF)
	{
		SDL_Surface *texture;

		texture = TEXTURE_CLIENT("skill_lvl_bg");
		widget->widgetSF = SDL_ConvertSurface(texture, texture->format, texture->flags);
	}

	box.x = widget->x1;
	box.y = widget->y1;
	SDL_BlitSurface(widget->widgetSF, NULL, ScreenSurface, &box);
}

/**
 * Show player doll widget with applied items from inventory.
 * @param widget The widget object. */
void widget_show_player_doll(widgetdata *widget)
{
	char *tooltip_text;
	int i, xpos, ypos, mx, my;
	SDL_Surface *texture_slot_border;

	tooltip_text = NULL;

	surface_show(ScreenSurface, widget->x1, widget->y1, NULL, TEXTURE_CLIENT("player_doll_bg"));

	string_show(ScreenSurface, FONT_SANS12, "<b>Ranged</b>", widget->x1 + 20, widget->y1 + 188, COLOR_HGOLD, TEXT_MARKUP, NULL);
	string_show(ScreenSurface, FONT_ARIAL10, "DMG", widget->x1 + 9, widget->y1 + 205, COLOR_HGOLD, 0, NULL);
	string_show_format(ScreenSurface, FONT_MONO10, widget->x1 + 40, widget->y1 + 205, COLOR_WHITE, 0, NULL, "%02d", cpl.stats.ranged_dam);
	string_show(ScreenSurface, FONT_ARIAL10, "WC", widget->x1 + 10, widget->y1 + 215, COLOR_HGOLD, 0, NULL);
	string_show_format(ScreenSurface, FONT_MONO10, widget->x1 + 40, widget->y1 + 215, COLOR_WHITE, 0, NULL, "%02d", cpl.stats.ranged_wc);
	string_show(ScreenSurface, FONT_ARIAL10, "WS", widget->x1 + 10, widget->y1 + 225, COLOR_HGOLD, 0, NULL);
	string_show_format(ScreenSurface, FONT_MONO10, widget->x1 + 40, widget->y1 + 225, COLOR_WHITE, 0, NULL, "%3.2fs", cpl.stats.ranged_ws / 1000.0);

	string_show(ScreenSurface, FONT_SANS12, "<b>Melee</b>", widget->x1 + 155, widget->y1 + 188, COLOR_HGOLD, TEXT_MARKUP, NULL);
	string_show(ScreenSurface, FONT_ARIAL10, "DMG", widget->x1 + 139, widget->y1 + 205, COLOR_HGOLD, 0, NULL);
	string_show_format(ScreenSurface, FONT_MONO10, widget->x1 + 170, widget->y1 + 205, COLOR_WHITE, 0, NULL, "%02d", cpl.stats.dam);
	string_show(ScreenSurface, FONT_ARIAL10, "WC", widget->x1 + 140, widget->y1 + 215, COLOR_HGOLD, 0, NULL);
	string_show_format(ScreenSurface, FONT_MONO10, widget->x1 + 170, widget->y1 + 215, COLOR_WHITE, 0, NULL, "%02d", cpl.stats.wc);
	string_show(ScreenSurface, FONT_ARIAL10, "WS", widget->x1 + 140, widget->y1 + 225, COLOR_HGOLD, 0, NULL);
	string_show_format(ScreenSurface, FONT_MONO10, widget->x1 + 170, widget->y1 + 225, COLOR_WHITE, 0, NULL, "%3.2fs", cpl.stats.weapon_speed);

	string_show(ScreenSurface, FONT_ARIAL10, "Speed", widget->x1 + 92, widget->y1 + 193, COLOR_HGOLD, 0, NULL);
	string_show_format(ScreenSurface, FONT_MONO10, widget->x1 + 93, widget->y1 + 205, COLOR_WHITE, 0, NULL, "%3.2f", (float) cpl.stats.speed / FLOAT_MULTF);
	string_show(ScreenSurface, FONT_ARIAL10, "AC", widget->x1 + 92, widget->y1 + 215, COLOR_HGOLD, 0, NULL);
	string_show_format(ScreenSurface, FONT_MONO10, widget->x1 + 92, widget->y1 + 225, COLOR_WHITE, 0, NULL, "%02d", cpl.stats.ac);

	texture_slot_border = TEXTURE_CLIENT("player_doll_slot_border");

	for (i = 0; i < PLAYER_DOLL_MAX; i++)
	{
		rectangle_create(ScreenSurface, widget->x1 + player_doll_positions[i][0], widget->y1 + player_doll_positions[i][1], texture_slot_border->w, texture_slot_border->h, PLAYER_DOLL_SLOT_COLOR);
	}

	surface_show(ScreenSurface, widget->x1, widget->y1, NULL, TEXTURE_CLIENT("player_doll"));

	SDL_GetMouseState(&mx, &my);

	for (i = 0; i < PLAYER_DOLL_MAX; i++)
	{
		surface_show(ScreenSurface, widget->x1 + player_doll_positions[i][0], widget->y1 + player_doll_positions[i][1], NULL, texture_slot_border);

		if (!cpl.player_doll[i])
		{
			continue;
		}

		xpos = widget->x1 + player_doll_positions[i][0] + 2;
		ypos = widget->y1 + player_doll_positions[i][1] + 2;

		object_show_centered(cpl.player_doll[i], xpos, ypos);

		/* Prepare item name tooltip */
		if (mx > xpos && mx <= xpos + INVENTORY_ICON_SIZE && my > ypos && my <= widget->y1 + ypos + INVENTORY_ICON_SIZE)
		{
			tooltip_text = cpl.player_doll[i]->s_name;
		}
	}

	/* Draw item name tooltip */
	if (tooltip_text)
	{
		tooltip_create(mx, my, FONT_ARIAL10, tooltip_text);
	}
}

/**
 * Show main level widget.
 *
 * Contains the player's level, experience in highest skill, and percent
 * in bubbles to how close you are to next level.
 * @param widget The widget object. */
void widget_show_main_lvl(widgetdata *widget)
{
	SDL_Rect box;

	if (!widget->widgetSF)
	{
		SDL_Surface *texture;

		texture = TEXTURE_CLIENT("main_level_bg");
		widget->widgetSF = SDL_ConvertSurface(texture, texture->format, texture->flags);
	}

	if (widget->redraw)
	{
		char buf[MAX_BUF];

		widget->redraw = 0;

		surface_show(widget->widgetSF, 0, 0, NULL, TEXTURE_CLIENT("main_level_bg"));

		string_show(widget->widgetSF, FONT_ARIAL10, "Level / Exp", 5, 5, COLOR_HGOLD, TEXT_OUTLINE, NULL);

		snprintf(buf, sizeof(buf), "<b>%d</b>", cpl.stats.level);
		string_show(widget->widgetSF, FONT_SERIF14, buf, widget->wd - 4 - string_get_width(FONT_SERIF14, buf, TEXT_MARKUP), 4, cpl.stats.level == s_settings->max_level ? COLOR_HGOLD : COLOR_WHITE, TEXT_MARKUP, NULL);

		string_show_format(widget->widgetSF, FONT_ARIAL10, 5, 20, COLOR_WHITE, 0, NULL, "%"FMT64, cpl.stats.exp);

		player_draw_exp_progress(widget->widgetSF, 4, 35, cpl.stats.exp, cpl.stats.level);
	}

	box.x = widget->x1;
	box.y = widget->y1;
	SDL_BlitSurface(widget->widgetSF, NULL, ScreenSurface, &box);
}

/**
 * Show skill experience widget. This also includes the action timer.
 * @param widget The widget object. */
void widget_show_skill_exp(widgetdata *widget)
{
	SDL_Rect box;
	static uint32 action_tick = 0;

	/* Pre-emptively tick down the skill delay timer */
	if (cpl.action_timer > 0)
	{
		if (LastTick - action_tick > 125)
		{
			cpl.action_timer -= (float) (LastTick - action_tick) / 1000.0f;

			if (cpl.action_timer < 0)
			{
				cpl.action_timer = 0;
			}

			action_tick = LastTick;
			WIDGET_REDRAW(widget);
		}
	}
	else
	{
		action_tick = LastTick;
	}

	if (!widget->widgetSF)
	{
		SDL_Surface *texture;

		texture = TEXTURE_CLIENT("skill_exp_bg");
		widget->widgetSF = SDL_ConvertSurface(texture, texture->format, texture->flags);
	}

	if (widget->redraw)
	{
		widget->redraw = 0;

		surface_show(widget->widgetSF, 0, 0, NULL, TEXTURE_CLIENT("skill_exp_bg"));

		string_show(widget->widgetSF, FONT_ARIAL10, "Used", 4, 0, COLOR_HGOLD, TEXT_OUTLINE, NULL);
		string_show(widget->widgetSF, FONT_ARIAL10, "Skill", 5, 9, COLOR_HGOLD, TEXT_OUTLINE, NULL);

		string_show_format(widget->widgetSF, FONT_ARIAL10, 40, 0, COLOR_WHITE, 0, NULL, "%1.2f sec", cpl.action_timer);
	}

	box.x = widget->x1;
	box.y = widget->y1;
	SDL_BlitSurface(widget->widgetSF, NULL, ScreenSurface, &box);
}

/**
 * Show regeneration widget.
 * @param widget The widget object. */
void widget_show_regeneration(widgetdata *widget)
{
	SDL_Rect box;

	if (!widget->widgetSF)
	{
		SDL_Surface *texture;

		texture = TEXTURE_CLIENT("regen_bg");
		widget->widgetSF = SDL_ConvertSurface(texture, texture->format, texture->flags);
	}

	if (widget->redraw)
	{
		char buf[MAX_BUF];

		widget->redraw = 0;

		surface_show(widget->widgetSF, 0, 0, NULL, TEXTURE_CLIENT("regen_bg"));

		string_show(widget->widgetSF, FONT_SANS8, "R", 4, 1, COLOR_HGOLD, TEXT_OUTLINE, NULL);
		string_show(widget->widgetSF, FONT_SANS8, "e", 4, 7, COLOR_HGOLD, TEXT_OUTLINE, NULL);
		string_show(widget->widgetSF, FONT_SANS8, "g", 4, 13, COLOR_HGOLD, TEXT_OUTLINE, NULL);
		string_show(widget->widgetSF, FONT_SANS8, "e", 4, 21, COLOR_HGOLD, TEXT_OUTLINE, NULL);
		string_show(widget->widgetSF, FONT_SANS8, "n", 4, 27, COLOR_HGOLD, TEXT_OUTLINE, NULL);

		/* Health */
		string_show(widget->widgetSF, FONT_ARIAL10, "HP:", 13, 3, COLOR_HGOLD, 0, NULL);
		snprintf(buf, sizeof(buf), "%2.1f/s", cpl.gen_hp);
		string_truncate_overflow(FONT_ARIAL10, buf, 45);
		string_show(widget->widgetSF, FONT_ARIAL10, buf, widget->wd - 5 - string_get_width(FONT_ARIAL10, buf, 0), 3, COLOR_WHITE, 0, NULL);

		/* Mana */
		string_show(widget->widgetSF, FONT_ARIAL10, "Mana:", 13, 13, COLOR_HGOLD, 0, NULL);
		snprintf(buf, sizeof(buf), "%2.1f/s", cpl.gen_sp);
		string_truncate_overflow(FONT_ARIAL10, buf, 45);
		string_show(widget->widgetSF, FONT_ARIAL10, buf, widget->wd - 5 - string_get_width(FONT_ARIAL10, buf, 0), 13, COLOR_WHITE, 0, NULL);
	}

	box.x = widget->x1;
	box.y = widget->y1;
	SDL_BlitSurface(widget->widgetSF, NULL, ScreenSurface, &box);
}

/**
 * Show container widget.
 * @param widget The widget object. */
void widget_show_container(widgetdata *widget)
{
	SDL_Rect box;

	/* Special case, menuitem is highlighted when mouse is moved over it. */
	if (widget->WidgetSubtypeID == MENU_ID)
	{
		widget_highlight_menu(widget);
	}

	/* If we don't have a backbuffer, create it. */
	if (!widget->widgetSF)
	{
		widget->widgetSF = SDL_CreateRGBSurface(get_video_flags(), widget->wd, widget->ht, video_get_bpp(), 0, 0, 0, 0);
	}

	if (widget->redraw)
	{
		widget->redraw = 0;

		SDL_FillRect(widget->widgetSF, NULL, 0);
		box.x = 0;
		box.y = 0;
		box.w = widget->wd;
		box.h = widget->ht;
		border_create_color(widget->widgetSF, &box, "606060");
	}

	box.x = widget->x1;
	box.y = widget->y1;
	SDL_BlitSurface(widget->widgetSF, NULL, ScreenSurface, &box);
}

/* Handles highlighting of menuitems when the cursor is hovering over them. */
void widget_highlight_menu(widgetdata *widget)
{
	widgetdata *tmp, *tmp2, *tmp3;
	_menu *menu = NULL, *tmp_menu = NULL;
	_menuitem *menuitem = NULL, *submenuitem = NULL;
	int visible, create_submenu = 0, x = 0, y = 0;

	/* Sanity check. Make sure widget is a menu. */
	if (!widget || widget->WidgetSubtypeID != MENU_ID)
	{
		return;
	}

	/* Check to see if the cursor is hovering over a menuitem or a widget inside it.
	 * We don't need to go recursive here, just scan the immediate children. */
	for (tmp = widget->inv; tmp; tmp = tmp->next)
	{
		visible = 0;

		/* Menuitem is being directly hovered over. Make the background visible for visual feedback. */
		if (tmp == widget_mouse_event.owner)
		{
			visible = 1;
		}
		/* The cursor is hovering over something the menuitem contains. This only needs to search the direct children,
		 * as there should be nothing contained within the children. */
		else if (tmp->inv)
		{
			for (tmp2 = tmp->inv; tmp2; tmp2 = tmp2->next)
			{
				if (tmp2 == widget_mouse_event.owner)
				{
					/* The cursor was hovering over something inside the menuitem. */
					visible = 1;
					break;
				}
			}
		}

		/* Only do any real working if the state of the menuitem changed. */
		if (tmp->visible == visible)
		{
			continue;
		}

		menu = MENU(widget);
		menuitem = MENUITEM(tmp);

		/* Cursor has just started to hover over the menuitem. */
		if (visible)
		{
			tmp->visible = 1;

			/* If the highlighted menuitem is a submenu, we need to create a submenu next to the menuitem. */
			if (menuitem->menu_type == MENU_SUBMENU)
			{
				create_submenu = 1;
				submenuitem = menuitem;
				x = tmp->x1 + widget->wd - 4;
				y = tmp->y1 - (CONTAINER(widget))->outer_padding_top;
			}
		}
		/* Cursor no longer hovers over the menuitem. */
		else
		{
			tmp->visible = 0;

			/* Let's check if we need to remove the submenu.
			 * Don't remove it if the cursor is hovering over the submenu itself,
			 * or a submenu of the submenu, etc. */
			if (menuitem->menu_type == MENU_SUBMENU && menu->submenu)
			{
				/* This will for sure get the menu that the cursor is hovering over. */
				tmp2 = get_outermost_container(widget_mouse_event.owner);

				/* Just in case the 'for sure' part of the last comment turns out to be incorrect... */
				if (tmp2 && tmp2->WidgetSubtypeID == MENU_ID)
				{
					/* Loop through the submenus to see if we find a match for the menu the cursor is hovering over. */
					for (tmp_menu = menu; tmp_menu->submenu && tmp_menu->submenu != tmp2; tmp_menu = MENU(tmp_menu->submenu))
					{
					}

					/* Remove any submenus related to menu if the menu underneath the cursor is not a submenu of menu. */
					if (!tmp_menu->submenu)
					{
						tmp2 = menu->submenu;

						while (tmp2)
						{
							tmp3 = (MENU(tmp2))->submenu;
							remove_widget_object(tmp2);
							tmp2 = tmp3;
						}

						menu->submenu = NULL;
					}
					else
					{
						/* Cursor is hovering over the submenu, so leave this menuitem highlighted. */
						tmp->visible = 1;
					}
				}
				/* Cursor is not over a menu, so leave the menuitem containing the submenu highlighted.
				 * We want to keep the submenu open, which should reduce annoyance if the user is not precise with the mouse. */
				else
				{
					tmp->visible = 1;
				}
			}
		}
	}

	/* If a submenu needs to be created, create it now. Make sure there can be only one submenu open here. */
	if (create_submenu && !menu->submenu)
	{
		tmp_menu = MENU(widget);

		tmp_menu->submenu = create_menu(x, y, tmp_menu->owner);

		if (tmp_menu->owner->WidgetTypeID == MAIN_INV_ID)
		{
			if (submenuitem->menu_func_ptr == menu_inv_filter_submenu)
			{
				add_menuitem(tmp_menu->submenu, "All", &menu_inv_filter_all, MENU_CHECKBOX, inventory_filter == INVENTORY_FILTER_ALL);
				add_menuitem(tmp_menu->submenu, "Applied", &menu_inv_filter_applied, MENU_CHECKBOX, inventory_filter & INVENTORY_FILTER_APPLIED);
				add_menuitem(tmp_menu->submenu, "Unapplied", &menu_inv_filter_unapplied, MENU_CHECKBOX, inventory_filter & INVENTORY_FILTER_UNAPPLIED);
				add_menuitem(tmp_menu->submenu, "Containers", &menu_inv_filter_containers, MENU_CHECKBOX, inventory_filter & INVENTORY_FILTER_CONTAINER);
				add_menuitem(tmp_menu->submenu, "Magical", &menu_inv_filter_magical, MENU_CHECKBOX, inventory_filter & INVENTORY_FILTER_MAGICAL);
				add_menuitem(tmp_menu->submenu, "Cursed", &menu_inv_filter_cursed, MENU_CHECKBOX, inventory_filter & INVENTORY_FILTER_CURSED);
				add_menuitem(tmp_menu->submenu, "Unidentified", &menu_inv_filter_unidentified, MENU_CHECKBOX, inventory_filter & INVENTORY_FILTER_UNIDENTIFIED);
				add_menuitem(tmp_menu->submenu, "Locked", &menu_inv_filter_locked, MENU_CHECKBOX, inventory_filter & INVENTORY_FILTER_LOCKED);
			}
			else if (submenuitem->menu_func_ptr == menu_inventory_submenu_more)
			{
				add_menuitem(tmp_menu->submenu, "Drop all", &menu_inventory_dropall, MENU_NORMAL, 0);
				add_menuitem(tmp_menu->submenu, "Ready", &menu_inventory_ready, MENU_NORMAL, 0);
				add_menuitem(tmp_menu->submenu, "Mark", &menu_inventory_mark, MENU_NORMAL, 0);
				add_menuitem(tmp_menu->submenu, "Lock", &menu_inventory_lock, MENU_NORMAL, 0);
				add_menuitem(tmp_menu->submenu, "Drag", &menu_inventory_drag, MENU_NORMAL, 0);
			}
		}
		else
		{
			/* TODO: Remove this later. It's hardcoded here for testing. */
			submenu_chatwindow_filters(tmp_menu->submenu, 0, 0);
			add_menuitem(tmp_menu->submenu, "Test", &menu_detach_widget, MENU_SUBMENU, 0);
			add_menuitem(tmp_menu->submenu, "Test2", &menu_detach_widget, MENU_SUBMENU, 0);
		}

		menu_finalize(tmp_menu->submenu);
	}
}

/** Handles events when the menuitem is clicked on. */
void widget_menu_event(widgetdata *widget, int x, int y)
{
	widgetdata *tmp;
	_menuitem *menuitem;

	/* Bypass this code when unnecessary, such as when a menu doesn't exist. */
	if (!cur_widget[MENU_ID])
	{
		return;
	}

	tmp = widget;

	/* If the widget isn't a menuitem, the user probably clicked on a widget it contains.
	 * In that case, the parent is probably the menuitem, so grab that instead. */
	if (tmp->WidgetSubtypeID != MENUITEM_ID && tmp->env && tmp->env->WidgetSubtypeID == MENUITEM_ID)
	{
		tmp = tmp->env;
	}

	/* Make sure the menuitem is in a menu first as a sanity check. If so, we found the menu. */
	if (tmp->env && tmp->env->WidgetSubtypeID == MENU_ID)
	{
		menuitem = MENUITEM(tmp);
		/* Get the owner of the menu to do the operations on,
		 * (this is what the user would have right clicked on).
		 * Make it the mouse owner as this is what the user is interacting with. */
		widget_mouse_event.owner = (MENU(tmp->env))->owner;

		if (widget_mouse_event.owner)
		{
			widget_menuitem_event(widget_mouse_event.owner, x, y, menuitem->menu_func_ptr);
		}
	}
}

/** Call the function for the menuitem that was clicked on. */
void widget_menuitem_event(widgetdata *widget, int x, int y, void (*menu_func_ptr)(widgetdata *, int, int))
{
	/* sanity check */
	if (!menu_func_ptr)
	{
		return;
	}

	menu_func_ptr(widget, x, y);
}

void widget_show_label(widgetdata *widget)
{
	_widget_label *label = LABEL(widget);

	string_show(ScreenSurface, label->font, label->text, widget->x1, widget->y1, label->color, 0, NULL);
}

void widget_show_texture(widgetdata *widget)
{
	_widget_texture *texture = WIDGET_TEXTURE(widget);

	surface_show(ScreenSurface, widget->x1, widget->y1, NULL, TEXTURE_SURFACE(texture->texture));
}

/**
 * Transform gender-string into its @ref GENDER_xxx "ID".
 * @param gender The gender string.
 * @return The gender's ID as one of @ref GENDER_xxx, or -1 if 'gender'
 * didn't match any of the existing genders. */
int gender_to_id(const char *gender)
{
	size_t i;

	for (i = 0; i < GENDER_MAX; i++)
	{
		if (strcmp(gender_noun[i], gender) == 0)
		{
			return i;
		}
	}

	return -1;
}

void player_doll_update_items(void)
{
	object *tmp;
	int i, ring_num;

	memset(&cpl.player_doll, 0, sizeof(cpl.player_doll));

	ring_num = 0;

	for (tmp = cpl.ob->inv; tmp; tmp = tmp->next)
	{
		if (!(tmp->flags & CS_FLAG_APPLIED) && !(tmp->flags & CS_FLAG_IS_READY))
		{
			continue;
		}

		if (tmp->flags & CS_FLAG_IS_READY)
		{
			i = PLAYER_DOLL_AMMO;
		}
		else if (tmp->itype == TYPE_AMULET)
		{
			i = PLAYER_DOLL_AMULET;
		}
		else if (tmp->itype == TYPE_WEAPON)
		{
			i = PLAYER_DOLL_WEAPON;
		}
		else if (tmp->itype == TYPE_GLOVES)
		{
			i = PLAYER_DOLL_GAUNTLETS;
		}
		else if (tmp->itype == TYPE_RING && ring_num == 0)
		{
			i = PLAYER_DOLL_RING_RIGHT;
			ring_num++;
		}
		else if (tmp->itype == TYPE_HELMET)
		{
			i = PLAYER_DOLL_HELM;
		}
		else if (tmp->itype == TYPE_ARMOUR)
		{
			i = PLAYER_DOLL_ARMOUR;
		}
		else if (tmp->itype == TYPE_GIRDLE)
		{
			i = PLAYER_DOLL_BELT;
		}
		else if (tmp->itype == TYPE_GREAVES)
		{
			i = PLAYER_DOLL_GREAVES;
		}
		else if (tmp->itype == TYPE_BOOTS)
		{
			i = PLAYER_DOLL_BOOTS;
		}
		else if (tmp->itype == TYPE_CLOAK)
		{
			i = PLAYER_DOLL_CLOAK;
		}
		else if (tmp->itype == TYPE_BRACERS)
		{
			i = PLAYER_DOLL_BRACERS;
		}
		else if (tmp->itype == TYPE_SHIELD)
		{
			i = PLAYER_DOLL_SHIELD;
		}
		else if (tmp->itype == TYPE_LIGHT_APPLY)
		{
			i = PLAYER_DOLL_LIGHT;
		}
		else if (tmp->itype == TYPE_RING && ring_num == 1)
		{
			i = PLAYER_DOLL_RING_LEFT;
			ring_num++;
		}
		else
		{
			continue;
		}

		cpl.player_doll[i] = tmp;
	}
}

void player_draw_exp_progress(SDL_Surface *surface, int x, int y, sint64 exp, uint8 level)
{
	SDL_Surface *texture_bubble_on, *texture_bubble_off;
	int line_width, offset, i;
	double fractional, integral;
	SDL_Rect box;

	texture_bubble_on = TEXTURE_CLIENT("exp_bubble_on");
	texture_bubble_off = TEXTURE_CLIENT("exp_bubble_off");

	line_width = texture_bubble_on->w * EXP_PROGRESS_BUBBLES;
	offset = (double) texture_bubble_on->h / 2.0 + 0.5;
	fractional = modf(((double) (exp - s_settings->level_exp[level]) / (double) (s_settings->level_exp[level + 1] - s_settings->level_exp[level]) * EXP_PROGRESS_BUBBLES), &integral);

	filledRectAlpha(surface, x, y, x + line_width + offset * 2, y + texture_bubble_on->h + offset * 4, 150);

	for (i = 0; i < EXP_PROGRESS_BUBBLES; i++)
	{
		surface_show(surface, x + offset + i * texture_bubble_on->w, y + offset, NULL, i < (int) integral ? texture_bubble_on : texture_bubble_off);
	}

	box.x = x + offset;
	box.y = y + texture_bubble_on->h + offset * 2;
	box.w = line_width;
	box.h = offset;

	rectangle_create(surface, box.x, box.y, box.w, box.h, "404040");

	box.w = (double) box.w * fractional;
	rectangle_create(surface, box.x, box.y, box.w, box.h, "0000ff");

	box.y += offset / 4;
	box.h /= 2;
	rectangle_create(surface, box.x, box.y, box.w, box.h, "4040ff");
}
