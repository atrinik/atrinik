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
 * Base Atrinik objects implementation.
 */

#include "object.h"
#include "lock.h"

using namespace atrinik;
using namespace boost;
using namespace std;

namespace atrinik {

std::atomic<uint64_t> Object::guid(0);

bool Object::load(string key, string val)
{
    if (key == "name") {
        name_ = val;
        return true;
    }

    return false;
}

string Object::dump()
{
    return dump_() + "end\n";
}

string Object::dump_()
{
    string s;

    s += "name " + name_ + "\n";
    return s;
}

uint64_t const Object::uid()
{
    return uid_;
}

const string& Object::name()
{
    strict_lock<Object> guard(*this);
    return name_;
}

void Object::name(const string& name)
{
    strict_lock<Object> guard(*this);
    name_ = name;
}

}
