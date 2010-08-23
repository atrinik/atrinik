## @file
## Implements the Guild class, which provides an API for guild management.

from Atrinik import *
import time

## Guild database file.
_guilddb_file = "../server/data/guilds"
_data_dir = "../server/data"

## Default configuration of the various guilds.
_config = {
	"Quick Wolves": [
		["/guilds/quick_wolves/guild", "/guilds/quick_wolves/oracle", "/guilds/quick_wolves/storage"],
		("/shattered_islands/world_0515", 12, 3),
		(2, 15),
		("/guilds/quick_wolves/guild", 20, 20),
	],
}

## The Guild class.
class Guild:
	## @defgroup guild_config Guild configuration
	## Getting configuration options of the guild.
	## @{

	## A list of maps that belong to the guild.
	maps = 0
	## A tuple containing map, X and Y where to send guild member if they
	## have been removed from the guild.
	kick_pos = 1
	## X/Y in a tuple where the player should be sent on current map if they
	## can't access the guild Oracle.
	oracle_pos = 2
	## Map, X and Y in a tuple where to teleport the player when they enter
	## the guild by talking to the manager NPC.
	enter_pos = 3

	## @}

	## @defgroup guild_flags Guild flags
	## Bitmask flags about the guild.
	## @{

	## The guild is closed. No guild applications are possible.
	guild_closed = 1
	## @}

	## @defgroup guild_member_flags Guild member flags
	## Bitmask flags about guild members.
	## @{

	## The member is a member of the guild, but has not been approved by
	## a guild administrator, therefore cannot enter guild or participate
	## in guild chats.
	member_requested = 1
	## The member is a guild administrator.
	member_administrator = 2
	## @}

	## Maximum number of characters supported for rank names.
	rank_max_chars = 20
	## Minimum number of hours that can be configured for rank's reset time.
	rank_reset_min = 1
	## Maximum number of hours that can be configured for rank's reset time.
	rank_reset_max = 24
	## Default reset time in hours.
	rank_reset_default = 24
	## Maximum value limit for ranks.
	rank_value_max = 10000000

	## The class initializer.
	## @param guild What guild we're managing. The default is None, which
	## makes it so no new entry is created in the database if the guild does
	## not exist.
	def __init__(self, guild = None):
		## Initialize guild name from parameters.
		self._guild = guild

		# Try to get the db from the cache.
		try:
			self._db = CacheGet("guild_db")
		# Not in cache.
		except:
			import shelve
			## Initialize guild database from the database file.
			self._db = shelve.open(_guilddb_file, writeback = True)
			# Add it to the cache.
			CacheAdd("guild_db", self._db)

		# If the guild does not exist yet, initialize a new one.
		if self._guild and not self._guild in self._db:
			self._db[self._guild] = {
				"members": {},
				"ranks": {},
				"flags": self.guild_closed,
				"founder": "",
			}

		self.load()

	## Change the managed guild.
	## @param guild Guild to change to.
	def set(self, guild):
		self._guild = guild
		self.load()

	## Load up various information from the guild database.
	## Should only be called once.
	def load(self):
		if not self._guild:
			return

		self._members = self._db[self._guild]["members"]
		self._ranks = self._db[self._guild]["ranks"]
		self._flags = self._db[self._guild]["flags"]
		self._founder = self._db[self._guild]["founder"]

	## Get the guild's name.
	## @return The guild's name.
	def get_name(self):
		return self._guild

	## Get default config option of the guild.
	## @param opt The option to get, one of @ref guild_config.
	## @return The config option.
	def get(self, opt):
		return _config[self._guild][opt]

	## Get the guild's members.
	## @return Dictionary of the guild's members.
	def get_members(self):
		return self._members

	## Add a member to the guild.
	## @param name The member's name.
	## @param flags Optional flags to set by default, one or a combination of @ref guild_member_flags.
	def member_add(self, name, flags = 0):
		self._db[self._guild]["members"][name] = {
			"flags": flags,
			"rank": None,
			"value_used": 0,
			"value_limit_time": 0,
		}

	## Remove a member from the guild.
	## @param name The member name to remove.
	## @return True if the member was removed, False otherwise.
	def member_remove(self, name):
		if not self.member_exists(name):
			return False

		# Check if this is just a membership request. If so, we don't need to remove
		# them from guild maps.
		requested = self._members[name]["flags"] & self.member_requested
		del self._db[self._guild]["members"][name]

		if not requested:
			# Try to find the player.
			member = FindPlayer(name)

			# If the player is online, remove them from the guild maps.
			# If they are not online, this will be taken care of later.
			if member != None:
				member.Write("You have been removed from the guild. Goodbye!", COLOR_RED)
				self.member_kick(member)

		return True

	## Approve a member for full guild membership.
	## @param name Member name to approve.
	## @return True on success, False on failure.
	def member_approve(self, name):
		if not self.member_exists(name):
			return False

		self._db[self._guild]["members"][name]["flags"] &= ~self.member_requested

		return True

	## Check if member has been approved for full guild membership.
	## @param name The member name to check.
	## @return True if they have been approved, False otherwise.
	def member_approved(self, name):
		if not self.member_exists(name) or self._members[name]["flags"] & self.member_requested:
			return False

		return True

	## Kick a player from the guild maps. The player won't be kicked if they
	## are not currently located in the guild maps.
	## @param player The player to check.
	def member_kick(self, player):
		if not player.map.path in self.get(self.maps):
			return

		(m, x, y) = self.get(self.kick_pos)
		player.TeleportTo(m, x, y)

	## Get the information about the specified guild member.
	## @param name The guild member.
	## @return The information about the member, None if there is no such
	## member in the guild.
	def member(self, name):
		if name in self._members:
			return self._members[name]

		return None

	## Find out whether the specified player is in a guild.
	## @param name The player's name.
	## @return None if the player is not in any guild, otherwise a tuple
	## containing the guild name, information about the member and whether
	## they have been approved or not.
	def pl_get_guild(self, name):
		for guild in self._db:
			if name in self._db[guild]["members"]:
				return (guild, self._db[guild]["members"], not self._db[guild]["members"][name]["flags"] & self.member_requested)

		return None

	## Make the specified member an administrator.
	## @param name Name of the member.
	## @return True if the member was made an administrator, False otherwise.
	def member_admin_make(self, name):
		if not self.member_approved(name):
			return False

		# Is the member an administrator already?
		if self._members[name]["flags"] & self.member_administrator:
			return False

		self._db[self._guild]["members"][name]["flags"] |= self.member_administrator

		return True

	## Remove administrator rights from the specified member.
	## @param name Name of the member.
	## @return True if the member had administrator rights taken away,
	## False otherwise.
	def member_admin_remove(self, name):
		if not self.member_approved(name):
			return False

		# Cannot remove administrator status from member who never had
		# it.
		if not self._members[name]["flags"] & self.member_administrator:
			return False

		self._db[self._guild]["members"][name]["flags"] &= ~self.member_administrator

		# Try to find the player.
		member = FindPlayer(name)

		# If the member is online, kick them from the guild.
		if member:
			member.Write("You have had guild administrator rights taken away.", COLOR_RED)
			self.member_kick(member)

		return True

	## Check if the specified member is a guild administrator.
	## @param name The member name to check.
	## @return True if the member is an administrator, False otherwise.
	def member_is_admin(self, name):
		if not self.member_approved(name):
			return False

		if not self._members[name]["flags"] & self.member_administrator:
			return False

		return True

	## Check if member can pick up the specified object.
	## @param name Member's name.
	## @param obj The object.
	## @return True if the member can pick up the object, False otherwise.
	def member_can_pick(self, name, obj):
		# Get the member's rank.
		rank_name = self.member_get_rank(name)

		if not rank_name:
			return True

		rank = self._ranks[rank_name]

		# Unlimited.
		if rank["value_limit"] == 0:
			return True

		m_time = self._members[name]["value_limit_time"]

		# Is there a timer that was previously added? If so, check whether
		# we should remove it.
		if m_time:
			if self.member_limit_remaining(name) == 0:
				self._db[self._guild]["members"][name]["value_limit_time"] = 0
				self._db[self._guild]["members"][name]["value_used"] = 0

		# Get the object's cost.
		val = obj.GetItemCost(obj, COST_TRUE)

		# Don't allow the object to be picked up if its value and the used
		# up value would go above the limit.
		if val + self._members[name]["value_used"] > rank["value_limit"]:
			return False

		# We passed the above check, add the object's value to the total,
		# and if the timer has not been started yet, start it.
		if val:
			self._db[self._guild]["members"][name]["value_used"] += val

			if not m_time:
				self._db[self._guild]["members"][name]["value_limit_time"] = int(time.time())

		return True

	## Check if a specified member exists.
	## @param name The member to check.
	## @return True if the member exists, False otherwise.
	def member_exists(self, name):
		return name in self._members

	## Set member's rank.
	## @param name Member's name.
	## @param rank Rank to set. If default or None, will clear the member's rank.
	## @return True on success, False on failure.
	def member_set_rank(self, name, rank = None):
		if not self.member_exists(name) or (rank and not self.rank_exists(rank)):
			return False

		self._db[self._guild]["members"][name]["rank"] = rank
		# Reset total used value and time.
		self._db[self._guild]["members"][name]["value_used"] = 0
		self._db[self._guild]["members"][name]["value_limit_time"] = 0

		return True

	## Calculate the number of seconds remaining for the value limit timer.
	## @param name Name of the member.
	## @return Number of seconds remaining.
	def member_limit_remaining(self, name):
		rank = self.member_get_rank(name)

		if not rank or not self._ranks[rank]["value_reset"]:
			return 0

		m_time = self._members[name]["value_limit_time"]

		if not m_time:
			return 0

		# Calculate the remaining time.
		remaining = m_time - int(time.time()) + 60 * 60 * self._ranks[rank]["value_reset"]

		if remaining <= 0:
			return 0

		return remaining

	## Get member's rank.
	## @param name Member's name.
	## @return Member's rank, None if no rank.
	def member_get_rank(self, name):
		if not self.member_exists(name):
			return None

		return self._members[name]["rank"]

	## Construct a string containing information about the specified rank,
	## its members, etc.
	## @param rank The rank.
	## @return The created string, or None if the passed rank doesn't exist.
	def rank_string(self, rank):
		if not self.rank_exists(rank):
			return None

		# Get the value limit.
		limit = self._ranks[rank]["value_limit"]

		# Either create a 1g, 50s etc string or show "unlimited".
		if limit:
			limit = CostString(limit)
		else:
			limit = "unlimited"

		# Construct the actual string.
		return "~{}~\nLimit: {} [reset: {} hour(s)] \nMembers: {}".format(rank, limit, self._ranks[rank]["value_reset"], ", ".join(self.rank_get_members(rank)))

	## Get all the ranks.
	## @return A dictionary of the currently configured ranks.
	def ranks_get(self):
		return self._ranks

	## Get all the ranks, sorted by the highest limit value first.
	## @return A sorted dictionary of the ranks.
	def ranks_get_sorted(self):
		return sorted(self.ranks_get().keys(), key = lambda rank: self._ranks[rank]["value_limit"] == 0 and self.rank_value_max + 1 or self._ranks[rank]["value_limit"], reverse = True)

	## Sanitize a rank name; strips whitespace, ensures the length is
	## valid, etc.
	## @param rank The rank name to sanitize.
	## @return Sanitized rank name, None if the rank name is invalid.
	def rank_sanitize(self, rank):
		# Strip whitespace.
		rank = rank.strip()

		if not rank:
			return None

		# Make sure the rank name has a valid length.
		if len(rank) > self.rank_max_chars:
			return None

		return rank

	## Create a new rank.
	## @param rank The rank name to create.
	## @return True on success, False on failure.
	def rank_add(self, rank):
		if self.rank_exists(rank):
			return False

		# Create the rank.
		self._db[self._guild]["ranks"][rank] = {
			"value_limit": 0,
			"value_reset": self.rank_reset_default,
		}

		return True

	## Remove a rank. Removing all members of the rank is done automatically.
	## @param rank Rank name to remove.
	## @return True on success, False on failure.
	def rank_remove(self, rank):
		if not self.rank_exists(rank):
			return False

		# Remove all members from this rank.
		for name in self.rank_get_members(rank):
			self._db[self._guild]["members"][name]["rank"] = None

		del self._db[self._guild]["ranks"][rank]

		return True

	## Get members of the specified rank.
	## @param rank Rand to get members of.
	## @return A list of the member names in the rank, None if the rank
	## doesn't exist.
	def rank_get_members(self, rank):
		if not self.rank_exists(rank):
			return None

		l = list(filter(lambda name: self._members[name]["rank"] == rank, self._members.keys()))
		l.sort()

		return l

	## Check if the specified rank exists.
	## @param rank The rank name to check.
	## @return True if the rank exists, False otherwise.
	def rank_exists(self, rank):
		return rank in self._ranks

	## Set rank's option.
	## @param rank The rank name.
	## @param what What to set.
	## @value value Value to set.
	## @return True on success, False on failure.
	def rank_set(self, rank, what, value):
		# Make sure the new value is of the same type as the old one.
		if not self.rank_exists(rank) or type(self._ranks[rank][what]) != type(value):
			return False

		self._db[self._guild]["ranks"][rank][what] = value

		return True

	## Get rank's option.
	## @param rank The rank name.
	## @param what What to get.
	## @return The option.
	def rank_get(self, rank, what):
		if not self.rank_exists(rank):
			return None

		return self._ranks[rank][what]

	## Set the guild's founder.
	## @param name The founder's name to set. Can be None.
	## @return True on success, False on failure.
	def set_founder(self, name):
		if name and not self.member_approved(name):
			return False

		self._db[self._guild]["founder"] = name

		return True

	## Check whether the specified member name is the guild's founder.
	## @param name The member's name to check.
	## @return True if the member is the guild's founder, False otherwise.
	def is_founder(self, name):
		if not self.member_approved(name):
			return False

		if self._founder != name:
			return False

		return True

	## Get the guild's founder.
	## @return The founder.
	def get_founder(self):
		return self._founder

	## Set a flag on the guild.
	## @param flag Flag to set, one or a combination of @ref guild_flags.
	def guild_set(self, flag):
		self._db[self._guild]["flags"] |= flag

	## Unset a flag from the guild.
	## @param flag Flag to unset, one or a combination of @ref guild_flags.
	def guild_unset(self, flag):
		self._db[self._guild]["flags"] &= ~flag

	## Check whether the guild has the specified flag set.
	## @param flag Flag to check, one or a combination of @ref guild_flags.
	## @return True if the flag is set, False otherwise.
	def guild_check(self, flag):
		if self._flags & flag:
			return True

		return False

	## Write to the log file.
	## @param s String to add to the log.
	def log_add(self, s):
		from datetime import datetime

		# Try to get the file pointer from the cache.
		try:
			fp = CacheGet("guild_fp_append")
		# Not in cache, make a new one.
		except:
			fp = open(_data_dir + "/" + self._guild + ".log", "a")
			CacheAdd("guild_fp_append", fp)

		fp.write("[" + datetime.now().strftime("%d-%m-%Y %H:%M:%S") + "] " + s + "\n")
