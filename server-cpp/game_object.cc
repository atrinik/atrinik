/*******************************************************************************
 *               Atrinik, a Multiplayer Online Role Playing Game               *
 *                                                                             *
 *       Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team       *
 *                                                                             *
 * This program is free software; you can redistribute it and/or modify it     *
 * under the terms of the GNU General Public License as published by the Free  *
 * Software Foundation; either version 2 of the License, or (at your option)   *
 * any later version.                                                          *
 *                                                                             *
 * This program is distributed in the hope that it will be useful, but WITHOUT *
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       *
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for    *
 * more details.                                                               *
 *                                                                             *
 * You should have received a copy of the GNU General Public License along     *
 * with this program; if not, write to the Free Software Foundation, Inc.,     *
 * 675 Mass Ave, Cambridge, MA 02139, USA.                                     *
 *                                                                             *
 * The author can be reached at admin@atrinik.org                              *
 ******************************************************************************/

/**
 * @file
 * Generic game object implementation.
 */

#include <boost/lexical_cast.hpp>

#include "game_object.h"

using namespace atrinik;
using namespace boost;

namespace atrinik {

GameObject::sobjects_t GameObject::archetypes;

void GameObject::load(const string& key, const string& val)
{
    if (key == "name") {
        name = val;
    } else if (key == "layer") {
        layer = lexical_cast<uint8_t>(val);
    } else if (key == "x") {
        x = lexical_cast<uint16_t>(val);
    } else if (key == "y") {
        y = lexical_cast<uint16_t>(val);
    }
}

string GameObject::dump()
{
    string s;

    s = "arch " + archname + "\n";
    s += "name " + name + "\n";
    s += "layer " + lexical_cast<string>(layer) + "\n";
    s += "x " + lexical_cast<string>(x) + "\n";
    s += "y " + lexical_cast<string>(y) + "\n";
    s += "end\n";

    return s;
}

}
