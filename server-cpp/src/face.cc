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

#include <algorithm>

#include <face.h>

using namespace atrinik;
using namespace std;

namespace atrinik {

int Face::uid(0);

void FaceManager::add(Face* face)
{
    faces_vector.push_back(face);
            
    if (!faces_map.insert(make_pair(face->name(), face)).second) {
        throw runtime_error("could not insert face");
    }
}

const Face& FaceManager::get(const std::string& name)
{
    FaceMap::const_iterator it = faces_map.find(name);
    
    if (it == faces_map.end()) { // Doesn't exist, return default
        return *faces_vector[0];
    }
    
    return *it->second;
}

const Face& FaceManager::get(Face::FaceId id)
{
    try {
        return *faces_vector.at(id);
    } catch (out_of_range&) { // Face ID doesn't exist, return default
        return *faces_vector[0];
    }
}

};
