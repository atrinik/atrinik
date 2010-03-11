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
 * Atrinik Python plugin region related code. */

#include <plugin_python.h>

/** Region fields structure. */
typedef struct
{
	/** Name of the field */
	char *name;

	/** Field type */
	field_type type;

	/** Offset in region structure */
	uint32 offset;
} region_fields_struct;

/**
 * Region fields. */
region_fields_struct region_fields[] =
{
	{"next",            FIELDTYPE_REGION,     offsetof(region, next)},
	{"parent",          FIELDTYPE_REGION,     offsetof(region, parent)},
	{"name",            FIELDTYPE_CSTR,       offsetof(region, name)},
	{"parent_name",     FIELDTYPE_CSTR,       offsetof(region, parent_name)},
	{"longname",        FIELDTYPE_CSTR,       offsetof(region, longname)},
	{"msg",             FIELDTYPE_CSTR,       offsetof(region, msg)},
	{"jailmap",         FIELDTYPE_CSTR,       offsetof(region, jailmap)},
	{"jailx",           FIELDTYPE_SINT16,     offsetof(region, jailx)},
	{"jaily",           FIELDTYPE_SINT16,     offsetof(region, jaily)},
};

/** Number of region fields */
#define NUM_REGIONFIELDS (sizeof(region_fields) / sizeof(region_fields[0]))

/**
 * Get region's attribute.
 * @param whoptr Python region wrapper.
 * @param fieldno Attribute ID.
 * @return Python object with the attribute value, NULL on failure. */
static PyObject *Region_GetAttribute(Atrinik_Region *r, int fieldno)
{
	void *field_ptr;

	if (fieldno < 0 || fieldno >= (int) NUM_REGIONFIELDS)
	{
		RAISE("Illegal field ID.");
	}

	field_ptr = (void *) ((char *) (r->region) + region_fields[fieldno].offset);

	switch (region_fields[fieldno].type)
	{
		case FIELDTYPE_SHSTR:
		case FIELDTYPE_CSTR:
			return Py_BuildValue("s", *(char **) field_ptr);

		case FIELDTYPE_SINT16:
			return Py_BuildValue("i", *(sint16 *) field_ptr);

		case FIELDTYPE_REGION:
			return wrap_region(*(region **) field_ptr);

		default:
			RAISE("BUG: Unknown field type.");
	}
}

/**
 * Create a new region wrapper.
 * @param type Type object.
 * @param args Unused.
 * @param kwds Unused.
 * @return The new wrapper. */
static PyObject *Atrinik_Region_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	Atrinik_Region *self;

	(void) args;
	(void) kwds;

	self = (Atrinik_Region *) type->tp_alloc(type, 0);

	if (self)
	{
		self->region = NULL;
	}

	return (PyObject *) self;
}

/**
 * Free a region wrapper.
 * @param self The wrapper to free. */
static void Atrinik_Region_dealloc(Atrinik_Region *self)
{
	self->region = NULL;
#ifndef IS_PY_LEGACY
	Py_TYPE(self)->tp_free((PyObject *) self);
#else
	self->ob_type->tp_free((PyObject *) self);
#endif
}

/**
 * Return a string representation of a region.
 * @param self The region type.
 * @return Python object containing the name of the region. */
static PyObject *Atrinik_Region_str(Atrinik_Region *self)
{
	return Py_BuildValue("s", self->region->name);
}

static int Atrinik_Region_InternalCompare(Atrinik_Region *left, Atrinik_Region *right)
{
	return (left->region < right->region ? -1 : (left->region == right->region ? 0 : 1));
}

static PyObject *Atrinik_Region_RichCompare(Atrinik_Region *left, Atrinik_Region *right, int op)
{
	int result;

	if (!left || !right || !PyObject_TypeCheck((PyObject *) left, &Atrinik_RegionType) || !PyObject_TypeCheck((PyObject *) right, &Atrinik_RegionType))
	{
		Py_INCREF(Py_NotImplemented);
		return Py_NotImplemented;
	}

	result = Atrinik_Region_InternalCompare(left, right);

	/* Based on how Python 3.0 (GPL compatible) implements it for internal types: */
	switch (op)
	{
		case Py_EQ:
			result = (result == 0);
			break;
		case Py_NE:
			result = (result != 0);
			break;
		case Py_LE:
			result = (result <= 0);
			break;
		case Py_GE:
			result = (result >= 0);
			break;
		case Py_LT:
			result = (result == -1);
			break;
		case Py_GT:
			result = (result == 1);
			break;
	}

	return PyBool_FromLong(result);
}

/** This is filled in when we initialize our region type. */
static PyGetSetDef Region_getseters[NUM_REGIONFIELDS + 1];

/** Our actual Python RegionType. */
PyTypeObject Atrinik_RegionType =
{
#ifdef IS_PY3K
	PyVarObject_HEAD_INIT(NULL, 0)
#else
	PyObject_HEAD_INIT(NULL)
	0,
#endif
	"Atrinik.Region",
	sizeof(Atrinik_Region),
	0,
	(destructor) Atrinik_Region_dealloc,
	NULL, NULL, NULL,
#ifdef IS_PY3K
	NULL,
#else
	(cmpfunc) Atrinik_Region_InternalCompare,
#endif
	0, 0, 0, 0, 0, 0,
	(reprfunc) Atrinik_Region_str,
	0, 0, 0,
	Py_TPFLAGS_DEFAULT,
	"Atrinik regions",
	NULL, NULL,
	(richcmpfunc) Atrinik_Region_RichCompare,
	0, 0, 0,
	NULL,
	0,
	Region_getseters,
	0, 0, 0, 0, 0, 0, 0,
	Atrinik_Region_new,
	0, 0, 0, 0, 0, 0, 0, 0
#ifndef IS_PY_LEGACY
	, 0
#endif
};

/**
 * Initialize the region wrapper.
 * @param module The Atrinik Python module.
 * @return 1 on success, 0 on failure. */
int Atrinik_Region_init(PyObject *module)
{
	int i;

	/* Field getters */
	for (i = 0; i < (int) NUM_REGIONFIELDS; i++)
	{
		PyGetSetDef *def = &Region_getseters[i];

		def->name = region_fields[i].name;
		def->get = (getter) Region_GetAttribute;
		def->set = NULL;
		def->doc = NULL;
		def->closure = (void *) i;
	}

	Region_getseters[NUM_REGIONFIELDS].name = NULL;

	Atrinik_RegionType.tp_new = PyType_GenericNew;

	if (PyType_Ready(&Atrinik_RegionType) < 0)
	{
		return 0;
	}

	Py_INCREF(&Atrinik_RegionType);
	PyModule_AddObject(module, "Region", (PyObject *) &Atrinik_RegionType);

	return 1;
}

/**
 * Utility method to wrap a region.
 * @param what Region to wrap.
 * @return Python object wrapping the real region. */
PyObject *wrap_region(region *what)
{
	Atrinik_Region *wrapper;

	/* Return None if no region was to be wrapped. */
	if (!what)
	{
		Py_INCREF(Py_None);
		return Py_None;
	}

	wrapper = PyObject_NEW(Atrinik_Region, &Atrinik_RegionType);

	if (wrapper)
	{
		wrapper->region = what;
	}

	return (PyObject *) wrapper;
}
