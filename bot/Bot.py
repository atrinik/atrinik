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
## The game-bot handling.

import shelve, re, struct, time, zlib
from BaseSocket import *
from CommandHandler import *
from Commands import *
from KillsLog import *
from PlayerInfo import *

## The game-bot class.
class Bot(BaseSocket):
    ## Initializes the game bot.
    ## @param data Tuple containing the bot's server, port, name and password.
    ## @param bots List of bots.
    ## @param config ConfigParser instance.
    ## @param section Section in the config with the settings for this bot.
    def __init__(self, data, bots, config, section):
        (self.host, self.port, self.name, self.pswd) = data
        self._handle_data_reset()

        # Open the database for this bot.
        self.db = shelve.open("{0}@{1}.db".format(self.name, self.host).replace(".", "-").replace("/", "-"), writeback = True)

        # Store the list of bots.
        self._bots = bots
        self.config = config
        self.section = section

        # Initialize various classes.
        self.ch = CommandHandler(self)
        self.pi = PlayerInfo(self)
        self.kl = KillsLog(self)
        self.cmds = Commands(self)
        BaseSocket.__init__(self)
        self.commands_load()

        ts = time.time()
        self.who_stamp = ts + self.config.getint("General", "who_delay")

    ## Load the possible commands.
    def commands_load(self):
        self.config.read(['config.cfg'])
        # List of recognized commands.
        self.commands = [
            ("^what does(?: (?:the )?acronym)? (\w+) (?:stand for|mean)(?:\?)?$", self.cmds.player_command_acronym),
            ("^(?:wtf|what) is (\w+)(?:\?)?$", self.cmds.player_command_acronym),
            ("^when was ([a-zA-Z0-9_-]+) last (?:on|online|logged in|here|seen)(?:\?)?$", self.cmds.player_command_seen),
            ("^seen ([a-zA-Z0-9_-]+)(?:\?)?$", self.cmds.player_command_seen),
            ("^(?:when (?:have (?:you|u) last seen|did (?:you|u) last see)|(?:last )?seen|have (?:you|u) seen|did (?:you|u) see) ([a-zA-Z0-9_-]+)(?:\?)?$", self.cmds.player_command_seen),
            ("^(?:what|which) (\w+) is ([a-zA-Z0-9_-]+)(?:\?)?$", self.cmds.player_command_info),

            ("^wh(?:o|ich player(s)?) (?:(?:has |have )?died|got killed)(?: the)? most(?: often)?( in(?: the)? arena)?(?:\?)?$", self.cmds.player_command_died),
            ("^wh(?:ich|at) (?:mo(?:b|nster)|beast|thing|creature)(s)? (?:is|are|r)(?: (?:the )?most)? (?:lethal|dangerous)(?:\?)?$", self.cmds.player_command_lethal),
            ("^wh(?:o|ich player(s)?) (?:has|have) killed(?: (?:the )?most)(?: players)? in(?: the)? arena(?:\?)?$", self.cmds.player_command_arena),
            ("^how (?:many times|much) (?:has|did) ([a-zA-Z0-9_-]+) die(?:d)?( in(?: the)? arena)?(?:\?)?$", self.cmds.player_command_died_player),
            ("^how (?:many times|much) (?:has|did) (.+) kill(?:ed)?( in(?: the)? arena)?(?:\?)?$", self.cmds.player_command_killed_count),
            ("^how (?:many times|much) (?:has|did) (.+) kill(?:ed)? ([a-zA-Z0-9_-]+)( in(?: the)? arena)?(?:\?)?$", self.cmds.player_command_killed_player),

            ("^reload commands(?:\.)?$", self.cmds.player_commands_reload),
            ("^chat (.+)$", self.cmds.player_command_chat),
            ("^(?:how much is )?(\d+) (\w+)(?: coin(?:s)?)? in (\w+)(?: coin(?:s)?)?(?:\?)?$", self.cmds.player_command_currency),
            ("^shut(?: )?down$", self.cmds.player_command_quit),
        ]

        # Is there a commands section?
        if not self.config.has_section("Commands"):
            return

        for (name, value) in self.config.items("Commands"):
            (regex, s) = re.match("\"(.+)\" (.+)", value).groups()
            self.commands.append((regex, s))

    ## Closes the bot.
    def close(self):
        self.db.close()
        self.disconnect()
        self._bots.remove(self)

    ## Sends connect data to the game server.
    def _post_connect(self):
        self.send(b"version 1055 1055 Atrinikbot")
        self.send(b"setup bot 1")

    ## Handles queue command sending.
    def _command_queue_handler(self, cmd):
        self.send_command(cmd)

    ## Send data to the server.
    ## @param s The data to send.
    def send(self, s):
        if type(s) != type(bytes()):
            s = s.encode()

        l = len(s)
        s = struct.pack("BB", (l >> 8 & 0xFF), l & 0xFF) + s
        l += 2
        sent = 0

        while sent < l:
            sent += self.socket.send(s[sent:])

    ## Send game command to the server.
    ## @param s The command to send.
    def send_command(self, s):
        self.send("cm {0}".format(s))

    ## Reset internal data used when reading data from socket.
    def _handle_data_reset(self):
        self._readbuf = b""
        self._readbuf_len = 0
        self._header_len = 0
        self._cmd_len = -1

    ## Handle reading data from socket.
    def handle_data(self):
        starttime = time.time()

        while True:
            if time.time() - starttime > 0.5:
                break

            # Less than 2 bytes read (possibly 0 so far), choose how much to read.
            if self._readbuf_len < 2:
                # If we read something already, check for bit marker and read more
                # if applicable, as this is as 3-byte header.
                if self._readbuf_len > 0 and struct.unpack("B", self._readbuf[:1])[0] & 0x80:
                    toread = 3 - self._readbuf_len
                # Standard 2-byte header.
                else:
                    toread = 2 - self._readbuf_len
            # Exactly 2 bytes read; if the bit marker is set, read 1 more, as this
            # a 3-byte header instead of the standard 2-byte one.
            elif self._readbuf_len == 2 and struct.unpack("B", self._readbuf[:1])[0] & 0x80:
                toread = 1
            else:
                # We got the header data, try to parse it.
                if self._readbuf_len <= 3:
                    # Figure out the header length.
                    self._header_len = 3 if struct.unpack("B", self._readbuf[:1])[0] & 0x80 else 2
                    self._cmd_len = 0

                    # Unpack the header.
                    unpacked = struct.unpack("{0}B".format(self._header_len), self._readbuf[:self._header_len])
                    i = 0

                    # 3-byte header.
                    if self._header_len == 3:
                        self._cmd_len += (unpacked[i] & 0x7f) << 16
                        i += 1

                    # 2-byte header or continuation of the above 3-byte one.
                    self._cmd_len += unpacked[i] << 8
                    i += 1
                    self._cmd_len += unpacked[i]

                # Now we know exactly how much data to read to complete the
                # command.
                toread = self._cmd_len + self._header_len - self._readbuf_len

            try:
                # Try to read the data.
                data = self.socket.recv(toread)
            except socket.error:
                return

            # Failed; reset connection.
            if not data:
                self._handle_data_reset()
                self._mark_reconnect()
                return

            # Store the data and update the data's total length.
            self._readbuf += data
            self._readbuf_len += len(data)

            # Have we completed reading the data for the command yet?
            if self._readbuf_len == self._cmd_len + self._header_len:
                # Strip off the header bytes.
                self._readbuf = self._readbuf[self._header_len:]
                # Get the command ID.
                (cmd_id,) = struct.unpack("B", self._readbuf[:1])

                # Command #0 marks compressed data.
                if cmd_id == 0:
                    # Figure out the uncompressed length.
                    ucomp_len = struct.unpack("I", self._readbuf[1:5][::-1])
                    # Uncompress the data.
                    self._readbuf = zlib.decompress(self._readbuf[5:])
                    # Get the actual command ID now.
                    (cmd_id,) = struct.unpack("B", self._readbuf[:1])

                # Handle the command.
                self.ch.handle_command(cmd_id, self._readbuf[1:])
                # Reset the internal pointers.
                self._handle_data_reset()

    ## Handle anything extra.
    def handle_extra(self):
        ts = time.time()

        # Check if it's time to send a /who command yet.
        if ts >= self.who_stamp:
            self.who_stamp = ts + self.config.getint("General", "who_delay")
            self.send_command("/who")

        # Handle command queue.
        self._command_queue_handle()

    ## Handle chat.
    ## @param name Player that activated the chat.
    ## @param msg The message player said.
    ## @param chat Chat type used (tell, shout, say, etc).
    def handle_chat(self, name, msg, chat):
        # Default message if we can't match any regex.
        ret = "I cannot answer that query."
        # Remove extraneous spaces between words.
        msg = " ".join(msg.split())

        # Try to parse the commands.
        for (regex, cmd) in self.commands:
            match = re.match(regex, msg, re.I)

            if match:
                if type(cmd) == type(str()):
                    ret = cmd.format(name)
                else:
                    ret = cmd(name, match.groups())

                break

        if chat == "tell":
            self.send_command("/tell {0} {1}".format(name, ret))
        elif chat == "say":
            self.send_command("/say {0}".format(ret))
