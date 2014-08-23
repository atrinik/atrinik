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
 * Map object implementation.
 */

#include <fstream>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <map_object.h>
#include <map_tile_object.h>
#include <game_object.h>
#include <logger.h>
#include <map_parser.h>

using namespace atrinik;
using namespace boost;
using namespace std;

namespace atrinik {

bool MapObject::load(const std::string& key, const std::string& val)
{
    BOOST_LOG_FUNCTION();

    if (key == "name") {
        name_ = val;
        return true;
    } else if (key == "bg_music") {
        bg_music_ = val;
        return true;
    } else if (key == "weather") {
        weather_ = val;
        return true;
    } else if (key == "region") {
        region_ = val;
        return true;
    } else if (key == "msg") {
        message_ = val;
        return true;
    } else if (key == "width") {
        size_.first = lexical_cast<int>(val);
        return true;
    } else if (key == "height") {
        size_.second = lexical_cast<int>(val);
        return true;
    } else if (key == "enter_x") {
        enter_pos_.first = lexical_cast<int>(val);
        return true;
    } else if (key == "enter_y") {
        enter_pos_.second = lexical_cast<int>(val);
        return true;
    } else if (key == "difficulty") {
        difficulty = lexical_cast<int>(val);
        return true;
    } else if (key == "darkness") {
        darkness = lexical_cast<int>(val);
        return true;
    } else if (key == "no_magic") {
        f_no_magic(true);
        return true;
    } else if (key == "no_harm") {
        f_no_harm(true);
        return true;
    } else if (key == "no_summon") {
        f_no_summon(true);
        return true;
    } else if (key == "no_player_save") {
        f_no_player_save(true);
        return true;
    } else if (key == "no_save") {
        f_no_save(true);
        return true;
    } else if (key == "outdoor") {
        f_outdoor(true);
        return true;
    } else if (key == "unique") {
        f_unique(true);
        return true;
    } else if (key == "fixed_resettime") {
        f_fixed_reset_time(true);
        return true;
    } else if (key == "fixed_login") {
        f_fixed_login(true);
        return true;
    } else if (key == "pvp") {
        f_pvp(true);
        return true;
    } else if (starts_with(key, "tile_path_")) {
        int id = lexical_cast<int>(key.substr(10)) - 1;

        if (id < 0 || id >= NumTiledMaps) {
            LOG(Error) << "tile_path_" << (id + 1) << " is not in valid range";
        } else {
            tile_path_[id] = val;
        }

        return true;
    }

    return false;
}

std::string MapObject::dump()
{
    string s = "arch map\n";

    if (!name_.empty()) {
        s += "name " + name_ + "\n";
    }

    if (!bg_music_.empty()) {
        s += "bg_music " + bg_music_ + "\n";
    }

    if (!weather_.empty()) {
        s += "weather " + weather_ + "\n";
    }

    if (!region_.empty()) {
        s += "region " + region_ + "\n";
    }

    if (!message_.empty()) {
        s += "msg\n";
        s += message_ + "\n";
        s += "endmsg\n";
    }

    s += "width " + lexical_cast<string>(size_.first) + "\n";
    s += "height " + lexical_cast<string>(size_.second) + "\n";

    if (enter_pos_.first) {
        s += "enter_x " + lexical_cast<string>(enter_pos_.first) + "\n";
    }

    if (enter_pos_.second) {
        s += "enter_y " + lexical_cast<string>(enter_pos_.second) + "\n";
    }

    if (difficulty) {
        s += "difficulty " + lexical_cast<string>(difficulty) + "\n";
    }

    if (darkness) {
        s += "darkness " + lexical_cast<string>(darkness) + "\n";
    }

    if (f_no_magic()) {
        s += "no_magic 1\n";
    }

    if (f_no_harm()) {
        s += "no_harm 1\n";
    }

    if (f_no_summon()) {
        s += "no_summon 1\n";
    }

    if (f_no_player_save()) {
        s += "no_player_save 1\n";
    }

    if (f_no_save()) {
        s += "no_save 1\n";
    }

    if (f_outdoor()) {
        s += "outdoor 1\n";
    }

    if (f_unique()) {
        s += "unique 1\n";
    }

    if (f_fixed_reset_time()) {
        s += "fixed_resettime 1\n";
    }

    if (f_fixed_login()) {
        s += "fixed_login 1\n";
    }

    if (f_pvp()) {
        s += "pvp 1\n";
    }

    for (int i = 0; i < 8; i++) {
        if (!tile_path_[i].empty()) {
            s += "tile_path_";
            s += lexical_cast<string>(i + 1);
            s += " ";
            s += tile_path_[i];
            s += "\n";
        }
    }

    s += "end\n";

    for (int x = 0; x < size_.first; x++) {
        for (int y = 0; y < size_.second; y++) {
            s += tile_get(x, y).dump();
        }
    }

    return s;
}

const string& MapObject::path()
{
    return path_;
}

void MapObject::allocate()
{
    inv.resize(size_.first * size_.second); // Reserve map tiles

    int len = size_.first * size_.second;

    for (int i = 0; i < len; i++) {
        inv[i].x(i / size_.first);
        inv[i].y(i % size_.second);
        inv[i].env(this);
    }
}

MapObject* MapObject::load_map(const std::string& path)
{
    // TODO: loaded check
    // TODO: load from binary if it exists

    ifstream file(path);

    if (!file.is_open()) {
        return nullptr;
    }

    MapObject* map = new MapObject(path);
    MapParser::load(file, map);

    return map;
}

}
