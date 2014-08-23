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

#include <face_parser.h>
#include <face.h>
#include <server.h>
#include <logger.h>

using namespace atrinik;
using namespace boost;
using namespace std;

namespace atrinik {

void FaceParser::load(const std::string& path)
{
    BOOST_LOG_FUNCTION();

    LOG(Detail) << "Loading faces from: " << path;
    ifstream file(path, ios::binary);

    if (!file.is_open()) {
        throw LOG_EXCEPTION(runtime_error("could not open file"));
    }

    string line;

    while (getline(file, line)) {
        if (!starts_with(line, "IMAGE ")) {
            LOG(Error) << "Unrecognized attribute (should begin with "
                    "'IMAGE '): " << line;
            throw LOG_EXCEPTION(runtime_error("corrupted faces file"));
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
            LOG(Error) << "Read " << file.gcount() <<
                    " bytes, expected to read " << bytes << " bytes";
            throw LOG_EXCEPTION(runtime_error("corrupted faces file"));
        }
    }

    LOG(Detail) << "Loaded " << FaceManager::manager.count() << " faces";
}

}
