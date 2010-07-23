## @file
## Implements the House class.

import json, time

## The House class, used for luxury houses.
class House:
	## The available houses.
	_houses = {
		"greyton": [
			1500000,
			"Greyton Luxury House",
			5000,
			120,
			10,
			("/shattered_islands/strakewood_island/greyton/house/luxury_house", 11, 26, 1),
			("/shattered_islands/strakewood_island/greyton/house/luxury_house", 45, 26, 1),
			("/shattered_islands/world_1111", 14, 15, 0),
			(11, 23),
			4,
			(11, 24),
		],
	}

	## Information about Everlink.
	_everlink = [
		("/shattered_islands/everlink/everlink_0102", 6, 7, 0),
	]

	## @defgroup house_info House information
	## Information about houses.
	## @{

	## Get the cost as an integer.
	cost = 0
	## Get the name of the house.
	name = 1
	## Daily fee.
	fee = 2
	## Maximum number of days for prepaid fees.
	fees_max_days = 3
	## Number of days paid in advance when the house is purchased.
	fees_prepaid = 4
	## Get path to the house in a tuple of (path, x, y).
	portal_house = 5
	## Get path where to teleport the player in the house when leaving
	## Everlink in a tuple of (path, x, y).
	portal_from_everlink = 6
	## Get path where to teleport the player when leaving the house map in
	## a tuple of (path, x, y).
	portal_out = 7
	## X/Y position in a tuple where to send stray items in the house map
	## (also this square will be ignored when looking for stray items).
	gardener_pos = 8
	## How far around to look each time the gardener moves.
	gardener_around = 9
	## Map position where to transfer player if they try to enter the house
	## (or leave it) and they haven't paid their fees and don't have enough
	## in bank.
	fees_unpaid = 10
	## @}

	## @defgroup everlink_info Everlink information
	## Information about Everlink.
	## @{

	## Get path to Everlink in a tuple of (path, x, y).
	everlink_pos = 0
	## @}

	## Name of the player info object used for storing information about
	## purchased houses.
	_pinfo_tag = "luxury_house"

	## The initializer.
	## @param activatorPlayer object that activated the script.
	## @param house House in question, can be changed with House::set_house().
	def __init__(self, activator, house):
		self._activator = activator
		self._house = house
		self._player_info = None
		self._player_houses = []

	## Change the house ID we're working on.
	## @param house The house name to change to.
	def set_house(self, house):
		self._house = house

	## Get the house that we're working on.
	## @return The house name.
	def get_house(self):
		return self._house

	## Get information about the current house.
	## @param setting What kind of information to get, one of @ref house_info.
	def get(self, setting):
		return self._houses[self._house][setting]

	## Get information about Everlink.
	## @param setting What Everlink information to get, one of @ref everlink_info.
	def get_everlink(self, setting):
		return self._everlink[setting]

	## Load the player info object that stores information about player's
	## purchased houses.
	##
	## If the player info object doesn't exist, it will be created.
	## @param create If False, will not create a new player info if it
	## doesn't exist.
	def _load_player_info(self, create = True):
		# Try to find the player info.
		self._player_info = self._activator.GetPlayerInfo(self._pinfo_tag)

		# It was not found, so create a new one.
		if not self._player_info:
			if create:
				self._player_info = self._activator.CreatePlayerInfo(self._pinfo_tag)
				self._player_info.msg = json.dumps(self._player_houses)
		else:
			self._player_houses = json.loads(self._player_info.msg)

	def _save_player_info(self):
		self._player_info.msg = json.dumps(self._player_houses)

	## Get last house information stored for the specified player by a
	## previous call to to House::set_last_house().
	## @return The last house information.
	def get_last_house(self):
		if not self._player_info:
			self._load_player_info(False)

		if not self._player_info:
			return None

		return self._player_info.slaying

	## Set information about the last house that the player left, which
	## can then be retrieved by House::set_last_house().
	## @param house The string to set.
	def set_last_house(self, house):
		if not self._player_info:
			self._load_player_info(False)

		self._player_info.slaying = house

	## Get the houses player has previously purchased.
	## @return A list of the purchased houses.
	def get_houses(self):
		if not self._player_info:
			self._load_player_info(False)

		return self._player_houses

	## Add an entry to player's list of purchased houses. If player doesn't
	## have player info tag about the houses yet, it will be created.
	def add_house(self):
		if not self._player_info:
			self._load_player_info()

		self._player_houses.append([self._house, int(time.time())])
		# Will also save the player info.
		self.fees_add(self.get(self.fees_prepaid))

	## Find out ID of a house in player's houses.
	##
	## @note House::_load_player_info() should have been called before.
	## @return -1 if the house was not found, the ID of the house otherwise.
	def _find_house(self):
		for i, house in enumerate(self._player_houses):
			if self._house == house[0]:
				return i

		return -1

	## Check if player owns the house we're working on.
	## @return True if the player owns the house, False otherwise.
	def has_house(self):
		if not self._player_info:
			self._load_player_info(False)

		if self._player_info and self._find_house() != -1:
			return True

		return False

	## Check if player's fees for the working house have expired.
	## @return True if they have expired, False otherwise.
	def fees_expired(self):
		if not self._player_info:
			self._load_player_info()

		i = self._find_house()

		if i == -1:
			return

		return int(time.time()) > self._player_houses[i][1]

	## Reset the timestamp of the fees expiration to the current time.
	def fees_reset(self):
		if not self._player_info:
			self._load_player_info()

		i = self._find_house()

		if i == -1:
			return

		self._player_houses[i][1] = int(time.time())
		self._save_player_info()

	## Add the specified number of days to the timestamp when fees expire.
	## @param days How many days to add, defaults to 1.
	def fees_add(self, days = 1):
		if not self._player_info:
			self._load_player_info()

		i = self._find_house()

		if i == -1:
			return

		self._player_houses[i][1] += 60 * 60 * 24 * days
		self._save_player_info()
