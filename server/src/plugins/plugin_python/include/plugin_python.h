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

#ifndef PLUGIN_PYTHON_H
#define PLUGIN_PYTHON_H

#include <Python.h>
#include <plugin.h>

/* This is for allowing both python 3 and python 2. */
#if PY_MAJOR_VERSION >= 3
#	define IS_PY3K
#else
#	if PY_MINOR_VERSION >= 6
#		define IS_PY26
#	else
#		define IS_PY_LEGACY
#	endif
#	if PY_MINOR_VERSION >= 5
#		define IS_PY25
#	endif
#endif

/* Fake some Python 2.x functions for Python 3.x */
#ifdef IS_PY3K
#	define PyString_Check PyUnicode_Check
#	define PyString_AsString _PyUnicode_AsString
#	define PyString_FromFormat PyBytes_FromFormat
#	define PyInt_Check PyLong_Check
#	define PyInt_AsLong PyLong_AsLong
#endif

#undef MODULEAPI

#ifdef WIN32
#	ifdef PYTHON_PLUGIN_EXPORTS
#		define MODULEAPI __declspec(dllexport)
#	else
#		define MODULEAPI __declspec(dllimport)
#	endif
#else
#	define MODULEAPI
#endif

/* give us some general infos out */
#define PYTHON_DEBUG

#define PLUGIN_NAME "Python"
#define PLUGIN_VERSION "Atrinik Python Plugin 1.0"

struct plugin_hooklist *hooks;

/* Some practical stuff, often used in the plugin */
#define WHO (whoptr->obj)
#define WHAT (whatptr->obj)
#define WHERE (whereptr->obj)

#undef LOG
#define LOG hooks->LOG

#undef FREE_AND_COPY_HASH
#undef FREE_AND_CLEAR_HASH

#define FREE_AND_COPY_HASH(_sv_,_nv_) { if (_sv_) hooks->free_string_shared(_sv_); _sv_=hooks->add_string(_nv_); }
#define FREE_AND_CLEAR_HASH(_nv_) {if(_nv_){hooks->free_string_shared(_nv_);_nv_ =NULL;}}

/* A generic exception that we use for error messages */
extern PyObject *AtrinikError;

/* Quick access to the exception. Use only in functions supposed to return pointers */
#define RAISE(msg) { PyErr_SetString(AtrinikError, (msg)); return NULL; }
#define INTRAISE(msg) { PyErr_SetString(PyExc_TypeError, (msg)); return -1; }

extern MODULEAPI int HandleEvent(va_list args);
extern MODULEAPI int HandleGlobalEvent(int event_type, va_list args);
extern MODULEAPI void init_Atrinik_Python();

/* The execution stack. Altough it is quite rare, a script can actually      */
/* trigger another script. The stack is used to keep track of those multiple */
/* levels of execution. A recursion stack of size 100 shout be sufficient.   */
/* If for some reason you think it is not enough, simply increase its size.  */
/* The code will still work, but the plugin will eat more memory.            */
#define MAX_RECURSIVE_CALL 100

extern int StackPosition;
extern object *StackActivator[MAX_RECURSIVE_CALL];
extern object *StackWho[MAX_RECURSIVE_CALL];
extern object *StackOther[MAX_RECURSIVE_CALL];
extern char *StackText[MAX_RECURSIVE_CALL];
extern int StackParm1[MAX_RECURSIVE_CALL];
extern int StackParm2[MAX_RECURSIVE_CALL];
extern int StackParm3[MAX_RECURSIVE_CALL];
extern int StackParm4[MAX_RECURSIVE_CALL];
extern int StackReturn[MAX_RECURSIVE_CALL];
extern char *StackOptions[MAX_RECURSIVE_CALL];

/** Type used for numeric constants */
typedef struct
{
	char *name;
	int value;
} Atrinik_Constant;

/** Types used in objects and maps structs */
typedef enum
{
	/* Pointer to shared string */
	FIELDTYPE_SHSTR,
	/* Pointer to C string */
	FIELDTYPE_CSTR,
	/* C string (array directly in struct) */
	FIELDTYPE_CARY,
	FIELDTYPE_UINT8, FIELDTYPE_SINT8,
	FIELDTYPE_UINT16, FIELDTYPE_SINT16,
	FIELDTYPE_UINT32, FIELDTYPE_SINT32,
	FIELDTYPE_UINT64, FIELDTYPE_SINT64,
	FIELDTYPE_FLOAT,
	FIELDTYPE_OBJECT, FIELDTYPE_MAP,
	/* object pointer + tag */
	FIELDTYPE_OBJECTREF
} field_type;


/**
 * @defgroup FIELDFLAG_xxx Field flags
 * Special flags for object attribute access.
 *@{*/
/** Changing value not allowed. */
#define FIELDFLAG_READONLY        1
/** Changing value is not allowed if object is a player. */
#define FIELDFLAG_PLAYER_READONLY 2
/** Fix player or monster after change */
#define FIELDFLAG_PLAYER_FIX      4
/*@}*/

extern PyTypeObject Atrinik_ObjectType;
extern PyObject *wrap_object(object *what);
extern int Atrinik_Object_init(PyObject *module);

typedef struct
{
	PyObject_HEAD
	/* Pointer to the Atrinik object we wrap */
	object *obj;
} Atrinik_Object;

extern PyTypeObject Atrinik_MapType;

extern PyObject *wrap_map(mapstruct *map);
extern int Atrinik_Map_init(PyObject *module);

typedef struct
{
	PyObject_HEAD
	/* Pointer to the Atrinik map we wrap */
	mapstruct *map;
} Atrinik_Map;

extern PyTypeObject Atrinik_PartyType;

extern PyObject *wrap_party(partylist_struct *party);
extern int Atrinik_Party_init(PyObject *module);

typedef struct
{
	PyObject_HEAD
	partylist_struct *party;
} Atrinik_Party;

extern MODULEAPI int cmd_customPython(object *op, char *params);

/** This structure is used to define one Python-implemented command. */
typedef struct PythonCmdStruct
{
	/** The name of the command, as known in the game. */
	char *name;

	/** The name of the script file to bind. */
	char *script;

	/** The speed of the command execution. */
	double speed;
} PythonCmd;

/** Number of custom commands to allow. */
#define NR_CUSTOM_CMD 1024
extern PythonCmd CustomCommand[NR_CUSTOM_CMD];
extern int NextCustomCommand;

#endif
