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
 * This file handles communication related functions, like /dmsay, /say,
 * /shout, etc.*/

#include <global.h>

/**
 * This function does three things:
 * -# Controls that we have a legal string; if not, return NULL
 * -# Removes all left whitespace (if all whitespace return NULL)
 * -# Change and/or process all control characters like '^', '~', etc.
 * @param ustring The string to cleanup
 * @return Cleaned up string, or NULL */
char *cleanup_chat_string(char *ustring)
{
	int i;

	if (!ustring)
	{
		return NULL;
	}

	/* This happens when whitespace only string was submitted. */
	if (!ustring || *ustring == '\0')
	{
		return NULL;
	}

	/* Now clear all special characters. */
	for (i = 0; *(ustring + i) != '\0'; i++)
	{
		if (*(ustring + i) == '~' || *(ustring + i) == '^' || *(ustring + i) == '|')
		{
			*(ustring + i) = ' ';
		}
	}

	/* Kill all whitespace. */
	while (*ustring != '\0' && isspace(*ustring))
	{
		ustring++;
	}

	return ustring;
}

/**
 * Say command, used to say a message for the whole map to hear.
 * @param op The object saying this.
 * @param params The message.
 * @return 1 on success, 0 on failure. */
int command_say(object *op, char *params)
{
	if (!params)
	{
		return 0;
	}

	LOG(llevInfo, "CLOG SAY:%s >%s<\n", query_name(op, NULL), params);

	params = cleanup_chat_string(params);

	/* This happens when whitespace only string was submitted. */
	if (!params || *params == '\0')
	{
		return 0;
	}

	communicate(op, params);

	return 1;
}

/**
 * This command is only available to DMs.
 *
 * It is similar to /shout, however, it will display the message in red
 * only to other logged in DMs using ::dm_list.
 * @param op The object saying this.
 * @param params The message.
 * @return 1 on success, 0 on failure. */
int command_dmsay(object *op, char *params)
{
	objectlink *ol;

	if (!params)
	{
		return 0;
	}

	LOG(llevInfo, "CLOG DMSAY:%s >%s<\n", query_name(op, NULL), params);

	params = cleanup_chat_string(params);

	/* This can happen when whitespace only string was sent */
	if (!params || *params == '\0')
	{
		return 0;
	}

	for (ol = dm_list; ol; ol = ol->next)
	{
		new_draw_info_format(NDI_UNIQUE | NDI_PLAYER | NDI_RED, ol->objlink.ob, "[DM Channel]: %s: %s", op->name, params);
	}

	return 1;
}

/**
 * Shout command, used to shout a message for everyone to hear.
 * @param op The object saying this
 * @param params The message
 * @return 1 on success, 0 on failure */
int command_shout(object *op, char *params)
{
	if (!params)
	{
		return 0;
	}

	LOG(llevInfo, "CLOG SHOUT:%s >%s<\n", query_name(op, NULL), params);

	params = cleanup_chat_string(params);

	/* This happens when whitespace only string was submitted. */
	if (!params || *params == '\0')
	{
		return 0;
	}

	new_draw_info_format(NDI_PLAYER | NDI_UNIQUE | NDI_ALL | NDI_ORANGE | NDI_SHOUT, NULL, "%s shouts: %s", op->name, params);

	/* Trigger the global SHOUT event. */
	trigger_global_event(EVENT_SHOUT, op, params);

	return 1;
}

/**
 * Tell message to a single player.
 * @param op The object doing the command.
 * @param params The player name to send message to, and the message.
 * @return 1 on success, 0 on failure */
int command_tell(object *op, char *params)
{
	const char *name_hash;
	char *name = NULL, *msg = NULL;
	player *pl;

	if (!params)
	{
		return 0;
	}

	LOG(llevInfo, "CLOG TELL:%s >%s<\n", query_name(op, NULL), params);

	params = cleanup_chat_string(params);

	/* This happens when whitespace only string was submitted. */
	if (!params || *params == '\0')
	{
		return 0;
	}

	name = params;
	msg = strchr(name, ' ');

	if (msg)
	{
		*(msg++) = '\0';

		if (*msg == '\0')
		{
			msg = NULL;
		}
	}

	if (!name)
	{
		new_draw_info(NDI_UNIQUE, op, "Tell whom what?");
		return 1;
	}

	adjust_player_name(name);
	name_hash = find_string(name);

	if (!name_hash)
	{
		new_draw_info(NDI_UNIQUE, op, "No such player.");
		return 1;
	}

	if (!msg)
	{
		new_draw_info_format(NDI_UNIQUE, op, "Tell %s what?", name);
		return 1;
	}

	/* Send to yourself? Intelligent... */
	if (op->name == name_hash)
	{
		new_draw_info(NDI_UNIQUE, op, "You tell yourself the news. Very smart.");
		return 1;
	}

	for (pl = first_player; pl; pl = pl->next)
	{
		if (pl->ob->name == name_hash)
		{
			if (pl->dm_stealth)
			{
				new_draw_info_format(NDI_PLAYER | NDI_UNIQUE | NDI_NAVY | NDI_TELL, pl->ob, "%s tells you (dm_stealth): %s", op->name, msg);
				/* We send it but we kick the "no such player" on. */
				break;
			}
			else
			{
				new_draw_info_format(NDI_PLAYER | NDI_UNIQUE, op, "You tell %s: %s", name, msg);
				new_draw_info_format(NDI_PLAYER | NDI_UNIQUE | NDI_NAVY | NDI_TELL, pl->ob, "%s tells you: %s", op->name, msg);
				return 1;
			}
		}
	}

	new_draw_info(NDI_UNIQUE, op, "No such player.");
	return 1;
}

/**
 * Command to tell something to a target.
 *
 * Usually used by the talk button in the client.
 * @param op The object doing this command.
 * @param params The message.
 * @return 1 on success, 0 on failure. */
int command_t_tell(object *op, char *params)
{
	object *t_obj;
	int i, xt, yt;
	mapstruct *m;

	if (!params)
	{
		return 0;
	}

	params = cleanup_chat_string(params);

	/* This happens when whitespace only string was submitted. */
	if (!params || *params == '\0')
	{
		return 0;
	}

	if (op->type == PLAYER)
	{
		t_obj = CONTR(op)->target_object;

		if (t_obj && CONTR(op)->target_object_count == t_obj->count)
		{
			/* Why do I do this and not direct distance calculation?
			 * Because the player perhaps has left the mapset with the
			 * target which will invoke some nasty searchings. */
			for (i = 0; i <= SIZEOFFREE2; i++)
			{
				xt = op->x + freearr_x[i];
				yt = op->y + freearr_y[i];

				if (!(m = get_map_from_coord(op->map, &xt, &yt)))
				{
					continue;
				}

				if (m == t_obj->map && xt == t_obj->x && yt == t_obj->y)
				{
					LOG(llevInfo, "CLOG T_TELL:%s >%s<\n", query_name(op, NULL), params);
					new_draw_info_format(NDI_UNIQUE | NDI_WHITE, op, "You say to %s: %s", query_name(t_obj, NULL), params);
					talk_to_npc(op, t_obj, params);
					play_sound_player_only(CONTR(op), SOUND_CLICK, SOUND_NORMAL, 0, 0);
					return 1;
				}
			}
		}
	}

	play_sound_player_only(CONTR(op), SOUND_WAND_POOF, SOUND_NORMAL, 0, 0);
	return 1;
}

/**
 * Look for an emote that is triggered with the command user either
 * targetting someone else other than him, or passing the player name
 * of the target as a parameter.
 * @param op The player.
 * @param target The target object.
 * @param str Name.
 * @param buf First buffer.
 * @param buf2 Second buffer.
 * @param buf3 Third buffer.
 * @param emotion Emotion ID. */
static void emote_other(object *op, object *target, char *str, char *buf, char *buf2, char *buf3, int emotion)
{
	const char *name = str;
	emotes_array *emote;

	if (target && target->name)
	{
		name = target->name;
	}

	emote = find_emote(emotion, emotes_other, emotes_other_size);

	if (emote)
	{
		sprintf(buf, emote->text1, name);
		sprintf(buf2, emote->text2, op->name);
		sprintf(buf3, emote->text3, op->name, name);
	}
	else
	{
		sprintf(buf, "You are still nuts.");
		sprintf(buf2, "You get the distinct feeling that %s is nuts.", op->name);
		sprintf(buf3, "%s is eyeing %s quizzically.", name, op->name);
	}
}

/**
 * Look for an emote that is triggered with the command user being the
 * target, hence the name 'self'.
 * @param op The player.
 * @param buf First buffer.
 * @param buf2 Second buffer.
 * @param emotion Emotion ID. */
static void emote_self(object *op, char *buf, char *buf2, int emotion)
{
	emotes_array *emote;

	/* Find the emotion. */
	emote = find_emote(emotion, emotes_self, emotes_self_size);

	/* Found it? */
	if (emote)
	{
		sprintf(buf, emote->text1, op->name);
		sprintf(buf2, emote->text2, op->name);
	}
	else
	{
		sprintf(buf, "My god! Is that LEGAL?");
		sprintf(buf2, "You look away from %s.", op->name);
	}
}

/**
 * This function covers all the emotion commands a player can issue.
 *
 * Based on the situation, it detects which emotion and how it should be
 * performed.
 *
 * If you want to add a new emotion command, you will need to modify one
 * of the emote arrays in @ref commands.c, add the emotion ID to
 * @ref commands.h and add a command wrapper to this function, similar
 * to the functions below this one.
 * @param op Player.
 * @param params Message.
 * @param emotion Emotion code, one of @ref EMOTE_xxx.
 * @return 0 on invalid emotion, 1 otherwise. */
static int basic_emote(object *op, char *params, int emotion)
{
	char buf[HUGE_BUF], buf2[HUGE_BUF], buf3[HUGE_BUF];

	LOG(llevDebug, "EMOTE: %s (params: >%s<) (t: %s) %d\n", query_name(op, NULL), params ? params : "NULL", CONTR(op) ? query_name(CONTR(op)->target_object, NULL) : "NULL", emotion);

	params = cleanup_chat_string(params);

	if (params && *params == '\0')
	{
		params = NULL;
	}
	else if (params)
	{
		if (emotion != EMOTE_ME && op->type == PLAYER)
		{
			adjust_player_name(params);
		}
	}

	if (!params)
	{
		emotes_array *emote;

		/* If we are a player with legal target, use it as target for the
		 * emote. */
		if (op->type == PLAYER && CONTR(op)->target_object != op && OBJECT_VALID(CONTR(op)->target_object, CONTR(op)->target_object_count) && CONTR(op)->target_object->name)
		{
			rv_vector rv;
			get_rangevector(op, CONTR(op)->target_object, &rv, 0);

			if (rv.distance <= 4)
			{
				emote_other(op, CONTR(op)->target_object, NULL, buf, buf2, buf3, emotion);
				new_draw_info(NDI_UNIQUE, op, buf);

				if (CONTR(op)->target_object->type == PLAYER)
				{
					new_draw_info(NDI_UNIQUE | NDI_YELLOW, CONTR(op)->target_object, buf2);
				}

				new_info_map_except(NDI_YELLOW | NDI_EMOTE, op->map, op->x, op->y, MAP_INFO_NORMAL, op, CONTR(op)->target_object, buf3);
				return 0;
			}

			new_draw_info(NDI_UNIQUE, op, "The target is not in range for this emote action.");
			return 0;
		}

		if (emotion == EMOTE_ME)
		{
			if (op->type == PLAYER)
			{
				new_draw_info(NDI_UNIQUE, op, "Usage: /me <emote to display>");
			}

			return 1;
		}

		emote = find_emote(emotion, emotes_no_target, emotes_no_target_size);

		if (emote)
		{
			snprintf(buf, sizeof(buf), emote->text1, op->name);
			snprintf(buf2, sizeof(buf2), emote->text2, op->name);
		}
		else
		{
			snprintf(buf, sizeof(buf), "%s dances with glee.", op->name);
			snprintf(buf2, sizeof(buf2), "You are nuts.");
		}

		new_info_map_except(NDI_YELLOW | NDI_EMOTE, op->map, op->x, op->y, MAP_INFO_NORMAL, op, op, buf);

		if (op->type == PLAYER)
		{
			new_draw_info(NDI_UNIQUE, op, buf2);
		}

		return 0;
	}
	/* We have params. */
	else
	{
		if (emotion == EMOTE_ME)
		{
			snprintf(buf, sizeof(buf), "%s %s", op->name, params);
			new_info_map_except(NDI_YELLOW | NDI_EMOTE, op->map, op->x, op->y, MAP_INFO_NORMAL, op, op, buf);

			if (op->type == PLAYER)
			{
				new_draw_info(NDI_UNIQUE, op, buf);
			}

			return 0;
		}
		/* Here we handle player and NPC emotes with parameter. */
		else
		{
			player *pl;

			if (op->type == PLAYER && strcmp(op->name, params) == 0)
			{
				emote_self(op, buf, buf2, emotion);
				new_draw_info(NDI_UNIQUE, op, buf);
				new_info_map_except(NDI_YELLOW | NDI_EMOTE, op->map, op->x, op->y, MAP_INFO_NORMAL, op, op, buf2);

				return 0;
			}

			pl = find_player(params);

			if (pl && pl->state == ST_PLAYING && !pl->dm_stealth)
			{
				rv_vector rv;

				get_rangevector(op, pl->ob, &rv, 0);

				emote_other(op, NULL, params, buf, buf2, buf3, emotion);

				if (rv.distance <= 4)
				{
					if (op->type == PLAYER)
					{
						emote_other(op, pl->ob, NULL, buf, buf2, buf3, emotion);
						new_draw_info(NDI_UNIQUE, op, buf);
						new_draw_info(NDI_UNIQUE | NDI_YELLOW | NDI_EMOTE, pl->ob, buf2);
						new_info_map_except(NDI_YELLOW | NDI_EMOTE, op->map, op->x, op->y, MAP_INFO_NORMAL, op, pl->ob, buf3);
					}
					else
					{
						new_draw_info(NDI_UNIQUE | NDI_YELLOW | NDI_EMOTE, pl->ob, buf2);
						new_info_map_except(NDI_YELLOW | NDI_EMOTE, op->map, op->x, op->y, MAP_INFO_NORMAL, NULL, pl->ob, buf3);
					}
				}
				else if (op->type == PLAYER)
				{
					new_draw_info(NDI_UNIQUE, op, "The target is not in range for this emote action.");
				}

				return 0;
			}

			if (op->type == PLAYER)
			{
				emote_self(op, buf, buf2, -1);
				new_draw_info(NDI_UNIQUE, op, buf);
				new_info_map_except(NDI_YELLOW | NDI_EMOTE, op->map, op->x, op->y, MAP_INFO_NORMAL, op, op, buf2);
			}
			else
			{
				emote_other(op, NULL, params, buf, buf2, buf3, emotion);
				new_info_map_except(NDI_YELLOW | NDI_EMOTE, op->map, op->x, op->y, MAP_INFO_NORMAL, NULL, NULL, buf3);
			}
		}
	}

	return 0;
}

/** @cond */

int command_nod(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_NOD);
}

int command_dance(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_DANCE);
}

int command_kiss(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_KISS);
}

int command_bounce(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_BOUNCE);
}

int command_smile(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_SMILE);
}

int command_cackle(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_CACKLE);
}

int command_laugh(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_LAUGH);
}

int command_giggle(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_GIGGLE);
}

int command_shake(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_SHAKE);
}

int command_puke(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_PUKE);
}

int command_growl(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_GROWL);
}

int command_scream(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_SCREAM);
}

int command_sigh(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_SIGH);
}

int command_sulk(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_SULK);
}

int command_hug(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_HUG);
}

int command_cry(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_CRY);
}

int command_poke(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_POKE);
}

int command_accuse(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_ACCUSE);
}

int command_grin(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_GRIN);
}

int command_bow(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_BOW);
}

int command_clap(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_CLAP);
}

int command_blush(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_BLUSH);
}

int command_burp(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_BURP);
}

int command_chuckle(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_CHUCKLE);
}

int command_cough(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_COUGH);
}

int command_flip(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_FLIP);
}

int command_frown(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_FROWN);
}

int command_gasp(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_GASP);
}

int command_glare(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_GLARE);
}

int command_groan(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_GROAN);
}

int command_hiccup(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_HICCUP);
}

int command_lick(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_LICK);
}

int command_pout(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_POUT);
}

int command_shiver(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_SHIVER);
}

int command_shrug(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_SHRUG);
}

int command_slap(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_SLAP);
}

int command_smirk(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_SMIRK);
}

int command_snap(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_SNAP);
}

int command_sneeze(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_SNEEZE);
}

int command_snicker(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_SNICKER);
}

int command_sniff(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_SNIFF);
}

int command_snore(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_SNORE);
}

int command_spit(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_SPIT);
}

int command_strut(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_STRUT);
}

int command_thank(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_THANK);
}

int command_twiddle(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_TWIDDLE);
}

int command_wave(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_WAVE);
}

int command_whistle(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_WHISTLE);
}

int command_wink(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_WINK);
}

int command_yawn(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_YAWN);
}

int command_beg(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_BEG);
}

int command_bleed(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_BLEED);
}

int command_cringe(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_CRINGE);
}

int command_think(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_THINK);
}

int command_me(object *op, char *params)
{
	return basic_emote(op, params, EMOTE_ME);
}

/** @endcond */
