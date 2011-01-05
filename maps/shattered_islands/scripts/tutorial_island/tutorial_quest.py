## @file
## Quest to get gate key on Tutorial Island.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()

msg = WhatIsMessage().strip().lower()

quest = {
	"quest_name": "Wolf Cubs",
	"type": QUEST_TYPE_KILL,
	"kills": 5,
	"message": "Kill 5 wolf cubs, then return to Tommy to get the key.",
}

# Arch of the gate key.
key_arch = "key_brown"
# Name of the gate key.
key_name = "Tutorial Key"

qm = QuestManager(activator, quest)

def give_key():
	# Find the key, clone it, and insert the clone inside the player
	me.CheckInventory(0, key_arch, key_name).Clone(CLONE_WITHOUT_INVENTORY).InsertInside(activator)

if msg == "hello" or msg == "hi" or msg == "hey":
	me.SayTo(activator, "\nHowdy %s, I am %s." % (activator.name, me.name))

	if not qm.started():
		me.SayTo(activator, "You'll need a key to pass through the gate.\nI'll need you to kill some wolf cubs before I give it to you.\nWill you ^accept^ this quest?", 1)
	elif qm.completed():
		if not activator.CheckInventory(2, key_arch, key_name):
			me.SayTo(activator, "You lost the key I gave you?! Luckily I have a spare one for you.", 1)
			give_key()
		else:
			me.SayTo(activator, "Go through the gate to end the tutorial.", 1)
	elif qm.finished():
		me.SayTo(activator, "Okay, here is the key! Now continue through the gate.", 1)
		give_key()
		qm.complete()
	else:
		me.SayTo(activator, "You still need to kill %d wolves." % qm.num_to_kill(), 1)

elif msg == "accept":
	if not qm.started():
		me.SayTo(activator, "\nKill 5 of the wolf cubs on this island and I will give you the key.")
		qm.start()

elif msg == "no":
	if not qm.started():
		me.SayTo(activator, "\nVery well, I'll ask someone else to help me.")