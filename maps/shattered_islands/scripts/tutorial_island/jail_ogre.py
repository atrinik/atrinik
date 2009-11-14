## @file
## Implements quest given by the ogre named Frah'ak in Tutorial Island
## jail. Once completed, the player is taught find traps skill.
##
## Also uses the /spit command to spit on anyone who says
## "Still there, Frah'ak?" near him.

from Atrinik import *
import string, os
from inspect import currentframe
from imp import load_source

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

execfile(os.path.dirname(currentframe().f_code.co_filename) + "/quests.py")

msg = WhatIsMessage().strip().lower()
text = string.split(msg)

## The QuestManager class.
QuestManager = load_source("QuestManager", CreatePathname("/python/QuestManager.py"))

## Initialize QuestManager.
qm = QuestManager.QuestManager(activator, quest_items["jail_ogre"]["info"])

# Jail guard annoying Frah'ak
if msg == "still there, frah'ak?":
	me.Communicate("/spit " + activator.name)

elif text[0] == "warrior":
	me.SayTo(activator, "Me big chief. Me ogre destroy you.\nStomp on. Dragon kakka.")

elif text[0] == "kobolds":
	me.SayTo(activator, "\nKobolds traitors!\nGive gold for note, kobolds don't bring note to ogres.\nMe tell you: Kill kobold chief!\nMe will teach you find traps skill!\nShow me note I will teach you.\nKobolds in hole next room. Secret entry in wall.")

	if not qm.started():
		me.SayTo(activator, "Do yo ^accept^ mission?", 1)

# Accept the quest.
elif text[0] == "accept":
	if not qm.started():
		me.SayTo(activator, "\nShow me the note I will teach you!")
		qm.start()
	elif qm.completed():
		me.SayTo(activator, "\nKobold chief bad time now, ha?")

elif msg == "teach me find traps":
	if not qm.finished():
		me.SayTo(activator, "\nNah, bring Frah'ak note from ^kobolds^ first!")
	elif not qm.completed():
		## Get the skill number of the find traps skill.
		skill = GetSkillNr("find traps")

		if skill == -1:
			me.SayTo(activator, "Unknown skill - find traps.")
		else:
			qm.complete()

			# Try to get the find traps skill object from the player; if
			# found, we don't give the player the skill again.
			if activator.GetSkill(TYPE_SKILL, skill) == None:
				activator.Write("%s takes %s from your inventory." % (me.name, quest_items["jail_ogre"]["info"]["item_name"]), COLOR_WHITE)
				me.SayTo(activator, "Here we go!")
				me.map.Message(me.x, me.y, MAP_INFO_NORMAL, "Frah'ak teaches some ancient skill.", COLOR_YELLOW)
				activator.AcquireSkill(skill)

elif msg == "hello" or msg == "hi" or msg == "hey":
	if not qm.started() or qm.completed():
		me.SayTo(activator, "\nYo shut up.\nYo grack zhal hihzuk alshzu...\nMe mighty ogre chief.\nMe ^warrior^ will destroy yo. They come.\nGuard and ^Kobolds^ will die then.")
	elif qm.finished():
		skill = GetSkillNr("find traps")

		if activator.GetSkill(TYPE_SKILL, skill) == None:
			me.Communicate("/grin " + activator.name)
			me.SayTo(activator, "\nAshahk! Yo bring me note!\nKobold chief bad time now, ha?\nNow me will teach you!\nSay ^teach me find traps^ now!")
		else:
			me.Communicate("/grin " + activator.name)
			qm.complete()
	else:
		me.SayTo(activator, "\nBring Frah'ak note from ^kobolds^ first!")

else:
	activator.Write("%s listens to you without answer." % me.name, COLOR_WHITE)
