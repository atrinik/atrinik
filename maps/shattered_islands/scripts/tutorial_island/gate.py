## @file
## This script implements gate guard of the Tutorial island.
##
## It checks what quests the player has solved on the Tutorial Island, and
## if all were solved, gives key for the gate. Otherwise the guard tells
## the player a hint where to go for the next quest.

from Atrinik import *
import string, os
from QuestManager import QuestManager

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

exec(open(CreatePathname("/shattered_islands/scripts/tutorial_island/quests.py")).read())

## Has the player done all quests?
done_all_quests = True

for quest_name in quest_items:
	## Initialize QuestManager.
	qm = QuestManager(activator, quest_items[quest_name]["info"])

	if not qm.started() or not qm.completed():
		done_all_quests = False

		break

if done_all_quests == True:
	## Arch of the gate key.
	key_arch = "key_brown"
	## Name of the gate key.
	key_name = "Tutorial Island key"

	## Check if the player already has the key.
	key_activator = activator.CheckInventory(2, key_arch, key_name)

	if key_activator == None:
		## The activator does not have the key, so find the one in the
		## guard's inventory.
		key_me = me.CheckInventory(0, key_arch, key_name)

		if key_me == None:
			me.SayTo(activator, "\nSomething is wrong, missing the key... Call a DM!")
		else:
			## Clone the key from the guard's inventory.
			new_key = key_me.Clone(CLONE_WITHOUT_INVENTORY)

			if new_key:
				me.SayTo(activator, "\nWell done, %s! You have completed all the quests on the Tutorial Island, and you may leave now.\nHere is your key." % activator.name)

				new_key.InsertInside(activator)
			else:
				me.SayTo(activator, "\nSomething is wrong... Call a DM!")
	else:
		me.SayTo(activator, "\nYou already have the key of this island.")
else:
	me.SayTo(activator, "\nSorry, you haven't completed all the Tutorial Island quests, so, I cannot give you key to the gate.\nHint:\n%s" % quest_items[quest_name]["hint"])
