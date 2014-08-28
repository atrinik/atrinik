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

#include <map_parser.h>
#include <game_object.h>
#include <logger.h>

using namespace atrinik;
using namespace boost;
using namespace std;

namespace atrinik {

void MapParser::load(const std::string& path, MapObjectPtr map)
{
    BOOST_LOG_FUNCTION();

    LOG(Detail) << "Loading map from: " << path;
    ifstream file(path);

    if (!file.is_open()) {
        throw LOG_EXCEPTION(runtime_error("could not open file"));
    }

    string line;

    // Verify map header exists
    if (!getline(file, line) || line != "arch map") {
        throw LOG_EXCEPTION(runtime_error("file is not a map"));
    }

    parse(file,
            [&map] (const std::string & key,
            const std::string & val) mutable -> bool
            {
                if (key.empty() && val == "end") {
                    return false;
                } else if (!map->load(key, val)) {
                    LOG(Error) << "Unrecognized attribute: " << key << " " <<
                            val;
                }

                return true;
            }
    );

    // TODO: width/height/etc checks

    map->allocate();

    auto pt = parse(file);

    // Load the rest of the objects
    for (auto it : pt) {
        auto obj = parse(it.second);
        int x = 0, y = 0;

        try {
            x = lexical_cast<int>(it.second.get<string>("x"));
        } catch (property_tree::ptree_bad_path&) {
        } catch (bad_lexical_cast&) {
        }

        try {
            y = lexical_cast<int>(it.second.get<string>("y"));
        } catch (property_tree::ptree_bad_path&) {
        } catch (bad_lexical_cast&) {
        }

        map->tile_get(x, y)->inv_push_back(obj);
    }

    LOG(Detail) << "Loaded map with " << pt.size() <<
            " objects (inventories not included)";
}

}
