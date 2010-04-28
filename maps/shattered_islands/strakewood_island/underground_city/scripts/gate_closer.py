## @file
## Script for the gate closer monster.

from Atrinik import *

# Find beacon for the gate's switch.
beacon_name = "underground_city_a_0602_switch"
beacon = LocateBeacon(beacon_name)

# No beacon?
if not beacon:
	raise error("Could not find beacon named '" + beacon_name + "'.")

# Make the waypoint apply the switch, so the click sound is not heard.
WhoAmI().Apply(beacon.environment, APPLY_TOGGLE)
