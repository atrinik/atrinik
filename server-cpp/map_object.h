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
 * Map object.
 */

#pragma once

#include "object.h"

using namespace atrinik;

namespace atrinik {

class MapObject : public Object {
private:
//    int f_is_floor : 1; ///< Whether the object is floor.
protected:
    MapObject(MapObject const& obj) : Object(obj)
    {
//        f_is_floor = obj.f_is_floor;
    }

    virtual string dump_();
public:
    using Object::Object;
    virtual MapObject *clone() const { return new MapObject(*this); }

    virtual bool load(string key, string val);
};

}
