/*******************************************************************************
*               Atrinik, a Multiplayer Online Role Playing Game                *
*                                                                              *
*       Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team        *
*                                                                              *
* This program is free software; you can redistribute it and/or modify it      *
* under the terms of the GNU General Public License as published by the Free   *
* Software Foundation; either version 2 of the License, or (at your option)    *
* any later version.                                                           *
*                                                                              *
* This program is distributed in the hope that it will be useful, but WITHOUT  *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for     *
* more details.                                                                *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* this program; if not, write to the Free Software Foundation, Inc., 675 Mass  *
* Ave, Cambridge, MA 02139, USA.                                               *
*                                                                              *
* The author can be reached at admin@atrinik.org                               *
*******************************************************************************/

/**
 * @file
 * Object parser implementation.
 */

#include <string.h>
#include <fstream>

#include "object_parser.h"

using namespace atrinik;
using namespace boost;
using namespace std;

namespace atrinik {

property_tree::ptree ObjectParser::parse(ifstream& file)
{
    string line;
    property_tree::ptree pt;

    while (getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        if (line == "end") {
            break;
        }

        string key, val;

        if (line == "msg") {
            key = line;

            while (getline(file, line)) {
                if (line == "endmsg") {
                    break;
                }

                val += line + "\n";
            }
        }
        else {
            size_t space = line.find_first_of(' ');

            key = line.substr(0, space);
            val = line.substr(space + 1);
        }

        if (key == "Object" || key == "arch") {
            property_tree::ptree pt2;

            pt2 = parse(file);
            pt2.put<string>(key, val);
            pt.add_child(key, pt2);
        }
        else {
            pt.put<string>(key, val);
        }
    }

    return pt;
}

}
