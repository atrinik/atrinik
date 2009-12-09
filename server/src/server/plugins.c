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
 * Handles the plugins code. */

#include <plugin.h>

#ifndef __CEXTRACT__
#include <sproto.h>
#endif

#include <plugproto.h>

/** Table of all loaded plugins */
CFPlugin PlugList[32];

/** Number of loaded plugins. */
int PlugNR = 0;

/**
 * Browse through the inventory of an object to find first event that
 * matches the event type of event_nr.
 * @param op The object to search in
 * @param event_nr The event number. See @ref event_numbers for a list of
 * possible event numbers.
 * @return Script object matching the event type */
object *get_event_object(object *op, int event_nr)
{
	object *tmp;

	/* For this first implementation we simply browse
	 * through the inventory of object op and stop
	 * when we find a script object from type event_nr. */
	for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
	{
		if (tmp->type == TYPE_EVENT_OBJECT && tmp->sub_type1 == event_nr)
		{
			return tmp;
		}
	}

	return tmp;
}

/**
 * Tries to find if a given command is handled by a plugin.
 * @note This function is called <b>before</b> the internal commands
 * are checked, meaning that you can "overwrite" them.
 * @param cmd The command name to find
 * @param op Object doing this command
 * @return Command array structure if found, NULL otherwise */
CommArray_s *find_plugin_command(const char *cmd, object *op)
{
	CFParm CmdParm;
	CFParm* RTNValue;
	int i;
	char cmdchar[10];
	static CommArray_s RTNCmd;

	strcpy(cmdchar, "command?");
	CmdParm.Value[0] = cmdchar;
	CmdParm.Value[1] = (char *)cmd;
	CmdParm.Value[2] = op;

	for (i = 0; i < PlugNR; i++)
	{
		RTNValue = (PlugList[i].propfunc(&CmdParm));

		if (RTNValue != NULL)
		{
			RTNCmd.name = (char *)(RTNValue->Value[0]);
			RTNCmd.func = (CommFunc)(RTNValue->Value[1]);
			RTNCmd.time = *(float *)(RTNValue->Value[2]);
			LOG(llevInfo, "RTNCMD: name %s, time %f\n", RTNCmd.name, RTNCmd.time);

			return &RTNCmd;
		}
	}

	return NULL;
}

/**
 * Display a list of loaded and loadable plugins in
 * player's window.
 * @param op The player to print the plugins to */
void displayPluginsList(object *op)
{
	char buf[MAX_BUF];
	struct dirent *currentfile;
	DIR *plugdir;
	int i;

	new_draw_info(NDI_UNIQUE, 0, op, "List of loaded plugins:");
	new_draw_info(NDI_UNIQUE, 0, op, "-----------------------");

	for (i = 0; i < PlugNR; i++)
	{
		new_draw_info_format(NDI_UNIQUE, 0, op, "%s, %s", PlugList[i].id, PlugList[i].fullname);
	}

	snprintf(buf, sizeof(buf), "%s/", PLUGINDIR);

	/* Open the plugins directory */
	if (!(plugdir = opendir(buf)))
	{
		return;
	}

	new_draw_info(NDI_UNIQUE, 0, op, "\nList of loadable plugins:");
	new_draw_info(NDI_UNIQUE, 0, op, "-----------------------");

	/* Go through the files in the directory */
	while ((currentfile = readdir(plugdir)))
	{
		if (strcmp(currentfile->d_name, "..") && strcmp(currentfile->d_name, ".") && !strstr(currentfile->d_name, ".txt"))
		{
			new_draw_info(NDI_UNIQUE, 0, op, currentfile->d_name);
		}
	}

	closedir(plugdir);
}

/**
 * Searches in the loaded plugins list for a plugin with a keyname of id.
 * @param id The keyname
 * @return The position of the plugin in the list if a matching one was
 * found or -1 if no correct plugin was detected. */
int findPlugin(const char *id)
{
	int i;

	for (i = 0; i < PlugNR; i++)
	{
		if (!strcmp(id, PlugList[i].id))
		{
			return i;
		}
	}

	return -1;
}

/**
 * Initializes plugins. Browses the plugins directory and calls
 * initOnePlugin for each file found, unless the file has ".txt"
 * in the name. */
void initPlugins()
{
	struct dirent *currentfile;
	DIR *plugdir;
	char plugindir_path[MAX_BUF], pluginfile[MAX_BUF];

	LOG(llevInfo, "Initializing plugins:\n");
	snprintf(plugindir_path, sizeof(plugindir_path), "%s/", PLUGINDIR);
	LOG(llevInfo, "Plugins directory is %s\n", plugindir_path);

	if (!(plugdir = opendir(plugindir_path)))
	{
		return;
	}

	while ((currentfile = readdir(plugdir)))
	{
		/* Don't load "." or ".." marker and files which have ".txt" inside */
		if (strcmp(currentfile->d_name, "..") && strcmp(currentfile->d_name, ".") && !strstr(currentfile->d_name, ".txt"))
		{
			snprintf(pluginfile, sizeof(pluginfile), "%s%s", plugindir_path, currentfile->d_name);
			LOG(llevInfo, "Registering plugin %s\n", currentfile->d_name);
			initOnePlugin(pluginfile);
		}
	}

	closedir(plugdir);
}

/**
 * Initializes a plugin known by its filename.
 *
 * The initialization process has several stages:
 * <ul>
 *   <li>Loading of the shared library itself</li>
 *   <li>Basic plugin information request</li>
 *   <li>CF Plugin specific initialization tasks (call to initPlugin())</li>
 *   <li>Hook bindings</li>
 * </ul>
 * @param pluginfile The plugin filename */
void initOnePlugin(const char *pluginfile)
{
	int i;
#ifdef WIN32
	HMODULE DLLInstance;
#else
	void *ptr = NULL;
#endif

#ifdef WIN32
	if ((DLLInstance = LoadLibrary(pluginfile)) == NULL)
#else
	if ((ptr = dlopen(pluginfile, RTLD_NOW | RTLD_GLOBAL)) == NULL)
#endif
	{
#ifdef WIN32
		LOG(llevBug, "BUG: Error while trying to load %s\n", pluginfile);
#else
		LOG(llevBug, "BUG: Error while trying to load %s, returned: %s\n", pluginfile, dlerror());
#endif

		return;
	}

#ifdef WIN32
	PlugList[PlugNR].libptr = DLLInstance;
	PlugList[PlugNR].initfunc = (f_plugin) (GetProcAddress(DLLInstance, "initPlugin"));
#else
	PlugList[PlugNR].libptr = ptr;
	PlugList[PlugNR].initfunc = (f_plugin) (dlsym(ptr, "initPlugin"));
#endif

	if (PlugList[PlugNR].initfunc == NULL)
	{
#ifdef WIN32
		LOG(llevBug, "BUG: Plugin init error\n");
#else
		LOG(llevBug, "BUG: Plugin init error: %s\n", dlerror());
#endif

		return;
	}
	else
	{
		CFParm *InitParm = PlugList[PlugNR].initfunc(NULL);

		LOG(llevInfo, "Plugin name: %s, known as %s\n", (char *) (InitParm->Value[1]), (char *) (InitParm->Value[0]));

		strcpy(PlugList[PlugNR].id, (char *) (InitParm->Value[0]));
		strcpy(PlugList[PlugNR].fullname, (char *) (InitParm->Value[1]));
	}

#ifdef WIN32
	PlugList[PlugNR].hookfunc = (f_plugin)(GetProcAddress(DLLInstance, "registerHook"));
	PlugList[PlugNR].eventfunc = (f_plugin)(GetProcAddress(DLLInstance, "triggerEvent"));
	PlugList[PlugNR].pinitfunc = (f_plugin)(GetProcAddress(DLLInstance, "postinitPlugin"));
	PlugList[PlugNR].propfunc = (f_plugin)(GetProcAddress(DLLInstance, "getPluginProperty"));
#else
	PlugList[PlugNR].hookfunc = (f_plugin)(dlsym(ptr, "registerHook"));
	PlugList[PlugNR].eventfunc = (f_plugin)(dlsym(ptr, "triggerEvent"));
	PlugList[PlugNR].pinitfunc = (f_plugin)(dlsym(ptr, "postinitPlugin"));
	PlugList[PlugNR].propfunc = (f_plugin)(dlsym(ptr, "getPluginProperty"));
#endif

	if (PlugList[PlugNR].pinitfunc == NULL)
	{
#ifdef WIN32
		LOG(llevBug, "BUG: Plugin postinit error\n");
#else
		LOG(llevBug, "BUG: Plugin postinit error: %s\n", dlerror());
#endif

		return;
	}

	for (i = 0; i < NR_EVENTS; i++)
	{
		PlugList[PlugNR].gevent[i] = 0;
	}

	if (PlugList[PlugNR].hookfunc == NULL)
	{
#ifdef WIN32
		LOG(llevBug, "BUG: Plugin hook error\n");
#else
		LOG(llevBug, "BUG: Plugin hook error: %s\n", dlerror());
#endif

		return;
	}
	else
	{
		CFParm *HookParm = (CFParm *)(malloc(sizeof(CFParm)));

#ifdef WIN32
		HookParm->Value[0] = (int *)(malloc(sizeof(int)));
#else
		HookParm->Value[0] = (const int *)(malloc(sizeof(int)));
#endif

		for (i = 1; i <= NR_OF_HOOKS; i++)
		{
			memcpy((void *) HookParm->Value[0], &i, sizeof(int));

			HookParm->Value[1] = HookList[i];

			PlugList[PlugNR].hookfunc(HookParm);
		}

		free((void *) HookParm->Value[0]);

		free(HookParm);
	}

	if (PlugList[PlugNR].eventfunc == NULL)
	{
#ifdef WIN32
		LOG(llevBug, "BUG: Event plugin error\n");
#else
		LOG(llevBug, "BUG: Event plugin error %s\n", dlerror());
#endif

		return;
	}

	PlugNR++;
	PlugList[PlugNR - 1].pinitfunc(NULL);

	LOG(llevInfo, "[Done]\n");
}

/**
 * Removes one plugin from memory. The plugin is identified by its keyname.
 * @param id The plugin keyname */
void removeOnePlugin(const char *id)
{
	int plid = findPlugin(id), j;

	if (plid < 0)
	{
		return;
	}

	/* We unload the library... */
#ifdef WIN32
	FreeLibrary(PlugList[plid].libptr);
#else
	dlclose(PlugList[plid].libptr);
#endif

	/* Then we copy the rest on the list back one position */
	PlugNR--;

	if (plid == 31)
	{
		return;
	}

	LOG(llevInfo, "Removing plugin: plid: %d, PlugNR: %d\n", plid, PlugNR);

	for (j = plid + 1; j < 32; j++)
	{
		PlugList[j - 1] = PlugList[j];
	}
}

/**
 * @defgroup plugin_hook_functions Plugin hook functions
 * Plugin hook functions. These are wrappers to crosslib functions, used
 * by plugins.
 *
 * @note <b>NEVER</b> call crosslib functions directly from a plugin if a
 * hook exists.
 *@{*/

/**
 * LOG() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Level of logging.
 * - <b>1</b>: Message string. */
CFParm *CFWLog(CFParm *PParm)
{
	LOG(*(int *) (PParm->Value[0]), "%s", (char *) (PParm->Value[1]));
	return NULL;
}

/**
 * fix_player() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: (player) object. */
CFParm *CFWFixPlayer(CFParm *PParm)
{
	fix_player((object *) (PParm->Value[0]));
	return NULL;
}

/**
 * new_info_map() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Color (flags) information.
 * - <b>1</b>: Map where the message should be heard.
 * - <b>2</b>: X position.
 * - <b>3</b>: Y position.
 * - <b>4</b>: Maximum distance message can be heard from.
 * - <b>5</b>: String message. */
CFParm *CFWNewInfoMap(CFParm *PParm)
{
	new_info_map(*(int *) (PParm->Value[0]), (struct mapdef *) (PParm->Value[1]), *(int *) (PParm->Value[2]), *(int *) (PParm->Value[3]), *(int *) (PParm->Value[4]), (char *) (PParm->Value[5]));
	return NULL;
}

/**
 * new_info_map_except() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Color (flags) information.
 * - <b>1</b>: Map where the message should be heard.
 * - <b>2</b>: X position.
 * - <b>3</b>: Y position.
 * - <b>4</b>: Maximum distance message can be heard from.
 * - <b>5</b>: Object 1 which should not hear this.
 * - <b>6</b>: Object 2 which should not hear this (can be NULL or same
 *   as the other one).
 * - <b>7</b>: String message. */
CFParm *CFWNewInfoMapExcept(CFParm *PParm)
{
	new_info_map_except(*(int *) (PParm->Value[0]), (struct mapdef *) (PParm->Value[1]), *(int *) (PParm->Value[2]), *(int *) (PParm->Value[3]), *(int *) (PParm->Value[4]), (object *) (PParm->Value[5]), (object *) (PParm->Value[6]), (char *) (PParm->Value[7]));
	return NULL;
}

/**
 * spring_trap() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Trap.
 * - <b>1</b>: Victim. */
CFParm *CFWSpringTrap(CFParm *PParm)
{
	spring_trap((object *) (PParm->Value[0]), (object *) (PParm->Value[1]));
	return NULL;
}

/**
 * cast_spell() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: op.
 * - <b>1</b>: Caster.
 * - <b>2</b>: Direction.
 * - <b>3</b>: Type of casting.
 * - <b>4</b>: Is it an ability or a wizard spell?
 * - <b>5</b>: Spell type.
 * - <b>6</b>: Optional arguments. */
CFParm *CFWCastSpell(CFParm *PParm)
{
	static int val;
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));

	val = cast_spell((object *) (PParm->Value[0]), (object *) (PParm->Value[1]), *(int *) (PParm->Value[2]), *(int *) (PParm->Value[3]), *(int *) (PParm->Value[4]), *(SpellTypeFrom *) (PParm->Value[5]), (char *) (PParm->Value[6]));
	CFP->Value[0] = (void *) (&val);

	return CFP;
}

/**
 * command_rskill() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Player.
 * - <b>1</b>: Parameters. */
CFParm *CFWCmdRSkill(CFParm *PParm)
{
	static int val;
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));

	val = command_rskill((object *) (PParm->Value[0]), (char *) (PParm->Value[1]));
	CFP->Value[0] = (void *) (&val);

	return CFP;
}

/**
 * become_follower() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object to change.
 * - <b>1</b>: New god object. */
CFParm *CFWBecomeFollower(CFParm *PParm)
{
	become_follower((object *) (PParm->Value[0]), (object *) (PParm->Value[1]));
	return NULL;
}

/**
 * pick_up() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Picker object.
 * - <b>1</b>: Object to pick up. */
CFParm *CFWPickup(CFParm *PParm)
{
	pick_up((object *) (PParm->Value[0]), (object *) (PParm->Value[1]));
	return NULL;
}

/**
 * get_map_ob() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Map.
 * - <b>1</b>: X position.
 * - <b>2</b>: Y position. */
CFParm *CFWGetMapObject(CFParm *PParm)
{
	object *val = NULL;
	static CFParm CFP;
	mapstruct *mt = (mapstruct *) PParm->Value[0];
	int x = *(int *) PParm->Value[1];
	int y = *(int *) PParm->Value[2];

	/* Tiled map check */
	if ((mt = out_of_map(mt, &x, &y)))
	{
		val = get_map_ob(mt, x, y);
	}

	CFP.Value[0] = (void *) (val);

	return &CFP;
}

/**
 * out_of_map() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Map.
 * - <b>1</b>: X position.
 * - <b>2</b>: Y position. */
CFParm *CFWOutOfMap(CFParm *PParm)
{
	static CFParm CFP;
	mapstruct *mt = (mapstruct *) PParm->Value[0];
	int *x = (int *) PParm->Value[1];
	int *y = (int *) PParm->Value[2];

	CFP.Value[0] = (void *) out_of_map(mt, x, y);

	return &CFP;
}

/**
 * esrv_send_item() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Player object.
 * - <b>1</b>: Object to update. */
CFParm *CFWESRVSendItem(CFParm *PParm)
{
	esrv_send_item((object *) (PParm->Value[0]), (object *) (PParm->Value[1]));
	return(PParm);
}

/**
 * find_player() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Name of the player to find. */
CFParm *CFWFindPlayer(CFParm *PParm)
{
	player *pl;
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));

	pl = find_player((char *) (PParm->Value[0]));
	CFP->Value[0] = (void *) (pl);

	return CFP;
}

/**
 * manual_apply() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object applying.
 * - <b>1</b>: Object to apply.
 * - <b>2</b>: Apply flags. */
CFParm *CFWManualApply(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	static int val;

	val = manual_apply((object *) (PParm->Value[0]), (object *) (PParm->Value[1]), *(int *) (PParm->Value[2]));
	CFP->Value[0] = &val;

	return CFP;
}

/**
 * command_drop() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Player.
 * - <b>1</b>: Parameters string. */
CFParm *CFWCmdDrop(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	static int val;

	val = command_drop((object *) (PParm->Value[0]), (char *) (PParm->Value[1]));
	CFP->Value[0] = &val;

	return CFP;
}

/**
 * transfer_ob() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object to transfer.
 * - <b>1</b>: X position.
 * - <b>2</b>: Y position.
 * - <b>3</b>: Random parameter.
 * - <b>4</b>: Originator object.
 * - <b>5</b>: Trap object. */
CFParm *CFWTransferObject(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	static int val;

	val = transfer_ob((object *) (PParm->Value[0]), *(int *) (PParm->Value[1]), *(int *) (PParm->Value[2]), *(int *) (PParm->Value[3]), (object *) (PParm->Value[4]), (object *) (PParm->Value[5]));
	CFP->Value[0] = &val;

	return CFP;
}

/**
 * kill_object() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object to kill.
 * - <b>1</b>: Damage to do.
 * - <b>2</b>: Killer object.
 * - <b>3</b>: Type of killing. */
CFParm *CFWKillObject(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	static int val;

	val = kill_object((object *) (PParm->Value[0]), *(int *) (PParm->Value[1]), (object *) (PParm->Value[2]), *(int *) (PParm->Value[3]));
	CFP->Value[0] = &val;

	return CFP;
}

/**
 * check_spell_known() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object to check.
 * - <b>1</b>: Spell index to search. */
CFParm* CFWCheckSpellKnown(CFParm* PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	static int val;

	val = check_spell_known((object *) (PParm->Value[0]), *(int *) (PParm->Value[1]));
	CFP->Value[0] = &val;

	return CFP;
}

/**
 * look_up_spell_name() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Name of the spell to look up. */
CFParm* CFWGetSpellNr(CFParm* PParm)
{
	static CFParm CFP;
	static int val;

	val = look_up_spell_name((char *) PParm->Value[0]);
	CFP.Value[0] = &val;

	return &CFP;
}

/**
 * do_learn_spell() and do_forget_spell() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object to affect.
 * - <b>1</b>: Spell number to learn or unlearn.
 * - <b>2</b>: Mode. If zero, learn the spell, otherwise unlearn it. */
CFParm *CFWDoLearnSpell(CFParm *PParm)
{
	/* if mode = 1, unlearn - if mode =0 learn */
	if (*(int *) (PParm->Value[2]))
	{
		do_forget_spell((object *) (PParm->Value[0]), *(int *) (PParm->Value[1]));
	}
	else
	{
		/* The 0 parameter is marker for special_prayer - godgiven
		 * spells, which will be deleted when player changes god. */
		do_learn_spell((object *) (PParm->Value[0]), *(int *) (PParm->Value[1]), 0);
	}

	return NULL;
}

/**
 * check_skill_known() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object to check.
 * - <b>1</b>: Skill ID to search for. */
CFParm *CFWCheckSkillKnown(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	static int val;

	val = check_skill_known((object *) (PParm->Value[0]), *(int *) (PParm->Value[1]));
	CFP->Value[0] = &val;

	return CFP;
}

/**
 * lookup_skill_by_name() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Name of the skill to look up. */
CFParm *CFWGetSkillNr(CFParm *PParm)
{
	static CFParm CFP;
	static int val;

	val = lookup_skill_by_name((char *) PParm->Value[0]);
	CFP.Value[0] = &val;

	return &CFP;
}

/**
 * do_learn_skill() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object to affect.
 * - <b>1</b>: Skill number to learn. */
CFParm *CFWDoLearnSkill(CFParm *PParm)
{
	learn_skill((object *) (PParm->Value[0]), NULL, NULL, *(int *) (PParm->Value[1]), 0);
	return NULL;
}

/**
 * esrv_send_inventory() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Player object.
 * - <b>1</b>: Object to send inventory of. */
CFParm *CFWESRVSendInventory(CFParm *PParm)
{
	esrv_send_inventory((object *) (PParm->Value[0]), (object *) (PParm->Value[1]));
	return NULL;
}

/**
 * create_artifact() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: op.
 * - <b>1</b>: Name of the artifact to create. */
CFParm *CFWCreateArtifact(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	object *val;

	val = create_artifact((object *) (PParm->Value[0]), (char *) (PParm->Value[1]));
	CFP->Value[0] = (void *) (val);

	return CFP;
}

/**
 * get_archetype() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Name of the archetype to search for. */
CFParm *CFWGetArchetype(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	object *val;

	val = get_archetype((char *) (PParm->Value[0]));
	CFP->Value[0] = (void *) (val);

	return CFP;
}

/**
 * update_ob_speed() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object to update. */
CFParm *CFWUpdateSpeed(CFParm *PParm)
{
	update_ob_speed((object *) (PParm->Value[0]));
	return NULL;
}

/**
 * update_object() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object to update.
 * - <b>1</b>: Flags. */
CFParm *CFWUpdateObject(CFParm *PParm)
{
	update_object((object *) (PParm->Value[0]), *(int *) (PParm->Value[1]));
	return NULL;
}

/**
 * find_animation() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Name of the animation to find. */
CFParm *CFWFindAnimation(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	static int val;

	val = find_animation((char *) (PParm->Value[0]));
	CFP->Value[0] = (void *) (&val);

	return CFP;
}

/**
 * get_archetype_by_object_name() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Name to search for. */
CFParm *CFWGetArchetypeByObjectName(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	object *val;

	val = get_archetype_by_object_name((char *) (PParm->Value[0]));
	CFP->Value[0] = (void *) (val);

	return CFP;
}

/**
 * insert_ob_in_ob() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object to insert.
 * - <b>1</b>: Where to insert the object. */
CFParm *CFWInsertObjectInObject(CFParm *PParm)
{
	static CFParm CFP;

	CFP.Value[0] = (void *) insert_ob_in_ob((object *) (PParm->Value[0]), (object *) (PParm->Value[1]));

	return &CFP;
}

/**
 * insert_ob_in_map() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object to insert.
 * - <b>1</b>: Map.
 * - <b>2</b>: Originator of the insertion.
 * - <b>3</b>: Flags. */
CFParm *CFWInsertObjectInMap(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	object *val;

	val = insert_ob_in_map((object *) (PParm->Value[0]), (mapstruct *) (PParm->Value[1]), (object *) (PParm->Value[2]), *(int *) (PParm->Value[3]));
	CFP->Value[0] = (void *) (val);

	return CFP;
}

/**
 * ready_map_name() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Name of the map to ready.
 * - <b>1</b>: Flags. */
CFParm *CFWReadyMapName(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	mapstruct *val;

	val = ready_map_name((char *) (PParm->Value[0]), *(int *) (PParm->Value[1]));
	CFP->Value[0] = (void *) (val);

	return CFP;
}

/**
 * Wrapper to swap apartments.
 * @param PParm Parameters array.
 * - <b>0</b>: Old map path.
 * - <b>1</b>: New map path.
 * - <b>2</b>: New X position.
 * - <b>3</b>: New Y position.
 * - <b>4</b>: Activator object. */
CFParm *CFWSwapApartments(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	char oldmappath[HUGE_BUF], newmappath[HUGE_BUF];
	int x = *(int *) PParm->Value[2], y = *(int *) PParm->Value[3], i, j;
	object *activator = (object *) PParm->Value[4], *op, *tmp, *tmp2, *dummy;
	mapstruct *oldmap, *newmap;
	int val = 1;

	snprintf(oldmappath, sizeof(oldmappath), "%s/%s/%s/%s", settings.localdir, settings.playerdir, activator->name, clean_path((char *) PParm->Value[0]));
	snprintf(newmappath, sizeof(newmappath), "%s/%s/%s/%s", settings.localdir, settings.playerdir, activator->name, clean_path((char *) PParm->Value[1]));

	/* So we can transfer our items from the old apartment. */
	oldmap = ready_map_name(oldmappath, 2);

	if (!oldmap)
	{
		LOG(llevBug, "BUG: CFWSwapApartments(): Could not get oldmap using ready_map_name().\n");
		val = 0;
		CFP->Value[0] = (void *) &val;
		return CFP;
	}

	/* Our new map. */
	newmap = ready_map_name(create_pathname((char *) PParm->Value[1]), 6);

	if (!newmap)
	{
		LOG(llevBug, "BUG: CFWSwapApartments(): Could not get newmap using ready_map_name().\n");
		val = 0;
		CFP->Value[0] = (void *) &val;
		return CFP;
	}

	/* Goes to player directory. */
	FREE_AND_COPY_HASH(newmap->path, newmappath);
	newmap->map_flags |= MAP_FLAG_UNIQUE;

	/* Go through every square on old apartment map, looking for things
	 * to transfer. */
	for (i = 0; i < MAP_WIDTH(oldmap); i++)
	{
		for (j = 0; j < MAP_HEIGHT(oldmap); j++)
		{
			for (op = get_map_ob(oldmap, i, j); op; op = tmp2)
			{
				tmp2 = op->above;

				/* We teleport any possible players here to emergency map. */
				if (op->type == PLAYER)
				{
					dummy = get_object();
					dummy->map = op->map;
					FREE_AND_COPY_HASH(EXIT_PATH(dummy), EMERGENCY_MAPPATH);
					FREE_AND_COPY_HASH(dummy->name, EMERGENCY_MAPPATH);
					enter_exit(op, dummy);
					continue;
				}

				/* If it's sys_object 1, there's no need to transfer it. */
				if (QUERY_FLAG(op, FLAG_SYS_OBJECT))
				{
					continue;
				}

				/* A pickable item... Tranfer it */
				if (!QUERY_FLAG(op, FLAG_NO_PICK))
				{
					remove_ob(op);
					op->x = x;
					op->y = y;
					insert_ob_in_map(op, newmap, NULL, INS_NO_MERGE | INS_NO_WALK_ON);
				}
				/* Fixed part of map */
				else
				{
					/* Now we test for containers, because player
					 * can have items stored in it. So, go through
					 * the container and look for things to transfer. */
					for (tmp = op->inv; tmp; tmp = tmp2)
					{
						tmp2 = tmp->below;

						if (QUERY_FLAG(tmp, FLAG_SYS_OBJECT) || QUERY_FLAG(tmp, FLAG_NO_PICK))
						{
							continue;
						}

						remove_ob(tmp);
						tmp->x = x;
						tmp->y = y;
						insert_ob_in_map(tmp, newmap, NULL, INS_NO_MERGE | INS_NO_WALK_ON);
					}
				}
			}
		}
	}

	/* Save the map */
	new_save_map(newmap, 0);

	/* Check for old save bed */
	if (strcmp(oldmap->path, CONTR(activator)->savebed_map) == 0)
	{
		strcpy(CONTR(activator)->savebed_map, "");
	}

	unlink(oldmap->path);

	/* Free the maps */
	free_map(newmap, 1);
	free_map(oldmap, 1);

	/* Success! */
	CFP->Value[0] = (void *) &val;
	return CFP;
}

/**
 * Wrapper to check if a player exists.
 * @param PParm Parameters array.
 * - <b>0</b>: Player name to find. */
CFParm *CFWPlayerExists(CFParm *PParm)
{
	static int val;
	char *player_name = (char *) PParm->Value[0], filename[MAX_BUF];
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	FILE *fp;

	snprintf(filename, sizeof(filename), "%s/%s/%s/%s.pl", settings.localdir, settings.playerdir, player_name, player_name);
	fp = fopen(filename, "r");

	if (fp)
	{
		fclose(fp);
		val = 1;
	}
	else
	{
		val = 0;
	}

	CFP->Value[0] = (void *) &val;
	return CFP;
}

/**
 * add_exp() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object to increase experience of.
 * - <b>1</b>: Amount of experience to add.
 * - <b>2</b>: Skill number to add experience in. */
CFParm *CFWAddExp(CFParm *PParm)
{
	add_exp((object *) (PParm->Value[0]), *(int *) (PParm->Value[1]), *(int *) (PParm->Value[2]));
	return PParm;
}

/**
 * determine_god() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object to determine the god of. */
CFParm* CFWDetermineGod(CFParm* PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	const char *val;

	val = determine_god((object *) (PParm->Value[0]));
	CFP->Value[0] = (void *) (val);

	return CFP;
}

/**
 * find_god() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Name of the god to search for. */
CFParm* CFWFindGod(CFParm* PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	object *val;

	val = find_god((char *) (PParm->Value[0]));
	CFP->Value[0] = (void *) (val);

	return CFP;
}

/**
 * dump_me() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object to dump. */
CFParm *CFWDumpObject(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	char *val = (char *) (malloc(sizeof(char) * 10240));

	dump_me((object *) (PParm->Value[0]), val);
	CFP->Value[0] = (void *) (val);

	return CFP;
}

/**
 * load_object_str() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object dump string to load. */
CFParm *CFWLoadObject(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	object *val;

	val = load_object_str((char *) (PParm->Value[0]));
	CFP->Value[0] = (void *) (val);

	return CFP;
}

/**
 * remove_ob() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object to remove.
 * @todo Drop inventory support. */
CFParm *CFWRemoveObject(CFParm *PParm)
{
	remove_ob((object *) (PParm->Value[0]));
	return NULL;
}

/**
 * FREE_AND_COPY_HASH() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: String to add. */
CFParm *CFWAddString(CFParm *PParm)
{
	static CFParm CFP;
	char *val = (char *) (PParm->Value[0]);

	CFP.Value[0] = NULL;
	FREE_AND_COPY_HASH(CFP.Value[0], val);

	return &CFP;
}

/**
 * FREE_AND_ADD_REF_HASH() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: String to add refcount. */
CFParm *CFWAddRefcount(CFParm *PParm)
{
	static CFParm CFP;
	char *val = (char *) (PParm->Value[0]);

	CFP.Value[0] = NULL;
	FREE_AND_ADD_REF_HASH(CFP.Value[0], val);

	return &CFP;
}

/**
 * FREE_AND_CLEAR_HASH() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: String to free. */
CFParm *CFWFreeString(CFParm *PParm)
{
	char *val = (char *)(PParm->Value[0]);

	FREE_AND_CLEAR_HASH(val);
	return NULL;
}

/**
 * Wrapper to get the first map.
 * @param PParm Unused. */
CFParm *CFWGetFirstMap(CFParm* PParm)
{
	CFParm *CFP = (CFParm *)(malloc(sizeof(CFParm)));

	(void) PParm;

	CFP->Value[0] = (void *) (first_map);
	return CFP;
}

/**
 * Wrapper to get the first player.
 * @param PParm Unused. */
CFParm *CFWGetFirstPlayer(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));

	(void) PParm;

	CFP->Value[0] = (void *) (first_player);
	return CFP;
}

/**
 * Wrapper to get the first archetype.
 * @param PParm Unused. */
CFParm *CFWGetFirstArchetype(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));

	(void) PParm;

	CFP->Value[0] = (void *) (first_archetype);
	return CFP;
}

/**
 * Wrapper to deposit money to bank object.
 * @param PParm Parameters array.
 * - <b>0</b>: Object depositing the money.
 * - <b>1</b>: Bank object.
 * - <b>2</b>: String parameters (how much money to deposit). */
CFParm *CFWDeposit(CFParm *PParm)
{
	static CFParm CFP;
	static int val = 0;
	int pos = 0;
	char *text = (char *) (PParm->Value[2]);
	object *who = (object *) (PParm->Value[0]), *bank = (object *) (PParm->Value[1]);
	_money_block money;

	get_word_from_string(text, &pos);
	get_money_from_string(text + pos , &money);

	CFP.Value[0] = (void *) &val;

	if (!money.mode)
	{
		val = -1;
		new_draw_info(NDI_UNIQUE, 0, who, "Deposit what?\nUse 'deposit all' or 'deposit 40 gold, 20 silver...'");
	}
	else if (money.mode == MONEYSTRING_ALL)
	{
		val = 1;
		bank->value += remove_money_type(who, who, -1, 0);
		fix_player(who);
	}
	else
	{
		if (money.mithril)
		{
			if (query_money_type(who, coins_arch[0]->clone.value) < money.mithril)
			{
				new_draw_info(NDI_UNIQUE, 0, who, "You don't have that much mithril coins.");
				return &CFP;
			}
		}

		if (money.gold)
		{
			if (query_money_type(who, coins_arch[1]->clone.value) < money.gold)
			{
				new_draw_info(NDI_UNIQUE, 0, who, "You don't have that much gold coins.");
				return &CFP;
			}
		}

		if (money.silver)
		{
			if (query_money_type(who, coins_arch[2]->clone.value) < money.silver)
			{
				new_draw_info(NDI_UNIQUE, 0, who, "You don't have that much silver coins.");
				return &CFP;
			}
		}

		if (money.copper)
		{
			if (query_money_type(who, coins_arch[3]->clone.value) < money.copper)
			{
				new_draw_info(NDI_UNIQUE, 0, who, "You don't have that much copper coins.");
				return &CFP;
			}
		}

		/* all ok - now remove the money from the player and add it to the bank object! */
		val = 1;

		if (money.mithril)
		{
			remove_money_type(who, who, coins_arch[0]->clone.value, money.mithril);
		}

		if (money.gold)
		{
			remove_money_type(who, who, coins_arch[1]->clone.value, money.gold);
		}

		if (money.silver)
		{
			remove_money_type(who, who, coins_arch[2]->clone.value, money.silver);
		}

		if (money.copper)
		{
			remove_money_type(who, who, coins_arch[3]->clone.value, money.copper);
		}

		bank->value += money.mithril * coins_arch[0]->clone.value + money.gold * coins_arch[1]->clone.value + money.silver * coins_arch[2]->clone.value + money.copper * coins_arch[3]->clone.value;
		fix_player(who);
	}

	return &CFP;
}

/**
 * Wrapper to withdraw money from bank object.
 * @param PParm Parameters array.
 * - <b>0</b>: Object withdrawing the money.
 * - <b>1</b>: Bank object.
 * - <b>2</b>: String parameters (how much money to withdraw). */
CFParm *CFWWithdraw(CFParm *PParm)
{
	static CFParm CFP;
	static int val = 0;
	int pos = 0;
	sint64 big_value;
	char *text = (char *) (PParm->Value[2]);
	object *who = (object *) (PParm->Value[0]), *bank = (object *) (PParm->Value[1]);
	_money_block money;

	get_word_from_string(text, &pos);
	get_money_from_string(text + pos , &money);
	CFP.Value[0] = (void *) &val;

	if (!money.mode)
	{
		val = -1;
		new_draw_info(NDI_UNIQUE, 0, who, "Withdraw what?\nUse 'withdraw all' or 'withdraw 30 gold, 20 silver...'");
	}
	else if (money.mode == MONEYSTRING_ALL)
	{
		val = 1;
		sell_item(NULL, who, bank->value);
		bank->value = 0;
		fix_player(who);
	}
	else
	{
		val = 1;

		/* Just to set a border.... */
		if (money.mithril > 100000 || money.gold > 100000 || money.silver > 1000000 || money.copper > 1000000)
		{
			new_draw_info(NDI_UNIQUE, 0, who, "Withdraw values are too high.");
			return &CFP;
		}

		big_value = money.mithril * coins_arch[0]->clone.value + money.gold * coins_arch[1]->clone.value + money.silver * coins_arch[2]->clone.value + money.copper * coins_arch[3]->clone.value;

		if (big_value > bank->value)
		{
			val = 0;
			return &CFP;
		}

		if (money.mithril)
		{
			insert_money_in_player(who, &coins_arch[0]->clone, money.mithril);
		}

		if (money.gold)
		{
			insert_money_in_player(who, &coins_arch[1]->clone, money.gold);
		}

		if (money.silver)
		{
			insert_money_in_player(who, &coins_arch[2]->clone, money.silver);
		}

		if (money.copper)
		{
			insert_money_in_player(who, &coins_arch[3]->clone, money.copper);
		}

		bank->value -= big_value;
		fix_player(who);
	}

	return &CFP;
}

/**
 * cost_string_from_value() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Value as integer. */
CFParm *CFWShowCost(CFParm *PParm)
{
	static CFParm CFP;

	CFP.Value[0] = cost_string_from_value(*(sint64 *) (PParm->Value[0]));
	return &CFP;
}

/**
 * query_cost() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object to evaluate.
 * - <b>1</b>: Who tries to sell or buy it.
 * - <b>2</b>: Flags. */
CFParm *CFWQueryCost(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	object *whatptr = (object *) (PParm->Value[0]), *whoptr = (object *) (PParm->Value[1]);
	int flag = *(int *) (PParm->Value[2]);
	static double val;

	val = query_cost(whatptr, whoptr, flag);
	CFP->Value[0] = (void *) &val;

	return CFP;
}

/**
 * query_money() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object we are looking for solvability at. */
CFParm *CFWQueryMoney(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	object *whoptr = (object *) (PParm->Value[0]);
	static int val;

	val = query_money(whoptr);
	CFP->Value[0] = (void *) &val;

	return CFP;
}

/**
 * pay_for_item() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object to pay for.
 * - <b>1</b>: Who is trying to buy the object. */
CFParm *CFWPayForItem(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	object *whatptr = (object *) (PParm->Value[0]), *whoptr = (object *) (PParm->Value[1]);
	static int val;

	val = pay_for_item(whatptr, whoptr);
	CFP->Value[0] = (void *) &val;

	return CFP;
}

/**
 * pay_for_amount() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Amount to pay.
 * - <b>1</b>: Who tries to pay for it. */
CFParm *CFWPayForAmount(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	int amount = *(int *) (PParm->Value[0]);
	object *whoptr = (object *) (PParm->Value[1]);
	static int val;

	val = pay_for_amount(amount, whoptr);
	CFP->Value[0] = (void *) &val;

	return CFP;
}

/**
 * new_draw_info() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Flags.
 * - <b>1</b>: Priority.
 * - <b>2</b>: Player object.
 * - <b>3</b>: Message. */
CFParm *CFWNewDrawInfo(CFParm *PParm)
{
	new_draw_info(*(int *) (PParm->Value[0]), *(int *) (PParm->Value[1]), (object *) (PParm->Value[2]), (char *) (PParm->Value[3]));
	return NULL;
}

/**
 * move_player() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Player to move.
 * - <b>1</b>: Direction of move. */
CFParm *CFWMovePlayer(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	static int val;

	val = move_player((object *) PParm->Value[0], *(int *) PParm->Value[1]);
	CFP->Value[0] = (void *) &val;

	return CFP;
}

/**
 * move_ob() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object to move.
 * - <b>1</b>: Direction of move.
 * - <b>2</b>: Originator. */
CFParm *CFWMoveObject(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	static int val;

	val = move_ob((object *) PParm->Value[0], *(int *) PParm->Value[1], (object *) PParm->Value[2]);
	CFP->Value[0] = (void *) &val;

	return CFP;
}

/**
 * send_plugin_custom_message() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Player object.
 * - <b>1</b>: Command ID.
 * - <b>2</b>: Message. */
CFParm *CFWSendCustomCommand(CFParm *PParm)
{
	send_plugin_custom_message((object *) (PParm->Value[0]), *(char *) PParm->Value[1], (char *) (PParm->Value[2]));
	return NULL;
}

/**
 * cftimer_create() wrapper.
 * @param PParm Parameters array. */
CFParm *CFWCFTimerCreate(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	static int val;

	val = cftimer_create(*(int *) (PParm->Value[0]), *(long *) (PParm->Value[1]), (object *) (PParm->Value[2]), *(int *) (PParm->Value[3]));
	CFP->Value[0] = (void *) (&val);

	return CFP;
}

/**
 * cftimer_destroy() wrapper.
 * @param PParm Parameters array. */
CFParm *CFWCFTimerDestroy(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	static int val;

	val = cftimer_destroy(*(int *) (PParm->Value[0]));
	CFP->Value[0] = (void *) (&val);

	return CFP;
}

/**
 * SET_ANIMATION() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object.
 * - <b>1</b>: Face. */
CFParm *CFWSetAnimation(CFParm *PParm)
{
	object *op = (object *) PParm->Value[0];
	int face = *(int *) PParm->Value[1];

	if (face != -1)
	{
		SET_ANIMATION(op, face);
	}

	update_object(op, UP_OBJ_FACE);
	return PParm;
}

/**
 * communicate() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object.
 * - <b>1</b>: String. */
CFParm *CFWCommunicate(CFParm *PParm)
{
	object *op = (object *) PParm->Value[0];
	char *string = (char *) PParm->Value[1];

	if (!op || !string)
	{
		return NULL;
	}

	communicate(op, string);
	return NULL;
}

/**
 * find_best_object_match() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object to find object in inventory.
 * - <b>1</b>: Name. */
CFParm *CFWFindBestObjectMatch(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) (malloc(sizeof(CFParm)));
	object *op = (object *) PParm->Value[0];
	char *param = (char *) PParm->Value[1];
	object *result;

	result = (object *) find_best_object_match(op, param);
	CFP->Value[0] = (void *) result;

	return CFP;
}

/**
 * player_apply_below() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Player object. */
CFParm *CFWApplyBelow(CFParm *PParm)
{
	object *op = (object *) PParm->Value[0];

	if (!op)
	{
		return NULL;
	}

	player_apply_below(op);
	return NULL;
}

/**
 * find_marked_object() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object. */
CFParm *CFWFindMarkedObject(CFParm *PParm)
{
	static CFParm CFP;
	object *op = (object *) PParm->Value[0];

	if (op)
	{
		op = find_marked_object(op);
	}

	CFP.Value[0] = (void *) op;

	return &CFP;
}

/**
 * cast_identify() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Caster.
 * - <b>1</b>: Target object.
 * - <b>2</b>: Single object to identify.
 * - <b>3</b>: Mode. */
CFParm *CFWIdentifyObject(CFParm *PParm)
{
	object *caster = (object *) PParm->Value[0], *target = (object *) PParm->Value[1], *op = (object *) PParm->Value[2];


	cast_identify(target, caster->level, op, *(int *) (PParm->Value[3]));

	if (caster)
	{
		play_sound_map(caster->map, caster->x, caster->y, spells[SP_IDENTIFY].sound, SOUND_SPELL);
	}
	else if (target)
	{
		play_sound_map(target->map, target->x, target->y, spells[SP_IDENTIFY].sound, SOUND_SPELL);
	}

	return NULL;
}

/**
 * object_create_clone() and copy_object() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object.
 * - <b>1</b>: Modes:
 *   - <b>0</b>: Clone with inventory.
 *   - <b>1</b>: Only duplicate the object without its contents and
 *     op->more. */
CFParm *CFWObjectCreateClone(CFParm *PParm)
{
	CFParm *CFP = (CFParm *) malloc(sizeof(CFParm));

	if (*(int *) PParm->Value[1] == 0)
	{
		CFP->Value[0] = object_create_clone((object *) PParm->Value[0]);
	}
	else if (*(int *) PParm->Value[1] == 1)
	{
		object *tmp = get_object();

		copy_object((object *) PParm->Value[0], tmp);
		CFP->Value[0] = tmp;
	}

	return CFP;
}

/**
 * enter_exit() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Object.
 * - <b>1</b>: Map name we use for destination.
 * - <b>2</b>: Map X.
 * - <b>3</b>: Map Y.
 * - <b>4</b>: Unique map?
 * - <b>5</b>: Message, may be NULL. */
CFParm *CFWTeleportObject(CFParm *PParm)
{
	object *current = get_object();

	FREE_AND_COPY_HASH(EXIT_PATH(current), (char *) PParm->Value[1]);
	EXIT_X(current) = *(int *) PParm->Value[2];
	EXIT_Y(current) = *(int *) PParm->Value[3];

	if (*(int *) PParm->Value[4])
	{
		current->last_eat = MAP_PLAYER_MAP;
	}

	if (PParm->Value[5])
	{
		FREE_AND_COPY_HASH(current->msg, (char *) PParm->Value[5]);
	}

	enter_exit((object *) PParm->Value[0], current);

	if (((object *) PParm->Value[0])->map)
	{
		play_sound_map(((object *) PParm->Value[0])->map, ((object *) PParm->Value[0])->x, ((object *) PParm->Value[0])->y, SOUND_TELEPORT, SOUND_NORMAL);
	}

	return NULL;
}

/**
 * The following is not really a wrapper like the others are. It is in
 * fact used to allow the plugin to request the global events it wants to
 * be aware of.
 *
 * All events can be seen as global; on the contrary, some events can't
 * be used as local: for example, BORN is only global.
 * @param PParm Parameters array.
 * - <b>0</b>: Number of the event to register.
 * - <b>1</b>: Name of the requesting plugin. */
CFParm *RegisterGlobalEvent(CFParm *PParm)
{
	int PNR = findPlugin((char *) (PParm->Value[1]));

	LOG(llevDebug, "Plugin %s (%d) registered the event %d\n", (char *) (PParm->Value[1]), PNR, *(int *) (PParm->Value[0]));

	PlugList[PNR].gevent[*(int *) (PParm->Value[0])] = 1;
	return NULL;
}

/**
 * Unregisters a global event.
 * @param PParm Parameters array.
 * - <b>0</b>: Number of the event to unregister.
 * - <b>1</b>: Name of the requesting plugin. */
CFParm *UnregisterGlobalEvent(CFParm *PParm)
{
	int PNR = findPlugin((char *) (PParm->Value[1]));

	PlugList[PNR].gevent[*(int *) (PParm->Value[0])] = 0;
	return NULL;
}

/**
 * When a specific global event occurs, this function is called.
 * @param PParm Parameters array.
 * - <b>0</b>: The global event ID. */
void GlobalEvent(CFParm *PParm)
{
	int i;

	for (i = 0; i < PlugNR; i++)
	{
		if (PlugList[i].gevent[*(int *) (PParm->Value[0])] != 0)
		{
			(PlugList[i].eventfunc)(PParm);
		}
	}
}

/**
 * play_sound_map() wrapper.
 * @param PParm Parameters array.
 * - <b>0</b>: Map.
 * - <b>1</b>: X position.
 * - <b>2</b>: Y position.
 * - <b>3</b>: Sound ID.
 * - <b>4</b>: Type of sound (0 = normal, 1 = spell).
 * @see sound_numbers_normal, sound_numbers_spell */
CFParm *CFWPlaySoundMap(CFParm *PParm)
{
	play_sound_map((mapstruct *) (PParm->Value[0]), *(int *) (PParm->Value[1]), *(int *) (PParm->Value[2]), *(int *) (PParm->Value[3]), *(int *) (PParm->Value[4]));
	return NULL;
}

/**
 * Wrapper to create an object.
 * @param PParm Parameters array.
 * - <b>0</b>: Archetype.
 * - <b>1</b>: Map.
 * - <b>2</b>: X position on map.
 * - <b>3</b>: Y position on map. */
CFParm *CFWCreateObject(CFParm *PParm)
{
	static CFParm CFP;
	archetype *arch;
	object *newobj;

	CFP.Value[0] = NULL;

	if (!(arch = find_archetype((char *) (PParm->Value[0]))))
	{
		return &CFP;
	}

	if (!(newobj = arch_to_object(arch)))
	{
		return &CFP;
	}

	newobj->x = *(int *) (PParm->Value[2]);
	newobj->y = *(int *) (PParm->Value[3]);

	newobj = insert_ob_in_map(newobj, (mapstruct *) (PParm->Value[1]), NULL, 0);

	CFP.Value[0] = newobj;
	return (&CFP);
}

/**
 * Wrapper to get time of day using get_tod().
 * @param PParm Unused. */
CFParm *CFWGetTod(CFParm *PParm)
{
	static CFParm CFP;
	timeofday_t tod;

	(void) PParm;

	get_tod(&tod);

	CFP.Value[0] = &tod;

	return &CFP;
}

/**
 * Wrapper for get_ob_key_value().
 * @param PParm Parameters array.
 * - <b>0</b>: Object.
 * - <b>1</b>: Key. */
CFParm *CFWGetObKeyValue(CFParm *PParm)
{
	static CFParm CFP;

	CFP.Value[0] = (void *) get_ob_key_value((object *) (PParm->Value[0]), (char *) (PParm->Value[1]));
	return &CFP;
}

/**
 * Wrapper for set_ob_key_value().
 * @param PParm Parameters array.
 * - <b>0</b>: Object.
 * - <b>1</b>: Key.
 * - <b>2</b>: Value.
 * - <b>3</b>: Add the value if it doesn't exist? */
CFParm *CFWSetObKeyValue(CFParm *PParm)
{
	static CFParm CFP;
	int ret = set_ob_key_value((object *) (PParm->Value[0]), (char *) (PParm->Value[1]), (char *) (PParm->Value[2]), *(int *) (PParm->Value[3]));

	CFP.Value[0] = (void *) &ret;
	return &CFP;
}

/*@}*/

/**
 * Handles triggering global events like EVENT_BORN, EVENT_MAPRESET,
 * etc.
 * @param event_type The event type
 * @param parm1 First parameter
 * @param parm2 Second parameter */
void trigger_global_event(int event_type, void *parm1, void *parm2)
{
#ifdef PLUGINS
	int evtid = event_type;
	CFParm CFP;

	CFP.Value[0] = (void *) (&evtid);
	CFP.Value[1] = (void *) (parm1);
	CFP.Value[2] = (void *) (parm2);

	GlobalEvent(&CFP);
#endif
}

/**
 * Handles triggering normal events like EVENT_ATTACK, EVENT_STOP,
 * etc.
 * @param event_type The event type
 * @param activator Activator object
 * @param me Object the event object is in
 * @param other Other object
 * @param msg Message
 * @param parm1 First parameter
 * @param parm2 Second parameter
 * @param parm3 Third parameter
 * @param flags Event flags
 * @return 1 if the event returns an event value, 0 otherwise */
int trigger_event(int event_type, object *const activator, object *const me, object *const other, const char *msg, int *parm1, int *parm2, int *parm3, int flags)
{
#ifdef PLUGINS
	CFParm CFP;
	object *event_obj;
	int plugin;

	if (me == NULL || !(me->event_flags & EVENT_FLAG(event_type)))
	{
		return 0;
	}

	if ((event_obj = get_event_object(me, event_type)) == NULL)
	{
		LOG(llevBug, "BUG: object with event flag and no event object: %s\n", STRING_OBJ_NAME(me));
		me->event_flags &= ~(1 << event_type);

		return 0;
	}

	/* Avoid double triggers and infinite loops */
	if (event_type == EVENT_SAY || event_type == EVENT_TRIGGER)
	{
		if ((long) event_obj->damage_round_tag == pticks)
		{
			LOG(llevDebug, "DEBUG: trigger_event(): Event object (type %d) for %s called twice the same round\n", event_type, STRING_OBJ_NAME(me));

			return 0;
		}

		event_obj->damage_round_tag = pticks;
	}

	CFP.Value[0] = &event_type;
	CFP.Value[1] = activator;
	CFP.Value[2] = me;
	CFP.Value[3] = other;
	CFP.Value[4] = (void *) msg;
	CFP.Value[5] = parm1;
	CFP.Value[6] = parm2;
	CFP.Value[7] = parm3;
	CFP.Value[8] = &flags;
	CFP.Value[9] = (char *) event_obj->race;
	CFP.Value[10] = (char *) event_obj->slaying;

	if (event_obj->name && (plugin = findPlugin(event_obj->name)) >= 0)
	{
		int returnvalue;
		CFParm *CFR;
#ifdef TIME_SCRIPTS
		struct timeval start, stop;
		uint64 start_u, stop_u;

		gettimeofday(&start, NULL);
#endif

		CFR = PlugList[plugin].eventfunc(&CFP);

		returnvalue = *(int *) (CFR->Value[0]);

#ifdef TIME_SCRIPTS
		gettimeofday(&stop, NULL);
		start_u = start.tv_sec * 1000000 + start.tv_usec;
		stop_u = stop.tv_sec * 1000000 + stop.tv_usec;

		LOG(llevDebug, "Running time: %2.6f seconds\n", (stop_u - start_u) / 1000000.0);
#endif

		return returnvalue;
	}
	else
	{
		LOG(llevBug, "BUG: event object with unknown plugin: %s, plugin %s\n", STRING_OBJ_NAME(me), STRING_OBJ_NAME(event_obj));
		me->event_flags &= ~(1 << event_type);
	}
#endif

	return 0;
}
