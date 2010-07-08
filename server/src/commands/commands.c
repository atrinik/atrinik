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
 * Command parsing related code. */

#include <global.h>
#include <ctype.h>

/** Normal game commands */
CommArray_s Commands[] =
{
	{"stay",           command_stay,           1.0f, 0},
	{"n",              command_north,          1.0f, CMD_FLAG_NO_PLAYER_SHOP},
	{"e",              command_east,           1.0f, CMD_FLAG_NO_PLAYER_SHOP},
	{"s",              command_south,          1.0f, CMD_FLAG_NO_PLAYER_SHOP},
	{"w",              command_west,           1.0f, CMD_FLAG_NO_PLAYER_SHOP},
	{"ne",             command_northeast,      1.0f, CMD_FLAG_NO_PLAYER_SHOP},
	{"se",             command_southeast,      1.0f, CMD_FLAG_NO_PLAYER_SHOP},
	{"sw",             command_southwest,      1.0f, CMD_FLAG_NO_PLAYER_SHOP},
	{"nw",             command_northwest,      1.0f, CMD_FLAG_NO_PLAYER_SHOP},
	{"apply",          command_apply,          1.0f, CMD_FLAG_NO_PLAYER_SHOP},
	{"target",         command_target,         0.1f, 0},
	{"combat",         command_combat,         0.1f, 0},
	{"pray",           command_praying,        0.2f, CMD_FLAG_NO_PLAYER_SHOP},
	{"run",            command_run,            1.0f, CMD_FLAG_NO_PLAYER_SHOP},
	{"run_stop",       command_run_stop,       0.1f, CMD_FLAG_NO_PLAYER_SHOP},
	{"cast",           command_cast_spell,     0.0f, CMD_FLAG_NO_PLAYER_SHOP},
	{"say",            command_say,            1.0f, 0},
	{"shout",          command_shout,          1.0f, 0},
	{"tell",           command_tell,           1.0f, 0},
	{"t_tell",         command_t_tell,         1.0f, CMD_FLAG_NO_PLAYER_SHOP},
	{"who",            command_who,            1.0f, 0},
	{"mapinfo",        command_mapinfo,        1.0f, 0},
	{"motd",           command_motd,           1.0f, 0},
	{"dm",             command_dm,             1.0f, CMD_FLAG_NO_PLAYER_SHOP},
	{"time",           command_time,           1.0f, 0},
	{"version",        command_version,        1.0f, 0},
	{"save",           command_save,           1.0f, 0},
	{"use_skill",      command_uskill,         0.1f, CMD_FLAG_NO_PLAYER_SHOP},
	{"ready_skill",    command_rskill,         0.1f, CMD_FLAG_NO_PLAYER_SHOP},
	{"hiscore",        command_hiscore,        1.0f, 0},
	{"afk",            command_afk,            1.0f, 0},
	{"drop",           command_drop,           1.0f, CMD_FLAG_NO_PLAYER_SHOP},
	{"take",           command_take,           1.0f, CMD_FLAG_NO_PLAYER_SHOP},
	{"party",          command_party,          0.0f, 0},
	{"gsay",           command_gsay,           1.0f, 0},
	{"push",           command_push_object,    1.0f, CMD_FLAG_NO_PLAYER_SHOP},
	{"left",           command_turn_left,      1.0f, 0},
	{"right",          command_turn_right,     1.0f, 0},
	{"whereami",       command_whereami,       1.0f, 0},
	{"ms_privacy",     command_ms_privacy,     1.0f, 0},
	{"statistics",     command_statistics,     1.0f, 0},
	{"rename",         command_rename_item,    1.0f, CMD_FLAG_NO_PLAYER_SHOP}
};

/** Size of normal commands */
const int CommandsSize = sizeof(Commands) / sizeof(CommArray_s);

/** Emotion commands */
CommArray_s CommunicationCommands [] =
{
	{"nod",           command_nod,           1.0, 0},
	{"dance",         command_dance,         1.0, 0},
	{"kiss",          command_kiss,          1.0, 0},
	{"bounce",        command_bounce,        1.0, 0},
	{"smile",         command_smile,         1.0, 0},
	{"cackle",        command_cackle,        1.0, 0},
	{"laugh",         command_laugh,         1.0, 0},
	{"giggle",        command_giggle,        1.0, 0},
	{"shake",         command_shake,         1.0, 0},
	{"puke",          command_puke,          1.0, 0},
	{"growl",         command_growl,         1.0, 0},
	{"scream",        command_scream,        1.0, 0},
	{"sigh",          command_sigh,          1.0, 0},
	{"sulk",          command_sulk,          1.0, 0},
	{"hug",           command_hug,           1.0, 0},
	{"cry",           command_cry,           1.0, 0},
	{"poke",          command_poke,          1.0, 0},
	{"accuse",        command_accuse,        1.0, 0},
	{"grin",          command_grin,          1.0, 0},
	{"bow",           command_bow,           1.0, 0},
	{"clap",          command_clap,          1.0, 0},
	{"blush",         command_blush,         1.0, 0},
	{"burp",          command_burp,          1.0, 0},
	{"chuckle",       command_chuckle,       1.0, 0},
	{"cough",         command_cough,         1.0, 0},
	{"flip",          command_flip,          1.0, 0},
	{"frown",         command_frown,         1.0, 0},
	{"gasp",          command_gasp,          1.0, 0},
	{"glare",         command_glare,         1.0, 0},
	{"groan",         command_groan,         1.0, 0},
	{"hiccup",        command_hiccup,        1.0, 0},
	{"lick",          command_lick,          1.0, 0},
	{"pout",          command_pout,          1.0, 0},
	{"shiver",        command_shiver,        1.0, 0},
	{"shrug",         command_shrug,         1.0, 0},
	{"slap",          command_slap,          1.0, 0},
	{"smirk",         command_smirk,         1.0, 0},
	{"snap",          command_snap,          1.0, 0},
	{"sneeze",        command_sneeze,        1.0, 0},
	{"snicker",       command_snicker,       1.0, 0},
	{"sniff",         command_sniff,         1.0, 0},
	{"snore",         command_snore,         1.0, 0},
	{"spit",          command_spit,          1.0, 0},
	{"strut",         command_strut,         1.0, 0},
	{"thank",         command_thank,         1.0, 0},
	{"twiddle",       command_twiddle,       1.0, 0},
	{"wave",          command_wave,          1.0, 0},
	{"whistle",       command_whistle,       1.0, 0},
	{"wink",          command_wink,          1.0, 0},
	{"yawn",          command_yawn,          1.0, 0},
	{"beg",           command_beg,           1.0, 0},
	{"bleed",         command_bleed,         1.0, 0},
	{"cringe",        command_cringe,        1.0, 0},
	{"think",         command_think,         1.0, 0},
	{"me",            command_me,            1.0, 0}
};

/** Size of emotion commands */
const int CommunicationCommandSize = sizeof(CommunicationCommands)/ sizeof(CommArray_s);

/** Wizard commands */
CommArray_s WizCommands [] =
{
	{"dmsay",          command_dmsay,                  0.0, 0},
	{"summon",         command_summon,                 0.0, 0},
	{"kick",           command_kick,                   0.0, 0},
	{"inventory",      command_inventory,              0.0, 0},
	{"plugin",         command_loadplugin,             0.0, 0},
	{"pluglist",       command_listplugins,            0.0, 0},
	{"motd_set",       command_motd_set,               0.0, 0},
	{"ban",            command_ban,                    0.0, 0},
	{"teleport",       command_teleport,               0.0, CMD_FLAG_NO_PLAYER_SHOP},
	{"goto",           command_goto,                   0.0, 0},
	{"shutdown",       command_shutdown,               0.0, 0},
	{"shutdown_now",   command_shutdown_now,           0.0, 0},
	{"resetmap",       command_resetmap,               0.0, 0},
	{"plugout",        command_unloadplugin,           0.0, 0},
	{"create",         command_create,                 0.0, CMD_FLAG_NO_PLAYER_SHOP},
	{"addexp",         command_addexp,                 0.0, 0},
	{"malloc",         command_malloc,                 0.0, 0},
	{"maps",           command_maps,                   0.0, 0},
	{"dump",           command_dump,                   0.0, 0},
	{"dm_stealth",     command_dm_stealth,             0.0, 0},
	{"dm_light",       command_dm_light,               0.0, 0},
	{"d_active",       command_dumpactivelist,         0.0, 0},
	{"d_arches",       command_dumpallarchetypes,      0.0, 0},
	{"d_maps",         command_dumpallmaps,            0.0, 0},
	{"d_map",          command_dumpmap,                0.0, 0},
	{"d_belowfull",    command_dumpbelowfull,          0.0, 0},
	{"d_below",        command_dumpbelow,              0.0, 0},
	{"set_map_light",  command_setmaplight,            0.0, 0},
	{"wizpass",        command_wizpass,                0.0, 0},
	{"learn_spell",    command_learn_spell,            0.0, 0},
	{"learn_prayer",   command_learn_special_prayer,   0.0, 0},
	{"ssdumptable",    command_ssdumptable,            0.0, 0},
	{"patch",          command_patch,                  0.0, CMD_FLAG_NO_PLAYER_SHOP},
	{"speed",          command_speed,                  0.0, 0},
	{"strings",        command_strings,                0.0, 0},
	{"debug",          command_debug,                  0.0, 0},
	{"freeze",         command_freeze,                 0.0, 0},
	{"remove",         command_remove,                 0.0, CMD_FLAG_NO_PLAYER_SHOP},
	{"stats",          command_stats,                  0.0, 0},
	{"forget_spell",   command_forget_spell,           0.0, 0},
	{"set_god",        command_setgod,                 0.0, 0},
	{"dm_password",    command_dm_password,            0.0, 0},
	{"follow",         command_follow,                 0.0, CMD_FLAG_NO_PLAYER_SHOP},
	{"insert_into",    command_insert_into,            0.0, CMD_FLAG_NO_PLAYER_SHOP},
	{"arrest",         command_arrest,                 0.0, 0},
	{"cmd_permission", command_cmd_permission,         0.0, 0},
	{"map_save",       command_map_save,               0.0, 0},
	{"map_reset",      command_map_reset,              0.0, 0},
	{"map_patch",      command_map_patch,              0.0, 0},
	{"no_shout",       command_no_shout,               0.0, 0},
	{"dmtake",         command_dmtake,                 0.0, 0}
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
 * Initialize all the commands and emotes.
 *
 * Sorts the commands and emotes using qsort(). */
void init_commands()
{
	qsort((char *) Commands, CommandsSize, sizeof(CommArray_s), compare_A);
	qsort((char *) CommunicationCommands, CommunicationCommandSize, sizeof(CommArray_s), compare_A);
	qsort((char *) WizCommands, WizCommandsSize, sizeof(CommArray_s), compare_A);
}

/**
 * Find a command element.
 * @param cmd The command name.
 * @param commarray The commands array to search in.
 * @param commsize The commands array size.
 * @return The command entry in the array if found. */
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
 * Check whether the specified player is able to perform a DM command.
 * @param pl Player being checked.
 * @param command Command that 'pl' wants to perform.
 * @return 1 if the player can do the command, 0 otherwise. */
int can_do_wiz_command(player *pl, const char *command)
{
	int i;

	/* We are the wizard. */
	if (QUERY_FLAG(pl->ob, FLAG_WIZ))
	{
		return 1;
	}

	/* No permission? */
	if (!pl->cmd_permissions)
	{
		return 0;
	}

	/* Check inside command permissions for this command. */
	for (i = 0; i < pl->num_cmd_permissions; i++)
	{
		if (pl->cmd_permissions[i] && !strcmp(command, pl->cmd_permissions[i]))
		{
			return 1;
		}
	}

	return 0;
}

/**
 * Execute a command by player.
 * @param pl The player who is issuing the command.
 * @param command The command.
 * @return 0 if not a valid command, otherwise return value of the called
 * command function is returned. */
int execute_newserver_command(object *pl, char *command)
{
	CommArray_s *csp;
	char *cp;

	/* Sanity check: all commands must begin with a slash, and there must be
	 * something else after the slash. */
	if (command[0] != '/' || !command[1])
	{
		new_draw_info_format(NDI_UNIQUE, pl, "'%s' is not a valid command.", command);
		return 0;
	}

	/* Jump over the slash. */
	command++;

	/* Remove the command from the parameters. */
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

	/* First look in the normal commands. */
	csp = find_command_element(command, Commands, CommandsSize);

	/* Not found? Try the emote commands. */
	if (!csp)
	{
		csp = find_command_element(command, CommunicationCommands, CommunicationCommandSize);
	}

	/* If still not found and we're a DM or have permission for the command,
	 * look in DM commands. */
	if (!csp && can_do_wiz_command(CONTR(pl), command))
	{
		csp = find_command_element(command, WizCommands, WizCommandsSize);

		if (csp)
		{
			LOG(llevInfo, "WIZ: %s: /%s %s\n", pl->name, command, STRING_SAFE(cp));
		}
	}

	/* Still not found? Maybe a plugin has registered this command. */
	if (!csp)
	{
		csp = find_plugin_command(command);
	}

	/* We didn't find the command anywhere. */
	if (!csp)
	{
		new_draw_info_format(NDI_UNIQUE, pl, "'/%s' is not a valid command.", command);
		return 0;
	}

	if (csp->flags & CMD_FLAG_NO_PLAYER_SHOP && QUERY_FLAG(pl, FLAG_PLAYER_SHOP))
	{
		new_draw_info(NDI_UNIQUE, pl, "You can't do that while in player shop.");
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
