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
 * Player parser implementation.
 */

#include <fstream>

#include <player_parser.h>
#include <game_object.h>
#include <logger.h>
#include <base_object_type.h>

using namespace atrinik;
using namespace boost;
using namespace std;

namespace atrinik {

GameObjectPtr PlayerParser::load(const std::string& path)
{
    BOOST_LOG_FUNCTION();

    LOG(Detail) << "Loading player from: " << path;
    ifstream file(path);

    if (!file.is_open()) {
        throw LOG_EXCEPTION(runtime_error("could not open file"));
    }

    property_tree::ptree pt;

    parse(file,
            [&pt] (const std::string & key,
            const std::string & val) mutable -> bool
            {
                if (key.empty()) {
                    if (val == "endplst") {
                        return false;
                    } else {
                        LOG(Error) << "Unrecognized attribute: " << key <<
                                " " << val;
                    }
                }

                pt.put<string>(key, val);
                return true;
            }
    );

    const auto& pt2 = parse(file);
    auto obj = parse(pt2.begin()->second);

    LOG(Detail) << "Loaded player: " <<
            obj->getinstance<BaseObjectType>()->name();

    return obj;
}

}
