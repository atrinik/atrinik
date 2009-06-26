/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
*                                                                       *
* This program is free software; you can redistribute it and/or modify  *
* it under the terms of the GNU General Public License as published by  *
* the Free Software Foundation; either version 3 of the License, or     *
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

/* This file handles various player related functions.  This includes
 * both things that operate on the player item, cpl structure, or
 * various commands that the player issues.
 *
 *  This file does most of the handling of commands from the client to
 *  server (see commands.c for server->client)
 *
 *  does most of the work for sending messages to the server
 *   Again, most of these appear self explanatory.  Most send a bunch of
 *   commands like apply, examine, fire, run, etc.  This looks like it
 *   was done by Mark to remove the old keypress stupidity I used.
 */

/* This translates the numeric direction id's into the actual direction
 * commands.  This lets us send the actual command (ie, 'north'), which
 * makes handling on the server side easier.
 */

#include <include.h>
#include <math.h>
/*
 *  Initialiazes player item, information is received from server
 */

_server_level server_level;

typedef enum _player_doll_enum {
	PDOLL_ARMOUR,
	PDOLL_HELM,
	PDOLL_GIRDLE,
	PDOLL_BOOT,
	PDOLL_RHAND,
	PDOLL_LHAND,
	PDOLL_RRING,
	PDOLL_LRING,
	PDOLL_BRACER,
	PDOLL_AMULET,
	PDOLL_SKILL,
	PDOLL_WAND,
	PDOLL_BOW,
	PDOLL_GAUNTLET,
	PDOLL_ROBE,
	PDOLL_LIGHT,

	/* must be last element */
	PDOLL_INIT
}_player_doll_enum;

typedef struct _player_doll_pos {
    int xpos;
    int ypos;
}_player_doll_pos;

_player_doll_pos player_doll[PDOLL_INIT] = {
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
static float weapon_speed_table[19] = {
	20.0f, 	18.0f, 	10.0f, 	8.0f, 	5.5f,
	4.25f, 	3.50f, 	3.05f, 	2.70f, 	2.35f,
	2.15f,	1.95f,	1.80f, 	1.60f, 	1.52f,
	1.44f, 	1.32f, 	1.25f, 	1.20f
};

void clear_player(void)
{
	memset(quick_slots, -1, sizeof(quick_slots));

	free_all_items(cpl.sack);
	free_all_items(cpl.below);
	free_all_items(cpl.ob);
	cpl.ob = player_item();
	init_player_data();
}

void new_player(long tag, char *name, long weight, short face)
{
	cpl.ob->tag = tag;
	cpl.ob->weight = (float) weight / 1000;
	cpl.ob->face = face;
	copy_name(cpl.ob->d_name, name);
}

void new_char(_server_char *nc)
{
	char buf[MAX_BUF];

	snprintf(buf, sizeof(buf), "nc %s %d %d %d %d %d %d %d", nc->char_arch[nc->gender_selected], nc->stats[0], nc->stats[1], nc->stats[2], nc->stats[3], nc->stats[4], nc->stats[5], nc->stats[6]);
    cs_write_string(csocket.fd, buf, strlen(buf));
}

void look_at(int x, int y)
{
	char buf[MAX_BUF];

	snprintf(buf, sizeof(buf), "lt %d %d", x, y);
	cs_write_string(csocket.fd, buf, strlen(buf));
}

void client_send_apply(int tag)
{
	char buf[MAX_BUF];

	snprintf(buf, sizeof(buf), "ap %d", tag);
	cs_write_string(csocket.fd, buf, strlen(buf));
}

void client_send_examine(int tag)
{
	char buf[MAX_BUF];

	snprintf(buf, sizeof(buf), "ex %d", tag);
	cs_write_string(csocket.fd, buf, strlen(buf));
}

/* Requests nrof objects of tag get moved to loc. */
void client_send_move(int loc, int tag, int nrof)
{
	char buf[MAX_BUF];

	snprintf(buf, sizeof(buf), "mv %d %d %d", loc, tag, nrof);
	cs_write_string(csocket.fd, buf, strlen(buf));
}


/* This should be used for all 'command' processing.  Other functions should
 * call this so that proper windowing will be done.
 * command is the text command, repeat is a count value, or -1 if none
 * is desired and we don't want to reset the current count.
 * must_send means we must send this command no matter what (ie, it is
 * an administrative type of command like fire_stop, and failure to send
 * it will cause definate problems.
 * return 1 if command was sent, 0 if not sent. */
int send_command(const char *command, int repeat, int must_send)
{
	char buf[MAX_BUF];
	static char last_command[MAX_BUF] = "";
	SockList sl;
	int commdiff = csocket.command_sent - csocket.command_received;

	if (commdiff < 0)
		commdiff += 256;

	/* Don't want to copy in administrative commands */
	if (!must_send)
		strcpy(last_command, command);

	csocket.command_sent++;
	/* max out at 255 */
	csocket.command_sent &= 0xff;

	sl.buf = (unsigned char *)buf;
	strcpy((char *)sl.buf, "cm ");
	sl.len = 3;
	SockList_AddShort(&sl, (uint16)csocket.command_sent);
	SockList_AddInt(&sl, repeat);
	strncpy((char *)sl.buf + sl.len, command, MAX_BUF - sl.len);
	sl.buf[MAX_BUF - 1]=0;
	sl.len += strlen(command);
	send_socklist(csocket.fd, sl);

	if (repeat != -1)
		cpl.count = 0;

	return 1;
}

void CompleteCmd(unsigned char *data, int len)
{
	if (len != 6)
	{
		fprintf(stderr, "comc - invalid length %d - ignoring\n", len);
	}

	csocket.command_received = GetShort_String(data);
	csocket.command_time = GetInt_String(data + 2);
}

/* This is an extended command (ie, 'who, 'whatever, etc).  In general,
 * we just send the command to the server, but there are a few that
 * we care about (bind, unbind)
 *
 * The command past to us can not be modified - if it is a keybinding,
 * we get passed the string that is that binding - modifying it effectively
 * changes the binding. */
void set_weight_limit (uint32 wlim)
{
    cpl.weight_limit = wlim;
}

void init_player_data(void)
{
	int i;

	new_player (0, "", 0,0);

	cpl.fire_on = cpl.firekey_on = 0;
	cpl.resize_twin = 0;
	cpl.resize_twin_marker = 0;
	cpl.run_on = cpl.runkey_on = 0;
	cpl.inventory_win = IWIN_BELOW;

	cpl.count_left = 0;
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
	cpl.input_text[0] = '\0';

	cpl.title[0] = '\0';
	cpl.alignment[0] = '\0';
	cpl.gender[0] = '\0';
	cpl.range[0] = '\0';

	for (i = 0; i < range_size; i++)
		cpl.ranges[i] = NULL;

	cpl.map_x = 0;
	cpl.map_y = 0;

	cpl.ob->nrof = 1;

	/* This is set from title in stat cmd */
	strcpy(cpl.pname, "");
	strcpy(cpl.title, "");

	strcpy(cpl.partyname, "");

	cpl.menustatus = MENU_NO;
	cpl.menustatus = MENU_NO;
	cpl.count_left = 0;

	/* Avoid division by 0 errors */
	cpl.stats.maxsp = 1;
	cpl.stats.maxhp = 1;
	cpl.stats.maxgrace = 1;

	/* Displayed weapon speed is weapon speed/speed */
	cpl.stats.speed = 0;
	cpl.stats.weapon_sp = 0;
	cpl.action_timer = 0.0f;
	cpl.input_text[0] = '\0';
	cpl.range[0] = '\0';
	cpl.last_command[0] = '\0';

	for (i = 0; i < range_size; i++)
		cpl.ranges[i] = NULL;

	cpl.map_x = 0;
	cpl.map_y = 0;
	cpl.container_tag = -997;
	cpl.container = NULL;
	cpl.magicmap = NULL;

	RangeFireMode = 0;
}

void widget_player_data_event(int x, int y)
{
    int mx = 0, my = 0;
    mx = x - cur_widget[PLAYER_INFO_ID].x1;
    my = y - cur_widget[PLAYER_INFO_ID].y1;

    if (mx >= 184 && mx <= 210 && my >=5 && my <= 35)
    {
		if (!client_command_check("/pray"))
			send_command("/pray", -1, SC_NORMAL);
    }
}

/* Player name, exp, level, title */
void widget_show_player_data(int x, int y)
{
	char buf[256];

	sprite_blt(Bitmaps[BITMAP_PLAYER_INFO], x, y, NULL, NULL);

	if (cpl.rank[0] != '\0')
		snprintf(buf, sizeof(buf), "%s %s\n", cpl.rank, cpl.pname);
	else
		strncpy(buf, cpl.pname, sizeof(buf));

	StringBlt(ScreenSurface, &SystemFont,buf,x + 6, y + 2, COLOR_HGOLD, NULL, NULL);

	snprintf(buf, sizeof(buf), "%s %s %s", cpl.gender, cpl.race, cpl.title);
	StringBlt(ScreenSurface, &SystemFont, buf, x + 6, y + 14, COLOR_HGOLD, NULL, NULL);

	if (strcmp(cpl.godname,"none") )
		snprintf(buf, sizeof(buf), "%s follower of %s", cpl.alignment, cpl.godname);
	else
		strncpy(buf, cpl.alignment, sizeof(buf));

	StringBlt(ScreenSurface, &SystemFont, buf, x + 6, y + 26, COLOR_HGOLD, NULL, NULL);

    /* Prayer button */
    sprite_blt(Bitmaps[BITMAP_PRAY], x + 184, y + 5, NULL, NULL);
}

/* Hp, SP, Grace */
void widget_player_stats(int x, int y)
{
    char buf[256];
    double temp;
    SDL_Rect box;
    int mx, my;
	_BLTFX bltfx;

    SDL_GetMouseState(&mx, &my);

    /* Let's look if we have a backbuffer SF, if not create one from the background */
    if (!widgetSF[STATS_ID])
        widgetSF[STATS_ID] = SDL_ConvertSurface(Bitmaps[BITMAP_STATS_BG]->bitmap, Bitmaps[BITMAP_STATS_BG]->bitmap->format, Bitmaps[BITMAP_STATS_BG]->bitmap->flags);

    /* We have a backbuffer SF, test for the redrawing flag and do the redrawing */
    if (cur_widget[STATS_ID].redraw)
    {
        cur_widget[STATS_ID].redraw = 0;

		/* We redraw here only all halfway static stuff
         * We simply don't need to redraw that stuff every frame, how often the stats change? */

        bltfx.surface = widgetSF[STATS_ID];
        bltfx.flags = 0;
        bltfx.dark_level = 0;
        bltfx.alpha = 0;

        sprite_blt(Bitmaps[BITMAP_STATS_BG], 0, 0, NULL, &bltfx);

        StringBlt(widgetSF[STATS_ID], &Font6x3Out, "Stats", 8, 1, COLOR_HGOLD, NULL, NULL);

		/* Strength */
        snprintf(buf, sizeof(buf), "%02d", cpl.stats.Str);
        StringBlt(widgetSF[STATS_ID], &SystemFont, "Str", 8, 17, COLOR_WHITE, NULL, NULL);
        StringBlt(widgetSF[STATS_ID], &SystemFont, buf, 30, 17, COLOR_GREEN, NULL, NULL);

		/* Dexterity */
        snprintf(buf, sizeof(buf), "%02d", cpl.stats.Dex);
        StringBlt(widgetSF[STATS_ID], &SystemFont, "Dex", 8, 28, COLOR_WHITE, NULL, NULL);
        StringBlt(widgetSF[STATS_ID], &SystemFont, buf, 30, 28, COLOR_GREEN, NULL, NULL);

		/* Constitution */
        snprintf(buf, sizeof(buf), "%02d", cpl.stats.Con);
        StringBlt(widgetSF[STATS_ID], &SystemFont, "Con", 8, 39, COLOR_WHITE, NULL, NULL);
        StringBlt(widgetSF[STATS_ID], &SystemFont, buf, 30, 39, COLOR_GREEN, NULL, NULL);

		/* Intelligence */
        snprintf(buf, sizeof(buf), "%02d", cpl.stats.Int);
        StringBlt(widgetSF[STATS_ID], &SystemFont, "Int", 8, 50, COLOR_WHITE, NULL, NULL);
        StringBlt(widgetSF[STATS_ID], &SystemFont, buf, 30, 50, COLOR_GREEN, NULL, NULL);

		/* Wisdom */
        snprintf(buf, sizeof(buf), "%02d", cpl.stats.Wis);
        StringBlt(widgetSF[STATS_ID], &SystemFont, "Wis", 8, 61, COLOR_WHITE, NULL, NULL);
        StringBlt(widgetSF[STATS_ID], &SystemFont, buf, 30, 61, COLOR_GREEN, NULL, NULL);

		/* Power */
        snprintf(buf, sizeof(buf), "%02d", cpl.stats.Pow);
        StringBlt(widgetSF[STATS_ID], &SystemFont, "Pow", 8, 72, COLOR_WHITE, NULL, NULL);
        StringBlt(widgetSF[STATS_ID], &SystemFont, buf, 30, 72, COLOR_GREEN, NULL, NULL);

		/* Charisma */
        snprintf(buf, sizeof(buf), "%02d", cpl.stats.Cha);
        StringBlt(widgetSF[STATS_ID], &SystemFont, "Cha", 8, 83, COLOR_WHITE, NULL, NULL);
        StringBlt(widgetSF[STATS_ID], &SystemFont, buf, 30, 83, COLOR_GREEN, NULL, NULL);

		/* Health */
		StringBlt(widgetSF[STATS_ID], &SystemFont, "Health", 58, 10, COLOR_WHITE, NULL, NULL);
		snprintf(buf, sizeof(buf), "%d (%d)", cpl.stats.hp, cpl.stats.maxhp);
		StringBlt(widgetSF[STATS_ID], &SystemFont, buf, 160 - get_string_pixel_length(buf, &SystemFont), 10, COLOR_GREEN, NULL, NULL);

		/* Mana */
		StringBlt(widgetSF[STATS_ID], &SystemFont, "Mana", 58, 34, COLOR_WHITE, NULL, NULL);
		snprintf(buf, sizeof(buf), "%d (%d)", cpl.stats.sp, cpl.stats.maxsp);
		StringBlt(widgetSF[STATS_ID], &SystemFont, buf, 160 - get_string_pixel_length(buf, &SystemFont), 34, COLOR_GREEN, NULL, NULL);

		/* Grace */
		StringBlt(widgetSF[STATS_ID], &SystemFont, "Grace", 58, 58, COLOR_WHITE, NULL, NULL);
		snprintf(buf, sizeof(buf), "%d (%d)", cpl.stats.grace, cpl.stats.maxgrace);
		StringBlt(widgetSF[STATS_ID], &SystemFont, buf, 160 - get_string_pixel_length(buf, &SystemFont), 58, COLOR_GREEN, NULL, NULL);

		/* Food */
		StringBlt(widgetSF[STATS_ID], &SystemFont, "Food", 58, 84, COLOR_WHITE, NULL, NULL);
    }

    /* Now we blit our backbuffer SF */
    box.x = x;
    box.y = y;
    SDL_BlitSurface(widgetSF[STATS_ID], NULL, ScreenSurface, &box);

	/* Health bar */
	if (cpl.stats.maxhp)
	{
		int tmp = cpl.stats.hp;

		if (tmp < 0)
			tmp = 0;

		temp = (double) tmp / (double) cpl.stats.maxhp;
		box.x = 0;
		box.y = 0;
		box.h = Bitmaps[BITMAP_HP]->bitmap->h;
		box.w = (int) (Bitmaps[BITMAP_HP]->bitmap->w*temp);

		if (tmp && !box.w)
			box.w = 1;

		if (box.w > Bitmaps[BITMAP_HP]->bitmap->w)
			box.w=Bitmaps[BITMAP_HP]->bitmap->w;

		sprite_blt(Bitmaps[BITMAP_HP_BACK], x + 57, y + 23, NULL, NULL);
		sprite_blt(Bitmaps[BITMAP_HP], x + 57, y + 23, &box, NULL);
	}

	/* Mana bar */
	if (cpl.stats.maxsp)
	{
		int tmp = cpl.stats.sp;

		if (tmp < 0)
			tmp = 0;

		temp = (double) tmp / (double) cpl.stats.maxsp;
		box.x = 0;
		box.y = 0;
		box.h = Bitmaps[BITMAP_SP]->bitmap->h;
		box.w = (int) (Bitmaps[BITMAP_SP]->bitmap->w * temp);

		if (tmp && !box.w)
			box.w = 1;

		if (box.w > Bitmaps[BITMAP_SP]->bitmap->w)
			box.w = Bitmaps[BITMAP_SP]->bitmap->w;

		sprite_blt(Bitmaps[BITMAP_SP_BACK], x + 57, y + 47, NULL, NULL);
		sprite_blt(Bitmaps[BITMAP_SP], x + 57, y + 47, &box, NULL);
	}

	/* Grace bar */
	if (cpl.stats.maxgrace)
	{
		int tmp = cpl.stats.grace;

		if (tmp < 0)
			tmp = 0;

		temp = (double) tmp / (double) cpl.stats.maxgrace;

		box.x = 0;
		box.y = 0;
		box.h = Bitmaps[BITMAP_GRACE]->bitmap->h;
		box.w = (int) (Bitmaps[BITMAP_GRACE]->bitmap->w * temp);

		if (tmp && !box.w)
			box.w = 1;

		if (box.w > Bitmaps[BITMAP_GRACE]->bitmap->w)
			box.w = Bitmaps[BITMAP_GRACE]->bitmap->w;

		sprite_blt(Bitmaps[BITMAP_GRACE_BACK], x + 57, y + 71, NULL, NULL);
		sprite_blt(Bitmaps[BITMAP_GRACE], x + 57, y + 71, &box, NULL);
	}

	/* Food bar */
	if (cpl.stats.food)
	{
		int tmp = cpl.stats.food;

		if (tmp < 0)
			tmp = 0;

		temp = (double) tmp / 1000;
		box.x = 0;
		box.y = 0;
		box.h = Bitmaps[BITMAP_FOOD]->bitmap->h;
		box.w = (int) (Bitmaps[BITMAP_FOOD]->bitmap->w * temp);

		if (tmp && !box.w)
			box.w = 1;

		if (box.w > Bitmaps[BITMAP_FOOD]->bitmap->w)
			box.w = Bitmaps[BITMAP_FOOD]->bitmap->w;

		sprite_blt(Bitmaps[BITMAP_FOOD_BACK], x + 87, y + 88, NULL, NULL);
		sprite_blt(Bitmaps[BITMAP_FOOD], x + 87, y + 88, &box, NULL);
	}
}

void widget_menubuttons(int x, int y)
{
    sprite_blt(Bitmaps[BITMAP_MENU_BUTTONS], x, y, NULL, NULL);
}

void widget_menubuttons_event(int x, int y)
{
    int dx = x - cur_widget[MENU_B_ID].x1, dy = y - cur_widget[MENU_B_ID].y1;

    if (dx >= 3 && dx <= 44)
    {
		/* Spell list */
		if (dy >= 1 && dy <= 24)
			check_menu_macros("?M_SPELL_LIST");
		/* Skill list */
		else if (dy >= 26 && dy <= 49)
			check_menu_macros("?M_SKILL_LIST");
		/* Party GUI */
		else if (dy >= 51 && dy <= 74)
    		cs_write_string(csocket.fd, "pt list", 7);
		/* Help system */
		else if (dy >= 76 && dy <= 99)
			show_help("main");
    }
}

void widget_skillgroups(int x, int y)
{
    char buf[256];
    SDL_Rect box;
	_BLTFX bltfx;

    if (!widgetSF[SKILL_LVL_ID])
        widgetSF[SKILL_LVL_ID] = SDL_ConvertSurface(Bitmaps[BITMAP_SKILL_LVL_BG]->bitmap, Bitmaps[BITMAP_SKILL_LVL_BG]->bitmap->format, Bitmaps[BITMAP_SKILL_LVL_BG]->bitmap->flags);

    if (cur_widget[SKILL_LVL_ID].redraw)
    {
        cur_widget[SKILL_LVL_ID].redraw = 0;

		bltfx.surface = widgetSF[SKILL_LVL_ID];
        bltfx.flags = 0;
        bltfx.alpha = 0;
        sprite_blt(Bitmaps[BITMAP_SKILL_LVL_BG], 0, 0, NULL, &bltfx);

        StringBlt(widgetSF[SKILL_LVL_ID], &Font6x3Out, "Skill Groups", 3, 1, COLOR_HGOLD, NULL, NULL);
        StringBlt(widgetSF[SKILL_LVL_ID], &Font6x3Out, "name / level", 3, 13, COLOR_HGOLD, NULL, NULL);

		/* Agility */
        snprintf(buf, sizeof(buf), " %d", cpl.stats.skill_level[0]);
        StringBlt(widgetSF[SKILL_LVL_ID], &SystemFont, "Ag:", 6, 26, COLOR_HGOLD, NULL, NULL);
        StringBlt(widgetSF[SKILL_LVL_ID], &SystemFont, buf, 44 - get_string_pixel_length(buf, &SystemFont), 26, COLOR_WHITE, NULL, NULL);

		/* Mental */
        snprintf(buf, sizeof(buf), " %d", cpl.stats.skill_level[2]);
        StringBlt(widgetSF[SKILL_LVL_ID], &SystemFont, "Me:", 6, 38, COLOR_HGOLD, NULL, NULL);
        StringBlt(widgetSF[SKILL_LVL_ID], &SystemFont, buf, 44 - get_string_pixel_length(buf, &SystemFont), 38, COLOR_WHITE, NULL, NULL);

		/* Magic */
        snprintf(buf, sizeof(buf), " %d", cpl.stats.skill_level[4]);
        StringBlt(widgetSF[SKILL_LVL_ID], &SystemFont, "Ma:", 6, 49, COLOR_HGOLD, NULL, NULL);
        StringBlt(widgetSF[SKILL_LVL_ID], &SystemFont, buf, 44 - get_string_pixel_length(buf, &SystemFont), 49, COLOR_WHITE, NULL, NULL);

		/* Personality */
        snprintf(buf, sizeof(buf), " %d", cpl.stats.skill_level[1]);
        StringBlt(widgetSF[SKILL_LVL_ID], &SystemFont, "Pe:", 6, 62, COLOR_HGOLD, NULL, NULL);
        StringBlt(widgetSF[SKILL_LVL_ID], &SystemFont, buf, 44 - get_string_pixel_length(buf, &SystemFont), 62, COLOR_WHITE, NULL, NULL);

		/* Physique */
        snprintf(buf, sizeof(buf), " %d", cpl.stats.skill_level[3]);
        StringBlt(widgetSF[SKILL_LVL_ID], &SystemFont, "Ph:", 6, 74, COLOR_HGOLD, NULL, NULL);
        StringBlt(widgetSF[SKILL_LVL_ID], &SystemFont, buf, 44 - get_string_pixel_length(buf, &SystemFont), 74, COLOR_WHITE, NULL, NULL);

		/* Wisdom */
        snprintf(buf, sizeof(buf), " %d", cpl.stats.skill_level[5]);
        StringBlt(widgetSF[SKILL_LVL_ID], &SystemFont, "Wi:", 6, 86, COLOR_HGOLD, NULL, NULL);
        StringBlt(widgetSF[SKILL_LVL_ID], &SystemFont, buf, 44 - get_string_pixel_length(buf, &SystemFont), 86, COLOR_WHITE, NULL, NULL);
    }

    box.x = x;
    box.y = y;
    SDL_BlitSurface(widgetSF[SKILL_LVL_ID], NULL, ScreenSurface, &box);
}

void widget_show_player_doll_event()
{
    int old_inv_win = cpl.inventory_win;
    int old_inv_tag = cpl.win_inv_tag;
    cpl.inventory_win = IWIN_INV;

    if (draggingInvItem(DRAG_GET_STATUS) == DRAG_QUICKSLOT)
    {
        cpl.win_inv_tag = cpl.win_quick_tag;

		/* Drop to player doll */
        if (!(locate_item(cpl.win_inv_tag))->applied)
            process_macro_keys(KEYFUNC_APPLY, 0);
    }

    if (draggingInvItem(DRAG_GET_STATUS) == DRAG_IWIN_INV)
    {
        if ((locate_item(cpl.win_inv_tag))->applied)
            draw_info("This is applied already!", COLOR_WHITE);
		/* Drop to player doll */
        else
            process_macro_keys(KEYFUNC_APPLY, 0);
    }

    cpl.inventory_win = old_inv_win;
    cpl.win_inv_tag = old_inv_tag;

    draggingInvItem(DRAG_NONE);
    itemExamined = 0;
}

/* Player doll with inventory */
void widget_show_player_doll(int x, int y)
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
		ws_temp = 0;
	else if (ws_temp > 18)
		ws_temp = 18;

    sprite_blt(Bitmaps[BITMAP_DOLL], x, y, NULL, NULL);

    if (!cpl.ob)
        return;

	/* Armour class */
    StringBlt(ScreenSurface, &SystemFont, "AC", x + 8, y + 50, COLOR_HGOLD, NULL, NULL);
    snprintf(buf, sizeof(buf), "%02d", cpl.stats.ac);
    StringBlt(ScreenSurface, &SystemFont, buf, x + 25, y + 50, COLOR_WHITE, NULL, NULL);

	/* Weapon class */
    StringBlt(ScreenSurface, &SystemFont, "WC", x + 150, y + 50, COLOR_HGOLD, NULL, NULL);
	snprintf(buf, sizeof(buf), "%02d", cpl.stats.wc);
    StringBlt(ScreenSurface, &SystemFont, buf, x + 173, y + 50, COLOR_WHITE, NULL, NULL);

	/* Damage */
    StringBlt(ScreenSurface, &SystemFont, "DMG", x + 150, y + 60, COLOR_HGOLD, NULL, NULL);
	snprintf(buf, sizeof(buf), "%02d", cpl.stats.dam);
    StringBlt(ScreenSurface, &SystemFont, buf, x + 173, y + 60, COLOR_WHITE, NULL, NULL);

	/* Weapon speed */
    StringBlt(ScreenSurface, &SystemFont, "WS", x + 150, y + 70, COLOR_HGOLD, NULL, NULL);
	snprintf(buf, sizeof(buf), "%3.2f sec", weapon_speed_table[ws_temp]);
    StringBlt(ScreenSurface, &SystemFont, buf, x + 173, y + 70, COLOR_WHITE, NULL, NULL);

	/* Moving speed */
    StringBlt(ScreenSurface, &SystemFont, "Speed ", x + 47, y + 190, COLOR_HGOLD, NULL, NULL);
    snprintf(buf, sizeof(buf), "%3.2f", (float)cpl.stats.speed / FLOAT_MULTF);
    StringBlt(ScreenSurface, &SystemFont, buf, x + 75, y + 190, COLOR_WHITE, NULL, NULL);

	/* Show items applied */
	for (tmp = cpl.ob->inv; tmp; tmp = tmp->next)
	{
		if (tmp->applied)
		{
            index = -1;

            if (tmp->itype == TYPE_ARMOUR)
                index = PDOLL_ARMOUR;
            else if (tmp->itype == TYPE_HELMET)
                index = PDOLL_HELM;
            else if (tmp->itype == TYPE_GIRDLE)
                index = PDOLL_GIRDLE;
            else if (tmp->itype == TYPE_BOOTS)
                index = PDOLL_BOOT;
            else if (tmp->itype == TYPE_WEAPON)
                index = PDOLL_RHAND;
            else if (tmp->itype == TYPE_SHIELD)
                index = PDOLL_LHAND;
            else if (tmp->itype == TYPE_RING)
                index = PDOLL_RRING;
            else if (tmp->itype == TYPE_BRACERS)
                index = PDOLL_BRACER;
            else if (tmp->itype == TYPE_AMULET)
                index = PDOLL_AMULET;
            else if (tmp->itype == TYPE_SKILL)
                index = PDOLL_SKILL;
            else if (tmp->itype == TYPE_BOW)
                index = PDOLL_BOW;
            else if (tmp->itype == TYPE_GLOVES)
                index = PDOLL_GAUNTLET;
            else if (tmp->itype == TYPE_CLOAK)
                index = PDOLL_ROBE;
            else if (tmp->itype == TYPE_LIGHT_APPLY)
                index = PDOLL_LIGHT;
            else if (tmp->itype == TYPE_WAND || tmp->itype == TYPE_ROD || tmp->itype == TYPE_HORN)
                index = PDOLL_WAND;

			if (index == PDOLL_RRING)
				index += ++ring_flag & 1;

			if (index != -1)
			{
				int mb;
				blt_inv_item_centered(tmp, player_doll[index].xpos + x, player_doll[index].ypos + y);
				mb = SDL_GetMouseState(&mx, &my);

				/* Prepare item name tooltip */
				if (mx >= x + player_doll[index].xpos && mx < x + player_doll[index].xpos + 33 && my >= y + player_doll[index].ypos && my < y + player_doll[index].ypos + 33)
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
		show_tooltip(mx, my, tooltip_text);
}

void widget_show_main_lvl(int x, int y)
{
    char buf[256];
    double multi, line;
    SDL_Rect box;
    int s, level_exp;
	_BLTFX bltfx;

    if (!widgetSF[MAIN_LVL_ID])
        widgetSF[MAIN_LVL_ID] = SDL_ConvertSurface(Bitmaps[BITMAP_MAIN_LVL_BG]->bitmap, Bitmaps[BITMAP_MAIN_LVL_BG]->bitmap->format, Bitmaps[BITMAP_MAIN_LVL_BG]->bitmap->flags);

    if (cur_widget[MAIN_LVL_ID].redraw)
    {
        cur_widget[MAIN_LVL_ID].redraw = 0;

		bltfx.surface = widgetSF[MAIN_LVL_ID];
        bltfx.flags = 0;
        bltfx.alpha = 0;

        sprite_blt(Bitmaps[BITMAP_MAIN_LVL_BG], 0, 0, NULL, &bltfx);

        StringBlt(widgetSF[MAIN_LVL_ID], &Font6x3Out, "Level / Exp", 4, 1, COLOR_HGOLD, NULL, NULL);
        snprintf(buf, sizeof(buf), "%d", cpl.stats.level);

        if (cpl.stats.level == MAX_LEVEL)
            StringBlt(widgetSF[MAIN_LVL_ID], &BigFont, buf, 91 - get_string_pixel_length(buf, &BigFont), 4, COLOR_HGOLD, NULL, NULL);
        else
            StringBlt(widgetSF[MAIN_LVL_ID], &BigFont, buf, 91 - get_string_pixel_length(buf, &BigFont), 4, COLOR_WHITE, NULL, NULL);

        snprintf(buf, sizeof(buf), "%d", cpl.stats.exp);
        StringBlt(widgetSF[MAIN_LVL_ID], &SystemFont, buf, 5, 20, COLOR_WHITE, NULL, NULL);

        /* Calc the exp bubbles */
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
                box.w = 1;

            if (box.w > Bitmaps[BITMAP_EXP_SLIDER]->bitmap->w)
                box.w = Bitmaps[BITMAP_EXP_SLIDER]->bitmap->w;

            sprite_blt(Bitmaps[BITMAP_EXP_SLIDER], 9, 49, &box, &bltfx);
        }

        for (s = 0; s < 10; s++)
            sprite_blt(Bitmaps[BITMAP_EXP_BUBBLE2], 10 + s * 8, 40, NULL, &bltfx);

        for (s = 0; s < (int) line; s++)
            sprite_blt(Bitmaps[BITMAP_EXP_BUBBLE1], 10 + s * 8, 40, NULL, &bltfx);
    }

    box.x = x;
    box.y = y;
    SDL_BlitSurface(widgetSF[MAIN_LVL_ID], NULL, ScreenSurface, &box);
}

void widget_show_skill_exp(int x, int y)
{
	_BLTFX bltfx;
	char buf[256];
    double multi, line;
    SDL_Rect box;
    int s, level_exp;
    long int liLExp = 0;
    long int liLExpTNL = 0;
    long int liTExp = 0;
    long int liTExpTNL = 0;
    float fLExpPercent = 0;
	multi = line = 0;
	static int action_tick = 0;

	/* Pre-emptively tick down the skill delay timer */
    if (cpl.action_timer > 0)
    {
        if (LastTick - action_tick > 125)
        {
            cpl.action_timer -= (float) (LastTick - action_tick) / 1000.0f;

            if (cpl.action_timer < 0)
                cpl.action_timer = 0;

            action_tick = LastTick;
			WIDGET_REDRAW(SKILL_EXP_ID);
        }
    }
    else
        action_tick = LastTick;


    if (!widgetSF[SKILL_EXP_ID])
        widgetSF[SKILL_EXP_ID] = SDL_ConvertSurface(Bitmaps[BITMAP_SKILL_EXP_BG]->bitmap, Bitmaps[BITMAP_SKILL_EXP_BG]->bitmap->format, Bitmaps[BITMAP_SKILL_EXP_BG]->bitmap->flags);

    if (cur_widget[SKILL_EXP_ID].redraw)
    {
        cur_widget[SKILL_EXP_ID].redraw = 0;

		bltfx.surface = widgetSF[SKILL_EXP_ID];
        bltfx.flags = 0;
        bltfx.alpha = 0;

        sprite_blt(Bitmaps[BITMAP_SKILL_EXP_BG], 0, 0, NULL, &bltfx);

        StringBlt(widgetSF[SKILL_EXP_ID], &Font6x3Out, "Used", 4, -1, COLOR_HGOLD, NULL, NULL);
        StringBlt(widgetSF[SKILL_EXP_ID], &Font6x3Out, "Skill", 4, 7, COLOR_HGOLD, NULL, NULL);

        if (cpl.skill_name[0] != 0)
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
                        snprintf(buf, sizeof(buf), "%s - level: %d", cpl.skill_name, skill_list[cpl.skill_g].entry[cpl.skill_e].exp_level);
                    else
                        snprintf(buf, sizeof(buf), "%s - level: **", cpl.skill_name);
					break;
            }

            StringBlt(widgetSF[SKILL_EXP_ID], &SystemFont, buf, 28, 0, COLOR_WHITE, NULL, NULL);

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
                    if(skill_list[cpl.skill_g].entry[cpl.skill_e].exp >=0)
                        snprintf(buf, sizeof(buf), "%d / %-9d", skill_list[cpl.skill_g].entry[cpl.skill_e].exp_level,skill_list[cpl.skill_g].entry[cpl.skill_e].exp);
                    else if(skill_list[cpl.skill_g].entry[cpl.skill_e].exp == -2)
                        snprintf(buf, sizeof(buf), "%d / **", skill_list[cpl.skill_g].entry[cpl.skill_e].exp_level);
                    else
                        snprintf(buf, sizeof(buf), "** / **");
					break;

                /* LExp% */
                case 1:
                    if (skill_list[cpl.skill_g].entry[cpl.skill_e].exp >= 0)
                        snprintf(buf, sizeof(buf), "%#05.2f%%", fLExpPercent);
                    else
                        snprintf(buf, sizeof(buf), "**.**%%");
					break;

                /* LExp/LExp tnl */
                case 2:
                    if (skill_list[cpl.skill_g].entry[cpl.skill_e].exp >= 0)
                        snprintf(buf, sizeof(buf), "%ld / %ld", liLExp, liLExpTNL);
                    else
                        snprintf(buf, sizeof(buf), "** / **");
					break;

                /* TExp/TExp tnl */
                case 3:
                    if (skill_list[cpl.skill_g].entry[cpl.skill_e].exp >= 0)
                        snprintf(buf, sizeof(buf), "%ld / %ld", liTExp, liTExpTNL);
                    else
                        snprintf(buf, sizeof(buf), "** / **");
					break;

                /* (LExp%) LExp/LExp tnl */
                case 4:
                    if (skill_list[cpl.skill_g].entry[cpl.skill_e].exp >= 0)
                        snprintf(buf, sizeof(buf), "%#05.2f%% - %ld", fLExpPercent, liLExpTNL - liLExp);
                    else
                        snprintf(buf, sizeof(buf), "(**.**%%) **");
					break;
            }

            if (skill_list[cpl.skill_g].entry[cpl.skill_e].exp_level == MAX_LEVEL)
                snprintf(buf, sizeof(buf), "Maximum level reached");

            StringBlt(widgetSF[SKILL_EXP_ID], &SystemFont, buf, 28, 9, COLOR_WHITE, NULL, NULL);

            snprintf(buf, sizeof(buf), "%1.2f sec", cpl.action_timer);
            StringBlt(widgetSF[SKILL_EXP_ID], &SystemFont, buf, 160, 0, COLOR_WHITE, NULL, NULL);
        }

        sprite_blt(Bitmaps[BITMAP_EXP_SKILL_BORDER], 143, 11, NULL, &bltfx);

        if (multi)
        {
            box.x = 0;
            box.y = 0;
            box.h = Bitmaps[BITMAP_EXP_SKILL_LINE]->bitmap->h;
            box.w = (int) (Bitmaps[BITMAP_EXP_SKILL_LINE]->bitmap->w * multi);

            if (!box.w)
                box.w = 1;

            if (box.w > Bitmaps[BITMAP_EXP_SKILL_LINE]->bitmap->w)
                box.w = Bitmaps[BITMAP_EXP_SKILL_LINE]->bitmap->w;

            sprite_blt(Bitmaps[BITMAP_EXP_SKILL_LINE], 146, 18, &box, &bltfx);
        }

        if (line > 0)
        {
            for (s = 0; s < (int) line; s++)
				sprite_blt(Bitmaps[BITMAP_EXP_SKILL_BUBBLE], 146 + s * 5, 13, NULL, &bltfx);
        }
    }

    box.x = x;
    box.y = y;
    SDL_BlitSurface(widgetSF[SKILL_EXP_ID], NULL, ScreenSurface, &box);
}

void widget_skill_exp_event()
{
    int i, ii, j, jj, bFound = 0;

    /* Let's find the skill... and setup the shortcuts to the exp values */
    for (ii = 0; ii <= SKILL_LIST_MAX && (!bFound); ii++)
    {
        jj = cpl.skill_g + ii;
        if (jj >= SKILL_LIST_MAX)
            jj -= SKILL_LIST_MAX;

        for (i = 0; i < DIALOG_LIST_ENTRY && (!bFound); i++)
        {
            /* First page, we have to be offset (and break before looping) */
            if (ii == 0)
            {
                j = cpl.skill_e + i + 1;
                if (j >= DIALOG_LIST_ENTRY)
                    break;
            }
            /* Other pages we look through MUST NOT BE OFFSET */
            else
                j = i;

            if (j >= DIALOG_LIST_ENTRY)
                j -= DIALOG_LIST_ENTRY;

            /* We have a list entry */
            if (skill_list[jj].entry[j].flag == LIST_ENTRY_KNOWN)
            {
                /* First one we find is the one we want */
                snprintf(cpl.skill_name, sizeof(cpl.skill_name), "%s", skill_list[jj].entry[j].name);
                cpl.skill_g = jj;
                cpl.skill_e = j;
                bFound = 1;
                break;
            }
        }
    }

    WIDGET_REDRAW(SKILL_EXP_ID);
}

void widget_show_regeneration(int x, int y)
{
    char buf[256];
    SDL_Rect box;
	_BLTFX bltfx;

    if (!widgetSF[REGEN_ID])
        widgetSF[REGEN_ID] = SDL_ConvertSurface(Bitmaps[BITMAP_REGEN_BG]->bitmap, Bitmaps[BITMAP_REGEN_BG]->bitmap->format, Bitmaps[BITMAP_REGEN_BG]->bitmap->flags);

    if (cur_widget[REGEN_ID].redraw)
    {
        cur_widget[REGEN_ID].redraw = 0;

		bltfx.surface = widgetSF[REGEN_ID];
        bltfx.flags = 0;
        bltfx.alpha = 0;

        sprite_blt(Bitmaps[BITMAP_REGEN_BG], 0, 0, NULL, &bltfx);

        x -= 173;

        StringBlt(widgetSF[REGEN_ID], &Font6x3Out, "Regeneration", 4, 1, COLOR_HGOLD, NULL, NULL);

		/* Health */
        StringBlt(widgetSF[REGEN_ID], &SystemFont, "HP", 61, 13, COLOR_HGOLD, NULL, NULL);
        snprintf(buf, sizeof(buf), "%2.1f", cpl.gen_hp);
        StringBlt(widgetSF[REGEN_ID], &SystemFont, buf, 75, 13, COLOR_WHITE, NULL, NULL);

		/* Mana */
        StringBlt(widgetSF[REGEN_ID], &SystemFont, "Mana", 5, 13, COLOR_HGOLD, NULL, NULL);
		snprintf(buf, sizeof(buf), "%2.1f", cpl.gen_sp);
        StringBlt(widgetSF[REGEN_ID], &SystemFont, buf, 35, 13, COLOR_WHITE, NULL, NULL);

		/* Grace */
        StringBlt(widgetSF[REGEN_ID], &SystemFont, "Grace", 5, 24, COLOR_HGOLD, NULL, NULL);
        snprintf(buf, sizeof(buf), "%2.1f", cpl.gen_grace);
        StringBlt(widgetSF[REGEN_ID], &SystemFont, buf, 35, 24, COLOR_WHITE, NULL, NULL);
    }

    box.x = x;
    box.y = y;
    SDL_BlitSurface(widgetSF[REGEN_ID], NULL, ScreenSurface, &box);
}
