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

#include <region_parser.h>
#include <region_object.h>
#include <logger.h>

using namespace atrinik;
using namespace boost;
using namespace std;

namespace atrinik {

void RegionParser::load(const std::string& path)
{
    BOOST_LOG_FUNCTION();

    RegionManager::setup();
    LOG(Detail) << "Loading regions from: " << path;
    ifstream file(path);

    if (!file.is_open()) {
        RegionManager::setup(false);
        throw LOG_EXCEPTION(runtime_error("could not open file"));
    }

    RegionObjectPtr region;

    parse(file,
            [&region] (const std::string & key,
            const std::string & val) mutable -> bool
            {
                if (key.empty() && val == "end") {
                    RegionManager::add(region);
                    region.reset();
                } else if (key == "region") {
                    region.reset(new RegionObject());
                    region->name(val);
                } else if (!region) {
                    LOG(Error) << "Unrecognized attribute (before region "
                            "definition): " << key << " " << val;
                    RegionManager::setup(false);
                    throw LOG_EXCEPTION(runtime_error(
                            "corrupted regions file"));
                } else if (!region->load(key, val)) {
                    LOG(Error) << "Unrecognized attribute: " << key << " " <<
                            val;
                    RegionManager::setup(false);
                    throw LOG_EXCEPTION(runtime_error(
                            "corrupted regions file"));
                }

                return true;
            }
    );

    LOG(Detail) << "Loaded " << RegionManager::count() <<
            " regions";
    RegionManager::setup(true);
}

}
