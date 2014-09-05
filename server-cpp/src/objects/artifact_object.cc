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
 * Artifact object implementation.
 */

#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/numeric/conversion/cast.hpp>

#include <artifact_object.h>
#include <artifact_parser.h>
#include <logger.h>

using namespace atrinik;
using namespace boost;
using namespace std;

namespace atrinik {

bool ArtifactObject::load_object(const std::string& key, const std::string& val)
{
    attributes.push_back(make_pair(key, val));
    return artifact->load(key, val);
}

bool ArtifactObject::load(const std::string& key, const std::string& val)
{
    if (key == "Allowed") {
        if (val != "none" && val != "all") {
            allowed.push_back(val);
        }

        return true;
    } else if (key == "artifact") {
        artifact->arch = val;
        return true;
    } else if (key == "def_arch") {
        artifact = GameObjectManager::get(val)->clone();
        return true;
    } else if (key == "chance") {
        chance = lexical_cast<uint16_t>(val);
        return true;
    } else if (key == "difficulty") {
        difficulty = numeric_cast<uint8_t>(lexical_cast<int>(val));
        return true;
    } else if (key == "t_style") {
        t_style = lexical_cast<int>(val);
        return true;
    } else if (key == "copy_artifact") {
        deep_copy = lexical_cast<bool>(val);
        return true;
    }

    return false;
}

std::string ArtifactObject::dump()
{
    string s;

    return s;
}

template<> bool Manager<ArtifactObjectManager>::use_secondary = true;

const char* ArtifactObjectManager::path()
{
    return "../arch/artifacts";
}

void ArtifactObjectManager::load()
{
    BOOST_LOG_FUNCTION();

    filesystem::path p(path());
    time_t t = filesystem::last_write_time(p);

    if (t > manager().timestamp) {
        ArtifactParser::load(path());
        manager().timestamp = t;
    }
}

void ArtifactObjectManager::add(ArtifactObjectPtr obj, int type)
{
    BOOST_LOG_FUNCTION();

    if (!manager().artifact_objects_map.insert(make_pair(obj->name(),
            obj)).second) {
        throw LOG_EXCEPTION(runtime_error("could not insert artifact object"));
    }

    manager().artifact_objects_list_map[type].push_back(obj);
}

boost::optional<ArtifactObjectPtrConst> ArtifactObjectManager::get(
        const std::string& name)
{
    BOOST_LOG_FUNCTION();

    auto result = manager().artifact_objects_map.find(name);

    if (result == manager().artifact_objects_map.end()) {
        return optional<ArtifactObjectPtrConst>();
    }

    return optional<ArtifactObjectPtrConst>(result->second);
}

ArtifactObjectManager::ArtifactObjectMap::size_type
ArtifactObjectManager::count()
{
    return manager().artifact_objects_map.size();
}

void ArtifactObjectManager::clear()
{
    artifact_objects_map.clear();
}

}
