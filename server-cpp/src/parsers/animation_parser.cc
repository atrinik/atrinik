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
 * Animation parser implementation.
 */

#include <string.h>
#include <fstream>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/numeric/conversion/cast.hpp>

#include <animation_parser.h>
#include <animation.h>
#include <server.h>

using namespace atrinik;
using namespace boost;
using namespace std;

namespace atrinik {

void AnimationParser::load_animations(const std::string& path)
{
    ifstream file(path);
    
    if (!file) {
        throw runtime_error("could not open file");
    }
    
    string line, name;
    Animation* animation = NULL;
    
    while (getline(file, line)) {
        if (line.empty() || starts_with(line, "#")) {
            continue;
        }

        size_t space = line.find_first_of(' ');
        
        if (space == string::npos) {
            // TODO: error
            continue;
        }
        
        string key = line.substr(0, space);
        string val = line.substr(space + 1);

        if (key == "anim") {
            animation = new Animation();
            name = val;
        } else if (!animation) {
            // TODO: error
        } else if (key == "facings") {
            try {
                animation->facings = numeric_cast<uint8_t>(
                        lexical_cast<int>(val));
            } catch (bad_cast&) {
                // TODO: error
            }

            while (getline(file, line)) {
                if (line == "mina") {
                    Server::server.animation.add(name, animation);
                    animation = NULL;
                    break;
                }

                try {
                    animation->frames.push_back(lexical_cast<uint16_t>(line));
                } catch (bad_cast&) {
                    // TODO: error
                }
            }
        }
    }
}

}
