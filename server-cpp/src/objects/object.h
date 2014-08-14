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
 * Base object.
 */

#pragma once

#include <stdint.h>
#include <string>
#include <atomic>
#include <list>

namespace atrinik {

class Object {
private:
    static std::atomic<uint64_t> guid; ///< Object GUID.
public:
    uint64_t uid; ///< Object's UID.

    Object() : uid(++guid)
    {
    }
    
    virtual ~Object()
    {
    }

    virtual Object* clone() const = 0;

    /**
     * Loads key/value pair into the object's internal structure.
     * @param key Key.
     * @param val Value.
     * @return Whether the value was loaded or not.
     */
    virtual bool load(const std::string& key, const std::string& val) = 0;

    /**
     * Function implementing object-specific dumping.
     * @return String dump.
     */
    virtual std::string dump() = 0;
};

template <typename T>
class ObjectCRTP : public Object {
public:
    using Object::Object;

    virtual Object* clone() const
    {
        return new T(static_cast<const T&> (*this));
    }
};

};
