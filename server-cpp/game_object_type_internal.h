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
 * Internal definitions for a game object type.
 */

#include <boost/preprocessor/slot/counter.hpp>
#include <string>

#if defined(GAME_OBJECT_TYPE_ID)
private:
    static GameObjectTypeFactoryRegister<GAME_OBJECT_TYPE_ID> reg;
#endif

protected:
    static inline const int type_()
    {
        return BOOST_PP_COUNTER;
#include BOOST_PP_UPDATE_COUNTER()
    }

    static const std::string gettypeid_()
    {
#if defined(GAME_OBJECT_TYPE_ID)
#define STR_VALUE(arg) #arg
#define STR_NAME(name) STR_VALUE(name)
#define GAME_OBJECT_TYPE_ID_STR STR_NAME(GAME_OBJECT_TYPE_ID)
        return GAME_OBJECT_TYPE_ID_STR;
#undef STR_NAME
#undef STR_VALUE
#else
        return "";
#endif
    }

public:
    virtual const int gettype()
    {
        return type_();
    }

    virtual const std::string gettypeid()
    {
        return gettypeid_();
    }

#undef GAME_OBJECT_TYPE_ID
