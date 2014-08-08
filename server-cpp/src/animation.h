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

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

namespace atrinik {

typedef std::vector<uint16_t> AnimationFrames;

class Animation {
private:
    static uint16_t uid;
public:
    Animation() : id(uid++)
    {
    }
    
    ~Animation()
    {
    }
    
    uint16_t id;
    
    uint8_t facings = 0;
    
    AnimationFrames frames;
};

typedef std::vector<Animation*> AnimationVector;
typedef std::unordered_map<std::string, Animation*> AnimationMap;

class AnimationManager {
public:
    AnimationManager();
    ~AnimationManager();
    
    void add(const std::string& name, Animation* animation);
    const Animation& get(const std::string& name);
    const Animation& get(uint16_t id);
private:
    AnimationVector animations_vector;
    AnimationMap animations_map;
};

};
