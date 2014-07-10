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
 * Map tile object.
 */

#pragma once

#include <string>

#include "object.h"

namespace atrinik {

class MapObject;
class GameObject;

typedef uint16_t coord_t;

struct coords_t {
    coord_t x;
    coord_t y;
};

class MapTileObject : public ObjectCRTP<MapTileObject> {
private:
    int x_;

    int y_;

    MapObject *env_ = NULL;
    std::list<GameObject*> inv_;
public:
    using Object::Object;

    int x()
    {
        return x_;
    }

    void x(int val)
    {
        x_ = val;
    }

    int y()
    {
        return y_;
    }

    void y(int val)
    {
        y_ = val;
    }

    MapObject *env()
    {
        return env_;
    }

    void env(MapObject *env)
    {
        env_ = env;
    }

    void inv_push_back(GameObject* obj);

    virtual bool load(const std::string& key, const std::string& val);
    virtual std::string dump();
};

}
