## @file
## Captain Regulus gives out a quest for remove traps skill, if the
## player finds crystalized slime master bracers from the slime master in
## the hole next to Captain Regulus.

from Atrinik import *
import string, os
from QuestManager import QuestManager

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

exec(open(CreatePathname("/shattered_islands/scripts/tutorial_island/quests.py")).read())

msg = WhatIsMessage().strip().lower()
text = msg.split()

## Initialize QuestManager.
qm = QuestManager(activator, quest_items["captain_regulus"]["info"])

# Give out information about the quest.
if text[0] == "quest":
	if not qm.started():
		me.SayTo(activator, "\nThere appears to be some kind of hole in the ground over here.\nI have sent some guards down in there, but none returned. I can hear some strange noises from under there, like slimes or something.\nI fear there is some kind of attack going to happen, if no one stops it before it happens.\nYou must go in there now, and kill their leader, whoever it is!\nDo you ^accept^ this quest?")
	elif qm.finished():
		me.SayTo(activator, "\nYou have done it! Really great! Now say ^teach me remove traps^ for the reward.")

# Accept the quest.
elif text[0] == "accept":
	if not qm.started():
		me.SayTo(activator, "\nReturn to me with a proof of your victory and I will reward you.")
		qm.start()
	elif qm.completed():
		me.SayTo(activator, "\nThank you for helping us out.")

# Reward.
elif msg == "teach me remove traps":
	if not qm.finished():
		me.SayTo(activator, "\nFirst kill the master slime!\nShow me a part of his body as proof!");
	elif not qm.completed():
		## Get the skill number of remove traps.
		skill = GetSkillNr("remove traps")

		if skill == -1:
			me.SayTo(activator, "Unknown skill - remove traps.")
		else:
			qm.complete()

			# Only teach them if they don't know the skill yet.
			if activator.GetSkill(TYPE_SKILL, skill) == None:
				me.SayTo(activator, "Here we go!")
				me.map.Message(me.x, me.y, MAP_INFO_NORMAL, "Regulus teaches some ancient skill.", COLOR_YELLOW)
				activator.AcquireSkill(skill)

# Greeting section.
elif msg == "hello" or msg == "hi" or msg == "hey":
	if not qm.started():
		me.SayTo(activator, "\nHello %s. If you are interested, I have a ^quest^ for you." % activator.name)
	elif qm.finished():
		skill = GetSkillNr("remove traps")

		if skill == -1:
			me.SayTo(activator, "Unknown skill - remove traps.")
		else:
			if activator.GetSkill(TYPE_SKILL, skill) == None:
				me.SayTo(activator, "\nYou have done it! Really great! Thanks so much! Now say ^teach me remove traps^ for the reward.")
			else:
				me.SayTo(activator, "\nThank you for helping us out, but it seems like you already know the remove traps skill so I have nothing to reward you with.")
				qm.complete()
	elif qm.completed():
		me.SayTo(activator, "\nThank you for helping us out.")
	else:
		me.SayTo(activator, "\nRemember, bring me back a proof of your victory over the slimes leader!")
