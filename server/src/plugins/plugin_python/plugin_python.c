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

/** A generic exception that we use for error messages */
PyObject *AtrinikError;

/* The execution stack. Altough it is quite rare, a script can actually      */
/* trigger another script. The stack is used to keep track of those multiple */
/* levels of execution. A recursion stack of size 100 shout be sufficient.   */
/* If for some reason you think it is not enough, simply increase its size.  */
/* The code will still work, but the plugin will eat more memory.            */
#define MAX_RECURSIVE_CALL 100
int StackPosition = 0;
object* StackActivator[MAX_RECURSIVE_CALL];
object* StackWho[MAX_RECURSIVE_CALL];
object* StackOther[MAX_RECURSIVE_CALL];
char* StackText[MAX_RECURSIVE_CALL];
int StackParm1[MAX_RECURSIVE_CALL];
int StackParm2[MAX_RECURSIVE_CALL];
int StackParm3[MAX_RECURSIVE_CALL];
int StackParm4[MAX_RECURSIVE_CALL];
int StackReturn[MAX_RECURSIVE_CALL];
char* StackOptions[MAX_RECURSIVE_CALL];

/**
 * @anchor plugin_python_constants
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
	{NULL, 0}
};

/** All the custom commands. */
PythonCmd CustomCommand[NR_CUSTOM_CMD];
/** Contains the index of the next command that needs to be run. */
int NextCustomCommand;

/* Stuff for python bytecode cache */
#define PYTHON_CACHE_SIZE 10

typedef struct
{
	const char *file;
	PyCodeObject *code;
	time_t cached_time, used_time;
} cacheentry;

static cacheentry python_cache[PYTHON_CACHE_SIZE];
static int RunPythonScript(const char *path, object *event_object);

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

	if (!PyArg_ParseTuple(args, "", NULL))
	{
		return NULL;
	}

	return wrap_object(StackWho[StackPosition]);
}

/**
 * <h1>Atrinik.WhoIsActivator()</h1>
 * Get the object that activated the current event.
 * @return The script activator. */
static PyObject *Atrinik_WhoIsActivator(PyObject *self, PyObject *args)
{
	(void) self;

	if (!PyArg_ParseTuple(args, "", NULL))
	{
		return NULL;
	}

	return wrap_object(StackActivator[StackPosition]);
}

/**
 * <h1>Atrinik.WhoIsOther()</h1>
 * @warning Untested. */
static PyObject *Atrinik_WhoIsOther(PyObject *self, PyObject *args)
{
	(void) self;

	if (!PyArg_ParseTuple(args, "", NULL))
	{
		return NULL;
	}

	return wrap_object(StackOther[StackPosition]);
}

/**
 * <h1>Atrinik.WhatIsMessage()</h1>
 * Gets the actual message in SAY events.
 * @return The message. */
static PyObject *Atrinik_WhatIsMessage(PyObject *self, PyObject *args)
{
	(void) self;

	if (!PyArg_ParseTuple(args, "", NULL))
	{
		return NULL;
	}

	return Py_BuildValue("s", StackText[StackPosition]);
}

/**
 * <h1>Atrinik.GetOptions()</h1>
 * Gets the script options (as passed in the event's slaying field).
 * @return The script options. */
static PyObject *Atrinik_GetOptions(PyObject *self, PyObject *args)
{
	(void) self;

	if (!PyArg_ParseTuple(args, "", NULL))
	{
		return NULL;
	}

	return Py_BuildValue("s", StackOptions[StackPosition]);
}

/**
 * <h1>Atrinik.GetReturnValue()</h1>
 * Gets the script's return value.
 * @return The return value */
static PyObject *Atrinik_GetReturnValue(PyObject *self, PyObject *args)
{
	(void) self;

	if (!PyArg_ParseTuple(args, "", NULL))
	{
		return NULL;
	}

	return Py_BuildValue("i", StackReturn[StackPosition]);
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

	StackReturn[StackPosition] = value;

	Py_INCREF(Py_None);
	return Py_None;
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

/*@}*/

MODULEAPI void *triggerEvent(int *type, ...)
{
	va_list args;
	int eventcode;
	static int result = 0;

	va_start(args, type);
	eventcode = va_arg(args, int);
	LOG(llevDebug, "PYTHON - triggerEvent:: eventcode %d\n", eventcode);

	switch (eventcode)
	{
		case EVENT_NONE:
			LOG(llevDebug, "PYTHON - Warning - EVENT_NONE requested\n");
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

MODULEAPI int HandleGlobalEvent(int event_type, va_list args)
{
	if (StackPosition == MAX_RECURSIVE_CALL)
	{
		LOG(llevDebug, "Can't execute script - No space left of stack\n");
		return 0;
	}

	StackPosition++;

	switch (event_type)
	{
		case EVENT_CRASH:
			LOG(llevDebug, "Unimplemented for now\n");
			break;

		case EVENT_BORN:
			StackActivator[StackPosition] = (object *) va_arg(args, void *);
			RunPythonScript("python/python_born.py", NULL);
			break;

		case EVENT_LOGIN:
			StackActivator[StackPosition] = ((player *) va_arg(args, void *))->ob;
			StackWho[StackPosition] = ((player *) va_arg(args, void *))->ob;
			StackText[StackPosition] = (char *) va_arg(args, void *);
			RunPythonScript("python/python_login.py", NULL);
			break;

		case EVENT_LOGOUT:
			StackActivator[StackPosition] = ((player *) va_arg(args, void *))->ob;
			StackWho[StackPosition] = ((player *) va_arg(args, void *))->ob;
			StackText[StackPosition] = (char *) va_arg(args, void *);
			RunPythonScript("python/python_logout.py", NULL);
			break;

		case EVENT_REMOVE:
			StackActivator[StackPosition] = (object *) va_arg(args, void *);
			RunPythonScript("python/python_remove.py", NULL);
			break;

		case EVENT_SHOUT:
			StackActivator[StackPosition] = (object *) va_arg(args, void *);
			StackText[StackPosition] = (char *) va_arg(args, void *);
			RunPythonScript("python/python_shout.py", NULL);
			break;

		case EVENT_MAPENTER:
			StackActivator[StackPosition] = (object *) va_arg(args, void *);
			RunPythonScript("python/python_mapenter.py", NULL);
			break;

		case EVENT_MAPLEAVE:
			StackActivator[StackPosition] = (object *) va_arg(args, void *);
			RunPythonScript("python/python_mapleave.py", NULL);
			break;

		case EVENT_CLOCK:
			RunPythonScript("python/python_clock.py", NULL);
			break;

		case EVENT_MAPRESET:
			StackText[StackPosition] = (char *) va_arg(args, void *);
			RunPythonScript("python/python_mapreset.py", NULL);
			break;
	}

	StackPosition--;
	return 0;
}

/**
 * Open a Python file. */
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
 * Return a file object from a Python file. */
static FILE *python_pyfile_asfile(PyObject* obj)
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
 * @return  */
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

	/* TODO: figure out how to get from server */
	/* 1 for timestamp checking and error messages */
	int maintenance_mode = 1;

	if (maintenance_mode)
	{
		if (!(scriptfile = python_openfile(fullpath)))
		{
			LOG(llevDebug, "PYTHON - The Script file %s can't be opened\n", path);
			return -1;
		}

		if (stat(fullpath, &stat_buf))
		{
			LOG(llevDebug, "PYTHON - The Script file %s can't be stat:ed\n", fullpath);

			if (scriptfile)
			{
				Py_DECREF(scriptfile);
			}

			return -1;
		}
	}

	/* Create a shared string */
	FREE_AND_COPY_HASH(sh_path, fullpath);

	/* Search through cache. Three cases:
	 * 1) script in cache, but older than file  -> replace cached (only in maintenance mode)
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
			/* case 3 */
			replace = &python_cache[i];
			break;
		}
		else if (python_cache[i].file == sh_path)
		{
			/* Found it. Compare timestamps. */
			if (python_cache[i].code == NULL || (maintenance_mode && python_cache[i].cached_time < stat_buf.st_mtime))
			{
#ifdef PYTHON_DEBUG
				LOG(llevDebug, "PYTHON:: File newer than cached bytecode -> reloading\n");
#endif
				/* case 1 */
				replace = &python_cache[i];
			}
			else
			{
#ifdef PYTHON_DEBUG
				LOG(llevDebug, "PYTHON:: Using cached version\n");
#endif
				/* case 2 */
				replace = NULL;
				run = &python_cache[i];
			}
			break;
		}
		/* prepare for case 4 */
		else if (replace == NULL || python_cache[i].used_time < replace->used_time)
			replace = &python_cache[i];
	}

	/* replace a specific cache index with the file */
	if (replace)
	{
		/* safe to call on NULL */
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
		if (!scriptfile && !(scriptfile = python_openfile(fullpath)))
		{
			LOG(llevDebug, "PYTHON - The Script file %s can't be opened\n", path);
			replace->code = NULL;
		}
		else
		{
			FILE *pyfile = python_pyfile_asfile(scriptfile);

			if ((n = PyParser_SimpleParseFile(pyfile, fullpath, Py_file_input)))
			{
				replace->code = PyNode_Compile(n, fullpath);
				PyNode_Free(n);
			}

			if (maintenance_mode)
			{
				if (PyErr_Occurred())
				{
					PyErr_Print();
				}
				else
				{
					replace->cached_time = stat_buf.st_mtime;
				}
			}

			run = replace;
		}
	}

	/* run an old or new code object */
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
			if (maintenance_mode)
			{
				PyErr_Print();
			}
		}
		else
		{
			/* only return 0 if we actually succeeded */
			result = 0;
			run->used_time = time(NULL);
		}

#ifdef PYTHON_DEBUG
		LOG(llevDebug, "closing (%d). ", StackPosition);
#endif
		Py_DECREF(globdict);
	}

	FREE_AND_CLEAR_HASH(sh_path);

	if (scriptfile)
	{
		Py_DECREF(scriptfile);
	}

	return result;
}


/*****************************************************************************/
/* Handles standard local events.                                            */
/*****************************************************************************/
MODULEAPI int HandleEvent(va_list args)
{
	char *script;

	if (StackPosition == MAX_RECURSIVE_CALL)
	{
		LOG(llevDebug, "PYTHON - Can't execute script - No space left on stack\n");
		return 0;
	}

	StackPosition++;
	StackActivator[StackPosition] = va_arg(args, object *);
	StackWho[StackPosition] = va_arg(args, object *);
	StackOther[StackPosition] = va_arg(args, object *);
	StackText[StackPosition] = va_arg(args, char *);
	StackParm1[StackPosition] = va_arg(args, int);
	StackParm2[StackPosition] = va_arg(args, int);
	StackParm3[StackPosition] = va_arg(args, int);
	StackParm4[StackPosition] = va_arg(args, int);
	script = va_arg(args, char *);
	StackOptions[StackPosition] = va_arg(args, char *);
	StackReturn[StackPosition] = 0;

#ifdef PYTHON_DEBUG
	LOG(llevDebug, "PYTHON - Ctart script file >%s<\n", script);
	LOG(llevDebug, "PYTHON - Call data: o1:>%s< o2:>%s< o3:>%s< text:>%s< i1:%d i2:%d i3:%d i4:%d SP:%d\n", STRING_OBJ_NAME(StackActivator[StackPosition]), STRING_OBJ_NAME(StackWho[StackPosition]), STRING_OBJ_NAME(StackOther[StackPosition]), STRING_SAFE(StackText[StackPosition]), StackParm1[StackPosition], StackParm2[StackPosition], StackParm3[StackPosition], StackParm4[StackPosition], StackPosition);
#endif

	if (RunPythonScript(script, StackWho[StackPosition]))
	{
		StackPosition--;
		return 0;
	}

#ifdef PYTHON_DEBUG
	LOG(llevDebug, "fixing. ");
#endif

	if (StackParm4[StackPosition] == SCRIPT_FIX_ALL)
	{
		if (StackOther[StackPosition] != NULL)
			hooks->fix_player(StackOther[StackPosition]);

		if (StackWho[StackPosition] != NULL)
			hooks->fix_player(StackWho[StackPosition]);

		if (StackActivator[StackPosition] != NULL)
			hooks->fix_player(StackActivator[StackPosition]);
	}
	else if (StackParm4[StackPosition] == SCRIPT_FIX_ACTIVATOR)
	{
		hooks->fix_player(StackActivator[StackPosition]);
	}

#ifdef PYTHON_DEBUG
	LOG(llevDebug, "done (returned: %d)!\n", StackReturn[StackPosition]);
#endif

	return StackReturn[StackPosition--];
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

MODULEAPI int cmd_customPython(object *op, char *params)
{
#ifdef PYTHON_DEBUG
	LOG(llevDebug, "PYTHON - cmd_customPython called:: script file: %s\n", CustomCommand[NextCustomCommand].script);
#endif

	if (StackPosition == MAX_RECURSIVE_CALL)
	{
		LOG(llevDebug, "PYTHON - Can't execute script - No space left of stack\n");
		return 0;
	}

	StackPosition++;
	StackActivator[StackPosition] = op;
	StackWho[StackPosition] = op;
	StackOther[StackPosition] = op;
	StackText[StackPosition] = params;
	StackReturn[StackPosition] = 0;

	RunPythonScript(CustomCommand[NextCustomCommand].script, NULL);

	LOG(llevDebug, "done (returned: %d)!\n", StackReturn[StackPosition]);

	return StackReturn[StackPosition--];
}

/*****************************************************************************/
/* The postinitPlugin function is called by the server when the plugin load  */
/* is complete. It lets the opportunity to the plugin to register some events*/
/*****************************************************************************/
MODULEAPI void postinitPlugin()
{
	struct timeval new_time;

	(void) GETTIMEOFDAY(&new_time);

	LOG(llevDebug, "PYTHON - Start postinitPlugin.\n");
	RunPythonScript("python/events/python_init.py", NULL);
}

/**
 * Here is the Python Declaration Table, used by the interpreter to make
 * an interface with the C code. */
static PyMethodDef AtrinikMethods[] =
{
	{"LoadObject",       Atrinik_LoadObject,          METH_VARARGS, 0},
	{"ReadyMap",         Atrinik_ReadyMap,            METH_VARARGS, 0},
	{"CheckMap",         Atrinik_CheckMap,            METH_VARARGS, 0},
	{"MatchString",      Atrinik_MatchString,         METH_VARARGS, 0},
	{"FindPlayer",       Atrinik_FindPlayer,          METH_VARARGS, 0},
	{"PlayerExists",     Atrinik_PlayerExists,        METH_VARARGS, 0},
	{"GetOptions",       Atrinik_GetOptions,          METH_VARARGS, 0},
	{"GetReturnValue",   Atrinik_GetReturnValue,      METH_VARARGS, 0},
	{"SetReturnValue",   Atrinik_SetReturnValue,      METH_VARARGS, 0},
	{"GetSpellNr",       Atrinik_GetSpellNr,          METH_VARARGS, 0},
	{"GetSpell",         Atrinik_GetSpell,            METH_VARARGS, 0},
	{"GetSkillNr",       Atrinik_GetSkillNr,          METH_VARARGS, 0},
	{"WhoAmI",           Atrinik_WhoAmI,              METH_VARARGS, 0},
	{"WhoIsActivator",   Atrinik_WhoIsActivator,      METH_VARARGS, 0},
	{"WhoIsOther",       Atrinik_WhoIsOther,          METH_VARARGS, 0},
	{"WhatIsMessage",    Atrinik_WhatIsMessage,       METH_VARARGS, 0},
	{"RegisterCommand",  Atrinik_RegisterCommand,     METH_VARARGS, 0},
	{"CreatePathname",   Atrinik_CreatePathname,      METH_VARARGS, 0},
	{"GetTime",          Atrinik_GetTime,             METH_VARARGS, 0},
	{"LocateBeacon",     Atrinik_LocateBeacon,        METH_VARARGS, 0},
	{"FindParty",        Atrinik_FindParty,           METH_VARARGS, 0},
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
MODULEAPI void init_Atrinik_Python()
{
	PyObject *m, *d;
	int i;

	LOG(llevDebug, "PYTHON - Start initAtrinik.\n");

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

	/* Initialize our objects */
	/* TODO: some better error handling */
	if (Atrinik_Object_init(m) || Atrinik_Map_init(m) || Atrinik_Party_init(m))
		return;

	/* Initialize direction constants */
	/* Gecko: TODO: error handling here */
	for (i = 0; module_constants[i].name; i++)
		PyModule_AddIntConstant(m, module_constants[i].name, module_constants[i].value);
}
