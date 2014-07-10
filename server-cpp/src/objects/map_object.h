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
 * Map object.
 */

#pragma once

#include <object.h>
#include <bit_flags.h>

namespace atrinik {

class MapTileObject;

class MapObject : public ObjectCRTP<MapObject> {
private:

    enum Flags {
        NoMagic = 0x01,
        NoHarm = 0x02,
        NoSummon = 0x04,
        NoPlayerSave = 0x08,
        NoSave = 0x10,
        Outdoor = 0x20,
        Unique = 0x40,
        FixedResetTime = 0x80,
        FixedLogin = 0x0100,
        Pvp = 0x0200,
    };

    uint32_t map_flags_;

    std::string name_;

    std::string path_;

    std::string bg_music_;

    std::string weather_;

    std::string region_;

    std::string message_;

    MapObject *tile_map_[8];

    std::string tile_path_[8];

    std::pair<int, int> enter_pos_;

    std::pair<int, int> size_;
public:

    int reset_timeout;

    int swap_time;

    int difficulty;

    int darkness;

    int light;

    std::vector<MapTileObject> inv;

    using Object::Object;

    explicit MapObject(const std::string& path) : ObjectCRTP(), path_(path),
    map_flags_(0)
    {
    }

    const std::string& path();
    void allocate();

    inline MapTileObject& tile_get(int x, int y)
    {
        return inv[x * size_.second + y];
    }

    inline const bool f_no_magic()
    {
        return BitFlagQuery(map_flags_, Flags::NoMagic);
    }

    inline void f_no_magic(bool val)
    {
        BitFlag(map_flags_, Flags::NoMagic, val);
    }

    inline const bool f_no_harm()
    {
        return BitFlagQuery(map_flags_, Flags::NoHarm);
    }

    inline void f_no_harm(bool val)
    {
        BitFlag(map_flags_, Flags::NoHarm, val);
    }

    inline const bool f_no_summon()
    {
        return BitFlagQuery(map_flags_, Flags::NoSummon);
    }

    inline void f_no_summon(bool val)
    {
        BitFlag(map_flags_, Flags::NoSummon, val);
    }

    inline const bool f_no_player_save()
    {
        return BitFlagQuery(map_flags_, Flags::NoPlayerSave);
    }

    inline void f_no_player_save(bool val)
    {
        BitFlag(map_flags_, Flags::NoPlayerSave, val);
    }

    inline const bool f_no_save()
    {
        return BitFlagQuery(map_flags_, Flags::NoSave);
    }

    inline void f_no_save(bool val)
    {
        BitFlag(map_flags_, Flags::NoSave, val);
    }

    inline const bool f_outdoor()
    {
        return BitFlagQuery(map_flags_, Flags::Outdoor);
    }

    inline void f_outdoor(bool val)
    {
        BitFlag(map_flags_, Flags::Outdoor, val);
    }

    inline const bool f_unique()
    {
        return BitFlagQuery(map_flags_, Flags::Unique);
    }

    inline void f_unique(bool val)
    {
        BitFlag(map_flags_, Flags::Unique, val);
    }

    inline const bool f_fixed_reset_time()
    {
        return BitFlagQuery(map_flags_, Flags::FixedResetTime);
    }

    inline void f_fixed_reset_time(bool val)
    {
        BitFlag(map_flags_, Flags::FixedResetTime, val);
    }

    inline const bool f_fixed_login()
    {
        return BitFlagQuery(map_flags_, Flags::FixedLogin);
    }

    inline void f_fixed_login(bool val)
    {
        BitFlag(map_flags_, Flags::FixedLogin, val);
    }

    inline const bool f_pvp()
    {
        return BitFlagQuery(map_flags_, Flags::Pvp);
    }

    inline void f_pvp(bool val)
    {
        BitFlag(map_flags_, Flags::Pvp, val);
    }

    virtual bool load(const std::string& key, const std::string& val);
    virtual std::string dump();
};

}
