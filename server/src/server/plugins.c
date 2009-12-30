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
#include <sproto.h>

static void register_global_event(char *plugin_name, int event_nr);
static void unregister_global_event(char *plugin_name, int event_nr);

struct plugin_hooklist hooklist =
{
	query_name,
	re_cmp,
	present_in_ob,
	players_on_map,
	create_pathname,
	normalize_path,
	LOG,
	free_string_shared,
	add_string,
	remove_ob,
	fix_player,
	insert_ob_in_ob,
	new_info_map,
	new_info_map_except,
	spring_trap,
	cast_spell,
	update_ob_speed,
	command_rskill,
	become_follower,
	pick_up,
	get_map_from_coord,
	esrv_send_item,
	find_player,
	manual_apply,
	command_drop,
	transfer_ob,
	kill_object,
	do_learn_spell,
	do_forget_spell,
	look_up_spell_name,
	check_spell_known,
	esrv_send_inventory,
	get_archetype,
	ready_map_name,
	add_exp,
	determine_god,
	find_god,
	register_global_event,
	unregister_global_event,
	load_object_str,
	query_cost,
	query_money,
	pay_for_item,
	pay_for_amount,
	new_draw_info,
	send_plugin_custom_message,
	communicate,
	object_create_clone,
	get_object,
	copy_object,
	enter_exit,
	play_sound_map,
	learn_skill,
	find_marked_object,
	cast_identify,
	lookup_skill_by_name,
	check_skill_known,
	find_archetype,
	arch_to_object,
	insert_ob_in_map,
	cost_string_from_value,
	bank_deposit,
	bank_withdraw,
	swap_apartments,
	player_exists,
	get_tod,
	get_ob_key_value,
	set_ob_key_value,
	drop,
	query_short_name,
	beacon_locate,
	strdup_local,
	adjust_player_name,
	find_party,
	add_party_member,
	remove_party_member,
	send_party_message,
	Write_String_To_Socket,
	dump_object,
	stringbuffer_new,
	stringbuffer_finish,

	season_name,
	weekdays,
	month_name,
	periodsofday,
	spells,
	&shstr_cons
};

/** Array of all loaded plugins */
static CFPlugin PlugList[32];

/** Number of loaded plugins. */
static int PlugNR = 0;

/**
 * Register a global event.
 * @param plugin_name Plugin's name.
 * @param event_nr Event ID to register. */
static void register_global_event(char *plugin_name, int event_nr)
{
	int PNR = findPlugin(plugin_name);

	LOG(llevDebug, "Plugin %s (%d) registered the event %d\n", plugin_name, PNR, event_nr);

	PlugList[PNR].gevent[event_nr] = 1;
}

/**
 * Unregister a global event.
 * @param plugin_name Plugin's name.
 * @param event_nr Event ID to unregister. */
static void unregister_global_event(char *plugin_name, int event_nr)
{
	int PNR = findPlugin(plugin_name);

	PlugList[PNR].gevent[event_nr] = 0;
}

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
CommArray_s *find_plugin_command(const char *cmd)
{
	int i;
	static CommArray_s rtn_cmd;

	for (i = 0; i < PlugNR; i++)
	{
		if (PlugList[i].propfunc(&i, "command?", cmd, &rtn_cmd))
		{
			return &rtn_cmd;
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
	PlugList[PlugNR].initfunc = (f_plug_init) (GetProcAddress(DLLInstance, "initPlugin"));
#else
	PlugList[PlugNR].libptr = ptr;
	PlugList[PlugNR].initfunc = (f_plug_init) (dlsym(ptr, "initPlugin"));
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
		PlugList[PlugNR].initfunc(&hooklist);
	}

#ifdef WIN32
	PlugList[PlugNR].eventfunc = (f_plug_api) (GetProcAddress(DLLInstance, "triggerEvent"));
	PlugList[PlugNR].pinitfunc = (f_plug_pinit) (GetProcAddress(DLLInstance, "postinitPlugin"));
	PlugList[PlugNR].propfunc = (f_plug_api) (GetProcAddress(DLLInstance, "getPluginProperty"));
#else
	PlugList[PlugNR].eventfunc = (f_plug_api) (dlsym(ptr, "triggerEvent"));
	PlugList[PlugNR].pinitfunc = (f_plug_pinit) (dlsym(ptr, "postinitPlugin"));
	PlugList[PlugNR].propfunc = (f_plug_api) (dlsym(ptr, "getPluginProperty"));
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

	if (PlugList[PlugNR].eventfunc == NULL)
	{
#ifdef WIN32
		LOG(llevBug, "BUG: Event plugin error\n");
#else
		LOG(llevBug, "BUG: Event plugin error %s\n", dlerror());
#endif

		return;
	}

	PlugList[PlugNR].propfunc(0, "Identification", PlugList[PlugNR].id, sizeof(PlugList[PlugNR].id));
	PlugList[PlugNR].propfunc(0, "FullName", PlugList[PlugNR].fullname, sizeof(PlugList[PlugNR].fullname));
	LOG(llevInfo, "Plugin name: %s, known as %s\n", PlugList[PlugNR].fullname, PlugList[PlugNR].id);

	PlugNR++;
	PlugList[PlugNR - 1].pinitfunc();

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
 * Handles triggering global events like EVENT_BORN, EVENT_MAPRESET,
 * etc.
 * @param event_type The event type
 * @param parm1 First parameter
 * @param parm2 Second parameter */
void trigger_global_event(int event_type, void *parm1, void *parm2)
{
#ifdef PLUGINS
	int i;

	for (i = 0; i < PlugNR; i++)
	{
		if (PlugList[i].gevent[event_type] != 0)
		{
			(PlugList[i].eventfunc)(0, event_type, parm1, parm2);
		}
	}
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

	if (event_obj->name && (plugin = findPlugin(event_obj->name)) >= 0)
	{
		int returnvalue;
#ifdef TIME_SCRIPTS
		struct timeval start, stop;
		uint64 start_u, stop_u;

		gettimeofday(&start, NULL);
#endif

		returnvalue = *(int *) PlugList[plugin].eventfunc(0, event_type, activator, me, other, msg, parm1, parm2, parm3, flags, event_obj->race, event_obj->slaying);

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
