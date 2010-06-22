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
 * This file handles various player related functions. This includes
 * both things that operate on the player item, ::cpl structure, or
 * various commands that the player issues.
 *
 * This file does most of the handling of commands from the client to
 * server (see commands.c for server->client)
 *
 * Does most of the work for sending messages to the server */

#include <include.h>
#include <math.h>

/** Widget container alpha background. */
SDL_Surface     *containerbg = NULL;
int             old_container_alpha = -1;

/** Server level structure. */
_server_level server_level;

/**
 * Player doll item positions.
 *
 * Used to determine where to put item sprites on the player doll. */
static _player_doll_pos player_doll[PDOLL_INIT] =
{
	{93,	55},
	{93,	8},
	{93,	100},
	{93,	158},
	{135,	95},
	{50,	95},
	{50,	134},
	{135,	134},
	{54,	51},
	{141,	10},
	{5,		148},
	{180,	148},
	{5,		108},
	{180,	108},
	{43,	10},
	{4,		10}
};

/** Weapon speed table. */
static float weapon_speed_table[19] =
{
	20.0f, 	18.0f, 	10.0f, 	8.0f, 	5.5f,
	4.25f, 	3.50f, 	3.05f, 	2.70f, 	2.35f,
	2.15f,	1.95f,	1.80f, 	1.60f, 	1.52f,
	1.44f, 	1.32f, 	1.25f, 	1.20f
};

/**
 * Clear the player data like quickslots, inventory items, etc. */
void clear_player()
{
	memset(quick_slots, -1, sizeof(quick_slots));

	free_all_items(cpl.sack);
	free_all_items(cpl.below);
	free_all_items(cpl.ob);
	free_all_items(cpl.shop);
	cpl.ob = player_item();
	init_player_data();
}

/**
 * Initialize new player.
 * @param tag Tag of the player.
 * @param name Name of the player.
 * @param weight Weight of the player.
 * @param face Face ID. */
void new_player(long tag, char *name, long weight, short face)
{
	cpl.ob->tag = tag;
	cpl.ob->weight = (float) weight / 1000;
	cpl.ob->face = face;
	copy_name(cpl.ob->d_name, name);
}

/**
 * Request to set up a new character with the selected statistics.
 * @param nc The server char. */
void new_char(_server_char *nc)
{
	char buf[MAX_BUF];

	snprintf(buf, sizeof(buf), "nc %s %d %d %d %d %d %d %d", nc->char_arch[nc->gender_selected], nc->stats[0], nc->stats[1], nc->stats[2], nc->stats[3], nc->stats[4], nc->stats[5], nc->stats[6]);
	cs_write_string(csocket.fd, buf, strlen(buf));
}

/**
 * Send apply command to server.
 * @param tag Item tag. */
void client_send_apply(int tag)
{
	char buf[MAX_BUF];

	snprintf(buf, sizeof(buf), "ap %d", tag);
	cs_write_string(csocket.fd, buf, strlen(buf));
}

/**
 * Send examine command to server.
 * @param tag Item tag. */
void client_send_examine(int tag)
{
	char buf[MAX_BUF];

	snprintf(buf, sizeof(buf), "ex %d", tag);
	cs_write_string(csocket.fd, buf, strlen(buf));
}

/**
 * Request nrof of objects of tag get moved to loc.
 * @param loc Location where to move the object.
 * @param tag Item tag.
 * @param nrof Number of objects from tag. */
void client_send_move(int loc, int tag, int nrof)
{
	char buf[MAX_BUF];

	snprintf(buf, sizeof(buf), "mv %d %d %d", loc, tag, nrof);
	cs_write_string(csocket.fd, buf, strlen(buf));
}

/**
 * This should be used for all 'command' processing. Other functions
 * should call this so that proper windowing will be done.
 * @param command Text command.
 * @param repeat Count value or -1 if none desired and we don't want
 * to reset the current count.
 * @param must_send Means we must send this command no matter what (ie,
 * it is an administrative type of command like fire_stop, and failure to
 * send it will cause definite problems).
 * @return 1 if command was sent, 0 otherwise. */
void send_command(const char *command, int repeat, int must_send)
{
	char buf[MAX_BUF];
	static char last_command[MAX_BUF] = "";
	SockList sl;
	int commdiff = csocket.command_sent - csocket.command_received;

	if (commdiff < 0)
	{
		commdiff += 256;
	}

	/* Don't want to copy in administrative commands */
	if (!must_send)
	{
		strncpy(last_command, command, sizeof(last_command) - 1);
	}

	csocket.command_sent++;
	/* Max out at 255 */
	csocket.command_sent &= 0xff;

	sl.buf = (unsigned char *) buf;
	strcpy((char *) sl.buf, "cm ");
	sl.len = 3;
	SockList_AddShort(&sl, (uint16) csocket.command_sent);
	SockList_AddInt(&sl, repeat);
	strncpy((char *) sl.buf + sl.len, command, MAX_BUF - sl.len);
	sl.buf[MAX_BUF - 1] = '\0';
	sl.len += (int) strlen(command);
	send_socklist(csocket.fd, sl);

	if (repeat != -1)
	{
		cpl.count = 0;
	}
}

/**
 * Receive complete command.
 * @param data The incoming data.
 * @param len Length of the data. */
void CompleteCmd(unsigned char *data, int len)
{
	if (len != 6)
	{
		LOG(llevError, "comc - invalid length %d - ignoring\n", len);
	}

	csocket.command_received = GetShort_String(data);
	csocket.command_time = GetInt_String(data + 2);
}

/**
 * Set the player's weight limit.
 * @param wlim The weight limit to set. */
void set_weight_limit(uint32 wlim)
{
	cpl.weight_limit = wlim;
}

/**
 * Initialize player data. */
void init_player_data()
{
	new_player(0, "", 0,0);

	cpl.dm = 0;
	cpl.fire_on = cpl.firekey_on = 0;
	cpl.run_on = cpl.runkey_on = 0;
	cpl.inventory_win = IWIN_BELOW;

	cpl.container_tag = -996;
	cpl.container = NULL;

	memset(&cpl.stats, 0, sizeof(Stats));

	cpl.stats.maxsp = 1;
	cpl.stats.maxhp = 1;
	cpl.gen_hp = 0.0f;
	cpl.gen_sp = 0.0f;
	cpl.gen_grace = 0.0f;
	cpl.target_hp = 0;

	cpl.stats.maxgrace = 1;
	cpl.stats.speed = 1;

	cpl.title[0] = '\0';
	cpl.alignment[0] = '\0';
	cpl.gender[0] = '\0';
	cpl.range[0] = '\0';

	cpl.ob->nrof = 1;

	/* This is set from title in stat cmd */
	strcpy(cpl.pname, "");
	strcpy(cpl.title, "");
	strcpy(cpl.partyname, "");

	cpl.menustatus = MENU_NO;
	cpl.menustatus = MENU_NO;

	/* Avoid division by 0 errors */
	cpl.stats.maxsp = 1;
	cpl.stats.maxhp = 1;
	cpl.stats.maxgrace = 1;

	/* Displayed weapon speed is weapon speed/speed */
	cpl.stats.speed = 0;
	cpl.stats.weapon_sp = 0;
	cpl.action_timer = 0.0f;
	cpl.last_command[0] = '\0';

	cpl.container_tag = -997;
	cpl.container = NULL;

	RangeFireMode = 0;

	/* Clear the shop structure and hide it */
	clear_shop(0);
}

/**
 * Mouse event on player data widget.
 * @param widget The widget object.
 * @param x Mouse X.
 * @param y Mouse Y. */
void widget_player_data_event(widgetdata *widget, int x, int y)
{
	int mx = x - widget->x1, my = y - widget->y1;

	if (mx >= 184 && mx <= 210 && my >= 5 && my <= 35)
	{
		if (!client_command_check("/pray"))
		{
			send_command("/pray", -1, SC_NORMAL);
		}
	}
}

/**
 * Show player data widget with name, gender, title, etc.
 * @param widget The widget object. */
void widget_show_player_data(widgetdata *widget)
{
	char buf[256];

	sprite_blt(Bitmaps[BITMAP_PLAYER_INFO], widget->x1, widget->y1, NULL, NULL);

	if (cpl.rank[0] != '\0')
	{
		snprintf(buf, sizeof(buf), "%s %s", cpl.rank, cpl.pname);
	}
	else
	{
		strncpy(buf, cpl.pname, sizeof(buf) - 1);
	}

	StringBlt(ScreenSurface, &SystemFont, buf, widget->x1 + 6, widget->y1 + 2, COLOR_HGOLD, NULL, NULL);

	snprintf(buf, sizeof(buf), "%s %s %s", cpl.gender, cpl.race, cpl.title);
	StringBlt(ScreenSurface, &SystemFont, buf, widget->x1 + 6, widget->y1 + 14, COLOR_HGOLD, NULL, NULL);

	if (strcmp(cpl.godname, "none"))
	{
		snprintf(buf, sizeof(buf), "%s follower of %s", cpl.alignment, cpl.godname);
	}
	else
	{
		strncpy(buf, cpl.alignment, sizeof(buf) - 1);
	}

	StringBlt(ScreenSurface, &SystemFont, buf, widget->x1 + 6, widget->y1 + 26, COLOR_HGOLD, NULL, NULL);

	/* Prayer button */
	sprite_blt(Bitmaps[BITMAP_PRAY], widget->x1 + 184, widget->y1 + 5, NULL, NULL);
}

/**
 * Show player stats widget with stats, health, mana, grace, etc.
 * @param widget The widget object. */
void widget_player_stats(widgetdata *widget)
{
	double temp;
	SDL_Rect box;
	int tmp;

	/* Let's look if we have a backbuffer SF, if not create one from the background */
	if (!widget->widgetSF)
	{
		widget->widgetSF = SDL_ConvertSurface(Bitmaps[BITMAP_STATS_BG]->bitmap, Bitmaps[BITMAP_STATS_BG]->bitmap->format, Bitmaps[BITMAP_STATS_BG]->bitmap->flags);
	}

	/* We have a backbuffer SF, test for the redrawing flag and do the redrawing */
	if (widget->redraw)
	{
		char buf[MAX_BUF];
		_BLTFX bltfx;

		widget->redraw = 0;

		/* We redraw here only all halfway static stuff *
		 * We simply don't need to redraw that stuff every frame, how often the stats change? */
		bltfx.surface = widget->widgetSF;
		bltfx.flags = 0;
		bltfx.dark_level = 0;
		bltfx.alpha = 0;

		sprite_blt(Bitmaps[BITMAP_STATS_BG], 0, 0, NULL, &bltfx);

		StringBlt(widget->widgetSF, &Font6x3Out, "Stats", 8, 1, COLOR_HGOLD, NULL, NULL);

		/* Strength */
		snprintf(buf, sizeof(buf), "%02d", cpl.stats.Str);
		StringBlt(widget->widgetSF, &SystemFont, "Str", 8, 17, COLOR_WHITE, NULL, NULL);
		StringBlt(widget->widgetSF, &SystemFont, buf, 30, 17, COLOR_GREEN, NULL, NULL);

		/* Dexterity */
		snprintf(buf, sizeof(buf), "%02d", cpl.stats.Dex);
		StringBlt(widget->widgetSF, &SystemFont, "Dex", 8, 28, COLOR_WHITE, NULL, NULL);
		StringBlt(widget->widgetSF, &SystemFont, buf, 30, 28, COLOR_GREEN, NULL, NULL);

		/* Constitution */
		snprintf(buf, sizeof(buf), "%02d", cpl.stats.Con);
		StringBlt(widget->widgetSF, &SystemFont, "Con", 8, 39, COLOR_WHITE, NULL, NULL);
		StringBlt(widget->widgetSF, &SystemFont, buf, 30, 39, COLOR_GREEN, NULL, NULL);

		/* Intelligence */
		snprintf(buf, sizeof(buf), "%02d", cpl.stats.Int);
		StringBlt(widget->widgetSF, &SystemFont, "Int", 8, 50, COLOR_WHITE, NULL, NULL);
		StringBlt(widget->widgetSF, &SystemFont, buf, 30, 50, COLOR_GREEN, NULL, NULL);

		/* Wisdom */
		snprintf(buf, sizeof(buf), "%02d", cpl.stats.Wis);
		StringBlt(widget->widgetSF, &SystemFont, "Wis", 8, 61, COLOR_WHITE, NULL, NULL);
		StringBlt(widget->widgetSF, &SystemFont, buf, 30, 61, COLOR_GREEN, NULL, NULL);

		/* Power */
		snprintf(buf, sizeof(buf), "%02d", cpl.stats.Pow);
		StringBlt(widget->widgetSF, &SystemFont, "Pow", 8, 72, COLOR_WHITE, NULL, NULL);
		StringBlt(widget->widgetSF, &SystemFont, buf, 30, 72, COLOR_GREEN, NULL, NULL);

		/* Charisma */
		snprintf(buf, sizeof(buf), "%02d", cpl.stats.Cha);
		StringBlt(widget->widgetSF, &SystemFont, "Cha", 8, 83, COLOR_WHITE, NULL, NULL);
		StringBlt(widget->widgetSF, &SystemFont, buf, 30, 83, COLOR_GREEN, NULL, NULL);

		/* Health */
		StringBlt(widget->widgetSF, &SystemFont, "Health", 58, 10, COLOR_WHITE, NULL, NULL);
		snprintf(buf, sizeof(buf), "%d (%d)", cpl.stats.hp, cpl.stats.maxhp);
		StringBlt(widget->widgetSF, &SystemFont, buf, 160 - get_string_pixel_length(buf, &SystemFont), 10, COLOR_GREEN, NULL, NULL);

		/* Mana */
		StringBlt(widget->widgetSF, &SystemFont, "Mana", 58, 34, COLOR_WHITE, NULL, NULL);
		snprintf(buf, sizeof(buf), "%d (%d)", cpl.stats.sp, cpl.stats.maxsp);
		StringBlt(widget->widgetSF, &SystemFont, buf, 160 - get_string_pixel_length(buf, &SystemFont), 34, COLOR_GREEN, NULL, NULL);

		/* Grace */
		StringBlt(widget->widgetSF, &SystemFont, "Grace", 58, 58, COLOR_WHITE, NULL, NULL);
		snprintf(buf, sizeof(buf), "%d (%d)", cpl.stats.grace, cpl.stats.maxgrace);
		StringBlt(widget->widgetSF, &SystemFont, buf, 160 - get_string_pixel_length(buf, &SystemFont), 58, COLOR_GREEN, NULL, NULL);

		/* Food */
		StringBlt(widget->widgetSF, &SystemFont, "Food", 58, 84, COLOR_WHITE, NULL, NULL);
	}

	/* Now we blit our backbuffer SF */
	box.x = widget->x1;
	box.y = widget->y1;
	SDL_BlitSurface(widget->widgetSF, NULL, ScreenSurface, &box);

	/* Health bar */
	if (cpl.stats.maxhp)
	{
		tmp = cpl.stats.hp;

		if (tmp < 0)
		{
			tmp = 0;
		}

		temp = (double) tmp / (double) cpl.stats.maxhp;
		box.x = 0;
		box.y = 0;
		box.h = Bitmaps[BITMAP_HP]->bitmap->h;
		box.w = (int) (Bitmaps[BITMAP_HP]->bitmap->w * temp);

		if (tmp && !box.w)
		{
			box.w = 1;
		}

		if (box.w > Bitmaps[BITMAP_HP]->bitmap->w)
		{
			box.w = Bitmaps[BITMAP_HP]->bitmap->w;
		}

		sprite_blt(Bitmaps[BITMAP_HP_BACK], widget->x1 + 57, widget->y1 + 23, NULL, NULL);
		sprite_blt(Bitmaps[BITMAP_HP], widget->x1 + 57, widget->y1 + 23, &box, NULL);
	}

	/* Mana bar */
	if (cpl.stats.maxsp)
	{
		tmp = cpl.stats.sp;

		if (tmp < 0)
		{
			tmp = 0;
		}

		temp = (double) tmp / (double) cpl.stats.maxsp;
		box.x = 0;
		box.y = 0;
		box.h = Bitmaps[BITMAP_SP]->bitmap->h;
		box.w = (int) (Bitmaps[BITMAP_SP]->bitmap->w * temp);

		if (tmp && !box.w)
		{
			box.w = 1;
		}

		if (box.w > Bitmaps[BITMAP_SP]->bitmap->w)
		{
			box.w = Bitmaps[BITMAP_SP]->bitmap->w;
		}

		sprite_blt(Bitmaps[BITMAP_SP_BACK], widget->x1 + 57, widget->y1 + 47, NULL, NULL);
		sprite_blt(Bitmaps[BITMAP_SP], widget->x1 + 57, widget->y1 + 47, &box, NULL);
	}

	/* Grace bar */
	if (cpl.stats.maxgrace)
	{
		tmp = cpl.stats.grace;

		if (tmp < 0)
		{
			tmp = 0;
		}

		temp = (double) tmp / (double) cpl.stats.maxgrace;

		box.x = 0;
		box.y = 0;
		box.h = Bitmaps[BITMAP_GRACE]->bitmap->h;
		box.w = (int) (Bitmaps[BITMAP_GRACE]->bitmap->w * temp);

		if (tmp && !box.w)
		{
			box.w = 1;
		}

		if (box.w > Bitmaps[BITMAP_GRACE]->bitmap->w)
		{
			box.w = Bitmaps[BITMAP_GRACE]->bitmap->w;
		}

		sprite_blt(Bitmaps[BITMAP_GRACE_BACK], widget->x1 + 57, widget->y1 + 71, NULL, NULL);
		sprite_blt(Bitmaps[BITMAP_GRACE], widget->x1 + 57, widget->y1 + 71, &box, NULL);
	}

	/* Food bar */
	tmp = cpl.stats.food;

	if (tmp < 0)
	{
		tmp = 0;
	}

	temp = (double) tmp / 1000;
	box.x = 0;
	box.y = 0;
	box.h = Bitmaps[BITMAP_FOOD]->bitmap->h;
	box.w = (int) (Bitmaps[BITMAP_FOOD]->bitmap->w * temp);

	if (tmp && !box.w)
	{
		box.w = 1;
	}

	if (box.w > Bitmaps[BITMAP_FOOD]->bitmap->w)
	{
		box.w = Bitmaps[BITMAP_FOOD]->bitmap->w;
	}

	sprite_blt(Bitmaps[BITMAP_FOOD_BACK], widget->x1 + 87, widget->y1 + 88, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_FOOD], widget->x1 + 87, widget->y1 + 88, &box, NULL);
}

/**
 * Show menu buttons widget.
 *
 * Menu buttons contain things like the Party button, spell list button,
 * etc.
 * @param widget The widget object. */
void widget_menubuttons(widgetdata *widget)
{
	sprite_blt(Bitmaps[BITMAP_MENU_BUTTONS], widget->x1, widget->y1, NULL, NULL);
}

/**
 * Handle mouse events over the menu buttons widget.
 *
 * Basically calls the right functions depending on which button was
 * clicked.
 * @param widget The widget object.
 * @param x X position of the mouse.
 * @param y Y position of the mouse. */
void widget_menubuttons_event(widgetdata *widget, int x, int y)
{
	int dx = x - widget->x1, dy = y - widget->y1;

	if (dx >= 3 && dx <= 44)
	{
		/* Spell list */
		if (dy >= 1 && dy <= 24)
		{
			check_menu_macros("?M_SPELL_LIST");
		}
		/* Skill list */
		else if (dy >= 26 && dy <= 49)
		{
			check_menu_macros("?M_SKILL_LIST");
		}
		/* Party GUI */
		else if (dy >= 51 && dy <= 74)
		{
			cs_write_string(csocket.fd, "pt list", 7);
		}
		/* Help system */
		else if (dy >= 76 && dy <= 99)
		{
			show_help("main");
		}
	}
}

/**
 * Show skill groups widget.
 * @param widget The widget object. */
void widget_skillgroups(widgetdata *widget)
{
	SDL_Rect box;

	if (!widget->widgetSF)
	{
		widget->widgetSF = SDL_ConvertSurface(Bitmaps[BITMAP_SKILL_LVL_BG]->bitmap, Bitmaps[BITMAP_SKILL_LVL_BG]->bitmap->format, Bitmaps[BITMAP_SKILL_LVL_BG]->bitmap->flags);
	}

	if (widget->redraw)
	{
		char buf[MAX_BUF];
		_BLTFX bltfx;

		widget->redraw = 0;
		bltfx.surface = widget->widgetSF;
		bltfx.flags = 0;
		bltfx.alpha = 0;
		sprite_blt(Bitmaps[BITMAP_SKILL_LVL_BG], 0, 0, NULL, &bltfx);

		StringBlt(widget->widgetSF, &Font6x3Out, "Skill Groups", 3, 1, COLOR_HGOLD, NULL, NULL);
		StringBlt(widget->widgetSF, &Font6x3Out, "name / level", 3, 13, COLOR_HGOLD, NULL, NULL);

		/* Agility */
		snprintf(buf, sizeof(buf), " %d", cpl.stats.skill_level[0]);
		StringBlt(widget->widgetSF, &SystemFont, "Ag:", 6, 26, COLOR_HGOLD, NULL, NULL);
		StringBlt(widget->widgetSF, &SystemFont, buf, 44 - get_string_pixel_length(buf, &SystemFont), 26, COLOR_WHITE, NULL, NULL);

		/* Mental */
		snprintf(buf, sizeof(buf), " %d", cpl.stats.skill_level[2]);
		StringBlt(widget->widgetSF, &SystemFont, "Me:", 6, 38, COLOR_HGOLD, NULL, NULL);
		StringBlt(widget->widgetSF, &SystemFont, buf, 44 - get_string_pixel_length(buf, &SystemFont), 38, COLOR_WHITE, NULL, NULL);

		/* Magic */
		snprintf(buf, sizeof(buf), " %d", cpl.stats.skill_level[4]);
		StringBlt(widget->widgetSF, &SystemFont, "Ma:", 6, 49, COLOR_HGOLD, NULL, NULL);
		StringBlt(widget->widgetSF, &SystemFont, buf, 44 - get_string_pixel_length(buf, &SystemFont), 49, COLOR_WHITE, NULL, NULL);

		/* Personality */
		snprintf(buf, sizeof(buf), " %d", cpl.stats.skill_level[1]);
		StringBlt(widget->widgetSF, &SystemFont, "Pe:", 6, 62, COLOR_HGOLD, NULL, NULL);
		StringBlt(widget->widgetSF, &SystemFont, buf, 44 - get_string_pixel_length(buf, &SystemFont), 62, COLOR_WHITE, NULL, NULL);

		/* Physique */
		snprintf(buf, sizeof(buf), " %d", cpl.stats.skill_level[3]);
		StringBlt(widget->widgetSF, &SystemFont, "Ph:", 6, 74, COLOR_HGOLD, NULL, NULL);
		StringBlt(widget->widgetSF, &SystemFont, buf, 44 - get_string_pixel_length(buf, &SystemFont), 74, COLOR_WHITE, NULL, NULL);

		/* Wisdom */
		snprintf(buf, sizeof(buf), " %d", cpl.stats.skill_level[5]);
		StringBlt(widget->widgetSF, &SystemFont, "Wi:", 6, 86, COLOR_HGOLD, NULL, NULL);
		StringBlt(widget->widgetSF, &SystemFont, buf, 44 - get_string_pixel_length(buf, &SystemFont), 86, COLOR_WHITE, NULL, NULL);
	}

	box.x = widget->x1;
	box.y = widget->y1;
	SDL_BlitSurface(widget->widgetSF, NULL, ScreenSurface, &box);
}

/**
 * Handle mouse events over player doll widget (dragging items). */
void widget_show_player_doll_event()
{
	int old_inv_win = cpl.inventory_win, old_inv_tag = cpl.win_inv_tag;
	cpl.inventory_win = IWIN_INV;

	if (draggingInvItem(DRAG_GET_STATUS) == DRAG_QUICKSLOT)
	{
		cpl.win_inv_tag = cpl.win_quick_tag;

		/* Drop to player doll */
		if (!(locate_item(cpl.win_inv_tag))->applied)
		{
			process_macro_keys(KEYFUNC_APPLY, 0);
		}
	}

	if (draggingInvItem(DRAG_GET_STATUS) == DRAG_IWIN_INV)
	{
		if ((locate_item(cpl.win_inv_tag))->applied)
		{
			draw_info("This is applied already!", COLOR_WHITE);
		}
		/* Drop to player doll */
		else
		{
			process_macro_keys(KEYFUNC_APPLY, 0);
		}
	}

	cpl.inventory_win = old_inv_win;
	cpl.win_inv_tag = old_inv_tag;

	draggingInvItem(DRAG_NONE);
	itemExamined = 0;
}

/**
 * Show player doll widget with applied items from inventory.
 * @param widget The widget object. */
void widget_show_player_doll(widgetdata *widget)
{
	item *tmp;
	char *tooltip_text = NULL;
	char buf[128];
	int index, tooltip_index = -1, ring_flag = 0;
	int mx, my;

	/* This is ugly to calculate because it's a curve which increases heavily
	 * with lower weapon_speed... So, we use a table */
	int ws_temp = cpl.stats.weapon_sp;

	if (ws_temp < 0)
	{
		ws_temp = 0;
	}
	else if (ws_temp > 18)
	{
		ws_temp = 18;
	}

	sprite_blt(Bitmaps[BITMAP_DOLL], widget->x1, widget->y1, NULL, NULL);

	if (!cpl.ob)
	{
		return;
	}

	/* Armour class */
	StringBlt(ScreenSurface, &SystemFont, "AC", widget->x1 + 8, widget->y1 + 50, COLOR_HGOLD, NULL, NULL);
	snprintf(buf, sizeof(buf), "%02d", cpl.stats.ac);
	StringBlt(ScreenSurface, &SystemFont, buf, widget->x1 + 25, widget->y1 + 50, COLOR_WHITE, NULL, NULL);

	/* Weapon class */
	StringBlt(ScreenSurface, &SystemFont, "WC", widget->x1 + 150, widget->y1 + 50, COLOR_HGOLD, NULL, NULL);
	snprintf(buf, sizeof(buf), "%02d", cpl.stats.wc);
	StringBlt(ScreenSurface, &SystemFont, buf, widget->x1 + 173, widget->y1 + 50, COLOR_WHITE, NULL, NULL);

	/* Damage */
	StringBlt(ScreenSurface, &SystemFont, "DMG", widget->x1 + 150, widget->y1 + 60, COLOR_HGOLD, NULL, NULL);
	snprintf(buf, sizeof(buf), "%02d", cpl.stats.dam);
	StringBlt(ScreenSurface, &SystemFont, buf, widget->x1 + 173, widget->y1 + 60, COLOR_WHITE, NULL, NULL);

	/* Weapon speed */
	StringBlt(ScreenSurface, &SystemFont, "WS", widget->x1 + 150, widget->y1 + 70, COLOR_HGOLD, NULL, NULL);
	snprintf(buf, sizeof(buf), "%3.2f sec", weapon_speed_table[ws_temp]);
	StringBlt(ScreenSurface, &SystemFont, buf, widget->x1 + 173, widget->y1 + 70, COLOR_WHITE, NULL, NULL);

	/* Moving speed */
	StringBlt(ScreenSurface, &SystemFont, "Speed ", widget->x1 + 47, widget->y1 + 190, COLOR_HGOLD, NULL, NULL);
	snprintf(buf, sizeof(buf), "%3.2f", (float) cpl.stats.speed / FLOAT_MULTF);
	StringBlt(ScreenSurface, &SystemFont, buf, widget->x1 + 75, widget->y1 + 190, COLOR_WHITE, NULL, NULL);

	/* Show items applied */
	for (tmp = cpl.ob->inv; tmp; tmp = tmp->next)
	{
		if (tmp->applied)
		{
			index = -1;

			switch (tmp->itype)
			{
				case TYPE_ARMOUR:
					index = PDOLL_ARMOUR;
					break;

				case TYPE_HELMET:
					index = PDOLL_HELM;
					break;

				case TYPE_GIRDLE:
					index = PDOLL_GIRDLE;
					break;

				case TYPE_BOOTS:
					index = PDOLL_BOOT;
					break;

				case TYPE_WEAPON:
					index = PDOLL_RHAND;
					break;

				case TYPE_SHIELD:
					index = PDOLL_LHAND;
					break;

				case TYPE_RING:
					index = PDOLL_RRING;
					break;

				case TYPE_BRACERS:
					index = PDOLL_BRACER;
					break;

				case TYPE_AMULET:
					index = PDOLL_AMULET;
					break;

				case TYPE_SKILL:
					index = PDOLL_SKILL;
					break;

				case TYPE_BOW:
					index = PDOLL_BOW;
					break;

				case TYPE_GLOVES:
					index = PDOLL_GAUNTLET;
					break;

				case TYPE_CLOAK:
					index = PDOLL_ROBE;
					break;

				case TYPE_LIGHT_APPLY:
					index = PDOLL_LIGHT;
					break;

				case TYPE_WAND:
				case TYPE_ROD:
				case TYPE_HORN:
					index = PDOLL_WAND;
					break;
			}

			if (index == PDOLL_RRING)
			{
				index += ++ring_flag & 1;
			}

			if (index != -1)
			{
				int mb;
				blt_inv_item_centered(tmp, player_doll[index].xpos + widget->x1, player_doll[index].ypos + widget->y1);
				mb = SDL_GetMouseState(&mx, &my);

				/* Prepare item name tooltip */
				if (mx >= widget->x1 + player_doll[index].xpos && mx < widget->x1 + player_doll[index].xpos + 33 && my >= widget->y1 + player_doll[index].ypos && my < widget->y1 + player_doll[index].ypos + 33)
				{
					tooltip_index = index;
					tooltip_text = tmp->s_name;

					if ((mb & SDL_BUTTON(SDL_BUTTON_LEFT)) && !draggingInvItem(DRAG_GET_STATUS))
					{
						cpl.win_pdoll_tag = tmp->tag;
						draggingInvItem(DRAG_PDOLL);
					}
				}
			}
		}
	}

	/* Draw item name tooltip */
	if (tooltip_index != -1)
	{
		show_tooltip(mx, my, tooltip_text);
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
		widget->widgetSF = SDL_ConvertSurface(Bitmaps[BITMAP_MAIN_LVL_BG]->bitmap, Bitmaps[BITMAP_MAIN_LVL_BG]->bitmap->format, Bitmaps[BITMAP_MAIN_LVL_BG]->bitmap->flags);
	}

	if (widget->redraw)
	{
		char buf[MAX_BUF];
		double multi, line;
		int s, level_exp;
		_BLTFX bltfx;

		widget->redraw = 0;
		bltfx.surface = widget->widgetSF;
		bltfx.flags = 0;
		bltfx.alpha = 0;

		sprite_blt(Bitmaps[BITMAP_MAIN_LVL_BG], 0, 0, NULL, &bltfx);

		StringBlt(widget->widgetSF, &Font6x3Out, "Level / Exp", 4, 1, COLOR_HGOLD, NULL, NULL);
		snprintf(buf, sizeof(buf), "%d", cpl.stats.level);

		if (cpl.stats.level == server_level.level)
		{
			StringBlt(widget->widgetSF, &BigFont, buf, 91 - get_string_pixel_length(buf, &BigFont), 4, COLOR_HGOLD, NULL, NULL);
		}
		else
		{
			StringBlt(widget->widgetSF, &BigFont, buf, 91 - get_string_pixel_length(buf, &BigFont), 4, COLOR_WHITE, NULL, NULL);
		}

		snprintf(buf, sizeof(buf), "%d", cpl.stats.exp);
		StringBlt(widget->widgetSF, &SystemFont, buf, 5, 20, COLOR_WHITE, NULL, NULL);

		/* Calculate the exp bubbles */
		level_exp = cpl.stats.exp - server_level.exp[cpl.stats.level];
		multi = modf(((double) level_exp / (double) (server_level.exp[cpl.stats.level + 1] - server_level.exp[cpl.stats.level]) * 10.0), &line);

		sprite_blt(Bitmaps[BITMAP_EXP_BORDER], 9, 49, NULL, &bltfx);

		if (multi)
		{
			box.x = 0;
			box.y = 0;
			box.h = Bitmaps[BITMAP_EXP_SLIDER]->bitmap->h;
			box.w = (int) (Bitmaps[BITMAP_EXP_SLIDER]->bitmap->w * multi);

			if (!box.w)
			{
				box.w = 1;
			}

			if (box.w > Bitmaps[BITMAP_EXP_SLIDER]->bitmap->w)
			{
				box.w = Bitmaps[BITMAP_EXP_SLIDER]->bitmap->w;
			}

			sprite_blt(Bitmaps[BITMAP_EXP_SLIDER], 9, 49, &box, &bltfx);
		}

		for (s = 0; s < 10; s++)
		{
			sprite_blt(Bitmaps[BITMAP_EXP_BUBBLE2], 10 + s * 8, 40, NULL, &bltfx);
		}

		for (s = 0; s < (int) line; s++)
		{
			sprite_blt(Bitmaps[BITMAP_EXP_BUBBLE1], 10 + s * 8, 40, NULL, &bltfx);
		}
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
		widget->widgetSF = SDL_ConvertSurface(Bitmaps[BITMAP_SKILL_EXP_BG]->bitmap, Bitmaps[BITMAP_SKILL_EXP_BG]->bitmap->format, Bitmaps[BITMAP_SKILL_EXP_BG]->bitmap->flags);
	}

	if (widget->redraw)
	{
		int s, level_exp;
		_BLTFX bltfx;
		char buf[MAX_BUF];
		long int liLExp = 0;
		long int liLExpTNL = 0;
		long int liTExp = 0;
		long int liTExpTNL = 0;
		float fLExpPercent = 0;
		double multi = 0, line = 0;

		widget->redraw = 0;
		bltfx.surface = widget->widgetSF;
		bltfx.flags = 0;
		bltfx.alpha = 0;

		sprite_blt(Bitmaps[BITMAP_SKILL_EXP_BG], 0, 0, NULL, &bltfx);

		StringBlt(widget->widgetSF, &Font6x3Out, "Used", 4, -1, COLOR_HGOLD, NULL, NULL);
		StringBlt(widget->widgetSF, &Font6x3Out, "Skill", 4, 7, COLOR_HGOLD, NULL, NULL);

		if (cpl.skill_name[0] != '\0')
		{
			switch (options.expDisplay)
			{
				/* Default */
				default:
				case 0:
					snprintf(buf, sizeof(buf), "%s", cpl.skill_name);
					break;

				/* LExp% || LExp/LExp tnl || TExp/TExp tnl || (LExp%) LExp/LExp tnl */
				case 1:
				case 2:
				case 3:
				case 4:
					if ((skill_list[cpl.skill_g].entry[cpl.skill_e].exp >= 0) || (skill_list[cpl.skill_g].entry[cpl.skill_e].exp == -2))
					{
						snprintf(buf, sizeof(buf), "%s - level: %d", cpl.skill_name, skill_list[cpl.skill_g].entry[cpl.skill_e].exp_level);
					}
					else
					{
						snprintf(buf, sizeof(buf), "%s - level: **", cpl.skill_name);
					}

					break;
			}

			StringBlt(widget->widgetSF, &SystemFont, buf, 28, 0, COLOR_WHITE, NULL, NULL);

			if (skill_list[cpl.skill_g].entry[cpl.skill_e].exp >= 0)
			{
				level_exp = skill_list[cpl.skill_g].entry[cpl.skill_e].exp - server_level.exp[skill_list[cpl.skill_g].entry[cpl.skill_e].exp_level];
				multi = modf(((double) level_exp / (double) (server_level.exp[skill_list[cpl.skill_g].entry[cpl.skill_e].exp_level + 1] - server_level.exp[skill_list[cpl.skill_g].entry[cpl.skill_e].exp_level]) * 10.0), &line);

				liTExp = skill_list[cpl.skill_g].entry[cpl.skill_e].exp;
				liTExpTNL = server_level.exp[skill_list[cpl.skill_g].entry[cpl.skill_e].exp_level + 1];

				liLExp = liTExp - server_level.exp[skill_list[cpl.skill_g].entry[cpl.skill_e].exp_level];
				liLExpTNL = liTExpTNL - server_level.exp[skill_list[cpl.skill_g].entry[cpl.skill_e].exp_level];

				fLExpPercent = ((float) liLExp / (float) (liLExpTNL)) * 100.0f;
			}

			switch (options.expDisplay)
			{
				/* Default */
				default:
				case 0:
					if (skill_list[cpl.skill_g].entry[cpl.skill_e].exp >= 0)
					{
						snprintf(buf, sizeof(buf), "%d / %-9d", skill_list[cpl.skill_g].entry[cpl.skill_e].exp_level,skill_list[cpl.skill_g].entry[cpl.skill_e].exp);
					}
					else if (skill_list[cpl.skill_g].entry[cpl.skill_e].exp == -2)
					{
						snprintf(buf, sizeof(buf), "%d / **", skill_list[cpl.skill_g].entry[cpl.skill_e].exp_level);
					}
					else
					{
						snprintf(buf, sizeof(buf), "** / **");
					}

					break;

				/* LExp% */
				case 1:
					if (skill_list[cpl.skill_g].entry[cpl.skill_e].exp >= 0)
					{
						snprintf(buf, sizeof(buf), "%#05.2f%%", fLExpPercent);
					}
					else
					{
						snprintf(buf, sizeof(buf), "**.**%%");
					}

					break;

				/* LExp/LExp tnl */
				case 2:
					if (skill_list[cpl.skill_g].entry[cpl.skill_e].exp >= 0)
					{
						snprintf(buf, sizeof(buf), "%ld / %ld", liLExp, liLExpTNL);
					}
					else
					{
						snprintf(buf, sizeof(buf), "** / **");
					}

					break;

				/* TExp/TExp tnl */
				case 3:
					if (skill_list[cpl.skill_g].entry[cpl.skill_e].exp >= 0)
					{
						snprintf(buf, sizeof(buf), "%ld / %ld", liTExp, liTExpTNL);
					}
					else
					{
						snprintf(buf, sizeof(buf), "** / **");
					}

					break;

				/* (LExp%) LExp/LExp tnl */
				case 4:
					if (skill_list[cpl.skill_g].entry[cpl.skill_e].exp >= 0)
					{
						snprintf(buf, sizeof(buf), "%#05.2f%% - %ld", fLExpPercent, liLExpTNL - liLExp);
					}
					else
					{
						snprintf(buf, sizeof(buf), "(**.**%%) **");
					}

					break;
			}

			if (skill_list[cpl.skill_g].entry[cpl.skill_e].exp_level == server_level.level)
			{
				strncpy(buf, "Maximum level reached", sizeof(buf) - 1);
			}

			StringBlt(widget->widgetSF, &SystemFont, buf, 28, 9, COLOR_WHITE, NULL, NULL);

			snprintf(buf, sizeof(buf), "%1.2f sec", cpl.action_timer);
			StringBlt(widget->widgetSF, &SystemFont, buf, 160, 0, COLOR_WHITE, NULL, NULL);
		}

		sprite_blt(Bitmaps[BITMAP_EXP_SKILL_BORDER], 143, 11, NULL, &bltfx);

		if (multi)
		{
			box.x = 0;
			box.y = 0;
			box.h = Bitmaps[BITMAP_EXP_SKILL_LINE]->bitmap->h;
			box.w = (int) (Bitmaps[BITMAP_EXP_SKILL_LINE]->bitmap->w * multi);

			if (!box.w)
			{
				box.w = 1;
			}

			if (box.w > Bitmaps[BITMAP_EXP_SKILL_LINE]->bitmap->w)
			{
				box.w = Bitmaps[BITMAP_EXP_SKILL_LINE]->bitmap->w;
			}

			sprite_blt(Bitmaps[BITMAP_EXP_SKILL_LINE], 146, 18, &box, &bltfx);
		}

		if (line > 0)
		{
			for (s = 0; s < (int) line; s++)
			{
				sprite_blt(Bitmaps[BITMAP_EXP_SKILL_BUBBLE], 146 + s * 5, 13, NULL, &bltfx);
			}
		}
	}

	box.x = widget->x1;
	box.y = widget->y1;
	SDL_BlitSurface(widget->widgetSF, NULL, ScreenSurface, &box);
}

/**
 * Handle mouse events over skill experience widget.
 * @param widget The widget object. */
void widget_skill_exp_event(widgetdata *widget)
{
	int i, ii, j, jj, bFound = 0;

	/* Let's find the skill... and setup the shortcuts to the exp values */
	for (ii = 0; ii <= SKILL_LIST_MAX && (!bFound); ii++)
	{
		jj = cpl.skill_g + ii;

		if (jj >= SKILL_LIST_MAX)
		{
			jj -= SKILL_LIST_MAX;
		}

		for (i = 0; i < DIALOG_LIST_ENTRY && (!bFound); i++)
		{
			/* First page, we have to be offset (and break before looping) */
			if (ii == 0)
			{
				j = cpl.skill_e + i + 1;

				if (j >= DIALOG_LIST_ENTRY)
				{
					break;
				}
			}
			/* Other pages we look through MUST NOT BE OFFSET */
			else
			{
				j = i;
			}

			if (j >= DIALOG_LIST_ENTRY)
			{
				j -= DIALOG_LIST_ENTRY;
			}

			/* We have a list entry */
			if (skill_list[jj].entry[j].flag == LIST_ENTRY_KNOWN)
			{
				/* First one we find is the one we want */
				strncpy(cpl.skill_name, skill_list[jj].entry[j].name, sizeof(cpl.skill_name) - 1);
				cpl.skill_g = jj;
				cpl.skill_e = j;
				bFound = 1;
				break;
			}
		}
	}

	WIDGET_REDRAW(widget);
}

/**
 * Show regeneration widget.
 * @param widget The widget object. */
void widget_show_regeneration(widgetdata *widget)
{
	SDL_Rect box;

	if (!widget->widgetSF)
	{
		widget->widgetSF = SDL_ConvertSurface(Bitmaps[BITMAP_REGEN_BG]->bitmap, Bitmaps[BITMAP_REGEN_BG]->bitmap->format, Bitmaps[BITMAP_REGEN_BG]->bitmap->flags);
	}

	if (widget->redraw)
	{
		char buf[MAX_BUF];
		_BLTFX bltfx;

		widget->redraw = 0;
		bltfx.surface = widget->widgetSF;
		bltfx.flags = 0;
		bltfx.alpha = 0;

		sprite_blt(Bitmaps[BITMAP_REGEN_BG], 0, 0, NULL, &bltfx);

		StringBlt(widget->widgetSF, &Font6x3Out, "Regeneration", 4, 1, COLOR_HGOLD, NULL, NULL);

		/* Health */
		StringBlt(widget->widgetSF, &SystemFont, "HP", 61, 13, COLOR_HGOLD, NULL, NULL);
		snprintf(buf, sizeof(buf), "%2.1f", cpl.gen_hp);
		StringBlt(widget->widgetSF, &SystemFont, buf, 75, 13, COLOR_WHITE, NULL, NULL);

		/* Mana */
		StringBlt(widget->widgetSF, &SystemFont, "Mana", 5, 13, COLOR_HGOLD, NULL, NULL);
		snprintf(buf, sizeof(buf), "%2.1f", cpl.gen_sp);
		StringBlt(cur_widget[REGEN_ID]->widgetSF, &SystemFont, buf, 35, 13, COLOR_WHITE, NULL, NULL);

		/* Grace */
		StringBlt(widget->widgetSF, &SystemFont, "Grace", 5, 24, COLOR_HGOLD, NULL, NULL);
		snprintf(buf, sizeof(buf), "%2.1f", cpl.gen_grace);
		StringBlt(widget->widgetSF, &SystemFont, buf, 35, 24, COLOR_WHITE, NULL, NULL);
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
	SDL_Rect   box, box2;
	_BLTFX     bltfx;
	int        x = widget->x1;
	int        y = widget->y1;

	/* special case, menuitem is highlighted when mouse is moved over it */
	if (widget->WidgetSubtypeID == MENU_ID)
	{
		widget_highlight_menu(widget);
	}

	box.x = box.y = 0;
	box.w = widget->wd;
	box.h = widget->ht;

	/* if we don't have a backbuffer, create it */
	if (!widget->widgetSF || old_alpha_option != options.use_TextwinAlpha)
	{
		/* need to do this, or the foreground could be semi-transparent too */
		SDL_SetAlpha(Bitmaps[BITMAP_TEXTWIN_MASK]->bitmap, SDL_SRCALPHA | SDL_RLEACCEL, 255);
		widget->widgetSF = SDL_ConvertSurface(Bitmaps[BITMAP_TEXTWIN_MASK]->bitmap, Bitmaps[BITMAP_TEXTWIN_MASK]->bitmap->format, Bitmaps[BITMAP_TEXTWIN_MASK]->bitmap->flags);
		SDL_SetColorKey(widget->widgetSF, SDL_SRCCOLORKEY | SDL_RLEACCEL, SDL_MapRGB(widget->widgetSF->format, 0, 0, 0));
	}

	/* backbuffering is a bit trickier
	 * we always blit the background extra because of the alpha */
	if (options.use_TextwinAlpha)
	{
		if (old_container_alpha != options.textwin_alpha || old_alpha_option != options.use_TextwinAlpha)
		{
			if (containerbg)
			{
				SDL_FreeSurface(containerbg);
			}

			SDL_SetAlpha(Bitmaps[BITMAP_TEXTWIN_MASK]->bitmap, SDL_SRCALPHA | SDL_RLEACCEL, options.textwin_alpha);
			containerbg = SDL_DisplayFormatAlpha(Bitmaps[BITMAP_TEXTWIN_MASK]->bitmap);
			SDL_SetAlpha(Bitmaps[BITMAP_TEXTWIN_MASK]->bitmap, SDL_SRCALPHA | SDL_RLEACCEL, 255);

			old_container_alpha = options.textwin_alpha;

			WIDGET_REDRAW(widget);
		}
		box2.x = x;
		box2.y = y;
		SDL_BlitSurface(containerbg, &box, ScreenSurface, &box2);
	}
	else
	{
		if (old_alpha_option != options.use_TextwinAlpha)
		{
			WIDGET_REDRAW(widget);
		}

		sprite_blt(Bitmaps[BITMAP_TEXTWIN], x, y, &box, NULL);
	}

	/* lets draw the widgets in the backbuffer */
	if (widget->redraw)
	{
		widget->redraw = 0;

		SDL_FillRect(widget->widgetSF, NULL, SDL_MapRGBA(widget->widgetSF->format, 0, 0, 0, options.textwin_alpha));

		bltfx.surface = widget->widgetSF;
		bltfx.flags = 0;
		bltfx.alpha = 0;
		if (options.use_TextwinAlpha)
		{
			box.x = 0;
			box.y = 0;
			box.h = 1;
			box.w = widget->wd;
			SDL_FillRect(widget->widgetSF, &box, SDL_MapRGBA(widget->widgetSF->format, 0x60, 0x60, 0x60, 255));
			box.y = widget->ht;
			box.h = 1;
			box.x = 0;
			box.w = widget->wd;
			SDL_FillRect(widget->widgetSF, &box, SDL_MapRGBA(widget->widgetSF->format, 0x60, 0x60, 0x60, 255));
			box.w = widget->wd;
			box.x = box.w - 1;
			box.w = 1;
			box.y = 0;
			box.h = widget->ht;
			SDL_FillRect(widget->widgetSF, &box, SDL_MapRGBA(widget->widgetSF->format, 0x60, 0x60, 0x60, 255));
			box.x = 0;
			box.y = 0;
			box.h = widget->ht;
			box.w = 1;
			SDL_FillRect(widget->widgetSF, &box, SDL_MapRGBA(widget->widgetSF->format, 0x60, 0x60, 0x60, 255));
		}
	}
	box.x = x;
	box.y = y;
	box2.x = 0;
	box2.y = 0;
	box2.w = widget->wd;
	box2.h = widget->ht + 1;

	SDL_BlitSurface(widget->widgetSF, &box2, ScreenSurface, &box);
}

void widget_highlight_menu(widgetdata *widget)
{
	widgetdata *tmp, *tmp2, *tmp3;
	_menu *menu = NULL, *tmp_menu = NULL;
    _menuitem *menuitem = NULL;
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
				x = tmp->x1 + tmp->wd;
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
						;
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
		/* TODO: Remove this later. It's hardcoded here for testing. */
		submenu_chatwindow_filters(tmp_menu->submenu, 0, 0);
		add_menuitem(tmp_menu->submenu, "Test", &menu_detach_widget, MENU_SUBMENU);
		add_menuitem(tmp_menu->submenu, "Test2", &menu_detach_widget, MENU_SUBMENU);
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

	/* If the widget isnt a menuitem, the user probably clicked on a widget it contains.
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

	StringBlt(ScreenSurface, label->font, label->text, widget->x1, widget->y1, label->color, NULL, NULL);
}
