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
 * Game object.
 */

#pragma once

#include <stdint.h>
#include <atomic>
#include <string.h>
#include <tbb/concurrent_hash_map.h>

#include "object.h"

using namespace std;
using namespace boost;

namespace atrinik {

/**
 * Game object types.
 */
enum GameObjectType {
    Floor = 71, ///< Floor object.
};

class GameObject : public Object {
public:
    using Object::Object;
    explicit GameObject(const string& archname);
    GameObject(GameObject const& obj);
    virtual GameObject *clone() const { return new GameObject(*this); }

    virtual bool load(string key, string val);
    virtual string dump_();

    /**
     * Acquires the game object's archetype name.
     * @return The archetype name.
     */
    string const archname();

    std::atomic<uint8_t> type; ///< Object type.
    std::atomic<uint64_t> value; ///< Object value.

private:
    string archname_; ///< Archetype name.
    uint8_t layer_; ///< Object's layer.
    int f_no_pass : 1; ///< Whether the object is impassable.
    int f_no_pick : 1; ///< Whether the object is unpickable.
};

struct GameObjectHashCmp {
    static size_t hash(const int value)
    {
        return static_cast<int>(value);
    }

    static size_t hash(const string value)
    {
        size_t result = 0;

        for (auto i: value) {
            result = (result * 131) + i;
        }

        return result;
    }

    static bool equal(const int x, const int y)
    {
        return x == y;
    }

    static bool equal(const string x, const string y)
    {
        return x == y;
    }
};

typedef tbb::concurrent_hash_map<uint64_t, GameObject*,
        GameObjectHashCmp> iobjects_t; ///< Game object hash map with UIDs

typedef tbb::concurrent_hash_map<std::string, GameObject*,
        GameObjectHashCmp> sobjects_t; ///< Game object hash map with strings

/**
 * Creates a game object.
 * @param archname Game object's archetype name.
 * @param type Type of the object.
 * @return Created object.
 * @throws atrinik:error on invalid/unimplemented object type.
 */
extern GameObject *create_game_object(const string& archname, int type);

}
