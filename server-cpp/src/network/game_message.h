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
 * Atrinik protocol message.
 */

#pragma once

#include <vector>
#include <algorithm>
#include <tbb/concurrent_queue.h>

namespace atrinik {

class GameMessage {
public:

    enum {
        header_length = 2
    };

    typedef std::vector<char> data_t;

    GameMessage() : body_length_(0), body_(header_length), idx_(0)
    {
    }

    GameMessage(const GameMessage& msg)
    : body_length_(msg.body_length_), body_(msg.length()), idx_(0)
    {
        memcpy(header(), msg.header(), msg.length());
    }

    const inline bool end() const
    {
        return idx_ >= body_length();
    }

    const inline bool empty() const
    {
        return body_length() == 0;
    }

    size_t length() const
    {
        return header_length + body_length_;
    }

    const char* header() const
    {
        return body_.data();
    }

    char* header()
    {
        return body_.data();
    }

    const char* body() const
    {
        return body_.data() + header_length;
    }

    char* body()
    {
        return body_.data() + header_length;
    }

    size_t body_length() const
    {
        return body_length_;
    }

    void encode_header()
    {
        if (body_length_ > 32 * 1024 - 1) {
            body_[0] = ((body_length_ >> 16) & 0xff) | 0x80;
            body_[1] = (body_length_ >> 8) & 0xff;
            body_.insert(body_.begin() + 2, body_length_ & 0xff);
        } else {
            body_[0] = (body_length_ >> 8) & 0xff;
            body_[1] = body_length_ & 0xff;
        }
    }

    bool decode_header()
    {
        body_length_ = (body_[0] << 8) + body_[1];

        if (body_length_ == 0) {
            return 0;
        }

        body_.reserve(header_length + body_length_);
        return true;
    }

    int8_t int8() const
    {
        if (body_length() - idx_ < 1) {
            return 0;
        }

        return body()[idx_++];
    }

    void int8(int8_t val)
    {
        body_length_ += 1;
        body_.reserve(header_length + body_length_);
        body_.push_back(val & 0xff);
    }

    int16_t int16() const
    {
        if (body_length() - idx_ < 2) {
            return 0;
        }

        return (body()[idx_++] << 8) + body()[idx_++];
    }

    void int16(int16_t val)
    {
        body_length_ += 2;
        body_.reserve(header_length + body_length_);
        body_.push_back((val >> 8) & 0xff);
        body_.push_back(val & 0xff);
    }

    int32_t int32() const
    {
        if (body_length() - idx_ < 4) {
            return 0;
        }

        return (body()[idx_++] << 24) + (body()[idx_++] << 16) +
                (body()[idx_++] << 8) + body()[idx_++];
    }

    void int32(int32_t val)
    {
        body_length_ += 4;
        body_.reserve(header_length + body_length_);
        body_.push_back((val >> 24) & 0xff);
        body_.push_back((val >> 16) & 0xff);
        body_.push_back((val >> 8) & 0xff);
        body_.push_back(val & 0xff);
    }

    int64_t int64() const
    {
        if (body_length() - idx_ < 8) {
            return 0;
        }

        return ((int64_t) body()[idx_++] << 56) +
                ((int64_t) body()[idx_++] << 48) +
                ((int64_t) body()[idx_++] << 40) +
                ((int64_t) body()[idx_++] << 32) +
                (body()[idx_++] << 24) + (body()[idx_++] << 16) +
                (body()[idx_++] << 8) + body()[idx_++];
    }

    void int64(int64_t val)
    {
        body_length_ += 8;
        body_.reserve(header_length + body_length_);
        body_.push_back((val >> 56) & 0xff);
        body_.push_back((val >> 48) & 0xff);
        body_.push_back((val >> 40) & 0xff);
        body_.push_back((val >> 32) & 0xff);
        body_.push_back((val >> 24) & 0xff);
        body_.push_back((val >> 16) & 0xff);
        body_.push_back((val >> 8) & 0xff);
        body_.push_back(val & 0xff);
    }

    std::string string() const
    {
        std::string s;

        if (body_length() - idx_ < 1) {
            return s;
        }

        data_t::const_iterator it = std::find(body_.begin() + header_length +
                idx_, body_.end(), 0);

        if (it != body_.end()) {
            size_t len = it - (body_.begin() + header_length + idx_);
            s.assign(body() + idx_, len);
            idx_ += len + 1;
        }

        return s;
    }

    void string(const std::string& val, bool terminated = true)
    {
        body_length_ += val.length() + terminated;
        body_.reserve(header_length + body_length_);
        std::copy(val.begin(), val.end(), std::back_inserter(body_));

        if (terminated) {
            body_.push_back(0);
        }
    }

private:
    data_t body_;
    size_t body_length_;
    mutable size_t idx_;
};

typedef tbb::concurrent_queue<GameMessage> GameMessageQueue;

namespace ServerCommands {
    enum ServerCommands {
        Control,
        AskFace,
        Setup,
        Version,
        RequestFile, ///< @deprecated
        Clear,
        RequestUpdate,
        Keepalive,
        Account,
        ItemExamine,
        ItemApply,
        ItemMove,
        Reply, ///< @deprecated
        PlayerCmd,
        ItemLock,
        ItemMark,
        Fire,
        Quickslot,
        QuestList,
        MovePath,
        ItemReady, ///< @deprecated
        Talk,
        Move,
        Target,

        Nrof
    };

    enum class SetupCommand {
        Sound,
        MapSize,
        Bot,
        DataURL,
    };

    enum class TargetCommand {
        MapXY = 1,
        Clear,
    };

    enum class AccountCommand {
        Login = 1,
        Register,
        LoginChar,
        NewChar,
        Pswd,
    };

    enum class TalkCommand {
        NPC = 1,
        Inv,
        Below,
        Container,
        NpcName,
    };

    enum class ControlCommand {
        Map = 1,
        Player,
    };

    enum class ControlCommandMap {
        Reset = 1,
    };

    enum class ControlCommandPlayer {
        Teleport = 1,
    };
};

namespace ClientCommands {
    enum ClientCommands {
        Map,
        DrawInfo,
        FileUpdate,
        Item,
        Sound,
        Target,
        ItemUpdate,
        ItemDelete,
        Stats,
        Image,
        Anim,
        SkillReady, ///< @deprecated
        Player,
        MapStats,
        SkillList, ///< @deprecated
        Version,
        Setup,
        Control,
        Data, ///< @deprecated
        Characters,
        Book,
        Party,
        Quickslot,
        Compressed,
        RegionMap,
        SoundAmbient,
        Interface,
        Notification,

        Nrof
    };

    enum class MapCommand {
        Same,
        New,
        Connected,
    };

    enum class TargetCommand {
        Self,
        Enemy,
        Friend,
    };

    enum class InterfaceCommand {
        Text,
        Link,
        Icon,
        Title,
        Input,
    };

    enum class NotificationCommand {
        Text,
        Action,
        Shortcut,
        Delay,
    };

    enum class MapCommandFlags {
        Multi = 0x01,
        Name = 0x02,
        Probe = 0x04,
        Height = 0x08,
        Zoom = 0x10,
        Align = 0x20,
        Double = 0x40,
        More = 0x80,
    };

    enum class MapCommandFlags2 {
        Alpha = 0x01,
        Rotate = 0x02,
        Infravision = 0x04,
        Target = 0x08,
    };

    enum class MapCommandFlagsExt {
        Anim = 0x01,
    };

    enum class MapCommandAnim {
        Damage = 1,
        Kill,
    };

    enum class MapCommandMask {
        Clear = 0x2,
        Darkness = 0x4,
    };

    enum class MapCommandLayer {
        Clear = 255,
    };

    enum class MapStatsCommand {
        Name = 1,
        Music,
        Weather,
        TextAnim,
    };

    enum class SoundCommand {
        Effect = 1,
        Background,
        Absolute,
    };

    enum class DrawInfoCommand {
        All = 1,
        Game,
        Chat,
        Local,
        Private,
        Guild,
        Party,
        Operator
    };

    class DrawInfoCommandColors {
    public:
        static const std::string white;
        static const std::string orange;
        static const std::string navy;
        static const std::string red;
        static const std::string green;
        static const std::string blue;
        static const std::string gray;
        static const std::string brown;
        static const std::string purple;
        static const std::string pink;
        static const std::string yellow;
        static const std::string dark_navy;
        static const std::string dark_green;
        static const std::string dark_orange;
        static const std::string light_purple;
        static const std::string light_gold;
        static const std::string dark_gold;
        static const std::string black;
    };

    enum class ItemCommandFlags {
        Location = 0x01,
        Flags = 0x02,
        Weight = 0x04,
        Face = 0x08,
        Name = 0x10,
        Anim = 0x20,
        AnimSpeed = 0x40,
        Nrof = 0x80,
        Direction = 0x0100,
        Type = 0x0200,
        Extra = 0x0400,
    };

    enum class ItemCommandObjectFlags {
        Applied = 0x01,
        Unpaid = 0x02,
        IsMagical = 0x04,
        Cursed = 0x08,
        Damned = 0x10,
        ContainerOpen = 0x20,
        Locked = 0x40,
        Trapped = 0x80,
        Weapon2H = 0x0100,
    };
};

};
