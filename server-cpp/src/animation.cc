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

#include <algorithm>

#include <animation.h>

using namespace atrinik;
using namespace std;

namespace atrinik {

bool Animation::cmp(Animation* a, Animation* b)
{
    return a->name() < b->name();
}

AnimationManager::AnimationManager()
{
    // Add an empty base animation - returned from getters in case the requested
    // animation ID/name cannot be found
    auto animation = new Animation("###none");
    animation->push_back(0);
    animation->facings(1);
    add(animation);
}

AnimationManager::~AnimationManager()
{
}

void AnimationManager::sort()
{
    std::sort(animations_vector.begin(), animations_vector.end(),
            Animation::cmp);
    
    Animation::AnimationId id = 0;
    
    for (auto animation : animations_vector) {
        animation->id(id++);
    }
}

void AnimationManager::add(Animation* animation)
{
    if (animation->facings() == 0 || (animation->facings() != 9 &&
            animation->facings() != 25)) {
        // TODO: log notice; invalid number of facings
        animation->facings(1);
    }
    
    if ((animation->size() % animation->facings()) != 0) {
        // TODO: log notice; number of frames is not an exact multiple of number
        // of facings
    }
    
    animations_vector.push_back(animation);
            
    if (!animations_map.insert(make_pair(animation->name(),
            animation)).second) {
        throw runtime_error("could not insert animation");
    }
}

const Animation& AnimationManager::get(const std::string& name)
{
    AnimationMap::const_iterator it = animations_map.find(name);
    
    if (it == animations_map.end()) { // Doesn't exist, return default
        return *animations_vector[0];
    }
    
    return *it->second;
}

const Animation& AnimationManager::get(Animation::AnimationId id)
{
    try {
        return *animations_vector.at(id);
    } catch (out_of_range&) { // Animation ID doesn't exist, return default
        return *animations_vector[0];
    }
}

};
