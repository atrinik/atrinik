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

    for (auto it : pt) {
        string name = it.second.get<string>(it.first);

/*
        // Try to parse the type
        try {
            type = lexical_cast<int>(it.second.get<string>("type"));
        } catch (std::exception &e) {
            //cout << e.what() << ": " << endl;
            continue;
        }

        GameObject *obj = new GameObject();
        obj->arch = archname;

        if (type == 98) {
            obj->addinstance<BaseObjectType>();
            obj->addinstance<GfxObjectType>();
            obj->addinstance<SignObjectType>();
        }

        // Load the attributes
        for (auto it2 : it.second) {
            obj->load(it2.first, it.second.get<string>(it2.first));
        }

        // Insert into archetypes hashmap
        GameObject::archetypes.insert(make_pair(archname, obj));
*/
    }

}

}
