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

/*****************************************************************************/
/* First, the headers. We only include plugin.h, because all other includes  */
/* are done into it, and plugproto.h (which is used only by this file).      */
/*****************************************************************************/
#include <plugin.h>
#include <sounds.h>

#ifndef __CEXTRACT__
#include <sproto.h>
#endif

#include <plugproto.h>

CFPlugin PlugList[32];
int PlugNR = 0;

/* get_event_object()
 * browse through the event_obj chain of the given object the
 * get a inserted script object from it.
 * 1: object
 * 2: EVENT_NR
 * return: script object matching EVENT_NR */
object *get_event_object(object *op, int event_nr)
{
	register object *tmp;
	/* for this first implementation we simply browse
	 * through the inventory of object op and stop
	 * when we find a script object from type event_nr. */
	for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
	{
		if (tmp->type == TYPE_EVENT_OBJECT && tmp->sub_type1 == event_nr)
			return tmp;
	}

	return tmp;
}

/*****************************************************************************/
/* Tries to find if a given command is handled by a plugin.                  */
/* Note that find_plugin_command is called *before* the internal commands are*/
/* checked, meaning that you can "overwrite" them.                           */
/*****************************************************************************/
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

/*****************************************************************************/
/* Searches in the loaded plugins list for a plugin with a keyname of id.    */
/* Returns the position of the plugin in the list if a matching one was found*/
/* or -1 if no correct plugin was detected.                                  */
/*****************************************************************************/
int findPlugin(const char* id)
{
	int i;
	for (i = 0; i < PlugNR; i++)
		if (!strcmp(id, PlugList[i].id))
			return i;
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

#ifdef WIN32
/*****************************************************************************/
/* WIN32 Plugin initialization. Initializes a plugin known by its filename.  */
/* The initialization process has several stages:                            */
/* - Loading of the DLL itself;                                              */
/* - Basical plugin information request;                                     */
/* - CF-Plugin specific initialization tasks (call to initPlugin());         */
/* - Hook bindings;                                                          */
/*****************************************************************************/
void initOnePlugin(const char* pluginfile)
{
	int i = 0;
	HMODULE DLLInstance;
	void *ptr = NULL;
	CFParm* HookParm;

	if ((DLLInstance = LoadLibrary(pluginfile))==NULL)
	{
		LOG(llevBug, "BUG: Error while trying to load %s\n", pluginfile);
		return;
	}
	PlugList[PlugNR].libptr = DLLInstance;
	PlugList[PlugNR].initfunc = (f_plugin)(GetProcAddress(DLLInstance, "initPlugin"));
	if (PlugList[PlugNR].initfunc == NULL)
	{
		LOG(llevBug, "BUG: Plugin init error\n");
		FreeLibrary(ptr);
		return;
	}
	else
	{
		CFParm* InitParm;
		InitParm = PlugList[PlugNR].initfunc(NULL);
		LOG(llevInfo, "Plugin name: %s, known as %s\n", (char *)(InitParm->Value[1]), (char *)(InitParm->Value[0]));
		strcpy(PlugList[PlugNR].id, (char *)(InitParm->Value[0]));
		strcpy(PlugList[PlugNR].fullname, (char *)(InitParm->Value[1]));
	}

	PlugList[PlugNR].hookfunc = (f_plugin)(GetProcAddress(DLLInstance, "registerHook"));
	PlugList[PlugNR].eventfunc = (f_plugin)(GetProcAddress(DLLInstance, "triggerEvent"));
	PlugList[PlugNR].pinitfunc = (f_plugin)(GetProcAddress(DLLInstance, "postinitPlugin"));
	PlugList[PlugNR].propfunc = (f_plugin)(GetProcAddress(DLLInstance, "getPluginProperty"));

	if (PlugList[PlugNR].pinitfunc == NULL)
	{
		LOG(llevBug, "BUG: Plugin postinit error\n");
		FreeLibrary(ptr);
		return;
	}

	for (i = 0; i < NR_EVENTS; i++)
		PlugList[PlugNR].gevent[i] = 0;

	if (PlugList[PlugNR].hookfunc == NULL)
	{
		LOG(llevBug, "BUG: Plugin hook error\n");
		FreeLibrary(ptr);
		return;
	}
	else
	{
		int j;
		i = 0;
		HookParm = (CFParm *)(malloc(sizeof(CFParm)));
		HookParm->Value[0] = (int *)(malloc(sizeof(int)));

		for (j = 1; j <= NR_OF_HOOKS; j++)
		{
			memcpy(HookParm->Value[0], &j, sizeof(int));
			HookParm->Value[1] = HookList[j];

			PlugList[PlugNR].hookfunc(HookParm);
		}
		free(HookParm->Value[0]);
		free(HookParm);
	}

	if (PlugList[PlugNR].eventfunc == NULL)
	{
		LOG(llevBug, "BUG: Event plugin error\n");
		FreeLibrary(ptr);
		return;
	}
	PlugNR++;
	PlugList[PlugNR-1].pinitfunc(NULL);
	LOG(llevInfo, "Done\n");
}

/*****************************************************************************/
/* Removes one plugin from memory. The plugin is identified by its keyname.  */
/*****************************************************************************/
void removeOnePlugin(const char *id)
{
	int plid;
	int j;
	LOG(llevDebug, "Warning - removeOnePlugin non-canon under Win32\n");
	plid = findPlugin(id);
	if (plid < 0)
		return;
	/* We unload the library... */
	FreeLibrary(PlugList[plid].libptr);
	/* Then we copy the rest on the list back one position */
	PlugNR--;
	if (plid == 31)
		return;

	for (j = plid + 1; j < 32; j++)
	{
		PlugList[j - 1] = PlugList[j];
	}
}

#else

#ifndef HAVE_SCANDIR

extern int alphasort(struct dirent **a, struct dirent **b);
#endif



/*****************************************************************************/
/* Removes one plugin from memory. The plugin is identified by its keyname.  */
/*****************************************************************************/
void removeOnePlugin(const char *id)
{
	int plid;
	int j;
	plid = findPlugin(id);

	if (plid < 0)
		return;

	/* We unload the library... */
	dlclose(PlugList[plid].libptr);
	/* Then we copy the rest on the list back one position */
	PlugNR--;

	if (plid == 31)
		return;

	LOG(llevInfo, "plid=%i, PlugNR=%i\n", plid, PlugNR);
	for (j = plid + 1; j < 32; j++)
	{
		PlugList[j - 1] = PlugList[j];
	}
}

/*****************************************************************************/
/* UNIX Plugin initialization. Initializes a plugin known by its filename.   */
/* The initialization process has several stages:                            */
/* - Loading of the DLL itself;                                              */
/* - Basical plugin information request;                                     */
/* - CF-Plugin specific initialization tasks (call to initPlugin());         */
/* - Hook bindings;                                                          */
/*****************************************************************************/
void initOnePlugin(const char* pluginfile)
{
	int i = 0;
	void *ptr = NULL;
	CFParm* HookParm;
	if ((ptr = dlopen(pluginfile, RTLD_NOW | RTLD_GLOBAL)) == NULL)
	{
		LOG(llevInfo,"Plugin error: %s\n", dlerror());
		return;
	}
	PlugList[PlugNR].libptr = ptr;
	PlugList[PlugNR].initfunc = (f_plugin)(dlsym(ptr, "initPlugin"));
	if (PlugList[PlugNR].initfunc == NULL)
	{
		LOG(llevInfo, "Plugin init error: %s\n", dlerror());
	}
	else
	{
		CFParm* InitParm;
		InitParm = PlugList[PlugNR].initfunc(NULL);
		LOG(llevInfo, "Plugin %s loaded under the name of %s\n", (char *)(InitParm->Value[1]), (char *)(InitParm->Value[0]));
		strcpy(PlugList[PlugNR].id, (char *)(InitParm->Value[0]));
		strcpy(PlugList[PlugNR].fullname, (char *)(InitParm->Value[1]));
	}
	PlugList[PlugNR].hookfunc = (f_plugin)(dlsym(ptr, "registerHook"));
	PlugList[PlugNR].eventfunc = (f_plugin)(dlsym(ptr, "triggerEvent"));
	PlugList[PlugNR].pinitfunc = (f_plugin)(dlsym(ptr, "postinitPlugin"));
	PlugList[PlugNR].propfunc = (f_plugin)(dlsym(ptr, "getPluginProperty"));
	LOG(llevInfo, "Done\n");
	if (PlugList[PlugNR].pinitfunc == NULL)
	{
		LOG(llevInfo, "Plugin postinit error: %s\n", dlerror());
	}

	for (i = 0; i < NR_EVENTS; i++)
	{
		PlugList[PlugNR].gevent[i] = 0;
	}

	if (PlugList[PlugNR].hookfunc == NULL)
	{
		LOG(llevInfo, "Plugin hook error: %s\n", dlerror());
	}
	else
	{
		int j;
		i = 0;
		HookParm = (CFParm *)(malloc(sizeof(CFParm)));
		HookParm->Value[0] = (const int *)(malloc(sizeof(int)));

		for (j = 1; j < NR_OF_HOOKS; j++)
		{
			memcpy((void *)HookParm->Value[0], &j, sizeof(int));
			HookParm->Value[1] = HookList[j];
			PlugList[PlugNR].hookfunc(HookParm);
		}
		free((void *)HookParm->Value[0]);
		free(HookParm);
	}

	if (PlugList[PlugNR].eventfunc == NULL)
	{
		LOG(llevBug, "BUG: Event plugin error %s\n", dlerror());
	}
	PlugNR++;
	PlugList[PlugNR - 1].pinitfunc(NULL);
	LOG(llevInfo, "[Done]\n");
}
#endif

/*****************************************************************************/
/* Hook functions. Those are wrappers to crosslib functions, used by plugins.*/
/* Remember : NEVER call crosslib functions directly from a plugin if a hook */
/* exists.                                                                   */
/*****************************************************************************/

/*****************************************************************************/
/* LOG wrapper                                                               */
/*****************************************************************************/
/* 0 - Level of logging;                                                     */
/* 1 - Message string                                                        */
/*****************************************************************************/
CFParm* CFWLog(CFParm* PParm)
{
	LOG(*(int *)(PParm->Value[0]), "%s", (char *)(PParm->Value[1]));
	return NULL;
}

/*****************************************************************************/
/* fix_player wrapper                                                        */
/*****************************************************************************/
/* 0 - (player) object                                                       */
/*****************************************************************************/
CFParm* CFWFixPlayer(CFParm* PParm)
{
	fix_player((object *)(PParm->Value[0]));
	return NULL;
}

/*****************************************************************************/
/* new_info_map wrapper.                                                     */
/*****************************************************************************/
/* 0 - Color information;                                                    */
/* 1 - Map where the message should be heard;                                */
/* 2 - x position                                                            */
/* 3 - y position                                                            */
/* 4 - distance message can be "heared"                                      */
/* 5 - Message.                                                              */
/*****************************************************************************/
CFParm* CFWNewInfoMap(CFParm* PParm)
{
	new_info_map(*(int *)(PParm->Value[0]), (struct mapdef *)(PParm->Value[1]), *(int *)(PParm->Value[2]), *(int *)(PParm->Value[3]), *(int *)(PParm->Value[4]), (char*)(PParm->Value[5]));
	return NULL;
}

/*****************************************************************************/
/* new_info_map wrapper.                                                     */
/*****************************************************************************/
/* 0 - Color information;                                                    */
/* 1 - Map where the message should be heard;                                */
/* 2 - x position                                                            */
/* 3 - y position                                                            */
/* 4 - distance message can be "heared"                                      */
/* 5 - object 1 which should not hear this                                   */
/* 6 - object 2 which should not hear this (van be NULL or same as other)    */
/* 7 - Message.                                                              */
/*****************************************************************************/
CFParm* CFWNewInfoMapExcept(CFParm* PParm)
{
	new_info_map_except(*(int *)(PParm->Value[0]), (struct mapdef *)(PParm->Value[1]), *(int *)(PParm->Value[2]), *(int *)(PParm->Value[3]), *(int *)(PParm->Value[4]), (object *)(PParm->Value[5]), (object *)(PParm->Value[6]), (char*)(PParm->Value[7]));
	return NULL;
}

/*****************************************************************************/
/* spring_trap wrapper.                                                      */
/*****************************************************************************/
/* 0 - Trap;                                                                 */
/* 1 - Victim.                                                               */
/*****************************************************************************/
CFParm* CFWSpringTrap(CFParm* PParm)
{
	spring_trap((object *)(PParm->Value[0]), (object *)(PParm->Value[1]));
	return NULL;
}

/*
 * type of firing express how the dir parameter was parsed
 *  if type is FIRE_DIRECTIONAL, it is a value from 0 to 8 corresponding to a direction
 *  if type is FIRE_POSITIONAL, the 16bits dir parameters is separated into 2 signed shorts
 *       8 lower bits : signed, relative x value from caster
 *       8 higher bits: signed, relative y value from caster
 *   use the following macros defined in <define.h> with FIRE_POSITIONAL:
 *       GET_X_FROM_DIR(dir)  extract the x value
 *       GET_Y_FROM_DIR(dir)  extract they value
 *       SET_DIR_FROM_XY(X,Y) get dir from x,y values
 */
/*****************************************************************************/
/* cast_spell wrapper.                                                       */
/*****************************************************************************/
/* 0 - op;                                                                   */
/* 1 - caster;                                                               */
/* 2 - direction;                                                            */
/* 3 - type of casting;                                                      */
/* 4 - is it an ability or a wizard spell ?                                  */
/* 5 - spelltype;                                                            */
/* 6 - optional args;                                                        */
/* 7 - type of firing;                                                       */
/*                                                                           */
/*****************************************************************************/
CFParm* CFWCastSpell(CFParm* PParm)
{
	static int val;
	CFParm *CFP;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	/*int cast_spell(object *op, object *caster, int dir, int type, int ability, */
	/*SpellTypeFrom item, char *stringarg); */
	val = cast_spell((object *)(PParm->Value[0]), (object *)(PParm->Value[1]), *(int *)(PParm->Value[2]), *(int *)(PParm->Value[3]), *(int *)(PParm->Value[4]), *(SpellTypeFrom *)(PParm->Value[5]), (char *)(PParm->Value[6])/*, *(int *) (PParm->Value[7])*/);
	CFP->Value[0] = (void *)(&val);
	return CFP;
}

/*****************************************************************************/
/* command_rskill wrapper.                                                   */
/*****************************************************************************/
/* 0 - player;                                                               */
/* 1 - parameters.                                                           */
/*****************************************************************************/
CFParm* CFWCmdRSkill(CFParm* PParm)
{
	static int val;
	CFParm *CFP;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	val = command_rskill((object *)(PParm->Value[0]), (char *)(PParm->Value[1]));
	CFP->Value[0] = (void *)(&val);
	return CFP;
}

/*****************************************************************************/
/* become_follower wrapper.                                                  */
/*****************************************************************************/
/* 0 - object to change;                                                     */
/* 1 - new god object.                                                       */
/*****************************************************************************/
CFParm* CFWBecomeFollower(CFParm* PParm)
{
	become_follower((object *)(PParm->Value[0]), (object *)(PParm->Value[1]));
	return NULL;
}

/*****************************************************************************/
/* pick_up wrapper.                                                          */
/*****************************************************************************/
/* 0 - picker object;                                                        */
/* 1 - picked object.                                                        */
/*****************************************************************************/
CFParm* CFWPickup(CFParm* PParm)
{
	pick_up((object *)(PParm->Value[0]), (object *)(PParm->Value[1]));
	return NULL;
}

/*****************************************************************************/
/* pick_up wrapper.                                                          */
/*****************************************************************************/
/* 0 - picker object;                                                        */
/* 1 - picked object.                                                        */
/*****************************************************************************/
CFParm* CFWGetMapObject(CFParm* PParm)
{
	object *val = NULL;
	static CFParm CFP;

	mapstruct *mt = (mapstruct *)PParm->Value[0];
	int x = *(int *)PParm->Value[1];
	int y = *(int *)PParm->Value[2];

	/* CFP = (CFParm*)(malloc(sizeof(CFParm))); */

	/* Gecko: added tiled map check */
	if ((mt = out_of_map(mt, &x, &y)))
		val = get_map_ob(mt, x, y);

	CFP.Value[0] = (void *)(val);
	return &CFP;
}

/*****************************************************************************/
/* out_of_map wrapper .                                                      */
/*****************************************************************************/
/* 0 - start map                                                             */
/* 1 - x                                                                     */
/* 2 - y                                                                     */
/*****************************************************************************/
CFParm* CFWOutOfMap(CFParm* PParm)
{
	static CFParm CFP;

	mapstruct *mt = (mapstruct *)PParm->Value[0];
	int *x = (int *)PParm->Value[1];
	int *y = (int *)PParm->Value[2];

	CFP.Value[0] = (void *)out_of_map(mt, x, y);

	return &CFP;
}

/*****************************************************************************/
/* esrv_send_item wrapper.                                                   */
/*****************************************************************************/
/* 0 - Player object;                                                        */
/* 1 - Object to update.                                                     */
/*****************************************************************************/
CFParm* CFWESRVSendItem(CFParm* PParm)
{
	esrv_send_item((object *)(PParm->Value[0]), (object *)(PParm->Value[1]));
	return(PParm);
}

/*****************************************************************************/
/* find_player wrapper.                                                      */
/*****************************************************************************/
/* 0 - name of the player to find.                                           */
/*****************************************************************************/
CFParm* CFWFindPlayer(CFParm* PParm)
{
	player *pl;
	CFParm *CFP;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	pl = find_player((char *)(PParm->Value[0]));
	CFP->Value[0] = (void *)(pl);
	return CFP;
}

/*****************************************************************************/
/* manual_apply wrapper.                                                     */
/*****************************************************************************/
/* 0 - object applying;                                                      */
/* 1 - object to apply;                                                      */
/* 2 - apply flags.                                                          */
/*****************************************************************************/
CFParm* CFWManualApply(CFParm* PParm)
{
	CFParm *CFP;
	static int val;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	val = manual_apply((object *)(PParm->Value[0]), (object *)(PParm->Value[1]), *(int *)(PParm->Value[2]));
	CFP->Value[0] = &val;
	return CFP;
}

/*****************************************************************************/
/* command_drop wrapper.                                                     */
/*****************************************************************************/
/* 0 - player;                                                               */
/* 1 - parameters string.                                                    */
/*****************************************************************************/
CFParm* CFWCmdDrop(CFParm* PParm)
{
	CFParm *CFP;
	static int val;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	val = command_drop((object *)(PParm->Value[0]), (char *)(PParm->Value[1]));
	CFP->Value[0] = &val;
	return CFP;
}

/*****************************************************************************/
/* command_take wrapper.                                                     */
/*****************************************************************************/
/* 0 - player;                                                               */
/* 1 - parameters string.                                                    */
/*****************************************************************************/
CFParm* CFWCmdTake(CFParm* PParm)
{
	CFParm *CFP;
	static int val;

	(void) PParm;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	/*val = command_take((object *)(PParm->Value[0]),(char *)(PParm->Value[1]));*/
	CFP->Value[0] = &val;
	return CFP;
}

/*****************************************************************************/
/* transfer_ob wrapper.                                                      */
/*****************************************************************************/
/* 0 - object to transfer;                                                   */
/* 1 - x position;                                                           */
/* 2 - y position;                                                           */
/* 3 - random param;                                                         */
/* 4 - originator object;                                                    */
/*****************************************************************************/
CFParm* CFWTransferObject(CFParm* PParm)
{
	CFParm *CFP;
	static int val;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	val = transfer_ob((object *)(PParm->Value[0]), *(int *)(PParm->Value[1]), *(int *)(PParm->Value[2]), *(int *)(PParm->Value[3]), (object *)(PParm->Value[4]), (object *)(PParm->Value[5]));
	CFP->Value[0] = &val;
	return CFP;
}

/*****************************************************************************/
/* command_title wrapper.                                                    */
/*****************************************************************************/
/* 0 - object;                                                               */
/* 1 - params string.                                                        */
/*****************************************************************************/
CFParm* CFWCmdTitle(CFParm* PParm)
{
	CFParm *CFP;
	static int val;

	(void) PParm;

	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	CFP->Value[0] = &val;
	return CFP;
}

/*****************************************************************************/
/* kill_object wrapper.                                                      */
/*****************************************************************************/
/* 0 - killed object;                                                        */
/* 1 - damage done;                                                          */
/* 2 - killer object;                                                        */
/* 3 - type of killing.                                                      */
/*****************************************************************************/
CFParm* CFWKillObject(CFParm* PParm)
{
	CFParm *CFP;
	static int val;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	val = kill_object((object *)(PParm->Value[0]), *(int *)(PParm->Value[1]), (object *)(PParm->Value[2]), *(int *)(PParm->Value[3]));
	CFP->Value[0] = &val;
	return CFP;
}

/*****************************************************************************/
/* check_spell_known wrapper.                                                */
/*****************************************************************************/
/* 0 - object to check;                                                      */
/* 1 - spell index to search.                                                */
/* return: 0: op has not this skill; 1: op has this skill                    */
/*****************************************************************************/
CFParm* CFWCheckSpellKnown(CFParm* PParm)
{
	CFParm *CFP;
	static int val;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	val = check_spell_known((object *)(PParm->Value[0]), *(int *)(PParm->Value[1]));
	CFP->Value[0] = &val;
	return CFP;
}

/*****************************************************************************/
/* check_spell_known wrapper.                                                */
/*****************************************************************************/
/* 0 - object to check;                                                      */
/* 1 - spell index to search.                                                */
/*****************************************************************************/
CFParm* CFWGetSpellNr(CFParm* PParm)
{
	static CFParm CFP;
	static int val;

	val = look_up_spell_name((char *)PParm->Value[0]);

	CFP.Value[0] = &val;
	return &CFP;
}

/*****************************************************************************/
/* do_learn_spell wrapper.                                                   */
/*****************************************************************************/
/* 0 - object to affect;                                                     */
/* 1 - spell index to learn;                                                 */
/* 2 - mode 0:learn , 1:unlearn												 */
/*****************************************************************************/
CFParm* CFWDoLearnSpell(CFParm* PParm)
{

	/* if mode = 1, unlearn - if mode =0 learn */
	if (*(int *)(PParm->Value[2]))
	{
		do_forget_spell((object *)(PParm->Value[0]), *(int *)(PParm->Value[1]));
	}
	else
	{
		do_learn_spell((object *)(PParm->Value[0]), *(int *)(PParm->Value[1]), 0);
		/* The 0 parameter is marker for special_prayer - godgiven spells,
		 * which will be deleted when player changes god. */
	}
	return NULL;
}


/*****************************************************************************/
/* check_skill_known wrapper.                                                */
/*****************************************************************************/
/* 0 - object to check;                                                      */
/* 1 - skill index to search.                                                */
/*****************************************************************************/
CFParm* CFWCheckSkillKnown(CFParm* PParm)
{
	CFParm *CFP;
	static int val;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	val = check_skill_known((object *)(PParm->Value[0]), *(int *)(PParm->Value[1]));
	CFP->Value[0] = &val;
	return CFP;
}

/*****************************************************************************/
/* check_skill_known wrapper.                                                */
/*****************************************************************************/
/* 0 - object to check;                                                      */
/* 1 - skill index to search.                                                */
/*****************************************************************************/
CFParm* CFWGetSkillNr(CFParm* PParm)
{
	static CFParm CFP;
	static int val;

	val = lookup_skill_by_name((char *)PParm->Value[0]);

	CFP.Value[0] = &val;
	return &CFP;
}


/*****************************************************************************/
/* do_learn_skill wrapper.                                                   */
/*****************************************************************************/
/* 0 - object to affect;                                                     */
/* 1 - skill index to learn;                                                 */
/* 3 - mode 0=leanr, 1=unlearn                                               */
/*****************************************************************************/
CFParm* CFWDoLearnSkill(CFParm* PParm)
{
	if (*(int *)(PParm->Value[2]))
	{
	}
	else
	{
		learn_skill((object *)(PParm->Value[0]), NULL, NULL, *(int *)(PParm->Value[1]), 0);
	}
	return NULL;
}

/*****************************************************************************/
/* esrv_send_inventory wrapper.                                              */
/*****************************************************************************/
/* 0 - player object.                                                        */
/* 1 - updated object.                                                       */
/*****************************************************************************/
CFParm* CFWESRVSendInventory(CFParm* PParm)
{
	esrv_send_inventory((object *)(PParm->Value[0]), (object *)(PParm->Value[1]));
	return NULL;
}

/*****************************************************************************/
/* create_artifact wrapper.                                                  */
/*****************************************************************************/
/* 0 - op;                                                                   */
/* 1 - name of the artifact to create.                                       */
/*****************************************************************************/
CFParm* CFWCreateArtifact(CFParm* PParm)
{
	CFParm *CFP;
	object* val;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	val = create_artifact((object *)(PParm->Value[0]), (char *)(PParm->Value[1]));
	CFP->Value[0] = (void *)(val);
	return CFP;
}

/*****************************************************************************/
/* get_archetype wrapper.                                                    */
/*****************************************************************************/
/* 0 - Name of the archetype to search for.                                  */
/*****************************************************************************/
CFParm* CFWGetArchetype(CFParm* PParm)
{
	/*object* get_archetype(char* name); */
	CFParm *CFP;
	object* val;

	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	val = get_archetype((char *)(PParm->Value[0]));

	CFP->Value[0] = (void *)(val);
	return CFP;
}

/*****************************************************************************/
/* update_ob_speed wrapper.                                                  */
/*****************************************************************************/
/* 0 - object to update.                                                     */
/*****************************************************************************/
CFParm* CFWUpdateSpeed(CFParm* PParm)
{
	update_ob_speed((object *)(PParm->Value[0]));
	return NULL;
}

/*****************************************************************************/
/* update_object wrapper.                                                    */
/*****************************************************************************/
/* 0 - object to update.                                                     */
/*****************************************************************************/
CFParm* CFWUpdateObject(CFParm* PParm)
{
	update_object((object *)(PParm->Value[0]), *(int *)(PParm->Value[1]));
	return NULL;
}

/*****************************************************************************/
/* find_animation wrapper.                                                   */
/*****************************************************************************/
/* 0 - name of the animation to find.                                        */
/*****************************************************************************/
CFParm* CFWFindAnimation(CFParm* PParm)
{
	CFParm *CFP;
	static int val;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	LOG(llevInfo, "CFWFindAnimation: %s\n",(char *)(PParm->Value[0]));
	val = find_animation((char *)(PParm->Value[0]));
	LOG(llevInfo, "Returned val: %i\n",val);
	CFP->Value[0] = (void *)(&val);
	return CFP;
}

/*****************************************************************************/
/* get_archetype_by_object_name wrapper                                      */
/*****************************************************************************/
/* 0 - name to search for.                                                   */
/*****************************************************************************/
CFParm* CFWGetArchetypeByObjectName(CFParm* PParm)
{
	CFParm *CFP;
	object* val;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	val = get_archetype_by_object_name((char *)(PParm->Value[0]));
	CFP->Value[0] = (void *)(val);
	return CFP;
}

/*****************************************************************************/
/* insert_ob_in_ob wrapper.                                                 */
/*****************************************************************************/
/* 0 - object to insert;                                                     */
/* 1 - target object;                                                        */
/*****************************************************************************/
CFParm* CFWInsertObjectInObject(CFParm* PParm)
{
	static CFParm CFP; /* do it static */

	CFP.Value[0] = (void *) insert_ob_in_ob((object *)(PParm->Value[0]), (object *)(PParm->Value[1]));
	return &CFP;
}

/*****************************************************************************/
/* insert_ob_in_map wrapper.                                                 */
/*****************************************************************************/
/* 0 - object to insert;                                                     */
/* 1 - map;                                                                  */
/* 2 - originator of the insertion;                                          */
/* 3 - integer flags.                                                        */
/*****************************************************************************/
CFParm* CFWInsertObjectInMap(CFParm* PParm)
{
	CFParm *CFP;
	object* val;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	val = insert_ob_in_map((object *)(PParm->Value[0]), (mapstruct *)(PParm->Value[1]), (object *)(PParm->Value[2]), *(int *)(PParm->Value[3]));
	CFP->Value[0] = (void *)(val);
	return CFP;
}


/*****************************************************************************/
/* ready_map_name wrapper.                                                   */
/*****************************************************************************/
/* 0 - name of the map to ready;                                             */
/* 1 - integer flags.                                                        */
/*****************************************************************************/
CFParm* CFWReadyMapName(CFParm* PParm)
{
	CFParm* CFP;
	mapstruct* val;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	val = ready_map_name((char *)(PParm->Value[0]), *(int *)(PParm->Value[1]));
	CFP->Value[0] = (void *)(val);
	return CFP;
}

/*****************************************************************************/
/* swap_apartments wrapper.                                                  */
/*****************************************************************************/
/* 0 - old map path;                                                         */
/* 1 - new map path;                                                         */
/* 2 - new x;                                                                */
/* 3 - new y;                                                                */
/* 4 - activator object.                                                     */
/*****************************************************************************/
CFParm* CFWSwapApartments(CFParm *PParm)
{
	CFParm* CFP;
	char *oldmappath = (char *)PParm->Value[0], *newmappath = (char *)PParm->Value[1], filename[MAX_BUF], filename2[MAX_BUF], buf[MAX_BUF];
	int x = *(int *)PParm->Value[2], y = *(int *)PParm->Value[3], i, j;
	object *activator = (object *)PParm->Value[4], *op, *tmp, *tmp2, *dummy;
	sqlite3 *db;
	sqlite3_stmt *statement;
	FILE *fp, *fp2;
	mapstruct *oldmap, *newmap;
	int val = 1;

	CFP = (CFParm*)(malloc(sizeof(CFParm)));

	/* Open the database */
	db_open(DB_DEFAULT, &db);

	/* Prepare the SQL to select unique map */
	if (!db_prepare_format(db, &statement, "SELECT data FROM unique_maps WHERE mapPath = '%s%s';", activator->name, clean_path(oldmappath)))
	{
		LOG(llevBug, "BUG: CFWSwapApartments(): SQL failed to prepare for selecting unique map! (%s)\n", db_errmsg(db));
		db_close(db);
		val = 0;
		CFP->Value[0] = (void*) &val;
		return CFP;
	}

	/* Run the SQL */
	if (db_step(statement) != SQLITE_ROW)
	{
		LOG(llevBug, "BUG: CFWSwapApartments(): Called to swap apartments, but no previous apartment for %s!", activator->name);
		val = 0;
		CFP->Value[0] = (void*) &val;
		return CFP;
	}

	/* Path to the temporary map */
	sprintf(filename, "%s/%s%s", settings.tmpdir, activator->name, clean_path(oldmappath));
	if ((fp = fopen(filename, "w")) == NULL)
	{
		LOG(llevBug, "BUG: CFWSwapApartments(): Failed to open temporary map file!\n");
		db_close(db);
		val = 0;
		CFP->Value[0] = (void*) &val;
		return CFP;
	}
	fputs((char *)db_column_text(statement, 0), fp);
	fclose(fp);

	/* So we can transfer our items from the old apartment. */
	oldmap = ready_map_name(filename, 2);

	/* First, make a copy of the apartment map we're upgrading to. */
	sprintf(filename2, "%s/%s%s", settings.tmpdir, activator->name, clean_path(newmappath));
	if ((fp = fopen(create_pathname(newmappath), "r")) == NULL)
	{
		LOG(llevBug, "BUG: CFWSwapApartments(): Failed to open original apartment map!\n");
		val = 0;
		CFP->Value[0] = (void*) &val;
		return CFP;
	}

	if ((fp2 = fopen(filename2, "w")) == NULL)
	{
		LOG(llevBug, "BUG: CFWSwapApartments(): Failed to open new temporary apartment map!\n");
		val = 0;
		CFP->Value[0] = (void*) &val;
		return CFP;
	}

	while (fgets(buf, MAX_BUF, fp) != NULL)
		fprintf(fp2, "%s", buf);

	fclose(fp2);
	fclose(fp);

	/* Our new map. */
	newmap = ready_map_name(filename2, 2);

	/* Go through every square on old apartment map, looking for things to transfer. */
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
					continue;

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
							continue;

						remove_ob(tmp);
						tmp->x = x;
						tmp->y = y;
						insert_ob_in_map(tmp, newmap, NULL, INS_NO_MERGE | INS_NO_WALK_ON);
					}
				}
			}
		}
	}

	/* Finalize it */
	db_finalize(statement);

	/* Prepare the query to delete old map from database */
	if (!db_prepare_format(db, &statement, "DELETE FROM unique_maps WHERE mapPath = '%s%s';", activator->name, clean_path(oldmappath)))
	{
		LOG(llevBug, "BUG: CFWSwapApartments(): Failed to prepare SQL query to remove old apartment! (%s)\n", db_errmsg(db));
		val = 0;
		CFP->Value[0] = (void*) &val;
		return CFP;
	}

	/* Run the query */
	db_step(statement);

	/* Finalize it */
	db_finalize(statement);

	/* Close the database */
	db_close(db);

	/* Mark this new map as unique map */
	newmap->map_flags |= MAP_FLAG_UNIQUE;

	/* Save the map */
	new_save_map(newmap, 0);

	/* Check for old save bed */
	if (strcmp(oldmap->path, CONTR(activator)->savebed_map) == 0)
		strcpy(CONTR(activator)->savebed_map, "");

	/* Free the maps */
	free_map(newmap, 1);
	free_map(oldmap, 1);

	/* Remove the temporary map files */
	unlink(newmap->path);
	unlink(oldmap->path);

	/* Success! */
	CFP->Value[0] = (void*) &val;
	return CFP;
}

/*****************************************************************************/
/* player_exists wrapper.                                                    */
/*****************************************************************************/
/* 0 - character name to find.                                               */
/*****************************************************************************/
CFParm* CFWPlayerExists(CFParm* PParm)
{
	sqlite3 *db;
	sqlite3_stmt *statement;
	int val = 0;
	char *playerName = (char *)PParm->Value[0];
	CFParm* CFP;

	CFP = (CFParm*)(malloc(sizeof(CFParm)));

	db_open(DB_DEFAULT, &db);

	if (!db_prepare_format(db, &statement, "SELECT playerName FROM players WHERE playerName = '%s' LIMIT 1;", playerName))
	{
		LOG(llevBug, "BUG: CFWPlayerExists(): Failed to prepare SQL query to check if player exists! (%s)\n", db_errmsg(db));
		db_close(db);
		CFP->Value[0] = (void *) &val;
		return CFP;
	}

	if (db_step(statement) == SQLITE_ROW)
		val = 1;

	db_finalize(statement);

	db_close(db);

	CFP->Value[0] = (void *) &val;
	return CFP;
}


/*****************************************************************************/
/* add_exp wrapper.                                                          */
/*****************************************************************************/
/* 0 - object to increase experience of.                                     */
/* 1 - amount of experience to add.                                          */
/* 2 - Skill number to add xp in                                             */
/*****************************************************************************/
CFParm* CFWAddExp(CFParm* PParm)
{
	add_exp((object *)(PParm->Value[0]), *(int *)(PParm->Value[1]), *(int *)(PParm->Value[2]));
	return(PParm);
}

/*****************************************************************************/
/* determine_god wrapper.                                                    */
/*****************************************************************************/
/* 0 - object to determine the god of.                                       */
/*****************************************************************************/
CFParm* CFWDetermineGod(CFParm* PParm)
{
	CFParm* CFP;
	const char* val;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	val = determine_god((object *)(PParm->Value[0]));
	CFP->Value[0] = (void *)(val);
	return CFP;
}

/*****************************************************************************/
/* find_god wrapper.                                                         */
/*****************************************************************************/
/* 0 - Name of the god to search for.                                        */
/*****************************************************************************/
CFParm* CFWFindGod(CFParm* PParm)
{
	CFParm* CFP;
	object* val;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	val = find_god((char *)(PParm->Value[0]));
	CFP->Value[0] = (void *)(val);
	return CFP;
}

/*****************************************************************************/
/* dump_me wrapper.                                                          */
/*****************************************************************************/
/* 0 - object to dump;                                                       */
/*****************************************************************************/
CFParm* CFWDumpObject(CFParm* PParm)
{
	CFParm* CFP;
	char*   val;
	/* object* ob; not used */
	val = (char *)(malloc(sizeof(char) * 10240));
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	dump_me((object *)(PParm->Value[0]), val);
	CFP->Value[0] = (void *)(val);
	return CFP;
}

/*****************************************************************************/
/* load_object_str wrapper.                                                  */
/*****************************************************************************/
/* 0 - object dump string to load.                                           */
/*****************************************************************************/
CFParm* CFWLoadObject(CFParm* PParm)
{
	CFParm* CFP;
	object* val;

	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	val = load_object_str((char *)(PParm->Value[0]));
	LOG(llevDebug, "CFWLoadObject: %s\n", query_name(val, NULL));
	CFP->Value[0] = (void *)(val);
	return CFP;
}

/*****************************************************************************/
/* remove_ob wrapper. TODO drop inv support                                  */
/*****************************************************************************/
/* 0 - object to remove.                                                     */
/*****************************************************************************/
CFParm* CFWRemoveObject(CFParm* PParm)
{
	remove_ob((object *)(PParm->Value[0]));
	return NULL;
}

CFParm* CFWAddString(CFParm* PParm)
{
	static CFParm CFP;
	char *val;
	/*CFP = (CFParm*)(malloc(sizeof(CFParm)));*/
	val = (char *)(PParm->Value[0]);
	CFP.Value[0]=NULL;
	FREE_AND_COPY_HASH(CFP.Value[0], val);
	return &CFP;
}

CFParm* CFWAddRefcount(CFParm* PParm)
{
	static CFParm CFP;
	char *val;
	val = (char *)(PParm->Value[0]);
	CFP.Value[0] = NULL;
	FREE_AND_ADD_REF_HASH(CFP.Value[0], val);
	return &CFP;
}

CFParm* CFWFreeString(CFParm* PParm)
{
	char* val;
	val = (char *)(PParm->Value[0]);
	FREE_AND_CLEAR_HASH(val);
	return NULL;
}

CFParm* CFWGetFirstMap(CFParm* PParm)
{
	CFParm* CFP;

	(void) PParm;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	CFP->Value[0] = (void*)(first_map) ;
	return CFP;
}

CFParm* CFWGetFirstPlayer(CFParm* PParm)
{
	CFParm* CFP;

	(void) PParm;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	CFP->Value[0] = (void*)(first_player) ;
	return CFP;
}

CFParm* CFWGetFirstArchetype(CFParm* PParm)
{
	CFParm* CFP;

	(void) PParm;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	CFP->Value[0] = (void*)(first_archetype) ;
	return CFP;
}

CFParm* CFWDeposit(CFParm* PParm)
{
	static CFParm CFP;
	static int val;
	int pos = 0;
	char *text = (char *)(PParm->Value[2]);
	object *who = (object *)(PParm->Value[0]), *bank = (object *)(PParm->Value[1]);
	_money_block money;

	val = 0;
	get_word_from_string(text, &pos);
	get_money_from_string(text + pos , &money);

	CFP.Value[0] = (void*) &val;

	if (!money.mode)
	{
		val=-1;
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
			remove_money_type(who, who, coins_arch[0]->clone.value, money.mithril);
		if (money.gold)
			remove_money_type(who, who, coins_arch[1]->clone.value, money.gold);
		if (money.silver)
			remove_money_type(who, who, coins_arch[2]->clone.value, money.silver);
		if (money.copper)
			remove_money_type(who, who, coins_arch[3]->clone.value, money.copper);

		bank->value += money.mithril * coins_arch[0]->clone.value + money.gold * coins_arch[1]->clone.value + money.silver * coins_arch[2]->clone.value + money.copper * coins_arch[3]->clone.value;
		fix_player(who);
	}

	return &CFP;
}

CFParm* CFWWithdraw(CFParm* PParm)
{
	static CFParm CFP;
	static int val;
	int pos = 0;
	/* TODO: value should be int64 later! */
	double big_value;
	char *text = (char *)(PParm->Value[2]);
	object *who = (object *)(PParm->Value[0]), *bank = (object *)(PParm->Value[1]);
	_money_block money;

	val = 0;
	get_word_from_string(text, &pos);
	get_money_from_string(text + pos , &money);
	CFP.Value[0] = (void*) &val;

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
		/* just to set a border.... */
		if (money.mithril > 100000 || money.gold > 100000 || money.silver > 1000000 || money.copper > 1000000)
		{
			new_draw_info(NDI_UNIQUE, 0, who, "Withdraw values are too high.");
			return &CFP;
		}
		big_value = (double)money.mithril * (double)coins_arch[0]->clone.value + (double)money.gold * (double)coins_arch[1]->clone.value + (double)money.silver * (double)coins_arch[2]->clone.value + (double)money.copper * (double)coins_arch[3]->clone.value;

		if (big_value > (double) bank->value)
		{
			val = 0;
			return &CFP;
		}

		if (money.mithril)
			insert_money_in_player(who, &coins_arch[0]->clone, money.mithril);
		if (money.gold)
			insert_money_in_player(who, &coins_arch[1]->clone, money.gold);
		if (money.silver)
			insert_money_in_player(who, &coins_arch[2]->clone, money.silver);
		if (money.copper)
			insert_money_in_player(who, &coins_arch[3]->clone, money.copper);

		bank->value -= (sint32) big_value;
		fix_player(who);
	}

	return &CFP;
}


/*****************************************************************************/
/* show_cost wrapper.                                                       */
/*****************************************************************************/
/* 0 - value as integer                                                      */
/* returns static string with "x gold, x silver ..."                         */
/* so CFP is static here too                                                 */
/*****************************************************************************/
CFParm* CFWShowCost(CFParm* PParm)
{
	static CFParm CFP;

	CFP.Value[0] = cost_string_from_value(*(int*)(PParm->Value[0]));
	return &CFP;
}

/*****************************************************************************/
/* query_cost wrapper.                                                       */
/*****************************************************************************/
/* 0 - object to evaluate.                                                   */
/* 1 - who tries to sell of buy it                                           */
/* 2 - F_SELL F_BUY or F_TRUE                                                */
/*****************************************************************************/
CFParm* CFWQueryCost(CFParm* PParm)
{
	CFParm* CFP;
	object* whatptr;
	object* whoptr;
	int flag;
	static double val;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	whatptr = (object *)(PParm->Value[0]);
	whoptr = (object *)(PParm->Value[1]);
	flag = *(int*)(PParm->Value[2]);
	val = query_cost(whatptr,whoptr,flag);
	CFP->Value[0] = (void*) &val;
	return CFP;
}

/*****************************************************************************/
/* query_money wrapper.                                                      */
/*****************************************************************************/
/* 0 - object we are looking for solvability at.                             */
/*****************************************************************************/
CFParm* CFWQueryMoney(CFParm* PParm)
{
	CFParm* CFP;
	object* whoptr;
	static int val;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	whoptr = (object *)(PParm->Value[0]);
	val=query_money (whoptr);
	CFP->Value[0] = (void*) &val;
	return CFP;
}

/*****************************************************************************/
/* pay_for_item wrapper.                                                     */
/*****************************************************************************/
/* 0 - object to pay.                                                        */
/* 1 - who tries to buy it                                                   */
/*****************************************************************************/
CFParm* CFWPayForItem(CFParm* PParm)
{
	CFParm* CFP;
	object* whatptr;
	object* whoptr;
	static int val;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	whatptr = (object *)(PParm->Value[0]);
	whoptr = (object *)(PParm->Value[1]);
	val= pay_for_item (whatptr,whoptr);
	CFP->Value[0] = (void*) &val;
	return CFP;
}

/*****************************************************************************/
/* pay_for_amount wrapper.                                                   */
/*****************************************************************************/
/* 0 - amount to pay.                                                        */
/* 1 - who tries to pay it                                                   */
/*****************************************************************************/
CFParm* CFWPayForAmount(CFParm* PParm)
{
	CFParm* CFP;
	int amount;
	object* whoptr;
	static int val;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	amount = *(int *)(PParm->Value[0]);
	whoptr = (object *)(PParm->Value[1]);
	val= pay_for_amount (amount,whoptr);
	CFP->Value[0] = (void*) &val;
	return CFP;
}

/*new_draw_info(int flags, int pri, object *pl, const char *buf); */
CFParm* CFWNewDrawInfo(CFParm* PParm)
{
	new_draw_info(*(int *)(PParm->Value[0]), *(int *)(PParm->Value[1]), (object *)(PParm->Value[2]), (char *)(PParm->Value[3]));
	return NULL;
}
/*****************************************************************************/
/* move_player wrapper.                                                      */
/*****************************************************************************/
/* 0 - player to move                                                        */
/* 1 - direction of move                                                     */
/*****************************************************************************/
CFParm* CFWMovePlayer (CFParm* PParm)
{
	CFParm* CFP;
	static int val;
	val = move_player((object*)PParm->Value[0], *(int*)PParm->Value[1]);
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	CFP->Value[0] = (void*) &val;
	return(CFP);
}
/*****************************************************************************/
/* move_object wrapper.                                                      */
/*****************************************************************************/
/* 0 - object to move                                                        */
/* 1 - direction of move                                                     */
/* 2 - originator                                                            */
/*****************************************************************************/
CFParm* CFWMoveObject (CFParm* PParm)
{
	CFParm* CFP;
	static int val;
	val = move_ob ((object*)PParm->Value[0], *(int*)PParm->Value[1], (object*)PParm->Value[2]);
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	CFP->Value[0] = (void*) &val;

	return(CFP);
}

CFParm* CFWSendCustomCommand(CFParm* PParm)
{
	send_plugin_custom_message((object *)(PParm->Value[0]), (char *)(PParm->Value[1]));
	return NULL;
}

/* not used atm! */
CFParm* CFWCFTimerCreate(CFParm* PParm)
{
	/* int cftimer_create(int id, long delay, object* ob, int mode) */
	CFParm* CFP;
	static int val;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	val = cftimer_create(*(int *)(PParm->Value[0]), *(long *)(PParm->Value[1]), (object *)(PParm->Value[2]), *(int *)(PParm->Value[3]));
	CFP->Value[0] = (void *)(&val);
	return CFP;
}

/* not used atm! */
CFParm* CFWCFTimerDestroy(CFParm* PParm)
{
	/*int cftimer_destroy(int id) */
	CFParm* CFP;
	static int val;
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	val = cftimer_destroy(*(int *)(PParm->Value[0]));
	CFP->Value[0] = (void *)(&val);
	return CFP;
}
/*****************************************************************************/
/* SET_ANIMATION wrapper.                                                    */
/*****************************************************************************/
/* 0 - object                                                                */
/* 1 - face                                                                  */
/*****************************************************************************/
CFParm* CFWSetAnimation (CFParm* PParm)
{
	object* op = (object*)PParm->Value[0];
	int face = *(int*)PParm->Value[1];
	if (face != -1)
	{
		SET_ANIMATION(op, face);
	}
	update_object(op, UP_OBJ_FACE);
	return(PParm);
}

/*****************************************************************************/
/* communicate wrapper.                                                      */
/*****************************************************************************/
/* 0 - object                                                                */
/* 1 - string                                                                */
/*****************************************************************************/
CFParm* CFWCommunicate (CFParm* PParm)
{
	/*char buf[MAX_BUF];*/
	object* op = (object*)PParm->Value[0];
	char* string = (char*)PParm->Value[1];
	if ((!op) || (!string))
		return NULL;

	communicate(op, string);
	return NULL;
}
/*****************************************************************************/
/* find_best_object_match wrapper.                                           */
/*****************************************************************************/
/* 0 - object to find object in inventory                                    */
/* 1 - name                                                                  */
/*****************************************************************************/
CFParm* CFWFindBestObjectMatch (CFParm* PParm)
{
	CFParm* CFP;
	object* op=(object*)PParm->Value[0];
	char* param=(char*)PParm->Value[1];
	object* result;
	result=(object*)find_best_object_match(op, param);
	CFP = (CFParm*)(malloc(sizeof(CFParm)));
	CFP->Value[0] = (void*) result;

	return(CFP);
}
/*****************************************************************************/
/* player_apply_below wrapper.                                               */
/*****************************************************************************/
/* 0 - object player                                                         */
/*****************************************************************************/
CFParm* CFWApplyBelow(CFParm* PParm)
{
	object* op = (object*)PParm->Value[0];

	if (!op)
		return NULL;

	player_apply_below(op);
	return NULL;
}
/*****************************************************************************/
/* free_object wrapper. TODO: get rid of                                     */
/*****************************************************************************/
/* 0 - object                                                                */
/*****************************************************************************/
CFParm* CFWFreeObject(CFParm* PParm)
{
	(void) PParm;

#if 0
	object* op = (object*)PParm->Value[0];
	if (op)
		free_object(op);
#endif
	return NULL;
}

/*****************************************************************************/
/* find_marked_object .                                                      */
/*****************************************************************************/
/* 0 - object                                                                */
/*****************************************************************************/
/* return: object or NULL                                                    */
/*****************************************************************************/
CFParm* CFWFindMarkedObject(CFParm* PParm)
{
	static CFParm CFP;

	object* op = (object*)PParm->Value[0];
	if (op)
		op = find_marked_object(op);

	CFP.Value[0] = (void*) op;

	return(&CFP);
}

/*****************************************************************************/
/* find_marked_object .                                                      */
/*****************************************************************************/
/* 0 - object, 1 - if set, send examine msg to this object                   */
/*****************************************************************************/
/* return: object or NULL                                                    */
/*****************************************************************************/
CFParm* CFWIdentifyObject(CFParm* PParm)
{
	object* caster = (object*)PParm->Value[0];
	object* target = (object*)PParm->Value[1];
	object* op = (object*)PParm->Value[2];


	cast_identify(target, caster->level, op, *(int *)(PParm->Value[3]));

	if (caster)
		play_sound_map(caster->map, caster->x, caster->y, spells[SP_IDENTIFY].sound , SOUND_SPELL);
	else if (target)
		play_sound_map(target->map, target->x, target->y, spells[SP_IDENTIFY].sound , SOUND_SPELL);

	return NULL;
}

/*****************************************************************************/
/* ObjectCreateClone object_copy wrapper.                                    */
/*****************************************************************************/
/* 0 - object                                                                */
/* 1 - type 0 = clone with inventory                                         */
/*          1 = only duplicate the object without it's content and op->more  */
/*****************************************************************************/
CFParm* CFWObjectCreateClone(CFParm* PParm)
{
	CFParm* CFP = (CFParm*)malloc(sizeof (CFParm));
	if (*(int*)PParm->Value[1] == 0)
		CFP->Value[0]=ObjectCreateClone ((object*)PParm->Value[0]);
	else if (*(int*)PParm->Value[1] == 1)
	{
		object* tmp;
		tmp = get_object();
		copy_object((object*)PParm->Value[0], tmp);
		CFP->Value[0] = tmp;
	}
	return CFP;
}

/*****************************************************************************/
/* teleport an object to another map                                         */
/*****************************************************************************/
/* 0 - object                                                                */
/* 1 - mapname we use for destination                                        */
/* 2 - mapx                                                                  */
/* 3 - mapy                                                                  */
/* 4 - unique?                                                               */
/* 5 - msg (used for random maps entering. May be NULL)                      */
/*****************************************************************************/
CFParm* CFWTeleportObject (CFParm* PParm)
{
	object* current;
	/*  char * mapname; not used
	    int mapx;
	    int mapy;
	    int unique; not used */
	current=get_object();
	FREE_AND_COPY_HASH(EXIT_PATH(current), (char*)PParm->Value[1]);
	EXIT_X(current) = *(int*)PParm->Value[2];
	EXIT_Y(current) = *(int*)PParm->Value[3];

	if (*(int*)PParm->Value[4])
		current->last_eat = MAP_PLAYER_MAP;

	if (PParm->Value[5])
		FREE_AND_COPY_HASH(current->msg, (char*)PParm->Value[5]);

	enter_exit((object*) PParm->Value[0],current);

	if (((object*) PParm->Value[0])->map)
		play_sound_map(((object*) PParm->Value[0])->map, ((object*) PParm->Value[0])->x, ((object*) PParm->Value[0])->y, SOUND_TELEPORT, SOUND_NORMAL);

	return NULL;
}

/*****************************************************************************/
/* The following is not really a wrapper like the others are.                */
/* It is in fact used to allow the plugin to request the global events it    */
/* wants to be aware of. All events can be seen as global; on the contrary,  */
/* some events can't be used as local: for example, BORN is only global.     */
/*****************************************************************************/
/* 0 - Number of the event to register;                                      */
/* 1 - String ID of the requesting plugin.                                   */
/*****************************************************************************/
CFParm* RegisterGlobalEvent(CFParm* PParm)
{
	int PNR = findPlugin((char *)(PParm->Value[1]));

#ifdef LOG_VERBOSE
	LOG(llevDebug, "Plugin %s (%i) registered the event %i\n", (char *)(PParm->Value[1]), PNR, *(int *)(PParm->Value[0]));
#endif

	LOG(llevDebug, "Plugin %s (%i) registered the event %i\n", (char *)(PParm->Value[1]), PNR, *(int *)(PParm->Value[0]));
	PlugList[PNR].gevent[*(int *)(PParm->Value[0])] = 1;
	return NULL;
}

/*****************************************************************************/
/* The following unregisters a global event.                                 */
/*****************************************************************************/
/* 0 - Number of the event to unregister;                                    */
/* 1 - String ID of the requesting plugin.                                   */
/*****************************************************************************/
CFParm* UnregisterGlobalEvent(CFParm* PParm)
{
	int PNR = findPlugin((char *)(PParm->Value[1]));
	PlugList[PNR].gevent[*(int *)(PParm->Value[0])] = 0;
	return NULL;
}

/*****************************************************************************/
/* When a specific global event occurs, this function is called.             */
/* Concerns events: BORN, QUIT, LOGIN, LOGOUT, SHOUT for now.                */
/*****************************************************************************/
/*                                                                           */
/*****************************************************************************/
void GlobalEvent(CFParm *PParm)
{
	int i;
	for (i = 0; i < PlugNR; i++)
	{
		if (PlugList[i].gevent[*(int *)(PParm->Value[0])] != 0)
		{
			(PlugList[i].eventfunc)(PParm);
		}
	}
}

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

	CFP.Value[0] = (void *)(&evtid);
	CFP.Value[1] = (void *)(parm1);
	CFP.Value[2] = (void *)(parm2);

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
		int count = 0;
		struct timeval start, stop;
		long long start_u, stop_u;
		gettimeofday(&start, NULL);

		for (count = 0; count < 10000; count++)
		{
			CFR = PlugList[plugin].eventfunc(&CFP);

			returnvalue = *(int *)(CFR->Value[0]);
		}

		gettimeofday(&stop, NULL);
		start_u = start.tv_sec * 1000000 + start.tv_usec;
		stop_u = stop.tv_sec * 1000000 + stop.tv_usec;

		LOG(llevDebug, "Running time: %2.4f s\n", (stop_u - start_u) / 1000000.0);
#else
		CFR = PlugList[plugin].eventfunc(&CFP);

		returnvalue = *(int *)(CFR->Value[0]);
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

/*
 * See sounds.h for sound numbers
 */
/*****************************************************************************/
/* play_sound_map wrapper.                                                   */
/*****************************************************************************/
/* 0 - map;                                                                  */
/* 1 - x;                                                                    */
/* 2 - y;                                                                    */
/* 3 - sound number                                                          */
/* 4 - type of sound (0=normal, 1=spell);                                    */
/*****************************************************************************/
CFParm* CFWPlaySoundMap(CFParm* PParm)
{
	play_sound_map((mapstruct *)(PParm->Value[0]), *(int *)(PParm->Value[1]), *(int *)(PParm->Value[2]), *(int *)(PParm->Value[3]), *(int *)(PParm->Value[4]));
	return NULL;
}

/*****************************************************************************/
/* create_object wrapper.                                                    */
/*****************************************************************************/
/* 0 - archetype                                                             */
/* 1 - map;                                                                  */
/* 2 - x;                                                                    */
/* 3 - y;                                                                    */
/*****************************************************************************/
CFParm* CFWCreateObject(CFParm* PParm)
{
	static CFParm CFP;
	archetype *arch;
	object *newobj;

	CFP.Value[0] = NULL;

	if (!(arch = find_archetype((char *)(PParm->Value[0]))))
		return(&CFP);

	if (!(newobj = arch_to_object(arch)))
		return(&CFP);

	newobj->x = *(int *)(PParm->Value[2]);
	newobj->y = *(int *)(PParm->Value[3]);

	newobj = insert_ob_in_map(newobj, (mapstruct *)(PParm->Value[1]), NULL, 0);

	CFP.Value[0] = newobj;
	return (&CFP);
}
