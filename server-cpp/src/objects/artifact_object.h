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
 * Artifact object.
 */

#pragma once

#include <stdint.h>
#include <string.h>
#include <unordered_map>
#include <vector>
#include <boost/optional.hpp>

#include <object.h>
#include <game_object.h>
#include <manager.h>

namespace atrinik {

class ArtifactObject : public ObjectCRTPShared<ArtifactObject> {
public:

    ArtifactObject() : ObjectCRTPShared()
    {
    }

    ~ArtifactObject()
    {
    }

    const std::string& name() const
    {
        return boost::get<std::string>(artifact->arch);
    }

    GameObjectPtrConst obj() const
    {
        return artifact;
    }

    bool load_object(const std::string& key, const std::string& val);

    virtual bool load(const std::string& key, const std::string& val);
    virtual std::string dump();
private:
    /**
     * Vector of attributes used to load the artifact-specific attributes into
     * the object.
     *  */
    std::vector<std::pair<std::string, std::string> > attributes;

    GameObjectPtrConst arch; ///< Default archetype of the artifact.

    GameObjectPtr artifact; ///< The artifact.

    std::vector<std::string> allowed; ///< Allowed archetypes.

    int t_style = 0; ///< Treasure style.

    std::uint16_t chance = 0; ///< Chance.

    std::uint8_t difficulty = 0; ///< Difficulty.

    /**
     * If set, the artifact will be deep-copied into the object instead of
     * only the extra attributes being copied.
     */
    bool deep_copy = false;

    /**
     * If true, this is a unique artifact that will never be dropped randomly.
     */
    bool unique = false;
};

typedef std::shared_ptr<ArtifactObject> ArtifactObjectPtr;
typedef std::shared_ptr<const ArtifactObject> ArtifactObjectPtrConst;

class ArtifactObjectManager : public Manager<ArtifactObjectManager> {
public:
    typedef std::unordered_map<std::string, ArtifactObjectPtr>
    ArtifactObjectMap; ///< Game object hash map with strings
    typedef std::vector<ArtifactObjectPtrConst> ArtifactObjectVector;
    typedef std::unordered_map<int, ArtifactObjectVector>
    ArtifactObjectVectorMap;

    static const char* path();
    static void load();
    static void add(ArtifactObjectPtr obj, int type);
    static boost::optional<ArtifactObjectPtrConst> get(const std::string& name);
    static ArtifactObjectMap::size_type count();
    void clear();

private:
    ArtifactObjectMap artifact_objects_map;
    ArtifactObjectVectorMap artifact_objects_list_map;
};

}
