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
 * Atrinik Python plugin archetype related code. */

#include <plugin_python.h>

/**
 * Archetype fields. */
/* @cparser
 * @page plugin_python_archetype_fields Python archetype fields
 * <h2>Python archetype fields</h2>
 * List of the archetype fields and their meaning. */
static fields_struct fields[] =
{
    {"name", FIELDTYPE_SHSTR, offsetof(archetype, name), 0, 0},
    {"next", FIELDTYPE_ARCH, offsetof(archetype, next), 0, 0},
    {"head", FIELDTYPE_ARCH, offsetof(archetype, head), 0, 0},
    {"more", FIELDTYPE_ARCH, offsetof(archetype, more), 0, 0},
    {"clone", FIELDTYPE_OBJECT2, offsetof(archetype, clone), 0, 0}
};
/* @endcparser */

/**
 * Get archetype's attribute.
 * @param at Python archetype wrapper.
 * @param context Void pointer to the field ID.
 * @return Python object with the attribute value, NULL on failure. */
static PyObject *get_attribute(Atrinik_Archetype *at, void *context)
{
    return generic_field_getter(context, at->at);
}

/**
 * Create a new archetype wrapper.
 * @param type Type object.
 * @param args Unused.
 * @param kwds Unused.
 * @return The new wrapper. */
static PyObject *Atrinik_Archetype_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    Atrinik_Archetype *at;

    (void) args;
    (void) kwds;

    at = (Atrinik_Archetype *) type->tp_alloc(type, 0);

    if (at) {
        at->at = NULL;
    }

    return (PyObject *) at;
}

/**
 * Free an archetype wrapper.
 * @param pl The wrapper to free. */
static void Atrinik_Archetype_dealloc(Atrinik_Archetype *at)
{
    at->at = NULL;
#ifndef IS_PY_LEGACY
    Py_TYPE(at)->tp_free((PyObject *) at);
#else
    at->ob_type->tp_free((PyObject *) at);
#endif
}

/**
 * Return a string representation of an archetype.
 * @param at The archetype.
 * @return Python object containing the name of the archetype. */
static PyObject *Atrinik_Archetype_str(Atrinik_Archetype *at)
{
    return Py_BuildValue("s", at->at->name);
}

static int Atrinik_Archetype_InternalCompare(Atrinik_Archetype *left, Atrinik_Archetype *right)
{
    return (left->at < right->at ? -1 : (left->at == right->at ? 0 : 1));
}

static PyObject *Atrinik_Archetype_RichCompare(Atrinik_Archetype *left, Atrinik_Archetype *right, int op)
{
    if (!left || !right || !PyObject_TypeCheck((PyObject *) left, &Atrinik_ArchetypeType) || !PyObject_TypeCheck((PyObject *) right, &Atrinik_ArchetypeType)) {
        Py_INCREF(Py_NotImplemented);
        return Py_NotImplemented;
    }

    return generic_rich_compare(op, Atrinik_Archetype_InternalCompare(left, right));
}

/**
 * This is filled in when we initialize our archetype type. */
static PyGetSetDef getseters[NUM_FIELDS + 1];

/** Our actual Python ArchetypeType. */
PyTypeObject Atrinik_ArchetypeType =
{
#ifdef IS_PY3K
    PyVarObject_HEAD_INIT(NULL, 0)
#else
    PyObject_HEAD_INIT(NULL)
    0,
#endif
    "Atrinik.Archetype",
    sizeof(Atrinik_Archetype),
    0,
    (destructor) Atrinik_Archetype_dealloc,
    NULL, NULL, NULL,
#ifdef IS_PY3K
    NULL,
#else
    (cmpfunc) Atrinik_Archetype_InternalCompare,
#endif
    0, 0, 0, 0, 0, 0,
    (reprfunc) Atrinik_Archetype_str,
    0, 0, 0,
    Py_TPFLAGS_DEFAULT,
    "Atrinik archetypes",
    NULL, NULL,
    (richcmpfunc) Atrinik_Archetype_RichCompare,
    0, 0, 0,
    NULL,
    0,
    getseters,
    0, 0, 0, 0, 0, 0, 0,
    Atrinik_Archetype_new,
    0, 0, 0, 0, 0, 0, 0, 0
#ifndef IS_PY_LEGACY
    , 0
#endif
#ifdef Py_TPFLAGS_HAVE_FINALIZE
    , NULL
#endif
};

/**
 * Initialize the archetype wrapper.
 * @param module The Atrinik Python module.
 * @return 1 on success, 0 on failure. */
int Atrinik_Archetype_init(PyObject *module)
{
    size_t i;

    /* Field getters */
    for (i = 0; i < NUM_FIELDS; i++) {
        PyGetSetDef *def = &getseters[i];

        def->name = fields[i].name;
        def->get = (getter) get_attribute;
        def->set = NULL;
        def->doc = NULL;
        def->closure = &fields[i];
    }

    getseters[i].name = NULL;

    Atrinik_ArchetypeType.tp_new = PyType_GenericNew;

    if (PyType_Ready(&Atrinik_ArchetypeType) < 0) {
        return 0;
    }

    Py_INCREF(&Atrinik_ArchetypeType);
    PyModule_AddObject(module, "Archetype", (PyObject *) &Atrinik_ArchetypeType);

    return 1;
}

/**
 * Utility method to wrap an archetype.
 * @param what Archetype to wrap.
 * @return Python object wrapping the real archetype. */
PyObject *wrap_archetype(archetype *at)
{
    Atrinik_Archetype *wrapper;

    /* Return None if no archetype was to be wrapped. */
    if (!at) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    wrapper = PyObject_NEW(Atrinik_Archetype, &Atrinik_ArchetypeType);

    if (wrapper) {
        wrapper->at = at;
    }

    return (PyObject *) wrapper;
}

