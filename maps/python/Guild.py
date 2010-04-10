## @file
## Python script to be used by other scripts in order to provide guild
## functions like removing/adding members, querying member ranks, etc.

from Atrinik import *
import string, os, shelve

## The Guild class.
##
## Uses the Python shelve module to keep track of guilds and their
## members.
class Guild:
	## Guild database file.
	guilddb_file = "../server/data/guilds"
	## The database dictionary, to be opened and filled with values by
	## Python shelve.
	guilddb = {}
	## Name of the guild we're managing.
	## Passed to by __init__().
	guildname = ""

	## @defgroup guild_flags Guild flags
	## Guild flags.
	## @{

	## No guild flag.
	GUILD_FLAG_NONE = 0
	## The guild is closed. No guild applications are possible.
	GUILD_FLAG_CLOSED = 1
	## @}

	## @defgroup guild_member_flags Guild member flags
	## Guild member flags.
	## @{

	## No guild member flag.
	MEMBER_FLAG_NONE = 0
	## The member is a member of the guild, but has not been approved by
	## a guild administrator, therefore cannot enter guild or participate
	## in guild chats.
	MEMBER_FLAG_REQUESTED = 1
	## The member is a guild administrator.
	MEMBER_FLAG_ADMINISTRATOR = 2
	## @}

	## The constructor.
	##
	## If the guild does not exist yet in the database, initialize a new
	## one.
	## @param guildname Name of the guild we're managing.
	def __init__(self, guildname):
		## Initialize guild database from the database.
		self.guilddb = shelve.open(self.guilddb_file)
		## Initialize guild name from parameters.
		self.guildname = guildname

		# If the guild does not exist yet, initialize a new one.
		if self.guildname != None and not self.guildname in self.guilddb:
			self.guilddb[self.guildname] = {
				"members": {},
				"status": self.GUILD_FLAG_CLOSED,
				"founder": "",
			}

	## Add a member to the guild we're managing, specified in
	## Guild::guildname.
	## @param name Name of the member to add.
	## @param flags Flags for the new member to have. See
	## @ref guild_member_flags
	def add_member(self, name, flags):
		temp = self.guilddb[self.guildname]
		temp["members"][name] = {
			"flags": flags,
		}
		self.guilddb[self.guildname] = temp

	## Remove a member from the guild we're managing, specified by
	## Guild::guildname.
	## @param name Name of the member to remove.
	## @param remover Game object carrying the event object that triggered
	## this.
	## @return True if the member was successfully removed, False
	## otherwise.
	def remove_member(self, name, remover):
		if not name in self.guilddb[self.guildname]["members"]:
			return False

		# Check if this is just a membership request. If so, we don't need to remove
		# them from guild maps.
		requested = self.guilddb[self.guildname]["members"][name]["flags"] & self.MEMBER_FLAG_REQUESTED

		temp = self.guilddb[self.guildname]
		del temp["members"][name]
		self.guilddb[self.guildname] = temp

		if not requested:
			# Try to find the player
			member = FindPlayer(name)

			# If the player is online, remove him from the guild maps.
			# If he is not, this will be taken care of later.
			if member != None:
				member.Write("You have been removed from the guild. Goodbye!", COLOR_RED)
				self.remove_player_from_guild_maps(member, remover)

		return True

	## Approve a member's application to the guild.
	## @param name Name of the member to approve.
	## @return True if the member was approved, False otherwise.
	def approve_member(self, name):
		if not name in self.guilddb[self.guildname]["members"]:
			return False

		temp = self.guilddb[self.guildname]
		temp["members"][name]["flags"] &= ~self.MEMBER_FLAG_REQUESTED
		self.guilddb[self.guildname] = temp

		return True

	## Check if guild member has been approved for full membership.
	## @param name Name of the member.
	## @return True if the member has been approved, False otherwise.
	def is_approved(self, name):
		if not name in self.guilddb[self.guildname]["members"] or self.guilddb[self.guildname]["members"][name]["flags"] & self.MEMBER_FLAG_REQUESTED:
			return False

		return True

	## Remove player from guild maps, if he's online.
	## @param player Player object to remove.
	## @param remover Who is removing the player.
	def remove_player_from_guild_maps(self, player, remover):
		guild_options_object = remover.CheckInventory(0, "note", "guild options")

		if guild_options_object:
			enter_x = enter_y = -1
			enter_map = ""
			guild_maps = []

			for line in guild_options_object.message.splitlines():
				if line.startswith("guild_maps: "):
					guild_maps = line[12:].split("|")
				elif line.startswith("enter_map: "):
					enter_map = line[11:]
				elif line.startswith("enter_x: "):
					enter_x = int(line[9:])
				elif line.startswith("enter_y: "):
					enter_y = int(line[9:])

			if enter_x != -1 and enter_y != -1 and player.map.path in guild_maps:
				if not enter_map:
					enter_map = remover.map.path

				player.TeleportTo(enter_map, enter_x, enter_y)

	## Get a member structure from guild members.
	## @param name Name of the member to look for.
	## @return None if the member is not valid, dictionary of the
	## member's values otherwise.
	def member(self, name):
		if name in self.guilddb[self.guildname]["members"]:
			return self.guilddb[self.guildname]["members"][name]

		return None

	## Check if specified member name is member of specified guild.
	## @param name Name of the member to check for.
	## @param guild Guild name we will be looking in for the member. If
	## None, use guild name from @ref guildname.
	## @return True if the member name is member of the guild, False
	## otherwise.
	def is_member_of(self, name, guild = None):
		# If no guild name passed, use default
		if guild == None:
			guild = self.guildname

		# No such guild or no such member name in our guild database?
		if not guild in self.guilddb or not name in self.guilddb[guild]["members"]:
			return False

		# Members who have not been approved are not counted as members.
		if self.guilddb[guild]["members"][name]["flags"] & self.MEMBER_FLAG_REQUESTED:
			return False

		return True

	## Check if specified member is a member of /any/ guild.
	## @param name Name of the member to check for.
	## @return Guild name the member is in, None otherwise.
	def is_in_guild(self, name):
		for guildname in self.guilddb:
			if name in self.guilddb[guildname]["members"] and self.is_member_of(name, guildname):
				return guildname

		return None

	## Make the specified member name an administrator of the guild
	## specified by @ref guildname.
	## @param name Name of the member to make administrator.
	## @return True if the member was made administrator, False
	## otherwise.
	def make_admin(self, name):
		if not self.is_member_of(name):
			return False

		# Is the member an administrator already?
		if self.guilddb[self.guildname]["members"][name]["flags"] & self.MEMBER_FLAG_ADMINISTRATOR:
			return False

		temp = self.guilddb[self.guildname]
		temp["members"][name]["flags"] |= self.MEMBER_FLAG_ADMINISTRATOR
		self.guilddb[self.guildname] = temp

		return True

	## Remove administrator status from member of the guild specified by
	## @ref guildname.
	## @param name Name of the member to take administrator rights from.
	## @param remover Game object carrying the event object that
	## triggered this.
	## @return True if the member was taken all administrator rights,
	## False otherwise.
	def remove_admin(self, name, remover):
		if not self.is_member_of(name):
			return False

		# Cannot remove administrator status from member who never had
		# it..
		if not self.guilddb[self.guildname]["members"][name]["flags"] & self.MEMBER_FLAG_ADMINISTRATOR:
			return False

		temp = self.guilddb[self.guildname]
		temp["members"][name]["flags"] &= ~self.MEMBER_FLAG_ADMINISTRATOR
		self.guilddb[self.guildname] = temp

		# Try to find the player
		member = FindPlayer(name)

		# If the member is online, and he's in the guild oracle map,
		# remove him from there.
		if member != None:
			member.Write("You have had administrator rights taken away.", COLOR_RED)
			self.remove_player_from_guild_maps(member, remover)

		return True

	## Check if specified member is an administrator in guild specified
	## by @ref guildname.
	## @param name Name of the member to check for.
	## @return True if the member is an administrator, False otherwise.
	def is_administrator(self, name):
		if not self.is_member_of(name):
			return False

		if not self.guilddb[self.guildname]["members"][name]["flags"] & self.MEMBER_FLAG_ADMINISTRATOR:
			return False

		return True

	## Change founder of the guild.
	##
	## Guild founder cannot be removed or have administrator rights taken
	## away by fellow guild administrators.
	## @param name Name of the guild member to make the new founder.
	## @return True if successfully changed the guild founder, False
	## otherwise.
	def make_founder(self, name):
		if name and not self.is_member_of(name):
			return False

		temp = self.guilddb[self.guildname]
		temp["founder"] = name
		self.guilddb[self.guildname] = temp

		return True

	## Check if specified member is the guild founder in guild specified
	## by @ref guildname.
	## @param name Name of the member to check for.
	## @return True if the member is the guild founder, False otherwise.
	def is_founder(self, name):
		if not self.is_member_of(name):
			return False

		if self.guilddb[self.guildname]["founder"] != name:
			return False

		return True

	## Get name of the guild's founder.
	## @return Guild's founder.
	def get_founder(self):
		return self.guilddb[self.guildname]["founder"]

	## Open the guild we're managing specified by @ref guildname.
	def open_guild(self):
		temp = self.guilddb[self.guildname]
		temp["status"] &= ~self.GUILD_FLAG_CLOSED
		self.guilddb[self.guildname] = temp

	## Close the guild we're managing specified by @ref guildname.
	def close_guild(self):
		temp = self.guilddb[self.guildname]
		temp["status"] |= self.GUILD_FLAG_CLOSED
		self.guilddb[self.guildname] = temp

	## Checks if the guild specified by @ref guildname is closed.
	## @return True if the guild is closed, False otherwise.
	def is_closed(self):
		if self.guilddb[self.guildname]["status"] & self.GUILD_FLAG_CLOSED:
			return True

		return False
