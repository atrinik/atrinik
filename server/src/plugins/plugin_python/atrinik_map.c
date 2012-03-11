/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Crossfire (Multiplayer game for X-windows).                 *
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
	{"next", FIELDTYPE_MAP, offsetof(mapstruct, next), FIELDFLAG_READONLY, 0},
	{"previous", FIELDTYPE_MAP, offsetof(mapstruct, previous), FIELDFLAG_READONLY, 0},

	{"name", FIELDTYPE_CSTR, offsetof(mapstruct, name), 0, 0},
	{"msg", FIELDTYPE_CSTR, offsetof(mapstruct, msg), 0, 0},
	{"reset_timeout", FIELDTYPE_UINT32, offsetof(mapstruct, reset_timeout), 0, 0},
	{"timeout", FIELDTYPE_SINT32, offsetof(mapstruct, timeout), 0, 0},
	{"difficulty", FIELDTYPE_UINT16, offsetof(mapstruct, difficulty), 0, 0},
	{"height", FIELDTYPE_UINT16, offsetof(mapstruct, height), FIELDFLAG_READONLY, 0},
	{"width", FIELDTYPE_UINT16, offsetof(mapstruct, width), FIELDFLAG_READONLY, 0},
	{"darkness", FIELDTYPE_UINT8, offsetof(mapstruct, darkness), 0, 0},
	{"path", FIELDTYPE_SHSTR, offsetof(mapstruct, path), FIELDFLAG_READONLY, 0},
	{"enter_x", FIELDTYPE_UINT8, offsetof(mapstruct, enter_x), 0, 0},
	{"enter_y", FIELDTYPE_UINT8, offsetof(mapstruct, enter_y), 0, 0},
	{"region", FIELDTYPE_REGION, offsetof(mapstruct, region), FIELDFLAG_READONLY, 0},
	{"bg_music", FIELDTYPE_CSTR, offsetof(mapstruct, bg_music), 0, 0},
	{"weather", FIELDTYPE_CSTR, offsetof(mapstruct, weather), 0, 0}
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
	NULL, "f_noharm", "f_nosummon", "f_fixed_login",
	"f_player_no_save", "f_unused2", "f_unused3", "f_pvp",
	"f_no_save"
};
/* @endcparser */

/** Number of map flags */
#define NUM_MAPFLAGS (sizeof(mapflag_names) / sizeof(mapflag_names[0]))

/**
 * @defgroup plugin_python_map_functions Python map functions
 * Map related functions used in Atrinik Python plugin.
 *@{*/

/**
 * <h1>map.GetFirstObject(int x, int y)</h1>
 * Get the first object on the tile. Use object::below to browse objects.
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
 * <h1>map.GetLastObject(int x, int y)</h1>
 * Get the last object on the tile. Use object::above to browse objects.
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
 * <h1>map.GetLayer(int x, int y, int layer, int [sub_layer = -1])</h1>
 * Construct a list containing objects with the specified layer on the
 * specified square.
 *
 * Note that there is another way to loop through objects on a square:
 *
 * @code
for ob in WhoIsActivator().map.GetFirstObject(WhoIsActivator().x, WhoIsActivator().y):
	print(ob)
 * @endcode
 * @param x X coordinate on map.
 * @param y Y coordinate on map.
 * @param layer Layer we are looking for, should be one of @ref LAYER_xxx.
 * @param sub_layer Sub-layer to look for; if -1, will look for all sub-layers.
 * @throws ValueError if 'layer' is invalid.
 * @throws AtrinikError if there was an error trying to get the objects (invalid
 * x/y or not on nearby tiled map, for example).
 * @return A list containing objects on the square with the specified layer. */
static PyObject *Atrinik_Map_GetLayer(Atrinik_Map *map, PyObject *args)
{
	int x, y;
	uint8 layer;
	sint8 sub_layer = -1;
	mapstruct *m;
	PyObject *list;
	object *tmp;

	if (!PyArg_ParseTuple(args, "iib|B", &x, &y, &layer, &sub_layer))
	{
		return NULL;
	}

	/* Validate the layer ID. */
	if (layer > NUM_LAYERS)
	{
		PyErr_SetString(PyExc_ValueError, "map.GetLayer(): Invalid layer ID.");
		return NULL;
	}

	if (!(m = hooks->get_map_from_coord(map->map, &x, &y)))
	{
		RAISE("map.GetLayer(): Unable to get map using get_map_from_coord().");
	}

	list = PyList_New(0);

	FOR_MAP_LAYER_BEGIN(m, x, y, layer, sub_layer, tmp)
	{
		PyList_Append(list, wrap_object(tmp));
	}
	FOR_MAP_LAYER_END

	return list;
}

/**
 * <h1>map.GetMapFromCoord(int x, int y)</h1>
 * Get real coordinates from map, taking tiling into consideration.
 * @param x X position on the map.
 * @param y Y position on the map.
 * @return A tuple containing new map, new X, and new Y to use. The new
 * map can be None. */
static PyObject *Atrinik_Map_GetMapFromCoord(Atrinik_Map *map, PyObject *args)
{
	int x, y;
	mapstruct *m;
	PyObject *tuple;

	if (!PyArg_ParseTuple(args, "ii", &x, &y))
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
 * <h1>map.PlaySound(string filename, int x, int y, int [type = @ref CMD_SOUND_EFFECT], int [loop = 0], int [volume = 0])</h1>
 * Play a sound on map.
 * @param filename Sound file to play.
 * @param x X position where the sound is playing from.
 * @param y Y position where the sound is playing from.
 * @param type Sound type being played, one of @ref CMD_SOUND_xxx.
 * @param loop How many times to loop the sound, -1 for infinite number.
 * @param volume Volume adjustment. */
static PyObject *Atrinik_Map_PlaySound(Atrinik_Map *map, PyObject *args, PyObject *keywds)
{
	static char *kwlist[] = {"filename", "x", "y", "type", "loop", "volume", NULL};
	const char *filename;
	int x, y, type = CMD_SOUND_EFFECT, loop = 0, volume = 0;

	if (!PyArg_ParseTupleAndKeywords(args, keywds, "sii|iii", kwlist, &filename, &x, &y, &type, &loop, &volume))
	{
		return NULL;
	}

	hooks->play_sound_map(map->map, type, filename, x, y, loop, volume);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>map.DrawInfo(int x, int y, string message, int [color = @ref COLOR_BLUE], int [type = @ref CHAT_TYPE_GAME], string [name = None], int [distance = @ref MAP_INFO_NORMAL])</h1>
 * Send a message to all players on a map.
 * @param x X position on the map.
 * @param y Y position on the map.
 * @param message The message to send.
 * @param color Color to use for the message. Can be one of @ref COLOR_xxx
 * or an HTML color notation.
 * @param type One of @ref CHAT_TYPE_xxx.
 * @param global If True, the message will be broadcasted to all players.
 * @param name Player name that is the source of this message, if applicable.
 * @param distance Maximum distance for players to be away from x,y to
 * hear the message. */
static PyObject *Atrinik_Map_DrawInfo(Atrinik_Map *map, PyObject *args, PyObject *keywds)
{
	static char *kwlist[] = {"x", "y", "distance", "message", "color", "type", "name", NULL};
	int x, y, distance;
	const char *message, *color, *name;
	uint8 type;

	color = COLOR_BLUE;
	type = CHAT_TYPE_GAME;
	name = NULL;
	distance = MAP_INFO_NORMAL;

	if (!PyArg_ParseTupleAndKeywords(args, keywds, "iis|sbzi", kwlist, &x, &y, &message, &color, &type, &name, &distance))
	{
		return NULL;
	}

	hooks->draw_info_map(type, name, color, map->map, x, y, distance, NULL, NULL, message);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>map.CreateObject(string archname, int x, int y)</h1>
 * Create an object on map.
 * @param archname Arch name of the object to create.
 * @param x X position on the map.
 * @param y Y position on the map.
 * @throws AtrinikError if 'archname' is not a valid archetype.
 * @return The created object. */
static PyObject *Atrinik_Map_CreateObject(Atrinik_Map *map, PyObject *args)
{
	const char *archname;
	int x, y;
	archetype *arch;
	object *newobj;

	if (!PyArg_ParseTuple(args, "sii", &archname, &x, &y))
	{
		return NULL;
	}

	if (!(arch = hooks->find_archetype(archname)) || !(newobj = hooks->arch_to_object(arch)))
	{
		RAISE("map.CreateObject(): Invalid archetype.");
		return NULL;
	}

	newobj->x = x;
	newobj->y = y;
	newobj = hooks->insert_ob_in_map(newobj, map->map, NULL, 0);

	return wrap_object(newobj);
}

/**
 * <h1>map.CountPlayers()</h1>
 * Count number of players on map.
 * @return The number of players on the map. */
static PyObject *Atrinik_Map_CountPlayers(Atrinik_Map *map, PyObject *args)
{
	(void) args;

	return Py_BuildValue("i", hooks->players_on_map(map->map));
}

/**
 * <h1>map.GetPlayers()</h1>
 * Get all the players on a specified map.
 * @return List containing pointers to player objects on the map. */
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
static PyObject *Atrinik_Map_Insert(Atrinik_Map *map, PyObject *args)
{
	Atrinik_Object *obj;
	sint16 x, y;

	if (!PyArg_ParseTuple(args, "O!hh", &Atrinik_ObjectType, &obj, &x, &y))
	{
		return NULL;
	}

	OBJEXISTCHECK(obj);

	if (!QUERY_FLAG(obj->obj, FLAG_REMOVED))
	{
		hooks->object_remove(obj->obj, 0);
	}

	obj->obj->x = x;
	obj->obj->y = y;
	hooks->insert_ob_in_map(obj->obj, map->map, NULL, 0);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>map.Wall(int x, int y)</h1>
 * Checks if there's a wall on the specified square.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @return A combination of @ref map_look_flags. */
static PyObject *Atrinik_Map_Wall(Atrinik_Map *map, PyObject *args)
{
	sint16 x, y;

	if (!PyArg_ParseTuple(args, "hh", &x, &y))
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
 * @throws AtrinikError if there was a problem getting the map (as a
 * result of modified x/y to consider tiling, for example).
 * @return A combination of @ref map_look_flags. */
static PyObject *Atrinik_Map_Blocked(Atrinik_Map *map, PyObject *args)
{
	Atrinik_Object *ob;
	int x, y, terrain;
	mapstruct *m;

	if (!PyArg_ParseTuple(args, "O!iii", &Atrinik_ObjectType, &ob, &x, &y, &terrain))
	{
		return NULL;
	}

	OBJEXISTCHECK(ob);

	if (!(m = hooks->get_map_from_coord(map->map, &x, &y)))
	{
		RAISE("Unable to get map using get_map_from_coord().");
	}

	return Py_BuildValue("i", hooks->blocked(ob->obj, m, x, y, terrain));
}

/**
 * <h1>map.FreeSpot(object ob, int x, int y, int start, int stop)</h1>
 * Find first free spot around map at x, y.
 * @param ob Involved object - will be used to find the spot this object
 * could move onto.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @param start Start in the @ref freearr_x "freearr" arrays; 0 will also
 * check the tile at xy, 1 will start searching around xy.
 * @param stop Where to stop in the @ref freearr_x "freearr" arrays; one
 * of @ref size_of_free_defines.
 * @throws ValueError if either start or stop are not in a valid range.
 * @return -1 on error, ID of the spot in the @ref freearr_x "freearr"
 * arrays otherwise. */
static PyObject *Atrinik_Map_FreeSpot(Atrinik_Map *map, PyObject *args)
{
	Atrinik_Object *ob;
	int x, y, start, stop;
	mapstruct *m;

	if (!PyArg_ParseTuple(args, "O!iiii", &Atrinik_ObjectType, &ob, &x, &y, &start, &stop))
	{
		return NULL;
	}

	OBJEXISTCHECK(ob);

	if (start < 0 || stop < 0)
	{
		PyErr_SetString(PyExc_ValueError, "map.FreeSpot(): 'start' and 'stop' cannot be negative.");
		return NULL;
	}

	if (stop > SIZEOFFREE)
	{
		PyErr_SetString(PyExc_ValueError, "map.FreeSpot(): 'stop' cannot be higher than SIZEOFFREE.");
		return NULL;
	}

	if (!(m = hooks->get_map_from_coord(map->map, &x, &y)))
	{
		return Py_BuildValue("i", -1);
	}

	return Py_BuildValue("i", hooks->find_free_spot(ob->obj->arch, ob->obj, m, x, y, start, stop + 1));
}

/**
 * <h1>map.GetDarkness(int x, int y)</h1>
 * Gets the darkness value of the specified square.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @return The darkness value. */
static PyObject *Atrinik_Map_GetDarkness(Atrinik_Map *map, PyObject *args)
{
	int x, y;
	mapstruct *m;

	if (!PyArg_ParseTuple(args, "ii", &x, &y))
	{
		return NULL;
	}

	m = hooks->get_map_from_coord(map->map, &x, &y);

	if (!m)
	{
		RAISE("Unable to get map using get_map_from_coord().");
	}

	return Py_BuildValue("i", hooks->map_get_darkness(m, x, y, NULL));
}

/**
 * <h1>map.GetPath(string path, bool [unique = map.f_unique], string [name = None])</h1>
 * Construct a path based on the path of 'map', with 'path' appended.
 * @param unique If True, construct a unique path.
 * @param name If 'map' is not unique and 'unique' is True, this is required
 * to determine which player the unique map belongs to.
 * @return The created path. */
static PyObject *Atrinik_Map_GetPath(Atrinik_Map *map, PyObject *args)
{
	const char *path, *name;
	int unique;
	char *cp;
	PyObject *ret;

	unique = MAP_UNIQUE(map->map) ? 1 : 0;
	name = NULL;

	if (!PyArg_ParseTuple(args, "s|is", &path, &unique, &name))
	{
		return NULL;
	}

	cp = hooks->map_get_path(map->map, path, unique, name);
	ret = Py_BuildValue("s", cp);
	free(cp);

	return ret;
}

/**
 * <h1>map.LocateBeacon(string name)</h1>
 * Locate a beacon.
 * @param name The beacon name to find.
 * @return The beacon if found, None otherwise. */
static PyObject *Atrinik_Map_LocateBeacon(Atrinik_Map *map, PyObject *args)
{
	const char *name;
	shstr *beacon_name = NULL;
	object *myob;

	if (!PyArg_ParseTuple(args, "s", &name))
	{
		return NULL;
	}

	if (MAP_UNIQUE(map->map))
	{
		char *filedir, *pl_name, *joined;

		filedir = hooks->path_dirname(map->map->path);
		pl_name = hooks->path_basename(filedir);
		joined = hooks->string_join("-", "/", pl_name, name, NULL);

		FREE_AND_COPY_HASH(beacon_name, joined);

		free(joined);
		free(pl_name);
		free(filedir);
	}
	else
	{
		FREE_AND_COPY_HASH(beacon_name, name);
	}

	myob = hooks->beacon_locate(beacon_name);
	FREE_AND_CLEAR_HASH(beacon_name);

	return wrap_object(myob);
}

/*@}*/

/** Available Python methods for the AtrinikMap object */
static PyMethodDef MapMethods[] =
{
	{"GetFirstObject", (PyCFunction) Atrinik_Map_GetFirstObject, METH_VARARGS, 0},
	{"GetLastObject", (PyCFunction) Atrinik_Map_GetLastObject, METH_VARARGS, 0},
	{"GetLayer", (PyCFunction) Atrinik_Map_GetLayer, METH_VARARGS, 0},
	{"GetMapFromCoord", (PyCFunction) Atrinik_Map_GetMapFromCoord, METH_VARARGS, 0},
	{"PlaySound", (PyCFunction) Atrinik_Map_PlaySound, METH_VARARGS | METH_KEYWORDS, 0},
	{"DrawInfo", (PyCFunction) Atrinik_Map_DrawInfo, METH_VARARGS | METH_KEYWORDS, 0},
	{"CreateObject", (PyCFunction) Atrinik_Map_CreateObject, METH_VARARGS, 0},
	{"CountPlayers", (PyCFunction) Atrinik_Map_CountPlayers, METH_NOARGS, 0},
	{"GetPlayers", (PyCFunction) Atrinik_Map_GetPlayers, METH_NOARGS, 0},
	{"Insert", (PyCFunction) Atrinik_Map_Insert, METH_VARARGS, 0},
	{"Wall", (PyCFunction) Atrinik_Map_Wall, METH_VARARGS, 0},
	{"Blocked", (PyCFunction) Atrinik_Map_Blocked, METH_VARARGS, 0},
	{"FreeSpot", (PyCFunction) Atrinik_Map_FreeSpot, METH_VARARGS, 0},
	{"GetDarkness", (PyCFunction) Atrinik_Map_GetDarkness, METH_VARARGS, 0},
	{"GetPath", (PyCFunction) Atrinik_Map_GetPath, METH_VARARGS, 0},
	{"LocateBeacon", (PyCFunction) Atrinik_Map_LocateBeacon, METH_VARARGS, 0},
	{NULL, NULL, 0, 0}
};

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

	if (((fields_struct *) context)->offset == offsetof(mapstruct, darkness))
	{
		hooks->set_map_darkness(map->map, map->map->darkness);
	}

	return 0;
}

/**
 * Get map's flag.
 * @param map Python map wrapper.
 * @param context Void pointer to the flag ID.
 * @retval Py_True The map has the flag set.
 * @retval Py_False The map doesn't have the flag set.
 * @retval NULL An error occurred. */
static PyObject *Map_GetFlag(Atrinik_Map *map, void *context)
{
	size_t flagno = (size_t) context;

	/* Should not happen. */
	if (flagno >= NUM_MAPFLAGS)
	{
		PyErr_SetString(PyExc_OverflowError, "Invalid flag ID.");
		return NULL;
	}

	Py_ReturnBoolean(map->map->map_flags & (1 << flagno));
}

/**
 * Set map's flag.
 * @param map Python map wrapper.
 * @param val Value to set. Should be either Py_True or Py_False.
 * @param context Void pointer to the flag ID.
 * @return 0 on success, -1 on failure. */
static int Map_SetFlag(Atrinik_Map *map, PyObject *val, void *context)
{
	size_t flagno = (size_t) context;

	/* Should not happen. */
	if (flagno >= NUM_MAPFLAGS)
	{
		PyErr_SetString(PyExc_OverflowError, "Invalid flag ID.");
		return -1;
	}

	if (val == Py_True)
	{
		map->map->map_flags |= (1U << flagno);
	}
	else if (val == Py_False)
	{
		map->map->map_flags &= ~(1U << flagno);
	}
	else
	{
		PyErr_SetString(PyExc_TypeError, "Flag value must be either True or False.");
		return -1;
	}

	return 0;
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
		def->set = (setter) Map_SetFlag;
		def->doc = NULL;
		def->closure = (void *) flagno;
	}

	getseters[i].name = NULL;

	Atrinik_MapType.tp_new = PyType_GenericNew;

	if (PyType_Ready(&Atrinik_MapType) < 0)
	{
		return 0;
	}

	Py_INCREF(&Atrinik_MapType);
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
