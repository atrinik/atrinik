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
 * Atrinik Python plugin region related code. */

#include <plugin_python.h>

/**
 * Region fields. */
/* @cparser
 * @page plugin_python_region_fields Python region fields
 * <h2>Python region fields</h2>
 * List of the region fields and their meaning. */
static fields_struct fields[] =
{
    {"next", FIELDTYPE_REGION, offsetof(region_struct, next), 0, 0},
    {"parent", FIELDTYPE_REGION, offsetof(region_struct, parent), 0, 0},
    {"name", FIELDTYPE_CSTR, offsetof(region_struct, name), 0, 0},
    {"longname", FIELDTYPE_CSTR, offsetof(region_struct, longname), 0, 0},
    {"msg", FIELDTYPE_CSTR, offsetof(region_struct, msg), 0, 0},
    {"jailmap", FIELDTYPE_CSTR, offsetof(region_struct, jailmap), 0, 0},
    {"jailx", FIELDTYPE_SINT16, offsetof(region_struct, jailx), 0, 0},
    {"jaily", FIELDTYPE_SINT16, offsetof(region_struct, jaily), 0, 0}
};
/* @endcparser */

/**
 * Get region's attribute.
 * @param r Python region wrapper.
 * @param context Void pointer to the field ID.
 * @return Python object with the attribute value, NULL on failure. */
static PyObject *Region_GetAttribute(Atrinik_Region *r, void *context)
{
    return generic_field_getter(context, r->region);
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

    if (self) {
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
    if (!left || !right || !PyObject_TypeCheck((PyObject *) left, &Atrinik_RegionType) || !PyObject_TypeCheck((PyObject *) right, &Atrinik_RegionType)) {
        Py_INCREF(Py_NotImplemented);
        return Py_NotImplemented;
    }

    return generic_rich_compare(op, Atrinik_Region_InternalCompare(left, right));
}

/**
 * This is filled in when we initialize our region type. */
static PyGetSetDef getseters[NUM_FIELDS + 1];

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
    getseters,
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
    size_t i;

    /* Field getters */
    for (i = 0; i < NUM_FIELDS; i++) {
        PyGetSetDef *def = &getseters[i];

        def->name = fields[i].name;
        def->get = (getter) Region_GetAttribute;
        def->set = NULL;
        def->doc = NULL;
        def->closure = &fields[i];
    }

    getseters[i].name = NULL;

    Atrinik_RegionType.tp_new = PyType_GenericNew;

    if (PyType_Ready(&Atrinik_RegionType) < 0) {
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
PyObject *wrap_region(region_struct *what)
{
    Atrinik_Region *wrapper;

    /* Return None if no region was to be wrapped. */
    if (!what) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    wrapper = PyObject_NEW(Atrinik_Region, &Atrinik_RegionType);

    if (wrapper) {
        wrapper->region = what;
    }

    return (PyObject *) wrapper;
}
