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
 * only to other logged in DMs, or those with /dmsay command permission.
 * @param op The object saying this.
 * @param params The message.
 * @return 1 on success, 0 on failure. */
int command_dmsay(object *op, char *params)
{
	player *pl;

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

	for (pl = first_player; pl; pl = pl->next)
	{
		if (can_do_wiz_command(pl, "dmsay"))
		{
			new_draw_info_format(NDI_UNIQUE | NDI_PLAYER | NDI_RED, pl->ob, "[DM Channel]: %s: %s", op->name, params);
		}
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

	if (CONTR(op)->no_shout)
	{
		new_draw_info(NDI_UNIQUE, op, "You are no longer allowed to shout.");
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
		else
		{
			msg = cleanup_chat_string(msg);
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

	if (!msg || *msg == '\0')
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
				new_draw_info_format(NDI_PLAYER | NDI_UNIQUE | NDI_NAVY, op, "You tell %s: %s", name, msg);
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
					play_sound_player_only(CONTR(op), CMD_SOUND_EFFECT, "scroll.ogg", 0, 0, 0, 0);
					return 1;
				}
			}
		}
	}

	play_sound_player_only(CONTR(op), CMD_SOUND_EFFECT, "rod.ogg", 0, 0, 0, 0);
	return 1;
}

/**
 * Look for an emote that is triggered with the command user either
 * targetting someone else other than him, or passing the player name
 * of the target as a parameter.
 * @param op The player.
 * @param target The target object.
 * @param str Name.
 * @param buf Buffer containing string to send to player.
 * @param size Size of 'buf'.
 * @param buf2 Buffer containing string to send to target.
 * @param size2 Size of 'buf2'.
 * @param buf3 Buffer containing string to send to everyone else on map.
 * @param size3 Size of 'buf3'.
 * @param emotion Emotion ID. */
static void emote_other(object *op, object *target, char *str, char *buf, size_t size, char *buf2, size_t size2, char *buf3, size_t size3, int emotion)
{
	const char *name = str;

	if (target && target->name)
	{
		name = target->name;
	}

	switch (emotion)
	{
		case EMOTE_NOD:
			snprintf(buf, size, "You nod solemnly to %s.", name);
			snprintf(buf2, size2, "%s nods solemnly to you.", op->name);
			snprintf(buf3, size3, "%s nods solemnly to %s.", op->name, name);
			break;

		case EMOTE_DANCE:
			snprintf(buf, size, "You grab %s and begin doing the Cha-Cha!", name);
			snprintf(buf2, size2, "%s grabs you, and begins dancing!", op->name);
			snprintf(buf3, size3, "Yipe! %s and %s are doing the Macarena!", op->name, name);
			break;

		case EMOTE_KISS:
			snprintf(buf, size, "You kiss %s.", name);
			snprintf(buf2, size2, "%s kisses you.", op->name);
			snprintf(buf3, size3, "%s kisses %s.", op->name, name);
			break;

		case EMOTE_BOUNCE:
			snprintf(buf, size, "You bounce around the room with %s.", name);
			snprintf(buf2, size2, "%s bounces around the room with you.", op->name);
			snprintf(buf3, size3, "%s bounces around the room with %s.", op->name, name);
			break;

		case EMOTE_SMILE:
			snprintf(buf, size, "You smile at %s.", name);
			snprintf(buf2, size2, "%s smiles at you.", op->name);
			snprintf(buf3, size3, "%s beams a smile at %s.", op->name, name);
			break;

		case EMOTE_LAUGH:
			snprintf(buf, size, "You take one look at %s and fall down laughing.", name);
			snprintf(buf2, size2, "%s looks at you and falls down on the ground laughing.", op->name);
			snprintf(buf3, size3, "%s looks at %s and falls down on the ground laughing.", op->name, name);
			break;

		case EMOTE_SHAKE:
			snprintf(buf, size, "You shake %s's hand.", name);
			snprintf(buf2, size2, "%s shakes your hand.", op->name);
			snprintf(buf3, size3, "%s shakes %s's hand.", op->name, name);
			break;

		case EMOTE_PUKE:
			snprintf(buf, size, "You puke on %s.", name);
			snprintf(buf2, size2, "%s pukes on your clothes!", op->name);
			snprintf(buf3, size3, "%s pukes on %s.", op->name, name);
			break;

		case EMOTE_HUG:
			snprintf(buf, size, "You hug %s.", name);
			snprintf(buf2, size2, "%s hugs you.", op->name);
			snprintf(buf3, size3, "%s hugs %s.", op->name, name);
			break;

		case EMOTE_CRY:
			snprintf(buf, size, "You cry on %s's shoulder.", name);
			snprintf(buf2, size2, "%s cries on your shoulder.", op->name);
			snprintf(buf3, size3, "%s cries on %s's shoulder.", op->name, name);
			break;

		case EMOTE_POKE:
			snprintf(buf, size, "You poke %s in the ribs.", name);
			snprintf(buf2, size2, "%s pokes you in the ribs.", op->name);
			snprintf(buf3, size3, "%s pokes %s in the ribs.", op->name, name);
			break;

		case EMOTE_ACCUSE:
			snprintf(buf, size, "You look accusingly at %s.", name);
			snprintf(buf2, size2, "%s looks accusingly at you.", op->name);
			snprintf(buf3, size3, "%s looks accusingly at %s.", op->name, name);
			break;

		case EMOTE_GRIN:
			snprintf(buf, size, "You grin at %s.", name);
			snprintf(buf2, size2, "%s grins evilly at you.", op->name);
			snprintf(buf3, size3, "%s grins evilly at %s.", op->name, name);
			break;

		case EMOTE_BOW:
			snprintf(buf, size, "You bow before %s.", name);
			snprintf(buf2, size2, "%s bows before you.", op->name);
			snprintf(buf3, size3, "%s bows before %s.", op->name, name);
			break;

		case EMOTE_FROWN:
			snprintf(buf, size, "You frown darkly at %s.", name);
			snprintf(buf2, size2, "%s frowns darkly at you.", op->name);
			snprintf(buf3, size3, "%s frowns darkly at %s.", op->name, name);
			break;

		case EMOTE_GLARE:
			snprintf(buf, size, "You glare icily at %s.", name);
			snprintf(buf2, size2, "%s glares icily at you, you feel cold to your bones.", op->name);
			snprintf(buf3, size3, "%s glares at %s.", op->name, name);
			break;

		case EMOTE_LICK:
			snprintf(buf, size, "You lick %s.", name);
			snprintf(buf2, size2, "%s licks you.", op->name);
			snprintf(buf3, size3, "%s licks %s.", op->name, name);
			break;

		case EMOTE_SHRUG:
			snprintf(buf, size, "You shrug at %s.", name);
			snprintf(buf2, size2, "%s shrugs at you.", op->name);
			snprintf(buf3, size3, "%s shrugs at %s.", op->name, name);
			break;

		case EMOTE_SLAP:
			snprintf(buf, size, "You slap %s.", name);
			snprintf(buf2, size2, "You are slapped by %s.", op->name);
			snprintf(buf3, size3, "%s slaps %s.", op->name, name);
			break;

		case EMOTE_SNEEZE:
			snprintf(buf, size, "You sneeze and a film of snot shoots onto %s.", name);
			snprintf(buf2, size2, "%s sneezes on you, you feel the snot cover you. EEEEEEW.", op->name);
			snprintf(buf3, size3, "%s sneezes and a film of snot covers %s.", op->name, name);
			break;

		case EMOTE_SNIFF:
			snprintf(buf, size, "You sniff %s.", name);
			snprintf(buf2, size2, "%s sniffs you.", op->name);
			snprintf(buf3, size3, "%s sniffs %s.", op->name, name);
			break;

		case EMOTE_SPIT:
			snprintf(buf, size, "You spit on %s.", name);
			snprintf(buf2, size2, "%s spits in your face!", op->name);
			snprintf(buf3, size3, "%s spits in %s's face.", op->name, name);
			break;

		case EMOTE_THANK:
			snprintf(buf, size, "You thank %s heartily.", name);
			snprintf(buf2, size2, "%s thanks you heartily.", op->name);
			snprintf(buf3, size3, "%s thanks %s heartily.", op->name, name);
			break;

		case EMOTE_WAVE:
			snprintf(buf, size, "You wave goodbye to %s.", name);
			snprintf(buf2, size2, "%s waves goodbye to you. Have a good journey.", op->name);
			snprintf(buf3, size3, "%s waves goodbye to %s.", op->name, name);
			break;

		case EMOTE_WHISTLE:
			snprintf(buf, size, "You whistle at %s.", name);
			snprintf(buf2, size2, "%s whistles at you.", op->name);
			snprintf(buf3, size3, "%s whistles at %s.", op->name, name);
			break;

		case EMOTE_WINK:
			snprintf(buf, size, "You wink suggestively at %s.", name);
			snprintf(buf2, size2, "%s winks suggestively at you.", op->name);
			snprintf(buf3, size3, "%s winks at %s.", op->name, name);
			break;

		case EMOTE_BEG:
			snprintf(buf, size, "You beg %s for mercy.", name);
			snprintf(buf2, size2, "%s begs you for mercy! Show no quarter!", op->name);
			snprintf(buf3, size3, "%s begs %s for mercy!", op->name, name);
			break;

		case EMOTE_BLEED:
			snprintf(buf, size, "You slash your wrist and bleed all over %s.", name);
			snprintf(buf2, size2, "%s slashes %s wrist and bleeds all over you.", op->name, gender_possessive[object_get_gender(op)]);
			snprintf(buf3, size3, "%s slashes %s wrist and bleeds all over %s.", op->name, gender_possessive[object_get_gender(op)], name);
			break;

		case EMOTE_CRINGE:
			snprintf(buf, size, "You cringe away from %s.", name);
			snprintf(buf2, size2, "%s cringes away from you.", op->name);
			snprintf(buf3, size3, "%s cringes away from %s in mortal terror.", op->name, name);
			break;

		default:
			snprintf(buf, size, "You are still nuts.");
			snprintf(buf2, size2, "You get the distinct feeling that %s is nuts.", op->name);
			snprintf(buf3, size3, "%s is eyeing %s quizzically.", name, op->name);
			break;
	}
}

/**
 * Look for an emote that is triggered with the command user being the
 * target, hence the name 'self'.
 * @param op The player doing the emotion.
 * @param buf Buffer containing string to send to player doing the emotion.
 * @param size Size of 'buf'.
 * @param buf2 Buffer containing string to send to everyone else on map.
 * @param size2 Size of 'buf2'.
 * @param emotion Emotion ID. */
static void emote_self(object *op, char *buf, size_t size, char *buf2, size_t size2, int emotion)
{
	switch (emotion)
	{
		case EMOTE_DANCE:
			snprintf(buf, size, "You skip and dance around by yourself.");
			snprintf(buf2, size2, "%s embraces %s and begins to dance!", op->name, gender_reflexive[object_get_gender(op)]);
			break;

		case EMOTE_LAUGH:
			snprintf(buf, size, "Laugh at yourself all you want, the others won't understand.");
			snprintf(buf2, size2, "%s is laughing at something.", op->name);
			break;

		case EMOTE_SHAKE:
			snprintf(buf, size, "You are shaken by yourself.");
			snprintf(buf2, size2, "%s shakes and quivers like a bowlful of jelly.", op->name);
			break;

		case EMOTE_PUKE:
			snprintf(buf, size, "You puke on yourself.");
			snprintf(buf2, size2, "%s pukes on %s clothes.", op->name, gender_possessive[object_get_gender(op)]);
			break;

		case EMOTE_HUG:
			snprintf(buf, size, "You hug yourself.");
			snprintf(buf2, size2, "%s hugs %s.", op->name, gender_reflexive[object_get_gender(op)]);
			break;

		case EMOTE_CRY:
			snprintf(buf, size, "You cry to yourself.");
			snprintf(buf2, size2, "%s sobs quietly to %s.", op->name, gender_reflexive[object_get_gender(op)]);
			break;

		case EMOTE_POKE:
			snprintf(buf, size, "You poke yourself in the ribs, feeling very silly.");
			snprintf(buf2, size2, "%s pokes %s in the ribs, looking very sheepish.", op->name, gender_reflexive[object_get_gender(op)]);
			break;

		case EMOTE_ACCUSE:
			snprintf(buf, size, "You accuse yourself.");
			snprintf(buf2, size2, "%s seems to have a bad conscience.", op->name);
			break;

		case EMOTE_BOW:
			snprintf(buf, size, "You kiss your toes.");
			snprintf(buf2, size2, "%s folds up like a jackknife and kisses %s own toes.", op->name, gender_possessive[object_get_gender(op)]);
			break;

		case EMOTE_FROWN:
			snprintf(buf, size, "You frown at yourself.");
			snprintf(buf2, size2, "%s frowns at %s.", op->name, gender_reflexive[object_get_gender(op)]);
			break;

		case EMOTE_GLARE:
			snprintf(buf, size, "You glare icily at your feet, they are suddenly very cold.");
			snprintf(buf2, size2, "%s glares at %s feet, what is bothering him?", op->name, gender_possessive[object_get_gender(op)]);
			break;

		case EMOTE_LICK:
			snprintf(buf, size, "You lick yourself.");
			snprintf(buf2, size2, "%s licks %s - YUCK.", op->name, gender_reflexive[object_get_gender(op)]);
			break;

		case EMOTE_SLAP:
			snprintf(buf, size, "You slap yourself, silly you.");
			snprintf(buf2, size2, "%s slaps %s, really strange...", op->name, gender_reflexive[object_get_gender(op)]);
			break;

		case EMOTE_SNEEZE:
			snprintf(buf, size, "You sneeze on yourself, what a mess!");
			snprintf(buf2, size2, "%s sneezes, and covers %s in a slimy substance.", op->name, gender_reflexive[object_get_gender(op)]);
			break;

		case EMOTE_SNIFF:
			snprintf(buf, size, "You sniff yourself.");
			snprintf(buf2, size2, "%s sniffs %s.", op->name, gender_reflexive[object_get_gender(op)]);
			break;

		case EMOTE_SPIT:
			snprintf(buf, size, "You drool all over yourself.");
			snprintf(buf2, size2, "%s drools all over %s.", op->name, gender_reflexive[object_get_gender(op)]);
			break;

		case EMOTE_THANK:
			snprintf(buf, size, "You thank yourself since nobody else wants to!");
			snprintf(buf2, size2, "%s thanks %s since you won't.", op->name, gender_reflexive[object_get_gender(op)]);
			break;

		case EMOTE_WAVE:
			snprintf(buf, size, "Are you going on adventures as well??");
			snprintf(buf2, size2, "%s waves goodbye to %s.", op->name, gender_reflexive[object_get_gender(op)]);
			break;

		case EMOTE_WHISTLE:
			snprintf(buf, size, "You whistle while you work.");
			snprintf(buf2, size2, "%s whistles to %s in boredom.", op->name, gender_reflexive[object_get_gender(op)]);
			break;

		case EMOTE_WINK:
			snprintf(buf, size, "You wink at yourself?? What are you up to?");
			snprintf(buf2, size2, "%s winks at %s - something strange is going on...", op->name, gender_reflexive[object_get_gender(op)]);
			break;

		case EMOTE_BLEED:
			snprintf(buf, size, "Very impressive! You wipe your blood all over yourself.");
			snprintf(buf2, size2, "%s performs some satanic ritual while wiping %s blood on %s.", op->name, gender_possessive[object_get_gender(op)], gender_reflexive[object_get_gender(op)]);
			break;

		default:
			snprintf(buf, size, "My god! is that LEGAL?");
			snprintf(buf2, size2, "You look away from %s.", op->name);
			break;
	}
}

/**
 * Emotes player can do with no target.
 * @param op The player doing the emotion.
 * @param buf Buffer containing string to send to everyone else on map.
 * @param size Size of 'buf'.
 * @param buf2 Buffer containing string to send to player doing the emotion.
 * @param size2 Size of 'buf2'.
 * @param emotion Emotion ID. */
static void emote_no_target(object *op, char *buf, size_t size, char *buf2, size_t size2, int emotion)
{
	switch (emotion)
	{
		case EMOTE_NOD:
			snprintf(buf, size, "%s nods solemnly.", op->name);
			snprintf(buf2, size2, "You nod solemnly.");
			break;

		case EMOTE_DANCE:
			snprintf(buf, size, "%s expresses %s through interpretive dance.", op->name, gender_reflexive[object_get_gender(op)]);
			snprintf(buf2, size2, "You dance with glee.");
			break;

		case EMOTE_KISS:
			snprintf(buf, size, "%s makes a weird facial contortion", op->name);
			snprintf(buf2, size2, "All the lonely people..");
			break;

		case EMOTE_BOUNCE:
			snprintf(buf, size, "%s bounces around.", op->name);
			snprintf(buf2, size2, "BOIINNNNNNGG!");
			break;

		case EMOTE_SMILE:
			snprintf(buf, size, "%s smiles happily.", op->name);
			snprintf(buf2, size2, "You smile happily.");
			break;

		case EMOTE_CACKLE:
			snprintf(buf, size, "%s throws back %s head and cackles with insane glee!", op->name, gender_possessive[object_get_gender(op)]);
			snprintf(buf2, size2, "You cackle gleefully.");
			break;

		case EMOTE_LAUGH:
			snprintf(buf, size, "%s falls down laughing.", op->name);
			snprintf(buf2, size2, "You fall down laughing.");
			break;

		case EMOTE_GIGGLE:
			snprintf(buf, size, "%s giggles.", op->name);
			snprintf(buf2, size2, "You giggle.");
			break;

		case EMOTE_SHAKE:
			snprintf(buf, size, "%s shakes %s head.", op->name, gender_possessive[object_get_gender(op)]);
			snprintf(buf2, size2, "You shake your head.");
			break;

		case EMOTE_PUKE:
			snprintf(buf, size, "%s pukes.", op->name);
			snprintf(buf2, size2, "Bleaaaaaghhhhhhh!");
			break;

		case EMOTE_GROWL:
			snprintf(buf, size, "%s growls.", op->name);
			snprintf(buf2, size2, "Grrrrrrrrr....");
			break;

		case EMOTE_SCREAM:
			snprintf(buf, size, "%s screams at the top of %s lungs!", op->name, gender_possessive[object_get_gender(op)]);
			snprintf(buf2, size2, "ARRRRRRRRRRGH!!!!!");
			break;

		case EMOTE_SIGH:
			snprintf(buf, size, "%s sighs loudly.", op->name);
			snprintf(buf2, size2, "You sigh.");
			break;

		case EMOTE_SULK:
			snprintf(buf, size, "%s sulks in the corner.", op->name);
			snprintf(buf2, size2, "You sulk.");
			break;

		case EMOTE_CRY:
			snprintf(buf, size, "%s bursts into tears.", op->name);
			snprintf(buf2, size2, "Waaaaaaahhh..");
			break;

		case EMOTE_GRIN:
			snprintf(buf, size, "%s grins evilly.", op->name);
			snprintf(buf2, size2, "You grin evilly.");
			break;

		case EMOTE_BOW:
			snprintf(buf, size, "%s bows deeply.", op->name);
			snprintf(buf2, size2, "You bow deeply.");
			break;

		case EMOTE_CLAP:
			snprintf(buf, size, "%s gives a round of applause.", op->name);
			snprintf(buf2, size2, "Clap, clap, clap.");
			break;

		case EMOTE_BLUSH:
			snprintf(buf, size, "%s blushes.", op->name);
			snprintf(buf2, size2, "Your cheeks are burning.");
			break;

		case EMOTE_BURP:
			snprintf(buf, size, "%s burps loudly.", op->name);
			snprintf(buf2, size2, "You burp loudly.");
			break;

		case EMOTE_CHUCKLE:
			snprintf(buf, size, "%s chuckles politely.", op->name);
			snprintf(buf2, size2, "You chuckle politely");
			break;

		case EMOTE_COUGH:
			snprintf(buf, size, "%s coughs loudly.", op->name);
			snprintf(buf2, size2, "Yuck, try to cover your mouth next time!");
			break;

		case EMOTE_FLIP:
			snprintf(buf, size, "%s flips head over heels.", op->name);
			snprintf(buf2, size2, "You flip head over heels.");
			break;

		case EMOTE_FROWN:
			snprintf(buf, size, "%s frowns.", op->name);
			snprintf(buf2, size2, "What's bothering you?");
			break;

		case EMOTE_GASP:
			snprintf(buf, size, "%s gasps in astonishment.", op->name);
			snprintf(buf2, size2, "You gasp in astonishment.");
			break;

		case EMOTE_GLARE:
			snprintf(buf, size, "%s glares around him.", op->name);
			snprintf(buf2, size2, "You glare at nothing in particular.");
			break;

		case EMOTE_GROAN:
			snprintf(buf, size, "%s groans loudly.", op->name);
			snprintf(buf2, size2, "You groan loudly.");
			break;

		case EMOTE_HICCUP:
			snprintf(buf, size, "%s hiccups.", op->name);
			snprintf(buf2, size2, "*HIC*");
			break;

		case EMOTE_LICK:
			snprintf(buf, size, "%s licks %s mouth and smiles.", op->name, gender_possessive[object_get_gender(op)]);
			snprintf(buf2, size2, "You lick your mouth and smile.");
			break;

		case EMOTE_POUT:
			snprintf(buf, size, "%s pouts.", op->name);
			snprintf(buf2, size2, "Aww, don't take it so hard.");
			break;

		case EMOTE_SHIVER:
			snprintf(buf, size, "%s shivers uncomfortably.", op->name);
			snprintf(buf2, size2, "Brrrrrrrrr.");
			break;

		case EMOTE_SHRUG:
			snprintf(buf, size, "%s shrugs helplessly.", op->name);
			snprintf(buf2, size2, "You shrug.");
			break;

		case EMOTE_SMIRK:
			snprintf(buf, size, "%s smirks.", op->name);
			snprintf(buf2, size2, "You smirk.");
			break;

		case EMOTE_SNAP:
			snprintf(buf, size, "%s snaps %s fingers.", op->name, gender_possessive[object_get_gender(op)]);
			snprintf(buf2, size2, "PRONTO! You snap your fingers.");
			break;

		case EMOTE_SNEEZE:
			snprintf(buf, size, "%s sneezes.", op->name);
			snprintf(buf2, size2, "Gesundheit!");
			break;

		case EMOTE_SNICKER:
			snprintf(buf, size, "%s snickers softly.", op->name);
			snprintf(buf2, size2, "You snicker softly.");
			break;

		case EMOTE_SNIFF:
			snprintf(buf, size, "%s sniffs sadly.", op->name);
			snprintf(buf2, size2, "You sniff sadly. *SNIFF*");
			break;

		case EMOTE_SNORE:
			snprintf(buf, size, "%s snores loudly.", op->name);
			snprintf(buf2, size2, "Zzzzzzzzzzzzzzz.");
			break;

		case EMOTE_SPIT:
			snprintf(buf, size, "%s spits over %s left shoulder.", op->name, gender_possessive[object_get_gender(op)]);
			snprintf(buf2, size2, "You spit over your left shoulder.");
			break;

		case EMOTE_STRUT:
			snprintf(buf, size, "%s struts proudly.", op->name);
			snprintf(buf2, size2, "Strut your stuff.");
			break;

		case EMOTE_TWIDDLE:
			snprintf(buf, size, "%s patiently twiddles %s thumbs.", op->name, gender_possessive[object_get_gender(op)]);
			snprintf(buf2, size2, "You patiently twiddle your thumbs.");
			break;

		case EMOTE_WAVE:
			snprintf(buf, size, "%s waves happily.", op->name);
			snprintf(buf2, size2, "You wave.");
			break;

		case EMOTE_WHISTLE:
			snprintf(buf, size, "%s whistles appreciatively.", op->name);
			snprintf(buf2, size2, "You whistle appreciatively.");
			break;

		case EMOTE_WINK:
			snprintf(buf, size, "%s winks suggestively.", op->name);
			snprintf(buf2, size2, "Have you got something in your eye?");
			break;

		case EMOTE_YAWN:
			snprintf(buf, size, "%s yawns sleepily.", op->name);
			snprintf(buf2, size2, "You open up your yap and let out a big breeze of stale air.");
			break;

		case EMOTE_CRINGE:
			snprintf(buf, size, "%s cringes in terror!", op->name);
			snprintf(buf2, size2, "You cringe in terror.");
			break;

		case EMOTE_BLEED:
			snprintf(buf, size, "%s is bleeding all over the carpet - got a spare tourniquet?", op->name);
			snprintf(buf2, size2, "You bleed all over your nice new armour.");
			break;

		case EMOTE_THINK:
			snprintf(buf, size, "%s closes %s eyes and thinks really hard.", op->name, gender_possessive[object_get_gender(op)]);
			snprintf(buf2, size2, "Anything in particular that you'd care to think about?");
			break;

		default:
			snprintf(buf, size, "%s dances with glee.", op->name);
			snprintf(buf2, size2, "You are nuts.");
			break;
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
 * @param op Player doing the emotion.
 * @param params Possible parameters for the emotion (target name for the
 * emotion, message for /me, etc).
 * @param emotion Emotion code, one of @ref EMOTE_xxx.
 * @return 0. */
static int basic_emote(object *op, char *params, int emotion)
{
	char buf[MAX_BUF], buf2[MAX_BUF], buf3[MAX_BUF];

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
		/* If we are a player with legal target, use it as target for the
		 * emote. */
		if (op->type == PLAYER && CONTR(op)->target_object != op && OBJECT_VALID(CONTR(op)->target_object, CONTR(op)->target_object_count) && CONTR(op)->target_object->name)
		{
			rv_vector rv;

			if (get_rangevector(op, CONTR(op)->target_object, &rv, 0) && rv.distance <= 4)
			{
				emote_other(op, CONTR(op)->target_object, NULL, buf, sizeof(buf), buf2, sizeof(buf2), buf3, sizeof(buf3), emotion);
				new_draw_info(NDI_YELLOW | NDI_PLAYER, op, buf);

				if (CONTR(op)->target_object->type == PLAYER)
				{
					new_draw_info(NDI_YELLOW | NDI_PLAYER, CONTR(op)->target_object, buf2);
				}

				new_info_map_except(NDI_YELLOW | NDI_EMOTE | NDI_PLAYER, op->map, op->x, op->y, MAP_INFO_NORMAL, op, CONTR(op)->target_object, buf3);
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

			return 0;
		}

		emote_no_target(op, buf, sizeof(buf), buf2, sizeof(buf2), emotion);

		if (op->type == PLAYER)
		{
			new_info_map_except(NDI_YELLOW | NDI_EMOTE | NDI_PLAYER, op->map, op->x, op->y, MAP_INFO_NORMAL, op, op, buf);
			new_draw_info(NDI_YELLOW | NDI_PLAYER, op, buf2);
		}
		else
		{
			new_info_map_except(NDI_YELLOW | NDI_EMOTE, op->map, op->x, op->y, MAP_INFO_NORMAL, op, op, buf);
		}

		return 0;
	}
	/* We have params. */
	else
	{
		if (emotion == EMOTE_ME)
		{
			snprintf(buf, sizeof(buf), "%s %s", op->name, params);
			new_info_map_except(NDI_YELLOW | NDI_EMOTE | NDI_PLAYER, op->map, op->x, op->y, MAP_INFO_NORMAL, op, op, buf);

			if (op->type == PLAYER)
			{
				new_draw_info(NDI_YELLOW | NDI_PLAYER, op, buf);
			}

			return 0;
		}
		/* Here we handle player and NPC emotes with parameter. */
		else
		{
			player *pl;

			if (op->type == PLAYER && strcmp(op->name, params) == 0)
			{
				emote_self(op, buf, sizeof(buf), buf2, sizeof(buf2), emotion);
				new_draw_info(NDI_YELLOW | NDI_PLAYER, op, buf);
				new_info_map_except(NDI_YELLOW | NDI_EMOTE | NDI_PLAYER, op->map, op->x, op->y, MAP_INFO_NORMAL, op, op, buf2);
				return 0;
			}

			pl = find_player(params);

			if (pl && pl->state == ST_PLAYING && !pl->dm_stealth)
			{
				rv_vector rv;

				if (get_rangevector(op, pl->ob, &rv, 0) && rv.distance <= 4)
				{
					if (op->type == PLAYER)
					{
						emote_other(op, pl->ob, NULL, buf, sizeof(buf), buf2, sizeof(buf2), buf3, sizeof(buf3), emotion);
						new_draw_info(NDI_YELLOW | NDI_PLAYER, op, buf);
						new_draw_info(NDI_YELLOW | NDI_EMOTE | NDI_PLAYER, pl->ob, buf2);
						new_info_map_except(NDI_YELLOW | NDI_EMOTE | NDI_PLAYER, op->map, op->x, op->y, MAP_INFO_NORMAL, op, pl->ob, buf3);
					}
					else
					{
						emote_other(op, NULL, params, buf, sizeof(buf), buf2, sizeof(buf2), buf3, sizeof(buf3), emotion);
						new_draw_info(NDI_YELLOW | NDI_EMOTE | NDI_PLAYER, pl->ob, buf2);
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
				emote_self(op, buf, sizeof(buf), buf2, sizeof(buf2), -1);
				new_draw_info(NDI_YELLOW | NDI_PLAYER, op, buf);
				new_info_map_except(NDI_YELLOW | NDI_EMOTE | NDI_PLAYER, op->map, op->x, op->y, MAP_INFO_NORMAL, op, op, buf2);
			}
			else
			{
				emote_other(op, NULL, params, buf, sizeof(buf), buf2, sizeof(buf2), buf3, sizeof(buf3), emotion);
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
