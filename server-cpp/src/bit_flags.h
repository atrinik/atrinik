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
 * Generic bit flags.
 */

#pragma once

#include <string>
#include <boost/lexical_cast.hpp>

namespace atrinik {

template <typename T>
inline void BitFlag(T& flags, uint64_t mask, bool val)
{
    if (val) {
        flags |= mask;
    } else {
        flags &= ~mask;
    }
}

template <typename T>
inline void BitFlag(T& flags, uint64_t mask, std::string val)
{
    BitFlag(flags, mask, boost::lexical_cast<int>(val));
}

template <typename T>
inline bool BitFlagQuery(T& flags, uint64_t mask)
{
    return flags & mask;
}

template <typename T>
inline void BitFlagToggle(T& flags, uint64_t mask)
{
    flags ^= mask;
}

template <typename T>
inline void BitFlagSet(T& flags, uint64_t mask)
{
    flags |= mask;
}

template <typename T>
inline void BitFlagClear(T& flags, uint64_t mask)
{
    flags &= ~mask;
}

}
