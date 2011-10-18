## @file
## Script for the torches in the first room of Underground City II.

import threading, os.path

def main():
	# The effect is going on, do not allow apply the torches.
	if me.f_splitting:
		SetReturnValue(1)
		return

	# Lighting up the torch, not much to do.
	if not me.glow_radius:
		return

	## Locate the torches.
	def locate_torches():
		beacons = ["uc_torch_1", "uc_torch_2", "uc_torch_3", "uc_torch_4"]

		for beacon in beacons:
			yield LocateBeacon(beacon).env

	torches = []

	# Locate the torches and add them to the 'torches' list. Also checks
	# whether all the torches are extinguished, and if not, exits.
	for torch in locate_torches():
		if torch != me and torch.glow_radius:
			return

		torches.append(torch)

	# All the torches are extinguished, set the flag that will disable
	# players from applying them for the duration of the effect.
	for torch in torches:
		torch.f_splitting = True

	## The threading timer function for the effect.
	## @param progress Progress in the effect.
	def timer(progress):
		if progress == 0 and activator:
			activator.Controller().Sound("gate_open.ogg")
			activator.Write("You hear the sound of old gears turning...", COLOR_YELLOW)

		# Go through the torches.
		for torch in torches:
			# Light/extinguish the torch.
			torch.Apply(torch, APPLY_NO_EVENT)

			# If the effect is ending, make the torches applyable again.
			if progress == 6:
				torch.f_splitting = False

		if progress == 6:
			return

		t = threading.Timer(0.5, timer, [progress + 1])
		t.start()

	# Make sure the map with the gate is in memory.
	ReadyMap(os.path.dirname(me.map.path) + "/underground_city_a_0602")
	# Apply the switch that opens the gate.
	me.Apply(LocateBeacon("uc_torch_switch").env, APPLY_TOGGLE)

	# Find the gate closer spawn point.
	beacon = LocateBeacon("uc_torch_gate_closer")

	# If the gate closer spawn point has the monster spawned, make it go
	# close the gate.
	if beacon.env.enemy:
		beacon.env.enemy.FindObject(archname = "waypoint", name = "wp1").f_cursed = True

	# Start the torches effect.
	t = threading.Timer(0.5, timer, [0])
	t.start()

main()
