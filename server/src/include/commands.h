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
 * Atrinik commands. */

#ifndef COMMANDS_H
#define COMMANDS_H

typedef int (*CommFunc)(object *op, char *params);

/** Command array entry structure */
typedef struct
{
	/** Name of the command */
	char *name;

	/** The function to call */
	CommFunc func;

	/** How long it takes to execute this command */
	float time;

	/** Command flags. */
	int flags;
} CommArray_s;

extern CommArray_s Commands[], WizCommands[], CommunicationCommands[];
extern const int CommandsSize, WizCommandsSize, CommunicationCommandSize;

/**
 * @defgroup CMD_FLAG_xxx Command flags
 * Command flags used by CommArray_s::flags.
 *@{*/
/** Don't allow the command to be used while in player shop. */
#define CMD_FLAG_NO_PLAYER_SHOP 1
/*@}*/

/**
 * @defgroup EMOTE_xxx Emote command numbers
 * Emote command numbers.
 *@{*/
#define EMOTE_NOD       1
#define EMOTE_DANCE     2
#define EMOTE_KISS      3
#define EMOTE_BOUNCE    4
#define EMOTE_SMILE     5
#define EMOTE_CACKLE    6
#define EMOTE_LAUGH     7
#define EMOTE_GIGGLE    8
#define EMOTE_SHAKE     9
#define EMOTE_PUKE      10
#define EMOTE_GROWL     11
#define EMOTE_SCREAM    12
#define EMOTE_SIGH      13
#define EMOTE_SULK      14
#define EMOTE_HUG       15
#define EMOTE_CRY       16
#define EMOTE_POKE      17
#define EMOTE_ACCUSE    18
#define EMOTE_GRIN      19
#define EMOTE_BOW       20
#define EMOTE_CLAP      21
#define EMOTE_BLUSH     22
#define EMOTE_BURP      23
#define EMOTE_CHUCKLE   24
#define EMOTE_COUGH     25
#define EMOTE_FLIP      26
#define EMOTE_FROWN     27
#define EMOTE_GASP      28
#define EMOTE_GLARE     29
#define EMOTE_GROAN     30
#define EMOTE_HICCUP    31
#define EMOTE_LICK      32
#define EMOTE_POUT      33
#define EMOTE_SHIVER    34
#define EMOTE_SHRUG     35
#define EMOTE_SLAP      36
#define EMOTE_SMIRK     37
#define EMOTE_SNAP      38
#define EMOTE_SNEEZE    39
#define EMOTE_SNICKER   40
#define EMOTE_SNIFF     41
#define EMOTE_SNORE     42
#define EMOTE_SPIT      43
#define EMOTE_STRUT     44
#define EMOTE_THANK     45
#define EMOTE_TWIDDLE   46
#define EMOTE_WAVE      47
#define EMOTE_WHISTLE   48
#define EMOTE_WINK      49
#define EMOTE_YAWN      50
#define EMOTE_BEG       51
#define EMOTE_BLEED     52
#define EMOTE_CRINGE    53
#define EMOTE_THINK     54
#define EMOTE_ME        55
/*@}*/

#endif
