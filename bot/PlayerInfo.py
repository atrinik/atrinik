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
## Player info storage.

import time

## Player info storage class.
class PlayerInfo:
	## Initialize the class.
	## @param bot The associated bot.
	def __init__(self, bot):
		self._bot = bot

		# Add new dictionary to the database if it doesn't exist yet.
		if not "players" in self._bot.db:
			self._bot.db["players"] = {}

	## Initialize a new player in the database.
	##
	## If the player is already in the database, their dictionary will not
	## be re-initialized.
	## @param name The player's name.
	def init_player(self, name):
		# Check that we have not yet initialized this player.
		if name in self._bot.db["players"]:
			return

		self._bot.db["players"][name] = {
			"timestamp": 0,
			"level": 0,
			"gender": None,
			"race": None,
			"class": None,
			"extra": None,
			"deaths": [],
			"deaths_arena": [],
		}

	## Update player's last seen timestamp.
	## @param name The player's name.
	def update_seen(self, name):
		self.init_player(name)
		self._bot.db["players"][name]["timestamp"] = int(time.time())

	## Update data about player from the /who list.
	## @param Tuple containing the player's name, gender, race, class (can be
	## None), level, and any extra data (such as " [BOT]")
	def update_data(self, data):
		(name, gender, race, player_class, level, extra) = data
		self.init_player(name)
		self._bot.db["players"][name]["gender"] = gender
		self._bot.db["players"][name]["race"] = race
		self._bot.db["players"][name]["class"] = player_class
		self._bot.db["players"][name]["level"] = int(level)
		self._bot.db["players"][name]["extra"] = extra

	## Log player's death.
	## @param name The player's name.
	## @param killer The killer.
	## @param how How the player was killed, for example, "with icestorm".
	## @param pvp If not None, the player was killed in PvP.
	def death_log(self, name, killer, how, pvp):
		self.init_player(name)

		if pvp:
			where = self._bot.db["players"][name]["deaths_arena"]
		else:
			where = self._bot.db["players"][name]["deaths"]

		where.append((killer, how))
