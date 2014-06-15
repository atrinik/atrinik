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
 * Game object.
 */

#pragma once

#include <stdint.h>
#include <atomic>
#include <string.h>
#include <tbb/concurrent_hash_map.h>

#include "object.h"

using namespace boost;
using namespace std;

namespace atrinik {

/**
 * Game object types.
 */
enum GameObjectType {
    Floor = 71, ///< Floor object.
};

class GameObject : public Object {
private:
    string name_;
    string archname_; ///< Archetype name.
    uint8_t layer_; ///< Object's layer.
    int f_no_pass : 1; ///< Whether the object is impassable.
    int f_no_pick : 1; ///< Whether the object is unpickable.

    int x_ = 0;
    int y_ = 0;
protected:

    GameObject(GameObject const& obj) : Object(obj)
    {
        name_ = obj.name_;
        archname_ = obj.archname_;
        layer_ = obj.layer_;
        f_no_pass = obj.f_no_pass;
        f_no_pick = obj.f_no_pick;
        arch = obj.arch ? obj.arch : &obj;
    }

    virtual string dump_();
public:
    using Object::Object;
    explicit GameObject(const string& archname);

    virtual GameObject *clone() const
    {
        return new GameObject(*this);
    }

    virtual bool load(string key, string val);

    /**
     * Acquires the game object's archetype name.
     * @return The archetype name.
     */
    string const& archname();

    const int x()
    {
        return x_;
    }

    const int y()
    {
        return y_;
    }

    std::atomic<uint8_t> type; ///< Object type. TODO: RTTI and type() method
    std::atomic<uint64_t> value; ///< Object value.

    const GameObject* arch = NULL;

    struct HashCmp {

        static size_t hash(const int value)
        {
            return static_cast<int> (value);
        }

        static size_t hash(const string value)
        {
            size_t result = 0;

            for (auto i : value) {
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
    HashCmp> iobjects_t; ///< Game object hash map with UIDs

    typedef tbb::concurrent_hash_map<string, GameObject*,
    HashCmp> sobjects_t; ///< Game object hash map with strings

    static GameObject::sobjects_t archetypes;
    static iobjects_t active_objects;
    static mutex active_objects_mutex;
};

/**
 * Creates a game object.
 * @param archname Game object's archetype name.
 * @param type Type of the object.
 * @return Created object.
 * @throws atrinik:error on invalid/unimplemented object type.
 */
extern GameObject *create_game_object(const string& archname, int type);

}
