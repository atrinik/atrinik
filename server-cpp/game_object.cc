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
#include "map_tile_object.h"

using namespace atrinik;
using namespace boost;

namespace atrinik {

GameObject::sobjects_t GameObject::archetypes;

bool GameObject::load(const string& key, const string& val)
{
    if (key == "name") {
        name = val;
        return true;
    } else if (key == "typeid") {
        getaddinstance(val);
        return true;
    } else {
        for (auto it : types) {
            if (it->load(key, val)) {
                return true;
            }
        }
    }

    return false;
}

string GameObject::dump()
{
    string s;
    MapTileObject *tile;

    s = "arch " + archname + "\n";

    tile = dynamic_cast<MapTileObject*>(env);

    if (tile != NULL) {
        s += tile->dump();
    }

    if (!name.empty()) {
        s += "name " + name + "\n";
    }

    for (auto it : types) {
        s += "typeid " + it->gettypeid() + "\n";
        s += it->dump();
    }

    for (auto it : inv) {
        s += it->dump();
    }

    s += "end\n";

    return s;
}

}
