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
 * Game object.
 */

#pragma once

#include <stdint.h>
#include <string.h>
#include <unordered_map>
#include <boost/variant.hpp>
#include <boost/optional.hpp>

#include <object.h>
#include <map_tile_object.h>
#include <game_object_type.h>
#include <manager.h>

namespace atrinik {

struct mapcoords_t : coords_t {
    std::string path;
};

class GameObject;
typedef std::shared_ptr<GameObject> GameObjectPtr;
typedef std::shared_ptr<const GameObject> GameObjectPtrConst;

class GameObject : public ObjectCRTPShared<GameObject> {
private:
    std::list<GameObjectType*> types;

    boost::variant<boost::blank, std::weak_ptr<MapTileObject>,
    std::weak_ptr<GameObject>> env_;

    std::list<GameObjectPtr> inv_;
public:
    boost::variant<GameObjectPtrConst, std::string> arch;

    GameObject() : ObjectCRTPShared()
    {
    }

    ~GameObject()
    {
    }

    GameObject(const GameObject& obj)
    {
        arch = obj.arch;

        for (auto &it : obj.types) {
            types.push_back(it->clone());
        }
    }

    void env(boost::variant<GameObjectPtr, MapTileObjectPtr> env)
    {
        env_ = env;
    }

    void inv_push_back(GameObjectPtr obj)
    {
        inv_.push_back(obj);
        obj->env_ = shared_from_this();
    }

    boost::optional<MapTileObjectPtr> map_tile()
    {
        try {
            return boost::optional<MapTileObjectPtr>(
                    boost::get<std::weak_ptr<MapTileObject>>(env_).lock());
        } catch (boost::bad_get) {
            return boost::optional<MapTileObjectPtr>();
        }
    }

    boost::optional<MapObjectPtr> map()
    {
        auto tile = map_tile();

        if (!tile) {
            return boost::optional<MapObjectPtr>();
        }

        return boost::optional<MapObjectPtr>((*tile)->env());
    }

    virtual bool load(const std::string& key, const std::string& val);
    virtual std::string dump();

    int getprimaryinstance() const
    {
        if (types.size() > 0) {
            return types.front()->gettype();
        }

        return -1;
    }

    template<class T>
    bool isinstance() const
    {
        for (auto it : types) {
            if (it->gettype() == T::type_()) {
                return true;
            }
        }

        return false;
    }

    bool isinstance(const std::string& val) const
    {
        for (auto it : types) {
            if (it->gettypeid() == val) {
                return false;
            }
        }

        return false;
    }

    template<class T>
    T* getaddinstance()
    {
        T* ptr = getinstance<T>();

        if (!ptr) {
            ptr = addinstance<T>();
        }

        return ptr;
    }

    GameObjectType *getaddinstance(const std::string& val)
    {
        GameObjectType* ptr = getinstance(val);

        if (!ptr) {
            ptr = addinstance(val);
        }

        return ptr;
    }

    template<class T>
    T* getinstance() const
    {
        for (auto it : types) {
            if (it->gettype() == T::type_()) {
                return dynamic_cast<T*> (it);
            }
        }

        return NULL;
    }

    GameObjectType* getinstance(const std::string& val) const
    {
        for (auto it : types) {
            if (it->gettypeid() == val) {
                return it;
            }
        }

        return NULL;
    }

    GameObjectType* getinstance(const int val) const
    {
        for (auto it : types) {
            if (it->gettype() == val) {
                return it;
            }
        }

        return NULL;
    }

    template<class T>
    T* addinstance()
    {
        T* ptr = new T();
        types.push_back(ptr);
        return ptr;
    }

    GameObjectType* addinstance(const std::string val)
    {
        GameObjectType* ptr = GameObjectTypeFactory::create_instance(val);
        types.push_back(ptr);
        return ptr;
    }

    void cleaninstances(std::vector<GameObjectType*> instances)
    {
        auto it = std::remove_if(types.begin(), types.end(),
                [instances](GameObjectType * type) -> bool
                {
                    return std::find(instances.begin(), instances.end(),
                            type) != instances.end();
                });
        types.erase(it, types.end());
    }

    /**
     * Old-style game object types. These only exist for backwards
     * compatibility with the "type X" attribute (which then gets mapped to the
     * appropriate type IDs in ObjectParser::assign_types).
     */
    enum class Types {
        None, Player, Bullet, Rod, Treasure, Potion, Food, Book = 8, Clock,
        Material, Duplicator, Lightning, Arrow, Bow, Weapon, Armour, Pedestal,
        Confusion = 19, Door, Key, Map, MagicMirror = 28, Spell, Shield = 33,
        Helmet, Greaves, Money, Class, Gravestone, Amulet, PlayerMover,
        Teleporter, Creator, Skill, Experience, Blindness = 49, God, Detector,
        SkillItem, DeadObject, Drink, Marker, HolyAltar, Pearl = 59, Gem,
        SoundAmbient, Firewall, CheckInv = 64, Exit = 66, ShopFloor = 68,
        ShopMat, Ring, Floor, Flesh, Inorganic, LightApply, Wall = 77,
        LightSource, MiscObject, Monster, SpawnPoint, LightRefill,
        SpawnPointMob, SpawnPointInfo, Organic = 86, Cloak, Cone, Spinner = 90,
        Gate, Button, Handle, WordOfRecall = 96, Sign = 98, Boots, Gloves,
        BaseInfo, RandomDrop, Bracers = 104, Poisoning, Savebed, Wand = 109,
        Ability, Scroll, Director, Girdle, Force, PotionEffect, Jewel, Nugget,
        EventObject, WaypointObject, QuestContainer, Container = 122,
        Wealth = 125, Beacon, MapEventObj, Compass = 151, MapInfo, SwarmSpell,
        Rune, ClientMapInfo, PowerCrystal, Corpse, Disease, Symptom, Nrof
    };
};

class GameObjectArchVisitor : public boost::static_visitor<std::string> {
public:

    const std::string& operator()(const std::string& s) const
    {
        return s;
    }

    const std::string& operator()(GameObjectPtrConst obj) const
    {
        return boost::get<std::string>(obj->arch);
    }
};

class GameObjectManager : public Manager<GameObjectManager> {
public:
    typedef std::unordered_map<std::string, GameObjectPtr>
    GameObjectMap; ///< Game object hash map with strings

    static std::string SingularityObjectName;

    static const char* path();
    static void load();
    static void add(const std::string& archname, GameObjectPtr obj);
    static GameObjectPtrConst get(const std::string& archname);
    static GameObjectMap::size_type count();
    void clear();

private:
    GameObjectMap game_objects_map;
};

}
