## @file
## Quest for power crystal, given by Gandyld.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()

msg = WhatIsMessage().strip().lower()
player_info_name = "gandyld_mana_crystal"
player_info = activator.GetPlayerInfo(player_info_name)
crystal_arch = "power_crystal"
crystal_name = "Gandyld's Mana Crystal"

## Load the quests and QuestManagers.
def load_quests():
	global qm, qm2

	quests = [
	{
		"quest_name": "Gandyld's Quest",
		"type": QUEST_TYPE_KILL,
		"kills": 1,
		"message": "Find and kill a purple worm in Old Outpost and then return to Gandyld, east of Aris.",
	},
	{
		"quest_name": "Gandyld's Quest II",
		"type": QUEST_TYPE_KILL,
		"kills": 1,
		"message": "Find and kill King Rhun at the end of Old Outpost and then return to Gandyld, east of Aris.",
	}]

	qm = QuestManager(activator, quests[0])
	qm2 = QuestManager(activator, quests[1])

## Create the crystal.
def create_crystal():
	crystal = activator.CreateObjectInside(crystal_arch, 1, 1)
	crystal.name = crystal_name
	# 1 gold
	crystal.value = 10000

	# Figure out how much capacity to give...
	if qm2.completed():
		crystal.maxsp = 200
	elif qm.completed():
		crystal.maxsp = 100
	else:
		crystal.maxsp = 50

	# So that it will disappear if we drop it
	crystal.f_startequip = True
	activator.Write("{0} hands you a shining mana crystal.".format(me.name), COLOR_GREEN)

## Upgrade an existing crystal.
def upgrade_crystal(crystal, capacity):
	me.SayTo(activator, "You have done it! Now allow me to boost your mana crystal...", 1)
	activator.Write("{0} casts some strange magic...".format(me.name), COLOR_BLUE)
	crystal.maxsp = capacity

if msg == "hello" or msg == "hi" or msg == "hey":
	if not player_info:
		activator.Write("\nThe old mage {0} mumbles something and slowly turns his attention to you.".format(me.name))
		me.SayTo(activator, "\nWhat is it? Can't you see I'm ^busy^ here?")
	else:
		crystal = activator.CheckInventory(2, crystal_arch, crystal_name)
		load_quests()

		if not crystal:
			me.SayTo(activator, "\nYou lost the mana crystal?! You're in luck, I have a spare one right here...")
			create_crystal()
		else:
			me.SayTo(activator, "\nHello again, {0}.".format(activator.name))

			# First quest
			if not qm.completed():
				if not qm.started():
					me.SayTo(activator, "Would you be interested in ^boosting^ your mana crystal?", 1)
				elif qm.finished():
					upgrade_crystal(crystal, 100)
					qm.complete()
				else:
					me.SayTo(activator, "Find and kill a purple worm in Old Outpost north of here in the Giant Mountains..", 1)
			# Second quest
			elif not qm2.completed():
				if not qm2.started():
					me.SayTo(activator, "Would you be interested in ^boosting^ your mana crystal even further?", 1)
				elif qm2.finished():
					upgrade_crystal(crystal, 200)
					qm2.complete()
				else:
					me.SayTo(activator, "Find and kill King Rhun at the end of Old Outpost, located north of here in the Giant Mountains.", 1)
			else:
				me.SayTo(activator, "Sorry, I cannot upgrade your mana crystal any further.", 1)

# Accept one of the quests
elif msg == "boosting":
	load_quests()

	if not qm.completed():
		if not qm.started():
			me.SayTo(activator, "\nFind and kill a purple worm in Old Outpost north of here in the Giant Mountains.")
			qm.start()
	elif not qm2.completed():
		if not qm2.started():
			me.SayTo(activator, "\nFind and kill King Rhun at the end of Old Outpost, located north of here in the Giant Mountains.")
			qm2.start()

elif not player_info:
	if msg == "busy":
		me.SayTo(activator, "\nYes, busy. I'm in the process of creating a very powerful ^mana crystal^.")

	elif msg == "mana crystal":
		me.SayTo(activator, "\nYou just won't don't want to give up, do you? Okay, I will ^tell^ you about mana crystals...")

	elif msg == "tell":
		me.SayTo(activator, "\nMana crystals are items sought after by mages. They allow you to ^store^ a certain amount of mana, you see.")

	elif msg == "store":
		me.SayTo(activator, "\nWhen a mage applies a mana crystal while he is full on mana, half of his mana will be transferred to the crystal. The mage can then apply the crystal at any time to get the mana back. Crystals have a maximum mana capacity, so mages are always after the ones that can hold the most.\nHmm, I seem to have a crystal I don't need right here. Do you ^want^ it?")

	elif msg == "want":
		activator.CreatePlayerInfo(player_info_name)
		load_quests()
		create_crystal()
