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
 * Generic game object implementation.
 */

#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include <map_tile_object.h>
#include <game_object.h>
#include <archetype_parser.h>
#include <artifact_object.h>
#include <logger.h>

using namespace atrinik;
using namespace boost;
using namespace std;

namespace atrinik {

bool GameObject::load(const std::string& key, const std::string& val)
{
    if (key == "typeid") {
        getaddinstance(val);
        return true;
    }

    for (auto it : types) {
        if (it->load(key, val)) {
            return true;
        }
    }

    return false;
}

std::string GameObject::dump()
{
    string s;

    s = "arch " + boost::apply_visitor(GameObjectArchVisitor(), arch) + "\n";

#if !defined(FUTURE)
    MapTileObject *tile = map_tile();

    if (tile != NULL) {
        if (tile->x() != 0) {
            s += "x " + lexical_cast<string>(tile->x()) + "\n";
        }

        if (tile->y() != 0) {
            s += "y " + lexical_cast<string>(tile->y()) + "\n";
        }
    }
#endif

    GameObjectPtrConst arch = boost::get<GameObjectPtrConst>(this->arch);

    for (auto it : types) {
        GameObjectType *base = NULL;

        if (arch) {
            base = arch->getinstance(it->gettype());
        }

#if defined(FUTURE)
        string typedump = it->dump(base);

        if (base == NULL || !typedump.empty()) {
            s += "typeid " + it->gettypeid() + "\n";
            s += typedump;
        }
#else
        s += it->dump(base);
#endif
    }

    for (auto obj : inv_) {
        s += obj->dump();
    }

    s += "end\n";

    return s;
}

std::string GameObjectManager::SingularityObjectName = "singularity";

template<> bool Manager<GameObjectManager>::use_secondary = true;

const char* GameObjectManager::path()
{
    return "../arch/archetypes";
}

void GameObjectManager::load()
{
    BOOST_LOG_FUNCTION();

    filesystem::path p(path());
    time_t t = filesystem::last_write_time(p);

    if (t > manager().timestamp) {
        ArchetypeParser::load(path());
        manager().timestamp = t;
    }
}

void GameObjectManager::add(const std::string& archname, GameObjectPtr obj)
{
    BOOST_LOG_FUNCTION();

    if (!manager().game_objects_map.insert(make_pair(archname, obj)).second) {
        throw LOG_EXCEPTION(runtime_error("could not insert game object"));
    }
}

GameObjectPtrConst GameObjectManager::get(const std::string& archname)
{
    BOOST_LOG_FUNCTION();

    auto result = manager().game_objects_map.find(archname);

    if (result == manager().game_objects_map.end()) {
        auto artifact = ArtifactObjectManager::get(archname);

        if (artifact) {
            return (*artifact)->obj();
        }

        LOG(Error) << "Unknown archetype: " << archname;
        return manager().game_objects_map["singularity"]; //
    }

    return result->second;
}

GameObjectManager::GameObjectMap::size_type GameObjectManager::count()
{
    return manager().game_objects_map.size();
}

void GameObjectManager::clear()
{
    game_objects_map.clear();
}

}
