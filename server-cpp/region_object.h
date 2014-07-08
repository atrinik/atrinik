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
 * Region object.
 */

#pragma once

#include "object.h"
#include "bit_flags.h"
#include "game_object.h"

namespace atrinik {

class RegionObject : public ObjectCRTP<RegionObject> {
private:

    enum Flags {
        MapQuest = 0x01,
    };

    uint8_t flags_;

    std::string name_;

    std::string parent_;

    std::string longname_;

    std::string msg_;

    mapcoords_t jail_;

    std::string map_first_;

    std::string map_bg_;

    RegionObject *env_ = NULL;
    std::list<RegionObject*> inv_;
public:
    using Object::Object;

    typedef std::map<std::string, RegionObject*>
    regions_t; ///< Regions hash map type

    static regions_t regions; ///< The regions hash map

    void inv_push_back(RegionObject* obj);

    inline const bool f_map_quest() const
    {
        return BitFlagQuery(flags_, Flags::MapQuest);
    }

    inline void f_map_quest(bool val)
    {
        BitFlag(flags_, Flags::MapQuest, val);
    }

    inline const std::string& name() const
    {
        return name_;
    }

    inline void name(const std::string& val)
    {
        name_ = val;
    }

    inline const std::string& parent() const
    {
        return parent_;
    }

    inline void parent(const std::string& val)
    {
        parent_ = val;
    }

    inline const std::string& longname() const
    {
        return longname_;
    }

    inline void longname(const std::string& val)
    {
        longname_ = val;
    }

    inline const std::string& msg() const
    {
        return msg_;
    }

    inline void msg(const std::string& val)
    {
        msg_ = val;
    }

    inline const mapcoords_t& jail() const
    {
        return jail_;
    }

    inline void jail(const mapcoords_t& val)
    {
        jail_ = val;
    }

    inline const std::string& map_first() const
    {
        return map_first_;
    }

    inline void map_first(const std::string& val)
    {
        map_first_ = val;
    }

    inline const std::string& map_bg() const
    {
        return map_bg_;
    }

    inline void map_bg(const std::string& val)
    {
        map_bg_ = val;
    }

    virtual bool load(const std::string& key, const std::string& val);
    virtual std::string dump();
};

}
