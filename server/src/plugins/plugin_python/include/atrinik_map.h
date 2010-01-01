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

#ifndef ATRINIK_MAP_H
#define ATRINIK_MAP_H

/* First the required header files - only the CF module interface and Python */
#include <Python.h>
#include <plugin.h>
#include <plugin_python.h>

/* Map object methods */
static PyObject *Atrinik_Map_GetFirstObjectOnSquare(Atrinik_Map *self, PyObject *args);
static PyObject *Atrinik_Map_PlaySound(Atrinik_Map *self, PyObject *args);
static PyObject *Atrinik_Map_Message(Atrinik_Map *self, PyObject *args);
static PyObject *Atrinik_Map_MapTileAt(Atrinik_Map *self, PyObject *args);
static PyObject *Atrinik_Map_CreateObject(Atrinik_Map *map, PyObject *args);
static PyObject *Atrinik_Map_CountPlayers(Atrinik_Map *map, PyObject *args);
static PyObject *Atrinik_Map_GetPlayers(Atrinik_Map *map, PyObject *args);

/* Object creator (not really needed, since the generic creator does the same thing...) */
static PyObject *Atrinik_Map_new(PyTypeObject *type, PyObject *args, PyObject *kwds);

/* str() function to get a string representation of this object */
static PyObject *Atrinik_Map_str(Atrinik_Map *self);

/* Object deallocator (needed) */
static void Atrinik_Map_dealloc(Atrinik_Map *self);

#endif
