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
 * Atrinik Python plugin party related code.
 *
 * @author Alex Tokar
 */

#include <plugin_python.h>

/**
 * Party fields.
 */
static fields_struct fields[] = {
    {"name", FIELDTYPE_SHSTR, offsetof(party_struct, name), FIELDFLAG_READONLY,
            0, "Name of the party.; str (readonly)"},
    {"leader", FIELDTYPE_SHSTR, offsetof(party_struct, leader), 0, 0,
            "Name of the party's leader (a player name).; str"},
    {"password", FIELDTYPE_CARY, offsetof(party_struct, passwd),
            FIELDFLAG_READONLY, 0, "Password required to join the party.; str"}
};

/** Documentation for Atrinik_Party_AddMember(). */
static const char doc_Atrinik_Party_AddMember[] =
".. method:: AddMember(player).\n\n"
"Add a player to the party.\n\n"
":param player: Player object to add to the party.\n"
":type player: :class:`Atrinik.Object.Object`\n"
":raises ValueError: If *player* is not a player object.\n"
":raises Atrinik.AtrinikError: If the player is already in a party.";

/**
 * Implements Atrinik.Party.Party.AddMember() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Party_AddMember(Atrinik_Party *self, PyObject *args)
{
    Atrinik_Object *ob;

    if (!PyArg_ParseTuple(args, "O!", &Atrinik_ObjectType, &ob)) {
        return NULL;
    }

    OBJEXISTCHECK(ob);

    if (ob->obj->type != PLAYER || CONTR(ob->obj) == NULL) {
        PyErr_SetString(PyExc_ValueError, "'player' must be a player object.");
        return NULL;
    } else if (CONTR(ob->obj)->party != NULL) {
        if (CONTR(ob->obj)->party == self->party) {
            RAISE("The specified player object is already in the party.");
        } else {
            RAISE("The specified player object is already in another party.");
        }
    }

    hooks->add_party_member(self->party, ob->obj);

    Py_INCREF(Py_None);
    return Py_None;
}

/** Documentation for Atrinik_Party_RemoveMember(). */
static const char doc_Atrinik_Party_RemoveMember[] =
".. method:: RemoveMember(player).\n\n"
"Remove a player from the party.\n\n"
":param player: Player object to remove from the party.\n"
":type player: :class:`Atrinik.Object.Object`\n"
":raises ValueError: If *player* is not a player object.\n"
":raises Atrinik.AtrinikError: If the player is not in a party.";

/**
 * Implements Atrinik.Party.Party.RemoveMember() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Party_RemoveMember(Atrinik_Party *self, PyObject *args)
{
    Atrinik_Object *ob;

    if (!PyArg_ParseTuple(args, "O!", &Atrinik_ObjectType, &ob)) {
        return NULL;
    }

    OBJEXISTCHECK(ob);

    if (ob->obj->type != PLAYER || CONTR(ob->obj) == NULL) {
        PyErr_SetString(PyExc_ValueError, "'player' must be a player object.");
        return NULL;
    } else if (CONTR(ob->obj)->party == NULL) {
        RAISE("The specified player is not in a party.");
    }

    hooks->remove_party_member(self->party, ob->obj);

    Py_INCREF(Py_None);
    return Py_None;
}

/** Documentation for Atrinik_Party_GetMembers(). */
static const char doc_Atrinik_Party_GetMembers[] =
".. method:: GetMembers().\n\n"
"Get members of the party.\n\n"
":returns: List containing the party members as player objects.\n"
":rtype: list of :class:`Atrinik.Object.Object`";

/**
 * Implements Atrinik.Party.Party.GetMembers() Python method.
 * @copydoc PyMethod_NOARGS
 */
static PyObject *Atrinik_Party_GetMembers(Atrinik_Party *self)
{
    PyObject *list = PyList_New(0);
    for (objectlink *ol = self->party->members; ol != NULL; ol = ol->next) {
        PyList_Append(list, wrap_object(ol->objlink.ob));
    }

    return list;
}

/** Documentation for Atrinik_Party_SendMessage(). */
static const char doc_Atrinik_Party_SendMessage[] =
".. method:: SendMessage(message, flags, player=None).\n\n"
"Send a message to members of the party.\n\n"
":param message: Message to send.\n"
":type message: str\n"
":param flags: One of the PARTY_MESSAGE_xxx constants, eg, :attr:"
"`~Atrinik.PARTY_MESSAGE_STATUS`\n"
":type flags: int\n"
":param player: Player object to exclude from sending the message to.\n"
":type player: :class:`Atrinik.Object.Object`";

/**
 * Implements Atrinik.Party.Party.SendMessage() Python method.
 * @copydoc PyMethod_VARARGS
 */
static PyObject *Atrinik_Party_SendMessage(Atrinik_Party *self, PyObject *args)
{
    Atrinik_Object *ob = NULL;
    int flags;
    const char *msg;

    if (!PyArg_ParseTuple(args, "si|O!", &msg, &flags, &Atrinik_ObjectType,
            &ob)) {
        return NULL;
    }

    if (ob != NULL) {
        OBJEXISTCHECK(ob);
    }

    hooks->send_party_message(self->party, msg, flags,
            ob != NULL ? ob->obj : NULL,  NULL);

    Py_INCREF(Py_None);
    return Py_None;
}

/** Available Python methods for the AtrinikParty object */
static PyMethodDef PartyMethods[] = {
    {"AddMember", (PyCFunction) Atrinik_Party_AddMember, METH_VARARGS,
            doc_Atrinik_Party_AddMember},
    {"RemoveMember", (PyCFunction) Atrinik_Party_RemoveMember, METH_VARARGS,
            doc_Atrinik_Party_RemoveMember},
    {"GetMembers", (PyCFunction) Atrinik_Party_GetMembers, METH_NOARGS,
            doc_Atrinik_Party_GetMembers},
    {"SendMessage", (PyCFunction) Atrinik_Party_SendMessage, METH_VARARGS,
            doc_Atrinik_Party_SendMessage},
    {NULL, NULL, 0, 0}
};

/**
 * Get party's attribute.
 * @param party Python party wrapper.
 * @param context Void pointer to the field.
 * @return Python object with the attribute value, NULL on failure.
 */
static PyObject *Party_GetAttribute(Atrinik_Party *party, void *context)
{
    return generic_field_getter(context, party->party);
}

/**
 * Set attribute of a party.
 * @param party Python party wrapper.
 * @param value Value to set.
 * @param context Void pointer to the field.
 * @return 0 on success, -1 on failure.
 */
static int Party_SetAttribute(Atrinik_Party *party, PyObject *value,
        void *context)
{
    if (generic_field_setter(context, party->party, value) == -1) {
        return -1;
    }

    return 0;
}

/**
 * Create a new party wrapper.
 * @param type Type object.
 * @param args Unused.
 * @param kwds Unused.
 * @return The new wrapper.
 */
static PyObject *Atrinik_Party_new(PyTypeObject *type, PyObject *args,
        PyObject *kwds)
{
    Atrinik_Party *self = (Atrinik_Party *) type->tp_alloc(type, 0);
    if (self != NULL) {
        self->party = NULL;
    }

    return (PyObject *) self;
}

/**
 * Free a party wrapper.
 * @param self The wrapper to free.
 */
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
 * @param self The party object.
 * @return Python object containing the name of the party.
 */
static PyObject *Atrinik_Party_str(Atrinik_Party *self)
{
    return Py_BuildValue("s", self->party->name);
}

static int Atrinik_Party_InternalCompare(Atrinik_Party *left,
        Atrinik_Party *right)
{
    return (left->party < right->party ? -1 :
        (left->party == right->party ? 0 : 1));
}

static PyObject *Atrinik_Party_RichCompare(Atrinik_Party *left,
        Atrinik_Party *right, int op)
{
    if (left == NULL || right == NULL ||
            !PyObject_TypeCheck((PyObject *) left, &Atrinik_PartyType) ||
            !PyObject_TypeCheck((PyObject *) right, &Atrinik_PartyType)) {
        Py_INCREF(Py_NotImplemented);
        return Py_NotImplemented;
    }

    return generic_rich_compare(op, Atrinik_Party_InternalCompare(left, right));
}

/** This is filled in when we initialize our party type. */
static PyGetSetDef getseters[NUM_FIELDS + 1];

/** Our actual Python PartyType. */
PyTypeObject Atrinik_PartyType = {
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
#ifdef Py_TPFLAGS_HAVE_FINALIZE
    , NULL
#endif
};

/**
 * Initialize the party wrapper.
 * @param module The Atrinik Python module.
 * @return 1 on success, 0 on failure.
 */
int Atrinik_Party_init(PyObject *module)
{
    size_t i;

    /* Field getters */
    for (i = 0; i < NUM_FIELDS; i++) {
        PyGetSetDef *def = &getseters[i];

        def->name = fields[i].name;
        def->get = (getter) Party_GetAttribute;
        def->set = (setter) Party_SetAttribute;
        def->doc = fields[i].doc;
        def->closure = &fields[i];
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
 * @return Python object wrapping the real party.
 */
PyObject *wrap_party(party_struct *what)
{
    /* Return None if no party was to be wrapped. */
    if (what == NULL) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    Atrinik_Party *wrapper = PyObject_NEW(Atrinik_Party, &Atrinik_PartyType);
    if (wrapper != NULL) {
        wrapper->party = what;
    }

    return (PyObject *) wrapper;
}
