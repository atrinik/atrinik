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

#include <global.h>
#include <sproto.h>

/**
 * @file
 * This file handles communication related functions, like /dmsay, /say, /shout, etc.*/

/**
 * This function does 3 things:
 *  1. Controls that we have a legal string; if not, return NULL
 *  2. Removes all left whitespace (if all whitespace return NULL)
 *  3. Change and/or process all control characters like '^', '~', etc.
 * @param ustring The string to cleanup
 * @return Cleaned up string, or NULL */
char *cleanup_chat_string(char *ustring)
{
	int i;

	if (!ustring)
		return NULL;

	/* this happens when whitespace only string was submitted */
	if (!ustring || *ustring == '\0')
		return NULL;

	/* now clear all control chars */
	for (i = 0; *(ustring + i) != '\0'; i++)
	{
		if (*(ustring + i) == '~' || *(ustring + i) == '^' || *(ustring + i) == '|')
			*(ustring + i) = ' ';
	}

	/* kill all whitespace */
	while (*ustring != '\0' && isspace(*ustring))
		ustring++;

	return ustring;
}

/**
 * Say command, used to say a message for the whole map to hear.
 * @param op The object saying this
 * @param params The message
 * @return 1 on success, 0 on failure */
int command_say(object *op, char *params)
{
	if (!params)
		return 0;

	LOG(llevInfo, "CLOG SAY:%s >%s<\n", query_name(op, NULL), params);

	params = cleanup_chat_string(params);
	/* this happens when whitespace only string was submitted */
	if (!params || *params == '\0')
		return 0;

	communicate(op, params);

	return 1;
}

/* This command is only available to DMs.
 * It is similar to /shout, however, it will display
 * the message in red to other logged in DMs. */
/**
 * This command is only available to DMs.
 * It is similar to /shout, however, it will display
 * the message in red only to other logged in DMs
 * using global DMs active linked list.
 * @param op The object saying this
 * @param params The message
 * @return 1 on success, 0 on failure */
int command_dmsay(object *op, char *params)
{
	active_DMs *tmp_dm_list;

	if (!params)
		return 0;

	LOG(llevInfo, "CLOG DMSAY:%s >%s<\n", query_name(op, NULL), params);

	params = cleanup_chat_string(params);
	/* This can happen when whitespace only string was sent */
	if (!params || *params == '\0')
		return 0;

	for (tmp_dm_list = dm_list; tmp_dm_list; tmp_dm_list = tmp_dm_list->next)
		new_draw_info_format(NDI_UNIQUE | NDI_PLAYER | NDI_RED, 0, tmp_dm_list->op, "[DM Channel]: %s: %s", op->name, params);

	return 1;
}

/**
 * Shout command, used to shout a message for everyone to hear.
 * @param op The object saying this
 * @param params The message
 * @return 1 on success, 0 on failure */
int command_shout(object *op, char *params)
{
	char buf[MAX_BUF];

	if (!params)
		return 0;

	LOG(llevInfo, "CLOG SHOUT:%s >%s<\n", query_name(op, NULL), params);

	params = cleanup_chat_string(params);
	/* this happens when whitespace only string was submited */
	if (!params || *params == '\0')
		return 0;

	strcpy(buf, op->name);
	strcat(buf, " shouts: ");
	strncat(buf, params, MAX_BUF - 30);
	buf[MAX_BUF - 30] = '\0';
	new_draw_info(NDI_PLAYER | NDI_UNIQUE | NDI_ALL | NDI_ORANGE, 1, NULL, buf);

	/* Trigger the global SHOUT event */
	trigger_global_event(EVENT_SHOUT, op, params);

	return 1;
}

/**
 * Tell message to a single player.
 * @param op The object saying it
 * @param params The player name to send message to, and the message
 * @return 1 on success, 0 on failure */
int command_tell(object *op, char *params)
{
	char buf[MAX_BUF], *name = NULL, *msg = NULL;
	char buf2[MAX_BUF];
	player *pl;

	if (!params)
		return 0;

	LOG(llevInfo, "CLOG TELL:%s >%s<\n", query_name(op, NULL), params);

	params = cleanup_chat_string(params);
	/* this happens when whitespace only string was submitted */
	if (!params || *params == '\0')
		return 0;

	name = params;
	msg = strchr(name, ' ');
	if (msg)
	{
		*(msg++) = 0;
		if (*msg == 0)
			msg = NULL;
	}

	if (name == NULL)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Tell whom what?");
		return 1;
	}
	else if (msg == NULL)
	{
		sprintf(buf, "Tell %s what?", name);
		new_draw_info(NDI_UNIQUE, 0, op, buf);
		return 1;
	}

	/* Send to yourself? Intelligent... */
	if (strncasecmp(op->name, name, MAX_NAME) == 0)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You tell yourself the news. Very smart.");
		return 1;
	}

	sprintf(buf, "%s tells you: ", op->name);
	strncat(buf, msg, MAX_BUF - strlen(buf) - 1);
	buf[MAX_BUF - 1] = 0;

	for (pl = first_player; pl != NULL; pl = pl->next)
	{
		if (!strncasecmp(pl->ob->name, name, MAX_NAME))
		{
			if (pl->dm_stealth)
			{
				sprintf(buf, "%s tells you (dm_stealth): ", op->name);
				strncat(buf, msg, MAX_BUF - strlen(buf) - 1);
				buf[MAX_BUF - 1] = 0;
				new_draw_info(NDI_PLAYER | NDI_UNIQUE | NDI_NAVY, 0, pl->ob, buf);
				/* Update last_tell value [mids 01/14/2002] */
				strcpy(pl->last_tell, op->name);
				/* we send it but we kick the "no such player" on */
				break;
			}
			else
			{
				sprintf(buf2, "You tell %s: %s", name, msg);
				new_draw_info(NDI_PLAYER | NDI_UNIQUE, 0, op, buf2);
				new_draw_info(NDI_PLAYER | NDI_UNIQUE | NDI_NAVY, 0, pl->ob, buf);
				/* Update last_tell value [mids 01/14/2002] */
				strcpy(pl->last_tell, op->name);
				return 1;
			}
		}
	}

	new_draw_info(NDI_UNIQUE, 0, op, "No such player.");
	return 1;
}


/**
 * Command to tell something to a target.
 * Usually used by the talk button in the client.
 * @param op The object saying this
 * @param params The message
 * @return 1 on success, 0 on failure */
int command_t_tell(object *op, char *params)
{
	char buf[256 * 2];
	object *t_obj;
	int i, xt, yt;
	mapstruct *m;

	if (!params)
		return 0;

	params = cleanup_chat_string(params);
	/* this happens when whitespace only string was submited */
	if (!params || *params == '\0')
		return 0;

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
				if (!(m = out_of_map(op->map, &xt, &yt)))
					continue;

				if (m == t_obj->map && xt == t_obj->x && yt == t_obj->y)
				{
					LOG(llevInfo, "CLOG T_TELL:%s >%s<\n", query_name(op, NULL), params);
					sprintf(buf, "You say to %s: ", query_name(t_obj, NULL));
					strncat(buf, params, MAX_BUF - strlen(buf) - 1);
					buf[MAX_BUF - 1] = 0;
					new_draw_info(NDI_WHITE, 0, op, buf);
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
 * Reply to last person who told you something.
 * @param op Object saying this
 * @param params The message
 * @return 1 on success, 0 on failure */
int command_reply(object *op, char *params)
{
	char buf[MAX_BUF];
	char buf2[MAX_BUF];
	player *pl;

	if (params)
		params = cleanup_chat_string(params);

	/* this happens when whitespace only string was submited */
	if (!params || *params == '\0')
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Reply what?");
		return 0;
	}

	if (CONTR(op)->last_tell[0] == '\0')
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You can't reply to nobody.");
		return 0;
	}

	/* Find player object of player to reply to and check if player still exists */
	pl = find_player(CONTR(op)->last_tell);

	if (pl == NULL || pl->dm_stealth)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You can't reply, this player left.");
		return 0;
	}

	sprintf(buf, "%s replies you: ", op->name);
	strncat(buf, params, MAX_BUF - strlen(buf) - 1);
	buf[MAX_BUF - 1] = 0;

	/* Update last_tell value */
	strcpy(pl->last_tell, op->name);

	new_draw_info(NDI_PLAYER | NDI_UNIQUE | NDI_NAVY, 0, pl->ob, buf);
	sprintf(buf2, "You reply %s: %s", pl->ob->name, params);
	new_draw_info(NDI_PLAYER | NDI_UNIQUE , 0, op, buf2);

	return 1;
}

static void emote_other(object *op, object *target, char *str, char *buf, char *buf2, char *buf3, int emotion)
{
	const char *name = str;

	if (target && target->name)
		name = target->name;

	switch (emotion)
	{
		case EMOTE_NOD:
			sprintf(buf, "You nod solemnly to %s.", name);
			sprintf(buf2, "%s nods solemnly to you.", op->name);
			sprintf(buf3, "%s nods solemnly to %s.", op->name, name);
			break;
		case EMOTE_DANCE:
			sprintf(buf, "You grab %s and begin doing the Cha-Cha!", name);
			sprintf(buf2, "%s grabs you, and begins dancing!", op->name);
			sprintf(buf3, "Yipe! %s and %s are doing the Macarena!", op->name, name);
			break;
		case EMOTE_KISS:
			sprintf(buf, "You kiss %s.", name);
			sprintf(buf2, "%s kisses you.", op->name);
			sprintf(buf3, "%s kisses %s.", op->name, name);
			break;
		case EMOTE_BOUNCE:
			sprintf(buf, "You bounce around the room with %s.", name);
			sprintf(buf2, "%s bounces around the room with you.", op->name);
			sprintf(buf3, "%s bounces around the room with %s.", op->name, name);
			break;
		case EMOTE_SMILE:
			sprintf(buf, "You smile at %s.", name);
			sprintf(buf2, "%s smiles at you.", op->name);
			sprintf(buf3, "%s beams a smile at %s.", op->name, name);
			break;
		case EMOTE_LAUGH:
			sprintf(buf, "You take one look at %s and fall down laughing.", name);
			sprintf(buf2, "%s looks at you and falls down on the ground laughing.", op->name);
			sprintf(buf3, "%s looks at %s and falls down on the ground laughing.", op->name, name);
			break;
		case EMOTE_SHAKE:
			sprintf(buf, "You shake %s's hand.", name);
			sprintf(buf2, "%s shakes your hand.", op->name);
			sprintf(buf3, "%s shakes %s's hand.", op->name, name);
			break;
		case EMOTE_PUKE:
			sprintf(buf, "You puke on %s.", name);
			sprintf(buf2, "%s pukes on your clothes!", op->name);
			sprintf(buf3, "%s pukes on %s.", op->name, name);
			break;
		case EMOTE_HUG:
			sprintf(buf, "You hug %s.", name);
			sprintf(buf2, "%s hugs you.", op->name);
			sprintf(buf3, "%s hugs %s.", op->name, name);
			break;
		case EMOTE_CRY:
			sprintf(buf, "You cry on %s's shoulder.", name);
			sprintf(buf2, "%s cries on your shoulder.", op->name);
			sprintf(buf3, "%s cries on %s's shoulder.", op->name, name);
			break;
		case EMOTE_POKE:
			sprintf(buf, "You poke %s in the ribs.", name);
			sprintf(buf2, "%s pokes you in the ribs.", op->name);
			sprintf(buf3, "%s pokes %s in the ribs.", op->name, name);
			break;
		case EMOTE_ACCUSE:
			sprintf(buf, "You look accusingly at %s.", name);
			sprintf(buf2, "%s looks accusingly at you.", op->name);
			sprintf(buf3, "%s looks accusingly at %s.", op->name, name);
			break;
		case EMOTE_GRIN:
			sprintf(buf, "You grin at %s.", name);
			sprintf(buf2, "%s grins evilly at you.", op->name);
			sprintf(buf3, "%s grins evilly at %s.", op->name, name);
			break;
		case EMOTE_BOW:
			sprintf(buf, "You bow before %s.", name);
			sprintf(buf2, "%s bows before you.", op->name);
			sprintf(buf3, "%s bows before %s.", op->name, name);
			break;
		case EMOTE_FROWN:
			sprintf(buf, "You frown darkly at %s.", name);
			sprintf(buf2, "%s frowns darkly at you.", op->name);
			sprintf(buf3, "%s frowns darkly at %s.", op->name, name);
			break;
		case EMOTE_GLARE:
			sprintf(buf, "You glare icily at %s.", name);
			sprintf(buf2, "%s glares icily at you, you feel cold to your bones.", op->name);
			sprintf(buf3, "%s glares at %s.", op->name, name);
			break;
		case EMOTE_LICK:
			sprintf(buf, "You lick %s.", name);
			sprintf(buf2, "%s licks you.", op->name);
			sprintf(buf3, "%s licks %s.", op->name, name);
			break;
		case EMOTE_SHRUG:
			sprintf(buf, "You shrug at %s.", name);
			sprintf(buf2, "%s shrugs at you.", op->name);
			sprintf(buf3, "%s shrugs at %s.", op->name, name);
			break;
		case EMOTE_SLAP:
			sprintf(buf, "You slap %s.", name);
			sprintf(buf2, "You are slapped by %s.", op->name);
			sprintf(buf3, "%s slaps %s.", op->name, name);
			break;
		case EMOTE_SNEEZE:
			sprintf(buf, "You sneeze at %s and a film of snot shoots onto him.", name);
			sprintf(buf2, "%s sneezes on you, you feel the snot cover you. EEEEEEW.", op->name);
			sprintf(buf3, "%s sneezes on %s and a film of snot covers him.", op->name, name);
			break;
		case EMOTE_SNIFF:
			sprintf(buf, "You sniff %s.", name);
			sprintf(buf2, "%s sniffs you.", op->name);
			sprintf(buf3, "%s sniffs %s", op->name, name);
			break;
		case EMOTE_SPIT:
			sprintf(buf, "You spit on %s.", name);
			sprintf(buf2, "%s spits in your face!", op->name);
			sprintf(buf3, "%s spits in %s's face.", op->name, name);
			break;
		case EMOTE_THANK:
			sprintf(buf, "You thank %s heartily.", name);
			sprintf(buf2, "%s thanks you heartily.", op->name);
			sprintf(buf3, "%s thanks %s heartily.", op->name, name);
			break;
		case EMOTE_WAVE:
			sprintf(buf, "You wave goodbye to %s.", name);
			sprintf(buf2, "%s waves goodbye to you. Have a good journey.", op->name);
			sprintf(buf3, "%s waves goodbye to %s.", op->name, name);
			break;
		case EMOTE_WHISTLE:
			sprintf(buf, "You whistle at %s.", name);
			sprintf(buf2, "%s whistles at you.", op->name);
			sprintf(buf3, "%s whistles at %s.", op->name, name);
			break;
		case EMOTE_WINK:
			sprintf(buf, "You wink suggestively at %s.", name);
			sprintf(buf2, "%s winks suggestively at you.", op->name);
			sprintf(buf3, "%s winks at %s.", op->name, name);
			break;
		case EMOTE_BEG:
			sprintf(buf, "You beg %s for mercy.", name);
			sprintf(buf2, "%s begs you for mercy! Show no quarter!", op->name);
			sprintf(buf3, "%s begs %s for mercy!", op->name, name);
			break;
		case EMOTE_BLEED:
			sprintf(buf, "You slash your wrist and bleed all over %s", name);
			sprintf(buf2, "%s slashes his wrist and bleeds all over you.", op->name);
			sprintf(buf3, "%s slashes his wrist and bleeds all over %s.", op->name, name);
			break;
		case EMOTE_CRINGE:
			sprintf(buf, "You cringe away from %s.", name);
			sprintf(buf2, "%s cringes away from you.", op->name);
			sprintf(buf3, "%s cringes away from %s in mortal terror.", op->name, name);
			break;
		default:
			sprintf(buf, "You are still nuts.");
			sprintf(buf2, "You get the distinct feeling that %s is nuts.", op->name);
			sprintf(buf3, "%s is eyeing %s quizzically.", name, op->name);
			break;
	}
}

static void emote_self(object *op, char *buf, char *buf2, int emotion)
{
	switch (emotion)
	{
		case EMOTE_DANCE:
			sprintf(buf, "You skip and dance around by yourself.");
			sprintf(buf2, "%s embraces himself and begins to dance!", op->name);
			break;
		case EMOTE_LAUGH:
			sprintf(buf, "Laugh at yourself all you want, the others won't understand.");
			sprintf(buf2, "%s is laughing at something.", op->name);
			break;
		case EMOTE_SHAKE:
			sprintf(buf, "You are shaken by yourself.");
			sprintf(buf2, "%s shakes and quivers like a bowlful of jelly.", op->name);
			break;
		case EMOTE_PUKE:
			sprintf(buf, "You puke on yourself.");
			sprintf(buf2, "%s pukes on his clothes.", op->name);
			break;
		case EMOTE_HUG:
			sprintf(buf, "You hug yourself.");
			sprintf(buf2, "%s hugs himself.", op->name);
			break;
		case EMOTE_CRY:
			sprintf(buf, "You cry to yourself.");
			sprintf(buf2, "%s sobs quietly to himself.", op->name);
			break;
		case EMOTE_POKE:
			sprintf(buf, "You poke yourself in the ribs, feeling very silly.");
			sprintf(buf2, "%s pokes himself in the ribs, looking very sheepish.", op->name);
			break;
		case EMOTE_ACCUSE:
			sprintf(buf, "You accuse yourself.");
			sprintf(buf2, "%s seems to have a bad conscience.", op->name);
			break;
		case EMOTE_BOW:
			sprintf(buf, "You kiss your toes.");
			sprintf(buf2, "%s folds up like a jackknife and kisses his own toes.", op->name);
			break;
		case EMOTE_FROWN:
			sprintf(buf, "You frown at yourself.");
			sprintf(buf2, "%s frowns at himself.", op->name);
			break;
		case EMOTE_GLARE:
			sprintf(buf, "You glare icily at your feet, they are suddenly very cold.");
			sprintf(buf2, "%s glares at his feet, what is bothering him?", op->name);
			break;
		case EMOTE_LICK:
			sprintf(buf, "You lick yourself.");
			sprintf(buf2, "%s licks himself - YUCK.", op->name);
			break;
		case EMOTE_SLAP:
			sprintf(buf, "You slap yourself, silly you.");
			sprintf(buf2, "%s slaps himself, really strange...", op->name);
			break;
		case EMOTE_SNEEZE:
			sprintf(buf, "You sneeze on yourself, what a mess!");
			sprintf(buf2, "%s sneezes, and covers himself in a slimy substance.", op->name);
			break;
		case EMOTE_SNIFF:
			sprintf(buf, "You sniff yourself.");
			sprintf(buf2, "%s sniffs himself.", op->name);
			break;
		case EMOTE_SPIT:
			sprintf(buf, "You drool all over yourself.");
			sprintf(buf2, "%s drools all over himself.", op->name);
			break;
		case EMOTE_THANK:
			sprintf(buf, "You thank yourself since nobody else wants to!");
			sprintf(buf2, "%s thanks himself since you won't.", op->name);
			break;
		case EMOTE_WAVE:
			sprintf(buf, "Are you going on adventures as well??");
			sprintf(buf2, "%s waves goodbye to himself.", op->name);
			break;
		case EMOTE_WHISTLE:
			sprintf(buf, "You whistle while you work.");
			sprintf(buf2, "%s whistles to himself in boredom.", op->name);
			break;
		case EMOTE_WINK:
			sprintf(buf, "You wink at yourself?? What are you up to?");
			sprintf(buf2, "%s winks at himself - something strange is going on...", op->name);
			break;
		case EMOTE_BLEED:
			sprintf(buf, "Very impressive! You wipe your blood all over yourself.");
			sprintf(buf2, "%s performs some satanic ritual while wiping his blood on himself.", op->name);
			break;
		default:
			sprintf(buf, "My god! is that LEGAL?");
			sprintf(buf2, "You look away from %s.", op->name);
			break;
	}
}

/*
 * This function covers basic emotions a player can have.  An emotion can be
 * one of three things currently.  Directed at oneself, directed at someone,
 * or directed at nobody.  The first set is nobody, the second at someone, and
 * the third is directed at oneself.  Every emotion does not have to be
 * filled out in every category.  The default case will take care of the ones
 * that are not.  Helper functions will call basic_emote with the proper
 * arguments, translating them into commands.  Adding a new emotion can be
 * done by editing command.c and command.h.
 * [garbled 09-25-2001]
 */
static int basic_emote(object *op, char *params, int emotion)
{
	char buf[HUGE_BUF] = "", buf2[HUGE_BUF] = "", buf3[HUGE_BUF] = "";
	player *pl;

	LOG(llevDebug, "EMOTE: %x (%s) (params: >%s<) (t: %s) %d\n", op, query_name(op, NULL), params ? params : "NULL", CONTR(op) ? query_name(CONTR(op)->target_object, NULL) : "NO CTRL!!", emotion);

	params = cleanup_chat_string(params);
	/* in this case we have 100% a illegal parameter */
	if (params && *params == '\0')
		params = NULL;

	if (!params)
	{
		/* if we are a player with legal target, use it as target for the emote */
		if (op->type == PLAYER && CONTR(op)->target_object != op && OBJECT_VALID(CONTR(op)->target_object, CONTR(op)->target_object_count) && CONTR(op)->target_object->name)
		{
			rv_vector rv;
			get_rangevector(op, CONTR(op)->target_object, &rv, 0);

			if (rv.distance <= 4)
			{
				emote_other(op, CONTR(op)->target_object, NULL, buf, buf2, buf3, emotion);
				new_draw_info(NDI_UNIQUE, 0, op, buf);
				if (CONTR(op)->target_object->type == PLAYER)
					new_draw_info(NDI_UNIQUE | NDI_YELLOW, 0, CONTR(op)->target_object, buf2);
				new_info_map_except(NDI_YELLOW, op->map, op->x, op->y, MAP_INFO_NORMAL, op, CONTR(op)->target_object, buf3);
				return 0;
			}
			new_draw_info(NDI_UNIQUE, 0, op, "The target is not in range for this emote action.");
			return 0;
		}

		switch (emotion)
		{
			case EMOTE_NOD:
				sprintf(buf, "%s nods solemnly.", op->name);
				sprintf(buf2, "You nod solemnly.");
				break;
			case EMOTE_DANCE:
				sprintf(buf, "%s expresses himself through interpretive dance.", op->name);
				sprintf(buf2, "You dance with glee.");
				break;
			case EMOTE_KISS:
				sprintf(buf, "%s makes a weird facial contortion.", op->name);
				sprintf(buf2, "All the lonely people...");
				break;
			case EMOTE_BOUNCE:
				sprintf(buf, "%s bounces around.", op->name);
				sprintf(buf2, "BOIINNNNNNGG!");
				break;
			case EMOTE_SMILE:
				sprintf(buf, "%s smiles happily.", op->name);
				sprintf(buf2, "You smile happily.");
				break;
			case EMOTE_CACKLE:
				sprintf(buf, "%s throws back his head and cackles with insane glee!", op->name);
				sprintf(buf2, "You cackle gleefully.");
				break;
			case EMOTE_LAUGH:
				sprintf(buf, "%s falls down laughing.", op->name);
				sprintf(buf2, "You fall down laughing.");
				break;
			case EMOTE_GIGGLE:
				sprintf(buf, "%s giggles.", op->name);
				sprintf(buf2, "You giggle.");
				break;
			case EMOTE_SHAKE:
				sprintf(buf, "%s shakes his head.", op->name);
				sprintf(buf2, "You shake your head.");
				break;
			case EMOTE_PUKE:
				sprintf(buf, "%s pukes.", op->name);
				sprintf(buf2, "Bleaaaaaghhhhhhh!");
				break;
			case EMOTE_GROWL:
				sprintf(buf, "%s growls.", op->name);
				sprintf(buf2, "Grrrrrrrrr....");
				break;
			case EMOTE_SCREAM:
				sprintf(buf, "%s screams at the top of his lungs!", op->name);
				sprintf(buf2, "ARRRRRRRRRRGH!!!!!");
				break;
			case EMOTE_SIGH:
				sprintf(buf, "%s sighs loudly.", op->name);
				sprintf(buf2, "You sigh.");
				break;
			case EMOTE_SULK:
				sprintf(buf, "%s sulks in the corner.", op->name);
				sprintf(buf2, "You sulk.");
				break;
			case EMOTE_CRY:
				sprintf(buf, "%s bursts into tears.", op->name);
				sprintf(buf2, "Waaaaaaahhh...");
				break;
			case EMOTE_GRIN:
				sprintf(buf, "%s grins evilly.", op->name);
				sprintf(buf2, "You grin evilly.");
				break;
			case EMOTE_BOW:
				sprintf(buf, "%s bows deeply.", op->name);
				sprintf(buf2, "You bow deeply.");
				break;
			case EMOTE_CLAP:
				sprintf(buf, "%s gives a round of applause.", op->name);
				sprintf(buf2, "Clap, clap, clap.");
				break;
			case EMOTE_BLUSH:
				sprintf(buf, "%s blushes.", op->name);
				sprintf(buf2, "Your cheeks are burning.");
				break;
			case EMOTE_BURP:
				sprintf(buf, "%s burps loudly.", op->name);
				sprintf(buf2, "You burp loudly.");
				break;
			case EMOTE_CHUCKLE:
				sprintf(buf, "%s chuckles politely.", op->name);
				sprintf(buf2, "You chuckle politely");
				break;
			case EMOTE_COUGH:
				sprintf(buf, "%s coughs loudly.", op->name);
				sprintf(buf2, "Yuck, try to cover your mouth next time!");
				break;
			case EMOTE_FLIP:
				sprintf(buf, "%s flips head over heels.", op->name);
				sprintf(buf2, "You flip head over heels.");
				break;
			case EMOTE_FROWN:
				sprintf(buf, "%s frowns.", op->name);
				sprintf(buf2, "What's bothering you?");
				break;
			case EMOTE_GASP:
				sprintf(buf, "%s gasps in astonishment.", op->name);
				sprintf(buf2, "You gasp in astonishment.");
				break;
			case EMOTE_GLARE:
				sprintf(buf, "%s glares around him.", op->name);
				sprintf(buf2, "You glare at nothing in particular.");
				break;
			case EMOTE_GROAN:
				sprintf(buf, "%s groans loudly.", op->name);
				sprintf(buf2, "You groan loudly.");
				break;
			case EMOTE_HICCUP:
				sprintf(buf, "%s hiccups.", op->name);
				sprintf(buf2, "*HIC*");
				break;
			case EMOTE_LICK:
				sprintf(buf, "%s licks his mouth and smiles.", op->name);
				sprintf(buf2, "You lick your mouth and smile.");
				break;
			case EMOTE_POUT:
				sprintf(buf, "%s pouts.", op->name);
				sprintf(buf2, "Aww, don't take it so hard.");
				break;
			case EMOTE_SHIVER:
				sprintf(buf, "%s shivers uncomfortably.", op->name);
				sprintf(buf2, "Brrrrrrrrr.");
				break;
			case EMOTE_SHRUG:
				sprintf(buf, "%s shrugs helplessly.", op->name);
				sprintf(buf2, "You shrug.");
				break;
			case EMOTE_SMIRK:
				sprintf(buf, "%s smirks.", op->name);
				sprintf(buf2, "You smirk.");
				break;
			case EMOTE_SNAP:
				sprintf(buf, "%s snaps his fingers.", op->name);
				sprintf(buf2, "PRONTO! You snap your fingers.");
				break;
			case EMOTE_SNEEZE:
				sprintf(buf, "%s sneezes.", op->name);
				sprintf(buf2, "Gesundheit!");
				break;
			case EMOTE_SNICKER:
				sprintf(buf, "%s snickers softly.", op->name);
				sprintf(buf2, "You snicker softly.");
				break;
			case EMOTE_SNIFF:
				sprintf(buf, "%s sniffs sadly.", op->name);
				sprintf(buf2, "You sniff sadly. *SNIFF*");
				break;
			case EMOTE_SNORE:
				sprintf(buf, "%s snores loudly.", op->name);
				sprintf(buf2, "Zzzzzzzzzzzzzzz.");
				break;
			case EMOTE_SPIT:
				sprintf(buf, "%s spits over his left shoulder.", op->name);
				sprintf(buf2, "You spit over your left shoulder.");
				break;
			case EMOTE_STRUT:
				sprintf(buf, "%s struts proudly.", op->name);
				sprintf(buf2, "Strut your stuff.");
				break;
			case EMOTE_TWIDDLE:
				sprintf(buf, "%s patiently twiddles his thumbs.", op->name);
				sprintf(buf2, "You patiently twiddle your thumbs.");
				break;
			case EMOTE_WAVE:
				sprintf(buf, "%s waves happily.", op->name);
				sprintf(buf2, "You wave.");
				break;
			case EMOTE_WHISTLE:
				sprintf(buf, "%s whistles appreciatively.", op->name);
				sprintf(buf2, "You whistle appreciatively.");
				break;
			case EMOTE_WINK:
				sprintf(buf, "%s winks suggestively.", op->name);
				sprintf(buf2, "Have you got something in your eye?");
				break;
			case EMOTE_YAWN:
				sprintf(buf, "%s yawns sleepily.", op->name);
				sprintf(buf2, "You open up your yap and let out a big breeze of stale air.");
				break;
			case EMOTE_CRINGE:
				sprintf(buf, "%s cringes in terror!", op->name);
				sprintf(buf2, "You cringe in terror.");
				break;
			case EMOTE_BLEED:
				sprintf(buf, "%s is bleeding all over the carpet - got a spare tourniquet?", op->name);
				sprintf(buf2, "You bleed all over your nice new armour.");
				break;
			case EMOTE_THINK:
				sprintf(buf, "%s closes his eyes and thinks really hard.", op->name);
				sprintf(buf2, "Anything in particular that you'd care to think about?");
				break;
			case EMOTE_ME:
				sprintf(buf2, "Usage: /me <emote to display>");
				if (op->type == PLAYER)
					new_draw_info(NDI_UNIQUE, 0, op, buf2);
				return 0;
				/* Do nothing, since we specified nothing to do. */
				break;
			default:
				sprintf(buf, "%s dances with glee.", op->name);
				sprintf(buf2, "You are a nut.");
				break;
		}

		new_info_map_except(NDI_YELLOW, op->map, op->x, op->y, MAP_INFO_NORMAL, op, op, buf);
		if (op->type == PLAYER)
			new_draw_info(NDI_UNIQUE, 0, op, buf2);
		return 0;
	}
	/* we have params */
	else
	{
		if (emotion == EMOTE_ME)
		{
			sprintf(buf, "%s %s", op->name, params);
			strcpy(buf2, buf);
			LOG(llevInfo, "ME:: %s\n", buf2);
			new_info_map_except(NDI_YELLOW, op->map, op->x, op->y, MAP_INFO_NORMAL, op, op, buf);
			if (op->type == PLAYER)
				new_draw_info(NDI_UNIQUE, 0, op, buf2);
			return 0;
		}
		/* ATM, we only allow "yourself" as parameter for players. */
		else if (op->type == PLAYER)
		{
			if (!strcmp(params, "yourself"))
				emote_self(op, buf, buf2, emotion);
			/* Force neutral default emote. */
			else
				emote_self(op, buf, buf2, -1);

			new_draw_info(NDI_UNIQUE, 0, op, buf);
			new_info_map_except(NDI_YELLOW, op->map, op->x, op->y, MAP_INFO_NORMAL, op, op, buf2);
		}
		/* Here we handle npc emotes with parameter. */
		else
		{
			emote_other(op, NULL, params, buf, buf2, buf3, emotion);

			/* This is somewhat costly - check every playername! */
			for (pl = first_player; pl != NULL; pl = pl->next)
			{
				if (!strncasecmp(pl->ob->name, params, MAX_NAME))
				{
					new_draw_info(NDI_UNIQUE | NDI_YELLOW, 0, pl->ob, buf2);
					new_info_map_except(NDI_YELLOW, op->map, op->x, op->y, MAP_INFO_NORMAL, NULL, pl->ob, buf3);
					return 0;
				}
			}

			new_info_map_except(NDI_YELLOW, op->map, op->x, op->y, MAP_INFO_NORMAL, NULL, NULL, buf3);
		}
	}

	return 0;
}

/*
 * everything from here on out are just wrapper calls to basic_emote
 */
int command_nod(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_NOD));
}

int command_dance(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_DANCE));
}

int command_kiss(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_KISS));
}

int command_bounce(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_BOUNCE));
}

int command_smile(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_SMILE));
}

int command_cackle(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_CACKLE));
}

int command_laugh(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_LAUGH));
}

int command_giggle(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_GIGGLE));
}

int command_shake(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_SHAKE));
}

int command_puke(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_PUKE));
}

int command_growl(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_GROWL));
}

int command_scream(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_SCREAM));
}

int command_sigh(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_SIGH));
}

int command_sulk(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_SULK));
}

int command_hug(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_HUG));
}

int command_cry(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_CRY));
}

int command_poke(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_POKE));
}

int command_accuse(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_ACCUSE));
}

int command_grin(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_GRIN));
}

int command_bow(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_BOW));
}

int command_clap(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_CLAP));
}

int command_blush(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_BLUSH));
}

int command_burp(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_BURP));
}

int command_chuckle(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_CHUCKLE));
}

int command_cough(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_COUGH));
}

int command_flip(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_FLIP));
}

int command_frown(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_FROWN));
}

int command_gasp(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_GASP));
}

int command_glare(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_GLARE));
}

int command_groan(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_GROAN));
}

int command_hiccup(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_HICCUP));
}

int command_lick(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_LICK));
}

int command_pout(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_POUT));
}

int command_shiver(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_SHIVER));
}

int command_shrug(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_SHRUG));
}

int command_slap(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_SLAP));
}

int command_smirk(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_SMIRK));
}

int command_snap(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_SNAP));
}

int command_sneeze(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_SNEEZE));
}

int command_snicker(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_SNICKER));
}

int command_sniff(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_SNIFF));
}

int command_snore(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_SNORE));
}

int command_spit(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_SPIT));
}

int command_strut(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_STRUT));
}

int command_thank(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_THANK));
}

int command_twiddle(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_TWIDDLE));
}

int command_wave(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_WAVE));
}

int command_whistle(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_WHISTLE));
}

int command_wink(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_WINK));
}

int command_yawn(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_YAWN));
}

int command_beg(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_BEG));
}

int command_bleed(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_BLEED));
}

int command_cringe(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_CRINGE));
}

int command_think(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_THINK));
}

int command_me(object *op, char *params)
{
	return(basic_emote(op, params, EMOTE_ME));
}
