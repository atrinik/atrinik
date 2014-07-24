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

#include <string>

#if defined(GAME_OBJECT_TYPE_ID)

private:
static GameObjectTypeFactoryRegister<GAME_OBJECT_TYPE_ID> reg;

protected:

static inline const int type_()
{
    return reg.id;
}

static const std::string gettypeid_()
{
#define STR_VALUE(arg) #arg
#define STR_NAME(name) STR_VALUE(name)
    return STR_NAME(GAME_OBJECT_TYPE_ID);
#undef STR_NAME
#undef STR_VALUE
}

public:

virtual const int gettype() const
{
    return type_();
}

virtual const std::string gettypeid() const
{
    return gettypeid_();
}

#undef GAME_OBJECT_TYPE_ID
#else
public:
virtual const int gettype() const = 0;
virtual const std::string gettypeid() const = 0;
#endif
