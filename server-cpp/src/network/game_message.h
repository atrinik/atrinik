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

#include <tbb/concurrent_queue.h>

namespace atrinik {

class game_message {
public:

    enum {
        header_length = 2
    };

    game_message() : body_length_(0), body_(header_length)
    {
    }

    game_message(const game_message& msg)
    : body_length_(msg.body_length_), body_(msg.length())
    {
        memcpy(header(), msg.header(), msg.length());
    }

    size_t length() const
    {
        return header_length + body_length_;
    }

    const char* header() const
    {
        return &body_[0];
    }

    char* header()
    {
        return &body_[0];
    }

    const char* body() const
    {
        return &body_[header_length];
    }

    char* body()
    {
        return &body_[header_length];
    }

    size_t body_length() const
    {
        return body_length_;
    }

    bool decode_header()
    {
        body_length_ = (body_[0] << 8) + body_[1];
        body_.reserve(header_length + body_length_);
        return true;
    }

private:
    std::vector<char> body_;
    uint16_t body_length_;
};

typedef tbb::concurrent_queue<game_message> game_message_queue;

}
