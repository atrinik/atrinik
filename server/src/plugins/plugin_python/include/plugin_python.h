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
/* This is a remake of CFPython module from Crossfire. The big changes done  */
/* are the addition of real object and map objects with methods and attribs. */
/* The attributes made it possible to remove almost all Set and Get          */
/* and the separation of functions into class methods led to the split into  */
/* three c files for better overview.                                        */
/* Below is the original blurb from CFPython.                                */
/*****************************************************************************/
/* Atrinik Python Plugin 1.0 - feb 2004                              */
/* Bjorn Axelsson                                                            */
/* Contact: gecko-at-acc.umu.se                                              */
/*****************************************************************************/

/*****************************************************************************/
/* CFPython - A Python module for Atrinik (Crossfire) RPG.                  */
/*****************************************************************************/
/* The goal of this module is to provide support for Python scripts into     */
/* Crossfire. Guile support existed before, but it was put directly in the   */
/* code, a thing that prevented easy building of Crossfire on platforms that */
/* didn't have a Guile port. And Guile was seen as difficult to learn and was*/
/* also less popular than Python in the Crossfire Community.                 */
/* So, I finally decided to replace Guile with Python and made it a separate */
/* module since it is not a "critical part" of the code. Of course, it also  */
/* means that it will never be as fast as it could be, but it allows more    */
/* flexibility (and although it is not as fast as compiled-in code, it should*/
/* be fast enough for nearly everything on most today computers).            */
/*****************************************************************************/
/* Please note that it is still very beta - some of the functions may not    */
/* work as expected and could even cause the server to crash.                */
/*****************************************************************************/
/* Version: 0.6 Beta  (also known as "Kharkov")                              */
/* Contact: yann.chachkoff@mailandnews.com                                   */
/*****************************************************************************/
/* That code is placed under the GNU General Public Licence (GPL)            */
/* (C)2001 by Chachkoff Yann (Feel free to deliver your complaints)          */
/*****************************************************************************/

#ifndef PLUGIN_PYTHON_H
#define PLUGIN_PYTHON_H

/* First the required header files - only the CF module interface and Python */
#include <Python.h>
#include <plugin.h>

#undef MODULEAPI
#ifdef WIN32
#ifdef PYTHON_PLUGIN_EXPORTS
#define MODULEAPI __declspec(dllexport)
#else
#define MODULEAPI __declspec(dllimport)
#endif /* ifdef PYTHON_PLUGIN_EXPORTS */
#else
#define MODULEAPI
#endif /* ifdef WIN32 */

#define PYTHON_DEBUG   /* give us some general infos out */

#define PLUGIN_NAME    "Python"
#define PLUGIN_VERSION "Atrinik Python Plugin 1.0"

/* The plugin properties and hook functions. A hook function is a pointer to */
/* a CF function wrapper. Basically, most CF functions that could be of any  */
/* use to the plugin have "wrappers", functions that all have the same args  */
/* and all returns the same type of data (CFParm); pointers to those functs. */
/* are passed to the plugin when it is initialized. They are what I called   */
/* "Hooks". It means that using any server function is slower from a plugin  */
/* than it is from the "inside" of the server code, because all arguments    */
/* need to be passed back and forth in a CFParm structure, but the difference*/
/* is not a problem, unless for time-critical code sections. Using such hooks*/
/* may of course sound complicated, but it allows much greater flexibility.  */
extern CFParm* PlugProps;
extern f_plugin PlugHooks[1024];

/* Some practical stuff, often used in the plugin                            */
#define WHO (whoptr->obj)
#define WHAT (whatptr->obj)
#define WHERE (whereptr->obj)

/* A generic exception that we use for error messages */
extern PyObject* AtrinikError;

/* Quick access to the exception. Use only in functions supposed to return pointers */
#define RAISE(msg) { PyErr_SetString(AtrinikError, (msg)); return NULL; }
#define INTRAISE(msg) { PyErr_SetString(PyExc_TypeError, (msg)); return -1; }

/* The declarations for the plugin interface. Every plugin should have those.*/
extern MODULEAPI CFParm* registerHook(CFParm* PParm);
extern MODULEAPI CFParm* triggerEvent(CFParm* PParm);
extern MODULEAPI CFParm* initPlugin(CFParm* PParm);
extern MODULEAPI CFParm* postinitPlugin(CFParm* PParm);
extern MODULEAPI CFParm* removePlugin(CFParm* PParm);
extern MODULEAPI CFParm* getPluginProperty(CFParm* PParm);

/* This one is used to cleanly pass args to the CF core */
extern CFParm GCFP;
extern CFParm GCFP0;
extern CFParm GCFP1;
extern CFParm GCFP2;

/* Those are used to handle the events. The first one is used when a player  */
/* attacks with a "scripted" weapon. HandleEvent is used for all other events*/
extern MODULEAPI int HandleUseWeaponEvent(CFParm* CFP);
extern MODULEAPI int HandleEvent(CFParm* CFP);
extern MODULEAPI int HandleGlobalEvent(CFParm* CFP);
/* Called to start the Python Interpreter.                                   */
extern MODULEAPI void init_Atrinik_Python();

/* The execution stack. Altough it is quite rare, a script can actually      */
/* trigger another script. The stack is used to keep track of those multiple */
/* levels of execution. A recursion stack of size 100 shout be sufficient.   */
/* If for some reason you think it is not enough, simply increase its size.  */
/* The code will still work, but the plugin will eat more memory.            */
#define MAX_RECURSIVE_CALL 100

extern int StackPosition;
extern object* StackActivator[MAX_RECURSIVE_CALL];
extern object* StackWho[MAX_RECURSIVE_CALL];
extern object* StackOther[MAX_RECURSIVE_CALL];
extern char* StackText[MAX_RECURSIVE_CALL];
extern int StackParm1[MAX_RECURSIVE_CALL];
extern int StackParm2[MAX_RECURSIVE_CALL];
extern int StackParm3[MAX_RECURSIVE_CALL];
extern int StackParm4[MAX_RECURSIVE_CALL];
extern int StackReturn[MAX_RECURSIVE_CALL];
extern char *StackOptions[MAX_RECURSIVE_CALL];

/* Type used for numeric constants */
typedef struct
{
	char *name;
	int value;
} Atrinik_Constant;

/* Types used in objects and maps structs */
typedef enum
{
	FIELDTYPE_SHSTR, /* Pointer to shared string */
	FIELDTYPE_CSTR,  /* Pointer to C string */
	FIELDTYPE_CARY,  /* C string (array directly in struct) */
	FIELDTYPE_UINT8, FIELDTYPE_SINT8,
	FIELDTYPE_UINT16, FIELDTYPE_SINT16,
	FIELDTYPE_UINT32, FIELDTYPE_SINT32,
	FIELDTYPE_FLOAT,
	FIELDTYPE_OBJECT, FIELDTYPE_MAP,
	FIELDTYPE_OBJECTREF /* object pointer + tag */
} field_type;

/* Special flags for object attribute access */
#define FIELDFLAG_READONLY        1 /* changing value not allowed */
#define FIELDFLAG_PLAYER_READONLY 2 /* changing value is not allowed if object is a player */
#define FIELDFLAG_PLAYER_FIX      4 /* fix player or monster after change */

/* Public AtrinikObject related functions and types */
extern PyTypeObject Atrinik_ObjectType;

extern PyObject *wrap_object(object *what);
extern int Atrinik_Object_init(PyObject *module);

typedef struct
{
	PyObject_HEAD
	object *obj; /* Pointer to the Atrinik object we wrap */
} Atrinik_Object;

/* Public AtrinikMap related functions and types */
extern PyTypeObject Atrinik_MapType;

extern PyObject *wrap_map(mapstruct *map);
extern int Atrinik_Map_init(PyObject *module);

typedef struct
{
	PyObject_HEAD
	mapstruct *map;  /* Pointer to the Atrinik map we wrap */
} Atrinik_Map;

/*****************************************************************************/
/* Commands management part.                                                 */
/* It is now possible to add commands to crossfire. The following stuff was  */
/* created to handle such commands.                                          */
/*****************************************************************************/

/* The "About Python" stuff. Bound to "python" command.                      */
extern MODULEAPI int cmd_aboutPython(object *op, char *params);
/* The following one handles all custom Python command calls.                */
extern MODULEAPI int cmd_customPython(object *op, char *params);

/* This structure is used to define one python-implemented crossfire command.*/
typedef struct PythonCmdStruct
{
	char *name;    /* The name of the command, as known in the game.    */
	char *script;  /* The name of the script file to bind.              */
	double speed;   /* The speed of the command execution.                   */
} PythonCmd;

/* This plugin allows up to 1024 custom commands.                            */
#define NR_CUSTOM_CMD 1024
extern PythonCmd CustomCommand[NR_CUSTOM_CMD];
/* This one contains the index of the next command that needs to be run. I do*/
/* not like the use of such a global variable, but it is the most convenient */
/* way I found to pass the command index to cmd_customPython.                */
extern int NextCustomCommand;
#endif
