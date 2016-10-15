/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
 *                                                                       *
 * Fork from Crossfire (Multiplayer game for X-windows).                 *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the Free Software           *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
 *                                                                       *
 * The author can be reached at admin@atrinik.org                        *
 ************************************************************************/

/**
 * @file
 * The type list.
 */

#ifdef OBJECT_TYPE_DECLARE
#   undef OBJECT_TYPE_INIT
#   define OBJECT_TYPE_INIT(what) \
        void CONCAT(object_type_init_, what)(void)
#else
#   undef OBJECT_TYPE_INIT
#   define OBJECT_TYPE_INIT(what) \
        CONCAT(object_type_init_, what)();
#endif

OBJECT_TYPE_INIT(ability);
OBJECT_TYPE_INIT(amulet);
OBJECT_TYPE_INIT(armour);
OBJECT_TYPE_INIT(arrow);
OBJECT_TYPE_INIT(base_info);
OBJECT_TYPE_INIT(beacon);
OBJECT_TYPE_INIT(blindness);
OBJECT_TYPE_INIT(book);
OBJECT_TYPE_INIT(book_spell);
OBJECT_TYPE_INIT(boots);
OBJECT_TYPE_INIT(bow);
OBJECT_TYPE_INIT(bracers);
OBJECT_TYPE_INIT(bullet);
OBJECT_TYPE_INIT(button);
OBJECT_TYPE_INIT(check_inv);
OBJECT_TYPE_INIT(client_map_info);
OBJECT_TYPE_INIT(cloak);
OBJECT_TYPE_INIT(clock);
OBJECT_TYPE_INIT(compass);
OBJECT_TYPE_INIT(cone);
OBJECT_TYPE_INIT(confusion);
OBJECT_TYPE_INIT(container);
OBJECT_TYPE_INIT(corpse);
OBJECT_TYPE_INIT(creator);
OBJECT_TYPE_INIT(dead_object);
OBJECT_TYPE_INIT(detector);
OBJECT_TYPE_INIT(director);
OBJECT_TYPE_INIT(disease);
OBJECT_TYPE_INIT(door);
OBJECT_TYPE_INIT(drink);
OBJECT_TYPE_INIT(duplicator);
OBJECT_TYPE_INIT(event_obj);
OBJECT_TYPE_INIT(exit);
OBJECT_TYPE_INIT(experience);
OBJECT_TYPE_INIT(firewall);
OBJECT_TYPE_INIT(flesh);
OBJECT_TYPE_INIT(floor);
OBJECT_TYPE_INIT(food);
OBJECT_TYPE_INIT(force);
OBJECT_TYPE_INIT(gate);
OBJECT_TYPE_INIT(gem);
OBJECT_TYPE_INIT(girdle);
OBJECT_TYPE_INIT(gloves);
OBJECT_TYPE_INIT(god);
OBJECT_TYPE_INIT(handle);
OBJECT_TYPE_INIT(helmet);
OBJECT_TYPE_INIT(holy_altar);
OBJECT_TYPE_INIT(inorganic);
OBJECT_TYPE_INIT(jewel);
OBJECT_TYPE_INIT(key);
OBJECT_TYPE_INIT(light_apply);
OBJECT_TYPE_INIT(lightning);
OBJECT_TYPE_INIT(light_refill);
OBJECT_TYPE_INIT(light_source);
OBJECT_TYPE_INIT(magic_mirror);
OBJECT_TYPE_INIT(map);
OBJECT_TYPE_INIT(map_event_obj);
OBJECT_TYPE_INIT(map_info);
OBJECT_TYPE_INIT(marker);
OBJECT_TYPE_INIT(material);
OBJECT_TYPE_INIT(misc_object);
OBJECT_TYPE_INIT(money);
OBJECT_TYPE_INIT(monster);
OBJECT_TYPE_INIT(nugget);
OBJECT_TYPE_INIT(organic);
OBJECT_TYPE_INIT(painting);
OBJECT_TYPE_INIT(pants);
OBJECT_TYPE_INIT(pearl);
OBJECT_TYPE_INIT(pedestal);
OBJECT_TYPE_INIT(player);
OBJECT_TYPE_INIT(player_mover);
OBJECT_TYPE_INIT(poisoning);
OBJECT_TYPE_INIT(potion);
OBJECT_TYPE_INIT(potion_effect);
OBJECT_TYPE_INIT(power_crystal);
OBJECT_TYPE_INIT(quest_container);
OBJECT_TYPE_INIT(random_drop);
OBJECT_TYPE_INIT(ring);
OBJECT_TYPE_INIT(rod);
OBJECT_TYPE_INIT(rune);
OBJECT_TYPE_INIT(savebed);
OBJECT_TYPE_INIT(scroll);
OBJECT_TYPE_INIT(shield);
OBJECT_TYPE_INIT(shop_floor);
OBJECT_TYPE_INIT(sign);
OBJECT_TYPE_INIT(skill);
OBJECT_TYPE_INIT(skill_item);
OBJECT_TYPE_INIT(sound_ambient);
OBJECT_TYPE_INIT(spawn_point);
OBJECT_TYPE_INIT(spawn_point_info);
OBJECT_TYPE_INIT(spawn_point_mob);
OBJECT_TYPE_INIT(spell);
OBJECT_TYPE_INIT(spinner);
OBJECT_TYPE_INIT(swarm_spell);
OBJECT_TYPE_INIT(symptom);
OBJECT_TYPE_INIT(treasure_t);
OBJECT_TYPE_INIT(trinket);
OBJECT_TYPE_INIT(wall);
OBJECT_TYPE_INIT(wand);
OBJECT_TYPE_INIT(waypoint);
OBJECT_TYPE_INIT(wealth);
OBJECT_TYPE_INIT(weapon);
OBJECT_TYPE_INIT(word_of_recall);
