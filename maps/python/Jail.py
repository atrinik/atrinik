## @file
## The Jail class, used for scripts in jails.

import json
import random
import datetime

from Atrinik import *


## Try to find a jail force inside player's inventory.
## @param player Player to look inside.
## @return The jail force.
def get_jail_force(player):
    return player.FindObject(name = "jail_force")

## Figure out how much time the player has left in the jail.
## @param player Player.
## @return String representing the amount of time, None if not jailed.
def get_jail_time(player):
    force = get_jail_force(player)

    if not force:
        return None

    return datetime.timedelta(seconds = int(force.food * 6.25))

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

    ## Jail a specified player for the specified amount of seconds.
    ## @param player Player to jail.
    ## @param time How many seconds to jail the player for.
    ## @param check Whether to check if the player is jailed already.
    ## @return True on success, False on failure.
    def jail(self, player, time, check = True):
        # Check if this player is already jailed...
        if check and get_jail_force(player):
            return False

        # Select a random jail.
        (m, x, y) = self.select_jail()
        map_path = self.me.map.GetPath(m)
        # Teleport the player to the jail map.
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
            pl.DrawInfo("You have been jailed for {0} for your actions.".format(datetime.timedelta(seconds = time)), COLOR_RED)
        else:
            force = player.CreateForce("jail_force")
            pl.DrawInfo("You have been jailed for life for your actions.", COLOR_RED)

        force.slaying = "jail_force"
        return True
