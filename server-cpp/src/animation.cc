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
 * Animation implementation.
 */

#include <boost/filesystem.hpp>

#include <animation.h>
#include <animation_parser.h>
#include <logger.h>

using namespace atrinik;
using namespace boost;
using namespace std;

namespace atrinik {

int Animation::uid(0);

template<> bool Manager<AnimationManager>::use_secondary = true;

const char* AnimationManager::path()
{
    return "../arch/animations";
}

void AnimationManager::load()
{
    BOOST_LOG_FUNCTION();

    filesystem::path p(path());
    time_t t = filesystem::last_write_time(p);

    if (t > manager().timestamp) {
        Animation::uid = 0;
        AnimationParser::load(path());
        manager().timestamp = t;
    }
}

void AnimationManager::add(AnimationPtr animation)
{
    BOOST_LOG_FUNCTION();

    if (animation->facings() == 0 || (animation->facings() != 1 &&
            animation->facings() != 9 && animation->facings() != 25)) {
        LOG(Error) << "Animation " << animation->name() <<
                " has invalid number of facings (" << animation->facings() <<
                "), setting to 1";
        animation->facings(1);
    }

    if ((animation->size() % animation->facings()) != 0) {
        LOG(Error) << "Number of frames (" << animation->size() <<
                ") in animation " << animation->name() <<
                " is not an exact multiple of number of facings (" <<
                animation->facings() << ")";
    }

    manager().animations_vector.push_back(animation);

    if (!manager().animations_map.insert(make_pair(animation->name(),
            animation)).second) {
        throw LOG_EXCEPTION(runtime_error("could not insert animation"));
    }
}

AnimationPtrConst AnimationManager::get(const std::string& name)
{
    const auto& it = manager().animations_map.find(name);

    if (it == manager().animations_map.end()) {
        // Doesn't exist, return default
        return manager().animations_vector[0];
    }

    return it->second;
}

AnimationPtrConst AnimationManager::get(Animation::AnimationId id)
{
    try {
        return manager().animations_vector.at(id);
    } catch (out_of_range&) { // Animation ID doesn't exist, return default
        return manager().animations_vector[0];
    }
}

AnimationManager::AnimationVector::size_type AnimationManager::count()
{
    return manager().animations_vector.size();
}

void AnimationManager::clear()
{
    animations_vector.clear();
    animations_map.clear();
}

};
