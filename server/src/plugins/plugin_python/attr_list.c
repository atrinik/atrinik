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
 * Implements AttrList Python object; this is a class similar to Python
 * lists, but with more limited functionality (it does not implement many
 * methods normal lists do; however, the AttrList object can be converted
 * to a list).
 *
 * These objects are used to wrap an array of data, for example,
 * player::known_spells array, which holds the spell IDs the player knows.
 * When Python requests to get player.known_spells, the code generates an
 * AttrList object, which "wraps" the actual array of known spells - it
 * holds a pointer to the structure, the offset in the structure, etc.
 * Thus, it is possible for Python to use __setitem__() and __getitem__()
 * methods on this AttrList object to change/extract the array data in the
 * most efficient way possible; all of the data of the array is never held
 * in memory (unless you implicitly convert it to a list, of course).
 *
 * The code uses many, many castings and does extensive error checking
 * of passed arguments, as the smallest imperfection could cause a memory
 * corruption, most likely resulting in a crash.
 *
 * If you are adding more field types to handle your array, you will need
 * to modify the following functions:
 *
 * - attr_list_len_ptr(): should return a properly cast pointer to the
 *   current length of the array
 * - attr_list_len(): should use the above, but cast the void pointer back
 *   into the proper type
 * - attr_list_get(): should have proper casting and configured field type
 *   for generic_field_getter()
 * - attr_list_set(): similar to the above, but with error checking, array
 *   resizing (in case of dynamic arrays), maximum array size checking
 *   (static sized arrays), etc.
 *
 * @author Alex Tokar */

#include <plugin_python.h>

/**
 * Get a pointer to integer that holds the maximum length depending on the
 * AttrList's field type.
 * @param al The AttrList pointer.
 * @return Pointer to the integer holding the maximum length. Must be cast
 * to the correct type before usage. */
static void *attr_list_len_ptr(Atrinik_AttrList *al)
{
    if (al->field == FIELDTYPE_CMD_PERMISSIONS) {
        return (char *) al->ptr + offsetof(player, num_cmd_permissions);
    }
    else if (al->field == FIELDTYPE_FACTIONS) {
        return (char *) al->ptr + offsetof(player, num_faction_ids);
    }
    else if (al->field == FIELDTYPE_REGION_MAPS) {
        return (char *) al->ptr + offsetof(player, num_region_maps);
    }

    /* Not reached. */
    return NULL;
}

/**
 * Similar to attr_list_len_ptr(), but does not return a pointer.
 * @param al AttrList being used.
 * @return The length of the provided AttrList. */
static unsigned PY_LONG_LONG attr_list_len(Atrinik_AttrList *al)
{
    if (al->field == FIELDTYPE_CMD_PERMISSIONS || al->field == FIELDTYPE_FACTIONS || al->field == FIELDTYPE_REGION_MAPS) {
        return *(int *) attr_list_len_ptr(al);
    }

    return 0;
}

/**
 * Get value from an AttrList object.
 * @param al The AttrList object.
 * @param key Index of the value to get. Can be NULL, in which case 'idx' or
 * 'str' will be used instead, depending on the type.
 * @param idx Index of the value to get.
 * @param str String index of the value to get.
 * @return 0 on success, -1 on failure. */
static PyObject *attr_list_get(Atrinik_AttrList *al, PyObject *key, unsigned PY_LONG_LONG idx, const char *str)
{
    void *ptr;
    fields_struct field = {"xxx", 0, 0, 0, 0};

    ptr = (char *) al->ptr + al->offset;

    if (al->field == FIELDTYPE_CMD_PERMISSIONS || al->field == FIELDTYPE_REGION_MAPS) {
        if (key) {
            idx = PyLong_AsUnsignedLongLong(key);
        }

        field.type = FIELDTYPE_CSTR;
        ptr = &(*(char ***) ptr)[idx];
    }
    else if (al->field == FIELDTYPE_FACTIONS) {
        unsigned PY_LONG_LONG len, i;

        if (key) {
            str = PyString_AsString(key);
        }

        len = attr_list_len(al);
        field.type = FIELDTYPE_SINT64;

        for (i = 0; i < len; i++) {
            if (!strcmp(str, *(const char **) (&(*(shstr ***) ptr)[i]))) {
                ptr = &(*(sint64 **) ((void *) ((char *) al->ptr + offsetof(player, faction_reputation))))[i];
                return generic_field_getter(&field, ptr);
            }
        }

        Py_INCREF(Py_None);
        return Py_None;
    }

    return generic_field_getter(&field, ptr);
}

/**
 * Check if the provided AttrList contains 'value'.
 * @param al The AttrList object.
 * @param value The value to try and find.
 * @return 1 if the value was found, 0 otherwise. */
static int attr_list_contains(Atrinik_AttrList *al, PyObject *value)
{
    unsigned PY_LONG_LONG i, len;
    PyObject *check;

    if (al->field == FIELDTYPE_FACTIONS) {
        PyErr_SetString(PyExc_NotImplementedError, "Attribute list does not implement contains method.");
        return -1;
    }

    len = attr_list_len(al);

    for (i = 0; i < len; i++) {
        /* attr_list_get() creates a new reference, so make sure to decrease
         * it later. */
        check = attr_list_get(al, NULL, i, NULL);

        /* Compare the two objects... */
        if (PyObject_RichCompareBool(check, value, Py_EQ) == 1) {
            Py_DECREF(check);
            return 1;
        }

        Py_DECREF(check);
    }

    return 0;
}

/**
 * Set new value for an array member that AttrList object is wrapping.
 * @param al The AttrList object.
 * @param key Index of the value to get. Can be NULL, in which case 'idx' will
 * be used instead.
 * @param idx Index of the value to get.
 * @param value New value to set.
 * @return 0 on success, -1 on failure. */
static int attr_list_set(Atrinik_AttrList *al, PyObject *key, unsigned PY_LONG_LONG idx, PyObject *value)
{
    unsigned PY_LONG_LONG len;
    void *ptr;
    fields_struct field = {"xxx", 0, 0, 0, 0};
    int ret;
    unsigned PY_LONG_LONG i;

    /* Get the current length of the list. */
    len = attr_list_len(al);
    ptr = (char *) al->ptr + al->offset;

    /* Command permissions. */
    if (al->field == FIELDTYPE_CMD_PERMISSIONS || al->field == FIELDTYPE_REGION_MAPS) {
        i = key ? PyLong_AsUnsignedLongLong(key) : idx;

        /* Over the maximum size; resize the array, as it's dynamic. */
        if (i >= len) {
            /* Increase the number of commands... */
            (*(int *) attr_list_len_ptr(al))++;
            /* And resize it. */
            *(char ***) ((void *) ((char *) al->ptr + al->offset)) = realloc(*(char ***) ((void *) ((char *) al->ptr + al->offset)), sizeof(char *) * attr_list_len(al));
            /* Make sure ptr points to the right memory... */
            ptr = (char *) al->ptr + al->offset;
            /* NULL the new member. */
            (*(char ***) ptr)[i] = NULL;
        }

        field.type = FIELDTYPE_CSTR;
        ptr = &(*(char ***) ptr)[i];
    }
    /* Factions. */
    else if (al->field == FIELDTYPE_FACTIONS) {
        char *str;

        str = PyString_AsString(key);
        field.type = FIELDTYPE_SINT64;

        /* Try to find an existing entry. */
        for (i = 0; i < len; i++) {
            if (!strcmp(str, *(const char **) (&(*(shstr ***) ptr)[i]))) {
                break;
            }
        }

        /* Doesn't exist, create it. */
        if (i == len) {
            (*(int *) attr_list_len_ptr(al))++;
            *(shstr ***) ((void *) ((char *) al->ptr + al->offset)) = realloc(*(shstr ***) ((void *) ((char *) al->ptr + al->offset)), sizeof(shstr *) * attr_list_len(al));
            *(shstr **) (&(*(shstr ***) ptr)[i]) = hooks->add_string((const char *) (char *) idx);
            *(sint64 **) ((void *) ((char *) al->ptr + offsetof(player, faction_reputation))) = realloc(*(sint64 **) ((void *) ((char *) al->ptr + offsetof(player, faction_reputation))), sizeof(sint64) * attr_list_len(al));
            /* Make sure ptr points to the right memory... */
            ptr = (char *) al->ptr + offsetof(player, faction_reputation);
            /* NULL the new member. */
            (*(sint64 **) ptr)[i] = 0;
        }

        ptr = &(*(sint64 **) ((void *) ((char *) al->ptr + offsetof(player, faction_reputation))))[i];
    }
    else {
        PyErr_SetString(PyExc_NotImplementedError, "The attribute list does not implement support for write operations.");
        return -1;
    }

    ret = generic_field_setter(&field, ptr, value);

    /* Success! */
    if (ret == 0) {
        if (al->field == FIELDTYPE_CMD_PERMISSIONS) {
            ((socket_struct *) (&(*(socket_struct **) ((void *) ((char *) al->ptr + offsetof(player, socket))))))->ext_title_flag = 1;
        }
    }
    /* Failure; overflow, invalid value or some other kind of error. */
    else if (ret == -1) {
        if (i >= len) {
            /* We tried to add a new command permission and we have already
             * resized the array, so shrink it back now, as we failed. */
            if (al->field == FIELDTYPE_CMD_PERMISSIONS || al->field == FIELDTYPE_REGION_MAPS) {
                /* Decrease the number of commands... */
                (*(int *) attr_list_len_ptr(al))--;
                /* And resize it. */
                *(char ***) ((void *) ((char *) al->ptr + al->offset)) = realloc(*(char ***) ((void *) ((char *) al->ptr + al->offset)), sizeof(char *) * attr_list_len(al));
            }
            else if (al->field == FIELDTYPE_FACTIONS) {
                (*(int *) attr_list_len_ptr(al))--;
                *(shstr ***) ((void *) ((char *) al->ptr + al->offset)) = realloc(*(shstr ***) ((void *) ((char *) al->ptr + al->offset)), sizeof(shstr *) * attr_list_len(al));
                *(sint64 **) ((void *) ((char *) al->ptr + offsetof(player, faction_reputation))) = realloc(*(sint64 **) ((void *) ((char *) al->ptr + offsetof(player, faction_reputation))), sizeof(sint64) * attr_list_len(al));
            }
        }
    }

    return ret;
}

/**
 * Implements value checking for both getter and setter methods.
 * @param al The AttrList object.
 * @param key Index to try and find.
 * @return 'key' if the value is valid, NULL otherwise. */
static PyObject *__getsetitem__(Atrinik_AttrList *al, PyObject *key)
{
    if (al->field == FIELDTYPE_FACTIONS) {
        if (!PyString_Check(key)) {
            PyErr_SetString(PyExc_ValueError, "__getitem__() failed; key must be a string.");
            return NULL;
        }
    }
    else {
        unsigned PY_LONG_LONG i, len;

        /* The key must be an integer. */
        if (!PyInt_Check(key)) {
            PyErr_SetString(PyExc_ValueError, "__getitem__() failed; key must be an integer.");
            return NULL;
        }

        i = PyLong_AsUnsignedLongLong(key);

        if (PyErr_Occurred()) {
            PyErr_SetString(PyExc_OverflowError, "__getitem__() failed; key's integer value is too large.");
            return NULL;
        }

        len = attr_list_len(al);

        if (i > len) {
            PyErr_Format(PyExc_ValueError, "__getitem__() failed; requested index (%"FMT64U ") too big (len: %"FMT64U ").", (uint64) i, (uint64) len);
            return NULL;
        }
    }

    return key;
}

/**
 * Implements the __getitem__() method.
 * @param al The AttrList object.
 * @param key Index to try and find.
 * @return The object from the AttrList at the specified index, NULL on
 * failure. */
static PyObject *__getitem__(Atrinik_AttrList *al, PyObject *key)
{
    key = __getsetitem__(al, key);

    if (key == NULL) {
        return NULL;
    }

    return attr_list_get(al, key, 0, NULL);
}

/**
 * Implements the append() method; this is equivalent to the following:
 * @code
 *    attr_list[len(attr_list)] = xyz
 * @endcode
 * @param al The AttrList object.
 * @param value Value to append.
 * @return None. */
static PyObject *append(Atrinik_AttrList *al, PyObject *value)
{
    unsigned PY_LONG_LONG i;

    if (al->field == FIELDTYPE_FACTIONS) {
        PyErr_SetString(PyExc_NotImplementedError, "This attribute list does not implement append method.");
        return NULL;
    }

    i = attr_list_len(al);
    attr_list_set(al, NULL, i, value);

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 * Implements the remove() method.
 * @param al The AttrList object.
 * @param value Value to remove.
 * @return None. */
static PyObject *attr_list_remove(Atrinik_AttrList *al, PyObject *value)
{
    unsigned PY_LONG_LONG i, len;
    PyObject *check;

    if (al->field == FIELDTYPE_FACTIONS) {
        PyErr_SetString(PyExc_NotImplementedError, "This attribute list does not implement remove method.");
        return NULL;
    }

    len = attr_list_len(al);

    for (i = 0; i < len; i++) {
        /* attr_list_get() creates a new reference, so make sure to decrease
         * it later. */
        check = attr_list_get(al, NULL, i, NULL);

        /* Compare the two objects... */
        if (PyObject_RichCompareBool(check, value, Py_EQ) == 1) {
            Py_DECREF(check);
            Py_INCREF(Py_None);
            attr_list_set(al, NULL, i, Py_None);

            Py_INCREF(Py_None);
            return Py_None;
        }

        Py_DECREF(check);
    }

    PyErr_SetString(PyExc_ValueError, "Value is not in attribute list.");
    return NULL;
}

/**
 * Implements the clear() method.
 * @param al The AttrList object.
 * @return None. */
static PyObject *attr_list_clear(Atrinik_AttrList *al)
{
    if (al->field == FIELDTYPE_CMD_PERMISSIONS || al->field == FIELDTYPE_REGION_MAPS) {
        if (*(char ***) ((void *) ((char *) al->ptr + al->offset)) != NULL) {
            free(*(char ***) ((void *) ((char *) al->ptr + al->offset)));
            *(char ***) ((void *) ((char *) al->ptr + al->offset)) = NULL;
            (*(int *) attr_list_len_ptr(al)) = 0;

            ((socket_struct *) (&(*(socket_struct **) ((void *) ((char *) al->ptr + offsetof(player, socket))))))->ext_title_flag = 1;
        }
    }
    else {
        PyErr_SetString(PyExc_NotImplementedError, "This attribute list does not implement clear method.");
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

/** Available Python methods for the AtrinikPlayer type. */
static PyMethodDef methods[] =
{
    {"__getitem__", (PyCFunction) __getitem__, METH_O | METH_COEXIST, 0},
    {"append", (PyCFunction) append, METH_O, 0},
    {"remove", (PyCFunction) attr_list_remove, METH_O, 0},
    {"clear", (PyCFunction) attr_list_clear, METH_NOARGS, 0},
    {NULL, NULL, 0, 0}
};

static int InternalCompare(Atrinik_AttrList *left, Atrinik_AttrList *right)
{
    return (left->field < right->field ? -1 : (left->field == right->field ? 0 : 1));
}

static PyObject *RichCompare(Atrinik_AttrList *left, Atrinik_AttrList *right, int op)
{
    if (!left || !right || !PyObject_TypeCheck((PyObject *) left, &Atrinik_AttrListType) || !PyObject_TypeCheck((PyObject *) right, &Atrinik_AttrListType)) {
        Py_INCREF(Py_NotImplemented);
        return Py_NotImplemented;
    }

    return generic_rich_compare(op, InternalCompare(left, right));
}

/**
 * Start iterating.
 * @param seq What to iterate through.
 * @return New iteration object. */
static PyObject *iter(PyObject *seq)
{
    Atrinik_AttrList *al, *orig_al = (Atrinik_AttrList *) seq;

    al = PyObject_NEW(Atrinik_AttrList, &Atrinik_AttrListType);
    al->iter = 0;
    al->ptr = orig_al->ptr;
    al->offset = orig_al->offset;
    al->field = orig_al->field;

    return (PyObject *) al;
}

/**
 * Keep iterating over an iteration object.
 * @param al The iteration object.
 * @return Next object from attribute list, NULL if the end was reached. */
static PyObject *iternext(Atrinik_AttrList *al)
{
    /* Possible to continue iteration? */
    if (al->iter < attr_list_len(al)) {
        al->iter++;

        if (al->field == FIELDTYPE_FACTIONS) {
            return attr_list_get(al, NULL, 0, *(char **) (&(*(shstr ***) (void *) ((char *) al->ptr + al->offset))[al->iter - 1]));
        }

        return attr_list_get(al, NULL, al->iter - 1, NULL);
    }

    /* Stop iteration. */
    return NULL;
}

/**
 * Wrapper for attr_list_len(), using Py_ssize_t return value.
 *
 * Used by the len() method.
 * @param al AttrList pointer len() is being called on.
 * @return Length of the attribute list. */
static Py_ssize_t __len__(Atrinik_AttrList *al)
{
    return attr_list_len(al);
}

/**
 * Wrapper for attr_list_set(), used by the __setitem__() method.
 * @param al AttrList pointer __setitem__() is being called on.
 * @return Return value of attr_list_set(). */
static int __setitem__(Atrinik_AttrList *al, PyObject *key, PyObject *value)
{
    key = __getsetitem__(al, key);

    if (key == NULL) {
        return -1;
    }

    return attr_list_set(al, key, 0, value);
}

/**
 * Wrapper for attr_list_contains(), used by the __contains__() method.
 * @param al AttrList pointer __contains__() is being called on.
 * @return Return value of attr_list_contains(). */
static int __contains__(Atrinik_AttrList *al, PyObject *value)
{
    return attr_list_contains(al, value);
}

/** Common sequence methods. */
static PySequenceMethods SequenceMethods =
{
    (lenfunc) __len__,
    NULL, NULL, NULL, NULL, NULL, NULL,
    (objobjproc) __contains__,
    NULL, NULL
};

/**
 * Defines what to map some common methods (len(), __setitem__() and
 * __getitem__()) to. */
static PyMappingMethods MappingMethods =
{
    (lenfunc) __len__,
    (binaryfunc) __getitem__,
    (objobjargproc) __setitem__,
};

/** AttrListType definition. */
PyTypeObject Atrinik_AttrListType =
{
#ifdef IS_PY3K
    PyVarObject_HEAD_INIT(NULL, 0)
#else
    PyObject_HEAD_INIT(NULL)
    0,
#endif
    "Atrinik.AttrList",
    sizeof(Atrinik_AttrList),
    0,
    NULL,
    NULL, NULL, NULL,
#ifdef IS_PY3K
    NULL,
#else
    (cmpfunc) InternalCompare,
#endif
    NULL,
    0,
    &SequenceMethods,
    &MappingMethods,
    0, 0,
    NULL,
    0, 0, 0,
    Py_TPFLAGS_DEFAULT,
    "Atrinik attr lists",
    NULL, NULL,
    (richcmpfunc) RichCompare,
    0,
    (getiterfunc) iter,
    (iternextfunc) iternext,
    methods,
    0,
    NULL,
    0, 0, 0, 0, 0, 0, 0,
    NULL,
    0, 0, 0, 0, 0, 0, 0, 0
#ifndef IS_PY_LEGACY
    , 0
#endif
};

/**
 * Initializes the AttrList module.
 * @param module The main Atrinik module.
 * @return 1 on success, 0 on failure. */
int Atrinik_AttrList_init(PyObject *module)
{
    Atrinik_AttrListType.tp_new = PyType_GenericNew;

    if (PyType_Ready(&Atrinik_AttrListType) < 0) {
        return 0;
    }

    Py_INCREF(&Atrinik_AttrListType);
    PyModule_AddObject(module, "AtrrList", (PyObject *) &Atrinik_AttrListType);

    return 1;
}

/**
 * Creates a new AttrList wrapping an array.
 * @param ptr Pointer to the structure the array is in.
 * @param offset Where the array is in the structure.
 * @param field Type of the array being handled; for example,
 * @ref FIELDTYPE_REGION_MAPS.
 * @return The new wrapper object. */
PyObject *wrap_attr_list(void *ptr, size_t offset, field_type field)
{
    Atrinik_AttrList *wrapper;

    wrapper = PyObject_NEW(Atrinik_AttrList, &Atrinik_AttrListType);

    if (wrapper) {
        wrapper->ptr = ptr;
        wrapper->offset = offset;
        wrapper->field = field;
    }

    return (PyObject *) wrapper;
}
