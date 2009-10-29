## @file
## Implements quest given by the ogre named Frah'ak in Tutorial Island
## jail. Once completed, the player is taught find traps skill.
##
## Also uses the /spit command to spit on anyone who says
## "Still there, Frah'ak?" near him.

from Atrinik import *
import string, os
from inspect import currentframe

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

execfile(os.path.dirname(currentframe().f_code.co_filename) + "/quests.py")

## Quest item arch name.
quest_arch_name = quest_items["jail_ogre"]["arch_name"]
## Quest item name.
quest_item_name = quest_items["jail_ogre"]["item_name"]

## Check if the activator has a quest object. If so, the quest was
## already completed.
qitem = activator.CheckQuestObject(quest_arch_name, quest_item_name)
## Check if the activator has the quest item we're looking for.
item = activator.CheckInventory(1, quest_arch_name, quest_item_name)

msg = WhatIsMessage().strip().lower()
text = string.split(msg)

# Jail guard annoying Frah'ak
if msg == "still there, frah'ak?":
	me.Communicate("/spit " + activator.name)

elif text[0] == "warrior":
	me.SayTo(activator, "Me big chief. Me ogre destroy you.\nStomp on. Dragon kakka.")

elif text[0] == "kobolds":
	me.SayTo(activator, "\nKobolds traitors!\nGive gold for note, kobolds don't bring note to ogres.\nMe tell you: Kill kobold chief!\nMe will teach you ^find traps^ skill!\nShow me note I will teach you.\nKobolds in hole next room. Secret entry in wall.")

elif msg == "teach me find traps":
	## Get the skill number of the find traps skill.
	skill = GetSkillNr("find traps")

	if skill == -1:
		me.SayTo(activator, "Unknown skill - find traps.")
	else:
		## Try to get the find traps skill object from the player; if
		## found, we don't give the player the skill again.
		sobj = activator.GetSkill(TYPE_SKILL, skill)

		if sobj == None:
			if qitem == None and item != None:
				activator.AddQuestObject(quest_arch_name, quest_item_name)
				activator.Write("%s takes %s from your inventory." % (me.name, item.name), COLOR_WHITE)
				item.Remove()
				me.SayTo(activator, "Here we go!")
				me.map.Message(me.x, me.y, MAP_INFO_NORMAL, "Frah'ak teach some ancient skill.", COLOR_YELLOW)
				activator.AcquireSkill(skill)
			else:
				me.SayTo(activator, "\nNah, bring Frah'ak note from ^kobolds^ first!")

elif msg == "find traps":
	skill = GetSkillNr("find traps")

	if skill == -1:
		me.SayTo(activator, "Unknown skill - find traps.")
	else:
		sobj = activator.GetSkill(TYPE_SKILL, skill)

		if sobj == None:
			if qitem == None and item != None:
				me.SayTo(activator, "\nFrah'ak tell yo truth!\n Say ^teach me find traps^ now!")
			else:
				me.SayTo(activator, "\nNah, bring Frah'ak note from ^kobolds^ first!")

elif msg == "hello" or msg == "hi" or msg == "hey":
	skill = GetSkillNr("find traps")

	if qitem == None and item != None and skill != -1 and activator.DoKnowSkill(skill) != 1:
		me.Communicate("/grin " + activator.name)
		me.SayTo(activator, "\nAshahk! Yo bring me note!\nKobold chief bad time now, ha?\nNow me will teach you!\nSay ^teach me find traps^ now!")
	else:
		me.SayTo(activator, "\nYo shut up.\nYo grack zhal hihzuk alshzu...\nMe mighty ogre chief.\nMe ^warrior^ will destroy yo. They come.\nGuard and ^Kobolds^ will die then.")

else:
	activator.Write("%s listens to you without answer." % me.name, COLOR_WHITE)
