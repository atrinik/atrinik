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
 * Map tile object implementation.
 */

#include <boost/lexical_cast.hpp>

#include <map_tile_object.h>
#include <game_object.h>

using namespace atrinik;
using namespace boost;
using namespace std;

namespace atrinik {

void MapTileObject::inv_push_back(GameObject* obj)
{
    inv_.push_back(obj);
    obj->env(this);
}

bool MapTileObject::load(const std::string& key, const std::string& val)
{
    if (key == "x") {
        x_ = lexical_cast<int>(val);
        return true;
    } else if (key == "y") {
        y_ = lexical_cast<int>(val);
        return true;
    }

    return false;
}

std::string MapTileObject::dump()
{
#if defined(FUTURE)
    if (inv_.empty()) {
        return "";
    }
    
    string s = "arch maptile\n";

    if (x_ != 0) {
        s += "x " + lexical_cast<string>(x_) + "\n";
    }

    if (y_ != 0) {
        s += "y " + lexical_cast<string>(y_) + "\n";
    }
#else
    string s = "";
#endif

    for (auto it : inv_) {
        s += it->dump();
    }

#if defined(FUTURE)
    s += "end\n";
#endif

    return s;
}

}
