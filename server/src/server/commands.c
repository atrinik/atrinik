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

/**
 * @file
 * Command parser */

#include <global.h>
#ifndef __CEXTRACT__
#include <sproto.h>
#endif
#include <ctype.h>

/** Normal game commands */
CommArray_s Commands[] =
{
	{"/stay",			command_stay,			1.0f},
	{"/n",				command_north,			1.0f},
	{"/e",				command_east,			1.0f},
	{"/s",				command_south,			1.0f},
	{"/w",				command_west,			1.0f},
	{"/ne",				command_northeast,		1.0f},
	{"/se",				command_southeast,		1.0f},
	{"/sw",				command_southwest,		1.0f},
	{"/nw",				command_northwest,		1.0f},
	{"/apply",			command_apply,			1.0f},	/* should be variable */
	{"/target",			command_target,			0.1f}, /* enter combat and attack object in reach */
	{"/combat",			command_combat,			0.1f}, /* toggle attack mode of player */
	{"/pray",			command_praying,		0.2f},
	{"/run",			command_run,			1.0f},
	{"/run_stop",		command_run_stop,		0.01f},
	{"/cast",			command_cast_spell,		0.0f},	/* use time comes from spells! */
	{"/say",			command_say,			1.0f},
	{"/shout",			command_shout,			1.0f},
	{"/tell",			command_tell,			1.0f},
	{"/t_tell",			command_t_tell,			1.0f},
	{"/who",			command_who,			1.0f},
	{"/mapinfo",		command_mapinfo,		1.0f},
	{"/motd",			command_motd,			1.0f},
	{"/dm",				command_dm,				1.0f},
	{"/reply",			command_reply,			1.0f},
	{"/time",			command_time,			1.0f},
	{"/version",		command_version,		1.0f},
	{"/mark",			command_mark,			1.0f},
	{"/save",			command_save,			1.0f},
	{"/use_skill",		command_uskill,			0.1f},
	{"/ready_skill",	command_rskill,			0.1f},
	{"/hiscore",      	command_hiscore,        1.0f},
	{"/bug",          	command_bug,            1.0f},
	{"/apartment",		command_apartment,		1.0f},
	{"/afk",			command_afk,			1.0f},
#if 0
	{"/listen",       	command_listen,         1.0f},
#endif
	{"/drop",         	command_drop,           1.0f},
	{"/party", 			command_party,			0.0f},
	{"/gsay", 			command_gsay,			1.0f},
	{"/push",			command_push_object, 	1.0f},
	{"/left",			command_turn_left,		1.0f},
	{"/right",			command_turn_right,		1.0f},
	{"/roll",			command_roll,			1.0f},
	/*  {"/sound",		command_sound,			1.0},*/
	/*  {"/delete",		command_quit,			1.0},*/
	/*  {"/pickup",		command_pickup,			1.0}, we don't want and need this anymore */
};

/** Size of normal commands */
const int CommandsSize = sizeof(Commands) / sizeof(CommArray_s);

/** Emotion commands */
CommArray_s CommunicationCommands [] =
{
	{"/nod", 		command_nod,		1.0},
	{"/dance", 		command_dance,		1.0},
	{"/kiss", 		command_kiss,		1.0},
	{"/bounce", 	command_bounce,		1.0},
	{"/smile", 		command_smile,		1.0},
	{"/cackle", 	command_cackle,		1.0},
	{"/laugh", 		command_laugh,		1.0},
	{"/giggle", 	command_giggle,		1.0},
	{"/shake", 		command_shake,		1.0},
	{"/puke", 		command_puke,		1.0},
	{"/growl", 		command_growl,		1.0},
	{"/scream", 	command_scream,		1.0},
	{"/sigh", 		command_sigh,		1.0},
	{"/sulk", 		command_sulk,		1.0},
	{"/hug", 		command_hug,		1.0},
	{"/cry", 		command_cry,		1.0},
	{"/poke", 		command_poke,		1.0},
	{"/accuse",		command_accuse,		1.0},
	{"/grin", 		command_grin,		1.0},
	{"/bow", 		command_bow,		1.0},
	{"/clap", 		command_clap,		1.0},
	{"/blush", 		command_blush,		1.0},
	{"/burp", 		command_burp,		1.0},
	{"/chuckle",	command_chuckle,	1.0},
	{"/cough", 		command_cough,		1.0},
	{"/flip", 		command_flip,		1.0},
	{"/frown", 		command_frown,		1.0},
	{"/gasp", 		command_gasp,		1.0},
	{"/glare", 		command_glare,		1.0},
	{"/groan", 		command_groan,		1.0},
	{"/hiccup", 	command_hiccup,		1.0},
	{"/lick", 		command_lick,		1.0},
	{"/pout", 		command_pout,		1.0},
	{"/shiver", 	command_shiver,		1.0},
	{"/shrug", 		command_shrug,		1.0},
	{"/slap", 		command_slap,		1.0},
	{"/smirk", 		command_smirk,		1.0},
	{"/snap", 		command_snap,		1.0},
	{"/sneeze", 	command_sneeze,		1.0},
	{"/snicker", 	command_snicker,	1.0},
	{"/sniff", 		command_sniff,		1.0},
	{"/snore", 		command_snore,		1.0},
	{"/spit", 		command_spit,		1.0},
	{"/strut", 		command_strut,		1.0},
	{"/thank", 		command_thank,		1.0},
	{"/twiddle", 	command_twiddle,	1.0},
	{"/wave", 		command_wave,		1.0},
	{"/whistle", 	command_whistle,	1.0},
	{"/wink", 		command_wink,		1.0},
	{"/yawn", 		command_yawn,		1.0},
	{"/beg", 		command_beg,		1.0},
	{"/bleed", 		command_bleed,		1.0},
	{"/cringe", 	command_cringe,		1.0},
	{"/think", 		command_think,		1.0},
	{"/me", 		command_me,			1.0},
};

/** Size of emotion commands */
const int CommunicationCommandSize = sizeof(CommunicationCommands)/ sizeof(CommArray_s);

/** Wizard commands */
CommArray_s WizCommands [] =
{
	{"/dmsay",			command_dmsay,					0.0},
	{"/summon", 		command_summon,					0.0},
	{"/kick", 			command_kick,					0.0},
	{"/inventory", 		command_inventory,				0.0},
	{"/plugin",			command_loadplugin,				0.0},
	{"/pluglist",		command_listplugins,			0.0},
	{"/motd_set",		command_motd_set,				0.0},
	{"/ban",			command_ban,					0.0},
	{"/teleport", 		command_teleport,				0.0},
	{"/goto", 			command_goto,					0.0},
	{"/shutdown", 		command_start_shutdown,			0.0},
	{"/shutdown_now", 	command_shutdown, 				0.0},
	{"/resetmap", 		command_reset,					0.0},
	{"/plugout",		command_unloadplugin,			0.0},
	{"/create", 		command_create,					0.0},
	{"/addexp", 		command_addexp,					0.0},
	{"/malloc", 		command_malloc,					0.0},
	{"/maps", 			command_maps,					0.0},
	{"/dump", 			command_dump,					0.0}, /* dump info of object nr. x */
	{"/dm_stealth", 	command_dm_stealth,				0.0},
	{"/dm_light", 		command_dm_light,				0.0},
	{"/d_active", 		command_dumpactivelist,			0.0},
	{"/d_arches", 		command_dumpallarchetypes,		0.0},
	{"/d_maps", 		command_dumpallmaps,			0.0},
	{"/d_map", 			command_dumpmap,				0.0},
	{"/d_objects", 		command_dumpallobjects,			0.0},
	{"/d_belowfull", 	command_dumpbelowfull,			0.0},
	{"/d_below", 		command_dumpbelow,				0.0},
	{"/d_friendly", 	command_dumpfriendlyobjects,	0.0},
	{"/set_map_light", 	command_setmaplight,			0.0},
	{"/wizpass", 		command_wizpass,				0.0},
	{"/learn_spell", 	command_learn_spell, 			0.0},
	{"/ssdumptable",    command_ssdumptable,            0.0},
	{"/patch",			command_patch,					0.0},
	{"/speed",          command_speed,                  0.0},
	{"/strings",        command_strings,                0.0},

	/*
	{"/archs", command_archs,	0.0},
	{"/abil", command_abil,0.0},
	{"/debug", command_debug,0.0},
	{"/fix_me", command_fix_me,	0.0},
	{"/forget_spell", command_forget_spell, 0.0},
	{"/invisible", command_invisible,0.0},
	{"/learn_special_prayer", command_learn_special_prayer, 0.0},
	{"/logs", command_logs,	0.0},
	{"/players", command_players,	0.0},
	{"/patch", command_patch,0.0},
	{"/printlos", command_printlos,0.0},
	{"/resistances", command_resistances,	0.0},
	{"/remove", command_remove,0.0},
	{"/strings", command_strings,	0.0},
	{"/set_god", command_setgod, 0.0},
	{"/stats", command_stats,0.0},
	{"/style_info", command_style_map_info, 0.0},
	#ifdef DEBUG_MALLOC_LEVEL
	{"/verify", command_malloc_verify,0.0},
	#endif
	*/
};

/** Size of Wizard commands */
const int WizCommandsSize = sizeof(WizCommands) / sizeof(CommArray_s);

/**
 * Compare two commands, for qsort() in init_command() and bsearch() in
 * find_command_element().
 * @param a The first command
 * @param b The second command
 * @return Return value of strcmp on the two command names. */
static int compare_A(const void *a, const void *b)
{
	return strcmp(((CommArray_s *) a)->name, ((CommArray_s *) b)->name);
}

/**
 * Initialize all the commands.
 *
 * Sorts the commands using qsort(). */
void init_commands()
{
	qsort((char *) Commands, CommandsSize, sizeof(CommArray_s), compare_A);
	qsort((char *) CommunicationCommands, CommunicationCommandSize, sizeof(CommArray_s), compare_A);
	qsort((char *) WizCommands, WizCommandsSize, sizeof(CommArray_s), compare_A);
}

/**
 * Find a command element.
 * @param cmd The command name
 * @param commarray The commands array to search in
 * @param commsize The commands array size
 * @return The command entry in the array if found */
CommArray_s *find_command_element(char *cmd, CommArray_s *commarray, int commsize)
{
	CommArray_s *asp, dummy;
	char *cp;

	for (cp = cmd; *cp; cp++)
	{
		*cp = tolower(*cp);
	}

	dummy.name = cmd;
	asp = (CommArray_s *) bsearch((void *) &dummy, (void *) commarray, commsize, sizeof(CommArray_s), compare_A);

	return asp;
}

/**
 * This function is called from the new client/server code.
 * pl is the player who is issuing the command, command is the
 * command.
 *  */
/**
 * This function is called from the new client/server code.
 * @param pl The player who is issuing the command
 * @param command The command
 * @return 0 if not a valid command, otherwise return value of the called
 * command function is returned. */
int execute_newserver_command(object *pl, char *command)
{
	CommArray_s *csp;
	char *cp;

	/* Remove the command from the parameters */
	cp = strchr(command, ' ');

	if (cp)
	{
		*(cp++) = '\0';
		cp = cleanup_string(cp);

		if (cp && *cp == '\0')
		{
			cp = NULL;
		}
	}

	csp = find_plugin_command(command,pl);

	if (!csp)
	{
		csp = find_command_element(command, Commands, CommandsSize);
	}

	if (!csp)
	{
		csp = find_command_element(command, CommunicationCommands, CommunicationCommandSize);
	}

	if (!csp && QUERY_FLAG(pl, FLAG_WIZ))
	{
		csp = find_command_element(command, WizCommands, WizCommandsSize);
	}

	if (csp == NULL)
	{
		new_draw_info_format(NDI_UNIQUE, 0, pl, "'%s' is not a valid command.", command);

		return 0;
	}

	pl->speed_left -= csp->time;

	/* A character time can never exceed his speed (which in many cases,
	 * if wearing armor, is less than one.)  Thus, in most cases, if
	 * the command takes 1.0, the player's speed will be less than zero.
	 * it is only really an issue if time goes below -1
	 * Due to various reasons that are too long to go into here, we will
	 * actually still execute player even if his time is less than 0,
	 * but greater than -1.  This is to improve the performance of the
	 * new client/server.  In theory, it shouldn't make much difference. */
#ifdef DEBUG
	if (csp->time && pl->speed_left < -2.0)
	{
		LOG(llevDebug, "DEBUG: execute_newclient_command: Player issued command that takes more time than he has left.\n");
	}
#endif

	return csp->func(pl, cp);
}
