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
 * Game message implementation.
 */

#include <game_message.h>

using namespace atrinik;
using namespace std;

namespace atrinik {

void GameMessage::encode_header()
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

bool GameMessage::decode_header()
{
    body_length_ = (body_[0] << 8) + body_[1];

    if (body_length_ == 0) {
        return 0;
    }

    body_.reserve(header_length + body_length_);
    return true;
}

int8_t GameMessage::int8() const
{
    if (body_length() - idx_ < 1) {
        return 0;
    }

    return body()[idx_++];
}

void GameMessage::int8(int8_t val)
{
    body_length_ += 1;
    body_.reserve(header_length + body_length_);
    body_.push_back(val & 0xff);
}

int16_t GameMessage::int16() const
{
    if (body_length() - idx_ < 2) {
        return 0;
    }

    return (body()[idx_++] << 8) + body()[idx_++];
}

void GameMessage::int16(int16_t val)
{
    body_length_ += 2;
    body_.reserve(header_length + body_length_);
    body_.push_back((val >> 8) & 0xff);
    body_.push_back(val & 0xff);
}

int32_t GameMessage::int32() const
{
    if (body_length() - idx_ < 4) {
        return 0;
    }

    return (body()[idx_++] << 24) + (body()[idx_++] << 16) +
            (body()[idx_++] << 8) + body()[idx_++];
}

void GameMessage::int32(int32_t val)
{
    body_length_ += 4;
    body_.reserve(header_length + body_length_);
    body_.push_back((val >> 24) & 0xff);
    body_.push_back((val >> 16) & 0xff);
    body_.push_back((val >> 8) & 0xff);
    body_.push_back(val & 0xff);
}

int64_t GameMessage::int64() const
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

void GameMessage::int64(int64_t val)
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

std::string GameMessage::string() const
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

void GameMessage::string(const std::string& val, bool terminated)
{
    body_length_ += val.length() + terminated;
    body_.reserve(header_length + body_length_);
    std::copy(val.begin(), val.end(), std::back_inserter(body_));

    if (terminated) {
        body_.push_back(0);
    }
}

using namespace ClientCommands;

const std::string DrawInfoCommandColors::white("ffffff");
const std::string DrawInfoCommandColors::orange("ff9900");
const std::string DrawInfoCommandColors::navy("00ffff");
const std::string DrawInfoCommandColors::red("ff3030");
const std::string DrawInfoCommandColors::green("00ff00");
const std::string DrawInfoCommandColors::blue("0080ff");
const std::string DrawInfoCommandColors::gray("999999");
const std::string DrawInfoCommandColors::brown("c07f40");
const std::string DrawInfoCommandColors::purple("cc66ff");
const std::string DrawInfoCommandColors::pink("ff9999");
const std::string DrawInfoCommandColors::yellow("ffff33");
const std::string DrawInfoCommandColors::dark_navy("00c4c2");
const std::string DrawInfoCommandColors::dark_green("006600");
const std::string DrawInfoCommandColors::dark_orange("ff6600");
const std::string DrawInfoCommandColors::light_purple("ff66ff");
const std::string DrawInfoCommandColors::light_gold("d4d553");
const std::string DrawInfoCommandColors::dark_gold("999900");
const std::string DrawInfoCommandColors::black("000000");

};
