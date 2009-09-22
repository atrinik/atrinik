from Atrinik import *
import string, os
from inspect import currentframe

activator = WhoIsActivator()
me = WhoAmI()

execfile(os.path.dirname(currentframe().f_code.co_filename) + "/quests.py")

done_all_quests = True

for quest_name in quest_items:
	qitem = activator.CheckQuestObject(quest_items[quest_name]["arch_name"], quest_items[quest_name]["item_name"])

	if qitem == None:
		done_all_quests = False

		break

if done_all_quests == True:
	key_arch = "key_brown"
	key_name = "Tutorial Island key"

	key_activator = activator.CheckInventory(0, key_arch, key_name)

	if key_activator == None:
		key_me = me.CheckInventory(0, key_arch, key_name)

		if key_me == None:
			me.SayTo(activator, "\nSomething is wrong, missing the key... Call a DM!")
		else:
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
