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
 * Atrinik Python plugin party related code. */

#include <plugin_python.h>

/**
 * Party fields. */
/* @cparser
 * @page plugin_python_party_fields Python party fields
 * <h2>Python party fields</h2>
 * List of the party fields and their meaning. */
static fields_struct fields[] =
{
    {"name", FIELDTYPE_SHSTR, offsetof(party_struct, name), FIELDFLAG_READONLY, 0},
    {"leader", FIELDTYPE_SHSTR, offsetof(party_struct, leader), 0, 0},
    {"password", FIELDTYPE_CARY, offsetof(party_struct, passwd), FIELDFLAG_READONLY, 0}
};
/* @endcparser */

/**
 * @defgroup plugin_python_party_functions Python party functions
 * Party related functions used in Atrinik Python plugin.
 *@{*/

/**
 * <h1>party.AddMember(object player)</h1>
 * Add a player to the specified party.
 * @param player Player object to add to the party.
 * @throws ValueError if 'player' is not a player object.
 * @throws AtrinikError if the player is already in the same or another party.
 * */
static PyObject *Atrinik_Party_AddMember(Atrinik_Party *party, PyObject *args)
{
    Atrinik_Object *ob;

    if (!PyArg_ParseTuple(args, "O!", &Atrinik_ObjectType, &ob)) {
        return NULL;
    }

    OBJEXISTCHECK(ob);

    if (ob->obj->type != PLAYER || !CONTR(ob->obj)) {
        PyErr_SetString(PyExc_ValueError, "party.AddMember(): 'player' must be a player object.");
        return NULL;
    }
    else if (CONTR(ob->obj)->party) {
        if (CONTR(ob->obj)->party == party->party) {
            RAISE("party.AddMember(): The specified player object is already in the specified party.");
        }
        else {
            RAISE("party.AddMember(): The specified player object is already in another party.");
        }
    }

    hooks->add_party_member(party->party, ob->obj);

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 * <h1>party.RemoveMember(object player)</h1>
 * Remove a player from the specified party.
 * @param player Player object to remove from the party.
 * @throws ValueError if 'player' is not a player object.
 * @throws AtrinikError if the player is not in a party. */
static PyObject *Atrinik_Party_RemoveMember(Atrinik_Party *party, PyObject *args)
{
    Atrinik_Object *ob;

    if (!PyArg_ParseTuple(args, "O!", &Atrinik_ObjectType, &ob)) {
        return NULL;
    }

    OBJEXISTCHECK(ob);

    if (ob->obj->type != PLAYER || !CONTR(ob->obj)) {
        PyErr_SetString(PyExc_ValueError, "party.RemoveMember(): 'player' must be a player object.");
        return NULL;
    }
    else if (!CONTR(ob->obj)->party) {
        RAISE("party.RemoveMember(): The specified player is not in a party.");
    }

    hooks->remove_party_member(party->party, ob->obj);

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 * <h1>party.GetMembers()</h1>
 * Get members of a specified party.
 * @return List containing player objects of the party members. */
static PyObject *Atrinik_Party_GetMembers(Atrinik_Party *party, PyObject *args)
{
    PyObject *list = PyList_New(0);
    objectlink *ol;

    (void) args;

    for (ol = party->party->members; ol; ol = ol->next) {
        PyList_Append(list, wrap_object(ol->objlink.ob));
    }

    return list;
}

/**
 * <h1>party.SendMessage(string message, int flags, object [player = None])</h1>
 * Send a message to members of a party.
 * @param message Message to send.
 * @param flags Flags. See @ref PARTY_MESSAGE_xxx.
 * @param player Player object to exclude from sending the message. */
static PyObject *Atrinik_Party_SendMessage(Atrinik_Party *party, PyObject *args)
{
    Atrinik_Object *ob = NULL;
    int flags;
    const char *msg;

    if (!PyArg_ParseTuple(args, "si|O!", &msg, &flags, &Atrinik_ObjectType, &ob)) {
        return NULL;
    }

    if (ob) {
        OBJEXISTCHECK(ob);
    }

    hooks->send_party_message(party->party, msg, flags, ob ? ob->obj : NULL, NULL);

    Py_INCREF(Py_None);
    return Py_None;
}

/*@}*/

/** Available Python methods for the AtrinikParty object */
static PyMethodDef PartyMethods[] =
{
    {"AddMember", (PyCFunction) Atrinik_Party_AddMember, METH_VARARGS, 0},
    {"RemoveMember", (PyCFunction) Atrinik_Party_RemoveMember, METH_VARARGS, 0},
    {"GetMembers", (PyCFunction) Atrinik_Party_GetMembers, METH_NOARGS, 0},
    {"SendMessage", (PyCFunction) Atrinik_Party_SendMessage, METH_VARARGS, 0},
    {NULL, NULL, 0, 0}
};

/**
 * Get party's attribute.
 * @param party Python party wrapper.
 * @param context Void pointer to the field.
 * @return Python object with the attribute value, NULL on failure. */
static PyObject *Party_GetAttribute(Atrinik_Party *party, void *context)
{
    return generic_field_getter((fields_struct *) context, party->party);
}

/**
 * Set attribute of a party.
 * @param party Python party wrapper.
 * @param value Value to set.
 * @param context Void pointer to the field.
 * @return 0 on success, -1 on failure. */
static int Party_SetAttribute(Atrinik_Party *party, PyObject *value, void *context)
{
    if (generic_field_setter((fields_struct *) context, party->party, value) == -1) {
        return -1;
    }

    return 0;
}

/**
 * Create a new party wrapper.
 * @param type Type object.
 * @param args Unused.
 * @param kwds Unused.
 * @return The new wrapper. */
static PyObject *Atrinik_Party_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    Atrinik_Party *self;

    (void) args;
    (void) kwds;

    self = (Atrinik_Party *) type->tp_alloc(type, 0);

    if (self) {
        self->party = NULL;
    }

    return (PyObject *) self;
}

/**
 * Free a party wrapper.
 * @param self The wrapper to free. */
static void Atrinik_Party_dealloc(Atrinik_Party *self)
{
    self->party = NULL;
#ifndef IS_PY_LEGACY
    Py_TYPE(self)->tp_free((PyObject *) self);
#else
    self->ob_type->tp_free((PyObject *) self);
#endif
}

/**
 * Return a string representation of a party.
 * @param self The party type.
 * @return Python object containing the name of the party. */
static PyObject *Atrinik_Party_str(Atrinik_Party *self)
{
    return Py_BuildValue("s", self->party->name);
}

static int Atrinik_Party_InternalCompare(Atrinik_Party *left, Atrinik_Party *right)
{
    return (left->party < right->party ? -1 : (left->party == right->party ? 0 : 1));
}

static PyObject *Atrinik_Party_RichCompare(Atrinik_Party *left, Atrinik_Party *right, int op)
{
    if (!left || !right || !PyObject_TypeCheck((PyObject *) left, &Atrinik_PartyType) || !PyObject_TypeCheck((PyObject *) right, &Atrinik_PartyType)) {
        Py_INCREF(Py_NotImplemented);
        return Py_NotImplemented;
    }

    return generic_rich_compare(op, Atrinik_Party_InternalCompare(left, right));
}

/** This is filled in when we initialize our party type. */
static PyGetSetDef getseters[NUM_FIELDS + 1];

/** Our actual Python PartyType. */
PyTypeObject Atrinik_PartyType =
{
#ifdef IS_PY3K
    PyVarObject_HEAD_INIT(NULL, 0)
#else
    PyObject_HEAD_INIT(NULL)
    0,
#endif
    "Atrinik.Party",
    sizeof(Atrinik_Party),
    0,
    (destructor) Atrinik_Party_dealloc,
    NULL, NULL, NULL,
#ifdef IS_PY3K
    NULL,
#else
    (cmpfunc) Atrinik_Party_InternalCompare,
#endif
    0, 0, 0, 0, 0, 0,
    (reprfunc) Atrinik_Party_str,
    0, 0, 0,
    Py_TPFLAGS_DEFAULT,
    "Atrinik parties",
    NULL, NULL,
    (richcmpfunc) Atrinik_Party_RichCompare,
    0, 0, 0,
    PartyMethods,
    0,
    getseters,
    0, 0, 0, 0, 0, 0, 0,
    Atrinik_Party_new,
    0, 0, 0, 0, 0, 0, 0, 0
#ifndef IS_PY_LEGACY
    , 0
#endif
};

/**
 * Initialize the party wrapper.
 * @param module The Atrinik Python module.
 * @return 1 on success, 0 on failure. */
int Atrinik_Party_init(PyObject *module)
{
    size_t i;

    /* Field getters */
    for (i = 0; i < NUM_FIELDS; i++) {
        PyGetSetDef *def = &getseters[i];

        def->name = fields[i].name;
        def->get = (getter) Party_GetAttribute;
        def->set = (setter) Party_SetAttribute;
        def->doc = NULL;
        def->closure = (void *) &fields[i];
    }

    getseters[i].name = NULL;

    Atrinik_PartyType.tp_new = PyType_GenericNew;

    if (PyType_Ready(&Atrinik_PartyType) < 0) {
        return 0;
    }

    Py_INCREF(&Atrinik_PartyType);
    PyModule_AddObject(module, "Party", (PyObject *) &Atrinik_PartyType);

    return 1;
}

/**
 * Utility method to wrap a party.
 * @param what Party to wrap.
 * @return Python object wrapping the real party. */
PyObject *wrap_party(party_struct *what)
{
    Atrinik_Party *wrapper;

    /* Return None if no party was to be wrapped. */
    if (!what) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    wrapper = PyObject_NEW(Atrinik_Party, &Atrinik_PartyType);

    if (wrapper) {
        wrapper->party = what;
    }

    return (PyObject *) wrapper;
}
