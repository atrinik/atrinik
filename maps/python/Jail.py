## @file
## The Jail class, used for scripts in jails.

from Atrinik import *
import json, os, random, datetime

## The jail class.
class Jail:
	## Initializer.
	## @param me Object that is carrying the event.
	def __init__(self, me):
		# Load up the jails.
		self.jails = json.loads(WhatIsEvent().msg)
		self.me = me

		if not self.jails:
			raise ValueError("Could not load any jails.")

	## Randomly select a jail from the list we have loaded previously.
	## @param Random member of 'self.jails'.
	def select_jail(self):
		return random.choice(self.jails)

	## Try to find a jail force inside player's inventory.
	## @param player Player to look inside.
	## @return The jail force.
	def get_jail_force(self, player):
		return player.FindObject(name = "jail_force")

	## Jail a specified player for the specified amount of seconds.
	## @param player Player to jail.
	## @param time How many seconds to jail the player for.
	## @param check Whether to check if the player is jailed already.
	## @return True on success, False on failure.
	def jail(self, player, time, check = True):
		# Check if this player is already jailed...
		if check and self.get_jail_force(player):
			return False

		# Select a random jail.
		(m, x, y) = self.select_jail()
		# Combine the jail map into a path.
		map_path = os.path.dirname(self.me.map.path) + "/" + m
		# Teleport the player to the map.
		player.TeleportTo(map_path, x, y)
		# Get player's controller, and set their save bed map, so they can't
		# escape by suicide.
		pl = player.Controller()
		pl.savebed_map = map_path
		pl.bed_x = x
		pl.bed_y = y

		# 'time' of 0 means forever.
		if time != 0:
			force = player.CreateForce("jail_force", int(time / 6.25))
			player.Write("You have been jailed for {0} for your actions.".format(datetime.timedelta(seconds = time)), COLOR_RED)
		else:
			force = player.CreateForce("jail_force")
			player.Write("You have been jailed for life for your actions.", COLOR_RED)

		force.slaying = "jail_force"
		return True
