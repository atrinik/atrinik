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
 * Base object type.
 */

#pragma once

#include <string>

#include "game_object_type.h"

namespace atrinik {

#define GAME_OBJECT_TYPE_ID BaseObjectType
class GAME_OBJECT_TYPE_ID : public GameObjectType {
#include "game_object_type_internal.h"
private:
    std::string name_;
public:
    inline const std::string& name()
    {
        return name_;
    }

    inline void name(const std::string& val) {
        name_ = val;
    }

    virtual bool load(const std::string& key, const std::string& val);
    virtual std::string dump();
};

}
