#!/usr/bin/env python
#*************************************************************************
#*            Atrinik, a Multiplayer Online Role Playing Game            *
#*                                                                       *
#*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
#*                                                                       *
#* Fork from Crossfire (Multiplayer game for X-windows).                 *
#*                                                                       *
#* This program is free software; you can redistribute it and/or modify  *
#* it under the terms of the GNU General Public License as published by  *
#* the Free Software Foundation; either version 2 of the License, or     *
#* (at your option) any later version.                                   *
#*                                                                       *
#* This program is distributed in the hope that it will be useful,       *
#* but WITHOUT ANY WARRANTY; without even the implied warranty of        *
#* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
#* GNU General Public License for more details.                          *
#*                                                                       *
#* You should have received a copy of the GNU General Public License     *
#* along with this program; if not, write to the Free Software           *
#* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
#*                                                                       *
#* The author can be reached at admin@atrinik.org                        *
#*************************************************************************

## @file
## The Atrinik bot script.

import CommitChecker, time, threading, shelve
from misc import *
from Bot import *
from CIA import *
from howie import Howie

try:
    from ConfigParser import ConfigParser
# Python 3.x
except:
    from configparser import ConfigParser

## Create database lock.
db_lock = threading.RLock()
## Create the database.
db = shelve.open("bot.db", writeback = True)
bots = []
cia = None

## The main function.
def main():
    # Create a new ConfigParser and read the config.
    config = ConfigParser()
    config.readfp(open("config.cfg"))
    config.read(["config-custom.cfg"])
    chatbot = None

    if config.getboolean("General", "cia"):
        cia = CIA()

    # Check configuration sections for game and IRC bots.
    for section in config.sections():
        if section.startswith("Game"):
            if not chatbot:
                chatbot = Howie.Howie()

            bot = Bot((config.get(section, "host"), config.getint(section, "port"), config.get(section, "name"), config.get(section, "pswd")), bots, config, section)
            bot.howie = chatbot
            bots.append(bot)

    # If commits checker is enabled, do some work.
    if CommitChecker.enabled:
        commits_check = time.time() + config.getfloat("General", "commits_check_delay")

        if not "branches" in db:
            db["branches"] = {}

    # The main infinite loop.
    while True:
        # No bots left, bail out.
        if not bots and not cia:
            print("No bots running left, bailing out.")
            break

        # Get the current time.
        t1 = time.time()

        # If commits checker is enabled and enough time has elapsed since
        # last time we checked for new commits, try to check for new commits
        # again.
        if CommitChecker.enabled and t1 >= commits_check:
            # Create the commits checker thread and start it.
            thread = CommitChecker.CommitChecker(config, db, db_lock, bots, cia)
            thread.start()
            # Update time of next check.
            commits_check = t1 + config.getfloat("General", "commits_check_delay")

        # Handle all active connections.
        for connection in bots:
            if not connection.handle_reconnect():
                continue

            connection.handle_data()
            connection.handle_extra()

        # Get the current time again.
        t2 = time.time()
        # Figure out how long to sleep for.
        sleep_time = config.getfloat("General", "sleep_time") - (t2 - t1)

        # Sleep for so much time if possible (do not try sleeping for
        # negative amounts of time).
        if sleep_time > 0.0:
            time.sleep(sleep_time)

try:
    main()
finally:
    # Close all connections.
    for connection in bots:
        connection.close()

    # Close the database.
    db_lock.acquire()
    db.close()
    db_lock.release()
