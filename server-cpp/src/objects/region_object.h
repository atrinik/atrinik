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

#include <memory>
#include <unordered_map>
#include <boost/optional.hpp>

#include <object.h>
#include <bit_flags.h>
#include <game_object.h>
#include <manager.h>

namespace atrinik {

class RegionObject;
typedef std::shared_ptr<RegionObject> RegionObjectPtr;

class RegionObject : public ObjectCRTP<RegionObject>,
public std::enable_shared_from_this<RegionObject> {
public:
    using Object::Object;

    virtual bool load(const std::string& key, const std::string& val);
    virtual std::string dump();

    void inv_push_back(RegionObjectPtr region);

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

    inline const std::string& longname(bool recursive = false) const
    {
        if (recursive) {
            if (!longname_.empty()) {
                return longname_;
            }

            auto env = env_.lock();

            if (env) {
                return env->longname(true);
            }
        }

        return longname_;
    }

    inline void longname(const std::string& val)
    {
        longname_ = val;
    }

    inline const std::string& msg(bool recursive = false) const
    {
        if (recursive) {
            if (!msg_.empty()) {
                return msg_;
            }

            auto env = env_.lock();

            if (env) {
                return env->msg(true);
            }
        }

        return msg_;
    }

    inline void msg(const std::string& val)
    {
        msg_ = val;
    }

    inline const mapcoords_t& jail(bool recursive = false) const
    {
        if (recursive) {
            if (!jail_.path.empty()) {
                return jail_;
            }

            auto env = env_.lock();

            if (env) {
                return env->jail(true);
            }
        }

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
private:

    /**
     * Possible region flags.
     */
    enum Flags {
        MapQuest = 0x01,
    };

    uint8_t flags_; ///< The region's flags/

    std::string name_; ///< Name of the region.

    std::string parent_; ///< Name of the region's parent.

    std::string longname_; ///< Long name of the region.

    std::string msg_; ///< Message of the region.

    mapcoords_t jail_; ///< Map path and X/Y coordinates of the region's jail.

    std::string map_first_; ///< A map that belongs to this region.

    std::string map_bg_; ///< Background color to use for the region map.

    std::weak_ptr<RegionObject> env_; ///< Parent region object.
    std::list<RegionObjectPtr> inv_; ///< Children of the region.
};

class RegionManager : public Manager<RegionManager> {
private:
    typedef std::unordered_map<std::string, RegionObjectPtr>
    RegionsMap;

    RegionsMap regions; ///< Map of the regions.
public:

    static const char* path();
    static void load();
    static void add(RegionObjectPtr region);
    static boost::optional<RegionObjectPtr> get(const std::string& name);
    static RegionsMap::size_type count();
    static void link_parents_children();
    void clear();
};

}
