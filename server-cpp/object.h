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
 * Base object.
 */

#pragma once

#include <stdint.h>
#include <string>
#include <atomic>
#include <boost/thread.hpp>
#include <boost/thread/lockable_adapter.hpp>

using namespace boost;
using namespace std;

namespace atrinik {

class Object
: public basic_lockable_adapter<mutex> {
private:
    static std::atomic<uint64_t> guid; ///< Object GUID.

    string name_; ///< Object's name.

    uint64_t uid_; ///< Object's UID.
protected:
    Object(const Object& obj)
    {
        boost::lock_guard<boost::mutex> _lock(obj.lockable());
        name_ = obj.name_;
    }

    /**
     * Function implementing object-specific dumping.
     * @return String dump.
     */
    virtual string dump_();

    virtual void clone(const Object& obj)
    {
        name_ = obj.name_;
    }
public:
    Object() : uid_(++guid) {}
    virtual ~Object() {}
    virtual Object *clone() const = 0;

    /**
     * Loads key/value pair into the object's internal structure.
     * @param key Key.
     * @param val Value.
     * @return true if the key/value was loaded, false otherwise.
     */
    virtual bool load(string key, string val);

    /**
     * Dumps the object contents as a string.
     * @return String dump.
     */
    string dump();

    /**
     * Acquires the object's UID.
     * @return UID.
     */
    uint64_t const uid();

    /**
     * Acquires the object's name.
     * @return Name.
     */
    const string& name();

    /**
     * Sets the object's name.
     * @param name Name.
     */
    void name(const string& name);
};

};
