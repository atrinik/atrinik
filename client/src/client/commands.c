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
 * Handles commands received by the server.  This does not necessarily
 * handle all commands - some might be in other files (like main.c)
 *
 * This file handles commans from the server->client.  See player.c
 * for client->server commands.
 *
 * this file contains most of the commands for the dispatch loop most of
 * the functions are self-explanatory, the pixmap/bitmap commands receive
 * the picture, and display it.  The drawinfo command draws a string
 * in the info window, the stats command updates the local copy of the stats
 * and displays it. handle_query prompts the user for input.
 * send_reply sends off the reply for the input.
 * player command gets the player information.
 * MapScroll scrolls the map on the client by some amount
 * MapCmd displays the map either with layer packing or stack packing.
 *   packing/unpacking is best understood by looking at the server code
 *   (server/ericserver.c)
 *   stack packing is easy, for every map entry that changed, we pack
 *   1 byte for the x/y location, 1 byte for the count, and 2 bytes per
 *   face in the stack.
 *   layer packing is harder, but I seem to remember more efficient:
 *   first we pack in a list of all map cells that changed and are now
 *   empty.  The end of this list is a 255, which is bigger that 121, the
 *   maximum packed map location.
 *   For each changed location we also pack in a list of all the faces and
 *   X/Y coordinates by layer, where the layer is the depth in the map.
 *   This essentially takes slices through the map rather than stacks.
 *   Then for each layer, (max is MAXMAPCELLFACES, a bad name) we start
 *   packing the layer into the message.  First we pack in a face, then
 *   for each place on the layer with the same face, we pack in the x/y
 *   location.  We mark the last x/y location with the high bit on
 *   (11*11 = 121 < 128).  We then continue on with the next face, which
 *   is why the code marks the faces as -1 if they are finished.  Finally
 *   we mark the last face in the layer again with the high bit, clearly
 *   limiting the total number of faces to 32767, the code comments it's
 *   16384, I'm not clear why, but the second bit may be used somewhere
 *   else as well.
 *   The unpacking routines basically perform the opposite operations. */

#include <global.h>

/**
 * Book command, used to initialize the book interface.
 * @param data Data of the book
 * @param len Length of the data */
void BookCmd(unsigned char *data, int len)
{
	sound_play_effect("book.ogg", 100);
	book_load((char *) data, len);
}

/**
 * Setup command. Used to set up a new server connection, initialize
 * necessary data, etc.
 * @param buf The incoming data.
 * @param len Length of data. */
void SetupCmd(char *buf, int len)
{
	int s;
	char *cmd, *param;

	server_files_clear_update();
	LOG(llevInfo, "Get SetupCmd:: %s\n", buf);

	for (s = 0; ;)
	{
		while (s < len && buf[s] == ' ')
		{
			s++;
		}

		if (s >= len)
		{
			break;
		}

		cmd = &buf[s];

		while (s < len && buf[s] != ' ')
		{
			s++;
		}

		if (s >= len)
		{
			break;
		}

		buf[s++] = '\0';

		if (s >= len)
		{
			break;
		}

		while (s < len && buf[s] == ' ')
		{
			s++;
		}

		if (s >= len)
		{
			break;
		}

		param = &buf[s];

		while (s < len && buf[s] != ' ')
		{
			s++;
		}

		buf[s++] = '\0';

		while (s < len && buf[s] == ' ')
		{
			s++;
		}

		if (!strcmp(cmd, "sound"))
		{
			if (!strcmp(param, "FALSE"))
			{
			}
		}
		else if (!strcmp(cmd, "mapsize"))
		{
		}
		else if (!strcmp(cmd, "map2cmd"))
		{
		}
		else if (!strcmp(cmd, "darkness"))
		{
		}
		else if (!strcmp(cmd, "facecache"))
		{
		}
		else if (server_files_parse_setup(cmd, param))
		{
		}
		else
		{
			LOG(llevBug, "Got setup for a command we don't understand: %s %s\n", cmd, param);
		}
	}

	if (GameStatus != GAME_STATUS_PLAY)
	{
		GameStatus = GAME_STATUS_REQUEST_FILES;
	}
}

/**
 * Handles when the server says we can't be added.  In reality, we need to
 * close the connection and quit out, because the client is going to close
 * us down anyways. */
void AddMeFail(unsigned char *data, int len)
{
	(void) data;
	(void) len;

	LOG(llevInfo, "addme_failed received.\n");
	GameStatus = GAME_STATUS_START;
}

/**
 * This is really a throwaway command - there really isn't any reason to
 * send addme_success commands. */
void AddMeSuccess(unsigned char *data, int len)
{
	(void) data;
	(void) len;

	LOG(llevInfo, "addme_success received.\n");
}

/**
 * Animation command.
 * @param data The incoming data
 * @param len Length of the data */
void AnimCmd(unsigned char *data, int len)
{
	short anum;
	int i, j;

	anum = GetShort_String(data);

	if (anum < 0)
	{
		fprintf(stderr, "AnimCmd: animation number invalid: %d\n", anum);
		return;
	}

	animations[anum].flags = *(data + 2);
	animations[anum].facings = *(data + 3);
	animations[anum].num_animations = (len - 4) / 2;

	if (animations[anum].num_animations < 1)
	{
		LOG(llevDebug, "AnimCmd: num animations invalid: %d\n", animations[anum].num_animations);
		return;
	}

	if (animations[anum].facings > 1)
	{
		animations[anum].frame = animations[anum].num_animations / animations[anum].facings;
	}
	else
	{
		animations[anum].frame = animations[anum].num_animations;
	}

	animations[anum].faces = malloc(sizeof(uint16) * animations[anum].num_animations);

	for (i = 4, j = 0; i < len; i += 2, j++)
	{
		animations[anum].faces[j] = GetShort_String(data + i);
		request_face(animations[anum].faces[j]);
	}

	if (j != animations[anum].num_animations)
	{
		LOG(llevDebug, "Calculated animations does not equal stored animations? (%d != %d)\n", j, animations[anum].num_animations);
	}
}

/**
 * Image command.
 * @param data The incoming data
 * @param len Length of the data */
void ImageCmd(unsigned char *data, int len)
{
	int pnum, plen;
	char buf[2048];
	FILE *stream;

	pnum = GetInt_String(data);
	plen = GetInt_String(data + 4);

	if (len < 8 || (len - 8) != plen)
	{
		LOG(llevBug, "ImageCmd(): Lengths don't compare (%d, %d)\n", (len - 8), plen);
		return;
	}

	/* Save picture to cache and load it to FaceList */
	sprintf(buf, DIRECTORY_CACHE"/%s", FaceList[pnum].name);
	LOG(llevInfo, "ImageFromServer: %s\n", FaceList[pnum].name);

	if ((stream = fopen_wrapper(buf, "wb+")) != NULL)
	{
		fwrite((char *) data + 8, 1, plen, stream);
		fclose(stream);
	}

	FaceList[pnum].sprite = sprite_tryload_file(buf, 0, NULL);
	map_udate_flag = 2;
	map_redraw_flag = 1;
	book_redraw();
	interface_redraw();
}

/**
 * Ready command.
 * @param data The incoming data
 * @param len Length of the data */
void SkillRdyCmd(char *data, int len)
{
	size_t type, id;

	(void) len;

	strncpy(cpl.skill_name, data, sizeof(cpl.skill_name) - 1);
	cpl.skill_name[sizeof(cpl.skill_name) - 1] = '\0';
	cpl.skill = NULL;

	if (skill_find(cpl.skill_name, &type, &id))
	{
		skill_entry_struct *skill = skill_get(type, id);

		if (skill->known)
		{
			cpl.skill = skill;
		}
	}

	WIDGET_REDRAW_ALL(SKILL_EXP_ID);
}

/**
 * Draw info command. Used to draw text from the server.
 * @param data The text to output. */
void DrawInfoCmd(unsigned char *data)
{
	char color[COLOR_BUF];
	int pos = 0;

	GetString_String(data, &pos, color, sizeof(color));
	draw_info(color, (char *) data + pos);
}

/**
 * New draw info command. Used to draw text from the server with various
 * flags, like color.
 * @param data The incoming data
 * @param len Length of the data */
void DrawInfoCmd2(unsigned char *data, int len)
{
	int flags;
	char color[COLOR_BUF], buf[20048], *tmp = NULL;
	int pos = 0;

	flags = (int) GetShort_String(data);
	pos += 2;

	GetString_String(data, &pos, color, sizeof(color));
	len -= pos;

	data += pos;

	if (len >= 0)
	{
		if (len > 20000)
		{
			len = 20000;
		}

		if (setting_get_int(OPT_CAT_GENERAL, OPT_CHAT_TIMESTAMPS) && (flags & NDI_PLAYER))
		{
			time_t now = time(NULL);
			char timebuf[32], *format;
			struct tm *tm = localtime(&now);
			size_t timelen;

			switch (setting_get_int(OPT_CAT_GENERAL, OPT_CHAT_TIMESTAMPS))
			{
				/* HH:MM */
				case 1:
				default:
					format = "%H:%M";
					break;

				/* HH:MM:SS */
				case 2:
					format = "%H:%M:%S";
					break;

				/* H:MM AM/PM */
				case 3:
					format = "%I:%M %p";
					break;

				/* H:MM:SS AM/PM */
				case 4:
					format = "%I:%M:%S %p";
					break;
			}

			timelen = strftime(timebuf, sizeof(timebuf), format, tm);

			if (timelen == 0)
			{
				strncpy(buf, (char *) data, len);
			}
			else
			{
				len += (int) timelen + 4;
				snprintf(buf, len, "[%s] %s", timebuf, (char *) data);
			}
		}
		else
		{
			strncpy(buf, (char *) data, len);
		}

		buf[len] = '\0';
	}
	else
	{
		buf[0] = '\0';
	}

	if (buf[0] && (flags & (NDI_PLAYER | NDI_SAY | NDI_SHOUT | NDI_TELL | NDI_EMOTE)))
	{
		tmp = strchr((char *) data, ' ');

		if (tmp)
		{
			*tmp = '\0';
		}
	}

	/* We have communication input */
	if (tmp)
	{
		if ((flags & NDI_SAY) && ignore_check((char *) data, "say"))
		{
			return;
		}

		if ((flags & NDI_SHOUT) && ignore_check((char *) data, "shout"))
		{
			return;
		}

		if ((flags & NDI_TELL) && ignore_check((char *) data, "tell"))
		{
			return;
		}

		if ((flags & NDI_EMOTE) && ignore_check((char *) data, "emote"))
		{
			return;
		}

		/* Save last incoming tell for client-sided /reply */
		if (flags & NDI_TELL)
		{
			strncpy(cpl.player_reply, (char *) data, sizeof(cpl.player_reply));
		}
	}

	if (flags & NDI_ANIM)
	{
		strncpy(msg_anim.message, buf, sizeof(msg_anim.message) - 1);
		msg_anim.tick = LastTick;
		strncpy(msg_anim.color, color, sizeof(msg_anim.color) - 1);
		msg_anim.color[sizeof(msg_anim.color) - 1] = '\0';
	}

	draw_info_flags(color, flags, buf);
}

/**
 * Target object command.
 * @param data The incoming data
 * @param len Length of the data */
void TargetObject(unsigned char *data, int len)
{
	int pos = 0;

	(void) len;

	cpl.target_mode = data[pos++];

	if (cpl.target_mode)
	{
		sound_play_effect("weapon_attack.ogg", 100);
	}
	else
	{
		sound_play_effect("weapon_hold.ogg", 100);
	}

	cpl.target_code = data[pos++];
	GetString_String(data, &pos, cpl.target_color, sizeof(cpl.target_color));
	GetString_String(data, &pos, cpl.target_name, sizeof(cpl.target_name));

	map_udate_flag = 2;
	map_redraw_flag = 1;
}

/**
 * Stats command. Used to update various things, like target's HP, mana regen, protections, etc
 * @param data The incoming data
 * @param len Length of the data */
void StatsCmd(unsigned char *data, int len)
{
	int i = 0;
	int c, temp;

	while (i < len)
	{
		c = data[i++];

		if (c >= CS_STAT_PROT_START && c <= CS_STAT_PROT_END)
		{
			cpl.stats.protection[c - CS_STAT_PROT_START] = (sint16) *(((signed char *) data) + i++);
			WIDGET_REDRAW_ALL(RESIST_ID);
		}
		else
		{
			switch (c)
			{
				case CS_STAT_TARGET_HP:
					cpl.target_hp = (int)*(data + i++);
					break;

				case CS_STAT_REG_HP:
					cpl.gen_hp = abs(GetShort_String(data + i)) / 10.0f;
					i += 2;
					WIDGET_REDRAW_ALL(REGEN_ID);
					break;

				case CS_STAT_REG_MANA:
					cpl.gen_sp = abs(GetShort_String(data + i)) / 10.0f;
					i += 2;
					WIDGET_REDRAW_ALL(REGEN_ID);
					break;

				case CS_STAT_REG_GRACE:
					cpl.gen_grace = abs(GetShort_String(data + i)) / 10.0f;
					i += 2;
					WIDGET_REDRAW_ALL(REGEN_ID);
					break;

				case CS_STAT_HP:
					temp = GetInt_String(data + i);

					if (temp < cpl.stats.hp && cpl.stats.food)
					{
						cpl.warn_hp = 1;

						if (cpl.stats.maxhp / 12 <= cpl.stats.hp - temp)
						{
							cpl.warn_hp = 2;
						}
					}

					cpl.stats.hp = temp;
					i += 4;
					WIDGET_REDRAW_ALL(STATS_ID);
					break;

				case CS_STAT_MAXHP:
					cpl.stats.maxhp = GetInt_String(data + i);
					i += 4;
					WIDGET_REDRAW_ALL(STATS_ID);
					break;

				case CS_STAT_SP:
					cpl.stats.sp = GetShort_String(data + i);
					i += 2;
					WIDGET_REDRAW_ALL(STATS_ID);
					break;

				case CS_STAT_MAXSP:
					cpl.stats.maxsp = GetShort_String(data + i);
					i += 2;
					WIDGET_REDRAW_ALL(STATS_ID);
					break;

				case CS_STAT_GRACE:
					cpl.stats.grace = GetShort_String(data + i);
					i += 2;
					WIDGET_REDRAW_ALL(STATS_ID);
					break;

				case CS_STAT_MAXGRACE:
					cpl.stats.maxgrace = GetShort_String(data + i);
					i += 2;
					WIDGET_REDRAW_ALL(STATS_ID);
					break;

				case CS_STAT_STR:
					temp = (int) *(data + i++);

					if (temp > cpl.stats.Str)
					{
						cpl.warn_statup = 1;
					}
					else
					{
						cpl.warn_statdown = 1;
					}

					cpl.stats.Str = temp;
					WIDGET_REDRAW_ALL(STATS_ID);
					break;

				case CS_STAT_INT:
					temp = (int) *(data + i++);

					if (temp > cpl.stats.Int)
					{
						cpl.warn_statup = 1;
					}
					else
					{
						cpl.warn_statdown = 1;
					}

					cpl.stats.Int = temp;
					WIDGET_REDRAW_ALL(STATS_ID);
					break;

				case CS_STAT_POW:
					temp = (int) *(data + i++);

					if (temp > cpl.stats.Pow)
					{
						cpl.warn_statup = 1;
					}
					else
					{
						cpl.warn_statdown = 1;
					}

					cpl.stats.Pow = temp;
					WIDGET_REDRAW_ALL(STATS_ID);
					break;

				case CS_STAT_WIS:
					temp = (int) *(data + i++);

					if (temp > cpl.stats.Wis)
					{
						cpl.warn_statup = 1;
					}
					else
					{
						cpl.warn_statdown = 1;
					}

					cpl.stats.Wis = temp;
					WIDGET_REDRAW_ALL(STATS_ID);
					break;

				case CS_STAT_DEX:
					temp = (int) *(data + i++);

					if (temp > cpl.stats.Dex)
					{
						cpl.warn_statup = 1;
					}
					else
					{
						cpl.warn_statdown = 1;
					}

					cpl.stats.Dex = temp;
					WIDGET_REDRAW_ALL(STATS_ID);
					break;

				case CS_STAT_CON:
					temp = (int) *(data + i++);

					if (temp > cpl.stats.Con)
					{
						cpl.warn_statup = 1;
					}
					else
					{
						cpl.warn_statdown = 1;
					}

					cpl.stats.Con = temp;
					WIDGET_REDRAW_ALL(STATS_ID);
					break;

				case CS_STAT_CHA:
					temp = (int) *(data + i++);

					if (temp > cpl.stats.Cha)
					{
						cpl.warn_statup = 1;
					}
					else
					{
						cpl.warn_statdown = 1;
					}

					cpl.stats.Cha = temp;
					WIDGET_REDRAW_ALL(STATS_ID);
					break;

				case CS_STAT_EXP:
					cpl.stats.exp = GetInt64_String(data + i);
					i += 8;
					WIDGET_REDRAW_ALL(MAIN_LVL_ID);
					break;

				case CS_STAT_LEVEL:
					cpl.stats.level = (char) *(data + i++);
					WIDGET_REDRAW_ALL(MAIN_LVL_ID);
					break;

				case CS_STAT_WC:
					cpl.stats.wc = GetShort_String(data + i);
					i += 2;
					break;

				case CS_STAT_AC:
					cpl.stats.ac = GetShort_String(data + i);
					i += 2;
					break;

				case CS_STAT_DAM:
					cpl.stats.dam = GetShort_String(data + i);
					i += 2;
					break;

				case CS_STAT_SPEED:
					cpl.stats.speed = GetInt_String(data + i);
					i += 4;
					break;

				case CS_STAT_FOOD:
					cpl.stats.food = GetShort_String(data + i);
					i += 2;
					WIDGET_REDRAW_ALL(STATS_ID);
					break;

				case CS_STAT_WEAP_SP:
					cpl.stats.weapon_sp = (int) *(data + i++);
					break;

				case CS_STAT_FLAGS:
					cpl.stats.flags = GetShort_String(data + i);
					i += 2;
					break;

				case CS_STAT_WEIGHT_LIM:
					set_weight_limit(GetInt_String(data + i));
					i += 4;
					break;

				case CS_STAT_ACTION_TIME:
					cpl.action_timer = abs(GetInt_String(data + i)) / 1000.0f;
					i += 4;
					WIDGET_REDRAW_ALL(SKILL_EXP_ID);
					break;

				case CS_STAT_SKILLEXP_AGILITY:
				case CS_STAT_SKILLEXP_PERSONAL:
				case CS_STAT_SKILLEXP_MENTAL:
				case CS_STAT_SKILLEXP_PHYSIQUE:
				case CS_STAT_SKILLEXP_MAGIC:
				case CS_STAT_SKILLEXP_WISDOM:
					cpl.stats.skill_exp[(c - CS_STAT_SKILLEXP_START) / 2] = GetInt64_String(data + i);
					i += 8;
					WIDGET_REDRAW_ALL(SKILL_LVL_ID);
					break;

				case CS_STAT_SKILLEXP_AGLEVEL:
				case CS_STAT_SKILLEXP_PELEVEL:
				case CS_STAT_SKILLEXP_MELEVEL:
				case CS_STAT_SKILLEXP_PHLEVEL:
				case CS_STAT_SKILLEXP_MALEVEL:
				case CS_STAT_SKILLEXP_WILEVEL:
					cpl.stats.skill_level[(c - CS_STAT_SKILLEXP_START - 1) / 2] = (sint16)*(data + i++);
					WIDGET_REDRAW_ALL(SKILL_LVL_ID);
					break;

				case CS_STAT_EXT_TITLE:
				{
					int rlen = data[i++];

					strncpy(cpl.ext_title, (const char *) data + i, rlen);
					cpl.ext_title[rlen] = '\0';
					i += rlen;

					if (strstr(cpl.ext_title, "[WIZ]"))
					{
						cpl.dm = 1;
					}
					else
					{
						cpl.dm = 0;
					}

					break;
				}

				case CS_STAT_RANGED_DAM:
					cpl.stats.ranged_dam = GetShort_String(data + i);
					i += 2;
					break;

				case CS_STAT_RANGED_WC:
					cpl.stats.ranged_wc = GetShort_String(data + i);
					i += 2;
					break;

				case CS_STAT_RANGED_WS:
					cpl.stats.ranged_ws = GetInt_String(data + i);
					i += 4;
					break;

				default:
					fprintf(stderr, "Unknown stat number %d\n", c);
			}
		}
	}

	if (i > len)
	{
		fprintf(stderr, "Got stats overflow, processed %d bytes out of %d\n", i, len);
	}
}

/**
 * Used by handle_query to open console for questions like name, password, etc.
 * @param cmd The question command. */
void PreParseInfoStat(char *cmd)
{
	/* Find input name */
	if (strstr(cmd, "What is your name?"))
	{
		LOG(llevInfo, "Login: Enter name\n");
		cpl.name[0] = '\0';
		cpl.password[0] = '\0';
		GameStatus = GAME_STATUS_NAME;
	}

	if (strstr(cmd, "What is your password?"))
	{
		LOG(llevInfo, "Login: Enter password\n");
		GameStatus = GAME_STATUS_PSWD;
	}

	if (strstr(cmd, "Please type your password again."))
	{
		LOG(llevInfo, "Login: Enter verify password\n");
		GameStatus = GAME_STATUS_VERIFYPSWD;
	}

	if (GameStatus >= GAME_STATUS_NAME && GameStatus <= GAME_STATUS_VERIFYPSWD)
	{
		text_input_open(64);
	}
}

/**
 * Handle server query question.
 * @param data The incoming data */
void handle_query(char *data)
{
	char *buf, *cp;

	buf = strchr(data, ' ');

	if (buf)
	{
		buf++;
	}

	if (buf)
	{
		cp = buf;

		while ((buf = strchr(buf, '\n')) != NULL)
		{
			*buf++ = '\0';
			LOG(llevInfo, "Received query string: %s\n", cp);
			PreParseInfoStat(cp);
			cp = buf;
		}
	}
}

/**
 * Sends a reply to the server.
 * @param text Null terminated string of text to send. */
void send_reply(char *text)
{
	char buf[HUGE_BUF];

	snprintf(buf, sizeof(buf), "reply %s", text);
	cs_write_string(buf, strlen(buf));
}

/**
 * This function copies relevant data from the archetype to the object.
 * Only copies data that was not set in the object structure.
 * @param data The incoming data
 * @param len Length of the data */
void PlayerCmd(unsigned char *data, int len)
{
	int tag, weight, face, pos = 0;

	(void) len;

	GameStatus = GAME_STATUS_PLAY;
	text_input_string_end_flag = 0;
	tag = GetInt_String(data);
	pos += 4;
	weight = GetInt_String(data + pos);
	pos += 4;
	face = GetInt_String(data + pos);
	request_face(face);
	pos += 4;

	new_player(tag, weight, (short) face);
	map_udate_flag = 2;
	map_redraw_flag = 1;

	ignore_list_load();
}

/**
 * ItemX command.
 * @param data The incoming data
 * @param len Length of the data */
void ItemXCmd(unsigned char *data, int len)
{
	int weight, loc, tag, face, flags, pos = 0, nlen, anim, nrof, dmode;
	uint8 itype, stype, item_qua, item_con, item_skill, item_level;
	uint8 animspeed, direction = 0;
	char name[MAX_BUF];

	map_udate_flag = 2;
	itype = stype = item_qua = item_con = item_skill = item_level = 0;

	dmode = GetInt_String(data);
	pos += 4;

	loc = GetInt_String(data+pos);

	if (dmode >= 0)
	{
		object_remove_inventory(object_find(loc));
	}

	/* send item flag */
	if (dmode == -4)
	{
		/* and redirect it to our invisible sack */
		if (loc == cpl.container_tag)
		{
			loc = -1;
		}
	}
	/* container flag! */
	else if (dmode == -1)
	{
		/* we catch the REAL container tag */
		cpl.container_tag = loc;
		object_remove_inventory(object_find(-1));

		/* if this happens, we want to close the container */
		if (loc == -1)
		{
			cpl.container_tag = -998;
			return;
		}

		/* and redirect it to our invisible sack */
		loc = -1;
	}

	pos += 4;

	if (pos == len && loc != -1)
	{
		LOG(llevBug, "ItemXCmd(): Got location with no other data\n");
	}
	else
	{
		while (pos < len)
		{
			tag = GetInt_String(data + pos);
			pos += 4;
			flags = GetInt_String(data + pos);
			pos += 4;
			weight = GetInt_String(data + pos);
			pos += 4;
			face = GetInt_String(data + pos);
			pos += 4;
			request_face(face);
			direction = data[pos++];

			if (loc)
			{
				itype = data[pos++];
				stype = data[pos++];
				item_qua = data[pos++];
				item_con = data[pos++];
				item_level = data[pos++];
				item_skill = data[pos++];
			}

			nlen = data[pos++];
			memcpy(name, (char *) data + pos, nlen);
			pos += nlen;
			name[nlen] = '\0';
			anim = GetShort_String(data + pos);
			pos += 2;
			animspeed = data[pos++];
			nrof = GetInt_String(data + pos);
			pos += 4;
			update_object(tag, loc, name, weight, face, flags, anim, animspeed, nrof, itype, stype, item_qua, item_con, item_skill, item_level, direction, 0);
		}

		if (pos > len)
		{
			LOG(llevBug, "ItemXCmd(): Overread buffer: %d > %d\n", pos, len);
		}
	}

	map_udate_flag = 2;
}

/**
 * ItemY command.
 * @param data The incoming data
 * @param len Length of the data */
void ItemYCmd(unsigned char *data, int len)
{
	int weight, loc, tag, face, flags, pos = 0, nlen, anim, nrof, dmode;
	uint8 itype, stype, item_qua, item_con, item_skill, item_level;
	uint8 animspeed, direction = 0;
	char name[MAX_BUF];

	map_udate_flag = 2;
	itype = stype = item_qua = item_con = item_skill = item_level = 0;

	dmode = GetInt_String(data);
	pos += 4;

	loc = GetInt_String(data + pos);

	if (dmode >= 0)
	{
		object_remove_inventory(object_find(loc));
	}

	/* send item flag */
	if (dmode == -4)
	{
		/* and redirect it to our invisible sack */
		if (loc == cpl.container_tag)
		{
			loc = -1;
		}
	}
	/* container flag! */
	else if (dmode == -1)
	{
		/* we catch the REAL container tag */
		cpl.container_tag = loc;
		object_remove_inventory(object_find(-1));

		/* if this happens, we want to close the container */
		if (loc == -1)
		{
			cpl.container_tag = -998;
			return;
		}

		/* and redirect it to our invisible sack */
		loc = -1;
	}


	pos += 4;

	if (pos == len && loc != -1)
	{
		/* server sends no clean command to clear below window */
	}
	else
	{
		while (pos < len)
		{
			tag = GetInt_String(data + pos);
			pos += 4;
			flags = GetInt_String(data + pos);
			pos += 4;
			weight = GetInt_String(data + pos);
			pos += 4;
			face = GetInt_String(data + pos);
			pos += 4;
			request_face(face);
			direction = data[pos++];

			if (loc)
			{
				itype = data[pos++];
				stype = data[pos++];
				item_qua = data[pos++];
				item_con = data[pos++];
				item_level = data[pos++];
				item_skill = data[pos++];
			}

			nlen = data[pos++];
			memcpy(name, (char *) data + pos, nlen);
			pos += nlen;
			name[nlen] = '\0';
			anim = GetShort_String(data + pos);
			pos += 2;
			animspeed = data[pos++];
			nrof = GetInt_String(data + pos);
			pos += 4;
			update_object(tag, loc, name, weight, face, flags, anim, animspeed, nrof, itype, stype, item_qua, item_con, item_skill, item_level, direction, 1);
		}

		if (pos > len)
		{
			LOG(llevBug, "ItemYCmd(): Overread buffer: %d > %d\n", pos, len);
		}
	}

	map_udate_flag = 2;
}

/**
 * Update item command. Updates some attributes of an item.
 * @param data The incoming data
 * @param len Length of the data */
void UpdateItemCmd(unsigned char *data, int len)
{
	int weight, loc, tag, face, sendflags, flags, pos = 0, nlen, anim, nrof;
	uint8 direction;
	char name[MAX_BUF];
	object *ip, *env = NULL;
	uint8 animspeed;

	map_udate_flag = 2;
	sendflags = GetShort_String(data);
	pos += 2;
	tag = GetInt_String(data + pos);
	pos += 4;
	ip = object_find(tag);

	if (!ip)
	{
		return;
	}

	*name = '\0';
	loc = ip->env ? ip->env->tag : 0;
	weight = (int) (ip->weight * 1000);
	face = ip->face;
	request_face(face);
	flags = ip->flags;
	anim = ip->animation_id;
	animspeed = (uint8) ip->anim_speed;
	nrof = ip->nrof;
	direction = ip->direction;

	if (sendflags & UPD_LOCATION)
	{
		loc = GetInt_String(data + pos);
		env = object_find(loc);

		if (!env)
		{
			fprintf(stderr, "UpdateItemCmd: unknown object tag (%d) for new location\n", loc);
		}

		pos += 4;
	}

	if (sendflags & UPD_FLAGS)
	{
		flags = GetInt_String(data + pos);
		pos += 4;
	}

	if (sendflags & UPD_WEIGHT)
	{
		weight = GetInt_String(data + pos);
		pos += 4;
	}

	if (sendflags & UPD_FACE)
	{
		face = GetInt_String(data + pos);
		request_face(face);
		pos += 4;
	}

	if (sendflags & UPD_DIRECTION)
	{
		direction = data[pos++];
	}

	if (sendflags & UPD_NAME)
	{
		nlen = data[pos++];
		memcpy(name, (char *) data + pos, nlen);
		pos += nlen;
		name[nlen] = '\0';
	}

	if (pos > len)
	{
		fprintf(stderr, "UpdateItemCmd: Overread buffer: %d > %d\n", pos, len);
		return;
	}

	if (sendflags & UPD_ANIM)
	{
		anim = GetShort_String(data + pos);
		pos += 2;
	}

	if (sendflags & UPD_ANIMSPEED)
	{
		animspeed = data[pos++];
	}

	if (sendflags & UPD_NROF)
	{
		nrof = GetInt_String(data + pos);
		pos += 4;
	}

	update_object(tag, loc, name, weight, face, flags, anim, animspeed, nrof, 254, 254, 254, 254, 254, 254, direction, 0);
	map_udate_flag = 2;
}

/**
 * Delete item command.
 * @param data The incoming data
 * @param len Length of the data */
void DeleteItem(unsigned char *data, int len)
{
	int pos = 0,tag;

	while (pos < len)
	{
		tag = GetInt_String(data);
		pos += 4;
		delete_object(tag);
	}

	if (pos > len)
	{
		fprintf(stderr, "ItemCmd: Overread buffer: %d > %d\n", pos, len);
	}

	map_udate_flag = 2;
}

/**
 * Delete inventory command.
 * @param data The incoming data */
void DeleteInventory(unsigned char *data)
{
	int tag;

	tag = atoi((const char *) data);

	if (tag < 0)
	{
		fprintf(stderr, "DeleteInventory: Invalid tag: %d\n", tag);
		return;
	}

	object_remove_inventory(object_find(tag));
	map_udate_flag = 2;
}

/**
 * Plays the footstep sounds when moving on the map. */
static void map_play_footstep(void)
{
	static int step = 0;
	static uint32 tick = 0;

	if (LastTick - tick > 125)
	{
		step++;

		if (step % 2)
		{
			sound_play_effect("step1.ogg", 100);
		}
		else
		{
			step = 0;
			sound_play_effect("step2.ogg", 100);
		}

		tick = LastTick;
	}
}

/**
 * Mapstats command.
 * @param data The incoming data.
 * @param len Length of the data. */
void MapStatsCmd(unsigned char *data, int len)
{
	int pos = 0;
	char buf[HUGE_BUF];
	uint8 type;

	/* Loop, trying to find data. */
	while (pos < len)
	{
		/* Get the type of this command... */
		type = (uint8) (data[pos++]);

		/* Change map name. */
		if (type == CMD_MAPSTATS_NAME)
		{
			strncpy(buf, (const char *) (data + pos), sizeof(buf) - 1);
			buf[sizeof(buf) - 1] = '\0';
			pos += strlen(buf) + 1;
			update_map_name(buf);
		}
		/* Change map music. */
		else if (type == CMD_MAPSTATS_MUSIC)
		{
			strncpy(buf, (const char *) (data + pos), sizeof(buf) - 1);
			buf[sizeof(buf) - 1] = '\0';
			pos += strlen(buf) + 1;
			update_map_bg_music(buf);
		}
		/* Change map weather. */
		else if (type == CMD_MAPSTATS_WEATHER)
		{
			strncpy(buf, (const char *) (data + pos), sizeof(buf) - 1);
			buf[sizeof(buf) - 1] = '\0';
			pos += strlen(buf) + 1;
			update_map_weather(buf);
		}
	}
}

/**
 * Map2 command.
 * @param data The incoming data
 * @param len Length of the data */
void Map2Cmd(unsigned char *data, int len)
{
	static int mx = 0, my = 0;
	int mask, x, y, pos = 0;
	int mapstat;
	int xpos, ypos;
	int layer, ext_flags;
	uint8 num_layers;

	mapstat = (uint8) (data[pos++]);

	if (mapstat != MAP_UPDATE_CMD_SAME)
	{
		char mapname[HUGE_BUF], bg_music[HUGE_BUF], weather[MAX_BUF];

		strncpy(mapname, (const char *) (data + pos), sizeof(mapname) - 1);
		pos += strlen(mapname) + 1;
		strncpy(bg_music, (const char *) (data + pos), sizeof(bg_music) - 1);
		pos += strlen(bg_music) + 1;
		strncpy(weather, (const char *) (data + pos), sizeof(weather) - 1);
		pos += strlen(weather) + 1;

		if (mapstat == MAP_UPDATE_CMD_NEW)
		{
			int map_w, map_h;

			map_w = (uint8) (data[pos++]);
			map_h = (uint8) (data[pos++]);
			xpos = (uint8) (data[pos++]);
			ypos = (uint8) (data[pos++]);
			mx = xpos;
			my = ypos;
			object_remove_inventory(object_find(0));
			init_map_data(map_w, map_h, xpos, ypos);
		}
		else
		{
			int xoff, yoff;

			mapstat = (sint8) (data[pos++]);
			xoff = (sint8) (data[pos++]);
			yoff = (sint8) (data[pos++]);
			xpos = (uint8) (data[pos++]);
			ypos = (uint8) (data[pos++]);
			mx = xpos;
			my = ypos;
			object_remove_inventory(object_find(0));
			display_mapscroll(xoff, yoff);

			map_play_footstep();
		}

		update_map_name(mapname);
		update_map_bg_music(bg_music);
		update_map_weather(weather);
	}
	else
	{
		xpos = (uint8) (data[pos++]);
		ypos = (uint8) (data[pos++]);

		/* we have moved */
		if ((xpos - mx || ypos - my))
		{
			object_remove_inventory(object_find(0));

			display_mapscroll(xpos - mx, ypos - my);
			map_play_footstep();
		}

		mx = xpos;
		my = ypos;
	}

	MapData.posx = xpos;
	MapData.posy = ypos;

	while (pos < len)
	{
		mask = GetShort_String(data + pos);
		pos += 2;
		x = (mask >> 11) & 0x1f;
		y = (mask >> 6) & 0x1f;

		/* Clear the whole cell? */
		if (mask & MAP2_MASK_CLEAR)
		{
			map_clear_cell(x, y);
			continue;
		}

		/* Do we have darkness information? */
		if (mask & MAP2_MASK_DARKNESS)
		{
			map_set_darkness(x, y, (uint8) (data[pos++]));
		}

		num_layers = data[pos++];

		/* Go through all the layers on this tile. */
		for (layer = 0; layer < num_layers; layer++)
		{
			uint8 type = data[pos++];

			/* Clear this layer. */
			if (type == MAP2_LAYER_CLEAR)
			{
				type = data[pos++];

				if (cpl.server_socket_version < 1058)
				{
					type--;
				}

				map_set_data(x, y, type, 0, 0, 0, "", "", 0, 0, 0, 0, 0, 0, 0, 0, 0);
			}
			/* We have some data. */
			else
			{
				sint16 face = GetShort_String(data + pos), height = 0, zoom_x = 0, zoom_y = 0, align = 0, rotate = 0;
				uint8 flags, obj_flags, quick_pos = 0, probe = 0, draw_double = 0, alpha = 0, infravision = 0;
				char player_name[64], player_color[COLOR_BUF];

				if (cpl.server_socket_version < 1058)
				{
					type--;
				}

				player_name[0] = '\0';
				player_color[0] = '\0';

				pos += 2;
				/* Request the face. */
				request_face(face);
				/* Object flags. */
				obj_flags = data[pos++];
				/* Flags of this layer. */
				flags = data[pos++];

				/* Multi-arch? */
				if (flags & MAP2_FLAG_MULTI)
				{
					quick_pos = data[pos++];
				}

				/* Player name? */
				if (flags & MAP2_FLAG_NAME)
				{
					GetString_String(data, &pos, player_name, sizeof(player_name));
					GetString_String(data, &pos, player_color, sizeof(player_color));
				}

				/* Target's HP? */
				if (flags & MAP2_FLAG_PROBE)
				{
					probe = data[pos++];
				}

				/* Z position? */
				if (flags & MAP2_FLAG_HEIGHT)
				{
					height = GetShort_String(data + pos);
					pos += 2;
				}

				/* Zoom? */
				if (flags & MAP2_FLAG_ZOOM)
				{
					zoom_x = GetShort_String(data + pos);
					pos += 2;

					if (cpl.server_socket_version < 1058)
					{
						zoom_y = zoom_x;
					}
					else
					{
						zoom_y = GetShort_String(data + pos);
						pos += 2;
					}
				}

				/* Align? */
				if (flags & MAP2_FLAG_ALIGN)
				{
					align = GetShort_String(data + pos);
					pos += 2;
				}

				/* Double? */
				if (flags & MAP2_FLAG_DOUBLE)
				{
					draw_double = 1;
				}

				if (flags & MAP2_FLAG_MORE)
				{
					uint32 flags2 = GetInt_String(data + pos);

					pos += 4;

					if (flags2 & MAP2_FLAG2_ALPHA)
					{
						alpha = data[pos++];
					}

					if (flags2 & MAP2_FLAG2_ROTATE)
					{
						rotate = GetShort_String(data + pos);
						pos += 2;
					}

					if (flags2 & MAP2_FLAG2_INFRAVISION)
					{
						infravision = 1;
					}
				}

				/* Set the data we figured out. */
				map_set_data(x, y, type, face, quick_pos, obj_flags, player_name, player_color, height, probe, zoom_x, zoom_y, align, draw_double, alpha, rotate, infravision);
			}
		}

		/* Get tile flags. */
		ext_flags = data[pos++];

		/* Animation? */
		if (ext_flags & MAP2_FLAG_EXT_ANIM)
		{
			uint8 anim_type;
			sint16 anim_value;

			anim_type = data[pos++];
			anim_value = GetShort_String(data + pos);
			pos += 2;

			add_anim(anim_type, xpos + x, ypos + y, anim_value);
		}
	}

	adjust_tile_stretch();
	map_udate_flag = 2;
	map_redraw_flag = 1;
}

/**
 * Magic map command. Currently unused. */
void MagicMapCmd(unsigned char *data, int len)
{
	(void) data;
	(void) len;
}

/**
 * Server informs the client about its version.
 * @param data Data.
 * @param len Length of data. */
void cmd_version(uint8 *data, int len)
{
	if (len > 4)
	{
		cpl.server_socket_version = 1057;
	}
	else
	{
		cpl.server_socket_version = GetInt_String(data);
	}
}

/**
 * Sends version and client name.
 * @param csock Socket to send this information to. */
void SendVersion(void)
{
	char buf[MAX_BUF];

	snprintf(buf, sizeof(buf), "version %d %s", SOCKET_VERSION, PACKAGE_NAME);
	cs_write_string(buf, strlen(buf));
}

/**
 * Request srv file.
 * @param csock Socket to request from
 * @param idx SRV file ID */
void RequestFile(int idx)
{
	char buf[MAX_BUF];

	sprintf(buf, "rf %d", idx);
	cs_write_string(buf, strlen(buf));
}

/**
 * Send an addme command to the server.
 * @param csock Socket to send the command to. */
void SendAddMe(void)
{
	cs_write_string("addme", 5);
}

/**
 * New char command.
 * Used when server tells us to go to the new character creation. */
void NewCharCmd(void)
{
	GameStatus = GAME_STATUS_NEW_CHAR;
}

/**
 * Data command.
 * Used when server sends us block of data, like new srv file.
 * @param data Incoming data
 * @param len Length of the data */
void DataCmd(unsigned char *data, int len)
{
	uint8 data_type;
	unsigned long len_ucomp;
	unsigned char *dest;

	data_type = *data++;
	len_ucomp = GetInt_String(data);
	data += 4;
	len -= 5;
	/* Allocate large enough buffer to hold the uncompressed file. */
	dest = malloc(len_ucomp);

	LOG(llevInfo, "DataCmd(): Uncompressing file #%d (len: %d, uncompressed len: %lu)\n", data_type, len, len_ucomp);
	uncompress((Bytef *) dest, (uLongf *) &len_ucomp, (const Bytef *) data, (uLong) len);
	data = dest;
	len = len_ucomp;
	server_file_save(data_type, data, len);
	free(dest);
}

/**
 * Shop command.
 * @param data Data buffer
 * @param len Length of the buffer */
void ShopCmd(unsigned char *data, int len)
{
	(void) data;
	(void) len;
}

/**
 * Quest list command.
 *
 * Uses the book GUI to show the quests.
 * @param data Data.
 * @param len Length of the data. */
void QuestListCmd(unsigned char *data, int len)
{
	sound_play_effect("book.ogg", 100);

	data += 4;
	book_load((char *) data, len - 4);
}

/**
 * Ready command. Marks an object with specified UID as readied (arrow,
 * quiver, etc).
 * @param data Data.
 * @param len Length of the data. */
void ReadyCmd(unsigned char *data, int len)
{
	int tag;
	uint8 type;

	(void) len;

	type = data[0];
	tag = GetInt_String(data + 1);

	if (type == READY_OBJ_ARROW)
	{
		fire_mode_tab[FIRE_MODE_BOW].amun = tag;
	}
	else if (type == READY_OBJ_THROW)
	{
		fire_mode_tab[FIRE_MODE_THROW].item = tag;
	}
}
