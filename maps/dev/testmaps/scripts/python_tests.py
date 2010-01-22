## @file
## Used to test Python functions.

from Atrinik import *

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

event_num = GetEventNumber()

if event_num == EVENT_SAY:
	msg = WhatIsMessage().lower()
	words = msg.split()

	## Find a marked object.
	marked = activator.FindMarkedObject()

if event_num == EVENT_TIMER:
	pl = FindPlayer(me.ReadKey("timer_player"))

	if pl:
		pl.Write("5 seconds are up!")

	me.WriteKey("timer_player")

# GetEquipment is much more efficient than looping player's inventory for
# applied equipment.
elif words[0] == "equipment" and len(words) > 1:
	if words[1] == "armor":
		if activator.GetEquipment(PLAYER_EQUIP_MAIL):
			me.SayTo(activator, "\nYour armor is '%s'." % activator.GetEquipment(PLAYER_EQUIP_MAIL).GetName())
		else:
			me.SayTo(activator, "\nYou are not wearing any armor.")
	elif words[1] == "weapon":
		if activator.GetEquipment(PLAYER_EQUIP_WEAPON):
			me.SayTo(activator, "\nYour weapon is '%s'." % activator.GetEquipment(PLAYER_EQUIP_WEAPON).GetName())
		else:
			me.SayTo(activator, "\nYou are not wielding any weapon.")
	elif words[1].isdigit():
		if activator.GetEquipment(int(words[1])):
			me.SayTo(activator, "\nYour equipment on slot %s is '%s'." % (words[1], activator.GetEquipment(int(words[1])).GetName()))
		else:
			me.SayTo(activator, "\nYou have no equipment on slot %s." % words[1])
	else:
		me.SayTo(activator, "\nTry ^equipment armor^, ^equipment weapon^ or ^equipment 4^.")

# Get player's god.
elif msg == "get god":
	me.SayTo(activator, "\nYour god is '%s'." % activator.GetGod())

# An example of using SetGod().
elif msg == "set god":
	if activator.GetGod() == "Tabernacle":
		activator.SetGod("Moroch")
	elif activator.GetGod() == "Moroch":
		activator.SetGod("Tabernacle")

	me.SayTo(activator, "\nYou are now a follower of %s." % activator.GetGod())

# An example of CreateObjectInside().
elif msg == "create object inside":
	activator.Write("You receive %s." % activator.CreateObjectInside("shortsword", 1, 1).GetName())

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
		activator.Write("Dropping %s." % marked.GetName())
		activator.Drop(marked)
		activator.Write("Picking up %s." % marked.GetName())
		activator.PickUp(marked)

# An example of GetName().
elif msg == "get object name":
	if not marked:
		me.SayTo(activator, "\nYou need to mark an object.")
	else:
		me.SayTo(activator, "\nYour marked object is:\n%s" % marked.GetName())

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
				me.SayTo(activator, "\nMarked object's 'custom_field' key value: %s" % marked.ReadKey("custom_field"))
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
	me.map.PlaySound(me.x, me.y, 32, 0)

# Example of using GetSaveBed().
elif msg == "savebed":
	savebed = activator.GetSaveBed()
	me.SayTo(activator, "\nYour save bed map: %s (%d, %d)" % (savebed["map"], savebed["x"], savebed["y"]))

# An example showing how to send custom binary commands to the client.
elif msg == "book":
	me.SayTo(activator, "\nOpening a fake book.")
	activator.SendCustomCommand(30, "book <b t=\"Fake Book\"><t t=\"A Fake Book\">This is a fake book, made using the SendCustomCommand() Python function.\n")

# Example usage of GetIP().
elif msg == "ip":
	me.SayTo(activator, "\nYour IP is: %s" % activator.GetIP())

elif msg == "exception":
	me.SayTo(activator, "\nI will now raise an exception...")
	raise error("Error Message")

elif words[0] == "player" and words[1] == "exists" and len(words) > 2:
	if PlayerExists(words[2]):
		me.SayTo(activator, "\nPlayer '%s' exists." % words[2])
	else:
		me.SayTo(activator, "\nPlayer '%s' does NOT exist." % words[2])

elif words[0] == "find" and words[1] == "player" and len(words) > 2:
	player = FindPlayer(words[2])

	if not player:
		me.SayTo(activator, "\nCould not find player %s." % words[2])
	else:
		me.SayTo(activator, "\n%s is on map: %s (%d, %d)" % (words[2], player.map.path, player.x, player.y))

elif words[0] == "beacon" and len(words) > 1:
	beacon = LocateBeacon(words[1])

	if not beacon:
		me.SayTo(activator, "\nCould not find beacon '%s'." % words[1])
	else:
		me.SayTo(activator, "\nFound beacon '%s':" % words[1])

		if beacon.environment:
			me.SayTo(activator, "In inventory of '%s'." % beacon.environment.name, 1)
		else:
			me.SayTo(activator, "On map '%s' (%d, %d)." % (beacon.map.path, beacon.x, beacon.y), 1)

elif msg == "timer":
	if me.ReadKey("timer_player"):
		me.SayTo(activator, "\nAlready talking to someone...")
	else:
		me.WriteKey("timer_player", activator.name)
		me.CreateTimer(5, 1)
		me.SayTo(activator, "\nOK! Will tell you when 5 seconds pass.")

else:
	me.SayTo(activator, "\nAvailable tests:\n^equipment EQUIPMENT^, ^get god^, ^set god^\n^create object inside^, ^apply object^\n^drop and pickup^, ^get object name^\n^get gender^, ^set gender GENDER^\n^rank^, ^alignment^\n^get key^, ^add key^, ^delete key^\n^sound^, ^savebed^, ^book^, ^ip^, ^exception^\n^player exists PLAYER^, ^find player PLAYER^\n^beacon BEACON^, ^timer^")
