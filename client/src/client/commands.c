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
 * Handles commands received by the server. This does not necessarily
 * handle all the commands - some might be in other files. */

#include <global.h>

/** @copydoc socket_command_struct::handle_func */
void socket_command_book(uint8 *data, size_t len, size_t pos)
{
	sound_play_effect("book.ogg", 100);
	book_load((char *) data, len);
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_setup(uint8 *data, size_t len, size_t pos)
{
	uint8 type;

	server_files_clear_update();

	while (pos < len)
	{
		type = packet_to_uint8(data, len, &pos);

		if (type == CMD_SETUP_SOUND)
		{
			packet_to_uint8(data, len, &pos);
		}
		else if (type == CMD_SETUP_MAPSIZE)
		{
			int x, y;

			x = packet_to_uint8(data, len, &pos);
			y = packet_to_uint8(data, len, &pos);

			setting_set_int(OPT_CAT_MAP, OPT_MAP_WIDTH, x);
			setting_set_int(OPT_CAT_MAP, OPT_MAP_HEIGHT, y);
		}
		else if (type == CMD_SETUP_SERVER_FILE)
		{
			uint8 file_type, state;

			file_type = packet_to_uint8(data, len, &pos);
			state = packet_to_uint8(data, len, &pos);

			if (state)
			{
				server_files_mark_update(file_type);
			}
		}
	}

	if (GameStatus != GAME_STATUS_PLAY)
	{
		GameStatus = GAME_STATUS_REQUEST_FILES;
	}
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_anim(uint8 *data, size_t len, size_t pos)
{
	uint16 anim_id;
	int i;

	anim_id = packet_to_uint16(data, len, &pos);
	animations[anim_id].flags = packet_to_uint8(data, len, &pos);
	animations[anim_id].facings = packet_to_uint8(data, len, &pos);
	animations[anim_id].num_animations = (len - pos) / 2;

	if (animations[anim_id].facings > 1)
	{
		animations[anim_id].frame = animations[anim_id].num_animations / animations[anim_id].facings;
	}
	else
	{
		animations[anim_id].frame = animations[anim_id].num_animations;
	}

	animations[anim_id].faces = malloc(sizeof(uint16) * animations[anim_id].num_animations);

	for (i = 0; pos < len; i++)
	{
		animations[anim_id].faces[i] = packet_to_uint16(data, len, &pos);
		request_face(animations[anim_id].faces[i]);
	}
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_image(uint8 *data, size_t len, size_t pos)
{
	uint32 facenum, filesize;
	char buf[HUGE_BUF];
	FILE *fp;

	facenum = packet_to_uint32(data, len, &pos);
	filesize = packet_to_uint32(data, len, &pos);

	/* Save picture to cache and load it to FaceList. */
	snprintf(buf, sizeof(buf), DIRECTORY_CACHE"/%s", FaceList[facenum].name);

	fp = fopen_wrapper(buf, "wb+");

	if (fp)
	{
		fwrite(data + pos, 1, filesize, fp);
		fclose(fp);
	}

	FaceList[facenum].sprite = sprite_tryload_file(buf, 0, NULL);
	map_udate_flag = 2;
	map_redraw_flag = 1;

	book_redraw();
	interface_redraw();
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_skill_ready(uint8 *data, size_t len, size_t pos)
{
	size_t type, id;

	packet_to_string(data, len, &pos, cpl.skill_name, sizeof(cpl.skill_name));
	cpl.skill = NULL;

	if (skill_find(cpl.skill_name, &type, &id))
	{
		skill_entry_struct *skill;

		skill = skill_get(type, id);

		if (skill->known)
		{
			cpl.skill = skill;
		}
	}

	WIDGET_REDRAW_ALL(SKILL_EXP_ID);
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_drawinfo(uint8 *data, size_t len, size_t pos)
{
	uint16 flags;
	char color[COLOR_BUF], *str;
	StringBuffer *sb;

	flags = packet_to_uint16(data, len, &pos);
	packet_to_string(data, len, &pos, color, sizeof(color));
	sb = stringbuffer_new();

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

		if (timelen != 0)
		{
			stringbuffer_append_printf(sb, "[%s] ", timebuf);
		}
	}

	packet_to_stringbuffer(data, len, &pos, sb);
	str = stringbuffer_finish(sb);

	if (flags & NDI_ANIM)
	{
		strncpy(msg_anim.message, str, sizeof(msg_anim.message) - 1);
		msg_anim.tick = LastTick;
		strncpy(msg_anim.color, color, sizeof(msg_anim.color) - 1);
		msg_anim.color[sizeof(msg_anim.color) - 1] = '\0';
	}

	draw_info_flags(color, flags, str);

	free(str);
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_target(uint8 *data, size_t len, size_t pos)
{
	cpl.target_mode = packet_to_uint8(data, len, &pos);

	if (cpl.target_mode)
	{
		sound_play_effect("weapon_attack.ogg", 100);
	}
	else
	{
		sound_play_effect("weapon_hold.ogg", 100);
	}

	cpl.target_code = packet_to_uint8(data, len, &pos);
	packet_to_string(data, len, &pos, cpl.target_color, sizeof(cpl.target_color));
	packet_to_string(data, len, &pos, cpl.target_name, sizeof(cpl.target_name));

	map_udate_flag = 2;
	map_redraw_flag = 1;
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_stats(uint8 *data, size_t len, size_t pos)
{
	uint8 type;
	int temp;

	while (pos < len)
	{
		type = packet_to_uint8(data, len, &pos);

		if (type >= CS_STAT_PROT_START && type <= CS_STAT_PROT_END)
		{
			cpl.stats.protection[type - CS_STAT_PROT_START] = packet_to_sint8(data, len, &pos);
			WIDGET_REDRAW_ALL(RESIST_ID);
		}
		else
		{
			switch (type)
			{
				case CS_STAT_TARGET_HP:
					cpl.target_hp = packet_to_uint8(data, len, &pos);
					break;

				case CS_STAT_REG_HP:
					cpl.gen_hp = abs(packet_to_uint16(data, len, &pos)) / 10.0f;
					WIDGET_REDRAW_ALL(REGEN_ID);
					break;

				case CS_STAT_REG_MANA:
					cpl.gen_sp = abs(packet_to_uint16(data, len, &pos)) / 10.0f;
					WIDGET_REDRAW_ALL(REGEN_ID);
					break;

				case CS_STAT_REG_GRACE:
					cpl.gen_grace = abs(packet_to_uint16(data, len, &pos)) / 10.0f;
					WIDGET_REDRAW_ALL(REGEN_ID);
					break;

				case CS_STAT_HP:
					temp = packet_to_uint32(data, len, &pos);

					if (temp < cpl.stats.hp && cpl.stats.food)
					{
						cpl.warn_hp = 1;

						if (cpl.stats.maxhp / 12 <= cpl.stats.hp - temp)
						{
							cpl.warn_hp = 2;
						}
					}

					cpl.stats.hp = temp;
					WIDGET_REDRAW_ALL(STATS_ID);
					break;

				case CS_STAT_MAXHP:
					cpl.stats.maxhp = packet_to_uint32(data, len, &pos);
					WIDGET_REDRAW_ALL(STATS_ID);
					break;

				case CS_STAT_SP:
					cpl.stats.sp = packet_to_uint16(data, len, &pos);
					WIDGET_REDRAW_ALL(STATS_ID);
					break;

				case CS_STAT_MAXSP:
					cpl.stats.maxsp = packet_to_uint16(data, len, &pos);
					WIDGET_REDRAW_ALL(STATS_ID);
					break;

				case CS_STAT_GRACE:
					cpl.stats.grace = packet_to_uint16(data, len, &pos);
					WIDGET_REDRAW_ALL(STATS_ID);
					break;

				case CS_STAT_MAXGRACE:
					cpl.stats.maxgrace = packet_to_uint16(data, len, &pos);
					WIDGET_REDRAW_ALL(STATS_ID);
					break;

				case CS_STAT_STR:
					temp = packet_to_uint8(data, len, &pos);

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
					temp = packet_to_uint8(data, len, &pos);

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
					temp = packet_to_uint8(data, len, &pos);

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
					temp = packet_to_uint8(data, len, &pos);

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
					temp = packet_to_uint8(data, len, &pos);

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
					temp = packet_to_uint8(data, len, &pos);

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
					temp = packet_to_uint8(data, len, &pos);

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
					cpl.stats.exp = packet_to_uint64(data, len, &pos);
					WIDGET_REDRAW_ALL(MAIN_LVL_ID);
					break;

				case CS_STAT_LEVEL:
					cpl.stats.level = packet_to_uint8(data, len, &pos);
					WIDGET_REDRAW_ALL(MAIN_LVL_ID);
					break;

				case CS_STAT_WC:
					cpl.stats.wc = packet_to_uint16(data, len, &pos);
					break;

				case CS_STAT_AC:
					cpl.stats.ac = packet_to_uint16(data, len, &pos);
					break;

				case CS_STAT_DAM:
					cpl.stats.dam = packet_to_uint16(data, len, &pos);
					break;

				case CS_STAT_SPEED:
					cpl.stats.speed = packet_to_uint32(data, len, &pos);
					break;

				case CS_STAT_FOOD:
					cpl.stats.food = packet_to_uint16(data, len, &pos);
					WIDGET_REDRAW_ALL(STATS_ID);
					break;

				case CS_STAT_WEAP_SP:
					cpl.stats.weapon_sp = packet_to_uint8(data, len, &pos);
					break;

				case CS_STAT_FLAGS:
					cpl.stats.flags = packet_to_uint16(data, len, &pos);
					break;

				case CS_STAT_WEIGHT_LIM:
					set_weight_limit(packet_to_uint32(data, len, &pos));
					break;

				case CS_STAT_ACTION_TIME:
					cpl.action_timer = abs(packet_to_uint32(data, len, &pos)) / 1000.0f;
					WIDGET_REDRAW_ALL(SKILL_EXP_ID);
					break;

				case CS_STAT_GENDER:
					cpl.gender = packet_to_uint8(data, len, &pos);
					break;

				case CS_STAT_EXT_TITLE:
				{
					packet_to_string(data, len, &pos, cpl.ext_title, sizeof(cpl.ext_title));
					break;
				}

				case CS_STAT_RANGED_DAM:
					cpl.stats.ranged_dam = packet_to_uint16(data, len, &pos);
					break;

				case CS_STAT_RANGED_WC:
					cpl.stats.ranged_wc = packet_to_uint16(data, len, &pos);
					break;

				case CS_STAT_RANGED_WS:
					cpl.stats.ranged_ws = packet_to_uint32(data, len, &pos);
					break;
			}
		}
	}
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_query(uint8 *data, size_t len, size_t pos)
{
	uint8 type;

	type = packet_to_uint8(data, len, &pos);

	if (type == CMD_QUERY_GET_NAME)
	{
		cpl.name[0] = '\0';
		cpl.password[0] = '\0';
		GameStatus = GAME_STATUS_NAME;
	}
	else if (type == CMD_QUERY_GET_PASSWORD)
	{
		GameStatus = GAME_STATUS_PSWD;
	}
	else if (type == CMD_QUERY_CONFIRM_PASSWORD)
	{
		GameStatus = GAME_STATUS_VERIFYPSWD;
	}

	if (GameStatus >= GAME_STATUS_NAME && GameStatus <= GAME_STATUS_VERIFYPSWD)
	{
		text_input_open(64);
	}
}

/**
 * Sends a reply to the server.
 * @param text Null terminated string of text to send. */
void send_reply(char *text)
{
	packet_struct *packet;

	packet = packet_new(SERVER_CMD_REPLY, 64, 0);
	packet_append_string_terminated(packet, text);
	socket_send_packet(packet);
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_player(uint8 *data, size_t len, size_t pos)
{
	int tag, weight, face;

	GameStatus = GAME_STATUS_PLAY;
	text_input_string_end_flag = 0;

	tag = packet_to_uint32(data, len, &pos);
	weight = packet_to_uint32(data, len, &pos);
	face = packet_to_uint32(data, len, &pos);
	request_face(face);

	new_player(tag, weight, face);
	map_udate_flag = 2;
	map_redraw_flag = 1;

	ignore_list_load();
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_itemx(uint8 *data, size_t len, size_t pos)
{
	int weight, loc, tag, face, flags, anim, nrof, dmode;
	uint8 itype, stype, item_qua, item_con, item_skill, item_level;
	uint8 animspeed, direction = 0;
	char name[MAX_BUF];

	itype = stype = item_qua = item_con = item_skill = item_level = 0;

	dmode = packet_to_sint32(data, len, &pos);
	loc = packet_to_sint32(data, len, &pos);

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

	while (pos < len)
	{
		tag = packet_to_uint32(data, len, &pos);
		flags = packet_to_uint32(data, len, &pos);
		weight = packet_to_uint32(data, len, &pos);
		face = packet_to_uint32(data, len, &pos);
		request_face(face);
		direction = packet_to_uint8(data, len, &pos);

		if (loc)
		{
			itype = packet_to_uint8(data, len, &pos);
			stype = packet_to_uint8(data, len, &pos);
			item_qua = packet_to_uint8(data, len, &pos);
			item_con = packet_to_uint8(data, len, &pos);
			item_level = packet_to_uint8(data, len, &pos);
			item_skill = packet_to_uint8(data, len, &pos);
		}

		packet_to_string(data, len, &pos, name, sizeof(name));
		anim = packet_to_uint16(data, len, &pos);;
		animspeed = packet_to_uint8(data, len, &pos);;
		nrof = packet_to_uint32(data, len, &pos);;
		update_object(tag, loc, name, weight, face, flags, anim, animspeed, nrof, itype, stype, item_qua, item_con, item_skill, item_level, direction, 0);
	}

	map_udate_flag = 2;
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_itemy(uint8 *data, size_t len, size_t pos)
{
	int weight, loc, tag, face, flags, anim, nrof, dmode;
	uint8 itype, stype, item_qua, item_con, item_skill, item_level;
	uint8 animspeed, direction = 0;
	char name[MAX_BUF];

	itype = stype = item_qua = item_con = item_skill = item_level = 0;

	dmode = packet_to_sint32(data, len, &pos);
	loc = packet_to_uint32(data, len, &pos);

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

	while (pos < len)
	{
		tag = packet_to_uint32(data, len, &pos);
		flags = packet_to_uint32(data, len, &pos);
		weight = packet_to_uint32(data, len, &pos);
		face = packet_to_uint32(data, len, &pos);
		request_face(face);
		direction = packet_to_uint8(data, len, &pos);

		if (loc)
		{
			itype = packet_to_uint8(data, len, &pos);
			stype = packet_to_uint8(data, len, &pos);
			item_qua = packet_to_uint8(data, len, &pos);
			item_con = packet_to_uint8(data, len, &pos);
			item_level = packet_to_uint8(data, len, &pos);
			item_skill = packet_to_uint8(data, len, &pos);
		}

		packet_to_string(data, len, &pos, name, sizeof(name));

		anim = packet_to_uint16(data, len, &pos);
		animspeed = packet_to_uint8(data, len, &pos);
		nrof = packet_to_uint32(data, len, &pos);
		update_object(tag, loc, name, weight, face, flags, anim, animspeed, nrof, itype, stype, item_qua, item_con, item_skill, item_level, direction, 1);
	}

	map_udate_flag = 2;
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_item_update(uint8 *data, size_t len, size_t pos)
{
	int weight, loc, tag, face, sendflags, flags, anim, nrof;
	uint8 direction;
	char name[MAX_BUF];
	object *ip;
	uint8 animspeed;

	sendflags = packet_to_uint16(data, len, &pos);
	tag = packet_to_uint32(data, len, &pos);
	ip = object_find(tag);

	if (!ip)
	{
		return;
	}

	name[0] = '\0';
	loc = ip->env ? ip->env->tag : 0;
	weight = (int) (ip->weight * 1000);
	face = ip->face;
	request_face(face);
	flags = ip->flags;
	anim = ip->animation_id;
	animspeed = ip->anim_speed;
	nrof = ip->nrof;
	direction = ip->direction;

	if (sendflags & UPD_LOCATION)
	{
		loc = packet_to_uint32(data, len, &pos);
	}

	if (sendflags & UPD_FLAGS)
	{
		flags = packet_to_uint32(data, len, &pos);
	}

	if (sendflags & UPD_WEIGHT)
	{
		weight = packet_to_uint32(data, len, &pos);
	}

	if (sendflags & UPD_FACE)
	{
		face = packet_to_uint32(data, len, &pos);
		request_face(face);
	}

	if (sendflags & UPD_DIRECTION)
	{
		direction = packet_to_uint8(data, len, &pos);
	}

	if (sendflags & UPD_NAME)
	{
		packet_to_string(data, len, &pos, name, sizeof(name));
	}

	if (sendflags & UPD_ANIM)
	{
		anim = packet_to_uint16(data, len, &pos);
	}

	if (sendflags & UPD_ANIMSPEED)
	{
		animspeed = packet_to_uint8(data, len, &pos);
	}

	if (sendflags & UPD_NROF)
	{
		nrof = packet_to_uint32(data, len, &pos);
	}

	update_object(tag, loc, name, weight, face, flags, anim, animspeed, nrof, 254, 254, 254, 254, 254, 254, direction, 0);
	map_udate_flag = 2;
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_item_delete(uint8 *data, size_t len, size_t pos)
{
	tag_t tag;

	while (pos < len)
	{
		tag = packet_to_uint32(data, len, &pos);
		delete_object(tag);
	}

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

/** @copydoc socket_command_struct::handle_func */
void socket_command_mapstats(uint8 *data, size_t len, size_t pos)
{
	uint8 type;
	char buf[HUGE_BUF];

	while (pos < len)
	{
		/* Get the type of this command... */
		type = packet_to_uint8(data, len, &pos);

		/* Change map name. */
		if (type == CMD_MAPSTATS_NAME)
		{
			packet_to_string(data, len, &pos, buf, sizeof(buf));
			update_map_name(buf);
		}
		/* Change map music. */
		else if (type == CMD_MAPSTATS_MUSIC)
		{
			packet_to_string(data, len, &pos, buf, sizeof(buf));
			update_map_bg_music(buf);
		}
		/* Change map weather. */
		else if (type == CMD_MAPSTATS_WEATHER)
		{
			packet_to_string(data, len, &pos, buf, sizeof(buf));
			update_map_weather(buf);
		}
	}
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_map(uint8 *data, size_t len, size_t pos)
{
	static int mx = 0, my = 0;
	int mask, x, y;
	int mapstat;
	int xpos, ypos;
	int layer, ext_flags;
	uint8 num_layers;

	mapstat = packet_to_uint8(data, len, &pos);

	if (mapstat != MAP_UPDATE_CMD_SAME)
	{
		char mapname[HUGE_BUF], bg_music[HUGE_BUF], weather[MAX_BUF];

		packet_to_string(data, len, &pos, mapname, sizeof(mapname));
		packet_to_string(data, len, &pos, bg_music, sizeof(bg_music));
		packet_to_string(data, len, &pos, weather, sizeof(weather));

		if (mapstat == MAP_UPDATE_CMD_NEW)
		{
			int map_w, map_h;

			map_w = packet_to_uint8(data, len, &pos);
			map_h = packet_to_uint8(data, len, &pos);
			xpos = packet_to_uint8(data, len, &pos);
			ypos = packet_to_uint8(data, len, &pos);
			mx = xpos;
			my = ypos;
			init_map_data(map_w, map_h, xpos, ypos);
		}
		else
		{
			int xoff, yoff;

			mapstat = packet_to_uint8(data, len, &pos);
			xoff = packet_to_sint8(data, len, &pos);
			yoff = packet_to_sint8(data, len, &pos);
			xpos = packet_to_uint8(data, len, &pos);
			ypos = packet_to_uint8(data, len, &pos);
			mx = xpos;
			my = ypos;
			display_mapscroll(xoff, yoff);

			map_play_footstep();
		}

		update_map_name(mapname);
		update_map_bg_music(bg_music);
		update_map_weather(weather);
	}
	else
	{
		xpos = packet_to_uint8(data, len, &pos);
		ypos = packet_to_uint8(data, len, &pos);

		/* Have we moved? */
		if ((xpos - mx || ypos - my))
		{
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
		mask = packet_to_uint16(data, len, &pos);
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
			map_set_darkness(x, y, packet_to_uint8(data, len, &pos));
		}

		num_layers = packet_to_uint8(data, len, &pos);

		/* Go through all the layers on this tile. */
		for (layer = 0; layer < num_layers; layer++)
		{
			uint8 type;

			type = packet_to_uint8(data, len, &pos);

			/* Clear this layer. */
			if (type == MAP2_LAYER_CLEAR)
			{
				map_set_data(x, y, packet_to_uint8(data, len, &pos), 0, 0, 0, "", "", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
			}
			/* We have some data. */
			else
			{
				sint16 face, height = 0, zoom_x = 0, zoom_y = 0, align = 0, rotate = 0;
				uint8 flags, obj_flags, quick_pos = 0, probe = 0, draw_double = 0, alpha = 0, infravision = 0, target_is_friend = 0;
				char player_name[64], player_color[COLOR_BUF];
				uint32 target_object_count = 0;

				player_name[0] = '\0';
				player_color[0] = '\0';

				face = packet_to_uint16(data, len, &pos);
				/* Request the face. */
				request_face(face);
				/* Object flags. */
				obj_flags = packet_to_uint8(data, len, &pos);
				/* Flags of this layer. */
				flags = packet_to_uint8(data, len, &pos);

				/* Multi-arch? */
				if (flags & MAP2_FLAG_MULTI)
				{
					quick_pos = packet_to_uint8(data, len, &pos);
				}

				/* Player name? */
				if (flags & MAP2_FLAG_NAME)
				{
					packet_to_string(data, len, &pos, player_name, sizeof(player_name));
					packet_to_string(data, len, &pos, player_color, sizeof(player_color));
				}

				/* Target's HP? */
				if (flags & MAP2_FLAG_PROBE)
				{
					probe = packet_to_uint8(data, len, &pos);
				}

				/* Z position? */
				if (flags & MAP2_FLAG_HEIGHT)
				{
					height = packet_to_sint16(data, len, &pos);
				}

				/* Zoom? */
				if (flags & MAP2_FLAG_ZOOM)
				{
					zoom_x = packet_to_uint16(data, len, &pos);
					zoom_y = packet_to_uint16(data, len, &pos);
				}

				/* Align? */
				if (flags & MAP2_FLAG_ALIGN)
				{
					align = packet_to_sint16(data, len, &pos);
				}

				/* Double? */
				if (flags & MAP2_FLAG_DOUBLE)
				{
					draw_double = 1;
				}

				if (flags & MAP2_FLAG_MORE)
				{
					uint32 flags2;

					flags2 = packet_to_uint32(data, len, &pos);

					if (flags2 & MAP2_FLAG2_ALPHA)
					{
						alpha = packet_to_uint8(data, len, &pos);
					}

					if (flags2 & MAP2_FLAG2_ROTATE)
					{
						rotate = packet_to_sint16(data, len, &pos);
					}

					if (flags2 & MAP2_FLAG2_INFRAVISION)
					{
						infravision = 1;
					}

					if (flags2 & MAP2_FLAG2_TARGET)
					{
						target_object_count = packet_to_uint32(data, len, &pos);
						target_is_friend = packet_to_uint8(data, len, &pos);
					}
				}

				/* Set the data we figured out. */
				map_set_data(x, y, type, face, quick_pos, obj_flags, player_name, player_color, height, probe, zoom_x, zoom_y, align, draw_double, alpha, rotate, infravision, target_object_count, target_is_friend);
			}
		}

		/* Get tile flags. */
		ext_flags = packet_to_uint8(data, len, &pos);

		/* Animation? */
		if (ext_flags & MAP2_FLAG_EXT_ANIM)
		{
			uint8 anim_type;
			sint16 anim_value;

			anim_type = packet_to_uint8(data, len, &pos);
			anim_value = packet_to_uint16(data, len, &pos);

			add_anim(anim_type, xpos + x, ypos + y, anim_value);
		}
	}

	adjust_tile_stretch();
	map_udate_flag = 2;
	map_redraw_flag = 1;
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_version(uint8 *data, size_t len, size_t pos)
{
	cpl.server_socket_version = packet_to_uint32(data, len, &pos);
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_new_char(uint8 *data, size_t len, size_t pos)
{
	GameStatus = GAME_STATUS_NEW_CHAR;
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_data(uint8 *data, size_t len, size_t pos)
{
	uint8 data_type;
	unsigned long len_ucomp;
	unsigned char *dest;

	data_type = packet_to_uint8(data, len, &pos);
	len_ucomp = packet_to_uint32(data, len, &pos);
	len -= pos;
	/* Allocate large enough buffer to hold the uncompressed file. */
	dest = malloc(len_ucomp);

	uncompress((Bytef *) dest, (uLongf *) &len_ucomp, (const Bytef *) data + pos, (uLong) len);
	server_file_save(data_type, dest, len_ucomp);
	free(dest);
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_item_ready(uint8 *data, size_t len, size_t pos)
{
	uint8 type;
	tag_t tag;

	type = packet_to_uint8(data, len, &pos);
	tag = packet_to_uint32(data, len, &pos);

	if (type == READY_OBJ_ARROW)
	{
		fire_mode_tab[FIRE_MODE_BOW].amun = tag;
	}
	else if (type == READY_OBJ_THROW)
	{
		fire_mode_tab[FIRE_MODE_THROW].item = tag;
	}
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_compressed(uint8 *data, size_t len, size_t pos)
{
	unsigned long ucomp_len;
	uint8 type, *dest;
	size_t dest_size;
	command_buffer *buf;

	type = packet_to_uint8(data, len, &pos);
	ucomp_len = packet_to_uint32(data, len, &pos);

	dest_size = ucomp_len + 1;
	dest = malloc(dest_size);

	if (!dest)
	{
		logger_print(LOG(ERROR), "OOM.");
	}

	dest[0] = type;
	uncompress((Bytef *) dest + 1, (uLongf *) &ucomp_len, (const Bytef *) data + pos, (uLong) len - pos);

	buf = command_buffer_new(ucomp_len + 1, dest);
	add_input_command(buf);

	free(dest);
}
