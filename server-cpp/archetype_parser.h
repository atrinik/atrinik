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
 * Archetype parser.
 */

#pragma once

#include "object_parser.h"

namespace atrinik {

class ArchetypeParser : public ObjectParser {
private:
    property_tree::ptree pt; ///< Parsed objects residing in a property tree.
    bool is_more = false; ///< Whether parser is in multi-part archetype.
    bool was_more = false; ///< Whether parser was in multi-part archetype.
public:
    /**
     * Recursively parses archetypes from a file.
     * @param file File.
     * @return Property tree.
     */
    virtual property_tree::ptree parse(ifstream& file);

    /**
     * Read archetypes from the specified file path.
     * @param path File to read from.
     */
    void read_archetypes(string path);

    /**
     * First archetypes loading pass.
     *
     * Walks parsed archetypes and creates base objects, loads parsed attributes
     * into their internal structures and populates the atrinik::archetypes
     * hashmap.
     */
    void load_archetypes_pass1();
};

}
