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
 * Atrinik Python plugin map related code. */

#include <plugin_python.h>

/** Map fields structure. */
typedef struct
{
	/** Name of the field */
	char *name;

	/** Field type */
	field_type type;

	/** Offset in map structure */
	uint32 offset;
} map_fields_struct;

/**
 * @anchor plugin_python_map_fields
 *
 * Map fields.
 *
 * List of the fields and their meaning:
 *
 * - <b>name</b>: @copydoc mapstruct::name
 * - <b>message</b>: @copydoc mapstruct::msg
 * - <b>reset_interval</b>: @copydoc mapstruct::reset_timeout
 * - <b>difficulty</b>: @copydoc mapstruct::difficulty
 * - <b>height</b>: @copydoc mapstruct::height
 * - <b>width</b>: @copydoc mapstruct::width
 * - <b>darkness</b>: @copydoc mapstruct::darkness
 * - <b>path</b>: @copydoc mapstruct::path
 * - <b>enter_x</b>: @copydoc mapstruct::enter_x
 * - <b>enter_y</b>: @copydoc mapstruct::enter_y */
map_fields_struct map_fields[] =
{
	{"name",            FIELDTYPE_CSTR,     offsetof(mapstruct, name)},
	{"message",         FIELDTYPE_CSTR,     offsetof(mapstruct, msg)},
	{"reset_interval",  FIELDTYPE_UINT32,   offsetof(mapstruct, reset_timeout)},
	{"difficulty",      FIELDTYPE_UINT16,   offsetof(mapstruct, difficulty)},
	{"height",          FIELDTYPE_UINT16,   offsetof(mapstruct, height)},
	{"width",           FIELDTYPE_UINT16,   offsetof(mapstruct, width)},
	{"darkness",        FIELDTYPE_UINT8,    offsetof(mapstruct, darkness)},
	{"path",            FIELDTYPE_SHSTR,    offsetof(mapstruct, path)},
	{"enter_x",         FIELDTYPE_UINT8,    offsetof(mapstruct, enter_x)},
	{"enter_y",         FIELDTYPE_UINT8,    offsetof(mapstruct, enter_y)}
};

/**
 * @anchor plugin_python_map_flags
 *
 * Map flags.
 *
 * List of the flags and their meaning:
 *
 * - <b>f_outdoor</b>: @copydoc MAP_FLAG_OUTDOOR
 * - <b>f_unique</b>: @copydoc MAP_FLAG_UNIQUE
 * - <b>f_fixed_rtime</b>: @copydoc MAP_FLAG_FIXED_RTIME
 * - <b>f_nomagic</b>: @copydoc MAP_FLAG_NOMAGIC
 * - <b>f_nopriest</b>: @copydoc MAP_FLAG_NOPRIEST
 * - <b>f_noharm</b>: @copydoc MAP_FLAG_NOHARM
 * - <b>f_nosummon</b>: @copydoc MAP_FLAG_NOSUMMON
 * - <b>f_fixed_login</b>: @copydoc MAP_FLAG_FIXED_LOGIN
 * - <b>f_permdeath</b>: @copydoc MAP_FLAG_PERMDEATH
 * - <b>f_ultradeath</b>: @copydoc MAP_FLAG_ULTRADEATH
 * - <b>f_ultimatedeath</b>: @copydoc MAP_FLAG_ULTIMATEDEATH
 * - <b>f_pvp</b>: @copydoc MAP_FLAG_PVP
 * - <b>f_no_save</b>: @copydoc MAP_FLAG_NO_SAVE
 * - <b>f_plugins</b>: @copydoc MAP_FLAG_PLUGINS
 *
 * @note These must be in same order as @ref map_flags "map flags". */
static char *mapflag_names[] =
{
	"f_outdoor",        "f_unique",     "f_fixed_rtime",    "f_nomagic",
	"f_nopriest",       "f_noharm",     "f_nosummon",       "f_fixed_login",
	"f_permdeath",      "f_ultradeath", "f_ultimatedeath",  "f_pvp",
	"f_no_save",        "f_plugins"
};

/** Number of map fields */
#define NUM_MAPFIELDS (sizeof(map_fields) / sizeof(map_fields[0]))

/** Number of map flags */
#define NUM_MAPFLAGS (sizeof(mapflag_names) / sizeof(mapflag_names[0]))

/**
 * @anchor plugin_python_map_constants
 * Map related constants */
static Atrinik_Constant map_constants[] =
{
	{"COST_TRUE",   F_TRUE},
	{"COST_BUY",    F_BUY},
	{"COST_SELL",   F_SELL},
	{NULL,          0}
};

/**
 * @defgroup plugin_python_map_functions Python plugin map functions
 * Map related functions used in Atrinik Python plugin.
 *@{*/

/**
 * <h1>map.GetFirstObjectOnSquare(<i>\<int\></i> x, <i>\<int\></i> y)</h1>
 *
 * Gets the bottom object on the tile. Use object::above to browse
 * objects.
 * @param x X position on the map
 * @param y Y position on the map
 * @return The object if found. */
static PyObject *Atrinik_Map_GetFirstObjectOnSquare(Atrinik_Map *map, PyObject *args)
{
	int x, y;
	object *val = NULL;
	mapstruct *m = map->map;

	if (!PyArg_ParseTuple(args, "ii", &x, &y))
	{
		return NULL;
	}

	if ((m = hooks->get_map_from_coord(m, &x, &y)))
	{
		val = get_map_ob(m, x, y);
	}

	return wrap_object(val);
}

/**
 * <h1>map.MapTileAt(<i>\<int\></i> x, <i>\<int\></i> y)</h1>
 * @param x X position on the map
 * @param y Y position on the map
 * @todo Do someting about the new modified coordinates too?
 * @warning Not tested. */
static PyObject *Atrinik_Map_MapTileAt(Atrinik_Map *map, PyObject *args)
{
	int x, y;

	if (!PyArg_ParseTuple(args, "ii", &x, &y))
	{
		return NULL;
	}

	return wrap_map(hooks->get_map_from_coord(map->map, &x, &y));
}

/**
 * <h1>map.PlaySound(<i>\<int\></i> x, <i>\<int\></i> y, <i>\<int\></i> soundnumber, <i>[int]</i> soundtype)</h1>
 *
 * Play a sound on map.
 * @param x X position on the map where the sound is coming from.
 * @param y Y position on the map where the sound is coming from.
 * @param soundnumber ID of the sound to play.
 * @param soundtype Type of the sound, one of:
 * - SOUNDTYPE_NORMAL (default): Sound number should be one of @ref sound_numbers_normal "normal sound numbers".
 * - SOUNDTYPE_SPELL: Sound number should be one of @ref sound_numbers_spell "spell sound numbers". */
static PyObject *Atrinik_Map_PlaySound(Atrinik_Map *whereptr, PyObject *args)
{
	int x, y, soundnumber, soundtype = SOUND_NORMAL;

	if (!PyArg_ParseTuple(args, "iii|i", &x, &y, &soundnumber, &soundtype))
	{
		return NULL;
	}

	hooks->play_sound_map(whereptr->map, x, y, soundnumber, soundtype);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>map.Message(<i>\<string\></i> message, <i>\<int\></i> x,
 * <i>\<int\></i> y, <i>\<int\></i> distance, <i>\<int\></i> color)</h1>
 *
 * Write a message to all players on a map.
 * @param x X position on the map
 * @param y Y position on the map
 * @param distance Maximum distance for players to be away from x, y to
 * hear the message.
 * @param color Color of the message, default is @ref NDI_BLUE. */
static PyObject *Atrinik_Map_Message(Atrinik_Map *map, PyObject *args)
{
	int color = NDI_BLUE | NDI_UNIQUE, x, y, d;
	char *message;

	if (!PyArg_ParseTuple(args, "iiis|i", &x, &y, &d, &message, &color))
	{
		return NULL;
	}

	hooks->new_info_map(color, map->map, x, y, d, message);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>map.CreateObject(<i>\<string\></i> arch_name, <i>\<int\></i> x,
 * <i>\<int\></i> y)</h1>
 *
 * Create an object on map.
 * @param arch_name Arch name of the object to create
 * @param x X position on the map
 * @param y Y position on the map
 * @warning Not tested. */
static PyObject *Atrinik_Map_CreateObject(Atrinik_Map *map, PyObject *args)
{
	char *txt;
	int x, y;
	archetype *arch;
	object *newobj;

	if (!PyArg_ParseTuple(args, "sii", &txt, &x, &y))
	{
		return NULL;
	}

	if (!(arch = hooks->find_archetype(txt)) || !(newobj = hooks->arch_to_object(arch)))
	{
		return NULL;
	}

	newobj->x = x;
	newobj->y = y;

	newobj = hooks->insert_ob_in_map(newobj, map->map, NULL, 0);

	return wrap_object(newobj);
}

/**
 * <h1>map.CountPlayers()</h1>
 *
 * Count number of players on map, using players_on_map().
 * @return The count of players on the map. */
static PyObject *Atrinik_Map_CountPlayers(Atrinik_Map *map, PyObject *args)
{
	(void) args;

	return Py_BuildValue("i", hooks->players_on_map(map->map));
}

/**
 * <h1>map.GetPlayers()</h1>
 *
 * Get all the players on a specified map.
 * @return Python list containing pointers to player objects on the
 * map. */
static PyObject *Atrinik_Map_GetPlayers(Atrinik_Map *map, PyObject *args)
{
	PyObject *list = PyList_New(0);
	object *tmp;

	(void) args;

	for (tmp = map->map->player_first; tmp; tmp = CONTR(tmp)->map_above)
	{
		PyList_Append(list, wrap_object(tmp));
	}

	return list;
}

/*@}*/

/**
 * Get map's attribute.
 * @param map Python map wrapper.
 * @param fieldno Attribute ID.
 * @return Python object with the attribute value, NULL on failure. */
static PyObject *Map_GetAttribute(Atrinik_Map *map, int fieldno)
{
	void *field_ptr;

	if (fieldno < 0 || fieldno >= (int) NUM_MAPFIELDS)
	{
		RAISE("Illegal field ID.");
	}

	field_ptr = (void *) ((char *) (map->map) + map_fields[fieldno].offset);

	switch (map_fields[fieldno].type)
	{
		case FIELDTYPE_SHSTR:
		case FIELDTYPE_CSTR:
			return Py_BuildValue("s", *(char **) field_ptr);

		case FIELDTYPE_CARY:
			return Py_BuildValue("s", (char *) field_ptr);

		case FIELDTYPE_UINT8:
			return Py_BuildValue("b", *(uint8 *) field_ptr);

		case FIELDTYPE_SINT8:
			return Py_BuildValue("b", *(sint8 *) field_ptr);

		case FIELDTYPE_UINT16:
			return Py_BuildValue("i", *(uint16 *) field_ptr);

		case FIELDTYPE_SINT16:
			return Py_BuildValue("i", *(sint16 *) field_ptr);

		case FIELDTYPE_UINT32:
			return Py_BuildValue("l", *(uint32 *) field_ptr);

		case FIELDTYPE_SINT32:
			return Py_BuildValue("l", *(sint32 *) field_ptr);

		case FIELDTYPE_FLOAT:
			return Py_BuildValue("f", *(float *) field_ptr);

		case FIELDTYPE_MAP:
			return wrap_map(*(mapstruct **) field_ptr);

		case FIELDTYPE_OBJECT:
			return wrap_object(*(object **) field_ptr);

		default:
			RAISE("BUG: Unknown field type.");
	}

	return NULL;
}

/**
 * Get map's flag.
 * @param map Python map wrapper.
 * @param flagno Flag to get.
 * @return 1 if the map has the flag set, 0 otherwise. */
static PyObject *Map_GetFlag(Atrinik_Map *map, int flagno)
{
	if (flagno < 0 || flagno >= (int) NUM_MAPFLAGS)
	{
		RAISE("Unknown flag.");
	}

	return Py_BuildValue("i", (map->map->map_flags & (1 << flagno)) ? 1 : 0);
}

/**
 * Create a new map wrapper.
 * @param type Type object.
 * @param args Unused.
 * @param kwds Unused.
 * @return The new wrapper. */
static PyObject *Atrinik_Map_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	Atrinik_Map *self;

	(void) args;
	(void) kwds;

	self = (Atrinik_Map *) type->tp_alloc(type, 0);

	if (self)
	{
		self->map = NULL;
	}

	return (PyObject *) self;
}

/**
 * Free a map wrapper.
 * @param self The wrapper to free. */
static void Atrinik_Map_dealloc(Atrinik_Map *self)
{
	self->map = NULL;
#ifndef IS_PY_LEGACY
	Py_TYPE(self)->tp_free((PyObject *) self);
#else
	self->ob_type->tp_free((PyObject *) self);
#endif
}

/**
 * Return a string representation of a map.
 * @param self The map type.
 * @return Python object containing the map path and name of the map. */
static PyObject *Atrinik_Map_str(Atrinik_Map *self)
{
	return PyString_FromFormat("[%s \"%s\"]", self->map->path, self->map->name);
}

/** Available Python methods for the AtrinikMap object */
static PyMethodDef MapMethods[] =
{
	{"GetFirstObjectOnSquare",  (PyCFunction)Atrinik_Map_GetFirstObjectOnSquare,    METH_VARARGS, 0},
	{"PlaySound",               (PyCFunction)Atrinik_Map_PlaySound,                 METH_VARARGS, 0},
	{"Message",                 (PyCFunction)Atrinik_Map_Message,                   METH_VARARGS, 0},
	{"MapTileAt",               (PyCFunction)Atrinik_Map_MapTileAt,                 METH_VARARGS, 0},
	{"CreateObject",            (PyCFunction)Atrinik_Map_CreateObject,              METH_VARARGS, 0},
	{"CountPlayers",            (PyCFunction)Atrinik_Map_CountPlayers,              METH_VARARGS, 0},
	{"GetPlayers",              (PyCFunction)Atrinik_Map_GetPlayers,                METH_VARARGS, 0},
	{NULL, NULL, 0, 0}
};

/** This is filled in when we initialize our map type. */
static PyGetSetDef Map_getseters[NUM_MAPFIELDS + NUM_MAPFLAGS + 1];

/** Our actual Python MapType. */
PyTypeObject Atrinik_MapType =
{
#ifdef IS_PY3K
	PyVarObject_HEAD_INIT(NULL, 0)
#else
	PyObject_HEAD_INIT(NULL)
	0,
#endif
	"Atrinik.Map",
	sizeof(Atrinik_Map),
	0,
	(destructor) Atrinik_Map_dealloc,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	(reprfunc) Atrinik_Map_str,
	0, 0, 0,
	Py_TPFLAGS_DEFAULT,
	"Atrinik maps",
	0, 0, 0, 0, 0, 0,
	MapMethods,
	0,
	Map_getseters,
	0, 0, 0, 0, 0, 0, 0,
	Atrinik_Map_new,
	0, 0, 0, 0, 0, 0, 0, 0
#ifndef IS_PY_LEGACY
	, 0
#endif
};

/**
 * Initialize the map wrapper.
 * @param module The Atrinik Python module.
 * @return 1 on success, 0 on failure. */
int Atrinik_Map_init(PyObject *module)
{
	int i;

	/* Field getters */
	for (i = 0; i < (int) NUM_MAPFIELDS; i++)
	{
		PyGetSetDef *def = &Map_getseters[i];

		def->name = map_fields[i].name;
		def->get = (getter) Map_GetAttribute;
		def->set = NULL;
		def->doc = NULL;
		def->closure = (void *) i;
	}

	/* Flag getters */
	for (i = 0; i < (int) NUM_MAPFLAGS; i++)
	{
		PyGetSetDef *def = &Map_getseters[i + NUM_MAPFIELDS];

		def->name = mapflag_names[i];
		def->get = (getter) Map_GetFlag;
		def->set = NULL;
		def->doc = NULL;
		def->closure = (void *) i;
	}

	Map_getseters[NUM_MAPFIELDS + NUM_MAPFLAGS].name = NULL;

	/* Add constants */
	for (i = 0; map_constants[i].name; i++)
	{
		if (PyModule_AddIntConstant(module, map_constants[i].name, map_constants[i].value))
		{
			return 0;
		}
	}

	Atrinik_MapType.tp_new = PyType_GenericNew;

	if (PyType_Ready(&Atrinik_MapType) < 0)
	{
		return 0;
	}

	return 1;
}

/**
 * Utility method to wrap a map.
 * @param what Map to wrap.
 * @return Python object wrapping the real map. */
PyObject *wrap_map(mapstruct *what)
{
	Atrinik_Map *wrapper;

	/* Return None if no map was to be wrapped. */
	if (!what)
	{
		Py_INCREF(Py_None);
		return Py_None;
	}

	wrapper = PyObject_NEW(Atrinik_Map, &Atrinik_MapType);

	if (wrapper)
	{
		wrapper->map = what;
	}

	return (PyObject *) wrapper;
}
