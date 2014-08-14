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
 * Archetype parser implementation.
 */

#include <fstream>
#include <boost/lexical_cast.hpp>

#include <archetype_parser.h>
#include <game_object.h>

using namespace atrinik;
using namespace boost;
using namespace std;

namespace atrinik {

void ArchetypeParser::load(const std::string& path)
{
    ifstream file(path);
    
    if (!file) {
        throw runtime_error("could not open file");
    }
    
    property_tree::ptree pt;
    bool is_more = false, was_more = false;
    
    parse(file,
            [&file, &pt, &is_more, &was_more] (const std::string & key,
            const std::string & val) mutable -> bool
            {
                if (key.empty() && val == "end") {
                    was_more = is_more;
                    is_more = false;
                    return false;
                } else if (key == "More") {
                    is_more = true;
                } else if (key == "Object" || key == "arch") {
                    property_tree::ptree pt2;

                    pt2 = parse(file);
                    pt2.put<string>(key, val);

                    if (was_more) {
                        property_tree::ptree *last = &pt.rbegin()->second;
                        optional<property_tree::ptree&> child;

                        child = last->get_child_optional("More");

                        if (!child) {
                            property_tree::ptree pt3;
                            pt3.add_child(key, pt2);
                            last->add_child("More", pt3);
                        } else {
                            child->add_child(key, pt2);
                        }
                    } else {
                        pt.add_child(key, pt2);
                    }
                } else {
                    pt.put<string>(key, val);
                }

                return true;
            });
    
    for (auto it : pt) {
        string archname = it.second.get<string>(it.first);
        int type;

        // Try to parse the type
        try {
            type = lexical_cast<int>(it.second.get<string>("type"));
        } catch (std::exception &e) {
            //cout << e.what() << ": " << endl;
            continue;
        }

        GameObject *obj = new GameObject();
        obj->arch = archname;

        assign_types(it.second, obj, static_cast<GameObject::Types>(type));

        // Load the attributes
        for (auto it2 : it.second) {
            obj->load(it2.first, it.second.get<string>(it2.first));
        }

        // Insert into archetypes hashmap
        GameObject::archetypes.insert(make_pair(archname, obj));
    }
}

}
