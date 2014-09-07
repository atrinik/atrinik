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
 * Map tile object.
 */

#pragma once

#include <string>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>

#include <object.h>
#include <game_object.h>
#include <gfx_object_type.h>

namespace atrinik {

class MapObject;
typedef std::shared_ptr<MapObject> MapObjectPtr;
class GameObject;
typedef std::shared_ptr<GameObject> GameObjectPtr;
class MapTileObject;
typedef std::shared_ptr<MapTileObject> MapTileObjectPtr;

typedef uint16_t coord_t;

struct coords_t {
    coord_t x;
    coord_t y;
};

class MapTileObject : public ObjectCRTPShared<MapTileObject> {
public:
    typedef boost::multi_index_container<GameObjectPtr,
    boost::multi_index::indexed_by<
    // Sort by regular layer
    boost::multi_index::ordered_non_unique<boost::multi_index::const_mem_fun<
    GameObject, int, &GameObject::layer> >,
    // Sort by effective layer
    boost::multi_index::ordered_non_unique<boost::multi_index::const_mem_fun<
    GameObject, int, &GameObject::layer_effective> >
    > > GameObjectPtrContainer;

    typedef boost::multi_index::nth_index<GameObjectPtrContainer, 0>::type
    GameObjectPtrLayerIndex;
    typedef boost::multi_index::nth_index<GameObjectPtrContainer, 0>::type
    GameObjectPtrLayerEffectiveIndex;

    typedef std::pair<
    GameObjectPtrLayerIndex::const_iterator,
    GameObjectPtrLayerIndex::const_iterator
    > GameObjectPtrLayerIterator;

    typedef std::pair<
    GameObjectPtrLayerEffectiveIndex::const_iterator,
    GameObjectPtrLayerEffectiveIndex::const_iterator
    > GameObjectPtrLayerEffectiveIterator;

    MapTileObject() : ObjectCRTPShared(), layer_index(inv_.get<0>()),
    layer_effective_index(inv_.get<0>())
    {
    }

    ~MapTileObject()
    {
    }

    enum class Layer {
        System,
        Floor,
        FloorMask,
        Item,
        Item2,
        Wall,
        Living,
        Effect,
    };

    static constexpr int NumLayers()
    {
        return 7;
    }

    static constexpr int NumSubLayers()
    {
        return 5;
    }

    static constexpr int NumEffectiveLayers()
    {
        return NumLayers() * NumSubLayers();
    }

    int x()
    {
        return x_;
    }

    void x(int val)
    {
        x_ = val;
    }

    int y()
    {
        return y_;
    }

    void y(int val)
    {
        y_ = val;
    }

    MapObjectPtr env()
    {
        return env_.lock();
    }

    void env(MapObjectPtr env)
    {
        env_ = env;
    }

    void inv_push_back(GameObjectPtr obj);

    GameObjectPtrLayerIterator get_obj_layer(int layer)
    {
        return layer_index.equal_range(layer);
    }

    GameObjectPtrLayerEffectiveIterator get_obj_layer_effective(
            int layer_effective)
    {
        return layer_effective_index.equal_range(layer_effective);
    }

    virtual bool load(const std::string& key, const std::string& val);
    virtual std::string dump();
private:
    int x_;

    int y_;

    std::weak_ptr<MapObject> env_;
    GameObjectPtrContainer inv_;

    GameObjectPtrLayerIndex& layer_index;
    GameObjectPtrLayerEffectiveIndex& layer_effective_index;
};

}
