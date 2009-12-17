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
 * Atrinik Python plugin party related code. */

#include <atrinik_party.h>

/** Available Python methods for the AtrinikParty object */
static PyMethodDef PartyMethods[] =
{
	{"AddMember",               (PyCFunction) Atrinik_Party_AddMember,               METH_VARARGS, 0},
	{"RemoveMember",            (PyCFunction) Atrinik_Party_RemoveMember,            METH_VARARGS, 0},
	{"GetMembers",              (PyCFunction) Atrinik_Party_GetMembers,              METH_VARARGS, 0},
	{"SendMessage",             (PyCFunction) Atrinik_Party_SendMessage,             METH_VARARGS, 0},
	{NULL, NULL, 0, 0}
};

/** Party fields structure. */
typedef struct
{
	/** Name of the field */
	char *name;

	/** Field type */
	field_type type;

	/** Offset in party structure */
	uint32 offset;

	/** Flags for special handling */
	uint32 flags;
} party_fields_struct;

/**
 * @anchor plugin_python_party_fields
 *
 * Party fields.
 *
 * List of the fields and their meaning:
 *
 * - <b>name</b>: @copydoc partylist_struct::name
 * - <b>leader</b>: @copydoc partylist_struct::leader
 * - <b>password</b>: @copydoc partylist_struct::passwd */
party_fields_struct party_fields[] =
{
	{"name",            FIELDTYPE_CSTR,     offsetof(partylist_struct, name),       FIELDFLAG_READONLY},
	{"leader",          FIELDTYPE_SHSTR,    offsetof(partylist_struct, leader),     0},
	{"password",        FIELDTYPE_CARY,     offsetof(partylist_struct, passwd),     FIELDFLAG_READONLY},
};

/** Number of party fields */
#define NUM_PARTYFIELDS (sizeof(party_fields) / sizeof(party_fields[0]))

/** This is filled in when we initialize our party type. */
static PyGetSetDef Party_getseters[NUM_PARTYFIELDS + 1];

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
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	(reprfunc) Atrinik_Party_str,
	0, 0, 0,
	Py_TPFLAGS_DEFAULT,
	"Atrinik parties",
	0, 0, 0, 0, 0, 0,
	PartyMethods,
	0,
	Party_getseters,
	0, 0, 0, 0, 0, 0, 0,
	Atrinik_Party_new,
	0, 0, 0, 0, 0, 0, 0, 0
#ifndef IS_PY_LEGACY
	, 0
#endif
};

/**
 * @anchor plugin_python_party_constants
 * Party related constants. */
static Atrinik_Constant party_constants[] =
{
	{"PARTY_MESSAGE_STATUS",   PARTY_MESSAGE_STATUS},
	{"PARTY_MESSAGE_CHAT",     PARTY_MESSAGE_CHAT},
	{NULL,                     0}
};

/**
 * @defgroup plugin_python_party_functions Python plugin party functions
 * Party related functions used in Atrinik Python plugin.
 *@{*/

/**
 * <h1>party.AddMember(<i>\<object\></i> player)</h1>
 * Add a player to the specified party.
 * @param player Player object to add to the party. */
static PyObject *Atrinik_Party_AddMember(Atrinik_Party *party, PyObject *args)
{
	Atrinik_Object *ob;
	char buf[MAX_BUF];

	if (!PyArg_ParseTuple(args, "|O!", &Atrinik_ObjectType, &ob))
	{
		return NULL;
	}

	if (ob->obj->type != PLAYER || !CONTR(ob->obj))
	{
		RAISE("First parameter must be a player object.");
	}
	else if (CONTR(ob->obj)->party)
	{
		if (CONTR(ob->obj)->party == party->party)
		{
			RAISE("The specified player object is already in the specified party.");
		}
		else
		{
			RAISE("The specified player object is already in another party.");
		}
	}

	hooks->add_party_member(party->party, ob->obj);
	snprintf(buf, sizeof(buf), "Xjoin\nsuccess\n%s", party->party->name);
	hooks->Write_String_To_Socket(&CONTR(ob->obj)->socket, BINARY_CMD_PARTY, buf, strlen(buf));

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>party.RemoveMember(<i>\<object\></i> player)</h1>
 * Remove a player from the specified party.
 * @param player Player object to remove from the party. */
static PyObject *Atrinik_Party_RemoveMember(Atrinik_Party *party, PyObject *args)
{
	Atrinik_Object *ob;
	char buf[MAX_BUF];

	if (!PyArg_ParseTuple(args, "|O!", &Atrinik_ObjectType, &ob))
	{
		return NULL;
	}

	if (ob->obj->type != PLAYER || !CONTR(ob->obj))
	{
		RAISE("First parameter must be a player object.");
	}
	else if (!CONTR(ob->obj)->party)
	{
		RAISE("The specified player is not in a party.");
	}

	hooks->remove_party_member(party->party, ob->obj);
	strcpy(buf, "Xjoin\nsuccess\n ");
	hooks->Write_String_To_Socket(&CONTR(ob->obj)->socket, BINARY_CMD_PARTY, buf, strlen(buf));

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

	for (ol = party->party->members; ol; ol = ol->next)
	{
		PyList_Append(list, wrap_object(ol->objlink.ob));
	}

	return list;
}

/**
 * <h1>party.SendMessage(<i>\<string\></i> message, <i>\<int\></i> flags,
 * <i>[object]</i> player)</h1>
 * Send a message to members of a party.
 * @param message Message to send.
 * @param flags Flags. See @ref PARTY_MESSAGE_xxx.
 * @param player Player object to exclude from sending the message. */
static PyObject *Atrinik_Party_SendMessage(Atrinik_Party *party, PyObject *args)
{
	Atrinik_Object *ob = NULL;
	int flags;
	char *msg;

	if (!PyArg_ParseTuple(args, "si|O!", &msg, &flags, &Atrinik_ObjectType, &ob))
	{
		return NULL;
	}

	hooks->send_party_message(party->party, msg, flags, ob ? ob->obj : NULL);

	Py_INCREF(Py_None);
	return Py_None;
}

/*@}*/

/* Party attribute getter */
static PyObject *Party_GetAttribute(Atrinik_Party *party, int fieldno)
{
	void *field_ptr;

	if (fieldno < 0 || fieldno >= (int) NUM_PARTYFIELDS)
	{
		RAISE("Illegal field ID");
	}

	field_ptr = (void *) ((char *) (party->party) + party_fields[fieldno].offset);

	/* TODO: better handling of types, signs, and overflows */
	switch (party_fields[fieldno].type)
	{
		case FIELDTYPE_SHSTR:
		case FIELDTYPE_CSTR:
			return Py_BuildValue("s", *(char **) field_ptr);

		case FIELDTYPE_CARY:
			return Py_BuildValue("s", (char *) field_ptr);

		default:
			RAISE("BUG: Unknown field type");
	}
}

/* Object attribute setter */
static int Party_SetAttribute(Atrinik_Object* whoptr, PyObject *value, int fieldno)
{
	void *field_ptr;
	uint32 offset;

	if (fieldno < 0 || fieldno >= (int) NUM_PARTYFIELDS)
	{
		INTRAISE("Illegal field ID");
	}

	if ((party_fields[fieldno].flags & FIELDFLAG_READONLY))
	{
		INTRAISE("Trying to modify readonly field");
	}

	offset = party_fields[fieldno].offset;
	field_ptr = (void *) ((char *) WHO + offset);

	switch (party_fields[fieldno].type)
	{
		case FIELDTYPE_SHSTR:
			if (PyString_Check(value))
			{
				const char *str = PyString_AsString(value);

				if (*(char **) field_ptr)
				{
					FREE_AND_CLEAR_HASH(*(char **) field_ptr);
				}

				if (str && strcmp(str, ""))
				{
					FREE_AND_COPY_HASH(*(const char **) field_ptr, str);
				}
			}
			else
			{
				INTRAISE("Illegal value for text field");
			}

			break;

		default:
			INTRAISE("BUG: Unknown field type");
	}

	return 0;
}

/* Initialize the Party Object Type */
int Atrinik_Party_init(PyObject *module)
{
	int i;

	/* Field getters */
	for (i = 0; i < (int) NUM_PARTYFIELDS; i++)
	{
		PyGetSetDef *def = &Party_getseters[i];

		def->name = party_fields[i].name;
		def->get = (getter) Party_GetAttribute;
		def->set = (setter) Party_SetAttribute;
		def->doc = NULL;
		def->closure = (void *) i;
	}

	Party_getseters[NUM_PARTYFIELDS].name = NULL;

	/* Add constants */
	for (i = 0; party_constants[i].name; i++)
	{
		if (PyModule_AddIntConstant(module, party_constants[i].name, party_constants[i].value))
		{
			return -1;
		}
	}

	Atrinik_PartyType.tp_new = PyType_GenericNew;

	if (PyType_Ready(&Atrinik_PartyType) < 0)
	{
		return -1;
	}

	return 0;
}

/* Create a new (uninitialized) Party wrapper */
static PyObject *Atrinik_Party_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	Atrinik_Party *self;

	(void) args;
	(void) kwds;

	self = (Atrinik_Party *) type->tp_alloc(type, 0);

	if (self)
	{
		self->party = NULL;
	}

	return (PyObject *) self;
}

/* Free a Party wrapper */
static void Atrinik_Party_dealloc(Atrinik_Party *self)
{
	self->party = NULL;
#ifndef IS_PY_LEGACY
	Py_TYPE(self)->tp_free((PyObject *) self);
#else
	self->ob_type->tp_free((PyObject *) self);
#endif
}

/* Return a string representation of this party (useful for debugging) */
static PyObject *Atrinik_Party_str(Atrinik_Party *self)
{
	return Py_BuildValue("s", self->party->name);
}

/* Utility method to wrap a party. */
PyObject *wrap_party(partylist_struct *what)
{
	Atrinik_Party *wrapper;

	/* Return None if no party was to be wrapped */
	if (what == NULL)
	{
		Py_INCREF(Py_None);
		return Py_None;
	}

	wrapper = PyObject_NEW(Atrinik_Party, &Atrinik_PartyType);

	if (wrapper != NULL)
	{
		wrapper->party = what;
	}

	return (PyObject *) wrapper;
}
