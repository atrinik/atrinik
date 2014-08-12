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
 * Animation.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace atrinik {

class Animation {
public:
    typedef std::uint16_t AnimationId;
    typedef std::vector<AnimationId> AnimationFrames;

    Animation(const std::string& name) : name_(name), id_(uid++)
    {
    }
    
    ~Animation()
    {
    }
    
    const std::string& name() const
    {
        return name_;
    }
    
    inline const AnimationId id() const
    {
        return id_;
    }
    
    inline const uint8_t facings() const
    {
        return facings_;
    }
    
    inline void facings(uint8_t facings)
    {
        facings_ = facings;
    }
    
    inline void push_back(AnimationFrames::value_type val)
    {
        frames.push_back(val);
    }
    
    AnimationFrames::value_type operator [](AnimationFrames::size_type i) const
    {
        return frames[i];
    }
    
    AnimationFrames::size_type size() const
    {
        return frames.size();
    }
    
    static bool cmp(Animation* a, Animation* b);
    
private:
    static int uid;
    
    AnimationFrames frames;
    
    AnimationId id_;
    
    uint8_t facings_ = 0;
    
    std::string name_;
};

class AnimationManager {
public:
    typedef std::vector<Animation*> AnimationVector;
    typedef std::unordered_map<std::string, Animation*> AnimationMap;

    AnimationManager();
    ~AnimationManager();
    
    void add(Animation* animation);
    const Animation& get(const std::string& name);
    const Animation& get(Animation::AnimationId id);
private:
    AnimationVector animations_vector;
    AnimationMap animations_map;
};

};
