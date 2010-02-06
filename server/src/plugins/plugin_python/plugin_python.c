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
 * Atrinik python plugin. */

#ifdef WIN32
#include <fcntl.h>
#endif
#include <plugin_python.h>

#include <compile.h>
#include <eval.h>
#ifdef STR
/* STR is redefined in node.h. Since this file doesn't use STR, we remove it */
#undef STR
#endif
#include <node.h>

/** Hooks. */
struct plugin_hooklist *hooks;

/** A generic exception that we use for error messages. */
PyObject *AtrinikError;

/** The context stack. */
static PythonContext *context_stack;
/** Current context. */
PythonContext *current_context;

/**
 * Useful constants */
static Atrinik_Constant module_constants[] =
{
	{"NORTH",       1},
	{"NORTHEAST",   2},
	{"EAST",        3},
	{"SOUTHEAST",   4},
	{"SOUTH",       5},
	{"SOUTHWEST",   6},
	{"WEST",        7},
	{"NORTHWEST",   8},

	{"llevError",   llevError},
	{"llevBug",     llevBug},
	{"llevInfo",    llevInfo},
	{"llevDebug",   llevDebug},

	{"EVENT_APPLY", EVENT_APPLY},
	{"EVENT_ATTACK", EVENT_ATTACK},
	{"EVENT_DEATH", EVENT_DEATH},
	{"EVENT_DROP", EVENT_DROP},
	{"EVENT_PICKUP", EVENT_PICKUP},
	{"EVENT_SAY", EVENT_SAY},
	{"EVENT_STOP", EVENT_STOP},
	{"EVENT_TIME", EVENT_TIME},
	{"EVENT_THROW", EVENT_THROW},
	{"EVENT_TRIGGER", EVENT_TRIGGER},
	{"EVENT_CLOSE", EVENT_CLOSE},
	{"EVENT_TIMER", EVENT_TIMER},
	{"EVENT_BORN", EVENT_BORN},
	{"EVENT_CLOCK", EVENT_CLOCK},
	{"EVENT_CRASH", EVENT_CRASH},
	{"EVENT_GDEATH", EVENT_GDEATH},
	{"EVENT_GKILL", EVENT_GKILL},
	{"EVENT_LOGIN", EVENT_LOGIN},
	{"EVENT_LOGOUT", EVENT_LOGOUT},
	{"EVENT_MAPENTER", EVENT_MAPENTER},
	{"EVENT_MAPLEAVE", EVENT_MAPLEAVE},
	{"EVENT_MAPRESET", EVENT_MAPRESET},
	{"EVENT_REMOVE", EVENT_REMOVE},
	{"EVENT_SHOUT", EVENT_SHOUT},
	{"EVENT_TELL", EVENT_TELL},

	{NULL, 0}
};

/** All the custom commands. */
static PythonCmd CustomCommand[NR_CUSTOM_CMD];
/** Contains the index of the next command that needs to be run. */
static int NextCustomCommand;

/** Maximum number of cached scripts. */
#define PYTHON_CACHE_SIZE 16

/** One cache entry. */
typedef struct
{
	/** The script file. */
	const char *file;

	/** The cached code. */
	PyCodeObject *code;

	/** Last cached time. */
	time_t cached_time;

	/** Last used time. */
	time_t used_time;
} cacheentry;

/** The Python cache. */
static cacheentry python_cache[PYTHON_CACHE_SIZE];

static int cmd_customPython(object *op, char *params);
static void init_Atrinik_Python();

/**
 * Initialize the context stack. */
static void initContextStack()
{
	current_context = NULL;
	context_stack = NULL;
}

/**
 * Push context to the context stack and to current context.
 * @param context The context to push. */
static void pushContext(PythonContext *context)
{
	if (current_context == NULL)
	{
		context_stack = context;
		context->down = NULL;
	}
	else
	{
		context->down = current_context;
	}

	current_context = context;
}

/**
 * Pop the first context from the current context, replacing it by the
 * next one in the list.
 * @return NULL if there is no current context, the previous current
 * context otherwise. */
static PythonContext *popContext()
{
	PythonContext *oldcontext;

	if (current_context)
	{
		oldcontext = current_context;
		current_context = current_context->down;
		return oldcontext;
	}

	return NULL;
}

/**
 * Free a context.
 * @param context Context to free. */
static void freeContext(PythonContext *context)
{
	free(context);
}

/**
 * @defgroup plugin_python_functions Python plugin functions
 *@{*/

/**
 * <h1>Atrinik.LoadObject(<i>\<string\></i> string)</h1>
 *
 * Load an object from string, for example, one stored using
 * @ref Atrinik_Object_Save "Save()".
 * @param string The string from which to load the actual object. */
static PyObject *Atrinik_LoadObject(PyObject *self, PyObject *args)
{
	char *dumpob;

	(void) self;

	if (!PyArg_ParseTuple(args, "s", &dumpob))
	{
		return NULL;
	}

	return wrap_object(hooks->load_object_str(dumpob));
}

/**
 * <h1>Atrinik.MatchString(<i>\<string\></i> firststr, <i>\<string\></i>
 * secondstr)</h1>
 *
 * Case insensitive string comparison.
 *
 * @param firststr The first string
 * @param secondstr The second string, can contain regular expressions
 * @return 1 if the two strings are the same, or 0 if they differ */
static PyObject *Atrinik_MatchString(PyObject *self, PyObject *args)
{
	char *premiere, *seconde;

	(void) self;

	if (!PyArg_ParseTuple(args, "ss", &premiere, &seconde))
	{
		return NULL;
	}

	return Py_BuildValue("i", (hooks->re_cmp(premiere, seconde) != NULL) ? 1 : 0);
}

/**
 * <h1>Atrinik.ReadyMap(<i>\<string\></i> name, <i>\<int\></i> unique)
 * </h1>
 *
 * Make sure the named map is loaded into memory.
 *
 * @param name Path to the map
 * @param unique Must be 1 if the map is unique. Optional, defaults to 0.
 * @return The loaded Atrinik map
 * @todo Don't crash if unique is wrong */
static PyObject *Atrinik_ReadyMap(PyObject *self, PyObject *args)
{
	char *mapname;
	int flags = 0, unique = 0;

	(void) self;

	if (!PyArg_ParseTuple(args, "s|i", &mapname, &unique))
	{
		return NULL;
	}

	if (unique)
	{
		flags = MAP_PLAYER_UNIQUE;
	}

	return wrap_map(hooks->ready_map_name(mapname, flags));
}

/**
 * <h1>Atrinik.CheckMap(<i>\<string\></i> arch, <i>\<string\></i>
 * map_path, <i>\<int\></i> x, <i>\<int\></i> y)</h1>
 *
 * @warning Unfinished, do not use.
 * @todo Finish. */
static PyObject *Atrinik_CheckMap(PyObject *self, PyObject *args)
{
	char *what;
	char *mapstr;
	int x, y;
	/*  object* foundob; */

	(void) self;

	/* Gecko: replaced coordinate tuple with separate x and y coordinates */
	if (!PyArg_ParseTuple(args, "ssii", &what, &mapstr, &x, &y))
	{
		return NULL;
	}

	RAISE("CheckMap() is not finished!");

	/*  foundob = present_arch(find_archetype(what), has_been_loaded(mapstr), x, y);
	    return wrap_object(foundob);*/
}

/**
 * <h1>Atrinik.FindPlayer(<i>\<string\></i> name)</h1>
 * Find a player.
 *
 * @param name The player name
 * @return The player's object if found, None otherwise */
static PyObject *Atrinik_FindPlayer(PyObject *self, PyObject *args)
{
	player *foundpl;
	object *foundob = NULL;
	char *txt;

	(void) self;

	if (!PyArg_ParseTuple(args, "s", &txt))
	{
		return NULL;
	}

	hooks->adjust_player_name(txt);
	foundpl = hooks->find_player(txt);

	if (foundpl != NULL)
	{
		foundob = foundpl->ob;
	}

	return wrap_object(foundob);
}

/**
 * <h1>Atrinik.PlayerExists(<i>\<string\></i> name)</h1>
 * Check if player exists.
 *
 * @param name The player name
 * @return 1 if the player exists, 0 otherwise */
static PyObject *Atrinik_PlayerExists(PyObject *self, PyObject *args)
{
	char *player_name;

	(void) self;

	if (!PyArg_ParseTuple(args, "s", &player_name))
	{
		return NULL;
	}

	hooks->adjust_player_name(player_name);

	return Py_BuildValue("i", hooks->player_exists(player_name));
}

/**
 * <h1>Atrinik.WhoAmI()</h1>
 * Get the owner of the active script (the object that has the event
 * handler).
 * @return The script owner. */
static PyObject *Atrinik_WhoAmI(PyObject *self, PyObject *args)
{
	(void) self;
	(void) args;
	return wrap_object(current_context->who);
}

/**
 * <h1>Atrinik.WhoIsActivator()</h1>
 * Get the object that activated the current event.
 * @return The script activator. */
static PyObject *Atrinik_WhoIsActivator(PyObject *self, PyObject *args)
{
	(void) self;
	(void) args;
	return wrap_object(current_context->activator);
}

/**
 * <h1>Atrinik.WhoIsOther()</h1>
 * @warning Untested. */
static PyObject *Atrinik_WhoIsOther(PyObject *self, PyObject *args)
{
	(void) self;
	(void) args;
	return wrap_object(current_context->event);
}

/**
 * <h1>Atrinik.WhatIsEvent()</h1>
 * Get the event object that caused this event to trigger.
 * @return The event object. */
static PyObject *Atrinik_WhatIsEvent(PyObject *self, PyObject *args)
{
	(void) self;
	(void) args;
	return wrap_object(current_context->event);
}

/**
 * <h1>Atrinik.GetEventNumber()</h1>
 * Get the ID of the event that is being triggered.
 * @return Event ID. */
static PyObject *Atrinik_GetEventNumber(PyObject *self, PyObject *args)
{
	(void) self;
	(void) args;
	return Py_BuildValue("i", current_context->event->sub_type1);
}

/**
 * <h1>Atrinik.WhatIsMessage()</h1>
 * Gets the actual message in SAY events.
 * @return The message. */
static PyObject *Atrinik_WhatIsMessage(PyObject *self, PyObject *args)
{
	(void) self;
	(void) args;
	return Py_BuildValue("s", current_context->text);
}

/**
 * <h1>Atrinik.GetOptions()</h1>
 * Gets the script options (as passed in the event's slaying field).
 * @return The script options. */
static PyObject *Atrinik_GetOptions(PyObject *self, PyObject *args)
{
	(void) self;
	(void) args;
	return Py_BuildValue("s", current_context->options);
}

/**
 * <h1>Atrinik.GetReturnValue()</h1>
 * Gets the script's return value.
 * @return The return value */
static PyObject *Atrinik_GetReturnValue(PyObject *self, PyObject *args)
{
	(void) self;
	(void) args;
	return Py_BuildValue("i", current_context->returnvalue);
}

/**
 * <h1>Atrinik.SetReturnValue(<i>\<int\></i> value)</h1>
 * Sets the script's return value.
 * @param value The new return value */
static PyObject *Atrinik_SetReturnValue(PyObject *self, PyObject *args)
{
	int value;

	(void) self;

	if (!PyArg_ParseTuple(args, "i", &value))
	{
		return NULL;
	}

	current_context->returnvalue = value;

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>Atrinik.GetEventParameters()</h1>
 * Get the parameters of an event. This varies from event to event, and
 * some events pass all parameters as 0. EVENT_ATTACK usually passes damage
 * done and the WC of the hit as second and third parameter, respectively.
 * @return A list of the event parameters. The last entry is the event flags,
 * used to determine whom to call fix_player() on after executing the script. */
static PyObject *Atrinik_GetEventParameters(PyObject *self, PyObject *args)
{
	unsigned int i;
	PyObject *list = PyList_New(0);

	(void) self;
	(void) args;

	for (i = 0; i < sizeof(current_context->parms) / sizeof(current_context->parms[0]); i++)
	{
		PyList_Append(list, Py_BuildValue("i", current_context->parms[i]));
	}

	return list;
}

/**
 * <h1>Atrinik.GetSpellNr(<i>\<string\></i> name)</h1>
 * Gets the number of the named spell.
 * @param name The spell name
 * @return Number of the spell, -1 if no such spell exists. */
static PyObject *Atrinik_GetSpellNr(PyObject *self, PyObject *args)
{
	char *spell;

	(void) self;

	if (!PyArg_ParseTuple(args, "s", &spell))
	{
		return NULL;
	}

	return Py_BuildValue("i", hooks->look_up_spell_name(spell));
}

/**
 * <h1>Atrinik.GetSpell(<i>\<int\></i> spell)</h1>
 * Get various information about a spell, including things like its
 * level, type, etc.
 * @param spell ID of the spell. */
static PyObject *Atrinik_GetSpell(PyObject *self, PyObject *args)
{
	int spell;
	PyObject *dict;

	(void) self;

	if (!PyArg_ParseTuple(args, "i", &spell))
	{
		return NULL;
	}

	if (spell < 0 || spell > NROFREALSPELLS)
	{
		RAISE("Invalid ID of a spell.");
	}

	dict = PyDict_New();

	PyDict_SetItemString(dict, "name", Py_BuildValue("s", hooks->spells[spell].name));
	PyDict_SetItemString(dict, "level", Py_BuildValue("i", hooks->spells[spell].level));
	PyDict_SetItemString(dict, "type", Py_BuildValue("s", hooks->spells[spell].type == SPELL_TYPE_WIZARD ? "wizard" : "priest"));
	PyDict_SetItemString(dict, "sp", Py_BuildValue("i", hooks->spells[spell].sp));
	PyDict_SetItemString(dict, "time", Py_BuildValue("i", hooks->spells[spell].time));

	return dict;
}

/**
 * <h1>Atrinik.GetSkillNr(<i>\<string\></i> name)</h1>
 * Gets the number of the named skill.
 * @param name The skill name
 * @return Number of the skill, -1 if no such skill exists. */
static PyObject *Atrinik_GetSkillNr(PyObject *self, PyObject *args)
{
	char *skill;

	(void) self;

	if (!PyArg_ParseTuple(args, "s", &skill))
	{
		return NULL;
	}

	return Py_BuildValue("i", hooks->lookup_skill_by_name(skill));
}

/**
 * <h1>Atrinik.RegisterCommand(<i>\<string\></i> cmdname,
 * <i>\<string\></i> scriptname, <i>\<double\></i> speed)</h1>
 * Register a custom command. */
static PyObject *Atrinik_RegisterCommand(PyObject *self, PyObject *args)
{
	char *cmdname, *scriptname;
	double cmdspeed;
	int i;

	(void) self;

	if (!PyArg_ParseTuple(args, "ssd", &cmdname, &scriptname, &cmdspeed))
	{
		return NULL;
	}

	for (i = 0; i < NR_CUSTOM_CMD; i++)
	{
		if (CustomCommand[i].name)
		{
			if (!strcmp(CustomCommand[i].name, cmdname))
			{
				RAISE("This command is already registered");
			}
		}
	}

	for (i = 0; i < NR_CUSTOM_CMD; i++)
	{
		if (CustomCommand[i].name == NULL)
		{
			CustomCommand[i].name = hooks->strdup_local(cmdname);
			CustomCommand[i].script = hooks->strdup_local(scriptname);
			CustomCommand[i].speed = cmdspeed;
			break;
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>Atrinik.CreatePathname(<i>\<string\></i> path)</h1>
 * Creates path to file in the maps directory using the create_pathname()
 * function.
 * @param path Path to file to create.
 * @return The path to file in the maps directory. */
static PyObject *Atrinik_CreatePathname(PyObject *self, PyObject *args)
{
	char *path;

	(void) self;

	if (!PyArg_ParseTuple(args, "s", &path))
	{
		return NULL;
	}

	return Py_BuildValue("s", hooks->create_pathname(path));
}

/**
 * <h1>Atrinik.GetTime()</h1>
 * Get game time using a hook for get_tod().
 * @return A dictionary containing all the information about the in-game
 * time:
 * - <b>year</b>: Current year.
 * - <b>month</b>: Current month.
 * - <b>month_name</b>: Name of the current month.
 * - <b>day</b>: Day.
 * - <b>hour</b>: Hour.
 * - <b>minute</b>: Minute.
 * - <b>dayofweek</b>: Day of the week.
 * - <b>dayofweek_name</b>: Name of the week day.
 * - <b>season</b>: Season.
 * - <b>season_name</b>: Name of the season.
 * - <b>periodofday</b>: Period of the day.
 * - <b>periodofday_name</b>: Name of the period of the day. */
static PyObject *Atrinik_GetTime(PyObject *self, PyObject *args)
{
	PyObject *dict = PyDict_New();
	timeofday_t tod;

	(void) self;
	(void) args;

	hooks->get_tod(&tod);

	PyDict_SetItemString(dict, "year", Py_BuildValue("i", tod.year + 1));
	PyDict_SetItemString(dict, "month", Py_BuildValue("i", tod.month + 1));
	PyDict_SetItemString(dict, "month_name", Py_BuildValue("s", hooks->month_name[tod.month]));
	PyDict_SetItemString(dict, "day", Py_BuildValue("i", tod.day + 1));
	PyDict_SetItemString(dict, "hour", Py_BuildValue("i", tod.hour));
	PyDict_SetItemString(dict, "minute", Py_BuildValue("i", tod.minute + 1));
	PyDict_SetItemString(dict, "dayofweek", Py_BuildValue("i", tod.dayofweek + 1));
	PyDict_SetItemString(dict, "dayofweek_name", Py_BuildValue("s", hooks->weekdays[tod.dayofweek]));
	PyDict_SetItemString(dict, "season", Py_BuildValue("i", tod.season + 1));
	PyDict_SetItemString(dict, "season_name", Py_BuildValue("s", hooks->season_name[tod.season]));
	PyDict_SetItemString(dict, "periodofday", Py_BuildValue("i", tod.periodofday + 1));
	PyDict_SetItemString(dict, "periodofday_name", Py_BuildValue("s", hooks->periodsofday[tod.periodofday]));

	return dict;
}

/**
 * <h1>Atrinik.LocateBeacon(<i>\<string\><i> beacon_name)</h1>
 * Locate a beacon.
 * @param beacon_name The beacon name to find.
 * @return The beacon if found, None otherwise. */
static PyObject *Atrinik_LocateBeacon(PyObject *self, PyObject *args)
{
	char *beacon_name;
	const char *name = NULL;
	object *myob;

	if (!PyArg_ParseTuple(args, "s", &beacon_name))
	{
		return NULL;
	}

	(void) self;

	FREE_AND_COPY_HASH(name, beacon_name);
	myob = hooks->beacon_locate(name);
	FREE_AND_CLEAR_HASH(name);

	return wrap_object(myob);
}

/**
 * <h1>Atrinik.FindParty(<i>\<string\><i> partyname)</h1>
 * Find a party by name.
 * @param partyname The party name to find.
 * @return The party if found, None otherwise. */
static PyObject *Atrinik_FindParty(PyObject *self, PyObject *args)
{
	char *partyname;

	(void) self;

	if (!PyArg_ParseTuple(args, "s", &partyname))
	{
		return NULL;
	}

	return wrap_party(hooks->find_party(partyname));
}

/**
 * <h1>Atrinik.CleanupChatString(<i>\<string\><i> string)</h1>
 * Cleans up a chat string, using cleanup_chat_string().
 * @param string The string to cleanup.
 * @return Cleaned up string - can be None. */
static PyObject *Atrinik_CleanupChatString(PyObject *self, PyObject *args)
{
	char *string;

	(void) self;

	if (!PyArg_ParseTuple(args, "s", &string))
	{
		return NULL;
	}

	return Py_BuildValue("s", hooks->cleanup_chat_string(string));
}

/**
 * <h1>Atrinik.LOG(<i>\<int\></i> mode, <i>\<string\><i> string)</h1>
 * Logs a message.
 * @param mode Logging mode to use, one of:
 * - llevError: An irrecoverable error. Will shut down the server.
 * - llevBug: A bug.
 * - llevInfo: Info.
 * - llevDebug: Debug information.
 * @param string The message to log. */
static PyObject *Atrinik_LOG(PyObject *self, PyObject *args)
{
	char *string;
	int mode;

	(void) self;

	if (!PyArg_ParseTuple(args, "is", &mode, &string))
	{
		return NULL;
	}

	LOG(mode, string);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>Atrinik.DestroyTimer(<i>\<int\></i> timer)</h1>
 * Destroy an existing timer.
 * @param timer ID of the timer.
 * @return 0 on success, anything lower on failure. */
static PyObject *Atrinik_DestroyTimer(PyObject *self, PyObject *args)
{
	int id;

	(void) self;

	if (!PyArg_ParseTuple(args, "i", &id))
	{
		return NULL;
	}

	return Py_BuildValue("i", hooks->cftimer_destroy(id));
}

/**
 * <h1>Atrinik.FindFace(<i>\<string\></i> face)</h1>
 * Find a face ID by its name.
 * @param face Name of the face to find.
 * @return ID of the face. */
static PyObject *Atrinik_FindFace(PyObject *self, PyObject *args)
{
	char *name;

	(void) self;

	if (!PyArg_ParseTuple(args, "s", &name))
	{
		return NULL;
	}

	return Py_BuildValue("i", hooks->find_face(name, 0));
}

/**
 * <h1>Atrinik.FindAnimation(<i>\<string\></i> animation)</h1>
 * Find an animation ID by its name.
 * @param animation Name of the animation to find.
 * @return ID of the animation. */
static PyObject *Atrinik_FindAnimation(PyObject *self, PyObject *args)
{
	char *name;

	(void) self;

	if (!PyArg_ParseTuple(args, "s", &name))
	{
		return NULL;
	}

	return Py_BuildValue("i", hooks->find_animation(name));
}

/*@}*/

/**
 * Open a Python file.
 * @param filename File to open.
 * @return Python object of the file, NULL on failure. */
static PyObject *python_openfile(char *filename)
{
	PyObject *scriptfile;
#ifdef IS_PY3K
	int fd = open(filename, O_RDONLY);

	if (fd == -1)
	{
		return NULL;
	}

	scriptfile = PyFile_FromFd(fd, filename, "r", -1, NULL, NULL, NULL, 1);
#else
	if (!(scriptfile = PyFile_FromString(filename, "r")))
	{
		return NULL;
	}
#endif

	return scriptfile;
}

/**
 * Return a FILE object from a Python file object.
 * @param obj Python object of the file.
 * @return FILE pointer to the file. */
static FILE *python_pyfile_asfile(PyObject *obj)
{
#ifdef IS_PY3K
	return fdopen(PyObject_AsFileDescriptor(obj), "r");
#else
	return PyFile_AsFile(obj);
#endif
}

/**
 * Execute a script, handling loading, parsing and caching.
 * @param path Path to the script.
 * @param event_object Event object.
 * @return -1 on failure, 0 on success. */
static int RunPythonScript(const char *path, object *event_object)
{
	PyObject *scriptfile;
	char *fullpath = hooks->create_pathname(path);
	const char *sh_path = NULL;
	struct stat stat_buf;
	int i, result = -1;
	cacheentry *replace = NULL, *run = NULL;
	struct _node *n;
	PyObject *globdict;

	if (event_object && fullpath[0] != '/')
	{
		char tmp_path[HUGE_BUF];
		object *outermost = event_object;

		while (outermost && outermost->env)
		{
			outermost = outermost->env;
		}

		if (outermost && outermost->map)
		{
			hooks->normalize_path(outermost->map->path, path, tmp_path);

			fullpath = hooks->create_pathname(tmp_path);
		}
	}

	if (!(scriptfile = python_openfile(fullpath)))
	{
		LOG(llevDebug, "PYTHON:: The script file %s can't be opened\n", path);
		return -1;
	}

	if (stat(fullpath, &stat_buf))
	{
		LOG(llevDebug, "PYTHON:: The script file %s can't be stat()ed\n", fullpath);
		Py_DECREF(scriptfile);
		return -1;
	}

	/* Create a shared string */
	FREE_AND_COPY_HASH(sh_path, fullpath);

	/* Search through cache. Three cases:
	 * 1) script in cache, but older than file  -> replace cached
	 * 2) script in cache and up to date        -> use cached
	 * 3) script not in cache, cache not full   -> add to end of cache
	 * 4) script not in cache, cache full       -> replace least recently used */
	for (i = 0; i < PYTHON_CACHE_SIZE; i++)
	{
		if (python_cache[i].file == NULL)
		{
#ifdef PYTHON_DEBUG
			LOG(llevDebug, "PYTHON:: Adding file to cache\n");
#endif
			/* Case 3 */
			replace = &python_cache[i];
			break;
		}
		else if (python_cache[i].file == sh_path)
		{
			/* Found it. Compare timestamps. */
			if (python_cache[i].code == NULL || python_cache[i].cached_time < stat_buf.st_mtime)
			{
#ifdef PYTHON_DEBUG
				LOG(llevDebug, "PYTHON:: File newer than cached bytecode -> reloading\n");
#endif
				/* Case 1 */
				replace = &python_cache[i];
			}
			else
			{
#ifdef PYTHON_DEBUG
				LOG(llevDebug, "PYTHON:: Using cached version\n");
#endif
				/* Case 2 */
				replace = NULL;
				run = &python_cache[i];
			}
			break;
		}
		/* Prepare for case 4 */
		else if (replace == NULL || python_cache[i].used_time < replace->used_time)
		{
			replace = &python_cache[i];
		}
	}

	/* Replace a specific cache index with the file */
	if (replace)
	{
		FILE *pyfile;

		/* Safe to call on NULL */
		Py_XDECREF(replace->code);
		replace->code = NULL;

		/* Need to replace path string? */
		if (replace->file != sh_path)
		{
			if (replace->file)
			{
#ifdef PYTHON_DEBUG
				LOG(llevDebug, "PYTHON:: Purging %s (cache index %d): \n", replace->file, replace - python_cache);
#endif
				FREE_AND_CLEAR_HASH(replace->file);
			}

			FREE_AND_COPY_HASH(replace->file, sh_path);
		}

		/* Load, parse and compile */
#ifdef PYTHON_DEBUG
		LOG(llevDebug, "PYTHON:: Parse and compile (cache index %d): \n", replace - python_cache);
#endif

		pyfile = python_pyfile_asfile(scriptfile);

		if ((n = PyParser_SimpleParseFile(pyfile, fullpath, Py_file_input)))
		{
			replace->code = PyNode_Compile(n, fullpath);
			PyNode_Free(n);
		}

		if (PyErr_Occurred())
		{
			PyErr_Print();
		}
		else
		{
			replace->cached_time = stat_buf.st_mtime;
		}

		run = replace;
	}

	/* Run an old or new code object */
	if (run && run->code)
	{
		/* Create a new environment with each execution. Don't want any old variables hanging around */
		globdict = PyDict_New();
		PyDict_SetItemString(globdict, "__builtins__", PyEval_GetBuiltins());

#ifdef PYTHON_DEBUG
		LOG(llevDebug, "PYTHON:: PyEval_EvalCode (cache index %d): \n", run - python_cache);
#endif

		PyEval_EvalCode(run->code, globdict, NULL);

		if (PyErr_Occurred())
		{
			PyErr_Print();
		}
		else
		{
			/* Only return 0 if we actually succeeded */
			result = 0;
			run->used_time = time(NULL);
		}

#ifdef PYTHON_DEBUG
		LOG(llevDebug, "closing. ");
#endif
		Py_DECREF(globdict);
	}

	FREE_AND_CLEAR_HASH(sh_path);
	Py_DECREF(scriptfile);

	return result;
}

/**
 * Handles standard local events.
 * @param args List of arguments for context.
 * @return 0 on failure, script's return value otherwise. */
static int HandleEvent(va_list args)
{
	char *script;
	PythonContext *context = malloc(sizeof(PythonContext));
	int rv;

	context->activator = va_arg(args, object *);
	context->who = va_arg(args, object *);
	context->other = va_arg(args, object *);
	context->event = va_arg(args, object *);
	context->text = va_arg(args, char *);
	context->parms[0] = va_arg(args, int);
	context->parms[1] = va_arg(args, int);
	context->parms[2] = va_arg(args, int);
	context->parms[3] = va_arg(args, int);
	script = va_arg(args, char *);
	context->options = va_arg(args, char *);
	context->returnvalue = 0;

#ifdef PYTHON_DEBUG
	LOG(llevDebug, "PYTHON:: Start script file >%s<\n", script);
	LOG(llevDebug, "PYTHON:: Call data: o1:>%s< o2:>%s< o3:>%s< text:>%s< i1:%d i2:%d i3:%d i4:%d\n", STRING_OBJ_NAME(context->activator), STRING_OBJ_NAME(context->who), STRING_OBJ_NAME(context->other), STRING_SAFE(context->text), context->parms[0], context->parms[1], context->parms[2], context->parms[3]);
#endif

	pushContext(context);

	if (RunPythonScript(script, context->who))
	{
		freeContext(context);
		return 0;
	}

#ifdef PYTHON_DEBUG
	LOG(llevDebug, "fixing. ");
#endif

	context = popContext();

	if (context->parms[3] == SCRIPT_FIX_ALL)
	{
		if (context->other)
		{
			hooks->fix_player(context->other);
		}

		if (context->who)
		{
			hooks->fix_player(context->who);
		}

		if (context->activator)
		{
			hooks->fix_player(context->activator);
		}
	}
	else if (context->parms[3] == SCRIPT_FIX_ACTIVATOR)
	{
		hooks->fix_player(context->activator);
	}

	rv = context->returnvalue;
	freeContext(context);

#ifdef PYTHON_DEBUG
	LOG(llevDebug, "done (returned: %d)!\n", rv);
#endif

	return rv;
}

/**
 * Handle a global event.
 * @param event_type The event type.
 * @param args List of arguments for context.
 * @return 0. */
static int HandleGlobalEvent(int event_type, va_list args)
{
	PythonContext *context = malloc(sizeof(PythonContext));

	context->activator = NULL;
	context->who = NULL;
	context->other = NULL;
	context->event = NULL;
	context->parms[0] = 0;
	context->parms[1] = 0;
	context->parms[2] = 0;
	context->parms[3] = 0;
	context->text = NULL;
	context->options = NULL;
	context->returnvalue = 0;

	pushContext(context);

	switch (event_type)
	{
		case EVENT_CRASH:
			LOG(llevDebug, "Unimplemented for now\n");
			break;

		case EVENT_BORN:
			context->activator = (object *) va_arg(args, void *);
			break;

		case EVENT_LOGIN:
			context->activator = ((player *) va_arg(args, void *))->ob;
			context->who = ((player *) va_arg(args, void *))->ob;
			context->text = (char *) va_arg(args, void *);
			break;

		case EVENT_LOGOUT:
			context->activator = ((player *) va_arg(args, void *))->ob;
			context->who = ((player *) va_arg(args, void *))->ob;
			context->text = (char *) va_arg(args, void *);
			break;

		case EVENT_REMOVE:
			context->activator = (object *) va_arg(args, void *);
			break;

		case EVENT_SHOUT:
			context->activator = (object *) va_arg(args, void *);
			context->text = (char *) va_arg(args, void *);
			break;

		case EVENT_MAPENTER:
			context->activator = (object *) va_arg(args, void *);
			break;

		case EVENT_MAPLEAVE:
			context->activator = (object *) va_arg(args, void *);
			break;

		case EVENT_CLOCK:
			break;

		case EVENT_MAPRESET:
			context->text = (char *) va_arg(args, void *);
			break;
	}

	if (RunPythonScript("python/events/python_event.py", NULL))
	{
		freeContext(context);
		return 0;
	}

	context = popContext();
	freeContext(context);

	return 0;
}

MODULEAPI void *triggerEvent(int *type, ...)
{
	va_list args;
	int eventcode;
	static int result = 0;

	va_start(args, type);
	eventcode = va_arg(args, int);

#ifdef PYTHON_DEBUG
	LOG(llevDebug, "PYTHON:: triggerEvent(): eventcode %d\n", eventcode);
#endif

	switch (eventcode)
	{
		case EVENT_NONE:
			LOG(llevDebug, "PYTHON:: Warning - EVENT_NONE requested\n");
			break;

		case EVENT_ATTACK:
		case EVENT_APPLY:
		case EVENT_DEATH:
		case EVENT_DROP:
		case EVENT_PICKUP:
		case EVENT_SAY:
		case EVENT_STOP:
		case EVENT_TELL:
		case EVENT_TIME:
		case EVENT_THROW:
		case EVENT_TRIGGER:
		case EVENT_CLOSE:
		case EVENT_TIMER:
			result = HandleEvent(args);
			break;

		case EVENT_BORN:
		case EVENT_CRASH:
		case EVENT_LOGIN:
		case EVENT_LOGOUT:
		case EVENT_REMOVE:
		case EVENT_SHOUT:
		case EVENT_MAPENTER:
		case EVENT_MAPLEAVE:
		case EVENT_CLOCK:
		case EVENT_MAPRESET:
			result = HandleGlobalEvent(eventcode, args);
			break;
	}

	va_end(args);
	return &result;
}

MODULEAPI void initPlugin(struct plugin_hooklist *hooklist)
{
	hooks = hooklist;

	LOG(llevDebug, "Atrinik Plugin loading.....\n");
	Py_Initialize();
	init_Atrinik_Python();
	LOG(llevDebug, "[Done]\n");
}

MODULEAPI void *getPluginProperty(int *type, ...)
{
	va_list args;
	const char *propname;
	int i, size;
	char *buf;

	va_start(args, type);
	propname = va_arg(args, const char *);

	if (!strcmp(propname, "command?"))
	{
		const char *cmdname = va_arg(args, const char *);
		CommArray_s *rtn_cmd = va_arg(args, CommArray_s *);

		va_end(args);

		for (i = 0; i < NR_CUSTOM_CMD; i++)
		{
			if (CustomCommand[i].name != NULL)
			{
				if (!strcmp(CustomCommand[i].name, cmdname))
				{
					rtn_cmd->name = CustomCommand[i].name;
					rtn_cmd->time = (float) CustomCommand[i].speed;
					rtn_cmd->func = cmd_customPython;
					NextCustomCommand = i;
					return rtn_cmd;
				}
			}
		}

		return NULL;
	}
	else if (!strcmp(propname, "Identification"))
	{
		buf = va_arg(args, char *);
		size = va_arg(args, int);
		va_end(args);
		snprintf(buf, size, PLUGIN_NAME);
		return NULL;
	}
	else if (!strcmp(propname, "FullName"))
	{
		buf = va_arg(args, char *);
		size = va_arg(args, int);
		va_end(args);
		snprintf(buf, size, PLUGIN_VERSION);
		return NULL;
	}

	va_end(args);
	return NULL;
}

/**
 * Run custom command using Python script.
 * @param op Object running the command.
 * @param params Command parameters.
 * @return 0 on failure, return value of the script otherwise. */
static int cmd_customPython(object *op, char *params)
{
	PythonContext *context = malloc(sizeof(PythonContext));
	int rv;

#ifdef PYTHON_DEBUG
	LOG(llevDebug, "PYTHON:: cmd_customPython called:: script file: %s\n", CustomCommand[NextCustomCommand].script);
#endif

	context->activator = op;
	context->who = op;
	context->other = op;
	context->event = NULL;
	context->parms[0] = 0;
	context->parms[1] = 0;
	context->parms[2] = 0;
	context->parms[3] = 0;
	context->text = params;
	context->options = NULL;
	context->returnvalue = 0;

	pushContext(context);

	if (RunPythonScript(CustomCommand[NextCustomCommand].script, NULL))
	{
		freeContext(context);
		return 0;
	}

	context = popContext();
	rv = context->returnvalue;
	freeContext(context);

#ifdef PYTHON_DEBUG
	LOG(llevDebug, "done (returned: %d)!\n", rv);
#endif

	return rv;
}

MODULEAPI void postinitPlugin()
{
	LOG(llevDebug, "PYTHON:: Start postinitPlugin.\n");
	initContextStack();
	RunPythonScript("python/events/python_init.py", NULL);
}

/**
 * Here is the Python Declaration Table, used by the interpreter to make
 * an interface with the C code. */
static PyMethodDef AtrinikMethods[] =
{
	{"LoadObject",          Atrinik_LoadObject,            METH_VARARGS, 0},
	{"ReadyMap",            Atrinik_ReadyMap,              METH_VARARGS, 0},
	{"CheckMap",            Atrinik_CheckMap,              METH_VARARGS, 0},
	{"MatchString",         Atrinik_MatchString,           METH_VARARGS, 0},
	{"FindPlayer",          Atrinik_FindPlayer,            METH_VARARGS, 0},
	{"PlayerExists",        Atrinik_PlayerExists,          METH_VARARGS, 0},
	{"GetOptions",          Atrinik_GetOptions,            METH_VARARGS, 0},
	{"GetReturnValue",      Atrinik_GetReturnValue,        METH_VARARGS, 0},
	{"SetReturnValue",      Atrinik_SetReturnValue,        METH_VARARGS, 0},
	{"GetSpellNr",          Atrinik_GetSpellNr,            METH_VARARGS, 0},
	{"GetSpell",            Atrinik_GetSpell,              METH_VARARGS, 0},
	{"GetSkillNr",          Atrinik_GetSkillNr,            METH_VARARGS, 0},
	{"WhoAmI",              Atrinik_WhoAmI,                METH_VARARGS, 0},
	{"WhoIsActivator",      Atrinik_WhoIsActivator,        METH_VARARGS, 0},
	{"WhoIsOther",          Atrinik_WhoIsOther,            METH_VARARGS, 0},
	{"WhatIsEvent",         Atrinik_WhatIsEvent,           METH_VARARGS, 0},
	{"GetEventNumber",      Atrinik_GetEventNumber,        METH_VARARGS, 0},
	{"WhatIsMessage",       Atrinik_WhatIsMessage,         METH_VARARGS, 0},
	{"RegisterCommand",     Atrinik_RegisterCommand,       METH_VARARGS, 0},
	{"CreatePathname",      Atrinik_CreatePathname,        METH_VARARGS, 0},
	{"GetTime",             Atrinik_GetTime,               METH_VARARGS, 0},
	{"LocateBeacon",        Atrinik_LocateBeacon,          METH_VARARGS, 0},
	{"FindParty",           Atrinik_FindParty,             METH_VARARGS, 0},
	{"CleanupChatString",   Atrinik_CleanupChatString,     METH_VARARGS, 0},
	{"LOG",                 Atrinik_LOG,                   METH_VARARGS, 0},
	{"DestroyTimer",        Atrinik_DestroyTimer,          METH_VARARGS, 0},
	{"FindFace",            Atrinik_FindFace,              METH_VARARGS, 0},
	{"FindAnimation",       Atrinik_FindAnimation,         METH_VARARGS, 0},
	{"GetEventParameters",  Atrinik_GetEventParameters,    METH_VARARGS, 0},
	{NULL, NULL, 0, 0}
};

#ifdef IS_PY3K
static PyModuleDef AtrinikModule =
{
	PyModuleDef_HEAD_INIT,
	"Atrinik",
	NULL,
	-1,
	AtrinikMethods,
	NULL, NULL, NULL, NULL
};

static PyObject *PyInit_Atrinik()
{
	PyObject *m = PyModule_Create(&AtrinikModule);
	Py_INCREF(m);
	return m;
}
#endif

/**
 * Initializes the Python Interpreter. */
static void init_Atrinik_Python()
{
	PyObject *m, *d;
	int i;

	LOG(llevDebug, "PYTHON:: Start initAtrinik.\n");

#ifdef IS_PY3K
	PyImport_AppendInittab("Atrinik", &PyInit_Atrinik);
#endif

#ifdef IS_PY3K
	m = PyImport_ImportModule("Atrinik");
#else
	m = Py_InitModule("Atrinik", AtrinikMethods);
#endif
	d = PyModule_GetDict(m);
	AtrinikError = PyErr_NewException("Atrinik.error", NULL, NULL);
	PyDict_SetItemString(d, "error", AtrinikError);

	for (i = 0; i < NR_CUSTOM_CMD; i++)
	{
		CustomCommand[i].name = NULL;
		CustomCommand[i].script = NULL;
		CustomCommand[i].speed = 0.0;
	}

	if (!Atrinik_Object_init(m) || !Atrinik_Map_init(m) || !Atrinik_Party_init(m))
	{
		return;
	}

	/* Initialize integer constants */
	for (i = 0; module_constants[i].name; i++)
	{
		PyModule_AddIntConstant(m, module_constants[i].name, module_constants[i].value);
	}
}
