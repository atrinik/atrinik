## @file
## Example quest that requires the player to find a quest item by killing
## a specific monster.

from Atrinik import *
from QuestManager import *

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

## Get the message.
msg = WhatIsMessage().strip().lower()

## The quest.
quest = {
	# Name of this quest.
	"quest_name": "Dev Testmaps Quest 3",
	# Type of this quest.
	"type": QUEST_TYPE_KILL_ITEM,
	# If the quest is of type 'QUEST_TYPE_KILL_ITEM' the following three are used.
	"arch_name": "gravedirt",
	"item_name": "pile",
	# Whether to keep the quest item.
	"quest_item_keep": 1,
	# Message that will appear in the player's quest list about this quest.
	"message": "Find pile of graveyard dirt from the red ant.",
}

qm = QuestManager(activator, quest)

if msg == "hello" or msg == "hi" or msg == "hey":
	if not qm.started():
		me.SayTo(activator, "\nHello %s. I can demonstrate how 'kill item' quest works.\nDo you ^accept^ this quest?" % activator.name)
	elif qm.completed():
		me.SayTo(activator, "\nThank you for helping me out.")
	elif qm.finished():
		me.SayTo(activator, "\nYou have done an excellent job! As this is just an example quest, there is no reward, but you can keep the graveyard dirt.")
		qm.complete()
	else:
		me.SayTo(activator, "\nYou still need to find the pile of graveyard dirt from an ant on this map.")

# Accept the quest.
elif msg == "accept":
	if not qm.started():
		me.SayTo(activator, "\nFind a pile of graveyard dirt by killing an ant on this map.")
		qm.start()
