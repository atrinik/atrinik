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
 * Python plugin related header file. */

#ifndef PLUGIN_PYTHON_H
#define PLUGIN_PYTHON_H

#include <Python.h>
#include <plugin.h>
#include <timers.h>

/** This is for allowing both python 3 and python 2. */
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

/* Fake some Python 2.x functions for Python 3.x. */
#ifdef IS_PY3K
#	define PyString_Check PyUnicode_Check
#	define PyString_AsString _PyUnicode_AsString
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

/** Print out some general information about scripts running. */
#ifndef PRODUCTION_SERVER
#	define PYTHON_DEBUG
#endif

/** Name of the plugin. */
#define PLUGIN_NAME "Python"
/** Name of the plugin, and its version. */
#define PLUGIN_VERSION "Atrinik Python Plugin 1.0"

extern struct plugin_hooklist *hooks;

/** Get object from Atrinik_Object 'whoptr'. */
#define WHO (whoptr->obj)
/** Get object from Atrinik_Object 'whatptr'. */
#define WHAT (whatptr->obj)
/** Get object from Atrinik_Object 'whereptr'. */
#define WHERE (whereptr->obj)

/**
 * @defgroup AROUND_xxx Types for object.SquaresAround()
 * The various types of squares returned by object.SquaresAround().
 * @note These are bitmasks.
 *@{*/
/** All squares around the object. */
#define AROUND_ALL 0
/** Ignore squares that have a wall. */
#define AROUND_WALL 1
/** Ignore squares that are blocking view. */
#define AROUND_BLOCKSVIEW 2
/** Ignore squares that are player only. */
#define AROUND_PLAYER_ONLY 4
/*@}*/

#undef LOG
/** @copydoc LOG */
#define LOG hooks->LOG

#undef FREE_AND_COPY_HASH
#undef FREE_AND_CLEAR_HASH

/**
 * Free old shared string and add new string.
 * @param _sv_ Shared string.
 * @param _nv_ String to copy to the shared string. */
#define FREE_AND_COPY_HASH(_sv_, _nv_)   \
{                                        \
	if (_sv_)                            \
	{                                    \
		hooks->free_string_shared(_sv_); \
	}                                    \
                                         \
	_sv_ = hooks->add_string(_nv_);      \
}
/**
 * Free old hash and add a reference to the new one.
 * @param _sv_ Pointer to shared string.
 * @param _nv_ String to add reference to. Must be a shared string. */
#define FREE_AND_CLEAR_HASH(_nv_)        \
{                                        \
	if (_nv_)                            \
	{                                    \
		hooks->free_string_shared(_nv_); \
		_nv_ = NULL;                     \
	}                                    \
}

extern PyObject *AtrinikError;

/** Raise an error using AtrinikError, and return NULL. */
#define RAISE(msg)                        \
{                                         \
	PyErr_SetString(AtrinikError, (msg)); \
	return NULL;                          \
}
/** Raise an error using AtrinikError, and return -1. */
#define INTRAISE(msg)                        \
{                                            \
	PyErr_SetString(PyExc_TypeError, (msg)); \
	return -1;                               \
}

/** The Python event context. */
typedef struct _pythoncontext
{
	/** Next context. */
	struct _pythoncontext *down;

	/** Event activator. */
	object *activator;

	/** Object that has the event object. */
	object *who;

	/** Other object involved. */
	object *other;

	/** The actual event object. */
	object *event;

	/** Text message (say event for example) */
	char *text;

	/** Event options. */
	char *options;

	/** Return value of the event. */
	int returnvalue;

	/** Integer parameters. */
	int parms[4];
} PythonContext;

extern PythonContext *current_context;

/** Type used for integer constants. */
typedef struct
{
	/** Name of the constant. */
	char *name;

	/** Value of the constant. */
	int value;
} Atrinik_Constant;

/** Types used in objects and maps structs. */
typedef enum
{
	/** Pointer to shared string. */
	FIELDTYPE_SHSTR,
	/** Pointer to C string. */
	FIELDTYPE_CSTR,
	/** C string (array directly in struct). */
	FIELDTYPE_CARY,
	/** Unsigned int8. */
	FIELDTYPE_UINT8,
	/** Signed int8. */
	FIELDTYPE_SINT8,
	/** Unsigned int16. */
	FIELDTYPE_UINT16,
	/** Signed int16. */
	FIELDTYPE_SINT16,
	/** Unsigned int32. */
	FIELDTYPE_UINT32,
	/** Signed int32. */
	FIELDTYPE_SINT32,
	/** Unsigned int64. */
	FIELDTYPE_UINT64,
	/** Signed int64. */
	FIELDTYPE_SINT64,
	/** Float. */
	FIELDTYPE_FLOAT,
	/** Pointer to object. */
	FIELDTYPE_OBJECT,
	/** Pointer to map. */
	FIELDTYPE_MAP,
	/** Object pointer + tag. */
	FIELDTYPE_OBJECTREF,
	/** Pointer to region. */
	FIELDTYPE_REGION,
	/** Pointer to a party. */
	FIELDTYPE_PARTY,
	/** Pointer to an archetype. */
	FIELDTYPE_ARCH
} field_type;

/**
 * @defgroup FIELDFLAG_xxx Field flags
 * Special flags for object attribute access.
 *@{*/
/** Changing value not allowed. */
#define FIELDFLAG_READONLY 1
/** Changing value is not allowed if object is a player. */
#define FIELDFLAG_PLAYER_READONLY 2
/** Fix player or monster after change. */
#define FIELDFLAG_PLAYER_FIX 4
/*@}*/

PyTypeObject Atrinik_ObjectType;
PyObject *wrap_object(object *what);
int Atrinik_Object_init(PyObject *module);

/**
 * @defgroup OBJ_ITER_TYPE_xxx Object iteration types
 * These determine how we're iterating over an object.
 *@{*/
/** Nothing to iterate over. */
#define OBJ_ITER_TYPE_NONE 0
/** Using object::below. */
#define OBJ_ITER_TYPE_BELOW 1
/** Using object::above. */
#define OBJ_ITER_TYPE_ABOVE 2
/** There is nothing below or above the object. */
#define OBJ_ITER_TYPE_ONE 3
/*@}*/

/** The Atrinik_Object structure. */
typedef struct Atrinik_Object
{
	PyObject_HEAD
	/** Pointer to the Atrinik object we wrap. */
	object *obj;
	/** Pointer for iteration. */
	struct Atrinik_Object *iter;
	/** @ref OBJ_ITER_TYPE_xxx "Iteration type". */
	uint8 iter_type;
} Atrinik_Object;

PyTypeObject Atrinik_MapType;
PyObject *wrap_map(mapstruct *map);
int Atrinik_Map_init(PyObject *module);

/** The Atrinik_Map structure. */
typedef struct
{
	PyObject_HEAD
	/** Pointer to the Atrinik map we wrap. */
	mapstruct *map;
} Atrinik_Map;

PyTypeObject Atrinik_PartyType;
PyObject *wrap_party(party_struct *party);
int Atrinik_Party_init(PyObject *module);

/** The Atrinik_Party structure. */
typedef struct
{
	PyObject_HEAD
	/** Pointer to the Atrinik party we wrap. */
	party_struct *party;
} Atrinik_Party;

PyTypeObject Atrinik_RegionType;
PyObject *wrap_region(region *region);
int Atrinik_Region_init(PyObject *module);

/** The Atrinik_Region structure. */
typedef struct
{
	PyObject_HEAD
	/** Pointer to the Atrinik region we wrap. */
	region *region;
} Atrinik_Region;

PyTypeObject Atrinik_PlayerType;
PyObject *wrap_player(player *pl);
int Atrinik_Player_init(PyObject *module);

/** The Atrinik_Player structure. */
typedef struct
{
	PyObject_HEAD
	/** Pointer to the Atrinik player we wrap. */
	player *pl;
} Atrinik_Player;

PyTypeObject Atrinik_ArchetypeType;
PyObject *wrap_archetype(archetype *at);
int Atrinik_Archetype_init(PyObject *module);

/** The Atrinik_Archetype structure. */
typedef struct
{
	PyObject_HEAD
	/** Pointer to the Atrinik archetype we wrap. */
	archetype *at;
} Atrinik_Archetype;

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

/**
 * General structure for Python object fields. */
typedef struct
{
	/**
	 * Name of the field. */
	char *name;

	/**
	 * Field type. */
	field_type type;

	/**
	 * Offset in player structure. */
	uint32 offset;

	/**
	 * Flags for special handling. */
	uint32 flags;

	/**
	 * Extra data for some special fields. */
	uint32 extra_data;
} fields_struct;

/**
 * Get number of fields in the fields array.
 * @return Number of fields. */
#define NUM_FIELDS (sizeof(fields) / sizeof(fields[0]))

/** Number of custom commands to allow. */
#define NR_CUSTOM_CMD 1024

#define OBJEXISTCHECK_INT(ob) \
{ \
	if (!ob || !ob->obj || hooks->was_destroyed(ob->obj, ob->obj->count)) \
	{ \
		PyErr_SetString(PyExc_ReferenceError, "Atrinik object no longer exists."); \
		return -1; \
	} \
}

/**
 * Helper macro for the object.SquaresAround() Python function. */
#define SQUARES_AROUND_ADD(_m, _x, _y) \
{ \
	PyObject *tuple = PyTuple_New(3); \
\
	PyTuple_SET_ITEM(tuple, 0, wrap_map((_m))); \
	PyTuple_SET_ITEM(tuple, 1, Py_BuildValue("i", (_x))); \
	PyTuple_SET_ITEM(tuple, 2, Py_BuildValue("i", (_y))); \
	PyList_Append(list, tuple); \
}

int generic_field_setter(fields_struct *field, void *ptr, PyObject *value);
PyObject *generic_field_getter(fields_struct *field, void *ptr);
void Py_INCREF_TYPE(PyTypeObject *ob);
PyObject *generic_rich_compare(int op, int result);
int python_call_int(PyObject *callable, PyObject *arglist);

#endif
