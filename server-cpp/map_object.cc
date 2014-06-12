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
 * Map object implementation.
 */

#include <boost/lexical_cast.hpp>

#include "map_object.h"

using namespace std;
using namespace atrinik;

namespace atrinik {

bool MapObject::load(string key, string val)
{
    if (key == "bg_music") {
        bg_music_ = val;
        return true;
    } else if (key == "weather") {
        weather_ = val;
        return true;
    } else if (key == "region") {
        region_ = val;
        return true;
    } else if (key == "width") {
        size_.first = lexical_cast<int>(val);
        return true;
    } else if (key == "height") {
        size_.second = lexical_cast<int>(val);
        return true;
    }

    return Object::load(key, val);
}

string MapObject::dump_()
{
    string s = "arch map\n";

    if (!bg_music_.empty()) {
        s += "bg_music " + bg_music_ + "\n";
    }

    if (!weather_.empty()) {
        s += "weather " + weather_ + "\n";
    }

    if (!region_.empty()) {
        s += "region " + region_ + "\n";
    }

    s += "width " + lexical_cast<string>(size_.first) + "\n";
    s += "height " + lexical_cast<string>(size_.second) + "\n";

    return s + Object::dump_();
}

const string& MapObject::path()
{
    return path_;
}

}
