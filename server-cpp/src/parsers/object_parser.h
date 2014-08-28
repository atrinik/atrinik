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
 * Object parser.
 */

#pragma once

#include <iostream>
#include <functional>
#include <boost/property_tree/ptree.hpp>

#include <game_object.h>

namespace atrinik {

class ObjectParser {
public:
    static void parse(std::ifstream& file, std::function<bool(
            const std::string&, const std::string&) > handler);

    /**
     * Recursively parses objects from the specified file.
     * @param file File to parse.
     * @return Property tree.
     */
    static boost::property_tree::ptree parse(std::ifstream& file,
            std::function<bool(const std::string&) > is_definition = nullptr);

    static GameObjectPtr parse(const boost::property_tree::ptree& tree);

    static void assign_types(const boost::property_tree::ptree& pt,
            GameObjectPtr obj, GameObject::Types type);
};

}
