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
 * Implements AttrList Python object; this is a class similar to Python
 * lists/dictionaries, but with more limited functionality (it does not
 * implement many methods normal lists do; however, the AttrList object can
 * be converted to a list).
 *
 * These objects are used to wrap an array of data, for example,
 * player::cmd_permissions array, which holds the command permissions the player
 * has access to.
 * When Python requests to get player.cmd_permissions, the code generates an
 * AttrList object, which "wraps" the actual array of command permissions - it
 * holds a pointer to the structure, the offset in the structure, etc.
 * Thus, it is possible for Python to use __setitem__() and __getitem__()
 * methods on this AttrList object to change/extract the array data in the
 * most efficient way possible; all of the data of the array is never held
 * in memory (unless you implicitly convert it to a list, of course).
 *
 * The code uses many castings and performs exhaustive error checking
 * of passed arguments, as the smallest imperfection could cause a memory
 * corruption, most likely resulting in a crash.
 *
 * @author Alex Tokar
 */

#include <plugin_python.h>
#include <plugin.h>
#include <toolkit/packet.h>
#include <player.h>

/**
 * Operations supported by attr_list_oper().
 */
typedef enum attr_list_oper {
    AL_OPER_SET, ///< __setitem__()
    AL_OPER_GET, ///< __getitem__()
    AL_OPER_REMOVE, ///< remove()
    AL_OPER_APPEND, //< append()
    AL_OPER_CONTAINS, ///< __contains__()
    AL_OPER_CLEAR, ///< clear()
    AL_OPER_ITEMS, ///< items()
    AL_OPER_ITER, ///< __iter__()
    AL_OPER_DELETE, ///< __del__()
} attr_list_oper_t;

/**
 * Calculate length of the specified AttrList instance.
 * @param al
 * AttrList being used.
 * @return
 * The length of the provided AttrList.
 */
static Py_ssize_t attr_list_len(Atrinik_AttrList *al)
{
    if (al->field == FIELDTYPE_CMD_PERMISSIONS) {
        return *(int *) ((char *) al->ptr +
                offsetof(player, num_cmd_permissions));
    } else if (al->field == FIELDTYPE_FACTIONS) {
        player_faction_t *factions = *(player_faction_t **) ((char *) al->ptr +
                al->offset);
        return HASH_CNT(hh, factions);
    } else if (al->field == FIELDTYPE_PACKETS) {
        packet_struct *head = *(packet_struct **) ((char *) al->ptr +
                al->offset), *packet;
        Py_ssize_t num = 0;
        DL_FOREACH(head, packet) {
            num++;
        }

        return num;
    }

    return 0;
}

/**
 * Perform specified operation on a command permissions attribute list.
 * @param al
 * The attribute list.
 * @param oper
 * Operation to perform.
 * @param key
 * Key.
 * @param value
 * Value.
 * @return
 * True on success, false on failure.
 */
static bool attr_list_oper_cmd_permissions(Atrinik_AttrList *al,
        attr_list_oper_t oper, PyObject *key, PyObject **value)
{
    char ***perms = ((char ***) ((char *) al->ptr + al->offset));
    int *len = (int *) ((char *) al->ptr +
            offsetof(player, num_cmd_permissions));
    socket_struct *ns = (socket_struct *) ((char *) al->ptr +
            offsetof(player, cs));

    if (oper == AL_OPER_CLEAR) {
        if (*perms == NULL) {
            return true;
        }

        for (int i = 0; i < *len; i++) {
            if ((*perms)[i] != NULL) {
                efree((*perms)[i]);
            }
        }

        efree(*perms);
        *perms = NULL;
        *len = 0;

        ns->ext_title_flag = true;
        return true;
    } else if (oper == AL_OPER_ITEMS) {
        HARD_ASSERT(value != NULL);
        *value = PyList_New(*len);
        for (int i = 0; i < *len; i++) {
            PyList_SetItem(*value, i, Py_BuildValue("s", (*perms)[i]));
        }

        return true;
    } else if (oper == AL_OPER_CONTAINS) {
        char *str = PyString_AsString(*value);
        for (int i = 0; i < *len; i++) {
            if ((*perms)[i] != NULL && strcmp((*perms)[i], str) == 0) {
                return true;
            }
        }

        return false;
    } else if (oper != AL_OPER_APPEND && oper != AL_OPER_GET &&
            oper != AL_OPER_SET && oper != AL_OPER_ITER &&
            oper != AL_OPER_REMOVE) {
        PyErr_SetString(PyExc_NotImplementedError, "operation not implemented");
        return false;
    }

    HARD_ASSERT(value != NULL);

    PY_LONG_LONG idx;
    if (oper == AL_OPER_APPEND) {
        *perms = erealloc(*perms, sizeof(**perms) * (*len + 1));
        idx = *len;
        (*perms)[idx] = NULL;
        (*len)++;
    } else if (oper == AL_OPER_ITER) {
        idx = al->iter.idx;
        if (idx >= *len) {
            return false;
        }

        al->iter.idx++;
    } else if (oper == AL_OPER_REMOVE) {
        char *str = PyString_AsString(*value);
        for (idx = 0; idx < *len; idx++) {
            if ((*perms)[idx] != NULL && strcmp((*perms)[idx], str) == 0) {
                break;
            }
        }

        if (idx >= *len) {
            PyErr_SetString(PyExc_ValueError, "value not found");
            return false;
        }
    } else {
        HARD_ASSERT(key != NULL);
        if (!PyLong_Check(key)) {
            PyErr_SetString(PyExc_TypeError, "index must be an integer");
            return false;
        }

        idx = PyLong_AsLongLong(key);
        if (idx < 0) {
            idx += *len;
        }

        if (idx >= *len) {
            PyErr_SetString(PyExc_IndexError, "index out of range");
            return false;
        }
    }

    void *ptr = &(*perms)[idx];
    fields_struct field = {"xxx", FIELDTYPE_CSTR, 0, 0, 0, NULL};

    if (oper == AL_OPER_GET || oper == AL_OPER_ITER) {
        *value = generic_field_getter(&field, ptr);
        if (*value == NULL) {
            return false;
        }
    } else {
        PyObject *what = oper == AL_OPER_REMOVE ? Py_None : *value;
        int ret = generic_field_setter(&field, ptr, what);
        if (ret == -1) {
            if (oper == AL_OPER_APPEND) {
                (*len)--;
                *perms = erealloc(*perms, sizeof(**perms) * (*len));
            }

            return false;
        }

        ns->ext_title_flag = true;
    }

    return true;
}

/**
 * Perform specified operation on a factions attribute list.
 * @param al
 * The attribute list.
 * @param oper
 * Operation to perform.
 * @param key
 * Key.
 * @param value
 * Value.
 * @return
 * True on success, false on failure.
 */
static bool attr_list_oper_factions(Atrinik_AttrList *al,
        attr_list_oper_t oper, PyObject *key, PyObject **value)
{
    HARD_ASSERT(al != NULL);

    if (oper == AL_OPER_CLEAR) {
        player_faction_t *factions = *(player_faction_t **) (
                (char *) al->ptr + al->offset), *faction, *tmp;
        HASH_ITER(hh, factions, faction, tmp) {
            hooks->player_faction_free(al->ptr, faction);
        }

        return true;
    } else if (oper == AL_OPER_ITEMS) {
        *value = PyList_New(0);
        player_faction_t *factions = *(player_faction_t **) (
                (char *) al->ptr + al->offset), *faction, *tmp;
        HASH_ITER(hh, factions, faction, tmp) {
            PyObject *tuple = PyTuple_New(2);
            PyTuple_SetItem(tuple, 0, Py_BuildValue("s", faction->name));
            PyTuple_SetItem(tuple, 1, Py_BuildValue("f", faction->reputation));
            PyList_Append(*value, tuple);
        }

        return true;
    } else if (oper != AL_OPER_CONTAINS && oper != AL_OPER_GET &&
            oper != AL_OPER_SET && oper != AL_OPER_DELETE &&
            oper != AL_OPER_ITER) {
        PyErr_SetString(PyExc_NotImplementedError, "operation not implemented");
        return false;
    }

    if (oper == AL_OPER_CONTAINS) {
        HARD_ASSERT(value != NULL);
        key = *value;
    }

    char *str = NULL;
    if (oper != AL_OPER_ITER) {
        HARD_ASSERT(key != NULL);
        if (!PyString_Check(key)) {
            PyErr_SetString(PyExc_TypeError, "key must be a string");
            return false;
        }

        str = PyString_AsString(key);
    }

    if (oper == AL_OPER_CONTAINS) {
        shstr *shared_str = hooks->find_string(str);
        if (shared_str == NULL) {
            return false;
        }

        player_faction_t *factions = *(player_faction_t **) (
                (char *) al->ptr + al->offset), *faction, *tmp;
        HASH_ITER(hh, factions, faction, tmp) {
            if (faction->name == shared_str) {
                return true;
            }
        }

        return false;
    }

    HARD_ASSERT(value != NULL);

    player_faction_t *faction;
    bool added_new = false;
    if (oper == AL_OPER_ITER) {
        player_faction_t *factions = *(player_faction_t **) ((char *) al->ptr +
                al->offset), *tmp;
        PY_LONG_LONG i = 0;
        HASH_ITER(hh, factions, faction, tmp) {
            if (i++ == al->iter.idx) {
                break;
            }
        }

        if (faction == NULL) {
            return false;
        }

        al->iter.idx++;
    } else {
        shstr *shared_str;
        if (oper == AL_OPER_SET) {
            shared_str = hooks->add_string(str);
        } else {
            shared_str = hooks->find_string(str);
            if (shared_str == NULL) {
                PyErr_SetString(PyExc_KeyError, "invalid key");
                return false;
            }
        }

        faction = hooks->player_faction_find(al->ptr, shared_str);
        if (faction == NULL) {
            if (oper != AL_OPER_SET) {
                PyErr_SetString(PyExc_KeyError, "invalid key");
                return false;
            }

            faction = hooks->player_faction_create(al->ptr, shared_str);
            added_new = true;
        }

        if (oper == AL_OPER_SET) {
            hooks->free_string_shared(shared_str);
        } else if (oper == AL_OPER_DELETE) {
            hooks->player_faction_free(al->ptr, faction);
            return true;
        }
    }

    void *ptr = &(*(double **) ((char *) (void *) faction +
            offsetof(player_faction_t, reputation)));
    fields_struct field = {"xxx", FIELDTYPE_DOUBLE, 0, 0, 0, NULL};

    if (oper == AL_OPER_GET || oper == AL_OPER_ITER) {
        *value = generic_field_getter(&field, ptr);
        if (*value == NULL) {
            return false;
        }
    } else {
        int ret = generic_field_setter(&field, ptr, *value);
        if (ret == -1) {
            if (added_new) {
                hooks->player_faction_free(al->ptr, faction);
            }

            return false;
        }
    }

    return true;
}

/**
 * Perform specified operation on a packets attribute list.
 * @param al
 * The attribute list.
 * @param oper
 * Operation to perform.
 * @param key
 * Key.
 * @param value
 * Value.
 * @return
 * True on success, false on failure.
 */
static bool attr_list_oper_packets(Atrinik_AttrList *al,
        attr_list_oper_t oper, PyObject *key, PyObject **value)
{
    HARD_ASSERT(al != NULL);

    if (oper == AL_OPER_SET || oper == AL_OPER_CONTAINS ||
            oper == AL_OPER_APPEND) {
        HARD_ASSERT(value != NULL);
        if (!PyBytes_Check(*value)) {
            PyErr_SetString(PyExc_TypeError, "value must be a bytes object");
            return false;
        }
    }

    packet_struct **head = (packet_struct **) ((char *) al->ptr + al->offset);
    if (oper == AL_OPER_CLEAR) {
        packet_struct *packet, *tmp;
        DL_FOREACH_SAFE(*head, packet, tmp) {
            hooks->packet_free(packet);
        }

        *head = NULL;
        return true;
    } else if (oper == AL_OPER_CONTAINS) {
        char *str = PyBytes_AsString(*value);
        size_t len = PyBytes_Size(*value);
        packet_struct *packet;
        DL_FOREACH(*head, packet) {
            if (packet->len == len && memcmp(packet->data, str, len) == 0) {
                return true;
            }
        }

        return false;
    } else if (oper != AL_OPER_GET && oper != AL_OPER_SET &&
            oper != AL_OPER_APPEND && oper != AL_OPER_ITER) {
        PyErr_SetString(PyExc_NotImplementedError, "operation not implemented");
        return false;
    }

    packet_struct *elem = NULL;
    if (oper == AL_OPER_ITER) {
        elem = al->iter.ptr;
        if (elem == NULL) {
            elem = *head;
        } else {
            elem = elem->next;
        }

        if (elem == NULL) {
            return false;
        }

        al->iter.ptr = elem;
    } else if (oper != AL_OPER_APPEND) {
        if (key == NULL) {
            PyErr_SetString(PyExc_NotImplementedError,
                    "operation not implemented");
            return false;
        }

        if (!PyLong_Check(key)) {
            PyErr_SetString(PyExc_TypeError, "index must be an integer");
            return false;
        }

        PY_LONG_LONG idx = PyLong_AsLongLong(key), i = 0;
        if (idx < 0) {
            DL_FOREACH_REVERSE(*head, elem) {
                if (--i == idx) {
                    break;
                }
            }
        } else {
            DL_FOREACH(*head, elem) {
                if (i++ == idx) {
                    break;
                }
            }
        }
    }

    if (oper != AL_OPER_APPEND && elem == NULL) {
        PyErr_SetString(PyExc_IndexError, "index out of range");
        return false;
    }

    if (oper == AL_OPER_GET || oper == AL_OPER_ITER) {
        *value = PyBytes_FromStringAndSize((const char *) elem->data,
                elem->len);
        return true;
    }

    packet_struct *packet = hooks->packet_new(0, PyBytes_Size(*value), 0);
    hooks->packet_append_data_len(packet, (uint8_t *) PyBytes_AsString(*value),
            PyBytes_Size(*value));

    if (oper == AL_OPER_SET) {
        DL_PREPEND_ELEM(*head, elem, packet);
        DL_DELETE(*head, elem);
        hooks->packet_free(elem);
    } else {
        DL_APPEND(*head, packet);
    }

    return true;
}

/**
 * Perform specified operation on the attribute list.
 * @param al
 * The attribute list.
 * @param oper
 * Operation to perform.
 * @param key
 * Key.
 * @param value
 * Value.
 * @return
 * True on success, false on failure.
 */
static bool attr_list_oper(Atrinik_AttrList *al, attr_list_oper_t oper,
        PyObject *key, PyObject **value)
{
    switch (al->field) {
    case FIELDTYPE_CMD_PERMISSIONS:
        return attr_list_oper_cmd_permissions(al, oper, key, value);

    case FIELDTYPE_FACTIONS:
        return attr_list_oper_factions(al, oper, key, value);

    case FIELDTYPE_PACKETS:
        return attr_list_oper_packets(al, oper, key, value);

    default:
        LOG(ERROR, "Unhandled field: %d", al->field);
        return false;
    }
}

/**
 * Implements the append() method.
 * @param al
 * The AttrList object.
 * @param value
 * Value to append.
 * @return
 * None.
 */
static PyObject *append(Atrinik_AttrList *al, PyObject *value)
{
    if (!attr_list_oper(al, AL_OPER_APPEND, NULL, &value)) {
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 * Implements the remove() method.
 * @param al
 * The AttrList object.
 * @param value
 * Value to remove.
 * @return
 * None.
 */
static PyObject *attr_list_remove(Atrinik_AttrList *al, PyObject *value)
{
    if (!attr_list_oper(al, AL_OPER_REMOVE, NULL, &value)) {
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 * Implements the clear() method.
 * @param al
 * The AttrList object.
 * @return
 * None.
 */
static PyObject *attr_list_clear(Atrinik_AttrList *al)
{
    if (!attr_list_oper(al, AL_OPER_CLEAR, NULL, NULL)) {
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 * Implements the items() method.
 * @param al
 * The AttrList object.
 * @return
 * List of the items in the AttrList.
 */
static PyObject *attr_list_items(Atrinik_AttrList *al)
{
    PyObject *value;
    if (!attr_list_oper(al, AL_OPER_ITEMS, NULL, &value)) {
        return NULL;
    }

    return value;
}

/** Available Python methods for the AtrinikAttrList type. */
static PyMethodDef methods[] = {
    {"append", (PyCFunction) append, METH_O, 0},
    {"remove", (PyCFunction) attr_list_remove, METH_O, 0},
    {"clear", (PyCFunction) attr_list_clear, METH_NOARGS, 0},
    {"items", (PyCFunction) attr_list_items, METH_NOARGS, 0},
    {NULL, NULL, 0, 0}
};

/**
 * Start iterating.
 * @param seq
 * What to iterate through.
 * @return
 * New iteration object.
 */
static PyObject *iter(PyObject *seq)
{
    Atrinik_AttrList *al, *orig_al = (Atrinik_AttrList *) seq;

    al = PyObject_NEW(Atrinik_AttrList, &Atrinik_AttrListType);
    al->iter.ptr = NULL;
    al->iter.idx = 0;
    al->ptr = orig_al->ptr;
    al->offset = orig_al->offset;
    al->field = orig_al->field;

    return (PyObject *) al;
}

/**
 * Keep iterating over an iteration object.
 * @param al
 * The iteration object.
 * @return
 * Next object from attribute list, NULL if the end was reached.
 */
static PyObject *iternext(Atrinik_AttrList *al)
{
    PyObject *value = NULL;
    attr_list_oper(al, AL_OPER_ITER, NULL, &value);
    return value;
}

/**
 * Implements the __len__() method.
 * @param al
 * AttrList instance.
 * @return
 * Length of the attribute list.
 */
static Py_ssize_t __len__(Atrinik_AttrList *al)
{
    return attr_list_len(al);
}

/**
 * Implements the __getitem__() method.
 * @param al
 * AttrList instance.
 * @param key
 * Index to try and find.
 * @return
 * The object from the AttrList at the specified index, NULL on
 * failure.
 */
static PyObject *__getitem__(Atrinik_AttrList *al, PyObject *key)
{
    PyObject *value = NULL;
    attr_list_oper(al, AL_OPER_GET, key, &value);
    return value;
}

/**
 * Implements the __setitem__() method.
 * @param al
 * AttrList instance.
 * @param key
 * Key/index.
 * @param value
 * Value to set.
 * @return
 * 0 on success, -1 on failure.
 */
static int __setitem__(Atrinik_AttrList *al, PyObject *key, PyObject *value)
{
    attr_list_oper_t oper = value != NULL ? AL_OPER_SET : AL_OPER_DELETE;
    if (attr_list_oper(al, oper, key, &value)) {
        return 0;
    }

    return -1;
}

/**
 * Implements the __contains__() method for attribute list.
 * @param al
 * AttrList instance.
 * @param value
 * Value to find.
 * @return
 * 1 if the value is in the attribute list, 0 otherwise.
 */
static int __contains__(Atrinik_AttrList *al, PyObject *value)
{
    return attr_list_oper(al, AL_OPER_CONTAINS, NULL, &value);
}

/** Common sequence methods. */
static PySequenceMethods SequenceMethods = {
    (lenfunc) __len__,
    NULL, NULL, NULL, NULL, NULL, NULL,
    (objobjproc) __contains__,
    NULL, NULL
};

/**
 * Defines what to map some common methods (len(), __setitem__() and
 * __getitem__()) to.
 */
static PyMappingMethods MappingMethods = {
    (lenfunc) __len__,
    (binaryfunc) __getitem__,
    (objobjargproc) __setitem__,
};

/** AttrListType definition. */
PyTypeObject Atrinik_AttrListType = {
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
    NULL,
    NULL,
    0,
    &SequenceMethods,
    &MappingMethods,
    0, 0,
    NULL,
    0, 0, 0,
    Py_TPFLAGS_DEFAULT,
    "Atrinik attr lists",
    NULL, NULL, NULL,
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
#ifdef Py_TPFLAGS_HAVE_FINALIZE
    , NULL
#endif
};

/**
 * Initializes the AttrList module.
 * @param module
 * The main Atrinik module.
 * @return
 * 1 on success, 0 on failure.
 */
int Atrinik_AttrList_init(PyObject *module)
{
    Atrinik_AttrListType.tp_new = PyType_GenericNew;

    if (PyType_Ready(&Atrinik_AttrListType) < 0) {
        return 0;
    }

    Py_INCREF(&Atrinik_AttrListType);
    PyModule_AddObject(module, "AttrList", (PyObject *) &Atrinik_AttrListType);

    return 1;
}

/**
 * Creates a new AttrList wrapping an array.
 * @param ptr
 * Pointer to the structure the array is in.
 * @param offset
 * Where the array is in the structure.
 * @param field
 * Type of the array being handled; for example,
 * @ref FIELDTYPE_FACTIONS.
 * @return
 * The new wrapper object.
 */
PyObject *wrap_attr_list(void *ptr, size_t offset, field_type field)
{
    Atrinik_AttrList *wrapper = PyObject_NEW(Atrinik_AttrList,
            &Atrinik_AttrListType);
    if (wrapper != NULL) {
        wrapper->ptr = ptr;
        wrapper->offset = offset;
        wrapper->field = field;
    }

    return (PyObject *) wrapper;
}
