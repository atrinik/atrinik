## @file
## This script implements gate guard of the Tutorial island.
##
## It checks what quests the player has solved on the Tutorial Island, and
## if all were solved, gives key for the gate. Otherwise the guard tells
## the player a hint where to go for the next quest.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()

exec(open(CreatePathname("/shattered_islands/scripts/tutorial_island/quests.py")).read())

def find_incomplete_quest():
	for quest_name in quest_items:
		## Initialize QuestManager.
		qm = QuestManager(activator, quest_items[quest_name]["info"])

		if not qm.started() or not qm.completed():
			return quest_name

	return None

def main():
	quest_name = find_incomplete_quest()

	if not quest_name:
		## Arch of the gate key.
		key_arch = "key_brown"
		## Name of the gate key.
		key_name = "Tutorial Island key"

		## Check if the player already has the key.
		key_activator = activator.FindObject(2, key_arch, key_name)

		if key_activator == None:
			## The activator does not have the key, so find the one in the
			## guard's inventory.
			key_me = me.FindObject(0, key_arch, key_name)

			if key_me == None:
				me.SayTo(activator, "\nSomething is wrong, missing the key... Call a DM!")
			else:
				## Clone the key from the guard's inventory.
				new_key = key_me.Clone(CLONE_WITHOUT_INVENTORY)

				if new_key:
					me.SayTo(activator, "\nWell done, {0}! You have completed all the quests on the Tutorial Island, and you may leave now.\nHere is your key.".format(activator.name))

					new_key.InsertInside(activator)
				else:
					me.SayTo(activator, "\nSomething is wrong... Call a DM!")
		else:
			me.SayTo(activator, "\nYou already have the key of this island.")
	else:
		me.SayTo(activator, "\nSorry, you haven't completed all the Tutorial Island quests, so, I cannot give you key to the gate.\nHint:\n{0}".format(quest_items[quest_name]["hint"]))

main()
