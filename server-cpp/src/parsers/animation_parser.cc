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
#include <logger.h>

using namespace atrinik;
using namespace boost;
using namespace std;

namespace atrinik {

void AnimationParser::load(const std::string& path)
{
    BOOST_LOG_FUNCTION();

    AnimationManager::setup();
    LOG(Detail) << "Loading animations from: " << path;
    ifstream file(path);

    if (!file.is_open()) {
        AnimationManager::setup(false);
        throw LOG_EXCEPTION(runtime_error("could not open file"));
    }

    // Add an empty base animation - returned from getters in case the requested
    // animation ID/name cannot be found
    AnimationPtr animation(new Animation("###none"));
    animation->push_back(0);
    AnimationManager::add(animation);

    animation.reset();

    parse(file,
            [&animation] (const std::string & key,
            const std::string & val) mutable -> bool
            {
                if (key.empty() && val == "mina") {
                    AnimationManager::add(animation);
                    animation.reset();
                } else if (key == "anim") {
                    animation.reset(new Animation(val));
                } else if (!animation) {
                    LOG(Error) << "Unrecognized attribute (before animation "
                            "definition): " << key << " " << val;
                    AnimationManager::setup(false);
                    throw LOG_EXCEPTION(runtime_error(
                            "corrupted animations file"));
                } else if (key == "facings") {
                    try {
                        animation->facings(numeric_cast<uint8_t>(
                                lexical_cast<int>(val)));
                    } catch (bad_cast&) {
                        LOG(Error) << "Bad value: " << key << " " << val;
                        AnimationManager::setup(false);
                        throw LOG_EXCEPTION(runtime_error(
                                "corrupted animations file"));
                    }
                } else {
                    animation->push_back(val);
                }

                return true;
            }
    );

    LOG(Detail) << "Loaded " << AnimationManager::count() <<
            " animations";
    AnimationManager::setup(true);
}

}
