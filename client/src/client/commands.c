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
 * Handles commands received by the server.  This does not necessarily
 * handle all commands - some might be in other files (like main.c)
 *
 * This file handles commans from the server->client.  See player.c
 * for client->server commands.
 *
 * this file contains most of the commands for the dispatch loop most of
 * the functions are self-explanatory, the pixmap/bitmap commands recieve
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

#include <include.h>
static int scrolldx = 0, scrolldy = 0;

/**
 * Do we need to clear the map due to a previous mapstats command?
 *
 * We can't clear the map in mapstats command, as that would leave the
 * map blacked out until map2 command is received. */
static int do_clear_map = 0;

/**
 * Book command, used to initialize the book interface.
 * @param data Data of the book
 * @param len Length of the data */
void BookCmd(unsigned char *data, int len)
{
	sound_play_effect(SOUND_BOOK, 0, 100);
	cpl.menustatus = MENU_BOOK;

	data += 4;

	gui_interface_book = book_gui_load((char *)data, len - 4);
}

/**
 * Party command, used to initialize the party GUI.
 * @param data Data for the party interface
 * @param len Length of the data */
void PartyCmd(unsigned char *data, int len)
{
	cpl.menustatus = MENU_PARTY;

	gui_interface_party = load_party_interface((char *)data, len);
}

/**
 * Sound command, used to play a sound.
 * @param data Data to initialize the music data from, like x, y, type, etc
 * @param len Length of the data */
void SoundCmd(unsigned char *data,  int len)
{
	int x, y, num, type;

	if (len != 5)
	{
		LOG(llevError, "Got invalid length on sound command: %d\n", len);
		return;
	}

	x = (signed char) data[0];
	y = (signed char) data[1];
	num = GetShort_String(data + 2);
	type = data[4];

	if (type == SOUND_SPELL)
	{
		if (num < 0 || num >= SPELL_SOUND_MAX)
		{
			LOG(llevError, "Got invalid spell sound id: %d\n", num);
			return;
		}

		/* This maps us to the spell sound table part */
		num += SOUND_MAX;
	}
	else
	{
		if (num < 0 || num >= SOUND_MAX)
		{
			LOG(llevError, "Got invalid sound id: %d\n", num);
			return;
		}
	}

	calculate_map_sound(num, x, y);
}

/**
 * Parse server file information from the setup command.
 * @param param Parameter for the command.
 * @param command The setup command (amf, hpf, etc).
 * @param type ID of the server file. */
static void parse_srv_setup(char *param, const char *command, int type)
{
	if (!strcmp(param, "FALSE"))
	{
		LOG(llevMsg, "Get %s:: %s\n", command, param);
	}
	else if (strcmp(param, "OK"))
	{
		char *cp;

		srv_client_files[type].status = SRV_CLIENT_STATUS_UPDATE;

		for (cp = param; *cp != '\0'; cp++)
		{
			if (*cp == '|')
			{
				*cp = '\0';
				srv_client_files[type].server_len = atoi(param);
				srv_client_files[type].server_crc = strtoul(cp + 1, NULL, 16);
				break;
			}
		}
	}
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

	scrolldy = scrolldx = 0;
	LOG(llevMsg, "Get SetupCmd:: %s\n", buf);

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
		else if (!strcmp(cmd, "skf"))
		{
			parse_srv_setup(param, cmd, SRV_CLIENT_SKILLS);
		}
		else if (!strcmp(cmd, "spf"))
		{
			parse_srv_setup(param, cmd, SRV_CLIENT_SPELLS);
		}
		else if (!strcmp(cmd, "stf"))
		{
			parse_srv_setup(param, cmd, SRV_CLIENT_SETTINGS);
		}
		else if (!strcmp(cmd, "bpf"))
		{
			parse_srv_setup(param, cmd, SRV_CLIENT_BMAPS);
		}
		else if (!strcmp(cmd, "amf"))
		{
			parse_srv_setup(param, cmd, SRV_CLIENT_ANIMS);
		}
		else if (!strcmp(cmd, "hpf"))
		{
			parse_srv_setup(param, cmd, SRV_CLIENT_HFILES);
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
		else
		{
			LOG(llevError, "Got setup for a command we don't understand: %s %s\n", cmd, param);
		}
	}

	GameStatus = GAME_STATUS_REQUEST_FILES;
}

/**
 * Only used if the server believes we are caching images.
 * We rely on the fact that the server will only send a face command for
 * a particular number once - at current time, we have no way of knowing
 * if we have already received a face for a particular number.
 * @param data Incoming data.
 * @param len Length of the data */
void Face1Cmd(unsigned char *data,  int len)
{
	int pnum;
	uint32  checksum;
	char *face;

	pnum = GetShort_String(data);
	checksum = GetInt_String(data + 2);
	face = (char*)data + 6;
	data[len] = '\0';

	finish_face_cmd(pnum, checksum, face);
}

/**
 * Handles when the server says we can't be added.  In reality, we need to
 * close the connection and quit out, because the client is going to close
 * us down anyways. */
void AddMeFail(unsigned char *data, int len)
{
	(void) data;
	(void) len;

	LOG(llevMsg, "addme_failed received.\n");
	GameStatus = GAME_STATUS_START;

	/* Add here error handling */
	return;
}

/**
 * This is really a throwaway command - there really isn't any reason to
 * send addme_success commands. */
void AddMeSuccess(unsigned char *data, int len)
{
	(void) data;
	(void) len;

	LOG(llevMsg, "addme_success received.\n");
	return;
}

/**
 * Goodbye command. Currently doesn't do anything. */
void GoodbyeCmd(unsigned char *data, int len)
{
	(void) data;
	(void) len;
}

/**
 * Animation command.
 * @param data The incoming data
 * @param len Length of the data
 */
void AnimCmd(unsigned char *data, int len)
{
	short anum;
	int i, j;

	anum = GetShort_String(data);
	if (anum < 0 || anum > MAXANIM)
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
		animations[anum].frame = animations[anum].num_animations / animations[anum].facings;
	else
		animations[anum].frame = animations[anum].num_animations;

	animations[anum].faces = malloc(sizeof(uint16) * animations[anum].num_animations);

	for (i = 4, j = 0; i < len; i += 2, j++)
	{
		animations[anum].faces[j] = GetShort_String(data + i);
		request_face(animations[anum].faces[j], 0);
	}

	if (j != animations[anum].num_animations)
		LOG(llevDebug, "Calculated animations does not equal stored animations? (%d != %d)\n", j, animations[anum].num_animations);

#if 0
	LOG(llevMsg, "Received animation %d, %d facings and %d faces\n", anum, animations[anum].facings, animations[anum].num_animations);
#endif
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
		LOG(llevError, "PixMapCmd: Lengths don't compare (%d, %d)\n", (len - 8), plen);
		return;
	}

	/* Save picture to cache and load it to FaceList */
	sprintf(buf, "%s%s", GetCacheDirectory(), FaceList[pnum].name);
	LOG(llevDebug, "ImageFromServer: %s\n", FaceList[pnum].name);

	if ((stream = fopen_wrapper(buf, "wb+")) != NULL)
	{
		fwrite((char *) data + 8, 1, plen, stream);
		fclose(stream);
	}

	FaceList[pnum].sprite = sprite_tryload_file(buf, 0, NULL);
	map_udate_flag = 2;
	map_redraw_flag = 1;
}

/**
 * Ready command.
 * @param data The incoming data
 * @param len Length of the data */
void SkillRdyCmd(char *data, int len)
{
	int i, ii;

	(void) len;

	strcpy(cpl.skill_name, data);
	WIDGET_REDRAW(SKILL_EXP_ID);

	/* lets find the skill... and setup the shortcuts to the exp values*/
	for (ii = 0; ii < SKILL_LIST_MAX; ii++)
	{
		for (i = 0; i < DIALOG_LIST_ENTRY; i++)
		{
			/* we have a list entry */
			if (skill_list[ii].entry[i].flag == LIST_ENTRY_KNOWN)
			{
				/* and is it the one we searched for? */
				if (!strcmp(skill_list[ii].entry[i].name, cpl.skill_name))
				{
					cpl.skill_g = ii;
					cpl.skill_e = i;
					return;
				}
			}
		}
	}
}

/**
 * Draw info command. Used to draw text from the server.
 * @param data The text to output. */
void DrawInfoCmd(unsigned char *data)
{
	int color = atoi((char *) data);
	char *buf;

	buf = strchr((char *) data, ' ');

	if (!buf)
	{
		LOG(llevError, "DrawInfoCmd - got no data\n");
		buf = "";
	}
	else
	{
		buf++;
	}

	draw_info(buf, color);
}

/**
 * New draw info command. Used to draw text from the server with various
 * flags, like color.
 * @param data The incoming data
 * @param len Length of the data */
void DrawInfoCmd2(unsigned char *data, int len)
{
	int flags;
	char buf[2048], *tmp = NULL;

	flags = (int) GetShort_String(data);
	data += 2;

	len -= 2;

	if (len >= 0)
	{
		if (len > 2000)
		{
			len = 2000;
		}

		if (options.chat_timestamp && (flags & NDI_PLAYER))
		{
			time_t now = time(NULL);
			char timebuf[32], *format;
			struct tm *tmp = localtime(&now);
			size_t timelen;

			switch (options.chat_timestamp)
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

			timelen = strftime(timebuf, sizeof(timebuf), format, tmp);

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
		msg_anim.message[len - 1] = '\0';
		msg_anim.flags = flags;
		msg_anim.tick = LastTick;
	}

	draw_info(buf, flags);
}

/**
 * Target object command.
 * @param data The incoming data
 * @param len Length of the data */
void TargetObject(unsigned char *data, int len)
{
	cpl.target_mode = *data++;

	if (cpl.target_mode)
		sound_play_effect(SOUND_WEAPON_ATTACK, 0, 100);
	else
		sound_play_effect(SOUND_WEAPON_HOLD, 0, 100);

	cpl.target_color = *data++;
	cpl.target_code = *data++;
	strncpy(cpl.target_name, (char *)data, len);
	map_udate_flag = 2;
	map_redraw_flag = 1;

#if 0
	char buf[MAX_BUF];
	sprintf(buf, "TO: %d %d >%s< (len: %d)\n", cpl.target_mode, cpl.target_code, cpl.target_name, len);
	draw_info(buf, COLOR_GREEN);
#endif
}

/**
 * Stats command. Used to update various things, like target's HP, mana regen, protections, etc
 * @param data The incoming data
 * @param len Length of the data */
void StatsCmd(unsigned char *data, int len)
{
	int i = 0;
	int c, temp;
	char *tmp, *tmp2;

	while (i < len)
	{
		c = data[i++];

		if (c >= CS_STAT_PROT_START && c <= CS_STAT_PROT_END)
		{
			cpl.stats.protection[c - CS_STAT_PROT_START] = (sint16)*(data + i++);
			WIDGET_REDRAW(RESIST_ID);
		}
		else
		{
			switch (c)
			{
				case CS_STAT_TARGET_HP:
					cpl.target_hp = (int)*(data + i++);
					break;

				case CS_STAT_REG_HP:
					cpl.gen_hp = ((float)GetShort_String(data + i)) / 10.0f;
					i += 2;
					WIDGET_REDRAW(REGEN_ID);
					break;

				case CS_STAT_REG_MANA:
					cpl.gen_sp = ((float)GetShort_String(data + i)) / 10.0f;
					i += 2;
					WIDGET_REDRAW(REGEN_ID);
					break;

				case CS_STAT_REG_GRACE:
					cpl.gen_grace = ((float)GetShort_String(data + i)) / 10.0f;
					i += 2;
					WIDGET_REDRAW(REGEN_ID);
					break;

				case CS_STAT_HP:
					temp = GetInt_String(data + i);

					if (temp < cpl.stats.hp && cpl.stats.food)
					{
						cpl.warn_hp = 1;
						if (cpl.stats.maxhp / 12 <= cpl.stats.hp-temp)
							cpl.warn_hp = 2;
					}
					cpl.stats.hp = temp;
					i += 4;
					WIDGET_REDRAW(STATS_ID);
					break;

				case CS_STAT_MAXHP:
					cpl.stats.maxhp = GetInt_String(data + i);
					i += 4;
					WIDGET_REDRAW(STATS_ID);
					break;

				case CS_STAT_SP:
					cpl.stats.sp = GetShort_String(data + i);
					i += 2;
					WIDGET_REDRAW(STATS_ID);
					break;

				case CS_STAT_MAXSP:
					cpl.stats.maxsp = GetShort_String(data + i);
					i += 2;
					WIDGET_REDRAW(STATS_ID);
					break;

				case CS_STAT_GRACE:
					cpl.stats.grace = GetShort_String(data + i);
					i += 2;
					WIDGET_REDRAW(STATS_ID);
					break;

				case CS_STAT_MAXGRACE:
					cpl.stats.maxgrace = GetShort_String(data + i);
					i += 2;
					WIDGET_REDRAW(STATS_ID);
					break;

				case CS_STAT_STR:
					temp = (int)*(data + i++);

					if (temp > cpl.stats.Str)
						cpl.warn_statup = 1;
					else
						cpl.warn_statdown = 1;

					cpl.stats.Str = temp;
					WIDGET_REDRAW(STATS_ID);
					break;

				case CS_STAT_INT:
					temp = (int)*(data + i++);

					if (temp > cpl.stats.Int)
						cpl.warn_statup = 1;
					else
						cpl.warn_statdown = 1;

					cpl.stats.Int = temp;
					WIDGET_REDRAW(STATS_ID);
					break;

				case CS_STAT_POW:
					temp = (int)*(data + i++);

					if (temp > cpl.stats.Pow)
						cpl.warn_statup = 1;
					else
						cpl.warn_statdown = 1;

					cpl.stats.Pow = temp;
					WIDGET_REDRAW(STATS_ID);
					break;

				case CS_STAT_WIS:
					temp = (int)*(data + i++);

					if (temp > cpl.stats.Wis)
						cpl.warn_statup = 1;
					else
						cpl.warn_statdown = 1;

					cpl.stats.Wis = temp;
					WIDGET_REDRAW(STATS_ID);
					break;

				case CS_STAT_DEX:
					temp = (int)*(data + i++);

					if (temp > cpl.stats.Dex)
						cpl.warn_statup = 1;
					else
						cpl.warn_statdown = 1;

					cpl.stats.Dex = temp;
					WIDGET_REDRAW(STATS_ID);
					break;

				case CS_STAT_CON:
					temp = (int)*(data + i++);

					if (temp > cpl.stats.Con)
						cpl.warn_statup = 1;
					else
						cpl.warn_statdown = 1;

					cpl.stats.Con = temp;
					WIDGET_REDRAW(STATS_ID);
					break;

				case CS_STAT_CHA:
					temp = (int)*(data + i++);

					if (temp > cpl.stats.Cha)
						cpl.warn_statup = 1;
					else
						cpl.warn_statdown = 1;

					cpl.stats.Cha = temp;
					WIDGET_REDRAW(STATS_ID);
					break;

				case CS_STAT_EXP:
					temp = GetInt_String(data + i);

					if (temp < cpl.stats.exp)
						cpl.warn_drain = 1;

					cpl.stats.exp = temp;
					i += 4;
					WIDGET_REDRAW(MAIN_LVL_ID);
					break;

				case CS_STAT_LEVEL:
					cpl.stats.level = (char)*(data + i++);
					WIDGET_REDRAW(MAIN_LVL_ID);
					break;

				case CS_STAT_WC:
					cpl.stats.wc = (char)GetShort_String(data + i);
					i += 2;
					break;

				case CS_STAT_AC:
					cpl.stats.ac = (char)GetShort_String(data + i);
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
					WIDGET_REDRAW(STATS_ID);
					break;

				case CS_STAT_WEAP_SP:
					cpl.stats.weapon_sp = (int)*(data + i++);
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
					cpl.action_timer = ((float)abs(GetInt_String(data + i))) / 1000.0f;
					i += 4;
					WIDGET_REDRAW(SKILL_EXP_ID);
					break;

				case CS_STAT_SKILLEXP_AGILITY:
				case CS_STAT_SKILLEXP_PERSONAL:
				case CS_STAT_SKILLEXP_MENTAL:
				case CS_STAT_SKILLEXP_PHYSIQUE:
				case CS_STAT_SKILLEXP_MAGIC:
				case CS_STAT_SKILLEXP_WISDOM:
					cpl.stats.skill_exp[(c - CS_STAT_SKILLEXP_START) / 2] = GetInt_String(data + i);
					i += 4;
					WIDGET_REDRAW(SKILL_LVL_ID);
					break;

				case CS_STAT_SKILLEXP_AGLEVEL:
				case CS_STAT_SKILLEXP_PELEVEL:
				case CS_STAT_SKILLEXP_MELEVEL:
				case CS_STAT_SKILLEXP_PHLEVEL:
				case CS_STAT_SKILLEXP_MALEVEL:
				case CS_STAT_SKILLEXP_WILEVEL:
					cpl.stats.skill_level[(c - CS_STAT_SKILLEXP_START - 1) / 2] = (sint16)*(data + i++);
					WIDGET_REDRAW(SKILL_LVL_ID);
					break;

				case CS_STAT_RANGE:
				{
					int rlen = data[i++];
					strncpy(cpl.range, (const char*)data + i, rlen);
					cpl.range[rlen] = '\0';
					i += rlen;
					break;
				}

				case CS_STAT_EXT_TITLE:
				{
					int rlen = data[i++];

					tmp = strchr((char *)data + i,'\n');
					*tmp = 0;
					strcpy(cpl.rank, (char *)data + i);
					tmp2 = strchr(tmp + 1, '\n');
					*tmp2 = 0;
					strcpy(cpl.pname, tmp + 1);
					tmp = strchr(tmp2 + 1, '\n');
					*tmp = 0;
					strcpy(cpl.race, tmp2 + 1);
					tmp2 = strchr(tmp + 1, '\n');
					*tmp2 = 0;
					/* Profession title */
					strcpy(cpl.title, tmp + 1);
					tmp = strchr(tmp2 + 1, '\n');
					*tmp = 0;
					strcpy(cpl.alignment, tmp2 + 1);
					tmp2 = strchr(tmp + 1, '\n');
					*tmp2 = 0;
					strcpy(cpl.godname, tmp + 1);
					strcpy(cpl.gender, tmp2 + 1);

					if (cpl.gender[0] == 'm')
						strcpy(cpl.gender, "male");
					else if (cpl.gender[0] == 'f')
						strcpy(cpl.gender, "female");
					else if (cpl.gender[0] == 'h')
						strcpy(cpl.gender, "hermaphrodite");
					else
						strcpy(cpl.gender, "neuter");

					i += rlen;

					/* prepare rank + name for fast access
					 * the pname is <name> <title>.
					 * is there no title, there is still
					 * always a ' ' at the end - we skip this
					 * here! */
					strcpy(cpl.rankandname, cpl.rank);
					strcat(cpl.rankandname, cpl.pname);

					if (strlen(cpl.rankandname) > 0)
						cpl.rankandname[strlen(cpl.rankandname) - 1] = 0;

					adjust_string(cpl.rank);
					adjust_string(cpl.rankandname);
					adjust_string(cpl.pname);
					adjust_string(cpl.race);
					adjust_string(cpl.title);
					adjust_string(cpl.alignment);
					adjust_string(cpl.gender);
					adjust_string(cpl.godname);

					if (strstr(cpl.pname, "[WIZ]"))
					{
						cpl.dm = 1;
					}
					else
					{
						cpl.dm = 0;
					}

					break;
				}

				default:
					fprintf(stderr, "Unknown stat number %d\n", c);
			}
		}
	}

	if (i > len)
		fprintf(stderr, "Got stats overflow, processed %d bytes out of %d\n", i, len);
}

/**
 * Used by handle_query to open console for questions like name, password, etc.
 * @param cmd The question command. */
void PreParseInfoStat(char *cmd)
{
	/* Find input name */
	if (strstr(cmd, "What is your name?"))
	{
		LOG(llevMsg, "Login: Enter name\n");
		cpl.name[0] = 0;
		cpl.password[0] = 0;
		GameStatus = GAME_STATUS_NAME;
	}

	if (strstr(cmd, "What is your password?"))
	{
		LOG(llevMsg, "Login: Enter password\n");
		GameStatus = GAME_STATUS_PSWD;
	}

	if (strstr(cmd, "Please type your password again."))
	{
		LOG(llevMsg, "Login: Enter verify password\n");
		GameStatus = GAME_STATUS_VERIFYPSWD;
	}

	if (GameStatus >= GAME_STATUS_NAME && GameStatus <= GAME_STATUS_VERIFYPSWD)
		open_input_mode(12);
}

/**
 * Handle server query question.
 * @param data The incoming data */
void handle_query(char *data)
{
	char *buf, *cp;

	buf = strchr(data, ' ');
	if (buf)
		buf++;

	if (buf)
	{
		cp = buf;
		while ((buf = strchr(buf, '\n')) != NULL)
		{
			*buf++ = '\0';
			LOG(llevMsg, "Received query string: %s\n", cp);
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
	char buf[MAXSOCKBUF];

	sprintf(buf, "reply %s", text);
	cs_write_string(csocket.fd, buf, strlen(buf));
}

/**
 * This function copies relevant data from the archetype to the object.
 * Only copies data that was not set in the object structure.
 * @param data The incoming data
 * @param len Length of the data */
void PlayerCmd(unsigned char *data, int len)
{
	char name[MAX_BUF];
	int tag, weight, face, i = 0, nlen;

	GameStatus = GAME_STATUS_PLAY;
	txtwin[TW_MIX].size = txtwin_start_size;
	InputStringEndFlag = 0;
	tag = GetInt_String(data);
	i += 4;
	weight = GetInt_String(data + i);
	i += 4;
	face = GetInt_String(data + i);
	request_face(face, 0);
	i += 4;
	nlen = data[i++];
	memcpy(name, (const char*)data + i, nlen);

	name[nlen] = '\0';
	i += nlen;

	if (i != len)
		fprintf(stderr, "PlayerCmd: lengths do not match (%d!=%d)\n", len, i);

	new_player(tag, name, weight, (short)face);
	map_draw_map_clear();
	map_transfer_flag = 1;
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

#if 0
	LOG(-1, "ITEMX:(%d) %s\n", dmode, locate_item(dmode) ? (locate_item(dmode)->d_name ? locate_item(dmode)->s_name : "no name") : "no LOC");
#endif

	loc = GetInt_String(data+pos);

	if (dmode >= 0)
		remove_item_inventory(locate_item(loc));

	/* send item flag */
	if (dmode == -4)
	{
		/* and redirect it to our invisible sack */
		if (loc == cpl.container_tag)
			loc = -1;
	}
	/* container flag! */
	else if (dmode == -1)
	{
		/* we catch the REAL container tag */
		cpl.container_tag = loc;
		remove_item_inventory(locate_item(-1));

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
		LOG(llevError, "ItemCmd: Got location with no other data\n");
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
			request_face(face, 0);
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
			memcpy(name, (char*)data + pos, nlen);
			pos += nlen;
			name[nlen] = '\0';
			anim = GetShort_String(data + pos);
			pos += 2;
			animspeed = data[pos++];
			nrof = GetInt_String(data+pos);
			pos += 4;
			update_item(tag, loc, name, weight, face, flags, anim, animspeed, nrof, itype, stype, item_qua, item_con, item_skill, item_level, direction, 0);
		}

		if (pos > len)
			LOG(llevError, "ItemCmd: ERROR: Overread buffer: %d > %d\n", pos, len);
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

#if 0
	LOG(-1, "ITEMY:(%d) %s\n", dmode, locate_item(dmode) ? (locate_item(dmode)->d_name ? locate_item(dmode)->s_name : "no name") : "no LOC");
#endif

	loc = GetInt_String(data + pos);

	if (dmode >= 0)
		remove_item_inventory(locate_item(loc));

	/* send item flag */
	if (dmode == -4)
	{
		/* and redirect it to our invisible sack */
		if (loc == cpl.container_tag)
			loc = -1;
	}
	/* container flag! */
	else if (dmode == -1)
	{
		/* we catch the REAL container tag */
		cpl.container_tag = loc;
		remove_item_inventory(locate_item(-1));

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
		/*LOG(llevError, "ItemCmd: Got location with no other data\n");*/
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
			request_face(face, 0);
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
			memcpy(name, (char*)data + pos, nlen);
			pos += nlen;
			name[nlen] = '\0';
			anim = GetShort_String(data + pos);
			pos += 2;
			animspeed = data[pos++];
			nrof = GetInt_String(data + pos);
			pos += 4;
			update_item(tag, loc, name, weight, face, flags, anim, animspeed, nrof, itype, stype, item_qua, item_con, item_skill, item_level, direction, 1);
		}

		if (pos > len)
			LOG(llevError, "ItemCmd: ERROR: Overread buffer: %d > %d\n", pos, len);
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
	item *ip, *env = NULL;
	uint8 animspeed;

	map_udate_flag = 2;
	sendflags = GetShort_String(data);
	pos += 2;
	tag = GetInt_String(data + pos);
	pos += 4;
	ip = locate_item(tag);

	if (!ip)
	{
		return;
	}

	*name = '\0';
	loc = ip->env ? ip->env->tag : 0;
	/*LOG(-1, "UPDATE: loc:%d tag:%d\n", loc, tag); */
	weight = (int)(ip->weight * 1000);
	face = ip->face;
	request_face(face, 0);
	flags = ip->flagsval;
	anim = ip->animation_id;
	animspeed = (uint8) ip->anim_speed;
	nrof = ip->nrof;
	direction = ip->direction;

	if (sendflags & UPD_LOCATION)
	{
		loc = GetInt_String(data + pos);
		env = locate_item(loc);

		if (!env)
			fprintf(stderr, "UpdateItemCmd: unknown object tag (%d) for new location\n", loc);

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
		request_face(face, 0);
		pos += 4;
	}

	if (sendflags & UPD_DIRECTION)
		direction = data[pos++];

	if (sendflags & UPD_NAME)
	{

		nlen = data[pos++];
		memcpy(name, (char*)data + pos, nlen);
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

	update_item(tag, loc, name, weight, face, flags, anim, animspeed, nrof, 254, 254, 254, 254, 254, 254, direction, 0);
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
		delete_item(tag);
	}

	if (pos > len)
		fprintf(stderr, "ItemCmd: Overread buffer: %d > %d\n", pos, len);

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

	remove_item_inventory(locate_item(tag));
	map_udate_flag = 2;
}

/**
 * Parses data returned by the server for quickslots.
 * First, it assigns all quickslots default values,
 * then it goes through the data, line-by-line and
 * assigns values from the server.
 * @param data The data to parse */
void QuickSlotCmd(char *data)
{
	char *p, *buf;
	int tag, slot, i, group_nr, class_nr;

	buf = (char *)malloc(strlen(data) + 1);

	sprintf(buf, "%s", data);

	p = strtok(buf, "\n");

	for (i = 0; i < MAX_QUICK_SLOTS * MAX_QUICKSLOT_GROUPS; i++)
	{
		quick_slots[i].spell = 0;
		quick_slots[i].tag = -1;
		quick_slots[i].classNr = 0;
		quick_slots[i].spellNr = 0;
		quick_slots[i].groupNr = 0;
	}

	while (p)
	{
		/* Item */
		if (p[0] == 'i' && sscanf(p, "i %d %d", &tag, &slot))
		{
			quick_slots[slot - 1].tag = tag;
		}
		/* Spell */
		else if (p[0] == 's' && sscanf(p, "s %d %d %d %d", &slot, &group_nr, &class_nr, &tag))
		{
			quick_slots[slot - 1].spell = 1;
			quick_slots[slot - 1].groupNr = group_nr;
			quick_slots[slot - 1].classNr = class_nr;
			quick_slots[slot - 1].spellNr = tag;
			quick_slots[slot - 1].tag = tag;
		}

		p = strtok(NULL, "\n");
	}

	free(buf);
}

/**
 * Map2 command.
 * @param data The incoming data
 * @param len Length of the data */
void Map2Cmd(unsigned char *data, int len)
{
	int mask, x, y, pos = 0, ext_flag, xdata;
	int ext1, ext2, ext3, probe;
	int ff0, ff1, ff2, ff3, ff_flag, xpos, ypos, pcolor;
	char pname1[64], pname2[64], pname3[64], pname4[64];
	uint16 face;

	if (scrolldx || scrolldy)
		display_mapscroll(scrolldx, scrolldy);

	scrolldy = scrolldx = 0;
	map_transfer_flag = 0;
	xpos = (uint8)(data[pos++]);

	/* its not xpos, its the changed map marker */
	if (xpos == 255)
	{
		xpos = (uint8)(data[pos++]);
	}

	ypos = (uint8)(data[pos++]);

	/* map windows is from range to +MAPWINSIZE_X */
	MapData.posx = xpos;
	MapData.posy = ypos;

	if (do_clear_map)
	{
		clear_map();
		do_clear_map = 0;
	}

	while (pos < len)
	{
		ext_flag = 0;
		ext1 = ext2 = ext3 = 0;
		/* first, we get the mask flag - it decribes what we now get */
		mask = GetShort_String(data + pos);
		pos += 2;
		x = (mask >> 11) & 0x1f;
		y = (mask >> 6) & 0x1f;

		/* these are the "damage tags" - shows damage an object got from somewhere.
		 * ff_flag hold the layer info and how much we got here.
		 * 0x08 means a damage comes from unknown or vanished source.
		 * this means the object is destroyed.
		 * the other flags are assigned to map layer. */
		if ((mask & 0x3f) == 0)
			display_map_clearcell(x, y);

		ext3 = ext2 = ext1 = -1;
		pname1[0] = 0;
		pname2[0] = 0;
		pname3[0] = 0;
		pname4[0] = 0;
		pcolor = 0;
		/* the ext flag defines special layer object assigned infos.
		 * Like the Zzz for sleep, paralyze msg, etc. */

		/* catch the ext. flag... */
		if (mask & 0x20)
		{
			ext_flag = (uint8)(data[pos++]);

			/* we have player names.... */
			if (ext_flag & 0x80)
			{
				char c;
				int i, pname_flag = (uint8)(data[pos++]);

				/* floor .... */
				if (pname_flag & 0x08)
				{
					i = 0;
					while ((c = (char)(data[pos++])))
					{
						pname1[i++] = c;
					}

					pname1[i] = 0;

					pcolor = GetShort_String(data + pos);
					pos += 2;
				}

				/* fm.... */
				if (pname_flag & 0x04)
				{
					i = 0;
					while ((c = (char)(data[pos++])))
					{
						pname2[i++] = c;
					}
					pname2[i] = 0;

					pcolor = GetShort_String(data + pos);
					pos += 2;
				}

				/* l1 .... */
				if (pname_flag & 0x02)
				{
					i = 0;
					while ((c = (char)(data[pos++])))
					{
						pname3[i++] = c;
					}
					pname3[i] = 0;

					pcolor = GetShort_String(data + pos);
					pos += 2;
				}

				/* l2 .... */
				if (pname_flag & 0x01)
				{
					i = 0;
					while ((c = (char)(data[pos++])))
					{
						pname4[i++] = c;
					}
					pname4[i] = 0;

					pcolor = GetShort_String(data + pos);
					pos += 2;
				}
			}

			/* damage add on the map */
			if (ext_flag & 0x40)
			{
				ff0 = ff1 = ff2 = ff3 = -1;
				ff_flag = (uint8)(data[pos++]);

				if (ff_flag & 0x8)
				{
					ff0 = GetShort_String(data + pos);
					pos+=2;
					add_anim(ANIM_KILL, xpos + x, ypos + y, ff0);
				}

				if (ff_flag & 0x4)
				{
					ff1 = GetShort_String(data + pos);
					pos += 2;
					add_anim(ANIM_DAMAGE, xpos + x, ypos + y, ff1);
				}

				if (ff_flag & 0x2)
				{
					ff2 = GetShort_String(data + pos);
					pos += 2;
					add_anim(ANIM_DAMAGE, xpos + x, ypos + y, ff2);
				}

				if (ff_flag & 0x1)
				{
					ff3 = GetShort_String(data + pos);
					pos += 2;
					add_anim(ANIM_DAMAGE, xpos + x, ypos + y, ff3);
				}
			}

			if (ext_flag & 0x08)
			{
				probe = 0;
				ext3 = (int)(data[pos++]);

				if (ext3 & FFLAG_PROBE)
					probe = (int)(data[pos++]);

				set_map_ext(x, y, 3, ext3, probe);
			}

			if (ext_flag & 0x10)
			{
				probe = 0;
				ext2 = (int)(data[pos++]);

				if (ext2 & FFLAG_PROBE)
					probe = (int)(data[pos++]);

				set_map_ext(x, y, 2, ext2, probe);
			}

			if (ext_flag & 0x20)
			{
				probe = 0;
				ext1 = (int)(data[pos++]);

				if (ext1 & FFLAG_PROBE)
					probe = (int)(data[pos++]);

				set_map_ext(x, y, 1, ext1, probe);
			}
		}

		if (mask & 0x10)
		{
			set_map_darkness(x, y, (uint8)(data[pos]));
			pos++;
		}

		/* at last, we get the layer faces.
		 * a set ext_flag here marks this entry as face from a multi tile arch.
		 * we got another byte then which all information we need to display
		 * this face in the right way (position and shift offsets) */
		if (mask & 0x8)
		{
			int z_height;

			face = GetShort_String(data + pos);
			pos += 2;
			request_face(face, 0);
			xdata = 0;
			z_height = GetShort_String(data + pos);
			pos += 2;
			set_map_face(x, y, 0, face, xdata, -1, pname1, pcolor, z_height);
		}

		if (mask & 0x4)
		{
			face = GetShort_String(data + pos);
			pos += 2;
			request_face(face, 0);
			xdata = 0;
			/* we have here a multi arch, fetch head offset */
			if (ext_flag & 0x04)
			{
				xdata = (uint8)(data[pos]);
				pos++;
			}
			set_map_face(x, y, 1, face, xdata, ext1, pname2, pcolor, 0);
		}

		if (mask & 0x2)
		{
			face = GetShort_String(data + pos);
			pos += 2;
			request_face(face, 0);
			xdata = 0;
			/* we have here a multi arch, fetch head offset */
			if (ext_flag & 0x02)
			{
				xdata = (uint8)(data[pos]);
				pos++;
			}
			set_map_face(x, y, 2, face, xdata, ext2, pname3, pcolor, 0);
		}

		if (mask & 0x1)
		{
			face = GetShort_String(data + pos);
			pos += 2;
			request_face(face, 0);
			/*LOG(0, "we got face: %x (%x) ->%s\n", face, face &~ 0x8000, FaceList[face &~ 0x8000].name ? FaceList[face &~ 0x8000].name : "(null)");*/
			xdata = 0;
			/* we have here a multi arch, fetch head offset */
			if (ext_flag & 0x01)
			{
				xdata = (uint8)(data[pos]);
				pos++;
			}
			set_map_face(x, y, 3, face, xdata, ext3, pname4, pcolor, 0);
		}
	}

	adjust_tile_stretch();
	map_udate_flag = 2;
	map_redraw_flag = 1;
}

/**
 * Map scroll command.
 * @param data The incoming data */
void map_scrollCmd(char *data)
{
	static int step = 0;
	static uint32 tick = 0;
	char *buf;

	scrolldx += atoi(data);
	buf = strchr(data, ' ');

	if (!buf)
	{
		LOG(llevError, "ERROR: map_scrollCmd(): Got short packet.\n");
		return;
	}

	buf++;
	scrolldy += atoi(buf);

	if (LastTick - tick > 125)
	{
		step++;

		if (step % 2)
		{
			sound_play_effect(SOUND_STEP1, 0, 100);
		}
		else
		{
			step = 0;
			sound_play_effect(SOUND_STEP2, 0, 100);
		}

		tick = LastTick;
	}
}

/**
 * Magic map command. Currently unused. */
void MagicMapCmd(unsigned char *data, int len)
{
	(void) data;
	(void) len;
}

/**
 * Version command. Currently unused.
 * @param data The incoming data. */
void VersionCmd(char *data)
{
	(void) data;
}

/**
 * Sends version and client name.
 * @param csock Socket to send this information to. */
void SendVersion(ClientSocket csock)
{
	char buf[MAX_BUF];

	snprintf(buf, sizeof(buf), "version %d %s", VERSION_SOCKET, PACKAGE_NAME);
	cs_write_string(csock.fd, buf, strlen(buf));
}

/**
 * Request srv file.
 * @param csock Socket to request from
 * @param index SRV file ID */
void RequestFile(ClientSocket csock, int index)
{
	char buf[MAX_BUF];

	sprintf(buf, "rf %d", index);
	cs_write_string(csock.fd, buf, strlen(buf));
}

/**
 * Send an addme command to the server.
 * @param csock Socket to send the command to. */
void SendAddMe(ClientSocket csock)
{
	cs_write_string(csock.fd, "addme", 5);
}

/**
 * Send set face mode command to the server.
 * @param csock Socket to send the command to
 * @param mode Face mode */
void SendSetFaceMode(ClientSocket csock, int mode)
{
	char buf[MAX_BUF];

	sprintf(buf, "setfacemode %d", mode);
	cs_write_string(csock.fd, buf, strlen(buf));
}

/**
 * Map stats command. Server sent us map data like map name, background music, etc.
 * @param data The incoming data */
void MapstatsCmd(unsigned char *data)
{
	char name[256], bg_music[256];
	char *tmp;
	int w, h, x, y;

	sscanf((char *) data, "%d %d %d %d %s", &w, &h, &x, &y, bg_music);
	tmp = strchr((char *) data, ' ');
	tmp = strchr(tmp + 1, ' ');
	tmp = strchr(tmp + 1, ' ');
	tmp = strchr(tmp + 1, ' ');
	tmp = strchr(tmp + 1, ' ');
	strcpy(name, tmp + 1);
	InitMapData(name, w, h, x, y, bg_music);
	map_udate_flag = 2;

	if (w > 1)
	{
		do_clear_map = 1;
	}
}

/**
 * Skill list command. Used to update player's skill list.
 * @param data The incoming data */
void SkilllistCmd(char *data)
{
	char *tmp, *tmp2, *tmp3, *tmp4;
	int l, e, i, ii, mode;
	char name[256];

#if 0
	LOG(-1,"sklist: %s\n", data);
#endif

	/* We grab our mode */
	mode = atoi(data);

	/* Now look for the members fo the list we have */
	for (; ;)
	{
		/* Find start of a name */
		tmp = strchr(data, '/');

		if (!tmp)
			return;

		data = tmp + 1;

		tmp2 = strchr(data, '/');

		if (tmp2)
		{
			strncpy(name, data, tmp2 - data);
			name[tmp2 - data] = 0;
			data = tmp2;
		}
		else
			strcpy(name, data);

#if 0
		LOG(-1, "sname (%d): >%s<\n", mode, name);
#endif

		tmp3 = strchr(name, '|');
		*tmp3 = 0;
		tmp4 = strchr(tmp3 + 1, '|');

		l = atoi(tmp3 + 1);
		e = atoi(tmp4 + 1);

		/* We have a name, the level and exp - now setup the list */
		for (ii = 0; ii < SKILL_LIST_MAX; ii++)
		{
			for (i = 0; i < DIALOG_LIST_ENTRY; i++)
			{
				/* We have a list entry */
				if (skill_list[ii].entry[i].flag != LIST_ENTRY_UNUSED)
				{
					/* And it is the one we searched for? */
					if (!strcmp(skill_list[ii].entry[i].name, name))
					{
#if 0
						LOG(-1, "skill found (%d): >%s< %d | %d\n", mode, name, l, e);
#endif
						/* Remove? */
						if (mode == SPLIST_MODE_REMOVE)
							skill_list[ii].entry[i].flag = LIST_ENTRY_USED;
						else
						{
							skill_list[ii].entry[i].flag = LIST_ENTRY_KNOWN;
							skill_list[ii].entry[i].exp = e;
							skill_list[ii].entry[i].exp_level = l;
							WIDGET_REDRAW(SKILL_EXP_ID);
						}
					}
				}
			}
		}
	}
}

/**
 * Spell list command. Used to update the player's spell list.
 * @param data The incoming data. */
void SpelllistCmd(char *data)
{
	int mode;
	char *tmp_data, *cp;

	/* We grab our mode */
	mode = atoi(data);

	tmp_data = strdup(data);
	cp = strtok(tmp_data, "/");

	while (cp)
	{
		int i, ii, spell_type, found = 0;
		char *tmp[3];

		if (split_string(cp, tmp, sizeof(tmp) / sizeof(*tmp), ':') != 3)
		{
			cp = strtok(NULL, "/");
			continue;
		}

		/* We have a name - now check the spelllist file and set the entry
		 * to KNOWN */
		for (i = 0; i < SPELL_LIST_MAX && !found; i++)
		{
			for (ii = 0; ii < DIALOG_LIST_ENTRY && !found; ii++)
			{
				for (spell_type = 0; spell_type < SPELL_LIST_CLASS; spell_type++)
				{
					if (spell_list[i].entry[spell_type][ii].flag >= LIST_ENTRY_USED)
					{
						if (!strcmp(spell_list[i].entry[spell_type][ii].name, tmp[0]))
						{
							/* Store the cost */
							spell_list[i].entry[spell_type][ii].cost = atoi(tmp[1]);
							/* Store the path relationship */
							spell_list[i].entry[spell_type][ii].path = tmp[2][0];

							if (mode == SPLIST_MODE_REMOVE)
							{
								spell_list[i].entry[spell_type][ii].flag = LIST_ENTRY_USED;
							}
							else
							{
								spell_list[i].entry[spell_type][ii].flag = LIST_ENTRY_KNOWN;
							}

							found = 1;
							break;
						}
					}
				}
			}
		}

		cp = strtok(NULL, "/");
	}

	free(tmp_data);
}

/**
 * Golem command. Used when casting golem like spells to grab the control of the golem.
 * @param data The incoming data. */
void GolemCmd(unsigned char *data)
{
	int mode, face;
	char *tmp, buf[256];

#if 0
	LOG(llevDebug, "golem: <%s>\n", data);
#endif

	/* We grab our mode */
	mode = atoi((char *)data);

	if (mode == GOLEM_CTR_RELEASE)
	{
		/* Find start of a name */
		tmp = strchr((char *)data, ' ');
		face = atoi(tmp + 1);
		request_face(face, 0);
		/* Find start of a name */
		tmp = strchr(tmp + 1, ' ');
		sprintf(buf, "You lose control of %s.", tmp + 1);
		draw_info(buf, COLOR_WHITE);
		fire_mode_tab[FIRE_MODE_SUMMON].item = FIRE_ITEM_NO;
		fire_mode_tab[FIRE_MODE_SUMMON].name[0] = 0;
	}
	else
	{
		/* Find start of a name */
		tmp = strchr((char *)data, ' ');
		face = atoi(tmp + 1);
		request_face(face, 0);
		/* Find start of a name */
		tmp = strchr(tmp + 1, ' ');
		sprintf(buf, "You get control of %s.", tmp + 1);
		draw_info(buf, COLOR_WHITE);
		fire_mode_tab[FIRE_MODE_SUMMON].item = face;
		strncpy(fire_mode_tab[FIRE_MODE_SUMMON].name, tmp + 1, 100);
		RangeFireMode = FIRE_MODE_SUMMON;
	}
}

/**
 * Save srv file.
 * @param path Path of the file
 * @param data Data to save
 * @param len Length of the data */
static void save_data_cmd_file(char *path, unsigned char *data, int len)
{
	FILE *stream;

	if ((stream = fopen_wrapper(path, "wb")) != NULL)
	{
		if (fwrite(data, 1, len, stream) != (size_t) len)
			LOG(llevError, "ERROR: save_data_cmd_file(): Write of %s failed. (len: %d)\n", path, len);

		fclose(stream);
	}
	else
		LOG(llevError, "ERROR: save_data_cmd_file(): Can't open %s for writing. (len: %d)\n", path, len);
}

/**
 * New char command.
 * Used when server tells us to go to the new character creation. */
void NewCharCmd()
{
	dialog_new_char_warn = 0;
	GameStatus = GAME_STATUS_NEW_CHAR;
}

/**
 * Data command.
 * Used when server sends us block of data, like new srv file.
 * @param data Incoming data
 * @param len Length of the data */
void DataCmd(unsigned char *data, int len)
{
	uint8 data_type = (uint8) (*data);
	uint8 data_comp;
	/* warning! if the uncompressed size of a incoming compressed(!) file is larger
	 * than this dest_len default setting, the file is cut and
	 * the rest skiped. Look at the zlib docu for more info. */
	unsigned long dest_len = 512 * 1024;
	unsigned char *dest;

	dest = malloc(dest_len);
	data_comp = (data_type & DATA_PACKED_CMD);

	len--;
	data++;

	switch (data_type &~ DATA_PACKED_CMD)
	{
		case DATA_CMD_SKILL_LIST:
			/* this is a server send skill list *
			 * uncompress when needed and save it */
			if (data_comp)
			{
				LOG(llevDebug, "data cmd: compressed skill list(len:%d)\n", len);
				uncompress((Bytef *) dest, (uLongf *) &dest_len, (const Bytef *) data, (uLong) len);
				data = dest;
				len = dest_len;
			}

			request_file_chain++;
			save_data_cmd_file(FILE_CLIENT_SKILLS, data, len);
			read_skills();
			break;

		case DATA_CMD_SPELL_LIST:
			if (data_comp)
			{
				LOG(llevDebug, "data cmd: compressed spell list(len:%d)\n", len);
				uncompress((Bytef *) dest, (uLongf *) &dest_len, (const Bytef *) data, (uLong) len);
				data = dest;
				len = dest_len;
			}
			request_file_chain++;
			save_data_cmd_file(FILE_CLIENT_SPELLS, data, len);
			read_spells();
			break;

		case DATA_CMD_SETTINGS_LIST:
			if (data_comp)
			{
				LOG(llevDebug, "data cmd: compressed settings file(len:%d)\n", len);
				uncompress((Bytef *) dest, (uLongf *) &dest_len, (const Bytef *) data, (uLong) len);
				data = dest;
				len = dest_len;
			}
			request_file_chain++;
			save_data_cmd_file(FILE_CLIENT_SETTINGS, data, len);
			/*read_settings();*/
			break;

		case DATA_CMD_BMAP_LIST:
			if (data_comp)
			{
				LOG(llevDebug, "data cmd: compressed bmaps file(len:%d)\n", len);
				uncompress((Bytef *) dest, (uLongf *) &dest_len, (const Bytef *) data, (uLong) len);
				data = dest;
				len = dest_len;
			}
			request_file_chain++;
			save_data_cmd_file(FILE_CLIENT_BMAPS, data, len);
			request_file_flags |= SRV_CLIENT_FLAG_BMAP;
			break;

		case DATA_CMD_ANIM_LIST:
			if (data_comp)
			{
				uncompress((Bytef *) dest, (uLongf *) &dest_len, (const Bytef *) data, (uLong) len);
				LOG(llevDebug, "data cmd: compressed anims file(len:%d) -> %d\n", len, dest_len);
				data = dest;
				len = dest_len;
			}
			request_file_chain++;
			save_data_cmd_file(FILE_CLIENT_ANIMS, data, len);
			request_file_flags |= SRV_CLIENT_FLAG_ANIM;
			break;

		case DATA_CMD_HFILES_LIST:
			if (data_comp)
			{
				uncompress((Bytef *) dest, (uLongf *) &dest_len, (const Bytef *) data, (uLong) len);
				LOG(llevDebug, "data cmd: compressed hfiles file(len:%d) -> %d\n", len, dest_len);
				data = dest;
				len = dest_len;
			}
			request_file_chain++;
			save_data_cmd_file(FILE_CLIENT_HFILES, data, len);
			request_file_flags |= SRV_CLIENT_FLAG_HFILES;
			break;

		default:
			LOG(llevError, "data cmd: unknown type %d (len:%d)\n", data_type, len);
	}

	free(dest);
}

/**
 * Shop command.
 * @param data Data buffer
 * @param len Length of the buffer */
void ShopCmd(unsigned char *data, int len)
{
	/* If we are loading */
	if (strncmp((char *) data, "load|", 5) == 0)
	{
		char *p;

		data += 5;

		/* Only can load once */
		if (shop_gui && shop_gui->shop_items)
		{
			return;
		}

		/* Initialize the shop */
		initialize_shop(SHOP_STATE_BUYING);

		p = strtok((char *) data, "|");

		snprintf(shop_gui->shop_owner, sizeof(shop_gui->shop_owner), "%s", p);

		p = strtok(NULL, "|");

		/* Loop through the data */
		while (p)
		{
			sint32 tag, price;
			int nrof;
			_shop_struct *shop_item_tmp;

			/* Get the tag, nrof and price */
			if (!sscanf(p, "%d:%d:%d", &tag, &nrof, &price))
			{
				return;
			}

			/* Allocate a new shop item */
			shop_item_tmp = (_shop_struct *) malloc(sizeof(_shop_struct));

			/* Set the values */
			shop_item_tmp->nrof = nrof;
			shop_item_tmp->price = price;
			shop_item_tmp->tag = tag;
			shop_item_tmp->next = NULL;

			/* One more item */
			shop_gui->shop_items_count++;

			/* If this is the first item, things are easier */
			if (!shop_gui->shop_items)
			{
				shop_gui->shop_items = shop_item_tmp;
			}
			/* Otherwise we need to calculate the end of the list */
			else
			{
				_shop_struct *shop_item_next = shop_gui->shop_items;
				int i;

				/* Loop until the end of the list */
				for (i = 1; i < shop_gui->shop_items_count - 1 && shop_item_next; i++, shop_item_next = shop_item_next->next)
				{
				}

				/* Append the item to the end of the list */
				shop_item_next->next = shop_item_tmp;
			}

			p = strtok(NULL, "|");
		}
	}
	else if (strncmp((char *) data, "close", 5) == 0)
	{
		clear_shop(0);
	}
	/* Otherwise this is data of a specific item */
	else
	{
		int tag, face, flags, pos = 0, nlen, anim, nrof;
		uint8 animspeed, direction = 0;
		char name[MAX_BUF];

		/* Loop until we reach end of the data */
		while (pos < len)
		{
			/* Get the item tag */
			tag = GetInt_String((unsigned char *) data + pos);
			pos += 4;

			/* Get the flags */
			flags = GetInt_String((unsigned char *) data + pos);
			pos += 4;

			/* Get the face */
			face = GetInt_String((unsigned char *) data + pos);
			pos += 4;

			/* Request the face now */
			request_face(face, 0);

			/* Get the direction the item is facing */
			direction = data[pos++];

			/* Get the item name */
			nlen = data[pos++];
			memcpy(name, data + pos, nlen);
			pos += nlen;
			name[nlen] = '\0';

			/* Get the animation */
			anim = GetShort_String((unsigned char *) data + pos);
			pos += 2;

			/* Get the animation speed */
			animspeed = data[pos++];

			/* Get the number of the items */
			nrof = GetInt_String((unsigned char *) data + pos);
			pos += 4;

			/* Update the item */
			update_item(tag, -2, name, -1, face, flags, anim, animspeed, nrof, 0, 0, 0, 0, 0, 0, direction, 1);
		}

		if (pos > len)
		{
			LOG(llevError, "ERROR: ShopCmd(): Overread buffer: %d > %d\n", pos, len);
		}
	}
}

/**
 * Quest list command.
 *
 * Uses the book GUI to show the quests.
 * @param data Data.
 * @param len Length of the data. */
void QuestListCmd(unsigned char *data, int len)
{
	sound_play_effect(SOUND_BOOK, 0, 100);
	cpl.menustatus = MENU_BOOK;

	data += 4;
	gui_interface_book = book_gui_load((char *) data, len - 4);
}
