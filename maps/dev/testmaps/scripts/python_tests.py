## @file
## Used to test Python functions.

from Atrinik import *

activator = WhoIsActivator()
me = WhoAmI()

event_num = GetEventNumber()

def main_timer():
	pl = FindPlayer(me.ReadKey("timer_player"))

	if pl:
		pl.Write("5 seconds are up!")

	me.WriteKey("timer_player")

def main_say():
	msg = WhatIsMessage().lower()
	words = msg.split()

	## Find a marked object.
	marked = activator.FindMarkedObject()

	if msg == "hello" or msg == "hey" or msg == "hi":
		me.SayTo(activator, "\nAvailable tests:\n^equipment EQUIPMENT^, ^get god^, ^set god^\n^create object inside^, ^apply object^\n^drop and pickup^, ^get object name^\n^get gender^, ^set gender GENDER^\n^rank^, ^alignment^\n^get key^, ^add key^, ^delete key^\n^sound^, ^savebed^, ^book^, ^ip^, ^exception^\n^player exists PLAYER^, ^find player PLAYER^\n^beacon BEACON^, ^timer^, ^compare^, ^region^")

	# GetEquipment is much more efficient than looping player's inventory for
	# applied equipment.
	elif words[0] == "equipment" and len(words) > 1:
		if words[1] == "armor":
			if activator.GetEquipment(PLAYER_EQUIP_MAIL):
				me.SayTo(activator, "\nYour armor is '{0}'.".format(activator.GetEquipment(PLAYER_EQUIP_MAIL).GetName()))
			else:
				me.SayTo(activator, "\nYou are not wearing any armor.")
		elif words[1] == "weapon":
			if activator.GetEquipment(PLAYER_EQUIP_WEAPON):
				me.SayTo(activator, "\nYour weapon is '{0}'.".format(activator.GetEquipment(PLAYER_EQUIP_WEAPON).GetName()))
			else:
				me.SayTo(activator, "\nYou are not wielding any weapon.")
		elif words[1].isdigit():
			if activator.GetEquipment(int(words[1])):
				me.SayTo(activator, "\nYour equipment on slot {0} is '{1}'.".format(words[1], activator.GetEquipment(int(words[1])).GetName()))
			else:
				me.SayTo(activator, "\nYou have no equipment on slot {0}.".format(words[1]))
		else:
			me.SayTo(activator, "\nTry ^equipment armor^, ^equipment weapon^ or ^equipment 4^.")

	# Get player's god.
	elif msg == "get god":
		me.SayTo(activator, "\nYour god is '{0}'.".format(activator.GetGod()))

	# An example of using SetGod().
	elif msg == "set god":
		if activator.GetGod() == "Tabernacle":
			activator.SetGod("Moroch")
		elif activator.GetGod() == "Moroch":
			activator.SetGod("Tabernacle")

		me.SayTo(activator, "\nYou are now a follower of {0}.".format(activator.GetGod()))

	# An example of CreateObjectInside().
	elif msg == "create object inside":
		activator.Write("You receive {0}.".format(activator.CreateObjectInside("shortsword", 1, 1).GetName()))

	# Example of using Apply().
	elif msg == "apply object":
		if not marked:
			me.SayTo(activator, "\nYou need to mark an object to apply.")
		else:
			activator.Apply(marked, APPLY_TOGGLE)

	# Example of both Drop() and PickUp().
	elif msg == "drop and pickup":
		if not marked:
			me.SayTo(activator, "\nYou need to mark an object.")
		else:
			activator.Write("Dropping {0}.".format(marked.GetName()))
			activator.Drop(marked)
			activator.Write("Picking up {0}.".format(marked.GetName()))
			activator.PickUp(marked)

	# An example of GetName().
	elif msg == "get object name":
		if not marked:
			me.SayTo(activator, "\nYou need to mark an object.")
		else:
			me.SayTo(activator, "\nYour marked object is:\n{0}".format(marked.GetName()))

	# An example of using GetGender().
	elif msg == "get gender":
		if activator.GetGender() == MALE:
			me.SayTo(activator, "\nYour gender is: male.")
		elif activator.GetGender() == FEMALE:
			me.SayTo(activator, "\nYour gender is: female.")
		elif activator.GetGender() == NEUTER:
			me.SayTo(activator, "\nYour gender is: neuter.")
		elif activator.GetGender() == HERMAPHRODITE:
			me.SayTo(activator, "\nYour gender is: hermaphrodite.")

		# Find out the gender ID.
		gender = activator.GetGender()
		me.SayTo(activator, "\nNoun: {0}\nSubjective: {1}\nSubjective (upper): {2}\nObjective: {3}\nPossessive: {4}\nReflexive: {5}".format(GetGenderStr(gender, "noun"), GetGenderStr(gender, "subjective"), GetGenderStr(gender, "subjective_upper"), GetGenderStr(gender, "objective"), GetGenderStr(gender, "possessive"), GetGenderStr(gender, "reflexive")), 1)
		me.SayTo(activator, "\nNoun: {0}\nSubjective: {1}\nSubjective (upper): {2}\nObjective: {3}\nPossessive: {4}\nReflexive: {5}".format(GetGenderStr(-1, "noun"), GetGenderStr(-1, "subjective"), GetGenderStr(-1, "subjective_upper"), GetGenderStr(-1, "objective"), GetGenderStr(-1, "possessive"), GetGenderStr(-1, "reflexive")), 1)

	# An example of using SetGender().
	elif words[0] == "set" and words[1] == "gender" and len(words) > 2:
		if words[2] == "male":
			activator.SetGender(MALE)
			me.SayTo(activator, "\nYour gender is now male.")
		elif words[2] == "female":
			activator.SetGender(FEMALE)
			me.SayTo(activator, "\nYour gender is now female.")
		elif words[2] == "neuter":
			activator.SetGender(NEUTER)
			me.SayTo(activator, "\nYour gender is now neuter.")
		elif words[2] == "hermaphrodite":
			activator.SetGender(HERMAPHRODITE)
			me.SayTo(activator, "\nYour gender is now hermaphrodite.")
		else:
			me.SayTo(activator, "\nUnknown gender. Try one of:\n^set gender male^, ^set gender female^, ^set gender neuter^ or ^set gender hermaphrodite^.")

	# An example of GetRank() and SetRank().
	elif msg == "rank":
		if not activator.GetRank():
			activator.SetRank("President")
		else:
			# "Mr" effectively clears the player's rank.
			activator.SetRank("Mr")

	# Example of using GetAlignmentForce() and GetAlignment().
	elif msg == "alignment":
		if activator.GetAlignmentForce().title == "true neutral":
			activator.SetAlignment("chaotic evil")
			me.SayTo(activator, "\nChanged your alignment to chaotic evil.")
		else:
			activator.SetAlignment("true neutral")
			me.SayTo(activator, "\nChanged your alignment to true neutral.")

	# Example usage of key values.
	elif (words[0] == "get" or words[0] == "add" or words[0] == "delete") and words[1] == "key":
		if not marked:
			me.SayTo(activator, "\nYou need to mark an object.")
		else:
			if words[0] == "get":
				if marked.ReadKey("custom_field"):
					me.SayTo(activator, "\nMarked object's 'custom_field' key value: {0}".format(marked.ReadKey("custom_field")))
				else:
					me.SayTo(activator, "\nMarked object does not have 'custom_field' key.")
			elif words[0] == "add":
				if marked.WriteKey("custom_field", "test string"):
					me.SayTo(activator, "\nSuccessfully set marked object's 'custom_field' key.")
				else:
					me.SayTo(activator, "\nFailed to set marked object's 'custom_field' key.")
			elif words[0] == "delete":
				if marked.WriteKey("custom_field"):
					me.SayTo(activator, "\nSuccessfully removed marked object's 'custom_field' key.")
				else:
					me.SayTo(activator, "\nFailed to remove marked object's 'custom_field' key.")

	# Example of using PlaySound().
	elif msg == "sound":
		me.SayTo(activator, "\nI can imitate a teleporter.")
		me.map.PlaySound(me.x, me.y, SOUND_TELEPORT)

	# Example of using GetSaveBed().
	elif msg == "savebed":
		savebed = activator.GetSaveBed()
		me.SayTo(activator, "\nYour save bed map: {0} ({1}, {2})".format(savebed["map"], savebed["x"], savebed["y"]))

	# An example showing how to send custom binary commands to the client.
	elif msg == "book":
		me.SayTo(activator, "\nOpening a fake book.")
		activator.SendCustomCommand(30, "book <b t=\"Fake Book\"><t t=\"A Fake Book\">This is a fake book, made using the SendCustomCommand() Python function.\n")

	elif msg == "ip":
		me.SayTo(activator, "\nYour IP is: {0}".format(activator.Controller().s_host))

	elif msg == "exception":
		me.SayTo(activator, "\nI will now raise an exception...")
		raise error("Error Message")

	elif words[0] == "player" and words[1] == "exists" and len(words) > 2:
		if PlayerExists(words[2]):
			me.SayTo(activator, "\nPlayer '{0}' exists.".format(words[2]))
		else:
			me.SayTo(activator, "\nPlayer '{1}' does NOT exist.".format(words[2]))

	elif words[0] == "find" and words[1] == "player" and len(words) > 2:
		player = FindPlayer(words[2])

		if not player:
			me.SayTo(activator, "\nCould not find player {0}.".format(words[2]))
		else:
			me.SayTo(activator, "\n{0} is on map: {1} ({2}, {3})".format(words[2], player.map.path, player.x, player.y))

	elif words[0] == "beacon" and len(words) > 1:
		beacon = LocateBeacon(words[1])

		if not beacon:
			me.SayTo(activator, "\nCould not find beacon '{0}'.".format(words[1]))
		else:
			me.SayTo(activator, "\nFound beacon '{0}':".format(words[1]))

			if beacon.env:
				me.SayTo(activator, "In inventory of '{0}'.".format(beacon.env.name, 1))
			else:
				me.SayTo(activator, "On map '{0}' ({1}, {2}).".format(beacon.map.path, beacon.x, beacon.y), 1)

	elif msg == "timer":
		if me.ReadKey("timer_player"):
			me.SayTo(activator, "\nAlready talking to someone...")
		else:
			me.WriteKey("timer_player", activator.name)
			me.CreateTimer(5, 1)
			me.SayTo(activator, "\nOK! Will tell you when 5 seconds pass.")

	elif msg == "compare":
		me.SayTo(activator, "\nComparing some objects...\n")
		me.SayTo(activator, "{0} {1} {2}".format(activator.name, activator == me and "==" or "!=", me.name), 1)

		obj = activator.map.GetLastObject(activator.x, activator.y)

		while obj:
			me.SayTo(activator, "{0} {1} {2}".format(activator.name, activator == obj and "==" or "!=", obj.name), 1)
			obj = obj.above

		me.SayTo(activator, "\nComparing some maps...\n", 1)
		me.SayTo(activator, "{0} {1} {2}".format(activator.map.path, activator.map == me.map and "==" or "!=", me.map.path), 1)
		emergency_map = ReadyMap("/emergency")
		me.SayTo(activator, "{0} {1} {2}".format(activator.map.path, activator.map == emergency_map and "==" or "!=", emergency_map.path), 1)

		me.SayTo(activator, "\nComparing some parties...\n", 1)
		party1 = activator.Controller().party
		party2 = activator.Controller().party

		if not party1:
			me.SayTo(activator, "You need to join a party to run this test.", 1)
		else:
			me.SayTo(activator, "{0} {1} {2}".format(party1.name, party1 == party2 and "==" or "!=", party2.name), 1)

		me.SayTo(activator, "\nComparing some regions...\n", 1)
		map = ReadyMap("/hall_of_dms")
		region1 = map.region
		region2 = map.region
		me.SayTo(activator, "{0} {1} {2}".format(region1.name, region1 == region2 and "==" or "!=", region2.name), 1)

	elif msg == "region":
		map = ReadyMap("/hall_of_dms")
		me.SayTo(activator, "\nMap '{0}' is in region '{1}' with message:\n{2}".format(map.path, map.region.name, map.region.msg))

if event_num == EVENT_TIMER:
	main_timer()
elif event_num == EVENT_SAY:
	main_say()
