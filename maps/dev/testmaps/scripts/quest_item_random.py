## @file
## Example quest that requires the player to find a quest item by killing
## a specific monster, but the quest item drops randomly.

from Atrinik import *
from QuestManager import QuestManager

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

## Get the message.
msg = WhatIsMessage().strip().lower()

## The quest.
quest = {
	# Name of this quest.
	"quest_name": "Dev Testmaps Quest 4",
	# Type of this quest.
	"type": QUEST_TYPE_KILL_ITEM,
	"arch_name": "rod_heavy",
	"item_name": "heavy rod",
	# Message that will appear in the player's quest list about this quest.
	"message": "Find heavy rod the gray bear has stolen.",
}

qm = QuestManager(activator, quest)

if msg == "hello" or msg == "hi" or msg == "hey":
	if not qm.started():
		me.SayTo(activator, "\nHello %s. I can demonstrate how 'kill item' quest works.\nDo you ^accept^ this quest?" % activator.name)
	elif qm.completed():
		me.SayTo(activator, "\nThank you for helping me out.")
	elif qm.finished():
		me.SayTo(activator, "\nYou have done an excellent job! As this is just an example quest, there is no reward.")
		qm.complete()
	else:
		me.SayTo(activator, "\nYou still need to find the heavy rod by killing gray bear on this map.\nHowever, the gray bear has only 1/10 chance of dropping the heavy rod.")

# Accept the quest.
elif msg == "accept":
	if not qm.started():
		me.SayTo(activator, "\nFind a heavy rod by killing gray bear on this map.\nHowever, the gray bear has only 1/10 chance of dropping the heavy rod.")
		qm.start()
