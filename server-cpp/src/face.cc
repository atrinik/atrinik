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
 * Face implementation.
 */

#include <boost/filesystem.hpp>

#include <face.h>
#include <face_parser.h>
#include <logger.h>

using namespace atrinik;
using namespace boost;
using namespace std;

namespace atrinik {

int Face::uid(0);

template<> bool Manager<FaceManager>::use_secondary = true;

const char* FaceManager::path()
{
    return "../arch/atrinik.0";
}

void FaceManager::load()
{
    BOOST_LOG_FUNCTION();

    filesystem::path p(path());
    time_t t = filesystem::last_write_time(p);

    if (t > manager().timestamp) {
        Face::uid = 0;
        FaceParser::load(path());
        manager().timestamp = t;
    }
}

void FaceManager::add(FacePtr face)
{
    BOOST_LOG_FUNCTION();

    manager().faces_vector.push_back(face);

    if (!manager().faces_map.insert(make_pair(face->name(), face)).second) {
        throw LOG_EXCEPTION(runtime_error("could not insert face"));
    }
}

FacePtrConst FaceManager::get(const std::string& name)
{
    const auto& it = manager().faces_map.find(name);

    if (it == manager().faces_map.end()) { // Doesn't exist, return default
        return manager().faces_vector[0];
    }

    return it->second;
}

FacePtrConst FaceManager::get(Face::FaceId id)
{
    try {
        return manager().faces_vector.at(id);
    } catch (out_of_range&) { // Face ID doesn't exist, return default
        return manager().faces_vector[0];
    }
}

FaceManager::FaceVector::size_type FaceManager::count()
{
    return manager().faces_vector.size();
}

void FaceManager::clear()
{
    faces_vector.clear();
    faces_map.clear();
}

};
