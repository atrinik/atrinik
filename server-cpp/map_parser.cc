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
 * Map parser implementation.
 */

#include <fstream>
#include <boost/property_tree/xml_parser.hpp>

#include "map_parser.h"
#include "game_object.h"

using namespace atrinik;
using namespace boost;

namespace atrinik {

static void parse_objects(MapObject* map, string archname,
        property_tree::ptree tree, GameObject* env = NULL)
{
    GameObject::sobjects_t::iterator result;

    result = GameObject::archetypes.find(archname);

    if (result == GameObject::archetypes.end()) {
        // TODO: error log
        return;
    }

    GameObject *obj = new GameObject(*result->second);

    // Load object attributes
    for (auto it : tree) {
        if (it.first == "arch") {
            string archname2;

            try {
                 archname2 = it.second.get<string>(it.first);
            } catch (property_tree::ptree_bad_path) {
                continue;
            }

            parse_objects(map, archname2, it.second, obj);
        } else {
            obj->load(it.first, tree.get<string>(it.first));
        }
    }

    // Add object to map's tile
    if (env == NULL) {
        MapTile& tile = map->tile_get(obj->x, obj->y);
        tile.objects.push_back(obj);
        obj->env = map;
    } else {
        env->inv.push_back(obj);
        obj->env = env;
    }
}

void MapParser::parse_map(MapObject* map)
{
    // Load map and parse it into a property tree
    ifstream file(map->path());
    property_tree::ptree pt = parse(file);
    property_tree::ptree::const_iterator it = pt.begin(), end = pt.end();

    if (it == end) {
        // TODO error message map header not found
        return;
    }

    // Load map header
    for (auto it2 : it->second) {
        map->load(it2.first, it->second.get<string>(it2.first));
    }

    map->allocate();

   // Load the rest of the objects
    for (it++; it != end; it++) {
        parse_objects(map, it->second.get<string>(it->first), it->second);
    }

    cout << map->dump() << endl;

    boost::property_tree::xml_writer_settings<char> settings('\t', 1);
    boost::property_tree::write_xml("map_output.xml", pt, std::locale(),
                                    settings);
}

MapObject* MapParser::load_map(const string& path)
{
    // TODO: loaded check
    // TODO: load from binary if it exists

    MapObject* map = new MapObject(path);
    parse_map(map);

    return map;
}

}