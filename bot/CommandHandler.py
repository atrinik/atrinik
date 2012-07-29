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
## Command handler.

import struct, re
from misc import NDI

## The command handler class.
class CommandHandler:
	## Initialize the command handler.
	## @param bot The associated bot.
	def __init__(self, bot):
		self._bot = bot
		# The various commands.
		self._commands = [
			# Handled specially - marks compressed data.
			("xxx", None),
			("COMC", None),
			("Map2", None),
			("Drawinfo", self.handle_command_draw_info),
			("Drawinfo2", self.handle_command_draw_info2),
			("File updates", None),
			("Item X", None),
			("Sound", None),
			("Target", None),
			("Update item", None),
			("Delete item", None),
			("Player stats", None),
			("Image", None),
			("Face1", None),
			("Animation", None),
			("Ready skill", None),
			("Player info", None),
			("Map stats", None),
			("Spell list", None),
			("Skill list", None),
			("Clear command queue", None),
			("Addme success", None),
			("Addme fail", None),
			("Version", None),
			("Goodbye", None),
			("Setup", self.handle_command_setup),
			("Query", self.handle_command_query),
			("Server file data", None),
			("New character", self.handle_command_newc),
			("Item Y", None),
			("Book GUI", None),
			("Party", None),
			("Quickslot", None),
			("Shop", None),
			("Quest list", None),
			("Region map", None),
		]

	## Handle a single command.
	## @param cmd_id The command ID.
	## @param data Command data.
	def handle_command(self, cmd_id, data):
		# Get the command.
		cmd = self._commands[cmd_id][1]

		# We do not bother handling this command.
		if not cmd:
			return

		cmd(data)

	## Handle the query command - used in the login process.
	## @param data The command data.
	def handle_command_query(self, data):
		data = " ".join(data.decode().split()[1:])

		# The bot's name.
		if data.startswith("What is your name?"):
			self._bot.send("reply {0}".format(self._bot.name))
		# The bot's password.
		elif data.startswith("What is your password?"):
			self._bot.send("reply {0}".format(self._bot.pswd))
		# Creating a new character for the bot - verify password.
		elif data.startswith("Please type your password again."):
			self._bot.send("reply {0}".format(self._bot.pswd))

	## Draw info command - only used for messages while still logging in.
	## @param data The command's data.
	def handle_command_draw_info(self, data):
		color = data.decode()[:6]
		data = data.decode()[6:]

		# Incompatibility warning, disconnect.
		if data == "This Atrinik server is outdated and incompatible with your client's version. Try another server.":
			self._bot.close()

	## Drawinfo2 command.
	## @param data The command's data.
	def handle_command_draw_info2(self, data):
		# Get the command flags.
		(flags,) = struct.unpack("H", data[:2][::-1])
		# Get the used color.
		color = data[2:8].decode()
		data = data[9:-1].decode()

		# Tell? Handle bot chat.
		if flags & NDI.TELL:
			match = re.match("([a-zA-Z0-9_-]+) tells you: (.+)", data)

			if match:
				(name, msg) = match.groups()
				self._bot.handle_chat(name, msg, "tell")
		elif flags & NDI.SAY:
			match = re.match("([a-zA-Z0-9_-]+) says: {0}, (.+)".format(self._bot.name), data)

			if match:
				(name, msg) = match.groups()
				self._bot.handle_chat(name, msg, "say")
		# Dark orange message.
		elif color == NDI.DK_ORANGE:
			# Entered the game, update timestamp.
			if data.find("entered the game.") != -1:
				match = re.match("([a-zA-Z0-9_-]+)(?: has)? entered the game\.", data)

				if match:
					self._bot.pi.update_seen(match.groups()[0])
		# White message.
		elif color == NDI.WHITE:
			# /who response.
			if data.find(" (lvl ") != -1:
				match = re.match("([a-zA-Z0-9_-]+) the (\w+) (\w+)(?: (\w+))? \(lvl (\d+)\)(?: (.+))?", data)

				if match:
					self._bot.pi.update_data(match.groups())
			# Player was killed - update logs.
			elif data.find(" killed ") != -1:
				match = re.match("(.+) killed ([a-zA-Z0-9_-]+)(?: with (.[^\(]+))?( \(duel\))?\.", data)

				if match:
					(killer, victim, how, pvp) = match.groups()
					self._bot.kl.kill_log(killer, victim, how, pvp)
					self._bot.pi.death_log(victim, killer, how, pvp)

	## New character command - send some default data to create this new
	## bot character.
	def handle_command_newc(self, data):
		self._bot.send(b"nc human_male 14 14 12 12 13 12 12")

	## Setup command response - request the server to add us.
	def handle_command_setup(self, data):
		self._bot.send(b"addme")
