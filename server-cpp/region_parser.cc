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
 * Region parser implementation.
 */

#include <fstream>
#include <boost/lexical_cast.hpp>

#include "region_parser.h"
#include "region_object.h"

using namespace atrinik;
using namespace boost;
using namespace std;

namespace atrinik {

void RegionParser::load(const std::string& path)
{
    ifstream file(path);
    property_tree::ptree pt = parse(file);

    // Load up the regions into the regions map
    for (auto it : pt) {
        string name = it.second.get<string>(it.first);

        RegionObject *obj = new RegionObject();
        obj->name(name);

        for (auto it2 : it.second) {
            obj->load(it2.first, it.second.get<string>(it2.first));
        }

        RegionObject::regions.insert(make_pair(name, obj));
    }

    // Link up children/parents
    for (auto it : RegionObject::regions) {
        if (it.second->parent().empty()) {
            continue;
        }

        RegionObject::regions_t::iterator it2 = RegionObject::regions.find(
                it.second->parent());

        if (it2 == RegionObject::regions.end()) {
            // TODO: error
            continue;
        }

        it2->second->inv_push_back(it.second);
    }

    /*for (auto it : RegionObject::regions) {
        cout << it.second->dump() << endl;
    }*/
}

}
