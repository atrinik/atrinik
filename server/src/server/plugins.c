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
 * Handles the plugins code. */

#include <plugin.h>

/** The plugin suffix. */
#ifndef WIN32
#	define PLUGIN_SUFFIX ".so"
#else
#	define PLUGIN_SUFFIX ".dll"
#endif

static void register_global_event(const char *plugin_name, int event_nr);
static void unregister_global_event(const char *plugin_name, int event_nr);

/** The actual hooklist. */
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
	object_get_value,
	object_set_value,
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
	cleanup_chat_string,
	cftimer_find_free_id,
	cftimer_create,
	cftimer_destroy,
	find_face,
	find_animation,
	play_sound_player_only,
	new_draw_info_format,
	was_destroyed,
	object_get_gender,
	change_abil,
	decrease_ob_nr,
	check_walk_off,
	wall,
	blocked,

	season_name,
	weekdays,
	month_name,
	periodsofday,
	spells,
	&shstr_cons,
	gender_noun,
	gender_subjective,
	gender_subjective_upper,
	gender_objective,
	gender_possessive,
	gender_reflexive,
	object_flag_names
};

/** The list of loaded plugins. */
static atrinik_plugin *plugins_list = NULL;

/**
 * Find a plugin by its identification string.
 * @param id Plugin's identification string.
 * @return Pointer to the found plugin, NULL if not found. */
static atrinik_plugin *find_plugin(const char *id)
{
	atrinik_plugin *plugin;

	if (!plugins_list)
	{
		return NULL;
	}

	for (plugin = plugins_list; plugin; plugin = plugin->next)
	{
		if (!strcmp(id, plugin->id))
		{
			return plugin;
		}
	}

	return NULL;
}

/**
 * Register a global event.
 * @param plugin_name Plugin's name.
 * @param event_nr Event ID to register. */
static void register_global_event(const char *plugin_name, int event_nr)
{
	atrinik_plugin *plugin = find_plugin(plugin_name);

	if (!plugin)
	{
		LOG(llevBug, "BUG: register_global_event(): Could not find plugin %s.\n", plugin_name);
		return;
	}

	LOG(llevInfo, "Plugin %s registered the event %d\n", plugin_name, event_nr);
	plugin->gevent[event_nr] = 1;
}

/**
 * Unregister a global event.
 * @param plugin_name Plugin's name.
 * @param event_nr Event ID to unregister. */
static void unregister_global_event(const char *plugin_name, int event_nr)
{
	atrinik_plugin *plugin = find_plugin(plugin_name);

	if (!plugin)
	{
		LOG(llevBug, "BUG: unregister_global_event(): Could not find plugin %s.\n", plugin_name);
		return;
	}

	plugin->gevent[event_nr] = 0;
}

/**
 * Browse through the inventory of an object to find first event that
 * matches the event type of event_nr.
 * @param op The object to search in.
 * @param event_nr The @ref event_numbers "event number".
 * @return Script object matching the event type. */
object *get_event_object(object *op, int event_nr)
{
	object *tmp;

	for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
	{
		if (tmp->type == EVENT_OBJECT && tmp->sub_type == event_nr)
		{
			return tmp;
		}
	}

	return tmp;
}

/**
 * Tries to find if a given command is handled by a plugin.
 * @param cmd The command name to find.
 * @return Command array structure if found, NULL otherwise. */
CommArray_s *find_plugin_command(const char *cmd)
{
	int i;
	atrinik_plugin *plugin;
	static CommArray_s rtn_cmd;

	if (!plugins_list)
	{
		return NULL;
	}

	for (plugin = plugins_list; plugin; plugin = plugin->next)
	{
		if (plugin->propfunc(&i, "command?", cmd, &rtn_cmd))
		{
			return &rtn_cmd;
		}
	}

	return NULL;
}

/**
 * Display a list of loaded and loadable plugins in player's window.
 * @param op The player to print the plugins to. */
void display_plugins_list(object *op)
{
	char buf[MAX_BUF];
	struct dirent *currentfile;
	DIR *plugdir;
	atrinik_plugin *plugin;

	new_draw_info(NDI_UNIQUE, op, "List of loaded plugins:");
	new_draw_info(NDI_UNIQUE, op, "-----------------------");

	for (plugin = plugins_list; plugin; plugin = plugin->next)
	{
		new_draw_info_format(NDI_UNIQUE, op, "%s, %s", plugin->id, plugin->fullname);
	}

	snprintf(buf, sizeof(buf), "%s/", PLUGINDIR);

	/* Open the plugins directory */
	if (!(plugdir = opendir(buf)))
	{
		return;
	}

	new_draw_info(NDI_UNIQUE, op, "\nList of loadable plugins:");
	new_draw_info(NDI_UNIQUE, op, "-----------------------");

	/* Go through the files in the directory */
	while ((currentfile = readdir(plugdir)))
	{
		size_t l = strlen(currentfile->d_name);

		if (l > strlen(PLUGIN_SUFFIX) && !strcmp(currentfile->d_name + l - strlen(PLUGIN_SUFFIX), PLUGIN_SUFFIX))
		{
			new_draw_info(NDI_UNIQUE, op, currentfile->d_name);
		}
	}

	closedir(plugdir);
}

/**
 * Initializes plugins. Browses the plugins directory and calls
 * init_plugin() for each plugin file found with the extension being
 * @ref PLUGIN_SUFFIX. */
void init_plugins()
{
	struct dirent *currentfile;
	DIR *plugdir;
	char pluginfile[MAX_BUF];

	LOG(llevInfo, "Initializing plugins from '%s':\n", PLUGINDIR);

	if (!(plugdir = opendir(PLUGINDIR)))
	{
		return;
	}

	while ((currentfile = readdir(plugdir)))
	{
		size_t l = strlen(currentfile->d_name);

		if (l > strlen(PLUGIN_SUFFIX) && !strcmp(currentfile->d_name + l - strlen(PLUGIN_SUFFIX), PLUGIN_SUFFIX))
		{
			snprintf(pluginfile, sizeof(pluginfile), "%s/%s", PLUGINDIR, currentfile->d_name);
			LOG(llevInfo, "Loading plugin %s\n", currentfile->d_name);
			init_plugin(pluginfile);
		}
	}

	closedir(plugdir);
}

#ifdef WIN32
/**
 * There is no dlerror() on Win32, so we make our own.
 * @return Returned error from loading a plugin. */
static const char *plugins_dlerror()
{
	static char buf[MAX_BUF];
	DWORD err = GetLastError();
	char *p;

	if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0, buf, sizeof(buf), NULL) == 0)
	{
		snprintf(buf, sizeof(buf), "error %lu", err);
	}

	p = strchr(buf, '\0');

	while (p > buf && (p[ - 1] == '\r' || p[ - 1] == '\n'))
	{
		p--;
	}

	*p = '\0';
	return buf;
}
#endif

/**
 * Initializes a plugin known by its filename.
 * @param pluginfile The plugin filename. */
void init_plugin(const char *pluginfile)
{
	int i;
	LIBPTRTYPE ptr;
	f_plug_api eventfunc, propfunc;
	f_plug_init initfunc;
	f_plug_pinit pinitfunc;
	atrinik_plugin *plugin;

	ptr = plugins_dlopen(pluginfile);

	if (!ptr)
	{
		LOG(llevBug, "BUG: Error while trying to load %s, returned: %s\n", pluginfile, plugins_dlerror());
		return;
	}

	initfunc = (f_plug_init) (plugins_dlsym(ptr, "initPlugin"));

	if (!initfunc)
	{
		LOG(llevBug, "BUG: Error while requesting 'initPlugin' from %s: %s\n", pluginfile, plugins_dlerror());
		plugins_dlclose(ptr);
		return;
	}

	eventfunc = (f_plug_api) (plugins_dlsym(ptr, "triggerEvent"));

	if (!eventfunc)
	{
		LOG(llevBug, "BUG: Error while requesting 'triggerEvent' from %s: %s\n", pluginfile, plugins_dlerror());
		plugins_dlclose(ptr);
		return;
	}

	pinitfunc = (f_plug_pinit) (plugins_dlsym(ptr, "postinitPlugin"));

	if (!pinitfunc)
	{
		LOG(llevBug, "BUG: Error while requesting 'postinitPlugin' from %s: %s\n", pluginfile, plugins_dlerror());
		plugins_dlclose(ptr);
		return;
	}

	propfunc = (f_plug_api) (plugins_dlsym(ptr, "getPluginProperty"));

	if (!propfunc)
	{
		LOG(llevBug, "BUG: Error while requesting 'getPluginProperty' from %s: %s\n", pluginfile, plugins_dlerror());
		plugins_dlclose(ptr);
		return;
	}

	plugin = malloc(sizeof(atrinik_plugin));

	for (i = 0; i < GEVENT_NUM; i++)
	{
		plugin->gevent[i] = 0;
	}

	plugin->eventfunc = eventfunc;
	plugin->propfunc = propfunc;
	plugin->libptr = ptr;
	plugin->next = NULL;

	initfunc(&hooklist);
	propfunc(0, "Identification", plugin->id, sizeof(plugin->id));
	propfunc(0, "FullName", plugin->fullname, sizeof(plugin->fullname));
	LOG(llevInfo, "Plugin name: %s, known as %s\n", plugin->fullname, plugin->id);

	if (!plugins_list)
	{
		plugins_list = plugin;
	}
	else
	{
		plugin->next = plugins_list;
		plugins_list = plugin;
	}

	pinitfunc();
	LOG(llevInfo, "[Done]\n");
}

/**
 * Removes one plugin from memory. The plugin is identified by its keyname.
 * @param id The plugin keyname. */
void remove_plugin(const char *id)
{
	atrinik_plugin *plugin, *prev = NULL;

	if (!plugins_list)
	{
		return;
	}

	for (plugin = plugins_list; plugin; prev = plugin, plugin = plugin->next)
	{
		if (!strcmp(plugin->id, id))
		{
			if (!prev)
			{
				plugins_list = plugin->next;
			}
			else
			{
				prev->next = plugin->next;
			}

			plugins_dlclose(plugin->libptr);
			free(plugin);
			break;
		}
	}
}

/**
 * Deinitialize all plugins. */
void remove_plugins()
{
	atrinik_plugin *plugin;

	if (!plugins_list)
	{
		return;
	}

	LOG(llevInfo, "Removing all plugins from memory.\n");

	for (plugin = plugins_list; plugin; )
	{
		atrinik_plugin *next = plugin->next;

		plugins_dlclose(plugin->libptr);
		free(plugin);
		plugin = next;
	}

	plugins_list = NULL;
}

/**
 * Initialize map event object.
 * @param ob What to initialize. */
void map_event_obj_init(object *ob)
{
	map_event *tmp;

	if (!ob->map)
	{
		LOG(llevBug, "BUG: Map event object not on map.\n");
		return;
	}

	tmp = malloc(sizeof(map_event));
	tmp->plugin = NULL;
	tmp->event = ob;

	tmp->next = ob->map->events;
	ob->map->events = tmp;
}

/**
 * Free a ::map_event.
 * @param tmp What to free. */
void map_event_free(map_event *tmp)
{
	free(tmp);
}

/**
 * Deinitialize map event object.
 * @param ob What to deinitialize. */
void map_event_obj_deinit(object *ob)
{
	map_event *tmp, *prev = NULL;

	if (!ob->map)
	{
		return;
	}

	for (tmp = ob->map->events; tmp; prev = tmp, tmp = tmp->next)
	{
		if (tmp->event == ob)
		{
			if (!prev)
			{
				ob->map->events = tmp->next;
			}
			else
			{
				prev->next = tmp->next;
			}

			map_event_free(tmp);
			break;
		}
	}
}

/**
 * Triggers a map-wide event.
 * @param event_id Event ID to trigger.
 * @param m Map we're working on.
 * @param activator Activator.
 * @param other Some other object related to this event.
 * @param text String related to this event.
 * @param parm Integer related to this event.
 * @return 1 if the event returns an event value, 0 otherwise. */
int trigger_map_event(int event_id, mapstruct *m, object *activator, object *other, char *text, int parm)
{
	map_event *tmp;

	if (!m->events)
	{
		return 0;
	}

	for (tmp = m->events; tmp; tmp = tmp->next)
	{
		if (tmp->event->sub_type == event_id)
		{
			/* Load the event object's plugin as needed. */
			if (!tmp->plugin)
			{
				tmp->plugin = find_plugin(tmp->event->name);

				if (!tmp->plugin)
				{
					LOG(llevBug, "BUG: trigger_map_event(): Tried to trigger map event #%d, but could not find plugin '%s'.\n", event_id, tmp->event->name);
					return 0;
				}
			}

			return *(int *) (tmp->plugin->eventfunc)(0, PLUGIN_EVENT_MAP, event_id, activator, tmp->event, other, tmp->event->race, tmp->event->slaying, text, parm);
		}
	}

	return 0;
}

/**
 * Handles triggering global events like EVENT_BORN, EVENT_MAPRESET,
 * etc.
 * @param event_type The event type.
 * @param parm1 First parameter.
 * @param parm2 Second parameter. */
void trigger_global_event(int event_type, void *parm1, void *parm2)
{
	atrinik_plugin *plugin;

	if (!plugins_list)
	{
		return;
	}

	for (plugin = plugins_list; plugin; plugin = plugin->next)
	{
		if (plugin->gevent[event_type])
		{
			(plugin->eventfunc)(0, PLUGIN_EVENT_GLOBAL, event_type, parm1, parm2);
		}
	}
}

/**
 * Handles triggering normal events like EVENT_ATTACK, EVENT_STOP,
 * etc.
 * @param event_type The event type.
 * @param activator Activator object.
 * @param me Object the event object is in.
 * @param other Other object.
 * @param msg Message.
 * @param parm1 First parameter.
 * @param parm2 Second parameter.
 * @param parm3 Third parameter.
 * @param flags Event flags.
 * @return 1 if the event returns an event value, 0 otherwise. */
int trigger_event(int event_type, object *const activator, object *const me, object *const other, const char *msg, int parm1, int parm2, int parm3, int flags)
{
	object *event_obj;
	atrinik_plugin *plugin;

	if (me == NULL || !(me->event_flags & EVENT_FLAG(event_type)) || !plugins_list)
	{
		return 0;
	}

	if ((event_obj = get_event_object(me, event_type)) == NULL)
	{
		LOG(llevBug, "BUG: Object with event flag and no event object: %s\n", STRING_OBJ_NAME(me));
		me->event_flags &= ~(1 << event_type);
		return 0;
	}

	if (event_obj->name && (plugin = find_plugin(event_obj->name)))
	{
		int returnvalue;
#ifdef TIME_SCRIPTS
		struct timeval start, stop;
		uint64 start_u, stop_u;

		gettimeofday(&start, NULL);
#endif

		returnvalue = *(int *) plugin->eventfunc(0, PLUGIN_EVENT_NORMAL, event_type, activator, me, other, event_obj, msg, parm1, parm2, parm3, flags, event_obj->race, event_obj->slaying);

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

	return 0;
}
