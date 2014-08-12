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

#include <object.h>
#include <map_tile_object.h>
#include <game_object_type.h>

namespace atrinik {

struct mapcoords_t : coords_t {
    std::string path;
};

class GameObject : public ObjectCRTP<GameObject> {
private:
    std::list<GameObjectType*> types;

    boost::variant<boost::blank, MapTileObject*, GameObject*> env_;

    std::list<GameObject*> inv_;
public:
    boost::variant<GameObject*, std::string> arch;

    GameObject() : ObjectCRTP()
    {
    }

    ~GameObject()
    {
    }

    GameObject(const GameObject& obj)
    {
        arch = obj.arch;

        for (auto it : obj.types) {
            types.push_back(it->clone());
        }
    }

    void env(boost::variant<GameObject*, MapTileObject*> env)
    {
        env_ = env;
    }

    void inv_push_back(GameObject* obj)
    {
        inv_.push_back(obj);
        obj->env_ = this;
    }

    MapTileObject* map_tile()
    {
        try {
            return boost::get<MapTileObject*>(env_);
        } catch (boost::bad_get) {
            return NULL;
        }
    }

    MapObject* map()
    {
        MapTileObject* tile = map_tile();

        if (tile == NULL) {
            return NULL;
        }

        return tile->env();
    }

    virtual bool load(const std::string& key, const std::string& val);
    virtual std::string dump();

    template<class T>
    bool isinstance() const
    {
        for (auto it : types) {
            if (it->gettype() == T::type_()) {
                return true;
            }
        }

        return false;
    }

    bool isinstance(const std::string& val) const
    {
        for (auto it : types) {
            if (it->gettypeid() == val) {
                return false;
            }
        }

        return false;
    }

    template<class T>
    T* getaddinstance()
    {
        T* ptr = getinstance<T>();

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
    T* getinstance() const
    {
        for (auto it : types) {
            if (it->gettype() == T::type_()) {
                return dynamic_cast<T*>(it);
            }
        }

        return NULL;
    }

    GameObjectType* getinstance(const std::string& val) const
    {
        for (auto it : types) {
            if (it->gettypeid() == val) {
                return it;
            }
        }

        return NULL;
    }

    GameObjectType* getinstance(const int val) const
    {
        for (auto it : types) {
            if (it->gettype() == val) {
                return it;
            }
        }

        return NULL;
    }

    template<class T>
    T* addinstance()
    {
        T* ptr = new T();
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

        static size_t hash(const std::string value)
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

        static bool equal(const std::string x, const std::string y)
        {
            return x == y;
        }
    };

    typedef std::map<uint64_t, GameObject*>
    iobjects_t; ///< Game object hash map with UIDs
    typedef std::map<std::string, GameObject*>
    sobjects_t; ///< Game object hash map with strings

    static sobjects_t archetypes;

    static const GameObject* find_archetype(const std::string& archname)
    {
        GameObject::sobjects_t::iterator result;

        result = GameObject::archetypes.find(archname);

        if (result == GameObject::archetypes.end()) {
            return NULL;
        }

        return result->second;
    }
};

class GameObjectArchVisitor : public boost::static_visitor<std::string> {
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
