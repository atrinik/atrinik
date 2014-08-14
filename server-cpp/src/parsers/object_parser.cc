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
 * Object parser implementation.
 */

#include <string.h>
#include <fstream>
#include <boost/algorithm/string/predicate.hpp>

#include <object_parser.h>
#include <base_object_type.h>
#include <player_object_type.h>
#include <sign_object_type.h>
#include <gfx_object_type.h>
#include <anim_object_type.h>
#include <msg_object_type.h>

using namespace atrinik;
using namespace boost;
using namespace std;

namespace atrinik {

void ObjectParser::parse(std::ifstream& file, std::function<bool(
        const std::string&, const std::string&) > handler)
{
    string line;

    while (getline(file, line)) {
        if (line.empty() || starts_with(line, "#")) {
            continue;
        }

        string key, val;

        if (line == "msg") {
            key = line;

            while (getline(file, line)) {
                if (line == "endmsg") {
                    break;
                }

                if (!val.empty()) {
                    val += "\n";
                }

                val += line;
            }
        } else {
            size_t space = line.find_first_of(' ');

            if (space == string::npos) {
                val = line;
            } else {
                key = line.substr(0, space);
                val = line.substr(space + 1);
            }
        }

        if (!handler(key, val)) {
            break;
        }
    }
}

property_tree::ptree ObjectParser::parse(ifstream& file,
        std::function<bool(const std::string&) > is_definition)
{
    property_tree::ptree pt;

    parse(file, [&file, &pt, is_definition] (const std::string& key,
            const std::string & val) mutable -> bool
            {
                if (key.empty()) {
                    if (val == "end") {
                        return false;
                    } else {
                        // TODO: parsing error
                    }
                }

                if ((is_definition && is_definition(key)) || key == "Object" ||
                        key == "arch") {
                    property_tree::ptree pt2;

                    pt2 = parse(file);
                    pt2.put<string>(key, val);
                    pt.add_child(key, pt2);
                } else {
                    pt.put<string>(key, val);
                }

                return true;
            });

    return pt;
}

template<typename T>
void assign_type(GameObject* obj, vector<GameObjectType*> v)
{
    v.push_back(obj->getaddinstance<T>());
}

void ObjectParser::assign_types(const boost::property_tree::ptree& pt,
        GameObject* obj, GameObject::Types type)
{
    vector<GameObjectType*> types;

    switch (type) {
    case GameObject::Types::Player:
        assign_type<BaseObjectType>(obj, types);
        assign_type<PlayerObjectType>(obj, types);
        break;

    case GameObject::Types::MiscObject:
        assign_type<BaseObjectType>(obj, types);
        assign_type<GfxObjectType>(obj, types);
        break;

    case GameObject::Types::Sign:
        assign_type<BaseObjectType>(obj, types);
        assign_type<SignObjectType>(obj, types);
        break;
    }

    if (pt.count("layer") != 0) {
        assign_type<GfxObjectType>(obj, types);
    }

    if (pt.count("animation") != 0) {
        assign_type<AnimObjectType>(obj, types);
    }

    if (pt.count("msg") != 0) {
        assign_type<MsgObjectType>(obj, types);
    }

    if (type != GameObject::Types::None) {
        obj->cleaninstances(types);
    }
}

}
