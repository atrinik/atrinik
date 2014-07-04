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
 * Sign object type.
 */

#pragma once

#include <string>

#include "game_object_type.h"
#include "bit_flags.h"

namespace atrinik {

class SignObjectType : public GameObjectType {
#include "game_object_type_internal.h"
private:
    enum Flags {
        IsFan = 0x01,
    };

    uint8_t flags = 0;
public:
    inline const bool f_is_fan()
    {
        return BitFlagQuery(flags, Flags::IsFan);
    }

    inline void f_is_fan(bool val)
    {
        BitFlag(flags, Flags::IsFan, val);
    }

    virtual bool load(const std::string& key, const std::string& val);
    virtual std::string dump();
};

}
