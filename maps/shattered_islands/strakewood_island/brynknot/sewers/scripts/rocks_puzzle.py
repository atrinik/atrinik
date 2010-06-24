## @file
## Script for the rocks puzzle in second level of Brynknot Sewers.

from Atrinik import *
import os

rock_beacons = [
	"brynknot_sewers_a_rock_1", "brynknot_sewers_a_rock_2",
	"brynknot_sewers_a_rock_3", "brynknot_sewers_a_rock_4"
]

for beacon_name in rock_beacons:
	beacon = LocateBeacon(beacon_name)

	if not beacon:
		raise Error("Could not find beacon '{0}'.".format(beacon_name))

	(map_name, x, y) = beacon.env.msg.split()
	m = ReadyMap(os.path.dirname(beacon.env.map.path) + "/" + map_name)

	if not m:
		raise Error("Could not load map.")

	m.Insert(beacon.env, int(x), int(y))

key = WhoAmI().CheckInventory(0, "key2")

if not key:
	raise Error("Could not find key inside myself.")

activator = WhoIsActivator()

if not activator.CheckInventory(2, "key2", key.name):
	key.Clone().InsertInside(activator)
