## @file
## Captain Regulus gives out a quest for remove traps skill, if the
## player finds crystalized slime master bracers from the slime master in
## the hole next to Captain Regulus.

from Atrinik import *
import string, os
from inspect import currentframe

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

execfile(os.path.dirname(currentframe().f_code.co_filename) + "/quests.py")

## Quest item arch name.
quest_arch_name = quest_items["captain_regulus"]["arch_name"]
## Quest item name.
quest_item_name = quest_items["captain_regulus"]["item_name"]

msg = WhatIsMessage().strip().lower()
text = string.split(msg)

## Check if the activator has a quest object. If so, the quest was
## already completed.
qitem = activator.CheckQuestObject(quest_arch_name, quest_item_name)

if text[0] == "quest":
	if qitem == None:
		me.SayTo(activator, "\nThere appears to be some kind of hole in the ground over here.\nI have sent some guards down in there, but none returned. I can hear some strange noises from under there, like slimes or something.\nI fear there is some kind of attack going to happen, if no one stops it before it happens.\nYou must go in there now, and kill their leader, whoever it is!\nBring me back some proof if you manage to do it.\nI will, of course, reward you.")
	else:
		me.SayTo(activator, "\nYou have done it! Really great! Now say ^teach me remove traps^ for the reward.")

elif msg == "teach me remove traps":
	if qitem == None:
		me.SayTo(activator, "\nFirst kill the master slime!\nShow me a part of his body as proof!");
	else:
		## Get the skill number of remove traps.
		skill = GetSkillNr("remove traps")

		if skill == -1:
			me.SayTo(activator, "Unknown skill - remove traps.")
		else:
			## Find out whether the player already knows remove traps. If
			## so, we don't teach him again.
			sobj = activator.GetSkill(TYPE_SKILL, skill)

			if sobj == None:
				me.SayTo(activator, "Here we go!")
				me.map.Message(me.x, me.y, MAP_INFO_NORMAL, "Regulus teaches some ancient skill.", COLOR_YELLOW)
				activator.AcquireSkill(skill)

elif msg == "hello" or msg == "hi" or msg == "hey":
	if qitem == None:
		me.SayTo(activator, "\nHello %s. If you are interested, I have a ^quest^ for you." % activator.name)
	else:
		skill = GetSkillNr("remove traps")

		if skill == -1:
			me.SayTo(activator, "Unknown skill - remove traps.")
		else:
			sobj = activator.GetSkill(TYPE_SKILL, skill)

			if sobj == None:
				me.SayTo(activator, "\nYou have done it! Really great! Thanks so much! Now say ^teach me remove traps^ for the reward.")
			else:
				me.SayTo(activator, "\nThank you for helping us out.")

else:
	activator.Write("%s listens to you without answer." % me.name, COLOR_WHITE)
