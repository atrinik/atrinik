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
 * Game object type.
 */

#pragma once

#include <string>
#include <map>

namespace atrinik {

class GameObjectType {
#include <game_object_type_internal.h>
public:
    virtual bool load(const std::string& key, const std::string& val) = 0;
    virtual std::string dump(const GameObjectType* base) = 0;
    virtual GameObjectType* clone() const = 0;
};

template <typename T>
class GameObjectTypeCRTP : public GameObjectType {
public:
    using GameObjectType::GameObjectType;

    virtual GameObjectType* clone() const
    {
        return new T(static_cast<const T&> (*this));
    }
};

template<typename T>
GameObjectType* create_game_object_type()
{
    return new T;
}

struct GameObjectTypeFactory {
    typedef std::map<std::string, GameObjectType*(*)() > MapType;
    static int guid;
    static MapType& map();
    static GameObjectType* create_instance(const std::string& s);
};

template<typename T>
struct GameObjectTypeFactoryRegister : GameObjectTypeFactory {
    int id;

    GameObjectTypeFactoryRegister(const std::string& s)
    {
        map().insert(std::make_pair(s, &create_game_object_type<T>));
        id = guid++;
    }
};

#define REGISTER_GAME_OBJECT_TYPE(_T) \
    GameObjectTypeFactoryRegister<_T> _T::reg(#_T)

}
