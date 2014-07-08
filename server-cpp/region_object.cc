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
 * Region object implementation.
 */

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "region_object.h"

using namespace atrinik;
using namespace boost;
using namespace std;

namespace atrinik {

RegionObject::regions_t RegionObject::regions;

bool RegionObject::load(const std::string& key, const std::string& val)
{
    if (key == "name") {
        name(val);
        return true;
    } else if (key == "parent") {
        parent(val);
        return true;
    } else if (key == "longname") {
        longname(val);
        return true;
    } else if (key == "msg") {
        msg(val);
        return true;
    } else if (key == "jailmap") {
        jailmap(val);
        return true;
    } else if (key == "jailx") {
        jailx(lexical_cast<uint16_t>(val));
        return true;
    } else if (key == "jaily") {
        jaily(lexical_cast<uint16_t>(val));
        return true;
    } else if (key == "map_first") {
        map_first(val);
        return true;
    } else if (key == "map_bg") {
        map_bg(val);
        return true;
    } else if (key == "map_quest") {
        f_map_quest(true);
        return true;
    }

    return false;
}

std::string RegionObject::dump()
{
    string s = "";

    s += "region " + name() + "\n";

    if (!parent().empty()) {
        s += "parent " + parent() + "\n";
    }

    if (!longname().empty()) {
        s += "longname " + longname() + "\n";
    }

    if (!msg().empty()) {
        s += "msg\n";
        s += msg() + "\n";
        s += "endmsg\n";
    }

    if (!jailmap().empty()) {
        s += "jailmap " + jailmap() + "\n";
    }

    if (jailx() != 0) {
        s += "jailx " + lexical_cast<string>(jailx()) + "\n";
    }

    if (jaily() != 0) {
        s += "jaily " + lexical_cast<string>(jaily()) + "\n";
    }

    if (!map_first().empty()) {
        s += "map_first " + map_first() + "\n";
    }

    if (!map_bg().empty()) {
        s += "map_bg " + map_bg() + "\n";
    }

    if (f_map_quest()) {
        s += "map_quest 1\n";
    }

    return s;
}

}
