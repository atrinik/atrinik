## @file
## Script for the rocks puzzle in second level of Brynknot Sewers.

from Atrinik import *
import os

me = WhoAmI()

# The rocks.
rocks = [
	["sewers_a_aaab", 13, 13, "brynknot_sewers_a_rock_1"],
	["sewers_a_01ab", 0, 4, "brynknot_sewers_a_rock_2"],
	["sewers_a_02ab", 1, 19, "brynknot_sewers_a_rock_3"],
	["sewers_a_02ab", 0, 9, "brynknot_sewers_a_rock_4"],
]

if GetEventNumber() == EVENT_TIMER:
	for rock in rocks:
		ReadyMap(os.path.dirname(me.map.path) + "/" + rock[0]).Insert(LocateBeacon(rock[3]).env, rock[1], rock[2])

	me.f_splitting = False
else:
	for rock in rocks:
		ReadyMap(os.path.dirname(me.map.path) + "/" + rock[0])

	key = me.FindObject(0, "key2")

	if not key:
		raise Error("Could not find key inside myself.")

	activator = WhoIsActivator()

	if not activator.FindObject(2, "key2", key.name):
		key.Clone().InsertInside(activator)

	if not me.f_splitting:
		me.CreateTimer(1, 2)
		me.f_splitting = True
