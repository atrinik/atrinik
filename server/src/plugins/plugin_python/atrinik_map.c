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
 * Atrinik Python plugin map related code. */

#include <atrinik_map.h>

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

/** This is filled in when we initialize our map type. */
static PyGetSetDef Map_getseters[NUM_MAPFIELDS + NUM_MAPFLAGS + 1];

PyTypeObject Atrinik_MapType =
{
	PyObject_HEAD_INIT(NULL)

	/* ob_size */
	0,

	/* tp_name */
	"Atrinik.Map",

	/* tp_basicsize */
	sizeof(Atrinik_Map),

	/* tp_itemsize */
	0,

	/* tp_dealloc */
	(destructor)Atrinik_Map_dealloc,

	/* tp_print */
	0,

	/* tp_getattr */
	0,

	/* tp_setattr */
	0,

	/* tp_compare */
	0,

	/* tp_repr */
	0,

	/* tp_as_number */
	0,

	/* tp_as_sequence */
	0,

	/* tp_as_mapping */
	0,

	/* tp_hash */
	0,

	/* tp_call */
	0,

	/* tp_str */
	(reprfunc)Atrinik_Map_str,

	/* tp_getattro */
	0,

	/* tp_setattro */
	0,

	/* tp_as_buffer */
	0,

	/* tp_flags */
	Py_TPFLAGS_DEFAULT,

	/* tp_doc */
	"Atrinik maps",

	/* tp_traverse */
	0,

	/* tp_clear */
	0,

	/* tp_richcompare */
	0,

	/* tp_weaklistoffset */
	0,

	/* tp_iter */
	0,

	/* tp_iternext */
	0,

	/* tp_methods */
	MapMethods,

	/* tp_members */
	0,

	/* tp_getset */
	Map_getseters,

	/* tp_base */
	0,

	/* tp_dict */
	0,

	/* tp_descr_get */
	0,

	/* tp_descr_set */
	0,

	/* tp_dictoffset */
	0,

	/* tp_init */
	0,

	/* tp_alloc */
	0,

	/* tp_new */
	Atrinik_Map_new,

	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0
};

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
 * <h1>map.PlaySound(<i>\<int\></i> x, <i>\<int\></i> y, <i>\<int\></i>
 * soundnumber, <i>\<int\></i> soundtype)</h1>
 *
 * Play a sound on map.
 * @param x X position on the map where the sound is coming from
 * @param y Y position on the map where the sound is coming from
 * @param soundnumber ID of the sound to play
 * @param soundtype Type of the sound
 * @todo Supply constants for the sounds */
static PyObject *Atrinik_Map_PlaySound(Atrinik_Map *whereptr, PyObject *args)
{
	int x, y, soundnumber, soundtype;

	if (!PyArg_ParseTuple(args, "iiii", &x, &y, &soundnumber, &soundtype))
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
 * @param color Color of the message, default is @ref NDI_BLUE.
 * @todo Add constants for the colors */
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

/* Map attribute getter */
static PyObject *Map_GetAttribute(Atrinik_Map *map, int fieldno)
{
	void *field_ptr;

	if (fieldno < 0 || fieldno >= (int) NUM_MAPFIELDS)
	{
		RAISE("Illegal field ID");
	}

	field_ptr = (void *) ((char *) (map->map) + map_fields[fieldno].offset);

	/* TODO: better handling of types, signs, and overflows */
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
			RAISE("BUG: Unknown field type");
	}
}

/* Map flag getter */
static PyObject *Map_GetFlag(Atrinik_Map *map, int flagno)
{
	if (flagno < 0 || flagno >= (int) NUM_MAPFLAGS)
	{
		RAISE("Unknown flag");
	}

	return Py_BuildValue("i", (map->map->map_flags & (1 << flagno)) ? 1 : 0);
}

/* Initialize the Map Object Type */
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
			return -1;
		}
	}

	Atrinik_MapType.tp_new = PyType_GenericNew;

	if (PyType_Ready(&Atrinik_MapType) < 0)
	{
		return -1;
	}

#if 0
	Py_INCREF(&Atrinik_MapType);
#endif

	return 0;
}

/* Create a new (uninitialized) Map wrapper */
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

/* Free a Map wrapper */
static void Atrinik_Map_dealloc(Atrinik_Map *self)
{
	self->map = NULL;
	self->ob_type->tp_free((PyObject*) self);
}

/* Return a string representation of this map (useful for debugging) */
static PyObject *Atrinik_Map_str(Atrinik_Map *self)
{
	char buf[HUGE_BUF];

	strcpy(buf, self->map->name);

	return PyString_FromFormat("[%s \"%s\"]", self->map->path, buf);
}

/* Utility method to wrap a map. */
PyObject *wrap_map(mapstruct *what)
{
	Atrinik_Map *wrapper;

	/* Return None if no map was to be wrapped */
	if (what == NULL)
	{
		Py_INCREF(Py_None);

		return Py_None;
	}

	wrapper = PyObject_NEW(Atrinik_Map, &Atrinik_MapType);

	if (wrapper != NULL)
	{
		wrapper->map = what;
	}

	return (PyObject *) wrapper;
}
