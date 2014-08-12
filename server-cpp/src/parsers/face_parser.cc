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
 * Face parser implementation.
 */

#include <string.h>
#include <sstream>
#include <fstream>
#include <boost/algorithm/string/predicate.hpp>

/*
#include <boost/lexical_cast.hpp>
#include <boost/numeric/conversion/cast.hpp>
 * */

#include <face_parser.h>
#include <face.h>
#include <server.h>

using namespace atrinik;
using namespace boost;
using namespace std;

namespace atrinik {

void FaceParser::load(const std::string& path)
{
    ifstream file(path, ios::binary);

    if (!file) {
        throw runtime_error("could not open file");
    }

    string line;

    while (getline(file, line)) {
        if (!starts_with(line, "IMAGE ")) {
            throw runtime_error("line does not begin with 'IMAGE '");
        }
        
        stringstream ss(line.substr(6));
        uint16_t id, bytes;
        string name;
        
        ss >> id >> bytes >> name;
        
        char* data = new char[bytes];
        
        if (file.read(data, bytes)) {
            Face* face = new Face(name);
            face->data(make_pair(data, bytes));
            FaceManager::manager.add(face);
        } else {
            // TODO: error, only read X number of bytes
        }
    }
}

}
