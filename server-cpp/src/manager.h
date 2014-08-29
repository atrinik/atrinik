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
 * Base manager.
 */

#pragma once

#include <memory>

namespace atrinik {

template <typename T>
class Manager {
public:
    typedef std::unique_ptr<T> ManagerPtr;

    time_t timestamp = 0;

    static T& primary()
    {
        static ManagerPtr ptr(new T());
        return *ptr;
    }

    static T& secondary()
    {
        static ManagerPtr ptr(new T());
        return *ptr;
    }

    static T& manager(bool get_secondary = true)
    {
        if (use_secondary == get_secondary) {
            return secondary();
        } else {
            return primary();
        }
    }

    static void setup()
    {
        use_secondary = !use_secondary;
    }

    static void setup(bool success)
    {
        if (success) {
            manager(false).clear();
        } else {
            use_secondary = !use_secondary;
        }
    }

private:
    static bool use_secondary;
};

}
