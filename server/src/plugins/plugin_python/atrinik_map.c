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

/**
 * Map fields. */
/* @cparser
 * @page plugin_python_map_fields Python map fields
 * <h2>Python map fields</h2>
 * List of the map fields and their meaning. */
static fields_struct fields[] =
{
	{"name", FIELDTYPE_CSTR, offsetof(mapstruct, name), 0, 0},
	{"msg", FIELDTYPE_CSTR, offsetof(mapstruct, msg), 0, 0},
	{"reset_timeout", FIELDTYPE_UINT32, offsetof(mapstruct, reset_timeout), 0, 0},
	{"difficulty", FIELDTYPE_UINT16, offsetof(mapstruct, difficulty), 0, 0},
	{"height", FIELDTYPE_UINT16, offsetof(mapstruct, height), FIELDFLAG_READONLY, 0},
	{"width", FIELDTYPE_UINT16, offsetof(mapstruct, width), FIELDFLAG_READONLY, 0},
	{"darkness", FIELDTYPE_UINT8, offsetof(mapstruct, darkness), 0, 0},
	{"path", FIELDTYPE_SHSTR, offsetof(mapstruct, path), FIELDFLAG_READONLY, 0},
	{"enter_x", FIELDTYPE_UINT8, offsetof(mapstruct, enter_x), 0, 0},
	{"enter_y", FIELDTYPE_UINT8, offsetof(mapstruct, enter_y), 0, 0},
	{"region", FIELDTYPE_REGION, offsetof(mapstruct, region), FIELDFLAG_READONLY, 0},
	{"bg_music", FIELDTYPE_CSTR, offsetof(mapstruct, bg_music), 0, 0}
};
/* @endcparser */

/**
 * Map flags.
 *
 * @note These must be in same order as @ref map_flags "map flags". */
/* @cparser MAP_FLAG_(.*)
 * @page plugin_python_map_flags Python map flags
 * <h2>Python map flags</h2>
 * List of the map flags and their meaning. */
static char *mapflag_names[] =
{
	"f_outdoor", "f_unique", "f_fixed_rtime", "f_nomagic",
	"f_nopriest", "f_noharm", "f_nosummon", "f_fixed_login",
	"f_permdeath", "f_ultradeath", "f_ultimatedeath", "f_pvp",
	"f_no_save", "f_plugins"
};
/* @endcparser */

/** Number of map flags */
#define NUM_MAPFLAGS (sizeof(mapflag_names) / sizeof(mapflag_names[0]))

/**
 * @defgroup plugin_python_map_functions Python map functions
 * Map related functions used in Atrinik Python plugin.
 *@{*/

/**
 * <h1>map.GetFirstObject(<i>\<int\></i> x, <i>\<int\></i> y)</h1>
 *
 * Gets the first object on the tile. Use object::below to browse
 * objects.
 * @param x X position on the map.
 * @param y Y position on the map.
 * @return The object if found. */
static PyObject *Atrinik_Map_GetFirstObject(Atrinik_Map *map, PyObject *args)
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
		/* Since map objects are loaded in reverse mode, the last one in
		 * in the list is actually the first. */
		val = GET_MAP_OB_LAST(m, x, y);
	}

	return wrap_object(val);
}

/**
 * <h1>map.GetLastObject(<i>\<int\></i> x, <i>\<int\></i> y)</h1>
 *
 * Gets the last object on the tile. Use object::above to browse
 * objects.
 * @param x X position on the map.
 * @param y Y position on the map.
 * @return The object if found. */
static PyObject *Atrinik_Map_GetLastObject(Atrinik_Map *map, PyObject *args)
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
		/* Since map objects are loaded in reverse mode, the first one in
		 * in the list is actually the last. */
		val = GET_MAP_OB(m, x, y);
	}

	return wrap_object(val);
}

/**
 * <h1>map.GetMapFromCoord(<i>\<int\></i> x, <i>\<int\></i> y)</h1>
 * Get real coordinates from map, taking tiling into consideration.
 * @param x X position on the map.
 * @param y Y position on the map.
 * @return A tuple containing new map, new X, and new Y to use. The new
 * map can be None. */
static PyObject *Atrinik_Map_GetMapFromCoord(Atrinik_Map *map, PyObject *args, PyObject *keywds)
{
	int x, y;
	static char *kwlist[] = {"x", "y", NULL};
	mapstruct *m;
	PyObject *tuple;

	if (!PyArg_ParseTupleAndKeywords(args, keywds, "ii", kwlist, &x, &y))
	{
		return NULL;
	}

	m = hooks->get_map_from_coord(map->map, &x, &y);
	tuple = PyTuple_New(3);
	PyTuple_SET_ITEM(tuple, 0, wrap_map(m));
	PyTuple_SET_ITEM(tuple, 1, Py_BuildValue("i", x));
	PyTuple_SET_ITEM(tuple, 2, Py_BuildValue("i", y));

	return tuple;
}

/**
 * <h1>map.PlaySound(\<int\> x, \<int\> y, \<string\> filename, [int] type, [int] loop, [int] volume)</h1>
 * Play a sound on map.
 * @param x X position where the sound is playing from.
 * @param y Y position where the sound is playing from.
 * @param filename Sound file to play.
 * @param type Sound type being played, one of @ref CMD_SOUND_xxx. By
 * default, @ref CMD_SOUND_EFFECT is used.
 * @param loop How many times to loop the sound, -1 for infinite number.
 * @param volume Volume adjustment. */
static PyObject *Atrinik_Map_PlaySound(Atrinik_Map *whereptr, PyObject *args)
{
	int x, y, type = CMD_SOUND_EFFECT, loop = 0, volume = 0;
	char *filename;

	if (!PyArg_ParseTuple(args, "iis|iii", &x, &y, &filename, &type, &loop, &volume))
	{
		return NULL;
	}

	hooks->play_sound_map(whereptr->map, type, filename, x, y, loop, volume);

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

/**
 * <h1>map.Insert(object ob, int x, int y)</h1>
 * Insert the specified object on map, removing it first if necessary.
 * @param ob Object to insert.
 * @param x X coordinate where to insert 'ob'.
 * @param y Y coordinate where to insert 'ob'. */
static PyObject *Atrinik_Map_Insert(Atrinik_Map *map, PyObject *args, PyObject *keywds)
{
	Atrinik_Object *ob;
	sint16 x, y;
	static char *kwlist[] = {"ob", "x", "y", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, keywds, "O!hh", kwlist, &Atrinik_ObjectType, &ob, &x, &y))
	{
		return NULL;
	}

	if (!QUERY_FLAG(ob->obj, FLAG_REMOVED))
	{
		hooks->remove_ob(ob->obj);
		hooks->check_walk_off(ob->obj, NULL, MOVE_APPLY_VANISHED);
	}

	ob->obj->x = x;
	ob->obj->y = y;
	hooks->insert_ob_in_map(ob->obj, map->map, NULL, 0);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>map.Wall(int x, int y)</h1>
 * Checks if there's a wall on the specified square.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @return A combination of @ref map_look_flags. */
static PyObject *Atrinik_Map_Wall(Atrinik_Map *map, PyObject *args, PyObject *keywds)
{
	sint16 x, y;
	static char *kwlist[] = {"x", "y", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, keywds, "hh", kwlist, &x, &y))
	{
		return NULL;
	}

	return Py_BuildValue("i", hooks->wall(map->map, x, y));
}

/**
 * <h1>map.Blocked(object ob, int x, int y, int terrain)</h1>
 * Check if specified square is blocked for 'ob' using blocked().
 *
 * If you simply need to check if there's a wall on a square, you should use
 * @ref Atrinik_Map_Wall "map.Wall()" instead.
 * @param ob Object we're checking.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @param terrain Terrain object is allowed to go to. One (or combination) of
 * @ref terrain_type_flags, or <code>ob.terrain_flag</code>
 * @return A combination of @ref map_look_flags. */
static PyObject *Atrinik_Map_Blocked(Atrinik_Map *map, PyObject *args, PyObject *keywds)
{
	Atrinik_Object *ob;
	int x, y, terrain;
	mapstruct *m;
	static char *kwlist[] = {"ob", "x", "y", "terrain", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, keywds, "O!iii", kwlist, &Atrinik_ObjectType, &ob, &x, &y, &terrain))
	{
		return NULL;
	}

	if (!(m = hooks->get_map_from_coord(map->map, &x, &y)))
	{
		RAISE("Unable to get map using get_map_from_coord().");
	}

	return Py_BuildValue("i", hooks->blocked(ob->obj, m, x, y, terrain));
}

/*@}*/

/**
 * Get map's attribute.
 * @param map Python map wrapper.
 * @param context Void pointer to the field.
 * @return Python object with the attribute value, NULL on failure. */
static PyObject *get_attribute(Atrinik_Map *map, void *context)
{
	return generic_field_getter((fields_struct *) context, map->map);
}

/**
 * Set attribute of a map.
 * @param map Python map wrapper.
 * @param value Value to set.
 * @param context Void pointer to the field.
 * @return 0 on success, -1 on failure. */
static int set_attribute(Atrinik_Map *map, PyObject *value, void *context)
{
	if (generic_field_setter((fields_struct *) context, map->map, value) == -1)
	{
		return -1;
	}

	return 0;
}

/**
 * Get map's flag.
 * @param map Python map wrapper.
 * @param context Void pointer to the flag ID.
 * @return 1 if the map has the flag set, 0 otherwise. */
static PyObject *Map_GetFlag(Atrinik_Map *map, void *context)
{
	size_t flagno = (size_t) context;

	if (flagno >= NUM_MAPFLAGS)
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
	char buf[HUGE_BUF];

	snprintf(buf, sizeof(buf), "[%s \"%s\"]", self->map->path, self->map->name);
	return Py_BuildValue("s", buf);
}

/** Available Python methods for the AtrinikMap object */
static PyMethodDef MapMethods[] =
{
	{"GetFirstObject", (PyCFunction) Atrinik_Map_GetFirstObject, METH_VARARGS, 0},
	{"GetLastObject", (PyCFunction) Atrinik_Map_GetLastObject, METH_VARARGS, 0},
	{"PlaySound", (PyCFunction) Atrinik_Map_PlaySound, METH_VARARGS, 0},
	{"Message", (PyCFunction) Atrinik_Map_Message, METH_VARARGS, 0},
	{"GetMapFromCoord", (PyCFunction) Atrinik_Map_GetMapFromCoord, METH_VARARGS | METH_KEYWORDS, 0},
	{"CreateObject", (PyCFunction) Atrinik_Map_CreateObject, METH_VARARGS, 0},
	{"CountPlayers", (PyCFunction) Atrinik_Map_CountPlayers, METH_VARARGS, 0},
	{"GetPlayers", (PyCFunction) Atrinik_Map_GetPlayers, METH_VARARGS, 0},
	{"Insert", (PyCFunction) Atrinik_Map_Insert, METH_VARARGS | METH_KEYWORDS, 0},
	{"Wall", (PyCFunction) Atrinik_Map_Wall, METH_VARARGS | METH_KEYWORDS, 0},
	{"Blocked", (PyCFunction) Atrinik_Map_Blocked, METH_VARARGS | METH_KEYWORDS, 0},
	{NULL, NULL, 0, 0}
};

static int Atrinik_Map_InternalCompare(Atrinik_Map *left, Atrinik_Map *right)
{
	return left->map < right->map ? -1 : (left->map == right->map ? 0 : 1);
}

static PyObject *Atrinik_Map_RichCompare(Atrinik_Map *left, Atrinik_Map *right, int op)
{
	if (!left || !right || !PyObject_TypeCheck((PyObject *) left, &Atrinik_MapType) || !PyObject_TypeCheck((PyObject *) right, &Atrinik_MapType))
	{
		Py_INCREF(Py_NotImplemented);
		return Py_NotImplemented;
	}

	return generic_rich_compare(op, Atrinik_Map_InternalCompare(left, right));
}

/** This is filled in when we initialize our map type. */
static PyGetSetDef getseters[NUM_FIELDS + NUM_MAPFLAGS + 1];

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
	NULL, NULL, NULL,
#ifdef IS_PY3K
	NULL,
#else
	(cmpfunc) Atrinik_Map_InternalCompare,
#endif
	0, 0, 0, 0, 0, 0,
	(reprfunc) Atrinik_Map_str,
	0, 0, 0,
	Py_TPFLAGS_DEFAULT,
	"Atrinik maps",
	NULL, NULL,
	(richcmpfunc) Atrinik_Map_RichCompare,
	0, 0, 0,
	MapMethods,
	0,
	getseters,
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
	size_t i, flagno;

	/* Field getters */
	for (i = 0; i < NUM_FIELDS; i++)
	{
		PyGetSetDef *def = &getseters[i];

		def->name = fields[i].name;
		def->get = (getter) get_attribute;
		def->set = (setter) set_attribute;
		def->doc = NULL;
		def->closure = (void *) &fields[i];
	}

	/* Flag getters */
	for (flagno = 0; flagno < NUM_MAPFLAGS; flagno++)
	{
		PyGetSetDef *def = &getseters[i++];

		def->name = mapflag_names[flagno];
		def->get = (getter) Map_GetFlag;
		def->set = NULL;
		def->doc = NULL;
		def->closure = (void *) flagno;
	}

	getseters[i].name = NULL;

	Atrinik_MapType.tp_new = PyType_GenericNew;

	if (PyType_Ready(&Atrinik_MapType) < 0)
	{
		return 0;
	}

	Py_INCREF_TYPE(&Atrinik_MapType);
	PyModule_AddObject(module, "Map", (PyObject *) &Atrinik_MapType);

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
