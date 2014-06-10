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

/**
 * @file
 * Generic game object implementation.
 */

#include <boost/lexical_cast.hpp>

#include "game_object.h"

using namespace atrinik;
using namespace boost;

namespace atrinik {

GameObject::GameObject(const string& archname) : Object()
{
    archname_ = archname;
}

bool GameObject::load(string key, string val)
{
    if (key == "type") {
        return true;
    }
    else if (key == "layer") {
        layer_ = lexical_cast<uint8_t>(val);
        return true;
    }

    return Object::load(key, val);
}

string GameObject::dump_()
{
    string s;

    s = "arch " + archname_ + "\n";
    s += "layer " + lexical_cast<string>(layer_) + "\n";
    s += Object::dump_();

    return s;
}

string const& GameObject::archname()
{
    return archname_;
}

}
