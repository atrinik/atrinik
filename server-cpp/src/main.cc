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
 * The main file.
 */

#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <openssl/ssl.h>

#include <object.h>
#include <game_object.h>
#include <archetype_parser.h>
#include <map_parser.h>
#include <region_parser.h>
#include <game_server.h>
#include <account.h>

using namespace atrinik;
using namespace boost;
using namespace std;

//void consumer()
//{
//    while (true) {
//        lock_guard<mutex> lock(GameObject::active_objects_mutex);
//        GameObject::iobjects_t::iterator it;
//        for (it = GameObject::active_objects.begin();
//                it != GameObject::active_objects.end(); it++) {
//            cout << it->second->name() << endl;
//        }
//    }
//}
//
//void producer()
//{
//    while (true) {
//        GameObject *obj;
//        obj = new GameObject("foo");
//        obj->name("test-" + lexical_cast<string>(obj->uid()));
//
//        GameObject::active_objects.insert(make_pair(obj->uid(), obj));
//    }
//}
//
//void deleter()
//{
//    while (true) {
//        lock_guard<mutex> lock(GameObject::active_objects_mutex);
//        GameObject::iobjects_t::iterator it = GameObject::active_objects.begin();
//
//        if (it != GameObject::active_objects.end()) {
//            GameObject *obj = it->second;
//            GameObject::active_objects.erase(obj->uid());
//            delete obj;
//        }
//    }
//}

//void socket_thread()
//{
//    try {
//        asio::io_service io_service;
//        game_server server(io_service);
//        io_service.run();
//    } catch (std::exception& e) {
//        cout << e.what() << endl;
//    }
//}

int main(int argc, char **argv)
{
    SSL_load_error_strings();
    SSL_library_init();
    
    ArchetypeParser *parser = new ArchetypeParser;
    parser->read_archetypes("../arch/archetypes");
    parser->load_archetypes_pass1();

    RegionParser *region_parser = new RegionParser;
    region_parser->load("../maps/regions.reg");

    MapParser* map_parser = new MapParser;
    map_parser->load_map(argc > 1 ? argv[1] : "../maps/hall_of_dms");

    asio::io_service io_service;
    asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v6(), 13360);
    game_server_ptr server(new GameServer(io_service, endpoint));
    thread bt(bind(&asio::io_service::run, &io_service));
    
    Account account;
    
    try {
        account.action_register("Test", "password", "password");
    } catch (const AccountError& e) {
        cout << e.what() << endl;
    }
    

    while (true) {
        server->process();
        usleep(125000);
    }

    bt.join();

//    thread thread_socket(socket_thread);
//    thread_socket.join();

    return 0;

//    thread thread1(consumer);
//    thread thread2(producer);
//    thread thread3(producer);
//    thread thread4(deleter);
//    thread thread5(deleter);
//    thread1.join();
//    thread2.join();
//    thread3.join();
//    thread4.join();
//    thread5.join();
//
//    return 0;
}
