/*******************************************************************************
*               Atrinik, a Multiplayer Online Role Playing Game                *
*                                                                              *
*       Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team        *
*                                                                              *
* This program is free software; you can redistribute it and/or modify it      *
* under the terms of the GNU General Public License as published by the Free   *
* Software Foundation; either version 2 of the License, or (at your option)    *
* any later version.                                                           *
*                                                                              *
* This program is distributed in the hope that it will be useful, but WITHOUT  *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for     *
* more details.                                                                *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* this program; if not, write to the Free Software Foundation, Inc., 675 Mass  *
* Ave, Cambridge, MA 02139, USA.                                               *
*                                                                              *
* The author can be reached at admin@atrinik.org                               *
*******************************************************************************/

#pragma once

#include <stdint.h>
#include <atomic>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>

#include "game_object.h"

using namespace boost;
using namespace std;

namespace atrinik {

extern std::atomic<uint64_t> uid;
extern iobjects_t active_objects;
extern sobjects_t archetypes;
extern mutex active_objects_mutex;

class error : public std::exception {
public:
    string s;
    error(string desc) : s(desc) {}
    ~error() throw() {}
    virtual const char *what() const throw () { return s.c_str(); }
};

}
