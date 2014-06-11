/*******************************************************************************
*               Atrinik, a Multiplayer Online Role Playing Game                *
*                                                                              *
*       Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team        *
*                                                                              *
* This program is free software; you can redistribute it and/or modify it      *
* under the terms of the GNU General Public License as published by the Free   *
* Software Foundation; either version 2 of the License, or (at your option)    *
* any later version.                                                           *
*                                                                              *
* This program is distributed in the hope that it will be useful, but WITHOUT  *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for     *
* more details.                                                                *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* this program; if not, write to the Free Software Foundation, Inc., 675 Mass  *
* Ave, Cambridge, MA 02139, USA.                                               *
*                                                                              *
* The author can be reached at admin@atrinik.org                               *
*******************************************************************************/

/**
 * @file
 * The main file.
 */

#include <boost/lexical_cast.hpp>

#include "object.h"
#include "game_object.h"
#include "archetype_parser.h"

using namespace atrinik;
using namespace boost;
using namespace std;
using namespace tbb;

void consumer()
{
    while (true) {
        lock_guard<mutex> lock(GameObject::active_objects_mutex);
        GameObject::iobjects_t::iterator it;
        for (it = GameObject::active_objects.begin();
                it != GameObject::active_objects.end(); it++) {
            cout << it->second->name() << endl;
        }
    }
}

void producer()
{
    while (true) {
        GameObject *obj;
        obj = new GameObject("foo");
        obj->name("test-" + lexical_cast<string>(obj->uid()));

        GameObject::active_objects.insert(make_pair(obj->uid(), obj));
    }
}

void deleter()
{
    while (true) {
        lock_guard<mutex> lock(GameObject::active_objects_mutex);
        GameObject::iobjects_t::iterator it = GameObject::active_objects.begin();

        if (it != GameObject::active_objects.end()) {
            GameObject *obj = it->second;
            GameObject::active_objects.erase(obj->uid());
            delete obj;
        }
    }
}

int main(int, char **)
{
    ArchetypeParser *parser = new ArchetypeParser;
    parser->read_archetypes("../arch/archetypes");
    parser->load_archetypes_pass1();

    GameObject::sobjects_t::accessor result;
    if (GameObject::archetypes.find(result, "ship_floor_we_light_1")) {
        cout << result->second->archname() << endl;
        GameObject *obj = result->second->clone();
        cout << obj->dump() << endl;
        delete obj;
    }

    delete parser;

    return 0;

    thread thread1(consumer);
    thread thread2(producer);
    thread thread3(producer);
    thread thread4(deleter);
    thread thread5(deleter);
    thread1.join();
    thread2.join();
    thread3.join();
    thread4.join();
    thread5.join();

    return 0;
}
