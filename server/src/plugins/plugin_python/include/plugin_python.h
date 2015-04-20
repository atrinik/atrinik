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
 * Python plugin related header file. */

#ifndef PLUGIN_PYTHON_H
#define PLUGIN_PYTHON_H

#include <Python.h>

#define GLOBAL_NO_PROTOTYPES
#include <global.h>

/** This is for allowing both python 3 and python 2. */
#if PY_MAJOR_VERSION >= 3
#define IS_PY3K
#else
#if PY_MINOR_VERSION >= 6
#define IS_PY26
#else
#define IS_PY_LEGACY
#endif
#if PY_MINOR_VERSION >= 5
#define IS_PY25
#endif
#endif

/* Fake some Python 2.x functions for Python 3.x. */
#ifdef IS_PY3K
#define PyString_Check PyUnicode_Check
#define PyString_AsString _PyUnicode_AsString
#define PyInt_Check PyLong_Check
#define PyInt_AsLong PyLong_AsLong
extern PyTypeObject PyIOBase_Type;
#define PyFile_Check(op) (PyObject_IsInstance((op), (PyObject *) &PyIOBase_Type))
#define PyString_FromFormat PyUnicode_FromFormat
#else
#define PyObject_AsFileDescriptor(op) (PyFile_AsFile((op)) ? PyFile_AsFile((op))->fd : -1)
#endif

/** Name of the plugin. */
#define PLUGIN_NAME "Python"
/** Name of the plugin, and its version. */
#define PLUGIN_VERSION "Atrinik Python Plugin 1.0"

extern struct plugin_hooklist *hooks;

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

/**
 * @defgroup INVENTORY_xxx Modes for object.FindObject()
 * Different modes for object.FindObject().
 *@{*/
/** Search only inside the inventory of the object. */
#define INVENTORY_ONLY 0
/**
 * Search inside the inventory of the object and inventories of
 * containers. */
#define INVENTORY_CONTAINERS 1
/**
 * Search inside the inventory of the object, inventories of
 * containers and inventories of other objects. */
#define INVENTORY_ALL 2
/*@}*/

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

#undef SET_ANIMATION
#define SET_ANIMATION(ob, newanim) ob->face = &(*hooks->new_faces)[(*hooks->animations)[ob->animation_id].faces[newanim]]
#undef NUM_ANIMATIONS
#define NUM_ANIMATIONS(ob) ((*hooks->animations)[ob->animation_id].num_animations)
#undef NUM_FACINGS
#define NUM_FACINGS(ob) ((*hooks->animations)[ob->animation_id].facings)

#undef emalloc
#undef efree
#undef ecalloc
#undef erealloc
#undef ereallocz
#undef estrdup
#undef estrndup

#ifndef NDEBUG
#define emalloc(_size) hooks->memory_emalloc(_size, __FILE__, __LINE__)
#define efree(_ptr) hooks->memory_efree(_ptr, __FILE__, __LINE__)
#define ecalloc(_nmemb, _size) \
    hooks->memory_ecalloc(_nmemb, _size, __FILE__, __LINE__)
#define erealloc(_ptr, _size) \
    hooks->memory_erealloc(_ptr, _size, __FILE__, __LINE__)
#define ereallocz(_ptr, _old_size, _new_size) \
    hooks->memory_reallocz(_ptr, _old_size, _new_size, __FILE__, __LINE__)
#define estrdup(_s) hooks->string_estrdup(_s, __FILE__, __LINE__)
#define estrndup(_s, _n) hooks->string_estrndup(_s, _n, __FILE__, __LINE__)
#else
#define emalloc(_size) hooks->memory_emalloc(_size)
#define efree(_ptr) hooks->memory_efree(_ptr)
#define ecalloc(_nmemb, _size) hooks->memory_ecalloc(_nmemb, _size)
#define erealloc(_ptr, _size) hooks->memory_erealloc(_ptr, _size)
#define ereallocz(_ptr, _old_size, _new_size) \
    hooks->memory_reallocz(_ptr, _old_size, _new_size)
#define estrdup(_s) hooks->string_estrdup(_s)
#define estrndup(_s, _n) hooks->string_estrndup(_s, _n)
#endif

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
typedef struct _pythoncontext {
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
typedef struct {
    /** Name of the constant. */
    const char *name;

    /** Value of the constant. */
    const long value;
} Atrinik_Constant;

/** Types used in objects and maps structs. */
typedef enum {
    /** Pointer to shared string. */
    FIELDTYPE_SHSTR,
    /** Pointer to C string. */
    FIELDTYPE_CSTR,
    /** C string (array directly in struct). */
    FIELDTYPE_CARY,
    /** Unsigned int8. */
    FIELDTYPE_UINT8,
    /** Signed int8. */
    FIELDTYPE_INT8,
    /** Unsigned int16. */
    FIELDTYPE_UINT16,
    /** Signed int16. */
    FIELDTYPE_INT16,
    /** Unsigned int32. */
    FIELDTYPE_UINT32,
    /** Signed int32. */
    FIELDTYPE_INT32,
    /** Unsigned int64. */
    FIELDTYPE_UINT64,
    /** Signed int64. */
    FIELDTYPE_INT64,
    /** Float. */
    FIELDTYPE_FLOAT,
    /** Pointer to object. */
    FIELDTYPE_OBJECT,
    /** Object. */
    FIELDTYPE_OBJECT2,
    /** Pointer to map. */
    FIELDTYPE_MAP,
    /** Object pointer + tag. */
    FIELDTYPE_OBJECTREF,
    /** Pointer to region. */
    FIELDTYPE_REGION,
    /** Pointer to a party. */
    FIELDTYPE_PARTY,
    /** Pointer to an archetype. */
    FIELDTYPE_ARCH,
    /** Pointer to a player. */
    FIELDTYPE_PLAYER,
    /** Face pointer. */
    FIELDTYPE_FACE,
    /**
     * Animation ID. The field is actually uint16, but the result is a
     * tuple containing the animation name and the animation ID. */
    FIELDTYPE_ANIMATION,
    /** uint8 that only accepts True/False. */
    FIELDTYPE_BOOLEAN,
    /** AttrList field type; the field is an array. */
    FIELDTYPE_LIST,
    /** Player's command permissions. */
    FIELDTYPE_CMD_PERMISSIONS,
    /** Player's faction reputations. */
    FIELDTYPE_FACTIONS,
    /** Object's connection value. */
    FIELDTYPE_CONNECTION,
    /** Treasure list. */
    FIELDTYPE_TREASURELIST
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
typedef struct Atrinik_Object {
    PyObject_HEAD

    /** Pointer to the Atrinik object we wrap. */
    object *obj;

    /** Pointer for iteration. */
    struct Atrinik_Object *iter;

    /** @ref OBJ_ITER_TYPE_xxx "Iteration type". */
    uint8_t iter_type;

    /** ID of the object. */
    tag_t count;
} Atrinik_Object;

PyTypeObject Atrinik_MapType;
PyObject *wrap_map(mapstruct *map);
int Atrinik_Map_init(PyObject *module);

/** The Atrinik_Map structure. */
typedef struct {
    PyObject_HEAD
    /** Pointer to the Atrinik map we wrap. */
    mapstruct *map;
} Atrinik_Map;

PyTypeObject Atrinik_PartyType;
PyObject *wrap_party(party_struct *party);
int Atrinik_Party_init(PyObject *module);

/** The Atrinik_Party structure. */
typedef struct {
    PyObject_HEAD
    /** Pointer to the Atrinik party we wrap. */
    party_struct *party;
} Atrinik_Party;

PyTypeObject Atrinik_RegionType;
PyObject *wrap_region(region_struct *region);
int Atrinik_Region_init(PyObject *module);

/** The Atrinik_Region structure. */
typedef struct {
    PyObject_HEAD
    /** Pointer to the Atrinik region we wrap. */
    region_struct *region;
} Atrinik_Region;

PyTypeObject Atrinik_PlayerType;
PyObject *wrap_player(player *pl);
int Atrinik_Player_init(PyObject *module);

/** The Atrinik_Player structure. */
typedef struct {
    PyObject_HEAD
    /** Pointer to the Atrinik player we wrap. */
    player *pl;
} Atrinik_Player;

PyTypeObject Atrinik_ArchetypeType;
PyObject *wrap_archetype(archetype *at);
int Atrinik_Archetype_init(PyObject *module);

/** The Atrinik_Archetype structure. */
typedef struct {
    PyObject_HEAD
    /** Pointer to the Atrinik archetype we wrap. */
    archetype *at;
} Atrinik_Archetype;

PyTypeObject Atrinik_AttrListType;
PyObject *wrap_attr_list(void *ptr, size_t offset, field_type field);
int Atrinik_AttrList_init(PyObject *module);

/** The Atrinik_AttrList structure. */
typedef struct {
    PyObject_HEAD

    /** Pointer to the structure the array is in. */
    void *ptr;

    /** Where in the structure the array is. */
    size_t offset;

    /**
     * Type of the array being handled; for example,
     * @ref FIELDTYPE_FACTIONS. */
    field_type field;

    /** Used to keep track of iteration index. */
    unsigned PY_LONG_LONG iter;
} Atrinik_AttrList;

/** One cache entry. */
typedef struct python_cache_entry {
    /** The script file. */
    char *file;

    /** The cached code. */
    PyCodeObject *code;

    /** Last cached time. */
    time_t cached_time;

    /** Hash handle. */
    UT_hash_handle hh;
} python_cache_entry;

/**
 * General structure for Python object fields. */
typedef struct {
    /**
     * Name of the field. */
    char *name;

    /**
     * Field type. */
    field_type type;

    /**
     * Offset in player structure. */
    size_t offset;

    /**
     * Flags for special handling. */
    uint32_t flags;

    /**
     * Extra data for some special fields. */
    uint32_t extra_data;
} fields_struct;

/**
 * Get number of fields in the fields array.
 * @return Number of fields. */
#define NUM_FIELDS (sizeof(fields) / sizeof(fields[0]))

#define OBJEXISTCHECK_INT(ob) \
    { \
        if (!(ob) || !(ob)->obj || (ob)->obj->count != (ob)->count || OBJECT_FREE((ob)->obj)) \
        { \
            PyErr_SetString(PyExc_ReferenceError, "Atrinik object no longer exists."); \
            return -1; \
        } \
    }

#define OBJEXISTCHECK(ob) \
    { \
        if (!(ob) || !(ob)->obj || (ob)->obj->count != (ob)->count || OBJECT_FREE((ob)->obj)) \
        { \
            PyErr_SetString(PyExc_ReferenceError, "Atrinik object no longer exists."); \
            return NULL; \
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

/**
 * Returns Py_True (increasing its reference) if 'val' is non-NULL, otherwise
 * returns Py_False. */
#define Py_ReturnBoolean(val) \
    { \
        if ((val)) \
        { \
            Py_INCREF(Py_True); \
            return Py_True; \
        } \
        else \
        { \
            Py_INCREF(Py_False); \
            return Py_False; \
        } \
    }

int generic_field_setter(fields_struct *field, void *ptr, PyObject *value);
PyObject *generic_field_getter(fields_struct *field, void *ptr);
PyObject *generic_rich_compare(int op, int result);
int python_call_int(PyObject *callable, PyObject *arglist);

#endif
