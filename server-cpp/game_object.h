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
#include <string.h>
#include <boost/variant.hpp>

#include "object.h"
#include "game_object_type.h"

using namespace std;

namespace atrinik {

class GameObject : public Object, GameObjectType {
private:
    list<GameObjectType*> types;
public:
    boost::variant<GameObject*, std::string> arch;

    using Object::Object;

    virtual bool load(const std::string& key, const std::string& val);
    virtual std::string dump();

    template<class T>
    bool isinstance()
    {
        for (auto it : types) {
            if (it->gettype() == T::type_()) {
                return true;
            }
        }

        return false;
    }

    bool isinstance(const std::string& val)
    {
        for (auto it : types) {
            if (it->gettypeid() == val) {
                return false;
            }
        }

        return false;
    }

    template<class T>
    GameObjectType* getaddinstance()
    {
        GameObjectType* ptr = getinstance<T>();

        if (!ptr) {
            ptr = addinstance<T>();
        }

        return ptr;
    }

    GameObjectType *getaddinstance(const std::string& val)
    {
        GameObjectType* ptr = getinstance(val);

        if (!ptr) {
            ptr = addinstance(val);
        }

        return ptr;
    }

    template<class T>
    GameObjectType* getinstance()
    {
        for (auto it : types) {
            if (it->gettype() == T::type()) {
                return it;
            }
        }

        return NULL;
    }

    GameObjectType* getinstance(const std::string& val)
    {
        for (auto it : types) {
            if (it->gettypeid() == val) {
                return it;
            }
        }

        return NULL;
    }

    template<class T>
    GameObjectType* addinstance()
    {
        GameObjectType* ptr = new T();
        types.push_back(ptr);
        return ptr;
    }

    GameObjectType* addinstance(const std::string val)
    {
        GameObjectType* ptr = GameObjectTypeFactory::create_instance(val);
        types.push_back(ptr);
        return ptr;
    }

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

    typedef map<uint64_t, GameObject*>
    iobjects_t; ///< Game object hash map with UIDs
    typedef map<string, GameObject*>
    sobjects_t; ///< Game object hash map with strings

    static sobjects_t archetypes;
};

class GameObjectArchVisitor : public boost::static_visitor<std::string>
{
public:
    const std::string& operator()(const std::string& s) const
    {
        return s;
    }

    const std::string& operator()(const GameObject* obj) const
    {
        return boost::get<std::string>(obj->arch);
    }
};

}
