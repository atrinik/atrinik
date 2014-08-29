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
 * Face.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

#include <manager.h>

namespace atrinik {

class Face {
public:
    typedef std::uint16_t FaceId;
    typedef std::pair<char*, size_t> FaceData;

    static int uid;

    Face(const std::string& name) : name_(name), id_(uid++)
    {
    }

    ~Face()
    {
    }

    const std::string& name() const
    {
        return name_;
    }

    inline const FaceId id() const
    {
        return id_;
    }

    inline const FaceData data() const
    {
        return data_;
    }

    inline void data(FaceData data)
    {
        data_ = data;
    }

private:
    FaceId id_;

    std::string name_;

    FaceData data_;
};

typedef std::shared_ptr<Face> FacePtr;
typedef std::shared_ptr<const Face> FacePtrConst;

class FaceManager : public Manager<FaceManager> {
public:
    typedef std::vector<FacePtr> FaceVector;
    typedef std::unordered_map<std::string, FacePtr> FaceMap;

    static const char* path();
    static void load();
    static void add(FacePtr face);
    static FacePtrConst get(const std::string& name);
    static FacePtrConst get(Face::FaceId id);
    static FaceVector::size_type count();
    void clear();
private:
    FaceVector faces_vector;
    FaceMap faces_map;
};

};
