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
 * Account parser implementation.
 */

#include <string.h>
#include <fstream>

#include <account_parser.h>

using namespace atrinik;
using namespace boost;
using namespace std;

namespace atrinik {

void AccountParser::parse(std::ifstream& file, AccountObject* obj)
{
    string line;

    while (getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        if (line == "end") {
            break;
        }

        size_t space = line.find_first_of(' ');
        obj->load(line.substr(0, space), line.substr(space + 1));
    }
}

}
