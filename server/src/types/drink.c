/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
 *                                                                       *
 * Fork from Crossfire (Multiplayer game for X-windows).                 *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the Free Software           *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
 *                                                                       *
 * The author can be reached at admin@atrinik.org                        *
 ************************************************************************/

/**
 * @file
 * Handles code for @ref DRINK "drink".
 *
 * @author Alex Tokar
 *
 * @todo
 * Since drinks are basically just food objects with different messages,
 * they should really be merged into one type, using sub-types to
 * differentiate them. Merging in flesh object types at the same time might
 * make sense as well.
 */

#include <global.h>
#include <object_methods.h>

/**
 * Initialize the drink type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(drink)
{
    OBJECT_METHODS(DRINK)->fallback = object_methods_get(FOOD);
}
