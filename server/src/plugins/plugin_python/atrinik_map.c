/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
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
 * Atrinik Python plugin map related code.
 *
 * @author Alex Tokar
 * @author Yann Chachkoff
 */

#include <plugin_python.h>

/**
 * Map fields.
 */
static fields_struct fields[] = {
    {"next", FIELDTYPE_MAP, offsetof(mapstruct, next), FIELDFLAG_READONLY, 0,
            "Next map in a doubly-linked list.; Atrinik.Map.Map or None "
            "(readonly)"},
    {"previous", FIELDTYPE_MAP, offsetof(mapstruct, prev), FIELDFLAG_READONLY,
            0, "Previous map in a doubly-linked list.; Atrinik.Map.Map or None "
            "(readonly)"},

    {"name", FIELDTYPE_SHSTR, offsetof(mapstruct, name), 0, 0,
            "Name of the map.; str or None"},
    {"msg", FIELDTYPE_CSTR, offsetof(mapstruct, msg), 0, 0,
            "Message map creator may have left.; str or None"},
    {"reset_timeout", FIELDTYPE_UINT32, offsetof(mapstruct, reset_timeout), 0,
            0, "How many seconds must elapse before this map should be reset.; "
            "int"},
    {"timeout", FIELDTYPE_INT32, offsetof(mapstruct, timeout), 0, 0,
            "When this reaches 0, the map will be swapped out.; int"},
    {"difficulty", FIELDTYPE_UINT16, offsetof(mapstruct, difficulty), 0, 0,
            "What level the player should be to play here. Affects treasures, "
            "random shops and various other things.; int"},
    {"height", FIELDTYPE_UINT16, offsetof(mapstruct, height),
            FIELDFLAG_READONLY, 0, "Height of the map.; int"},
    {"width", FIELDTYPE_UINT16, offsetof(mapstruct, width),
            FIELDFLAG_READONLY, 0, "Width of the map.; int"},
    {"darkness", FIELDTYPE_UINT8, offsetof(mapstruct, darkness), 0, 0,
            "Indicates the base light value on this map. This value is only "
            "used when the map is not marked as outdoor.; int"},
    {"path", FIELDTYPE_SHSTR, offsetof(mapstruct, path), FIELDFLAG_READONLY, 0,
            "Path to the map file.; str (readonly)"},
    {"enter_x", FIELDTYPE_UINT8, offsetof(mapstruct, enter_x), 0, 0,
            "Used to indicate the X position of where to put the player when "
            "he logs in to the map if the map has :attr:`f_fixed_login` set.;"
            "int"},
    {"enter_y", FIELDTYPE_UINT8, offsetof(mapstruct, enter_y), 0, 0,
            "Used to indicate the Y position of where to put the player when "
            "he logs in to the map if the map has :attr:`f_fixed_login` set.;"
            "int"},
    {"region", FIELDTYPE_REGION, offsetof(mapstruct, region),
            FIELDFLAG_READONLY, 0, "Region the map is in.; "
            "Atrinik.Region.Region or None (readonly)"},
    {"bg_music", FIELDTYPE_SHSTR, offsetof(mapstruct, bg_music), 0, 0,
            "Background music of the map.; str or None"},
    {"weather", FIELDTYPE_SHSTR, offsetof(mapstruct, weather), 0, 0,
            "Weather of the map.; str or None"}
};

/**
 * Map flags.
 *
 * @note These must be in same order as @ref map_flags "map flags".
 */
static char *mapflag_names[] = {
    "f_outdoor", "f_unique", "f_fixed_rtime", "f_nomagic",
    "f_height_diff", "f_noharm", "f_nosummon", "f_fixed_login",
    "f_player_no_save", NULL, NULL, NULL, "f_pvp",
    "f_no_save"
};

/** Docstrings for #mapflag_names. */
static char *mapflag_docs[] = {
    "Whether the map is an outdoor map.",
    "Special unique map.",
    "If true, reset time is not affected by players entering/leaving the map.",
    "No spells.",
    "Height difference will be taken into account when rendering the map.",
    "No harmful spells like fireball, magic bullet, etc.",
    "Don't allow any summoning spells.",
    "When set, a player login on this map will force the player to enter the "
            "specified :attr:`~Atrinik.Map.enter_x` "
            ":attr:`~Atrinik.Map.enter_y` coordinates",
    "Players cannot save on this map.",
    NULL, NULL, NULL,
    "PvP is possible on this map.",
    "Don't save the map - only used with unique maps.",
};

/** Number of map flags */
#define NUM_MAPFLAGS (sizeof(mapflag_names) / sizeof(mapflag_names[0]))

/** Documentation for Atrinik_Map_Objects(). */
static const char doc_Atrinik_Map_Objects[] =
".. method:: Objects(x, y).\n\n"
"Iterate objects on the specified map square, from first to last, eg::\n\n"
"    for obj in Atrinik.WhoIsActivator().map.Objects(0, 0):\n"
"        print(obj)"
"\n\n"
":param x: X coordinate on the map.\n"
":type x: int\n"
":param y: Y coordinate on the map.\n"
":type y: int\n"
":returns: Iterator object.\n"
":rtype: :class:`Atrinik.Object.ObjectIterator`";

/**
 * Implements Atrinik.Map.Map.Objects() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Map_Objects(Atrinik_Map *self, PyObject *args)
{
    int x, y;

    if (!PyArg_ParseTuple(args, "ii", &x, &y)) {
        return NULL;
    }

    mapstruct *m = hooks->get_map_from_coord(self->map, &x, &y);
    if (m != NULL) {
        /* Since map objects are loaded in reverse mode, the last one in
         * in the list is actually the first. */
        return wrap_object_iterator(GET_MAP_OB_LAST(m, x, y));
    }

    return wrap_object_iterator(NULL);
}

/** Documentation for Atrinik_Map_ObjectsReversed(). */
static const char doc_Atrinik_Map_ObjectsReversed[] =
".. method:: ObjectsReversed(x, y).\n\n"
"Iterate objects on the specified map square, from last to first, eg::\n\n"
"    for obj in Atrinik.WhoIsActivator().map.ObjectsReversed(0, 0):\n"
"        print(obj)"
"\n\n"
":param x: X coordinate on the map.\n"
":type x: int\n"
":param y: Y coordinate on the map.\n"
":type y: int\n"
":returns: Iterator object.\n"
":rtype: :class:`Atrinik.Object.ObjectIterator`";

/**
 * Implements Atrinik.Map.Map.ObjectsReversed() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Map_ObjectsReversed(Atrinik_Map *self, PyObject *args)
{
    int x, y;

    if (!PyArg_ParseTuple(args, "ii", &x, &y)) {
        return NULL;
    }

    mapstruct *m = hooks->get_map_from_coord(self->map, &x, &y);
    if (m != NULL) {
        /* Since map objects are loaded in reverse mode, the first one in
         * in the list is actually the last. */
        return wrap_object_iterator(GET_MAP_OB(m, x, y));
    }

    return wrap_object_iterator(NULL);
}

/** Documentation for Atrinik_Map_GetLayer(). */
static const char doc_Atrinik_Map_GetLayer[] =
".. method:: GetLayer(x, y, layer, sub_layer=-1).\n\n"
"Construct a list containing objects with the specified layer on the specified "
"square.\n\nNote that there is another way to loop through objects on a "
"square, which is less memory intensive, and can be stopped at any time with a "
"break::\n\n"
"    for obj in Atrinik.WhoIsActivator().map.Objects(0, 0):\n"
"        print(obj)"
"\n\n"
":param x: X coordinate on the map.\n"
":type x: int\n"
":param y: Y coordinate on the map.\n"
":type y: int\n"
":param layer: Layer to look for - one of LAYER_xxx constants, eg, :attr:"
"`~Atrinik.LAYER_WALL`.\n"
":type layer: int\n"
":param sub_layer: Sub-layer to look for; if -1, will look for all "
"sub-layers.\n"
":type sub_layer: int\n"
":returns: A list containing objects on the square with the specified layer.\n"
":rtype: list of :class:`Atrinik.Object.Object`\n"
":raises ValueError: If invalid layer ID was specified.\n"
":raises Atrinik.AtrinikError: If there was an error trying to get the "
"objects (invalid X/Y, or not on a nearby tiled map, for example).";

/**
 * Implements Atrinik.Map.Map.GetLayer() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Map_GetLayer(Atrinik_Map *self, PyObject *args)
{
    int x, y;
    uint8_t layer;
    int8_t sub_layer = -1;

    if (!PyArg_ParseTuple(args, "iib|B", &x, &y, &layer, &sub_layer)) {
        return NULL;
    }

    /* Validate the layer ID. */
    if (layer > NUM_LAYERS) {
        PyErr_SetString(PyExc_ValueError, "Invalid layer ID.");
        return NULL;
    }

    mapstruct *m = hooks->get_map_from_coord(self->map, &x, &y);
    if (m == NULL) {
        RAISE("Unable to get map using get_map_from_coord().");
    }

    PyObject *list = PyList_New(0);

    object *tmp;
    FOR_MAP_LAYER_BEGIN(m, x, y, layer, sub_layer, tmp) {
        PyList_Append(list, wrap_object(tmp));
    } FOR_MAP_LAYER_END

    return list;
}

/** Documentation for Atrinik_Map_GetMapFromCoord(). */
static const char doc_Atrinik_Map_GetMapFromCoord[] =
".. method:: GetMapFromCoord(x, y).\n\n"
"Get real coordinates from map, taking tiling into consideration.\n\n"
":param x: X coordinate on the map.\n"
":type x: int\n"
":param y: Y coordinate on the map.\n"
":type y: int\n"
":returns: A tuple containing new map, new X, and new Y to use. The new map "
"can be None.\n"
":rtype: tuple";

/**
 * Implements Atrinik.Map.Map.GetMapFromCoord() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Map_GetMapFromCoord(Atrinik_Map *self, PyObject *args)
{
    int x, y;

    if (!PyArg_ParseTuple(args, "ii", &x, &y)) {
        return NULL;
    }

    mapstruct *m = hooks->get_map_from_coord(self->map, &x, &y);
    PyObject *tuple = PyTuple_New(3);
    PyTuple_SET_ITEM(tuple, 0, wrap_map(m));
    PyTuple_SET_ITEM(tuple, 1, Py_BuildValue("i", x));
    PyTuple_SET_ITEM(tuple, 2, Py_BuildValue("i", y));

    return tuple;
}

/** Documentation for Atrinik_Map_PlaySound(). */
static const char doc_Atrinik_Map_PlaySound[] =
".. method:: PlaySound(filename, x, y, type=Atrinik.CMD_SOUND_EFFECT, loop=0, "
"volume=0).\n\n"
"Play a sound on the map.\n\n"
":param filename: Sound file to play.\n"
":type filename: str\n"
":param x: X position where the sound is playing from.\n"
":type x: int\n"
":param y: Y position where the sound is playing from.\n"
":type y: int\n"
":param type: Sound type to play, one of the CMD_SOUND_xxx constants, eg, "
":attr:`~Atrinik.CMD_SOUND_BACKGROUND`.\n"
":type type: int\n"
":param loop: How many times to loop the sound, -1 to loop infinitely.\n"
":type loop: int\n"
":param volume: Volume adjustment.\n"
":type volume: int";

/**
 * Implements Atrinik.Map.Map.PlaySound() Python method.
 * @copydoc PyMethod_VARARGS_KEYWORDS
 */
static PyObject *Atrinik_Map_PlaySound(Atrinik_Map *self, PyObject *args,
        PyObject *keywds)
{
    static char *kwlist[] = {
        "filename", "x", "y", "type", "loop", "volume", NULL
    };
    const char *filename;
    int x, y, type = CMD_SOUND_EFFECT, loop = 0, volume = 0;

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "sii|iii", kwlist,
            &filename, &x, &y, &type, &loop, &volume)) {
        return NULL;
    }

    hooks->play_sound_map(self->map, type, filename, x, y, loop, volume);

    Py_INCREF(Py_None);
    return Py_None;
}

/** Documentation for Atrinik_Map_DrawInfo(). */
static const char doc_Atrinik_Map_DrawInfo[] =
".. method:: DrawInfo(x, y, message, color=Atrinik.COLOR_BLUE, "
"type=Atrinik.CHAT_TYPE_GAME, name=None, distance=Atrinik.MAP_INFO_NORMAL).\n\n"
"Send a message to all players on the map.\n\n"
":param x: X position on the map.\n"
":type x: int\n"
":param y: Y position on the map.\n"
":type y: int\n"
":param message: The message to send.\n"
":type message: str\n"
":param color: Color to use for the message. Can be one of the COLOR_xxx "
"constants (eg, :attr:`~Atrinik.COLOR_RED`) or a regular HTML color notation "
"(eg, '00ff00')\n"
":type color: str\n"
":param type: One of the CHAT_TYPE_xxx constants, eg, :attr:"
"`~Atrinik.CHAT_TYPE_CHAT`.\n"
":type type: int\n"
":param name: Player name that is the source of this message, if applicable.\n"
":type name: str or None\n"
":param distance: Maximum distance for players to be away from *x*, *y* to "
"hear the message.\n"
":type distance: int";

/**
 * Implements Atrinik.Map.Map.DrawInfo() Python method.
 * @copydoc PyMethod_VARARGS_KEYWORDS
 */
static PyObject *Atrinik_Map_DrawInfo(Atrinik_Map *self, PyObject *args,
        PyObject *keywds)
{
    static char *kwlist[] = {
        "x", "y", "message", "color", "type", "name", "distance", NULL
    };
    int x, y, distance;
    const char *message, *color, *name;
    uint8_t type;

    color = COLOR_BLUE;
    type = CHAT_TYPE_GAME;
    name = NULL;
    distance = MAP_INFO_NORMAL;

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "iis|sbzi", kwlist, &x, &y,
            &message, &color, &type, &name, &distance)) {
        return NULL;
    }

    hooks->draw_info_map(type, name, color, self->map, x, y, distance, NULL,
            NULL, message);

    Py_INCREF(Py_None);
    return Py_None;
}

/** Documentation for Atrinik_Map_CreateObject(). */
static const char doc_Atrinik_Map_CreateObject[] =
".. method:: CreateObject(archname, x, y).\n\n"
"Create an object on the map.\n\n"
":param archname: Arch name of the object to create.\n"
":type archname: str\n"
":param x: X position on the map.\n"
":type x: int\n"
":param y: Y position on the map.\n"
":type y: int\n"
":returns: The created object.\n"
":rtype: :class:`Atrinik.Object.Object`\n"
":raises Atrinik.AtrinikError: If *archname* is not a valid archetype.";

/**
 * Implements Atrinik.Map.Map.CreateObject() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Map_CreateObject(Atrinik_Map *self, PyObject *args)
{
    const char *archname;
    int x, y;

    if (!PyArg_ParseTuple(args, "sii", &archname, &x, &y)) {
        return NULL;
    }

    archetype_t *arch = hooks->arch_find(archname);
    if (arch == NULL) {
        RAISE("Invalid archetype.");
        return NULL;
    }

    object *newobj = hooks->arch_to_object(arch);
    newobj->x = x;
    newobj->y = y;
    newobj = hooks->insert_ob_in_map(newobj, self->map, NULL, 0);

    return wrap_object(newobj);
}

/** Documentation for Atrinik_Map_CountPlayers(). */
static const char doc_Atrinik_Map_CountPlayers[] =
".. method:: CountPlayers().\n\n"
"Count number of players on map.\n\n"
":returns: The number of players on the map.\n"
":rtype: int";

/**
 * Implements Atrinik.Map.Map.CountPlayers() Python method.
 * @copydoc PyMethod_NOARGS
 */
static PyObject *Atrinik_Map_CountPlayers(Atrinik_Map *self)
{
    return Py_BuildValue("i", hooks->players_on_map(self->map));
}

/** Documentation for Atrinik_Map_GetPlayers(). */
static const char doc_Atrinik_Map_GetPlayers[] =
".. method:: GetPlayers().\n\n"
"Get all the players on a specified map.\n\n"
":returns: List containing player objects on the map.\n"
":rtype: :attr:`list` of :class:`Atrinik.Object.Object`";

/**
 * Implements Atrinik.Map.Map.GetPlayers() Python method.
 * @copydoc PyMethod_NOARGS
 */
static PyObject *Atrinik_Map_GetPlayers(Atrinik_Map *self)
{
    PyObject *list = PyList_New(0);

    for (object *tmp = self->map->player_first; tmp != NULL;
            tmp = CONTR(tmp)->map_above) {
        PyList_Append(list, wrap_object(tmp));
    }

    return list;
}

/** Documentation for Atrinik_Map_Insert(). */
static const char doc_Atrinik_Map_Insert[] =
".. method:: Insert(obj, x, y).\n\n"
"Insert the specified object on map, removing it first if necessary.\n\n"
":param obj: Object to insert.\n"
":type obj: :class:`Atrinik.Object.Object`\n"
":param x: X position on the map.\n"
":type x: int\n"
":param y: Y position on the map.\n"
":type y: int\n"
":returns: The inserted object. Can be None on failure, or different from "
"*obj* in case of merging.\n"
":rtype: :class:`Atrinik.Object.Object` or None";

/**
 * Implements Atrinik.Map.Map.CreateObject() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Map_Insert(Atrinik_Map *self, PyObject *args)
{
    Atrinik_Object *obj;
    int16_t x, y;

    if (!PyArg_ParseTuple(args, "O!hh", &Atrinik_ObjectType, &obj, &x, &y)) {
        return NULL;
    }

    OBJEXISTCHECK(obj);

    if (!QUERY_FLAG(obj->obj, FLAG_REMOVED)) {
        hooks->object_remove(obj->obj, 0);
    }

    obj->obj->x = x;
    obj->obj->y = y;
    return wrap_object(hooks->insert_ob_in_map(obj->obj, self->map, NULL, 0));
}

/** Documentation for Atrinik_Map_Wall(). */
static const char doc_Atrinik_Map_Wall[] =
".. method:: Wall(x, y).\n\n"
"Checks if there's a wall on the specified square.\n\n"
":param x: X position on the map.\n"
":type x: int\n"
":param y: Y position on the map.\n"
":type y: int\n"
":returns: A combination of P_xxx, eg, :attr:`~Atrinik.P_NO_PASS`. Zero means "
"the square is not blocked by anything.\n"
":rtype: int";

/**
 * Implements Atrinik.Map.Map.Wall() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Map_Wall(Atrinik_Map *self, PyObject *args)
{
    int16_t x, y;

    if (!PyArg_ParseTuple(args, "hh", &x, &y)) {
        return NULL;
    }

    return Py_BuildValue("i", hooks->wall(self->map, x, y));
}

/** Documentation for Atrinik_Map_Blocked(). */
static const char doc_Atrinik_Map_Blocked[] =
".. method:: Blocked(obj, x, y, terrain).\n\n"
"Check if specified square is blocked for *obj*.\n\nIf you simply need to "
"check if there's a wall on a square, you should use :meth:"
"`~Atrinik.Map.Map.Wall` instead.\n\n"
":param obj: Object to check.\n"
":type obj: :class:`Atrinik.Object.Object`\n"
":param x: X position on the map.\n"
":type x: int\n"
":param y: Y position on the map.\n"
":type y: int\n"
":param terrain: Terrain the object is allowed to go to. One (or a "
"combination) of the TERRAIN_xxx constants, eg, :attr:"
"`~Atrinik.TERRAIN_AIRBREATH`. The object's :attr:"
"`~Atrinik.Object.Object.terrain_flag` can be used for this purpose.\n"
":type terrain: int\n"
":returns: A combination of P_xxx, eg, :attr:`~Atrinik.P_NO_PASS`. Zero means "
"the square is not blocked for the object.\n"
":rtype: int\n"
":raises: Atrinik.AtrinikError: If there was a problem resolving the specified "
"X/Y coordinates to a tiled map (if they were outside the map).";

/**
 * Implements Atrinik.Map.Map.Blocked() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Map_Blocked(Atrinik_Map *self, PyObject *args)
{
    Atrinik_Object *obj;
    int x, y, terrain;

    if (!PyArg_ParseTuple(args, "O!iii", &Atrinik_ObjectType, &obj, &x, &y,
            &terrain)) {
        return NULL;
    }

    OBJEXISTCHECK(obj);

    mapstruct *m = hooks->get_map_from_coord(self->map, &x, &y);
    if (m == NULL) {
        RAISE("Unable to get map using get_map_from_coord().");
    }

    return Py_BuildValue("i", hooks->blocked(obj->obj, m, x, y, terrain));
}

/** Documentation for Atrinik_Map_FreeSpot(). */
static const char doc_Atrinik_Map_FreeSpot[] =
".. method:: FreeSpot(obj, x, y, start, stop).\n\n"
"Find first free spot around map at *x*, *y*. The resulting value can be used "
"as an index to both free spot arrays; :attr:`Atrinik.freearr_x` and "
":attr:`Atrinik.freearr_y`.\n\n"
":param obj: Involved object - will be used to find the spot this object could "
"move onto.\n"
":type obj: :class:`Atrinik.Object.Object`\n"
":param x: X position on the map.\n"
":type x: int\n"
":param y: Y position on the map.\n"
":type y: int\n"
":param start: Start in the free spot arrays; 0 will also check the tile at "
"*x*, *y*, whereas 1 will start searching only around *x*, *y*.\n"
":type start: int\n"
":param stop: Where to stop in the free spot arrays; one of the SIZEOFFREEx "
"constants, eg, :attr:`~Atrinik.SIZEOFFREE1`."
":type stop: int\n"
":returns: -1 on error, index to the free spot arrays otherwise.\n"
":rtype: int\n"
":raises: ValueError: If either *start* or *stop* are not in a valid range.";

/**
 * Implements Atrinik.Map.Map.FreeSpot() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Map_FreeSpot(Atrinik_Map *self, PyObject *args)
{
    Atrinik_Object *obj;
    int x, y, start, stop;

    if (!PyArg_ParseTuple(args, "O!iiii", &Atrinik_ObjectType, &obj, &x, &y,
            &start, &stop)) {
        return NULL;
    }

    OBJEXISTCHECK(obj);

    if (start < 0 || stop < 0) {
        PyErr_SetString(PyExc_ValueError,
                "'start' and 'stop' cannot be negative.");
        return NULL;
    }

    if (stop >= SIZEOFFREE) {
        PyErr_SetString(PyExc_ValueError,
                "'stop' cannot be higher than or equal to SIZEOFFREE.");
        return NULL;
    }

    mapstruct *m = hooks->get_map_from_coord(self->map, &x, &y);
    if (m == NULL) {
        return Py_BuildValue("i", -1);
    }

    return Py_BuildValue("i", hooks->find_free_spot(obj->obj->arch, obj->obj,
            m, x, y, start, stop));
}

/** Documentation for Atrinik_Map_GetDarkness(). */
static const char doc_Atrinik_Map_GetDarkness[] =
".. method:: GetDarkness(x, y).\n\n"
"Gets the darkness value of the specified square.\n\n"
":param x: X position on the map.\n"
":type x: int\n"
":param y: Y position on the map.\n"
":type y: int\n"
":returns: The darkness value.\n"
":rtype: int\n"
":raises: Atrinik.AtrinikError: If there was a problem resolving the specified "
"X/Y coordinates to a tiled map (if they were outside the map).";

/**
 * Implements Atrinik.Map.Map.GetDarkness() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Map_GetDarkness(Atrinik_Map *self, PyObject *args)
{
    int x, y;

    if (!PyArg_ParseTuple(args, "ii", &x, &y)) {
        return NULL;
    }

    mapstruct *m = hooks->get_map_from_coord(self->map, &x, &y);
    if (m == NULL) {
        RAISE("Unable to get map using get_map_from_coord().");
    }

    return Py_BuildValue("i", hooks->map_get_darkness(m, x, y, NULL));
}

/** Documentation for Atrinik_Map_GetPath(). */
static const char doc_Atrinik_Map_GetPath[] =
".. method:: GetPath(path=None, unique=None, name=None).\n\n"
"Construct a path based on the path of the map, with *path* appended.\n\n"
":param path: Path to append. If None, will append the filename of the map "
"instead.\n"
":type path: str or None\n"
":param unique: Whether to construct a unique path. If None, unique flag of "
"the map will be used.\n"
":type unique: bool or None\n"
":param name: If the map is not unique and *unique* is True, this is required "
"to determine which player the unique map path should belong to.\n"
":type name: str or None\n"
":returns: The created path.\n"
":rtype: str";

/**
 * Implements Atrinik.Map.Map.GetPath() Python method.
 * @copydoc PyMethod_VARARGS_KEYWORDS
 */
static PyObject *Atrinik_Map_GetPath(Atrinik_Map *self, PyObject *args,
        PyObject *keywds)
{
    static char *kwlist[] = {"path", "unique", "name", NULL};
    const char *path, *name;
    int unique;

    path = NULL;
    unique = MAP_UNIQUE(self->map) != 0;
    name = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "|zis", kwlist, &path,
            &unique, &name)) {
        return NULL;
    }

    char *cp = hooks->map_get_path(self->map, path, unique, name);
    PyObject *ret = Py_BuildValue("s", cp);
    efree(cp);

    return ret;
}

/** Documentation for Atrinik_Map_LocateBeacon(). */
static const char doc_Atrinik_Map_LocateBeacon[] =
".. method:: LocateBeacon(name).\n\n"
"Locate a beacon.\n\n"
":param name: The beacon name to find.\n"
":type name: str\n"
":returns: The beacon object if found, None otherwise.\n"
":rtype: :class:`Atrinik.Object.Object` or None";

/**
 * Implements Atrinik.Map.Map.LocateBeacon() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Map_LocateBeacon(Atrinik_Map *self, PyObject *args)
{
    const char *name;

    if (!PyArg_ParseTuple(args, "s", &name)) {
        return NULL;
    }

    shstr *beacon_name;
    if (MAP_UNIQUE(self->map)) {
        /* The map is unique, so we need to do some work to create a unique
         * beacon name from the map's unique path. */
        char *filedir = hooks->path_dirname(self->map->path);
        char *pl_name = hooks->path_basename(filedir);
        char *joined = hooks->string_join("-", "/", pl_name, name, NULL);

        beacon_name = hooks->add_string(joined);

        efree(joined);
        efree(pl_name);
        efree(filedir);
    } else {
        beacon_name = hooks->add_string(name);
    }

    object *obj = hooks->beacon_locate(beacon_name);
    hooks->free_string_shared(beacon_name);

    return wrap_object(obj);
}

/** Documentation for Atrinik_Map_Redraw(). */
static const char doc_Atrinik_Map_Redraw[] =
".. method:: Redraw(x, y, layer=-1, sub_layer=-1).\n\n"
"Redraw the specified tile for all players.\n\n"
":param x: X position on the map.\n"
":type x: int\n"
":param y: Y position on the map.\n"
":type y: int\n"
":param layer: Layer to redraw. If -1, will redraw all layers.\n"
":type layer: int\n"
":param sub_layer: Sub-layer to redraw. If -1, will redraw all sub-layers.\n"
":type sub_layer: int\n"
":raises Atrinik.AtrinikError: If either coordinates are not within the map.\n"
":raises Atrinik.AtrinikError: If *layer* is not within a valid range.\n"
":raises Atrinik.AtrinikError: If *sub_layer* is not within a valid range.";

/**
 * Implements Atrinik.Map.Map.Redraw() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Map_Redraw(Atrinik_Map *self, PyObject *args)
{
    int x, y, layer, sub_layer;

    layer = -1;
    sub_layer = -1;

    if (!PyArg_ParseTuple(args, "ii|ii", &x, &y, &layer, &sub_layer)) {
        return NULL;
    }

    if (x < 0 || x >= self->map->width) {
        RAISE("Invalid X coordinate.");
    }

    if (y < 0 || y >= self->map->height) {
        RAISE("Invalid X coordinate.");
    }

    if (layer < -1 || layer > NUM_LAYERS) {
        RAISE("Invalid layer.");
    }

    if (sub_layer < -1 || sub_layer >= NUM_SUB_LAYERS) {
        RAISE("Invalid sub-layer.");
    }

    hooks->map_redraw(self->map, x, y, layer, sub_layer);

    Py_INCREF(Py_None);
    return Py_None;
}

/** Documentation for Atrinik_Map_Save(). */
static const char doc_Atrinik_Map_Save[] =
".. method:: Save().\n\n"
"Saves the specified map.\n\n";

/**
 * Implements Atrinik.Map.Map.Save() Python method.
 * @copydoc PyMethod_NOARGS
 */
static PyObject *Atrinik_Map_Save(Atrinik_Map *self)
{
    hooks->new_save_map(self->map, 0);

    Py_INCREF(Py_None);
    return Py_None;
}

/** Available Python methods for the AtrinikMap object */
static PyMethodDef MapMethods[] = {
    {"Objects", (PyCFunction) Atrinik_Map_Objects, METH_VARARGS,
            doc_Atrinik_Map_Objects},
    {"ObjectsReversed", (PyCFunction) Atrinik_Map_ObjectsReversed, METH_VARARGS,
            doc_Atrinik_Map_ObjectsReversed},
    {"GetLayer", (PyCFunction) Atrinik_Map_GetLayer, METH_VARARGS,
            doc_Atrinik_Map_GetLayer},
    {"GetMapFromCoord", (PyCFunction) Atrinik_Map_GetMapFromCoord, METH_VARARGS,
            doc_Atrinik_Map_GetMapFromCoord},
    {"PlaySound", (PyCFunction) Atrinik_Map_PlaySound,
            METH_VARARGS | METH_KEYWORDS, doc_Atrinik_Map_PlaySound},
    {"DrawInfo", (PyCFunction) Atrinik_Map_DrawInfo,
            METH_VARARGS | METH_KEYWORDS, doc_Atrinik_Map_DrawInfo},
    {"CreateObject", (PyCFunction) Atrinik_Map_CreateObject, METH_VARARGS,
            doc_Atrinik_Map_CreateObject},
    {"CountPlayers", (PyCFunction) Atrinik_Map_CountPlayers, METH_NOARGS,
            doc_Atrinik_Map_CountPlayers},
    {"GetPlayers", (PyCFunction) Atrinik_Map_GetPlayers, METH_NOARGS,
            doc_Atrinik_Map_GetPlayers},
    {"Insert", (PyCFunction) Atrinik_Map_Insert, METH_VARARGS,
            doc_Atrinik_Map_Insert},
    {"Wall", (PyCFunction) Atrinik_Map_Wall, METH_VARARGS,
            doc_Atrinik_Map_Wall},
    {"Blocked", (PyCFunction) Atrinik_Map_Blocked, METH_VARARGS,
            doc_Atrinik_Map_Blocked},
    {"FreeSpot", (PyCFunction) Atrinik_Map_FreeSpot, METH_VARARGS,
            doc_Atrinik_Map_FreeSpot},
    {"GetDarkness", (PyCFunction) Atrinik_Map_GetDarkness, METH_VARARGS,
            doc_Atrinik_Map_GetDarkness},
    {"GetPath", (PyCFunction) Atrinik_Map_GetPath, METH_VARARGS | METH_KEYWORDS,
            doc_Atrinik_Map_GetPath},
    {"LocateBeacon", (PyCFunction) Atrinik_Map_LocateBeacon, METH_VARARGS,
            doc_Atrinik_Map_LocateBeacon},
    {"Redraw", (PyCFunction) Atrinik_Map_Redraw, METH_VARARGS,
            doc_Atrinik_Map_Redraw},
    {"Save", (PyCFunction) Atrinik_Map_Save, METH_NOARGS,
            doc_Atrinik_Map_Save},

    {NULL, NULL, 0, NULL}
};

/**
 * Get map's attribute.
 * @param map
 * Python map wrapper.
 * @param context
 * Void pointer to the field.
 * @return
 * Python object with the attribute value, NULL on failure.
 */
static PyObject *get_attribute(Atrinik_Map *map, void *context)
{
    return generic_field_getter(context, map->map);
}

/**
 * Set attribute of a map.
 * @param map
 * Python map wrapper.
 * @param value
 * Value to set.
 * @param context
 * Void pointer to the field.
 * @return
 * 0 on success, -1 on failure.
 */
static int set_attribute(Atrinik_Map *map, PyObject *value, void *context)
{
    if (generic_field_setter(context, map->map, value) == -1) {
        return -1;
    }

    if (((fields_struct *) context)->offset == offsetof(mapstruct, darkness)) {
        hooks->set_map_darkness(map->map, map->map->darkness);
    }

    return 0;
}

/**
 * Get map's flag.
 * @param map
 * Python map wrapper.
 * @param context
 * Void pointer to the flag ID.
 * @retval Py_True The map has the flag set.
 * @retval Py_False The map doesn't have the flag set.
 * @retval NULL An error occurred.
 */
static PyObject *Map_GetFlag(Atrinik_Map *map, void *context)
{
    size_t flagno = (size_t) context;

    /* Should not happen. */
    if (flagno >= NUM_MAPFLAGS) {
        PyErr_SetString(PyExc_OverflowError, "Invalid flag ID.");
        return NULL;
    }

    return Py_BuildBoolean(map->map->map_flags & (1 << flagno));
}

/**
 * Set map's flag.
 * @param map
 * Python map wrapper.
 * @param val
 * Value to set. Should be either Py_True or Py_False.
 * @param context
 * Void pointer to the flag ID.
 * @return
 * 0 on success, -1 on failure.
 */
static int Map_SetFlag(Atrinik_Map *map, PyObject *val, void *context)
{
    size_t flagno = (size_t) context;

    /* Should not happen. */
    if (flagno >= NUM_MAPFLAGS) {
        PyErr_SetString(PyExc_OverflowError, "Invalid flag ID.");
        return -1;
    }

    if (val == Py_True) {
        map->map->map_flags |= (1U << flagno);
    } else if (val == Py_False) {
        map->map->map_flags &= ~(1U << flagno);
    } else {
        PyErr_SetString(PyExc_TypeError,
                "Flag value must be either True or False.");
        return -1;
    }

    return 0;
}

/**
 * Create a new map wrapper.
 * @param type
 * Type object.
 * @param args
 * Unused.
 * @param kwds
 * Unused.
 * @return
 * The new wrapper.
 */
static PyObject *Atrinik_Map_new(PyTypeObject *type, PyObject *args,
        PyObject *kwds)
{
    Atrinik_Map *self = (Atrinik_Map *) type->tp_alloc(type, 0);
    if (self != NULL) {
        self->map = NULL;
    }

    return (PyObject *) self;
}

/**
 * Free a map wrapper.
 * @param self
 * The wrapper to free.
 */
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
 * @param self
 * The map object.
 * @return
 * Python object containing the map path and name of the map.
 */
static PyObject *Atrinik_Map_str(Atrinik_Map *self)
{
    char buf[HUGE_BUF];
    snprintf(VS(buf), "[%s \"%s\"]", self->map->path, self->map->name);
    return Py_BuildValue("s", buf);
}

static int Atrinik_Map_InternalCompare(Atrinik_Map *left, Atrinik_Map *right)
{
    return left->map < right->map ? -1 : (left->map == right->map ? 0 : 1);
}

static PyObject *Atrinik_Map_RichCompare(Atrinik_Map *left, Atrinik_Map *right,
        int op)
{
    if (left == NULL || right == NULL ||
            !PyObject_TypeCheck((PyObject *) left, &Atrinik_MapType) ||
            !PyObject_TypeCheck((PyObject *) right, &Atrinik_MapType)) {
        Py_INCREF(Py_NotImplemented);
        return Py_NotImplemented;
    }

    return generic_rich_compare(op, Atrinik_Map_InternalCompare(left, right));
}

/** This is filled in when we initialize our map type. */
static PyGetSetDef getseters[NUM_FIELDS + NUM_MAPFLAGS + 1];

/** Our actual Python MapType. */
PyTypeObject Atrinik_MapType = {
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
#ifdef Py_TPFLAGS_HAVE_FINALIZE
    , NULL
#endif
};

/**
 * Initialize the map wrapper.
 * @param module
 * The Atrinik Python module.
 * @return
 * 1 on success, 0 on failure.
 */
int Atrinik_Map_init(PyObject *module)
{
    size_t i, flagno;

    /* Field getters */
    for (i = 0; i < NUM_FIELDS; i++) {
        PyGetSetDef *def = &getseters[i];

        def->name = fields[i].name;
        def->get = (getter) get_attribute;
        def->set = (setter) set_attribute;
        def->doc = fields[i].doc;
        def->closure = &fields[i];
    }

    /* Flag getters */
    for (flagno = 0; flagno < NUM_MAPFLAGS; flagno++) {
        PyGetSetDef *def;

        if (mapflag_names[flagno] == NULL) {
            continue;
        }

        def = &getseters[i++];

        def->name = mapflag_names[flagno];
        def->get = (getter) Map_GetFlag;
        def->set = (setter) Map_SetFlag;
        def->doc = mapflag_docs[flagno];
        def->closure = (void *) flagno;
    }

    getseters[i].name = NULL;

    Atrinik_MapType.tp_new = PyType_GenericNew;

    if (PyType_Ready(&Atrinik_MapType) < 0) {
        return 0;
    }

    Py_INCREF(&Atrinik_MapType);
    PyModule_AddObject(module, "Map", (PyObject *) &Atrinik_MapType);

    return 1;
}

/**
 * Utility method to wrap a map.
 * @param what
 * Map to wrap.
 * @return
 * Python object wrapping the real map.
 */
PyObject *wrap_map(mapstruct *what)
{
    /* Return None if no map was to be wrapped. */
    if (what == NULL) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    Atrinik_Map *wrapper = PyObject_NEW(Atrinik_Map, &Atrinik_MapType);
    if (wrapper != NULL) {
        wrapper->map = what;
    }

    return (PyObject *) wrapper;
}
