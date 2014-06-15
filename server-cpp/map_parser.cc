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

#include "map_parser.h"
#include "game_object.h"

using namespace atrinik;
using namespace boost;

namespace atrinik {

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
        string archname = it->second.get<string>(it->first);
        GameObject::sobjects_t::accessor result;

        if (GameObject::archetypes.find(result, archname)) {
            GameObject *obj = result->second->clone();

            // Load object attributes
            for (auto it2 : it->second) {
                obj->load(it2.first, it->second.get<string>(it2.first));
            }

            // Add object to map's tile
            MapTile& tile = map->tile_get(obj->x(), obj->y());
            tile.objects.push_back(obj);
        } else {
            // TODO error log
        }
    }

    cout << map->dump() << endl;
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
