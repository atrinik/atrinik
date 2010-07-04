## @file
## Script to control torches in the first room of Underground City
## level 2.
##
## Handles both applying the torches and timer event.

from Atrinik import *
import os

activator = WhoIsActivator()
me = WhoAmI()

## How many torches we need to extinguish.
num_torches = 4
## Map with the gate.
gate_map = "underground_city_a_0602"
## Map with the torches.
torch_map = "underground_city_a_0501"

## Locate torches.
## @param ignore If True, ignore the torch we're applying.
## @return List of the torches.
def locate_torches(ignore = False):
	torches = []

	for i in range(1, num_torches + 1):
		# Food marks the ID of the torch.
		if ignore and i == me.food:
			continue

		# Now find the beacon for the torch
		beacon_name = torch_map + "_torch_" + str(i)
		torch_beacon = LocateBeacon(beacon_name)

		# If we couldn't find the beacon, raise an error.
		if not torch_beacon:
			raise error("Could not find beacon named '" + beacon_name + "'.")

		torches.append(torch_beacon.env)

	return torches

# Timer event, for making the gate closer go close the gate,
# and to create effect with the lights.
if GetEventNumber() == EVENT_TIMER:
	# The first timer event, so make the gate closer close the gate.
	if not me.last_heal:
		beacon_name = gate_map + "_skelly"
		skelly_beacon = LocateBeacon(beacon_name)

		# No beacon?
		if not skelly_beacon:
			raise error("Could not find beacon named '" + beacon_name + "'.")

		# If we found the spawn point and the spawn point has something spawned...
		if skelly_beacon.env and skelly_beacon.env.enemy:
			# Find the monster's starting waypoint
			wp = skelly_beacon.env.enemy.CheckInventory(0, "waypoint", "wp1")

			if not wp:
				raise error("Gate closer monster is missing waypoint.")

			# This activates the waypoint.
			wp.f_cursed = True

	# Re-create the timer a few times for the effect.
	if me.last_heal < 4:
		me.last_heal += 1
		me.CreateTimer(4, 2)
	else:
		me.last_heal = 0

	# Find the torches.
	torches = locate_torches()

	for torch in torches:
		# We apply the torch, but to save a bit of CPU time, we do not execute apply event.
		me.Apply(torch, APPLY_NO_EVENT)

		# If we are finishing the effect, make the torch possible to apply again.
		if not me.last_heal:
			torch.f_splitting = False

# Splitting is used to mark that an effect is going on and we
# should not be able to apply them.
elif me.f_splitting:
	SetReturnValue(1)
else:
	## How many torches have been extinguished.
	num_unlit = 0

	# Are we extinguishing this one too?
	if me.glow_radius:
		num_unlit += 1

	# Locate the torches, ignoring the one we're applying.
	torches = locate_torches(True)

	for torch in torches:
		# If it has glow_radius, it means it's lit.
		if not torch.glow_radius:
			num_unlit += 1

	# Are all the torches extinguished?
	if num_unlit == num_torches:
		# Ready the map.
		ReadyMap(os.path.dirname(me.map.path) + "/" + gate_map)

		# Now find beacon for the gate's switch.
		beacon_name = gate_map + "_switch"
		switch_beacon = LocateBeacon(beacon_name)

		# Raise an error if the beacon was not found.
		if not switch_beacon:
			raise error("Could not find beacon named '" + beacon_name + "'.")

		# Apply the switch.
		switch_beacon.env.Apply(switch_beacon.env, APPLY_TOGGLE)

		# Give the player a hint...
		activator.Sound(SOUND_GATE_OPEN)
		activator.Write("You hear the sound of old gears turning...", COLOR_YELLOW)

		# Make all torches impossible to apply until the effect finishes.
		me.f_splitting = True

		for torch in torches:
			torch.f_splitting = True

		# Create a timer.
		me.CreateTimer(4, 2)
	else:
		# Drop the player a hint that extinguishing the torch did something.
		activator.Sound(SOUND_TURN_HANDLE)
