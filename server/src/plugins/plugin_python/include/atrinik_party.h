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

#ifndef ATRINIK_PARTY_H
#define ATRINIK_PARTY_H

#include <Python.h>
#include <plugin.h>
#include <plugin_python.h>

static PyObject *Atrinik_Party_AddMember(Atrinik_Party *party, PyObject *args);
static PyObject *Atrinik_Party_RemoveMember(Atrinik_Party *party, PyObject *args);
static PyObject *Atrinik_Party_GetMembers(Atrinik_Party *party, PyObject *args);
static PyObject *Atrinik_Party_SendMessage(Atrinik_Party *party, PyObject *args);

static PyObject *Atrinik_Party_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
static void Atrinik_Party_dealloc(Atrinik_Party *self);
static PyObject *Atrinik_Party_str(Atrinik_Party *self);

#endif
