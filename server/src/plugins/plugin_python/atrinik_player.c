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
 * Atrinik Python plugin player related code. */

#include <plugin_python.h>

/**
 * Player fields. */
/* @cparser
 * @page plugin_python_player_fields Python player fields
 * <h2>Python player fields</h2>
 * List of the player fields and their meaning. */
static fields_struct fields[] =
{
	{"party", FIELDTYPE_PARTY, offsetof(player, party), 0, 0},
	/* Shall not be modified in any way. Instead, one should use @ref Atrinik_Player_Fix "player.Fix()",
	 * which will set class_ob to the last @ref CLASS object it finds in
	 * player's inventory. In some cases, this is done automatically after
	 * a script has finished executing (say events do this, for example). */
	{"class_ob", FIELDTYPE_OBJECT, offsetof(player, class_ob), FIELDFLAG_READONLY, 0},
	{"savebed_map", FIELDTYPE_CARY, offsetof(player, savebed_map), 0, sizeof(((player *) NULL)->savebed_map)},
	{"bed_x", FIELDTYPE_SINT16, offsetof(player, bed_x), 0, 0},
	{"bed_y", FIELDTYPE_SINT16, offsetof(player, bed_y), 0, 0},
	{"ob", FIELDTYPE_OBJECT, offsetof(player, ob), FIELDFLAG_READONLY, 0},
	{"quest_container", FIELDTYPE_OBJECT, offsetof(player, quest_container), FIELDFLAG_READONLY, 0},

	{"s_ext_title_flag", FIELDTYPE_UINT8, offsetof(player, socket.ext_title_flag), 0, 0},
	{"s_host", FIELDTYPE_CSTR, offsetof(player, socket.host), FIELDFLAG_READONLY, 0}
};
/* @endcparser */

/**
 * @defgroup plugin_python_player_functions Python player functions
 * Functions that can only be used on players (not the objects they are controlling).
 *
 * To access object's player controller, you can use something like:
 *
 * @code
 * activator = WhoIsActivator()
 * player = activator.Controller()
 * @endcode
 *
 * In the above example, player points to the player structure (which Python
 * is wrapping) that is controlling the object 'activator'. In this way, you can,
 * for example, use something like this to get player's save bed, among other
 * things:
 *
 * @code
 * print(WhoIsActivator().Controller().savebed_map)
 * @endcode
 *@{*/

/**
 * <h1>player.GetEquipment(\<int\> slot)</h1>
 * Get a player's current equipment for a given slot.
 * @param slot One of @ref PLAYER_EQUIP_xxx constants.
 * @throws ValueError if 'slot' is lower than 0 or higher than @ref PLAYER_EQUIP_MAX.
 * @return The equipment for the given slot, can be None. */
static PyObject *Atrinik_Player_GetEquipment(Atrinik_Player *pl, PyObject *args)
{
	int slot;

	if (!PyArg_ParseTuple(args, "i", &slot))
	{
		return NULL;
	}

	if (slot < 0 || slot >= PLAYER_EQUIP_MAX)
	{
		PyErr_SetString(PyExc_ValueError, "Invalid slot number.");
		return NULL;
	}

	return wrap_object(pl->pl->equipment[slot]);
}

/*@}*/

/** Available Python methods for the AtrinikPlayer type. */
static PyMethodDef methods[] =
{
	{"GetEquipment", (PyCFunction) Atrinik_Player_GetEquipment, METH_VARARGS, 0},
	{NULL, NULL, 0, 0}
};

/**
 * Get player's attribute.
 * @param pl Python player wrapper.
 * @param context Void pointer to the field ID.
 * @return Python object with the attribute value, NULL on failure. */
static PyObject *get_attribute(Atrinik_Player *pl, void *context)
{
	return generic_field_getter((fields_struct *) context, pl->pl);
}

/**
 * Set attribute of a player.
 * @param whoptr Python player wrapper.
 * @param value Value to set.
 * @param context Void pointer to the field.
 * @return 0 on success, -1 on failure. */
static int set_attribute(Atrinik_Player *pl, PyObject *value, void *context)
{
	if (generic_field_setter((fields_struct *) context, pl->pl, value) == -1)
	{
		return -1;
	}

	return 0;
}

/**
 * Create a new player wrapper.
 * @param type Type object.
 * @param args Unused.
 * @param kwds Unused.
 * @return The new wrapper. */
static PyObject *Atrinik_Player_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	Atrinik_Player *pl;

	(void) args;
	(void) kwds;

	pl = (Atrinik_Player *) type->tp_alloc(type, 0);

	if (pl)
	{
		pl->pl = NULL;
	}

	return (PyObject *) pl;
}

/**
 * Free a player wrapper.
 * @param pl The wrapper to free. */
static void Atrinik_Player_dealloc(Atrinik_Player *pl)
{
	pl->pl = NULL;
#ifndef IS_PY_LEGACY
	Py_TYPE(pl)->tp_free((PyObject *) pl);
#else
	pl->ob_type->tp_free((PyObject *) pl);
#endif
}

/**
 * Return a string representation of a player.
 * @param pl The player.
 * @return Python object containing the name of the player. */
static PyObject *Atrinik_Player_str(Atrinik_Player *pl)
{
	return Py_BuildValue("s", pl->pl->ob->name);
}

static int Atrinik_Player_InternalCompare(Atrinik_Player *left, Atrinik_Player *right)
{
	return (left->pl < right->pl ? -1 : (left->pl == right->pl ? 0 : 1));
}

static PyObject *Atrinik_Player_RichCompare(Atrinik_Player *left, Atrinik_Player *right, int op)
{
	if (!left || !right || !PyObject_TypeCheck((PyObject *) left, &Atrinik_PlayerType) || !PyObject_TypeCheck((PyObject *) right, &Atrinik_PlayerType))
	{
		Py_INCREF(Py_NotImplemented);
		return Py_NotImplemented;
	}

	return generic_rich_compare(op, Atrinik_Player_InternalCompare(left, right));
}

/**
 * This is filled in when we initialize our player type. */
static PyGetSetDef getseters[NUM_FIELDS + 1];

/** Our actual Python PlayerType. */
PyTypeObject Atrinik_PlayerType =
{
#ifdef IS_PY3K
	PyVarObject_HEAD_INIT(NULL, 0)
#else
	PyObject_HEAD_INIT(NULL)
	0,
#endif
	"Atrinik.Player",
	sizeof(Atrinik_Player),
	0,
	(destructor) Atrinik_Player_dealloc,
	NULL, NULL, NULL,
#ifdef IS_PY3K
	NULL,
#else
	(cmpfunc) Atrinik_Player_InternalCompare,
#endif
	0, 0, 0, 0, 0, 0,
	(reprfunc) Atrinik_Player_str,
	0, 0, 0,
	Py_TPFLAGS_DEFAULT,
	"Atrinik players",
	NULL, NULL,
	(richcmpfunc) Atrinik_Player_RichCompare,
	0, 0, 0,
	methods,
	0,
	getseters,
	0, 0, 0, 0, 0, 0, 0,
	Atrinik_Player_new,
	0, 0, 0, 0, 0, 0, 0, 0
#ifndef IS_PY_LEGACY
	, 0
#endif
};

/**
 * Initialize the player wrapper.
 * @param module The Atrinik Python module.
 * @return 1 on success, 0 on failure. */
int Atrinik_Player_init(PyObject *module)
{
	size_t i;

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

	getseters[i].name = NULL;

	Atrinik_PlayerType.tp_new = PyType_GenericNew;

	if (PyType_Ready(&Atrinik_PlayerType) < 0)
	{
		return 0;
	}

	Py_INCREF_TYPE(&Atrinik_PlayerType);
	PyModule_AddObject(module, "Player", (PyObject *) &Atrinik_PlayerType);

	return 1;
}

/**
 * Utility method to wrap a player.
 * @param what Player to wrap.
 * @return Python object wrapping the real player. */
PyObject *wrap_player(player *pl)
{
	Atrinik_Player *wrapper;

	/* Return None if no player was to be wrapped. */
	if (!pl)
	{
		Py_INCREF(Py_None);
		return Py_None;
	}

	wrapper = PyObject_NEW(Atrinik_Player, &Atrinik_PlayerType);

	if (wrapper)
	{
		wrapper->pl = pl;
	}

	return (PyObject *) wrapper;
}
