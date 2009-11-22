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
 * Atrinik python plugin. */

/*****************************************************************************/
/* Atrinik - A Python module for Atrinik RPG.                                */
/*****************************************************************************/
/* The goal of this module is to provide support for Python scripts into     */
/* Atrinik. Python is here used in a extended way as a generic plugin.       */
/* Thats not a fast way to use this - but extrem flexible (we can load/      */
/* change and test script over and over in a running server) and easy to     */
/* extend - we simply don't add somewhere code in the server except some     */
/* jump points for the plugin model - if we want change someone the script   */
/* language or add another - it will not change anything in the plugin       */
/* interface.                                                                */
/*****************************************************************************/
/* Please note that it is still very beta - some of the functions may not    */
/* work as expected and could even cause the server to crash.                */
/*****************************************************************************/
/* Version history:                                                          */
/* 0.1 "Ophiuchus"   - Initial Alpha release                                 */
/* 0.5 "Stalingrad"  - Message length overflow corrected.                    */
/* 0.6 "Kharkov"     - Message and Write correctly redefined.                */
/* 0.x "xxx"         - Work in progress                                      */
/*****************************************************************************/
/* Version: 0.6 Beta (also known as "Kharkov")                               */
/*****************************************************************************/
/* That code is placed under the GNU General Public Licence (GPL)            */
/* (C)2001 by Chachkoff Yann (Feel free to deliver your complaints)          */
/*****************************************************************************/

/* First let's include the header file needed                                */

#include <plugin_python.h>
#include <inline.h>
#include <plugin_common.h>

#include <compile.h>
#include <eval.h>
#ifdef STR
/* STR is redefined in node.h. Since this file doesn't use STR, we remove it */
#undef STR
#endif
#include <node.h>

/* Global data objects */

/* The plugin properties and hook functions. A hook function is a pointer to  */
/* a CF function wrapper. Basically, most CF functions that could be of any   */
/* use to the plugin have "wrappers", functions that all have the same args   */
/* and all returns the same type of data (CFParm); pointers to those functs.  */
/* are passed to the plugin when it is initialized. They are what I called    */
/* "Hooks". It means that using any server function is slower from a plugin   */
/* than it is from the "inside" of the server code, because all arguments     */
/* need to be passed back and forth in a CFParm structure, but the difference */
/* is not a problem, unless for time-critical code sections. Using such hooks */
/* may of course sound complicated, but it allows much greater flexibility.   */
CFParm* PlugProps;
f_plugin PlugHooks[1024];

/* A generic exception that we use for error messages */
PyObject *AtrinikError;

/* This one is used to cleanly pass args to the Atrinik core */
CFParm GCFP;
CFParm GCFP0;
CFParm GCFP1;
CFParm GCFP2;

/* Atrinik methods */
static PyObject *Atrinik_MatchString(PyObject *self, PyObject *args);
static PyObject *Atrinik_ReadyMap(PyObject *self, PyObject *args);
static PyObject *Atrinik_FindPlayer(PyObject *self, PyObject *args);
static PyObject *Atrinik_PlayerExists(PyObject *self, PyObject *args);
static PyObject *Atrinik_WhoAmI(PyObject *self, PyObject *args);
static PyObject *Atrinik_WhoIsActivator(PyObject *self, PyObject *args);
static PyObject *Atrinik_WhatIsMessage(PyObject *self, PyObject *args);
static PyObject *Atrinik_WhoIsOther(PyObject *self, PyObject *args);
static PyObject *Atrinik_GetOptions(PyObject *self, PyObject *args);
static PyObject *Atrinik_GetSpellNr(PyObject *self, PyObject *args);
static PyObject *Atrinik_GetSkillNr(PyObject *self, PyObject *args);
static PyObject *Atrinik_CheckMap(PyObject *self, PyObject *args);
static PyObject *Atrinik_RegisterCommand(PyObject *self, PyObject *args);
static PyObject *Atrinik_LoadObject(PyObject *self, PyObject *args);
static PyObject *Atrinik_GetReturnValue(PyObject *self, PyObject *args);
static PyObject *Atrinik_SetReturnValue(PyObject *self, PyObject *args);
static PyObject *Atrinik_CreatePathname(PyObject *self, PyObject *args);
static PyObject *Atrinik_GetTime(PyObject *self, PyObject *args);

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
	{"GetSkillNr",       Atrinik_GetSkillNr,          METH_VARARGS, 0},
	{"WhoAmI",           Atrinik_WhoAmI,              METH_VARARGS, 0},
	{"WhoIsActivator",   Atrinik_WhoIsActivator,      METH_VARARGS, 0},
	{"WhoIsOther",       Atrinik_WhoIsOther,          METH_VARARGS, 0},
	{"WhatIsMessage",    Atrinik_WhatIsMessage,       METH_VARARGS, 0},
	{"RegisterCommand",  Atrinik_RegisterCommand,     METH_VARARGS, 0},
	{"CreatePathname",   Atrinik_CreatePathname,      METH_VARARGS, 0},
	{"GetTime",          Atrinik_GetTime,             METH_VARARGS, 0},
	{NULL, NULL, 0, 0}
};

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

/* Commands management part */
PythonCmd CustomCommand[NR_CUSTOM_CMD];
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
 * @warning Untested. */
static PyObject *Atrinik_LoadObject(PyObject *self, PyObject *args)
{
	object *whoptr;
	char *dumpob;
	CFParm *CFR;

	(void) self;

	if (!PyArg_ParseTuple(args, "s", &dumpob))
	{
		return NULL;
	}

	/* First step: We create the object */
	GCFP.Value[0] = (void *) (dumpob);
	CFR = (PlugHooks[HOOK_LOADOBJECT])(&GCFP);
	whoptr = (object *) (CFR->Value[0]);

	free(CFR);

	return wrap_object(whoptr);
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

	return Py_BuildValue("i", (re_cmp(premiere, seconde) != NULL) ? 1 : 0);
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
	mapstruct *mymap;
	int flags = 0, unique = 0;
	CFParm *CFR;

	(void) self;

	if (!PyArg_ParseTuple(args, "s|i", &mapname, &unique))
	{
		return NULL;
	}

	if (unique)
	{
		flags = MAP_PLAYER_UNIQUE;
	}

	GCFP.Value[0] = (void *) (mapname);
	GCFP.Value[1] = (void *) (&flags);

	CFR = (PlugHooks[HOOK_READYMAPNAME])(&GCFP);
	mymap = (mapstruct *) (CFR->Value[0]);

	if (mymap != NULL)
	{
		plugin_log(llevDebug, "Map file is %s\n", mymap->path);
	}

	free(CFR);

	return wrap_map(mymap);
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
	CFParm *CFR;
	char *txt;

	(void) self;

	if (!PyArg_ParseTuple(args, "s", &txt))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (txt);
	CFR = (PlugHooks[HOOK_FINDPLAYER])(&GCFP);
	foundpl = (player *) (CFR->Value[0]);
	free(CFR);

	if (foundpl != NULL)
	{
		foundob = foundpl->ob;
	}

	return wrap_object(foundob);
}

/**
 * <h1>Atrinik.PlayerExists(<i>\<string\></i> name)</h1>
 * Check if player exists in the database.
 *
 * @param name The player name
 * @return 1 if the player exists, 0 otherwise */
static PyObject *Atrinik_PlayerExists(PyObject *self, PyObject *args)
{
	char *playerName;
	CFParm *CFR;
	int value;

	(void) self;

	if (!PyArg_ParseTuple(args, "s", &playerName))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (playerName);
	CFR = (PlugHooks[HOOK_PLAYEREXISTS])(&GCFP);
	value = *(int *) (CFR->Value[0]);

	free(CFR);

	return Py_BuildValue("i", value);
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
	CFParm *CFR;
	int value;

	(void) self;

	if (!PyArg_ParseTuple(args, "s", &spell))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (spell);
	CFR = (PlugHooks[HOOK_CHECKFORSPELLNAME])(&GCFP);

	value = *(int *) (CFR->Value[0]);

	return Py_BuildValue("i", value);
}

/**
 * <h1>Atrinik.GetSkillNr(<i>\<string\></i> name)</h1>
 * Gets the number of the named skill.
 * @param name The skill name
 * @return Number of the skill, -1 if no such skill exists. */
static PyObject *Atrinik_GetSkillNr(PyObject *self, PyObject *args)
{
	char *skill;
	CFParm *CFR;
	int value;

	(void) self;

	if (!PyArg_ParseTuple(args, "s", &skill))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (skill);
	CFR = (PlugHooks[HOOK_CHECKFORSKILLNAME])(&GCFP);

	value = *(int *) (CFR->Value[0]);

	return Py_BuildValue("i", value);
}

/**
 * <h1>Atrinik.RegisterCommand(<i>\<string\></i> cmdname,
 * <i>\<string\></i> scriptname, <i>\<double\></i> speed)</h1>
 * @warning Untested. */
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
				plugin_log(llevDebug, "PYTHON:: This command is already registered!\n");
				RAISE("This command is already registered");
			}
		}
	}

	for (i = 0; i < NR_CUSTOM_CMD; i++)
	{
		if (CustomCommand[i].name == NULL)
		{
			CustomCommand[i].name = (char *) (malloc(sizeof(char) * strlen(cmdname)));
			CustomCommand[i].script = (char *) (malloc(sizeof(char) * strlen(scriptname)));
			strcpy(CustomCommand[i].name, cmdname);
			strcpy(CustomCommand[i].script, scriptname);
			CustomCommand[i].speed = cmdspeed;
			i = NR_CUSTOM_CMD;
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

	return Py_BuildValue("s", create_pathname(path));
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
	CFParm *CFR;
	timeofday_t tod;

	(void) self;
	(void) args;

	CFR = (PlugHooks[HOOK_GETTOD])(&GCFP);
	tod = *(timeofday_t *) (CFR->Value[0]);

	PyDict_SetItemString(dict, "year", Py_BuildValue("i", tod.year + 1));
	PyDict_SetItemString(dict, "month", Py_BuildValue("i", tod.month + 1));
	PyDict_SetItemString(dict, "month_name", Py_BuildValue("s", month_name[tod.month]));
	PyDict_SetItemString(dict, "day", Py_BuildValue("i", tod.day + 1));
	PyDict_SetItemString(dict, "hour", Py_BuildValue("i", tod.hour));
	PyDict_SetItemString(dict, "minute", Py_BuildValue("i", tod.minute + 1));
	PyDict_SetItemString(dict, "dayofweek", Py_BuildValue("i", tod.dayofweek + 1));
	PyDict_SetItemString(dict, "dayofweek_name", Py_BuildValue("s", weekdays[tod.dayofweek]));
	PyDict_SetItemString(dict, "season", Py_BuildValue("i", tod.season + 1));
	PyDict_SetItemString(dict, "season_name", Py_BuildValue("s", season_name[tod.season]));
	PyDict_SetItemString(dict, "periodofday", Py_BuildValue("i", tod.periodofday + 1));
	PyDict_SetItemString(dict, "periodofday_name", Py_BuildValue("s", periodsofday[tod.periodofday]));

	return dict;
}

/*@}*/

/*****************************************************************************/
/* The Plugin Management Part.                                               */
/* Most of the functions below should exist in any CF plugin. They are used  */
/* to glue the plugin to the server core. All functions follow the same      */
/* declaration scheme (taking a CFParm* arg, returning a CFParm) to make the */
/* plugin interface as general as possible. And since the loading of modules */
/* isn't time-critical, it is never a problem. It could also make using      */
/* programming languages other than C to write plugins a little easier, but  */
/* this has yet to be proven.                                                */
/*****************************************************************************/

/*****************************************************************************/
/* Called whenever a Hook Function needs to be connected to the plugin.      */
/*****************************************************************************/
MODULEAPI CFParm* registerHook(CFParm* PParm)
{
	int Pos;
	f_plugin Hook;

	Pos = *(int*)(PParm->Value[0]);
	Hook = (f_plugin)(PParm->Value[1]);
	PlugHooks[Pos] = Hook;
	return NULL;
}

/*****************************************************************************/
/* Called whenever an event is triggered, both Local and Global ones.        */
/*****************************************************************************/
/* Two types of events exist in CF:                                          */
/* - Local events: They are triggered by a single action on a single object. */
/*                 Nearly any object can trigger a local event               */
/*                 To warn the plugin of a local event, the map-maker needs  */
/*                 to use the event... tags in the objects of their maps.    */
/* - Global events: Those are triggered by actions concerning CF as a whole. */
/*                 Those events may or may not be triggered by a particular  */
/*                 object; they can't be specified by event... tags in maps. */
/*                 The plugin should register itself for all global events it*/
/*                 wants to be aware of.                                     */
/* Why those two types ? Local Events are made to manage interactions between*/
/* objects, for example to create complex scenarios. Global Events are made  */
/* to allow logging facilities and server management. Global Events tends to */
/* require more CPU time than Local Events, and are sometimes difficult to   */
/* bind to any specific object.                                              */
/*****************************************************************************/
MODULEAPI CFParm* triggerEvent(CFParm* PParm)
{
	/*CFParm *CFP; */
	int eventcode;
	static int result;

	eventcode = *(int *)(PParm->Value[0]);
	plugin_log(llevDebug, "PYTHON - triggerEvent:: eventcode %d\n", eventcode);

	switch (eventcode)
	{
		case EVENT_NONE:
			plugin_log(llevDebug, "PYTHON - Warning - EVENT_NONE requested\n");
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
			result = HandleEvent(PParm);
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
			result = HandleGlobalEvent(PParm);
			break;
	}

	GCFP.Value[0] = (void *)(&result);
	return &GCFP;
}

/*****************************************************************************/
/* Handles standard global events.                                           */
/*****************************************************************************/
MODULEAPI int HandleGlobalEvent(CFParm* PParm)
{
	if (StackPosition == MAX_RECURSIVE_CALL)
	{
		plugin_log(llevDebug, "Can't execute script - No space left of stack\n");
		return 0;
	}

	StackPosition++;

	switch (*(int *)(PParm->Value[0]))
	{
		case EVENT_CRASH:
			plugin_log(llevDebug, "Unimplemented for now\n");
			break;

		case EVENT_BORN:
			StackActivator[StackPosition] = (object *)(PParm->Value[1]);
			/*plugin_log(llevDebug, "Event BORN generated by %s\n",query_name(StackActivator[StackPosition])); */
			RunPythonScript("python/python_born.py", NULL);
			break;

		case EVENT_LOGIN:
			StackActivator[StackPosition] = ((player *)(PParm->Value[1]))->ob;
			StackWho[StackPosition] = ((player *)(PParm->Value[1]))->ob;
			StackText[StackPosition] = (char *)(PParm->Value[2]);
			/*plugin_log(llevDebug, "Event LOGIN generated by %s\n",query_name(StackActivator[StackPosition])); */
			/*plugin_log(llevDebug, "IP is %s\n", (char *)(PParm->Value[2])); */
			RunPythonScript("python/python_login.py", NULL);
			break;

		case EVENT_LOGOUT:
			StackActivator[StackPosition] = ((player *)(PParm->Value[1]))->ob;
			StackWho[StackPosition] = ((player *)(PParm->Value[1]))->ob;
			StackText[StackPosition] = (char *)(PParm->Value[2]);
			/*plugin_log(llevDebug, "Event LOGOUT generated by %s\n",query_name(StackActivator[StackPosition])); */
			RunPythonScript("python/python_logout.py", NULL);
			break;

		case EVENT_REMOVE:
			StackActivator[StackPosition] = (object *)(PParm->Value[1]);
			/*plugin_log(llevDebug, "Event REMOVE generated by %s\n",query_name(StackActivator[StackPosition])); */
			RunPythonScript("python/python_remove.py", NULL);
			break;

		case EVENT_SHOUT:
			StackActivator[StackPosition] = (object *)(PParm->Value[1]);
			StackText[StackPosition] = (char *)(PParm->Value[2]);
			/*plugin_log(llevDebug, "Event SHOUT generated by %s\n",query_name(StackActivator[StackPosition])); */
			/*plugin_log(llevDebug, "Message shout is %s\n",StackText[StackPosition]); */
			RunPythonScript("python/python_shout.py", NULL);
			break;

		case EVENT_MAPENTER:
			StackActivator[StackPosition] = (object *)(PParm->Value[1]);
			/*plugin_log(llevDebug, "Event MAPENTER generated by %s\n",query_name(StackActivator[StackPosition])); */
			RunPythonScript("python/python_mapenter.py", NULL);
			break;

		case EVENT_MAPLEAVE:
			StackActivator[StackPosition] = (object *)(PParm->Value[1]);
			/*plugin_log(llevDebug, "Event MAPLEAVE generated by %s\n",query_name(StackActivator[StackPosition])); */
			RunPythonScript("python/python_mapleave.py", NULL);
			break;

		case EVENT_CLOCK:
			/*plugin_log(llevDebug, "Event CLOCK generated\n"); */
			RunPythonScript("python/python_clock.py", NULL);
			break;

		case EVENT_MAPRESET:
			StackText[StackPosition] = (char *)(PParm->Value[1]);/* Map name/path */
			plugin_log(llevDebug, "Event MAPRESET generated by %s\n", StackText[StackPosition]);
			RunPythonScript("python/python_mapreset.py", NULL);
			break;
	}

	StackPosition--;
	return 0;
}

/********************************************************************/
/* Execute a script, handling loading, parsing and caching          */
/********************************************************************/
static int RunPythonScript(const char *path, object *event_object)
{
	PyObject *scriptfile;
	char *fullpath = create_pathname(path);
	const char *sh_path;
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
			normalize_path(outermost->map->path, path, tmp_path);

			fullpath = create_pathname(tmp_path);
		}
	}

	/* TODO: figure out how to get from server */
	/* 1 for timestamp checking and error messages */
	int maintenance_mode = 1;

	if (maintenance_mode)
	{
		if (!(scriptfile = PyFile_FromString(fullpath, "r")))
		{
			plugin_log(llevDebug, "PYTHON - The Script file %s can't be opened\n", path);
			return -1;
		}

		if (stat(fullpath, &stat_buf))
		{
			plugin_log(llevDebug, "PYTHON - The Script file %s can't be stat:ed\n", fullpath);

			if (scriptfile)
			{
				Py_DECREF(scriptfile);
			}

			return -1;
		}
	}

	/* Create a shared string */
	sh_path = add_string_hook(fullpath);

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
			plugin_log(llevDebug, "PYTHON:: Adding file to cache\n");
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
				plugin_log(llevDebug, "PYTHON:: File newer than cached bytecode -> reloading\n");
#endif
				/* case 1 */
				replace = &python_cache[i];
			}
			else
			{
#ifdef PYTHON_DEBUG
				plugin_log(llevDebug, "PYTHON:: Using cached version\n");
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
				plugin_log(llevDebug, "PYTHON:: Purging %s (cache index %d): \n", replace->file, replace - python_cache);
#endif
				free_string_hook(replace->file);
			}

			replace->file = add_string_hook(sh_path);
			/* TODO: would get minor speedup with add_ref_hook() */
		}

		/* Load, parse and compile */
#ifdef PYTHON_DEBUG
		plugin_log(llevDebug, "PYTHON:: Parse and compile (cache index %d): \n", replace - python_cache);
#endif
		if (!scriptfile && !(scriptfile = PyFile_FromString(fullpath, "r")))
		{
			plugin_log(llevDebug, "PYTHON - The Script file %s can't be opened\n", path);
			replace->code = NULL;
		}
		else
		{
			FILE *pyfile = PyFile_AsFile(scriptfile);

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
		plugin_log(llevDebug, "PYTHON:: PyEval_EvalCode (cache index %d): \n", run - python_cache);
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
		plugin_log(llevDebug, "closing (%d). ", StackPosition);
#endif
		Py_DECREF(globdict);
	}

	free_string_hook(sh_path);

	if (scriptfile)
	{
		Py_DECREF(scriptfile);
	}

	return result;
}


/*****************************************************************************/
/* Handles standard local events.                                            */
/*****************************************************************************/
MODULEAPI int HandleEvent(CFParm* PParm)
{
#ifdef PYTHON_DEBUG
	plugin_log(llevDebug, "PYTHON - HandleEvent:: start script file >%s<\n",(char *)(PParm->Value[9]));
	plugin_log(llevDebug, "PYTHON - call data:: o1:>%s< o2:>%s< o3:>%s< text:>%s< i1:%d i2:%d i3:%d i4:%d SP:%d\n", STRING_OBJ_NAME((object *)(PParm->Value[1])), STRING_OBJ_NAME((object *)(PParm->Value[2])), STRING_OBJ_NAME((object *)(PParm->Value[3])), STRING_SAFE((char *)(PParm->Value[4])), PParm->Value[5] ? *(int *)(PParm->Value[5]) : 0, PParm->Value[6] ? *(int *)(PParm->Value[6]) : 0, PParm->Value[7] ? *(int *)(PParm->Value[7]) : 0, PParm->Value[8] ? *(int *)(PParm->Value[8]) : 0, StackPosition);
#endif

	if (StackPosition == MAX_RECURSIVE_CALL)
	{
		plugin_log(llevDebug, "PYTHON - Can't execute script - No space left of stack\n");
		return 0;
	}

	StackPosition++;
	StackActivator[StackPosition] = (object *)(PParm->Value[1]);
	StackWho[StackPosition] = (object *)(PParm->Value[2]);
	StackOther[StackPosition] = (object *)(PParm->Value[3]);
	StackText[StackPosition] = PParm->Value[4] ? (char *)(PParm->Value[4]) : NULL;
	StackParm1[StackPosition] = PParm->Value[5] ? *(int *)(PParm->Value[5]) : 0;
	StackParm2[StackPosition] = PParm->Value[6] ? *(int *)(PParm->Value[6]) : 0;
	StackParm3[StackPosition] = PParm->Value[7] ? *(int *)(PParm->Value[7]) : 0;
	StackParm4[StackPosition] = PParm->Value[8] ? *(int *)(PParm->Value[8]) : 0;
	StackOptions[StackPosition] = PParm->Value[10] ? (char *)(PParm->Value[10]) : NULL;
	StackReturn[StackPosition] = 0;

	if (RunPythonScript((char *)(PParm->Value[9]), (object *)(PParm->Value[2])))
	{
		StackPosition--;
		return 0;
	}

#ifdef PYTHON_DEBUG
	plugin_log(llevDebug, "fixing. ");
#endif

	if (StackParm4[StackPosition] == SCRIPT_FIX_ALL)
	{
		if (StackOther[StackPosition] != NULL)
			fix_player_hook(StackOther[StackPosition]);

		if (StackWho[StackPosition] != NULL)
			fix_player_hook(StackWho[StackPosition]);

		if (StackActivator[StackPosition] != NULL)
			fix_player_hook(StackActivator[StackPosition]);
	}
	else if (StackParm4[StackPosition] == SCRIPT_FIX_ACTIVATOR)
	{
		fix_player_hook(StackActivator[StackPosition]);
	}

#ifdef PYTHON_DEBUG
	plugin_log(llevDebug, "done (returned: %d)!\n", StackReturn[StackPosition]);
#endif

	return StackReturn[StackPosition--];
}

/*****************************************************************************/
/* Plugin initialization.                                                    */
/*****************************************************************************/
/* It is required that:                                                      */
/* - The first returned value of the CFParm structure is the "internal" name */
/*   of the plugin, used by objects to identify it.                          */
/* - The second returned value is the name "in clear" of the plugin, used for*/
/*   information purposes.                                                   */
/*****************************************************************************/
MODULEAPI CFParm* initPlugin(CFParm* PParm)
{
	(void) PParm;

	LOG(llevDebug, "Atrinik Plugin loading.....\n");
	Py_Initialize();
	init_Atrinik_Python();
	LOG(llevDebug, "[Done]\n");

	GCFP.Value[0] = (void *) PLUGIN_NAME;
	GCFP.Value[1] = (void *) PLUGIN_VERSION;
	return &GCFP;
}

/*****************************************************************************/
/* Used to do cleanup before killing the plugin.                             */
/*****************************************************************************/
MODULEAPI CFParm* removePlugin(CFParm* PParm)
{
	(void) PParm;

	return NULL;
}

/*****************************************************************************/
/* This function is called to ask various informations to the plugin.        */
/*****************************************************************************/
MODULEAPI CFParm* getPluginProperty(CFParm* PParm)
{
	double dblval = 0.0;
	int i;

	if (PParm != NULL)
	{
		if (PParm->Value[0] && !strcmp((char *)(PParm->Value[0]), "command?"))
		{
			if (PParm->Value[1] && !strcmp((char *)(PParm->Value[1]), PLUGIN_NAME))
			{
				GCFP.Value[0] = PParm->Value[1];
				GCFP.Value[1] = &cmd_aboutPython;
				GCFP.Value[2] = &dblval;
				return &GCFP;
			}
			else
			{
				for (i = 0; i < NR_CUSTOM_CMD; i++)
				{
					if (CustomCommand[i].name)
					{
						if (!strcmp(CustomCommand[i].name, (char *)(PParm->Value[1])))
						{
							plugin_log(llevDebug, "PYTHON - Running command %s\n", CustomCommand[i].name);
							GCFP.Value[0] = PParm->Value[1];
							GCFP.Value[1] = cmd_customPython;
							GCFP.Value[2] = &(CustomCommand[i].speed);
							NextCustomCommand = i;
							return &GCFP;
						}
					}
				}
			}
		}
		else
		{
			plugin_log(llevDebug, "PYTHON - Unknown property tag: %s\n", (char *)(PParm->Value[0]));
		}
	}

	return NULL;
}

MODULEAPI int cmd_customPython(object *op, char *params)
{
#ifdef PYTHON_DEBUG
	plugin_log(llevDebug, "PYTHON - cmd_customPython called:: script file: %s\n", CustomCommand[NextCustomCommand].script);
#endif

	if (StackPosition == MAX_RECURSIVE_CALL)
	{
		plugin_log(llevDebug, "PYTHON - Can't execute script - No space left of stack\n");
		return 0;
	}

	StackPosition++;
	StackActivator[StackPosition] = op;
	StackWho[StackPosition] = op;
	StackOther[StackPosition] = op;
	StackText[StackPosition] = params;
	StackReturn[StackPosition] = 0;

	RunPythonScript(CustomCommand[NextCustomCommand].script, NULL);

	plugin_log(llevDebug, "done (returned: %d)!\n", StackReturn[StackPosition]);

	return StackReturn[StackPosition--];
}

MODULEAPI int cmd_aboutPython(object *op, char *params)
{
	int color = NDI_BLUE | NDI_UNIQUE;
	char message[1024];

	(void) params;

	sprintf(message, "%s (Kharkov)\n(C) 2001 by Gros. The Plugin code is under GPL.", PLUGIN_VERSION);
	GCFP.Value[0] = (void *)(&color);
	GCFP.Value[1] = (void *)(op->map);
	GCFP.Value[2] = (void *)(message);

	(PlugHooks[HOOK_NEWINFOMAP])(&GCFP);
	return 0;
}

/*****************************************************************************/
/* The postinitPlugin function is called by the server when the plugin load  */
/* is complete. It lets the opportunity to the plugin to register some events*/
/*****************************************************************************/
MODULEAPI CFParm* postinitPlugin(CFParm* PParm)
{
	/*    int i; */
	/* We can now register some global events if we want */
	/* We'll only register the global-only events :      */
	/* BORN, CRASH, LOGIN, LOGOUT, REMOVE, and SHOUT.    */
	/* The events APPLY, ATTACK, DEATH, DROP, PICKUP, SAY*/
	/* STOP, TELL, TIME, THROW and TRIGGER are already   */
	/* handled on a per-object basis and I simply don't  */
	/* see how useful they could be for the Python stuff.*/
	/* Registering them as local would be probably useful*/
	/* for extended logging facilities.                  */

	/* this is a extrem silly code part to remove a linker warning
	 * from VS c++ 6.x build. The optimizer will drop a warning that
	 * a function (timeGettime() ) is not used inside gettimeofday() and
	 * so he can remove the whole system .lib where it is in. This also means
	 * its not needed to load the .dll at runtime and thats what it tell us.
	 * this force a call and remove the warning from build. Its redundant
	 * code to give us a warning free build... without using any #ifdef
	 * or pragma. */
	struct timeval new_time;
	(void) GETTIMEOFDAY(&new_time);
	(void) PParm;

	plugin_log(llevDebug, "PYTHON - Start postinitPlugin.\n");

#if 0
	GCFP.Value[1] = (void *)(add_string_hook(PLUGIN_NAME));
#endif
	GCFP.Value[1] = (void *)PLUGIN_NAME;

#if 0
	i = EVENT_BORN;
	GCFP.Value[0] = (void *)(&i);
	(PlugHooks[HOOK_REGISTEREVENT])(&GCFP);

	i = EVENT_CRASH;
	GCFP.Value[0] = (void *)(&i);
	(PlugHooks[HOOK_REGISTEREVENT])(&GCFP);

	i = EVENT_LOGIN;
	GCFP.Value[0] = (void *)(&i);
	(PlugHooks[HOOK_REGISTEREVENT])(&GCFP);

	i = EVENT_LOGOUT;
	GCFP.Value[0] = (void *)(&i);
	(PlugHooks[HOOK_REGISTEREVENT])(&GCFP);

	i = EVENT_REMOVE;
	GCFP.Value[0] = (void *)(&i);
	(PlugHooks[HOOK_REGISTEREVENT])(&GCFP);

	i = EVENT_SHOUT;
	GCFP.Value[0] = (void *)(&i);
	(PlugHooks[HOOK_REGISTEREVENT])(&GCFP);

	i = EVENT_MAPENTER;
	GCFP.Value[0] = (void *)(&i);
	(PlugHooks[HOOK_REGISTEREVENT])(&GCFP);

	i = EVENT_MAPLEAVE;
	GCFP.Value[0] = (void *)(&i);
	(PlugHooks[HOOK_REGISTEREVENT])(&GCFP);

	i = EVENT_CLOCK;
	GCFP.Value[0] = (void *)(&i);
	(PlugHooks[HOOK_REGISTEREVENT])(&GCFP);

	i = EVENT_MAPRESET;
	GCFP.Value[0] = (void *)(&i);
	(PlugHooks[HOOK_REGISTEREVENT])(&GCFP);
#endif

	RunPythonScript("python/events/python_init.py", NULL);

	return NULL;
}

/*****************************************************************************/
/* Initializes the Python Interpreter.                                       */
/*****************************************************************************/
MODULEAPI void init_Atrinik_Python()
{
	PyObject *m, *d;
	int i;

	LOG(llevDebug, "PYTHON - Start initAtrinik.\n");

	m = Py_InitModule("Atrinik", AtrinikMethods);
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
	if (Atrinik_Object_init(m) || Atrinik_Map_init(m))
		return;

	/* Initialize direction constants */
	/* Gecko: TODO: error handling here */
	for (i = 0; module_constants[i].name; i++)
		PyModule_AddIntConstant(m, module_constants[i].name, module_constants[i].value);
}
