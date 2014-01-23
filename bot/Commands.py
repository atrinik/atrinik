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
## Player commands handling.

from misc import command_execute
import locale, time, sys

## The commands class.
class Commands:
    ## Initialize the commands.
    ## @param bot The associated bot.
    def __init__(self, bot):
        self._bot = bot
        # Allowed info for ::player_command_info.
        self._info_allowed = ("level", "class", "race", "gender")
        self._currencies = [
            ("coppers", 1),
            ("silvers", 100),
            ("golds", 10000),
#            ("jades", 1000000),
            ("mithrils", 10000000),
        ]
        locale.setlocale(locale.LC_ALL, "")

    ## Get entry from the players dictionary.
    ## @param name The player's name.
    ## @param entry Entry to get.
    ## @return The return, or None if the player doesn't exist.
    def _get_entry(self, name, entry):
        try:
            return self._bot.db["players"][name][entry]
        except KeyError:
            return None

    ## Ask what an acronym means.
    ## @param name The player's name.
    ## @param groups Data from regex that triggered this.
    def player_command_acronym(self, name, groups):
        ret = command_execute(["wtf", "is", groups[0]])

        if not ret:
            return "Gee... I don't know what {0} means...".format(groups[0])

        return ret.strip()

    ## Ask when the bot last saw a player online.
    ## @param name The player's name.
    ## @param groups Data from regex that triggered this.
    def player_command_seen(self, name, groups):
        player = groups[0].capitalize()
        entry = self._get_entry(player, "timestamp")

        if not entry:
            return "I have never ever seen {0}.".format(player)

        return "Last time I saw {0}: {1}".format(player, time.strftime("%A, %d %B %Y %H:%M:%S UTC (+0000)", time.gmtime(entry)))

    ## Ask information (gender/level/etc) about a player.
    ## @param name The player's name.
    ## @param groups Data from regex that triggered this.
    def player_command_info(self, name, groups):
        info = groups[0].lower()
        player = groups[1].capitalize()

        if not player in self._bot.db["players"]:
            return "I don't know anything about {0}.".format(player)

        if info == "lvl":
            info = "level"

        if not info in self._info_allowed:
            entry = None
        else:
            entry = self._get_entry(player, info)

        if not entry:
            return "I don't know {0}'s {1}. Try one of: {2}.".format(player, info, ", ".join(self._info_allowed))

        return "{0}'s {1} is {2}.".format(player, info, entry)

    ## Get player(s) with the most deaths.
    ## @param name The player's name.
    ## @param groups Data from regex that triggered this.
    def player_command_died(self, name, groups):
        (multiple, pvp) = groups
        entry = "deaths_arena" if pvp else "deaths"

        # Sort the players by the number of highest deaths - highest first.
        l = sorted(self._bot.db["players"], key = lambda key: len(self._bot.db["players"][key][entry]), reverse = True)

        # No deaths...
        if not l or not self._bot.db["players"][l[0]][entry]:
            return "No player has ever died{0}.".format(" in the arena" if pvp else "")

        # Show top #x.
        if multiple:
            return "Players with most deaths{0}: {1}".format(" in the arena" if pvp else "", ", ".join(list(map(lambda player: "{0} ({1})".format(player, len(self._bot.db["players"][player][entry])), [player for player in l[:min(self._bot.config.getint("General", "max_kill_top"), len(l))] if self._bot.db["players"][player][entry]]))))
        # Just the first one.
        else:
            return "{0} has been killed most often{1}, with {2} deaths.".format(l[0], " in the arena" if pvp else "", len(self._bot.db["players"][l[0]][entry]))

    ## Ask the bot how many times a player has died.
    ## @param name The player's name.
    ## @param groups Data from regex that triggered this.
    def player_command_died_player(self, name, groups):
        (player, pvp) = groups
        player = player.capitalize()
        entry = "deaths_arena" if pvp else "deaths"

        if not player in self._bot.db["players"]:
            return "I don't know anything about {0}.".format(player)

        num = len(self._bot.db["players"][player][entry])

        if not num:
            return "{0} has never ever died{1}.".format(player, " in the arena" if pvp else "")

        return "{0} has died {1} times{2}.".format(player, num, " in the arena" if pvp else "")

    ## Ask which monster(s) are most lethal.
    ## @param name The player's name.
    ## @param groups Data from regex that triggered this.
    def player_command_lethal(self, name, groups):
        (multiple,) = groups
        # Sort the kills log, by highest number of kills.
        l = sorted(self._bot.db["kills"], key = lambda key: len(self._bot.db["kills"][key]["normal"]), reverse = True)

        # No kills yet.
        if not l or not self._bot.db["kills"][l[0]]["normal"]:
            return "Nothing has ever killed anyone."

        # Top #x killers.
        if multiple:
            return "Most dangerous killers: {0}".format(", ".join(list(map(lambda key: "{0} ({1})".format(self._bot.db["kills"][key]["name"], len(self._bot.db["kills"][key]["normal"])), [key for key in l[:min(self._bot.config.getint("General", "max_kill_top"), len(l))] if self._bot.db["kills"][key]["normal"]]))))
        # Just the first one.
        else:
            return "{0} is the most dangerous, with {1} kills.".format(self._bot.db["kills"][l[0]]["name"], len(self._bot.db["kills"][l[0]]["normal"]))

    ## Ask which player(s) killed the most in the arena.
    ## @param name The player's name.
    ## @param groups Data from regex that triggered this.
    def player_command_arena(self, name, groups):
        (multiple,) = groups
        # Sort kills in the arena, highest first.
        l = sorted(self._bot.db["kills"], key = lambda key: len(self._bot.db["kills"][key]["arena"]), reverse = True)

        # No arena kills.
        if not l or not self._bot.db["kills"][l[0]]["arena"]:
            return "No player has ever killed anyone in the arena."

        # Show top #x arena killers.
        if multiple:
            return "Players with most kills in the arena: {0}".format(", ".join(list(map(lambda key: "{0} ({1})".format(self._bot.db["kills"][key]["name"], len(self._bot.db["kills"][key]["arena"])), [key for key in l[:min(self._bot.config.getint("General", "max_kill_top"), len(l))] if self._bot.db["kills"][key]["arena"]]))))
        # Just the first one.
        else:
            print("{0} has killed the most players in the arena, with {1} kills.".format(self._bot.db["kills"][l[0]]["name"], len(self._bot.db["kills"][l[0]]["arena"])))
            return "{0} has killed the most players in the arena, with {1} kills.".format(self._bot.db["kills"][l[0]]["name"], len(self._bot.db["kills"][l[0]]["arena"]))

    ## Ask the bot how many times something has killed.
    ## @param name The player's name.
    ## @param groups Data from regex that triggered this.
    def player_command_killed_count(self, name, groups):
        (killer, pvp) = groups
        killer = killer.lower()
        entry = "arena" if pvp else "normal"

        if not killer in self._bot.db["kills"] or not self._bot.db["kills"][killer][entry]:
            return "{0} has never ever killed{1}.".format(killer, " in the arena" if pvp else "")

        return "{0} has killed {1} times{2}.".format(self._bot.db["kills"][killer]["name"], len(self._bot.db["kills"][killer][entry]), " in the arena" if pvp else "")

    ## Ask the bot how many times something/someone has killed a particular player.
    ## @param name The player's name.
    ## @param groups Data from regex that triggered this.
    def player_command_killed_player(self, name, groups):
        (killer, victim, pvp) = groups
        killer = killer.lower()
        victim = victim.lower()
        entry = "arena" if pvp else "normal"

        if not killer in self._bot.db["kills"] or not self._bot.db["kills"][killer][entry]:
            return "{0} has never ever killed{1}.".format(killer, " in the arena" if pvp else "")

        l = [t for t in self._bot.db["kills"][killer][entry] if t[0].lower() == victim]

        if not l:
            return "{0} has never ever killed {1}{2}.".format(self._bot.db["kills"][killer]["name"], victim, " in the arena" if pvp else "")

        return "{0} has killed {1} {2} times{3}.".format(self._bot.db["kills"][killer]["name"], l[0][0], len(l), " in the arena" if pvp else "")

    ## Privileged command: reload the configuration file, and the defined
    ## commands it has.
    ## @param name The player's name.
    ## @param groups Data from regex that triggered this.
    def player_commands_reload(self, name, groups):
        # The player is not allowed to do this...
        if not name in self._bot.config.get(self._bot.section, "admins").split(","):
            return "I don't really want to..."

        self._bot.commands_load()
        return "Reloaded list of commands."

    ## Triggers the chatbot.
    ## @param name The player's name.
    ## @param groups Data from regex that triggered this.
    def player_command_chat(self, name, groups):
        (msg, ) = groups
        msg = msg.replace("{{bot}}", "").replace(self._bot.name, "{{bot}}")
        return " ".join(self._bot.howie.submit(msg, name, "{0}@{1}:{2}-{3}".format(self._bot.name, self._bot.host, self._bot.port, name)).replace("\n", " ").replace("{{bot}}", self._bot.name).split())

    ## Calculates currency.
    ## @param name The player's name.
    ## @param groups Data from regex that triggered this.
    def player_command_currency(self, name, groups):
        (amount, currency1, currency2) = groups

        try:
            (def_currency1, ) = (t for t in self._currencies if t[0].startswith(currency1))
            (def_currency2, ) = (t for t in self._currencies if t[0].startswith(currency2))
        except ValueError:
            return "Gee... I can't calculate that one..."

        amount = int(amount)

        if def_currency1[1] * amount >= def_currency2[1]:
            return "{0} {1} coin(s) in {2} coin(s): {3}".format(amount, def_currency1[0][:-1], def_currency2[0][:-1], locale.format("%d", def_currency1[1] * amount / def_currency2[1], grouping = True))

        return "Gee... I can't answer that..."

    ## Exits the bot.
    ## @param name The player's name.
    ## @param groups Data from regex that triggered this.
    def player_command_quit(self, name, groups):
        if not name in self._bot.config.get(self._bot.section, "admins").split(","):
            return "I don't really want to..."

        sys.exit(0)
